// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ServiceBase.h"
#include "Core/TResult.h"

// Forward declarations
class UBlueprint;
class UClass;

/**
 * Service for Blueprint lifecycle operations (create, compile, reparent, delete)
 * Extracted from BlueprintCommands.cpp for better separation of concerns
 */
class VIBEUE_API FBlueprintLifecycleService : public FServiceBase
{
public:
    explicit FBlueprintLifecycleService(TSharedPtr<FServiceContext> Context);
    
    // Lifecycle operations
    TResult<UBlueprint*> CreateBlueprint(const FString& Name, const FString& ParentClass);
    TResult<void> CompileBlueprint(UBlueprint* Blueprint);
    TResult<void> ReparentBlueprint(UBlueprint* Blueprint, UClass* NewParentClass);
    TResult<void> DeleteBlueprint(UBlueprint* Blueprint);
    
    // Compilation utilities
    TResult<TArray<FString>> GetCompilationErrors(UBlueprint* Blueprint);
    TResult<bool> IsCompiled(UBlueprint* Blueprint);

private:
    // Helper to find parent class from string descriptor
    UClass* FindParentClass(const FString& ClassDescriptor);
};
