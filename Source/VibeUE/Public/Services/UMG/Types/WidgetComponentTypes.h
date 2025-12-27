// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Misc/Optional.h"

/**
 * Component-specific data structures for UMG services.
 */

/**
 * Simple name/type descriptor used for reporting component operations.
 */
struct VIBEUE_API FWidgetComponentRecord
{
    FString Name;
    FString Type;
};

/**
 * Request payload for AddChildToPanel operations.
 */
struct VIBEUE_API FWidgetAddChildRequest
{
    FString WidgetBlueprintName;
    FString ChildName;
    FString ParentName;
    FString ParentType;
    bool bReparentIfExists = true;
    TOptional<int32> InsertIndex;
    TSharedPtr<FJsonObject> SlotProperties;
};

/**
 * Result payload for AddChildToPanel operations.
 */
struct VIBEUE_API FWidgetAddChildResult
{
    FString WidgetBlueprintName;
    FString ChildName;
    FString ParentName;
    FString ParentType;
    bool bReparented = false;
    bool bSlotPropertiesApplied = false;
    bool bStructureChanged = false;
    TOptional<int32> ChildIndex;
};

/**
 * Request payload for RemoveComponent operations.
 */
struct VIBEUE_API FWidgetRemoveComponentRequest
{
    FString ComponentName;
    bool bRemoveChildren = false;
    bool bRemoveFromVariables = false;
};

/**
 * Result payload for RemoveComponent operations.
 */
struct VIBEUE_API FWidgetRemoveComponentResult
{
    FString WidgetBlueprintName;
    FString ComponentName;
    FString ParentName;
    FString ParentType;
    bool bVariableCleanupPerformed = false;
    bool bStructureChanged = false;
    TArray<FWidgetComponentRecord> RemovedComponents;
    TArray<FWidgetComponentRecord> OrphanedChildren;
};

/**
 * Request payload for SetSlotProperties operations.
 */
struct VIBEUE_API FWidgetSlotUpdateRequest
{
    FString WidgetBlueprintName;
    FString WidgetName;
    FString SlotTypeOverride;
    TSharedPtr<FJsonObject> SlotProperties;
};

/**
 * Result payload for SetSlotProperties operations.
 */
struct VIBEUE_API FWidgetSlotUpdateResult
{
    FString WidgetBlueprintName;
    FString WidgetName;
    FString SlotType;
    bool bApplied = false;
    TSharedPtr<FJsonObject> AppliedProperties;
};
