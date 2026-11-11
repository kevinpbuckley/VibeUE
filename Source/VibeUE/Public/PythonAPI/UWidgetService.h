// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
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
 * Slot layout data for a widget component.
 * SlotType indicates which fields are populated:
 *   "Canvas"        -> AnchorMin/Max, Offsets, Alignment, ZOrder, bAutoSize
 *   "VerticalBox"   -> SizeRule, SizeValue, Padding, HorizontalAlignment, VerticalAlignment
 *   "HorizontalBox" -> SizeRule, SizeValue, Padding, HorizontalAlignment, VerticalAlignment
 *   "Overlay"       -> Padding, HorizontalAlignment, VerticalAlignment
 *   "None"          -> root widget (no parent slot)
 */
USTRUCT(BlueprintType)
struct FWidgetSlotInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString SlotType; // "Canvas", "VerticalBox", "HorizontalBox", "Overlay", "None", or raw class name

	// Canvas slot fields
	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FVector2D AnchorMin = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FVector2D AnchorMax = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FMargin Offsets; // position+size (or margins) depending on anchor mode

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FVector2D Alignment = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	int32 ZOrder = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	bool bAutoSize = false;

	// Box slot fields (VerticalBox / HorizontalBox)
	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString SizeRule; // "Fill" or "Automatic"

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	float SizeValue = 1.0f; // Fill coefficient

	// Box + Overlay slot fields
	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FMargin Padding;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment = HAlign_Fill;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment = VAlign_Fill;
};

/**
 * Full state snapshot of a single widget component.
 * Combines hierarchy info, slot layout, and all properties in one struct.
 */
USTRUCT(BlueprintType)
struct FWidgetComponentSnapshot
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

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FWidgetSlotInfo SlotInfo;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	TArray<FWidgetPropertyInfo> Properties;
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
 * Font configuration for text-based widget components.
 */
USTRUCT(BlueprintType)
struct FWidgetFontInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Font")
	FString FontFamily;

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Font")
	FString Typeface = TEXT("Regular");

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Font")
	int32 Size = 12;

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Font")
	int32 LetterSpacing = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Font")
	FString Color = TEXT("(R=1.0,G=1.0,B=1.0,A=1.0)");

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Font")
	FString ShadowOffset = TEXT("(X=0.0,Y=0.0)");

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Font")
	FString ShadowColor = TEXT("(R=0.0,G=0.0,B=0.0,A=0.5)");

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Font")
	int32 OutlineSize = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Font")
	FString OutlineColor = TEXT("(R=0.0,G=0.0,B=0.0,A=1.0)");
};

/**
 * Brush configuration for widget components that expose FSlateBrush values.
 */
USTRUCT(BlueprintType)
struct FWidgetBrushInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Brush")
	FString ResourcePath;

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Brush")
	FString TintColor = TEXT("(R=1.0,G=1.0,B=1.0,A=1.0)");

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Brush")
	FString DrawAs = TEXT("Image");

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Brush")
	FString ImageSize = TEXT("(X=0.0,Y=0.0)");

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Brush")
	FString Margin = TEXT("(Left=0.0,Top=0.0,Right=0.0,Bottom=0.0)");

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Brush")
	FString CornerRadius = TEXT("(TopLeft=0.0,TopRight=0.0,BottomRight=0.0,BottomLeft=0.0)");
};

/**
 * Keyframe data for a widget animation property track.
 */
USTRUCT(BlueprintType)
struct FWidgetAnimKeyframe
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Animation")
	float Time = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Animation")
	FString Value;

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Animation")
	FString Interpolation = TEXT("Linear");
};

/**
 * Summary information about a widget animation.
 */
USTRUCT(BlueprintType)
struct FWidgetAnimInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Animation")
	FString AnimationName;

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Animation")
	float Duration = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Animation")
	int32 TrackCount = 0;
};

/**
 * Result of capturing an off-screen widget preview render.
 */
USTRUCT(BlueprintType)
struct FWidgetPreviewResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Preview")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Preview")
	FString OutputPath;

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Preview")
	int32 Width = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Preview")
	int32 Height = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Widget|Preview")
	FString ErrorMessage;
};

/**
 * Handle for a widget instance spawned during PIE.
 */
USTRUCT(BlueprintType)
struct FPIEWidgetHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Widget|PIE")
	bool bValid = false;

	UPROPERTY(BlueprintReadWrite, Category = "Widget|PIE")
	FString InstanceId;

	UPROPERTY(BlueprintReadWrite, Category = "Widget|PIE")
	FString ErrorMessage;
};

/**
 * Widget service exposed directly to Python.
 *
 * Provides 41 widget management actions:
 * - list_widget_blueprints: List all Widget Blueprints in the project
 * - get_hierarchy: Get widget hierarchy tree
 * - get_root_widget: Get the root widget of a Widget Blueprint
 * - list_components: List all widget components in a Widget Blueprint
 * - search_types: Get available widget types (native types + discovered WBPs for reference)
 * - get_component_properties: Get properties for a specific component
 * - add_component: Add a widget component to a Widget Blueprint (native types or custom WBPs by name)
 * - remove_component: Remove a widget component from a Widget Blueprint
 * - rename_widget: Rename a widget component in a Widget Blueprint
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
 * - set_font: Apply a full font configuration to a text widget
 * - get_font: Read a full font configuration from a text widget
 * - set_brush: Apply a brush to a widget brush slot
 * - get_brush: Read a brush from a widget brush slot
 * - list_animations: List all widget animations on a Widget Blueprint
 * - create_animation: Create a new widget animation
 * - remove_animation: Remove a widget animation
 * - add_animation_track: Add a property track to a widget animation
 * - add_keyframe: Add a keyframe to a widget animation track
 * - capture_preview: Render a widget preview to a PNG file
 * - start_pie: Start Play-In-Editor
 * - stop_pie: Stop Play-In-Editor
 * - is_pie_running: Check whether PIE is active
 * - spawn_widget_in_pie: Spawn a widget instance into the PIE viewport
 * - get_live_property: Read a property from a live PIE widget instance
 * - remove_widget_from_pie: Remove a spawned PIE widget instance from the viewport
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
class VIBEUE_API UWidgetService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	// =================================================================
	// Discovery Methods (list_components, search_types, get_component_properties)
	// =================================================================







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
	 * @param ChildIndex - Position among the parent's children to insert at (0-based). -1 (default) appends at
	 *        the end. Any other value must be within [0, parent's current child count] or the call fails with
	 *        an error - it does not silently fall back to append
	 * @return Result with success status and details
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Widgets")
	static FWidgetAddComponentResult AddComponent(
		const FString& WidgetPath,
		const FString& ComponentType,
		const FString& ComponentName,
		const FString& ParentName = TEXT(""),
		bool bIsVariable = true,
		int32 ChildIndex = -1);

	/**
	 * Move an existing widget component to a new position among its current parent's children.
	 * Maps to action="reorder_component"
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @param ComponentName - Name of the component to move
	 * @param NewIndex - New 0-based position among the parent's children
	 * @return True if the widget was found, has a panel parent, and was moved
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Widgets")
	static bool ReorderComponent(
		const FString& WidgetPath,
		const FString& ComponentName,
		int32 NewIndex);

	/**
	 * Remove a widget component from a Widget Blueprint.
	 * Maps to action="remove_component"
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @param ComponentName - Name of the component to remove
	 * @param bRemoveChildren - Whether to also remove child widgets
	 * @return Result with removed components and orphaned children
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Widgets")
	static FWidgetRemoveComponentResult RemoveComponent(
		const FString& WidgetPath,
		const FString& ComponentName,
		bool bRemoveChildren = false);

	/**
	 * Rename a widget component in the Widget Blueprint.
	 * Maps to action="rename_widget"
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @param OldName - Current name of the widget component
	 * @param NewName - New name for the widget component
	 * @return True if the rename was successful
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Widgets")
	static bool RenameWidget(
		const FString& WidgetPath,
		const FString& OldName,
		const FString& NewName);

	/**
	 * Move an existing widget to a new parent panel (re-parent), preserving the widget object.
	 * Maps to action="reparent_widget".
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @param WidgetName - Name of the widget to move
	 * @param NewParentName - Name of the destination panel widget (must be a UPanelWidget)
	 * @return True if the widget was re-slotted under the new parent
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Widgets")
	static bool ReparentWidget(
		const FString& WidgetPath,
		const FString& WidgetName,
		const FString& NewParentName);

	// =================================================================
	// Validation (validate)
	// =================================================================


	// =================================================================
	// Property Access (get_property, set_property, list_properties)
	// =================================================================




	// =================================================================
	// Font and Brush Access
	// =================================================================





	// =================================================================
	// Animation Authoring
	// =================================================================






	// =================================================================
	// Preview Capture
	// =================================================================


	// =================================================================
	// PIE Widget Lifecycle
	// =================================================================




	/**
	 * Spawn a widget into the PIE viewport.
	 * Maps to action="spawn_widget_in_pie"
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Widgets|PIE")
	static FPIEWidgetHandle SpawnWidgetInPIE(
		const FString& WidgetPath,
		int32 ZOrder = 0);


	/**
	 * Remove a widget from the PIE viewport.
	 * Maps to action="remove_widget_from_pie"
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Widgets|PIE")
	static bool RemoveWidgetFromPIE(const FPIEWidgetHandle& Handle);

	// =================================================================
	// Event Handling (get_available_events, bind_events)
	// =================================================================


	/**
	 * Bind an event to a function in the Widget Blueprint.
	 * Maps to action="bind_events"
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @param WidgetName - Name of the widget component that owns the event (e.g., "PlayButton")
	 * @param EventName - Name of the event (e.g., "OnClicked")
	 * @param FunctionName - Name of the function to call
	 * @return True if binding was successful
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Widgets")
	static bool BindEvent(
		const FString& WidgetPath,
		const FString& WidgetName,
		const FString& EventName,
		const FString& FunctionName);

	// =================================================================
	// ViewModel Management (MVVM)
	// =================================================================



	/**
	 * Remove a ViewModel from a Widget Blueprint.
	 * Maps to action="remove_view_model"
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @param ViewModelName - Name of the ViewModel to remove
	 * @return True if the ViewModel was removed successfully
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Widgets|ViewModel")
	static bool RemoveViewModel(
		const FString& WidgetPath,
		const FString& ViewModelName);




	// =================================================================
	// Snapshot API (get_widget_snapshot, get_component_snapshot)
	// =================================================================



	// =================================================================
	// Existence Checks
	// =================================================================



private:
	/** Build slot info by casting Widget->Slot to the concrete slot type */
	static FWidgetSlotInfo BuildSlotInfo(class UWidget* Widget);

	/** Collect all properties from a widget (all flags, not editable-only) */
	static TArray<FWidgetPropertyInfo> CollectWidgetProperties(class UWidget* Widget);

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
