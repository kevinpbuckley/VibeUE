// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Types/MVVMBindingMode.h"
#include "UWidgetService.generated.h"

/**
 * Information about a widget in a Widget Blueprint
 */
USTRUCT(BlueprintType)
struct FWidgetInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString WidgetName;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString WidgetClass;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString ParentWidget;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	bool bIsRootWidget = false;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	bool bIsVariable = false;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	TArray<FString> Children;
};

/**
 * Information about a widget component property
 */
USTRUCT(BlueprintType)
struct FWidgetPropertyInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString PropertyName;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString PropertyType;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString Category;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString CurrentValue;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	bool bIsEditable = true;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	bool bIsBlueprintVisible = true;
};

/**
 * Information about a widget event
 */
USTRUCT(BlueprintType)
struct FWidgetEventInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString EventName;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString EventType;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString Description;
};

/**
 * Result of validation operation
 */
USTRUCT(BlueprintType)
struct FWidgetValidationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	bool bIsValid = true;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	TArray<FString> Errors;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString ValidationMessage;
};

/**
 * Result of adding a component
 */
USTRUCT(BlueprintType)
struct FWidgetAddComponentResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString ComponentName;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString ComponentType;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString ParentName;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	bool bIsVariable = false;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString ErrorMessage;
};

/**
 * Result of removing a component
 */
USTRUCT(BlueprintType)
struct FWidgetRemoveComponentResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	TArray<FString> RemovedComponents;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	TArray<FString> OrphanedChildren;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString ErrorMessage;
};

/**
 * Information about a ViewModel registered on a Widget Blueprint
 */
USTRUCT(BlueprintType)
struct FWidgetViewModelInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString ViewModelName;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString ViewModelClassName;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString CreationType;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString ViewModelId;
};

/**
 * Information about an MVVM binding on a Widget Blueprint
 */
USTRUCT(BlueprintType)
struct FWidgetViewModelBindingInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	int32 BindingIndex = -1;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString SourcePath;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString DestinationPath;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString BindingMode;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	bool bEnabled = true;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString BindingId;
};

/**
 * Widget service exposed directly to Python.
 *
 * Provides 22 widget management actions:
 * - list_widget_blueprints: List all Widget Blueprints in the project
 * - get_hierarchy: Get widget hierarchy tree
 * - get_root_widget: Get the root widget of a Widget Blueprint
 * - list_components: List all widget components in a Widget Blueprint
 * - search_types: Get available widget types (native types + discovered WBPs for reference)
 * - get_component_properties: Get properties for a specific component
 * - add_component: Add a widget component to a Widget Blueprint (native types or custom WBPs by name)
 * - remove_component: Remove a widget component from a Widget Blueprint
 * - validate: Validate widget hierarchy for errors
 * - get_property: Get a specific property value
 * - set_property: Set a specific property value
 * - list_properties: List all editable properties of a component
 * - get_available_events: Get available events for a widget type
 * - bind_event: Bind an event to a function
 * - list_view_models: List all ViewModels registered on a Widget Blueprint
 * - add_view_model: Add a ViewModel to a Widget Blueprint
 * - remove_view_model: Remove a ViewModel from a Widget Blueprint
 * - list_view_model_bindings: List all MVVM bindings on a Widget Blueprint
 * - add_view_model_binding: Create a binding between a ViewModel property and a widget property
 * - remove_view_model_binding: Remove an MVVM binding by index
 * - widget_blueprint_exists: Check if a Widget Blueprint exists
 * - widget_exists: Check if a widget component exists in a Widget Blueprint
 *
 * Python Usage:
 *   import unreal
 *
 *   # List all Widget Blueprints
 *   widgets = unreal.WidgetService.list_widget_blueprints()
 *
 *   # Get widget hierarchy
 *   hierarchy = unreal.WidgetService.get_hierarchy("/Game/UI/WBP_MainMenu")
 *
 *   # List components in a widget
 *   components = unreal.WidgetService.list_components("/Game/UI/WBP_MainMenu")
 *
 *   # Add a button component
 *   result = unreal.WidgetService.add_component("/Game/UI/WBP_MainMenu", "Button", "MyButton", "CanvasPanel_0", True)
 *
 *   # Get available widget types
 *   types = unreal.WidgetService.search_types()
 *
 *   # Get property value
 *   value = unreal.WidgetService.get_property("/Game/UI/WBP_MainMenu", "MyButton", "Visibility")
 *
 *   # Set property value
 *   unreal.WidgetService.set_property("/Game/UI/WBP_MainMenu", "MyButton", "Visibility", "Visible")
 *
 *   # List ViewModels
 *   vms = unreal.WidgetService.list_view_models("/Game/UI/WBP_MainMenu")
 *
 *   # Add a ViewModel
 *   unreal.WidgetService.add_view_model("/Game/UI/WBP_MainMenu", "MyHealthViewModel", "HealthVM")
 *
 *   # Add a ViewModel binding
 *   unreal.WidgetService.add_view_model_binding("/Game/UI/WBP_MainMenu", "HealthVM", "CurrentHealth", "HealthBar", "Percent", "OneWayToDestination")
 *
 * @note This replaces the JSON-based manage_umg_widget MCP tool
 */
UCLASS(BlueprintType)
class VIBEUE_API UWidgetService : public UObject
{
	GENERATED_BODY()

public:
	// =================================================================
	// Discovery Methods (list_components, search_types, get_component_properties)
	// =================================================================

	/**
	 * List all Widget Blueprint assets.
	 *
	 * @param PathFilter - Optional path filter
	 * @return Array of Widget Blueprint paths
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets")
	static TArray<FString> ListWidgetBlueprints(const FString& PathFilter = TEXT("/Game"));

	/**
	 * Get widget hierarchy for a Widget Blueprint.
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @return Array of widget information in hierarchy order
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets")
	static TArray<FWidgetInfo> GetHierarchy(const FString& WidgetPath);

	/**
	 * Get the root widget of a Widget Blueprint.
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @return Name of the root widget, or empty if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets")
	static FString GetRootWidget(const FString& WidgetPath);

	/**
	 * List all widget components in a Widget Blueprint.
	 * Maps to action="list_components"
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @return Array of widget component information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets")
	static TArray<FWidgetInfo> ListComponents(const FString& WidgetPath);

	/**
	 * Get available widget types that can be created.
	 * Maps to action="search_types"
	 * Returns built-in native types plus discovered Widget Blueprints (prefixed with [WBP]).
	 *
	 * @param FilterText - Optional filter to narrow results
	 * @return Array of widget type names (Button, TextBlock, etc.) and discovered WBPs
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets")
	static TArray<FString> SearchTypes(const FString& FilterText = TEXT(""));

	/**
	 * Get detailed properties for a specific widget component.
	 * Maps to action="get_component_properties"
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @param ComponentName - Name of the component to inspect
	 * @return Array of property information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets")
	static TArray<FWidgetPropertyInfo> GetComponentProperties(const FString& WidgetPath, const FString& ComponentName);

	// =================================================================
	// Component Management (add_component, remove_component)
	// =================================================================

	/**
	 * Add a new widget component to a Widget Blueprint.
	 * Maps to action="add_component"
	 * Supports both native widget types (TextBlock, Button, etc.) and custom Widget Blueprints by name.
	 * Custom WBPs are resolved via the Asset Registry and compiled before use.
	 * Circular references (a WBP containing itself) are detected and rejected.
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @param ComponentType - Type of widget: native type name (e.g. "Button") or WBP asset name (e.g. "WBP_HealthBar")
	 * @param ComponentName - Name for the new component
	 * @param ParentName - Name of parent panel (empty for root)
	 * @param bIsVariable - Whether to expose as a variable
	 * @return Result with success status and details
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets")
	static FWidgetAddComponentResult AddComponent(
		const FString& WidgetPath,
		const FString& ComponentType,
		const FString& ComponentName,
		const FString& ParentName = TEXT(""),
		bool bIsVariable = true);

	/**
	 * Remove a widget component from a Widget Blueprint.
	 * Maps to action="remove_component"
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @param ComponentName - Name of the component to remove
	 * @param bRemoveChildren - Whether to also remove child widgets
	 * @return Result with removed components and orphaned children
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets")
	static FWidgetRemoveComponentResult RemoveComponent(
		const FString& WidgetPath,
		const FString& ComponentName,
		bool bRemoveChildren = false);

	// =================================================================
	// Validation (validate)
	// =================================================================

	/**
	 * Validate widget hierarchy for errors.
	 * Maps to action="validate"
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @return Validation result with any errors found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets")
	static FWidgetValidationResult Validate(const FString& WidgetPath);

	// =================================================================
	// Property Access (get_property, set_property, list_properties)
	// =================================================================

	/**
	 * Get a specific property value from a widget component.
	 * Maps to action="get_property"
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @param ComponentName - Name of the component
	 * @param PropertyName - Name of the property to get
	 * @return Property value as string (empty if not found)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets")
	static FString GetProperty(
		const FString& WidgetPath,
		const FString& ComponentName,
		const FString& PropertyName);

	/**
	 * Set a property value on a widget component.
	 * Maps to action="set_property"
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @param ComponentName - Name of the component
	 * @param PropertyName - Name of the property to set
	 * @param PropertyValue - Value to set (as string)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets")
	static bool SetProperty(
		const FString& WidgetPath,
		const FString& ComponentName,
		const FString& PropertyName,
		const FString& PropertyValue);

	/**
	 * List all editable properties of a widget component.
	 * Maps to action="list_properties"
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @param ComponentName - Name of the component
	 * @param bEditableOnly - Whether to only return editable properties
	 * @return Array of property information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets")
	static TArray<FWidgetPropertyInfo> ListProperties(
		const FString& WidgetPath,
		const FString& ComponentName,
		bool bEditableOnly = true);

	// =================================================================
	// Event Handling (get_available_events, bind_events)
	// =================================================================

	/**
	 * Get available events for a widget type or component.
	 * Maps to action="get_available_events"
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @param ComponentName - Name of the component (optional)
	 * @param WidgetType - Type of widget to query events for (optional)
	 * @return Array of available events
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets")
	static TArray<FWidgetEventInfo> GetAvailableEvents(
		const FString& WidgetPath,
		const FString& ComponentName = TEXT(""),
		const FString& WidgetType = TEXT(""));

	/**
	 * Bind an event to a function in the Widget Blueprint.
	 * Maps to action="bind_events"
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @param EventName - Name of the event (e.g., "OnClicked")
	 * @param FunctionName - Name of the function to call
	 * @return True if binding was successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets")
	static bool BindEvent(
		const FString& WidgetPath,
		const FString& EventName,
		const FString& FunctionName);

	// =================================================================
	// ViewModel Management (MVVM)
	// =================================================================

	/**
	 * List all ViewModels registered on a Widget Blueprint.
	 * Maps to action="list_view_models"
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @return Array of ViewModel information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|ViewModel")
	static TArray<FWidgetViewModelInfo> ListViewModels(const FString& WidgetPath);

	/**
	 * Add a ViewModel to a Widget Blueprint.
	 * Maps to action="add_view_model"
	 * The ViewModel class must implement INotifyFieldValueChanged (typically inherits from UMVVMViewModelBase).
	 * Classes are resolved by name from C++ or Blueprint ViewModel assets.
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @param ViewModelClassName - Name of the ViewModel class (e.g. "MyHealthViewModel" or full path)
	 * @param ViewModelName - Property name/alias for this ViewModel instance (e.g. "HealthVM")
	 * @param CreationType - How the ViewModel is created: "CreateInstance" (default), "Manual", "GlobalViewModelCollection", "PropertyPath", "Resolver"
	 * @return True if the ViewModel was added successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|ViewModel")
	static bool AddViewModel(
		const FString& WidgetPath,
		const FString& ViewModelClassName,
		const FString& ViewModelName,
		const FString& CreationType = TEXT("CreateInstance"));

	/**
	 * Remove a ViewModel from a Widget Blueprint.
	 * Maps to action="remove_view_model"
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @param ViewModelName - Name of the ViewModel to remove
	 * @return True if the ViewModel was removed successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|ViewModel")
	static bool RemoveViewModel(
		const FString& WidgetPath,
		const FString& ViewModelName);

	/**
	 * List all MVVM bindings on a Widget Blueprint.
	 * Maps to action="list_view_model_bindings"
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @return Array of binding information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|ViewModel")
	static TArray<FWidgetViewModelBindingInfo> ListViewModelBindings(const FString& WidgetPath);

	/**
	 * Add an MVVM binding between a ViewModel property and a widget property.
	 * Maps to action="add_view_model_binding"
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @param ViewModelName - Name of the ViewModel (as registered via add_view_model)
	 * @param ViewModelProperty - Property name on the ViewModel (e.g. "CurrentHealth")
	 * @param WidgetName - Name of the target widget component (e.g. "HealthBar")
	 * @param WidgetProperty - Property name on the widget (e.g. "Percent")
	 * @param BindingMode - Binding direction: "OneWayToDestination" (default), "TwoWay", "OneTimeToDestination", "OneWayToSource", "OneTimeToSource"
	 * @return True if binding was created successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|ViewModel")
	static bool AddViewModelBinding(
		const FString& WidgetPath,
		const FString& ViewModelName,
		const FString& ViewModelProperty,
		const FString& WidgetName,
		const FString& WidgetProperty,
		const FString& BindingMode = TEXT("OneWayToDestination"));

	/**
	 * Remove an MVVM binding from a Widget Blueprint by index.
	 * Maps to action="remove_view_model_binding"
	 * Use list_view_model_bindings to get valid indices.
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @param BindingIndex - Index of the binding to remove (from list_view_model_bindings)
	 * @return True if the binding was removed successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|ViewModel")
	static bool RemoveViewModelBinding(
		const FString& WidgetPath,
		int32 BindingIndex);

	// =================================================================
	// Existence Checks
	// =================================================================

	/**
	 * Check if a Widget Blueprint exists at the given path.
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @return True if Widget Blueprint exists
	 *
	 * Example:
	 *   if not unreal.WidgetService.widget_blueprint_exists("/Game/UI/WBP_MainMenu"):
	 *       # Create the widget blueprint
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|Exists")
	static bool WidgetBlueprintExists(const FString& WidgetPath);

	/**
	 * Check if a widget component exists in a Widget Blueprint.
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @param ComponentName - Name of the widget component
	 * @return True if component exists
	 *
	 * Example:
	 *   if not unreal.WidgetService.widget_exists("/Game/UI/WBP_MainMenu", "StartButton"):
	 *       unreal.WidgetService.add_component("/Game/UI/WBP_MainMenu", "Button", "StartButton", "CanvasPanel_0")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|Exists")
	static bool WidgetExists(const FString& WidgetPath, const FString& ComponentName);

private:
	/** Helper to load and validate a Widget Blueprint */
	static class UWidgetBlueprint* LoadWidgetBlueprint(const FString& WidgetPath);
	
	/** Helper to find a widget component by name */
	static class UWidget* FindWidgetByName(class UWidgetBlueprint* WidgetBP, const FString& ComponentName);
	
	/** Helper to create a widget class from type name */
	static TSubclassOf<class UWidget> FindWidgetClass(const FString& TypeName);

	/** Helper to find a ViewModel class by name (C++ or Blueprint ViewModel) */
	static UClass* FindViewModelClass(const FString& ClassName);

	/** Helper to get or create the MVVM Blueprint View for a Widget Blueprint */
	static class UMVVMBlueprintView* GetOrCreateMVVMView(class UWidgetBlueprint* WidgetBP);

	/** Helper to convert EMVVMBindingMode to string */
	static FString BindingModeToString(EMVVMBindingMode Mode);

	/** Helper to convert string to EMVVMBindingMode */
	static EMVVMBindingMode StringToBindingMode(const FString& ModeString);
};
