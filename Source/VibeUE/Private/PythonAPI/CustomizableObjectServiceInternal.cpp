// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "CustomizableObjectServiceInternal.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EdGraph/EdGraph.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "FileHelpers.h"
#include "IMessageLogListing.h"
#include "Logging/TokenizedMessage.h"
#include "MessageLogModule.h"
#include "Misc/OutputDevice.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"
#include "Misc/StringOutputDevice.h"
#include "Modules/ModuleManager.h"
#include "UObject/CoreRedirects.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace VibeCO
{
	namespace
	{
		const TCHAR* const ObjectClassPath = TEXT("/Script/CustomizableObject.CustomizableObject");
		const TCHAR* const NodeBaseClassPath = TEXT("/Script/CustomizableObjectEditor.CustomizableObjectNode");
		const TCHAR* const NodeObjectClassPath = TEXT("/Script/CustomizableObjectEditor.CustomizableObjectNodeObject");
		const TCHAR* const SwitchClassPath = TEXT("/Script/CustomizableObjectEditor.CONodeSwitch");

		UClass* FindScriptClass(const TCHAR* Path)
		{
			return FindObject<UClass>(nullptr, Path);
		}

		FString DirectionName(EEdGraphPinDirection Direction)
		{
			return Direction == EGPD_Input ? TEXT("input") : TEXT("output");
		}
	}

	// ---- Class lookup ------------------------------------------------------------------------

	UClass* GetObjectClass()
	{
		return FindScriptClass(ObjectClassPath);
	}

	UClass* GetNodeBaseClass()
	{
		return FindScriptClass(NodeBaseClassPath);
	}

	FString CheckAvailable()
	{
		// Both halves: the runtime class alone exists in a cooked game, and the editor module is what
		// owns the graph, the schema and the compiler.
		if (!GetObjectClass() || !GetNodeBaseClass()
			|| !FModuleManager::Get().IsModuleLoaded(TEXT("CustomizableObjectEditor")))
		{
			return TEXT("Mutable is not available: this project has the Mutable plugin disabled. Enable it (Edit > "
						"Plugins > Mutable, Beta) and restart the editor. VibeUE does not enable it for you.");
		}
		return FString();
	}

	UClass* ResolveNodeClass(const FString& Name, FString& OutError)
	{
		UClass* const Base = GetNodeBaseClass();
		const FString Trimmed = Name.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			OutError = TEXT("NodeClass is empty. discover_node_types lists the classes.");
			return nullptr;
		}
		OutError = CheckNameLength(Trimmed, TEXT("NodeClass"));
		if (!OutError.IsEmpty())
		{
			return nullptr;
		}

		UClass* Found = nullptr;
		if (Trimmed.StartsWith(TEXT("/Script/")))
		{
			Found = FindObject<UClass>(nullptr, *Trimmed);
		}
		else
		{
			// Node classes also live in the Experimental Mutable* plugins, so the class is found by
			// name across every loaded module rather than assumed to be in CustomizableObjectEditor.
			const FString WithoutPrefix = Trimmed.StartsWith(TEXT("U"), ESearchCase::CaseSensitive)
				? Trimmed.Mid(1) : FString();
			for (TObjectIterator<UClass> It; It; ++It)
			{
				if (It->IsChildOf(Base) && (It->GetName() == Trimmed
					|| (!WithoutPrefix.IsEmpty() && It->GetName() == WithoutPrefix)))
				{
					Found = *It;
					break;
				}
			}

			// Legacy names from older Mutable versions (DefaultMutable.ini redirects), e.g.
			// CustomizableObjectNodeMaterial -> CONodeSkeletalMeshSection.
			if (!Found)
			{
				const FCoreRedirectObjectName Old(FName(*Trimmed), NAME_None,
					FName(TEXT("/Script/CustomizableObjectEditor")));
				const FCoreRedirectObjectName New = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Class, Old);
				if (New != Old)
				{
					Found = FindObject<UClass>(nullptr, *New.ToString());
				}
			}
		}

		if (!Found || !Found->IsChildOf(Base))
		{
			OutError = FString::Printf(TEXT("'%s' is not a Customizable Object node class. "
				"discover_node_types lists the classes add_node accepts."), *Trimmed);
			return nullptr;
		}
		if (Found->HasAnyClassFlags(CLASS_Abstract))
		{
			OutError = FString::Printf(TEXT("%s is abstract; pick one of its subclasses "
				"(discover_node_types with a filter)."), *Found->GetName());
			return nullptr;
		}
		OutError.Reset();
		return Found;
	}

	FString WhyNotPlaceable(const UClass* Class)
	{
		if (Class && Class->GetName() == TEXT("CustomizableObjectNodeTunnel"))
		{
			return TEXT("CustomizableObjectNodeTunnel is a Macro Library input/output node; it only exists inside "
						"macro graphs, which this service does not edit");
		}
		return FString();
	}

	// ---- Assets ------------------------------------------------------------------------------

	FString CheckNameLength(const FString& Value, const TCHAR* What)
	{
		// NAME_SIZE is the engine's own bound; constructing a longer FName is a fatal error.
		if (Value.Len() >= NAME_SIZE)
		{
			return FString::Printf(TEXT("%s is %d characters long; names are limited to %d"),
				What, Value.Len(), NAME_SIZE - 1);
		}
		return FString();
	}

	FString CheckWritableAssetPath(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty())
		{
			return TEXT("AssetPath is empty");
		}
		const FString LengthError = CheckNameLength(AssetPath, TEXT("AssetPath"));
		if (!LengthError.IsEmpty())
		{
			return LengthError;
		}
		const FString PackagePath = FPackageName::ObjectPathToPackageName(AssetPath);
		if (!FPackageName::IsValidLongPackageName(PackagePath))
		{
			return FString::Printf(TEXT("Not a valid asset path: %s"), *AssetPath);
		}
		for (const TCHAR* Forbidden : { TEXT("/Engine/"), TEXT("/Script/"), TEXT("/Temp/") })
		{
			if (PackagePath.StartsWith(Forbidden))
			{
				return FString::Printf(TEXT("%s is under %s, which this service does not write to. "
					"Use /Game (or a plugin's content root)."), *AssetPath, Forbidden);
			}
		}
		return FString();
	}

	FString ToObjectPath(const FString& AssetPath)
	{
		const FString Trimmed = AssetPath.TrimStartAndEnd();
		if (Trimmed.Contains(TEXT(".")))
		{
			return Trimmed;
		}
		return Trimmed + TEXT(".") + FPackageName::GetShortName(Trimmed);
	}

	FString PackagePathOf(const UObject* Object)
	{
		return Object ? Object->GetOutermost()->GetName() : FString();
	}

	UEdGraph* GetSourceGraph(UObject* Object)
	{
		if (!Object)
		{
			return nullptr;
		}
		// UCustomizableObject::Source is a private, editor-only UPROPERTY() — reflection is the one
		// route to it that needs no Mutable header.
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Object->GetClass(), TEXT("Source"));
		return Property ? Cast<UEdGraph>(Property->GetObjectPropertyValue_InContainer(Object)) : nullptr;
	}

	FString OpenObject(const FString& AssetPath, UObject*& OutObject, UEdGraph*& OutGraph, bool bAllowNoGraph)
	{
		OutObject = nullptr;
		OutGraph = nullptr;

		FString Error = CheckAvailable();
		if (!Error.IsEmpty())
		{
			return Error;
		}
		if (AssetPath.TrimStartAndEnd().IsEmpty())
		{
			return TEXT("AssetPath is empty");
		}
		Error = CheckNameLength(AssetPath, TEXT("AssetPath"));
		if (!Error.IsEmpty())
		{
			return Error;
		}

		UObject* Object = LoadObject<UObject>(nullptr, *ToObjectPath(AssetPath), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Object)
		{
			return FString::Printf(TEXT("Customizable Object not found: %s"), *AssetPath);
		}
		if (!Object->IsA(GetObjectClass()))
		{
			return FString::Printf(TEXT("%s is a %s, not a Customizable Object"),
				*AssetPath, *Object->GetClass()->GetName());
		}

		OutObject = Object;
		OutGraph = GetSourceGraph(Object);
		if (!OutGraph && !bAllowNoGraph)
		{
			return FString::Printf(TEXT("%s has no editor graph (cooked, or created outside the editor)"), *AssetPath);
		}
		return FString();
	}

	FString OpenForWrite(const FString& AssetPath, UObject*& OutObject, UEdGraph*& OutGraph)
	{
		const FString Error = OpenObject(AssetPath, OutObject, OutGraph);
		if (!Error.IsEmpty())
		{
			return Error;
		}
		return CheckWritableAssetPath(PackagePathOf(OutObject));
	}

	void BumpVersionId(UObject* Object)
	{
		// Mutable keys its compiled-data cache on VersionId. The engine refreshes it from a
		// package-dirty hook that is only registered in PostLoad (and from an open CO editor), so a
		// factory-new asset edited here would otherwise keep its first VersionId forever and a
		// compile could be served stale data.
		FStructProperty* VersionProperty = Object ? FindFProperty<FStructProperty>(Object->GetClass(), TEXT("VersionId")) : nullptr;
		if (VersionProperty && VersionProperty->Struct == TBaseStructure<FGuid>::Get())
		{
			*VersionProperty->ContainerPtrToValuePtr<FGuid>(Object) = FGuid::NewGuid();
		}
	}

	FString CommitEdit(UObject* Object, UEdGraph* Graph)
	{
		if (!Object)
		{
			return TEXT("CommitEdit: null object");
		}

		BumpVersionId(Object);

		if (Graph)
		{
			Graph->NotifyGraphChanged();
		}

		Object->MarkPackageDirty();
		UPackage* Package = Object->GetOutermost();
		FString Filename;
		const bool bHasFilename = FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), Filename,
			FPackageName::GetAssetPackageExtension());

		// bOnlyDirty = false: a position-only change is not guaranteed to dirty the package, and a
		// skipped save must not report success.
		if (!UEditorLoadingAndSavingUtils::SavePackages({ Package }, /*bOnlyDirty*/ false))
		{
			return FString::Printf(TEXT("The edit was applied in memory but %s failed to save "
				"(read-only file or a held file lock?)"), *Package->GetName());
		}

		// SavePackages' return value is not proof. For an asset deleted while something still
		// referenced it, the object lingers in memory, SavePackages returns true, and no file is
		// written: every edit would "succeed" into nothing. The file existing is the proof (timestamps
		// are too coarse to tell two saves a few milliseconds apart).
		if (!bHasFilename || !IFileManager::Get().FileExists(*Filename))
		{
			return FString::Printf(TEXT("%s did not reach disk. If the asset was deleted, this is a stale in-memory "
				"copy kept alive by a reference; restart the editor (or recreate the asset under a new name)."),
				*Package->GetName());
		}

		// Child objects are found — by this service AND by Mutable's compiler — through the Asset
		// Registry's package referencers. The registry learns a saved file's new dependencies from
		// its directory watcher, which lags (and never runs in a commandlet), so a child parented a
		// moment ago would be invisible to the very next compile of its root.
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.ScanModifiedAssetFiles({ FPaths::ConvertRelativePathToFull(Filename) });
		return FString();
	}

	// ---- Nodes & pins ------------------------------------------------------------------------

	FString NodeIdOf(const UEdGraphNode* Node)
	{
		return Node ? Node->NodeGuid.ToString(EGuidFormats::Digits) : FString();
	}

	bool IsMutableNode(const UEdGraphNode* Node)
	{
		UClass* const Base = GetNodeBaseClass();
		return Node && Base && Node->IsA(Base);
	}

	UEdGraphNode* FindNode(UEdGraph* Graph, const FString& NodeId, FString& OutError)
	{
		const FString Trimmed = NodeId.TrimStartAndEnd();
		if (!Graph)
		{
			OutError = TEXT("no graph");
			return nullptr;
		}
		if (Trimmed.IsEmpty())
		{
			OutError = TEXT("NodeId is empty. get_graph lists every NodeId.");
			return nullptr;
		}

		FGuid Guid;
		const bool bIsGuid = FGuid::Parse(Trimmed, Guid);
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && ((bIsGuid && Node->NodeGuid == Guid) || (!bIsGuid && Node->GetName() == Trimmed)))
			{
				OutError.Reset();
				return Node;
			}
		}
		OutError = FString::Printf(TEXT("No node '%s' in %s. get_graph lists every NodeId."),
			*Trimmed, *PackagePathOf(Graph));
		return nullptr;
	}

	UEdGraphNode* FindBaseObjectNode(UEdGraph* Graph)
	{
		UClass* const ObjectNodeClass = FindScriptClass(NodeObjectClassPath);
		if (!Graph || !ObjectNodeClass)
		{
			return nullptr;
		}
		const FBoolProperty* IsBase = FindFProperty<FBoolProperty>(ObjectNodeClass, TEXT("bIsBase"));
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->IsA(ObjectNodeClass) && (!IsBase || IsBase->GetPropertyValue_InContainer(Node)))
			{
				return Node;
			}
		}
		return nullptr;
	}

	UEdGraphPin* FindPin(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction, FString& OutError)
	{
		const FString Wanted = PinName.TrimStartAndEnd();
		if (!Node)
		{
			OutError = TEXT("no node");
			return nullptr;
		}
		if (Wanted.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Pin name is empty (node %s)"), *NodeIdOf(Node));
			return nullptr;
		}

		// Three passes, most exact first, so a pin whose internal name is another pin's label still
		// resolves to itself.
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			TArray<UEdGraphPin*> Matches;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->bOrphanedPin || (Direction != EGPD_MAX && Pin->Direction != Direction))
				{
					continue;
				}
				const FString Name = Pin->PinName.ToString();
				const bool bMatch =
					Pass == 0 ? Name.Equals(Wanted, ESearchCase::CaseSensitive)
					: Pass == 1 ? Name.Equals(Wanted, ESearchCase::IgnoreCase)
					: Pin->GetDisplayName().ToString().TrimStartAndEnd().Equals(Wanted, ESearchCase::IgnoreCase);
				if (bMatch)
				{
					Matches.Add(Pin);
				}
			}
			if (Matches.Num() == 1)
			{
				OutError.Reset();
				return Matches[0];
			}
			if (Matches.Num() > 1)
			{
				OutError = FString::Printf(TEXT("Pin '%s' is ambiguous on node %s (%d matches); "
					"pass the exact PinName from get_node"), *Wanted, *NodeIdOf(Node), Matches.Num());
				return nullptr;
			}
		}

		TArray<FString> Available;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && !Pin->bOrphanedPin && (Direction == EGPD_MAX || Pin->Direction == Direction))
			{
				Available.Add(FString::Printf(TEXT("'%s' (%s, %s)"), *Pin->PinName.ToString(),
					*DirectionName(Pin->Direction), *Pin->PinType.PinCategory.ToString()));
			}
		}
		OutError = FString::Printf(TEXT("No %spin '%s' on %s (%s). Available: %s"),
			Direction == EGPD_MAX ? TEXT("") : *(DirectionName(Direction) + TEXT(" ")),
			*Wanted, *NodeIdOf(Node), *Node->GetClass()->GetName(),
			Available.Num() ? *FString::Join(Available, TEXT(", ")) : TEXT("<none>"));
		return nullptr;
	}

	FVibeCOPinInfo DescribePin(const UEdGraphPin* Pin)
	{
		FVibeCOPinInfo Info;
		if (!Pin)
		{
			return Info;
		}
		Info.PinName = Pin->PinName.ToString();
		Info.DisplayName = Pin->GetDisplayName().ToString().TrimStartAndEnd();
		Info.Direction = DirectionName(Pin->Direction);
		Info.Category = Pin->PinType.PinCategory.ToString();
		Info.SubCategory = Pin->PinType.PinSubCategory.IsNone() ? FString() : Pin->PinType.PinSubCategory.ToString();
		Info.bIsArray = Pin->PinType.IsArray();
		Info.bHidden = Pin->bHidden;
		for (const UEdGraphPin* Linked : Pin->LinkedTo)
		{
			if (Linked && Linked->GetOwningNodeUnchecked())
			{
				FVibeCOPinLink Link;
				Link.NodeId = NodeIdOf(Linked->GetOwningNode());
				Link.PinName = Linked->PinName.ToString();
				Info.LinkedTo.Add(Link);
			}
		}
		return Info;
	}

	TArray<FVibeCOPinInfo> DescribePins(const UEdGraphNode* Node)
	{
		TArray<FVibeCOPinInfo> Pins;
		if (Node)
		{
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				// Orphaned pins are leftovers from a reconstruct that lost a pin still carrying links;
				// they cannot be connected to and the compiler ignores them.
				if (Pin && !Pin->bOrphanedPin)
				{
					Pins.Add(DescribePin(Pin));
				}
			}
		}
		return Pins;
	}

	FVibeCONodeInfo DescribeNode(const UEdGraphNode* Node)
	{
		FVibeCONodeInfo Info;
		if (!Node)
		{
			return Info;
		}
		Info.NodeId = NodeIdOf(Node);
		Info.NodeName = Node->GetName();
		Info.NodeClass = Node->GetClass()->GetName();
		Info.Title = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Replace(TEXT("\n"), TEXT(" "));
		Info.PosX = Node->NodePosX;
		Info.PosY = Node->NodePosY;
		Info.bCanDelete = Node->CanUserDeleteNode();
		if (Node->bHasCompilerMessage)
		{
			Info.Message = Node->ErrorMsg;
		}
		Info.Pins = DescribePins(Node);
		return Info;
	}

	void FinishNewNode(UEdGraph* Graph, UEdGraphNode* Node, int32 PosX, int32 PosY)
	{
		// The sequence of FCustomizableObjectSchemaAction_NewNode::CreateNode. ReconstructNode, not
		// AllocateDefaultPins: "Mutable node lifecycle always starts at ReconstructNode". The two
		// non-reflected steps it also runs (BeginConstruct, PostBackwardsCompatibleFixup) are empty
		// or re-run ReconstructNode for every class that can be created here.
		Node->SetFlags(RF_Transactional);
		Graph->AddNode(Node, /*bUserAction*/ true, /*bSelectNewNode*/ false);
		Node->CreateNewGuid();
		Node->PostPlacedNewNode();
		Node->ReconstructNode();
		Node->NodePosX = PosX;
		Node->NodePosY = PosY;
	}

	void RefreshDependentSwitches(UEdGraphNode* Node)
	{
		UClass* const SwitchClass = FindScriptClass(SwitchClassPath);
		if (!Node || !SwitchClass)
		{
			return;
		}
		// A Switch builds its option pins from the Enum Parameter feeding its "Switch Parameter" pin,
		// but only when that link changes — editing the enum's Values alone leaves stale options.
		TSet<UEdGraphNode*> Switches;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output)
			{
				for (const UEdGraphPin* Linked : Pin->LinkedTo)
				{
					UEdGraphNode* Owner = Linked ? Linked->GetOwningNodeUnchecked() : nullptr;
					if (Owner && Owner->IsA(SwitchClass))
					{
						Switches.Add(Owner);
					}
				}
			}
		}
		for (UEdGraphNode* Switch : Switches)
		{
			Switch->Modify();
			Switch->ReconstructNode();
		}
	}

	// ---- Properties --------------------------------------------------------------------------

	namespace
	{
		/**
		 * True for FEdGraphPinReference (and containers of it): the nodes' own handles to their pins,
		 * rebuilt by ReconstructNode. Writing one points a node at a pin it does not own.
		 */
		bool HoldsPinReference(const FProperty* Property)
		{
			if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
			{
				return Struct->Struct && Struct->Struct->GetFName() == TEXT("EdGraphPinReference");
			}
			if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
			{
				return HoldsPinReference(Array->Inner);
			}
			if (const FSetProperty* Set = CastField<FSetProperty>(Property))
			{
				return HoldsPinReference(Set->ElementProp);
			}
			if (const FMapProperty* Map = CastField<FMapProperty>(Property))
			{
				return HoldsPinReference(Map->KeyProp) || HoldsPinReference(Map->ValueProp);
			}
			return false;
		}

		/**
		 * ImportText is lenient in two ways that turn a typo into a silent wrong value: a numeric
		 * property takes "abc" as 0, and a bool takes any unknown word as false.
		 */
		FString CheckScalarText(const FProperty* Property, const FString& Value)
		{
			const FString Trimmed = Value.TrimStartAndEnd();
			if (const FNumericProperty* Numeric = CastField<FNumericProperty>(Property))
			{
				if (!Numeric->IsEnum() && !Trimmed.IsNumeric())
				{
					return FString::Printf(TEXT("'%s' is not a number (%s is %s)"),
						*Value, *Property->GetName(), *Property->GetCPPType());
				}
			}
			else if (CastField<FBoolProperty>(Property))
			{
				static const TCHAR* const Accepted[] = { TEXT("True"), TEXT("False"), TEXT("1"), TEXT("0"),
					TEXT("Yes"), TEXT("No") };
				bool bKnown = false;
				for (const TCHAR* Word : Accepted)
				{
					bKnown |= Trimmed.Equals(Word, ESearchCase::IgnoreCase);
				}
				if (!bKnown)
				{
					return FString::Printf(TEXT("'%s' is not a boolean (use True or False)"), *Value);
				}
			}
			return FString();
		}
	}

	bool IsNodeProperty(const FProperty* Property)
	{
		UClass* const Base = GetNodeBaseClass();
		if (!Property || !Base || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)
			|| HoldsPinReference(Property))
		{
			return false;
		}
		// Declared by a concrete Mutable node class: UEdGraphNode's own fields (pins, position, GUID)
		// have dedicated methods, and CustomizableObjectNode's are pin bookkeeping.
		const UClass* Owner = Property->GetOwnerClass();
		return Owner && Owner != Base && Owner->IsChildOf(Base);
	}

	FProperty* FindNodeProperty(UObject* Node, const FString& PropertyName, FString& OutError)
	{
		if (!Node)
		{
			OutError = TEXT("no node");
			return nullptr;
		}
		const FString Trimmed = PropertyName.TrimStartAndEnd();
		OutError = CheckNameLength(Trimmed, TEXT("PropertyName"));
		if (!OutError.IsEmpty())
		{
			return nullptr;
		}
		FProperty* Property = Trimmed.IsEmpty() ? nullptr : Node->GetClass()->FindPropertyByName(FName(*Trimmed));
		if (!Property)
		{
			OutError = FString::Printf(TEXT("%s has no property '%s'. get_node_properties lists them."),
				*Node->GetClass()->GetName(), *Trimmed);
			return nullptr;
		}
		// Identity, not authoring: bIsBase makes a second Base Object (the compiler needs exactly one),
		// and Identifier / NodeGuid-style ids are what other objects and states refer to.
		if (Property && (Property->GetFName() == TEXT("bIsBase") || Property->GetFName() == TEXT("Identifier")))
		{
			OutError = FString::Printf(TEXT("%s::%s is node identity and is not settable"),
				*Node->GetClass()->GetName(), *Trimmed);
			return nullptr;
		}
		if (!IsNodeProperty(Property))
		{
			OutError = FString::Printf(TEXT("%s::%s is not a node authoring property (transient, deprecated, "
				"or graph bookkeeping; pins and position have their own methods)"),
				*Node->GetClass()->GetName(), *Trimmed);
			return nullptr;
		}
		return Property;
	}

	FString ExportValue(const FProperty* Property, const UObject* Container)
	{
		FString Out;
		if (Property && Container)
		{
			// The container is passed as its own delta, which makes ExportText write every member
			// rather than only those differing from a default.
			Property->ExportText_InContainer(0, Out, Container, Container, const_cast<UObject*>(Container), PPF_None);
		}
		return Out;
	}

	FString ImportValue(FProperty* Property, UObject* Container, const FString& Value)
	{
		if (!Property || !Container)
		{
			return TEXT("no property or container");
		}
		const FString ScalarError = CheckScalarText(Property, Value);
		if (!ScalarError.IsEmpty())
		{
			return ScalarError;
		}

		// Value-level pre-image: ImportText applies struct/array literals member by member and stops
		// at the first bad one, so a failed import must be undone rather than left half-applied.
		void* const Snapshot = FMemory::Malloc(Property->GetSize(), Property->GetMinAlignment());
		Property->InitializeValue(Snapshot);
		Property->CopyCompleteValue(Snapshot, Property->ContainerPtrToValuePtr<void>(Container));
		ON_SCOPE_EXIT
		{
			Property->DestroyValue(Snapshot);
			FMemory::Free(Snapshot);
		};

		FStringOutputDevice ImportErrors;
		const TCHAR* Imported = Property->ImportText_InContainer(*Value, Container, Container, PPF_None, &ImportErrors);
		if (!Imported)
		{
			Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(Container), Snapshot);
			FString Error = FString::Printf(TEXT("could not parse '%s' as %s (%s::%s)"),
				*Value, *Property->GetCPPType(), *Container->GetClass()->GetName(), *Property->GetName());
			if (!ImportErrors.IsEmpty())
			{
				Error += TEXT(": ") + FString(ImportErrors).TrimStartAndEnd();
			}
			return Error;
		}
		return FString();
	}

	TArray<FVibeCOPropertyInfo> DescribeProperties(const UObject* Container)
	{
		TArray<FVibeCOPropertyInfo> Result;
		if (!Container)
		{
			return Result;
		}
		for (TFieldIterator<FProperty> It(Container->GetClass()); It; ++It)
		{
			const FProperty* Property = *It;
			if (!IsNodeProperty(Property))
			{
				continue;
			}
			FVibeCOPropertyInfo Info;
			Info.Name = Property->GetName();
			FString ExtendedType;
			Info.Type = Property->GetCPPType(&ExtendedType) + ExtendedType;
			Info.Value = ExportValue(Property, Container);
			Info.bEditable = Property->HasAnyPropertyFlags(CPF_Edit) && !Property->HasAnyPropertyFlags(CPF_EditConst);
			Info.DeclaredBy = Property->GetOwnerClass()->GetName();
			Result.Add(MoveTemp(Info));
		}
		return Result;
	}

	FString ReadPropertyText(const UObject* Container, const TCHAR* PropertyName)
	{
		const FProperty* Property = Container ? Container->GetClass()->FindPropertyByName(PropertyName) : nullptr;
		return Property ? ExportValue(Property, Container) : FString();
	}

	FString ReadObjectPath(const UObject* Container, const TCHAR* PropertyName)
	{
		const FProperty* Property = Container ? Container->GetClass()->FindPropertyByName(PropertyName) : nullptr;
		if (const FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property))
		{
			const FSoftObjectPtr& Ptr = *Soft->ContainerPtrToValuePtr<FSoftObjectPtr>(Container);
			return Ptr.IsNull() ? FString() : Ptr.ToSoftObjectPath().ToString();
		}
		if (const FObjectPropertyBase* Hard = CastField<FObjectPropertyBase>(Property))
		{
			const UObject* Value = Hard->GetObjectPropertyValue_InContainer(Container);
			return Value ? Value->GetPathName() : FString();
		}
		return FString();
	}

	// ---- Reflected calls ---------------------------------------------------------------------

	bool CallFunction(UObject* Target, const TCHAR* FunctionName, const TArray<FString>& Args,
		FString* OutReturn, FString& OutError)
	{
		UFunction* Function = Target ? Target->FindFunction(FName(FunctionName)) : nullptr;
		if (!Function)
		{
			OutError = FString::Printf(TEXT("%s has no function %s"),
				Target ? *Target->GetClass()->GetName() : TEXT("<null>"), FunctionName);
			return false;
		}

		const int32 Size = FMath::Max<int32>(Function->ParmsSize, 1);
		void* const Parms = FMemory::Malloc(Size, Function->GetMinAlignment());
		FMemory::Memzero(Parms, Size);
		Function->InitializeStruct(Parms);
		ON_SCOPE_EXIT
		{
			Function->DestroyStruct(Parms);
			FMemory::Free(Parms);
		};

		int32 ArgIndex = 0;
		FProperty* ReturnProperty = nullptr;
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				ReturnProperty = *It;
				continue;
			}
			// A pure out parameter takes no argument; const-ref and by-value parameters do.
			if (It->HasAnyPropertyFlags(CPF_OutParm) && !It->HasAnyPropertyFlags(CPF_ConstParm | CPF_ReferenceParm))
			{
				continue;
			}
			if (!Args.IsValidIndex(ArgIndex))
			{
				OutError = FString::Printf(TEXT("%s expects more than %d argument(s)"), FunctionName, Args.Num());
				return false;
			}
			if (!It->ImportText_Direct(*Args[ArgIndex], It->ContainerPtrToValuePtr<void>(Parms), nullptr, PPF_None))
			{
				OutError = FString::Printf(TEXT("%s: could not parse argument %d '%s' as %s"),
					FunctionName, ArgIndex, *Args[ArgIndex], *It->GetCPPType());
				return false;
			}
			++ArgIndex;
		}

		Target->ProcessEvent(Function, Parms);

		if (OutReturn)
		{
			OutReturn->Reset();
			if (ReturnProperty)
			{
				ReturnProperty->ExportTextItem_Direct(*OutReturn,
					ReturnProperty->ContainerPtrToValuePtr<void>(Parms), nullptr, nullptr, PPF_None);
			}
		}
		OutError.Reset();
		return true;
	}

	FString CallForText(UObject* Target, const TCHAR* FunctionName, const TArray<FString>& Args, const FString& Fallback)
	{
		FString Return;
		FString Error;
		return CallFunction(Target, FunctionName, Args, &Return, Error) ? Return : Fallback;
	}

	// ---- Compile -----------------------------------------------------------------------------

	namespace
	{
		/** Collects Mutable's log lines for the duration of one compile. */
		class FMutableLogCapture final : public FOutputDevice
		{
		public:
			struct FLine
			{
				ELogVerbosity::Type Verbosity;
				FString Category;
				FString Text;
			};

			virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
			{
				// "Mutable" is the message-log listing mirrored to the output log — where the
				// compiler's node-level errors arrive. The others are plain UE_LOG categories.
				static const FName Categories[] = { TEXT("Mutable"), TEXT("LogMutable"), TEXT("LogMutableCore"),
					TEXT("LogCustomizableObjectEditor") };
				bool bWanted = false;
				for (const FName& Wanted : Categories)
				{
					bWanted |= Category == Wanted;
				}
				if (!bWanted || !V)
				{
					return;
				}
				FScopeLock Lock(&Mutex);
				Lines.Add({ static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask),
					Category.ToString(), FString(V).TrimStartAndEnd() });
			}

			virtual bool CanBeUsedOnMultipleThreads() const override { return true; }
			virtual bool CanBeUsedOnAnyThread() const override { return true; }

			TArray<FLine> TakeLines()
			{
				FScopeLock Lock(&Mutex);
				return MoveTemp(Lines);
			}

		private:
			FCriticalSection Mutex;
			TArray<FLine> Lines;
		};

		bool SetStructMember(UScriptStruct* Struct, void* Data, const TCHAR* Member, const FString& Text, FString& OutError)
		{
			FProperty* Property = Struct->FindPropertyByName(Member);
			if (!Property)
			{
				OutError = FString::Printf(TEXT("FCompileParams has no member %s in this engine version"), Member);
				return false;
			}
			if (!Property->ImportText_Direct(*Text, Property->ContainerPtrToValuePtr<void>(Data), nullptr, PPF_None))
			{
				OutError = FString::Printf(TEXT("'%s' is not a valid value for %s"), *Text, Member);
				return false;
			}
			return true;
		}

		const TSharedPtr<IMessageLogListing> GetMutableListing()
		{
			if (!FModuleManager::Get().IsModuleLoaded(TEXT("MessageLog")))
			{
				return nullptr;
			}
			FMessageLogModule& MessageLog = FModuleManager::GetModuleChecked<FMessageLogModule>(TEXT("MessageLog"));
			if (!MessageLog.IsRegisteredLogListing(TEXT("Mutable")))
			{
				return nullptr;
			}
			return MessageLog.GetLogListing(TEXT("Mutable"));
		}
	}

	void Compile(UObject* Object, const FString& OptimizationLevel, const FString& TextureCompression,
		FVibeCOCompileResult& OutResult)
	{
		OutResult.State = TEXT("Failed");

		UFunction* Function = Object ? Object->FindFunction(TEXT("Compile")) : nullptr;
		FStructProperty* ParamsProperty = nullptr;
		if (Function)
		{
			TFieldIterator<FProperty> FirstParameter(Function);
			if (FirstParameter && FirstParameter->HasAnyPropertyFlags(CPF_Parm))
			{
				ParamsProperty = CastField<FStructProperty>(*FirstParameter);
			}
		}
		if (!ParamsProperty)
		{
			OutResult.Error = TEXT("UCustomizableObject::Compile(FCompileParams) was not found; this Mutable "
								   "version is not supported");
			return;
		}

		void* const Parms = FMemory::Malloc(FMath::Max<int32>(Function->ParmsSize, 1), Function->GetMinAlignment());
		FMemory::Memzero(Parms, FMath::Max<int32>(Function->ParmsSize, 1));
		Function->InitializeStruct(Parms);
		ON_SCOPE_EXIT
		{
			Function->DestroyStruct(Parms);
			FMemory::Free(Parms);
		};

		UScriptStruct* const Struct = ParamsProperty->Struct;
		void* const ParamsData = ParamsProperty->ContainerPtrToValuePtr<void>(Parms);
		// bSkipIfNotOutOfDate defaults to true and judges "out of date" from SAVED package hashes, so
		// in-memory edits compile to "skipped" and the previous model is reported as current.
		FString Error;
		if (!SetStructMember(Struct, ParamsData, TEXT("bAsync"), TEXT("False"), Error)
			|| !SetStructMember(Struct, ParamsData, TEXT("bSkipIfNotOutOfDate"), TEXT("False"), Error)
			|| !SetStructMember(Struct, ParamsData, TEXT("bSkipIfCompiled"), TEXT("False"), Error)
			|| !SetStructMember(Struct, ParamsData, TEXT("OptimizationLevel"), OptimizationLevel, Error)
			|| !SetStructMember(Struct, ParamsData, TEXT("TextureCompression"), TextureCompression, Error))
		{
			OutResult.Error = Error + TEXT(" (OptimizationLevel: None | Maximum | FromCustomizableObject; "
										   "TextureCompression: None | Fast | HighQuality)");
			return;
		}

		// The "Mutable" message-log listing is the compiler's own record; it backs up the output-log
		// capture in case a listing was registered without mirroring.
		const TSharedPtr<IMessageLogListing> ListingBefore = GetMutableListing();
		const int32 ListingCountBefore = ListingBefore.IsValid() ? ListingBefore->GetFilteredMessages().Num() : 0;

		FMutableLogCapture Capture;
		GLog->AddOutputDevice(&Capture);
		Object->ProcessEvent(Function, Parms);
		GLog->FlushThreadedLogs();
		GLog->RemoveOutputDevice(&Capture);

		for (const FMutableLogCapture::FLine& Line : Capture.TakeLines())
		{
			OutResult.Log.Add(FString::Printf(TEXT("%s: %s"), *Line.Category, *Line.Text));
			if (Line.Verbosity == ELogVerbosity::Error || Line.Verbosity == ELogVerbosity::Fatal)
			{
				OutResult.Errors.AddUnique(Line.Text);
			}
			else if (Line.Verbosity == ELogVerbosity::Warning)
			{
				OutResult.Warnings.AddUnique(Line.Text);
			}
		}

		if (const TSharedPtr<IMessageLogListing> Listing = GetMutableListing())
		{
			const TArray<TSharedRef<FTokenizedMessage>>& Messages = Listing->GetFilteredMessages();
			// A listing that shrank started a new page during the compile: read all of it.
			const int32 First = (Listing == ListingBefore && Messages.Num() >= ListingCountBefore) ? ListingCountBefore : 0;
			for (int32 Index = First; Index < Messages.Num(); ++Index)
			{
				const FString Text = Messages[Index]->ToText().ToString().TrimStartAndEnd();
				const EMessageSeverity::Type Severity = Messages[Index]->GetSeverity();
				if (Severity == EMessageSeverity::Error)
				{
					OutResult.Errors.AddUnique(Text);
				}
				else if (Severity == EMessageSeverity::Warning || Severity == EMessageSeverity::PerformanceWarning)
				{
					OutResult.Warnings.AddUnique(Text);
				}
			}
		}

		OutResult.bIsCompiled = CallForText(Object, TEXT("IsCompiled")).Equals(TEXT("True"), ESearchCase::IgnoreCase);
		OutResult.bSuccess = OutResult.bIsCompiled && OutResult.Errors.Num() == 0;
		OutResult.State = OutResult.bSuccess ? TEXT("Completed") : TEXT("Failed");
	}
}
