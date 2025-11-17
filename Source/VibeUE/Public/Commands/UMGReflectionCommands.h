// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Forward declarations to avoid heavy includes in header
class UWidgetBlueprint;
class UWidget;
class FServiceContext;

/**
 * UMG Reflection Commands - Generic widget discovery and creation using reflection
 * 
 * This class provides reflection-based tools for discovering and adding UMG widgets,
 * mirroring the functionality of Unreal Engine's Widget Palette.
 */
class VIBEUE_API FUMGReflectionCommands
{
public:
	FUMGReflectionCommands();
	explicit FUMGReflectionCommands(TSharedPtr<FServiceContext> InServiceContext);
	~FUMGReflectionCommands();

	/**
	 * Handle reflection-based UMG commands
	 */
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandName, const TSharedPtr<FJsonObject>& Params);

private:
	// Service Context for accessing shared utilities
	TSharedPtr<FServiceContext> ServiceContext;

	// Command Handlers
	/**
	 * Handle add_widget_component command - Add a widget component to a UserWidget Blueprint
	 */
	TSharedPtr<FJsonObject> HandleAddWidgetComponent(const TSharedPtr<FJsonObject>& Params);

	// Widget Discovery System
	/**
	 * Discover all available widget classes using UClass reflection
	 */
	TArray<UClass*> DiscoverWidgetClasses(bool bIncludeEngine = true, bool bIncludeCustom = true);

	/**
	 * Get widget category for a given widget class
	 */
	FString GetWidgetCategory(UClass* WidgetClass);

	/**
	 * Check if widget class supports children
	 */
	bool DoesWidgetSupportChildren(UClass* WidgetClass);

	/**
	 * Get maximum children count for a widget class (-1 = unlimited)
	 */
	int32 GetMaxChildrenCount(UClass* WidgetClass);

	/**
	 * Check parent-child compatibility
	 */
	bool IsParentChildCompatible(UClass* ParentClass, UClass* ChildClass);

	/**
	 * Get supported child types for a parent widget
	 */
	TArray<FString> GetSupportedChildTypes(UClass* ParentClass);

	// Widget Creation Helpers
	/**
	 * Create widget component instance and add to parent
	 */
	TSharedPtr<FJsonObject> CreateAndAddWidgetComponent(
		UWidgetBlueprint* WidgetBlueprint,
		UClass* ComponentClass,
		const FString& ComponentName,
		const FString& ParentName,
		bool bIsVariable,
		const TSharedPtr<FJsonObject>& Properties
	);

	/**
	 * Validate widget creation parameters
	 */
	TSharedPtr<FJsonObject> ValidateWidgetCreation(
		UWidgetBlueprint* WidgetBlueprint,
		UClass* ComponentClass,
		const FString& ParentName
	);

	/**
	 * Apply initial properties to widget component
	 */
	void ApplyWidgetProperties(UWidget* Widget, const TSharedPtr<FJsonObject>& Properties);
};