// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "Types/MVVMBindingMode.h"
#include "UWidgetService.generated.h"


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
 * Widget Service - the nine Widget Blueprint operations the engine's toolsets cannot do.
 *
 * Cut to the delta on the 5-9 branch. Nothing in Epic's surface mutates a `UWidgetTree` (its only
 * WidgetTree symbol is the read-only `WidgetStaticAnalysisToolset.GetWidgetTreeDepth`), which is
 * why the tree edits below still live here. Use the engine for everything else:
 *  - read the tree, its properties and its slot layout in one call:
 *    `WidgetStaticAnalysisToolset.DescribeWidget` (walk-order `Widgets[]`, each with
 *    `Properties[]` and `SlotProperties[]`; pass bSkipDefaultValues=false to see defaults too).
 *    Also `GetWidgetConnectionMap` (binding/event audit) and `TraceWidgetPropertyOverrides`
 *    ("who actually sets this property?")
 *  - any property read or write, fonts and brushes included -- `FSlateFontInfo` and `FSlateBrush`
 *    are plain USTRUCTs, so they are ordinary property writes now:
 *    `EditorToolset.ObjectToolset` (GetProperties / SetProperties / ListProperties /
 *    GetPropertySchema). Slot layout is a write on the widget's `Slot` object. Native widget
 *    types: `ObjectToolset.SearchSubclasses` with BaseClass=/Script/UMG.Widget
 *  - animations: `WidgetAnimationToolset` (CreateWidgetAnimation / ListWidgetAnimations /
 *    RenameWidgetAnimation / SetWidgetAnimationLength / RemoveWidgetAnimation /
 *    AddWidgetToAnimation), then the MovieScene*Extensions chain for tracks and keys -- a
 *    `UWidgetAnimation` is a `UMovieSceneSequence`
 *  - ViewModels and MVVM bindings: `MVVMToolset` (AddViewModelToWidget / ListWidgetViewModels /
 *    ListWidgetViewBindings / CreateViewBinding / RemoveWidgetViewBinding / SetBindingMode /
 *    ListWidgetViewEvents)
 *  - asset existence, discovery, validation and save: `EditorToolset.AssetToolset`
 *    (Exists / FindAssets / ValidateAssets / SaveAsset); compile:
 *    `BlueprintToolsets.BlueprintToolset.CompileBlueprint`
 *  - PIE lifecycle and the designer: `EditorToolset.EditorAppToolset` (StartPIE -- async --,
 *    StopPIE, IsPIERunning, OpenEditorForAsset). A live widget's property is
 *    `SlateInspectorToolset.Snapshot` -> `GetUMGInfo` -> `ObjectToolset.GetProperties`, and the
 *    only preview capture left is `SlateInspectorToolset.Screenshot` against the open designer
 *    (`EditorAppToolset.CaptureAssetImage` refuses Widget Blueprints outright)
 *
 * What is left is the five tree mutations, the event bind, the two PIE-instance calls and the one
 * MVVM removal that none of those can do.
 */
UCLASS(BlueprintType)
class VIBEUE_API UWidgetService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	// =================================================================
	// Widget tree mutation
	// =================================================================

	/**
	 * Add a new widget component to a Widget Blueprint.
	 * Supports both native widget types (TextBlock, Button, etc.) and custom Widget Blueprints by name.
	 * Custom WBPs are resolved via the Asset Registry and compiled before use.
	 * Circular references (a WBP containing itself) are detected and rejected.
	 *
	 * An empty ParentName makes the widget the tree's root - that, not bIsVariable, is what makes a
	 * root. Widget names are unique per Widget Blueprint, not per parent, so reusing a name across
	 * sibling subtrees lands the component on the wrong parent or no-ops.
	 *
	 * Example:
	 *   unreal.WidgetService.add_component(path, "CanvasPanel", "RootCanvas", "", True)
	 *   unreal.WidgetService.add_component(path, "Button", "PlayButton", "RootCanvas", False)
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
	// PIE widget instances
	// =================================================================

	/**
	 * Construct a widget instance and add it to the PIE viewport.
	 *
	 * No engine tool constructs a UUserWidget or calls AddToViewport -- CommonUIToolset.ActivateWidget
	 * only addresses widgets the game already created. Start PIE first with
	 * EditorToolset.EditorAppToolset.StartPIE (async: it is not live in the same call), then read the
	 * live instance through SlateInspectorToolset.Snapshot -> GetUMGInfo -> ObjectToolset.GetProperties.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Widgets|PIE")
	static FPIEWidgetHandle SpawnWidgetInPIE(
		const FString& WidgetPath,
		int32 ZOrder = 0);

	/** Remove a widget instance spawned by SpawnWidgetInPIE from the PIE viewport. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Widgets|PIE")
	static bool RemoveWidgetFromPIE(const FPIEWidgetHandle& Handle);

	// =================================================================
	// Event binding
	// =================================================================

	/**
	 * Bind an event to a function in the Widget Blueprint.
	 *
	 * The engine equivalent, BlueprintToolsets.BlueprintToolset.AddComponentBoundEvent, is typed
	 * UActorComponent* and a UWidget is not one, so this is the only path. Hand-built bound-event
	 * nodes compile but never fire. Available event names come from MVVMToolset.ListWidgetViewEvents.
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
	// MVVM removal
	// =================================================================

	/**
	 * Remove a ViewModel from a Widget Blueprint.
	 *
	 * MVVMToolset can add a ViewModel to a widget (AddViewModelToWidget) and list them
	 * (ListWidgetViewModels) but has no tool that removes one. Individual bindings are a different
	 * thing and do have one: MVVMToolset.RemoveWidgetViewBinding, which takes a binding GUID.
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @param ViewModelName - Name of the ViewModel to remove
	 * @return True if the ViewModel was removed successfully
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Widgets|ViewModel")
	static bool RemoveViewModel(
		const FString& WidgetPath,
		const FString& ViewModelName);

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
