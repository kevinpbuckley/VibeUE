// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"

// Forward declarations
class UWidget;
class UWidgetBlueprint;
class UPanelWidget;
class UWidgetTree;

/**
 * @struct FWidgetComponentInfo
 * @brief Structure holding widget component information
 * 
 * Contains essential metadata about a widget component including its name,
 * type, parent, children, and variable status.
 */
struct VIBEUE_API FWidgetComponentInfo
{
    /** Component name */
    FString Name;
    
    /** Component type (class name) */
    FString Type;
    
    /** Parent component name (empty if root) */
    FString ParentName;
    
    /** Child component names */
    TArray<FString> Children;
    
    /** Whether this component is exposed as a variable */
    bool bIsVariable;

    FWidgetComponentInfo()
        : bIsVariable(false)
    {
    }
};

/**
 * @class FWidgetComponentService
 * @brief Service responsible for UMG component management
 * 
 * This service provides focused widget component CRUD operations extracted from
 * UMGCommands.cpp. It handles component lifecycle (add, remove, list), hierarchy
 * management (parent/child relationships), and component information queries.
 * 
 * All methods return TResult<T> for type-safe error handling, avoiding the need
 * for runtime JSON parsing in the service layer.
 * 
 * @note This is part of Phase 3 refactoring (Task 12) to extract UMG component
 * management into focused services as per CPP_REFACTORING_DESIGN.md
 * 
 * @see TResult
 * @see FServiceBase
 * @see Issue #32
 */
class VIBEUE_API FWidgetComponentService : public FServiceBase
{
public:
    /**
     * @brief Constructor
     * @param Context Service context for shared state (can be nullptr)
     */
    explicit FWidgetComponentService(TSharedPtr<FServiceContext> Context);

    /**
     * @brief Add a component to a widget blueprint
     * 
     * Creates a new widget component of the specified type and adds it to the widget tree.
     * The component can be added to a specific parent or to the root widget.
     * 
     * @param Widget Widget blueprint to add component to
     * @param ComponentType Widget class type (e.g., "TextBlock", "Button")
     * @param ComponentName Unique name for the component
     * @param ParentName Name of parent component (empty for root)
     * @return TResult containing the created widget on success, or error code and message on failure
     * 
     * @note Extracted from UMGCommands.cpp component add logic
     */
    TResult<UWidget*> AddComponent(
        UWidgetBlueprint* Widget,
        const FString& ComponentType,
        const FString& ComponentName,
        const FString& ParentName = FString());

    /**
     * @brief Remove a component from a widget blueprint
     * 
     * Removes the specified component from the widget tree. Can optionally remove
     * child components or reparent them to the root.
     * 
     * @param Widget Widget blueprint to remove component from
     * @param ComponentName Name of component to remove
     * @param bRemoveChildren If true, remove child components; if false, reparent them
     * @return TResult indicating success or error
     * 
     * @note Extracted from HandleRemoveUMGComponent (~80 lines)
     */
    TResult<void> RemoveComponent(
        UWidgetBlueprint* Widget,
        const FString& ComponentName,
        bool bRemoveChildren = true);

    /**
     * @brief List all components in a widget blueprint
     * 
     * Returns comprehensive information about all components in the widget tree
     * including their names, types, parents, children, and variable status.
     * 
     * @param Widget Widget blueprint to list components from
     * @return TResult containing array of component information
     * 
     * @note Extracted from HandleListWidgetComponents (~60 lines)
     */
    TResult<TArray<FWidgetComponentInfo>> ListComponents(UWidgetBlueprint* Widget);

    /**
     * @brief Set parent of a component
     * 
     * Changes the parent of a component in the widget tree hierarchy.
     * 
     * @param Widget Widget blueprint containing the component
     * @param ComponentName Name of component to reparent
     * @param NewParentName Name of new parent component
     * @return TResult indicating success or error
     * 
     * @note Extracted from hierarchy manipulation logic (~70 lines)
     */
    TResult<void> SetParent(
        UWidgetBlueprint* Widget,
        const FString& ComponentName,
        const FString& NewParentName);

    /**
     * @brief Get parent of a component
     * 
     * Returns the name of the parent component.
     * 
     * @param Widget Widget blueprint containing the component
     * @param ComponentName Name of component
     * @return TResult containing parent component name (empty if root)
     */
    TResult<FString> GetParent(
        UWidgetBlueprint* Widget,
        const FString& ComponentName);

    /**
     * @brief Get children of a component
     * 
     * Returns list of child component names for a panel widget.
     * 
     * @param Widget Widget blueprint containing the component
     * @param ComponentName Name of component
     * @return TResult containing array of child component names
     */
    TResult<TArray<FString>> GetChildren(
        UWidgetBlueprint* Widget,
        const FString& ComponentName);

    /**
     * @brief Get detailed information about a component
     * 
     * Returns comprehensive information about a specific component.
     * 
     * @param Widget Widget blueprint containing the component
     * @param ComponentName Name of component
     * @return TResult containing component information
     */
    TResult<FWidgetComponentInfo> GetComponentInfo(
        UWidgetBlueprint* Widget,
        const FString& ComponentName);

    /**
     * @brief Check if a component exists
     * 
     * Verifies that a component with the given name exists in the widget tree.
     * 
     * @param Widget Widget blueprint to check
     * @param ComponentName Name of component to check
     * @return TResult containing true if component exists, false otherwise
     */
    TResult<bool> ComponentExists(
        UWidgetBlueprint* Widget,
        const FString& ComponentName);

    /**
     * @brief Add a child widget to a widget switcher slot
     * 
     * Adds a child widget to a WidgetSwitcher component at a specific slot index.
     * 
     * @param Widget Widget blueprint containing the switcher
     * @param SwitcherName Name of the WidgetSwitcher component
     * @param ChildWidgetName Name of the child widget to add
     * @param SlotIndex Optional slot index (uses next available if not specified)
     * @param OutActualSlotIndex Optional output parameter for the actual slot index used
     * @return Success or error result
     */
    TResult<void> AddWidgetSwitcherSlot(
        UWidgetBlueprint* Widget,
        const FString& SwitcherName,
        const FString& ChildWidgetName,
        int32 SlotIndex = -1,
        int32* OutActualSlotIndex = nullptr);

protected:
    /** @brief Gets the service name for logging */
    virtual FString GetServiceName() const override { return TEXT("WidgetComponentService"); }

private:
    /**
     * @brief Validate widget blueprint is not null
     * @param Widget Widget blueprint to validate
     * @return Success or error result
     */
    TResult<void> ValidateWidget(UWidgetBlueprint* Widget) const;

    /**
     * @brief Find a widget component by name
     * @param WidgetTree Widget tree to search
     * @param ComponentName Name of component to find
     * @return Pointer to widget or nullptr if not found
     */
    UWidget* FindComponent(UWidgetTree* WidgetTree, const FString& ComponentName) const;

    /**
     * @brief Collect all children of a widget recursively
     * @param Widget Parent widget
     * @param OutChildren Output array for children
     */
    void CollectChildren(UWidget* Widget, TArray<UWidget*>& OutChildren) const;

    /**
     * @brief Get widget class from type string
     * @param ComponentType Type string (e.g., "TextBlock")
     * @return Widget class or nullptr if not found
     */
    UClass* GetWidgetClass(const FString& ComponentType) const;
};
