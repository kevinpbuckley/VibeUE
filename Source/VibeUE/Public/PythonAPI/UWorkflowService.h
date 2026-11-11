// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UWorkflowService.generated.h"

/** High-level, evidence-producing workflows built from VibeUE's lower-level editor primitives. */
UCLASS(BlueprintType)
class VIBEUE_API UWorkflowService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/** Authoritative project/engine/toolchain/build context as JSON. Read-only. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Workflow")
	static FString GetEnvironment();












	/** Module lifecycle hooks for mutation observation and interrupted-run recovery. */
	static void InitializeJournal();
	static void ShutdownJournal();

	/** Internal workflow composition helpers. They intentionally are not exposed as agent tools. */
};
