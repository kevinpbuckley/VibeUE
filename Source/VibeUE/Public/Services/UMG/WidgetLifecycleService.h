// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Services/UMG/Types/WidgetTypes.h"
#include "Core/Result.h"

// Forward declarations
class UWidgetBlueprint;

/**
 * @class FWidgetLifecycleService
 * @brief Service for widget blueprint lifecycle management
 * 
 * This service provides widget blueprint creation and deletion functionality
 * extracted from UMGCommands.cpp. It handles creating new widget blueprints,
 * deleting them, and managing their lifecycle.
 * 
 * All methods return TResult<T> for type-safe error handling.
 * 
 * @see TResult
 * @see FServiceBase
 */
class VIBEUE_API FWidgetLifecycleService : public FServiceBase
{
public:
    /**
     * @brief Constructor
     * @param Context Service context for shared state (can be nullptr)
     */
    explicit FWidgetLifecycleService(TSharedPtr<FServiceContext> Context);

    // FServiceBase interface
    virtual FString GetServiceName() const override { return TEXT("WidgetLifecycleService"); }

    /**
     * @brief Create a new widget blueprint
     * 
     * @param WidgetName Name for the new widget blueprint
     * @param PackagePath Package path (default "/Game/UI/")
     * @param ParentClass Parent class (default UUserWidget)
     * @return TResult containing the created widget blueprint or error
     */
    TResult<UWidgetBlueprint*> CreateWidgetBlueprint(
        const FString& WidgetName,
        const FString& PackagePath = TEXT("/Game/UI/"),
        const FString& ParentClass = TEXT("UserWidget")
    );

    /**
     * @brief Delete a widget blueprint
     * 
     * @param WidgetBlueprintPath Full path to widget blueprint to delete
     * @return TResult indicating success or error
     */
    TResult<void> DeleteWidgetBlueprint(const FString& WidgetBlueprintPath);

    /**
     * @brief Check if a widget blueprint can be deleted
     * 
     * @param WidgetBlueprint Widget blueprint to check
     * @return TResult containing true if can be deleted, false otherwise
     */
    TResult<bool> CanDeleteWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint);

    /**
     * @brief Compile a widget blueprint
     * 
     * @param WidgetBlueprint Widget blueprint to compile
     * @return TResult indicating success or error
     */
    TResult<void> CompileWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint);

    /**
     * @brief Save a widget blueprint
     * 
     * @param WidgetBlueprint Widget blueprint to save
     * @return TResult indicating success or error
     */
    TResult<void> SaveWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint);

private:
    /**
     * @brief Validate widget blueprint name
     * 
     * @param WidgetName Name to validate
     * @return True if valid
     */
    bool IsValidWidgetName(const FString& WidgetName);

    /**
     * @brief Get parent class UClass from name
     * 
     * @param ParentClassName Parent class name
     * @return UClass pointer or nullptr
     */
    UClass* GetParentClass(const FString& ParentClassName);
};
