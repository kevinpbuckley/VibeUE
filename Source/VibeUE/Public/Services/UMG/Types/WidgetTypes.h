#pragma once

#include "CoreMinimal.h"

/**
 * UMG Widget Type Definitions
 * 
 * This header contains data structures related to UMG widgets,
 * widget components, and widget hierarchies.
 */

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
 * @struct FWidgetInfo
 * @brief Structure holding basic widget blueprint information
 * 
 * Contains essential metadata about a widget blueprint asset including its name,
 * path, parent class, and type information.
 */
struct VIBEUE_API FWidgetInfo
{
    /** The name of the widget blueprint asset */
    FString Name;
    
    /** Full object path to the widget blueprint */
    FString Path;
    
    /** Package path containing the widget blueprint */
    FString PackagePath;
    
    /** Name of the parent widget class */
    FString ParentClass;
    
    /** Widget class type name */
    FString WidgetType;

    FWidgetInfo()
    {}
};

/**
 * @struct FWidgetTypeInfo
 * @brief Structure holding metadata about a widget type
 * 
 * Contains information about a widget class including its type name,
 * full class path, whether it's a panel type, and other metadata.
 */
struct VIBEUE_API FWidgetTypeInfo
{
    /** The widget type name (e.g., "Button", "TextBlock") */
    FString TypeName;
    
    /** Full class path to the widget class */
    FString ClassPath;
    
    /** Display category for the widget */
    FString Category;
    
    /** Whether this widget type can contain children */
    bool bIsPanelWidget;
    
    /** Whether this is a commonly used widget type */
    bool bIsCommonWidget;
    
    FWidgetTypeInfo()
        : bIsPanelWidget(false)
        , bIsCommonWidget(false)
    {
    }
};
