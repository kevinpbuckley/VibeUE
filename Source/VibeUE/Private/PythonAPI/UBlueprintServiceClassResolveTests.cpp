// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Guid.h"
#include "PythonAPI/UBlueprintService.h"
#include "PythonAPI/BlueprintClassResolve.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "WidgetBlueprint.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"

#if WITH_AUTOMATION_TESTS

// Regression coverage for BlueprintService bug B6 (ZombieGame round 4, 2026-10-23):
//
//  B6  Short-name class resolution never found Widget or Anim Blueprints. ResolveClassByName's
//      loaded lookup used FindFirstObject<UClass>(..., ExactClass), which only matches native
//      classes, and its asset-registry fallback ran GetAssetsByClass(UBlueprint) without
//      bSearchSubClasses, so only plain UBlueprint assets matched. Every caller that takes a class
//      name (create_node_by_key FUNC keys, add_delegate_bind_node, build_graph nodes, class pin
//      defaults) refused "WBP_Menu_C" while accepting the full generated-class path.
//
// Every test except ShortNameAmbiguityIsRefused drives the PUBLIC service API only (CreateNodeByKey),
// so it fails on the unfixed plugin at the assertion named in its comment. The ambiguity test calls
// the policy function (BlueprintClassResolve.h) with synthetic candidates and is new-behaviour only.
// Throwaway Blueprints are never saved. Ones the service must load by path live in-memory under
// /Game/__VibeUETest and are registered with AssetCreated (the only way UEditorAssetLibrary can load
// an unsaved asset); the rest live in GetTransientPackage(). Every name carries a GUID suffix, so no
// two Blueprints in memory ever share an object name (two same-named Blueprints created in memory in
// /Temp packages crashed the headless host inside FKismetEditorUtilities::CreateBlueprint, 2026-10-23).

namespace VibeClassResolveTestUtil
{
	static FString UniqueSuffix()
	{
		return FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(12);
	}

	static UBlueprint* MakeBlueprintIn(UObject* Outer, const FString& Name, UClass* ParentClass,
		TSubclassOf<UBlueprint> BlueprintClass, TSubclassOf<UBlueprintGeneratedClass> GeneratedClass)
	{
		// CreateBlueprint check()s that the name is free in Outer; refuse here instead of asserting.
		if (!Outer || !ParentClass || FindObject<UObject>(Outer, *Name))
		{
			return nullptr;
		}
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass, Outer, FName(*Name), BPTYPE_Normal, BlueprintClass, GeneratedClass);
		if (UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Blueprint))
		{
			// No skeleton asset is needed: a template Anim Blueprint compiles without one (the Anim
			// Blueprint factory's "template" option does exactly this).
			AnimBlueprint->bIsTemplate = true;
			AnimBlueprint->TargetSkeleton = nullptr;
		}
		if (Blueprint)
		{
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
		}
		return Blueprint;
	}

	static UBlueprint* MakeActorBlueprintIn(UObject* Outer, const FString& Name)
	{
		return MakeBlueprintIn(Outer, Name, AActor::StaticClass(), UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	}

	static UBlueprint* MakeWidgetBlueprintIn(UObject* Outer, const FString& Name)
	{
		return MakeBlueprintIn(Outer, Name, UUserWidget::StaticClass(), UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass());
	}

	static UBlueprint* MakeAnimBlueprintIn(UObject* Outer, const FString& Name)
	{
		return MakeBlueprintIn(Outer, Name, UAnimInstance::StaticClass(), UAnimBlueprint::StaticClass(), UAnimBlueprintGeneratedClass::StaticClass());
	}

	// In-memory /Game package registered with the asset registry, so the service can load it by path.
	static UBlueprint* MakeRegistered(const FString& PackageName, UBlueprint* (*Maker)(UObject*, const FString&))
	{
		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			return nullptr;
		}
		UBlueprint* Blueprint = Maker(Package, FPackageName::GetShortName(PackageName));
		if (Blueprint)
		{
			FAssetRegistryModule::AssetCreated(Blueprint);
		}
		return Blueprint;
	}

	// Unregister (when registered) and drop the standalone flags so GC reclaims the never-saved asset.
	static void Release(UBlueprint* Blueprint, bool bRegistered)
	{
		if (Blueprint)
		{
			if (bRegistered)
			{
				FAssetRegistryModule::AssetDeleted(Blueprint);
			}
			Blueprint->ClearFlags(RF_Standalone | RF_Public);
			// Never-saved test package: leave it clean so no later auto-save sweep writes it.
			Blueprint->GetPackage()->SetDirtyFlag(false);
		}
	}

	// Short-name resolution through the public API: "FUNC <Class>::<Function>" resolves <Class>
	// with ResolveClassByName, then finds the (inherited) function and places a call node.
	static bool PlacesCallNode(const FString& HostPath, const FString& ClassName, const TCHAR* FunctionName)
	{
		const FString Key = FString::Printf(TEXT("FUNC %s::%s"), *ClassName, FunctionName);
		return !UBlueprintService::CreateNodeByKey(HostPath, TEXT("EventGraph"), Key, 0.0f, 0.0f).IsEmpty();
	}
}

// B6, registered (asset-registry-visible) Blueprints. On 5-8 the plain Actor Blueprint is the
// passing control (the old registry fallback finds plain UBlueprint assets); the FIRST failing
// assertion is "Widget Blueprint resolves by short name with _C".
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBlueprintServiceShortNameWidgetAnimTest, "VibeUE.BlueprintService.ShortNameResolvesWidgetAndAnimBlueprints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeBlueprintServiceShortNameWidgetAnimTest::RunTest(const FString&)
{
	using namespace VibeClassResolveTestUtil;

	const FString Suffix = UniqueSuffix();
	const FString HostPath = FString::Printf(TEXT("/Game/__VibeUETest/BP_B6Host_%s"), *Suffix);
	const FString ActorName = FString::Printf(TEXT("BP_B6Actor_%s"), *Suffix);
	const FString WidgetName = FString::Printf(TEXT("WBP_B6Widget_%s"), *Suffix);
	const FString AnimName = FString::Printf(TEXT("ABP_B6Anim_%s"), *Suffix);

	UBlueprint* Host = MakeRegistered(HostPath, &MakeActorBlueprintIn);
	UBlueprint* ActorBP = MakeRegistered(FString::Printf(TEXT("/Game/__VibeUETest/%s"), *ActorName), &MakeActorBlueprintIn);
	UBlueprint* WidgetBP = MakeRegistered(FString::Printf(TEXT("/Game/__VibeUETest/%s"), *WidgetName), &MakeWidgetBlueprintIn);
	UBlueprint* AnimBP = MakeRegistered(FString::Printf(TEXT("/Game/__VibeUETest/%s"), *AnimName), &MakeAnimBlueprintIn);
	ON_SCOPE_EXIT
	{
		Release(Host, true);
		Release(ActorBP, true);
		Release(WidgetBP, true);
		Release(AnimBP, true);
	};

	if (!TestNotNull(TEXT("created the host Blueprint"), Host) ||
		!TestNotNull(TEXT("created the Actor Blueprint"), ActorBP) ||
		!TestNotNull(TEXT("created the Widget Blueprint"), WidgetBP) ||
		!TestNotNull(TEXT("created the Anim Blueprint"), AnimBP))
	{
		return false;
	}
	TestTrue(TEXT("Widget Blueprint compiled to a UWidgetBlueprintGeneratedClass"), Cast<UWidgetBlueprintGeneratedClass>(WidgetBP->GeneratedClass.Get()) != nullptr);
	TestTrue(TEXT("Anim Blueprint compiled to a UAnimBlueprintGeneratedClass"), Cast<UAnimBlueprintGeneratedClass>(AnimBP->GeneratedClass.Get()) != nullptr);

	// (a) Control: a plain Actor Blueprint by short name (passes before the fix too).
	TestTrue(TEXT("Actor Blueprint resolves by short name with _C"), PlacesCallNode(HostPath, ActorName + TEXT("_C"), TEXT("K2_DestroyActor")));

	// (b) Widget Blueprint: the first assertion that fails on 5-8.
	TestTrue(TEXT("Widget Blueprint resolves by short name with _C"), PlacesCallNode(HostPath, WidgetName + TEXT("_C"), TEXT("RemoveFromParent")));
	TestTrue(TEXT("Widget Blueprint resolves by short name without _C"), PlacesCallNode(HostPath, WidgetName, TEXT("RemoveFromParent")));

	// (c) Anim Blueprint, skeleton-less template.
	TestTrue(TEXT("Anim Blueprint resolves by short name with _C"), PlacesCallNode(HostPath, AnimName + TEXT("_C"), TEXT("TryGetPawnOwner")));

	// Full generated-class path is unchanged.
	TestTrue(TEXT("Widget Blueprint still resolves by its full generated-class path"),
		PlacesCallNode(HostPath, FString::Printf(TEXT("/Game/__VibeUETest/%s.%s_C"), *WidgetName, *WidgetName), TEXT("RemoveFromParent")));

	return true;
}

// B6, loaded-class path: Blueprints in GetTransientPackage() are invisible to the asset registry, so
// only a loaded-class lookup that accepts Blueprint generated classes can find them. On 5-8 the
// FIRST failing assertion is "transient Actor Blueprint resolves by short name" (ExactClass skips
// every UBlueprintGeneratedClass, plain ones included).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBlueprintServiceShortNameLoadedTest, "VibeUE.BlueprintService.ShortNameResolvesLoadedTransientBlueprints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeBlueprintServiceShortNameLoadedTest::RunTest(const FString&)
{
	using namespace VibeClassResolveTestUtil;

	const FString Suffix = UniqueSuffix();
	const FString HostPath = FString::Printf(TEXT("/Game/__VibeUETest/BP_B6LoadedHost_%s"), *Suffix);
	const FString ActorName = FString::Printf(TEXT("BP_B6TransientActor_%s"), *Suffix);
	const FString WidgetName = FString::Printf(TEXT("WBP_B6TransientWidget_%s"), *Suffix);
	const FString AnimName = FString::Printf(TEXT("ABP_B6TransientAnim_%s"), *Suffix);

	UBlueprint* Host = MakeRegistered(HostPath, &MakeActorBlueprintIn);
	UBlueprint* ActorBP = MakeActorBlueprintIn(GetTransientPackage(), ActorName);
	UBlueprint* WidgetBP = MakeWidgetBlueprintIn(GetTransientPackage(), WidgetName);
	UBlueprint* AnimBP = MakeAnimBlueprintIn(GetTransientPackage(), AnimName);
	ON_SCOPE_EXIT
	{
		Release(Host, true);
		Release(ActorBP, false);
		Release(WidgetBP, false);
		Release(AnimBP, false);
	};

	if (!TestNotNull(TEXT("created the host Blueprint"), Host) ||
		!TestNotNull(TEXT("created the transient Actor Blueprint"), ActorBP) ||
		!TestNotNull(TEXT("created the transient Widget Blueprint"), WidgetBP) ||
		!TestNotNull(TEXT("created the transient Anim Blueprint"), AnimBP))
	{
		return false;
	}

	TestTrue(TEXT("transient Actor Blueprint resolves by short name"), PlacesCallNode(HostPath, ActorName + TEXT("_C"), TEXT("K2_DestroyActor")));
	TestTrue(TEXT("transient Widget Blueprint resolves by short name"), PlacesCallNode(HostPath, WidgetName + TEXT("_C"), TEXT("RemoveFromParent")));
	TestTrue(TEXT("transient Anim Blueprint resolves by short name"), PlacesCallNode(HostPath, AnimName + TEXT("_C"), TEXT("TryGetPawnOwner")));
	return true;
}

// B6 ambiguity policy, on synthetic candidates. Creating two same-named Blueprints in memory to get
// real candidates crashed the headless host (EXCEPTION_ACCESS_VIOLATION inside the SECOND
// FKismetEditorUtilities::CreateBlueprint, 2026-10-23), so the policy function is driven directly:
// no Blueprint is created and nothing is loaded (no candidate carries a valid FAssetData).
// Two packages sharing a short name are refused with one Warning listing both paths, sorted, with
// no /Game preference; one loaded candidate returns its class; none returns nullptr. Compiles only
// with the fix (the policy function is new).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBlueprintServiceShortNameAmbiguityTest, "VibeUE.BlueprintService.ShortNameAmbiguityIsRefused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeBlueprintServiceShortNameAmbiguityTest::RunTest(const FString&)
{
	using VibeBlueprintClassResolve::FCandidate;

	const FString ShortName = TEXT("BP_B6Dup");

	FCandidate GameCandidate;
	GameCandidate.PackageName = FName(TEXT("/Game/VibeB6DupA/BP_B6Dup"));
	GameCandidate.ObjectPath = TEXT("/Game/VibeB6DupA/BP_B6Dup.BP_B6Dup");

	FCandidate EngineCandidate;
	EngineCandidate.PackageName = FName(TEXT("/Engine/VibeB6DupB/BP_B6Dup"));
	EngineCandidate.ObjectPath = TEXT("/Engine/VibeB6DupB/BP_B6Dup.BP_B6Dup");

	// /Game is listed first on purpose: the Warning must sort, and must not prefer /Game.
	AddExpectedMessagePlain(
		TEXT("short name 'BP_B6Dup' is ambiguous, 2 Blueprints match: /Engine/VibeB6DupB/BP_B6Dup.BP_B6Dup, /Game/VibeB6DupA/BP_B6Dup.BP_B6Dup"),
		ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	TestNull(TEXT("two packages sharing a short name are refused"),
		VibeBlueprintClassResolve::ChooseCandidate(ShortName, { GameCandidate, EngineCandidate }));

	FCandidate LoadedCandidate = GameCandidate;
	LoadedCandidate.Class = AActor::StaticClass();
	TestTrue(TEXT("a single loaded candidate returns its class"),
		VibeBlueprintClassResolve::ChooseCandidate(ShortName, { LoadedCandidate }) == AActor::StaticClass());

	TestNull(TEXT("no candidate resolves to nothing"),
		VibeBlueprintClassResolve::ChooseCandidate(ShortName, TArray<FCandidate>()));

	TestNull(TEXT("a single candidate with neither a class nor a valid asset resolves to nothing (and loads nothing)"),
		VibeBlueprintClassResolve::ChooseCandidate(ShortName, { GameCandidate }));
	return true;
}

#endif // WITH_AUTOMATION_TESTS
