// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UCustomizableObjectService.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "CustomizableObjectServiceInternal.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Factories/Factory.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace
{
	const TCHAR* const FactoryClassPath = TEXT("/Script/CustomizableObjectEditor.CustomizableObjectFactory");

	/** Classes the engine migrates to a newer node on load; creating one would be undone next load. */
	bool IsLegacyNodeClass(const UClass* Class)
	{
		static const TCHAR* const Legacy[] = {
			TEXT("CustomizableObjectNodeComponentMesh"),
			TEXT("CustomizableObjectNodeComponentPassthroughMesh"),
		};
		for (const TCHAR* Name : Legacy)
		{
			if (Class->GetName() == Name)
			{
				return true;
			}
		}
		return false;
	}

	/** Enum-style option names from a node's "Values" array of { Name, ... } structs. */
	TArray<FString> ReadEnumValues(const UObject* Node)
	{
		TArray<FString> Options;
		const FArrayProperty* Values = CastField<FArrayProperty>(Node->GetClass()->FindPropertyByName(TEXT("Values")));
		const FStructProperty* Inner = Values ? CastField<FStructProperty>(Values->Inner) : nullptr;
		const FProperty* NameProperty = Inner ? Inner->Struct->FindPropertyByName(TEXT("Name")) : nullptr;
		if (!NameProperty)
		{
			return Options;
		}
		FScriptArrayHelper Helper(Values, Values->ContainerPtrToValuePtr<void>(Node));
		for (int32 Index = 0; Index < Helper.Num(); ++Index)
		{
			FString Name;
			NameProperty->ExportTextItem_Direct(Name, NameProperty->ContainerPtrToValuePtr<void>(Helper.GetRawPtr(Index)),
				nullptr, nullptr, PPF_None);
			Options.Add(Name);
		}
		return Options;
	}

	/** Saved Customizable Objects whose Base Object node names Parent as its ParentObject. */
	TArray<FString> FindChildObjects(UObject* Parent)
	{
		TArray<FString> Children;
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		// The same discovery the Mutable compiler uses: hard package referencers of the parent, of
		// the Customizable Object class, whose ParentObject is the parent. Unsaved children are not
		// in the registry and — equally — are invisible to the compiler.
		TArray<FName> Referencers;
		Registry.GetReferencers(FName(*VibeCO::PackagePathOf(Parent)), Referencers,
			UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::EDependencyQuery::Hard);

		const FTopLevelAssetPath ObjectClassPath = VibeCO::GetObjectClass()->GetClassPathName();
		for (const FName& PackageName : Referencers)
		{
			TArray<FAssetData> Assets;
			Registry.GetAssetsByPackageName(PackageName, Assets);
			for (const FAssetData& Asset : Assets)
			{
				if (Asset.AssetClassPath != ObjectClassPath)
				{
					continue;
				}
				UObject* Child = Asset.GetAsset();
				UEdGraphNode* Base = VibeCO::FindBaseObjectNode(VibeCO::GetSourceGraph(Child));
				if (Base && VibeCO::ReadObjectPath(Base, TEXT("ParentObject")) == Parent->GetPathName())
				{
					Children.AddUnique(PackageName.ToString());
				}
			}
		}
		Children.Sort();
		return Children;
	}
}

bool UCustomizableObjectService::IsMutableAvailable()
{
	return VibeCO::CheckAvailable().IsEmpty();
}

TArray<FString> UCustomizableObjectService::ListCustomizableObjects(const FString& DirectoryPath)
{
	TArray<FString> Paths;
	if (!IsMutableAvailable() || DirectoryPath.IsEmpty() || !VibeCO::CheckNameLength(DirectoryPath, TEXT("DirectoryPath")).IsEmpty())
	{
		return Paths;
	}

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	FARFilter Filter;
	Filter.ClassPaths.Add(VibeCO::GetObjectClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(*DirectoryPath));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);
	for (const FAssetData& Asset : Assets)
	{
		Paths.Add(Asset.PackageName.ToString());
	}
	Paths.Sort();
	return Paths;
}

FVibeCOEditResult UCustomizableObjectService::CreateCustomizableObject(const FString& AssetPath)
{
	FVibeCOEditResult Result;
	Result.Error = VibeCO::CheckAvailable();
	if (Result.Error.IsEmpty())
	{
		Result.Error = VibeCO::CheckWritableAssetPath(AssetPath.TrimStartAndEnd());
	}
	if (!Result.Error.IsEmpty())
	{
		return Result;
	}

	const FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath.TrimStartAndEnd());
	const FString PackageDir = FPackageName::GetLongPackagePath(PackageName);
	const FString AssetName = FPackageName::GetShortName(PackageName);

	// Both halves: the file covers an asset this process never loaded, FindPackage one created
	// earlier this session and not yet saved.
	FString Filename;
	const bool bOnDisk = FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename,
		FPackageName::GetAssetPackageExtension()) && IFileManager::Get().FileExists(*Filename);
	if (bOnDisk)
	{
		Result.Error = FString::Printf(TEXT("Asset already exists: %s"), *PackageName);
		return Result;
	}
	if (UPackage* InMemory = FindPackage(nullptr, *PackageName))
	{
		// In memory but not on disk: either unsaved new work (dirty) or the remains of a deleted asset.
		// The latter is freed by a collection unless something still references it.
		if (InMemory->IsDirty())
		{
			Result.Error = FString::Printf(TEXT("Asset already exists (unsaved): %s"), *PackageName);
			return Result;
		}
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		if (FindPackage(nullptr, *PackageName))
		{
			Result.Error = FString::Printf(TEXT("%s was deleted but is still loaded because something references it "
				"(a child object's ParentObject, an open editor). Restart the editor, or use a different name."),
				*PackageName);
			return Result;
		}
	}

	// The engine's own factory: it creates the UCustomizableObjectGraph and the Base Object node the
	// editor expects, which a bare NewObject would not.
	UClass* FactoryClass = FindObject<UClass>(nullptr, FactoryClassPath);
	UFactory* Factory = FactoryClass ? NewObject<UFactory>(GetTransientPackage(), FactoryClass) : nullptr;
	if (!Factory)
	{
		Result.Error = TEXT("CustomizableObjectFactory was not found; this Mutable version is not supported");
		return Result;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	UObject* Created = AssetTools.CreateAsset(AssetName, PackageDir, VibeCO::GetObjectClass(), Factory);
	if (!Created)
	{
		Result.Error = FString::Printf(TEXT("Failed to create Customizable Object %s"), *PackageName);
		return Result;
	}

	UEdGraph* Graph = VibeCO::GetSourceGraph(Created);
	Result.Error = VibeCO::CommitEdit(Created, Graph);
	if (!Result.Error.IsEmpty())
	{
		return Result;
	}

	const UEdGraphNode* Base = VibeCO::FindBaseObjectNode(Graph);
	Result.NodeId = VibeCO::NodeIdOf(Base);
	Result.Pins = VibeCO::DescribePins(Base);
	Result.bSuccess = true;
	return Result;
}

FVibeCOObjectInfo UCustomizableObjectService::GetObjectInfo(const FString& AssetPath)
{
	FVibeCOObjectInfo Info;
	UObject* Object = nullptr;
	UEdGraph* Graph = nullptr;
	Info.Error = VibeCO::OpenObject(AssetPath, Object, Graph, /*bAllowNoGraph*/ true);
	if (!Info.Error.IsEmpty())
	{
		return Info;
	}

	Info.bIsCompiled = VibeCO::CallForText(Object, TEXT("IsCompiled")).Equals(TEXT("True"), ESearchCase::IgnoreCase);

	if (Graph)
	{
		Info.NodeCount = Graph->Nodes.Num();
		const UEdGraphNode* Base = VibeCO::FindBaseObjectNode(Graph);
		Info.BaseNodeId = VibeCO::NodeIdOf(Base);
		Info.ParentObject = FPackageName::ObjectPathToPackageName(VibeCO::ReadObjectPath(Base, TEXT("ParentObject")));
		Info.bIsChildObject = !Info.ParentObject.IsEmpty();

		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (!VibeCO::IsMutableNode(Node))
			{
				continue;
			}
			// Parameter nodes carry ParameterName; a Group node is itself an enum parameter over its
			// child objects, named by GroupName.
			FVibeCOParameterInfo Parameter;
			if (Node->GetClass()->FindPropertyByName(TEXT("ParameterName")))
			{
				Parameter.Name = VibeCO::ReadPropertyText(Node, TEXT("ParameterName"));
			}
			else if (Node->GetClass()->FindPropertyByName(TEXT("GroupName")))
			{
				Parameter.Name = VibeCO::ReadPropertyText(Node, TEXT("GroupName"));
			}
			else
			{
				continue;
			}
			FString Type = Node->GetClass()->GetName();
			Type.RemoveFromStart(TEXT("CustomizableObjectNode"));
			Type.RemoveFromStart(TEXT("CONode"));
			Parameter.Type = Type;
			Parameter.Options = ReadEnumValues(Node);
			Parameter.NodeId = VibeCO::NodeIdOf(Node);
			Info.GraphParameters.Add(MoveTemp(Parameter));
		}
	}

	Info.ChildObjects = FindChildObjects(Object);

	if (Info.bIsCompiled)
	{
		const int32 ParameterCount = FCString::Atoi(*VibeCO::CallForText(Object, TEXT("GetParameterCount"), {}, TEXT("0")));
		for (int32 Index = 0; Index < ParameterCount; ++Index)
		{
			FVibeCOParameterInfo Parameter;
			Parameter.Name = VibeCO::CallForText(Object, TEXT("GetParameterName"), { FString::FromInt(Index) });
			Parameter.Type = VibeCO::CallForText(Object, TEXT("GetParameterTypeByName"), { Parameter.Name });
			if (Parameter.Type == TEXT("Int"))
			{
				const int32 OptionCount = FCString::Atoi(
					*VibeCO::CallForText(Object, TEXT("GetEnumParameterNumValues"), { Parameter.Name }, TEXT("0")));
				for (int32 Option = 0; Option < OptionCount; ++Option)
				{
					Parameter.Options.Add(VibeCO::CallForText(Object, TEXT("GetEnumParameterValue"),
						{ Parameter.Name, FString::FromInt(Option) }));
				}
			}
			Info.CompiledParameters.Add(MoveTemp(Parameter));
		}

		const int32 StateCount = FCString::Atoi(*VibeCO::CallForText(Object, TEXT("GetStateCount"), {}, TEXT("0")));
		for (int32 Index = 0; Index < StateCount; ++Index)
		{
			Info.States.Add(VibeCO::CallForText(Object, TEXT("GetStateName"), { FString::FromInt(Index) }));
		}

		const int32 ComponentCount = FCString::Atoi(*VibeCO::CallForText(Object, TEXT("GetComponentCount"), {}, TEXT("0")));
		for (int32 Index = 0; Index < ComponentCount; ++Index)
		{
			Info.Components.Add(VibeCO::CallForText(Object, TEXT("GetComponentName"), { FString::FromInt(Index) }));
		}
	}

	Info.bSuccess = true;
	return Info;
}

FVibeCOGraphInfo UCustomizableObjectService::GetGraph(const FString& AssetPath)
{
	FVibeCOGraphInfo Info;
	UObject* Object = nullptr;
	UEdGraph* Graph = nullptr;
	Info.Error = VibeCO::OpenObject(AssetPath, Object, Graph);
	if (!Info.Error.IsEmpty())
	{
		return Info;
	}

	for (const UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}
		Info.Nodes.Add(VibeCO::DescribeNode(Node));
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->bOrphanedPin || Pin->Direction != EGPD_Output)
			{
				continue;
			}
			for (const UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked && Linked->GetOwningNodeUnchecked())
				{
					FVibeCOConnectionInfo Connection;
					Connection.FromNodeId = VibeCO::NodeIdOf(Node);
					Connection.FromPin = Pin->PinName.ToString();
					Connection.ToNodeId = VibeCO::NodeIdOf(Linked->GetOwningNode());
					Connection.ToPin = Linked->PinName.ToString();
					Info.Connections.Add(MoveTemp(Connection));
				}
			}
		}
	}
	Info.bSuccess = true;
	return Info;
}

FVibeCONodeInfo UCustomizableObjectService::GetNode(const FString& AssetPath, const FString& NodeId)
{
	UObject* Object = nullptr;
	UEdGraph* Graph = nullptr;
	FString Error = VibeCO::OpenObject(AssetPath, Object, Graph);
	if (!Error.IsEmpty())
	{
		return FVibeCONodeInfo();
	}
	return VibeCO::DescribeNode(VibeCO::FindNode(Graph, NodeId, Error));
}

TArray<FVibeCONodeTypeInfo> UCustomizableObjectService::DiscoverNodeTypes(const FString& Filter, bool bIncludeHidden)
{
	TArray<FVibeCONodeTypeInfo> Types;
	if (!IsMutableAvailable())
	{
		return Types;
	}

	UClass* const Base = VibeCO::GetNodeBaseClass();
	const FString Needle = Filter.TrimStartAndEnd();
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class->IsChildOf(Base) || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_NewerVersionExists)
			|| Class->GetName().StartsWith(TEXT("SKEL_")) || Class->GetName().StartsWith(TEXT("REINST_")))
		{
			continue;
		}

		FVibeCONodeTypeInfo Type;
		Type.ClassName = Class->GetName();
		Type.DisplayName = Class->GetDisplayNameText().ToString();
		Type.Tooltip = Class->GetToolTipText(/*bShortTooltip*/ true).ToString();
		const UEdGraphNode* Default = Cast<UEdGraphNode>(Class->GetDefaultObject());
		Type.bDeprecated = Class->HasAnyClassFlags(CLASS_Deprecated) || IsLegacyNodeClass(Class)
			|| (Default && Default->IsDeprecated());
		Type.bPlaceable = !Type.bDeprecated && !Class->HasAnyClassFlags(CLASS_NotPlaceable | CLASS_HideDropDown)
			&& VibeCO::WhyNotPlaceable(Class).IsEmpty();
		Type.Module = Class->GetOutermost()->GetName();
		Type.Module.RemoveFromStart(TEXT("/Script/"));

		if (!bIncludeHidden && !Type.bPlaceable)
		{
			continue;
		}
		if (!Needle.IsEmpty() && !Type.ClassName.Contains(Needle) && !Type.DisplayName.Contains(Needle))
		{
			continue;
		}
		Types.Add(MoveTemp(Type));
	}

	Types.Sort([](const FVibeCONodeTypeInfo& A, const FVibeCONodeTypeInfo& B) { return A.ClassName < B.ClassName; });
	return Types;
}

TArray<FVibeCOPropertyInfo> UCustomizableObjectService::GetNodeTypeProperties(const FString& NodeClass)
{
	if (!IsMutableAvailable())
	{
		return {};
	}
	FString Error;
	UClass* Class = VibeCO::ResolveNodeClass(NodeClass, Error);
	return Class ? VibeCO::DescribeProperties(Class->GetDefaultObject()) : TArray<FVibeCOPropertyInfo>();
}

TArray<FVibeCOPropertyInfo> UCustomizableObjectService::GetNodeProperties(const FString& AssetPath, const FString& NodeId)
{
	UObject* Object = nullptr;
	UEdGraph* Graph = nullptr;
	FString Error = VibeCO::OpenObject(AssetPath, Object, Graph);
	if (!Error.IsEmpty())
	{
		return {};
	}
	return VibeCO::DescribeProperties(VibeCO::FindNode(Graph, NodeId, Error));
}
