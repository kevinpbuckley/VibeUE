#pragma once

#include "CoreMinimal.h"

/**
 * Core Widget Type Definitions
 * 
 * This header contains core widget data structures used across UMG services.
 */

/**
 * @struct FWidgetInfo
 * @brief Information about a single widget component
 */
struct VIBEUE_API FWidgetInfo
{
    /** Name of the widget */
    FString Name;
    
    /** Widget class type (e.g., "TextBlock", "Button") */
    FString Type;
    
    /** Name of the parent widget (empty if root) */
    FString ParentName;
    
    /** Names of child widgets */
    TArray<FString> Children;
    
    /** Whether this widget is exposed as a variable */
    bool bIsVariable;

    FWidgetInfo()
        : bIsVariable(false)
    {}
};

/**
 * @struct FWidgetHierarchy
 * @brief Complete widget hierarchy information
 */
struct VIBEUE_API FWidgetHierarchy
{
    /** All widget components in the hierarchy */
    TArray<FWidgetInfo> Components;
    
    /** Total number of widgets */
    int32 TotalCount;

    FWidgetHierarchy()
        : TotalCount(0)
    {}
};

/**
 * @struct FWidgetBlueprintInfo
 * @brief Detailed information about a Widget Blueprint
 */
struct VIBEUE_API FWidgetBlueprintInfo
{
    /** Name of the widget blueprint */
    FString Name;
    
    /** Full object path */
    FString Path;
    
    /** Package path */
    FString PackagePath;
    
    /** Parent class name */
    FString ParentClass;
    
    /** Root widget name (if any) */
    FString RootWidget;
    
    /** Total widget count */
    int32 WidgetCount;

    FWidgetBlueprintInfo()
        : WidgetCount(0)
    {}
    /** Detailed components (if requested) */
    TArray<FWidgetInfo> Components;

    /** Variable names exposed by the blueprint */
    TArray<FString> VariableNames;

    /** Discovered event names */
    TArray<FString> EventNames;

    /** Animation names */
    TArray<FString> AnimationNames;
};

/**
 * @struct FWidgetSlotInfo
 * @brief Information about a widget slot configuration
 */
struct VIBEUE_API FWidgetSlotInfo
{
    /** Widget name */
    FString WidgetName;
    
    /** Slot type (e.g., "CanvasPanelSlot", "HorizontalBoxSlot") */
    FString SlotType;
    
    /** Slot properties as key-value pairs */
    TMap<FString, FString> Properties;
};

/**
 * @struct FWidgetComponentInfo
 * @brief Detailed information about a single widget component used by handlers
 */
struct VIBEUE_API FWidgetComponentInfo
{
    FString Name;
    FString Type;
    bool bIsVariable = false;
    bool bIsEnabled = true;
    FString Visibility;
    TOptional<FWidgetSlotInfo> SlotInfo;
    FWidgetComponentInfo() : bIsVariable(false), bIsEnabled(true) {}
};

/**
 * @struct FWidgetEventInfo
 * @brief Representation of an event or callable function on a widget class/blueprint
 */
struct VIBEUE_API FWidgetEventInfo
{
    FString Name;
    FString Type;
    FString Description;
    FWidgetEventInfo() {}
};

/**
 * @struct FWidgetInputMapping
 * @brief Mapping request for binding an input/event to a blueprint function
 */
struct VIBEUE_API FWidgetInputMapping
{
    FString EventName;
    FString FunctionName;
    FWidgetInputMapping() {}
};

/**
 * @struct FWidgetReferenceInfo
 * @brief Reference information discovered during widget deletion checks
 */
struct VIBEUE_API FWidgetReferenceInfo
{
    FString PackageName;
    FString ReferenceType;
    FWidgetReferenceInfo() {}
};

/**
 * @struct FWidgetDeleteResult
 * @brief Result payload returned when deleting a widget blueprint asset
 */
struct VIBEUE_API FWidgetDeleteResult
{
    FString WidgetName;
    FString AssetPath;
    bool bReferencesChecked = false;
    TArray<FWidgetReferenceInfo> References;
    int32 ReferenceCount = 0;
    bool bWasOpenInEditor = false;
    bool bDeletionSucceeded = false;
    FString ErrorMessage;

    FWidgetDeleteResult() {}
};
