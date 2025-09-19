#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

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
	~FUMGReflectionCommands();

	/**
	 * Handle reflection-based UMG commands
	 */
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandName, const TSharedPtr<FJsonObject>& Params);

private:
	/**
	 * Get all available widget types using reflection
	 * Parameters: category (optional), include_custom (bool), include_engine (bool), parent_compatibility (optional)
	 */
	TSharedPtr<FJsonObject> HandleGetAvailableWidgets(const TSharedPtr<FJsonObject>& Params);

	/**
	 * Add widget component using reflection-based validation
	 * Parameters: widget_name, component_type, component_name, parent_name, position, size, is_variable, properties
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