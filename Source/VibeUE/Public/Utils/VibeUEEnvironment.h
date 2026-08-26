// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;

/** Canonical, read-only project/engine/build context shared by the tool and readiness signal. */
class VIBEUE_API FVibeUEEnvironment
{
public:
	static TSharedRef<FJsonObject> BuildObject();
	static FString BuildJson();
};
