// Copyright VibeUE 2025

#pragma once

#include "CoreMinimal.h"

/**
 * Widget Reflection Type Definitions
 * 
 * This header contains reflection-related data structures for discovering widget types.
 */

/**
 * @struct FWidgetClassInfo
 * @brief Information about a widget class discovered via reflection
 */
struct VIBEUE_API FWidgetClassInfo
{
    /** Widget class name (e.g., "Button", "TextBlock") */
    FString ClassName;
    
    /** Full class path */
    FString ClassPath;
    
    /** Widget category (e.g., "Common", "Panel", "Input") */
    FString Category;
    
    /** Whether this supports child widgets */
    bool bSupportsChildren;
    
    /** Maximum number of children (-1 for unlimited) */
    int32 MaxChildren;
    
    /** Whether this is a panel widget */
    bool bIsPanel;
    
    /** Whether this is an engine widget */
    bool bIsEngineWidget;
    
    /** Whether this is a custom user widget */
    bool bIsCustomWidget;
    
    /** Supported child types (empty if all types supported) */
    TArray<FString> SupportedChildTypes;

    FWidgetClassInfo()
        : bSupportsChildren(false)
        , MaxChildren(0)
        , bIsPanel(false)
        , bIsEngineWidget(true)
        , bIsCustomWidget(false)
    {}
};

/**
 * @struct FWidgetCompatibilityInfo
 * @brief Information about parent-child widget compatibility
 */
struct VIBEUE_API FWidgetCompatibilityInfo
{
    /** Parent widget class name */
    FString ParentClass;
    
    /** Child widget class name */
    FString ChildClass;
    
    /** Whether this combination is compatible */
    bool bIsCompatible;
    
    /** Reason for incompatibility (if applicable) */
    FString IncompatibilityReason;

    FWidgetCompatibilityInfo()
        : bIsCompatible(true)
    {}
};
