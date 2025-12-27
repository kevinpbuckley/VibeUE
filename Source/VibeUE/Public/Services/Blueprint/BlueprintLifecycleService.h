// Copyright Buckley Builds LLC 2025 All Rights Reserved.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"

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
    
    // FServiceBase interface
    virtual FString GetServiceName() const override { return TEXT("BlueprintLifecycleService"); }
    
    // Lifecycle operations
    TResult<UBlueprint*> CreateBlueprint(const FString& Name, const FString& ParentClass);
    TResult<void> CompileBlueprint(UBlueprint* Blueprint);
    TResult<void> ReparentBlueprint(UBlueprint* Blueprint, UClass* NewParentClass);
    TResult<void> ReparentBlueprint(UBlueprint* Blueprint, const FString& NewParentClassName);
    TResult<void> DeleteBlueprint(UBlueprint* Blueprint);
    
    // Compilation utilities
    TResult<TArray<FString>> GetCompilationErrors(UBlueprint* Blueprint);
    TResult<bool> IsCompiled(UBlueprint* Blueprint);

private:
    // Helper to find parent class from string descriptor
    UClass* FindParentClass(const FString& ClassDescriptor);
};
