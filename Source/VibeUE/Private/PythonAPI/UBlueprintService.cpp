// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UBlueprintService.h"
#include "PythonAPI/BlueprintTypeParser.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"       // For WBP widget component class discovery
#include "EditorAssetLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Logging/TokenizedMessage.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallArrayFunction.h"   // For array library functions with wildcard pins
#include "K2Node_GetArrayItem.h"        // For Array Get (replaces deprecated Array_Get)
#include "K2Node_CreateDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_EnhancedInputAction.h"  // For Enhanced Input Action event nodes
#include "K2Node_AddDelegate.h"          // For delegate bind nodes (add_delegate_bind_node)
#include "K2Node_CreateDelegate.h"       // For create event nodes (add_create_delegate_node)
#include "K2Node_MakeStruct.h"           // For STRUCT key: creating typed struct Make nodes
#include "InputAction.h"                 // For UInputAction
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "EdGraphSchema_K2.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "Factories/BlueprintFactory.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "SubobjectDataSubsystem.h"
// For BlueprintActionDatabase - proper node discovery
#include "BlueprintActionDatabase.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintVariableNodeSpawner.h"
#include "BlueprintEventNodeSpawner.h"
// For OpenFunctionGraph - Blueprint Editor navigation
#include "Subsystems/AssetEditorSubsystem.h"
#include "BlueprintEditor.h"
// For FScopedTransaction (undo support)
#include "ScopedTransaction.h"

namespace
{
	static UEdGraph* ResolveBlueprintGraph(UBlueprint* Blueprint, const FString& GraphName)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		if (GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
		{
			for (UEdGraph* UberGraph : Blueprint->UbergraphPages)
			{
				if (UberGraph && UberGraph->GetFName() == UEdGraphSchema_K2::GN_EventGraph)
				{
					return UberGraph;
				}
			}
		}

		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);

		for (UEdGraph* Graph : Graphs)
		{
			if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
			{
				return Graph;
			}
		}

		return nullptr;
	}

	static UClass* ResolveClassByName(const FString& ClassName)
	{
		if (ClassName.IsEmpty())
		{
			return nullptr;
		}

		if (UClass* FoundClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::ExactClass))
		{
			return FoundClass;
		}

		if (!ClassName.StartsWith(TEXT("U"), ESearchCase::CaseSensitive))
		{
			if (UClass* FoundClass = FindFirstObject<UClass>(*FString::Printf(TEXT("U%s"), *ClassName), EFindFirstObjectOptions::ExactClass))
			{
				return FoundClass;
			}
		}

		if (!ClassName.StartsWith(TEXT("A"), ESearchCase::CaseSensitive))
		{
			if (UClass* FoundClass = FindFirstObject<UClass>(*FString::Printf(TEXT("A%s"), *ClassName), EFindFirstObjectOptions::ExactClass))
			{
				return FoundClass;
			}
		}

		if (UClass* FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ClassName)))
		{
			return FoundClass;
		}

		if (!ClassName.StartsWith(TEXT("A"), ESearchCase::CaseSensitive))
		{
			if (UClass* FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.A%s"), *ClassName)))
			{
				return FoundClass;
			}
		}

		if (!ClassName.StartsWith(TEXT("U"), ESearchCase::CaseSensitive))
		{
			if (UClass* FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.U%s"), *ClassName)))
			{
				return FoundClass;
			}
		}

		return nullptr;
	}

	// Loads a UScriptStruct by asset path. Accepts both full paths ("/Game/X/Foo.Foo")
	// and package-only paths ("/Game/X/Foo") by auto-appending the asset name suffix.
	static UScriptStruct* LoadStructByPath(const FString& StructPath)
	{
		if (StructPath.IsEmpty()) return nullptr;

		// Try the path as-is first (handles "/Script/Engine.HitResult" and "/Game/X/Foo.Foo")
		if (UScriptStruct* Found = LoadObject<UScriptStruct>(nullptr, *StructPath))
		{
			return Found;
		}

		// If no dot suffix, auto-append the last path component as the asset name
		// e.g. "/Game/StateTree/FStartChasingPayload" -> "/Game/StateTree/FStartChasingPayload.FStartChasingPayload"
		if (!StructPath.Contains(TEXT(".")))
		{
			FString AssetName;
			StructPath.Split(TEXT("/"), nullptr, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (!AssetName.IsEmpty())
			{
				if (UScriptStruct* Found = LoadObject<UScriptStruct>(nullptr, *FString::Printf(TEXT("%s.%s"), *StructPath, *AssetName)))
				{
					return Found;
				}
			}
		}

		return nullptr;
	}

	static FString NormalizeBlueprintNodeSearchText(const FString& Text)
	{
		FString Normalized;
		Normalized.Reserve(Text.Len());

		for (const TCHAR Character : Text)
		{
			if (FChar::IsAlnum(Character))
			{
				Normalized.AppendChar(FChar::ToLower(Character));
			}
		}

		return Normalized;
	}

	static bool MatchesBlueprintFunctionSearch(const FString& RequestedName, const FString& CandidateName)
	{
		const FString NormalizedRequested = NormalizeBlueprintNodeSearchText(RequestedName);
		const FString NormalizedCandidate = NormalizeBlueprintNodeSearchText(CandidateName);

		if (NormalizedRequested.IsEmpty() || NormalizedCandidate.IsEmpty())
		{
			return false;
		}

		if (NormalizedRequested == NormalizedCandidate)
		{
			return true;
		}

		if (NormalizedCandidate.StartsWith(TEXT("k2")) && NormalizedCandidate.RightChop(2) == NormalizedRequested)
		{
			return true;
		}

		if (NormalizedRequested.StartsWith(TEXT("k2")) && NormalizedRequested.RightChop(2) == NormalizedCandidate)
		{
			return true;
		}

		return false;
	}

	static UBlueprintFunctionNodeSpawner* FindBestFunctionSpawner(
		UBlueprint* Blueprint,
		UEdGraph* UiGraph,
		UClass* OwnerClass,
		const FString& RequestedFunctionName)
	{
		if (!Blueprint || !OwnerClass || RequestedFunctionName.IsEmpty())
		{
			return nullptr;
		}

		const FString NormalizedRequested = NormalizeBlueprintNodeSearchText(RequestedFunctionName);
		if (NormalizedRequested.IsEmpty())
		{
			return nullptr;
		}

		UBlueprintFunctionNodeSpawner* BestSpawner = nullptr;
		int32 BestScore = MIN_int32;

		const FBlueprintActionDatabase::FActionRegistry& ActionRegistry = FBlueprintActionDatabase::Get().GetAllActions();
		for (const TPair<FObjectKey, FBlueprintActionDatabase::FActionList>& Entry : ActionRegistry)
		{
			for (UBlueprintNodeSpawner* NodeSpawner : Entry.Value)
			{
				UBlueprintFunctionNodeSpawner* FunctionSpawner = Cast<UBlueprintFunctionNodeSpawner>(NodeSpawner);
				if (!FunctionSpawner)
				{
					continue;
				}

				const UFunction* CandidateFunction = FunctionSpawner->GetFunction();
				if (!CandidateFunction || !CandidateFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable))
				{
					continue;
				}

				UClass* CandidateOwnerClass = CandidateFunction->GetOwnerClass();
				if (!CandidateOwnerClass)
				{
					continue;
				}

				const bool bExactOwnerMatch = CandidateOwnerClass == OwnerClass;
				const bool bInheritedOwnerMatch = OwnerClass->IsChildOf(CandidateOwnerClass);
				if (!bExactOwnerMatch && !bInheritedOwnerMatch)
				{
					continue;
				}

				const FBlueprintActionUiSpec& UiSpec = FunctionSpawner->PrimeDefaultUiSpec(UiGraph);
				const FString DisplayName = UiSpec.MenuName.ToString();
				const FString Tooltip = UiSpec.Tooltip.ToString();
				const FString Keywords = UiSpec.Keywords.ToString();

				int32 Score = bExactOwnerMatch ? 20 : 10;
				if (MatchesBlueprintFunctionSearch(RequestedFunctionName, CandidateFunction->GetName()))
				{
					Score += 90;
				}
				if (MatchesBlueprintFunctionSearch(RequestedFunctionName, CandidateFunction->GetDisplayNameText().ToString()))
				{
					Score += 85;
				}
				if (MatchesBlueprintFunctionSearch(RequestedFunctionName, DisplayName))
				{
					Score += 100;
				}

				const FString NormalizedKeywords = NormalizeBlueprintNodeSearchText(Keywords);
				const FString NormalizedTooltip = NormalizeBlueprintNodeSearchText(Tooltip);
				if (!NormalizedKeywords.IsEmpty() && NormalizedKeywords.Contains(NormalizedRequested))
				{
					Score += 25;
				}
				if (!NormalizedTooltip.IsEmpty() && NormalizedTooltip.Contains(NormalizedRequested))
				{
					Score += 10;
				}

				if (Score > BestScore)
				{
					BestScore = Score;
					BestSpawner = FunctionSpawner;
				}
			}
		}

		return BestScore > 20 ? BestSpawner : nullptr;
	}

	static FString BuildEventSpawnerKey(const UBlueprintEventNodeSpawner* EventSpawner)
	{
		if (!EventSpawner)
		{
			return FString();
		}

		if (EventSpawner->IsForCustomEvent())
		{
			return TEXT("EVENT CUSTOM");
		}

		const UFunction* EventFunction = EventSpawner->GetEventFunction();
		if (!EventFunction || !EventFunction->GetOwnerClass())
		{
			return FString();
		}

		return FString::Printf(TEXT("EVENT %s::%s"), *EventFunction->GetOwnerClass()->GetName(), *EventFunction->GetName());
	}
}

UBlueprint* UBlueprintService::LoadBlueprint(const FString& BlueprintPath)
{
	if (BlueprintPath.IsEmpty())
	{
		return nullptr;
	}

	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(BlueprintPath);
	return Cast<UBlueprint>(LoadedObject);
}

bool UBlueprintService::GetBlueprintInfo(const FString& BlueprintPath, FBlueprintDetailedInfo& OutInfo)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBlueprintService::GetBlueprintInfo: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	OutInfo.BlueprintName = Blueprint->GetName();
	OutInfo.BlueprintPath = BlueprintPath;
	OutInfo.bIsWidgetBlueprint = Blueprint->IsA<UWidgetBlueprint>();

	// Get parent class
	if (UClass* ParentClass = Blueprint->ParentClass)
	{
		OutInfo.ParentClass = ParentClass->GetName();
	}

	// Get variables
	OutInfo.Variables = ListVariables(BlueprintPath);

	// Get functions
	OutInfo.Functions = ListFunctions(BlueprintPath);

	// Get components
	OutInfo.Components = ListComponents(BlueprintPath);

	return true;
}

TArray<FBlueprintVariableInfo> UBlueprintService::ListVariables(const FString& BlueprintPath)
{
	TArray<FBlueprintVariableInfo> Variables;

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return Variables;
	}

	for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables)
	{
		FBlueprintVariableInfo VarInfo;
		VarInfo.VariableName = VarDesc.VarName.ToString();
		VarInfo.VariableType = FBlueprintTypeParser::GetFriendlyTypeName(VarDesc.VarType);
		VarInfo.Category = VarDesc.Category.ToString();
		VarInfo.bIsPublic = (VarDesc.PropertyFlags & CPF_DisableEditOnInstance) == 0;
		VarInfo.bIsExposed = (VarDesc.PropertyFlags & CPF_ExposeOnSpawn) != 0;
		VarInfo.DefaultValue = VarDesc.DefaultValue;

		Variables.Add(VarInfo);
	}

	return Variables;
}

TArray<FBlueprintFunctionInfo> UBlueprintService::ListFunctions(const FString& BlueprintPath)
{
	TArray<FBlueprintFunctionInfo> Functions;

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return Functions;
	}

	// First, enumerate from compiled GeneratedClass (provides full type info)
	if (UClass* GeneratedClass = Blueprint->GeneratedClass)
	{
		for (TFieldIterator<UFunction> FuncIt(GeneratedClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
		{
			UFunction* Function = *FuncIt;
			if (!Function)
			{
				continue;
			}

			FBlueprintFunctionInfo FuncInfo;
			FuncInfo.FunctionName = Function->GetName();
			FuncInfo.bIsPure = Function->HasAnyFunctionFlags(FUNC_BlueprintPure);

			UFunction* SuperFunc = Function->GetSuperFunction();
			FuncInfo.bIsOverride = (SuperFunc != nullptr);

			for (TFieldIterator<FProperty> PropIt(Function); PropIt; ++PropIt)
			{
				FProperty* Prop = *PropIt;
				if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					FuncInfo.ReturnType = Prop->GetCPPType();
				}
				else if (Prop->HasAnyPropertyFlags(CPF_Parm))
				{
					FString ParamStr = FString::Printf(TEXT("%s: %s"), *Prop->GetName(), *Prop->GetCPPType());
					FuncInfo.Parameters.Add(ParamStr);
				}
			}

			if (FuncInfo.ReturnType.IsEmpty())
			{
				FuncInfo.ReturnType = TEXT("void");
			}

			Functions.Add(FuncInfo);
		}
	}

	// Also enumerate FunctionGraphs to catch functions added since last compile
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		const FString GraphName = Graph->GetName();

		// Skip if already found in the compiled class
		const bool bAlreadyFound = Functions.ContainsByPredicate([&GraphName](const FBlueprintFunctionInfo& F)
		{
			return F.FunctionName == GraphName;
		});

		if (!bAlreadyFound)
		{
			FBlueprintFunctionInfo FuncInfo;
			FuncInfo.FunctionName = GraphName;
			FuncInfo.bIsPure = false;
			FuncInfo.bIsOverride = false;
			FuncInfo.ReturnType = TEXT("void");
			Functions.Add(FuncInfo);
		}
	}

	return Functions;
}

bool UBlueprintService::OpenFunctionGraph(const FString& BlueprintPath, const FString& FunctionName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Warning, TEXT("OpenFunctionGraph: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Get the asset editor subsystem
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("OpenFunctionGraph: AssetEditorSubsystem not available"));
		return false;
	}

	// Open the blueprint in the editor
	if (!AssetEditorSubsystem->OpenEditorForAsset(Blueprint))
	{
		UE_LOG(LogTemp, Warning, TEXT("OpenFunctionGraph: Failed to open editor for blueprint"));
		return false;
	}

	// Get the blueprint editor instance
	IAssetEditorInstance* EditorInstance = AssetEditorSubsystem->FindEditorForAsset(Blueprint, false);
	FBlueprintEditor* BlueprintEditor = static_cast<FBlueprintEditor*>(EditorInstance);
	if (!BlueprintEditor)
	{
		UE_LOG(LogTemp, Warning, TEXT("OpenFunctionGraph: Could not get blueprint editor instance"));
		return false;
	}

	// Find the target graph
	UEdGraph* TargetGraph = nullptr;
	FString FunctionLower = FunctionName.ToLower();

	// Check if it's the EventGraph (uber graph)
	if (FunctionLower == TEXT("eventgraph") || FunctionLower == TEXT("event graph") || FunctionName.IsEmpty())
	{
		if (Blueprint->UbergraphPages.Num() > 0)
		{
			TargetGraph = Blueprint->UbergraphPages[0];
		}
	}
	else
	{
		// Search function graphs
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph && Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
			{
				TargetGraph = Graph;
				break;
			}
		}

		// If not found, also check uber graphs by name (some graphs might be there)
		if (!TargetGraph)
		{
			for (UEdGraph* Graph : Blueprint->UbergraphPages)
			{
				if (Graph && Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
				{
					TargetGraph = Graph;
					break;
				}
			}
		}
	}

	if (!TargetGraph)
	{
		UE_LOG(LogTemp, Warning, TEXT("OpenFunctionGraph: Could not find graph '%s' in blueprint"), *FunctionName);
		return false;
	}

	// Open the graph in the editor
	BlueprintEditor->OpenDocument(TargetGraph, FDocumentTracker::OpenNewDocument);

	UE_LOG(LogTemp, Log, TEXT("OpenFunctionGraph: Opened graph '%s' in blueprint '%s'"), *TargetGraph->GetName(), *BlueprintPath);
	return true;
}

TArray<FBlueprintComponentInfo> UBlueprintService::ListComponents(const FString& BlueprintPath)
{
	TArray<FBlueprintComponentInfo> Components;

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return Components;
	}

	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	if (!SCS)
	{
		return Components;
	}

	const TArray<USCS_Node*>& AllNodes = SCS->GetAllNodes();
	for (USCS_Node* Node : AllNodes)
	{
		if (!Node)
		{
			continue;
		}

		FBlueprintComponentInfo CompInfo;
		CompInfo.ComponentName = Node->GetVariableName().ToString();

		if (UClass* ComponentClass = Node->ComponentClass)
		{
			CompInfo.ComponentClass = ComponentClass->GetName();
			CompInfo.bIsSceneComponent = ComponentClass->IsChildOf<USceneComponent>();
		}

		if (Node->ParentComponentOrVariableName != NAME_None)
		{
			CompInfo.AttachParent = Node->ParentComponentOrVariableName.ToString();
		}

		CompInfo.bIsRootComponent = (Node == SCS->GetDefaultSceneRootNode());

		// Get children
		for (USCS_Node* ChildNode : Node->GetChildNodes())
		{
			if (ChildNode)
			{
				CompInfo.Children.Add(ChildNode->GetVariableName().ToString());
			}
		}

		Components.Add(CompInfo);
	}

	// Also expose inherited/native components from the parent C++ class.
	// These are the "grayed out" components the Blueprint Editor shows but that are NOT SCS nodes.
	// The AI must know they exist; it cannot delete them — it should call set_root_component() instead.
	if (Blueprint->ParentClass)
	{
		UObject* ParentCDO = Blueprint->ParentClass->GetDefaultObject(false);
		if (AActor* ParentActor = Cast<AActor>(ParentCDO))
		{
			TArray<UActorComponent*> NativeComponents;
			ParentActor->GetComponents(NativeComponents, false);

			// Build a set of SCS component names to avoid duplicates
			TSet<FString> SCSNames;
			for (const FBlueprintComponentInfo& C : Components)
			{
				SCSNames.Add(C.ComponentName);
			}

			for (UActorComponent* NativeComp : NativeComponents)
			{
				if (!NativeComp)
				{
					continue;
				}

				FString NativeName = NativeComp->GetName();
				if (SCSNames.Contains(NativeName))
				{
					continue;
				}

				FBlueprintComponentInfo InheritedInfo;
				InheritedInfo.ComponentName  = NativeName;
				InheritedInfo.ComponentClass = NativeComp->GetClass()->GetName();
				InheritedInfo.bIsSceneComponent = NativeComp->IsA<USceneComponent>();
				InheritedInfo.bIsInherited   = true;

				if (USceneComponent* NativeSC = Cast<USceneComponent>(NativeComp))
				{
					if (ParentActor->GetRootComponent() == NativeSC)
					{
						InheritedInfo.bIsRootComponent = true;
					}
				}

				Components.Add(InheritedInfo);
			}
		}
	}

	return Components;
}

TArray<FBlueprintComponentInfo> UBlueprintService::GetComponentHierarchy(const FString& BlueprintPath)
{
	// For simplicity, return the same as ListComponents
	// Could be enhanced to build a proper hierarchy tree
	return ListComponents(BlueprintPath);
}

FString UBlueprintService::GetParentClass(const FString& BlueprintPath)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint || !Blueprint->ParentClass)
	{
		return FString();
	}

	return Blueprint->ParentClass->GetName();
}

bool UBlueprintService::IsWidgetBlueprint(const FString& BlueprintPath)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return false;
	}

	return Blueprint->IsA<UWidgetBlueprint>();
}

// ============================================================================
// COMPONENT MANAGEMENT (manage_blueprint_component actions)
// ============================================================================

TArray<FComponentTypeInfo> UBlueprintService::GetAvailableComponents(const FString& SearchFilter, int32 MaxResults)
{
	TArray<FComponentTypeInfo> Results;
	
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		
		// Only include ActorComponent classes
		if (!Class->IsChildOf<UActorComponent>())
		{
			continue;
		}
		
		// Skip abstract, deprecated, hidden classes
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Hidden))
		{
			continue;
		}
		
		// Apply search filter
		if (!SearchFilter.IsEmpty())
		{
			FString ClassName = Class->GetName();
			FString DisplayName = Class->GetDisplayNameText().ToString();
			if (!ClassName.Contains(SearchFilter, ESearchCase::IgnoreCase) &&
				!DisplayName.Contains(SearchFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}
		
		FComponentTypeInfo Info;
		Info.Name = Class->GetName();
		Info.DisplayName = Class->GetDisplayNameText().ToString();
		Info.ClassPath = Class->GetPathName();
		Info.bIsSceneComponent = Class->IsChildOf<USceneComponent>();
		Info.bIsPrimitiveComponent = Class->IsChildOf<UPrimitiveComponent>();
		Info.bIsAbstract = Class->HasAnyClassFlags(CLASS_Abstract);
		
		// Get category from metadata
		if (const FString* CategoryMeta = Class->FindMetaData(TEXT("Category")))
		{
			Info.Category = *CategoryMeta;
		}
		else
		{
			Info.Category = TEXT("Miscellaneous");
		}
		
		// Get base class
		if (UClass* SuperClass = Class->GetSuperClass())
		{
			Info.BaseClass = SuperClass->GetName();
		}
		
		Results.Add(Info);
		
		// Limit results
		if (MaxResults > 0 && Results.Num() >= MaxResults)
		{
			break;
		}
	}
	
	// Sort by name
	Results.Sort([](const FComponentTypeInfo& A, const FComponentTypeInfo& B) {
		return A.Name < B.Name;
	});
	
	return Results;
}

bool UBlueprintService::GetComponentInfo(const FString& ComponentType, FComponentDetailedInfo& OutInfo)
{
	// Find the class
	UClass* ComponentClass = nullptr;
	
	// Try to find by exact name first
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (Class->IsChildOf<UActorComponent>())
		{
			if (Class->GetName() == ComponentType || Class->GetName() == ComponentType + TEXT("Component"))
			{
				ComponentClass = Class;
				break;
			}
		}
	}
	
	// Try by path
	if (!ComponentClass)
	{
		ComponentClass = FindObject<UClass>(nullptr, *ComponentType);
	}
	
	if (!ComponentClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetComponentInfo: Component type not found: %s"), *ComponentType);
		return false;
	}
	
	OutInfo.Name = ComponentClass->GetName();
	OutInfo.DisplayName = ComponentClass->GetDisplayNameText().ToString();
	OutInfo.ClassPath = ComponentClass->GetPathName();
	OutInfo.bIsSceneComponent = ComponentClass->IsChildOf<USceneComponent>();
	OutInfo.bIsPrimitiveComponent = ComponentClass->IsChildOf<UPrimitiveComponent>();
	
	// Get category
	if (const FString* CategoryMeta = ComponentClass->FindMetaData(TEXT("Category")))
	{
		OutInfo.Category = *CategoryMeta;
	}
	
	// Get parent class
	if (UClass* SuperClass = ComponentClass->GetSuperClass())
	{
		OutInfo.ParentClass = SuperClass->GetName();
	}
	
	// Count properties and functions
	OutInfo.PropertyCount = 0;
	OutInfo.FunctionCount = 0;
	
	for (TFieldIterator<FProperty> PropIt(ComponentClass); PropIt; ++PropIt)
	{
		if (PropIt->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
		{
			OutInfo.PropertyCount++;
		}
	}
	
	for (TFieldIterator<UFunction> FuncIt(ComponentClass); FuncIt; ++FuncIt)
	{
		if (FuncIt->HasAnyFunctionFlags(FUNC_BlueprintCallable))
		{
			OutInfo.FunctionCount++;
		}
	}
	
	return true;
}

bool UBlueprintService::AddComponent(
	const FString& BlueprintPath,
	const FString& ComponentType,
	const FString& ComponentName,
	const FString& ParentName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddComponent: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}
	
	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	if (!SCS)
	{
		UE_LOG(LogTemp, Error, TEXT("AddComponent: Blueprint has no SCS: %s"), *BlueprintPath);
		return false;
	}
	
	// Find component class
	UClass* ComponentClass = nullptr;
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (Class->IsChildOf<UActorComponent>())
		{
			if (Class->GetName() == ComponentType || 
				Class->GetName() == ComponentType + TEXT("Component") ||
				Class->GetPathName() == ComponentType)
			{
				if (!Class->HasAnyClassFlags(CLASS_Abstract))
				{
					ComponentClass = Class;
					break;
				}
			}
		}
	}
	
	if (!ComponentClass)
	{
		UE_LOG(LogTemp, Error, TEXT("AddComponent: Component type not found or abstract: %s"), *ComponentType);
		return false;
	}
	
	// Check for duplicate name
	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			UE_LOG(LogTemp, Warning, TEXT("AddComponent: Component '%s' already exists"), *ComponentName);
			return false;
		}
	}
	
	// Create new SCS node
	USCS_Node* NewNode = SCS->CreateNode(ComponentClass, FName(*ComponentName));
	if (!NewNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AddComponent: Failed to create SCS node for %s"), *ComponentType);
		return false;
	}
	
	// Attach to parent if specified
	if (!ParentName.IsEmpty())
	{
		USCS_Node* ParentNode = nullptr;
		for (USCS_Node* Node : SCS->GetAllNodes())
		{
			if (Node && Node->GetVariableName().ToString() == ParentName)
			{
				ParentNode = Node;
				break;
			}
		}
		
		if (ParentNode)
		{
			ParentNode->AddChildNode(NewNode);
			// CRITICAL: Call SetParent to properly set ParentComponentOrVariableName
			// AddChildNode only manages the ChildNodes array, it does NOT set the parent reference
			NewNode->SetParent(ParentNode);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AddComponent: Parent '%s' not found, adding to root"), *ParentName);
			SCS->AddNode(NewNode);
		}
	}
	else
	{
		// Add to root
		SCS->AddNode(NewNode);
	}
	
	// Mark blueprint as modified and recompile so the Blueprint Editor viewport refreshes immediately.
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("AddComponent: Added '%s' of type '%s' to %s"), *ComponentName, *ComponentType, *BlueprintPath);
	return true;
}

bool UBlueprintService::RemoveComponent(
	const FString& BlueprintPath,
	const FString& ComponentName,
	bool bRemoveChildren)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveComponent: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}
	
	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	if (!SCS)
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveComponent: Blueprint has no SCS: %s"), *BlueprintPath);
		return false;
	}
	
	// Find the node to remove
	USCS_Node* NodeToRemove = nullptr;
	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			NodeToRemove = Node;
			break;
		}
	}
	
	if (!NodeToRemove)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveComponent: Component '%s' not found"), *ComponentName);
		return false;
	}
	
	// If removing children, recursively remove them first
	if (bRemoveChildren)
	{
		TArray<USCS_Node*> ChildNodes = NodeToRemove->GetChildNodes();
		for (USCS_Node* Child : ChildNodes)
		{
			if (Child)
			{
				FString ChildName = Child->GetVariableName().ToString();
				// Recursively remove each child with its descendants
				RemoveComponent(BlueprintPath, ChildName, true);
			}
		}
	}
	else
	{
		// If not removing children, reparent them first
		TArray<USCS_Node*> ChildNodes = NodeToRemove->GetChildNodes();
		USCS_Node* ParentNode = nullptr;
		
		// Find parent of the node being removed
		for (USCS_Node* Node : SCS->GetAllNodes())
		{
			if (Node)
			{
				for (USCS_Node* Child : Node->GetChildNodes())
				{
					if (Child == NodeToRemove)
					{
						ParentNode = Node;
						break;
					}
				}
			}
		}
		
		// Move children to parent or root
		for (USCS_Node* Child : ChildNodes)
		{
			NodeToRemove->RemoveChildNode(Child);
			if (ParentNode)
			{
				ParentNode->AddChildNode(Child);
			}
			else
			{
				SCS->AddNode(Child);
			}
		}
	}
	
	// Remove the node
	SCS->RemoveNode(NodeToRemove);
	
	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	
	UE_LOG(LogTemp, Log, TEXT("RemoveComponent: Removed '%s' from %s"), *ComponentName, *BlueprintPath);
	return true;
}

// Helper to find component template in blueprint
static UActorComponent* FindComponentTemplate(UBlueprint* Blueprint, const FString& ComponentName)
{
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		return nullptr;
	}
	
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			return Node->ComponentTemplate;
		}
	}
	
	return nullptr;
}

bool UBlueprintService::GetComponentProperty(
	const FString& BlueprintPath,
	const FString& ComponentName,
	const FString& PropertyName,
	FString& OutValue)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("GetComponentProperty: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}
	
	UActorComponent* Component = FindComponentTemplate(Blueprint, ComponentName);
	if (!Component)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetComponentProperty: Component '%s' not found"), *ComponentName);
		return false;
	}
	
	// Find the property
	FProperty* Property = Component->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetComponentProperty: Property '%s' not found on component '%s'"), *PropertyName, *ComponentName);
		return false;
	}
	
	// Get value as string
	const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Component);
	Property->ExportTextItem_Direct(OutValue, ValuePtr, nullptr, Component, PPF_None);
	
	return true;
}

bool UBlueprintService::SetComponentProperty(
	const FString& BlueprintPath,
	const FString& ComponentName,
	const FString& PropertyName,
	const FString& PropertyValue)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SetComponentProperty: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}
	
	UActorComponent* Component = FindComponentTemplate(Blueprint, ComponentName);
	if (!Component)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetComponentProperty: Component '%s' not found"), *ComponentName);
		return false;
	}
	
	// Find the property
	FProperty* Property = Component->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetComponentProperty: Property '%s' not found on component '%s'"), *PropertyName, *ComponentName);
		return false;
	}
	
	// Set value from string
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Component);

	// Mark component and blueprint as modified before making changes
	Component->PreEditChange(Property);
	Component->Modify();
	Blueprint->Modify();

	// Special handling: struct properties wrapping asset references (e.g. StateTreeReference).
	// When the value looks like an asset path ("/Game/..."), ImportText_Direct expects the full
	// struct-text format "(FieldName=Value)" and silently does nothing with a bare path.
	// Instead, find the first soft-object or object property inside the struct and set it directly.
	bool bHandledAsStruct = false;
	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		const bool bLooksLikePath = PropertyValue.StartsWith(TEXT("/"));
		if (bLooksLikePath)
		{
			for (TFieldIterator<FProperty> It(StructProp->Struct); It; ++It)
			{
				if (FSoftObjectProperty* SoftProp = CastField<FSoftObjectProperty>(*It))
				{
					void* InnerPtr = SoftProp->ContainerPtrToValuePtr<void>(ValuePtr);
					FSoftObjectPath SoftPath(PropertyValue);
					FSoftObjectPtr SoftRef(SoftPath);
					SoftProp->SetPropertyValue(InnerPtr, SoftRef);
					bHandledAsStruct = true;
					break;
				}
				if (FObjectProperty* ObjProp = CastField<FObjectProperty>(*It))
				{
					void* InnerPtr = ObjProp->ContainerPtrToValuePtr<void>(ValuePtr);
					UObject* Loaded = StaticLoadObject(ObjProp->PropertyClass, nullptr, *PropertyValue);
					if (Loaded)
					{
						ObjProp->SetObjectPropertyValue(InnerPtr, Loaded);
						bHandledAsStruct = true;
					}
					break;
				}
			}
			if (!bHandledAsStruct)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("SetComponentProperty: No inner soft-object/object property found in struct '%s' for path '%s'. Falling back to ImportText."),
					*StructProp->Struct->GetName(), *PropertyValue);
			}
		}
	}

	if (!bHandledAsStruct)
	{
		if (!Property->ImportText_Direct(*PropertyValue, ValuePtr, Component, PPF_None))
		{
			UE_LOG(LogTemp, Error, TEXT("SetComponentProperty: Failed to set property '%s' to '%s'"), *PropertyName, *PropertyValue);
			return false;
		}

		// For object properties, verify the asset was actually loaded (not silently set to null).
		// This catches invalid paths like "/Engine/BasicShapes.Cube" that ImportText_Direct accepts
		// syntactically but which resolve to no asset, reporting a false success.
		if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
		{
			UObject* LoadedObj = ObjProp->GetObjectPropertyValue(ValuePtr);
			const bool bWasNoneIntent = PropertyValue.IsEmpty()
				|| PropertyValue.Equals(TEXT("None"), ESearchCase::IgnoreCase)
				|| PropertyValue.Equals(TEXT("null"),  ESearchCase::IgnoreCase);

			if (!LoadedObj && !bWasNoneIntent)
			{
				UE_LOG(LogTemp, Error,
					TEXT("SetComponentProperty: Object property '%s' resolved to null — path '%s' is invalid. "
					     "Use the full object path format: /Package/Folder/AssetName.AssetName"),
					*PropertyName, *PropertyValue);
				return false;
			}
		}
	}

	// Notify the component template that its property changed so the Blueprint Editor
	// Details panel and viewport refresh correctly.
	FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
	Component->PostEditChangeProperty(PropertyChangedEvent);

	// Mark blueprint as structurally modified (covers mesh/component visual changes) and recompile.
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("SetComponentProperty: Set '%s.%s' = '%s'"), *ComponentName, *PropertyName, *PropertyValue);
	return true;
}

TArray<FComponentPropertyInfo> UBlueprintService::GetAllComponentProperties(
	const FString& BlueprintPath,
	const FString& ComponentName,
	bool bIncludeInherited)
{
	TArray<FComponentPropertyInfo> Results;
	
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("GetAllComponentProperties: Failed to load blueprint: %s"), *BlueprintPath);
		return Results;
	}
	
	UActorComponent* Component = FindComponentTemplate(Blueprint, ComponentName);
	if (!Component)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetAllComponentProperties: Component '%s' not found"), *ComponentName);
		return Results;
	}
	
	UClass* ComponentClass = Component->GetClass();
	
	for (TFieldIterator<FProperty> PropIt(ComponentClass); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		
		// Skip if not including inherited
		if (!bIncludeInherited && Property->GetOwnerClass() != ComponentClass)
		{
			continue;
		}
		
		// Skip transient properties
		if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient))
		{
			continue;
		}
		
		FComponentPropertyInfo Info;
		Info.PropertyName = Property->GetName();
		Info.PropertyType = Property->GetCPPType();
		Info.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);
		Info.bIsInherited = (Property->GetOwnerClass() != ComponentClass);
		
		// Get category
		if (Property->HasMetaData(TEXT("Category")))
		{
			Info.Category = Property->GetMetaData(TEXT("Category"));
		}
		
		// Get current value
		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Component);
		Property->ExportTextItem_Direct(Info.Value, ValuePtr, nullptr, Component, PPF_None);
		
		Results.Add(Info);
	}
	
	return Results;
}

TArray<FComponentPropertyInfo> UBlueprintService::ListComponentProperties(
	const FString& BlueprintPath,
	const FString& ComponentName,
	bool bIncludeInherited)
{
	// This is an alias for GetAllComponentProperties with a more intuitive name
	return GetAllComponentProperties(BlueprintPath, ComponentName, bIncludeInherited);
}

bool UBlueprintService::SetRootComponent(
	const FString& BlueprintPath,
	const FString& ComponentName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SetRootComponent: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}
	
	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	if (!SCS)
	{
		UE_LOG(LogTemp, Error, TEXT("SetRootComponent: Blueprint has no SCS: %s"), *BlueprintPath);
		return false;
	}
	
	// Find the component node to make root
	USCS_Node* NewRootNode = nullptr;
	USCS_Node* CurrentRootNode = SCS->GetDefaultSceneRootNode();
	
	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			NewRootNode = Node;
			break;
		}
	}
	
	if (!NewRootNode)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetRootComponent: Component '%s' not found"), *ComponentName);
		return false;
	}
	
	// Check if it's already the root
	if (NewRootNode == CurrentRootNode)
	{
		UE_LOG(LogTemp, Log, TEXT("SetRootComponent: '%s' is already the root component"), *ComponentName);
		return true;
	}
	
	// Ensure the new root is a SceneComponent
	if (!NewRootNode->ComponentTemplate || !NewRootNode->ComponentTemplate->IsA<USceneComponent>())
	{
		UE_LOG(LogTemp, Error, TEXT("SetRootComponent: '%s' is not a SceneComponent and cannot be root"), *ComponentName);
		return false;
	}
	
	// Store children of the current root (if any) to reparent them
	TArray<USCS_Node*> ChildrenToReparent;
	if (CurrentRootNode)
	{
		ChildrenToReparent = CurrentRootNode->GetChildNodes();
	}
	
	// Find and remove the new root from its current parent
	USCS_Node* NewRootCurrentParent = nullptr;
	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		if (Node)
		{
			for (USCS_Node* Child : Node->GetChildNodes())
			{
				if (Child == NewRootNode)
				{
					NewRootCurrentParent = Node;
					break;
				}
			}
			if (NewRootCurrentParent)
			{
				break;
			}
		}
	}
	
	// Mark blueprint as modifying
	Blueprint->Modify();
	
	// Remove new root from its current parent
	if (NewRootCurrentParent)
	{
		NewRootCurrentParent->RemoveChildNode(NewRootNode);
	}
	else
	{
		// It might be a root node itself, remove it from root nodes
		SCS->RemoveNode(NewRootNode);
	}
	
	// If there was a current root, we need to handle it
	if (CurrentRootNode && CurrentRootNode != NewRootNode)
	{
		// Remove children from current root first (we'll add them to new root)
		for (USCS_Node* Child : ChildrenToReparent)
		{
			if (Child && Child != NewRootNode)
			{
				CurrentRootNode->RemoveChildNode(Child);
			}
		}
		
		// Detach the current root from being THE root  
		// Make the old root a child of the new root
		SCS->RemoveNode(CurrentRootNode);
		NewRootNode->AddChildNode(CurrentRootNode);
		// CRITICAL: Call SetParent to properly set ParentComponentOrVariableName
		CurrentRootNode->SetParent(NewRootNode);
	}
	
	// Add new root as a root node
	SCS->AddNode(NewRootNode);
	
	// Reparent the old children (except the new root) to the new root
	for (USCS_Node* Child : ChildrenToReparent)
	{
		if (Child && Child != NewRootNode && Child != CurrentRootNode)
		{
			NewRootNode->AddChildNode(Child);
			// CRITICAL: Call SetParent to properly set ParentComponentOrVariableName
			Child->SetParent(NewRootNode);
		}
	}
	
	// Mark blueprint as structurally modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	
	UE_LOG(LogTemp, Log, TEXT("SetRootComponent: Set '%s' as root component in %s"), *ComponentName, *BlueprintPath);
	return true;
}

bool UBlueprintService::CompareComponents(
	const FString& BlueprintPathA,
	const FString& ComponentNameA,
	const FString& BlueprintPathB,
	const FString& ComponentNameB,
	FString& OutDifferences)
{
	// Get properties from both components
	TArray<FComponentPropertyInfo> PropsA = GetAllComponentProperties(BlueprintPathA, ComponentNameA, true);
	TArray<FComponentPropertyInfo> PropsB = GetAllComponentProperties(BlueprintPathB, ComponentNameB, true);
	
	if (PropsA.Num() == 0)
	{
		OutDifferences = FString::Printf(TEXT("Component '%s' not found in blueprint A or has no properties"), *ComponentNameA);
		return false;
	}
	
	if (PropsB.Num() == 0)
	{
		OutDifferences = FString::Printf(TEXT("Component '%s' not found in blueprint B or has no properties"), *ComponentNameB);
		return false;
	}
	
	TArray<FString> Differences;
	
	// Build map of properties from A
	TMap<FString, FComponentPropertyInfo> MapA;
	for (const FComponentPropertyInfo& Prop : PropsA)
	{
		MapA.Add(Prop.PropertyName, Prop);
	}
	
	// Build map of properties from B
	TMap<FString, FComponentPropertyInfo> MapB;
	for (const FComponentPropertyInfo& Prop : PropsB)
	{
		MapB.Add(Prop.PropertyName, Prop);
	}
	
	// Find properties only in A
	for (const auto& Pair : MapA)
	{
		if (!MapB.Contains(Pair.Key))
		{
			Differences.Add(FString::Printf(TEXT("Property '%s' only in A (%s)"), *Pair.Key, *Pair.Value.PropertyType));
		}
	}
	
	// Find properties only in B
	for (const auto& Pair : MapB)
	{
		if (!MapA.Contains(Pair.Key))
		{
			Differences.Add(FString::Printf(TEXT("Property '%s' only in B (%s)"), *Pair.Key, *Pair.Value.PropertyType));
		}
	}
	
	// Compare matching properties
	for (const auto& PairA : MapA)
	{
		if (MapB.Contains(PairA.Key))
		{
			const FComponentPropertyInfo& PropA = PairA.Value;
			const FComponentPropertyInfo& PropB = MapB[PairA.Key];
			
			// Check type difference
			if (PropA.PropertyType != PropB.PropertyType)
			{
				Differences.Add(FString::Printf(TEXT("Property '%s' type differs: '%s' vs '%s'"), 
					*PairA.Key, *PropA.PropertyType, *PropB.PropertyType));
			}
			// Check value difference (only for same types)
			else if (PropA.Value != PropB.Value)
			{
				// Truncate long values
				FString ValA = PropA.Value.Len() > 50 ? PropA.Value.Left(47) + TEXT("...") : PropA.Value;
				FString ValB = PropB.Value.Len() > 50 ? PropB.Value.Left(47) + TEXT("...") : PropB.Value;
				Differences.Add(FString::Printf(TEXT("Property '%s' value differs: '%s' vs '%s'"), 
					*PairA.Key, *ValA, *ValB));
			}
		}
	}
	
	if (Differences.Num() == 0)
	{
		OutDifferences = TEXT("Components are identical");
	}
	else
	{
		OutDifferences = FString::Join(Differences, TEXT("\n"));
	}
	
	return true;
}

bool UBlueprintService::ReparentComponent(
	const FString& BlueprintPath,
	const FString& ComponentName,
	const FString& NewParentName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("ReparentComponent: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}
	
	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	if (!SCS)
	{
		UE_LOG(LogTemp, Error, TEXT("ReparentComponent: Blueprint has no SCS: %s"), *BlueprintPath);
		return false;
	}
	
	// Find the component to reparent
	USCS_Node* NodeToReparent = nullptr;
	USCS_Node* CurrentParent = nullptr;
	
	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		if (Node)
		{
			if (Node->GetVariableName().ToString() == ComponentName)
			{
				NodeToReparent = Node;
			}
			
			// Check if this node is the current parent
			for (USCS_Node* Child : Node->GetChildNodes())
			{
				if (Child && Child->GetVariableName().ToString() == ComponentName)
				{
					CurrentParent = Node;
				}
			}
		}
	}
	
	if (!NodeToReparent)
	{
		UE_LOG(LogTemp, Warning, TEXT("ReparentComponent: Component '%s' not found"), *ComponentName);
		return false;
	}
	
	// Find new parent
	USCS_Node* NewParent = nullptr;
	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == NewParentName)
		{
			NewParent = Node;
			break;
		}
	}
	
	if (!NewParent)
	{
		UE_LOG(LogTemp, Warning, TEXT("ReparentComponent: New parent '%s' not found"), *NewParentName);
		return false;
	}
	
	// Prevent circular parenting
	if (NodeToReparent == NewParent)
	{
		UE_LOG(LogTemp, Error, TEXT("ReparentComponent: Cannot parent component to itself"));
		return false;
	}
	
	// Check for circular reference (NewParent can't be a descendant of NodeToReparent)
	TArray<USCS_Node*> Descendants;
	TFunction<void(USCS_Node*)> CollectDescendants = [&](USCS_Node* Node) {
		for (USCS_Node* Child : Node->GetChildNodes())
		{
			if (Child)
			{
				Descendants.Add(Child);
				CollectDescendants(Child);
			}
		}
	};
	CollectDescendants(NodeToReparent);
	
	if (Descendants.Contains(NewParent))
	{
		UE_LOG(LogTemp, Error, TEXT("ReparentComponent: Circular reference - new parent is a descendant"));
		return false;
	}
	
	// Remove from current parent
	if (CurrentParent)
	{
		CurrentParent->RemoveChildNode(NodeToReparent);
	}
	else
	{
		// It's a root node
		SCS->RemoveNode(NodeToReparent);
	}
	
	// Add to new parent
	NewParent->AddChildNode(NodeToReparent);
	
	// CRITICAL: Call SetParent to properly set ParentComponentOrVariableName
	// AddChildNode only manages the ChildNodes array, it does NOT set the parent reference
	NodeToReparent->SetParent(NewParent);
	
	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	
	UE_LOG(LogTemp, Log, TEXT("ReparentComponent: Moved '%s' to parent '%s'"), *ComponentName, *NewParentName);
	return true;
}

// ============================================================================
// VARIABLE MANAGEMENT (Phase 1)
// ============================================================================

bool UBlueprintService::AddVariable(
	const FString& BlueprintPath,
	const FString& VariableName,
	const FString& VariableType,
	const FString& DefaultValue,
	bool bIsArray,
	const FString& ContainerType)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddVariable: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Check if variable already exists
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName.ToString() == VariableName)
		{
			UE_LOG(LogTemp, Warning, TEXT("AddVariable: Variable '%s' already exists in %s"), *VariableName, *BlueprintPath);
			return false;
		}
	}

	// Parse the type string
	FEdGraphPinType PinType;
	FString ErrorMessage;
	if (!FBlueprintTypeParser::ParseTypeString(VariableType, PinType, bIsArray, ContainerType, ErrorMessage))
	{
		UE_LOG(LogTemp, Error, TEXT("AddVariable: Failed to parse type '%s': %s"), *VariableType, *ErrorMessage);
		return false;
	}

	// Create variable description
	FBPVariableDescription NewVar;
	NewVar.VarName = FName(*VariableName);
	NewVar.VarGuid = FGuid::NewGuid();
	NewVar.VarType = PinType;
	NewVar.FriendlyName = VariableName;
	NewVar.Category = FText::FromString(TEXT("Default"));
	NewVar.DefaultValue = DefaultValue;
	NewVar.PropertyFlags = CPF_Edit | CPF_BlueprintVisible | CPF_DisableEditOnInstance;

	// Add variable to blueprint
	Blueprint->NewVariables.Add(NewVar);

	// Mark blueprint as modified and refresh
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("AddVariable: Added variable '%s' of type '%s' to %s"), *VariableName, *VariableType, *BlueprintPath);
	return true;
}

bool UBlueprintService::SetVariableDefaultValue(
	const FString& BlueprintPath,
	const FString& VariableName,
	const FString& DefaultValue)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SetVariableDefaultValue: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find the variable
	for (FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName.ToString() == VariableName)
		{
			Var.DefaultValue = DefaultValue;
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
			UE_LOG(LogTemp, Log, TEXT("SetVariableDefaultValue: Set '%s' default to '%s'"), *VariableName, *DefaultValue);
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("SetVariableDefaultValue: Variable '%s' not found in %s"), *VariableName, *BlueprintPath);
	return false;
}

bool UBlueprintService::RemoveVariable(
	const FString& BlueprintPath,
	const FString& VariableName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveVariable: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find and remove the variable
	for (int32 i = 0; i < Blueprint->NewVariables.Num(); ++i)
	{
		if (Blueprint->NewVariables[i].VarName.ToString() == VariableName)
		{
			FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, Blueprint->NewVariables[i].VarName);
			UE_LOG(LogTemp, Log, TEXT("RemoveVariable: Removed variable '%s' from %s"), *VariableName, *BlueprintPath);
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("RemoveVariable: Variable '%s' not found in %s"), *VariableName, *BlueprintPath);
	return false;
}

bool UBlueprintService::GetVariableInfo(
	const FString& BlueprintPath,
	const FString& VariableName,
	FBlueprintVariableDetailedInfo& OutInfo)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("GetVariableInfo: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find the variable
	for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables)
	{
		if (VarDesc.VarName.ToString() == VariableName)
		{
			OutInfo.VariableName = VarDesc.VarName.ToString();
			OutInfo.VariableType = FBlueprintTypeParser::GetFriendlyTypeName(VarDesc.VarType);
			OutInfo.Category = VarDesc.Category.ToString();
			OutInfo.DefaultValue = VarDesc.DefaultValue;

			// Get tooltip from metadata
			if (VarDesc.HasMetaData(TEXT("tooltip")))
			{
				OutInfo.Tooltip = VarDesc.GetMetaData(TEXT("tooltip"));
			}

			// Build type path from pin type
			if (VarDesc.VarType.PinSubCategoryObject.IsValid())
			{
				const UObject* TypeObj = VarDesc.VarType.PinSubCategoryObject.Get();
				if (TypeObj)
				{
					OutInfo.TypePath = TypeObj->GetPathName();
				}
			}
			else
			{
				// Primitive types
				OutInfo.TypePath = FString::Printf(TEXT("/Script/CoreUObject.%sProperty"), *VarDesc.VarType.PinCategory.ToString());
			}

			// Property flags
			OutInfo.bIsInstanceEditable = (VarDesc.PropertyFlags & CPF_DisableEditOnInstance) == 0;
			OutInfo.bIsExposeOnSpawn = (VarDesc.PropertyFlags & CPF_ExposeOnSpawn) != 0;
			OutInfo.bIsPrivate = VarDesc.VarType.bIsConst;
			OutInfo.bIsBlueprintReadOnly = (VarDesc.PropertyFlags & CPF_BlueprintReadOnly) != 0;
			OutInfo.bIsExposeToCinematics = (VarDesc.PropertyFlags & CPF_Interp) != 0;

			// Container type
			OutInfo.bIsArray = (VarDesc.VarType.ContainerType == EPinContainerType::Array);
			OutInfo.bIsSet = (VarDesc.VarType.ContainerType == EPinContainerType::Set);
			OutInfo.bIsMap = (VarDesc.VarType.ContainerType == EPinContainerType::Map);

			// Replication
			if (VarDesc.RepNotifyFunc != NAME_None)
			{
				OutInfo.ReplicationCondition = TEXT("RepNotify");
			}
			else if (VarDesc.PropertyFlags & CPF_Net)
			{
				OutInfo.ReplicationCondition = TEXT("Replicated");
			}
			else
			{
				OutInfo.ReplicationCondition = TEXT("None");
			}

			UE_LOG(LogTemp, Log, TEXT("GetVariableInfo: Got info for '%s' in %s"), *VariableName, *BlueprintPath);
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("GetVariableInfo: Variable '%s' not found in %s"), *VariableName, *BlueprintPath);
	return false;
}

bool UBlueprintService::ModifyVariable(
	const FString& BlueprintPath,
	const FString& VariableName,
	const FString& NewName,
	const FString& NewCategory,
	const FString& NewTooltip,
	const FString& NewDefaultValue,
	int32 bSetInstanceEditable,
	int32 bSetExposeOnSpawn,
	int32 bSetPrivate,
	int32 bSetBlueprintReadOnly,
	const FString& NewReplicationCondition)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("ModifyVariable: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find the variable
	FBPVariableDescription* FoundVar = nullptr;
	for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
	{
		if (VarDesc.VarName.ToString() == VariableName)
		{
			FoundVar = &VarDesc;
			break;
		}
	}

	if (!FoundVar)
	{
		UE_LOG(LogTemp, Warning, TEXT("ModifyVariable: Variable '%s' not found in %s"), *VariableName, *BlueprintPath);
		return false;
	}

	bool bModified = false;

	// Rename if specified
	if (!NewName.IsEmpty() && NewName != VariableName)
	{
		FBlueprintEditorUtils::RenameMemberVariable(Blueprint, FoundVar->VarName, FName(*NewName));
		bModified = true;
	}

	// Update category
	if (!NewCategory.IsEmpty())
	{
		FoundVar->Category = FText::FromString(NewCategory);
		bModified = true;
	}

	// Update tooltip (stored as metadata)
	if (!NewTooltip.IsEmpty())
	{
		FoundVar->SetMetaData(TEXT("tooltip"), NewTooltip);
		bModified = true;
	}

	// Update default value
	if (!NewDefaultValue.IsEmpty())
	{
		FoundVar->DefaultValue = NewDefaultValue;
		bModified = true;
	}

	// Update property flags
	if (bSetInstanceEditable >= 0)
	{
		if (bSetInstanceEditable > 0)
		{
			FoundVar->PropertyFlags &= ~CPF_DisableEditOnInstance;
		}
		else
		{
			FoundVar->PropertyFlags |= CPF_DisableEditOnInstance;
		}
		bModified = true;
	}

	if (bSetExposeOnSpawn >= 0)
	{
		if (bSetExposeOnSpawn > 0)
		{
			FoundVar->PropertyFlags |= CPF_ExposeOnSpawn;
		}
		else
		{
			FoundVar->PropertyFlags &= ~CPF_ExposeOnSpawn;
		}
		bModified = true;
	}

	if (bSetBlueprintReadOnly >= 0)
	{
		if (bSetBlueprintReadOnly > 0)
		{
			FoundVar->PropertyFlags |= CPF_BlueprintReadOnly;
		}
		else
		{
			FoundVar->PropertyFlags &= ~CPF_BlueprintReadOnly;
		}
		bModified = true;
	}

	// Update replication
	if (!NewReplicationCondition.IsEmpty())
	{
		if (NewReplicationCondition.Equals(TEXT("Replicated"), ESearchCase::IgnoreCase))
		{
			FoundVar->PropertyFlags |= CPF_Net;
			FoundVar->RepNotifyFunc = NAME_None;
		}
		else if (NewReplicationCondition.Equals(TEXT("RepNotify"), ESearchCase::IgnoreCase))
		{
			FoundVar->PropertyFlags |= CPF_Net;
			// Create OnRep function name
			FString OnRepName = FString::Printf(TEXT("OnRep_%s"), *FoundVar->VarName.ToString());
			FoundVar->RepNotifyFunc = FName(*OnRepName);
		}
		else if (NewReplicationCondition.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			FoundVar->PropertyFlags &= ~CPF_Net;
			FoundVar->RepNotifyFunc = NAME_None;
		}
		bModified = true;
	}

	if (bModified)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		UE_LOG(LogTemp, Log, TEXT("ModifyVariable: Modified variable '%s' in %s"), *VariableName, *BlueprintPath);
	}

	return true;
}

TArray<FVariableTypeInfo> UBlueprintService::SearchVariableTypes(
	const FString& SearchTerm,
	const FString& Category,
	int32 MaxResults)
{
	TArray<FVariableTypeInfo> Results;

	// Pre-defined basic types (always available)
	struct FBuiltInType
	{
		FString TypeName;
		FString TypePath;
		FString Category;
		FString Description;
	};

	TArray<FBuiltInType> BuiltInTypes = {
		// Basic types
		{ TEXT("Boolean"), TEXT("bool"), TEXT("Basic"), TEXT("True or false value") },
		{ TEXT("Byte"), TEXT("byte"), TEXT("Basic"), TEXT("8-bit unsigned integer (0-255)") },
		{ TEXT("Integer"), TEXT("int"), TEXT("Basic"), TEXT("32-bit signed integer") },
		{ TEXT("Integer64"), TEXT("int64"), TEXT("Basic"), TEXT("64-bit signed integer") },
		{ TEXT("Float"), TEXT("float"), TEXT("Basic"), TEXT("Single precision floating point (32-bit)") },
		{ TEXT("Double"), TEXT("double"), TEXT("Basic"), TEXT("Double precision floating point (64-bit)") },
		{ TEXT("Name"), TEXT("FName"), TEXT("Basic"), TEXT("Unique identifier name") },
		{ TEXT("String"), TEXT("FString"), TEXT("Basic"), TEXT("Text string") },
		{ TEXT("Text"), TEXT("FText"), TEXT("Basic"), TEXT("Localizable text") },

		// Common structures
		{ TEXT("Vector"), TEXT("FVector"), TEXT("Structure"), TEXT("3D vector (X, Y, Z)") },
		{ TEXT("Vector2D"), TEXT("FVector2D"), TEXT("Structure"), TEXT("2D vector (X, Y)") },
		{ TEXT("Vector4"), TEXT("FVector4"), TEXT("Structure"), TEXT("4D vector (X, Y, Z, W)") },
		{ TEXT("Rotator"), TEXT("FRotator"), TEXT("Structure"), TEXT("Rotation in 3D space (Pitch, Yaw, Roll)") },
		{ TEXT("Transform"), TEXT("FTransform"), TEXT("Structure"), TEXT("Location, rotation, and scale") },
		{ TEXT("Quat"), TEXT("FQuat"), TEXT("Structure"), TEXT("Quaternion rotation") },
		{ TEXT("Color"), TEXT("FColor"), TEXT("Structure"), TEXT("RGBA color (0-255)") },
		{ TEXT("LinearColor"), TEXT("FLinearColor"), TEXT("Structure"), TEXT("Linear RGBA color (0.0-1.0)") },
		{ TEXT("DateTime"), TEXT("FDateTime"), TEXT("Structure"), TEXT("Date and time") },
		{ TEXT("Timespan"), TEXT("FTimespan"), TEXT("Structure"), TEXT("Time duration") },
		{ TEXT("Guid"), TEXT("FGuid"), TEXT("Structure"), TEXT("Globally unique identifier") },
		{ TEXT("IntPoint"), TEXT("FIntPoint"), TEXT("Structure"), TEXT("2D integer point") },
		{ TEXT("IntVector"), TEXT("FIntVector"), TEXT("Structure"), TEXT("3D integer vector") },
		{ TEXT("Box"), TEXT("FBox"), TEXT("Structure"), TEXT("3D axis-aligned bounding box") },
		{ TEXT("Box2D"), TEXT("FBox2D"), TEXT("Structure"), TEXT("2D axis-aligned bounding box") },

		// Common object types
		{ TEXT("Object"), TEXT("UObject"), TEXT("Object"), TEXT("Base Unreal object reference") },
		{ TEXT("Actor"), TEXT("AActor"), TEXT("Object"), TEXT("Actor reference") },
		{ TEXT("Pawn"), TEXT("APawn"), TEXT("Object"), TEXT("Pawn reference") },
		{ TEXT("Character"), TEXT("ACharacter"), TEXT("Object"), TEXT("Character reference") },
		{ TEXT("PlayerController"), TEXT("APlayerController"), TEXT("Object"), TEXT("Player controller reference") },
		{ TEXT("ActorComponent"), TEXT("UActorComponent"), TEXT("Object"), TEXT("Actor component reference") },
		{ TEXT("SceneComponent"), TEXT("USceneComponent"), TEXT("Object"), TEXT("Scene component reference") },
		{ TEXT("StaticMeshComponent"), TEXT("UStaticMeshComponent"), TEXT("Object"), TEXT("Static mesh component") },
		{ TEXT("SkeletalMeshComponent"), TEXT("USkeletalMeshComponent"), TEXT("Object"), TEXT("Skeletal mesh component") },
		{ TEXT("Texture2D"), TEXT("UTexture2D"), TEXT("Object"), TEXT("2D texture reference") },
		{ TEXT("Material"), TEXT("UMaterialInterface"), TEXT("Object"), TEXT("Material reference") },
		{ TEXT("SoundBase"), TEXT("USoundBase"), TEXT("Object"), TEXT("Sound reference") },
		{ TEXT("ParticleSystem"), TEXT("UParticleSystem"), TEXT("Object"), TEXT("Particle system reference") },
		{ TEXT("DataTable"), TEXT("UDataTable"), TEXT("Object"), TEXT("Data table reference") },
		{ TEXT("CurveFloat"), TEXT("UCurveFloat"), TEXT("Object"), TEXT("Float curve reference") },
		{ TEXT("AnimMontage"), TEXT("UAnimMontage"), TEXT("Object"), TEXT("Animation montage reference") },
		{ TEXT("AnimSequence"), TEXT("UAnimSequence"), TEXT("Object"), TEXT("Animation sequence reference") },
		{ TEXT("Blueprint"), TEXT("UBlueprint"), TEXT("Object"), TEXT("Blueprint asset reference") },
		{ TEXT("UserWidget"), TEXT("UUserWidget"), TEXT("Object"), TEXT("User widget reference") },
		{ TEXT("World"), TEXT("UWorld"), TEXT("Object"), TEXT("World reference") },
	};

	// Filter and add matching types
	for (const FBuiltInType& Type : BuiltInTypes)
	{
		// Check category filter
		if (!Category.IsEmpty() && !Type.Category.Equals(Category, ESearchCase::IgnoreCase))
		{
			continue;
		}

		// Check search term
		if (!SearchTerm.IsEmpty())
		{
			if (!Type.TypeName.Contains(SearchTerm, ESearchCase::IgnoreCase) &&
				!Type.TypePath.Contains(SearchTerm, ESearchCase::IgnoreCase) &&
				!Type.Description.Contains(SearchTerm, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		FVariableTypeInfo Info;
		Info.TypeName = Type.TypeName;
		Info.TypePath = Type.TypePath;
		Info.Category = Type.Category;
		Info.Description = Type.Description;
		Results.Add(Info);

		if (MaxResults > 0 && Results.Num() >= MaxResults)
		{
			break;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("SearchVariableTypes: Found %d types matching '%s' (category: '%s')"),
		Results.Num(), *SearchTerm, *Category);

	return Results;
}

// ============================================================================
// FUNCTION MANAGEMENT (Phase 2)
// ============================================================================

bool UBlueprintService::CreateFunction(
	const FString& BlueprintPath,
	const FString& FunctionName,
	bool bIsPure)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateFunction: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Check if function already exists
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			UE_LOG(LogTemp, Warning, TEXT("CreateFunction: Function '%s' already exists in %s"), *FunctionName, *BlueprintPath);
			return true;  // Idempotent - not an error
		}
	}

	// Create new function graph
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*FunctionName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

	if (!NewGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateFunction: Failed to create graph for '%s'"), *FunctionName);
		return false;
	}

	// Set up the function graph
	FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, NewGraph, /*bIsUserCreated=*/true, /*SignatureFromClass=*/nullptr);

	// Find the function entry node and set pure flag
	if (bIsPure)
	{
		TArray<UK2Node_FunctionEntry*> EntryNodes;
		NewGraph->GetNodesOfClass(EntryNodes);
		if (EntryNodes.Num() > 0 && EntryNodes[0])
		{
			EntryNodes[0]->AddExtraFlags(FUNC_BlueprintPure);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("CreateFunction: Created function '%s' in %s"), *FunctionName, *BlueprintPath);
	return true;
}

bool UBlueprintService::AddFunctionParameter(
	const FString& BlueprintPath,
	const FString& FunctionName,
	const FString& ParameterName,
	const FString& ParameterType,
	bool bIsOutput,
	bool bIsReference,
	const FString& DefaultValue,
	bool bIsArray,
	const FString& ContainerType)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddFunctionParameter: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddFunctionParameter: Function '%s' not found in %s"), *FunctionName, *BlueprintPath);
		return false;
	}

	// Parse the type string
	FEdGraphPinType PinType;
	FString ErrorMessage;
	if (!FBlueprintTypeParser::ParseTypeString(ParameterType, PinType, bIsArray, ContainerType, ErrorMessage))
	{
		UE_LOG(LogTemp, Error, TEXT("AddFunctionParameter: Failed to parse type '%s': %s"), *ParameterType, *ErrorMessage);
		return false;
	}

	// Set reference flag
	if (bIsReference)
	{
		PinType.bIsReference = true;
	}

	// Find the appropriate node (entry for inputs, result for outputs)
	if (bIsOutput)
	{
		// Add to function result node
		TArray<UK2Node_FunctionResult*> ResultNodes;
		FunctionGraph->GetNodesOfClass(ResultNodes);

		if (ResultNodes.Num() == 0)
		{
			// Create result node if it doesn't exist
			UK2Node_FunctionResult* ResultNode = NewObject<UK2Node_FunctionResult>(FunctionGraph);
			FunctionGraph->AddNode(ResultNode, false, false);
			ResultNode->CreateNewGuid();
			ResultNode->PostPlacedNewNode();
			ResultNode->AllocateDefaultPins();
			ResultNodes.Add(ResultNode);
		}

		if (ResultNodes.Num() > 0 && ResultNodes[0])
		{
			ResultNodes[0]->CreateUserDefinedPin(FName(*ParameterName), PinType, EGPD_Input);
		}
	}
	else
	{
		// Add to function entry node
		TArray<UK2Node_FunctionEntry*> EntryNodes;
		FunctionGraph->GetNodesOfClass(EntryNodes);

		if (EntryNodes.Num() > 0 && EntryNodes[0])
		{
			EntryNodes[0]->CreateUserDefinedPin(FName(*ParameterName), PinType, EGPD_Output);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("AddFunctionParameter: No entry node found in function '%s'"), *FunctionName);
			return false;
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddFunctionParameter: Added parameter '%s' (%s) to function '%s'"), *ParameterName, bIsOutput ? TEXT("output") : TEXT("input"), *FunctionName);
	return true;
}

bool UBlueprintService::AddFunctionLocalVariable(
	const FString& BlueprintPath,
	const FString& FunctionName,
	const FString& VariableName,
	const FString& VariableType,
	const FString& DefaultValue,
	bool bIsArray,
	const FString& ContainerType)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddFunctionLocalVariable: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddFunctionLocalVariable: Function '%s' not found in %s"), *FunctionName, *BlueprintPath);
		return false;
	}

	// Parse the type string
	FEdGraphPinType PinType;
	FString ErrorMessage;
	if (!FBlueprintTypeParser::ParseTypeString(VariableType, PinType, bIsArray, ContainerType, ErrorMessage))
	{
		UE_LOG(LogTemp, Error, TEXT("AddFunctionLocalVariable: Failed to parse type '%s': %s"), *VariableType, *ErrorMessage);
		return false;
	}

	// Find the function entry node
	TArray<UK2Node_FunctionEntry*> EntryNodes;
	FunctionGraph->GetNodesOfClass(EntryNodes);

	if (EntryNodes.Num() == 0 || !EntryNodes[0])
	{
		UE_LOG(LogTemp, Error, TEXT("AddFunctionLocalVariable: No entry node found in function '%s'"), *FunctionName);
		return false;
	}

	UK2Node_FunctionEntry* EntryNode = EntryNodes[0];

	// Create local variable description
	FBPVariableDescription LocalVar;
	LocalVar.VarName = FName(*VariableName);
	LocalVar.VarGuid = FGuid::NewGuid();
	LocalVar.VarType = PinType;
	LocalVar.FriendlyName = VariableName;
	LocalVar.DefaultValue = DefaultValue;
	LocalVar.Category = FText::FromString(TEXT("Local Variables"));

	// Add to entry node's local variables
	EntryNode->LocalVariables.Add(LocalVar);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddFunctionLocalVariable: Added local variable '%s' to function '%s'"), *VariableName, *FunctionName);
	return true;
}

TArray<FBlueprintFunctionParameterInfo> UBlueprintService::GetFunctionParameters(
	const FString& BlueprintPath,
	const FString& FunctionName)
{
	TArray<FBlueprintFunctionParameterInfo> Parameters;

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return Parameters;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		return Parameters;
	}

	// Get parameters from entry node
	TArray<UK2Node_FunctionEntry*> EntryNodes;
	FunctionGraph->GetNodesOfClass(EntryNodes);

	if (EntryNodes.Num() > 0 && EntryNodes[0])
	{
		UK2Node_FunctionEntry* EntryNode = EntryNodes[0];
		for (UEdGraphPin* Pin : EntryNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output && Pin->PinName != UEdGraphSchema_K2::PN_Then)
			{
				FBlueprintFunctionParameterInfo ParamInfo;
				ParamInfo.ParameterName = Pin->PinName.ToString();
				ParamInfo.ParameterType = FBlueprintTypeParser::GetFriendlyTypeName(Pin->PinType);
				ParamInfo.bIsOutput = false;
				ParamInfo.bIsReference = Pin->PinType.bIsReference;
				ParamInfo.DefaultValue = Pin->DefaultValue;
				Parameters.Add(ParamInfo);
			}
		}
	}

	// Get output parameters from result node
	TArray<UK2Node_FunctionResult*> ResultNodes;
	FunctionGraph->GetNodesOfClass(ResultNodes);

	if (ResultNodes.Num() > 0 && ResultNodes[0])
	{
		UK2Node_FunctionResult* ResultNode = ResultNodes[0];
		for (UEdGraphPin* Pin : ResultNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input && Pin->PinName != UEdGraphSchema_K2::PN_Execute)
			{
				FBlueprintFunctionParameterInfo ParamInfo;
				ParamInfo.ParameterName = Pin->PinName.ToString();
				ParamInfo.ParameterType = FBlueprintTypeParser::GetFriendlyTypeName(Pin->PinType);
				ParamInfo.bIsOutput = true;
				ParamInfo.bIsReference = Pin->PinType.bIsReference;
				ParamInfo.DefaultValue = Pin->DefaultValue;
				Parameters.Add(ParamInfo);
			}
		}
	}

	return Parameters;
}

bool UBlueprintService::DeleteFunction(
	const FString& BlueprintPath,
	const FString& FunctionName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("DeleteFunction: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		UE_LOG(LogTemp, Warning, TEXT("DeleteFunction: Function '%s' not found in %s"), *FunctionName, *BlueprintPath);
		return false;
	}

	// Remove the function graph
	FBlueprintEditorUtils::RemoveGraph(Blueprint, FunctionGraph, EGraphRemoveFlags::Recompile);
	UE_LOG(LogTemp, Log, TEXT("DeleteFunction: Deleted function '%s' from %s"), *FunctionName, *BlueprintPath);
	return true;
}

bool UBlueprintService::GetFunctionInfo(
	const FString& BlueprintPath,
	const FString& FunctionName,
	FBlueprintFunctionDetailedInfo& OutInfo)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("GetFunctionInfo: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetFunctionInfo: Function '%s' not found in %s"), *FunctionName, *BlueprintPath);
		return false;
	}

	OutInfo.FunctionName = FunctionGraph->GetName();
	OutInfo.GraphGuid = FunctionGraph->GraphGuid.ToString();
	OutInfo.NodeCount = FunctionGraph->Nodes.Num();

	// Get entry node for parameters and local variables
	TArray<UK2Node_FunctionEntry*> EntryNodes;
	FunctionGraph->GetNodesOfClass(EntryNodes);

	if (EntryNodes.Num() > 0 && EntryNodes[0])
	{
		UK2Node_FunctionEntry* EntryNode = EntryNodes[0];

		// Check if pure
		OutInfo.bIsPure = EntryNode->HasAnyExtraFlags(FUNC_BlueprintPure);

		// Get input parameters
		for (UEdGraphPin* Pin : EntryNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output && Pin->PinName != UEdGraphSchema_K2::PN_Then)
			{
				FBlueprintFunctionParameterInfo ParamInfo;
				ParamInfo.ParameterName = Pin->PinName.ToString();
				ParamInfo.ParameterType = FBlueprintTypeParser::GetFriendlyTypeName(Pin->PinType);
				ParamInfo.bIsOutput = false;
				ParamInfo.bIsReference = Pin->PinType.bIsReference;
				ParamInfo.DefaultValue = Pin->DefaultValue;
				OutInfo.InputParameters.Add(ParamInfo);
			}
		}

		// Get local variables
		for (const FBPVariableDescription& VarDesc : EntryNode->LocalVariables)
		{
			FBlueprintLocalVariableInfo LocalInfo;
			LocalInfo.VariableName = VarDesc.VarName.ToString();
			LocalInfo.FriendlyName = VarDesc.FriendlyName;
			LocalInfo.VariableType = FBlueprintTypeParser::GetFriendlyTypeName(VarDesc.VarType);
			LocalInfo.DisplayType = UEdGraphSchema_K2::TypeToText(VarDesc.VarType).ToString();
			LocalInfo.DefaultValue = VarDesc.DefaultValue;
			LocalInfo.Category = VarDesc.Category.ToString();
			LocalInfo.Guid = VarDesc.VarGuid.ToString();
			LocalInfo.bIsConst = VarDesc.VarType.bIsConst || ((VarDesc.PropertyFlags & CPF_BlueprintReadOnly) != 0);
			LocalInfo.bIsReference = VarDesc.VarType.bIsReference;
			LocalInfo.bIsArray = (VarDesc.VarType.ContainerType == EPinContainerType::Array);
			LocalInfo.bIsSet = (VarDesc.VarType.ContainerType == EPinContainerType::Set);
			LocalInfo.bIsMap = (VarDesc.VarType.ContainerType == EPinContainerType::Map);
			OutInfo.LocalVariables.Add(LocalInfo);
		}
	}

	// Get output parameters from result node
	TArray<UK2Node_FunctionResult*> ResultNodes;
	FunctionGraph->GetNodesOfClass(ResultNodes);

	if (ResultNodes.Num() > 0 && ResultNodes[0])
	{
		UK2Node_FunctionResult* ResultNode = ResultNodes[0];
		for (UEdGraphPin* Pin : ResultNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input && Pin->PinName != UEdGraphSchema_K2::PN_Execute)
			{
				FBlueprintFunctionParameterInfo ParamInfo;
				ParamInfo.ParameterName = Pin->PinName.ToString();
				ParamInfo.ParameterType = FBlueprintTypeParser::GetFriendlyTypeName(Pin->PinType);
				ParamInfo.bIsOutput = true;
				ParamInfo.bIsReference = Pin->PinType.bIsReference;
				ParamInfo.DefaultValue = Pin->DefaultValue;
				OutInfo.OutputParameters.Add(ParamInfo);
			}
		}
	}

	// Check if this is an override
	if (Blueprint->GeneratedClass)
	{
		UFunction* Function = Blueprint->GeneratedClass->FindFunctionByName(FName(*FunctionName));
		if (Function)
		{
			OutInfo.bIsOverride = (Function->GetSuperFunction() != nullptr);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("GetFunctionInfo: Got info for function '%s' in %s"), *FunctionName, *BlueprintPath);
	return true;
}

bool UBlueprintService::AddFunctionInput(
	const FString& BlueprintPath,
	const FString& FunctionName,
	const FString& ParameterName,
	const FString& ParameterType)
{
	return AddFunctionParameter(BlueprintPath, FunctionName, ParameterName, ParameterType, /*bIsOutput=*/false);
}

bool UBlueprintService::AddFunctionOutput(
	const FString& BlueprintPath,
	const FString& FunctionName,
	const FString& ParameterName,
	const FString& ParameterType)
{
	return AddFunctionParameter(BlueprintPath, FunctionName, ParameterName, ParameterType, /*bIsOutput=*/true);
}

bool UBlueprintService::RemoveFunctionParameter(
	const FString& BlueprintPath,
	const FString& FunctionName,
	const FString& ParameterName,
	bool bIsOutput)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveFunctionParameter: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveFunctionParameter: Function '%s' not found in %s"), *FunctionName, *BlueprintPath);
		return false;
	}

	bool bFound = false;

	if (!bIsOutput)
	{
		// Remove from entry node (input parameters)
		TArray<UK2Node_FunctionEntry*> EntryNodes;
		FunctionGraph->GetNodesOfClass(EntryNodes);

		if (EntryNodes.Num() > 0 && EntryNodes[0])
		{
			UK2Node_FunctionEntry* EntryNode = EntryNodes[0];
			for (int32 i = EntryNode->Pins.Num() - 1; i >= 0; --i)
			{
				UEdGraphPin* Pin = EntryNode->Pins[i];
				if (Pin && Pin->Direction == EGPD_Output && Pin->PinName.ToString().Equals(ParameterName, ESearchCase::IgnoreCase))
				{
					Pin->BreakAllPinLinks();
					EntryNode->Pins.RemoveAt(i);
					bFound = true;
					break;
				}
			}
		}
	}
	else
	{
		// Remove from result node (output parameters)
		TArray<UK2Node_FunctionResult*> ResultNodes;
		FunctionGraph->GetNodesOfClass(ResultNodes);

		for (UK2Node_FunctionResult* ResultNode : ResultNodes)
		{
			if (ResultNode)
			{
				for (int32 i = ResultNode->Pins.Num() - 1; i >= 0; --i)
				{
					UEdGraphPin* Pin = ResultNode->Pins[i];
					if (Pin && Pin->Direction == EGPD_Input && Pin->PinName.ToString().Equals(ParameterName, ESearchCase::IgnoreCase))
					{
						Pin->BreakAllPinLinks();
						ResultNode->Pins.RemoveAt(i);
						bFound = true;
						break;
					}
				}
			}
			if (bFound) break;
		}
	}

	if (!bFound)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveFunctionParameter: Parameter '%s' not found in function '%s'"), *ParameterName, *FunctionName);
		return false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("RemoveFunctionParameter: Removed parameter '%s' from function '%s'"), *ParameterName, *FunctionName);
	return true;
}

bool UBlueprintService::RemoveFunctionLocalVariable(
	const FString& BlueprintPath,
	const FString& FunctionName,
	const FString& VariableName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveFunctionLocalVariable: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveFunctionLocalVariable: Function '%s' not found in %s"), *FunctionName, *BlueprintPath);
		return false;
	}

	FName VarFName(*VariableName);

	// Try to find and remove the local variable
	UK2Node_FunctionEntry* EntryNode = nullptr;
	TArray<UK2Node_FunctionEntry*> EntryNodes;
	FunctionGraph->GetNodesOfClass(EntryNodes);

	if (EntryNodes.Num() > 0)
	{
		EntryNode = EntryNodes[0];
	}

	if (!EntryNode)
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveFunctionLocalVariable: No entry node found in function '%s'"), *FunctionName);
		return false;
	}

	// Find the local variable
	bool bFound = false;
	for (int32 Index = 0; Index < EntryNode->LocalVariables.Num(); ++Index)
	{
		if (EntryNode->LocalVariables[Index].VarName == VarFName)
		{
			EntryNode->LocalVariables.RemoveAt(Index);
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveFunctionLocalVariable: Local variable '%s' not found in function '%s'"), *VariableName, *FunctionName);
		return false;
	}

	// Remove any variable nodes referencing this local
	FBlueprintEditorUtils::RemoveVariableNodes(Blueprint, VarFName, true, FunctionGraph);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("RemoveFunctionLocalVariable: Removed local variable '%s' from function '%s'"), *VariableName, *FunctionName);
	return true;
}

bool UBlueprintService::UpdateFunctionLocalVariable(
	const FString& BlueprintPath,
	const FString& FunctionName,
	const FString& VariableName,
	const FString& NewName,
	const FString& NewType,
	const FString& NewDefaultValue)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("UpdateFunctionLocalVariable: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("UpdateFunctionLocalVariable: Function '%s' not found in %s"), *FunctionName, *BlueprintPath);
		return false;
	}

	// Get entry node
	TArray<UK2Node_FunctionEntry*> EntryNodes;
	FunctionGraph->GetNodesOfClass(EntryNodes);

	if (EntryNodes.Num() == 0 || !EntryNodes[0])
	{
		UE_LOG(LogTemp, Error, TEXT("UpdateFunctionLocalVariable: No entry node found in function '%s'"), *FunctionName);
		return false;
	}

	UK2Node_FunctionEntry* EntryNode = EntryNodes[0];

	// Find the local variable
	FBPVariableDescription* VarDesc = nullptr;
	for (FBPVariableDescription& Var : EntryNode->LocalVariables)
	{
		if (Var.VarName.ToString().Equals(VariableName, ESearchCase::IgnoreCase))
		{
			VarDesc = &Var;
			break;
		}
	}

	if (!VarDesc)
	{
		UE_LOG(LogTemp, Warning, TEXT("UpdateFunctionLocalVariable: Local variable '%s' not found in function '%s'"), *VariableName, *FunctionName);
		return false;
	}

	bool bModified = false;

	// Update type if specified
	if (!NewType.IsEmpty())
	{
		FEdGraphPinType NewPinType;
		FString ErrorMessage;
		if (FBlueprintTypeParser::ParseTypeString(NewType, NewPinType, false, TEXT(""), ErrorMessage))
		{
			VarDesc->VarType = NewPinType;
			VarDesc->DefaultValue.Empty(); // Clear default when type changes
			bModified = true;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UpdateFunctionLocalVariable: Failed to parse type '%s': %s"), *NewType, *ErrorMessage);
		}
	}

	// Update default value if specified
	if (!NewDefaultValue.IsEmpty())
	{
		VarDesc->DefaultValue = NewDefaultValue;
		bModified = true;
	}

	// Update name if specified
	if (!NewName.IsEmpty() && !NewName.Equals(VariableName, ESearchCase::CaseSensitive))
	{
		VarDesc->VarName = FName(*NewName);
		VarDesc->FriendlyName = FName::NameToDisplayString(NewName, VarDesc->VarType.PinCategory == UEdGraphSchema_K2::PC_Boolean);
		bModified = true;
	}

	if (bModified)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		UE_LOG(LogTemp, Log, TEXT("UpdateFunctionLocalVariable: Updated local variable '%s' in function '%s'"), *VariableName, *FunctionName);
	}

	return true;
}

TArray<FBlueprintLocalVariableInfo> UBlueprintService::ListFunctionLocalVariables(
	const FString& BlueprintPath,
	const FString& FunctionName)
{
	TArray<FBlueprintLocalVariableInfo> LocalVariables;

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return LocalVariables;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		return LocalVariables;
	}

	// Get entry node
	TArray<UK2Node_FunctionEntry*> EntryNodes;
	FunctionGraph->GetNodesOfClass(EntryNodes);

	if (EntryNodes.Num() > 0 && EntryNodes[0])
	{
		UK2Node_FunctionEntry* EntryNode = EntryNodes[0];

		for (const FBPVariableDescription& VarDesc : EntryNode->LocalVariables)
		{
			FBlueprintLocalVariableInfo LocalInfo;
			LocalInfo.VariableName = VarDesc.VarName.ToString();
			LocalInfo.FriendlyName = VarDesc.FriendlyName;
			LocalInfo.VariableType = FBlueprintTypeParser::GetFriendlyTypeName(VarDesc.VarType);
			LocalInfo.DisplayType = UEdGraphSchema_K2::TypeToText(VarDesc.VarType).ToString();
			LocalInfo.DefaultValue = VarDesc.DefaultValue;
			LocalInfo.Category = VarDesc.Category.ToString();
			LocalInfo.Guid = VarDesc.VarGuid.ToString();
			LocalInfo.bIsConst = VarDesc.VarType.bIsConst || ((VarDesc.PropertyFlags & CPF_BlueprintReadOnly) != 0);
			LocalInfo.bIsReference = VarDesc.VarType.bIsReference;
			LocalInfo.bIsArray = (VarDesc.VarType.ContainerType == EPinContainerType::Array);
			LocalInfo.bIsSet = (VarDesc.VarType.ContainerType == EPinContainerType::Set);
			LocalInfo.bIsMap = (VarDesc.VarType.ContainerType == EPinContainerType::Map);
			LocalVariables.Add(LocalInfo);
		}
	}

	return LocalVariables;
}

// ============================================================================
// NODE MANAGEMENT (Phase 3)
// ============================================================================

UEdGraph* UBlueprintService::FindGraph(UBlueprint* Blueprint, const FString& GraphName)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);

	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			return Graph;
		}
	}

	return nullptr;
}

UEdGraphNode* UBlueprintService::FindNodeById(UEdGraph* Graph, const FString& NodeId)
{
	if (!Graph)
	{
		return nullptr;
	}

	FGuid SearchGuid;
	if (!FGuid::Parse(NodeId, SearchGuid))
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && Node->NodeGuid == SearchGuid)
		{
			return Node;
		}
	}

	return nullptr;
}

FString UBlueprintService::AddGetVariableNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& VariableName,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddGetVariableNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	// Special handling for Animation Blueprints and EventGraph
	UEdGraph* Graph = nullptr;
	UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Blueprint);
	if (AnimBP && GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
	{
		// AnimBPs may store EventGraph in UbergraphPages
		for (UEdGraph* UberGraph : Blueprint->UbergraphPages)
		{
			if (UberGraph && UberGraph->GetFName() == UEdGraphSchema_K2::GN_EventGraph)
			{
				Graph = UberGraph;
				break;
			}
		}
	}
	
	if (!Graph)
	{
		Graph = FindGraph(Blueprint, GraphName);
	}

	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddGetVariableNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	// Validate variable exists — check compiled GeneratedClass first, fall back to NewVariables
	// (uncompiled BPs won't have the property in GeneratedClass yet)
	bool bVariableFound = false;
	if (Blueprint->GeneratedClass && FindFProperty<FProperty>(Blueprint->GeneratedClass, FName(*VariableName)))
	{
		bVariableFound = true;
	}
	if (!bVariableFound)
	{
		for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables)
		{
			if (VarDesc.VarName == FName(*VariableName))
			{
				bVariableFound = true;
				break;
			}
		}
	}
	if (!bVariableFound)
	{
		UE_LOG(LogTemp, Error, TEXT("AddGetVariableNode: Variable '%s' not found in %s"), *VariableName, *BlueprintPath);
		return FString();
	}

	// Create the get variable node
	UK2Node_VariableGet* GetNode = NewObject<UK2Node_VariableGet>(Graph);
	GetNode->VariableReference.SetSelfMember(FName(*VariableName));

	// Add to graph
	Graph->AddNode(GetNode, false, false);
	GetNode->CreateNewGuid();
	GetNode->PostPlacedNewNode();
	GetNode->AllocateDefaultPins();

	// Set position
	GetNode->NodePosX = PosX;
	GetNode->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddGetVariableNode: Added get node for '%s' in %s"), *VariableName, *GraphName);

	return GetNode->NodeGuid.ToString();
}

FString UBlueprintService::AddMemberGetNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& TargetClass,
	const FString& MemberName,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddMemberGetNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	UEdGraph* Graph = nullptr;
	UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Blueprint);
	if (AnimBP && GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
	{
		for (UEdGraph* UberGraph : Blueprint->UbergraphPages)
		{
			if (UberGraph && UberGraph->GetFName() == UEdGraphSchema_K2::GN_EventGraph)
			{
				Graph = UberGraph;
				break;
			}
		}
	}
	if (!Graph)
	{
		Graph = FindGraph(Blueprint, GraphName);
	}
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddMemberGetNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	// Resolve the target class — TObjectIterator search (finds engine, plugin, and project classes)
	UClass* OwnerClass = nullptr;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->GetName() == TargetClass)
		{
			OwnerClass = *It;
			break;
		}
	}

	if (!OwnerClass)
	{
		UE_LOG(LogTemp, Error, TEXT("AddMemberGetNode: Class '%s' not found"), *TargetClass);
		return FString();
	}

	// Verify the member exists on the class
	FProperty* MemberProp = FindFProperty<FProperty>(OwnerClass, FName(*MemberName));
	if (!MemberProp)
	{
		UE_LOG(LogTemp, Error, TEXT("AddMemberGetNode: Member '%s' not found on class '%s'"), *MemberName, *TargetClass);
		return FString();
	}

	// Create the variable get node with an external member reference
	UK2Node_VariableGet* GetNode = NewObject<UK2Node_VariableGet>(Graph);
	GetNode->VariableReference.SetExternalMember(FName(*MemberName), OwnerClass);

	Graph->AddNode(GetNode, false, false);
	GetNode->CreateNewGuid();
	GetNode->PostPlacedNewNode();
	GetNode->AllocateDefaultPins();

	GetNode->NodePosX = PosX;
	GetNode->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddMemberGetNode: Added member get for '%s::%s' in %s"), *TargetClass, *MemberName, *GraphName);

	return GetNode->NodeGuid.ToString();
}

FString UBlueprintService::AddValidatedGetNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& VariableName,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddValidatedGetNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	// Special handling for Animation Blueprints and EventGraph
	UEdGraph* Graph = nullptr;
	UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Blueprint);
	if (AnimBP && GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
	{
		for (UEdGraph* UberGraph : Blueprint->UbergraphPages)
		{
			if (UberGraph && UberGraph->GetFName() == UEdGraphSchema_K2::GN_EventGraph)
			{
				Graph = UberGraph;
				break;
			}
		}
	}

	if (!Graph)
	{
		Graph = FindGraph(Blueprint, GraphName);
	}

	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddValidatedGetNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	// Find the variable property
	FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, FName(*VariableName));
	if (!Property)
	{
		UE_LOG(LogTemp, Error, TEXT("AddValidatedGetNode: Variable '%s' not found in %s"), *VariableName, *BlueprintPath);
		return FString();
	}

	// Create the get variable node
	UK2Node_VariableGet* GetNode = NewObject<UK2Node_VariableGet>(Graph);
	GetNode->VariableReference.SetSelfMember(FName(*VariableName));

	// Set to non-pure (impure) variation before AllocateDefaultPins so the node
	// gets execution pins. AllocateDefaultPins -> CreateImpurePins will auto-
	// select ValidatedObject for object references (or Branch for primitives).
	if (FEnumProperty* VariationProp = FindFProperty<FEnumProperty>(UK2Node_VariableGet::StaticClass(), TEXT("CurrentVariation")))
	{
		FNumericProperty* UnderlyingProp = VariationProp->GetUnderlyingProperty();
		void* PropContainer = VariationProp->ContainerPtrToValuePtr<void>(GetNode);
		UnderlyingProp->SetIntPropertyValue(PropContainer, (int64)EGetNodeVariation::ValidatedObject);
	}

	// Add to graph
	Graph->AddNode(GetNode, false, false);
	GetNode->CreateNewGuid();
	GetNode->PostPlacedNewNode();
	GetNode->AllocateDefaultPins();

	// Set position
	GetNode->NodePosX = PosX;
	GetNode->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddValidatedGetNode: Added validated get for '%s' in %s"), *VariableName, *GraphName);

	return GetNode->NodeGuid.ToString();
}

FString UBlueprintService::AddSetVariableNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& VariableName,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddSetVariableNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	// Special handling for Animation Blueprints and EventGraph
	UEdGraph* Graph = nullptr;
	UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Blueprint);
	if (AnimBP && GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
	{
		// AnimBPs may store EventGraph in UbergraphPages
		for (UEdGraph* UberGraph : Blueprint->UbergraphPages)
		{
			if (UberGraph && UberGraph->GetFName() == UEdGraphSchema_K2::GN_EventGraph)
			{
				Graph = UberGraph;
				break;
			}
		}
	}
	
	if (!Graph)
	{
		Graph = FindGraph(Blueprint, GraphName);
	}

	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddSetVariableNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	// Validate variable exists — check compiled GeneratedClass first, fall back to NewVariables
	// (uncompiled BPs won't have the property in GeneratedClass yet)
	bool bVariableFound = false;
	if (Blueprint->GeneratedClass && FindFProperty<FProperty>(Blueprint->GeneratedClass, FName(*VariableName)))
	{
		bVariableFound = true;
	}
	if (!bVariableFound)
	{
		for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables)
		{
			if (VarDesc.VarName == FName(*VariableName))
			{
				bVariableFound = true;
				break;
			}
		}
	}
	if (!bVariableFound)
	{
		UE_LOG(LogTemp, Error, TEXT("AddSetVariableNode: Variable '%s' not found in %s"), *VariableName, *BlueprintPath);
		return FString();
	}

	// Create the set variable node
	UK2Node_VariableSet* SetNode = NewObject<UK2Node_VariableSet>(Graph);
	SetNode->VariableReference.SetSelfMember(FName(*VariableName));

	// Add to graph
	Graph->AddNode(SetNode, false, false);
	SetNode->CreateNewGuid();
	SetNode->PostPlacedNewNode();
	SetNode->AllocateDefaultPins();

	// Set position
	SetNode->NodePosX = PosX;
	SetNode->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddSetVariableNode: Added set node for '%s' in %s"), *VariableName, *GraphName);

	return SetNode->NodeGuid.ToString();
}

FString UBlueprintService::AddBranchNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddBranchNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddBranchNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	// Create the branch node
	UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(Graph);

	// Add to graph
	Graph->AddNode(BranchNode, false, false);
	BranchNode->CreateNewGuid();
	BranchNode->PostPlacedNewNode();
	BranchNode->AllocateDefaultPins();

	// Set position
	BranchNode->NodePosX = PosX;
	BranchNode->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddBranchNode: Added branch node to %s"), *GraphName);

	return BranchNode->NodeGuid.ToString();
}

FString UBlueprintService::AddCastNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& TargetClass,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddCastNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddCastNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	// Find the target class
	UClass* TargetUClass = FindFirstObject<UClass>(*TargetClass, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("AddCastNode"));
	if (!TargetUClass)
	{
		UE_LOG(LogTemp, Error, TEXT("AddCastNode: Class '%s' not found"), *TargetClass);
		return FString();
	}

	// Create the cast node
	UK2Node_DynamicCast* CastNode = NewObject<UK2Node_DynamicCast>(Graph);
	CastNode->TargetType = TargetUClass;

	// Add to graph
	Graph->AddNode(CastNode, false, false);
	CastNode->CreateNewGuid();
	CastNode->PostPlacedNewNode();
	CastNode->AllocateDefaultPins();

	// Set position
	CastNode->NodePosX = PosX;
	CastNode->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddCastNode: Added cast to '%s' in %s"), *TargetClass, *GraphName);

	return CastNode->NodeGuid.ToString();
}

FString UBlueprintService::AddEventNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& EventName,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddEventNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	// For AnimBlueprints, check UbergraphPages for EventGraph first
	UEdGraph* Graph = nullptr;
	UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Blueprint);
	if (AnimBP && GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
	{
		for (UEdGraph* UberGraph : Blueprint->UbergraphPages)
		{
			if (UberGraph && UberGraph->GetFName() == UEdGraphSchema_K2::GN_EventGraph)
			{
				Graph = UberGraph;
				break;
			}
		}
	}
	
	if (!Graph)
	{
		Graph = FindGraph(Blueprint, GraphName);
	}

	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddEventNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	// Try to find the event function
	UFunction* EventFunction = nullptr;
	
	// Check parent class for the event function
	if (Blueprint->ParentClass)
	{
		EventFunction = Blueprint->ParentClass->FindFunctionByName(FName(*EventName));
	}

	if (!EventFunction)
	{
		UE_LOG(LogTemp, Error, TEXT("AddEventNode: Event function '%s' not found in parent class"), *EventName);
		return FString();
	}

	// Create the event node
	UK2Node_Event* EventNode = NewObject<UK2Node_Event>(Graph);
	EventNode->EventReference.SetExternalMember(FName(*EventName), Blueprint->ParentClass);
	EventNode->bOverrideFunction = true;

	// Add to graph
	Graph->AddNode(EventNode, false, false);
	EventNode->CreateNewGuid();
	EventNode->PostPlacedNewNode();
	EventNode->AllocateDefaultPins();

	// Set position
	EventNode->NodePosX = PosX;
	EventNode->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddEventNode: Added event '%s' in %s"), *EventName, *GraphName);

	return EventNode->NodeGuid.ToString();
}

FString UBlueprintService::AddCustomEventNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& EventName,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddCustomEventNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	UEdGraph* Graph = ResolveBlueprintGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddCustomEventNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	const FName CustomEventName = EventName.IsEmpty() ? NAME_None : FName(*EventName);
	UBlueprintEventNodeSpawner* EventSpawner = UBlueprintEventNodeSpawner::Create(UK2Node_CustomEvent::StaticClass(), CustomEventName);
	if (!EventSpawner)
	{
		UE_LOG(LogTemp, Error, TEXT("AddCustomEventNode: Failed to create event spawner for '%s'"), *EventName);
		return FString();
	}

	UEdGraphNode* SpawnedNode = EventSpawner->Invoke(Graph, IBlueprintNodeBinder::FBindingSet(), FVector2D(PosX, PosY));
	if (!SpawnedNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AddCustomEventNode: Failed to spawn custom event '%s'"), *EventName);
		return FString();
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddCustomEventNode: Added custom event '%s' in %s"), *EventName, *GraphName);

	return SpawnedNode->NodeGuid.ToString();
}

FString UBlueprintService::AddCreateEventNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& FunctionName,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddCreateEventNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	UEdGraph* Graph = ResolveBlueprintGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddCreateEventNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	UK2Node_CreateDelegate* CreateDelegateNode = NewObject<UK2Node_CreateDelegate>(Graph);

	Graph->AddNode(CreateDelegateNode, false, false);
	CreateDelegateNode->CreateNewGuid();
	CreateDelegateNode->PostPlacedNewNode();
	CreateDelegateNode->AllocateDefaultPins();

	if (!FunctionName.IsEmpty())
	{
		CreateDelegateNode->SetFunction(FName(*FunctionName));
	}

	CreateDelegateNode->NodePosX = PosX;
	CreateDelegateNode->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddCreateEventNode: Added create event node for '%s' in %s"), *FunctionName, *GraphName);

	return CreateDelegateNode->NodeGuid.ToString();
}

FString UBlueprintService::AddInputActionNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& InputActionPath,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddInputActionNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	// Find the graph
	UEdGraph* Graph = nullptr;
	if (GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
	{
		for (UEdGraph* UberGraph : Blueprint->UbergraphPages)
		{
			if (UberGraph && UberGraph->GetFName() == UEdGraphSchema_K2::GN_EventGraph)
			{
				Graph = UberGraph;
				break;
			}
		}
	}
	
	if (!Graph)
	{
		Graph = FindGraph(Blueprint, GraphName);
	}

	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddInputActionNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	// Load the Input Action asset
	UInputAction* InputAction = Cast<UInputAction>(UEditorAssetLibrary::LoadAsset(InputActionPath));
	if (!InputAction)
	{
		UE_LOG(LogTemp, Error, TEXT("AddInputActionNode: Failed to load Input Action asset: %s"), *InputActionPath);
		return FString();
	}

	// Create the Enhanced Input Action node
	UK2Node_EnhancedInputAction* InputActionNode = NewObject<UK2Node_EnhancedInputAction>(Graph);
	InputActionNode->InputAction = InputAction;

	// Add to graph
	Graph->AddNode(InputActionNode, false, false);
	InputActionNode->CreateNewGuid();
	InputActionNode->PostPlacedNewNode();
	InputActionNode->AllocateDefaultPins();

	// Set position
	InputActionNode->NodePosX = PosX;
	InputActionNode->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddInputActionNode: Added Enhanced Input Action '%s' in %s"), *InputAction->GetName(), *GraphName);

	return InputActionNode->NodeGuid.ToString();
}

FString UBlueprintService::AddPrintStringNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddPrintStringNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddPrintStringNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	// Create the print string node
	UK2Node_CallFunction* PrintNode = NewObject<UK2Node_CallFunction>(Graph);

	// Set the function to call
	UFunction* PrintStringFunc = UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString));
	if (PrintStringFunc)
	{
		PrintNode->SetFromFunction(PrintStringFunc);
	}

	// Add to graph
	Graph->AddNode(PrintNode, false, false);
	PrintNode->CreateNewGuid();
	PrintNode->PostPlacedNewNode();
	PrintNode->AllocateDefaultPins();

	// Set position
	PrintNode->NodePosX = PosX;
	PrintNode->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddPrintStringNode: Added print string node to %s"), *GraphName);

	return PrintNode->NodeGuid.ToString();
}

bool UBlueprintService::ConnectNodes(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& SourceNodeId,
	const FString& SourcePinName,
	const FString& TargetNodeId,
	const FString& TargetPinName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("ConnectNodes: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("ConnectNodes: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return false;
	}

	// Find source node
	UEdGraphNode* SourceNode = FindNodeById(Graph, SourceNodeId);
	if (!SourceNode)
	{
		UE_LOG(LogTemp, Error, TEXT("ConnectNodes: Source node '%s' not found"), *SourceNodeId);
		return false;
	}

	// Find target node
	UEdGraphNode* TargetNode = FindNodeById(Graph, TargetNodeId);
	if (!TargetNode)
	{
		UE_LOG(LogTemp, Error, TEXT("ConnectNodes: Target node '%s' not found"), *TargetNodeId);
		return false;
	}

	// Ensure pins are allocated — default auto-placed K2Node_Event nodes (BeginPlay, Tick)
	// may have an empty Pins array until AllocateDefaultPins() is called explicitly.
	if (SourceNode->Pins.Num() == 0)
	{
		SourceNode->AllocateDefaultPins();
	}
	if (TargetNode->Pins.Num() == 0)
	{
		TargetNode->AllocateDefaultPins();
	}

	// Normalise Branch node pin name aliases: editor shows True/False, internal names are then/else.
	auto NormalisePinName = [](const FString& Name) -> FString
	{
		if (Name.Equals(TEXT("True"), ESearchCase::IgnoreCase))  return TEXT("then");
		if (Name.Equals(TEXT("False"), ESearchCase::IgnoreCase)) return TEXT("else");
		return Name;
	};
	const FString ResolvedSourcePin = NormalisePinName(SourcePinName);
	const FString ResolvedTargetPin = NormalisePinName(TargetPinName);

	// Find source pin (output)
	UEdGraphPin* SourcePin = nullptr;
	for (UEdGraphPin* Pin : SourceNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output &&
			(Pin->PinName.ToString().Equals(ResolvedSourcePin, ESearchCase::IgnoreCase) ||
			 Pin->PinName == FName(*ResolvedSourcePin)))
		{
			SourcePin = Pin;
			break;
		}
	}

	if (!SourcePin)
	{
		UE_LOG(LogTemp, Error, TEXT("ConnectNodes: Source pin '%s' not found on node '%s'"), *SourcePinName, *SourceNodeId);
		return false;
	}

	// Find target pin (input)
	UEdGraphPin* TargetPin = nullptr;
	for (UEdGraphPin* Pin : TargetNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input &&
			(Pin->PinName.ToString().Equals(ResolvedTargetPin, ESearchCase::IgnoreCase) ||
			 Pin->PinName == FName(*ResolvedTargetPin)))
		{
			TargetPin = Pin;
			break;
		}
	}

	if (!TargetPin)
	{
		UE_LOG(LogTemp, Error, TEXT("ConnectNodes: Target pin '%s' not found on node '%s'"), *TargetPinName, *TargetNodeId);
		return false;
	}

	// Make the connection
	const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema());
	if (Schema)
	{
		bool bConnected = Schema->TryCreateConnection(SourcePin, TargetPin);
		if (bConnected)
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
			UE_LOG(LogTemp, Log, TEXT("ConnectNodes: Connected '%s'.'%s' to '%s'.'%s'"),
				*SourceNodeId, *SourcePinName, *TargetNodeId, *TargetPinName);
			return true;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ConnectNodes: TryCreateConnection failed for '%s'.'%s' -> '%s'.'%s' (type mismatch or incompatible pins)"),
				*SourceNodeId, *SourcePinName, *TargetNodeId, *TargetPinName);
			return false;
		}
	}

	UE_LOG(LogTemp, Error, TEXT("ConnectNodes: Failed to get schema for graph '%s'"), *GraphName);
	return false;
}

TArray<FBlueprintNodeInfo> UBlueprintService::GetNodesInGraph(
	const FString& BlueprintPath,
	const FString& GraphName)
{
	TArray<FBlueprintNodeInfo> NodeInfos;

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return NodeInfos;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		return NodeInfos;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		FBlueprintNodeInfo NodeInfo;
		NodeInfo.NodeId = Node->NodeGuid.ToString();
		NodeInfo.NodeType = Node->GetClass()->GetName();
		NodeInfo.NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		NodeInfo.PosX = Node->NodePosX;
		NodeInfo.PosY = Node->NodePosY;

		// Get pin names (for quick reference)
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin)
			{
				NodeInfo.PinNames.Add(Pin->PinName.ToString());

				// Also add detailed pin info
				FBlueprintPinInfo PinInfo;
				PinInfo.PinName = Pin->PinName.ToString();
				PinInfo.PinType = Pin->PinType.PinCategory.ToString();
				PinInfo.bIsInput = (Pin->Direction == EGPD_Input);
				PinInfo.bIsConnected = Pin->LinkedTo.Num() > 0;
				PinInfo.DefaultValue = Pin->DefaultValue;
				NodeInfo.Pins.Add(PinInfo);
			}
		}

		NodeInfos.Add(NodeInfo);
	}

	return NodeInfos;
}

FBlueprintCompileResult UBlueprintService::CompileBlueprint(const FString& BlueprintPath)
{
	FBlueprintCompileResult Result;

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("CompileBlueprint: Failed to load blueprint: %s"), *BlueprintPath);
		Result.Errors.Add(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
		Result.NumErrors = 1;
		return Result;
	}

	FCompilerResultsLog CompileResults;
	CompileResults.bSilentMode = false;
	CompileResults.bLogInfoOnly = false;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &CompileResults);

	Result.bSuccess = (Blueprint->Status != BS_Error);
	Result.NumErrors = CompileResults.NumErrors;
	Result.NumWarnings = CompileResults.NumWarnings;

	for (const TSharedRef<FTokenizedMessage>& Msg : CompileResults.Messages)
	{
		const FString MsgText = Msg->ToText().ToString();
		if (Msg->GetSeverity() == EMessageSeverity::Error)
		{
			Result.Errors.Add(MsgText);
		}
		else if (Msg->GetSeverity() == EMessageSeverity::Warning || Msg->GetSeverity() == EMessageSeverity::PerformanceWarning)
		{
			Result.Warnings.Add(MsgText);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("CompileBlueprint: Compiled %s - Success: %s, Errors: %d, Warnings: %d"),
		*BlueprintPath,
		Result.bSuccess ? TEXT("true") : TEXT("false"),
		Result.NumErrors,
		Result.NumWarnings);

	return Result;
}

// ============================================================================
// ADVANCED NODE OPERATIONS (Phase 4)
// ============================================================================

FString UBlueprintService::AddFunctionCallNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& FunctionOwnerClass,
	const FString& FunctionName,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddFunctionCallNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddFunctionCallNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	// Find the class that owns the function
	UClass* OwnerClass = nullptr;
	UFunction* Function = nullptr;
	UEdGraphNode* SpawnedNode = nullptr;
	
	// Check if this is a self-function call
	if (FunctionOwnerClass.IsEmpty() || FunctionOwnerClass.Equals(TEXT("Self"), ESearchCase::IgnoreCase))
	{
		// First check the generated class for compiled functions
		if (Blueprint->GeneratedClass)
		{
			Function = Blueprint->GeneratedClass->FindFunctionByName(FName(*FunctionName));
		}
		
		// If not found, check function graphs for user-defined functions
		if (!Function)
		{
			for (UEdGraph* FuncGraph : Blueprint->FunctionGraphs)
			{
				if (FuncGraph && FuncGraph->GetFName() == FName(*FunctionName))
				{
					// Found the function graph, use the generated class function
					if (Blueprint->GeneratedClass)
					{
						Function = Blueprint->GeneratedClass->FindFunctionByName(FName(*FunctionName));
					}
					break;
				}
			}
		}
		
		if (Function)
		{
			OwnerClass = Blueprint->GeneratedClass;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("AddFunctionCallNode: Self function '%s' not found in blueprint"), *FunctionName);
			return FString();
		}
	}
	else
	{
		// Map common class names to their actual classes
		if (FunctionOwnerClass.Equals(TEXT("KismetMathLibrary"), ESearchCase::IgnoreCase))
		{
			OwnerClass = UKismetMathLibrary::StaticClass();
		}
		else if (FunctionOwnerClass.Equals(TEXT("KismetSystemLibrary"), ESearchCase::IgnoreCase))
		{
			OwnerClass = UKismetSystemLibrary::StaticClass();
		}
		else if (FunctionOwnerClass.Equals(TEXT("KismetStringLibrary"), ESearchCase::IgnoreCase))
		{
			OwnerClass = UKismetStringLibrary::StaticClass();
		}
		else if (FunctionOwnerClass.Equals(TEXT("KismetArrayLibrary"), ESearchCase::IgnoreCase))
		{
			OwnerClass = UKismetArrayLibrary::StaticClass();
		}
		else if (FunctionOwnerClass.Equals(TEXT("GameplayStatics"), ESearchCase::IgnoreCase))
		{
			OwnerClass = UGameplayStatics::StaticClass();
		}
		else
		{
			OwnerClass = ResolveClassByName(FunctionOwnerClass);
		}

		if (!OwnerClass)
		{
			UE_LOG(LogTemp, Error, TEXT("AddFunctionCallNode: Class '%s' not found"), *FunctionOwnerClass);
			return FString();
		}
		
		// Find the function
		Function = OwnerClass->FindFunctionByName(FName(*FunctionName));
		if (!Function)
		{
			if (UBlueprintFunctionNodeSpawner* FunctionSpawner = FindBestFunctionSpawner(Blueprint, Graph, OwnerClass, FunctionName))
			{
				SpawnedNode = FunctionSpawner->Invoke(Graph, IBlueprintNodeBinder::FBindingSet(), FVector2D(PosX, PosY));
				if (SpawnedNode)
				{
					FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
					if (const UFunction* ResolvedFunction = FunctionSpawner->GetFunction())
					{
						UE_LOG(LogTemp, Log, TEXT("AddFunctionCallNode: Resolved '%s::%s' via node spawner fallback to %s::%s"), *FunctionOwnerClass, *FunctionName, *ResolvedFunction->GetOwnerClass()->GetName(), *ResolvedFunction->GetName());
					}
					return SpawnedNode->NodeGuid.ToString();
				}

				UE_LOG(LogTemp, Error, TEXT("AddFunctionCallNode: Spawner fallback matched '%s' in class '%s' but failed to invoke"), *FunctionName, *FunctionOwnerClass);
				return FString();
			}

			UE_LOG(LogTemp, Error, TEXT("AddFunctionCallNode: Function '%s' not found in class '%s'"), *FunctionName, *FunctionOwnerClass);
			return FString();
		}
	}

	// Create the call function node
	UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
	CallNode->SetFromFunction(Function);

	// Add to graph
	Graph->AddNode(CallNode, false, false);
	CallNode->CreateNewGuid();
	CallNode->PostPlacedNewNode();
	CallNode->AllocateDefaultPins();

	// Set position
	CallNode->NodePosX = PosX;
	CallNode->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddFunctionCallNode: Added %s::%s to %s"), *FunctionOwnerClass, *FunctionName, *GraphName);

	return CallNode->NodeGuid.ToString();
}

FString UBlueprintService::AddComparisonNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& ComparisonType,
	const FString& ValueType,
	float PosX,
	float PosY)
{
	// Build the function name based on comparison type and value type
	FString FunctionName;
	
	// UE 5.7: Float operations are now Double - normalize the type
	FString NormalizedType = ValueType;
	if (ValueType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
	{
		NormalizedType = TEXT("Double");
	}
	
	if (ComparisonType.Equals(TEXT("Greater"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("Greater_%s%s"), *NormalizedType, *NormalizedType);
	}
	else if (ComparisonType.Equals(TEXT("Less"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("Less_%s%s"), *NormalizedType, *NormalizedType);
	}
	else if (ComparisonType.Equals(TEXT("GreaterEqual"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("GreaterEqual_%s%s"), *NormalizedType, *NormalizedType);
	}
	else if (ComparisonType.Equals(TEXT("LessEqual"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("LessEqual_%s%s"), *NormalizedType, *NormalizedType);
	}
	else if (ComparisonType.Equals(TEXT("Equal"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("EqualEqual_%s%s"), *NormalizedType, *NormalizedType);
	}
	else if (ComparisonType.Equals(TEXT("NotEqual"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("NotEqual_%s%s"), *NormalizedType, *NormalizedType);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AddComparisonNode: Unknown comparison type '%s'"), *ComparisonType);
		return FString();
	}

	return AddFunctionCallNode(BlueprintPath, GraphName, TEXT("KismetMathLibrary"), FunctionName, PosX, PosY);
}

FString UBlueprintService::AddMathNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& MathOperation,
	const FString& ValueType,
	float PosX,
	float PosY)
{
	// Build the function name based on operation and value type
	FString FunctionName;
	
	// UE 5.7: Float operations are now Double - normalize the type
	FString NormalizedType = ValueType;
	if (ValueType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
	{
		NormalizedType = TEXT("Double");
	}
	
	if (MathOperation.Equals(TEXT("Add"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("Add_%s%s"), *NormalizedType, *NormalizedType);
	}
	else if (MathOperation.Equals(TEXT("Subtract"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("Subtract_%s%s"), *NormalizedType, *NormalizedType);
	}
	else if (MathOperation.Equals(TEXT("Multiply"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("Multiply_%s%s"), *NormalizedType, *NormalizedType);
	}
	else if (MathOperation.Equals(TEXT("Divide"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("Divide_%s%s"), *NormalizedType, *NormalizedType);
	}
	else if (MathOperation.Equals(TEXT("Clamp"), ESearchCase::IgnoreCase))
	{
		// Clamp has a different naming convention
		if (ValueType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
		{
			FunctionName = TEXT("FClamp");
		}
		else if (ValueType.Equals(TEXT("Int"), ESearchCase::IgnoreCase))
		{
			FunctionName = TEXT("Clamp");
		}
		else if (ValueType.Equals(TEXT("Double"), ESearchCase::IgnoreCase))
		{
			FunctionName = TEXT("FClamp64");
		}
		else
		{
			FunctionName = TEXT("FClamp");
		}
	}
	else if (MathOperation.Equals(TEXT("Min"), ESearchCase::IgnoreCase))
	{
		if (ValueType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
		{
			FunctionName = TEXT("FMin");
		}
		else
		{
			FunctionName = TEXT("Min");
		}
	}
	else if (MathOperation.Equals(TEXT("Max"), ESearchCase::IgnoreCase))
	{
		if (ValueType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
		{
			FunctionName = TEXT("FMax");
		}
		else
		{
			FunctionName = TEXT("Max");
		}
	}
	else if (MathOperation.Equals(TEXT("Abs"), ESearchCase::IgnoreCase))
	{
		FunctionName = TEXT("Abs");
	}
	else if (MathOperation.Equals(TEXT("Negate"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("Negate_%s"), *NormalizedType);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AddMathNode: Unknown math operation '%s'"), *MathOperation);
		return FString();
	}

	return AddFunctionCallNode(BlueprintPath, GraphName, TEXT("KismetMathLibrary"), FunctionName, PosX, PosY);
}

TArray<FBlueprintConnectionInfo> UBlueprintService::GetConnections(
	const FString& BlueprintPath,
	const FString& GraphName)
{
	TArray<FBlueprintConnectionInfo> Connections;

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return Connections;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		return Connections;
	}

	// Track which connections we've already added to avoid duplicates
	TSet<FString> AddedConnections;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output)
			{
				continue;
			}

			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (!LinkedPin || !LinkedPin->GetOwningNode())
				{
					continue;
				}

				// Create a unique key for this connection to avoid duplicates
				FString ConnectionKey = FString::Printf(TEXT("%s.%s->%s.%s"),
					*Node->NodeGuid.ToString(),
					*Pin->PinName.ToString(),
					*LinkedPin->GetOwningNode()->NodeGuid.ToString(),
					*LinkedPin->PinName.ToString());

				if (AddedConnections.Contains(ConnectionKey))
				{
					continue;
				}
				AddedConnections.Add(ConnectionKey);

				FBlueprintConnectionInfo Connection;
				Connection.SourceNodeId = Node->NodeGuid.ToString();
				Connection.SourceNodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
				Connection.SourcePinName = Pin->PinName.ToString();
				Connection.TargetNodeId = LinkedPin->GetOwningNode()->NodeGuid.ToString();
				Connection.TargetNodeTitle = LinkedPin->GetOwningNode()->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
				Connection.TargetPinName = LinkedPin->PinName.ToString();

				Connections.Add(Connection);
			}
		}
	}

	return Connections;
}

TArray<FBlueprintPinInfo> UBlueprintService::GetNodePins(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId)
{
	TArray<FBlueprintPinInfo> PinInfos;

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return PinInfos;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		return PinInfos;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("GetNodePins: Node '%s' not found"), *NodeId);
		return PinInfos;
	}

	// Default auto-placed event nodes may have an empty Pins array — allocate if needed.
	if (Node->Pins.Num() == 0)
	{
		Node->AllocateDefaultPins();
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin)
		{
			continue;
		}

		FBlueprintPinInfo PinInfo;
		PinInfo.PinName = Pin->PinName.ToString();
		PinInfo.PinType = Pin->PinType.PinCategory.ToString();
		PinInfo.bIsInput = (Pin->Direction == EGPD_Input);
		PinInfo.bIsConnected = Pin->LinkedTo.Num() > 0;
		PinInfo.DefaultValue = Pin->DefaultValue;

		PinInfos.Add(PinInfo);
	}

	return PinInfos;
}

bool UBlueprintService::DisconnectPin(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId,
	const FString& PinName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("DisconnectPin: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("DisconnectPin: Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("DisconnectPin: Node '%s' not found"), *NodeId);
		return false;
	}

	// Find the pin
	UEdGraphPin* TargetPin = nullptr;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			TargetPin = Pin;
			break;
		}
	}

	if (!TargetPin)
	{
		UE_LOG(LogTemp, Error, TEXT("DisconnectPin: Pin '%s' not found on node '%s'"), *PinName, *NodeId);
		return false;
	}

	if (TargetPin->LinkedTo.Num() == 0)
	{
		return true; // Already disconnected
	}

	// Break all connections
	TargetPin->BreakAllPinLinks();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("DisconnectPin: Disconnected pin '%s' on node '%s'"), *PinName, *NodeId);
	return true;
}

bool UBlueprintService::DeleteNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("DeleteNode: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("DeleteNode: Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("DeleteNode: Node '%s' not found"), *NodeId);
		return false;
	}

	// Don't delete entry or result nodes
	if (Node->IsA<UK2Node_FunctionEntry>() || Node->IsA<UK2Node_FunctionResult>())
	{
		UE_LOG(LogTemp, Error, TEXT("DeleteNode: Cannot delete function entry or result nodes"));
		return false;
	}

	// Break all connections first
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin)
		{
			Pin->BreakAllPinLinks();
		}
	}

	// Remove the node
	Graph->RemoveNode(Node);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("DeleteNode: Deleted node '%s' from graph '%s'"), *NodeId, *GraphName);
	return true;
}

bool UBlueprintService::SetNodePosition(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNodePosition: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNodePosition: Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNodePosition: Node '%s' not found"), *NodeId);
		return false;
	}

	// Set the position
	Node->NodePosX = static_cast<int32>(PosX);
	Node->NodePosY = static_cast<int32>(PosY);
	
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("SetNodePosition: Moved node '%s' to (%d, %d)"), *NodeId, Node->NodePosX, Node->NodePosY);
	return true;
}

FString UBlueprintService::CreateBlueprint(
	const FString& BlueprintName,
	const FString& ParentClass,
	const FString& BlueprintPath)
{
	if (BlueprintName.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("CreateBlueprint: Blueprint name is empty"));
		return FString();
	}

	// Determine parent class
	UClass* ParentClassPtr = AActor::StaticClass(); // Default to Actor
	if (!ParentClass.IsEmpty())
	{
		// Try common class names directly
		if (ParentClass.Equals(TEXT("Actor"), ESearchCase::IgnoreCase))
		{
			ParentClassPtr = AActor::StaticClass();
		}
		else if (ParentClass.Equals(TEXT("Pawn"), ESearchCase::IgnoreCase))
		{
			ParentClassPtr = APawn::StaticClass();
		}
		else if (ParentClass.Equals(TEXT("Character"), ESearchCase::IgnoreCase))
		{
			ParentClassPtr = ACharacter::StaticClass();
		}
		else if (ParentClass.Equals(TEXT("PlayerController"), ESearchCase::IgnoreCase))
		{
			ParentClassPtr = APlayerController::StaticClass();
		}
		else
		{
			// Try to find the class by full path first
			ParentClassPtr = FindObject<UClass>(nullptr, *ParentClass);
			if (!ParentClassPtr)
			{
				// Try with /Script/Engine. prefix
				FString FullPath = FString::Printf(TEXT("/Script/Engine.%s"), *ParentClass);
				ParentClassPtr = FindObject<UClass>(nullptr, *FullPath);
			}
			if (!ParentClassPtr)
			{
				// Search all loaded UClass objects by short name (catches plugin classes like StateTreeTaskBlueprintBase)
				for (TObjectIterator<UClass> It; It; ++It)
				{
					if (It->GetName().Equals(ParentClass, ESearchCase::IgnoreCase) ||
						It->GetName().Equals(FString(TEXT("U")) + ParentClass, ESearchCase::IgnoreCase) ||
						It->GetName().Equals(FString(TEXT("A")) + ParentClass, ESearchCase::IgnoreCase))
					{
						ParentClassPtr = *It;
						UE_LOG(LogTemp, Log, TEXT("CreateBlueprint: Resolved parent class '%s' via object search to '%s'"), *ParentClass, *It->GetPathName());
						break;
					}
				}
			}
			if (!ParentClassPtr)
			{
				// Return error rather than silently creating with wrong parent
				UE_LOG(LogTemp, Error, TEXT("CreateBlueprint: Parent class '%s' not found. Use the full class path (e.g. '/Script/ModuleName.ClassName') or ensure the module is loaded."), *ParentClass);
				return FString();
			}
		}
	}

	// Build proper package path
	FString PackagePath = BlueprintPath;
	PackagePath.ReplaceInline(TEXT("\\"), TEXT("/"));
	PackagePath.TrimStartAndEndInline();
	while (PackagePath.EndsWith(TEXT("/")))
	{
		PackagePath.LeftChopInline(1);
	}
	if (PackagePath.IsEmpty())
	{
		PackagePath = TEXT("/Game/Blueprints");
	}

	const FString FullAssetPath = PackagePath + TEXT("/") + BlueprintName;

	// Check if blueprint already exists
	if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateBlueprint: Blueprint already exists at '%s', returning existing path"), *FullAssetPath);
		return FullAssetPath;
	}

	// Create package
	UPackage* Package = CreatePackage(*FullAssetPath);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateBlueprint: Failed to create package for '%s'"), *FullAssetPath);
		return FString();
	}

	// Create blueprint using BlueprintFactory
	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->ParentClass = ParentClassPtr;

	UBlueprint* NewBlueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(
		UBlueprint::StaticClass(),
		Package,
		*BlueprintName,
		RF_Standalone | RF_Public,
		nullptr,
		GWarn
	));

	if (!NewBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateBlueprint: Factory failed to create blueprint '%s'"), *BlueprintName);
		return FString();
	}

	// Notify the asset registry
	FAssetRegistryModule::AssetCreated(NewBlueprint);

	// Mark package dirty
	Package->MarkPackageDirty();

	// Save the asset
	if (!UEditorAssetLibrary::SaveAsset(NewBlueprint->GetPathName(), false))
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateBlueprint: Created blueprint but failed to save"));
	}

	UE_LOG(LogTemp, Log, TEXT("CreateBlueprint: Created blueprint '%s' at '%s'"), *BlueprintName, *NewBlueprint->GetPathName());
	return NewBlueprint->GetPathName();
}

bool UBlueprintService::GetProperty(
	const FString& BlueprintPath,
	const FString& PropertyName,
	FString& OutValue)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("GetProperty: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UClass* GeneratedClass = Blueprint->GeneratedClass;
	if (!GeneratedClass)
	{
		UE_LOG(LogTemp, Error, TEXT("GetProperty: Blueprint has no generated class"));
		return false;
	}

	UObject* CDO = GeneratedClass->GetDefaultObject();
	if (!CDO)
	{
		UE_LOG(LogTemp, Error, TEXT("GetProperty: Failed to get CDO"));
		return false;
	}

	// Find property
	FProperty* Property = GeneratedClass->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		UE_LOG(LogTemp, Error, TEXT("GetProperty: Property '%s' not found"), *PropertyName);
		return false;
	}

	// Export property value to string
	const void* PropertyValue = Property->ContainerPtrToValuePtr<void>(CDO);
	Property->ExportTextItem_Direct(OutValue, PropertyValue, nullptr, nullptr, PPF_None);

	UE_LOG(LogTemp, Log, TEXT("GetProperty: Got property '%s' = '%s'"), *PropertyName, *OutValue);
	return true;
}

bool UBlueprintService::SetProperty(
	const FString& BlueprintPath,
	const FString& PropertyName,
	const FString& PropertyValue)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SetProperty: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UClass* GeneratedClass = Blueprint->GeneratedClass;
	if (!GeneratedClass)
	{
		UE_LOG(LogTemp, Error, TEXT("SetProperty: Blueprint has no generated class"));
		return false;
	}

	UObject* CDO = GeneratedClass->GetDefaultObject();
	if (!CDO)
	{
		UE_LOG(LogTemp, Error, TEXT("SetProperty: Failed to get CDO"));
		return false;
	}

	// Find property
	FProperty* Property = GeneratedClass->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		UE_LOG(LogTemp, Error, TEXT("SetProperty: Property '%s' not found"), *PropertyName);
		return false;
	}

	// Import property value from string
	void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(CDO);
	Property->ImportText_Direct(*PropertyValue, PropertyAddr, nullptr, PPF_None);

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UEditorAssetLibrary::SaveAsset(BlueprintPath, false);

	UE_LOG(LogTemp, Log, TEXT("SetProperty: Set property '%s' = '%s'"), *PropertyName, *PropertyValue);
	return true;
}

bool UBlueprintService::ReparentBlueprint(
	const FString& BlueprintPath,
	const FString& NewParentClass)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("ReparentBlueprint: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	if (NewParentClass.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("ReparentBlueprint: New parent class is empty"));
		return false;
	}

	// Find the new parent class - try common class names first
	UClass* NewParent = nullptr;
	if (NewParentClass.Equals(TEXT("Actor"), ESearchCase::IgnoreCase))
	{
		NewParent = AActor::StaticClass();
	}
	else if (NewParentClass.Equals(TEXT("Pawn"), ESearchCase::IgnoreCase))
	{
		NewParent = APawn::StaticClass();
	}
	else if (NewParentClass.Equals(TEXT("Character"), ESearchCase::IgnoreCase))
	{
		NewParent = ACharacter::StaticClass();
	}
	else if (NewParentClass.Equals(TEXT("PlayerController"), ESearchCase::IgnoreCase))
	{
		NewParent = APlayerController::StaticClass();
	}
	else
	{
		// Try to find by full path first
		NewParent = FindObject<UClass>(nullptr, *NewParentClass);
		if (!NewParent)
		{
			// Try with /Script/Engine. prefix
			FString FullPath = FString::Printf(TEXT("/Script/Engine.%s"), *NewParentClass);
			NewParent = FindObject<UClass>(nullptr, *FullPath);
		}
		if (!NewParent)
		{
			// Search all loaded UClass objects by short name (catches plugin classes like StateTreeTaskBlueprintBase)
			for (TObjectIterator<UClass> It; It; ++It)
			{
				if (It->GetName().Equals(NewParentClass, ESearchCase::IgnoreCase) ||
					It->GetName().Equals(FString(TEXT("U")) + NewParentClass, ESearchCase::IgnoreCase) ||
					It->GetName().Equals(FString(TEXT("A")) + NewParentClass, ESearchCase::IgnoreCase))
				{
					NewParent = *It;
					UE_LOG(LogTemp, Log, TEXT("ReparentBlueprint: Resolved parent class '%s' via object search to '%s'"), *NewParentClass, *It->GetPathName());
					break;
				}
			}
		}
	}

	if (!NewParent)
	{
		UE_LOG(LogTemp, Error, TEXT("ReparentBlueprint: New parent class '%s' not found"), *NewParentClass);
		return false;
	}

	// Perform reparenting (UE5 doesn't have FBlueprintEditorUtils::ReparentBlueprint)
	FString OldParentName = Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("None");
	
	// Directly set the parent class
	Blueprint->ParentClass = NewParent;
	
	// Mark for recompilation and save
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	UEditorAssetLibrary::SaveAsset(BlueprintPath, false);
	
	UE_LOG(LogTemp, Log, TEXT("ReparentBlueprint: Reparented '%s' from '%s' to '%s'"),
		*Blueprint->GetName(), *OldParentName, *NewParent->GetName());

	return true;
}

bool UBlueprintService::DiffBlueprints(
	const FString& BlueprintPathA,
	const FString& BlueprintPathB,
	FString& OutDifferences)
{
	UBlueprint* BlueprintA = LoadBlueprint(BlueprintPathA);
	UBlueprint* BlueprintB = LoadBlueprint(BlueprintPathB);

	if (!BlueprintA || !BlueprintB)
	{
		UE_LOG(LogTemp, Error, TEXT("DiffBlueprints: Failed to load one or both blueprints"));
		return false;
	}

	TArray<FString> Differences;

	// Compare parent classes
	FString ParentA = BlueprintA->ParentClass ? BlueprintA->ParentClass->GetName() : TEXT("None");
	FString ParentB = BlueprintB->ParentClass ? BlueprintB->ParentClass->GetName() : TEXT("None");
	if (ParentA != ParentB)
	{
		Differences.Add(FString::Printf(TEXT("Parent Class: '%s' vs '%s'"), *ParentA, *ParentB));
	}

	// Compare variables
	TSet<FName> VarsA, VarsB;
	for (const FBPVariableDescription& Var : BlueprintA->NewVariables)
	{
		VarsA.Add(Var.VarName);
	}
	for (const FBPVariableDescription& Var : BlueprintB->NewVariables)
	{
		VarsB.Add(Var.VarName);
	}

	TSet<FName> OnlyInA = VarsA.Difference(VarsB);
	TSet<FName> OnlyInB = VarsB.Difference(VarsA);

	if (OnlyInA.Num() > 0)
	{
		TArray<FString> VarNames;
		for (FName VarName : OnlyInA)
		{
			VarNames.Add(VarName.ToString());
		}
		Differences.Add(FString::Printf(TEXT("Variables only in A: %s"), *FString::Join(VarNames, TEXT(", "))));
	}

	if (OnlyInB.Num() > 0)
	{
		TArray<FString> VarNames;
		for (FName VarName : OnlyInB)
		{
			VarNames.Add(VarName.ToString());
		}
		Differences.Add(FString::Printf(TEXT("Variables only in B: %s"), *FString::Join(VarNames, TEXT(", "))));
	}

	// Compare components
	TArray<FBlueprintComponentInfo> CompsA = ListComponents(BlueprintPathA);
	TArray<FBlueprintComponentInfo> CompsB = ListComponents(BlueprintPathB);

	if (CompsA.Num() != CompsB.Num())
	{
		Differences.Add(FString::Printf(TEXT("Component count: %d vs %d"), CompsA.Num(), CompsB.Num()));
	}

	// Build output
	if (Differences.Num() == 0)
	{
		OutDifferences = TEXT("Blueprints are identical");
		return true; // Return true even when identical so Python gets the output string
	}

	OutDifferences = FString::Join(Differences, TEXT("\n"));
	return true; // Has differences
}

// ============================================================================
// NODE MANAGEMENT - Advanced Operations
// ============================================================================

TArray<FBlueprintNodeTypeInfo> UBlueprintService::DiscoverNodes(
	const FString& BlueprintPath,
	const FString& SearchTerm,
	const FString& Category,
	int32 MaxResults)
{
	TArray<FBlueprintNodeTypeInfo> Results;

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("DiscoverNodes: Failed to load blueprint: %s"), *BlueprintPath);
		return Results;
	}

	FString SearchLower = SearchTerm.ToLower();
	FString CategoryLower = Category.ToLower();
	
	// Track seen spawner keys to avoid duplicates
	TSet<FString> SeenSpawnerKeys;
	UEdGraph* EventGraph = ResolveBlueprintGraph(Blueprint, TEXT("EventGraph"));
	
	// Helper lambda to add a function to results
	auto AddFunctionToResults = [&](UFunction* Func, const FString& InCategory, const FString& OwnerClassName) -> bool
	{
		if (!Func || Results.Num() >= MaxResults)
		{
			return false;
		}
		
		// Only include BlueprintCallable functions
		if (!Func->HasAnyFunctionFlags(FUNC_BlueprintCallable))
		{
			return false;
		}
		
		// Skip hidden/internal functions
		if (Func->HasMetaData(TEXT("BlueprintInternalUseOnly")))
		{
			return false;
		}
		
		FString FuncName = Func->GetName();
		FString DisplayName = Func->GetDisplayNameText().ToString();
		if (DisplayName.IsEmpty())
		{
			DisplayName = FuncName;
		}
		
		// Filter by category if specified
		if (!CategoryLower.IsEmpty())
		{
			if (!InCategory.ToLower().Contains(CategoryLower))
			{
				return false;
			}
		}
		
		// Filter by search term
		if (!SearchLower.IsEmpty())
		{
			bool bMatches = DisplayName.ToLower().Contains(SearchLower) ||
			                FuncName.ToLower().Contains(SearchLower);
			
			// Also check keywords
			FString Keywords = Func->GetMetaData(TEXT("Keywords"));
			if (!Keywords.IsEmpty())
			{
				bMatches = bMatches || Keywords.ToLower().Contains(SearchLower);
			}
			
			if (!bMatches)
			{
				return false;
			}
		}
		
		FString SpawnerKey = FString::Printf(TEXT("FUNC %s::%s"), *OwnerClassName, *FuncName);
		if (SeenSpawnerKeys.Contains(SpawnerKey))
		{
			return false;
		}
		SeenSpawnerKeys.Add(SpawnerKey);
		
		FBlueprintNodeTypeInfo Info;
		Info.DisplayName = DisplayName;
		Info.Category = InCategory;
		Info.NodeClass = TEXT("K2Node_CallFunction");
		Info.SpawnerKey = SpawnerKey;
		Info.bIsPure = Func->HasAnyFunctionFlags(FUNC_BlueprintPure);
		Info.bIsLatent = Func->HasMetaData(TEXT("Latent"));
		Info.Tooltip = Func->GetMetaData(TEXT("ToolTip"));
		
		// Get keywords
		FString Keywords = Func->GetMetaData(TEXT("Keywords"));
		if (!Keywords.IsEmpty())
		{
			Keywords.ParseIntoArray(Info.Keywords, TEXT(","), true);
		}
		
		Results.Add(Info);
		return true;
	};

	auto AddNodeSpawnerToResults = [&](UBlueprintNodeSpawner* NodeSpawner) -> bool
	{
		if (!NodeSpawner || Results.Num() >= MaxResults)
		{
			return false;
		}

		UBlueprintEventNodeSpawner* EventSpawner = Cast<UBlueprintEventNodeSpawner>(NodeSpawner);
		if (!EventSpawner)
		{
			return false;
		}

		if (EventSpawner->IsForCustomEvent())
		{
			return false;
		}

		const UFunction* EventFunction = EventSpawner->GetEventFunction();
		if (EventFunction)
		{
			UClass* OwnerClass = EventFunction->GetOwnerClass();
			if (!OwnerClass || !Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(OwnerClass))
			{
				return false;
			}
		}

		const FString SpawnerKey = BuildEventSpawnerKey(EventSpawner);
		if (SpawnerKey.IsEmpty() || SeenSpawnerKeys.Contains(SpawnerKey))
		{
			return false;
		}

		UEdGraph* UiGraph = EventGraph;
		if (!UiGraph && Blueprint->UbergraphPages.Num() > 0)
		{
			UiGraph = Blueprint->UbergraphPages[0].Get();
		}

		const FBlueprintActionUiSpec& UiSpec = NodeSpawner->PrimeDefaultUiSpec(UiGraph);
		const FString DisplayName = UiSpec.MenuName.ToString();
		const FString Keywords = UiSpec.Keywords.ToString();
		const FString MenuCategory = UiSpec.Category.ToString();

		if (!CategoryLower.IsEmpty() && !MenuCategory.ToLower().Contains(CategoryLower))
		{
			return false;
		}

		if (!SearchLower.IsEmpty())
		{
			const FString EventFunctionName = EventFunction ? EventFunction->GetName() : FString(TEXT("Custom Event"));
			const bool bMatches = DisplayName.ToLower().Contains(SearchLower) ||
				EventFunctionName.ToLower().Contains(SearchLower) ||
				Keywords.ToLower().Contains(SearchLower);

			if (!bMatches)
			{
				return false;
			}
		}

		SeenSpawnerKeys.Add(SpawnerKey);

		FBlueprintNodeTypeInfo Info;
		Info.DisplayName = DisplayName;
		Info.Category = MenuCategory.IsEmpty() ? TEXT("Add Event") : MenuCategory;
		Info.NodeClass = NodeSpawner->NodeClass ? NodeSpawner->NodeClass->GetName() : TEXT("K2Node_Event");
		Info.SpawnerKey = SpawnerKey;
		Info.bIsPure = false;
		Info.bIsLatent = false;
		Info.Tooltip = UiSpec.Tooltip.ToString();

		TArray<FString> ParsedKeywords;
		Keywords.ParseIntoArrayWS(ParsedKeywords);
		Info.Keywords = MoveTemp(ParsedKeywords);

		Results.Add(Info);
		return true;
	};
	
	// 1. Add blueprint's own functions (Self functions)
	if (UBlueprintGeneratedClass* GenClass = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
	{
		for (TFieldIterator<UFunction> It(GenClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if (Results.Num() >= MaxResults) break;
			AddFunctionToResults(*It, TEXT("Self Functions"), TEXT("Self"));
		}
	}
	
	// 2. Add parent class functions by walking the entire class hierarchy
	// This ensures we get Character, Pawn, Actor functions etc.
	UClass* CurrentClass = Blueprint->ParentClass;
	while (CurrentClass && Results.Num() < MaxResults)
	{
		FString ClassName = CurrentClass->GetName();
		FString CategoryStr = FString::Printf(TEXT("Parent: %s"), *ClassName);
		
		// Only get functions defined directly on this class (not inherited)
		for (TFieldIterator<UFunction> It(CurrentClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if (Results.Num() >= MaxResults) break;
			AddFunctionToResults(*It, CategoryStr, ClassName);
		}
		
		// Move up the hierarchy
		CurrentClass = CurrentClass->GetSuperClass();
		
		// Stop at UObject
		if (CurrentClass && CurrentClass->GetName() == TEXT("Object"))
		{
			break;
		}
	}

	// 2.5 Add event spawners from the Blueprint action database.
	{
		const FBlueprintActionDatabase::FActionRegistry& ActionRegistry = FBlueprintActionDatabase::Get().GetAllActions();
		for (const TPair<FObjectKey, FBlueprintActionDatabase::FActionList>& Entry : ActionRegistry)
		{
			if (Results.Num() >= MaxResults)
			{
				break;
			}

			for (UBlueprintNodeSpawner* NodeSpawner : Entry.Value)
			{
				if (Results.Num() >= MaxResults)
				{
					break;
				}

				AddNodeSpawnerToResults(NodeSpawner);
			}
		}
	}

	// 2.6 Surface Add Custom Event with a stable key.
	{
		const FString DisplayName = TEXT("Add Custom Event...");
		const FString Keywords = TEXT("Custom Event Add Event Delegate");
		const FString SpawnerKey = TEXT("EVENT CUSTOM");
		const FString EventCategory = TEXT("Add Event");

		const bool bCategoryMatches = CategoryLower.IsEmpty() || EventCategory.ToLower().Contains(CategoryLower);
		const bool bSearchMatches = SearchLower.IsEmpty() || DisplayName.ToLower().Contains(SearchLower) || Keywords.ToLower().Contains(SearchLower);

		if (bCategoryMatches && bSearchMatches && !SeenSpawnerKeys.Contains(SpawnerKey) && Results.Num() < MaxResults)
		{
			SeenSpawnerKeys.Add(SpawnerKey);

			FBlueprintNodeTypeInfo Info;
			Info.DisplayName = DisplayName;
			Info.Category = EventCategory;
			Info.NodeClass = TEXT("K2Node_CustomEvent");
			Info.SpawnerKey = SpawnerKey;
			Info.bIsPure = false;
			Info.bIsLatent = false;
			Info.Tooltip = TEXT("Add a new custom event entry point to the graph.");
			Info.Keywords = { TEXT("Custom"), TEXT("Event"), TEXT("Delegate") };

			Results.Add(Info);
		}
	}

	// 2.7 Surface Create Event / Create Delegate for delegate workflows.
	{
		const FString DisplayName = TEXT("Create Event");
		const FString Keywords = TEXT("Create Delegate Delegate Event");
		const FString SpawnerKey = TEXT("NODE K2Node_CreateDelegate");
		const FString DelegateCategory = TEXT("Delegates");

		const bool bCategoryMatches = CategoryLower.IsEmpty() || DelegateCategory.ToLower().Contains(CategoryLower);
		const bool bSearchMatches = SearchLower.IsEmpty() || DisplayName.ToLower().Contains(SearchLower) || Keywords.ToLower().Contains(SearchLower);

		if (bCategoryMatches && bSearchMatches && !SeenSpawnerKeys.Contains(SpawnerKey) && Results.Num() < MaxResults)
		{
			SeenSpawnerKeys.Add(SpawnerKey);

			FBlueprintNodeTypeInfo Info;
			Info.DisplayName = DisplayName;
			Info.Category = DelegateCategory;
			Info.NodeClass = TEXT("K2Node_CreateDelegate");
			Info.SpawnerKey = SpawnerKey;
			Info.bIsPure = true;
			Info.bIsLatent = false;
			Info.Tooltip = TEXT("Create a delegate value from a function reference.");
			Info.Keywords = { TEXT("Create"), TEXT("Delegate"), TEXT("Event") };

			Results.Add(Info);
		}
	}
	
	// 3. Add common library functions - use static list for performance
	// These are the most commonly used blueprint function libraries
	// (Avoiding TObjectIterator which is slow and caused lockups)
	TArray<TPair<UClass*, FString>> FunctionLibraries;
	FunctionLibraries.Add({UKismetMathLibrary::StaticClass(), TEXT("Math")});
	FunctionLibraries.Add({UKismetSystemLibrary::StaticClass(), TEXT("Utilities")});
	FunctionLibraries.Add({UKismetStringLibrary::StaticClass(), TEXT("String")});
	FunctionLibraries.Add({UKismetArrayLibrary::StaticClass(), TEXT("Array")});
	FunctionLibraries.Add({UGameplayStatics::StaticClass(), TEXT("Game")});
	
	// Add more commonly needed libraries
	if (UClass* TextLibrary = FindObject<UClass>(nullptr, TEXT("/Script/Engine.KismetTextLibrary")))
	{
		FunctionLibraries.Add({TextLibrary, TEXT("Text")});
	}
	if (UClass* InputLibrary = FindObject<UClass>(nullptr, TEXT("/Script/Engine.KismetInputLibrary")))
	{
		FunctionLibraries.Add({InputLibrary, TEXT("Input")});
	}
	if (UClass* RenderingLibrary = FindObject<UClass>(nullptr, TEXT("/Script/Engine.KismetRenderingLibrary")))
	{
		FunctionLibraries.Add({RenderingLibrary, TEXT("Rendering")});
	}
	if (UClass* MaterialLibrary = FindObject<UClass>(nullptr, TEXT("/Script/Engine.KismetMaterialLibrary")))
	{
		FunctionLibraries.Add({MaterialLibrary, TEXT("Material")});
	}
	if (UClass* AILibrary = FindObject<UClass>(nullptr, TEXT("/Script/AIModule.AIBlueprintHelperLibrary")))
	{
		FunctionLibraries.Add({AILibrary, TEXT("AI")});
	}
	if (UClass* NavLibrary = FindObject<UClass>(nullptr, TEXT("/Script/NavigationSystem.NavigationSystemV1")))
	{
		FunctionLibraries.Add({NavLibrary, TEXT("Navigation")});
	}
	if (UClass* WidgetLibrary = FindObject<UClass>(nullptr, TEXT("/Script/UMG.WidgetBlueprintLibrary")))
	{
		FunctionLibraries.Add({WidgetLibrary, TEXT("Widget")});
	}
	if (UClass* SlateBPLibrary = FindObject<UClass>(nullptr, TEXT("/Script/UMG.SlateBlueprintLibrary")))
	{
		FunctionLibraries.Add({SlateBPLibrary, TEXT("Slate")});
	}
	
	// Iterate through all function libraries
	for (const auto& LibPair : FunctionLibraries)
	{
		if (Results.Num() >= MaxResults) break;
		
		UClass* LibClass = LibPair.Key;
		const FString& LibCategory = LibPair.Value;
		
		if (!LibClass) continue;
		
		for (TFieldIterator<UFunction> It(LibClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if (Results.Num() >= MaxResults) break;
			AddFunctionToResults(*It, LibCategory, LibClass->GetName());
		}
	}
	
	// 4. Add component-related functions from common component types
	TArray<UClass*> ComponentClasses = {
		UActorComponent::StaticClass(),
		USceneComponent::StaticClass(),
		UPrimitiveComponent::StaticClass()
	};
	
	for (UClass* CompClass : ComponentClasses)
	{
		if (!CompClass || Results.Num() >= MaxResults) continue;
		
		for (TFieldIterator<UFunction> It(CompClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if (Results.Num() >= MaxResults) break;
			FString CompCategory = FString::Printf(TEXT("Component: %s"), *CompClass->GetName());
			AddFunctionToResults(*It, CompCategory, CompClass->GetName());
		}
	}
	
	// 5. For Widget Blueprints: scan the widget tree and add functions from each widget class
	if (UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(Blueprint))
	{
		if (WidgetBP->WidgetTree)
		{
			TSet<UClass*> SeenWidgetClasses;
			WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget)
			{
				if (!Widget || Results.Num() >= MaxResults) return;

				UClass* WidgetClass = Widget->GetClass();
				if (!WidgetClass || SeenWidgetClasses.Contains(WidgetClass)) return;
				SeenWidgetClasses.Add(WidgetClass);

				FString WidgetCategory = FString::Printf(TEXT("Widget: %s"), *WidgetClass->GetName());

				// Walk the widget class hierarchy (stop at UWidget/UObject)
				UClass* WalkClass = WidgetClass;
				while (WalkClass && Results.Num() < MaxResults)
				{
					FString WalkCategory = FString::Printf(TEXT("Widget: %s"), *WalkClass->GetName());
					for (TFieldIterator<UFunction> It(WalkClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
					{
						if (Results.Num() >= MaxResults) break;
						AddFunctionToResults(*It, WalkCategory, WalkClass->GetName());
					}
					WalkClass = WalkClass->GetSuperClass();
					if (WalkClass && (WalkClass->GetName() == TEXT("Widget") || WalkClass->GetName() == TEXT("Object")))
						break;
				}
			});
		}
	}

	UE_LOG(LogTemp, Log, TEXT("DiscoverNodes: Found %d nodes matching '%s' in category '%s'"),
		Results.Num(), *SearchTerm, *Category);

	return Results;
}

bool UBlueprintService::GetNodeDetails(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId,
	FBlueprintNodeDetailedInfo& OutInfo)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("GetNodeDetails: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("GetNodeDetails: Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("GetNodeDetails: Node '%s' not found"), *NodeId);
		return false;
	}

	// Basic info
	OutInfo.NodeId = Node->NodeGuid.ToString();
	OutInfo.NodeClass = Node->GetClass()->GetName();
	OutInfo.NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
	OutInfo.FullTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	OutInfo.GraphName = Graph->GetName();
	OutInfo.Tooltip = Node->GetTooltipText().ToString();
	OutInfo.PosX = Node->NodePosX;
	OutInfo.PosY = Node->NodePosY;

	// Determine graph scope
	if (Blueprint->UbergraphPages.Contains(Graph))
	{
		OutInfo.GraphScope = TEXT("event");
	}
	else if (Blueprint->FunctionGraphs.Contains(Graph))
	{
		OutInfo.GraphScope = TEXT("function");
	}
	else if (Blueprint->MacroGraphs.Contains(Graph))
	{
		OutInfo.GraphScope = TEXT("macro");
	}
	else
	{
		OutInfo.GraphScope = TEXT("unknown");
	}

	// Check if pure (K2Node has this)
	if (UK2Node* K2Node = Cast<UK2Node>(Node))
	{
		OutInfo.bIsPure = K2Node->IsNodePure();
	}

	// Function call specific info
	if (UK2Node_CallFunction* FuncNode = Cast<UK2Node_CallFunction>(Node))
	{
		if (UFunction* Func = FuncNode->GetTargetFunction())
		{
			OutInfo.FunctionName = Func->GetName();
			OutInfo.FunctionClass = Func->GetOuterUClass()->GetName();
			OutInfo.bIsLatent = Func->HasMetaData(TEXT("Latent"));
		}
	}

	// Variable node specific info
	if (UK2Node_VariableGet* VarGetNode = Cast<UK2Node_VariableGet>(Node))
	{
		OutInfo.VariableName = VarGetNode->GetVarName().ToString();
	}
	else if (UK2Node_VariableSet* VarSetNode = Cast<UK2Node_VariableSet>(Node))
	{
		OutInfo.VariableName = VarSetNode->GetVarName().ToString();
	}

	// Get the schema for pin operations
	const UEdGraphSchema_K2* Schema = Cast<const UEdGraphSchema_K2>(Graph->GetSchema());

	// Process pins
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin || Pin->bHidden)
		{
			continue;
		}

		FBlueprintPinDetailedInfo PinInfo;
		PinInfo.PinName = Pin->PinName.ToString();
		PinInfo.DisplayName = Pin->GetDisplayName().ToString();
		PinInfo.PinCategory = Pin->PinType.PinCategory.ToString();
		PinInfo.PinSubCategory = Pin->PinType.PinSubCategory.ToString();
		
		if (Pin->PinType.PinSubCategoryObject.IsValid())
		{
			PinInfo.TypePath = Pin->PinType.PinSubCategoryObject->GetPathName();
		}
		
		PinInfo.bIsInput = (Pin->Direction == EGPD_Input);
		PinInfo.bIsConnected = Pin->LinkedTo.Num() > 0;
		PinInfo.bIsHidden = Pin->bHidden;
		PinInfo.bIsArray = Pin->PinType.ContainerType == EPinContainerType::Array;
		PinInfo.bIsReference = Pin->PinType.bIsReference;
		PinInfo.DefaultValue = Pin->DefaultValue;
		PinInfo.Tooltip = Pin->PinToolTip;

		// Check if can split
		if (Schema && Pin)
		{
			PinInfo.bCanSplit = Schema->CanSplitStructPin(*Pin);
			PinInfo.bIsSplit = Pin->SubPins.Num() > 0;
		}

		// Get connections
		for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			if (LinkedPin && LinkedPin->GetOwningNode())
			{
				FString Connection = FString::Printf(TEXT("%s:%s"),
					*LinkedPin->GetOwningNode()->NodeGuid.ToString(),
					*LinkedPin->PinName.ToString());
				PinInfo.Connections.Add(Connection);
			}
		}

		if (PinInfo.bIsInput)
		{
			OutInfo.InputPins.Add(PinInfo);
		}
		else
		{
			OutInfo.OutputPins.Add(PinInfo);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("GetNodeDetails: Got details for node '%s' (%s)"), *NodeId, *OutInfo.NodeTitle);
	return true;
}

bool UBlueprintService::SetNodePinValue(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId,
	const FString& PinName,
	const FString& Value)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNodePinValue: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNodePinValue: Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNodePinValue: Node '%s' not found"), *NodeId);
		return false;
	}

	// Find the pin
	UEdGraphPin* Pin = nullptr;
	for (UEdGraphPin* TestPin : Node->Pins)
	{
		if (TestPin && TestPin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			Pin = TestPin;
			break;
		}
	}

	if (!Pin)
	{
		// Try display name
		for (UEdGraphPin* TestPin : Node->Pins)
		{
			if (TestPin && TestPin->GetDisplayName().ToString().Equals(PinName, ESearchCase::IgnoreCase))
			{
				Pin = TestPin;
				break;
			}
		}
	}

	if (!Pin)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNodePinValue: Pin '%s' not found on node"), *PinName);
		return false;
	}

	if (Pin->Direction != EGPD_Input)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNodePinValue: Pin '%s' is not an input pin"), *PinName);
		return false;
	}

	// Set the default value — class/object reference pins use DefaultObject, not DefaultValue
	const UEdGraphSchema* Schema = Graph->GetSchema();
	const UEdGraphSchema_K2* K2Schema = Cast<UEdGraphSchema_K2>(Schema);
	const FName PinCategory = Pin->PinType.PinCategory;

	if (PinCategory == UEdGraphSchema_K2::PC_Class || PinCategory == UEdGraphSchema_K2::PC_SoftClass)
	{
		// Resolve the class with U/A prefix fallbacks
		UClass* ResolvedClass = LoadObject<UClass>(nullptr, *Value);
		if (!ResolvedClass)
			ResolvedClass = FindFirstObject<UClass>(*Value, EFindFirstObjectOptions::ExactClass);
		if (!ResolvedClass)
			ResolvedClass = FindFirstObject<UClass>(*FString::Printf(TEXT("U%s"), *Value), EFindFirstObjectOptions::ExactClass);
		if (!ResolvedClass)
			ResolvedClass = FindFirstObject<UClass>(*FString::Printf(TEXT("A%s"), *Value), EFindFirstObjectOptions::ExactClass);

		if (ResolvedClass)
		{
			if (K2Schema)
				K2Schema->TrySetDefaultObject(*Pin, ResolvedClass);
			else
				Pin->DefaultObject = ResolvedClass;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SetNodePinValue: Could not resolve class '%s' for class reference pin '%s'"), *Value, *PinName);
			return false;
		}
	}
	else if (PinCategory == UEdGraphSchema_K2::PC_Object || PinCategory == UEdGraphSchema_K2::PC_SoftObject)
	{
		// Load object by path and set DefaultObject
		UObject* ResolvedObject = LoadObject<UObject>(nullptr, *Value);
		if (ResolvedObject)
		{
			if (K2Schema)
				K2Schema->TrySetDefaultObject(*Pin, ResolvedObject);
			else
				Pin->DefaultObject = ResolvedObject;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SetNodePinValue: Could not load object '%s' for object reference pin '%s'"), *Value, *PinName);
			return false;
		}
	}
	else
	{
		// Primitive/string/enum/struct — use schema string path
		if (Schema)
			Schema->TrySetDefaultValue(*Pin, Value);
		else
			Pin->DefaultValue = Value;
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("SetNodePinValue: Set pin '%s' on node '%s' to '%s'"), *PinName, *NodeId, *Value);
	return true;
}

bool UBlueprintService::SplitPin(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId,
	const FString& PinName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SplitPin: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("SplitPin: Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("SplitPin: Node '%s' not found"), *NodeId);
		return false;
	}

	// Find the pin
	UEdGraphPin* Pin = nullptr;
	for (UEdGraphPin* TestPin : Node->Pins)
	{
		if (TestPin && TestPin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			Pin = TestPin;
			break;
		}
	}

	if (!Pin)
	{
		UE_LOG(LogTemp, Error, TEXT("SplitPin: Pin '%s' not found on node"), *PinName);
		return false;
	}

	// Get schema
	const UEdGraphSchema_K2* Schema = Cast<const UEdGraphSchema_K2>(Graph->GetSchema());
	if (!Schema)
	{
		UE_LOG(LogTemp, Error, TEXT("SplitPin: Failed to get K2 schema"));
		return false;
	}

	// Check if can split
	if (!Schema->CanSplitStructPin(*Pin))
	{
		UE_LOG(LogTemp, Error, TEXT("SplitPin: Pin '%s' cannot be split (not a splittable struct type)"), *PinName);
		return false;
	}

	// Already split?
	if (Pin->SubPins.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("SplitPin: Pin '%s' is already split"), *PinName);
		return true; // Already in desired state
	}

	// Perform split
	Schema->SplitPin(Pin);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("SplitPin: Split pin '%s' on node '%s'"), *PinName, *NodeId);
	return true;
}

bool UBlueprintService::RecombinePin(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId,
	const FString& PinName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("RecombinePin: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("RecombinePin: Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("RecombinePin: Node '%s' not found"), *NodeId);
		return false;
	}

	// Find the pin (or its parent if already split)
	UEdGraphPin* Pin = nullptr;
	for (UEdGraphPin* TestPin : Node->Pins)
	{
		if (TestPin && TestPin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			Pin = TestPin;
			break;
		}
	}

	// Check parent if pin is a sub-pin (e.g., ReturnValue_X -> ReturnValue)
	if (!Pin)
	{
		for (UEdGraphPin* TestPin : Node->Pins)
		{
			if (TestPin && TestPin->ParentPin)
			{
				if (TestPin->ParentPin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
				{
					Pin = TestPin->ParentPin;
					break;
				}
			}
		}
	}

	if (!Pin)
	{
		UE_LOG(LogTemp, Error, TEXT("RecombinePin: Pin '%s' not found on node"), *PinName);
		return false;
	}

	// Make sure we have the parent pin
	if (Pin->ParentPin)
	{
		Pin = Pin->ParentPin;
	}

	// Check if already recombined
	if (Pin->SubPins.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("RecombinePin: Pin '%s' is already recombined"), *PinName);
		return true;
	}

	// Get schema
	const UEdGraphSchema_K2* Schema = Cast<const UEdGraphSchema_K2>(Graph->GetSchema());
	if (!Schema)
	{
		UE_LOG(LogTemp, Error, TEXT("RecombinePin: Failed to get K2 schema"));
		return false;
	}

	// Perform recombine
	Schema->RecombinePin(Pin);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("RecombinePin: Recombined pin '%s' on node '%s'"), *PinName, *NodeId);
	return true;
}

bool UBlueprintService::RefreshNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId,
	bool bCompile)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("RefreshNode: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("RefreshNode: Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("RefreshNode: Node '%s' not found"), *NodeId);
		return false;
	}

	// Reconstruct the node (refreshes pins based on current function signature)
	Node->ReconstructNode();
	
	// Mark as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	// Compile if requested
	if (bCompile)
	{
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
	}

	UE_LOG(LogTemp, Log, TEXT("RefreshNode: Refreshed node '%s' in graph '%s'"), *NodeId, *GraphName);
	return true;
}

bool UBlueprintService::ConfigureNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId,
	const FString& PropertyName,
	const FString& Value)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("ConfigureNode: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("ConfigureNode: Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("ConfigureNode: Node '%s' not found"), *NodeId);
		return false;
	}

	// Find the property on the node
	FProperty* Property = Node->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		UE_LOG(LogTemp, Error, TEXT("ConfigureNode: Property '%s' not found on node"), *PropertyName);
		return false;
	}

	// Try to set the property value
	void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Node);
	
	// Handle special cases for class/object references
	if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
	{
		// Resolve with full path first, then U/A prefix fallbacks
		UClass* LoadedClass = LoadObject<UClass>(nullptr, *Value);
		if (!LoadedClass)
			LoadedClass = FindFirstObject<UClass>(*Value, EFindFirstObjectOptions::ExactClass);
		if (!LoadedClass)
			LoadedClass = FindFirstObject<UClass>(*FString::Printf(TEXT("U%s"), *Value), EFindFirstObjectOptions::ExactClass);
		if (!LoadedClass)
			LoadedClass = FindFirstObject<UClass>(*FString::Printf(TEXT("A%s"), *Value), EFindFirstObjectOptions::ExactClass);

		if (LoadedClass)
		{
			ClassProp->SetPropertyValue(PropertyAddr, LoadedClass);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ConfigureNode: Failed to load class '%s'"), *Value);
			return false;
		}
	}
	else if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
	{
		// Use generic import for soft class references
		Property->ImportText_Direct(*Value, PropertyAddr, nullptr, PPF_None);
	}
	else
	{
		// Use generic import
		Property->ImportText_Direct(*Value, PropertyAddr, nullptr, PPF_None);
	}

	// Reconstruct node to apply changes
	Node->ReconstructNode();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("ConfigureNode: Set property '%s' = '%s' on node '%s'"), *PropertyName, *Value, *NodeId);
	return true;
}

FString UBlueprintService::CreateNodeByKey(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& SpawnerKey,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Graph '%s' not found"), *GraphName);
		return FString();
	}

	// Parse spawner key - format: "FUNC ClassName::FunctionName", "NODE NodeClassName", or "EVENT ClassName::FunctionName"
	FString KeyType, KeyValue;
	if (!SpawnerKey.Split(TEXT(" "), &KeyType, &KeyValue))
	{
		UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Invalid spawner key format: %s"), *SpawnerKey);
		return FString();
	}

	UEdGraphNode* NewNode = nullptr;

	if (KeyType.Equals(TEXT("FUNC"), ESearchCase::IgnoreCase))
	{
		// Function call node
		FString ClassName, FunctionName;
		if (!KeyValue.Split(TEXT("::"), &ClassName, &FunctionName))
		{
			UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Invalid function key format: %s"), *KeyValue);
			return FString();
		}

		// Find the function
		UClass* OwnerClass = ResolveClassByName(ClassName);

		if (!OwnerClass)
		{
			UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Class '%s' not found"), *ClassName);
			return FString();
		}

		UFunction* Function = OwnerClass->FindFunctionByName(*FunctionName);
		if (!Function)
		{
			UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Function '%s' not found in class '%s'"), *FunctionName, *ClassName);
			return FString();
		}

		// Create the function call node - use the correct subclass for array functions
		// Array library functions need UK2Node_CallArrayFunction for wildcard pin type propagation
		bool bHasArrayPointerParms = Function->HasMetaData(FBlueprintMetadata::MD_ArrayParam);

		UK2Node_CallFunction* FuncNode;
		if (bHasArrayPointerParms)
		{
			FuncNode = NewObject<UK2Node_CallArrayFunction>(Graph);
			UE_LOG(LogTemp, Log, TEXT("CreateNodeByKey: Creating array function node for '%s' (has ArrayParm metadata)"), *FunctionName);
		}
		else
		{
			FuncNode = NewObject<UK2Node_CallFunction>(Graph);
		}
		FuncNode->SetFromFunction(Function);
		FuncNode->NodePosX = PosX;
		FuncNode->NodePosY = PosY;
		Graph->AddNode(FuncNode, false, false);
		FuncNode->CreateNewGuid();
		FuncNode->PostPlacedNewNode();
		FuncNode->AllocateDefaultPins();
		NewNode = FuncNode;
	}
	else if (KeyType.Equals(TEXT("EVENT"), ESearchCase::IgnoreCase))
	{
		UBlueprintEventNodeSpawner* EventSpawner = nullptr;

		if (KeyValue.Equals(TEXT("CUSTOM"), ESearchCase::IgnoreCase) || KeyValue.StartsWith(TEXT("CUSTOM::"), ESearchCase::IgnoreCase))
		{
			FName CustomEventName = NAME_None;
			if (KeyValue.StartsWith(TEXT("CUSTOM::"), ESearchCase::IgnoreCase))
			{
				const FString RequestedName = KeyValue.RightChop(8);
				if (!RequestedName.IsEmpty())
				{
					CustomEventName = FName(*RequestedName);
				}
			}

			EventSpawner = UBlueprintEventNodeSpawner::Create(UK2Node_CustomEvent::StaticClass(), CustomEventName);
		}
		else
		{
			FString ClassName;
			FString FunctionName;
			if (!KeyValue.Split(TEXT("::"), &ClassName, &FunctionName))
			{
				UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Invalid event key format: %s"), *KeyValue);
				return FString();
			}

			UClass* OwnerClass = ResolveClassByName(ClassName);
			if (!OwnerClass)
			{
				UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Event class '%s' not found"), *ClassName);
				return FString();
			}

			UFunction* EventFunction = OwnerClass->FindFunctionByName(*FunctionName);
			if (!EventFunction)
			{
				UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Event function '%s' not found in class '%s'"), *FunctionName, *ClassName);
				return FString();
			}

			EventSpawner = UBlueprintEventNodeSpawner::Create(EventFunction);
		}

		if (!EventSpawner)
		{
			UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Failed to create event spawner for key '%s'"), *SpawnerKey);
			return FString();
		}

		NewNode = EventSpawner->Invoke(Graph, IBlueprintNodeBinder::FBindingSet(), FVector2D(PosX, PosY));
	}
	else if (KeyType.Equals(TEXT("NODE"), ESearchCase::IgnoreCase))
	{
		// Generic node creation - find the node class
		UClass* NodeClass = FindFirstObject<UClass>(*KeyValue, EFindFirstObjectOptions::ExactClass);
		if (!NodeClass || !NodeClass->IsChildOf(UEdGraphNode::StaticClass()))
		{
			UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Node class '%s' not found"), *KeyValue);
			return FString();
		}

		NewNode = NewObject<UEdGraphNode>(Graph, NodeClass);
		Graph->AddNode(NewNode, false, false);
		NewNode->CreateNewGuid();
		NewNode->PostPlacedNewNode();
		NewNode->AllocateDefaultPins();
		NewNode->NodePosX = PosX;
		NewNode->NodePosY = PosY;
	}
	else if (KeyType.Equals(TEXT("STRUCT"), ESearchCase::IgnoreCase))
	{
		// STRUCT <path> — creates a K2Node_MakeStruct typed to the given struct.
		// Accepts "/Game/X/Foo.Foo", "/Game/X/Foo" (auto-suffix), or "/Script/Engine.HitResult".
		UScriptStruct* StructType = LoadStructByPath(KeyValue);
		if (!StructType)
		{
			UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Struct type '%s' not found"), *KeyValue);
			return FString();
		}

		UClass* MakeStructClass = FindFirstObject<UClass>(TEXT("K2Node_MakeStruct"), EFindFirstObjectOptions::ExactClass);
		if (!MakeStructClass)
		{
			UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: K2Node_MakeStruct class not found"));
			return FString();
		}

		UK2Node_MakeStruct* MakeStructNode = NewObject<UK2Node_MakeStruct>(Graph, MakeStructClass);
		MakeStructNode->StructType = StructType;
		MakeStructNode->NodePosX = PosX;
		MakeStructNode->NodePosY = PosY;
		Graph->AddNode(MakeStructNode, false, false);
		MakeStructNode->CreateNewGuid();
		MakeStructNode->PostPlacedNewNode();
		MakeStructNode->AllocateDefaultPins();
		NewNode = MakeStructNode;
	}
	else if (KeyType.Equals(TEXT("INSTANCED_STRUCT"), ESearchCase::IgnoreCase))
	{
		// INSTANCED_STRUCT <struct_path> — creates a MakeInstancedStruct function call node
		// with the wildcard Value pin pre-typed to the given struct.
		UClass* LibClass = ResolveClassByName(TEXT("BlueprintInstancedStructLibrary"));
		if (!LibClass)
		{
			UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: BlueprintInstancedStructLibrary not found"));
			return FString();
		}

		UFunction* MakeFunc = LibClass->FindFunctionByName(TEXT("MakeInstancedStruct"));
		if (!MakeFunc)
		{
			UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: MakeInstancedStruct function not found"));
			return FString();
		}

		UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(Graph);
		FuncNode->SetFromFunction(MakeFunc);
		FuncNode->NodePosX = PosX;
		FuncNode->NodePosY = PosY;
		Graph->AddNode(FuncNode, false, false);
		FuncNode->CreateNewGuid();
		FuncNode->PostPlacedNewNode();
		FuncNode->AllocateDefaultPins();

		// Pre-type the wildcard Value pin if a struct path is provided
		if (!KeyValue.IsEmpty())
		{
			if (UScriptStruct* StructType = LoadStructByPath(KeyValue))
			{
				if (UEdGraphPin* ValuePin = FuncNode->FindPin(TEXT("Value")))
				{
					ValuePin->PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
					ValuePin->PinType.PinSubCategoryObject = StructType;
					FuncNode->ReconstructNode();
				}
			}
		}

		NewNode = FuncNode;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Unknown key type: %s"), *KeyType);
		return FString();
	}

	if (NewNode)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		UE_LOG(LogTemp, Log, TEXT("CreateNodeByKey: Created node with key '%s' at (%f, %f)"), *SpawnerKey, PosX, PosY);
		return NewNode->NodeGuid.ToString();
	}

	return FString();
}

// ============================================================================
// EXISTENCE CHECKS - Fast boolean checks before creation (Idempotency)
// ============================================================================

bool UBlueprintService::BlueprintExists(const FString& BlueprintPath)
{
	if (BlueprintPath.IsEmpty())
	{
		return false;
	}

	// Fast path: use DoesAssetExist which doesn't load the asset
	return UEditorAssetLibrary::DoesAssetExist(BlueprintPath);
}

bool UBlueprintService::VariableExists(const FString& BlueprintPath, const FString& VariableName)
{
	if (VariableName.IsEmpty())
	{
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return false;
	}

	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName.ToString().Equals(VariableName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

bool UBlueprintService::FunctionExists(const FString& BlueprintPath, const FString& FunctionName)
{
	if (FunctionName.IsEmpty())
	{
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return false;
	}

	// Check function graphs
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName().ToString().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	// Also check generated class for functions (including inherited/overridden)
	if (UClass* GeneratedClass = Blueprint->GeneratedClass)
	{
		if (GeneratedClass->FindFunctionByName(FName(*FunctionName)))
		{
			return true;
		}
	}

	return false;
}

bool UBlueprintService::ComponentExists(const FString& BlueprintPath, const FString& ComponentName)
{
	if (ComponentName.IsEmpty())
	{
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return false;
	}

	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	if (!SCS)
	{
		return false;
	}

	const TArray<USCS_Node*>& AllNodes = SCS->GetAllNodes();
	for (USCS_Node* Node : AllNodes)
	{
		if (Node && Node->GetVariableName().ToString().Equals(ComponentName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

bool UBlueprintService::LocalVariableExists(
	const FString& BlueprintPath,
	const FString& FunctionName,
	const FString& VariableName)
{
	if (FunctionName.IsEmpty() || VariableName.IsEmpty())
	{
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return false;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName().ToString().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		return false;
	}

	// Get the entry node which contains local variables
	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
		{
			for (const FBPVariableDescription& LocalVar : EntryNode->LocalVariables)
			{
				if (LocalVar.VarName.ToString().Equals(VariableName, ESearchCase::IgnoreCase))
				{
					return true;
				}
			}
			break;
		}
	}

	return false;
}

bool UBlueprintService::NodeExists(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeTitle)
{
	if (GraphName.IsEmpty() || NodeTitle.IsEmpty())
	{
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		return false;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		// Check full title
		FString FullTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		if (FullTitle.Equals(NodeTitle, ESearchCase::IgnoreCase))
		{
			return true;
		}

		// Also check compact title (shorter version)
		FString CompactTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		if (CompactTitle.Equals(NodeTitle, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

bool UBlueprintService::FunctionCallExists(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& FunctionName)
{
	if (GraphName.IsEmpty() || FunctionName.IsEmpty())
	{
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		return false;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
		{
			FName FuncName = CallNode->FunctionReference.GetMemberName();
			if (FuncName.ToString().Equals(FunctionName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
	}

	return false;
}

FString UBlueprintService::AddDelegateBindNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& TargetClass,
	const FString& DelegateName,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddDelegateBindNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddDelegateBindNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	UClass* OwnerClass = nullptr;
	bool bSelfContext = false;

	if (TargetClass.IsEmpty() || TargetClass.Equals(TEXT("Self"), ESearchCase::IgnoreCase))
	{
		OwnerClass = Blueprint->GeneratedClass;
		bSelfContext = true;
	}
	else
	{
		OwnerClass = FindFirstObject<UClass>(*TargetClass, EFindFirstObjectOptions::ExactClass);
		if (!OwnerClass)
		{
			OwnerClass = FindFirstObject<UClass>(*FString::Printf(TEXT("U%s"), *TargetClass), EFindFirstObjectOptions::ExactClass);
		}
		if (!OwnerClass)
		{
			OwnerClass = FindFirstObject<UClass>(*FString::Printf(TEXT("A%s"), *TargetClass), EFindFirstObjectOptions::ExactClass);
		}
	}

	if (!OwnerClass)
	{
		UE_LOG(LogTemp, Error, TEXT("AddDelegateBindNode: Class '%s' not found"), *TargetClass);
		return FString();
	}

	FMulticastDelegateProperty* DelegateProp = nullptr;
	for (TFieldIterator<FMulticastDelegateProperty> PropIt(OwnerClass); PropIt; ++PropIt)
	{
		if (PropIt->GetName().Equals(DelegateName, ESearchCase::IgnoreCase))
		{
			DelegateProp = *PropIt;
			break;
		}
	}

	if (!DelegateProp)
	{
		UE_LOG(LogTemp, Error, TEXT("AddDelegateBindNode: Delegate '%s' not found on class '%s'"), *DelegateName, *OwnerClass->GetName());
		return FString();
	}

	UK2Node_AddDelegate* DelegateNode = NewObject<UK2Node_AddDelegate>(Graph);
	DelegateNode->SetFromProperty(DelegateProp, bSelfContext, OwnerClass);

	Graph->AddNode(DelegateNode, false, false);
	DelegateNode->CreateNewGuid();
	DelegateNode->PostPlacedNewNode();
	DelegateNode->AllocateDefaultPins();

	DelegateNode->NodePosX = PosX;
	DelegateNode->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddDelegateBindNode: Added bind node for %s::%s in %s"), *OwnerClass->GetName(), *DelegateName, *GraphName);

	return DelegateNode->NodeGuid.ToString();
}

FString UBlueprintService::AddCreateDelegateNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& FunctionName,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddCreateDelegateNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddCreateDelegateNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	UK2Node_CreateDelegate* Node = NewObject<UK2Node_CreateDelegate>(Graph);
	Node->SelectedFunctionName = FName(*FunctionName);

	Graph->AddNode(Node, false, false);
	Node->CreateNewGuid();
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();

	Node->NodePosX = PosX;
	Node->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddCreateDelegateNode: Created delegate node for function '%s' in %s"), *FunctionName, *GraphName);

	return Node->NodeGuid.ToString();
}

// ============================================================================
// FUNCTION OVERRIDES
// ============================================================================

TArray<FOverridableFunctionInfo> UBlueprintService::ListOverridableFunctions(const FString& BlueprintPath)
{
	TArray<FOverridableFunctionInfo> Result;

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint || !Blueprint->ParentClass)
	{
		return Result;
	}

	TSet<FName> ExistingGraphNames;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph)
		{
			ExistingGraphNames.Add(Graph->GetFName());
		}
	}

	for (UClass* Class = Blueprint->ParentClass; Class && Class != UObject::StaticClass(); Class = Class->GetSuperClass())
	{
		for (TFieldIterator<UFunction> FuncIt(Class, EFieldIterationFlags::None); FuncIt; ++FuncIt)
		{
			UFunction* Func = *FuncIt;
			if (!Func)
			{
				continue;
			}

			if (Func->GetOwnerClass() != Class)
			{
				continue;
			}

			if (!Func->HasAnyFunctionFlags(FUNC_BlueprintEvent))
			{
				continue;
			}

			const bool bIsNativeEvent = Func->HasAnyFunctionFlags(FUNC_Native);
			FProperty* RetProp = Func->GetReturnProperty();
			const bool bHasReturnValue = (RetProp != nullptr);
			const bool bIsEventStyle = !bHasReturnValue && Func->HasAnyFunctionFlags(FUNC_Event);

			bool bAlreadyOverridden = ExistingGraphNames.Contains(Func->GetFName());
			if (!bAlreadyOverridden && bIsEventStyle)
			{
				for (UEdGraph* UberGraph : Blueprint->UbergraphPages)
				{
					if (!UberGraph)
					{
						continue;
					}

					for (UEdGraphNode* Node : UberGraph->Nodes)
					{
						if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
						{
							if (EventNode->EventReference.GetMemberName() == Func->GetFName())
							{
								bAlreadyOverridden = true;
								break;
							}
						}
					}

					if (bAlreadyOverridden)
					{
						break;
					}
				}
			}

			FOverridableFunctionInfo Info;
			Info.FunctionName = Func->GetName();
			Info.OwnerClass = Class->GetName();
			Info.bIsNativeEvent = bIsNativeEvent;
			Info.bIsEventStyle = bIsEventStyle;
			Info.bAlreadyOverridden = bAlreadyOverridden;
			Info.ReturnType = RetProp ? RetProp->GetCPPType() : TEXT("void");

			for (TFieldIterator<FProperty> PropIt(Func); PropIt && PropIt->HasAnyPropertyFlags(CPF_Parm); ++PropIt)
			{
				if (PropIt->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					continue;
				}

				Info.Parameters.Add(FString::Printf(TEXT("%s:%s"), *PropIt->GetName(), *PropIt->GetCPPType()));
			}

			Result.Add(Info);
		}
	}

	return Result;
}

bool UBlueprintService::OverrideFunction(const FString& BlueprintPath, const FString& FunctionName)
{
	if (FunctionName.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("OverrideFunction: FunctionName is empty"));
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint || !Blueprint->ParentClass)
	{
		UE_LOG(LogTemp, Error, TEXT("OverrideFunction: Failed to load blueprint or no parent class: %s"), *BlueprintPath);
		return false;
	}

	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			UE_LOG(LogTemp, Log, TEXT("OverrideFunction: '%s' already overridden in %s"), *FunctionName, *BlueprintPath);
			return true;
		}
	}

	UFunction* TargetFunc = nullptr;
	UClass* FuncOwnerClass = nullptr;
	for (UClass* Class = Blueprint->ParentClass; Class && Class != UObject::StaticClass(); Class = Class->GetSuperClass())
	{
		UFunction* Found = Class->FindFunctionByName(FName(*FunctionName), EIncludeSuperFlag::ExcludeSuper);
		if (Found)
		{
			TargetFunc = Found;
			FuncOwnerClass = Class;
			break;
		}
	}

	if (!TargetFunc)
	{
		UE_LOG(LogTemp, Error, TEXT("OverrideFunction: '%s' not found in parent hierarchy of %s"), *FunctionName, *BlueprintPath);
		return false;
	}

	if (!TargetFunc->HasAnyFunctionFlags(FUNC_BlueprintEvent))
	{
		UE_LOG(LogTemp, Error, TEXT("OverrideFunction: '%s' is not a BlueprintEvent (not overridable)"), *FunctionName);
		return false;
	}

	const bool bHasReturnValue = (TargetFunc->GetReturnProperty() != nullptr);
	if (!bHasReturnValue && TargetFunc->HasAnyFunctionFlags(FUNC_Event))
	{
		UEdGraph* EventGraph = FindGraph(Blueprint, TEXT("EventGraph"));
		if (!EventGraph && Blueprint->UbergraphPages.Num() > 0)
		{
			EventGraph = Blueprint->UbergraphPages[0];
		}

		if (!EventGraph)
		{
			UE_LOG(LogTemp, Error, TEXT("OverrideFunction: EventGraph not found in %s"), *BlueprintPath);
			return false;
		}

		for (UEdGraphNode* Node : EventGraph->Nodes)
		{
			if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
			{
				if (EventNode->EventReference.GetMemberName().ToString().Equals(FunctionName, ESearchCase::IgnoreCase))
				{
					UE_LOG(LogTemp, Log, TEXT("OverrideFunction: Event node '%s' already exists in EventGraph of %s"), *FunctionName, *BlueprintPath);
					return true;
				}
			}
		}

		UK2Node_Event* EventNode = NewObject<UK2Node_Event>(EventGraph);
		EventNode->EventReference.SetExternalMember(FName(*FunctionName), FuncOwnerClass);
		EventNode->bOverrideFunction = true;

		EventGraph->AddNode(EventNode, false, false);
		EventNode->CreateNewGuid();
		EventNode->PostPlacedNewNode();
		EventNode->AllocateDefaultPins();

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		UE_LOG(LogTemp, Log, TEXT("OverrideFunction: Added event node '%s' to EventGraph of %s"), *FunctionName, *BlueprintPath);
		return true;
	}

	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*FunctionName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

	if (!NewGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("OverrideFunction: Failed to create graph for '%s'"), *FunctionName);
		return false;
	}

	FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, NewGraph, false, FuncOwnerClass);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("OverrideFunction: Created override function graph '%s' in %s"), *FunctionName, *BlueprintPath);
	return true;
}

bool UBlueprintService::SetCollisionSettings(
	const FString& BlueprintPath,
	const FString& ComponentName,
	const FString& CollisionEnabled,
	const FString& ObjectType,
	const FString& CollisionProfile,
	const TMap<FString, FString>& ChannelResponses)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SetCollisionSettings: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UActorComponent* Component = FindComponentTemplate(Blueprint, ComponentName);
	if (!Component)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetCollisionSettings: Component '%s' not found in %s"), *ComponentName, *BlueprintPath);
		return false;
	}

	UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
	if (!PrimComp)
	{
		UE_LOG(LogTemp, Error, TEXT("SetCollisionSettings: '%s' is not a UPrimitiveComponent (type: %s)"),
			*ComponentName, *Component->GetClass()->GetName());
		return false;
	}

	// Helper: channel name string -> ECollisionChannel
	auto ParseChannel = [](const FString& S) -> ECollisionChannel
	{
		if (S.Equals(TEXT("WorldStatic"),   ESearchCase::IgnoreCase)) return ECC_WorldStatic;
		if (S.Equals(TEXT("WorldDynamic"),  ESearchCase::IgnoreCase)) return ECC_WorldDynamic;
		if (S.Equals(TEXT("Pawn"),          ESearchCase::IgnoreCase)) return ECC_Pawn;
		if (S.Equals(TEXT("Visibility"),    ESearchCase::IgnoreCase)) return ECC_Visibility;
		if (S.Equals(TEXT("Camera"),        ESearchCase::IgnoreCase)) return ECC_Camera;
		if (S.Equals(TEXT("PhysicsBody"),   ESearchCase::IgnoreCase)) return ECC_PhysicsBody;
		if (S.Equals(TEXT("Vehicle"),       ESearchCase::IgnoreCase)) return ECC_Vehicle;
		if (S.Equals(TEXT("Destructible"),  ESearchCase::IgnoreCase)) return ECC_Destructible;
		return ECC_MAX; // unknown
	};

	// Helper: response string -> ECollisionResponse
	auto ParseResponse = [](const FString& S) -> ECollisionResponse
	{
		if (S.Equals(TEXT("Overlap"), ESearchCase::IgnoreCase)) return ECR_Overlap;
		if (S.Equals(TEXT("Block"),   ESearchCase::IgnoreCase)) return ECR_Block;
		return ECR_Ignore; // default / "Ignore"
	};

	Blueprint->Modify();
	PrimComp->Modify();

	// Set collision profile first — this resets the response table according to the profile.
	// Set it before individual channel overrides so "Custom" + per-channel responses works correctly.
	if (!CollisionProfile.IsEmpty())
	{
		PrimComp->BodyInstance.SetCollisionProfileName(FName(*CollisionProfile));
		UE_LOG(LogTemp, Log, TEXT("SetCollisionSettings: '%s' CollisionProfile = '%s'"), *ComponentName, *CollisionProfile);
	}

	// Set collision enabled type
	if (!CollisionEnabled.IsEmpty())
	{
		ECollisionEnabled::Type EnabledType = ECollisionEnabled::QueryAndPhysics;
		if      (CollisionEnabled.Equals(TEXT("NoCollision"),     ESearchCase::IgnoreCase)) EnabledType = ECollisionEnabled::NoCollision;
		else if (CollisionEnabled.Equals(TEXT("QueryOnly"),       ESearchCase::IgnoreCase)) EnabledType = ECollisionEnabled::QueryOnly;
		else if (CollisionEnabled.Equals(TEXT("PhysicsOnly"),     ESearchCase::IgnoreCase)) EnabledType = ECollisionEnabled::PhysicsOnly;
		else if (CollisionEnabled.Equals(TEXT("QueryAndPhysics"), ESearchCase::IgnoreCase)) EnabledType = ECollisionEnabled::QueryAndPhysics;
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SetCollisionSettings: Unknown CollisionEnabled value '%s', expected NoCollision/QueryOnly/PhysicsOnly/QueryAndPhysics"), *CollisionEnabled);
		}
		PrimComp->BodyInstance.SetCollisionEnabled(EnabledType);
		UE_LOG(LogTemp, Log, TEXT("SetCollisionSettings: '%s' CollisionEnabled = '%s'"), *ComponentName, *CollisionEnabled);
	}

	// Set object type (what collision channel this component occupies)
	if (!ObjectType.IsEmpty())
	{
		ECollisionChannel Channel = ParseChannel(ObjectType);
		if (Channel != ECC_MAX)
		{
			PrimComp->BodyInstance.SetObjectType(Channel);
			UE_LOG(LogTemp, Log, TEXT("SetCollisionSettings: '%s' ObjectType = '%s'"), *ComponentName, *ObjectType);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SetCollisionSettings: Unknown ObjectType '%s'"), *ObjectType);
		}
	}

	// Set per-channel collision responses
	for (const TPair<FString, FString>& Pair : ChannelResponses)
	{
		ECollisionChannel Channel = ParseChannel(Pair.Key);
		if (Channel == ECC_MAX)
		{
			UE_LOG(LogTemp, Warning, TEXT("SetCollisionSettings: Unknown channel '%s', skipping"), *Pair.Key);
			continue;
		}
		ECollisionResponse Response = ParseResponse(Pair.Value);
		PrimComp->BodyInstance.SetResponseToChannel(Channel, Response);
		UE_LOG(LogTemp, Log, TEXT("SetCollisionSettings: '%s' channel '%s' = '%s'"), *ComponentName, *Pair.Key, *Pair.Value);
	}

	// Notify the editor so the Details panel and viewport refresh
	FPropertyChangedEvent PropertyChangedEvent(nullptr, EPropertyChangeType::ValueSet);
	PrimComp->PostEditChangeProperty(PropertyChangedEvent);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("SetCollisionSettings: Updated collision on '%s' in %s"), *ComponentName, *BlueprintPath);
	return true;
}

// ============================================================================
// BATCH GRAPH BUILDER
// ============================================================================

UEdGraphPin* UBlueprintService::ResolvePinByName(
	UEdGraphNode* Node,
	const FString& PinName,
	EEdGraphPinDirection PreferredDirection)
{
	if (!Node || PinName.IsEmpty())
	{
		return nullptr;
	}

	// Ensure pins are allocated
	if (Node->Pins.Num() == 0)
	{
		Node->AllocateDefaultPins();
	}

	// Normalise Branch node pin name aliases
	FString ResolvedName = PinName;
	if (PinName.Equals(TEXT("True"), ESearchCase::IgnoreCase))
	{
		ResolvedName = TEXT("then");
	}
	else if (PinName.Equals(TEXT("False"), ESearchCase::IgnoreCase))
	{
		ResolvedName = TEXT("else");
	}

	// 1. Exact match (case-insensitive)
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->PinName.ToString().Equals(ResolvedName, ESearchCase::IgnoreCase))
		{
			if (PreferredDirection == EGPD_MAX || Pin->Direction == PreferredDirection)
			{
				return Pin;
			}
		}
	}

	// 1b. Exact match without direction preference (if direction didn't match above)
	if (PreferredDirection != EGPD_MAX)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->PinName.ToString().Equals(ResolvedName, ESearchCase::IgnoreCase))
			{
				return Pin;
			}
		}
	}

	// 2. Alias resolution
	if (PinName.Equals(TEXT("execute"), ESearchCase::IgnoreCase) || PinName.Equals(TEXT("exec"), ESearchCase::IgnoreCase))
	{
		// First exec input pin
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->Direction == EGPD_Input)
			{
				return Pin;
			}
		}
	}
	else if (PinName.Equals(TEXT("then"), ESearchCase::IgnoreCase) || PinName.Equals(TEXT("output"), ESearchCase::IgnoreCase))
	{
		// First exec output pin
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->Direction == EGPD_Output)
			{
				return Pin;
			}
		}
	}
	else if (PinName.Equals(TEXT("value"), ESearchCase::IgnoreCase) || PinName.Equals(TEXT("result"), ESearchCase::IgnoreCase))
	{
		// First non-exec output pin
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec && Pin->Direction == EGPD_Output)
			{
				return Pin;
			}
		}
	}

	// 3. Try display name
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->GetDisplayName().ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			if (PreferredDirection == EGPD_MAX || Pin->Direction == PreferredDirection)
			{
				return Pin;
			}
		}
	}

	return nullptr;
}

FString UBlueprintService::GetAvailablePinNames(UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
	TArray<FString> Names;
	if (Node)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction)
			{
				Names.Add(Pin->PinName.ToString());
			}
		}
	}
	return FString::Join(Names, TEXT(", "));
}

UEdGraphNode* UBlueprintService::CreateNodeFromDesc(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FGraphNodeDesc& Desc,
	float PosX, float PosY,
	FString& OutError)
{
	const FString& Type = Desc.Type;

	// ── function_call ──
	if (Type.Equals(TEXT("function_call"), ESearchCase::IgnoreCase))
	{
		const FString* ClassName = Desc.Params.Find(TEXT("class"));
		const FString* FunctionName = Desc.Params.Find(TEXT("function"));
		if (!ClassName || !FunctionName)
		{
			OutError = FString::Printf(TEXT("Node '%s': function_call requires 'class' and 'function' params"), *Desc.Ref);
			return nullptr;
		}

		UClass* OwnerClass = ResolveClassByName(*ClassName);
		if (!OwnerClass)
		{
			OutError = FString::Printf(TEXT("Node '%s': Class '%s' not found"), *Desc.Ref, **ClassName);
			return nullptr;
		}

		UFunction* Function = OwnerClass->FindFunctionByName(**FunctionName);
		if (!Function)
		{
			// Fallback: try spawner-based resolution for display-name matching
			FBlueprintActionDatabase& ActionDB = FBlueprintActionDatabase::Get();
			const FBlueprintActionDatabase::FActionRegistry& ActionRegistry = ActionDB.GetAllActions();
			for (auto It = ActionRegistry.CreateConstIterator(); It; ++It)
			{
				if (!It->Key.ResolveObjectPtr())
				{
					continue;
				}
				for (UBlueprintNodeSpawner* Spawner : It->Value)
				{
					if (UBlueprintFunctionNodeSpawner* FuncSpawner = Cast<UBlueprintFunctionNodeSpawner>(Spawner))
					{
						const UFunction* SpawnerFunc = FuncSpawner->GetFunction();
						if (SpawnerFunc && SpawnerFunc->GetOwnerClass()->IsChildOf(OwnerClass) &&
							SpawnerFunc->GetName().Equals(*FunctionName, ESearchCase::IgnoreCase))
						{
							Function = const_cast<UFunction*>(SpawnerFunc);
							break;
						}
					}
				}
				if (Function) break;
			}
		}

		if (!Function)
		{
			OutError = FString::Printf(TEXT("Node '%s': Function '%s' not found in class '%s'"), *Desc.Ref, **FunctionName, **ClassName);
			return nullptr;
		}

		// Use correct subclass for array functions (wildcard pin type propagation)
		UK2Node_CallFunction* FuncNode;
		if (Function->HasMetaData(FBlueprintMetadata::MD_ArrayParam))
		{
			FuncNode = NewObject<UK2Node_CallArrayFunction>(Graph);
		}
		else
		{
			FuncNode = NewObject<UK2Node_CallFunction>(Graph);
		}
		FuncNode->SetFromFunction(Function);
		FuncNode->NodePosX = PosX;
		FuncNode->NodePosY = PosY;
		Graph->AddNode(FuncNode, false, false);
		FuncNode->CreateNewGuid();
		FuncNode->PostPlacedNewNode();
		FuncNode->AllocateDefaultPins();
		return FuncNode;
	}

	// ── spawner_key ──
	if (Type.Equals(TEXT("spawner_key"), ESearchCase::IgnoreCase))
	{
		const FString* Key = Desc.Params.Find(TEXT("key"));
		if (!Key)
		{
			OutError = FString::Printf(TEXT("Node '%s': spawner_key requires 'key' param"), *Desc.Ref);
			return nullptr;
		}

		FString KeyType, KeyValue;
		if (!Key->Split(TEXT(" "), &KeyType, &KeyValue))
		{
			OutError = FString::Printf(TEXT("Node '%s': Invalid spawner key format '%s'"), *Desc.Ref, **Key);
			return nullptr;
		}

		if (KeyType.Equals(TEXT("FUNC"), ESearchCase::IgnoreCase))
		{
			FString ClassName, FunctionName;
			if (!KeyValue.Split(TEXT("::"), &ClassName, &FunctionName))
			{
				OutError = FString::Printf(TEXT("Node '%s': Invalid function key format '%s'"), *Desc.Ref, *KeyValue);
				return nullptr;
			}

			UClass* OwnerClass = ResolveClassByName(ClassName);
			if (!OwnerClass)
			{
				OutError = FString::Printf(TEXT("Node '%s': Class '%s' not found"), *Desc.Ref, *ClassName);
				return nullptr;
			}

			UFunction* Function = OwnerClass->FindFunctionByName(*FunctionName);
			if (!Function)
			{
				OutError = FString::Printf(TEXT("Node '%s': Function '%s' not found in '%s'"), *Desc.Ref, *FunctionName, *ClassName);
				return nullptr;
			}

			// Use correct subclass for array functions (wildcard pin type propagation)
			UK2Node_CallFunction* FuncNode;
			if (Function->HasMetaData(FBlueprintMetadata::MD_ArrayParam))
			{
				FuncNode = NewObject<UK2Node_CallArrayFunction>(Graph);
			}
			else
			{
				FuncNode = NewObject<UK2Node_CallFunction>(Graph);
			}
			FuncNode->SetFromFunction(Function);
			FuncNode->NodePosX = PosX;
			FuncNode->NodePosY = PosY;
			Graph->AddNode(FuncNode, false, false);
			FuncNode->CreateNewGuid();
			FuncNode->PostPlacedNewNode();
			FuncNode->AllocateDefaultPins();
			return FuncNode;
		}
		else if (KeyType.Equals(TEXT("EVENT"), ESearchCase::IgnoreCase))
		{
			UBlueprintEventNodeSpawner* EventSpawner = nullptr;

			if (KeyValue.Equals(TEXT("CUSTOM"), ESearchCase::IgnoreCase) || KeyValue.StartsWith(TEXT("CUSTOM::"), ESearchCase::IgnoreCase))
			{
				FName CustomEventName = NAME_None;
				if (KeyValue.StartsWith(TEXT("CUSTOM::"), ESearchCase::IgnoreCase))
				{
					CustomEventName = FName(*KeyValue.RightChop(8));
				}
				EventSpawner = UBlueprintEventNodeSpawner::Create(UK2Node_CustomEvent::StaticClass(), CustomEventName);
			}
			else
			{
				FString ClassName, FuncName;
				if (!KeyValue.Split(TEXT("::"), &ClassName, &FuncName))
				{
					OutError = FString::Printf(TEXT("Node '%s': Invalid event key format '%s'"), *Desc.Ref, *KeyValue);
					return nullptr;
				}

				UClass* OwnerClass = ResolveClassByName(ClassName);
				if (!OwnerClass)
				{
					OutError = FString::Printf(TEXT("Node '%s': Event class '%s' not found"), *Desc.Ref, *ClassName);
					return nullptr;
				}

				UFunction* EventFunc = OwnerClass->FindFunctionByName(*FuncName);
				if (!EventFunc)
				{
					OutError = FString::Printf(TEXT("Node '%s': Event function '%s' not found in '%s'"), *Desc.Ref, *FuncName, *ClassName);
					return nullptr;
				}

				EventSpawner = UBlueprintEventNodeSpawner::Create(EventFunc);
			}

			if (!EventSpawner)
			{
				OutError = FString::Printf(TEXT("Node '%s': Failed to create event spawner"), *Desc.Ref);
				return nullptr;
			}

			UEdGraphNode* NewNode = EventSpawner->Invoke(Graph, IBlueprintNodeBinder::FBindingSet(), FVector2D(PosX, PosY));
			if (!NewNode)
			{
				OutError = FString::Printf(TEXT("Node '%s': Event spawner invoke failed"), *Desc.Ref);
			}
			return NewNode;
		}
		else if (KeyType.Equals(TEXT("NODE"), ESearchCase::IgnoreCase))
		{
			UClass* NodeClass = FindFirstObject<UClass>(*KeyValue, EFindFirstObjectOptions::ExactClass);
			if (!NodeClass || !NodeClass->IsChildOf(UEdGraphNode::StaticClass()))
			{
				OutError = FString::Printf(TEXT("Node '%s': Node class '%s' not found"), *Desc.Ref, *KeyValue);
				return nullptr;
			}

			UEdGraphNode* NewNode = NewObject<UEdGraphNode>(Graph, NodeClass);
			NewNode->NodePosX = PosX;
			NewNode->NodePosY = PosY;
			Graph->AddNode(NewNode, false, false);
			NewNode->CreateNewGuid();
			NewNode->PostPlacedNewNode();
			NewNode->AllocateDefaultPins();
			return NewNode;
		}
		else if (KeyType.Equals(TEXT("STRUCT"), ESearchCase::IgnoreCase))
		{
			UScriptStruct* StructType = LoadStructByPath(KeyValue);
			if (!StructType)
			{
				OutError = FString::Printf(TEXT("Node '%s': Struct type '%s' not found"), *Desc.Ref, *KeyValue);
				return nullptr;
			}

			UClass* MakeStructClass = FindFirstObject<UClass>(TEXT("K2Node_MakeStruct"), EFindFirstObjectOptions::ExactClass);
			if (!MakeStructClass)
			{
				OutError = FString::Printf(TEXT("Node '%s': K2Node_MakeStruct class not found"), *Desc.Ref);
				return nullptr;
			}

			UK2Node_MakeStruct* MakeStructNode = NewObject<UK2Node_MakeStruct>(Graph, MakeStructClass);
			MakeStructNode->StructType = StructType;
			MakeStructNode->NodePosX = PosX;
			MakeStructNode->NodePosY = PosY;
			Graph->AddNode(MakeStructNode, false, false);
			MakeStructNode->CreateNewGuid();
			MakeStructNode->PostPlacedNewNode();
			MakeStructNode->AllocateDefaultPins();
			return MakeStructNode;
		}
		else if (KeyType.Equals(TEXT("INSTANCED_STRUCT"), ESearchCase::IgnoreCase))
		{
			UClass* LibClass = ResolveClassByName(TEXT("BlueprintInstancedStructLibrary"));
			UFunction* MakeFunc = LibClass ? LibClass->FindFunctionByName(TEXT("MakeInstancedStruct")) : nullptr;
			if (!MakeFunc)
			{
				OutError = FString::Printf(TEXT("Node '%s': MakeInstancedStruct not found"), *Desc.Ref);
				return nullptr;
			}

			UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(Graph);
			FuncNode->SetFromFunction(MakeFunc);
			FuncNode->NodePosX = PosX;
			FuncNode->NodePosY = PosY;
			Graph->AddNode(FuncNode, false, false);
			FuncNode->CreateNewGuid();
			FuncNode->PostPlacedNewNode();
			FuncNode->AllocateDefaultPins();

			if (!KeyValue.IsEmpty())
			{
				if (UScriptStruct* StructType = LoadStructByPath(KeyValue))
				{
					if (UEdGraphPin* ValuePin = FuncNode->FindPin(TEXT("Value")))
					{
						ValuePin->PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
						ValuePin->PinType.PinSubCategoryObject = StructType;
						FuncNode->ReconstructNode();
					}
				}
			}

			return FuncNode;
		}

		OutError = FString::Printf(TEXT("Node '%s': Unknown spawner key type '%s'"), *Desc.Ref, *KeyType);
		return nullptr;
	}

	// ── variable_get ──
	if (Type.Equals(TEXT("variable_get"), ESearchCase::IgnoreCase))
	{
		const FString* VarName = Desc.Params.Find(TEXT("variable"));
		if (!VarName)
		{
			OutError = FString::Printf(TEXT("Node '%s': variable_get requires 'variable' param"), *Desc.Ref);
			return nullptr;
		}

		UK2Node_VariableGet* GetNode = NewObject<UK2Node_VariableGet>(Graph);
		GetNode->VariableReference.SetSelfMember(FName(**VarName));
		GetNode->NodePosX = PosX;
		GetNode->NodePosY = PosY;
		Graph->AddNode(GetNode, false, false);
		GetNode->CreateNewGuid();
		GetNode->PostPlacedNewNode();
		GetNode->AllocateDefaultPins();
		return GetNode;
	}

	// ── variable_set ──
	if (Type.Equals(TEXT("variable_set"), ESearchCase::IgnoreCase))
	{
		const FString* VarName = Desc.Params.Find(TEXT("variable"));
		if (!VarName)
		{
			OutError = FString::Printf(TEXT("Node '%s': variable_set requires 'variable' param"), *Desc.Ref);
			return nullptr;
		}

		UK2Node_VariableSet* SetNode = NewObject<UK2Node_VariableSet>(Graph);
		SetNode->VariableReference.SetSelfMember(FName(**VarName));
		SetNode->NodePosX = PosX;
		SetNode->NodePosY = PosY;
		Graph->AddNode(SetNode, false, false);
		SetNode->CreateNewGuid();
		SetNode->PostPlacedNewNode();
		SetNode->AllocateDefaultPins();
		return SetNode;
	}

	// ── make_struct ──
	// Creates a K2Node_MakeStruct typed to a specific struct.
	// Params: struct = "/Game/X/Foo.Foo" or "/Game/X/Foo" or "/Script/Engine.HitResult"
	if (Type.Equals(TEXT("make_struct"), ESearchCase::IgnoreCase))
	{
		const FString* StructPath = Desc.Params.Find(TEXT("struct"));
		if (!StructPath)
		{
			OutError = FString::Printf(TEXT("Node '%s': make_struct requires 'struct' param"), *Desc.Ref);
			return nullptr;
		}

		UScriptStruct* StructType = LoadStructByPath(*StructPath);
		if (!StructType)
		{
			OutError = FString::Printf(TEXT("Node '%s': Struct type '%s' not found"), *Desc.Ref, **StructPath);
			return nullptr;
		}

		UClass* MakeStructClass = FindFirstObject<UClass>(TEXT("K2Node_MakeStruct"), EFindFirstObjectOptions::ExactClass);
		if (!MakeStructClass)
		{
			OutError = FString::Printf(TEXT("Node '%s': K2Node_MakeStruct class not found"), *Desc.Ref);
			return nullptr;
		}

		UK2Node_MakeStruct* MakeStructNode = NewObject<UK2Node_MakeStruct>(Graph, MakeStructClass);
		MakeStructNode->StructType = StructType;
		MakeStructNode->NodePosX = PosX;
		MakeStructNode->NodePosY = PosY;
		Graph->AddNode(MakeStructNode, false, false);
		MakeStructNode->CreateNewGuid();
		MakeStructNode->PostPlacedNewNode();
		MakeStructNode->AllocateDefaultPins();
		return MakeStructNode;
	}

	// ── instanced_struct ──
	// Creates a MakeInstancedStruct function call node (wraps any struct in FInstancedStruct).
	// Params: struct = "/Game/X/Foo" (optional — pre-types the wildcard Value pin)
	if (Type.Equals(TEXT("instanced_struct"), ESearchCase::IgnoreCase))
	{
		UClass* LibClass = ResolveClassByName(TEXT("BlueprintInstancedStructLibrary"));
		UFunction* MakeFunc = LibClass ? LibClass->FindFunctionByName(TEXT("MakeInstancedStruct")) : nullptr;
		if (!MakeFunc)
		{
			OutError = FString::Printf(TEXT("Node '%s': MakeInstancedStruct not found — ensure Engine module is loaded"), *Desc.Ref);
			return nullptr;
		}

		UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(Graph);
		FuncNode->SetFromFunction(MakeFunc);
		FuncNode->NodePosX = PosX;
		FuncNode->NodePosY = PosY;
		Graph->AddNode(FuncNode, false, false);
		FuncNode->CreateNewGuid();
		FuncNode->PostPlacedNewNode();
		FuncNode->AllocateDefaultPins();

		// If a struct path is provided, pre-type the wildcard Value pin
		const FString* StructPath = Desc.Params.Find(TEXT("struct"));
		if (StructPath)
		{
			if (UScriptStruct* StructType = LoadStructByPath(*StructPath))
			{
				if (UEdGraphPin* ValuePin = FuncNode->FindPin(TEXT("Value")))
				{
					ValuePin->PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
					ValuePin->PinType.PinSubCategoryObject = StructType;
					FuncNode->ReconstructNode();
				}
			}
		}

		return FuncNode;
	}

	// ── event ──
	if (Type.Equals(TEXT("event"), ESearchCase::IgnoreCase))
	{
		const FString* EventName = Desc.Params.Find(TEXT("event"));
		if (!EventName)
		{
			OutError = FString::Printf(TEXT("Node '%s': event requires 'event' param"), *Desc.Ref);
			return nullptr;
		}

		if (!Blueprint->ParentClass)
		{
			OutError = FString::Printf(TEXT("Node '%s': Blueprint has no parent class"), *Desc.Ref);
			return nullptr;
		}

		UFunction* EventFunction = Blueprint->ParentClass->FindFunctionByName(FName(**EventName));
		if (!EventFunction)
		{
			OutError = FString::Printf(TEXT("Node '%s': Event '%s' not found in parent class '%s'"), *Desc.Ref, **EventName, *Blueprint->ParentClass->GetName());
			return nullptr;
		}

		UK2Node_Event* EventNode = NewObject<UK2Node_Event>(Graph);
		EventNode->EventReference.SetExternalMember(FName(**EventName), Blueprint->ParentClass);
		EventNode->bOverrideFunction = true;
		EventNode->NodePosX = PosX;
		EventNode->NodePosY = PosY;
		Graph->AddNode(EventNode, false, false);
		EventNode->CreateNewGuid();
		EventNode->PostPlacedNewNode();
		EventNode->AllocateDefaultPins();
		return EventNode;
	}

	// ── custom_event ──
	if (Type.Equals(TEXT("custom_event"), ESearchCase::IgnoreCase))
	{
		const FString* EventName = Desc.Params.Find(TEXT("name"));
		FName CustomEventName = EventName && !EventName->IsEmpty() ? FName(**EventName) : NAME_None;

		UBlueprintEventNodeSpawner* EventSpawner = UBlueprintEventNodeSpawner::Create(UK2Node_CustomEvent::StaticClass(), CustomEventName);
		if (!EventSpawner)
		{
			OutError = FString::Printf(TEXT("Node '%s': Failed to create event spawner"), *Desc.Ref);
			return nullptr;
		}

		UEdGraphNode* SpawnedNode = EventSpawner->Invoke(Graph, IBlueprintNodeBinder::FBindingSet(), FVector2D(PosX, PosY));
		if (!SpawnedNode)
		{
			OutError = FString::Printf(TEXT("Node '%s': Failed to spawn custom event"), *Desc.Ref);
		}
		return SpawnedNode;
	}

	// ── branch ──
	if (Type.Equals(TEXT("branch"), ESearchCase::IgnoreCase))
	{
		UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(Graph);
		BranchNode->NodePosX = PosX;
		BranchNode->NodePosY = PosY;
		Graph->AddNode(BranchNode, false, false);
		BranchNode->CreateNewGuid();
		BranchNode->PostPlacedNewNode();
		BranchNode->AllocateDefaultPins();
		return BranchNode;
	}

	// ── cast ──
	if (Type.Equals(TEXT("cast"), ESearchCase::IgnoreCase))
	{
		const FString* TargetClass = Desc.Params.Find(TEXT("target_class"));
		if (!TargetClass)
		{
			OutError = FString::Printf(TEXT("Node '%s': cast requires 'target_class' param"), *Desc.Ref);
			return nullptr;
		}

		UClass* TargetUClass = FindFirstObject<UClass>(**TargetClass, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("BuildGraph"));
		if (!TargetUClass)
		{
			OutError = FString::Printf(TEXT("Node '%s': Cast target class '%s' not found"), *Desc.Ref, **TargetClass);
			return nullptr;
		}

		UK2Node_DynamicCast* CastNode = NewObject<UK2Node_DynamicCast>(Graph);
		CastNode->TargetType = TargetUClass;
		CastNode->NodePosX = PosX;
		CastNode->NodePosY = PosY;
		Graph->AddNode(CastNode, false, false);
		CastNode->CreateNewGuid();
		CastNode->PostPlacedNewNode();
		CastNode->AllocateDefaultPins();
		return CastNode;
	}

	// ── print_string ──
	if (Type.Equals(TEXT("print_string"), ESearchCase::IgnoreCase))
	{
		UK2Node_CallFunction* PrintNode = NewObject<UK2Node_CallFunction>(Graph);
		UFunction* PrintFunc = UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString));
		if (PrintFunc)
		{
			PrintNode->SetFromFunction(PrintFunc);
		}
		PrintNode->NodePosX = PosX;
		PrintNode->NodePosY = PosY;
		Graph->AddNode(PrintNode, false, false);
		PrintNode->CreateNewGuid();
		PrintNode->PostPlacedNewNode();
		PrintNode->AllocateDefaultPins();
		return PrintNode;
	}

	// ── input_action ──
	if (Type.Equals(TEXT("input_action"), ESearchCase::IgnoreCase))
	{
		const FString* ActionPath = Desc.Params.Find(TEXT("action"));
		if (!ActionPath)
		{
			OutError = FString::Printf(TEXT("Node '%s': input_action requires 'action' param"), *Desc.Ref);
			return nullptr;
		}

		UInputAction* InputAction = Cast<UInputAction>(UEditorAssetLibrary::LoadAsset(*ActionPath));
		if (!InputAction)
		{
			OutError = FString::Printf(TEXT("Node '%s': Failed to load Input Action '%s'"), *Desc.Ref, **ActionPath);
			return nullptr;
		}

		UK2Node_EnhancedInputAction* ActionNode = NewObject<UK2Node_EnhancedInputAction>(Graph);
		ActionNode->InputAction = InputAction;
		ActionNode->NodePosX = PosX;
		ActionNode->NodePosY = PosY;
		Graph->AddNode(ActionNode, false, false);
		ActionNode->CreateNewGuid();
		ActionNode->PostPlacedNewNode();
		ActionNode->AllocateDefaultPins();
		return ActionNode;
	}

	// ── math ──
	if (Type.Equals(TEXT("math"), ESearchCase::IgnoreCase))
	{
		const FString* Operation = Desc.Params.Find(TEXT("operation"));
		const FString* OperandType = Desc.Params.Find(TEXT("operand_type"));
		if (!Operation || !OperandType)
		{
			OutError = FString::Printf(TEXT("Node '%s': math requires 'operation' and 'operand_type' params"), *Desc.Ref);
			return nullptr;
		}

		// Normalize Float → Double for UE 5.7
		FString NType = *OperandType;
		if (NType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
		{
			NType = TEXT("Double");
		}

		FString FunctionName;
		if (Operation->Equals(TEXT("Add"), ESearchCase::IgnoreCase))
			FunctionName = FString::Printf(TEXT("Add_%s%s"), *NType, *NType);
		else if (Operation->Equals(TEXT("Subtract"), ESearchCase::IgnoreCase))
			FunctionName = FString::Printf(TEXT("Subtract_%s%s"), *NType, *NType);
		else if (Operation->Equals(TEXT("Multiply"), ESearchCase::IgnoreCase))
			FunctionName = FString::Printf(TEXT("Multiply_%s%s"), *NType, *NType);
		else if (Operation->Equals(TEXT("Divide"), ESearchCase::IgnoreCase))
			FunctionName = FString::Printf(TEXT("Divide_%s%s"), *NType, *NType);
		else if (Operation->Equals(TEXT("Clamp"), ESearchCase::IgnoreCase))
			FunctionName = NType.Equals(TEXT("Int"), ESearchCase::IgnoreCase) ? TEXT("Clamp") : TEXT("FClamp");
		else if (Operation->Equals(TEXT("Abs"), ESearchCase::IgnoreCase))
			FunctionName = TEXT("Abs");
		else
		{
			OutError = FString::Printf(TEXT("Node '%s': Unknown math operation '%s'"), *Desc.Ref, **Operation);
			return nullptr;
		}

		UFunction* MathFunc = UKismetMathLibrary::StaticClass()->FindFunctionByName(*FunctionName);
		if (!MathFunc)
		{
			OutError = FString::Printf(TEXT("Node '%s': Math function '%s' not found"), *Desc.Ref, *FunctionName);
			return nullptr;
		}

		UK2Node_CallFunction* MathNode = NewObject<UK2Node_CallFunction>(Graph);
		MathNode->SetFromFunction(MathFunc);
		MathNode->NodePosX = PosX;
		MathNode->NodePosY = PosY;
		Graph->AddNode(MathNode, false, false);
		MathNode->CreateNewGuid();
		MathNode->PostPlacedNewNode();
		MathNode->AllocateDefaultPins();
		return MathNode;
	}

	// ── comparison ──
	if (Type.Equals(TEXT("comparison"), ESearchCase::IgnoreCase))
	{
		const FString* Operation = Desc.Params.Find(TEXT("operation"));
		const FString* OperandType = Desc.Params.Find(TEXT("operand_type"));
		if (!Operation || !OperandType)
		{
			OutError = FString::Printf(TEXT("Node '%s': comparison requires 'operation' and 'operand_type' params"), *Desc.Ref);
			return nullptr;
		}

		FString NType = *OperandType;
		if (NType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
		{
			NType = TEXT("Double");
		}

		FString FunctionName;
		if (Operation->Equals(TEXT("Greater"), ESearchCase::IgnoreCase))
			FunctionName = FString::Printf(TEXT("Greater_%s%s"), *NType, *NType);
		else if (Operation->Equals(TEXT("Less"), ESearchCase::IgnoreCase))
			FunctionName = FString::Printf(TEXT("Less_%s%s"), *NType, *NType);
		else if (Operation->Equals(TEXT("GreaterEqual"), ESearchCase::IgnoreCase))
			FunctionName = FString::Printf(TEXT("GreaterEqual_%s%s"), *NType, *NType);
		else if (Operation->Equals(TEXT("LessEqual"), ESearchCase::IgnoreCase))
			FunctionName = FString::Printf(TEXT("LessEqual_%s%s"), *NType, *NType);
		else if (Operation->Equals(TEXT("Equal"), ESearchCase::IgnoreCase))
			FunctionName = FString::Printf(TEXT("EqualEqual_%s%s"), *NType, *NType);
		else if (Operation->Equals(TEXT("NotEqual"), ESearchCase::IgnoreCase))
			FunctionName = FString::Printf(TEXT("NotEqual_%s%s"), *NType, *NType);
		else
		{
			OutError = FString::Printf(TEXT("Node '%s': Unknown comparison '%s'"), *Desc.Ref, **Operation);
			return nullptr;
		}

		UFunction* CmpFunc = UKismetMathLibrary::StaticClass()->FindFunctionByName(*FunctionName);
		if (!CmpFunc)
		{
			OutError = FString::Printf(TEXT("Node '%s': Comparison function '%s' not found"), *Desc.Ref, *FunctionName);
			return nullptr;
		}

		UK2Node_CallFunction* CmpNode = NewObject<UK2Node_CallFunction>(Graph);
		CmpNode->SetFromFunction(CmpFunc);
		CmpNode->NodePosX = PosX;
		CmpNode->NodePosY = PosY;
		Graph->AddNode(CmpNode, false, false);
		CmpNode->CreateNewGuid();
		CmpNode->PostPlacedNewNode();
		CmpNode->AllocateDefaultPins();
		return CmpNode;
	}

	// ── delegate_bind ──
	if (Type.Equals(TEXT("delegate_bind"), ESearchCase::IgnoreCase))
	{
		const FString* DelegateName = Desc.Params.Find(TEXT("delegate"));
		const FString* ComponentName = Desc.Params.Find(TEXT("component"));
		if (!DelegateName)
		{
			OutError = FString::Printf(TEXT("Node '%s': delegate_bind requires 'delegate' param"), *Desc.Ref);
			return nullptr;
		}

		const FString TargetClassName = ComponentName ? *ComponentName : TEXT("Self");
		UClass* OwnerClass = nullptr;
		bool bSelfContext = false;
		if (TargetClassName.IsEmpty() || TargetClassName.Equals(TEXT("Self"), ESearchCase::IgnoreCase))
		{
			OwnerClass = Blueprint->GeneratedClass;
			bSelfContext = true;
		}
		else
		{
			OwnerClass = ResolveClassByName(TargetClassName);
		}

		if (!OwnerClass)
		{
			OutError = FString::Printf(TEXT("Node '%s': Class '%s' not found"), *Desc.Ref, *TargetClassName);
			return nullptr;
		}

		FMulticastDelegateProperty* DelegateProp = nullptr;
		for (TFieldIterator<FMulticastDelegateProperty> PropIt(OwnerClass); PropIt; ++PropIt)
		{
			if (PropIt->GetName().Equals(*DelegateName, ESearchCase::IgnoreCase))
			{
				DelegateProp = *PropIt;
				break;
			}
		}

		if (!DelegateProp)
		{
			OutError = FString::Printf(TEXT("Node '%s': Delegate '%s' not found on '%s'"), *Desc.Ref, **DelegateName, *OwnerClass->GetName());
			return nullptr;
		}

		UK2Node_AddDelegate* DelegateNode = NewObject<UK2Node_AddDelegate>(Graph);
		DelegateNode->SetFromProperty(DelegateProp, bSelfContext, OwnerClass);
		DelegateNode->NodePosX = PosX;
		DelegateNode->NodePosY = PosY;
		Graph->AddNode(DelegateNode, false, false);
		DelegateNode->CreateNewGuid();
		DelegateNode->PostPlacedNewNode();
		DelegateNode->AllocateDefaultPins();
		return DelegateNode;
	}

	// ── create_event ──
	if (Type.Equals(TEXT("create_event"), ESearchCase::IgnoreCase))
	{
		const FString* FunctionName = Desc.Params.Find(TEXT("function"));
		if (!FunctionName)
		{
			OutError = FString::Printf(TEXT("Node '%s': create_event requires 'function' param"), *Desc.Ref);
			return nullptr;
		}

		UK2Node_CreateDelegate* Node = NewObject<UK2Node_CreateDelegate>(Graph);
		if (!FunctionName->IsEmpty())
		{
			Node->SetFunction(FName(**FunctionName));
		}
		Node->NodePosX = PosX;
		Node->NodePosY = PosY;
		Graph->AddNode(Node, false, false);
		Node->CreateNewGuid();
		Node->PostPlacedNewNode();
		Node->AllocateDefaultPins();
		return Node;
	}

	// ── validated_get ──
	if (Type.Equals(TEXT("validated_get"), ESearchCase::IgnoreCase))
	{
		const FString* VarName = Desc.Params.Find(TEXT("variable"));
		if (!VarName)
		{
			OutError = FString::Printf(TEXT("Node '%s': validated_get requires 'variable' param"), *Desc.Ref);
			return nullptr;
		}

		UK2Node_VariableGet* GetNode = NewObject<UK2Node_VariableGet>(Graph);
		GetNode->VariableReference.SetSelfMember(FName(**VarName));

		// Set to ValidatedObject variation for impure (with exec pins)
		if (FEnumProperty* VariationProp = FindFProperty<FEnumProperty>(UK2Node_VariableGet::StaticClass(), TEXT("CurrentVariation")))
		{
			FNumericProperty* UnderlyingProp = VariationProp->GetUnderlyingProperty();
			void* PropContainer = VariationProp->ContainerPtrToValuePtr<void>(GetNode);
			UnderlyingProp->SetIntPropertyValue(PropContainer, (int64)EGetNodeVariation::ValidatedObject);
		}

		GetNode->NodePosX = PosX;
		GetNode->NodePosY = PosY;
		Graph->AddNode(GetNode, false, false);
		GetNode->CreateNewGuid();
		GetNode->PostPlacedNewNode();
		GetNode->AllocateDefaultPins();
		return GetNode;
	}

	// ── member_get ──
	if (Type.Equals(TEXT("member_get"), ESearchCase::IgnoreCase))
	{
		const FString* MemberName = Desc.Params.Find(TEXT("member"));
		const FString* ClassName = Desc.Params.Find(TEXT("class"));
		if (!MemberName || !ClassName)
		{
			OutError = FString::Printf(TEXT("Node '%s': member_get requires 'member' and 'class' params"), *Desc.Ref);
			return nullptr;
		}

		UClass* OwnerClass = nullptr;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->GetName() == *ClassName)
			{
				OwnerClass = *It;
				break;
			}
		}

		if (!OwnerClass)
		{
			OutError = FString::Printf(TEXT("Node '%s': Class '%s' not found"), *Desc.Ref, **ClassName);
			return nullptr;
		}

		FProperty* MemberProp = FindFProperty<FProperty>(OwnerClass, FName(**MemberName));
		if (!MemberProp)
		{
			OutError = FString::Printf(TEXT("Node '%s': Member '%s' not found on '%s'"), *Desc.Ref, **MemberName, **ClassName);
			return nullptr;
		}

		UK2Node_VariableGet* GetNode = NewObject<UK2Node_VariableGet>(Graph);
		GetNode->VariableReference.SetExternalMember(FName(**MemberName), OwnerClass);
		GetNode->NodePosX = PosX;
		GetNode->NodePosY = PosY;
		Graph->AddNode(GetNode, false, false);
		GetNode->CreateNewGuid();
		GetNode->PostPlacedNewNode();
		GetNode->AllocateDefaultPins();
		return GetNode;
	}

	// ── create_delegate ──
	if (Type.Equals(TEXT("create_delegate"), ESearchCase::IgnoreCase))
	{
		const FString* FunctionName = Desc.Params.Find(TEXT("function"));
		if (!FunctionName)
		{
			OutError = FString::Printf(TEXT("Node '%s': create_delegate requires 'function' param"), *Desc.Ref);
			return nullptr;
		}

		UK2Node_CreateDelegate* Node = NewObject<UK2Node_CreateDelegate>(Graph);
		Node->SelectedFunctionName = FName(**FunctionName);
		Node->NodePosX = PosX;
		Node->NodePosY = PosY;
		Graph->AddNode(Node, false, false);
		Node->CreateNewGuid();
		Node->PostPlacedNewNode();
		Node->AllocateDefaultPins();
		return Node;
	}

	OutError = FString::Printf(TEXT("Node '%s': Unknown type '%s'"), *Desc.Ref, *Type);
	return nullptr;
}

// ────────────────────────────────────────────────────────────────
// BuildGraph
// ────────────────────────────────────────────────────────────────

bool UBlueprintService::BuildGraph(
	const FString& BlueprintPath,
	const FString& GraphName,
	const TArray<FGraphNodeDesc>& Nodes,
	const TArray<FGraphConnectionDesc>& Connections,
	const TArray<FGraphPinDefaultDesc>& PinDefaults,
	bool bAutoLayout,
	bool bCompileAfter,
	FBuildGraphResult& OutResult)
{
	OutResult = FBuildGraphResult();

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		OutResult.Errors.Add(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
		UE_LOG(LogTemp, Error, TEXT("BuildGraph: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = ResolveBlueprintGraph(Blueprint, GraphName);
	if (!Graph)
	{
		Graph = FindGraph(Blueprint, GraphName);
	}
	if (!Graph)
	{
		OutResult.Errors.Add(FString::Printf(TEXT("Graph '%s' not found in %s"), *GraphName, *BlueprintPath));
		UE_LOG(LogTemp, Error, TEXT("BuildGraph: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return false;
	}

	// Wrap entire operation in a scoped transaction for Ctrl+Z undo
	FScopedTransaction Transaction(NSLOCTEXT("BlueprintService", "BuildGraph", "Build Graph (Batch)"));

	// Map from local ref → created node pointer
	TMap<FString, UEdGraphNode*> RefToNode;

	// ── Phase 1: Create Nodes ──
	UE_LOG(LogTemp, Log, TEXT("BuildGraph: Creating %d nodes in %s::%s"), Nodes.Num(), *BlueprintPath, *GraphName);

	// Position layout: spread nodes out if auto-layout will run later
	float CurrentX = 0.0f;
	float CurrentY = 0.0f;
	const float SpacingX = 400.0f;
	const float SpacingY = 200.0f;

	for (int32 i = 0; i < Nodes.Num(); i++)
	{
		const FGraphNodeDesc& Desc = Nodes[i];

		if (Desc.Ref.IsEmpty())
		{
			OutResult.Errors.Add(FString::Printf(TEXT("Node at index %d has empty ref"), i));
			OutResult.NodesFailed++;
			continue;
		}

		if (RefToNode.Contains(Desc.Ref))
		{
			OutResult.Errors.Add(FString::Printf(TEXT("Duplicate ref '%s' at index %d"), *Desc.Ref, i));
			OutResult.NodesFailed++;
			continue;
		}

		// Place in a grid if auto-layout is on (positions will be overwritten)
		float PosX = bAutoLayout ? (float)(i % 5) * SpacingX : (float)(i % 5) * SpacingX;
		float PosY = bAutoLayout ? (float)(i / 5) * SpacingY : (float)(i / 5) * SpacingY;

		FString Error;
		UEdGraphNode* NewNode = CreateNodeFromDesc(Blueprint, Graph, Desc, PosX, PosY, Error);

		if (NewNode)
		{
			RefToNode.Add(Desc.Ref, NewNode);
			OutResult.RefToNodeId.Add(Desc.Ref, NewNode->NodeGuid.ToString());
			OutResult.NodesCreated++;
			UE_LOG(LogTemp, Log, TEXT("BuildGraph: Created node '%s' (%s) → %s"), *Desc.Ref, *Desc.Type, *NewNode->NodeGuid.ToString());
		}
		else
		{
			OutResult.Errors.Add(Error);
			OutResult.NodesFailed++;
			UE_LOG(LogTemp, Warning, TEXT("BuildGraph: %s"), *Error);
		}
	}

	// ── Phase 2: Wire Connections ──
	UE_LOG(LogTemp, Log, TEXT("BuildGraph: Wiring %d connections"), Connections.Num());

	const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema());

	// Helper: resolve a ref string to a node — tries local refs first, then existing GUIDs in the graph
	auto ResolveNodeRef = [&](const FString& Ref) -> UEdGraphNode*
	{
		// 1. Local ref from nodes created in this build_graph call
		if (UEdGraphNode** Found = RefToNode.Find(Ref))
		{
			return *Found;
		}
		// 2. Existing node GUID already in the graph (32-char hex with no hyphens)
		if (Ref.Len() == 32)
		{
			FGuid ParsedGuid;
			FGuid::Parse(Ref, ParsedGuid);
			if (ParsedGuid.IsValid())
			{
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					if (Node && Node->NodeGuid == ParsedGuid)
					{
						return Node;
					}
				}
			}
		}
		return nullptr;
	};

	for (int32 i = 0; i < Connections.Num(); i++)
	{
		const FGraphConnectionDesc& Conn = Connections[i];

		// Parse "Ref.PinName" format
		FString FromRef, FromPinName, ToRef, ToPinName;

		if (!Conn.From.Split(TEXT("."), &FromRef, &FromPinName))
		{
			OutResult.Warnings.Add(FString::Printf(TEXT("Connection %d: Invalid 'from' format '%s' (expected 'Ref.PinName')"), i, *Conn.From));
			OutResult.ConnectionsFailed++;
			continue;
		}
		if (!Conn.To.Split(TEXT("."), &ToRef, &ToPinName))
		{
			OutResult.Warnings.Add(FString::Printf(TEXT("Connection %d: Invalid 'to' format '%s' (expected 'Ref.PinName')"), i, *Conn.To));
			OutResult.ConnectionsFailed++;
			continue;
		}

		UEdGraphNode* FromNode = ResolveNodeRef(FromRef);
		if (!FromNode)
		{
			OutResult.Warnings.Add(FString::Printf(TEXT("Connection %d: Source ref '%s' not found (not a local ref or existing GUID)"), i, *FromRef));
			OutResult.ConnectionsFailed++;
			continue;
		}
		UEdGraphNode** FromNodePtr = &FromNode;

		UEdGraphNode* ToNode = ResolveNodeRef(ToRef);
		if (!ToNode)
		{
			OutResult.Warnings.Add(FString::Printf(TEXT("Connection %d: Target ref '%s' not found (not a local ref or existing GUID)"), i, *ToRef));
			OutResult.ConnectionsFailed++;
			continue;
		}
		UEdGraphNode** ToNodePtr = &ToNode;

		UEdGraphPin* SourcePin = ResolvePinByName(*FromNodePtr, FromPinName, EGPD_Output);
		if (!SourcePin)
		{
			OutResult.Warnings.Add(FString::Printf(TEXT("Connection %d: Output pin '%s' not found on '%s'. Available outputs: [%s]"),
				i, *FromPinName, *FromRef, *GetAvailablePinNames(*FromNodePtr, EGPD_Output)));
			OutResult.ConnectionsFailed++;
			continue;
		}

		UEdGraphPin* TargetPin = ResolvePinByName(*ToNodePtr, ToPinName, EGPD_Input);
		if (!TargetPin)
		{
			OutResult.Warnings.Add(FString::Printf(TEXT("Connection %d: Input pin '%s' not found on '%s'. Available inputs: [%s]"),
				i, *ToPinName, *ToRef, *GetAvailablePinNames(*ToNodePtr, EGPD_Input)));
			OutResult.ConnectionsFailed++;
			continue;
		}

		bool bConnected = Schema ? Schema->TryCreateConnection(SourcePin, TargetPin) : false;
		if (bConnected)
		{
			OutResult.ConnectionsMade++;
		}
		else
		{
			OutResult.Warnings.Add(FString::Printf(TEXT("Connection %d: Schema rejected '%s.%s' → '%s.%s' (type mismatch?)"),
				i, *FromRef, *FromPinName, *ToRef, *ToPinName));
			OutResult.ConnectionsFailed++;
		}
	}

	// ── Phase 3: Set Pin Defaults ──
	UE_LOG(LogTemp, Log, TEXT("BuildGraph: Setting %d pin defaults"), PinDefaults.Num());

	for (int32 i = 0; i < PinDefaults.Num(); i++)
	{
		const FGraphPinDefaultDesc& PinDefault = PinDefaults[i];

		UEdGraphNode* ResolvedNode = ResolveNodeRef(PinDefault.NodeRef);
		if (!ResolvedNode)
		{
			OutResult.Warnings.Add(FString::Printf(TEXT("PinDefault %d: Node ref '%s' not found (not a local ref or existing GUID)"), i, *PinDefault.NodeRef));
			OutResult.DefaultsFailed++;
			continue;
		}
		UEdGraphNode** NodePtr = &ResolvedNode;

		UEdGraphPin* Pin = ResolvePinByName(*NodePtr, PinDefault.PinName, EGPD_Input);
		if (!Pin)
		{
			OutResult.Warnings.Add(FString::Printf(TEXT("PinDefault %d: Pin '%s' not found on '%s'. Available inputs: [%s]"),
				i, *PinDefault.PinName, *PinDefault.NodeRef, *GetAvailablePinNames(*NodePtr, EGPD_Input)));
			OutResult.DefaultsFailed++;
			continue;
		}

		if (Schema)
		{
			Schema->TrySetDefaultValue(*Pin, PinDefault.Value);
		}
		else
		{
			Pin->DefaultValue = PinDefault.Value;
		}

		OutResult.DefaultsSet++;
	}

	// ── Phase 4: Auto-Layout ──
	if (bAutoLayout)
	{
		FString LayoutError;
		AutoLayoutGraph(BlueprintPath, GraphName, LayoutError);
		if (!LayoutError.IsEmpty())
		{
			OutResult.Warnings.Add(FString::Printf(TEXT("Auto-layout warning: %s"), *LayoutError));
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	// ── Phase 5: Compile ──
	if (bCompileAfter)
	{
		FCompilerResultsLog CompileResults;
		CompileResults.bSilentMode = false;
		CompileResults.bLogInfoOnly = false;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &CompileResults);

		OutResult.bCompiled = true;
		OutResult.CompileErrors = CompileResults.NumErrors;
		OutResult.CompileWarnings = CompileResults.NumWarnings;

		for (const TSharedRef<FTokenizedMessage>& Msg : CompileResults.Messages)
		{
			const FString MsgText = Msg->ToText().ToString();
			if (Msg->GetSeverity() == EMessageSeverity::Error)
			{
				OutResult.Errors.Add(FString::Printf(TEXT("Compile: %s"), *MsgText));
			}
			else if (Msg->GetSeverity() == EMessageSeverity::Warning || Msg->GetSeverity() == EMessageSeverity::PerformanceWarning)
			{
				OutResult.Warnings.Add(FString::Printf(TEXT("Compile: %s"), *MsgText));
			}
		}
	}

	OutResult.bSuccess = (OutResult.NodesFailed == 0 && OutResult.CompileErrors == 0);

	UE_LOG(LogTemp, Log, TEXT("BuildGraph: Complete — %d/%d nodes, %d/%d connections, %d/%d defaults. Success: %s"),
		OutResult.NodesCreated, Nodes.Num(),
		OutResult.ConnectionsMade, Connections.Num(),
		OutResult.DefaultsSet, PinDefaults.Num(),
		OutResult.bSuccess ? TEXT("true") : TEXT("false"));

	return OutResult.bSuccess;
}

// ────────────────────────────────────────────────────────────────
// AutoLayoutGraph
// ────────────────────────────────────────────────────────────────

bool UBlueprintService::AutoLayoutGraph(
	const FString& BlueprintPath,
	const FString& GraphName,
	FString& OutError)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		OutError = FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = ResolveBlueprintGraph(Blueprint, GraphName);
	if (!Graph)
	{
		Graph = FindGraph(Blueprint, GraphName);
	}
	if (!Graph)
	{
		OutError = FString::Printf(TEXT("Graph '%s' not found"), *GraphName);
		return false;
	}

	// Collect all layoutable nodes (skip comments and knots for now)
	TArray<UEdGraphNode*> LayoutNodes;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && Node->CanUserDeleteNode())
		{
			LayoutNodes.Add(Node);
		}
		else if (Node)
		{
			// Entry/Result nodes — include them too
			LayoutNodes.Add(Node);
		}
	}

	if (LayoutNodes.Num() == 0)
	{
		return true; // Nothing to layout
	}

	// ── Pass 1: Layer Assignment (longest-path from roots) ──
	// Build adjacency: exec output → target node
	TMap<UEdGraphNode*, TArray<UEdGraphNode*>> ExecSuccessors;
	TMap<UEdGraphNode*, TArray<UEdGraphNode*>> ExecPredecessors;
	TMap<UEdGraphNode*, TArray<UEdGraphNode*>> DataConsumers; // pure node → nodes that use its output

	for (UEdGraphNode* Node : LayoutNodes)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin) continue;

			if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (LinkedPin && LinkedPin->GetOwningNode())
					{
						UEdGraphNode* Target = LinkedPin->GetOwningNode();
						ExecSuccessors.FindOrAdd(Node).AddUnique(Target);
						ExecPredecessors.FindOrAdd(Target).AddUnique(Node);
					}
				}
			}
			else if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (LinkedPin && LinkedPin->GetOwningNode())
					{
						DataConsumers.FindOrAdd(Node).AddUnique(LinkedPin->GetOwningNode());
					}
				}
			}
		}
	}

	// Identify roots: nodes with no incoming exec
	TArray<UEdGraphNode*> Roots;
	for (UEdGraphNode* Node : LayoutNodes)
	{
		bool bHasExecInput = false;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->Direction == EGPD_Input && Pin->LinkedTo.Num() > 0)
			{
				bHasExecInput = true;
				break;
			}
		}

		// Also check if node has exec pins at all
		bool bHasAnyExecPin = false;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				bHasAnyExecPin = true;
				break;
			}
		}

		if (!bHasExecInput && bHasAnyExecPin)
		{
			Roots.Add(Node);
		}
	}

	// BFS to assign layers
	TMap<UEdGraphNode*, int32> NodeLayer;
	TQueue<UEdGraphNode*> Queue;

	for (UEdGraphNode* Root : Roots)
	{
		NodeLayer.Add(Root, 0);
		Queue.Enqueue(Root);
	}

	while (!Queue.IsEmpty())
	{
		UEdGraphNode* Current;
		Queue.Dequeue(Current);

		int32 CurrentLayer = NodeLayer[Current];
		const TArray<UEdGraphNode*>* Succs = ExecSuccessors.Find(Current);
		if (Succs)
		{
			for (UEdGraphNode* Succ : *Succs)
			{
				int32 NewLayer = CurrentLayer + 1;
				int32* ExistingLayer = NodeLayer.Find(Succ);
				if (!ExistingLayer || *ExistingLayer < NewLayer)
				{
					NodeLayer.Add(Succ, NewLayer);
					Queue.Enqueue(Succ);
				}
			}
		}
	}

	// Check if exec-based BFS produced any meaningful layering.
	// If all layered nodes are at layer 0 (common for pure functions with no exec connections),
	// fall back to data-flow-based layering instead.
	bool bExecFlowIsFlat = true;
	for (auto& Pair : NodeLayer)
	{
		if (Pair.Value > 0)
		{
			bExecFlowIsFlat = false;
			break;
		}
	}

	if (bExecFlowIsFlat && LayoutNodes.Num() > 1)
	{
		// ── Data-flow fallback: assign layers by longest data-flow path from sources ──
		// Build data-flow adjacency: for each node, find nodes whose outputs feed into it
		TMap<UEdGraphNode*, TArray<UEdGraphNode*>> DataPredecessors;
		for (UEdGraphNode* Node : LayoutNodes)
		{
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
				{
					for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
					{
						if (LinkedPin && LinkedPin->GetOwningNode())
						{
							DataPredecessors.FindOrAdd(Node).AddUnique(LinkedPin->GetOwningNode());
						}
					}
				}
			}
		}

		// Topological sort via BFS (Kahn's algorithm) to assign layers by data-flow depth.
		// Nodes with no data predecessors start at layer 0.
		NodeLayer.Empty();

		// Compute in-degree for each node in data flow
		TMap<UEdGraphNode*, int32> InDegree;
		for (UEdGraphNode* Node : LayoutNodes)
		{
			InDegree.FindOrAdd(Node); // ensure entry exists (default 0)
		}
		for (auto& Pair : DataPredecessors)
		{
			InDegree.FindOrAdd(Pair.Key) = Pair.Value.Num();
		}

		// Seed BFS with data sources (in-degree 0)
		TQueue<UEdGraphNode*> DataQueue;
		for (auto& Pair : InDegree)
		{
			if (Pair.Value == 0)
			{
				NodeLayer.Add(Pair.Key, 0);
				DataQueue.Enqueue(Pair.Key);
			}
		}

		while (!DataQueue.IsEmpty())
		{
			UEdGraphNode* Current;
			DataQueue.Dequeue(Current);
			int32 CurrentLayer = NodeLayer.FindRef(Current);

			const TArray<UEdGraphNode*>* Consumers = DataConsumers.Find(Current);
			if (Consumers)
			{
				for (UEdGraphNode* Consumer : *Consumers)
				{
					int32 NewLayer = CurrentLayer + 1;
					int32* ExistingLayer = NodeLayer.Find(Consumer);
					if (!ExistingLayer || *ExistingLayer < NewLayer)
					{
						NodeLayer.Add(Consumer, NewLayer);
					}

					// Decrement in-degree; enqueue when all predecessors processed
					int32& Deg = InDegree.FindOrAdd(Consumer);
					Deg--;
					if (Deg <= 0)
					{
						DataQueue.Enqueue(Consumer);
					}
				}
			}
		}

		// Assign any remaining unreached nodes to layer 0
		for (UEdGraphNode* Node : LayoutNodes)
		{
			if (!NodeLayer.Contains(Node))
			{
				NodeLayer.Add(Node, 0);
			}
		}
	}
	else
	{
		// Original exec-based layering worked — place pure nodes relative to consumers.
		for (UEdGraphNode* Node : LayoutNodes)
		{
			if (NodeLayer.Contains(Node)) continue;

			bool bHasExecPin = false;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
				{
					bHasExecPin = true;
					break;
				}
			}

			if (!bHasExecPin)
			{
				// Find the minimum layer of any consumer
				int32 MinConsumerLayer = INT32_MAX;
				const TArray<UEdGraphNode*>* Consumers = DataConsumers.Find(Node);
				if (Consumers)
				{
					for (UEdGraphNode* Consumer : *Consumers)
					{
						const int32* ConsumerLayer = NodeLayer.Find(Consumer);
						if (ConsumerLayer && *ConsumerLayer < MinConsumerLayer)
						{
							MinConsumerLayer = *ConsumerLayer;
						}
					}
				}

				if (MinConsumerLayer == INT32_MAX)
				{
					MinConsumerLayer = 0; // Disconnected pure node
				}

				// Place pure nodes one column LEFT of their consumer so they appear
				// as data inputs flowing into the exec node, not stacked alongside it.
				NodeLayer.Add(Node, FMath::Max(0, MinConsumerLayer - 1));
			}
			else
			{
				// Exec node never reached by BFS — disconnected island
				NodeLayer.Add(Node, 0);
			}
		}
	}

	// ── Pass 2: Identify connected execution chains for vertical band separation ──
	// Each independent event chain gets its own Y band so chains never overlap.
	// Build reverse data lookup: consumer → pure nodes that feed it.
	TMap<UEdGraphNode*, TArray<UEdGraphNode*>> DataProviders;
	for (auto& Pair : DataConsumers)
		for (UEdGraphNode* Consumer : Pair.Value)
			DataProviders.FindOrAdd(Consumer).AddUnique(Pair.Key);

	// BFS flood-fill treating exec + data edges as undirected to find components.
	TMap<UEdGraphNode*, int32> NodeChainId;
	int32 NumChains = 0;
	for (UEdGraphNode* StartNode : LayoutNodes)
	{
		if (NodeChainId.Contains(StartNode)) continue;
		int32 ChainId = NumChains++;
		TQueue<UEdGraphNode*> BFSQ;
		BFSQ.Enqueue(StartNode);
		NodeChainId.Add(StartNode, ChainId);
		while (!BFSQ.IsEmpty())
		{
			UEdGraphNode* Cur;
			BFSQ.Dequeue(Cur);
			auto Visit = [&](UEdGraphNode* N)
			{
				if (N && !NodeChainId.Contains(N))
				{
					NodeChainId.Add(N, ChainId);
					BFSQ.Enqueue(N);
				}
			};
			if (const TArray<UEdGraphNode*>* S = ExecSuccessors.Find(Cur))   for (UEdGraphNode* N : *S) Visit(N);
			if (const TArray<UEdGraphNode*>* P = ExecPredecessors.Find(Cur)) for (UEdGraphNode* N : *P) Visit(N);
			if (const TArray<UEdGraphNode*>* P = DataProviders.Find(Cur))    for (UEdGraphNode* N : *P) Visit(N);
			if (const TArray<UEdGraphNode*>* C = DataConsumers.Find(Cur))    for (UEdGraphNode* N : *C) Visit(N);
		}
	}

	// Sort chains: most event nodes first, then by total node count (largest chains on top).
	TMap<int32, int32> ChainEventCount;
	TMap<int32, int32> ChainNodeCount;
	for (auto& Pair : NodeChainId)
	{
		ChainNodeCount.FindOrAdd(Pair.Value)++;
		if (Pair.Key->IsA<UK2Node_Event>() || Pair.Key->IsA<UK2Node_CustomEvent>())
			ChainEventCount.FindOrAdd(Pair.Value)++;
	}

	TArray<int32> ChainOrder;
	for (int32 i = 0; i < NumChains; i++) ChainOrder.Add(i);
	ChainOrder.Sort([&](int32 A, int32 B)
	{
		int32 AE = ChainEventCount.FindRef(A), BE = ChainEventCount.FindRef(B);
		if (AE != BE) return AE > BE;
		return ChainNodeCount.FindRef(A) > ChainNodeCount.FindRef(B);
	});

	TMap<int32, int32> ChainRank; // original chain ID → display rank
	for (int32 i = 0; i < ChainOrder.Num(); i++)
		ChainRank.Add(ChainOrder[i], i);

	// ── Pass 3: Group by (chain rank, layer) and sort within each group ──
	// Rank → Layer → nodes
	TMap<int32, TMap<int32, TArray<UEdGraphNode*>>> ChainLayerMap;
	for (auto& Pair : NodeLayer)
	{
		int32 Rank = ChainRank.FindRef(NodeChainId.FindRef(Pair.Key));
		ChainLayerMap.FindOrAdd(Rank).FindOrAdd(Pair.Value).Add(Pair.Key);
	}

	// Within each (chain, layer): event nodes first, then exec nodes, then pure nodes.
	// Within each group sort alphabetically.
	for (auto& ChainPair : ChainLayerMap)
	{
		for (auto& LayerPair : ChainPair.Value)
		{
			LayerPair.Value.Sort([](const UEdGraphNode& A, const UEdGraphNode& B)
			{
				bool aIsEvent = A.IsA<UK2Node_Event>() || A.IsA<UK2Node_CustomEvent>();
				bool bIsEvent = B.IsA<UK2Node_Event>() || B.IsA<UK2Node_CustomEvent>();
				if (aIsEvent != bIsEvent) return aIsEvent;

				bool aHasExec = false, bHasExec = false;
				for (const UEdGraphPin* P : A.Pins)
					if (P && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) { aHasExec = true; break; }
				for (const UEdGraphPin* P : B.Pins)
					if (P && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) { bHasExec = true; break; }
				if (aHasExec != bHasExec) return aHasExec;

				return A.GetNodeTitle(ENodeTitleType::FullTitle).ToString()
					 < B.GetNodeTitle(ENodeTitleType::FullTitle).ToString();
			});
		}
	}

	// ── Pass 4: Assign positions with per-chain Y bands ──
	// X uses the global layer key (so all chains share aligned columns).
	// Y uses a per-chain base offset so chains never overlap vertically.
	const float ColumnWidth  = 450.0f;
	const float RowHeight    = 180.0f;
	const float ChainGap     = 120.0f; // extra vertical gap between independent chains
	const float MarginLeft   = 100.0f;
	const float MarginTop    = 100.0f;

	// Compute Y base for each chain rank.
	TArray<int32> SortedRanks;
	ChainLayerMap.GetKeys(SortedRanks);
	SortedRanks.Sort();

	TMap<int32, float> ChainBaseY;
	float CurY = MarginTop;
	for (int32 Rank : SortedRanks)
	{
		ChainBaseY.Add(Rank, CurY);
		int32 MaxNodesInLayer = 0;
		for (auto& LayerPair : ChainLayerMap[Rank])
			MaxNodesInLayer = FMath::Max(MaxNodesInLayer, LayerPair.Value.Num());
		CurY += (MaxNodesInLayer * RowHeight) + ChainGap;
	}

	FScopedTransaction Transaction(NSLOCTEXT("BlueprintService", "AutoLayout", "Auto-Layout Graph"));

	for (auto& ChainPair : ChainLayerMap)
	{
		float BaseY = ChainBaseY.FindRef(ChainPair.Key);
		for (auto& LayerPair : ChainPair.Value)
		{
			int32 LayerKey = LayerPair.Key;
			const TArray<UEdGraphNode*>& Nodes = LayerPair.Value;
			for (int32 NodeIdx = 0; NodeIdx < Nodes.Num(); NodeIdx++)
			{
				UEdGraphNode* Node = Nodes[NodeIdx];
				Node->Modify();
				Node->NodePosX = MarginLeft + (LayerKey * ColumnWidth);
				Node->NodePosY = BaseY + (NodeIdx * RowHeight);
			}
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("AutoLayoutGraph: Laid out %d nodes in %d chains in %s::%s"),
		LayoutNodes.Num(), NumChains, *BlueprintPath, *GraphName);

	return true;
}

// ────────────────────────────────────────────────────────────────
// GetGraphDefinition
// ────────────────────────────────────────────────────────────────

bool UBlueprintService::GetGraphDefinition(
	const FString& BlueprintPath,
	const FString& GraphName,
	TArray<FGraphNodeDesc>& OutNodes,
	TArray<FGraphConnectionDesc>& OutConnections,
	TArray<FGraphPinDefaultDesc>& OutPinDefaults,
	FString& OutError)
{
	OutNodes.Empty();
	OutConnections.Empty();
	OutPinDefaults.Empty();

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		OutError = FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = ResolveBlueprintGraph(Blueprint, GraphName);
	if (!Graph)
	{
		Graph = FindGraph(Blueprint, GraphName);
	}
	if (!Graph)
	{
		OutError = FString::Printf(TEXT("Graph '%s' not found"), *GraphName);
		return false;
	}

	// Build node → ref map
	TMap<UEdGraphNode*, FString> NodeToRef;
	int32 UnnamedIdx = 0;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node) continue;

		FGraphNodeDesc Desc;

		// Determine type and params by inspecting the node class
		if (UK2Node_CallFunction* FuncNode = Cast<UK2Node_CallFunction>(Node))
		{
			const UFunction* Function = FuncNode->GetTargetFunction();
			if (Function)
			{
				Desc.Type = TEXT("function_call");
				Desc.Params.Add(TEXT("class"), Function->GetOwnerClass()->GetName());
				Desc.Params.Add(TEXT("function"), Function->GetName());

				// Generate a readable ref from function name
				Desc.Ref = FString::Printf(TEXT("%s_%d"), *Function->GetName(), UnnamedIdx++);
			}
			else
			{
				Desc.Type = TEXT("spawner_key");
				Desc.Params.Add(TEXT("key"), FString::Printf(TEXT("FUNC %s"), *FuncNode->FunctionReference.GetMemberName().ToString()));
				Desc.Ref = FString::Printf(TEXT("Node_%d"), UnnamedIdx++);
			}
		}
		else if (UK2Node_VariableGet* GetNode = Cast<UK2Node_VariableGet>(Node))
		{
			// Check if it's a member get (external reference) or self variable get
			if (GetNode->VariableReference.IsLocalScope() || GetNode->VariableReference.IsSelfContext())
			{
				Desc.Type = TEXT("variable_get");
				Desc.Params.Add(TEXT("variable"), GetNode->VariableReference.GetMemberName().ToString());
			}
			else
			{
				Desc.Type = TEXT("member_get");
				Desc.Params.Add(TEXT("member"), GetNode->VariableReference.GetMemberName().ToString());
				if (UClass* MemberParent = GetNode->VariableReference.GetMemberParentClass())
				{
					Desc.Params.Add(TEXT("class"), MemberParent->GetName());
				}
			}
			Desc.Ref = FString::Printf(TEXT("Get_%s_%d"), *GetNode->VariableReference.GetMemberName().ToString(), UnnamedIdx++);
		}
		else if (UK2Node_VariableSet* SetNode = Cast<UK2Node_VariableSet>(Node))
		{
			Desc.Type = TEXT("variable_set");
			Desc.Params.Add(TEXT("variable"), SetNode->VariableReference.GetMemberName().ToString());
			Desc.Ref = FString::Printf(TEXT("Set_%s_%d"), *SetNode->VariableReference.GetMemberName().ToString(), UnnamedIdx++);
		}
		else if (UK2Node_IfThenElse* BranchNode = Cast<UK2Node_IfThenElse>(Node))
		{
			Desc.Type = TEXT("branch");
			Desc.Ref = FString::Printf(TEXT("Branch_%d"), UnnamedIdx++);
		}
		else if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
		{
			Desc.Type = TEXT("cast");
			if (CastNode->TargetType)
			{
				Desc.Params.Add(TEXT("target_class"), CastNode->TargetType->GetName());
			}
			Desc.Ref = FString::Printf(TEXT("Cast_%d"), UnnamedIdx++);
		}
		else if (UK2Node_CustomEvent* CustomEventNode = Cast<UK2Node_CustomEvent>(Node))
		{
			Desc.Type = TEXT("custom_event");
			Desc.Params.Add(TEXT("name"), CustomEventNode->CustomFunctionName.ToString());
			Desc.Ref = FString::Printf(TEXT("CE_%s"), *CustomEventNode->CustomFunctionName.ToString());
		}
		else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
		{
			Desc.Type = TEXT("event");
			Desc.Params.Add(TEXT("event"), EventNode->EventReference.GetMemberName().ToString());
			Desc.Ref = EventNode->EventReference.GetMemberName().ToString();
		}
		else if (UK2Node_EnhancedInputAction* InputNode = Cast<UK2Node_EnhancedInputAction>(Node))
		{
			Desc.Type = TEXT("input_action");
			if (InputNode->InputAction)
			{
				Desc.Params.Add(TEXT("action"), InputNode->InputAction->GetPathName());
			}
			Desc.Ref = FString::Printf(TEXT("Input_%d"), UnnamedIdx++);
		}
		else if (UK2Node_AddDelegate* DelegateNode = Cast<UK2Node_AddDelegate>(Node))
		{
			Desc.Type = TEXT("delegate_bind");
			Desc.Params.Add(TEXT("delegate"), DelegateNode->GetPropertyName().ToString());
			Desc.Ref = FString::Printf(TEXT("Bind_%d"), UnnamedIdx++);
		}
		else if (UK2Node_CreateDelegate* CreateDelegateNode = Cast<UK2Node_CreateDelegate>(Node))
		{
			Desc.Type = TEXT("create_delegate");
			Desc.Params.Add(TEXT("function"), CreateDelegateNode->SelectedFunctionName.ToString());
			Desc.Ref = FString::Printf(TEXT("CreateDelegate_%d"), UnnamedIdx++);
		}
		else if (Node->IsA<UK2Node_FunctionEntry>() || Node->IsA<UK2Node_FunctionResult>())
		{
			// Function entry/result — skip, these are auto-created
			continue;
		}
		else
		{
			// Generic fallback: use spawner_key with NODE prefix
			Desc.Type = TEXT("spawner_key");
			Desc.Params.Add(TEXT("key"), FString::Printf(TEXT("NODE %s"), *Node->GetClass()->GetName()));
			Desc.Ref = FString::Printf(TEXT("Node_%d"), UnnamedIdx++);
		}

		NodeToRef.Add(Node, Desc.Ref);
		OutNodes.Add(Desc);

		// Collect non-default pin values
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input &&
				!Pin->DefaultValue.IsEmpty() &&
				Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec &&
				Pin->LinkedTo.Num() == 0)
			{
				FGraphPinDefaultDesc PinDefault;
				PinDefault.NodeRef = Desc.Ref;
				PinDefault.PinName = Pin->PinName.ToString();
				PinDefault.Value = Pin->DefaultValue;
				OutPinDefaults.Add(PinDefault);
			}
		}
	}

	// Build connections
	TSet<FString> SeenConnections; // Prevent duplicates
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node || !NodeToRef.Contains(Node)) continue;

		const FString& SourceRef = NodeToRef[Node];

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output) continue;

			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (!LinkedPin) continue;
				UEdGraphNode* TargetNode = LinkedPin->GetOwningNode();
				if (!TargetNode || !NodeToRef.Contains(TargetNode)) continue;

				const FString& TargetRef = NodeToRef[TargetNode];
				FString ConnKey = FString::Printf(TEXT("%s.%s→%s.%s"),
					*SourceRef, *Pin->PinName.ToString(),
					*TargetRef, *LinkedPin->PinName.ToString());

				if (SeenConnections.Contains(ConnKey)) continue;
				SeenConnections.Add(ConnKey);

				FGraphConnectionDesc Conn;
				Conn.From = FString::Printf(TEXT("%s.%s"), *SourceRef, *Pin->PinName.ToString());
				Conn.To = FString::Printf(TEXT("%s.%s"), *TargetRef, *LinkedPin->PinName.ToString());
				OutConnections.Add(Conn);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("GetGraphDefinition: Exported %d nodes, %d connections, %d pin defaults from %s::%s"),
		OutNodes.Num(), OutConnections.Num(), OutPinDefaults.Num(), *BlueprintPath, *GraphName);

	return true;
}
