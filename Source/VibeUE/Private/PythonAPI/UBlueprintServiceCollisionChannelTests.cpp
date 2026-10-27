// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "PythonAPI/UBlueprintService.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/Package.h"

#if WITH_AUTOMATION_TESTS

// Regression coverage for set_collision_settings custom channels (ZombieGame round 4, 2026-10-23).
// The service understood only the 8 engine channel names: a project's custom channel names
// ("Weapon") and the ECC_GameTraceChannelN spellings were skipped with a Warning while the call
// still returned true, and the template change never reached placed actors (the compile reinstanced
// them with the stale value as a per-instance override). Drives the public API only, so it compiles
// against the unfixed plugin. The Blueprint lives in-memory under /Game/__VibeUETest, registered with
// AssetCreated so LoadBlueprint can find it, never saved; the world is a throwaway in an
// RF_Transient /Temp package.

namespace VibeCollisionTestUtil
{
	static FString UniqueSuffix()
	{
		return FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(12);
	}

	static UBlueprint* MakeActorBlueprintIn(UObject* Outer, const FString& Name)
	{
		// CreateBlueprint check()s that the name is free in Outer; refuse here instead of asserting.
		if (!Outer || FindObject<UObject>(Outer, *Name))
		{
			return nullptr;
		}
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(), Outer, FName(*Name), BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		if (Blueprint)
		{
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
		}
		return Blueprint;
	}

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

	static UPackage* MakeTransientTempPackage(const FString& Name)
	{
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/%s"), *Name));
		if (Package)
		{
			Package->SetFlags(RF_Transient);
		}
		return Package;
	}

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
}

// Collision: custom channel spellings, the unknown-channel refusal, and propagation to placed
// actors that did not override the template. On 5-8 the FIRST failing assertion is
// "template GameTraceChannel1 response is Ignore" (ECC_GameTraceChannel1 was skipped as unknown).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBlueprintServiceCollisionCustomChannelTest, "VibeUE.BlueprintService.SetCollisionSettingsAcceptsCustomChannels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeBlueprintServiceCollisionCustomChannelTest::RunTest(const FString&)
{
	using namespace VibeCollisionTestUtil;

	const FString Suffix = UniqueSuffix();
	const FString Path = FString::Printf(TEXT("/Game/__VibeUETest/BP_CollisionChannels_%s"), *Suffix);
	const FString ComponentName = TEXT("VibeCollisionMesh");

	UBlueprint* Blueprint = MakeRegistered(Path, &MakeActorBlueprintIn);
	ON_SCOPE_EXIT { Release(Blueprint, true); };
	if (!TestNotNull(TEXT("created the test Blueprint"), Blueprint) ||
		!TestTrue(TEXT("added a StaticMeshComponent"), UBlueprintService::AddComponent(Path, TEXT("StaticMeshComponent"), ComponentName, TEXT(""))))
	{
		return false;
	}
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	auto FindTemplate = [&]() -> UPrimitiveComponent*
	{
		if (!Blueprint->SimpleConstructionScript)
		{
			return nullptr;
		}
		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->GetVariableName().ToString() == ComponentName)
			{
				return Cast<UPrimitiveComponent>(Node->ComponentTemplate.Get());
			}
		}
		return nullptr;
	};
	UPrimitiveComponent* Template = FindTemplate();
	if (!TestNotNull(TEXT("found the component template"), Template))
	{
		return false;
	}

	// A throwaway world with two placed instances: one untouched, one that overrides Camera.
	UPackage* WorldPackage = MakeTransientTempPackage(FString::Printf(TEXT("VibeCollisionWorld_%s"), *Suffix));
	UWorld* World = WorldPackage ? UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false, FName(*FString::Printf(TEXT("VibeCollisionWorld_%s"), *Suffix)), WorldPackage) : nullptr;
	ON_SCOPE_EXIT
	{
		if (World)
		{
			World->DestroyWorld(/*bInformEngineOfWorld*/ false);
			World->RemoveFromRoot();
		}
		if (WorldPackage)
		{
			WorldPackage->SetDirtyFlag(false);
		}
	};
	if (!TestNotNull(TEXT("created the throwaway world"), World))
	{
		return false;
	}

	const FName UntouchedTag(*FString::Printf(TEXT("VibeUntouched_%s"), *Suffix));
	const FName OverrideTag(*FString::Printf(TEXT("VibeOverride_%s"), *Suffix));
	AActor* Untouched = World->SpawnActor<AActor>(Blueprint->GeneratedClass);
	AActor* Overridden = World->SpawnActor<AActor>(Blueprint->GeneratedClass);
	if (!TestNotNull(TEXT("spawned the untouched instance"), Untouched) || !TestNotNull(TEXT("spawned the overriding instance"), Overridden))
	{
		return false;
	}
	Untouched->Tags.Add(UntouchedTag);
	Overridden->Tags.Add(OverrideTag);

	auto FindInstanceComponent = [&](FName Tag) -> UPrimitiveComponent*
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (IsValid(*It) && It->Tags.Contains(Tag))
			{
				TInlineComponentArray<UPrimitiveComponent*> Components(*It);
				for (UPrimitiveComponent* Component : Components)
				{
					if (Component && Component->GetName() == ComponentName)
					{
						return Component;
					}
				}
			}
		}
		return nullptr;
	};
	UPrimitiveComponent* OverriddenComponent = FindInstanceComponent(OverrideTag);
	if (!TestNotNull(TEXT("overriding instance has the SCS component"), OverriddenComponent) ||
		!TestTrue(TEXT("untouched instance has the SCS component"), FindInstanceComponent(UntouchedTag) != nullptr))
	{
		return false;
	}

	// 0. Profile-only change. The untouched instance must end on the NEW profile name, not on
	// "Custom": a redundant object-type/response write after the profile write would invalidate the
	// profile name and turn the instance into a permanent override.
	{
		const FName TargetProfile = Template->GetCollisionProfileName() == FName(TEXT("BlockAll"))
			? FName(TEXT("OverlapAllDynamic")) : FName(TEXT("BlockAll"));
		TestTrue(FString::Printf(TEXT("set_collision_settings applies profile '%s' alone"), *TargetProfile.ToString()),
			UBlueprintService::SetCollisionSettings(Path, ComponentName, TEXT(""), TEXT(""), TargetProfile.ToString(), TMap<FString, FString>()));
		Template = FindTemplate();
		if (!TestNotNull(TEXT("component template present after the profile change"), Template))
		{
			return false;
		}
		TestEqual(TEXT("template carries the new profile"), Template->GetCollisionProfileName().ToString(), TargetProfile.ToString());
		if (UPrimitiveComponent* UntouchedAfterProfile = FindInstanceComponent(UntouchedTag))
		{
			TestEqual(TEXT("untouched placed instance follows the profile (not Custom)"),
				UntouchedAfterProfile->GetCollisionProfileName().ToString(), Template->GetCollisionProfileName().ToString());
		}
		else
		{
			AddError(TEXT("untouched placed instance's component disappeared after the profile change"));
		}
	}

	// Profile responses inherit the host project's custom-channel defaults (Interaction may
	// already Ignore GameTraceChannel1). Seed only our fixture after the profile-only check so
	// the writes below exercise real transitions, with one matching instance and one override.
	UPrimitiveComponent* UntouchedBeforeWrite = FindInstanceComponent(UntouchedTag);
	OverriddenComponent = FindInstanceComponent(OverrideTag);
	if (!TestNotNull(TEXT("untouched instance present before seeding responses"), UntouchedBeforeWrite) ||
		!TestNotNull(TEXT("overriding instance present before seeding responses"), OverriddenComponent))
	{
		return false;
	}
	FCollisionResponseContainer InitialResponses = Template->GetCollisionResponseToChannels();
	InitialResponses.SetResponse(ECC_GameTraceChannel1, ECR_Block);
	InitialResponses.SetResponse(ECC_Visibility, ECR_Overlap);
	InitialResponses.SetResponse(ECC_Camera, ECR_Block);
	Template->SetCollisionResponseToChannels(InitialResponses);
	UntouchedBeforeWrite->SetCollisionResponseToChannels(InitialResponses);
	OverriddenComponent->SetCollisionResponseToChannels(InitialResponses);
	OverriddenComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Overlap);
	FCollisionResponseContainer OverrideResponses = InitialResponses;
	OverrideResponses.SetResponse(ECC_Camera, ECR_Overlap);
	TestTrue(TEXT("precondition: the template has the seeded response table"),
		Template->GetCollisionResponseToChannels() == InitialResponses);
	TestTrue(TEXT("precondition: untouched placed instance matches the seeded template"),
		UntouchedBeforeWrite->GetCollisionResponseToChannels() == InitialResponses);
	TestTrue(TEXT("precondition: overriding placed instance differs only on Camera"),
		OverriddenComponent->GetCollisionResponseToChannels() == OverrideResponses);

	const ECollisionResponse OldChannel1 = Template->GetCollisionResponseToChannel(ECC_GameTraceChannel1);
	TestNotEqual(TEXT("precondition: the template does not already ignore GameTraceChannel1"), static_cast<int32>(OldChannel1), static_cast<int32>(ECR_Ignore));

	// 1. Raw enum spelling of a game channel plus an engine name.
	TMap<FString, FString> Responses;
	Responses.Add(TEXT("ECC_GameTraceChannel1"), TEXT("Ignore"));
	Responses.Add(TEXT("Visibility"), TEXT("Block"));
	TestTrue(TEXT("set_collision_settings accepts ECC_GameTraceChannel1 + Visibility"),
		UBlueprintService::SetCollisionSettings(Path, ComponentName, TEXT(""), TEXT(""), TEXT(""), Responses));

	Template = FindTemplate();
	if (!TestNotNull(TEXT("component template still present after compile"), Template))
	{
		return false;
	}
	TestEqual(TEXT("template GameTraceChannel1 response is Ignore"), static_cast<int32>(Template->GetCollisionResponseToChannel(ECC_GameTraceChannel1)), static_cast<int32>(ECR_Ignore));
	TestEqual(TEXT("template Visibility response is Block"), static_cast<int32>(Template->GetCollisionResponseToChannel(ECC_Visibility)), static_cast<int32>(ECR_Block));

	// 2. Propagation: the untouched placed instance follows the template; the overriding one keeps
	// its own response table (the details panel's rule).
	if (UPrimitiveComponent* UntouchedComponent = FindInstanceComponent(UntouchedTag))
	{
		TestEqual(TEXT("untouched placed instance received GameTraceChannel1 = Ignore"), static_cast<int32>(UntouchedComponent->GetCollisionResponseToChannel(ECC_GameTraceChannel1)), static_cast<int32>(ECR_Ignore));
		TestTrue(TEXT("untouched placed instance received the template's full response table"),
			UntouchedComponent->GetCollisionResponseToChannels() == Template->GetCollisionResponseToChannels());
	}
	else
	{
		AddError(TEXT("untouched placed instance's component disappeared after the compile"));
	}
	if (UPrimitiveComponent* OverrideComponent = FindInstanceComponent(OverrideTag))
	{
		TestEqual(TEXT("overriding instance kept its Camera override"), static_cast<int32>(OverrideComponent->GetCollisionResponseToChannel(ECC_Camera)), static_cast<int32>(ECR_Overlap));
		TestEqual(TEXT("overriding instance kept its old GameTraceChannel1 response"), static_cast<int32>(OverrideComponent->GetCollisionResponseToChannel(ECC_GameTraceChannel1)), static_cast<int32>(OldChannel1));
		TestTrue(TEXT("overriding instance kept its full response table"),
			OverrideComponent->GetCollisionResponseToChannels() == OverrideResponses);
	}
	else
	{
		AddError(TEXT("overriding placed instance's component disappeared after the compile"));
	}

	// 3. Configured custom channel NAME, read from the host's own channel table.
	const UCollisionProfile* Profile = UCollisionProfile::Get();
	int32 CustomIndex = INDEX_NONE;
	FString CustomName;
	for (int32 Index = ECC_GameTraceChannel1; Index <= ECC_GameTraceChannel18 && Profile; ++Index)
	{
		const FString Name = Profile->ReturnChannelNameFromContainerIndex(Index).ToString();
		const FString DefaultName = FString::Printf(TEXT("GameTraceChannel%d"), Index - ECC_GameTraceChannel1 + 1);
		if (!Name.IsEmpty() && Name != TEXT("None") && !Name.Equals(DefaultName, ESearchCase::IgnoreCase))
		{
			CustomIndex = Index;
			CustomName = Name;
			break;
		}
	}
	if (CustomIndex != INDEX_NONE)
	{
		TMap<FString, FString> NamedResponses;
		NamedResponses.Add(CustomName, TEXT("Overlap"));
		TestTrue(FString::Printf(TEXT("set_collision_settings accepts the configured channel name '%s'"), *CustomName),
			UBlueprintService::SetCollisionSettings(Path, ComponentName, TEXT(""), TEXT(""), TEXT(""), NamedResponses));
		if (UPrimitiveComponent* Updated = FindTemplate())
		{
			TestEqual(FString::Printf(TEXT("template '%s' response is Overlap"), *CustomName),
				static_cast<int32>(Updated->GetCollisionResponseToChannel(static_cast<ECollisionChannel>(CustomIndex))), static_cast<int32>(ECR_Overlap));
		}
	}
	else
	{
		AddWarning(TEXT("No custom collision channel is configured in this host (DefaultChannelResponses); the positive custom-name case is skipped."));
	}

	// 4. An unknown name is refused before anything is written, and the error lists the valid names.
	Template = FindTemplate();
	if (!TestNotNull(TEXT("component template present before the refusal check"), Template))
	{
		return false;
	}
	const FCollisionResponseContainer BeforeRefusal = Template->GetCollisionResponseToChannels();
	TMap<FString, FString> Bad;
	Bad.Add(TEXT("Visibility"), TEXT("Ignore"));
	Bad.Add(TEXT("VibeNotAChannel"), TEXT("Block"));
	AddExpectedErrorPlain(TEXT("SetCollisionSettings: Unknown channel 'VibeNotAChannel'. Valid channels: WorldStatic, WorldDynamic, Pawn, Visibility, Camera, PhysicsBody, Vehicle, Destructible, "),
		EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("an unknown channel name refuses the call"),
		UBlueprintService::SetCollisionSettings(Path, ComponentName, TEXT(""), TEXT(""), TEXT(""), Bad));
	if (UPrimitiveComponent* AfterRefusal = FindTemplate())
	{
		TestTrue(TEXT("the refused call wrote nothing (Visibility untouched)"), AfterRefusal->GetCollisionResponseToChannels() == BeforeRefusal);
	}

	// 5. An unknown response or CollisionEnabled value is refused too, instead of defaulting to
	// Ignore / QueryAndPhysics (which propagation would then copy onto every placed actor).
	TMap<FString, FString> BadResponse;
	BadResponse.Add(TEXT("Visibility"), TEXT("Blok"));
	AddExpectedErrorPlain(TEXT("SetCollisionSettings: Unknown response 'Blok' for channel 'Visibility'"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("an unknown response refuses the call"),
		UBlueprintService::SetCollisionSettings(Path, ComponentName, TEXT(""), TEXT(""), TEXT(""), BadResponse));
	AddExpectedErrorPlain(TEXT("SetCollisionSettings: Unknown CollisionEnabled value 'QueryAndFizzics'"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("an unknown CollisionEnabled value refuses the call"),
		UBlueprintService::SetCollisionSettings(Path, ComponentName, TEXT("QueryAndFizzics"), TEXT(""), TEXT(""), TMap<FString, FString>()));
	if (UPrimitiveComponent* AfterBadValues = FindTemplate())
	{
		TestTrue(TEXT("the refused value calls wrote nothing"), AfterBadValues->GetCollisionResponseToChannels() == BeforeRefusal);
	}

	return true;
}

#endif // WITH_AUTOMATION_TESTS
