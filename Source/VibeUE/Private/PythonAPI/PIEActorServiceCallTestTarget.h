// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PIEActorServiceCallTestTarget.generated.h"

/** Test-only enum for the CallFunction value checks (Alpha=0, Beta=1, plus the generated _MAX). */
UENUM()
enum class EVibeUEPIECallTestEnum : uint8
{
	Alpha,
	Beta
};

/**
 * Test-only target for the VibeUE.PIEActorService.CallFunction.* automation tests. A plain transient
 * UObject (no world, no net role), so UPIEActorService::CallFunctionOnObject can be exercised headless
 * without PIE. The UFUNCTIONs carry no specifiers, so none of them is exposed to Python or Blueprint.
 */
UCLASS(Transient, NotBlueprintable, NotBlueprintType, HideDropdown)
class UVibeUEPIECallTestTarget : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	int32 Add(int32 A, int32 B) { return A + B; }

	UFUNCTION()
	void SetLabel(const FString& InLabel) { Label = InLabel; }

	/** Records GAllowActorScriptExecutionInEditor at call time; returns true when it was lowered. */
	UFUNCTION()
	bool GuardWasLowered()
	{
		bGuardSeenDuringCall = GAllowActorScriptExecutionInEditor;
		return !GAllowActorScriptExecutionInEditor;
	}

	UFUNCTION()
	void WithOut(int32 In, int32& Out, FString& OutText)
	{
		Out = In * 2;
		OutText = FString::Printf(TEXT("in=%d"), In);
	}

	UFUNCTION()
	FString Describe(UObject* Other) { return Other ? Other->GetName() : FString(TEXT("null")); }

	/**
	 * A declared C++ default, so a missing argument takes it instead of MISSING_ARG. Exec only so UHT
	 * stores the CPP_Default_ metadata (it does for BlueprintCallable/Exec/AICallable functions); nothing
	 * routes exec commands to this transient object.
	 */
	UFUNCTION(Exec)
	int32 Scale(int32 Value, int32 Factor = 3) { return Value * Factor; }

	UFUNCTION()
	int32 SmallInts(int8 Small, uint16 Medium) { return static_cast<int32>(Small) + static_cast<int32>(Medium); }

	UFUNCTION()
	int32 PickEnum(EVibeUEPIECallTestEnum Choice) { return static_cast<int32>(Choice); }

	UPROPERTY(Transient)
	FString Label;

	UPROPERTY(Transient)
	bool bGuardSeenDuringCall = true;
};
