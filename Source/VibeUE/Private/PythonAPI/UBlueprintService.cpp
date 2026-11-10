// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UBlueprintService.h"
#include "PythonAPI/BlueprintTypeParser.h"
#include "Engine/CollisionProfile.h"     // Custom collision channel names (set_collision_settings)
#include "PythonAPI/BlueprintClassResolve.h" // Short-name ambiguity policy (VibeBlueprintClassResolve)
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Level.h"                // Level Blueprint resolution (LoadBlueprint on a map path)
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
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
#include "K2Node_Variable.h"            // Base of Get/Set nodes — reference counting for RemoveMemberVariable
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallArrayFunction.h"   // For array library functions with wildcard pins
#include "K2Node_GetArrayItem.h"        // For Array Get (replaces deprecated Array_Get)
#include "K2Node_CreateDelegate.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_CustomEvent.h"
#include "EdGraphSchema_K2_Actions.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_EnhancedInputAction.h"  // For Enhanced Input Action event nodes
#include "K2Node_AddDelegate.h"          // For delegate bind nodes (add_delegate_bind_node)
#include "K2Node_CreateDelegate.h"       // For create event nodes (add_create_delegate_node)
#include "K2Node_CallDelegate.h"         // For dispatcher broadcast nodes (add_call_delegate_node)
#include "K2Node_MakeStruct.h"           // For STRUCT key: creating typed struct Make nodes
#include "K2Node_Timeline.h"             // For UK2Node_Timeline (add_timeline)
#include "Engine/TimelineTemplate.h"     // For UTimelineTemplate / FTTFloatTrack
#include "Curves/CurveFloat.h"           // For UCurveFloat / FRichCurve (timeline float tracks)
#include "Curves/CurveVector.h"          // For UCurveVector (timeline vector tracks)
#include "Curves/CurveLinearColor.h"     // For UCurveLinearColor (timeline color tracks)
#include "EdGraphNode_Comment.h"         // For UEdGraphNode_Comment (comment box nodes)
#include "K2Node_MacroInstance.h"        // For UK2Node_MacroInstance (add_macro_instance_node)
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
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#include "Factories/BlueprintFactory.h"
#include "WidgetBlueprintFactory.h"      // For creating WidgetBlueprints (UBlueprintFactory can't)
#include "Blueprint/UserWidget.h"
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
#include "Framework/Docking/TabManager.h"   // For FGlobalTabmanager (active-tab anchoring)
#include "Widgets/Docking/SDockTab.h"        // For SDockTab::GetTabManagerPtr
#include "Materials/Material.h"              // For UMaterial::MaterialGraph (focused-graph context)
#include "MaterialGraph/MaterialGraph.h"     // For UMaterialGraph
#include "IMaterialEditor.h"                 // For IMaterialEditor::GetSelectedNodes
#include "AssetRegistry/ARFilter.h"          // For FARFilter (resolve original material asset path)
// For FScopedTransaction (undo support)
#include "ScopedTransaction.h"
// For AnalyzeGraphLayout JSON report
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"

UClass* VibeBlueprintClassResolve::ChooseCandidate(const FString& ShortName, const TArray<FCandidate>& Candidates)
{
	if (Candidates.Num() > 1)
	{
		TArray<FString> Paths;
		for (const FCandidate& Candidate : Candidates)
		{
			Paths.Add(Candidate.ObjectPath);
		}
		Paths.Sort();
		UE_LOG(LogTemp, Warning, TEXT("ResolveClassByName: short name '%s' is ambiguous, %d Blueprints match: %s. Pass the full asset path instead."),
			*ShortName, Paths.Num(), *FString::Join(Paths, TEXT(", ")));
		return nullptr;
	}

	if (Candidates.Num() == 1)
	{
		if (Candidates[0].Class)
		{
			return Candidates[0].Class;
		}
		if (Candidates[0].Asset.IsValid())
		{
			if (UBlueprint* Blueprint = Cast<UBlueprint>(Candidates[0].Asset.GetAsset()))
			{
				return Blueprint->GeneratedClass;
			}
		}
	}

	return nullptr;
}

namespace
{
	class FScopedVibePropertyValue
	{
	public:
		explicit FScopedVibePropertyValue(FProperty* InProperty)
			: Property(InProperty)
			, Data(InProperty ? FMemory::Malloc(InProperty->GetSize(), InProperty->GetMinAlignment()) : nullptr)
		{
			if (Data)
			{
				Property->InitializeValue(Data);
			}
		}

		~FScopedVibePropertyValue()
		{
			if (Data)
			{
				Property->DestroyValue(Data);
				FMemory::Free(Data);
			}
		}

		FScopedVibePropertyValue(const FScopedVibePropertyValue&) = delete;
		FScopedVibePropertyValue& operator=(const FScopedVibePropertyValue&) = delete;

		explicit operator bool() const { return Data != nullptr; }
		void* GetData() const { return Data; }

	private:
		FProperty* Property = nullptr;
		void* Data = nullptr;
	};

	/** Parse into default-initialized storage, then replace the destination as one value. ImportText
	 * otherwise merges compound members into their existing storage, so an explicit empty array or
	 * tag container can leave old entries behind. */
	static bool ImportPropertyValueReplacing(
		FProperty* Property,
		void* Destination,
		UObject* Owner,
		const FString& Text)
	{
		FScopedVibePropertyValue ParsedValue(Property);
		if (!ParsedValue || !Property->ImportText_Direct(*Text, ParsedValue.GetData(), Owner, PPF_None))
		{
			return false;
		}

		Property->CopyCompleteValue(Destination, ParsedValue.GetData());
		return true;
	}

	/**
	 * UE 5.3+ keeps the two legacy GameplayEffect tag containers as read-only compatibility
	 * mirrors. UGameplayEffect::PreSave clears them and rebuilds them from GEComponents, so a
	 * successful ImportText on the legacy property alone is discarded during SaveAsset.
	 *
	 * Keep BlueprintService independent of the optional GameplayAbilities plugin by using
	 * reflection. When the target really is a GameplayEffect, mirror the requested legacy value
	 * into the modern component that owns it before the asset is saved.
	 */
	static bool TrySetGameplayEffectTagCompatibilityProperty(
		UObject* CDO,
		const FString& PropertyName,
		const FString& PropertyValue,
		bool& bOutHandled,
		FString& InOutExpectedValue)
	{
		bOutHandled = false;

		const TCHAR* ComponentClassPath = nullptr;
		const TCHAR* ComponentPropertyName = nullptr;
		if (PropertyName == TEXT("InheritableGameplayEffectTags"))
		{
			ComponentClassPath = TEXT("/Script/GameplayAbilities.AssetTagsGameplayEffectComponent");
			ComponentPropertyName = TEXT("InheritableAssetTags");
		}
		else if (PropertyName == TEXT("InheritableOwnedTagsContainer"))
		{
			ComponentClassPath = TEXT("/Script/GameplayAbilities.TargetTagsGameplayEffectComponent");
			ComponentPropertyName = TEXT("InheritableGrantedTagsContainer");
		}
		else
		{
			return true;
		}

		bool bIsGameplayEffect = false;
		for (UClass* Class = CDO ? CDO->GetClass() : nullptr; Class; Class = Class->GetSuperClass())
		{
			if (Class->GetPathName() == TEXT("/Script/GameplayAbilities.GameplayEffect"))
			{
				bIsGameplayEffect = true;
				break;
			}
		}
		if (!bIsGameplayEffect)
		{
			return true;
		}
		bOutHandled = true;

		UClass* ComponentClass = LoadObject<UClass>(nullptr, ComponentClassPath);
		FArrayProperty* ComponentsProperty = FindFProperty<FArrayProperty>(CDO->GetClass(), TEXT("GEComponents"));
		FObjectPropertyBase* ComponentObjectProperty = ComponentsProperty
			? CastField<FObjectPropertyBase>(ComponentsProperty->Inner)
			: nullptr;
		FProperty* ComponentValueProperty = ComponentClass
			? ComponentClass->FindPropertyByName(ComponentPropertyName)
			: nullptr;
		if (!ComponentClass || !ComponentsProperty || !ComponentObjectProperty || !ComponentValueProperty)
		{
			UE_LOG(LogTemp, Error,
				TEXT("SetProperty: GameplayEffect compatibility mapping for '%s' is unavailable in this engine build"),
				*PropertyName);
			return false;
		}

		void* ComponentsAddress = ComponentsProperty->ContainerPtrToValuePtr<void>(CDO);
		FScriptArrayHelper Components(ComponentsProperty, ComponentsAddress);
		UObject* Component = nullptr;
		int32 AddedComponentIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Components.Num(); ++Index)
		{
			UObject* Candidate = ComponentObjectProperty->GetObjectPropertyValue(Components.GetRawPtr(Index));
			if (Candidate && Candidate->IsA(ComponentClass))
			{
				Component = Candidate;
				break;
			}
		}

		if (!Component)
		{
			CDO->Modify();
			Component = NewObject<UObject>(
				CDO,
				ComponentClass,
				NAME_None,
				CDO->GetMaskedFlags(RF_PropagateToSubObjects) | RF_Transactional);
			if (!Component)
			{
				UE_LOG(LogTemp, Error,
					TEXT("SetProperty: Failed to create GameplayEffect component '%s' for '%s'"),
					ComponentClassPath, *PropertyName);
				return false;
			}

			AddedComponentIndex = Components.AddValue();
			ComponentObjectProperty->SetObjectPropertyValue(Components.GetRawPtr(AddedComponentIndex), Component);
			UE_LOG(LogTemp, Log,
				TEXT("SetProperty: Added GameplayEffect component '%s' for legacy property '%s'"),
				ComponentClassPath, *PropertyName);
		}

		Component->Modify();
		void* ComponentValueAddress = ComponentValueProperty->ContainerPtrToValuePtr<void>(Component);
		if (!ImportPropertyValueReplacing(
			ComponentValueProperty, ComponentValueAddress, Component, PropertyValue))
		{
			if (AddedComponentIndex != INDEX_NONE)
			{
				Components.RemoveValues(AddedComponentIndex, 1);
				Component->MarkAsGarbage();
			}
			UE_LOG(LogTemp, Error,
				TEXT("SetProperty: Failed to parse '%s' for GameplayEffect component property '%s'"),
				*PropertyValue, ComponentPropertyName);
			return false;
		}

#if WITH_EDITOR
		FPropertyChangedEvent ChangedEvent(ComponentValueProperty, EPropertyChangeType::ValueSet);
		Component->PostEditChangeProperty(ChangedEvent);
#endif
		// The component recomputes CombinedTags from Added/Removed (and any parent), so this is the
		// canonical value the legacy compatibility mirror must contain after save.
		ComponentValueProperty->ExportTextItem_Direct(
			InOutExpectedValue, ComponentValueAddress, nullptr, Component, PPF_None);
		return true;
	}

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

	// Generated classes left behind by recompiles, hot reload and deletes carry these prefixes.
	// They are never the class a caller means.
	static bool IsStaleGeneratedClassName(const FString& Name)
	{
		return Name.StartsWith(TEXT("SKEL_"), ESearchCase::CaseSensitive)
			|| Name.StartsWith(TEXT("REINST_"), ESearchCase::CaseSensitive)
			|| Name.StartsWith(TEXT("TRASHCLASS_"), ESearchCase::CaseSensitive)
			|| Name.StartsWith(TEXT("HOTRELOADED_"), ESearchCase::CaseSensitive);
	}

	// Resolve a Blueprint by short name (no "_C", no path) to its generated class. Candidates are
	// the union, de-duplicated by package, of
	//   - LOADED generated classes named "<Short>_C" whose Blueprint's current GeneratedClass is
	//     that class (stale/outdated classes skipped), and
	//   - asset-registry assets of ANY UBlueprint subclass named "<Short>" (plain, Widget, Anim,
	//     Editor Utility...). Without bSearchSubClasses only plain UBlueprint assets match (bug B6).
	// The ambiguity policy (two or more packages = refusal, no implicit preference) is
	// VibeBlueprintClassResolve::ChooseCandidate, documented in BlueprintClassResolve.h.
	static UClass* ResolveBlueprintShortName(const FString& ShortName)
	{
		// FName construction from a string longer than NAME_SIZE is a Fatal engine log.
		if (ShortName.IsEmpty() || ShortName.Len() + 2 >= NAME_SIZE)
		{
			return nullptr;
		}

		using VibeBlueprintClassResolve::FCandidate;
		TArray<FCandidate> Candidates;
		auto HasPackage = [&Candidates](FName PackageName)
		{
			return Candidates.ContainsByPredicate([PackageName](const FCandidate& C) { return C.PackageName == PackageName; });
		};

		// (1) Loaded generated classes. FNAME_Find: a name absent from the name table cannot name
		// any object, so a miss costs nothing and adds nothing to the table.
		const FName GeneratedFName(*(ShortName + TEXT("_C")), FNAME_Find);
		if (!GeneratedFName.IsNone())
		{
			TArray<UObject*> Found;
			StaticFindAllObjectsFast(Found, UClass::StaticClass(), GeneratedFName, EFindObjectFlags::None);
			for (UObject* Object : Found)
			{
				UClass* Class = Cast<UClass>(Object);
				if (!IsValid(Class) || Class->HasAnyClassFlags(CLASS_NewerVersionExists) || IsStaleGeneratedClassName(Class->GetName()))
				{
					continue;
				}
				UBlueprint* Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy);
				if (!IsValid(Blueprint) || Blueprint->GeneratedClass != Class)
				{
					continue; // native, or an outdated class its Blueprint no longer points at
				}
				const FName PackageName = Blueprint->GetPackage()->GetFName();
				if (!HasPackage(PackageName))
				{
					FCandidate& Candidate = Candidates.AddDefaulted_GetRef();
					Candidate.PackageName = PackageName;
					Candidate.ObjectPath = Blueprint->GetPathName();
					Candidate.Class = Class;
				}
			}
		}

		// (2) Asset registry, every UBlueprint subclass.
		const FName AssetFName(*ShortName, FNAME_Find);
		if (!AssetFName.IsNone())
		{
			IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
			TArray<FAssetData> Assets;
			Registry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses*/ true);
			for (const FAssetData& Asset : Assets)
			{
				if (Asset.AssetName == AssetFName && !HasPackage(Asset.PackageName))
				{
					FCandidate& Candidate = Candidates.AddDefaulted_GetRef();
					Candidate.PackageName = Asset.PackageName;
					Candidate.ObjectPath = Asset.GetObjectPathString();
					Candidate.Asset = Asset;
				}
			}
		}

		return VibeBlueprintClassResolve::ChooseCandidate(ShortName, Candidates);
	}

	// Resolve a class name as node keys, class pins and node properties spell it. Tried in order:
	//   1. A native class by its object name ("Actor", "KismetSystemLibrary"; the C++ prefix is
	//      not stripped, so "AActor" does not resolve), then /Script/Engine.<Name>. Native
	//      classes always win.
	//   2. A full asset path ("/Game/UI/WBP_Menu", "/Game/UI/WBP_Menu.WBP_Menu",
	//      "/Game/UI/WBP_Menu.WBP_Menu_C"): the asset is loaded and its generated class returned.
	//   3. A Blueprint short name with or without "_C" ("BP_Enemy", "WBP_Menu_C", "ABP_Zombie_C"),
	//      any Blueprint type, loaded or not, under the ambiguity policy of ResolveBlueprintShortName.
	// A "SKEL_" prefix is stripped first so older discover_nodes keys still resolve.
	static UClass* ResolveClassByName(const FString& ClassName)
	{
		if (ClassName.IsEmpty())
		{
			return nullptr;
		}

		// Fully qualified paths identify an exact loaded class, including
		// BlueprintGeneratedClass subclasses and classes in transient fixtures.
		// The short-name ExactClass search below intentionally matches UClass
		// itself; it must not turn a loaded generated-class path into an asset load.
		if (ClassName.StartsWith(TEXT("/")))
		{
			if (UClass* ExactClass = FindObject<UClass>(nullptr, *ClassName))
			{
				return ExactClass;
			}
		}

		// discover_nodes historically emitted skeleton-class names (SKEL_<BP>_C) for Blueprint
		// functions and variables; the skeleton class is transient and not resolvable here, but the
		// persistent generated class is "<BP>_C". Strip the prefix so those older keys still resolve.
		if (ClassName.StartsWith(TEXT("SKEL_"), ESearchCase::CaseSensitive))
		{
			return ResolveClassByName(ClassName.RightChop(5));
		}

		// ExactClass is deliberate in the native lookups: it matches only objects whose class is
		// exactly UClass (native classes), so a Blueprint can never shadow a native type. Blueprint
		// generated classes are found by the short-name step at the end.
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

		// Blueprint asset path forms: "/Game/Path/BP_Foo", "/Game/Path/BP_Foo.BP_Foo",
		// or "/Game/Path/BP_Foo.BP_Foo_C". FindObject/FindFirstObject only see classes
		// already loaded into memory — for an unloaded Blueprint we have to actually
		// load the asset to materialize its GeneratedClass.
		if (ClassName.StartsWith(TEXT("/")))
		{
			FString PackagePath = ClassName;
			int32 DotIdx;
			if (PackagePath.FindChar(TEXT('.'), DotIdx))
			{
				PackagePath.LeftInline(DotIdx);
			}
			if (UObject* Loaded = UEditorAssetLibrary::LoadAsset(PackagePath))
			{
				if (UBlueprint* BP = Cast<UBlueprint>(Loaded))
				{
					if (BP->GeneratedClass)
					{
						return BP->GeneratedClass;
					}
				}
				if (UClass* DirectClass = Cast<UClass>(Loaded))
				{
					return DirectClass;
				}
			}
			return nullptr;
		}

		// Blueprint short name: "BP_PatrolPointManager_C" or "BP_PatrolPointManager", loaded or not.
		FString ShortName = ClassName;
		if (ShortName.EndsWith(TEXT("_C"), ESearchCase::CaseSensitive))
		{
			ShortName.LeftChopInline(2);
		}
		if (ShortName.Contains(TEXT("/")) || ShortName.Contains(TEXT(".")))
		{
			return nullptr;
		}
		return ResolveBlueprintShortName(ShortName);
	}

	// Blueprint functions and variables are registered against the transient SKELETON class
	// (SKEL_<BP>_C) during editing; that name is not resolvable by create_node_by_key. Map a
	// skeleton/generated class back to the persistent generated-class name so discover_nodes emits
	// keys that round-trip. Native classes (no ClassGeneratedBy) pass through unchanged.
	static FString GetResolvableClassName(const UClass* Class)
	{
		if (!Class)
		{
			return FString();
		}
		if (const UBlueprint* BP = Cast<UBlueprint>(Class->ClassGeneratedBy))
		{
			if (BP->GeneratedClass)
			{
				return BP->GeneratedClass->GetName();
			}
		}
		return Class->GetName();
	}

	// Reduce a skeleton/generated Blueprint class to its authoritative (persistent generated) class
	// so a class-hierarchy test is not defeated by the fact that variable spawners are registered
	// against the SKEL_ class. Native classes pass through unchanged.
	static UClass* GetAuthoritativeClass(UClass* Class)
	{
		if (Class)
		{
			if (const UBlueprint* BP = Cast<UBlueprint>(Class->ClassGeneratedBy))
			{
				if (BP->GeneratedClass)
				{
					return BP->GeneratedClass;
				}
			}
		}
		return Class;
	}

	// Single implementation of the compiler-message harvest so BuildGraph and CompileBlueprint agree
	// on what counts as an error vs a warning. Prefix is prepended to each message (BuildGraph tags
	// them "Compile: "; CompileBlueprint passes an empty prefix).
	static void HarvestCompilerMessages(const FCompilerResultsLog& CompileResults, const FString& Prefix,
		TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
	{
		for (const TSharedRef<FTokenizedMessage>& Msg : CompileResults.Messages)
		{
			const FString MsgText = Msg->ToText().ToString();
			if (Msg->GetSeverity() == EMessageSeverity::Error)
			{
				OutErrors.Add(Prefix + MsgText);
			}
			else if (Msg->GetSeverity() == EMessageSeverity::Warning || Msg->GetSeverity() == EMessageSeverity::PerformanceWarning)
			{
				OutWarnings.Add(Prefix + MsgText);
			}
		}
	}

	// Object/class default of a pin as an object path, empty when the pin holds none. Hard
	// object/class pins store the resolved object in DefaultObject; soft object/class pins store
	// the path string in DefaultValue. Lets get_node_pins report class/object defaults that the
	// string-only DefaultValue field does not capture for hard reference pins.
	static FString GetPinDefaultObjectPath(const UEdGraphPin* Pin)
	{
		if (!Pin)
		{
			return FString();
		}
		if (Pin->DefaultObject)
		{
			return Pin->DefaultObject->GetPathName();
		}
		const FName Cat = Pin->PinType.PinCategory;
		if ((Cat == UEdGraphSchema_K2::PC_SoftObject || Cat == UEdGraphSchema_K2::PC_SoftClass) && !Pin->DefaultValue.IsEmpty())
		{
			return Pin->DefaultValue;
		}
		return FString();
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

	// Result of trying to write a class/object default onto a pin.
	//   NotApplicable — the pin is not a class/object reference pin; caller should fall back to
	//                   Schema->TrySetDefaultValue (plain string default).
	//   Applied        — the class/object was resolved and landed on the pin.
	//   Failed         — the pin IS a class/object pin but the value could not be resolved, or the
	//                    pin's metaclass/class rejected it (OutError describes which).
	enum class EApplyPinDefaultResult : uint8 { NotApplicable, Applied, Failed };

	// Shared class/object pin-default path used by BOTH SetNodePinValue and BuildGraph's pin-default
	// loop (issue #552 follow-up). A hard class/object pin stores its default in Pin->DefaultObject,
	// which Schema->TrySetDefaultValue (a string path) cannot write — so a build_graph pin_default
	// like {"ActorClass":"/Script/Engine.StaticMeshActor"} used to silently no-op. Resolve the
	// class/object, write the field that matches (soft pins keep the path string in DefaultValue),
	// then read it back so a metaclass rejection is a hard failure, not a false success.
	static EApplyPinDefaultResult ApplyPinDefault(UEdGraphPin* Pin, const FString& Value, FString& OutError)
	{
		if (!Pin)
		{
			OutError = TEXT("null pin");
			return EApplyPinDefaultResult::Failed;
		}

		const FName PinCategory = Pin->PinType.PinCategory;
		const bool bClass  = (PinCategory == UEdGraphSchema_K2::PC_Class  || PinCategory == UEdGraphSchema_K2::PC_SoftClass);
		const bool bObject = (PinCategory == UEdGraphSchema_K2::PC_Object || PinCategory == UEdGraphSchema_K2::PC_SoftObject);
		if (!bClass && !bObject)
		{
			return EApplyPinDefaultResult::NotApplicable;
		}

		const UEdGraphSchema_K2* K2Schema = nullptr;
		if (const UEdGraphNode* OwningNode = Pin->GetOwningNodeUnchecked())
		{
			if (const UEdGraph* OwningGraph = OwningNode->GetGraph())
			{
				K2Schema = Cast<UEdGraphSchema_K2>(OwningGraph->GetSchema());
			}
		}

		if (bClass)
		{
			// Resolve the full path first, then native names (with U/A prefix fallbacks) and Blueprint
			// short names of any Blueprint type ("BP_Enemy_C", "WBP_Menu") via ResolveClassByName.
			UClass* ResolvedClass = LoadObject<UClass>(nullptr, *Value);
			if (!ResolvedClass)
				ResolvedClass = ResolveClassByName(Value);

			if (!ResolvedClass)
			{
				OutError = FString::Printf(TEXT("could not resolve class '%s' for class reference pin '%s'"), *Value, *Pin->PinName.ToString());
				return EApplyPinDefaultResult::Failed;
			}

			const bool bSoft = (PinCategory == UEdGraphSchema_K2::PC_SoftClass);
			if (K2Schema)
				K2Schema->TrySetDefaultObject(*Pin, ResolvedClass);
			else if (bSoft)
				Pin->DefaultValue = ResolvedClass->GetPathName();
			else
				Pin->DefaultObject = ResolvedClass;

			const bool bLanded = bSoft
				? Pin->DefaultValue.Equals(ResolvedClass->GetPathName())
				: (Pin->DefaultObject == ResolvedClass);
			if (!bLanded)
			{
				OutError = FString::Printf(TEXT("class '%s' was not accepted on class pin '%s' (the pin's expected metaclass likely rejected it)"), *Value, *Pin->PinName.ToString());
				return EApplyPinDefaultResult::Failed;
			}
			return EApplyPinDefaultResult::Applied;
		}

		// Object reference pin.
		UObject* ResolvedObject = LoadObject<UObject>(nullptr, *Value);
		if (!ResolvedObject)
		{
			OutError = FString::Printf(TEXT("could not load object '%s' for object reference pin '%s'"), *Value, *Pin->PinName.ToString());
			return EApplyPinDefaultResult::Failed;
		}

		const bool bSoft = (PinCategory == UEdGraphSchema_K2::PC_SoftObject);
		if (K2Schema)
			K2Schema->TrySetDefaultObject(*Pin, ResolvedObject);
		else if (bSoft)
			Pin->DefaultValue = ResolvedObject->GetPathName();
		else
			Pin->DefaultObject = ResolvedObject;

		const bool bLanded = bSoft
			? Pin->DefaultValue.Equals(ResolvedObject->GetPathName())
			: (Pin->DefaultObject == ResolvedObject);
		if (!bLanded)
		{
			OutError = FString::Printf(TEXT("object '%s' was not accepted on object pin '%s' (the pin's expected class likely rejected it)"), *Value, *Pin->PinName.ToString());
			return EApplyPinDefaultResult::Failed;
		}
		return EApplyPinDefaultResult::Applied;
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

	// Item 16: map a "Get X"/"Set X" spawner menu name (or a bare name) to an SCS or inherited
	// component variable that belongs to the TARGET blueprint's own class hierarchy. The Blueprint
	// action database does not reliably register a spawner for SCS component variables (so
	// "SPAWN K2Node_VariableGet|Get <Component>" returned an empty id), and any same-named spawner it
	// does surface may belong to an unrelated blueprint. Returns the actual member FName, or NAME_None
	// when no component of the target matches. Matching ignores case and non-alphanumerics so the
	// display name ("Static Mesh") lines up with the variable name ("StaticMesh").
	static FName ResolveSelfComponentVariable(UBlueprint* Blueprint, const FString& MenuOrVarName)
	{
		if (!Blueprint)
		{
			return NAME_None;
		}

		FString Requested = MenuOrVarName;
		Requested.TrimStartAndEndInline();
		if (Requested.StartsWith(TEXT("Get "), ESearchCase::IgnoreCase))
			Requested = Requested.RightChop(4);
		else if (Requested.StartsWith(TEXT("Set "), ESearchCase::IgnoreCase))
			Requested = Requested.RightChop(4);

		const FString NormRequested = NormalizeBlueprintNodeSearchText(Requested);
		if (NormRequested.IsEmpty())
		{
			return NAME_None;
		}

		// 1. SCS nodes declared on this blueprint (covers components added before a compile has
		// reflected them onto the generated class).
		if (USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript)
		{
			for (USCS_Node* Node : SCS->GetAllNodes())
			{
				if (!Node)
				{
					continue;
				}
				const FName VarName = Node->GetVariableName();
				if (!VarName.IsNone() && NormalizeBlueprintNodeSearchText(VarName.ToString()) == NormRequested)
				{
					return VarName;
				}
			}
		}

		// 2. Component object properties on the generated class (covers inherited native components).
		if (UClass* GenClass = Blueprint->GeneratedClass)
		{
			for (TFieldIterator<FProperty> It(GenClass); It; ++It)
			{
				if (const FObjectProperty* ObjProp = CastField<FObjectProperty>(*It))
				{
					if (ObjProp->PropertyClass && ObjProp->PropertyClass->IsChildOf(UActorComponent::StaticClass()) &&
						NormalizeBlueprintNodeSearchText(ObjProp->GetName()) == NormRequested)
					{
						return ObjProp->GetFName();
					}
				}
			}
		}

		return NAME_None;
	}

	// Does this property answer to the requested (already normalised) name? A menu name is the
	// property's DISPLAY string, not its raw name — and for a bool the display string drops the
	// Hungarian "b" prefix, so AActor::bCanBeDamaged surfaces in the menu as "Can Be Damaged".
	// Matching the raw name alone therefore misses every boolean, which is exactly what made the
	// qualified key "…|Get Can Be Damaged|Actor" fail. Try both spellings.
	static bool PropertyMatchesSearchName(const FProperty* Property, const FString& NormRequested)
	{
		if (!Property)
		{
			return false;
		}
		if (NormalizeBlueprintNodeSearchText(Property->GetName()) == NormRequested)
		{
			return true;
		}
		const FString DisplayName = FName::NameToDisplayString(Property->GetName(), Property->IsA<FBoolProperty>());
		return NormalizeBlueprintNodeSearchText(DisplayName) == NormRequested;
	}

	// Issue #609: the same action-database staleness that hid SCS component variables (see
	// ResolveSelfComponentVariable above) also hides a member variable that was just added with
	// add_member_variable — the DB has no spawner for it yet, so "SPAWN K2Node_VariableGet|Get My Var"
	// returned an empty id and the natural "add a variable, then wire it up" flow failed on step two.
	// This resolves any variable the TARGET Blueprint genuinely owns (its own NewVariables, or an
	// inherited property on its class hierarchy) so it can be bound directly as a self member, using
	// the same normalised-name matching as the component resolver ("My Var" <-> "MyVar").
	// It deliberately does NOT widen what is reachable: a property owned by an unrelated class is not
	// on this Blueprint's hierarchy and still falls through to the ownership check.
	static FName ResolveSelfMemberVariable(UBlueprint* Blueprint, const FString& MenuOrVarName)
	{
		if (!Blueprint)
		{
			return NAME_None;
		}

		FString Requested = MenuOrVarName;
		Requested.TrimStartAndEndInline();
		if (Requested.StartsWith(TEXT("Get "), ESearchCase::IgnoreCase))
			Requested = Requested.RightChop(4);
		else if (Requested.StartsWith(TEXT("Set "), ESearchCase::IgnoreCase))
			Requested = Requested.RightChop(4);

		const FString NormRequested = NormalizeBlueprintNodeSearchText(Requested);
		if (NormRequested.IsEmpty())
		{
			return NAME_None;
		}

		// 1. Variables declared on this Blueprint, including ones added moments ago that no compile
		//    has reflected onto the generated class yet.
		for (const FBPVariableDescription& Var : Blueprint->NewVariables)
		{
			if (Var.VarName.IsNone())
			{
				continue;
			}
			const FString RawName = Var.VarName.ToString();
			// Same display-name rule as PropertyMatchesSearchName: a bool declared "bIsOpen" appears
			// in the menu as "Is Open".
			const bool bIsBool = (Var.VarType.PinCategory == UEdGraphSchema_K2::PC_Boolean);
			if (NormalizeBlueprintNodeSearchText(RawName) == NormRequested
				|| NormalizeBlueprintNodeSearchText(FName::NameToDisplayString(RawName, bIsBool)) == NormRequested)
			{
				return Var.VarName;
			}
		}

		// 2. Any property on the generated class (covers inherited native/Blueprint variables).
		if (UClass* GenClass = Blueprint->GeneratedClass)
		{
			for (TFieldIterator<FProperty> It(GenClass); It; ++It)
			{
				if (PropertyMatchesSearchName(*It, NormRequested))
				{
					return It->GetFName();
				}
			}
		}

		return NAME_None;
	}

	// Issue #610: bind a variable get/set to an EXPLICITLY named owning class, for the qualified-key
	// form "SPAWN K2Node_VariableGet|Get Jump Max Count|Character". Returns the resolved property's
	// name and fills OutOwnerClass, or NAME_None when the class or the property cannot be resolved.
	static FName ResolveExplicitOwnerVariable(const FString& OwnerClassName, const FString& MenuOrVarName, UClass*& OutOwnerClass)
	{
		OutOwnerClass = ResolveClassByName(OwnerClassName);
		if (!OutOwnerClass)
		{
			return NAME_None;
		}

		FString Requested = MenuOrVarName;
		Requested.TrimStartAndEndInline();
		if (Requested.StartsWith(TEXT("Get "), ESearchCase::IgnoreCase))
			Requested = Requested.RightChop(4);
		else if (Requested.StartsWith(TEXT("Set "), ESearchCase::IgnoreCase))
			Requested = Requested.RightChop(4);

		const FString NormRequested = NormalizeBlueprintNodeSearchText(Requested);
		if (NormRequested.IsEmpty())
		{
			return NAME_None;
		}

		for (TFieldIterator<FProperty> It(OutOwnerClass); It; ++It)
		{
			if (PropertyMatchesSearchName(*It, NormRequested))
			{
				return It->GetFName();
			}
		}

		return NAME_None;
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

	// BlueprintService edits are unsafe under Play-In-Editor, so refuse every load while PIE runs:
	// log a fixed PIE_ACTIVE: prefix naming the Blueprint path and return nullptr (LoadBlueprint is
	// the shared choke point every BlueprintService entry point funnels through). The early return
	// matters for subobject (":") paths: UEditorAssetLibrary::LoadAsset already refuses during PIE,
	// but StaticLoadObject below does not, so without it a Level Blueprint path would still resolve
	// and be edited while the log claimed the call was refused.
	if (GEditor && GEditor->PlayWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("PIE_ACTIVE: LoadBlueprint refused '%s' — a Play-In-Editor session is running; stop PIE before using BlueprintService."), *BlueprintPath);
		return nullptr;
	}

	// Subobject paths (":" present) don't load through the asset library — resolve them
	// directly. Covers explicit LevelScriptBlueprint paths like
	// /Game/Maps/MyMap.MyMap:PersistentLevel.MyMap.
	if (BlueprintPath.Contains(TEXT(":")))
	{
		return Cast<UBlueprint>(StaticLoadObject(UObject::StaticClass(), nullptr, *BlueprintPath));
	}

	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(BlueprintPath);
	if (UBlueprint* Blueprint = Cast<UBlueprint>(LoadedObject))
	{
		return Blueprint;
	}

	// A map/world path resolves to its persistent level's Level Blueprint, so every
	// BlueprintService function works on level scripts by passing the map path.
	if (const UWorld* World = Cast<UWorld>(LoadedObject))
	{
		if (World->PersistentLevel)
		{
			return World->PersistentLevel->GetLevelScriptBlueprint(/*bDontCreate*/ false);
		}
	}

	return nullptr;
}






// The actual scene root is the first root-level scene component node that is not attached
// to an inherited/native parent. GetDefaultSceneRootNode() only returns the auto-generated
// DefaultSceneRoot, which is detached from the tree once a user scene component becomes root.
static USCS_Node* FindActualSceneRootNode(USimpleConstructionScript* SCS)
{
	for (USCS_Node* Node : SCS->GetRootNodes())
	{
		if (Node && Node->ComponentClass && Node->ComponentClass->IsChildOf<USceneComponent>()
			&& Node->ParentComponentOrVariableName == NAME_None)
		{
			return Node;
		}
	}
	return nullptr;
}





// ============================================================================
// COMPONENT MANAGEMENT (manage_blueprint_component actions)
// ============================================================================



/**
 * Attach Child under Parent within the SAME SimpleConstructionScript.
 * ParentComponentOrVariableName (+ ParentComponentOwnerClassName / bIsParentComponentNative)
 * is reserved for attachment to INHERITED (parent-class or native) components. When it names
 * the node's own-SCS parent, the Blueprint compiles and behaves normally, but the engine's
 * hierarchy fixup (FSceneHierarchyMapper::FixupParentage) fires the "possible cyclic linkage"
 * ensure on package load — which BuildCookRun counts as an error after every package has
 * already cooked (issues #371, #523). Same-SCS attachment is expressed by the parent's
 * ChildNodes array alone; USCS_Node::SetParent must only be used for genuinely inherited
 * parents. Clearing the fields here also heals nodes corrupted by earlier writes.
 */
static void AttachSameScsChild(USCS_Node* Parent, USCS_Node* Child)
{
	Parent->AddChildNode(Child);
	Child->Modify();
	Child->ParentComponentOrVariableName = NAME_None;
	Child->ParentComponentOwnerClassName = NAME_None;
	Child->bIsParentComponentNative = false;
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








// ============================================================================
// VARIABLE MANAGEMENT (Phase 1)
// ============================================================================







// ============================================================================
// EVENT DISPATCHER MANAGEMENT
// ============================================================================

namespace
{
	// Locate a delegate signature graph by name on a blueprint.
	static UEdGraph* FindDelegateSignatureGraph(UBlueprint* Blueprint, const FString& DispatcherName)
	{
		if (!Blueprint)
		{
			return nullptr;
		}
		for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs)
		{
			if (Graph && Graph->GetName().Equals(DispatcherName, ESearchCase::IgnoreCase))
			{
				return Graph;
			}
		}
		return nullptr;
	}

	// Resolve a multicast delegate property from the blueprint's skeleton or generated class.
	static FMulticastDelegateProperty* FindDispatcherProperty(UBlueprint* Blueprint, const FString& DispatcherName)
	{
		if (!Blueprint)
		{
			return nullptr;
		}
		const FName DispatcherFName(*DispatcherName);
		auto FindOn = [&DispatcherName, &DispatcherFName](UClass* Class) -> FMulticastDelegateProperty*
		{
			if (!Class)
			{
				return nullptr;
			}
			for (TFieldIterator<FMulticastDelegateProperty> It(Class); It; ++It)
			{
				if (It->GetFName() == DispatcherFName || It->GetName().Equals(DispatcherName, ESearchCase::IgnoreCase))
				{
					return *It;
				}
			}
			return nullptr;
		};
		if (FMulticastDelegateProperty* P = FindOn(Blueprint->SkeletonGeneratedClass)) { return P; }
		return FindOn(Blueprint->GeneratedClass);
	}
}





// ============================================================================
// FUNCTION MANAGEMENT (Phase 2)
// ============================================================================

bool UBlueprintService::CreateMacroGraph(const FString& BlueprintPath, const FString& MacroName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateMacroGraph: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Idempotent — return true if the macro graph already exists
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph && Graph->GetName() == MacroName)
		{
			UE_LOG(LogTemp, Warning, TEXT("CreateMacroGraph: Macro '%s' already exists in %s"), *MacroName, *BlueprintPath);
			return true;
		}
	}

	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*MacroName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

	if (!NewGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateMacroGraph: Failed to create graph '%s' in %s"), *MacroName, *BlueprintPath);
		return false;
	}

	NewGraph->bEditable = true;
	FBlueprintEditorUtils::AddMacroGraph(Blueprint, NewGraph, /*bIsUserCreated=*/true, /*SignatureFromClass=*/nullptr);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("CreateMacroGraph: Created macro '%s' in %s"), *MacroName, *BlueprintPath);
	return true;
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

	// Disambiguator 1: a full graph object path (Graph->GetPathName()) is unique even when
	// several graphs share a short name — list_graphs reports it in FBlueprintGraphInfo.GraphPath.
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetPathName() == GraphName)
		{
			return Graph;
		}
	}

	// Exact short-name match — the common case. Count duplicates so an ambiguous name warns
	// rather than silently aliasing to whichever graph happens to come first.
	UEdGraph* FirstMatch = nullptr;
	int32 MatchCount = 0;
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			if (!FirstMatch)
			{
				FirstMatch = Graph;
			}
			++MatchCount;
		}
	}

	if (FirstMatch)
	{
		if (MatchCount > 1)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("FindGraph: %d graphs are named '%s' in blueprint '%s'; returning the first. ")
				TEXT("Disambiguate by passing the graph object path (see list_graphs' graph_path) or '%s#<index>' (0-based, in GetAllGraphs order)."),
				MatchCount, *GraphName, *Blueprint->GetName(), *GraphName);
		}
		return FirstMatch;
	}

	// Disambiguator 2: "Name#<index>" — 0-based index among same-named graphs in GetAllGraphs
	// order. Only consulted when no graph literally matches GraphName, so a graph genuinely
	// named "Foo#2" is still found by the exact match above.
	int32 HashIdx = INDEX_NONE;
	if (GraphName.FindLastChar(TEXT('#'), HashIdx) && HashIdx > 0)
	{
		const FString BaseName = GraphName.Left(HashIdx);
		const FString IndexStr = GraphName.Mid(HashIdx + 1);
		if (!IndexStr.IsEmpty() && IndexStr.IsNumeric())
		{
			const int32 WantIndex = FCString::Atoi(*IndexStr);
			int32 Seen = 0;
			for (UEdGraph* Graph : Graphs)
			{
				if (Graph && Graph->GetName() == BaseName)
				{
					if (Seen == WantIndex)
					{
						return Graph;
					}
					++Seen;
				}
			}
			UE_LOG(LogTemp, Warning,
				TEXT("FindGraph: index %d is out of range for graph name '%s' (%d match(es)) in blueprint '%s'"),
				WantIndex, *BaseName, Seen, *Blueprint->GetName());
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



// Returns true if the editable-pin node already has a user-defined pin with this name.
// (Avoids UK2Node_EditablePinBase::UserDefinedPinExists, which isn't exported from BlueprintGraph.)
static bool VibeUE_HasUserDefinedPin(const UK2Node_EditablePinBase* Node, const FName PinName)
{
	if (!Node)
	{
		return false;
	}
	for (const TSharedPtr<FUserPinInfo>& Info : Node->UserDefinedPins)
	{
		if (Info.IsValid() && Info->PinName == PinName)
		{
			return true;
		}
	}
	return false;
}

UK2Node_CustomEvent* UBlueprintService::ResolveCustomEventNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId,
	UBlueprint*& OutBlueprint,
	FString& OutError)
{
	OutBlueprint = LoadBlueprint(BlueprintPath);
	if (!OutBlueprint)
	{
		OutError = FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath);
		return nullptr;
	}

	UEdGraph* Graph = ResolveBlueprintGraph(OutBlueprint, GraphName);
	if (!Graph)
	{
		OutError = FString::Printf(TEXT("Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return nullptr;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		OutError = FString::Printf(TEXT("Node '%s' not found in graph '%s'"), *NodeId, *GraphName);
		return nullptr;
	}

	UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node);
	if (!CustomEvent)
	{
		OutError = FString::Printf(TEXT("Node '%s' is a %s, not a Custom Event"), *NodeId, *Node->GetClass()->GetName());
		return nullptr;
	}

	return CustomEvent;
}





// ── Timelines ──

// Resolve a timeline template on a blueprint by variable name.
static UTimelineTemplate* VibeUE_ResolveTimeline(UBlueprint* Blueprint, const FString& TimelineName, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return nullptr;
	}
	UTimelineTemplate* Template = Blueprint->FindTimelineTemplateByVariableName(FName(*TimelineName));
	if (!Template)
	{
		OutError = FString::Printf(TEXT("Timeline '%s' not found on %s"), *TimelineName, *Blueprint->GetName());
	}
	return Template;
}

// Find a float track on a timeline template by name.
static FTTFloatTrack* VibeUE_FindFloatTrack(UTimelineTemplate* Template, const FString& TrackName)
{
	if (!Template)
	{
		return nullptr;
	}
	const FName Wanted(*TrackName);
	return Template->FloatTracks.FindByPredicate([Wanted](const FTTFloatTrack& Track) { return Track.GetTrackName() == Wanted; });
}

// Find a track of any type on the timeline by name. Returns the base pointer and fills the type + index.
static FTTTrackBase* VibeUE_FindAnyTrack(UTimelineTemplate* Template, const FName Name, FTTTrackBase::ETrackType& OutType, int32& OutIndex)
{
	if (!Template)
	{
		return nullptr;
	}
	for (int32 i = 0; i < Template->FloatTracks.Num(); ++i)
	{
		if (Template->FloatTracks[i].GetTrackName() == Name) { OutType = FTTTrackBase::TT_FloatInterp; OutIndex = i; return &Template->FloatTracks[i]; }
	}
	for (int32 i = 0; i < Template->VectorTracks.Num(); ++i)
	{
		if (Template->VectorTracks[i].GetTrackName() == Name) { OutType = FTTTrackBase::TT_VectorInterp; OutIndex = i; return &Template->VectorTracks[i]; }
	}
	for (int32 i = 0; i < Template->LinearColorTracks.Num(); ++i)
	{
		if (Template->LinearColorTracks[i].GetTrackName() == Name) { OutType = FTTTrackBase::TT_LinearColorInterp; OutIndex = i; return &Template->LinearColorTracks[i]; }
	}
	for (int32 i = 0; i < Template->EventTracks.Num(); ++i)
	{
		if (Template->EventTracks[i].GetTrackName() == Name) { OutType = FTTTrackBase::TT_Event; OutIndex = i; return &Template->EventTracks[i]; }
	}
	return nullptr;
}

// Collect the FRichCurve(s) backing a track (1 for float/event, 3 for vector, 4 for linear color).
static void VibeUE_TrackCurves(FTTTrackBase* Track, FTTTrackBase::ETrackType Type, TArray<FRichCurve*>& OutCurves)
{
	if (!Track)
	{
		return;
	}
	switch (Type)
	{
	case FTTTrackBase::TT_FloatInterp:
		if (UCurveFloat* C = static_cast<FTTFloatTrack*>(Track)->CurveFloat) { OutCurves.Add(&C->FloatCurve); }
		break;
	case FTTTrackBase::TT_VectorInterp:
		if (UCurveVector* C = static_cast<FTTVectorTrack*>(Track)->CurveVector) { for (int32 i = 0; i < 3; ++i) { OutCurves.Add(&C->FloatCurves[i]); } }
		break;
	case FTTTrackBase::TT_LinearColorInterp:
		if (UCurveLinearColor* C = static_cast<FTTLinearColorTrack*>(Track)->CurveLinearColor) { for (int32 i = 0; i < 4; ++i) { OutCurves.Add(&C->FloatCurves[i]); } }
		break;
	case FTTTrackBase::TT_Event:
		if (UCurveFloat* C = static_cast<FTTEventTrack*>(Track)->CurveKeys) { OutCurves.Add(&C->FloatCurve); }
		break;
	}
}

// Return the UObject curve(s) of a track (so callers can Modify() them).
static void VibeUE_TrackCurveObjects(FTTTrackBase* Track, FTTTrackBase::ETrackType Type, TArray<UCurveBase*>& OutCurves)
{
	if (!Track)
	{
		return;
	}
	switch (Type)
	{
	case FTTTrackBase::TT_FloatInterp:
		if (UCurveFloat* C = static_cast<FTTFloatTrack*>(Track)->CurveFloat) { OutCurves.Add(C); }
		break;
	case FTTTrackBase::TT_VectorInterp:
		if (UCurveVector* C = static_cast<FTTVectorTrack*>(Track)->CurveVector) { OutCurves.Add(C); }
		break;
	case FTTTrackBase::TT_LinearColorInterp:
		if (UCurveLinearColor* C = static_cast<FTTLinearColorTrack*>(Track)->CurveLinearColor) { OutCurves.Add(C); }
		break;
	case FTTTrackBase::TT_Event:
		if (UCurveFloat* C = static_cast<FTTEventTrack*>(Track)->CurveKeys) { OutCurves.Add(C); }
		break;
	}
}

static void VibeUE_ParseInterp(const FString& InMode, ERichCurveInterpMode& OutInterp, ERichCurveTangentMode& OutTangent)
{
	OutInterp = RCIM_Cubic;
	OutTangent = RCTM_Auto;
	const FString Mode = InMode.TrimStartAndEnd();
	if (Mode.Equals(TEXT("Linear"), ESearchCase::IgnoreCase)) { OutInterp = RCIM_Linear; }
	else if (Mode.Equals(TEXT("Constant"), ESearchCase::IgnoreCase)) { OutInterp = RCIM_Constant; }
	else if (Mode.Equals(TEXT("CubicUser"), ESearchCase::IgnoreCase)) { OutInterp = RCIM_Cubic; OutTangent = RCTM_User; }
	// "Auto" / "CubicAuto" / anything else → cubic + auto tangents (smooth)
}

static void VibeUE_AddCurveKey(FRichCurve& Curve, float Time, float Value, ERichCurveInterpMode Interp, ERichCurveTangentMode Tangent)
{
	const FKeyHandle KeyHandle = Curve.AddKey(Time, Value, /*bUnwindRotation*/false, FKeyHandle());
	Curve.SetKeyInterpMode(KeyHandle, Interp);
	Curve.SetKeyTangentMode(KeyHandle, Tangent);
	Curve.AutoSetTangents();
}

// Find the display index of a (type,index) track. Returns INDEX_NONE if not present.
static int32 VibeUE_FindDisplayIndex(UTimelineTemplate* Template, FTTTrackBase::ETrackType Type, int32 TrackIndex)
{
	const int32 Num = Template->GetNumDisplayTracks();
	for (int32 i = 0; i < Num; ++i)
	{
		const FTTTrackId Id = Template->GetDisplayTrackId(i);
		if (Id.TrackType == static_cast<int32>(Type) && Id.TrackIndex == TrackIndex)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

FString UBlueprintService::AddTimeline(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& TimelineName,
	float Length,
	bool bUseLastKeyFrame,
	bool bAutoPlay,
	bool bLoop,
	float PosX,
	float PosY,
	bool bReplaceExisting)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTimeline: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	if (!FBlueprintEditorUtils::DoesSupportTimelines(Blueprint))
	{
		UE_LOG(LogTemp, Error, TEXT("AddTimeline: Blueprint %s does not support timelines"), *BlueprintPath);
		return FString();
	}

	UEdGraph* Graph = ResolveBlueprintGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTimeline: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	const FName DesiredName = TimelineName.IsEmpty() ? FBlueprintEditorUtils::FindUniqueTimelineName(Blueprint) : FName(*TimelineName);
	if (Blueprint->FindTimelineTemplateByVariableName(DesiredName))
	{
		// A leftover UTimelineTemplate (e.g. its Timeline node was deleted but the template stayed)
		// blocks re-adding the same name. Previously this returned an empty string and logged only
		// to UE_LOG, invisible from Python. Now: replace on request, otherwise return a visible
		// "ERROR:" sentinel that says how to proceed.
		if (bReplaceExisting)
		{
			if (!RemoveTimeline(BlueprintPath, DesiredName.ToString()))
			{
				const FString Sentinel = FString::Printf(TEXT("ERROR: Timeline '%s' already exists on %s and could not be removed for replacement"), *DesiredName.ToString(), *BlueprintPath);
				UE_LOG(LogTemp, Error, TEXT("AddTimeline: %s"), *Sentinel);
				return Sentinel;
			}
			// Re-resolve the graph: RemoveTimeline recompiles, which can invalidate the pointer.
			Graph = ResolveBlueprintGraph(Blueprint, GraphName);
			if (!Graph)
			{
				const FString Sentinel = FString::Printf(TEXT("ERROR: Graph '%s' not found in %s after removing timeline '%s'"), *GraphName, *BlueprintPath, *DesiredName.ToString());
				UE_LOG(LogTemp, Error, TEXT("AddTimeline: %s"), *Sentinel);
				return Sentinel;
			}
		}
		else
		{
			const FString Sentinel = FString::Printf(TEXT("ERROR: Timeline '%s' already exists on %s (a deleted Timeline node can leave its template behind). Call remove_timeline first, or pass replace_existing=true."), *DesiredName.ToString(), *BlueprintPath);
			UE_LOG(LogTemp, Error, TEXT("AddTimeline: %s"), *Sentinel);
			return Sentinel;
		}
	}

	// Create the Timeline node.
	UK2Node_Timeline* TimelineNode = NewObject<UK2Node_Timeline>(Graph);
	Graph->AddNode(TimelineNode, false, false);
	TimelineNode->CreateNewGuid();
	TimelineNode->PostPlacedNewNode();
	TimelineNode->NodePosX = PosX;
	TimelineNode->NodePosY = PosY;
	TimelineNode->TimelineName = DesiredName;
	TimelineNode->bAutoPlay = bAutoPlay;
	TimelineNode->bLoop = bLoop;

	// Create the backing template (links to the node by name).
	UTimelineTemplate* Template = FBlueprintEditorUtils::AddNewTimeline(Blueprint, DesiredName);
	if (!Template)
	{
		FBlueprintEditorUtils::RemoveNode(Blueprint, TimelineNode, /*bDontRecompile*/true);
		UE_LOG(LogTemp, Error, TEXT("AddTimeline: AddNewTimeline failed for '%s'"), *DesiredName.ToString());
		return FString();
	}
	Template->bAutoPlay = bAutoPlay;
	Template->bLoop = bLoop;
	if (bUseLastKeyFrame)
	{
		Template->LengthMode = ETimelineLengthMode::TL_LastKeyFrame;
	}
	else
	{
		Template->LengthMode = ETimelineLengthMode::TL_TimelineLength;
		Template->TimelineLength = Length;
	}

	TimelineNode->AllocateDefaultPins();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddTimeline: Added timeline '%s' in %s"), *DesiredName.ToString(), *GraphName);
	return TimelineNode->NodeGuid.ToString();
}

bool UBlueprintService::AddTimelineFloatTrack(
	const FString& BlueprintPath,
	const FString& TimelineName,
	const FString& TrackName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTimelineFloatTrack: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	FString Error;
	UTimelineTemplate* Template = VibeUE_ResolveTimeline(Blueprint, TimelineName, Error);
	if (!Template)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTimelineFloatTrack: %s"), *Error);
		return false;
	}

	if (TrackName.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("AddTimelineFloatTrack: TrackName is empty"));
		return false;
	}
	const FName TrackFName(*TrackName);
	if (!Template->IsNewTrackNameValid(TrackFName))
	{
		UE_LOG(LogTemp, Error, TEXT("AddTimelineFloatTrack: Track name '%s' is not valid/unique on timeline '%s'"), *TrackName, *TimelineName);
		return false;
	}

	UK2Node_Timeline* TimelineNode = FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, Template);
	if (!TimelineNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTimelineFloatTrack: No Timeline node found for '%s'"), *TimelineName);
		return false;
	}

	UClass* OwnerClass = Blueprint->GeneratedClass;
	if (!OwnerClass)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTimelineFloatTrack: Blueprint has no GeneratedClass"));
		return false;
	}

	TimelineNode->Modify();
	Template->Modify();

	FTTFloatTrack NewTrack;
	NewTrack.SetTrackName(TrackFName, Template);
	NewTrack.CurveFloat = NewObject<UCurveFloat>(OwnerClass, NAME_None, RF_Public | RF_Transactional);
	const int32 NewIndex = Template->FloatTracks.Add(NewTrack);

	FTTTrackId NewTrackId;
	NewTrackId.TrackType = FTTTrackBase::TT_FloatInterp;
	NewTrackId.TrackIndex = NewIndex;
	Template->AddDisplayTrack(NewTrackId);

	TimelineNode->ReconstructNode();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddTimelineFloatTrack: Added float track '%s' to timeline '%s'"), *TrackName, *TimelineName);
	return true;
}

bool UBlueprintService::AddTimelineFloatKey(
	const FString& BlueprintPath,
	const FString& TimelineName,
	const FString& TrackName,
	float Time,
	float Value,
	const FString& InterpMode)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTimelineFloatKey: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	FString Error;
	UTimelineTemplate* Template = VibeUE_ResolveTimeline(Blueprint, TimelineName, Error);
	if (!Template)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTimelineFloatKey: %s"), *Error);
		return false;
	}

	FTTFloatTrack* Track = VibeUE_FindFloatTrack(Template, TrackName);
	if (!Track || !Track->CurveFloat)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTimelineFloatKey: Float track '%s' not found (or has no curve) on timeline '%s'"), *TrackName, *TimelineName);
		return false;
	}

	ERichCurveInterpMode CurveInterp = RCIM_Cubic;
	ERichCurveTangentMode CurveTangent = RCTM_Auto;
	const FString Mode = InterpMode.TrimStartAndEnd();
	if (Mode.Equals(TEXT("Linear"), ESearchCase::IgnoreCase))
	{
		CurveInterp = RCIM_Linear;
	}
	else if (Mode.Equals(TEXT("Constant"), ESearchCase::IgnoreCase))
	{
		CurveInterp = RCIM_Constant;
	}
	else if (Mode.Equals(TEXT("CubicUser"), ESearchCase::IgnoreCase))
	{
		CurveInterp = RCIM_Cubic;
		CurveTangent = RCTM_User;
	}
	// "Auto" / "CubicAuto" / anything else → cubic + auto tangents (smooth)

	UCurveFloat* Curve = Track->CurveFloat;
	Curve->Modify();
	FRichCurve& RichCurve = Curve->FloatCurve;
	const FKeyHandle KeyHandle = RichCurve.AddKey(Time, Value, /*bUnwindRotation*/false, FKeyHandle());
	RichCurve.SetKeyInterpMode(KeyHandle, CurveInterp);
	RichCurve.SetKeyTangentMode(KeyHandle, CurveTangent);
	RichCurve.AutoSetTangents();

	if (UK2Node_Timeline* TimelineNode = FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, Template))
	{
		// Length may follow the last keyframe — refresh the node so any length-dependent state updates.
		TimelineNode->ReconstructNode();
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddTimelineFloatKey: Added key (t=%.4f v=%.4f mode=%s) to '%s.%s'"), Time, Value, *Mode, *TimelineName, *TrackName);
	return true;
}

TArray<FBlueprintTimelineInfo> UBlueprintService::GetTimelines(const FString& BlueprintPath)
{
	TArray<FBlueprintTimelineInfo> Result;
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("GetTimelines: Failed to load blueprint: %s"), *BlueprintPath);
		return Result;
	}

	for (const UTimelineTemplate* Template : Blueprint->Timelines)
	{
		if (!Template)
		{
			continue;
		}
		FBlueprintTimelineInfo Info;
		Info.TimelineName = Template->GetVariableName().ToString();
		Info.TrackCount = Template->FloatTracks.Num() + Template->VectorTracks.Num()
			+ Template->LinearColorTracks.Num() + Template->EventTracks.Num();
		Info.Length = Template->TimelineLength;
		Info.bLoop = Template->bLoop;
		Info.bAutoPlay = Template->bAutoPlay;
		Result.Add(Info);
	}
	return Result;
}

// Validate a new track add. Returns the template + node and the new-track FName, or nullptr on failure.
static UTimelineTemplate* VibeUE_PrepareTrackAdd(const TCHAR* FnName, UBlueprint* Blueprint, const FString& TimelineName, const FString& TrackName,
	FName& OutTrackFName, UK2Node_Timeline*& OutNode)
{
	OutNode = nullptr;
	FString Error;
	UTimelineTemplate* Template = VibeUE_ResolveTimeline(Blueprint, TimelineName, Error);
	if (!Template) { UE_LOG(LogTemp, Error, TEXT("%s: %s"), FnName, *Error); return nullptr; }
	if (TrackName.IsEmpty()) { UE_LOG(LogTemp, Error, TEXT("%s: TrackName is empty"), FnName); return nullptr; }
	OutTrackFName = FName(*TrackName);
	if (!Template->IsNewTrackNameValid(OutTrackFName))
	{
		UE_LOG(LogTemp, Error, TEXT("%s: Track name '%s' is not valid/unique on timeline '%s'"), FnName, *TrackName, *TimelineName);
		return nullptr;
	}
	OutNode = FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, Template);
	if (!OutNode) { UE_LOG(LogTemp, Error, TEXT("%s: No Timeline node found for '%s'"), FnName, *TimelineName); return nullptr; }
	if (!Blueprint->GeneratedClass) { UE_LOG(LogTemp, Error, TEXT("%s: Blueprint has no GeneratedClass"), FnName); return nullptr; }
	return Template;
}

static void VibeUE_FinishTrackAdd(UBlueprint* Blueprint, UTimelineTemplate* Template, UK2Node_Timeline* Node, FTTTrackBase::ETrackType Type, int32 NewIndex)
{
	FTTTrackId NewTrackId;
	NewTrackId.TrackType = static_cast<int32>(Type);
	NewTrackId.TrackIndex = NewIndex;
	Template->AddDisplayTrack(NewTrackId);
	Node->ReconstructNode();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

bool UBlueprintService::AddTimelineVectorTrack(const FString& BlueprintPath, const FString& TimelineName, const FString& TrackName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint) { UE_LOG(LogTemp, Error, TEXT("AddTimelineVectorTrack: Failed to load blueprint: %s"), *BlueprintPath); return false; }
	FName TrackFName; UK2Node_Timeline* Node = nullptr;
	UTimelineTemplate* Template = VibeUE_PrepareTrackAdd(TEXT("AddTimelineVectorTrack"), Blueprint, TimelineName, TrackName, TrackFName, Node);
	if (!Template) { return false; }
	Node->Modify(); Template->Modify();
	FTTVectorTrack NewTrack;
	NewTrack.SetTrackName(TrackFName, Template);
	NewTrack.CurveVector = NewObject<UCurveVector>(Blueprint->GeneratedClass, NAME_None, RF_Public | RF_Transactional);
	const int32 NewIndex = Template->VectorTracks.Add(NewTrack);
	VibeUE_FinishTrackAdd(Blueprint, Template, Node, FTTTrackBase::TT_VectorInterp, NewIndex);
	UE_LOG(LogTemp, Log, TEXT("AddTimelineVectorTrack: Added vector track '%s' to timeline '%s'"), *TrackName, *TimelineName);
	return true;
}

bool UBlueprintService::AddTimelineColorTrack(const FString& BlueprintPath, const FString& TimelineName, const FString& TrackName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint) { UE_LOG(LogTemp, Error, TEXT("AddTimelineColorTrack: Failed to load blueprint: %s"), *BlueprintPath); return false; }
	FName TrackFName; UK2Node_Timeline* Node = nullptr;
	UTimelineTemplate* Template = VibeUE_PrepareTrackAdd(TEXT("AddTimelineColorTrack"), Blueprint, TimelineName, TrackName, TrackFName, Node);
	if (!Template) { return false; }
	Node->Modify(); Template->Modify();
	FTTLinearColorTrack NewTrack;
	NewTrack.SetTrackName(TrackFName, Template);
	NewTrack.CurveLinearColor = NewObject<UCurveLinearColor>(Blueprint->GeneratedClass, NAME_None, RF_Public | RF_Transactional);
	const int32 NewIndex = Template->LinearColorTracks.Add(NewTrack);
	VibeUE_FinishTrackAdd(Blueprint, Template, Node, FTTTrackBase::TT_LinearColorInterp, NewIndex);
	UE_LOG(LogTemp, Log, TEXT("AddTimelineColorTrack: Added color track '%s' to timeline '%s'"), *TrackName, *TimelineName);
	return true;
}

bool UBlueprintService::AddTimelineEventTrack(const FString& BlueprintPath, const FString& TimelineName, const FString& TrackName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint) { UE_LOG(LogTemp, Error, TEXT("AddTimelineEventTrack: Failed to load blueprint: %s"), *BlueprintPath); return false; }
	FName TrackFName; UK2Node_Timeline* Node = nullptr;
	UTimelineTemplate* Template = VibeUE_PrepareTrackAdd(TEXT("AddTimelineEventTrack"), Blueprint, TimelineName, TrackName, TrackFName, Node);
	if (!Template) { return false; }
	Node->Modify(); Template->Modify();
	FTTEventTrack NewTrack;
	NewTrack.SetTrackName(TrackFName, Template);
	NewTrack.CurveKeys = NewObject<UCurveFloat>(Blueprint->GeneratedClass, NAME_None, RF_Public | RF_Transactional);
	NewTrack.CurveKeys->bIsEventCurve = true;
	const int32 NewIndex = Template->EventTracks.Add(NewTrack);
	VibeUE_FinishTrackAdd(Blueprint, Template, Node, FTTTrackBase::TT_Event, NewIndex);
	UE_LOG(LogTemp, Log, TEXT("AddTimelineEventTrack: Added event track '%s' to timeline '%s'"), *TrackName, *TimelineName);
	return true;
}

bool UBlueprintService::RemoveTimelineTrack(const FString& BlueprintPath, const FString& TimelineName, const FString& TrackName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint) { UE_LOG(LogTemp, Error, TEXT("RemoveTimelineTrack: Failed to load blueprint: %s"), *BlueprintPath); return false; }
	FString Error;
	UTimelineTemplate* Template = VibeUE_ResolveTimeline(Blueprint, TimelineName, Error);
	if (!Template) { UE_LOG(LogTemp, Error, TEXT("RemoveTimelineTrack: %s"), *Error); return false; }

	FTTTrackBase::ETrackType Type; int32 Index;
	if (!VibeUE_FindAnyTrack(Template, FName(*TrackName), Type, Index))
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveTimelineTrack: Track '%s' not found on timeline '%s'"), *TrackName, *TimelineName);
		return false;
	}
	UK2Node_Timeline* TimelineNode = FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, Template);
	if (!TimelineNode) { UE_LOG(LogTemp, Error, TEXT("RemoveTimelineTrack: No Timeline node for '%s'"), *TimelineName); return false; }

	TimelineNode->Modify();
	Template->Modify();

	const int32 DisplayIdx = VibeUE_FindDisplayIndex(Template, Type, Index);
	if (DisplayIdx != INDEX_NONE)
	{
		Template->RemoveDisplayTrack(DisplayIdx);
	}
	switch (Type)
	{
	case FTTTrackBase::TT_FloatInterp: Template->FloatTracks.RemoveAt(Index); break;
	case FTTTrackBase::TT_VectorInterp: Template->VectorTracks.RemoveAt(Index); break;
	case FTTTrackBase::TT_LinearColorInterp: Template->LinearColorTracks.RemoveAt(Index); break;
	case FTTTrackBase::TT_Event: Template->EventTracks.RemoveAt(Index); break;
	}

	TimelineNode->ReconstructNode();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("RemoveTimelineTrack: Removed track '%s' from timeline '%s'"), *TrackName, *TimelineName);
	return true;
}

bool UBlueprintService::RenameTimelineTrack(const FString& BlueprintPath, const FString& TimelineName, const FString& OldTrackName, const FString& NewTrackName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint) { UE_LOG(LogTemp, Error, TEXT("RenameTimelineTrack: Failed to load blueprint: %s"), *BlueprintPath); return false; }
	FString Error;
	UTimelineTemplate* Template = VibeUE_ResolveTimeline(Blueprint, TimelineName, Error);
	if (!Template) { UE_LOG(LogTemp, Error, TEXT("RenameTimelineTrack: %s"), *Error); return false; }

	const FName OldName(*OldTrackName);
	const FName NewName(*NewTrackName);
	FTTTrackBase::ETrackType Type; int32 Index;
	FTTTrackBase* Track = VibeUE_FindAnyTrack(Template, OldName, Type, Index);
	if (!Track)
	{
		UE_LOG(LogTemp, Error, TEXT("RenameTimelineTrack: Track '%s' not found on timeline '%s'"), *OldTrackName, *TimelineName);
		return false;
	}
	if (NewTrackName.IsEmpty() || !Template->IsNewTrackNameValid(NewName))
	{
		UE_LOG(LogTemp, Error, TEXT("RenameTimelineTrack: New name '%s' is not valid/unique on timeline '%s'"), *NewTrackName, *TimelineName);
		return false;
	}
	UK2Node_Timeline* TimelineNode = FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, Template);
	if (!TimelineNode) { UE_LOG(LogTemp, Error, TEXT("RenameTimelineTrack: No Timeline node for '%s'"), *TimelineName); return false; }

	TimelineNode->Modify();
	Template->Modify();

	for (UEdGraphPin* Pin : TimelineNode->Pins)
	{
		if (Pin && Pin->PinName == OldName)
		{
			Pin->Modify();
			Pin->PinName = NewName;
			break;
		}
	}
	Track->SetTrackName(NewName, Template);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("RenameTimelineTrack: Renamed '%s' -> '%s' on timeline '%s'"), *OldTrackName, *NewTrackName, *TimelineName);
	return true;
}

bool UBlueprintService::AddTimelineVectorKey(const FString& BlueprintPath, const FString& TimelineName, const FString& TrackName, float Time, float X, float Y, float Z, const FString& InterpMode)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint) { UE_LOG(LogTemp, Error, TEXT("AddTimelineVectorKey: Failed to load blueprint: %s"), *BlueprintPath); return false; }
	FString Error;
	UTimelineTemplate* Template = VibeUE_ResolveTimeline(Blueprint, TimelineName, Error);
	if (!Template) { UE_LOG(LogTemp, Error, TEXT("AddTimelineVectorKey: %s"), *Error); return false; }
	FTTTrackBase::ETrackType Type; int32 Index;
	FTTTrackBase* Track = VibeUE_FindAnyTrack(Template, FName(*TrackName), Type, Index);
	if (!Track || Type != FTTTrackBase::TT_VectorInterp)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTimelineVectorKey: Vector track '%s' not found on timeline '%s'"), *TrackName, *TimelineName);
		return false;
	}
	UCurveVector* Curve = static_cast<FTTVectorTrack*>(Track)->CurveVector;
	if (!Curve) { UE_LOG(LogTemp, Error, TEXT("AddTimelineVectorKey: track '%s' has no curve"), *TrackName); return false; }
	ERichCurveInterpMode I; ERichCurveTangentMode Tn; VibeUE_ParseInterp(InterpMode, I, Tn);
	Curve->Modify();
	const float V[3] = { X, Y, Z };
	for (int32 i = 0; i < 3; ++i) { VibeUE_AddCurveKey(Curve->FloatCurves[i], Time, V[i], I, Tn); }
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddTimelineVectorKey: Added key (t=%.4f) to '%s.%s'"), Time, *TimelineName, *TrackName);
	return true;
}

bool UBlueprintService::AddTimelineColorKey(const FString& BlueprintPath, const FString& TimelineName, const FString& TrackName, float Time, float R, float G, float B, float A, const FString& InterpMode)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint) { UE_LOG(LogTemp, Error, TEXT("AddTimelineColorKey: Failed to load blueprint: %s"), *BlueprintPath); return false; }
	FString Error;
	UTimelineTemplate* Template = VibeUE_ResolveTimeline(Blueprint, TimelineName, Error);
	if (!Template) { UE_LOG(LogTemp, Error, TEXT("AddTimelineColorKey: %s"), *Error); return false; }
	FTTTrackBase::ETrackType Type; int32 Index;
	FTTTrackBase* Track = VibeUE_FindAnyTrack(Template, FName(*TrackName), Type, Index);
	if (!Track || Type != FTTTrackBase::TT_LinearColorInterp)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTimelineColorKey: Color track '%s' not found on timeline '%s'"), *TrackName, *TimelineName);
		return false;
	}
	UCurveLinearColor* Curve = static_cast<FTTLinearColorTrack*>(Track)->CurveLinearColor;
	if (!Curve) { UE_LOG(LogTemp, Error, TEXT("AddTimelineColorKey: track '%s' has no curve"), *TrackName); return false; }
	ERichCurveInterpMode I; ERichCurveTangentMode Tn; VibeUE_ParseInterp(InterpMode, I, Tn);
	Curve->Modify();
	const float V[4] = { R, G, B, A };
	for (int32 i = 0; i < 4; ++i) { VibeUE_AddCurveKey(Curve->FloatCurves[i], Time, V[i], I, Tn); }
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddTimelineColorKey: Added key (t=%.4f) to '%s.%s'"), Time, *TimelineName, *TrackName);
	return true;
}

bool UBlueprintService::AddTimelineEventKey(const FString& BlueprintPath, const FString& TimelineName, const FString& TrackName, float Time)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint) { UE_LOG(LogTemp, Error, TEXT("AddTimelineEventKey: Failed to load blueprint: %s"), *BlueprintPath); return false; }
	FString Error;
	UTimelineTemplate* Template = VibeUE_ResolveTimeline(Blueprint, TimelineName, Error);
	if (!Template) { UE_LOG(LogTemp, Error, TEXT("AddTimelineEventKey: %s"), *Error); return false; }
	FTTTrackBase::ETrackType Type; int32 Index;
	FTTTrackBase* Track = VibeUE_FindAnyTrack(Template, FName(*TrackName), Type, Index);
	if (!Track || Type != FTTTrackBase::TT_Event)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTimelineEventKey: Event track '%s' not found on timeline '%s'"), *TrackName, *TimelineName);
		return false;
	}
	UCurveFloat* Curve = static_cast<FTTEventTrack*>(Track)->CurveKeys;
	if (!Curve) { UE_LOG(LogTemp, Error, TEXT("AddTimelineEventKey: track '%s' has no curve"), *TrackName); return false; }
	Curve->Modify();
	// Event keys are constant-interp markers; value is irrelevant.
	VibeUE_AddCurveKey(Curve->FloatCurve, Time, 1.0f, RCIM_Constant, RCTM_Auto);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddTimelineEventKey: Added event key (t=%.4f) to '%s.%s'"), Time, *TimelineName, *TrackName);
	return true;
}

bool UBlueprintService::RemoveTimelineKey(const FString& BlueprintPath, const FString& TimelineName, const FString& TrackName, float Time, float Tolerance)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint) { UE_LOG(LogTemp, Error, TEXT("RemoveTimelineKey: Failed to load blueprint: %s"), *BlueprintPath); return false; }
	FString Error;
	UTimelineTemplate* Template = VibeUE_ResolveTimeline(Blueprint, TimelineName, Error);
	if (!Template) { UE_LOG(LogTemp, Error, TEXT("RemoveTimelineKey: %s"), *Error); return false; }
	FTTTrackBase::ETrackType Type; int32 Index;
	FTTTrackBase* Track = VibeUE_FindAnyTrack(Template, FName(*TrackName), Type, Index);
	if (!Track) { UE_LOG(LogTemp, Error, TEXT("RemoveTimelineKey: Track '%s' not found on timeline '%s'"), *TrackName, *TimelineName); return false; }

	TArray<UCurveBase*> CurveObjs; VibeUE_TrackCurveObjects(Track, Type, CurveObjs);
	TArray<FRichCurve*> Curves; VibeUE_TrackCurves(Track, Type, Curves);
	for (UCurveBase* C : CurveObjs) { C->Modify(); }

	int32 Removed = 0;
	for (FRichCurve* Curve : Curves)
	{
		FKeyHandle KH = Curve->FindKey(Time, Tolerance);
		while (Curve->IsKeyHandleValid(KH))
		{
			Curve->DeleteKey(KH);
			++Removed;
			KH = Curve->FindKey(Time, Tolerance);
		}
	}
	if (Removed == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveTimelineKey: No key near t=%.4f on '%s.%s'"), Time, *TimelineName, *TrackName);
		return false;
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("RemoveTimelineKey: Removed %d key(s) near t=%.4f from '%s.%s'"), Removed, Time, *TimelineName, *TrackName);
	return true;
}

bool UBlueprintService::ClearTimelineTrackKeys(const FString& BlueprintPath, const FString& TimelineName, const FString& TrackName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint) { UE_LOG(LogTemp, Error, TEXT("ClearTimelineTrackKeys: Failed to load blueprint: %s"), *BlueprintPath); return false; }
	FString Error;
	UTimelineTemplate* Template = VibeUE_ResolveTimeline(Blueprint, TimelineName, Error);
	if (!Template) { UE_LOG(LogTemp, Error, TEXT("ClearTimelineTrackKeys: %s"), *Error); return false; }
	FTTTrackBase::ETrackType Type; int32 Index;
	FTTTrackBase* Track = VibeUE_FindAnyTrack(Template, FName(*TrackName), Type, Index);
	if (!Track) { UE_LOG(LogTemp, Error, TEXT("ClearTimelineTrackKeys: Track '%s' not found on timeline '%s'"), *TrackName, *TimelineName); return false; }

	TArray<UCurveBase*> CurveObjs; VibeUE_TrackCurveObjects(Track, Type, CurveObjs);
	TArray<FRichCurve*> Curves; VibeUE_TrackCurves(Track, Type, Curves);
	for (UCurveBase* C : CurveObjs) { C->Modify(); }
	for (FRichCurve* Curve : Curves) { Curve->Reset(); }
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("ClearTimelineTrackKeys: Cleared all keys on '%s.%s'"), *TimelineName, *TrackName);
	return true;
}

bool UBlueprintService::ModifyTimeline(const FString& BlueprintPath, const FString& TimelineName, const FString& NewName,
	float Length, int32 UseLastKeyFrame, int32 AutoPlay, int32 Loop, int32 Replicated, int32 IgnoreTimeDilation)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint) { UE_LOG(LogTemp, Error, TEXT("ModifyTimeline: Failed to load blueprint: %s"), *BlueprintPath); return false; }
	FString Error;
	UTimelineTemplate* Template = VibeUE_ResolveTimeline(Blueprint, TimelineName, Error);
	if (!Template) { UE_LOG(LogTemp, Error, TEXT("ModifyTimeline: %s"), *Error); return false; }
	UK2Node_Timeline* TimelineNode = FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, Template);

	bool bChanged = false;

	if (!NewName.IsEmpty())
	{
		const FName Old = Template->GetVariableName();
		const FName New(*NewName);
		if (Old != New)
		{
			if (!FBlueprintEditorUtils::RenameTimeline(Blueprint, Old, New))
			{
				UE_LOG(LogTemp, Error, TEXT("ModifyTimeline: RenameTimeline '%s' -> '%s' failed"), *Old.ToString(), *NewName);
				return false;
			}
			bChanged = true;
			// Re-resolve under the new name for any further changes.
			Template = Blueprint->FindTimelineTemplateByVariableName(New);
			TimelineNode = Template ? FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, Template) : nullptr;
			if (!Template) { return bChanged; }
		}
	}

	Template->Modify();
	if (TimelineNode) { TimelineNode->Modify(); }

	if (UseLastKeyFrame >= 0)
	{
		Template->LengthMode = (UseLastKeyFrame != 0) ? ETimelineLengthMode::TL_LastKeyFrame : ETimelineLengthMode::TL_TimelineLength;
		bChanged = true;
	}
	if (Length >= 0.0f)
	{
		Template->TimelineLength = Length;
		if (UseLastKeyFrame < 0) { Template->LengthMode = ETimelineLengthMode::TL_TimelineLength; }
		bChanged = true;
	}
	if (AutoPlay >= 0) { Template->bAutoPlay = (AutoPlay != 0); if (TimelineNode) { TimelineNode->bAutoPlay = (AutoPlay != 0); } bChanged = true; }
	if (Loop >= 0) { Template->bLoop = (Loop != 0); if (TimelineNode) { TimelineNode->bLoop = (Loop != 0); } bChanged = true; }
	if (Replicated >= 0) { Template->bReplicated = (Replicated != 0); if (TimelineNode) { TimelineNode->bReplicated = (Replicated != 0); } bChanged = true; }
	if (IgnoreTimeDilation >= 0) { Template->bIgnoreTimeDilation = (IgnoreTimeDilation != 0); if (TimelineNode) { TimelineNode->bIgnoreTimeDilation = (IgnoreTimeDilation != 0); } bChanged = true; }

	if (!bChanged)
	{
		UE_LOG(LogTemp, Warning, TEXT("ModifyTimeline: nothing to change for '%s'"), *TimelineName);
		return false;
	}
	if (TimelineNode) { TimelineNode->ReconstructNode(); }
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("ModifyTimeline: updated timeline '%s'"), *TimelineName);
	return true;
}

bool UBlueprintService::RemoveTimeline(const FString& BlueprintPath, const FString& TimelineName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint) { UE_LOG(LogTemp, Error, TEXT("RemoveTimeline: Failed to load blueprint: %s"), *BlueprintPath); return false; }
	FString Error;
	UTimelineTemplate* Template = VibeUE_ResolveTimeline(Blueprint, TimelineName, Error);
	if (!Template) { UE_LOG(LogTemp, Error, TEXT("RemoveTimeline: %s"), *Error); return false; }

	if (UK2Node_Timeline* TimelineNode = FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, Template))
	{
		FBlueprintEditorUtils::RemoveNode(Blueprint, TimelineNode, /*bDontRecompile*/true);
	}
	FBlueprintEditorUtils::RemoveTimeline(Blueprint, Template, /*bDontRecompile*/false);
	UE_LOG(LogTemp, Log, TEXT("RemoveTimeline: Removed timeline '%s' from %s"), *TimelineName, *BlueprintPath);
	return true;
}



namespace
{
	// Estimate a node's size for bounding-box math when NodeWidth/NodeHeight haven't been
	// computed yet (Slate widget hasn't run). The defaults are intentionally generous so
	// the comment box doesn't visually clip the wrapped nodes.
	static void EstimateNodeBounds(UEdGraphNode* Node, float& OutMinX, float& OutMinY, float& OutMaxX, float& OutMaxY)
	{
		const float DefaultWidth = 256.0f;
		const float DefaultHeight = 128.0f;

		const float W = (Node && Node->NodeWidth  > 0.0f) ? Node->NodeWidth  : DefaultWidth;
		const float H = (Node && Node->NodeHeight > 0.0f) ? Node->NodeHeight : DefaultHeight;

		OutMinX = Node ? Node->NodePosX : 0.0f;
		OutMinY = Node ? Node->NodePosY : 0.0f;
		OutMaxX = OutMinX + W;
		OutMaxY = OutMinY + H;
	}

	static UEdGraphNode_Comment* SpawnCommentNode(
		UEdGraph* Graph,
		const FString& CommentText,
		float PosX, float PosY,
		float Width, float Height,
		float R, float G, float B, float A)
	{
		if (!Graph)
		{
			return nullptr;
		}

		UEdGraphNode_Comment* CommentNode = NewObject<UEdGraphNode_Comment>(Graph);
		Graph->AddNode(CommentNode, /*bFromUI=*/false, /*bSelectNewNode=*/false);
		CommentNode->CreateNewGuid();
		CommentNode->PostPlacedNewNode();
		CommentNode->AllocateDefaultPins();

		CommentNode->NodePosX  = PosX;
		CommentNode->NodePosY  = PosY;
		CommentNode->NodeWidth  = FMath::Max(64.0f, Width);
		CommentNode->NodeHeight = FMath::Max(64.0f, Height);
		CommentNode->NodeComment = CommentText;
		CommentNode->CommentColor = FLinearColor(R, G, B, A);

		return CommentNode;
	}
}






namespace
{
	// Helper: convert a UEdGraphNode into the FBlueprintNodeInfo struct used by
	// GetNodesInGraph / GetSelectedNodes. Mirrors the body of the GetNodesInGraph
	// loop so both APIs produce identical shapes.
	static FBlueprintNodeInfo MakeBlueprintNodeInfoFromNode(UEdGraphNode* Node)
	{
		FBlueprintNodeInfo NodeInfo;
		if (!Node)
		{
			return NodeInfo;
		}

		NodeInfo.NodeId = Node->NodeGuid.ToString();
		NodeInfo.NodeType = Node->GetClass()->GetName();
		NodeInfo.NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		NodeInfo.PosX = Node->NodePosX;
		NodeInfo.PosY = Node->NodePosY;

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin)
			{
				continue;
			}

			NodeInfo.PinNames.Add(Pin->PinName.ToString());

			FBlueprintPinInfo PinInfo;
			PinInfo.PinName = Pin->PinName.ToString();
			PinInfo.PinType = Pin->PinType.PinCategory.ToString();
			PinInfo.bIsInput = (Pin->Direction == EGPD_Input);
			PinInfo.bIsConnected = Pin->LinkedTo.Num() > 0;
			PinInfo.DefaultValue = Pin->GetDefaultAsString();
			PinInfo.DefaultObject = GetPinDefaultObjectPath(Pin);
			NodeInfo.Pins.Add(PinInfo);
		}

		return NodeInfo;
	}
}





// ============================================================================
// ADVANCED NODE OPERATIONS (Phase 4)
// ============================================================================











// ============================================================================
// NODE MANAGEMENT - Advanced Operations
// ============================================================================









// ============================================================================
// EXISTENCE CHECKS - Fast boolean checks before creation (Idempotency)
// ============================================================================











// ============================================================================
// FUNCTION OVERRIDES
// ============================================================================




bool UBlueprintService::RefreshBlueprintEditor(const FString& BlueprintPath)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("RefreshBlueprintEditor: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (!AssetEditorSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("RefreshBlueprintEditor: AssetEditorSubsystem not available"));
		return false;
	}

	IAssetEditorInstance* EditorInstance = AssetEditorSubsystem->FindEditorForAsset(Blueprint, /*bFocusIfOpen=*/false);
	if (!EditorInstance)
	{
		UE_LOG(LogTemp, Log, TEXT("RefreshBlueprintEditor: Blueprint '%s' has no editor open — nothing to refresh"), *BlueprintPath);
		return false;
	}

	// Rebuild node state on the Blueprint itself, then ask the open editor to redraw so the SCS
	// viewport re-runs the construction script.
	FBlueprintEditorUtils::RefreshAllNodes(Blueprint);

	if (EditorInstance->GetEditorName() == FName(TEXT("BlueprintEditor"))
		|| EditorInstance->GetEditorName() == FName(TEXT("WidgetBlueprintEditor"))
		|| EditorInstance->GetEditorName() == FName(TEXT("AnimationBlueprintEditor")))
	{
		FBlueprintEditor* BlueprintEditor = static_cast<FBlueprintEditor*>(EditorInstance);
		BlueprintEditor->RefreshEditors();
	}

	UE_LOG(LogTemp, Log, TEXT("RefreshBlueprintEditor: refreshed open editor for '%s'"), *BlueprintPath);
	return true;
}

namespace
{
	// The channels a Blueprint component may name: the 8 engine channels and the 18 game channels.
	// ECC_EngineTraceChannel1..6 are reserved for the engine and never offered.
	bool IsUserFacingCollisionChannel(int32 Index)
	{
		return (Index >= static_cast<int32>(ECC_WorldStatic) && Index <= static_cast<int32>(ECC_Destructible))
			|| (Index >= static_cast<int32>(ECC_GameTraceChannel1) && Index <= static_cast<int32>(ECC_GameTraceChannel18));
	}

	// The name the project knows a channel by: the configured Name for a custom channel
	// (DefaultEngine.ini +DefaultChannelResponses=(...,Name="Weapon")), else the enum name without
	// its ECC_ prefix (an unconfigured custom channel reads "GameTraceChannel4").
	FString CollisionChannelDisplayName(int32 Index)
	{
		if (const UCollisionProfile* Profile = UCollisionProfile::Get())
		{
			const FName Name = Profile->ReturnChannelNameFromContainerIndex(Index);
			if (!Name.IsNone())
			{
				return Name.ToString();
			}
		}
		FString EnumName = StaticEnum<ECollisionChannel>()->GetNameStringByValue(Index);
		EnumName.RemoveFromStart(TEXT("ECC_"));
		return EnumName;
	}

	// Resolve a collision channel name for set_collision_settings (ChannelResponses keys and
	// ObjectType). Case-insensitive. Accepts the engine channel names ("WorldStatic".."Destructible"),
	// a project's configured custom channel names (read from UCollisionProfile at call time, with or
	// without spaces or an "ECC_" prefix), and the raw enum names "ECC_WorldStatic".."ECC_Destructible"
	// and "ECC_GameTraceChannel1".."ECC_GameTraceChannel18".
	bool ResolveCollisionChannel(const FString& InName, ECollisionChannel& OutChannel)
	{
		FString Name = InName.TrimStartAndEnd();
		if (Name.IsEmpty())
		{
			return false;
		}

		// Raw enum spelling: ECC_WorldStatic, ECC_GameTraceChannel4, ...
		if (const UEnum* ChannelEnum = StaticEnum<ECollisionChannel>())
		{
			for (int32 EnumIndex = 0; EnumIndex < ChannelEnum->NumEnums(); ++EnumIndex)
			{
				const int32 Value = static_cast<int32>(ChannelEnum->GetValueByIndex(EnumIndex));
				if (IsUserFacingCollisionChannel(Value)
					&& ChannelEnum->GetNameStringByIndex(EnumIndex).Equals(Name, ESearchCase::IgnoreCase))
				{
					OutChannel = static_cast<ECollisionChannel>(Value);
					return true;
				}
			}
		}

		// Display / configured names, with or without spaces and with or without an ECC_ prefix
		// (the engine's script name for a custom channel is "ECC_" + the name with spaces removed).
		Name.RemoveFromStart(TEXT("ECC_"), ESearchCase::IgnoreCase);
		const FString Compact = Name.Replace(TEXT(" "), TEXT(""));
		for (int32 Index = 0; Index <= static_cast<int32>(ECC_GameTraceChannel18); ++Index)
		{
			if (!IsUserFacingCollisionChannel(Index))
			{
				continue;
			}
			const FString DisplayName = CollisionChannelDisplayName(Index);
			if (DisplayName.Equals(Name, ESearchCase::IgnoreCase)
				|| DisplayName.Replace(TEXT(" "), TEXT("")).Equals(Compact, ESearchCase::IgnoreCase))
			{
				OutChannel = static_cast<ECollisionChannel>(Index);
				return true;
			}
		}
		return false;
	}

	// Every accepted channel name, for the refusal message.
	FString DescribeValidCollisionChannels()
	{
		TArray<FString> Names;
		for (int32 Index = 0; Index <= static_cast<int32>(ECC_GameTraceChannel18); ++Index)
		{
			if (IsUserFacingCollisionChannel(Index))
			{
				Names.Add(CollisionChannelDisplayName(Index));
			}
		}
		return FString::Join(Names, TEXT(", ")) + TEXT(" (or the raw enum names ECC_WorldStatic..ECC_Destructible, ECC_GameTraceChannel1..18)");
	}
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
			NewNode->AllocateDefaultPins();
			// Match UBlueprintNodeSpawner: placement callbacks may require allocated pins.
			NewNode->PostPlacedNewNode();
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

		// An override event (BeginPlay, Tick, overlaps, ...) can exist only once per Blueprint, and
		// subclass templates ship ghost nodes for the common ones — adopt the existing node instead
		// of stacking a duplicate (issue #551). Same contract as CreateNodeByKey (issue #349).
		UClass* EventOwnerClass = EventFunction->GetOwnerClass();
		UK2Node_Event* ExistingEvent = FBlueprintEditorUtils::FindOverrideForFunction(Blueprint, EventOwnerClass, EventFunction->GetFName());
		if (!ExistingEvent)
		{
			// Ghost nodes can record the Blueprint's immediate parent rather than the declaring
			// class as the member parent — match by name like OverrideFunction does.
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				UK2Node_Event* Candidate = Cast<UK2Node_Event>(Node);
				if (Candidate && Candidate->bOverrideFunction &&
					Candidate->EventReference.GetMemberName().ToString().Equals(*EventName, ESearchCase::IgnoreCase))
				{
					ExistingEvent = Candidate;
					break;
				}
			}
		}
		if (ExistingEvent)
		{
			if (ExistingEvent->GetGraph() != Graph)
			{
				OutError = FString::Printf(TEXT("Node '%s': Event '%s' is already placed in graph '%s' — an override event can exist only once per Blueprint"),
					*Desc.Ref, **EventName, *GetNameSafe(ExistingEvent->GetGraph()));
				return nullptr;
			}
			ExistingEvent->Modify();
			if (ExistingEvent->GetDesiredEnabledState() != ENodeEnabledState::Enabled)
			{
				// Ghost nodes ship disabled; adopting one for a build means the caller wants it live.
				ExistingEvent->SetEnabledState(ENodeEnabledState::Enabled, /*bUserAction=*/false);
			}
			if (ExistingEvent->Pins.Num() == 0)
			{
				ExistingEvent->AllocateDefaultPins();
			}
			return ExistingEvent;
		}

		UK2Node_Event* EventNode = NewObject<UK2Node_Event>(Graph);
		// Bind to the declaring class, not the immediate parent — a subclass parent class here made
		// the reference miss the engine's existing-override comparison (issue #551).
		EventNode->EventReference.SetExternalMember(FName(**EventName), EventOwnerClass);
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

		// Custom event names are unique per Blueprint — adopt an existing node instead of stacking
		// a duplicate, matching CreateNodeByKey's contract (issue #551).
		if (CustomEventName != NAME_None)
		{
			if (UK2Node_Event* ExistingCustom = FBlueprintEditorUtils::FindCustomEventNode(Blueprint, CustomEventName))
			{
				if (ExistingCustom->GetGraph() != Graph)
				{
					OutError = FString::Printf(TEXT("Node '%s': Custom event '%s' already exists in graph '%s'"),
						*Desc.Ref, *CustomEventName.ToString(), *GetNameSafe(ExistingCustom->GetGraph()));
					return nullptr;
				}
				return ExistingCustom;
			}
		}

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

		// ResolveClassByName handles "BP_Foo", "BP_Foo_C", and "/Game/..." forms and loads unloaded
		// Blueprints — the bare FindFirstObject needed an exact, already-loaded "_C" name (issue #552).
		UClass* TargetUClass = ResolveClassByName(**TargetClass);
		if (!TargetUClass)
		{
			TargetUClass = FindFirstObject<UClass>(**TargetClass, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("BuildGraph"));
		}
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

		// ResolveClassByName accepts "BP_Foo", "BP_Foo_C", and "/Game/..." forms and loads unloaded
		// Blueprints; the raw iterator needed an exact, already-loaded class name (issue #552).
		UClass* OwnerClass = ResolveClassByName(*ClassName);
		if (!OwnerClass)
		{
			for (TObjectIterator<UClass> It; It; ++It)
			{
				if (It->GetName() == *ClassName)
				{
					OwnerClass = *It;
					break;
				}
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

	// ── member_set ──
	// Symmetric to member_get: SET a property that belongs to ANOTHER class (e.g. a
	// component's property like UCharacterMovementComponent::MaxWalkSpeed). Produces a
	// K2Node_VariableSet with bSelfContext=false, exposing the value pin plus a typed
	// "self"/Target pin to wire the owning object (e.g. a Get CharacterMovement node).
	// Use plain variable_set for properties on the Blueprint itself.
	// Params: member = property name, class = owning class name (e.g. "CharacterMovementComponent").
	if (Type.Equals(TEXT("member_set"), ESearchCase::IgnoreCase))
	{
		const FString* MemberName = Desc.Params.Find(TEXT("member"));
		const FString* ClassName = Desc.Params.Find(TEXT("class"));
		if (!MemberName || !ClassName)
		{
			OutError = FString::Printf(TEXT("Node '%s': member_set requires 'member' and 'class' params"), *Desc.Ref);
			return nullptr;
		}

		// ResolveClassByName accepts "BP_Foo", "BP_Foo_C", and "/Game/..." forms and loads unloaded
		// Blueprints; the raw iterator needed an exact, already-loaded class name (issue #552).
		UClass* OwnerClass = ResolveClassByName(*ClassName);
		if (!OwnerClass)
		{
			for (TObjectIterator<UClass> It; It; ++It)
			{
				if (It->GetName() == *ClassName)
				{
					OwnerClass = *It;
					break;
				}
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

		UK2Node_VariableSet* SetNode = NewObject<UK2Node_VariableSet>(Graph);
		SetNode->VariableReference.SetExternalMember(FName(**MemberName), OwnerClass);
		SetNode->NodePosX = PosX;
		SetNode->NodePosY = PosY;
		Graph->AddNode(SetNode, false, false);
		SetNode->CreateNewGuid();
		SetNode->PostPlacedNewNode();
		SetNode->AllocateDefaultPins();
		return SetNode;
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

// Defined with the layout algorithm further down; declared here because BuildGraph's
// comment-box phase needs the same estimated node geometry.
namespace VibeUELayout
{
	static float EstimateNodeHeight(const UEdGraphNode* Node);
	static float EstimateNodeWidth(const UEdGraphNode* Node);
}



// ────────────────────────────────────────────────────────────────
// AutoLayoutGraph
// ────────────────────────────────────────────────────────────────

// ────────────────────────────────────────────────────────────────
// Shared layered graph layout used by AutoLayoutGraph + AutoLayoutSelectedNodes.
// Columns by dependency depth (longest path over BOTH exec and data edges, so
// deep data chains step rightward instead of collapsing into one column),
// independent components stacked into non-overlapping horizontal bands, wire
// crossings reduced with median ordering sweeps, and rows spaced by node size.
// ────────────────────────────────────────────────────────────────
namespace VibeUELayout
{
	static float EstimateNodeHeight(const UEdGraphNode* Node)
	{
		int32 In = 0, Out = 0;
		for (const UEdGraphPin* P : Node->Pins)
		{
			if (!P || P->bHidden) continue;
			if (P->Direction == EGPD_Input) ++In; else ++Out;
		}
		const int32 Rows = FMath::Max3(In, Out, 1);
		return 72.0f + (float)Rows * 26.0f;
	}

	static float EstimateNodeWidth(const UEdGraphNode* Node)
	{
		// Approximate the rendered widget: header sized by title, body sized by the
		// longest input + output pin label pair (~7 px/char at 1:1 zoom).
		const FString Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		int32 MaxIn = 0, MaxOut = 0;
		for (const UEdGraphPin* P : Node->Pins)
		{
			if (!P || P->bHidden) continue;
			const int32 Len = (P->PinFriendlyName.IsEmpty() ? P->PinName.ToString() : P->PinFriendlyName.ToString()).Len();
			if (P->Direction == EGPD_Input) { MaxIn = FMath::Max(MaxIn, Len); }
			else { MaxOut = FMath::Max(MaxOut, Len); }
		}
		const float TitleW = 48.0f + (float)Title.Len() * 7.0f;
		const float PinsW = 64.0f + (float)(MaxIn + MaxOut) * 7.0f;
		return FMath::Clamp(FMath::Max(TitleW, PinsW), 140.0f, 560.0f);
	}

	// Center-Y offset of a pin's row within its node, using the same geometry model as
	// EstimateNodeHeight (header ≈46 px, then 26 px per visible pin row per direction).
	static float PinRowCenterY(const UEdGraphNode* Node, const UEdGraphPin* Pin)
	{
		int32 Row = 0;
		for (const UEdGraphPin* P : Node->Pins)
		{
			if (!P || P->bHidden || P->Direction != Pin->Direction) continue;
			if (P == Pin) break;
			++Row;
		}
		return 46.0f + (float)Row * 26.0f;
	}

	// Strict segment intersection (shared endpoints / collinear touches don't count).
	static bool SegmentsIntersect(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D)
	{
		auto Cross = [](const FVector2D& O, const FVector2D& P, const FVector2D& Q)
		{
			return (P.X - O.X) * (Q.Y - O.Y) - (P.Y - O.Y) * (Q.X - O.X);
		};
		const double d1 = Cross(C, D, A), d2 = Cross(C, D, B), d3 = Cross(A, B, C), d4 = Cross(A, B, D);
		return ((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) && ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0));
	}

	// Lay out Nodes left-to-right anchored at (OriginX, OriginY). Returns component count.
	static int32 LayeredLayout(const TArray<UEdGraphNode*>& Nodes, float OriginX, float OriginY)
	{
		if (Nodes.Num() == 0)
		{
			return 0;
		}

		TSet<UEdGraphNode*> InSet(Nodes);

		// Precedence edges (source -> target) for every link (exec AND data) within the set.
		TMap<UEdGraphNode*, TArray<UEdGraphNode*>> Succ;
		TMap<UEdGraphNode*, TArray<UEdGraphNode*>> Pred;
		TMap<UEdGraphNode*, TArray<UEdGraphNode*>> ExecSucc;
		TMap<UEdGraphNode*, TArray<UEdGraphNode*>> ExecPred;
		for (UEdGraphNode* N : Nodes)
		{
			for (UEdGraphPin* P : N->Pins)
			{
				if (!P || P->Direction != EGPD_Output)
				{
					continue;
				}
				for (UEdGraphPin* L : P->LinkedTo)
				{
					UEdGraphNode* T = L ? L->GetOwningNode() : nullptr;
					if (!T || T == N || !InSet.Contains(T))
					{
						continue;
					}
					Succ.FindOrAdd(N).AddUnique(T);
					Pred.FindOrAdd(T).AddUnique(N);
					if (P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
					{
						ExecSucc.FindOrAdd(N).AddUnique(T);
						ExecPred.FindOrAdd(T).AddUnique(N);
					}
				}
			}
		}

		// Break cycles for layering: classify edges with an iterative DFS and drop
		// back-edges (edges pointing at a node still on the DFS stack). The remaining
		// edges form a DAG, so longest-path layering is well-defined and a cycle (a loop
		// body wired back to its loop, recursion, etc.) can never inflate columns. The
		// back-edge is still drawn as a wire; it just doesn't drive column assignment.
		TMap<UEdGraphNode*, int32> VisitRank; // DFS pre-order rank (pin-ordered) for stable within-layer seeding
		TMap<UEdGraphNode*, TArray<UEdGraphNode*>> LayerSucc;
		TMap<UEdGraphNode*, TArray<UEdGraphNode*>> LayerPred;
		{
			TSet<UEdGraphNode*> Visited;
			TSet<UEdGraphNode*> OnStack;
			// Seed DFS from exec entry points (events) first, then other roots (no
			// predecessors), then everything else — so the dropped edge is the genuine
			// loop-back rather than an arbitrary forward edge, and loop bodies still flow L->R.
			TArray<UEdGraphNode*> SeedOrder;
			// Primary events (BeginPlay/Tick/input/overrides) seed before custom events so
			// the main event chain gets the earliest visit ranks (issue #354).
			for (UEdGraphNode* SN : Nodes)
			{
				if (SN->IsA<UK2Node_Event>() && !SN->IsA<UK2Node_CustomEvent>()) { SeedOrder.AddUnique(SN); }
			}
			for (UEdGraphNode* SN : Nodes)
			{
				if (SN->IsA<UK2Node_CustomEvent>()) { SeedOrder.AddUnique(SN); }
			}
			for (UEdGraphNode* SN : Nodes)
			{
				const TArray<UEdGraphNode*>* PP = Pred.Find(SN);
				if (!PP || PP->Num() == 0) { SeedOrder.AddUnique(SN); }
			}
			for (UEdGraphNode* SN : Nodes) { SeedOrder.AddUnique(SN); }

			for (UEdGraphNode* Seed : SeedOrder)
			{
				if (Visited.Contains(Seed))
				{
					continue;
				}
				TArray<TPair<UEdGraphNode*, int32>> Stack;
				Stack.Push(TPair<UEdGraphNode*, int32>(Seed, 0));
				OnStack.Add(Seed);
					VisitRank.Add(Seed, VisitRank.Num());
				while (Stack.Num() > 0)
				{
					const int32 TopIdx = Stack.Num() - 1;
					UEdGraphNode* U = Stack[TopIdx].Key;
					const int32 ChildI = Stack[TopIdx].Value;
					const TArray<UEdGraphNode*>* Children = Succ.Find(U);
					if (Children && ChildI < Children->Num())
					{
						Stack[TopIdx].Value = ChildI + 1;
						UEdGraphNode* V = (*Children)[ChildI];
						if (OnStack.Contains(V))
						{
							continue; // back-edge: skip for layering
						}
						LayerSucc.FindOrAdd(U).AddUnique(V);
						LayerPred.FindOrAdd(V).AddUnique(U);
						if (!Visited.Contains(V))
						{
							OnStack.Add(V);
								VisitRank.Add(V, VisitRank.Num());
							Stack.Push(TPair<UEdGraphNode*, int32>(V, 0));
						}
					}
					else
					{
						OnStack.Remove(U);
						Visited.Add(U);
						Stack.Pop();
					}
				}
			}
		}

		// Layer = longest path over the acyclic edge set (Kahn topological order).
		TMap<UEdGraphNode*, int32> Layer;
		TMap<UEdGraphNode*, int32> Deg;
		for (UEdGraphNode* N : Nodes)
		{
			Layer.Add(N, 0);
			Deg.Add(N, 0);
		}
		for (const TPair<UEdGraphNode*, TArray<UEdGraphNode*>>& Pair : LayerPred)
		{
			Deg[Pair.Key] = Pair.Value.Num();
		}

		TQueue<UEdGraphNode*> Q;
		for (UEdGraphNode* N : Nodes)
		{
			if (Deg[N] == 0)
			{
				Q.Enqueue(N);
			}
		}
		while (!Q.IsEmpty())
		{
			UEdGraphNode* C = nullptr;
			Q.Dequeue(C);
			if (const TArray<UEdGraphNode*>* S = LayerSucc.Find(C))
			{
				for (UEdGraphNode* T : *S)
				{
					Layer[T] = FMath::Max(Layer[T], Layer[C] + 1);
					if (--Deg[T] <= 0)
					{
						Q.Enqueue(T);
					}
				}
			}
		}

		// Pull pure (exec-less) nodes rightward toward their consumers (ALAP): longest-path
		// layering from roots drops a getter that feeds a column-7 node into column 0,
		// producing a wire that spans the whole graph (issue #427). Relax each pure node up
		// to just left of its nearest consumer so data feeders sit adjacent to their users;
		// pure chains ripple right over successive passes.
		{
			auto IsPure = [](const UEdGraphNode* N)
			{
				for (const UEdGraphPin* P : N->Pins)
				{
					if (P && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) { return false; }
				}
				return true;
			};
			bool bChanged = true;
			for (int32 Guard = 0; bChanged && Guard <= Nodes.Num(); ++Guard)
			{
				bChanged = false;
				for (UEdGraphNode* N : Nodes)
				{
					const TArray<UEdGraphNode*>* S = LayerSucc.Find(N);
					if (!S || S->Num() == 0 || !IsPure(N)) { continue; }
					int32 MinSucc = MAX_int32;
					for (UEdGraphNode* T : *S) { MinSucc = FMath::Min(MinSucc, Layer[T]); }
					if (MinSucc != MAX_int32 && MinSucc - 1 > Layer[N])
					{
						Layer[N] = MinSucc - 1;
						bChanged = true;
					}
				}
			}
		}

		// Components via undirected flood fill (for vertical band separation).
		TMap<UEdGraphNode*, int32> Comp;
		int32 NumComp = 0;
		for (UEdGraphNode* Start : Nodes)
		{
			if (Comp.Contains(Start))
			{
				continue;
			}
			const int32 Id = NumComp++;
			TQueue<UEdGraphNode*> BQ;
			BQ.Enqueue(Start);
			Comp.Add(Start, Id);
			while (!BQ.IsEmpty())
			{
				UEdGraphNode* Cur = nullptr;
				BQ.Dequeue(Cur);
				auto Visit = [&](UEdGraphNode* M)
				{
					if (M && !Comp.Contains(M))
					{
						Comp.Add(M, Id);
						BQ.Enqueue(M);
					}
				};
				if (const TArray<UEdGraphNode*>* S = Succ.Find(Cur)) { for (UEdGraphNode* M : *S) Visit(M); }
				if (const TArray<UEdGraphNode*>* Pp = Pred.Find(Cur)) { for (UEdGraphNode* M : *Pp) Visit(M); }
			}
		}

		// Rank components: those containing primary events (BeginPlay/Tick/input — real
		// entry points) first, then any-event components, then larger ones. A timer
		// callback Custom Event lives in its own component (the timer links by function
		// name, not a wire), and without the primary-event key a large callback body
		// out-ranked the small BeginPlay chain and was drawn above it (issue #354).
		TArray<int32> CompNodeCount;
		TArray<int32> CompEventCount;
		TArray<int32> CompPrimaryEventCount;
		CompNodeCount.Init(0, NumComp);
		CompEventCount.Init(0, NumComp);
		CompPrimaryEventCount.Init(0, NumComp);
		for (const TPair<UEdGraphNode*, int32>& Pair : Comp)
		{
			CompNodeCount[Pair.Value]++;
			if (Pair.Key->IsA<UK2Node_Event>() || Pair.Key->IsA<UK2Node_CustomEvent>())
			{
				CompEventCount[Pair.Value]++;
				if (!Pair.Key->IsA<UK2Node_CustomEvent>())
				{
					CompPrimaryEventCount[Pair.Value]++;
				}
			}
		}
		TArray<int32> CompOrder;
		for (int32 i = 0; i < NumComp; ++i)
		{
			CompOrder.Add(i);
		}
		CompOrder.Sort([&](int32 A, int32 B)
		{
			if (CompPrimaryEventCount[A] != CompPrimaryEventCount[B]) return CompPrimaryEventCount[A] > CompPrimaryEventCount[B];
			if (CompEventCount[A] != CompEventCount[B]) return CompEventCount[A] > CompEventCount[B];
			return CompNodeCount[A] > CompNodeCount[B];
		});
		TArray<int32> CompRank;
		CompRank.Init(0, NumComp);
		for (int32 i = 0; i < CompOrder.Num(); ++i)
		{
			CompRank[CompOrder[i]] = i;
		}

		// rank -> layer -> nodes, seeded in current visual order for determinism.
		TMap<int32, TMap<int32, TArray<UEdGraphNode*>>> Grid;
		for (UEdGraphNode* N : Nodes)
		{
			Grid.FindOrAdd(CompRank[Comp[N]]).FindOrAdd(Layer[N]).Add(N);
		}
		for (TPair<int32, TMap<int32, TArray<UEdGraphNode*>>>& RankPair : Grid)
		{
			for (TPair<int32, TArray<UEdGraphNode*>>& LayerPair : RankPair.Value)
			{
				LayerPair.Value.Sort([&VisitRank](const UEdGraphNode& A, const UEdGraphNode& B)
				{
					const int32* RA = VisitRank.Find(const_cast<UEdGraphNode*>(&A));
					const int32* RB = VisitRank.Find(const_cast<UEdGraphNode*>(&B));
					const int32 VA = RA ? *RA : MAX_int32;
					const int32 VB = RB ? *RB : MAX_int32;
					if (VA != VB) return VA < VB;
					return A.NodePosY < B.NodePosY;
				});
			}
		}

		// Index of each node within its layer.
		TMap<UEdGraphNode*, int32> Idx;
		for (TPair<int32, TMap<int32, TArray<UEdGraphNode*>>>& RankPair : Grid)
		{
			for (TPair<int32, TArray<UEdGraphNode*>>& LayerPair : RankPair.Value)
			{
				for (int32 i = 0; i < LayerPair.Value.Num(); ++i)
				{
					Idx.Add(LayerPair.Value[i], i);
				}
			}
		}

		// Median ordering sweeps (down by predecessors, up by successors) to cut crossings.
		for (int32 Sweep = 0; Sweep < 4; ++Sweep)
		{
			const bool bDown = (Sweep % 2) == 0;
			for (TPair<int32, TMap<int32, TArray<UEdGraphNode*>>>& RankPair : Grid)
			{
				TArray<int32> LayerKeys;
				RankPair.Value.GetKeys(LayerKeys);
				LayerKeys.Sort();
				if (!bDown)
				{
					for (int32 a = 0, b = LayerKeys.Num() - 1; a < b; ++a, --b) { LayerKeys.Swap(a, b); }
				}
				for (int32 LK : LayerKeys)
				{
					TArray<UEdGraphNode*>& LayerNodes = RankPair.Value[LK];
					TArray<TPair<float, UEdGraphNode*>> Keyed;
					Keyed.Reserve(LayerNodes.Num());
					for (UEdGraphNode* N : LayerNodes)
					{
						const TArray<UEdGraphNode*>* Adj = bDown ? Pred.Find(N) : Succ.Find(N);
						float Median = (float)Idx[N];
						if (Adj && Adj->Num() > 0)
						{
							TArray<int32> Positions;
							for (UEdGraphNode* M : *Adj)
							{
								if (const int32* PIdx = Idx.Find(M)) { Positions.Add(*PIdx); }
							}
							if (Positions.Num() > 0)
							{
								Positions.Sort();
								Median = (float)Positions[Positions.Num() / 2];
							}
						}
						Keyed.Add(TPair<float, UEdGraphNode*>(Median, N));
					}
					Keyed.StableSort([](const TPair<float, UEdGraphNode*>& A, const TPair<float, UEdGraphNode*>& B)
					{
						return A.Key < B.Key;
					});
					for (int32 i = 0; i < Keyed.Num(); ++i)
					{
						LayerNodes[i] = Keyed[i].Value;
						Idx[LayerNodes[i]] = i;
					}
				}
			}
		}

		// Assign positions: X by layer; Y straightened by aligning each node to the
		// median center of its connected neighbors (priority method) so chains and the
		// exec backbone stay horizontal instead of sloping. Independent components are
		// placed in stacked, non-overlapping bands.
		const float ColumnGap = 140.0f;      // clearance between a column's widest node and the next column
		const float MinColumnWidth = 280.0f; // floor so sparse columns don't collapse
		const float RowGap = 56.0f;
		const float ComponentGap = 160.0f;

		// Column X by cumulative measured widths (widest node per layer, across all bands so
		// bands stay column-aligned) instead of a fixed 420 px stride: skinny getter columns
		// pack tight, wide SpawnActor/Timeline columns get the room they render at (issue #427).
		TMap<int32, float> LayerMaxW;
		for (UEdGraphNode* N : Nodes)
		{
			float& W = LayerMaxW.FindOrAdd(Layer[N], MinColumnWidth);
			W = FMath::Max(W, EstimateNodeWidth(N));
		}
		TMap<int32, float> LayerX;
		{
			TArray<int32> AllLayers;
			LayerMaxW.GetKeys(AllLayers);
			AllLayers.Sort();
			float X = OriginX;
			for (int32 LK : AllLayers)
			{
				LayerX.Add(LK, X);
				X += LayerMaxW[LK] + ColumnGap;
			}
		}

		TArray<int32> Ranks;
		Grid.GetKeys(Ranks);
		Ranks.Sort();

		float BandTop = OriginY;
		for (int32 Rank : Ranks)
		{
			TMap<int32, TArray<UEdGraphNode*>>& Layers = Grid[Rank];

			TArray<int32> LayerKeys;
			Layers.GetKeys(LayerKeys);
			LayerKeys.Sort();

			// Band-relative Y, seeded by a simple top-aligned stack per column.
			TMap<UEdGraphNode*, float> Y;
			for (int32 LK : LayerKeys)
			{
				float Cursor = 0.0f;
				for (UEdGraphNode* N : Layers[LK])
				{
					Y.Add(N, Cursor);
					Cursor += EstimateNodeHeight(N) + RowGap;
				}
			}

			// Alignment sweeps: pull each node toward the median center-Y of its neighbors
			// on the already-placed side, then resolve overlaps in column order. Alternating
			// L->R / R->L converges to straight chains without a systematic downward drift.
			for (int32 Iter = 0; Iter < 6; ++Iter)
			{
				const bool bLeftToRight = (Iter % 2) == 0;
				TArray<int32> Order = LayerKeys;
				if (!bLeftToRight)
				{
					for (int32 a = 0, b = Order.Num() - 1; a < b; ++a, --b) { Order.Swap(a, b); }
				}
				for (int32 LK : Order)
				{
					TArray<UEdGraphNode*>& LayerNodes = Layers[LK];
					float Cursor = -FLT_MAX;
					for (UEdGraphNode* N : LayerNodes)
					{
						const float H = EstimateNodeHeight(N);
						// Align to the median center-Y of ALL neighbors (both sides) so leaf nodes
							// (e.g. a loop's Completed output) snap next to their single neighbor.
							TArray<float> Centers;
							// Prefer EXEC neighbors so the execution backbone stays straight; fall back
							// to data neighbors only for pure (exec-less) nodes like math/getters.
							auto Gather = [&](const TMap<UEdGraphNode*, TArray<UEdGraphNode*>>& AdjMap)
							{
								if (const TArray<UEdGraphNode*>* A = AdjMap.Find(N))
								{
									for (UEdGraphNode* M : *A)
									{
										if (const float* MY = Y.Find(M)) { Centers.Add(*MY + EstimateNodeHeight(M) * 0.5f); }
									}
								}
							};
							Gather(ExecPred);
							Gather(ExecSucc);
							if (Centers.Num() == 0) { Gather(Pred); Gather(Succ); }
							float DesiredTop = Y[N];
							if (Centers.Num() > 0)
							{
								Centers.Sort();
								DesiredTop = Centers[Centers.Num() / 2] - H * 0.5f;
							}
							const float Target = FMath::Max(DesiredTop, Cursor);
						Y[N] = Target;
						Cursor = Target + H + RowGap;
					}
				}
			}

			// Shift the band so its top sits at BandTop, commit positions, advance.
			float MinY = FLT_MAX;
			float MaxY = -FLT_MAX;
			for (const TPair<UEdGraphNode*, float>& YPair : Y)
			{
				MinY = FMath::Min(MinY, YPair.Value);
				MaxY = FMath::Max(MaxY, YPair.Value + EstimateNodeHeight(YPair.Key));
			}
			const float Shift = BandTop - MinY;
			for (int32 LK : LayerKeys)
			{
				for (UEdGraphNode* N : Layers[LK])
				{
					N->Modify();
					N->NodePosX = (int32)LayerX[LK];
					N->NodePosY = (int32)(Y[N] + Shift);
				}
			}

			BandTop += (MaxY - MinY) + ComponentGap;
		}

		return NumComp;
	}
} // namespace VibeUELayout


// ────────────────────────────────────────────────────────────────
// AutoLayoutSelectedNodes
// ────────────────────────────────────────────────────────────────


// ────────────────────────────────────────────────────────────────
// AnalyzeGraphLayout
// ────────────────────────────────────────────────────────────────

bool UBlueprintService::AnalyzeGraphLayout(
	const FString& BlueprintPath,
	const FString& GraphName,
	FString& OutReportJson,
	FString& OutError)
{
	OutReportJson.Empty();

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

	// Real nodes only — comment boxes are decoration, not graph structure.
	TArray<UEdGraphNode*> GNodes;
	for (UEdGraphNode* N : Graph->Nodes)
	{
		if (N && !N->IsA<UEdGraphNode_Comment>())
		{
			GNodes.Add(N);
		}
	}

	struct FNodeBox { float X, Y, W, H; };
	TMap<UEdGraphNode*, FNodeBox> Boxes;
	for (UEdGraphNode* N : GNodes)
	{
		Boxes.Add(N, { (float)N->NodePosX, (float)N->NodePosY,
			VibeUELayout::EstimateNodeWidth(N), VibeUELayout::EstimateNodeHeight(N) });
	}

	// Wires: one entry per link, walked from output pins only (no double counting).
	// Endpoints use the same estimated geometry model as the layout itself.
	struct FWire { UEdGraphNode* From; UEdGraphNode* To; FVector2D A; FVector2D B; bool bExec; };
	TArray<FWire> Wires;
	for (UEdGraphNode* N : GNodes)
	{
		for (UEdGraphPin* P : N->Pins)
		{
			if (!P || P->Direction != EGPD_Output)
			{
				continue;
			}
			for (UEdGraphPin* L : P->LinkedTo)
			{
				UEdGraphNode* T = L ? L->GetOwningNode() : nullptr;
				if (!T || T == N || !Boxes.Contains(T))
				{
					continue;
				}
				const FNodeBox& SB = Boxes[N];
				const FNodeBox& TB = Boxes[T];
				FWire W;
				W.From = N;
				W.To = T;
				W.A = FVector2D(SB.X + SB.W, SB.Y + VibeUELayout::PinRowCenterY(N, P));
				W.B = FVector2D(TB.X, TB.Y + VibeUELayout::PinRowCenterY(T, L));
				W.bExec = (P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
				Wires.Add(W);
			}
		}
	}

	auto NodeLabel = [](const UEdGraphNode* N)
	{
		return N->GetNodeTitle(ENodeTitleType::ListView).ToString();
	};

	// Node overlaps (boxes shrunk 2 px so mere edge-touching doesn't count).
	int32 OverlapCount = 0;
	TArray<TSharedPtr<FJsonValue>> OverlapList;
	for (int32 i = 0; i < GNodes.Num(); ++i)
	{
		const FNodeBox& A = Boxes[GNodes[i]];
		for (int32 j = i + 1; j < GNodes.Num(); ++j)
		{
			const FNodeBox& B = Boxes[GNodes[j]];
			const bool bOverlap =
				A.X + 2.0f < B.X + B.W - 2.0f && B.X + 2.0f < A.X + A.W - 2.0f &&
				A.Y + 2.0f < B.Y + B.H - 2.0f && B.Y + 2.0f < A.Y + A.H - 2.0f;
			if (bOverlap)
			{
				++OverlapCount;
				if (OverlapList.Num() < 50)
				{
					OverlapList.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%s <-> %s"),
						*NodeLabel(GNodes[i]), *NodeLabel(GNodes[j]))));
				}
			}
		}
	}

	// Wire crossings — straight-line approximation; wires sharing a node are excluded
	// (fan-out from one pin always "touches" at the source and is not a readability defect).
	int32 Crossings = 0;
	for (int32 i = 0; i < Wires.Num(); ++i)
	{
		for (int32 j = i + 1; j < Wires.Num(); ++j)
		{
			const FWire& W1 = Wires[i];
			const FWire& W2 = Wires[j];
			if (W1.From == W2.From || W1.From == W2.To || W1.To == W2.From || W1.To == W2.To)
			{
				continue;
			}
			if (VibeUELayout::SegmentsIntersect(W1.A, W1.B, W2.A, W2.B))
			{
				++Crossings;
			}
		}
	}

	// Backward wires, lengths, exec straightness.
	int32 ExecWireCount = 0;
	int32 BackwardExec = 0;
	TArray<TSharedPtr<FJsonValue>> BackwardList;
	TArray<TSharedPtr<FJsonValue>> LongWireList;
	double TotalLen = 0.0;
	double ExecAbsDy = 0.0;
	int32 LongWires = 0;
	for (const FWire& W : Wires)
	{
		const double Len = FVector2D::Distance(W.A, W.B);
		TotalLen += Len;
		if (Len > 1500.0)
		{
			++LongWires;
			if (LongWireList.Num() < 50)
			{
				LongWireList.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%s -> %s (%d px)"),
					*NodeLabel(W.From), *NodeLabel(W.To), (int32)Len)));
			}
		}
		if (W.bExec)
		{
			++ExecWireCount;
			ExecAbsDy += FMath::Abs(W.B.Y - W.A.Y);
			if (W.B.X < W.A.X)
			{
				++BackwardExec;
				if (BackwardList.Num() < 50)
				{
					BackwardList.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%s -> %s"),
						*NodeLabel(W.From), *NodeLabel(W.To))));
				}
			}
		}
	}

	// Graph bounds.
	float MinX = 0.0f, MinY = 0.0f, MaxX = 0.0f, MaxY = 0.0f;
	bool bFirst = true;
	for (const TPair<UEdGraphNode*, FNodeBox>& Pair : Boxes)
	{
		const FNodeBox& B = Pair.Value;
		MinX = bFirst ? B.X : FMath::Min(MinX, B.X);
		MinY = bFirst ? B.Y : FMath::Min(MinY, B.Y);
		MaxX = bFirst ? B.X + B.W : FMath::Max(MaxX, B.X + B.W);
		MaxY = bFirst ? B.Y + B.H : FMath::Max(MaxY, B.Y + B.H);
		bFirst = false;
	}

	// Human/agent-readable issue summary — empty array means the layout looks clean.
	TArray<TSharedPtr<FJsonValue>> Issues;
	if (OverlapCount > 0)
	{
		Issues.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%d overlapping node pair(s)"), OverlapCount)));
	}
	if (BackwardExec > 0)
	{
		Issues.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%d backward exec wire(s) — execution flows right-to-left somewhere"), BackwardExec)));
	}
	if (Wires.Num() > 0 && Crossings > Wires.Num())
	{
		Issues.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("wire crossings (%d) exceed wire count (%d) — graph is hard to trace"), Crossings, Wires.Num())));
	}
	if (LongWires > 0)
	{
		Issues.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%d wire(s) longer than 1500 px — consider moving producers next to consumers"), LongWires)));
	}

	// Per-node bounding boxes for model auditing.
	TArray<TSharedPtr<FJsonValue>> NodeArr;
	for (UEdGraphNode* N : GNodes)
	{
		const FNodeBox& B = Boxes[N];
		TSharedPtr<FJsonObject> NObj = MakeShared<FJsonObject>();
		NObj->SetStringField(TEXT("id"), N->NodeGuid.ToString());
		NObj->SetStringField(TEXT("title"), NodeLabel(N));
		NObj->SetNumberField(TEXT("x"), B.X);
		NObj->SetNumberField(TEXT("y"), B.Y);
		NObj->SetNumberField(TEXT("width"), B.W);
		NObj->SetNumberField(TEXT("height"), B.H);
		NodeArr.Add(MakeShared<FJsonValueObject>(NObj));
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("nodeCount"), GNodes.Num());
	Root->SetNumberField(TEXT("wireCount"), Wires.Num());
	Root->SetNumberField(TEXT("execWireCount"), ExecWireCount);
	Root->SetNumberField(TEXT("nodeOverlaps"), OverlapCount);
	Root->SetArrayField(TEXT("overlappingPairs"), OverlapList);
	Root->SetNumberField(TEXT("wireCrossings"), Crossings);
	Root->SetNumberField(TEXT("backwardExecWires"), BackwardExec);
	Root->SetArrayField(TEXT("backwardExecWireList"), BackwardList);
	Root->SetNumberField(TEXT("longWires"), LongWires);
	Root->SetArrayField(TEXT("longWireList"), LongWireList);
	Root->SetNumberField(TEXT("totalWireLength"), FMath::RoundToDouble(TotalLen));
	Root->SetNumberField(TEXT("avgWireLength"), Wires.Num() > 0 ? FMath::RoundToDouble(TotalLen / Wires.Num()) : 0.0);
	Root->SetNumberField(TEXT("execWireMeanAbsDeltaY"), ExecWireCount > 0 ? FMath::RoundToDouble(ExecAbsDy / ExecWireCount) : 0.0);
	{
		TSharedPtr<FJsonObject> BoundsObj = MakeShared<FJsonObject>();
		BoundsObj->SetNumberField(TEXT("minX"), MinX);
		BoundsObj->SetNumberField(TEXT("minY"), MinY);
		BoundsObj->SetNumberField(TEXT("maxX"), MaxX);
		BoundsObj->SetNumberField(TEXT("maxY"), MaxY);
		Root->SetObjectField(TEXT("bounds"), BoundsObj);
	}
	Root->SetArrayField(TEXT("issues"), Issues);
	Root->SetArrayField(TEXT("nodes"), NodeArr);
	Root->SetStringField(TEXT("note"), TEXT("Sizes are estimated widget dimensions (same model auto-layout uses); crossings use straight-line wire approximation."));

	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutReportJson);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	return true;
}

// ────────────────────────────────────────────────────────────────
// GetGraphDefinition
// ────────────────────────────────────────────────────────────────


bool UBlueprintService::AddInterface(
	const FString& BlueprintPath,
	const FString& InterfacePath)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddInterface: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	if (InterfacePath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("AddInterface: Interface path is empty"));
		return false;
	}

	// Resolve interface class - try multiple strategies
	UClass* InterfaceClass = nullptr;
	// Native interfaces (/Script/...) are classes, not Blueprint assets; load them directly.
	if (InterfacePath.StartsWith(TEXT("/Script/")))
	{
		InterfaceClass = LoadClass<UInterface>(nullptr, *InterfacePath);
	}

	// Strategy 1: Try loading as a Blueprint asset path
	UBlueprint* InterfaceBP = InterfaceClass ? nullptr : Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *InterfacePath));
	if (InterfaceBP)
	{
		InterfaceClass = InterfaceBP->GeneratedClass;
	}

	// Strategy 2: Try with _C suffix as a class path
	if (!InterfaceClass)
	{
		FString ClassPath = InterfacePath;
		if (!ClassPath.EndsWith(TEXT("_C")))
		{
			ClassPath = InterfacePath + TEXT(".") + FPaths::GetCleanFilename(InterfacePath) + TEXT("_C");
		}
		InterfaceClass = LoadClass<UObject>(nullptr, *ClassPath);
	}

	// Strategy 3: Search by short name across all loaded Blueprint assets
	if (!InterfaceClass)
	{
		for (TObjectIterator<UBlueprint> It; It; ++It)
		{
			if (It->GetName().Equals(InterfacePath, ESearchCase::IgnoreCase) ||
				It->GetName().Equals(InterfacePath.Replace(TEXT("/"), TEXT("")), ESearchCase::IgnoreCase))
			{
				if (It->BlueprintType == BPTYPE_Interface)
				{
					InterfaceClass = It->GeneratedClass;
					UE_LOG(LogTemp, Log, TEXT("AddInterface: Resolved interface '%s' via object search to '%s'"), *InterfacePath, *It->GetPathName());
					break;
				}
			}
		}
	}

	if (!InterfaceClass)
	{
		UE_LOG(LogTemp, Error, TEXT("AddInterface: Interface '%s' not found. Provide the full asset path (e.g., /Game/interface/BPI_TestInterface)"), *InterfacePath);
		return false;
	}

	// The resolved class must actually be an interface. Implementing a non-interface
	// class and then compiling trips an engine assertion in the Kismet compiler
	// (Interface->HasAnyClassFlags(CLASS_Interface)), which crashes the editor.
	// Reject it here instead.
	if (!InterfaceClass->HasAnyClassFlags(CLASS_Interface))
	{
		UE_LOG(LogTemp, Error, TEXT("AddInterface: '%s' resolves to '%s', which is not a Blueprint Interface. Provide a Blueprint Interface asset."),
			*InterfacePath, *InterfaceClass->GetName());
		return false;
	}

	// Check if interface is already implemented
	for (const FBPInterfaceDescription& Desc : Blueprint->ImplementedInterfaces)
	{
		if (Desc.Interface == InterfaceClass)
		{
			UE_LOG(LogTemp, Log, TEXT("AddInterface: Interface '%s' is already implemented on '%s'"), *InterfaceClass->GetName(), *Blueprint->GetName());
			return true;
		}
	}

	// Add the interface
	FTopLevelAssetPath InterfaceAssetPath = InterfaceClass->GetClassPathName();
	FBlueprintEditorUtils::ImplementNewInterface(Blueprint, InterfaceAssetPath);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("AddInterface: Added interface '%s' to '%s'"),
		*InterfaceClass->GetName(), *Blueprint->GetName());

	return true;
}

bool UBlueprintService::RemoveInterface(
	const FString& BlueprintPath,
	const FString& InterfacePath)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveInterface: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	if (InterfacePath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveInterface: Interface path is empty"));
		return false;
	}

	// Find the interface in the implemented list
	UClass* InterfaceClass = nullptr;
	int32 FoundIndex = INDEX_NONE;

	for (int32 i = 0; i < Blueprint->ImplementedInterfaces.Num(); ++i)
	{
		const FBPInterfaceDescription& Desc = Blueprint->ImplementedInterfaces[i];
		if (Desc.Interface)
		{
			FString InterfaceName = Desc.Interface->GetName();
			FString InterfacePkgPath = Desc.Interface->GetPathName();

			if (InterfaceName.Equals(InterfacePath, ESearchCase::IgnoreCase) ||
				InterfaceName.Equals(InterfacePath + TEXT("_C"), ESearchCase::IgnoreCase) ||
				InterfacePkgPath.Contains(InterfacePath))
			{
				InterfaceClass = Desc.Interface;
				FoundIndex = i;
				break;
			}
		}
	}

	if (FoundIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveInterface: Interface '%s' not found on blueprint '%s'"), *InterfacePath, *Blueprint->GetName());
		return false;
	}

	// Remove the interface
	FTopLevelAssetPath InterfaceAssetPath = InterfaceClass->GetClassPathName();
	FBlueprintEditorUtils::RemoveInterface(Blueprint, InterfaceAssetPath);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("RemoveInterface: Removed interface '%s' from '%s'"),
		*InterfaceClass->GetName(), *Blueprint->GetName());

	return true;
}
