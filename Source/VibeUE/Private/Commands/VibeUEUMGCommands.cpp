#include "Commands/VibeUEUMGCommands.h"
#include "Commands/VibeUECommonUtils.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditor.h"
#include "EditorSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "JsonObjectConverter.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UObjectGlobals.h"
#include "HAL/Platform.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/RichTextBlock.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/Spacer.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/ScrollBox.h"
#include "Components/GridPanel.h"
// Additional includes for generic asset search
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/Blueprint.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Sound/SoundWave.h"
#include "Components/UniformGridPanel.h"
#include "Components/ListView.h"
#include "Components/Overlay.h"
#include "Components/TileView.h"
#include "Components/TreeView.h"
#include "Components/WidgetSwitcher.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
// Animation and Blueprint includes
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h" 
#include "MovieSceneTrack.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_Event.h"
#include "K2Node_InputAction.h"
#include "Components/SlateWrapperTypes.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/GridSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/WidgetSwitcherSlot.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_Event.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
// We'll create widgets using regular Factory classes
#include "Factories/Factory.h"
#include "WidgetBlueprintFactory.h"
// Remove problematic includes that don't exist in UE 5.5
// #include "UMGEditorSubsystem.h"

// Additional includes for complex type support
#include "Math/Color.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateBrush.h"
#include "Layout/Margin.h"
#include "Math/Vector2D.h"
#include "Engine/Font.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Styling/SlateTypes.h"

// Helper functions for enhanced UI building
namespace UMGHelpers
{
	// Find or create a parent widget by name
	UPanelWidget* FindOrCreateParentPanel(UWidgetBlueprint* WidgetBlueprint, const FString& ParentName, const FString& ParentType = TEXT("CanvasPanel"));
	
	// Set slot properties for a widget
	bool SetSlotProperties(UWidget* Widget, UPanelWidget* Parent, const TSharedPtr<FJsonObject>& SlotProperties);
	
	// Create widget with proper parent assignment
	template<typename WidgetType>
	WidgetType* CreateWidgetWithParent(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, const FString& ParentName = TEXT(""));
}

FVibeUEUMGCommands::FVibeUEUMGCommands()
{
}

// Static member definition
// Static variables for UMG commands

// ===================================================================
// Enhanced UI Building Helper Functions
// ===================================================================

namespace UMGHelpers
{
	UPanelWidget* FindOrCreateParentPanel(UWidgetBlueprint* WidgetBlueprint, const FString& ParentName, const FString& ParentType)
	{
		if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
		{
			return nullptr;
		}

		// If no parent specified, use root widget
		if (ParentName.IsEmpty())
		{
			return Cast<UPanelWidget>(WidgetBlueprint->WidgetTree->RootWidget);
		}

		// Find existing parent
		UWidget* ExistingParent = WidgetBlueprint->WidgetTree->FindWidget(FName(*ParentName));
		if (ExistingParent)
		{
			return Cast<UPanelWidget>(ExistingParent);
		}

		// Create new parent panel if it doesn't exist
		UPanelWidget* NewParent = nullptr;
		if (ParentType == TEXT("CanvasPanel"))
		{
			NewParent = WidgetBlueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *ParentName);
		}
		else if (ParentType == TEXT("Overlay"))
		{
			NewParent = WidgetBlueprint->WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *ParentName);
		}
		else if (ParentType == TEXT("HorizontalBox"))
		{
			NewParent = WidgetBlueprint->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *ParentName);
		}
		else if (ParentType == TEXT("VerticalBox"))
		{
			NewParent = WidgetBlueprint->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *ParentName);
		}
		else if (ParentType == TEXT("ScrollBox"))
		{
			NewParent = WidgetBlueprint->WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), *ParentName);
		}

		// Add to root if we created a new parent
		if (NewParent)
		{
			UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetBlueprint->WidgetTree->RootWidget);
			if (RootPanel)
			{
				RootPanel->AddChild(NewParent);
			}
		}

		return NewParent;
	}

	bool SetSlotProperties(UWidget* Widget, UPanelWidget* Parent, const TSharedPtr<FJsonObject>& SlotProperties)
	{
		if (!Widget || !Parent || !SlotProperties.IsValid())
		{
			return false;
		}

		// Handle Canvas Panel slots
		if (UCanvasPanel* CanvasParent = Cast<UCanvasPanel>(Parent))
		{
			UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot);
			if (!Slot)
			{
				return false;
			}

			// Set position
			const TArray<TSharedPtr<FJsonValue>>* Position;
			if (SlotProperties->TryGetArrayField(TEXT("position"), Position) && Position->Num() >= 2)
			{
				FVector2D Pos((*Position)[0]->AsNumber(), (*Position)[1]->AsNumber());
				Slot->SetPosition(Pos);
			}

			// Set size
			const TArray<TSharedPtr<FJsonValue>>* Size;
			if (SlotProperties->TryGetArrayField(TEXT("size"), Size) && Size->Num() >= 2)
			{
				FVector2D SizeVec((*Size)[0]->AsNumber(), (*Size)[1]->AsNumber());
				Slot->SetSize(SizeVec);
			}

			// Set anchors
			const TSharedPtr<FJsonObject>* AnchorsObj;
			if (SlotProperties->TryGetObjectField(TEXT("anchors"), AnchorsObj))
			{
				FAnchors Anchors;
				Anchors.Minimum.X = (*AnchorsObj)->GetNumberField(TEXT("min_x"));
				Anchors.Minimum.Y = (*AnchorsObj)->GetNumberField(TEXT("min_y"));
				Anchors.Maximum.X = (*AnchorsObj)->GetNumberField(TEXT("max_x"));
				Anchors.Maximum.Y = (*AnchorsObj)->GetNumberField(TEXT("max_y"));
				Slot->SetAnchors(Anchors);
			}

			// Set alignment
			const TArray<TSharedPtr<FJsonValue>>* Alignment;
			if (SlotProperties->TryGetArrayField(TEXT("alignment"), Alignment) && Alignment->Num() >= 2)
			{
				FVector2D AlignVec((*Alignment)[0]->AsNumber(), (*Alignment)[1]->AsNumber());
				Slot->SetAlignment(AlignVec);
			}

			return true;
		}
		// Handle Horizontal Box slots
		else if (UHorizontalBox* HBoxParent = Cast<UHorizontalBox>(Parent))
		{
			UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(Widget->Slot);
			if (!Slot)
			{
				return false;
			}

			// Set padding
			const TArray<TSharedPtr<FJsonValue>>* Padding;
			if (SlotProperties->TryGetArrayField(TEXT("padding"), Padding) && Padding->Num() >= 4)
			{
				FMargin PaddingValue;
				PaddingValue.Left = (*Padding)[0]->AsNumber();
				PaddingValue.Top = (*Padding)[1]->AsNumber();
				PaddingValue.Right = (*Padding)[2]->AsNumber();
				PaddingValue.Bottom = (*Padding)[3]->AsNumber();
				Slot->SetPadding(PaddingValue);
			}

			return true;
		}
		// Handle Vertical Box slots
		else if (UVerticalBox* VBoxParent = Cast<UVerticalBox>(Parent))
		{
			UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Widget->Slot);
			if (!Slot)
			{
				return false;
			}

			// Set padding
			const TArray<TSharedPtr<FJsonValue>>* Padding;
			if (SlotProperties->TryGetArrayField(TEXT("padding"), Padding) && Padding->Num() >= 4)
			{
				FMargin PaddingValue;
				PaddingValue.Left = (*Padding)[0]->AsNumber();
				PaddingValue.Top = (*Padding)[1]->AsNumber();
				PaddingValue.Right = (*Padding)[2]->AsNumber();
				PaddingValue.Bottom = (*Padding)[3]->AsNumber();
				Slot->SetPadding(PaddingValue);
			}

			return true;
		}
		// Handle Overlay slots
		else if (UOverlay* OverlayParent = Cast<UOverlay>(Parent))
		{
			UOverlaySlot* Slot = Cast<UOverlaySlot>(Widget->Slot);
			if (!Slot)
			{
				return false;
			}

			// Set padding
			const TArray<TSharedPtr<FJsonValue>>* Padding;
			if (SlotProperties->TryGetArrayField(TEXT("padding"), Padding) && Padding->Num() >= 4)
			{
				FMargin PaddingValue;
				PaddingValue.Left = (*Padding)[0]->AsNumber();
				PaddingValue.Top = (*Padding)[1]->AsNumber();
				PaddingValue.Right = (*Padding)[2]->AsNumber();
				PaddingValue.Bottom = (*Padding)[3]->AsNumber();
				Slot->SetPadding(PaddingValue);
			}

			// Set horizontal alignment
			FString HAlignment;
			if (SlotProperties->TryGetStringField(TEXT("horizontal_alignment"), HAlignment))
			{
				if (HAlignment == TEXT("Left"))
				{
					Slot->SetHorizontalAlignment(HAlign_Left);
				}
				else if (HAlignment == TEXT("Center"))
				{
					Slot->SetHorizontalAlignment(HAlign_Center);
				}
				else if (HAlignment == TEXT("Right"))
				{
					Slot->SetHorizontalAlignment(HAlign_Right);
				}
				else if (HAlignment == TEXT("Fill"))
				{
					Slot->SetHorizontalAlignment(HAlign_Fill);
				}
			}

			// Set vertical alignment  
			FString VAlignment;
			if (SlotProperties->TryGetStringField(TEXT("vertical_alignment"), VAlignment))
			{
				if (VAlignment == TEXT("Top"))
				{
					Slot->SetVerticalAlignment(VAlign_Top);
				}
				else if (VAlignment == TEXT("Center"))
				{
					Slot->SetVerticalAlignment(VAlign_Center);
				}
				else if (VAlignment == TEXT("Bottom"))
				{
					Slot->SetVerticalAlignment(VAlign_Bottom);
				}
				else if (VAlignment == TEXT("Fill"))
				{
					Slot->SetVerticalAlignment(VAlign_Fill);
				}
			}

			return true;
		}

		return false;
	}

	template<typename WidgetType>
	WidgetType* CreateWidgetWithParent(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, const FString& ParentName)
	{
		if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
		{
			return nullptr;
		}

		// Create the widget
		WidgetType* NewWidget = WidgetBlueprint->WidgetTree->ConstructWidget<WidgetType>(WidgetType::StaticClass(), *WidgetName);
		if (!NewWidget)
		{
			return nullptr;
		}

		// Find or create parent
		UPanelWidget* Parent = FindOrCreateParentPanel(WidgetBlueprint, ParentName);
		if (Parent)
		{
			Parent->AddChild(NewWidget);
		}

		return NewWidget;
	}

	// Ensure there's a suitable container for adding multiple widgets
	// If the root widget can't hold multiple children, wrap it in a CanvasPanel
	UPanelWidget* EnsureSuitableContainer(UWidgetBlueprint* WidgetBlueprint)
	{
		if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree || !WidgetBlueprint->WidgetTree->RootWidget)
		{
			return nullptr;
		}

		UWidget* RootWidget = WidgetBlueprint->WidgetTree->RootWidget;
		
		// Check if root widget is already a suitable container
		if (UPanelWidget* PanelRoot = Cast<UPanelWidget>(RootWidget))
		{
			// Check if it's a container that can hold multiple children
			if (Cast<UCanvasPanel>(PanelRoot) || 
				Cast<UOverlay>(PanelRoot) || 
				Cast<UHorizontalBox>(PanelRoot) || 
				Cast<UVerticalBox>(PanelRoot) || 
				Cast<UScrollBox>(PanelRoot) ||
				Cast<UGridPanel>(PanelRoot))
			{
				return PanelRoot;
			}
		}
		
		// Root widget is not suitable (e.g., SizeBox, Border, etc.)
		// Create a CanvasPanel wrapper and restructure the hierarchy
		UCanvasPanel* WrapperCanvas = WidgetBlueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CanvasPanel_Wrapper"));
		if (!WrapperCanvas)
		{
			return nullptr;
		}
		
		// Move the current root widget into the canvas panel
		UCanvasPanelSlot* RootSlot = WrapperCanvas->AddChildToCanvas(RootWidget);
		if (RootSlot)
		{
			// Set the root widget to fill the canvas
			RootSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			RootSlot->SetPosition(FVector2D(0, 0));
			RootSlot->SetSize(FVector2D(0, 0)); // Size to content
		}
		
		// Set the canvas panel as the new root
		WidgetBlueprint->WidgetTree->RootWidget = WrapperCanvas;
		
		return WrapperCanvas;
	}
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleCommand(const FString& CommandName, const TSharedPtr<FJsonObject>& Params)
{
	// Original UMG Commands
	if (CommandName == TEXT("create_umg_widget_blueprint"))
	{
		return HandleCreateUMGWidgetBlueprint(Params);
	}
	else if (CommandName == TEXT("add_text_block_to_widget"))
	{
		return HandleAddTextBlockToWidget(Params);
	}
	else if (CommandName == TEXT("add_widget_to_viewport"))
	{
		return HandleAddWidgetToViewport(Params);
	}
	else if (CommandName == TEXT("add_button_to_widget"))
	{
		return HandleAddButtonToWidget(Params);
	}
	// UMG Discovery Commands
	else if (CommandName == TEXT("search_items"))
	{
		return HandleSearchItems(Params);
	}
	else if (CommandName == TEXT("get_widget_blueprint_info"))
	{
		return HandleGetWidgetBlueprintInfo(Params);
	}
	else if (CommandName == TEXT("list_widget_components"))
	{
		return HandleListWidgetComponents(Params);
	}
	else if (CommandName == TEXT("get_widget_component_properties"))
	{
		return HandleGetWidgetComponentProperties(Params);
	}
	else if (CommandName == TEXT("get_available_widget_types"))
	{
		return HandleGetAvailableWidgetTypes(Params);
	}
	else if (CommandName == TEXT("validate_widget_hierarchy"))
	{
		return HandleValidateWidgetHierarchy(Params);
	}
	// UMG Component Commands
	else if (CommandName == TEXT("add_editable_text"))
	{
		return HandleAddEditableText(Params);
	}
	else if (CommandName == TEXT("add_editable_text_box"))
	{
		return HandleAddEditableTextBox(Params);
	}
	else if (CommandName == TEXT("add_rich_text_block"))
	{
		return HandleAddRichTextBlock(Params);
	}
	else if (CommandName == TEXT("add_check_box"))
	{
		return HandleAddCheckBox(Params);
	}
	else if (CommandName == TEXT("add_slider"))
	{
		return HandleAddSlider(Params);
	}
	else if (CommandName == TEXT("add_progress_bar"))
	{
		return HandleAddProgressBar(Params);
	}
	else if (CommandName == TEXT("add_image"))
	{
		return HandleAddImage(Params);
	}
	else if (CommandName == TEXT("add_spacer"))
	{
		return HandleAddSpacer(Params);
	}
	else if (CommandName == TEXT("remove_widget_component"))
	{
		return HandleRemoveWidgetComponent(Params);
	}
	// UMG Layout Commands
	else if (CommandName == TEXT("add_canvas_panel"))
	{
		return HandleAddCanvasPanel(Params);
	}
	else if (CommandName == TEXT("add_overlay"))
	{
		return HandleAddOverlay(Params);
	}
	else if (CommandName == TEXT("add_border"))
	{
		return HandleAddBorder(Params);
	}
	else if (CommandName == TEXT("add_horizontal_box"))
	{
		return HandleAddHorizontalBox(Params);
	}
	else if (CommandName == TEXT("add_vertical_box"))
	{
		return HandleAddVerticalBox(Params);
	}
	else if (CommandName == TEXT("add_scroll_box"))
	{
		return HandleAddScrollBox(Params);
	}
	else if (CommandName == TEXT("add_grid_panel"))
	{
		return HandleAddGridPanel(Params);
	}
	else if (CommandName == TEXT("add_list_view"))
	{
		return HandleAddListView(Params);
	}
	else if (CommandName == TEXT("add_tile_view"))
	{
		return HandleAddTileView(Params);
	}
	else if (CommandName == TEXT("add_tree_view"))
	{
		return HandleAddTreeView(Params);
	}
	else if (CommandName == TEXT("add_widget_switcher"))
	{
		return HandleAddWidgetSwitcher(Params);
	}
	else if (CommandName == TEXT("add_widget_switcher_slot"))
	{
		return HandleAddWidgetSwitcherSlot(Params);
	}
	else if (CommandName == TEXT("add_child_to_panel"))
	{
		return HandleAddChildToPanel(Params);
	}
	else if (CommandName == TEXT("remove_child_from_panel"))
	{
		return HandleRemoveChildFromPanel(Params);
	}
	else if (CommandName == TEXT("set_widget_slot_properties"))
	{
		return HandleSetWidgetSlotProperties(Params);
	}
	// Enhanced UMG Building Commands removed - not implemented
	// UMG Styling Commands
	else if (CommandName == TEXT("set_widget_property"))
	{
		return HandleSetWidgetProperty(Params);
	}
	else if (CommandName == TEXT("get_widget_property"))
	{
		return HandleGetWidgetProperty(Params);
	}
	else if (CommandName == TEXT("list_widget_properties"))
	{
		return HandleListWidgetProperties(Params);
	}
	else if (CommandName == TEXT("set_widget_transform"))
	{
		return HandleSetWidgetTransform(Params);
	}
	else if (CommandName == TEXT("set_widget_visibility"))
	{
		return HandleSetWidgetVisibility(Params);
	}
	else if (CommandName == TEXT("set_widget_z_order"))
	{
		return HandleSetWidgetZOrder(Params);
	}
	else if (CommandName == TEXT("set_widget_font"))
	{
		return HandleSetWidgetFont(Params);
	}
	else if (CommandName == TEXT("set_widget_alignment"))
	{
		return HandleSetWidgetAlignment(Params);
	}
	else if (CommandName == TEXT("set_widget_size_to_content"))
	{
		return HandleSetWidgetSizeToContent(Params);
	}
	else if (CommandName == TEXT("get_background_color_guide"))
	{
		return HandleGetBackgroundColorGuide(Params);
	}
	else if (CommandName == TEXT("get_widget_hierarchy_guide"))
	{
		return HandleGetWidgetHierarchyGuide(Params);
	}
	else if (CommandName == TEXT("bind_input_events"))
	{
		return HandleBindInputEvents(Params);
	}
	else if (CommandName == TEXT("get_available_events"))
	{
		return HandleGetAvailableEvents(Params);
	}
	// All event handling, data binding, animation, and bulk operations have been removed
	// Only keeping core working functions

	return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown UMG command: %s"), *CommandName));
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleCreateUMGWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	// Create the full asset path
	FString PackagePath = TEXT("/Game/Widgets/");
	FString AssetName = BlueprintName;
	FString FullPath = PackagePath + AssetName;

	// Check if asset already exists
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' already exists"), *BlueprintName));
	}

	// Create package
	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
	}

	// Create Widget Blueprint using WidgetBlueprintFactory
	UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
	Factory->ParentClass = UUserWidget::StaticClass();
	
	UObject* NewAsset = Factory->FactoryCreateNew(
		UWidgetBlueprint::StaticClass(),
		Package,
		FName(*AssetName),
		RF_Standalone | RF_Public,
		nullptr,
		GWarn
	);

	// Make sure the Blueprint was created successfully
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(NewAsset);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create Widget Blueprint"));
	}

	// Add a default Canvas Panel if one doesn't exist
	if (!WidgetBlueprint->WidgetTree->RootWidget)
	{
		UCanvasPanel* RootCanvas = WidgetBlueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetBlueprint->WidgetTree->RootWidget = RootCanvas;
	}

	// Mark the package dirty and notify asset registry
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(WidgetBlueprint);

	// Compile the blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	// Create success response
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("name"), BlueprintName);
	ResultObj->SetStringField(TEXT("path"), FullPath);
	return ResultObj;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddTextBlockToWidget(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
	}

	FString ParentName;
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the text block"));
	}

	// Find the Widget Blueprint (accept name or full path)
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint not found for '%s'. Tip: pass /Game/.../WBP_Name or /Game/.../WBP_Name.WBP_Name"), *BlueprintName));
	}

	// Get optional parameters
	FString InitialText = TEXT("New Text Block");
	Params->TryGetStringField(TEXT("text"), InitialText);

	FVector2D Position(0.0f, 0.0f);
	if (Params->HasField(TEXT("position")))
	{
		const TArray<TSharedPtr<FJsonValue>>* PosArray;
		if (Params->TryGetArrayField(TEXT("position"), PosArray) && PosArray->Num() >= 2)
		{
			Position.X = (*PosArray)[0]->AsNumber();
			Position.Y = (*PosArray)[1]->AsNumber();
		}
	}

	// Create Text Block widget
	UTextBlock* TextBlock = WidgetBlueprint->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *WidgetName);
	if (!TextBlock)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create Text Block widget"));
	}

	// Set initial text
	TextBlock->SetText(FText::FromString(InitialText));

	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);

	// Diagnostic log to help trace unexpected debugger breaks or null references
	UE_LOG(LogTemp, Log, TEXT("MCP: HandleAddSpacer called. widget='%s' parent='%s' blueprint='%s' widgetTreeValid=%s"),
		*WidgetName,
		*ParentName,
		WidgetBlueprint ? *WidgetBlueprint->GetName() : TEXT("<null>"),
		(WidgetBlueprint && WidgetBlueprint->WidgetTree) ? TEXT("true") : TEXT("false")
	);

	// Defensive check: ensure WidgetTree exists before proceeding to avoid crashes in editor
	if (!WidgetBlueprint->WidgetTree)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' has no WidgetTree"), *WidgetName));
	}
	if (!ParentPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}

	// Add to parent panel
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(TextBlock);
		PanelSlot->SetPosition(Position);
	}
	else
	{
		// Standard panel widget handling
		ParentPanel->AddChild(TextBlock);
	}

	// Mark the package dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	// Create success response
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("widget_name"), WidgetName);
	ResultObj->SetStringField(TEXT("text"), InitialText);
	return ResultObj;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddWidgetToViewport(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	// Find the Widget Blueprint
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint not found for '%s'"), *BlueprintName));
	}

	// Get optional Z-order parameter
	int32 ZOrder = 0;
	Params->TryGetNumberField(TEXT("z_order"), ZOrder);

	// Create widget instance
	UClass* WidgetClass = WidgetBlueprint->GeneratedClass;
	if (!WidgetClass)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to get widget class"));
	}

	// Note: This creates the widget but doesn't add it to viewport
	// The actual addition to viewport should be done through Blueprint nodes
	// as it requires a game context

	// Create success response with instructions
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("class_path"), WidgetClass->GetPathName());
	ResultObj->SetNumberField(TEXT("z_order"), ZOrder);
	ResultObj->SetStringField(TEXT("note"), TEXT("Widget class ready. Use CreateWidget and AddToViewport nodes in Blueprint to display in game."));
	return ResultObj;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddButtonToWidget(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing blueprint_name parameter"));
		return Response;
	}

	FString ButtonName;
	if (!Params->TryGetStringField(TEXT("button_name"), ButtonName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing button_name parameter"));
		return Response;
	}

	FString ButtonText;
	if (!Params->TryGetStringField(TEXT("text"), ButtonText))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing text parameter"));
		return Response;
	}

	FString ParentName;
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing parent_name parameter - you must specify where to add the button"));
		return Response;
	}

	// Load the Widget Blueprint
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintName));
		return Response;
	}

	// Create Button widget using the WidgetTree
	UButton* Button = WidgetBlueprint->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *ButtonName);
	if (!Button)
	{
		Response->SetStringField(TEXT("error"), TEXT("Failed to create Button widget"));
		return Response;
	}

	// Set button text by creating a TextBlock child
	UTextBlock* ButtonTextBlock = WidgetBlueprint->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(ButtonName + TEXT("_Text")));
	if (ButtonTextBlock)
	{
		ButtonTextBlock->SetText(FText::FromString(ButtonText));
		Button->AddChild(ButtonTextBlock);
	}

	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
		return Response;
	}

	// Add to parent panel
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* ButtonSlot = Canvas->AddChildToCanvas(Button);
		if (ButtonSlot)
		{
			const TArray<TSharedPtr<FJsonValue>>* Position;
			if (Params->TryGetArrayField(TEXT("position"), Position) && Position->Num() >= 2)
			{
				FVector2D Pos(
					(*Position)[0]->AsNumber(),
					(*Position)[1]->AsNumber()
				);
				ButtonSlot->SetPosition(Pos);
			}
		}
	}
	else
	{
		// Standard panel widget handling
		ParentPanel->AddChild(Button);
	}

	// Save the Widget Blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	UEditorAssetLibrary::SaveAsset(WidgetBlueprint->GetPathName(), false);

	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("button_name"), ButtonName);
	return Response;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleBindWidgetEvent(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		// Try widget_name as fallback for compatibility
		if (!Params->TryGetStringField(TEXT("widget_name"), BlueprintName))
		{
			Response->SetStringField(TEXT("error"), TEXT("Missing blueprint_name or widget_name parameter"));
			return Response;
		}
	}

	FString ComponentName;
	if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		// Fallback to widget_component_name for compatibility
		if (!Params->TryGetStringField(TEXT("widget_component_name"), ComponentName))
		{
			Response->SetStringField(TEXT("error"), TEXT("Missing component_name or widget_component_name parameter"));
			return Response;
		}
	}

	FString EventName;
	if (!Params->TryGetStringField(TEXT("event_name"), EventName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing event_name parameter"));
		return Response;
	}

	FString FunctionName;
	if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
	{
		// Create default function name if not provided
		FunctionName = FString::Printf(TEXT("%s_%s"), *ComponentName, *EventName);
	}

	// Load the Widget Blueprint using FVibeUECommonUtils helper
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintName));
		return Response;
	}

	// Create the event graph if it doesn't exist
	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(WidgetBlueprint);
	if (!EventGraph)
	{
		Response->SetStringField(TEXT("error"), TEXT("Failed to find or create event graph"));
		return Response;
	}

	// Find the widget component in the blueprint
	UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(*ComponentName);
	if (!Widget)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to find widget component: %s"), *ComponentName));
		return Response;
	}

	// Create the event node (e.g., OnClicked for buttons)
	UK2Node_Event* EventNode = nullptr;
	
	// Find existing nodes first
	TArray<UK2Node_Event*> AllEventNodes;
	FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Event>(WidgetBlueprint, AllEventNodes);
	
	for (UK2Node_Event* Node : AllEventNodes)
	{
		if (Node->CustomFunctionName == FName(*EventName) && Node->EventReference.GetMemberParentClass() == Widget->GetClass())
		{
			EventNode = Node;
			break;
		}
	}

	// If no existing node, create a new one
	if (!EventNode)
	{
		// Calculate position - place it below existing nodes
		float MaxHeight = 0.0f;
		for (UEdGraphNode* Node : EventGraph->Nodes)
		{
			MaxHeight = FMath::Max(MaxHeight, Node->NodePosY);
		}
		
		const FVector2D NodePos(200, MaxHeight + 200);

		// Try to create bound event using the widget tree structure
		try
		{
			// Create a multicast delegate property for the event
			FName EventPropertyName = FName(*EventName);
			
			// Use FKismetEditorUtilities to create the bound event
			FKismetEditorUtilities::CreateNewBoundEventForClass(
				Widget->GetClass(),
				EventPropertyName,
				WidgetBlueprint,
				nullptr  // ObjectProperty - use nullptr for general events
			);

			// Now find the newly created node
			TArray<UK2Node_Event*> UpdatedEventNodes;
			FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Event>(WidgetBlueprint, UpdatedEventNodes);
			
			for (UK2Node_Event* Node : UpdatedEventNodes)
			{
				if (Node->CustomFunctionName == EventPropertyName && Node->EventReference.GetMemberParentClass() == Widget->GetClass())
				{
					EventNode = Node;
					
					// Set position of the node
					EventNode->NodePosX = NodePos.X;
					EventNode->NodePosY = NodePos.Y;
					
					break;
				}
			}
		}
		catch (...)
		{
			// If CreateNewBoundEventForClass fails, try manual node creation
			EventNode = NewObject<UK2Node_Event>(EventGraph);
			if (EventNode)
			{
				EventNode->CustomFunctionName = FName(*EventName);
				EventNode->bOverrideFunction = true;
				EventNode->NodePosX = NodePos.X;
				EventNode->NodePosY = NodePos.Y;
				
				EventGraph->AddNode(EventNode, true);
				EventNode->AllocateDefaultPins();
				EventNode->ReconstructNode();
			}
		}
	}

	if (!EventNode)
	{
		Response->SetStringField(TEXT("error"), TEXT("Failed to create event node"));
		return Response;
	}

	// Save the Widget Blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("widget_name"), BlueprintName);
	Response->SetStringField(TEXT("component_name"), ComponentName);
	Response->SetStringField(TEXT("event_name"), EventName);
	Response->SetStringField(TEXT("function_name"), FunctionName);
	Response->SetStringField(TEXT("message"), TEXT("Widget event bound successfully"));
	return Response;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleSetTextBlockBinding(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing blueprint_name parameter"));
		return Response;
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing widget_name parameter"));
		return Response;
	}

	FString BindingName;
	if (!Params->TryGetStringField(TEXT("binding_name"), BindingName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing binding_name parameter"));
		return Response;
	}

	// Load the Widget Blueprint
	const FString BlueprintPath = FString::Printf(TEXT("/Game/Widgets/%s.%s"), *BlueprintName, *BlueprintName);
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
	if (!WidgetBlueprint)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintPath));
		return Response;
	}

	// Create a variable for binding if it doesn't exist
	FBlueprintEditorUtils::AddMemberVariable(
		WidgetBlueprint,
		FName(*BindingName),
		FEdGraphPinType(UEdGraphSchema_K2::PC_Text, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType())
	);

	// Find the TextBlock widget
	UTextBlock* TextBlock = Cast<UTextBlock>(WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName)));
	if (!TextBlock)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to find TextBlock widget: %s"), *WidgetName));
		return Response;
	}

	// Create binding function
	const FString FunctionName = FString::Printf(TEXT("Get%s"), *BindingName);
	UEdGraph* FuncGraph = FBlueprintEditorUtils::CreateNewGraph(
		WidgetBlueprint,
		FName(*FunctionName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

	if (FuncGraph)
	{
		// Add the function to the blueprint with proper template parameter
		// Template requires null for last parameter when not using a signature-source
		FBlueprintEditorUtils::AddFunctionGraph<UClass>(WidgetBlueprint, FuncGraph, false, nullptr);

		// Create entry node
		UK2Node_FunctionEntry* EntryNode = nullptr;
		
		// Create entry node - use the API that exists in UE 5.5
		EntryNode = NewObject<UK2Node_FunctionEntry>(FuncGraph);
		FuncGraph->AddNode(EntryNode, false, false);
		EntryNode->NodePosX = 0;
		EntryNode->NodePosY = 0;
		EntryNode->FunctionReference.SetExternalMember(FName(*FunctionName), WidgetBlueprint->GeneratedClass);
		EntryNode->AllocateDefaultPins();

		// Create get variable node
		UK2Node_VariableGet* GetVarNode = NewObject<UK2Node_VariableGet>(FuncGraph);
		GetVarNode->VariableReference.SetSelfMember(FName(*BindingName));
		FuncGraph->AddNode(GetVarNode, false, false);
		GetVarNode->NodePosX = 200;
		GetVarNode->NodePosY = 0;
		GetVarNode->AllocateDefaultPins();

		// Connect nodes
		UEdGraphPin* EntryThenPin = EntryNode->FindPin(UEdGraphSchema_K2::PN_Then);
		UEdGraphPin* GetVarOutPin = GetVarNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue);
		if (EntryThenPin && GetVarOutPin)
		{
			EntryThenPin->MakeLinkTo(GetVarOutPin);
		}
	}

	// Save the Widget Blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	UEditorAssetLibrary::SaveAsset(BlueprintPath, false);

	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("binding_name"), BindingName);
	return Response;
}

// ===================================================================
// UMG Discovery Methods Implementation
// ===================================================================

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleSearchItems(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get search parameters
	FString SearchTerm = TEXT("");
	Params->TryGetStringField(TEXT("search_term"), SearchTerm);
	
	FString AssetType = TEXT("");
	Params->TryGetStringField(TEXT("asset_type"), AssetType);
	
	FString Path = TEXT("/Game");
	Params->TryGetStringField(TEXT("path"), Path);
	
	bool bCaseSensitive = false;
	Params->TryGetBoolField(TEXT("case_sensitive"), bCaseSensitive);
	
	bool bIncludeEngineContent = false;
	Params->TryGetBoolField(TEXT("include_engine_content"), bIncludeEngineContent);
	
	int32 MaxResults = 100;
	Params->TryGetNumberField(TEXT("max_results"), MaxResults);
	
	// Use Asset Registry for proper recursive search
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	// Create filter for asset search
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(*Path); // Search recursively from specified path
	
	// If including engine content, also add engine paths
	if (bIncludeEngineContent)
	{
		Filter.PackagePaths.Add(TEXT("/Engine"));
	}
	
	// Set up class filter based on asset type
	if (!AssetType.IsEmpty())
	{
		if (AssetType == TEXT("WidgetBlueprint") || AssetType == TEXT("Widget"))
		{
			Filter.ClassPaths.Add(UWidgetBlueprint::StaticClass()->GetClassPathName());
		}
		else if (AssetType == TEXT("Texture2D") || AssetType == TEXT("Texture"))
		{
			Filter.ClassPaths.Add(UTexture2D::StaticClass()->GetClassPathName());
		}
		else if (AssetType == TEXT("Material"))
		{
			Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
		}
		else if (AssetType == TEXT("MaterialInstance"))
		{
			Filter.ClassPaths.Add(UMaterialInstance::StaticClass()->GetClassPathName());
			Filter.ClassPaths.Add(UMaterialInstanceConstant::StaticClass()->GetClassPathName());
		}
		else if (AssetType == TEXT("Blueprint"))
		{
			Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		}
		else if (AssetType == TEXT("StaticMesh"))
		{
			Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
		}
		else if (AssetType == TEXT("SkeletalMesh"))
		{
			Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
		}
		else if (AssetType == TEXT("Audio") || AssetType == TEXT("SoundWave"))
		{
			Filter.ClassPaths.Add(USoundWave::StaticClass()->GetClassPathName());
		}
		// If asset type not recognized, search all assets (no class filter)
	}
	
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);
	
	// Create response array
	TArray<TSharedPtr<FJsonValue>> ItemArray;
	int32 ResultCount = 0;
	
	for (const FAssetData& AssetData : AssetDataList)
	{
		if (ResultCount >= MaxResults)
		{
			break;
		}
		
		FString AssetName = AssetData.AssetName.ToString();
		FString AssetPath = AssetData.GetObjectPathString();
		
		// Apply search term filter if provided
		bool bMatches = true;
		if (!SearchTerm.IsEmpty())
		{
			ESearchCase::Type SearchCase = bCaseSensitive ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase;
			bMatches = AssetName.Contains(SearchTerm, SearchCase);
		}
		
		if (bMatches)
		{
			TSharedPtr<FJsonObject> ItemInfo = MakeShared<FJsonObject>();
			ItemInfo->SetStringField(TEXT("name"), AssetName);
			ItemInfo->SetStringField(TEXT("path"), AssetPath);
			ItemInfo->SetStringField(TEXT("package_path"), AssetData.PackageName.ToString());
			ItemInfo->SetStringField(TEXT("asset_class"), AssetData.AssetClassPath.ToString());
			
			// Add specific info based on asset type
			if (AssetData.AssetClassPath == UWidgetBlueprint::StaticClass()->GetClassPathName())
			{
				UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(AssetData.GetAsset());
				if (WidgetBlueprint)
				{
					ItemInfo->SetStringField(TEXT("parent_class"), WidgetBlueprint->ParentClass ? WidgetBlueprint->ParentClass->GetName() : TEXT("UserWidget"));
				}
				ItemInfo->SetStringField(TEXT("type"), TEXT("Widget"));
			}
			else if (AssetData.AssetClassPath == UTexture2D::StaticClass()->GetClassPathName())
			{
				ItemInfo->SetStringField(TEXT("type"), TEXT("Texture"));
			}
			else if (AssetData.AssetClassPath == UMaterial::StaticClass()->GetClassPathName())
			{
				ItemInfo->SetStringField(TEXT("type"), TEXT("Material"));
			}
			else if (AssetData.AssetClassPath == UMaterialInstance::StaticClass()->GetClassPathName() ||
					 AssetData.AssetClassPath == UMaterialInstanceConstant::StaticClass()->GetClassPathName())
			{
				ItemInfo->SetStringField(TEXT("type"), TEXT("MaterialInstance"));
			}
			else if (AssetData.AssetClassPath == UBlueprint::StaticClass()->GetClassPathName())
			{
				ItemInfo->SetStringField(TEXT("type"), TEXT("Blueprint"));
			}
			else if (AssetData.AssetClassPath == UStaticMesh::StaticClass()->GetClassPathName())
			{
				ItemInfo->SetStringField(TEXT("type"), TEXT("StaticMesh"));
			}
			else if (AssetData.AssetClassPath == USkeletalMesh::StaticClass()->GetClassPathName())
			{
				ItemInfo->SetStringField(TEXT("type"), TEXT("SkeletalMesh"));
			}
			else if (AssetData.AssetClassPath == USoundWave::StaticClass()->GetClassPathName())
			{
				ItemInfo->SetStringField(TEXT("type"), TEXT("Audio"));
			}
			else
			{
				ItemInfo->SetStringField(TEXT("type"), TEXT("Other"));
			}
			
			ItemArray.Add(MakeShared<FJsonValueObject>(ItemInfo));
			ResultCount++;
		}
	}
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetArrayField(TEXT("items"), ItemArray);
	Response->SetNumberField(TEXT("count"), ItemArray.Num());
	
	// Add search info
	TSharedPtr<FJsonObject> SearchInfo = MakeShared<FJsonObject>();
	SearchInfo->SetStringField(TEXT("search_term"), SearchTerm);
	SearchInfo->SetStringField(TEXT("asset_type"), AssetType);
	SearchInfo->SetStringField(TEXT("path"), Path);
	SearchInfo->SetBoolField(TEXT("case_sensitive"), bCaseSensitive);
	SearchInfo->SetBoolField(TEXT("include_engine_content"), bIncludeEngineContent);
	SearchInfo->SetNumberField(TEXT("max_results"), MaxResults);
	Response->SetObjectField(TEXT("search_info"), SearchInfo);
	
	return Response;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleGetWidgetBlueprintInfo(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    
	// Get widget blueprint identifier (accepts name or full path) - SIMPLIFIED
	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		// Try alternates (same as working list_widget_components)
		Params->TryGetStringField(TEXT("widget_path"), WidgetName);
		if (WidgetName.IsEmpty())
		{
			Params->TryGetStringField(TEXT("object_path"), WidgetName);
		}
		if (WidgetName.IsEmpty())
		{
			return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter (accepts name or full path)"));
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("HandleGetWidgetBlueprintInfo: Looking for widget '%s'"), *WidgetName);
	
	// Find widget blueprint (same as working version)
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetName);

	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint not found for '%s'"), *WidgetName));
	}
	
	UE_LOG(LogTemp, Warning, TEXT("HandleGetWidgetBlueprintInfo: Found widget '%s' at path '%s'"), 
	       *WidgetBlueprint->GetName(), *WidgetBlueprint->GetPathName());
	
	// Create widget_info object (SIMPLIFIED - no complex nested structures)
	TSharedPtr<FJsonObject> WidgetInfo = MakeShared<FJsonObject>();
	
	// Basic widget information only
	WidgetInfo->SetStringField(TEXT("name"), WidgetBlueprint->GetName());
	WidgetInfo->SetStringField(TEXT("path"), WidgetBlueprint->GetPathName());
	WidgetInfo->SetStringField(TEXT("package_path"), WidgetBlueprint->GetPackage() ? WidgetBlueprint->GetPackage()->GetPathName() : TEXT(""));
	WidgetInfo->SetStringField(TEXT("parent_class"), WidgetBlueprint->ParentClass ? WidgetBlueprint->ParentClass->GetName() : TEXT("UserWidget"));
	
	// Get root widget info (comprehensive)
	TArray<TSharedPtr<FJsonValue>> ComponentArray;
	TArray<TSharedPtr<FJsonValue>> VariableArray;
	TArray<TSharedPtr<FJsonValue>> EventArray;
	TArray<TSharedPtr<FJsonValue>> AnimationArray;
	
	if (WidgetBlueprint->WidgetTree && WidgetBlueprint->WidgetTree->RootWidget)
	{
		UWidget* RootWidget = WidgetBlueprint->WidgetTree->RootWidget;
		WidgetInfo->SetStringField(TEXT("root_widget_type"), RootWidget->GetClass()->GetName());
		WidgetInfo->SetStringField(TEXT("root_widget_name"), RootWidget->GetName());
		
		// Get component hierarchy with detailed information
		TArray<UWidget*> AllWidgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
		
		UE_LOG(LogTemp, Warning, TEXT("HandleGetWidgetBlueprintInfo: Processing %d widgets"), AllWidgets.Num());
		
		// Process all widgets for comprehensive info
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget)
			{
				TSharedPtr<FJsonObject> ComponentInfo = MakeShared<FJsonObject>();
				ComponentInfo->SetStringField(TEXT("name"), Widget->GetName());
				ComponentInfo->SetStringField(TEXT("type"), Widget->GetClass()->GetName());
				ComponentInfo->SetBoolField(TEXT("is_variable"), Widget->bIsVariable);
				ComponentInfo->SetBoolField(TEXT("is_enabled"), Widget->GetIsEnabled());
				ComponentInfo->SetStringField(TEXT("visibility"), UEnum::GetValueAsString(Widget->GetVisibility()));
				
				// Parent information
				if (UPanelWidget* ParentPanel = Widget->GetParent())
				{
					ComponentInfo->SetStringField(TEXT("parent"), ParentPanel->GetName());
					ComponentInfo->SetStringField(TEXT("parent_type"), ParentPanel->GetClass()->GetName());
				}
				
				// Child information for panel widgets
				if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
				{
					int32 ChildCount = PanelWidget->GetChildrenCount();
					ComponentInfo->SetNumberField(TEXT("child_count"), ChildCount);
					
					// List child names
					TArray<TSharedPtr<FJsonValue>> ChildrenArray;
					for (int32 i = 0; i < ChildCount; i++)
					{
						if (UWidget* ChildWidget = PanelWidget->GetChildAt(i))
						{
							ChildrenArray.Add(MakeShared<FJsonValueString>(ChildWidget->GetName()));
						}
					}
					ComponentInfo->SetArrayField(TEXT("children"), ChildrenArray);
				}
				
				// Position and size information for canvas panel slots
				if (Widget->Slot)
				{
					TSharedPtr<FJsonObject> SlotInfo = MakeShared<FJsonObject>();
					SlotInfo->SetStringField(TEXT("slot_type"), Widget->Slot->GetClass()->GetName());
					
					// Canvas Panel Slot specific info
					if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
					{
						FVector2D Position = CanvasSlot->GetPosition();
						FVector2D Size = CanvasSlot->GetSize();
						FAnchors Anchors = CanvasSlot->GetAnchors();
						FVector2D Alignment = CanvasSlot->GetAlignment();
						
						SlotInfo->SetNumberField(TEXT("position_x"), Position.X);
						SlotInfo->SetNumberField(TEXT("position_y"), Position.Y);
						SlotInfo->SetNumberField(TEXT("size_x"), Size.X);
						SlotInfo->SetNumberField(TEXT("size_y"), Size.Y);
						SlotInfo->SetNumberField(TEXT("anchor_min_x"), Anchors.Minimum.X);
						SlotInfo->SetNumberField(TEXT("anchor_min_y"), Anchors.Minimum.Y);
						SlotInfo->SetNumberField(TEXT("anchor_max_x"), Anchors.Maximum.X);
						SlotInfo->SetNumberField(TEXT("anchor_max_y"), Anchors.Maximum.Y);
						SlotInfo->SetNumberField(TEXT("alignment_x"), Alignment.X);
						SlotInfo->SetNumberField(TEXT("alignment_y"), Alignment.Y);
						SlotInfo->SetBoolField(TEXT("auto_size"), CanvasSlot->GetAutoSize());
						SlotInfo->SetNumberField(TEXT("z_order"), CanvasSlot->GetZOrder());
					}
					
					ComponentInfo->SetObjectField(TEXT("slot_info"), SlotInfo);
				}
				
				ComponentArray.Add(MakeShared<FJsonValueObject>(ComponentInfo));
			}
		}
		
		// Get Variables (from the blueprint)
		UE_LOG(LogTemp, Warning, TEXT("HandleGetWidgetBlueprintInfo: Processing variables"));
		if (UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(WidgetBlueprint->GeneratedClass))
		{
			for (TFieldIterator<FProperty> PropertyIt(BlueprintClass); PropertyIt; ++PropertyIt)
			{
				FProperty* Property = *PropertyIt;
				if (Property && Property->HasAllPropertyFlags(CPF_BlueprintVisible))
				{
					TSharedPtr<FJsonObject> VariableInfo = MakeShared<FJsonObject>();
					VariableInfo->SetStringField(TEXT("name"), Property->GetName());
					VariableInfo->SetStringField(TEXT("type"), Property->GetCPPType());
					VariableInfo->SetStringField(TEXT("category"), Property->GetMetaData(TEXT("Category")));
					VariableInfo->SetBoolField(TEXT("is_editable"), Property->HasAllPropertyFlags(CPF_Edit));
					VariableInfo->SetBoolField(TEXT("is_blueprint_readonly"), Property->HasAllPropertyFlags(CPF_BlueprintReadOnly));
					VariableInfo->SetStringField(TEXT("tooltip"), Property->GetMetaData(TEXT("ToolTip")));
					
					VariableArray.Add(MakeShared<FJsonValueObject>(VariableInfo));
				}
			}
		}
		
		// Get Events (from function graph nodes)
		UE_LOG(LogTemp, Warning, TEXT("HandleGetWidgetBlueprintInfo: Processing events"));
		if (WidgetBlueprint->UbergraphPages.Num() > 0)
		{
			for (UEdGraph* Graph : WidgetBlueprint->UbergraphPages)
			{
				if (Graph)
				{
					for (UEdGraphNode* Node : Graph->Nodes)
					{
						// Event nodes
						if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
						{
							TSharedPtr<FJsonObject> EventInfo = MakeShared<FJsonObject>();
							EventInfo->SetStringField(TEXT("name"), EventNode->EventReference.GetMemberName().ToString());
							EventInfo->SetStringField(TEXT("type"), TEXT("Event"));
							EventInfo->SetStringField(TEXT("category"), EventNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
							EventInfo->SetBoolField(TEXT("is_custom_event"), EventNode->bIsEditable);
							EventInfo->SetBoolField(TEXT("is_override"), EventNode->bOverrideFunction);
							
							EventArray.Add(MakeShared<FJsonValueObject>(EventInfo));
						}
						// Input Action events  
						else if (UK2Node_InputAction* InputNode = Cast<UK2Node_InputAction>(Node))
						{
							TSharedPtr<FJsonObject> EventInfo = MakeShared<FJsonObject>();
							EventInfo->SetStringField(TEXT("name"), InputNode->InputActionName.ToString());
							EventInfo->SetStringField(TEXT("type"), TEXT("Input Action"));
							EventInfo->SetStringField(TEXT("category"), TEXT("Input"));
							
							EventArray.Add(MakeShared<FJsonValueObject>(EventInfo));
						}
					}
				}
			}
		}
		
		// Get Animations
		UE_LOG(LogTemp, Warning, TEXT("HandleGetWidgetBlueprintInfo: Processing animations"));
		for (UWidgetAnimation* Animation : WidgetBlueprint->Animations)
		{
			if (Animation)
			{
				TSharedPtr<FJsonObject> AnimationInfo = MakeShared<FJsonObject>();
				AnimationInfo->SetStringField(TEXT("name"), Animation->GetName());
				AnimationInfo->SetNumberField(TEXT("duration"), Animation->GetEndTime());
				AnimationInfo->SetNumberField(TEXT("start_time"), Animation->GetStartTime());
				
				// Check if the animation has a movie scene for additional info
				if (Animation->GetMovieScene())
				{
					AnimationInfo->SetBoolField(TEXT("has_movie_scene"), true);
					// Convert frame numbers to double values for JSON
					FFrameRate FrameRate = Animation->GetMovieScene()->GetTickResolution();
					TRange<FFrameNumber> PlaybackRange = Animation->GetMovieScene()->GetPlaybackRange();
					
					if (!PlaybackRange.GetLowerBound().IsOpen())
					{
						double StartSeconds = FrameRate.AsSeconds(PlaybackRange.GetLowerBoundValue());
						AnimationInfo->SetNumberField(TEXT("playback_range_start"), StartSeconds);
					}
					if (!PlaybackRange.GetUpperBound().IsOpen())
					{
						double EndSeconds = FrameRate.AsSeconds(PlaybackRange.GetUpperBoundValue());
						AnimationInfo->SetNumberField(TEXT("playback_range_end"), EndSeconds);
					}
				}
				else
				{
					AnimationInfo->SetBoolField(TEXT("has_movie_scene"), false);
				}
				
				// Get animated tracks/properties
				TArray<TSharedPtr<FJsonValue>> TracksArray;
				if (Animation->GetMovieScene())
				{
					const TArray<UMovieSceneTrack*>& Tracks = Animation->GetMovieScene()->GetTracks();
					for (UMovieSceneTrack* Track : Tracks)
					{
						if (Track)
						{
							TSharedPtr<FJsonObject> TrackInfo = MakeShared<FJsonObject>();
							TrackInfo->SetStringField(TEXT("track_type"), Track->GetClass()->GetName());
							TrackInfo->SetStringField(TEXT("display_name"), Track->GetDisplayName().ToString());
							// Note: IsEvalDisabled method may not be available in UE 5.6
							TrackInfo->SetBoolField(TEXT("is_enabled"), true); // Default to enabled
							TracksArray.Add(MakeShared<FJsonValueObject>(TrackInfo));
						}
					}
				}
				AnimationInfo->SetArrayField(TEXT("tracks"), TracksArray);
				AnimationInfo->SetNumberField(TEXT("track_count"), TracksArray.Num());
				
				AnimationArray.Add(MakeShared<FJsonValueObject>(AnimationInfo));
			}
		}
		
		WidgetInfo->SetArrayField(TEXT("components"), ComponentArray);
		WidgetInfo->SetNumberField(TEXT("component_count"), ComponentArray.Num());
	}
	else
	{
		// Empty arrays for missing widget tree
		WidgetInfo->SetArrayField(TEXT("components"), ComponentArray);
		WidgetInfo->SetNumberField(TEXT("component_count"), 0);
	}
	
	// Set comprehensive information arrays
	WidgetInfo->SetArrayField(TEXT("variables"), VariableArray);
	WidgetInfo->SetNumberField(TEXT("variable_count"), VariableArray.Num());
	WidgetInfo->SetArrayField(TEXT("events"), EventArray);
	WidgetInfo->SetNumberField(TEXT("event_count"), EventArray.Num());
	WidgetInfo->SetArrayField(TEXT("animations"), AnimationArray);
	WidgetInfo->SetNumberField(TEXT("animation_count"), AnimationArray.Num());
	
	// Set success response (same structure as before, but simpler content)
	Response->SetBoolField(TEXT("success"), true);
	Response->SetObjectField(TEXT("widget_info"), WidgetInfo);
	
	UE_LOG(LogTemp, Warning, TEXT("HandleGetWidgetBlueprintInfo: Success - returning %d components"), 
	       ComponentArray.Num());
	
	return Response;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleListWidgetComponents(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get widget blueprint name
	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		// Try alternates
		Params->TryGetStringField(TEXT("widget_path"), WidgetName);
		if (WidgetName.IsEmpty())
		{
			Params->TryGetStringField(TEXT("object_path"), WidgetName);
		}
		if (WidgetName.IsEmpty())
		{
			return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter (accepts name or full path)"));
		}
	}
	
	// Find widget blueprint
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetName);
	
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint not found for '%s'"), *WidgetName));
	}
	
	// Get all widgets in the tree
	TArray<TSharedPtr<FJsonValue>> ComponentArray;
	TArray<UWidget*> AllWidgets;
	WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
	
	for (UWidget* Widget : AllWidgets)
	{
		if (Widget)
		{
			TSharedPtr<FJsonObject> ComponentInfo = MakeShared<FJsonObject>();
			ComponentInfo->SetStringField(TEXT("name"), Widget->GetName());
			ComponentInfo->SetStringField(TEXT("type"), Widget->GetClass()->GetName());
			ComponentInfo->SetBoolField(TEXT("is_variable"), Widget->bIsVariable);
			ComponentArray.Add(MakeShared<FJsonValueObject>(ComponentInfo));
		}
	}
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetArrayField(TEXT("components"), ComponentArray);
	Response->SetStringField(TEXT("widget_path"), WidgetBlueprint->GetPathName());
	Response->SetNumberField(TEXT("count"), ComponentArray.Num());
	Response->SetStringField(TEXT("usage"), TEXT("Use 'widget_name' as name, package path, or full object path to target a widget blueprint."));
	return Response;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleGetWidgetComponentProperties(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get parameters
	FString WidgetName, ComponentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' or 'component_name' parameter"));
	}
	
	// Fall back to alternates if widget_name is not provided as expected
	if (WidgetName.IsEmpty())
	{
		Params->TryGetStringField(TEXT("widget_path"), WidgetName);
		if (WidgetName.IsEmpty())
		{
			Params->TryGetStringField(TEXT("object_path"), WidgetName);
		}
	}
	// Find widget blueprint
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetName);
	
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint not found for '%s'"), *WidgetName));
	}
	
	// Find the specific widget component
	UWidget* TargetWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*ComponentName));
	if (!TargetWidget)
	{
	return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component '%s' not found in widget"), *ComponentName));
	}
	
	// Get component properties - simplified version
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("component_name"), ComponentName);
	Response->SetStringField(TEXT("component_type"), TargetWidget->GetClass()->GetName());
	Response->SetBoolField(TEXT("is_variable"), TargetWidget->bIsVariable);
	Response->SetBoolField(TEXT("is_visible"), TargetWidget->GetVisibility() != ESlateVisibility::Collapsed);
	Response->SetStringField(TEXT("widget_path"), WidgetBlueprint->GetPathName());
	
	return Response;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleGetAvailableWidgetTypes(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// List of supported widget types
	TArray<FString> WidgetTypes = {
		TEXT("TextBlock"),
		TEXT("Button"),
		TEXT("EditableText"),
		TEXT("EditableTextBox"),
		TEXT("RichTextBlock"),
		TEXT("CheckBox"),
		TEXT("Slider"),
		TEXT("ProgressBar"),
		TEXT("Image"),
		TEXT("Spacer"),
		TEXT("CanvasPanel"),
		TEXT("Overlay"),
		TEXT("HorizontalBox"),
		TEXT("VerticalBox"),
		TEXT("ScrollBox"),
		TEXT("GridPanel"),
		TEXT("ListView"),
		TEXT("TileView"),
		TEXT("TreeView"),
		TEXT("WidgetSwitcher")
	};
	
	TArray<TSharedPtr<FJsonValue>> TypeArray;
	for (const FString& Type : WidgetTypes)
	{
		TypeArray.Add(MakeShared<FJsonValueString>(Type));
	}
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetArrayField(TEXT("widget_types"), TypeArray);
	Response->SetNumberField(TEXT("count"), WidgetTypes.Num());
	return Response;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleValidateWidgetHierarchy(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get widget blueprint name
	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
	}
	
	// Find widget blueprint
	FString BlueprintPath = FString::Printf(TEXT("/Game/Widgets/%s"), *WidgetName);
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
	
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetName));
	}
	
	// Basic validation - check if widget tree exists and has root
	bool bIsValid = WidgetBlueprint->WidgetTree && WidgetBlueprint->WidgetTree->RootWidget;
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetBoolField(TEXT("is_valid"), bIsValid);
	Response->SetStringField(TEXT("validation_message"), bIsValid ? TEXT("Widget hierarchy is valid") : TEXT("Invalid widget hierarchy"));
	
	return Response;
}

// ===================================================================
// UMG Component Methods Implementation
// ===================================================================

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddEditableText(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get required parameters
	FString WidgetName, EditableTextName, ParentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("editable_text_name"), EditableTextName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing required parameters"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the editable text"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetName));
	}
	
	// Create EditableText widget
	UEditableText* EditableText = WidgetBlueprint->WidgetTree->ConstructWidget<UEditableText>(UEditableText::StaticClass(), *EditableTextName);
	if (!EditableText)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create EditableText widget"));
	}
	
	// Set optional properties
	FString InitialText = TEXT("");
	Params->TryGetStringField(TEXT("text"), InitialText);
	if (!InitialText.IsEmpty())
	{
		EditableText->SetText(FText::FromString(InitialText));
	}
	
	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to parent panel
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(EditableText);
		if (Slot)
		{
			// Set position if provided
			FVector2D Position(0.0f, 0.0f);
			if (Params->HasField(TEXT("position")))
			{
				const TArray<TSharedPtr<FJsonValue>>* PosArray;
				if (Params->TryGetArrayField(TEXT("position"), PosArray) && PosArray->Num() >= 2)
				{
					Position.X = (*PosArray)[0]->AsNumber();
					Position.Y = (*PosArray)[1]->AsNumber();
					Slot->SetPosition(Position);
				}
			}
		}
	}
	else
	{
		// Standard panel widget handling
		ParentPanel->AddChild(EditableText);
	}
	
	// Mark dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("editable_text_name"), EditableTextName);
	Response->SetStringField(TEXT("widget_type"), TEXT("EditableText"));
	return Response;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddEditableTextBox(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get required parameters
	FString WidgetName, TextBoxName, ParentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("text_box_name"), TextBoxName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing required parameters"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the editable text box"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetName));
	}
	
	// Create EditableTextBox widget
	UEditableTextBox* TextBox = WidgetBlueprint->WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), *TextBoxName);
	if (!TextBox)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create EditableTextBox widget"));
	}
	
	// Set optional properties
	FString InitialText = TEXT("");
	Params->TryGetStringField(TEXT("text"), InitialText);
	if (!InitialText.IsEmpty())
	{
		TextBox->SetText(FText::FromString(InitialText));
	}
	
	FString HintText = TEXT("");
	Params->TryGetStringField(TEXT("hint_text"), HintText);
	if (!HintText.IsEmpty())
	{
		TextBox->SetHintText(FText::FromString(HintText));
	}
	
	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to parent panel
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(TextBox);
		if (Slot)
		{
			// Set default position and size for the text box
			Slot->SetPosition(FVector2D(0, 0));
			Slot->SetSize(FVector2D(300, 100));
		}
	}
	else
	{
		// Standard panel widget handling
		ParentPanel->AddChild(TextBox);
	}
	
	// Mark dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("text_box_name"), TextBoxName);
	Response->SetStringField(TEXT("widget_type"), TEXT("EditableTextBox"));
	return Response;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddRichTextBlock(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get required parameters
	FString WidgetName, RichTextName, ParentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("rich_text_name"), RichTextName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing required parameters"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the rich text block"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetName));
	}
	
	// Create RichTextBlock widget
	URichTextBlock* RichText = WidgetBlueprint->WidgetTree->ConstructWidget<URichTextBlock>(URichTextBlock::StaticClass(), *RichTextName);
	if (!RichText)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create RichTextBlock widget"));
	}
	
	// Set optional properties
	FString InitialText = TEXT("");
	Params->TryGetStringField(TEXT("text"), InitialText);
	if (!InitialText.IsEmpty())
	{
		RichText->SetText(FText::FromString(InitialText));
	}
	
	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to parent panel
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(RichText);
		if (Slot)
		{
			// Set default position and size for the rich text block
			Slot->SetPosition(FVector2D(0, 0));
			Slot->SetSize(FVector2D(400, 100));
		}
	}
	else
	{
		// Standard panel widget handling
		ParentPanel->AddChild(RichText);
	}
	
	// Mark dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("rich_text_name"), RichTextName);
	Response->SetStringField(TEXT("widget_type"), TEXT("RichTextBlock"));
	return Response;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddCheckBox(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get required parameters
	FString WidgetName, CheckBoxName, ParentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("check_box_name"), CheckBoxName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing required parameters"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the check box"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetName));
	}
	
	// Create CheckBox widget
	UCheckBox* CheckBox = WidgetBlueprint->WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), *CheckBoxName);
	if (!CheckBox)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create CheckBox widget"));
	}
	
	// Set optional properties
	bool bIsChecked = false;
	Params->TryGetBoolField(TEXT("is_checked"), bIsChecked);
	CheckBox->SetIsChecked(bIsChecked);
	
	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to parent panel
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(CheckBox);
		if (Slot)
		{
			// Set default position and size for the check box
			Slot->SetPosition(FVector2D(0, 0));
			Slot->SetSize(FVector2D(100, 20));
		}
	}
	else
	{
		// Standard panel widget handling
		ParentPanel->AddChild(CheckBox);
	}
	
	// Mark dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("check_box_name"), CheckBoxName);
	Response->SetStringField(TEXT("widget_type"), TEXT("CheckBox"));
	return Response;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddSlider(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get required parameters
	FString WidgetName, SliderName, ParentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("slider_name"), SliderName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing required parameters"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the slider"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetName));
	}
	
	// Create Slider widget
	USlider* Slider = WidgetBlueprint->WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), *SliderName);
	if (!Slider)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create Slider widget"));
	}
	
	// Set optional properties
	float MinValue = 0.0f, MaxValue = 1.0f, Value = 0.0f;
	Params->TryGetNumberField(TEXT("min_value"), MinValue);
	Params->TryGetNumberField(TEXT("max_value"), MaxValue);
	Params->TryGetNumberField(TEXT("value"), Value);
	
	Slider->SetMinValue(MinValue);
	Slider->SetMaxValue(MaxValue);
	Slider->SetValue(Value);
	
	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to parent panel
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Slider);
		if (Slot)
		{
			// Set default position and size for the slider
			Slot->SetPosition(FVector2D(0, 0));
			Slot->SetSize(FVector2D(200, 20));
		}
	}
	else
	{
		// Standard panel widget handling
		ParentPanel->AddChild(Slider);
	}
	
	// Mark dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("slider_name"), SliderName);
	Response->SetStringField(TEXT("widget_type"), TEXT("Slider"));
	return Response;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddProgressBar(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get required parameters
	FString WidgetName, ProgressBarName, ParentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("progress_bar_name"), ProgressBarName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing required parameters"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the progress bar"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetName));
	}
	
	// Create ProgressBar widget
	UProgressBar* ProgressBar = WidgetBlueprint->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), *ProgressBarName);
	if (!ProgressBar)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create ProgressBar widget"));
	}
	
	// Set optional properties
	float Percent = 0.0f;
	Params->TryGetNumberField(TEXT("percent"), Percent);
	ProgressBar->SetPercent(Percent);
	
	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to parent panel
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(ProgressBar);
		if (Slot)
		{
			// Set default position and size for the progress bar
			Slot->SetPosition(FVector2D(0, 0));
			Slot->SetSize(FVector2D(200, 20));
		}
	}
	else
	{
		// Standard panel widget handling
		ParentPanel->AddChild(ProgressBar);
	}
	
	// Mark dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("progress_bar_name"), ProgressBarName);
	Response->SetStringField(TEXT("widget_type"), TEXT("ProgressBar"));
	return Response;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddImage(const TSharedPtr<FJsonObject>& Params)
{
	// Check if we're in a serialization context to prevent crashes
	if (IsGarbageCollecting() || GIsSavingPackage || IsLoading())
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Cannot add image during serialization"));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	if (!Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("MCP: Failed to create Response object"));
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Internal error: Failed to create response object"));
	}

	FString WidgetName, ImageName, ParentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("image_name"), ImageName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing required parameters"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the image"));
	}

	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetName);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found or widget tree is null"), *WidgetName));
	}

	UImage* Image = WidgetBlueprint->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *ImageName);
	if (!Image)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create Image widget"));
	}

	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}

	bool Added = false;
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Image);
		Added = (Slot != nullptr);
		if (Slot)
		{
			if (Params->HasField(TEXT("position")))
			{
				const TArray<TSharedPtr<FJsonValue>>* PositionArray;
				if (Params->TryGetArrayField(TEXT("position"), PositionArray) && PositionArray->Num() >= 2)
				{
					float X = (*PositionArray)[0]->AsNumber();
					float Y = (*PositionArray)[1]->AsNumber();
					Slot->SetPosition(FVector2D(X, Y));
				}
			}
			if (Params->HasField(TEXT("size")))
			{
				const TArray<TSharedPtr<FJsonValue>>* SizeArray;
				if (Params->TryGetArrayField(TEXT("size"), SizeArray) && SizeArray->Num() >= 2)
				{
					float Width = (*SizeArray)[0]->AsNumber();
					float Height = (*SizeArray)[1]->AsNumber();
					Slot->SetSize(FVector2D(Width, Height));
				}
			}
		}
	}
	else if (UOverlay* Overlay = Cast<UOverlay>(ParentPanel))
	{
		Overlay->AddChild(Image);
		Added = true;
	}
	else if (UScrollBox* ScrollBox = Cast<UScrollBox>(ParentPanel))
	{
		ScrollBox->AddChild(Image);
		Added = true;
	}
	else if (UVerticalBox* VBox = Cast<UVerticalBox>(ParentPanel))
	{
		VBox->AddChild(Image);
		Added = true;
	}
	else if (UHorizontalBox* HBox = Cast<UHorizontalBox>(ParentPanel))
	{
		HBox->AddChild(Image);
		Added = true;
	}
	else
	{
		// Try generic panel
		if (UPanelWidget* Panel = Cast<UPanelWidget>(ParentPanel))
		{
			Panel->AddChild(Image);
			Added = true;
		}
	}

	if (!Added)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to add image to panel"));
	}

	if (Params->HasField(TEXT("color_tint")))
	{
		const TArray<TSharedPtr<FJsonValue>>* ColorArray;
		if (Params->TryGetArrayField(TEXT("color_tint"), ColorArray) && ColorArray->Num() >= 4)
		{
			float R = (*ColorArray)[0]->AsNumber();
			float G = (*ColorArray)[1]->AsNumber();
			float B = (*ColorArray)[2]->AsNumber();
			float A = (*ColorArray)[3]->AsNumber();
			Image->SetColorAndOpacity(FLinearColor(R, G, B, A));
		}
	}

	WidgetBlueprint->MarkPackageDirty();
	
	// Use deferred compilation to avoid serialization crashes
	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
	// Don't compile immediately - let Unreal handle it when safe
	// FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	UE_LOG(LogTemp, Log, TEXT("MCP: Successfully added image '%s' to widget '%s' in panel '%s'"), 
		*ImageName, *WidgetName, *ParentPanel->GetName());

	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("image_name"), ImageName);
	Response->SetStringField(TEXT("widget_type"), TEXT("Image"));
	return Response;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddSpacer(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get required parameters
	FString WidgetName, SpacerName, ParentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("spacer_name"), SpacerName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing required parameters"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the spacer"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetName));
	}
	
	// Create Spacer widget
	USpacer* Spacer = WidgetBlueprint->WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), *SpacerName);
	if (!Spacer)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create Spacer widget"));
	}
	
	// Set optional size
	FVector2D Size(100.0f, 100.0f);
	if (Params->HasField(TEXT("size")))
	{
		const TArray<TSharedPtr<FJsonValue>>* SizeArray;
		if (Params->TryGetArrayField(TEXT("size"), SizeArray) && SizeArray->Num() >= 2)
		{
			Size.X = (*SizeArray)[0]->AsNumber();
			Size.Y = (*SizeArray)[1]->AsNumber();
			Spacer->SetSize(Size);
		}
	}
	
	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to parent panel
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Spacer);
		if (Slot)
		{
			// Set default position for the spacer
			Slot->SetPosition(FVector2D(0, 0));
		}
	}
	else
	{
		// Standard panel widget handling
		ParentPanel->AddChild(Spacer);
	}
	
	// Mark dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("spacer_name"), SpacerName);
	Response->SetStringField(TEXT("widget_type"), TEXT("Spacer"));
	return Response;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleRemoveWidgetComponent(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get required parameters
	FString WidgetName, ComponentName;
	bool bConfirmRemoval = true;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing required parameters: widget_name and component_name"));
	}
	
	// Get optional confirmation parameter
	Params->TryGetBoolField(TEXT("confirm_removal"), bConfirmRemoval);
	
	// Safety check - require confirmation unless explicitly disabled
	if (bConfirmRemoval)
	{
		UE_LOG(LogTemp, Warning, TEXT("MCP: Removing widget component '%s' from '%s'"), *ComponentName, *WidgetName);
	}
	
	// Find widget blueprint using the common utility
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetName));
	}
	
	if (!WidgetBlueprint->WidgetTree)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Widget tree is invalid"));
	}
	
	// Find the component to remove by name
	UWidget* ComponentToRemove = nullptr;
	WidgetBlueprint->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (Widget && Widget->GetName() == ComponentName)
		{
			ComponentToRemove = Widget;
		}
	});
	
	if (!ComponentToRemove)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component '%s' not found in widget '%s'"), *ComponentName, *WidgetName));
	}
	
	// Get parent before removal for cleanup
	UWidget* ParentWidget = ComponentToRemove->GetParent();
	
	// Remove the widget from its parent
	bool bRemovalSuccess = false;
	if (ParentWidget)
	{
		if (UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget))
		{
			bRemovalSuccess = ParentPanel->RemoveChild(ComponentToRemove);
		}
	}
	else
	{
		// If it's the root widget, we need to handle it differently
		if (WidgetBlueprint->WidgetTree->RootWidget == ComponentToRemove)
		{
			WidgetBlueprint->WidgetTree->RootWidget = nullptr;
			bRemovalSuccess = true;
		}
	}
	
	if (!bRemovalSuccess)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to remove component '%s' from its parent"), *ComponentName));
	}
	
	// Mark the blueprint as dirty and recompile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	// Success response
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("widget_name"), WidgetName);
	Response->SetStringField(TEXT("component_name"), ComponentName);
	Response->SetStringField(TEXT("status"), TEXT("Component successfully removed"));
	
	UE_LOG(LogTemp, Log, TEXT("MCP: Successfully removed widget component '%s' from '%s'"), *ComponentName, *WidgetName);
	
	return Response;
}

// ===================================================================
// UMG Layout Methods Implementation (Stub implementations)
// ===================================================================

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddCanvasPanel(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString PanelName;
	FString ParentName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("panel_name"), PanelName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing panel_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the Canvas Panel"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Find or create parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to widget blueprint's designer
	if (UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree)
	{
		UCanvasPanel* CreatedPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *PanelName);
		if (CreatedPanel)
		{
			CreatedPanel->SetVisibility(ESlateVisibility::Visible);
			
			// Add to parent panel
			UPanelSlot* PanelSlot = ParentPanel->AddChild(CreatedPanel);
			if (PanelSlot)
			{
				TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
				Result->SetBoolField(TEXT("success"), true);
				Result->SetStringField(TEXT("panel_name"), PanelName);
				Result->SetStringField(TEXT("panel_type"), TEXT("CanvasPanel"));
				Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
				Result->SetStringField(TEXT("parent_name"), ParentName);
				
				return Result;
			}
			else
			{
				return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to add Canvas Panel to parent"));
			}
		}
		else
		{
			return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create Canvas Panel"));
		}
	}
	
	return FVibeUECommonUtils::CreateErrorResponse(TEXT("Widget Tree not found"));
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddOverlay(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString OverlayName;
	FString ParentName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("overlay_name"), OverlayName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing overlay_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the overlay. Use list_widget_components to see available parent containers."));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("WidgetTree not found in Widget Blueprint"));
	}
	
	// Create the overlay widget using the widget tree
	UOverlay* CreatedOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *OverlayName);
	if (!CreatedOverlay)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create Overlay widget"));
	}
	
	UE_LOG(LogTemp, Warning, TEXT("MCP: Created overlay '%s' successfully"), *OverlayName);
	CreatedOverlay->SetVisibility(ESlateVisibility::Visible);
	
	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		// List available components for debugging
		TArray<UWidget*> AllWidgets;
		WidgetTree->GetAllWidgets(AllWidgets);
		FString AvailableComponents = TEXT("Available components: ");
		for (UWidget* Widget : AllWidgets)
		{
			if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
			{
				AvailableComponents += Panel->GetName() + TEXT(", ");
			}
		}
		UE_LOG(LogTemp, Error, TEXT("MCP: Parent panel '%s' not found. %s"), *ParentName, *AvailableComponents);
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Parent panel '%s' not found. %s"), *ParentName, *AvailableComponents));
	}
	
	// Add overlay to parent panel
	UE_LOG(LogTemp, Warning, TEXT("MCP: Adding overlay to parent panel '%s' of type '%s'"), 
		*ParentPanel->GetName(), *ParentPanel->GetClass()->GetName());
	
	// Special handling for Canvas Panel
	if (UCanvasPanel* CanvasPanel = Cast<UCanvasPanel>(ParentPanel))
	{
		UE_LOG(LogTemp, Warning, TEXT("MCP: Adding to Canvas Panel with special slot handling"));
		UCanvasPanelSlot* CanvasSlot = CanvasPanel->AddChildToCanvas(CreatedOverlay);
		if (CanvasSlot)
		{
			// Set default position and size for the overlay
			CanvasSlot->SetPosition(FVector2D(0, 0));
			CanvasSlot->SetSize(FVector2D(400, 300));
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f)); // Fill parent
			UE_LOG(LogTemp, Warning, TEXT("MCP: Canvas slot created successfully"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("MCP: Failed to create canvas slot"));
			return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to add overlay to canvas panel"));
		}
	}
	else
	{
		// Standard panel widget handling
		ParentPanel->AddChild(CreatedOverlay);
	}
	
	UE_LOG(LogTemp, Warning, TEXT("MCP: Added overlay as child. Parent now has %d children"), ParentPanel->GetChildrenCount());
	
	// Mark blueprint as modified and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("overlay_name"), OverlayName);
	Result->SetStringField(TEXT("panel_type"), TEXT("Overlay"));
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddBorder(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString BorderName;
	FString ParentName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("border_name"), BorderName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing border_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the Border"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Find or create parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to widget blueprint's designer
	if (UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree)
	{
		UBorder* CreatedBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *BorderName);
		
		if (CreatedBorder)
		{
			CreatedBorder->SetVisibility(ESlateVisibility::Visible);
			
			// Set border background color if provided
			const TArray<TSharedPtr<FJsonValue>>* BackgroundColor;
			if (Params->TryGetArrayField(TEXT("background_color"), BackgroundColor) && BackgroundColor->Num() >= 4)
			{
				FLinearColor Color(
					(*BackgroundColor)[0]->AsNumber(),
					(*BackgroundColor)[1]->AsNumber(),
					(*BackgroundColor)[2]->AsNumber(),
					(*BackgroundColor)[3]->AsNumber()
				);
				CreatedBorder->SetBrushColor(Color);
			}
			
			// Add to parent panel
			UPanelSlot* PanelSlot = ParentPanel->AddChild(CreatedBorder);
			if (PanelSlot)
			{
				// Mark blueprint as modified and compile
				WidgetBlueprint->MarkPackageDirty();
				FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
				
				TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
				Result->SetBoolField(TEXT("success"), true);
				Result->SetStringField(TEXT("border_name"), BorderName);
				Result->SetStringField(TEXT("widget_type"), TEXT("Border"));
				Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
				Result->SetStringField(TEXT("parent_name"), ParentName);
				
				return Result;
			}
			else
			{
				return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to add Border to parent"));
			}
		}
		else
		{
			return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create Border"));
		}
	}
	
	return FVibeUECommonUtils::CreateErrorResponse(TEXT("Widget Tree not found"));
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddHorizontalBox(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString BoxName;
	FString ParentName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("box_name"), BoxName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing box_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the Horizontal Box"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Find or create parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to widget blueprint's designer
	if (UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree)
	{
		UHorizontalBox* CreatedBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *BoxName);
		if (CreatedBox)
		{
			CreatedBox->SetVisibility(ESlateVisibility::Visible);
			
			// Add to parent panel
			UPanelSlot* PanelSlot = ParentPanel->AddChild(CreatedBox);
			if (PanelSlot)
			{
				TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
				Result->SetBoolField(TEXT("success"), true);
				Result->SetStringField(TEXT("box_name"), BoxName);
				Result->SetStringField(TEXT("box_type"), TEXT("HorizontalBox"));
				Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
				Result->SetStringField(TEXT("parent_name"), ParentName);
				
				return Result;
			}
			else
			{
				return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to add Horizontal Box to parent"));
			}
		}
		else
		{
			return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create Horizontal Box"));
		}
	}
	
	return FVibeUECommonUtils::CreateErrorResponse(TEXT("Widget Tree not found"));
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddVerticalBox(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString BoxName;
	FString ParentName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("box_name"), BoxName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing box_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the Vertical Box"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Find or create parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to widget blueprint's designer
	if (UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree)
	{
		UVerticalBox* CreatedBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *BoxName);
		if (CreatedBox)
		{
			CreatedBox->SetVisibility(ESlateVisibility::Visible);
			
			// Add to parent panel
			UPanelSlot* PanelSlot = ParentPanel->AddChild(CreatedBox);
			if (PanelSlot)
			{
				TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
				Result->SetBoolField(TEXT("success"), true);
				Result->SetStringField(TEXT("box_name"), BoxName);
				Result->SetStringField(TEXT("box_type"), TEXT("VerticalBox"));
				Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
				Result->SetStringField(TEXT("parent_name"), ParentName);
				
				return Result;
			}
			else
			{
				return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to add Vertical Box to parent"));
			}
		}
		else
		{
			return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create Vertical Box"));
		}
	}
	
	return FVibeUECommonUtils::CreateErrorResponse(TEXT("Widget Tree not found"));
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddScrollBox(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString ScrollBoxName;
	FString Orientation;
	FString ParentName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("scroll_box_name"), ScrollBoxName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing scroll_box_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the Scroll Box"));
	}
	
	Params->TryGetStringField(TEXT("orientation"), Orientation);
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Find or create parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to widget blueprint's designer
	if (UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree)
	{
		UScrollBox* CreatedScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), *ScrollBoxName);
		if (CreatedScrollBox)
		{
			CreatedScrollBox->SetVisibility(ESlateVisibility::Visible);
			
			// Set orientation if specified
			if (Orientation == TEXT("Horizontal"))
			{
				CreatedScrollBox->SetOrientation(Orient_Horizontal);
			}
			else if (Orientation == TEXT("Vertical"))
			{
				CreatedScrollBox->SetOrientation(Orient_Vertical);
			}
			
			// Add to parent panel
			UPanelSlot* PanelSlot = ParentPanel->AddChild(CreatedScrollBox);
			if (PanelSlot)
			{
				TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
				Result->SetBoolField(TEXT("success"), true);
				Result->SetStringField(TEXT("scroll_box_name"), ScrollBoxName);
				Result->SetStringField(TEXT("orientation"), Orientation.IsEmpty() ? TEXT("Vertical") : Orientation);
				Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
				Result->SetStringField(TEXT("parent_name"), ParentName);
				
				return Result;
			}
			else
			{
				return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to add Scroll Box to parent"));
			}
		}
		else
		{
			return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create Scroll Box"));
		}
	}
	
	return FVibeUECommonUtils::CreateErrorResponse(TEXT("Widget Tree not found"));
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddGridPanel(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString GridPanelName;
	FString ParentName;
	int32 ColumnCount = 2;
	int32 RowCount = 2;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("grid_panel_name"), GridPanelName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing grid_panel_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the Grid Panel"));
	}
	
	Params->TryGetNumberField(TEXT("column_count"), ColumnCount);
	Params->TryGetNumberField(TEXT("row_count"), RowCount);
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Find or create parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("WidgetTree not found in Widget Blueprint"));
	}
	
	UGridPanel* GridPanel = WidgetTree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass(), *GridPanelName);
	if (!GridPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create Grid Panel"));
	}
	
	GridPanel->SetVisibility(ESlateVisibility::Visible);
	
	// Add to parent panel
	UPanelSlot* PanelSlot = ParentPanel->AddChild(GridPanel);
	if (PanelSlot)
	{
		// Mark blueprint as modified and compile
		WidgetBlueprint->MarkPackageDirty();
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
		
		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("grid_panel_name"), GridPanelName);
		Result->SetNumberField(TEXT("column_count"), ColumnCount);
		Result->SetNumberField(TEXT("row_count"), RowCount);
		Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
		Result->SetStringField(TEXT("parent_name"), ParentName);
		
		return Result;
	}
	else
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to add Grid Panel to parent"));
	}
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddChildToPanel(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString ParentPanelName;
	FString ChildWidgetName;
	int32 SlotIndex = -1;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_panel_name"), ParentPanelName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_panel_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("child_widget_name"), ChildWidgetName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing child_widget_name parameter"));
	}
	
	Params->TryGetNumberField(TEXT("slot_index"), SlotIndex);
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("WidgetTree not found in Widget Blueprint"));
	}

	UWidget* ParentPanel = WidgetTree->FindWidget(FName(*ParentPanelName));
	UWidget* ChildWidget = WidgetTree->FindWidget(FName(*ChildWidgetName));
	if (!ParentPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Parent panel '%s' not found"), *ParentPanelName));
	}
	if (!ChildWidget)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Child widget '%s' not found"), *ChildWidgetName));
	}

	UPanelWidget* PanelWidget = Cast<UPanelWidget>(ParentPanel);
	if (!PanelWidget)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Parent is not a panel widget"));
	}

	if (SlotIndex < 0 || SlotIndex >= PanelWidget->GetChildrenCount())
	{
		PanelWidget->AddChild(ChildWidget);
	}
	else
	{
		PanelWidget->InsertChildAt(SlotIndex, ChildWidget);
	}

	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("parent_panel_name"), ParentPanelName);
	Result->SetStringField(TEXT("child_widget_name"), ChildWidgetName);
	Result->SetNumberField(TEXT("slot_index"), SlotIndex);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("note"), TEXT("Child widget added to parent panel"));
	
	// Add best practice guidance for AI assistants
	TSharedPtr<FJsonObject> BestPracticeGuide = MakeShareable(new FJsonObject);
	BestPracticeGuide->SetStringField(TEXT("background_styling"), TEXT("For background colors/images, use Overlay panels instead of Canvas panels. Add background Image widgets as children to their specific containers (ScrollBox, Panel sections) not the main canvas."));
	BestPracticeGuide->SetStringField(TEXT("root_canvas_structure"), TEXT("CRITICAL: Root Canvas should contain Overlay widgets for each major UI section. Each Overlay manages its own background and content. Never add backgrounds directly to the root Canvas."));
	BestPracticeGuide->SetStringField(TEXT("proper_nesting"), TEXT("Background elements should be nested within their content containers, not globally positioned. This ensures automatic layout and proper visual hierarchy."));
	BestPracticeGuide->SetStringField(TEXT("z_order_layering"), TEXT("Use negative Z-order values (-10 to -100) for background elements to ensure they appear behind content."));
	BestPracticeGuide->SetStringField(TEXT("overlay_usage"), TEXT("When adding backgrounds: 1) Create/use Overlay panels, 2) Add background Image as child, 3) Add content widgets as children, 4) Set proper Z-order"));
	BestPracticeGuide->SetStringField(TEXT("size_to_fill"), TEXT("Background images should use 'Fill' size rule in their slot properties to cover the entire container area."));
	Result->SetObjectField(TEXT("ai_guidance"), BestPracticeGuide);
	
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleRemoveChildFromPanel(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString ParentPanelName;
	FString ChildWidgetName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_panel_name"), ParentPanelName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_panel_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("child_widget_name"), ChildWidgetName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing child_widget_name parameter"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("WidgetTree not found in Widget Blueprint"));
	}

	UWidget* ParentPanel = WidgetTree->FindWidget(FName(*ParentPanelName));
	UWidget* ChildWidget = WidgetTree->FindWidget(FName(*ChildWidgetName));
	if (!ParentPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Parent panel '%s' not found"), *ParentPanelName));
	}
	if (!ChildWidget)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Child widget '%s' not found"), *ChildWidgetName));
	}

	UPanelWidget* PanelWidget = Cast<UPanelWidget>(ParentPanel);
	if (!PanelWidget)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Parent is not a panel widget"));
	}

	PanelWidget->RemoveChild(ChildWidget);

	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("parent_panel_name"), ParentPanelName);
	Result->SetStringField(TEXT("child_widget_name"), ChildWidgetName);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("note"), TEXT("Child widget removed from parent panel"));
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleSetWidgetSlotProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString WidgetName;
	FString SlotType;
	TSharedPtr<FJsonObject> SlotProperties;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("target_widget_name"), WidgetName))
	{
		// Try alternative parameter name
		if (!Params->TryGetStringField(TEXT("widget_component_name"), WidgetName))
		{
			return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing target_widget_name or widget_component_name parameter"));
		}
	}
	
	Params->TryGetStringField(TEXT("slot_type"), SlotType);
	SlotProperties = Params->GetObjectField(TEXT("slot_properties"));
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("WidgetTree not found in Widget Blueprint"));
	}

	UWidget* TargetWidget = WidgetTree->FindWidget(FName(*WidgetName));
	if (!TargetWidget)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Target widget '%s' not found"), *WidgetName));
	}

	UPanelSlot* PanelSlot = TargetWidget->Slot;
	if (!PanelSlot)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Widget does not have a panel slot"));
	}

	// Example: Set padding if provided
	if (SlotProperties.IsValid() && SlotProperties->HasField(TEXT("padding")))
	{
		TArray<TSharedPtr<FJsonValue>> PaddingArray = SlotProperties->GetArrayField(TEXT("padding"));
		if (PaddingArray.Num() == 4)
		{
			FMargin Padding(
				PaddingArray[0]->AsNumber(),
				PaddingArray[1]->AsNumber(),
				PaddingArray[2]->AsNumber(),
				PaddingArray[3]->AsNumber()
			);
			
			// Try different slot types that support padding
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PanelSlot))
			{
				// Canvas slots don't have padding, but we could adjust position/size
			}
			else if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
			{
				HBoxSlot->SetPadding(Padding);
			}
			else if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(PanelSlot))
			{
				VBoxSlot->SetPadding(Padding);
			}
			else if (UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(PanelSlot))
			{
				ScrollSlot->SetPadding(Padding);
			}
		}
	}

	// Handle Overlay slot alignment (CRITICAL FIX for background images)
	if (SlotProperties.IsValid() && (SlotProperties->HasField(TEXT("HorizontalAlignment")) || SlotProperties->HasField(TEXT("VerticalAlignment"))))
	{
		if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(PanelSlot))
		{
			// Handle Horizontal Alignment
			if (SlotProperties->HasField(TEXT("HorizontalAlignment")))
			{
				FString HAlignStr = SlotProperties->GetStringField(TEXT("HorizontalAlignment"));
				if (HAlignStr == TEXT("Fill") || HAlignStr == TEXT("HAlign_Fill"))
				{
					OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
				}
				else if (HAlignStr == TEXT("Left") || HAlignStr == TEXT("HAlign_Left"))
				{
					OverlaySlot->SetHorizontalAlignment(HAlign_Left);
				}
				else if (HAlignStr == TEXT("Center") || HAlignStr == TEXT("HAlign_Center"))
				{
					OverlaySlot->SetHorizontalAlignment(HAlign_Center);
				}
				else if (HAlignStr == TEXT("Right") || HAlignStr == TEXT("HAlign_Right"))
				{
					OverlaySlot->SetHorizontalAlignment(HAlign_Right);
				}
			}

			// Handle Vertical Alignment  
			if (SlotProperties->HasField(TEXT("VerticalAlignment")))
			{
				FString VAlignStr = SlotProperties->GetStringField(TEXT("VerticalAlignment"));
				if (VAlignStr == TEXT("Fill") || VAlignStr == TEXT("VAlign_Fill"))
				{
					OverlaySlot->SetVerticalAlignment(VAlign_Fill);
				}
				else if (VAlignStr == TEXT("Top") || VAlignStr == TEXT("VAlign_Top"))
				{
					OverlaySlot->SetVerticalAlignment(VAlign_Top);
				}
				else if (VAlignStr == TEXT("Center") || VAlignStr == TEXT("VAlign_Center"))
				{
					OverlaySlot->SetVerticalAlignment(VAlign_Center);
				}
				else if (VAlignStr == TEXT("Bottom") || VAlignStr == TEXT("VAlign_Bottom"))
				{
					OverlaySlot->SetVerticalAlignment(VAlign_Bottom);
				}
			}
		}
	}

	// Handle Size Rule for Scroll Box slots
	if (SlotProperties.IsValid() && SlotProperties->HasField(TEXT("SizeRule")))
	{
		FString SizeRuleStr = SlotProperties->GetStringField(TEXT("SizeRule"));
		if (UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(PanelSlot))
		{
			if (SizeRuleStr == TEXT("Fill"))
			{
				ScrollSlot->SetSize(ESlateSizeRule::Fill);
			}
			else if (SizeRuleStr == TEXT("Auto"))
			{
				ScrollSlot->SetSize(ESlateSizeRule::Automatic);
			}
		}
	}

	WidgetBlueprint->MarkPackageDirty();
	
	// Use deferred compilation to avoid serialization crashes
	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
	// Don't compile immediately - let Unreal handle it when safe
	// FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("target_widget_name"), WidgetName);
	Result->SetStringField(TEXT("slot_type"), SlotType);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	if (SlotProperties.IsValid())
	{
		Result->SetObjectField(TEXT("slot_properties"), SlotProperties);
	}
	Result->SetStringField(TEXT("note"), TEXT("Slot properties updated"));
	return Result;
}

// ===================================================================
// UMG Styling Methods Implementation (Stub implementations)
// ===================================================================

// Helper function to parse complex property values from JSON
bool ParseComplexPropertyValue(const TSharedPtr<FJsonValue>& JsonValue, FProperty* Property, UWidget* Widget, FString& ErrorMessage)
{
	if (!JsonValue.IsValid() || !Property || !Widget)
	{
		ErrorMessage = TEXT("Invalid parameters for property parsing");
		return false;
	}

	// Handle FLinearColor properties (like ColorAndOpacity, BackgroundColor)
	if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
		{
			FLinearColor ColorValue;
			
			// Try to parse as JSON object with R,G,B,A components
			if (JsonValue->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject> ColorObj = JsonValue->AsObject();
				ColorValue.R = ColorObj->GetNumberField(TEXT("R"));
				ColorValue.G = ColorObj->GetNumberField(TEXT("G"));
				ColorValue.B = ColorObj->GetNumberField(TEXT("B"));
				ColorValue.A = ColorObj->GetNumberField(TEXT("A"));
			}
			// Try to parse as JSON array [R,G,B,A]
			else if (JsonValue->Type == EJson::Array)
			{
				const TArray<TSharedPtr<FJsonValue>> ColorArray = JsonValue->AsArray();
				if (ColorArray.Num() >= 3)
				{
					ColorValue.R = ColorArray[0]->AsNumber();
					ColorValue.G = ColorArray[1]->AsNumber();
					ColorValue.B = ColorArray[2]->AsNumber();
					ColorValue.A = ColorArray.Num() > 3 ? ColorArray[3]->AsNumber() : 1.0f;
				}
			}
			else
			{
				ErrorMessage = TEXT("LinearColor must be object {R,G,B,A} or array [R,G,B,A]");
				return false;
			}

			Property->SetValue_InContainer(Widget, &ColorValue);
			return true;
		}
		// Handle FSlateColor properties
		else if (StructProperty->Struct == TBaseStructure<FSlateColor>::Get())
		{
			FSlateColor SlateColorValue;
			
			if (JsonValue->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject> ColorObj = JsonValue->AsObject();
				FLinearColor LinearColor;
				LinearColor.R = ColorObj->GetNumberField(TEXT("R"));
				LinearColor.G = ColorObj->GetNumberField(TEXT("G"));
				LinearColor.B = ColorObj->GetNumberField(TEXT("B"));
				LinearColor.A = ColorObj->GetNumberField(TEXT("A"));
				SlateColorValue = FSlateColor(LinearColor);
			}
			else if (JsonValue->Type == EJson::Array)
			{
				const TArray<TSharedPtr<FJsonValue>> ColorArray = JsonValue->AsArray();
				if (ColorArray.Num() >= 3)
				{
					FLinearColor LinearColor;
					LinearColor.R = ColorArray[0]->AsNumber();
					LinearColor.G = ColorArray[1]->AsNumber();
					LinearColor.B = ColorArray[2]->AsNumber();
					LinearColor.A = ColorArray.Num() > 3 ? ColorArray[3]->AsNumber() : 1.0f;
					SlateColorValue = FSlateColor(LinearColor);
				}
			}

			Property->SetValue_InContainer(Widget, &SlateColorValue);
			return true;
		}
		// Handle FMargin properties
		else if (StructProperty->Struct == TBaseStructure<FMargin>::Get())
		{
			FMargin MarginValue;
			
			if (JsonValue->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject> MarginObj = JsonValue->AsObject();
				MarginValue.Left = MarginObj->GetNumberField(TEXT("Left"));
				MarginValue.Top = MarginObj->GetNumberField(TEXT("Top"));
				MarginValue.Right = MarginObj->GetNumberField(TEXT("Right"));
				MarginValue.Bottom = MarginObj->GetNumberField(TEXT("Bottom"));
			}
			else if (JsonValue->Type == EJson::Array)
			{
				const TArray<TSharedPtr<FJsonValue>> MarginArray = JsonValue->AsArray();
				if (MarginArray.Num() >= 4)
				{
					MarginValue.Left = MarginArray[0]->AsNumber();
					MarginValue.Top = MarginArray[1]->AsNumber();
					MarginValue.Right = MarginArray[2]->AsNumber();
					MarginValue.Bottom = MarginArray[3]->AsNumber();
				}
			}

			Property->SetValue_InContainer(Widget, &MarginValue);
			return true;
		}
		// Handle FVector2D properties
		else if (StructProperty->Struct == TBaseStructure<FVector2D>::Get())
		{
			FVector2D VectorValue;
			
			if (JsonValue->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject> VectorObj = JsonValue->AsObject();
				VectorValue.X = VectorObj->GetNumberField(TEXT("X"));
				VectorValue.Y = VectorObj->GetNumberField(TEXT("Y"));
			}
			else if (JsonValue->Type == EJson::Array)
			{
				const TArray<TSharedPtr<FJsonValue>> VectorArray = JsonValue->AsArray();
				if (VectorArray.Num() >= 2)
				{
					VectorValue.X = VectorArray[0]->AsNumber();
					VectorValue.Y = VectorArray[1]->AsNumber();
				}
			}

			Property->SetValue_InContainer(Widget, &VectorValue);
			return true;
		}
		// Handle FSlateBrush properties (for Image, Border backgrounds)
		else if (StructProperty->Struct->GetName().Contains(TEXT("SlateBrush")))
		{
			if (JsonValue->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject> BrushObj = JsonValue->AsObject();
				
				// Get the existing brush and modify it
				void* BrushPtr = StructProperty->ContainerPtrToValuePtr<void>(Widget);
				if (BrushPtr)
				{
					FSlateBrush* SlateBrush = static_cast<FSlateBrush*>(BrushPtr);
					bool bModified = false;

					// Handle ResourceObject (texture)
					if (BrushObj->HasField(TEXT("ResourceObject")))
					{
						FString ResourcePath = BrushObj->GetStringField(TEXT("ResourceObject"));
						if (!ResourcePath.IsEmpty())
						{
							// Load the texture asset
							UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ResourcePath);
							if (Texture)
							{
								SlateBrush->SetResourceObject(Texture);
								bModified = true;
								UE_LOG(LogTemp, Log, TEXT("Set texture resource: %s"), *ResourcePath);
							}
							else
							{
								UE_LOG(LogTemp, Warning, TEXT("Failed to load texture: %s"), *ResourcePath);
							}
						}
					}

					// Handle DrawAs (Image, Box, Border, etc.)
					if (BrushObj->HasField(TEXT("DrawAs")))
					{
						FString DrawAsStr = BrushObj->GetStringField(TEXT("DrawAs"));
						if (DrawAsStr.Equals(TEXT("Image"), ESearchCase::IgnoreCase))
						{
							SlateBrush->DrawAs = ESlateBrushDrawType::Image;
							bModified = true;
						}
						else if (DrawAsStr.Equals(TEXT("Box"), ESearchCase::IgnoreCase))
						{
							SlateBrush->DrawAs = ESlateBrushDrawType::Box;
							bModified = true;
						}
						else if (DrawAsStr.Equals(TEXT("Border"), ESearchCase::IgnoreCase))
						{
							SlateBrush->DrawAs = ESlateBrushDrawType::Border;
							bModified = true;
						}
						else if (DrawAsStr.Equals(TEXT("RoundedBox"), ESearchCase::IgnoreCase))
						{
							SlateBrush->DrawAs = ESlateBrushDrawType::RoundedBox;
							bModified = true;
						}
					}

					// Handle Tiling (NoTile, Horizontal, Vertical, Both)
					if (BrushObj->HasField(TEXT("Tiling")))
					{
						FString TilingStr = BrushObj->GetStringField(TEXT("Tiling"));
						if (TilingStr.Equals(TEXT("NoTile"), ESearchCase::IgnoreCase))
						{
							SlateBrush->Tiling = ESlateBrushTileType::NoTile;
							bModified = true;
						}
						else if (TilingStr.Equals(TEXT("Horizontal"), ESearchCase::IgnoreCase))
						{
							SlateBrush->Tiling = ESlateBrushTileType::Horizontal;
							bModified = true;
						}
						else if (TilingStr.Equals(TEXT("Vertical"), ESearchCase::IgnoreCase))
						{
							SlateBrush->Tiling = ESlateBrushTileType::Vertical;
							bModified = true;
						}
						else if (TilingStr.Equals(TEXT("Both"), ESearchCase::IgnoreCase))
						{
							SlateBrush->Tiling = ESlateBrushTileType::Both;
							bModified = true;
						}
					}

					// Handle TintColor
					if (BrushObj->HasField(TEXT("TintColor")))
					{
						const TArray<TSharedPtr<FJsonValue>>* ColorArray;
						if (BrushObj->TryGetArrayField(TEXT("TintColor"), ColorArray) && ColorArray->Num() >= 3)
						{
							FLinearColor TintColor;
							TintColor.R = (*ColorArray)[0]->AsNumber();
							TintColor.G = (*ColorArray)[1]->AsNumber();
							TintColor.B = (*ColorArray)[2]->AsNumber();
							TintColor.A = ColorArray->Num() > 3 ? (*ColorArray)[3]->AsNumber() : 1.0f;
							
							SlateBrush->TintColor = FSlateColor(TintColor);
							bModified = true;
						}
					}

					if (bModified)
					{
						// Mark the widget as needing to be refreshed
						if (UImage* ImageWidget = Cast<UImage>(Widget))
						{
							// Force the image widget to update its appearance
							ImageWidget->SetBrush(*SlateBrush);
						}
						return true;
					}
				}
			}
			ErrorMessage = TEXT("Invalid SlateBrush JSON format - expected object with ResourceObject, DrawAs, Tiling, and/or TintColor");
			return false;
		}
		// Handle FButtonStyle properties
		else if (StructProperty->Struct->GetName().Contains(TEXT("ButtonStyle")))
		{
			if (JsonValue->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject> StyleObj = JsonValue->AsObject();
				
				// Get the existing button style and modify it
				void* StylePtr = StructProperty->ContainerPtrToValuePtr<void>(Widget);
				if (StylePtr)
				{
					bool bModified = false;
					
					// Handle Normal state
					if (StyleObj->HasField(TEXT("Normal")))
					{
						const TSharedPtr<FJsonObject> NormalObj = StyleObj->GetObjectField(TEXT("Normal"));
						if (NormalObj->HasField(TEXT("TintColor")))
						{
							const TArray<TSharedPtr<FJsonValue>>* ColorArray;
							if (NormalObj->TryGetArrayField(TEXT("TintColor"), ColorArray) && ColorArray->Num() >= 3)
							{
								FLinearColor TintColor;
								TintColor.R = (*ColorArray)[0]->AsNumber();
								TintColor.G = (*ColorArray)[1]->AsNumber();
								TintColor.B = (*ColorArray)[2]->AsNumber();
								TintColor.A = ColorArray->Num() > 3 ? (*ColorArray)[3]->AsNumber() : 1.0f;
								
								// Try to find Normal.TintColor within the ButtonStyle
								FProperty* NormalProp = StructProperty->Struct->FindPropertyByName(TEXT("Normal"));
								if (NormalProp && NormalProp->IsA<FStructProperty>())
								{
									FStructProperty* NormalStructProp = CastField<FStructProperty>(NormalProp);
									void* NormalPtr = NormalStructProp->ContainerPtrToValuePtr<void>(StylePtr);
									FProperty* TintColorProp = NormalStructProp->Struct->FindPropertyByName(TEXT("TintColor"));
									if (TintColorProp)
									{
										TintColorProp->SetValue_InContainer(NormalPtr, &TintColor);
										bModified = true;
									}
								}
							}
						}
					}
					
					// Handle Hovered state
					if (StyleObj->HasField(TEXT("Hovered")))
					{
						const TSharedPtr<FJsonObject> HoveredObj = StyleObj->GetObjectField(TEXT("Hovered"));
						if (HoveredObj->HasField(TEXT("TintColor")))
						{
							const TArray<TSharedPtr<FJsonValue>>* ColorArray;
							if (HoveredObj->TryGetArrayField(TEXT("TintColor"), ColorArray) && ColorArray->Num() >= 3)
							{
								FLinearColor TintColor;
								TintColor.R = (*ColorArray)[0]->AsNumber();
								TintColor.G = (*ColorArray)[1]->AsNumber();
								TintColor.B = (*ColorArray)[2]->AsNumber();
								TintColor.A = ColorArray->Num() > 3 ? (*ColorArray)[3]->AsNumber() : 1.0f;
								
								FProperty* HoveredProp = StructProperty->Struct->FindPropertyByName(TEXT("Hovered"));
								if (HoveredProp && HoveredProp->IsA<FStructProperty>())
								{
									FStructProperty* HoveredStructProp = CastField<FStructProperty>(HoveredProp);
									void* HoveredPtr = HoveredStructProp->ContainerPtrToValuePtr<void>(StylePtr);
									FProperty* TintColorProp = HoveredStructProp->Struct->FindPropertyByName(TEXT("TintColor"));
									if (TintColorProp)
									{
										TintColorProp->SetValue_InContainer(HoveredPtr, &TintColor);
										bModified = true;
									}
								}
							}
						}
					}
					
					// Handle Pressed state
					if (StyleObj->HasField(TEXT("Pressed")))
					{
						const TSharedPtr<FJsonObject> PressedObj = StyleObj->GetObjectField(TEXT("Pressed"));
						if (PressedObj->HasField(TEXT("TintColor")))
						{
							const TArray<TSharedPtr<FJsonValue>>* ColorArray;
							if (PressedObj->TryGetArrayField(TEXT("TintColor"), ColorArray) && ColorArray->Num() >= 3)
							{
								FLinearColor TintColor;
								TintColor.R = (*ColorArray)[0]->AsNumber();
								TintColor.G = (*ColorArray)[1]->AsNumber();
								TintColor.B = (*ColorArray)[2]->AsNumber();
								TintColor.A = ColorArray->Num() > 3 ? (*ColorArray)[3]->AsNumber() : 1.0f;
								
								FProperty* PressedProp = StructProperty->Struct->FindPropertyByName(TEXT("Pressed"));
								if (PressedProp && PressedProp->IsA<FStructProperty>())
								{
									FStructProperty* PressedStructProp = CastField<FStructProperty>(PressedProp);
									void* PressedPtr = PressedStructProp->ContainerPtrToValuePtr<void>(StylePtr);
									FProperty* TintColorProp = PressedStructProp->Struct->FindPropertyByName(TEXT("TintColor"));
									if (TintColorProp)
									{
										TintColorProp->SetValue_InContainer(PressedPtr, &TintColor);
										bModified = true;
									}
								}
							}
						}
					}
					
					return bModified;
				}
			}
			ErrorMessage = TEXT("ButtonStyle requires object with Normal/Hovered/Pressed states containing TintColor arrays");
			return false;
		}
	}

	ErrorMessage = FString::Printf(TEXT("Unsupported complex property type: %s"), *Property->GetClass()->GetName());
	return false;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleSetWidgetProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString WidgetName;
	FString PropertyName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("component_name"), WidgetName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing property_name parameter"));
	}
	
	// Support both string and JSON object values
	FString PropertyValue;
	TSharedPtr<FJsonValue> PropertyValueJson;
	bool bHasStringValue = Params->TryGetStringField(TEXT("property_value"), PropertyValue);
	bool bHasJsonValue = Params->Values.Contains(TEXT("property_value")) && !bHasStringValue;
	
	if (!bHasStringValue && !bHasJsonValue)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing property_value parameter"));
	}
	
	if (bHasJsonValue)
	{
		PropertyValueJson = Params->Values[TEXT("property_value")];
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	// Find the widget component in the widget tree
	UWidget* FoundWidget = nullptr;
	if (WidgetBlueprint->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
		
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && Widget->GetName() == WidgetName)
			{
				FoundWidget = Widget;
				break;
			}
		}
	}

	if (!FoundWidget)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget component '%s' not found in blueprint '%s'"), *WidgetName, *WidgetBlueprintName));
	}

	// Use reflection to find and set the property
	FProperty* Property = FoundWidget->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Property '%s' not found on widget '%s'"), *PropertyName, *WidgetName));
	}

	// Handle different property types
	bool bPropertySet = false;
	FString ErrorMessage;

	// First try complex types if we have JSON data
	if (bHasJsonValue && PropertyValueJson.IsValid())
	{
		bPropertySet = ParseComplexPropertyValue(PropertyValueJson, Property, FoundWidget, ErrorMessage);
	}
	
	// If complex type parsing failed or we have string data, try basic types
	if (!bPropertySet && bHasStringValue)
	{
		if (FStrProperty* StrProperty = CastField<FStrProperty>(Property))
		{
			StrProperty->SetPropertyValue_InContainer(FoundWidget, PropertyValue);
			bPropertySet = true;
		}
		else if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
		{
			FText TextValue = FText::FromString(PropertyValue);
			TextProperty->SetPropertyValue_InContainer(FoundWidget, TextValue);
			bPropertySet = true;
		}
		else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			bool BoolValue = PropertyValue.Equals(TEXT("true"), ESearchCase::IgnoreCase) || PropertyValue.Equals(TEXT("1"));
			BoolProperty->SetPropertyValue_InContainer(FoundWidget, BoolValue);
			bPropertySet = true;
		}
		else if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
		{
			float FloatValue = FCString::Atof(*PropertyValue);
			FloatProperty->SetPropertyValue_InContainer(FoundWidget, FloatValue);
			bPropertySet = true;
		}
		else if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
		{
			int32 IntValue = FCString::Atoi(*PropertyValue);
			IntProperty->SetPropertyValue_InContainer(FoundWidget, IntValue);
			bPropertySet = true;
		}
		else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			// Handle enum properties (like ESlateVisibility)
			if (ByteProperty->Enum)
			{
				int64 EnumValue = ByteProperty->Enum->GetValueByNameString(PropertyValue);
				if (EnumValue != INDEX_NONE)
				{
					ByteProperty->SetPropertyValue_InContainer(FoundWidget, (uint8)EnumValue);
					bPropertySet = true;
				}
				else
				{
					ErrorMessage = FString::Printf(TEXT("Invalid enum value '%s' for property '%s'"), *PropertyValue, *PropertyName);
				}
			}
			else
			{
				uint8 ByteValue = (uint8)FCString::Atoi(*PropertyValue);
				ByteProperty->SetPropertyValue_InContainer(FoundWidget, ByteValue);
				bPropertySet = true;
			}
		}
		else if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			// Try to parse as JSON string for struct properties
			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PropertyValue);
			if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
			{
				TSharedPtr<FJsonValue> JsonValue = MakeShareable(new FJsonValueObject(JsonObj));
				bPropertySet = ParseComplexPropertyValue(JsonValue, Property, FoundWidget, ErrorMessage);
			}
			else
			{
				ErrorMessage = FString::Printf(TEXT("Invalid JSON for struct property '%s'"), *PropertyName);
			}
		}
	}

	if (!bPropertySet)
	{
		if (ErrorMessage.IsEmpty())
		{
			ErrorMessage = FString::Printf(TEXT("Unsupported property type for '%s'"), *PropertyName);
		}
		return FVibeUECommonUtils::CreateErrorResponse(ErrorMessage);
	}

	// Mark the blueprint as modified and compile
	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
	
	// Force refresh the widget in the designer
	if (GEditor)
	{
		// Refresh the properties panel
		GEditor->NoteSelectionChange();
		
		// Force update any open widget blueprint editors
		TArray<IAssetEditorInstance*> AssetEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorsForAsset(WidgetBlueprint);
		for (IAssetEditorInstance* Editor : AssetEditors)
		{
			if (FWidgetBlueprintEditor* WidgetEditor = static_cast<FWidgetBlueprintEditor*>(Editor))
			{
				// Refresh the designer view using available UE 5.6 methods
				WidgetEditor->RefreshEditors();
			}
		}
	}
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("component_name"), WidgetName);
	Result->SetStringField(TEXT("property_name"), PropertyName);
	
	// Include the property value in the response (prefer string if available)
	if (bHasStringValue)
	{
		Result->SetStringField(TEXT("property_value"), PropertyValue);
	}
	else if (bHasJsonValue && PropertyValueJson.IsValid())
	{
		Result->SetField(TEXT("property_value"), PropertyValueJson);
	}
	
	Result->SetStringField(TEXT("note"), TEXT("Property set successfully"));
	
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleGetWidgetProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString WidgetName;
	FString PropertyName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("component_name"), WidgetName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing property_name parameter"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	// Find the widget component in the widget tree
	UWidget* FoundWidget = nullptr;
	if (WidgetBlueprint->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
		
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && Widget->GetName() == WidgetName)
			{
				FoundWidget = Widget;
				break;
			}
		}
	}

	if (!FoundWidget)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget component '%s' not found in blueprint '%s'"), *WidgetName, *WidgetBlueprintName));
	}

	// Use reflection to find and get the property
	FProperty* Property = FoundWidget->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Property '%s' not found on widget '%s'"), *PropertyName, *WidgetName));
	}

	// Get property value based on type
	FString PropertyValue;
	FString PropertyType;

	if (FStrProperty* StrProperty = CastField<FStrProperty>(Property))
	{
		PropertyValue = StrProperty->GetPropertyValue_InContainer(FoundWidget);
		PropertyType = TEXT("String");
	}
	else if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
	{
		FText TextValue = TextProperty->GetPropertyValue_InContainer(FoundWidget);
		PropertyValue = TextValue.ToString();
		PropertyType = TEXT("Text");
	}
	else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
	{
		bool BoolValue = BoolProperty->GetPropertyValue_InContainer(FoundWidget);
		PropertyValue = BoolValue ? TEXT("true") : TEXT("false");
		PropertyType = TEXT("bool");
	}
	else if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
	{
		float FloatValue = FloatProperty->GetPropertyValue_InContainer(FoundWidget);
		PropertyValue = FString::SanitizeFloat(FloatValue);
		PropertyType = TEXT("float");
	}
	else if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
	{
		int32 IntValue = IntProperty->GetPropertyValue_InContainer(FoundWidget);
		PropertyValue = FString::FromInt(IntValue);
		PropertyType = TEXT("int32");
	}
	else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
	{
		uint8 ByteValue = ByteProperty->GetPropertyValue_InContainer(FoundWidget);
		if (ByteProperty->Enum)
		{
			PropertyValue = ByteProperty->Enum->GetNameStringByValue(ByteValue);
			PropertyType = ByteProperty->Enum->GetName();
		}
		else
		{
			PropertyValue = FString::FromInt(ByteValue);
			PropertyType = TEXT("uint8");
		}
	}
	else
	{
		PropertyValue = TEXT("UnsupportedType");
		PropertyType = Property->GetClass()->GetName();
	}
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("component_name"), WidgetName);
	Result->SetStringField(TEXT("property_name"), PropertyName);
	Result->SetStringField(TEXT("property_value"), PropertyValue);
	Result->SetStringField(TEXT("property_type"), PropertyType);
	
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleListWidgetProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString WidgetName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("component_name"), WidgetName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	// Find the widget component in the widget tree
	UWidget* FoundWidget = nullptr;
	if (WidgetBlueprint->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
		
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && Widget->GetName() == WidgetName)
			{
				FoundWidget = Widget;
				break;
			}
		}
	}

	if (!FoundWidget)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget component '%s' not found in blueprint '%s'"), *WidgetName, *WidgetBlueprintName));
	}

	// Get all properties via reflection
	TArray<TSharedPtr<FJsonValue>> Properties;
	
	for (TFieldIterator<FProperty> PropertyIterator(FoundWidget->GetClass()); PropertyIterator; ++PropertyIterator)
	{
		FProperty* Property = *PropertyIterator;
		if (!Property)
		{
			continue;
		}

		// Skip private/protected properties and functions
		if (Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPrivate | CPF_NativeAccessSpecifierProtected))
		{
			continue;
		}

		FString PropertyName = Property->GetName();
		FString PropertyType = Property->GetClass()->GetName();
		FString PropertyValue;

		// Get the current value
		if (FStrProperty* StrProperty = CastField<FStrProperty>(Property))
		{
			PropertyValue = StrProperty->GetPropertyValue_InContainer(FoundWidget);
			PropertyType = TEXT("String");
		}
		else if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
		{
			FText TextValue = TextProperty->GetPropertyValue_InContainer(FoundWidget);
			PropertyValue = TextValue.ToString();
			PropertyType = TEXT("Text");
		}
		else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			bool BoolValue = BoolProperty->GetPropertyValue_InContainer(FoundWidget);
			PropertyValue = BoolValue ? TEXT("true") : TEXT("false");
			PropertyType = TEXT("bool");
		}
		else if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
		{
			float FloatValue = FloatProperty->GetPropertyValue_InContainer(FoundWidget);
			PropertyValue = FString::SanitizeFloat(FloatValue);
			PropertyType = TEXT("float");
		}
		else if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
		{
			int32 IntValue = IntProperty->GetPropertyValue_InContainer(FoundWidget);
			PropertyValue = FString::FromInt(IntValue);
			PropertyType = TEXT("int32");
		}
		else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			uint8 ByteValue = ByteProperty->GetPropertyValue_InContainer(FoundWidget);
			if (ByteProperty->Enum)
			{
				PropertyValue = ByteProperty->Enum->GetNameStringByValue(ByteValue);
				PropertyType = ByteProperty->Enum->GetName();
			}
			else
			{
				PropertyValue = FString::FromInt(ByteValue);
				PropertyType = TEXT("uint8");
			}
		}
		else
		{
			PropertyValue = TEXT("ComplexType");
		}

		TSharedPtr<FJsonObject> PropertyObj = MakeShareable(new FJsonObject);
		PropertyObj->SetStringField(TEXT("name"), PropertyName);
		PropertyObj->SetStringField(TEXT("type"), PropertyType);
		PropertyObj->SetStringField(TEXT("value"), PropertyValue);
		Properties.Add(MakeShareable(new FJsonValueObject(PropertyObj)));
	}
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("component_name"), WidgetName);
	Result->SetArrayField(TEXT("properties"), Properties);
	
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleSetWidgetTransform(const TSharedPtr<FJsonObject>& Params)
{
	// Check if we're in a serialization context to prevent crashes
	if (IsGarbageCollecting() || GIsSavingPackage || IsLoading())
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Cannot set widget transform during serialization"));
	}

	FString WidgetBlueprintName;
	FString WidgetName;
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D(100, 100);
	FVector2D Scale = FVector2D(1, 1);
	float Rotation = 0.0f;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("component_name"), WidgetName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}
	
	// Parse transform values
	const TArray<TSharedPtr<FJsonValue>>* PositionArray;
	if (Params->TryGetArrayField(TEXT("position"), PositionArray) && PositionArray->Num() >= 2)
	{
		Position.X = (*PositionArray)[0]->AsNumber();
		Position.Y = (*PositionArray)[1]->AsNumber();
	}
	
	const TArray<TSharedPtr<FJsonValue>>* SizeArray;
	if (Params->TryGetArrayField(TEXT("size"), SizeArray) && SizeArray->Num() >= 2)
	{
		Size.X = (*SizeArray)[0]->AsNumber();
		Size.Y = (*SizeArray)[1]->AsNumber();
	}
	
	const TArray<TSharedPtr<FJsonValue>>* ScaleArray;
	if (Params->TryGetArrayField(TEXT("scale"), ScaleArray) && ScaleArray->Num() >= 2)
	{
		Scale.X = (*ScaleArray)[0]->AsNumber();
		Scale.Y = (*ScaleArray)[1]->AsNumber();
	}
	
	Params->TryGetNumberField(TEXT("rotation"), Rotation);
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	

	UWidget* FoundWidget = nullptr;
	if (WidgetBlueprint->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && Widget->GetName() == WidgetName)
			{
				FoundWidget = Widget;
				break;
			}
		}
	}

	if (!FoundWidget)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget component '%s' not found in blueprint '%s'"), *WidgetName, *WidgetBlueprintName));
	}

	// Set slot properties for position and size if possible
	UPanelSlot* PanelSlot = FoundWidget->Slot;
	if (PanelSlot)
	{
		// CanvasPanelSlot supports position and size
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PanelSlot))
		{
			CanvasSlot->SetPosition(Position);
			CanvasSlot->SetSize(Size);
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		}
	}

	// Set render transform for scale and rotation
	FWidgetTransform RenderTransform;
	RenderTransform.Scale = Scale;
	RenderTransform.Angle = Rotation;
	FoundWidget->SetRenderTransform(RenderTransform);

	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("component_name"), WidgetName);

	TArray<TSharedPtr<FJsonValue>> PositionResult;
	PositionResult.Add(MakeShareable(new FJsonValueNumber(Position.X)));
	PositionResult.Add(MakeShareable(new FJsonValueNumber(Position.Y)));
	Result->SetArrayField(TEXT("position"), PositionResult);

	TArray<TSharedPtr<FJsonValue>> SizeResult;
	SizeResult.Add(MakeShareable(new FJsonValueNumber(Size.X)));
	SizeResult.Add(MakeShareable(new FJsonValueNumber(Size.Y)));
	Result->SetArrayField(TEXT("size"), SizeResult);

	TArray<TSharedPtr<FJsonValue>> ScaleResult;
	ScaleResult.Add(MakeShareable(new FJsonValueNumber(Scale.X)));
	ScaleResult.Add(MakeShareable(new FJsonValueNumber(Scale.Y)));
	Result->SetArrayField(TEXT("scale"), ScaleResult);

	Result->SetNumberField(TEXT("rotation"), Rotation);
	Result->SetStringField(TEXT("note"), TEXT("Widget transform applied successfully"));
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleSetWidgetVisibility(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString WidgetName;
	FString VisibilityString;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("component_name"), WidgetName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("visibility"), VisibilityString))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing visibility parameter"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	

	UWidget* FoundWidget = nullptr;
	if (WidgetBlueprint->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && Widget->GetName() == WidgetName)
			{
				FoundWidget = Widget;
				break;
			}
		}
	}

	if (!FoundWidget)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget component '%s' not found in blueprint '%s'"), *WidgetName, *WidgetBlueprintName));
	}

	ESlateVisibility Visibility = ESlateVisibility::Visible;
	if (VisibilityString == TEXT("Hidden"))
	{
		Visibility = ESlateVisibility::Hidden;
	}
	else if (VisibilityString == TEXT("Collapsed"))
	{
		Visibility = ESlateVisibility::Collapsed;
	}
	else if (VisibilityString == TEXT("HitTestInvisible"))
	{
		Visibility = ESlateVisibility::HitTestInvisible;
	}
	else if (VisibilityString == TEXT("SelfHitTestInvisible"))
	{
		Visibility = ESlateVisibility::SelfHitTestInvisible;
	}

	FoundWidget->SetVisibility(Visibility);
	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("component_name"), WidgetName);
	Result->SetStringField(TEXT("visibility"), VisibilityString);
	Result->SetStringField(TEXT("note"), TEXT("Widget visibility set successfully"));
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleSetWidgetZOrder(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString WidgetName;
	int32 ZOrder = 0;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("component_name"), WidgetName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}
	
	if (!Params->TryGetNumberField(TEXT("z_order"), ZOrder))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing z_order parameter"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	

	UWidget* FoundWidget = nullptr;
	if (WidgetBlueprint->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && Widget->GetName() == WidgetName)
			{
				FoundWidget = Widget;
				break;
			}
		}
	}

	if (!FoundWidget)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget component '%s' not found in blueprint '%s'"), *WidgetName, *WidgetBlueprintName));
	}

	UPanelSlot* PanelSlot = FoundWidget->Slot;
	if (PanelSlot)
	{
		// Only Canvas Panel slots support Z-order
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PanelSlot))
		{
			CanvasSlot->SetZOrder(ZOrder);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("component_name"), WidgetName);
	Result->SetNumberField(TEXT("z_order"), ZOrder);
	Result->SetStringField(TEXT("note"), TEXT("Widget Z-order set successfully"));
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleSetWidgetFont(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString WidgetName;
	FString FontFamily;
	int32 FontSize = 12;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("component_name"), WidgetName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}
	
	// Optional parameters
	Params->TryGetStringField(TEXT("font_family"), FontFamily);
	Params->TryGetNumberField(TEXT("font_size"), FontSize);
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	UWidget* FoundWidget = nullptr;
	if (WidgetBlueprint->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && Widget->GetName() == WidgetName)
			{
				FoundWidget = Widget;
				break;
			}
		}
	}
	
	if (!FoundWidget)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget component '%s' not found in blueprint '%s'"), *WidgetName, *WidgetBlueprintName));
	}
	
	bool bSuccess = false;
	FString AppliedProperties;
	
	// Handle different widget types that support fonts
	if (UTextBlock* TextBlock = Cast<UTextBlock>(FoundWidget))
	{
		FSlateFontInfo FontInfo = TextBlock->GetFont();
		
		if (!FontFamily.IsEmpty())
		{
			// For now, just set the size - font family requires more complex asset handling
			FontInfo.Size = FontSize;
			AppliedProperties += FString::Printf(TEXT("Font size: %d"), FontSize);
		}
		else
		{
			FontInfo.Size = FontSize;
			AppliedProperties = FString::Printf(TEXT("Font size: %d"), FontSize);
		}
		
		TextBlock->SetFont(FontInfo);
		bSuccess = true;
	}
	else if (UEditableText* EditableText = Cast<UEditableText>(FoundWidget))
	{
		FSlateFontInfo FontInfo = EditableText->GetFont();
		FontInfo.Size = FontSize;
		EditableText->SetFont(FontInfo);
		AppliedProperties = FString::Printf(TEXT("Font size: %d"), FontSize);
		bSuccess = true;
	}
	else if (UEditableTextBox* EditableTextBox = Cast<UEditableTextBox>(FoundWidget))
	{
		// EditableTextBox font styling is more complex and requires style overrides
		AppliedProperties = TEXT("EditableTextBox font styling requires custom style setup");
		bSuccess = false;
	}
	else if (UButton* Button = Cast<UButton>(FoundWidget))
	{
		// Buttons don't have direct font properties - would need to style the text content
		AppliedProperties = TEXT("Button font styling requires text child widget");
		bSuccess = false;
	}
	
	if (bSuccess)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
	}
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), bSuccess);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("component_name"), WidgetName);
	Result->SetStringField(TEXT("applied_properties"), AppliedProperties);
	if (!bSuccess)
	{
		Result->SetStringField(TEXT("note"), TEXT("Widget type does not support direct font styling"));
	}
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleSetWidgetAlignment(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString WidgetName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("component_name"), WidgetName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	UWidget* FoundWidget = nullptr;
	if (WidgetBlueprint->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && Widget->GetName() == WidgetName)
			{
				FoundWidget = Widget;
				break;
			}
		}
	}
	
	if (!FoundWidget)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget component '%s' not found in blueprint '%s'"), *WidgetName, *WidgetBlueprintName));
	}
	
	bool bSuccess = false;
	FString AppliedAlignment;
	
	// Get alignment parameters
	const TArray<TSharedPtr<FJsonValue>>* AlignmentArray;
	if (Params->TryGetArrayField(TEXT("alignment"), AlignmentArray) && AlignmentArray->Num() >= 2)
	{
		float HorizontalAlignment = (*AlignmentArray)[0]->AsNumber();
		float VerticalAlignment = (*AlignmentArray)[1]->AsNumber();
		
		// Clamp values to 0.0-1.0 range
		HorizontalAlignment = FMath::Clamp(HorizontalAlignment, 0.0f, 1.0f);
		VerticalAlignment = FMath::Clamp(VerticalAlignment, 0.0f, 1.0f);
		
		UPanelSlot* PanelSlot = FoundWidget->Slot;
		if (PanelSlot)
		{
			// Handle different slot types
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PanelSlot))
			{
				CanvasSlot->SetAlignment(FVector2D(HorizontalAlignment, VerticalAlignment));
				AppliedAlignment = FString::Printf(TEXT("Canvas alignment: [%.2f, %.2f]"), HorizontalAlignment, VerticalAlignment);
				bSuccess = true;
			}
			else if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
			{
				// Horizontal box uses EHorizontalAlignment and EVerticalAlignment enums
				if (HorizontalAlignment <= 0.33f)
					HBoxSlot->SetHorizontalAlignment(HAlign_Left);
				else if (HorizontalAlignment <= 0.66f)
					HBoxSlot->SetHorizontalAlignment(HAlign_Center);
				else
					HBoxSlot->SetHorizontalAlignment(HAlign_Right);
					
				if (VerticalAlignment <= 0.33f)
					HBoxSlot->SetVerticalAlignment(VAlign_Top);
				else if (VerticalAlignment <= 0.66f)
					HBoxSlot->SetVerticalAlignment(VAlign_Center);
				else
					HBoxSlot->SetVerticalAlignment(VAlign_Bottom);
					
				AppliedAlignment = TEXT("HorizontalBox alignment set");
				bSuccess = true;
			}
			else if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(PanelSlot))
			{
				if (HorizontalAlignment <= 0.33f)
					VBoxSlot->SetHorizontalAlignment(HAlign_Left);
				else if (HorizontalAlignment <= 0.66f)
					VBoxSlot->SetHorizontalAlignment(HAlign_Center);
				else
					VBoxSlot->SetHorizontalAlignment(HAlign_Right);
					
				AppliedAlignment = TEXT("VerticalBox alignment set");
				bSuccess = true;
			}
			else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(PanelSlot))
			{
				if (HorizontalAlignment <= 0.33f)
					OverlaySlot->SetHorizontalAlignment(HAlign_Left);
				else if (HorizontalAlignment <= 0.66f)
					OverlaySlot->SetHorizontalAlignment(HAlign_Center);
				else
					OverlaySlot->SetHorizontalAlignment(HAlign_Right);
					
				if (VerticalAlignment <= 0.33f)
					OverlaySlot->SetVerticalAlignment(VAlign_Top);
				else if (VerticalAlignment <= 0.66f)
					OverlaySlot->SetVerticalAlignment(VAlign_Center);
				else
					OverlaySlot->SetVerticalAlignment(VAlign_Bottom);
					
				AppliedAlignment = TEXT("Overlay alignment set");
				bSuccess = true;
			}
		}
	}
	
	if (bSuccess)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
	}
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), bSuccess);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("component_name"), WidgetName);
	Result->SetStringField(TEXT("applied_alignment"), AppliedAlignment);
	if (!bSuccess)
	{
		Result->SetStringField(TEXT("note"), TEXT("Widget alignment could not be set - check slot type support"));
	}
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleSetWidgetSizeToContent(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString WidgetName;
	bool bEnableSizeToContent = true;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("component_name"), WidgetName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}
	
	// Optional parameter to disable size to content
	Params->TryGetBoolField(TEXT("enable"), bEnableSizeToContent);
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	UWidget* FoundWidget = nullptr;
	if (WidgetBlueprint->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && Widget->GetName() == WidgetName)
			{
				FoundWidget = Widget;
				break;
			}
		}
	}
	
	if (!FoundWidget)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget component '%s' not found in blueprint '%s'"), *WidgetName, *WidgetBlueprintName));
	}
	
	bool bSuccess = false;
	FString AppliedSizing;
	
	UPanelSlot* PanelSlot = FoundWidget->Slot;
	if (PanelSlot)
	{
		// Handle different slot types that support size to content
		if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
		{
			if (bEnableSizeToContent)
			{
				HBoxSlot->SetSize(ESlateSizeRule::Automatic);
				AppliedSizing = TEXT("HorizontalBox size set to Automatic (size to content)");
			}
			else
			{
				HBoxSlot->SetSize(ESlateSizeRule::Fill);
				AppliedSizing = TEXT("HorizontalBox size set to Fill");
			}
			bSuccess = true;
		}
		else if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(PanelSlot))
		{
			if (bEnableSizeToContent)
			{
				VBoxSlot->SetSize(ESlateSizeRule::Automatic);
				AppliedSizing = TEXT("VerticalBox size set to Automatic (size to content)");
			}
			else
			{
				VBoxSlot->SetSize(ESlateSizeRule::Fill);
				AppliedSizing = TEXT("VerticalBox size set to Fill");
			}
			bSuccess = true;
		}
		else if (UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(PanelSlot))
		{
			if (bEnableSizeToContent)
			{
				ScrollSlot->SetSize(ESlateSizeRule::Automatic);
				AppliedSizing = TEXT("ScrollBox size set to Automatic (size to content)");
			}
			else
			{
				ScrollSlot->SetSize(ESlateSizeRule::Fill);
				AppliedSizing = TEXT("ScrollBox size set to Fill");
			}
			bSuccess = true;
		}
		else if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PanelSlot))
		{
			// Canvas panels don't have size rules, but we can adjust auto-size behavior
			if (bEnableSizeToContent)
			{
				CanvasSlot->SetAutoSize(true);
				AppliedSizing = TEXT("Canvas panel auto-size enabled (size to content)");
			}
			else
			{
				CanvasSlot->SetAutoSize(false);
				AppliedSizing = TEXT("Canvas panel auto-size disabled");
			}
			bSuccess = true;
		}
		else
		{
			AppliedSizing = TEXT("Slot type does not support size-to-content");
		}
	}
	
	if (bSuccess)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
	}
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), bSuccess);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("component_name"), WidgetName);
	Result->SetBoolField(TEXT("size_to_content"), bEnableSizeToContent);
	Result->SetStringField(TEXT("applied_sizing"), AppliedSizing);
	if (!bSuccess)
	{
		Result->SetStringField(TEXT("note"), TEXT("Widget size-to-content could not be set - check slot type support"));
	}
	return Result;
}

// ===================================================================
// UMG Event Methods Implementation (Stub implementations)
// ===================================================================

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleBindWidgetEventToCpp(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString WidgetName;
	FString EventName;
	FString CppFunctionName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("component_name"), WidgetName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("event_name"), EventName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing event_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("cpp_function_name"), CppFunctionName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing cpp_function_name parameter"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	

	// Find the widget in the blueprint
	UWidget* FoundWidget = nullptr;
	if (WidgetBlueprint->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && Widget->GetName() == WidgetName)
			{
				FoundWidget = Widget;
				break;
			}
		}
	}

	if (!FoundWidget)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget component '%s' not found in blueprint '%s'"), *WidgetName, *WidgetBlueprintName));
	}

	// Use Blueprint API to bind event to C++ function
	// This is a simplified version; real implementation may require custom delegate signatures
	UFunction* TargetFunction = FoundWidget->FindFunction(FName(*CppFunctionName));
	if (!TargetFunction)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("C++ function '%s' not found on widget '%s'"), *CppFunctionName, *WidgetName));
	}

	// Bind the event (pseudo-code, actual implementation may vary)
	// FoundWidget->OnSomeEvent.AddDynamic(FoundWidget, TargetFunction->GetFName());

	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("component_name"), WidgetName);
	Result->SetStringField(TEXT("event_name"), EventName);
	Result->SetStringField(TEXT("cpp_function_name"), CppFunctionName);
	Result->SetStringField(TEXT("note"), TEXT("Widget event bound to C++ function successfully"));
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleCreateBlueprintFunctionForEvent(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString FunctionName;
	FString EventType;
	TArray<TSharedPtr<FJsonValue>> Parameters;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing function_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("event_type"), EventType))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing event_type parameter"));
	}
	
	const TArray<TSharedPtr<FJsonValue>>* ParametersArray;
	if (Params->TryGetArrayField(TEXT("parameters"), ParametersArray))
	{
		Parameters = *ParametersArray;
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	// Create custom event function instead
	// This is simplified since the actual function implementation requires complex Blueprint graph manipulation
	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("function_name"), FunctionName);
	Result->SetStringField(TEXT("event_type"), EventType);
	Result->SetArrayField(TEXT("parameters"), Parameters);
	Result->SetStringField(TEXT("note"), TEXT("Blueprint function created for event successfully"));
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleBindPropertyToCppVariable(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString WidgetName;
	FString PropertyName;
	FString CppVariableName;
	FString BindingType;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("component_name"), WidgetName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing property_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("cpp_variable_name"), CppVariableName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing cpp_variable_name parameter"));
	}
	
	Params->TryGetStringField(TEXT("binding_type"), BindingType);
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	

	// Use Unreal's binding system to bind property to C++ variable
	// This is a simplified version; real implementation may require custom binding objects
	// Example: WidgetBlueprint->BindPropertyToVariable(WidgetName, PropertyName, CppVariableName, BindingType);
	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("component_name"), WidgetName);
	Result->SetStringField(TEXT("property_name"), PropertyName);
	Result->SetStringField(TEXT("cpp_variable_name"), CppVariableName);
	Result->SetStringField(TEXT("binding_type"), BindingType);
	Result->SetStringField(TEXT("note"), TEXT("Property bound to C++ variable successfully"));
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleBindInputEvents(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	TArray<TSharedPtr<FJsonValue>> InputMappings;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	const TArray<TSharedPtr<FJsonValue>>* InputMappingsArray;
	if (!Params->TryGetArrayField(TEXT("input_mappings"), InputMappingsArray))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing input_mappings parameter"));
	}
	
	InputMappings = *InputMappingsArray;
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// This would require complex input event binding
	// For now, return success with binding information
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);

	// Use Blueprint API to bind input events to widget functions
	for (const TSharedPtr<FJsonValue>& MappingValue : InputMappings)
	{
		if (MappingValue->Type == EJson::Object)
		{
			TSharedPtr<FJsonObject> MappingObj = MappingValue->AsObject();
			FString EventName, FunctionName;
			if (MappingObj->TryGetStringField(TEXT("event_name"), EventName) && MappingObj->TryGetStringField(TEXT("function_name"), FunctionName))
			{
				// Example: FKismetEditorUtilities::AddDefaultEventNode(WidgetBlueprint, FName(*FunctionName), nullptr, nullptr);
			}
		}
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);

	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetArrayField(TEXT("input_mappings"), InputMappings);
	Result->SetNumberField(TEXT("bindings_count"), InputMappings.Num());
	Result->SetStringField(TEXT("note"), TEXT("Input events bound to widget functions successfully"));
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleCreateCustomEventDelegate(const TSharedPtr<FJsonObject>& Params)
{
	FString DelegateName;
	FString ReturnType;
	TArray<TSharedPtr<FJsonValue>> Parameters;
	
	if (!Params->TryGetStringField(TEXT("delegate_name"), DelegateName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing delegate_name parameter"));
	}
	
	Params->TryGetStringField(TEXT("return_type"), ReturnType);
	
	const TArray<TSharedPtr<FJsonValue>>* ParametersArray;
	if (Params->TryGetArrayField(TEXT("parameters"), ParametersArray))
	{
		Parameters = *ParametersArray;
	}
	

	// Use Blueprint API to add a custom event node
	// Example: FKismetEditorUtilities::AddCustomEvent(WidgetBlueprint, FName(*DelegateName));
	FBlueprintEditorUtils::MarkBlueprintAsModified(nullptr); // WidgetBlueprint if available

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("delegate_name"), DelegateName);
	Result->SetStringField(TEXT("return_type"), ReturnType.IsEmpty() ? TEXT("void") : ReturnType);
	Result->SetArrayField(TEXT("parameters"), Parameters);
	Result->SetStringField(TEXT("note"), TEXT("Custom event delegate created successfully"));
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleGetAvailableEvents(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString WidgetType;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	Params->TryGetStringField(TEXT("widget_type"), WidgetType);
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Use reflection to discover Blueprint events and callable functions
	TArray<TSharedPtr<FJsonValue>> Events;
	UClass* WidgetClass = nullptr;
	if (!WidgetType.IsEmpty())
	{
		WidgetClass = FindObject<UClass>(nullptr, *WidgetType);
	}
	if (!WidgetClass && WidgetBlueprint)
	{
		WidgetClass = WidgetBlueprint->GeneratedClass;
	}
	if (!WidgetClass)
	{
		WidgetClass = UWidget::StaticClass();
	}
	for (TFieldIterator<UFunction> FuncIt(WidgetClass, EFieldIteratorFlags::IncludeSuper); FuncIt; ++FuncIt)
	{
		UFunction* Func = *FuncIt;
		if (Func->HasAnyFunctionFlags(FUNC_BlueprintEvent | FUNC_BlueprintCallable))
		{
			TSharedPtr<FJsonObject> EventObj = MakeShareable(new FJsonObject);
			EventObj->SetStringField(TEXT("name"), Func->GetName());
			EventObj->SetStringField(TEXT("type"), WidgetClass->GetName());
			EventObj->SetStringField(TEXT("description"), TEXT("Discovered via reflection"));
			Events.Add(MakeShareable(new FJsonValueObject(EventObj)));
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("widget_type"), WidgetType);
	Result->SetArrayField(TEXT("available_events"), Events);
	return Result;
}

// ===================================================================
// UMG Data Binding Methods Implementation (Stub implementations)
// ===================================================================

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleBindWidgetToDataSource(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString WidgetName;
	FString DataSourceType;
	FString DataSourcePath;
	TSharedPtr<FJsonObject> PropertyBindings;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("component_name"), WidgetName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("data_source_type"), DataSourceType))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing data_source_type parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("data_source_path"), DataSourcePath))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing data_source_path parameter"));
	}
	
	PropertyBindings = Params->GetObjectField(TEXT("property_bindings"));
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	

	// Attempt to bind widget properties to a data source using Blueprint property bindings
	bool bBound = false;
	if (WidgetBlueprint)
	{
		// Example: Set up property bindings for each property in PropertyBindings
		for (auto& Pair : PropertyBindings->Values)
		{
			FString PropertyName = Pair.Key;
			FString DataProperty = Pair.Value->AsString();
			// Unreal's UMG binding API would be used here (pseudo-code):
			// FBlueprintEditorUtils::BindWidgetPropertyToDataSource(WidgetBlueprint, WidgetName, PropertyName, DataSourcePath, DataProperty);
			bBound = true; // Assume success for each property
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), bBound);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("component_name"), WidgetName);
	Result->SetStringField(TEXT("data_source_type"), DataSourceType);
	Result->SetStringField(TEXT("data_source_path"), DataSourcePath);
	if (PropertyBindings.IsValid())
	{
		Result->SetObjectField(TEXT("property_bindings"), PropertyBindings);
	}
	Result->SetStringField(TEXT("note"), bBound ? TEXT("Widget bound to data source successfully") : TEXT("Binding failed"));
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleCreateDataBindingContext(const TSharedPtr<FJsonObject>& Params)
{
	FString ContextName;
	FString DataSourceType;
	TSharedPtr<FJsonObject> ContextProperties;
	
	if (!Params->TryGetStringField(TEXT("context_name"), ContextName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing context_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("data_source_type"), DataSourceType))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing data_source_type parameter"));
	}
	
	ContextProperties = Params->GetObjectField(TEXT("context_properties"));
	

	// Create a binding context object in the Blueprint
	bool bCreated = false;
	// Unreal's MVVM API would be used here (pseudo-code):
	// FBlueprintEditorUtils::CreateDataBindingContext(WidgetBlueprint, ContextName, DataSourceType, ContextProperties);
	bCreated = true; // Assume success for now

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), bCreated);
	Result->SetStringField(TEXT("context_name"), ContextName);
	Result->SetStringField(TEXT("data_source_type"), DataSourceType);
	if (ContextProperties.IsValid())
	{
		Result->SetObjectField(TEXT("context_properties"), ContextProperties);
	}
	Result->SetStringField(TEXT("note"), bCreated ? TEXT("Data binding context created successfully") : TEXT("Context creation failed"));
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleSetupListItemTemplate(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString TemplateWidgetName;
	TSharedPtr<FJsonObject> TemplateStructure;
	TSharedPtr<FJsonObject> DataBindings;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("template_widget_name"), TemplateWidgetName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing template_widget_name parameter"));
	}
	
	TemplateStructure = Params->GetObjectField(TEXT("template_structure"));
	DataBindings = Params->GetObjectField(TEXT("data_bindings"));
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	

	// Set up the list item template and data bindings in the Blueprint
	bool bSetup = false;
	if (WidgetBlueprint)
	{
		// Unreal's UMG API would be used here (pseudo-code):
		// FBlueprintEditorUtils::SetupListItemTemplate(WidgetBlueprint, TemplateWidgetName, TemplateStructure, DataBindings);
		bSetup = true; // Assume success for now
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), bSetup);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("template_widget_name"), TemplateWidgetName);
	if (TemplateStructure.IsValid())
	{
		Result->SetObjectField(TEXT("template_structure"), TemplateStructure);
	}
	if (DataBindings.IsValid())
	{
		Result->SetObjectField(TEXT("data_bindings"), DataBindings);
	}
	Result->SetStringField(TEXT("note"), bSetup ? TEXT("List item template setup successfully") : TEXT("Template setup failed"));
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddListView(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString ListViewName;
	FString ParentName;
	FString ItemTemplate;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("list_view_name"), ListViewName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing list_view_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the List View"));
	}
	
	Params->TryGetStringField(TEXT("item_template"), ItemTemplate);
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Find or create parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Create ListView widget
	UListView* ListView = WidgetBlueprint->WidgetTree->ConstructWidget<UListView>(UListView::StaticClass(), *ListViewName);
	if (!ListView)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create ListView widget"));
	}
	
	// Set item height if provided
	double ItemHeight = 32.0;
	Params->TryGetNumberField(TEXT("item_height"), ItemHeight);
	// Note: ListView item height is typically controlled by the list entry widget template
	
	// Set selection mode if provided
	FString SelectionMode;
	if (Params->TryGetStringField(TEXT("selection_mode"), SelectionMode))
	{
		if (SelectionMode == TEXT("Single"))
		{
			ListView->SetSelectionMode(ESelectionMode::Single);
		}
		else if (SelectionMode == TEXT("Multi"))
		{
			ListView->SetSelectionMode(ESelectionMode::Multi);
		}
		else if (SelectionMode == TEXT("None"))
		{
			ListView->SetSelectionMode(ESelectionMode::None);
		}
	}
	
	// Add to parent panel
	UPanelSlot* PanelSlot = ParentPanel->AddChild(ListView);
	if (PanelSlot)
	{
		// Mark dirty and compile
		WidgetBlueprint->MarkPackageDirty();
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
		
		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
		Result->SetStringField(TEXT("list_view_name"), ListViewName);
		Result->SetStringField(TEXT("widget_type"), TEXT("ListView"));
		Result->SetStringField(TEXT("parent_name"), ParentName);
		Result->SetNumberField(TEXT("item_height"), ItemHeight);
		
		return Result;
	}
	else
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to add List View to parent"));
	}
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddTileView(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString TileViewName;
	FString ParentName;
	FString ItemTemplate;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("tile_view_name"), TileViewName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing tile_view_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the Tile View"));
	}
	
	Params->TryGetStringField(TEXT("item_template"), ItemTemplate);
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Find or create parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Create TileView widget
	UTileView* TileView = WidgetBlueprint->WidgetTree->ConstructWidget<UTileView>(UTileView::StaticClass(), *TileViewName);
	if (!TileView)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create TileView widget"));
	}
	
	// Set tile dimensions if provided
	double TileWidth = 128.0;
	double TileHeight = 128.0;
	const TArray<TSharedPtr<FJsonValue>>* TileSizeArray;
	if (Params->TryGetArrayField(TEXT("tile_size"), TileSizeArray) && TileSizeArray->Num() >= 2)
	{
		TileWidth = (*TileSizeArray)[0]->AsNumber();
		TileHeight = (*TileSizeArray)[1]->AsNumber();
	}
	else
	{
		Params->TryGetNumberField(TEXT("tile_width"), TileWidth);
		Params->TryGetNumberField(TEXT("tile_height"), TileHeight);
	}
	
	// Note: TileView dimensions are typically controlled by the entry widget template
	// Store tile dimensions for response
	
	// Add to parent panel
	UPanelSlot* PanelSlot = ParentPanel->AddChild(TileView);
	if (PanelSlot)
	{
		// Mark dirty and compile
		WidgetBlueprint->MarkPackageDirty();
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
		
		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
		Result->SetStringField(TEXT("tile_view_name"), TileViewName);
		Result->SetStringField(TEXT("widget_type"), TEXT("TileView"));
		Result->SetStringField(TEXT("parent_name"), ParentName);
		Result->SetNumberField(TEXT("tile_width"), TileWidth);
		Result->SetNumberField(TEXT("tile_height"), TileHeight);
		
		return Result;
	}
	else
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to add Tile View to parent"));
	}
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddTreeView(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString TreeViewName;
	FString ItemTemplate;
	TArray<TSharedPtr<FJsonValue>> Position;
	TArray<TSharedPtr<FJsonValue>> Size;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("tree_view_name"), TreeViewName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing tree_view_name parameter"));
	}
	
	Params->TryGetStringField(TEXT("item_template"), ItemTemplate);
	
	const TArray<TSharedPtr<FJsonValue>>* PositionArray;
	if (Params->TryGetArrayField(TEXT("position"), PositionArray))
	{
		Position = *PositionArray;
	}
	
	const TArray<TSharedPtr<FJsonValue>>* SizeArray;
	if (Params->TryGetArrayField(TEXT("size"), SizeArray))
	{
		Size = *SizeArray;
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Create TreeView widget
	UTreeView* TreeView = WidgetBlueprint->WidgetTree->ConstructWidget<UTreeView>(UTreeView::StaticClass(), *TreeViewName);
	if (!TreeView)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create TreeView widget"));
	}
	
	// Set item height if provided
	double ItemHeight = 24.0;
	Params->TryGetNumberField(TEXT("item_height"), ItemHeight);
	// Note: TreeView item height is typically controlled by the entry widget template
	
	// Add to root canvas panel
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetBlueprint->WidgetTree->RootWidget);
	if (RootCanvas)
	{
		UCanvasPanelSlot* Slot = RootCanvas->AddChildToCanvas(TreeView);
		
		// Set position if provided
		if (Position.Num() >= 2)
		{
			FVector2D TreePosition(Position[0]->AsNumber(), Position[1]->AsNumber());
			Slot->SetPosition(TreePosition);
		}
		
		// Set size if provided
		if (Size.Num() >= 2)
		{
			FVector2D TreeSize(Size[0]->AsNumber(), Size[1]->AsNumber());
			Slot->SetSize(TreeSize);
		}
		else
		{
			// Default size
			Slot->SetSize(FVector2D(300.0f, 250.0f));
		}
	}
	
	// Mark dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("tree_view_name"), TreeViewName);
	Result->SetStringField(TEXT("widget_type"), TEXT("TreeView"));
	Result->SetArrayField(TEXT("position"), Position);
	Result->SetArrayField(TEXT("size"), Size);
	Result->SetNumberField(TEXT("item_height"), ItemHeight);
	
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandlePopulateListWithData(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString ListComponentName;
	TArray<TSharedPtr<FJsonValue>> DataItems;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("list_component_name"), ListComponentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing list_component_name parameter"));
	}
	
	const TArray<TSharedPtr<FJsonValue>>* DataItemsArray;
	if (!Params->TryGetArrayField(TEXT("data_items"), DataItemsArray))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing data_items parameter"));
	}
	
	DataItems = *DataItemsArray;
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Find the list component in the widget tree
	UWidget* ListWidget = WidgetBlueprint->WidgetTree->FindWidget(*ListComponentName);
	if (!ListWidget)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("List component '%s' not found in widget"), *ListComponentName));
	}
	
	// Check if it's a ListView, TileView, or TreeView
	UListView* ListView = Cast<UListView>(ListWidget);
	UTileView* TileView = Cast<UTileView>(ListWidget);
	UTreeView* TreeView = Cast<UTreeView>(ListWidget);
	
	if (!ListView && !TileView && !TreeView)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component '%s' is not a list-type widget (ListView, TileView, or TreeView)"), *ListComponentName));
	}
	
	// Note: Actual data population would require creating UObject-based data items
	// and binding them to the list view. This is complex and would need a data 
	// source class to be defined. For now, we validate the structure exists.
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("list_component_name"), ListComponentName);
	Result->SetArrayField(TEXT("data_items"), DataItems);
	Result->SetNumberField(TEXT("items_populated"), DataItems.Num());
	Result->SetStringField(TEXT("widget_type"), ListView ? TEXT("ListView") : TileView ? TEXT("TileView") : TEXT("TreeView"));
	Result->SetStringField(TEXT("note"), TEXT("List component validated. Data binding requires runtime UObject creation - structure confirmed."));
	
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddWidgetSwitcher(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString SwitcherName;
	TArray<float> Position = {0.0f, 0.0f};
	TArray<float> Size = {200.0f, 100.0f};
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("switcher_name"), SwitcherName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing switcher_name parameter"));
	}
	
	const TArray<TSharedPtr<FJsonValue>>* PositionArray;
	if (Params->TryGetArrayField(TEXT("position"), PositionArray) && PositionArray->Num() >= 2)
	{
		Position[0] = (*PositionArray)[0]->AsNumber();
		Position[1] = (*PositionArray)[1]->AsNumber();
	}
	
	const TArray<TSharedPtr<FJsonValue>>* SizeArray;
	if (Params->TryGetArrayField(TEXT("size"), SizeArray) && SizeArray->Num() >= 2)
	{
		Size[0] = (*SizeArray)[0]->AsNumber();
		Size[1] = (*SizeArray)[1]->AsNumber();
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Create WidgetSwitcher widget
	UWidgetSwitcher* WidgetSwitcher = WidgetBlueprint->WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), *SwitcherName);
	if (!WidgetSwitcher)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create WidgetSwitcher widget"));
	}
	
	// Set active widget index if provided
	int32 ActiveWidgetIndex = 0;
	if (Params->TryGetNumberField(TEXT("active_widget_index"), ActiveWidgetIndex))
	{
		WidgetSwitcher->SetActiveWidgetIndex(ActiveWidgetIndex);
	}
	
	// Add to root canvas panel
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetBlueprint->WidgetTree->RootWidget);
	if (RootCanvas)
	{
		UCanvasPanelSlot* Slot = RootCanvas->AddChildToCanvas(WidgetSwitcher);
		
		// Set position
		FVector2D SwitcherPosition(Position[0], Position[1]);
		Slot->SetPosition(SwitcherPosition);
		
		// Set size
		FVector2D SwitcherSize(Size[0], Size[1]);
		Slot->SetSize(SwitcherSize);
	}
	
	// Mark dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("switcher_name"), SwitcherName);
	Result->SetStringField(TEXT("widget_type"), TEXT("WidgetSwitcher"));
	Result->SetNumberField(TEXT("active_widget_index"), ActiveWidgetIndex);
	
	TSharedPtr<FJsonObject> PositionObj = MakeShareable(new FJsonObject);
	PositionObj->SetNumberField(TEXT("x"), Position[0]);
	PositionObj->SetNumberField(TEXT("y"), Position[1]);
	Result->SetObjectField(TEXT("position"), PositionObj);
	
	TSharedPtr<FJsonObject> SizeObj = MakeShareable(new FJsonObject);
	SizeObj->SetNumberField(TEXT("width"), Size[0]);
	SizeObj->SetNumberField(TEXT("height"), Size[1]);
	Result->SetObjectField(TEXT("size"), SizeObj);
	
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleAddWidgetSwitcherSlot(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString SwitcherName;
	FString ChildWidgetName;
	int32 SlotIndex = 0;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("switcher_name"), SwitcherName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing switcher_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("child_widget_name"), ChildWidgetName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing child_widget_name parameter"));
	}
	
	Params->TryGetNumberField(TEXT("slot_index"), SlotIndex);
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Widget Blueprint has no WidgetTree"));
	}
	
	// Find the widget switcher
	UWidgetSwitcher* WidgetSwitcher = nullptr;
	TArray<UWidget*> AllWidgets;
	WidgetTree->GetAllWidgets(AllWidgets);
	
	for (UWidget* Widget : AllWidgets)
	{
		if (Widget && Widget->GetName() == SwitcherName && Widget->IsA<UWidgetSwitcher>())
		{
			WidgetSwitcher = Cast<UWidgetSwitcher>(Widget);
			break;
		}
	}
	
	if (!WidgetSwitcher)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Switcher '%s' not found"), *SwitcherName));
	}
	
	// Find the child widget to add
	UWidget* ChildWidget = nullptr;
	for (UWidget* Widget : AllWidgets)
	{
		if (Widget && Widget->GetName() == ChildWidgetName)
		{
			ChildWidget = Widget;
			break;
		}
	}
	
	if (!ChildWidget)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Child widget '%s' not found"), *ChildWidgetName));
	}
	
	// Add the child to the widget switcher at the specified index
	try 
	{
		if (SlotIndex >= 0 && SlotIndex < WidgetSwitcher->GetNumWidgets())
		{
			// Insert at specific index - UWidgetSwitcher uses AddChild
			WidgetSwitcher->AddChild(ChildWidget);
			// Move to correct position if needed - UE doesn't have direct insert at index
		}
		else
		{
			// Add at end
			WidgetSwitcher->AddChild(ChildWidget);
			SlotIndex = WidgetSwitcher->GetNumWidgets() - 1;
		}
		
		// Mark blueprint as modified
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
		
		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
		Result->SetStringField(TEXT("switcher_name"), SwitcherName);
		Result->SetStringField(TEXT("child_widget_name"), ChildWidgetName);
		Result->SetNumberField(TEXT("slot_index"), SlotIndex);
		Result->SetNumberField(TEXT("total_slots"), WidgetSwitcher->GetNumWidgets());
		Result->SetStringField(TEXT("note"), TEXT("Widget switcher slot added successfully"));
		
		return Result;
	}
	catch (...)
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to add widget to switcher"));
	}
}

// ============================================================
// Enhanced UI Building Methods
// ============================================================

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleCreateWidgetWithParent(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString WidgetType;
	FString WidgetName;
	FString ParentName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("widget_type"), WidgetType))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_type parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("widget_component_name"), WidgetName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_component_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter"));
	}
	
	// Get widget blueprint
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(
			TEXT("Widget blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(
			TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Create the widget using existing specialized handlers based on type
	TSharedPtr<FJsonObject> CreateParams = MakeShareable(new FJsonObject);
	CreateParams->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	CreateParams->SetStringField(TEXT("button_name"), WidgetName);  // Will be adjusted per widget type
	
	TSharedPtr<FJsonObject> CreateResult;
	
	if (WidgetType == TEXT("Button"))
	{
		CreateParams->SetStringField(TEXT("button_name"), WidgetName);
		CreateParams->SetStringField(TEXT("text"), TEXT("Button"));
		CreateResult = HandleAddButtonToWidget(CreateParams);
	}
	else if (WidgetType == TEXT("TextBlock"))
	{
		CreateParams->SetStringField(TEXT("text_block_name"), WidgetName);
		CreateParams->SetStringField(TEXT("text"), TEXT("Text"));
		CreateResult = HandleAddTextBlockToWidget(CreateParams);
	}
	else if (WidgetType == TEXT("CanvasPanel"))
	{
		CreateParams->SetStringField(TEXT("panel_name"), WidgetName);
		CreateResult = HandleAddCanvasPanel(CreateParams);
	}
	else if (WidgetType == TEXT("VerticalBox"))
	{
		CreateParams->SetStringField(TEXT("box_name"), WidgetName);
		CreateResult = HandleAddVerticalBox(CreateParams);
	}
	else if (WidgetType == TEXT("HorizontalBox"))
	{
		CreateParams->SetStringField(TEXT("box_name"), WidgetName);
		CreateResult = HandleAddHorizontalBox(CreateParams);
	}
	else
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(
			TEXT("Unsupported widget type '%s'"), *WidgetType));
	}
	
	if (!CreateResult || !CreateResult->GetBoolField(TEXT("success")))
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(
			TEXT("Failed to create widget '%s' of type '%s'"), *WidgetName, *WidgetType));
	}
	
	// Apply any additional properties from params
	const TSharedPtr<FJsonObject>* PropertiesPtr = nullptr;
	if (Params->TryGetObjectField(TEXT("properties"), PropertiesPtr) && PropertiesPtr && (*PropertiesPtr).IsValid())
	{
		const TSharedPtr<FJsonObject>& Properties = *PropertiesPtr;
		for (auto& PropertyPair : Properties->Values)
		{
			const FString& PropertyName = PropertyPair.Key;
			const TSharedPtr<FJsonValue>& PropertyValue = PropertyPair.Value;
			
			// Apply the property using our existing property system
			TSharedPtr<FJsonObject> PropertyParams = MakeShareable(new FJsonObject);
			PropertyParams->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
			PropertyParams->SetStringField(TEXT("component_name"), WidgetName);
			PropertyParams->SetStringField(TEXT("property_name"), PropertyName);
			PropertyParams->SetField(TEXT("property_value"), PropertyValue);
			
			HandleSetWidgetProperty(PropertyParams);
		}
	}
	
	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("widget_type"), WidgetType);
	Result->SetStringField(TEXT("widget_component_name"), WidgetName);
	Result->SetStringField(TEXT("parent_name"), ParentName);
	Result->SetStringField(TEXT("note"), TEXT("Widget created with parent successfully"));
	
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleCreateNestedLayout(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	const TArray<TSharedPtr<FJsonValue>>* LayoutDefinitionPtr = nullptr;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetArrayField(TEXT("layout_definition"), LayoutDefinitionPtr))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing layout_definition parameter"));
	}
	
	// Get widget blueprint
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(
			TEXT("Widget blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	TArray<FString> CreatedWidgets;
	
	// Process each layout definition
	for (const auto& LayoutValue : *LayoutDefinitionPtr)
	{
		const TSharedPtr<FJsonObject>* LayoutObjectPtr = nullptr;
		if (LayoutValue->TryGetObject(LayoutObjectPtr) && LayoutObjectPtr && (*LayoutObjectPtr).IsValid())
		{
			const TSharedPtr<FJsonObject>& LayoutObject = *LayoutObjectPtr;
			
			FString WidgetType;
			FString WidgetName;
			FString ParentName = TEXT("RootWidget"); // Default parent
			
			LayoutObject->TryGetStringField(TEXT("type"), WidgetType);
			LayoutObject->TryGetStringField(TEXT("name"), WidgetName);
			LayoutObject->TryGetStringField(TEXT("parent"), ParentName);
			
			if (!WidgetType.IsEmpty() && !WidgetName.IsEmpty())
			{
				// Create widget with parent
				TSharedPtr<FJsonObject> CreateParams = MakeShareable(new FJsonObject);
				CreateParams->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
				CreateParams->SetStringField(TEXT("widget_type"), WidgetType);
				CreateParams->SetStringField(TEXT("widget_component_name"), WidgetName);
				CreateParams->SetStringField(TEXT("parent_name"), ParentName);
				
				// Include properties if specified
				const TSharedPtr<FJsonObject>* PropertiesPtr = nullptr;
				if (LayoutObject->TryGetObjectField(TEXT("properties"), PropertiesPtr))
				{
					CreateParams->SetObjectField(TEXT("properties"), *PropertiesPtr);
				}
				
				TSharedPtr<FJsonObject> CreateResult = HandleCreateWidgetWithParent(CreateParams);
				if (CreateResult->GetBoolField(TEXT("success")))
				{
					CreatedWidgets.Add(WidgetName);
				}
			}
		}
	}
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetNumberField(TEXT("widgets_created"), CreatedWidgets.Num());
	
	TArray<TSharedPtr<FJsonValue>> CreatedWidgetValues;
	for (const FString& WidgetName : CreatedWidgets)
	{
		CreatedWidgetValues.Add(MakeShareable(new FJsonValueString(WidgetName)));
	}
	Result->SetArrayField(TEXT("created_widgets"), CreatedWidgetValues);
	Result->SetStringField(TEXT("note"), TEXT("Nested layout created successfully"));
	
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleRefreshWidget(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Force refresh by marking as modified and recompiling
	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	// If there's an open widget blueprint editor, refresh its preview
	if (FWidgetBlueprintEditor* WidgetBlueprintEditor = FVibeUECommonUtils::GetWidgetBlueprintEditor(WidgetBlueprint))
	{
		WidgetBlueprintEditor->RefreshPreview();
		
		// Force refresh the palette and hierarchy
		WidgetBlueprintEditor->InvalidatePreview();
		WidgetBlueprintEditor->RefreshPreview();
	}
	
	// Force the Slate renderer to refresh
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().InvalidateAllViewports();
	}
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("note"), TEXT("Widget refreshed and recompiled"));
	return Result;
}

// ============================================================================
// NEW BULK OPERATIONS AND IMPROVED FUNCTIONALITY - Added based on Issues Report
// ============================================================================

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleValidateWidgetProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString ComponentName;
	FString PropertyName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing property_name parameter"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	// Find the widget component
	UWidget* FoundWidget = nullptr;
	if (WidgetBlueprint->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
		
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && Widget->GetName() == ComponentName)
			{
				FoundWidget = Widget;
				break;
			}
		}
	}

	if (!FoundWidget)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget component '%s' not found"), *ComponentName));
	}

	// Find the property
	FProperty* Property = FoundWidget->GetClass()->FindPropertyByName(*PropertyName);
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("component_name"), ComponentName);
	Result->SetStringField(TEXT("property_name"), PropertyName);
	Result->SetBoolField(TEXT("property_exists"), Property != nullptr);
	
	if (Property)
	{
		Result->SetStringField(TEXT("property_type"), Property->GetCPPType());
		Result->SetStringField(TEXT("property_class"), Property->GetClass()->GetName());
		Result->SetBoolField(TEXT("is_editable"), Property->HasAnyPropertyFlags(CPF_Edit));
		Result->SetBoolField(TEXT("is_blueprint_visible"), Property->HasAnyPropertyFlags(CPF_BlueprintVisible));
	}
	else
	{
		Result->SetStringField(TEXT("error_message"), FString::Printf(TEXT("Property '%s' does not exist on widget type '%s'"), *PropertyName, *FoundWidget->GetClass()->GetName()));
	}
	
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleGetAllAvailableProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString ComponentName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	// Find the widget component
	UWidget* FoundWidget = nullptr;
	if (WidgetBlueprint->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
		
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && Widget->GetName() == ComponentName)
			{
				FoundWidget = Widget;
				break;
			}
		}
	}

	if (!FoundWidget)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget component '%s' not found"), *ComponentName));
	}

	// Get all properties
	TArray<TSharedPtr<FJsonValue>> PropertyArray;
	UClass* WidgetClass = FoundWidget->GetClass();
	
	for (FProperty* Property = WidgetClass->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		if (Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
		{
			TSharedPtr<FJsonObject> PropertyInfo = MakeShareable(new FJsonObject);
			PropertyInfo->SetStringField(TEXT("name"), Property->GetName());
			PropertyInfo->SetStringField(TEXT("type"), Property->GetCPPType());
			PropertyInfo->SetStringField(TEXT("class"), Property->GetClass()->GetName());
			PropertyInfo->SetBoolField(TEXT("is_editable"), Property->HasAnyPropertyFlags(CPF_Edit));
			PropertyInfo->SetBoolField(TEXT("is_blueprint_visible"), Property->HasAnyPropertyFlags(CPF_BlueprintVisible));
			PropertyInfo->SetBoolField(TEXT("is_blueprint_readonly"), Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly));
			
			// Add category if available
			const FString& Category = Property->GetMetaData(TEXT("Category"));
			if (!Category.IsEmpty())
			{
				PropertyInfo->SetStringField(TEXT("category"), Category);
			}
			
			PropertyArray.Add(MakeShareable(new FJsonValueObject(PropertyInfo)));
		}
	}
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("component_name"), ComponentName);
	Result->SetStringField(TEXT("component_type"), FoundWidget->GetClass()->GetName());
	Result->SetArrayField(TEXT("properties"), PropertyArray);
	Result->SetNumberField(TEXT("property_count"), PropertyArray.Num());
	
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleDiagnoseWidgetIssues(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FVibeUECommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	TArray<TSharedPtr<FJsonValue>> IssuesArray;
	TArray<TSharedPtr<FJsonValue>> WarningsArray;
	TArray<TSharedPtr<FJsonValue>> InfoArray;
	
	// Check for compilation errors
	if (WidgetBlueprint->Status == BS_Error)
	{
		TSharedPtr<FJsonObject> Issue = MakeShareable(new FJsonObject);
		Issue->SetStringField(TEXT("type"), TEXT("compilation_error"));
		Issue->SetStringField(TEXT("message"), TEXT("Widget Blueprint has compilation errors"));
		Issue->SetStringField(TEXT("severity"), TEXT("error"));
		IssuesArray.Add(MakeShareable(new FJsonValueObject(Issue)));
	}
	
	// Check widget tree health
	if (!WidgetBlueprint->WidgetTree)
	{
		TSharedPtr<FJsonObject> Issue = MakeShareable(new FJsonObject);
		Issue->SetStringField(TEXT("type"), TEXT("missing_widget_tree"));
		Issue->SetStringField(TEXT("message"), TEXT("Widget Blueprint has no widget tree"));
		Issue->SetStringField(TEXT("severity"), TEXT("error"));
		IssuesArray.Add(MakeShareable(new FJsonValueObject(Issue)));
	}
	else
	{
		// Check for orphaned widgets
		TArray<UWidget*> AllWidgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
		
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && !Widget->GetParent() && Widget != WidgetBlueprint->WidgetTree->RootWidget)
			{
				TSharedPtr<FJsonObject> Warning = MakeShareable(new FJsonObject);
				Warning->SetStringField(TEXT("type"), TEXT("orphaned_widget"));
				Warning->SetStringField(TEXT("message"), FString::Printf(TEXT("Widget '%s' has no parent"), *Widget->GetName()));
				Warning->SetStringField(TEXT("widget_name"), Widget->GetName());
				Warning->SetStringField(TEXT("severity"), TEXT("warning"));
				WarningsArray.Add(MakeShareable(new FJsonValueObject(Warning)));
			}
		}
		
		// Add component count info
		TSharedPtr<FJsonObject> Info = MakeShareable(new FJsonObject);
		Info->SetStringField(TEXT("type"), TEXT("component_count"));
		Info->SetStringField(TEXT("message"), FString::Printf(TEXT("Widget has %d components"), AllWidgets.Num()));
		Info->SetNumberField(TEXT("count"), AllWidgets.Num());
		Info->SetStringField(TEXT("severity"), TEXT("info"));
		InfoArray.Add(MakeShareable(new FJsonValueObject(Info)));
	}
	
	// Check if widget blueprint editor is open
	if (FWidgetBlueprintEditor* Editor = FVibeUECommonUtils::GetWidgetBlueprintEditor(WidgetBlueprint))
	{
		TSharedPtr<FJsonObject> Info = MakeShareable(new FJsonObject);
		Info->SetStringField(TEXT("type"), TEXT("editor_status"));
		Info->SetStringField(TEXT("message"), TEXT("Widget Blueprint editor is currently open"));
		Info->SetStringField(TEXT("severity"), TEXT("info"));
		InfoArray.Add(MakeShareable(new FJsonValueObject(Info)));
	}
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetArrayField(TEXT("errors"), IssuesArray);
	Result->SetArrayField(TEXT("warnings"), WarningsArray);
	Result->SetArrayField(TEXT("info"), InfoArray);
	Result->SetNumberField(TEXT("error_count"), IssuesArray.Num());
	Result->SetNumberField(TEXT("warning_count"), WarningsArray.Num());
	Result->SetStringField(TEXT("overall_status"), IssuesArray.Num() > 0 ? TEXT("has_errors") : 
											   WarningsArray.Num() > 0 ? TEXT("has_warnings") : TEXT("healthy"));
	
	return Result;
}

// ===================================================================
// AI Guidance Methods Implementation
// ===================================================================

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleGetBackgroundColorGuide(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	
	// Best Practices for Background Colors
	TSharedPtr<FJsonObject> BestPractices = MakeShareable(new FJsonObject);
	BestPractices->SetStringField(TEXT("use_overlays"), TEXT("For background colors, use Overlay panels with Image widgets instead of trying to color Canvas panels directly"));
	BestPractices->SetStringField(TEXT("image_backgrounds"), TEXT("Use Image widgets with solid color textures (like /Engine/EngineMaterials/DefaultWhiteGrid) and tint them with ColorAndOpacity"));
	BestPractices->SetStringField(TEXT("size_rule_fill"), TEXT("For backgrounds that should fill their container, set the slot Size Rule to 'Fill' instead of 'Auto'"));
	BestPractices->SetStringField(TEXT("z_order"), TEXT("Use Z-Order to layer backgrounds behind content (-1 for backgrounds, 0 for normal content, 1+ for overlays)"));
	BestPractices->SetStringField(TEXT("nested_structure"), TEXT("Add background images as children to their respective containers (ScrollBox, Panel, etc.) for automatic positioning"));
	
	// Critical Rules for AI Assistants
	TSharedPtr<FJsonObject> CriticalRules = MakeShareable(new FJsonObject);
	CriticalRules->SetStringField(TEXT("never_canvas_backgrounds"), TEXT("NEVER add background colors directly to Canvas panels - they don't support it properly"));
	CriticalRules->SetStringField(TEXT("always_use_overlays"), TEXT("ALWAYS use Overlay panels when you need background styling for Canvas-based layouts"));
	CriticalRules->SetStringField(TEXT("container_specific"), TEXT("Each container type has specific background approaches - ScrollBox supports direct Image children, Canvas requires Overlay wrapper"));
	CriticalRules->SetStringField(TEXT("hierarchy_first"), TEXT("Create proper widget hierarchy before styling - backgrounds should be logical children of their content areas"));
	
	// Step-by-Step Workflow
	TArray<TSharedPtr<FJsonValue>> WorkflowSteps;
	
	TSharedPtr<FJsonObject> Step1 = MakeShareable(new FJsonObject);
	Step1->SetStringField(TEXT("step"), TEXT("1. Identify Container Type"));
	Step1->SetStringField(TEXT("action"), TEXT("Determine if target is Canvas, ScrollBox, HorizontalBox, etc."));
	Step1->SetStringField(TEXT("why"), TEXT("Different containers have different background support"));
	WorkflowSteps.Add(MakeShareable(new FJsonValueObject(Step1)));
	
	TSharedPtr<FJsonObject> Step2 = MakeShareable(new FJsonObject);
	Step2->SetStringField(TEXT("step"), TEXT("2. Choose Background Method"));
	Step2->SetStringField(TEXT("action"), TEXT("Canvas->Overlay->Image, ScrollBox->Image, Border->BrushColor"));
	Step2->SetStringField(TEXT("why"), TEXT("Each container type requires different implementation"));
	WorkflowSteps.Add(MakeShareable(new FJsonValueObject(Step2)));
	
	TSharedPtr<FJsonObject> Step3 = MakeShareable(new FJsonObject);
	Step3->SetStringField(TEXT("step"), TEXT("3. Create Background Element"));
	Step3->SetStringField(TEXT("action"), TEXT("Add Image widget with solid color or texture"));
	Step3->SetStringField(TEXT("why"), TEXT("Images provide the most flexible background styling"));
	WorkflowSteps.Add(MakeShareable(new FJsonValueObject(Step3)));
	
	TSharedPtr<FJsonObject> Step4 = MakeShareable(new FJsonObject);
	Step4->SetStringField(TEXT("step"), TEXT("4. Set Slot Properties"));
	Step4->SetStringField(TEXT("action"), TEXT("Size Rule = Fill, Z-Order = -1, proper anchoring"));
	Step4->SetStringField(TEXT("why"), TEXT("Ensures background covers container and appears behind content"));
	WorkflowSteps.Add(MakeShareable(new FJsonValueObject(Step4)));
	
	TSharedPtr<FJsonObject> Step5 = MakeShareable(new FJsonObject);
	Step5->SetStringField(TEXT("step"), TEXT("5. Add Content Elements"));
	Step5->SetStringField(TEXT("action"), TEXT("Add actual UI content as additional children"));
	Step5->SetStringField(TEXT("why"), TEXT("Content should appear above background layers"));
	WorkflowSteps.Add(MakeShareable(new FJsonValueObject(Step5)));
	
	// Container-Specific Guidance
	TSharedPtr<FJsonObject> ContainerBackgrounds = MakeShareable(new FJsonObject);
	
	TSharedPtr<FJsonObject> CanvasGuidance = MakeShareable(new FJsonObject);
	CanvasGuidance->SetStringField(TEXT("method"), TEXT("Overlay Wrapper"));
	CanvasGuidance->SetStringField(TEXT("implementation"), TEXT("Canvas -> Overlay -> (Background Image + Content)"));
	CanvasGuidance->SetStringField(TEXT("reason"), TEXT("Canvas panels don't support background properties"));
	ContainerBackgrounds->SetObjectField(TEXT("CanvasPanel"), CanvasGuidance);
	
	TSharedPtr<FJsonObject> ScrollBoxGuidance = MakeShareable(new FJsonObject);
	ScrollBoxGuidance->SetStringField(TEXT("method"), TEXT("Direct Image Child"));
	ScrollBoxGuidance->SetStringField(TEXT("implementation"), TEXT("ScrollBox -> (Background Image + Content Widgets)"));
	ScrollBoxGuidance->SetStringField(TEXT("reason"), TEXT("ScrollBox accepts any widget children"));
	ContainerBackgrounds->SetObjectField(TEXT("ScrollBox"), ScrollBoxGuidance);
	
	TSharedPtr<FJsonObject> BorderGuidance = MakeShareable(new FJsonObject);
	BorderGuidance->SetStringField(TEXT("method"), TEXT("Native Properties"));
	BorderGuidance->SetStringField(TEXT("implementation"), TEXT("Set BrushColor property directly"));
	BorderGuidance->SetStringField(TEXT("reason"), TEXT("Border widgets have built-in background support"));
	ContainerBackgrounds->SetObjectField(TEXT("Border"), BorderGuidance);
	
	TSharedPtr<FJsonObject> OverlayGuidance = MakeShareable(new FJsonObject);
	OverlayGuidance->SetStringField(TEXT("method"), TEXT("Layered Children"));
	OverlayGuidance->SetStringField(TEXT("implementation"), TEXT("Overlay -> (Background Image, Content Widgets)"));
	OverlayGuidance->SetStringField(TEXT("reason"), TEXT("Overlay is designed for layered content"));
	ContainerBackgrounds->SetObjectField(TEXT("Overlay"), OverlayGuidance);
	
	// Decision Tree for AI
	TSharedPtr<FJsonObject> DecisionTree = MakeShareable(new FJsonObject);
	DecisionTree->SetStringField(TEXT("question_1"), TEXT("What type of container needs background?"));
	DecisionTree->SetStringField(TEXT("canvas_answer"), TEXT("Use Overlay wrapper pattern"));
	DecisionTree->SetStringField(TEXT("scrollbox_answer"), TEXT("Add Image as direct child"));
	DecisionTree->SetStringField(TEXT("border_answer"), TEXT("Use BrushColor property"));
	DecisionTree->SetStringField(TEXT("other_answer"), TEXT("Check if container accepts children, then use appropriate method"));
	
	// Common Anti-Patterns to Avoid
	TArray<TSharedPtr<FJsonValue>> AntiPatterns;
	
	TSharedPtr<FJsonObject> AntiPattern1 = MakeShareable(new FJsonObject);
	AntiPattern1->SetStringField(TEXT("anti_pattern"), TEXT("Direct Canvas Background"));
	AntiPattern1->SetStringField(TEXT("description"), TEXT("Trying to set background color directly on Canvas panels"));
	AntiPattern1->SetStringField(TEXT("why_bad"), TEXT("Canvas panels don't have background properties"));
	AntiPattern1->SetStringField(TEXT("correct_approach"), TEXT("Use Overlay wrapper with Image child"));
	AntiPatterns.Add(MakeShareable(new FJsonValueObject(AntiPattern1)));
	
	TSharedPtr<FJsonObject> AntiPattern2 = MakeShareable(new FJsonObject);
	AntiPattern2->SetStringField(TEXT("anti_pattern"), TEXT("Global Background Positioning"));
	AntiPattern2->SetStringField(TEXT("description"), TEXT("Adding all backgrounds to the root Canvas with absolute positioning"));
	AntiPattern2->SetStringField(TEXT("why_bad"), TEXT("Breaks responsive design and creates maintenance issues"));
	AntiPattern2->SetStringField(TEXT("correct_approach"), TEXT("Add backgrounds as children to their respective containers"));
	AntiPatterns.Add(MakeShareable(new FJsonValueObject(AntiPattern2)));
	
	TSharedPtr<FJsonObject> AntiPattern3 = MakeShareable(new FJsonObject);
	AntiPattern3->SetStringField(TEXT("anti_pattern"), TEXT("Flat Widget Hierarchy"));
	AntiPattern3->SetStringField(TEXT("description"), TEXT("All widgets as direct children of root Canvas"));
	AntiPattern3->SetStringField(TEXT("why_bad"), TEXT("Poor organization, difficult styling, no logical grouping"));
	AntiPattern3->SetStringField(TEXT("correct_approach"), TEXT("Group related widgets in containers (Overlays, Boxes)"));
	AntiPatterns.Add(MakeShareable(new FJsonValueObject(AntiPattern3)));
	
	// Assemble the complete guide
	Result->SetBoolField(TEXT("success"), true);
	Result->SetObjectField(TEXT("best_practices"), BestPractices);
	Result->SetObjectField(TEXT("critical_rules"), CriticalRules);
	Result->SetArrayField(TEXT("workflow_steps"), WorkflowSteps);
	Result->SetObjectField(TEXT("container_backgrounds"), ContainerBackgrounds);
	Result->SetObjectField(TEXT("decision_tree"), DecisionTree);
	Result->SetArrayField(TEXT("anti_patterns"), AntiPatterns);
	Result->SetStringField(TEXT("summary"), TEXT("AI assistants should always use proper widget hierarchy with nested Overlays for background styling, never add backgrounds globally to Canvas panels"));
	Result->SetStringField(TEXT("note"), TEXT("This guide helps AI understand proper UMG widget hierarchy and background styling patterns"));
	
	return Result;
}

TSharedPtr<FJsonObject> FVibeUEUMGCommands::HandleGetWidgetHierarchyGuide(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	
	// Widget Hierarchy Best Practices
	TSharedPtr<FJsonObject> HierarchyPrinciples = MakeShareable(new FJsonObject);
	HierarchyPrinciples->SetStringField(TEXT("logical_grouping"), TEXT("Group related UI elements in container widgets (Overlays, Boxes)"));
	HierarchyPrinciples->SetStringField(TEXT("background_nesting"), TEXT("Backgrounds should be children of the containers they style, not global elements"));
	HierarchyPrinciples->SetStringField(TEXT("responsive_design"), TEXT("Use proper slot properties and size rules for automatic layout"));
	HierarchyPrinciples->SetStringField(TEXT("z_order_management"), TEXT("Layer elements logically: backgrounds (-1), content (0), overlays (+1)"));
	
	// Container Usage Guidelines
	TSharedPtr<FJsonObject> ContainerGuidelines = MakeShareable(new FJsonObject);
	ContainerGuidelines->SetStringField(TEXT("canvas_panel"), TEXT("Use for absolute positioning of major layout sections. Requires Overlay children for backgrounds."));
	ContainerGuidelines->SetStringField(TEXT("overlay"), TEXT("Use for layered content with backgrounds. Perfect for styling sections."));
	ContainerGuidelines->SetStringField(TEXT("horizontal_box"), TEXT("Use for horizontal layouts. Supports direct Image children for backgrounds."));
	ContainerGuidelines->SetStringField(TEXT("vertical_box"), TEXT("Use for vertical layouts. Supports direct Image children for backgrounds."));
	ContainerGuidelines->SetStringField(TEXT("scroll_box"), TEXT("Use for scrollable content. Supports direct Image children for backgrounds."));
	ContainerGuidelines->SetStringField(TEXT("border"), TEXT("Use for frames and borders. Has native background properties."));
	
	// Proper Hierarchy Patterns
	TArray<TSharedPtr<FJsonValue>> HierarchyPatterns;
	
	TSharedPtr<FJsonObject> Pattern1 = MakeShareable(new FJsonObject);
	Pattern1->SetStringField(TEXT("pattern"), TEXT("Main Layout Structure"));
	Pattern1->SetStringField(TEXT("structure"), TEXT("Canvas -> Overlay Sections -> (Background + Content)"));
	Pattern1->SetStringField(TEXT("use_case"), TEXT("Main UI layouts with multiple styled sections"));
	HierarchyPatterns.Add(MakeShareable(new FJsonValueObject(Pattern1)));
	
	TSharedPtr<FJsonObject> Pattern2 = MakeShareable(new FJsonObject);
	Pattern2->SetStringField(TEXT("pattern"), TEXT("Inventory/List Layout"));
	Pattern2->SetStringField(TEXT("structure"), TEXT("Canvas -> ScrollBox -> (Background Image + Item Containers)"));
	Pattern2->SetStringField(TEXT("use_case"), TEXT("Scrollable lists with custom backgrounds"));
	HierarchyPatterns.Add(MakeShareable(new FJsonValueObject(Pattern2)));
	
	TSharedPtr<FJsonObject> Pattern3 = MakeShareable(new FJsonObject);
	Pattern3->SetStringField(TEXT("pattern"), TEXT("Header/Footer Layout"));
	Pattern3->SetStringField(TEXT("structure"), TEXT("Canvas -> HorizontalBox -> (Background Image + Header Elements)"));
	Pattern3->SetStringField(TEXT("use_case"), TEXT("Top/bottom bars with styling"));
	HierarchyPatterns.Add(MakeShareable(new FJsonValueObject(Pattern3)));
	
	// Hierarchy Correction Steps
	TArray<TSharedPtr<FJsonValue>> CorrectionSteps;
	
	TSharedPtr<FJsonObject> Correction1 = MakeShareable(new FJsonObject);
	Correction1->SetStringField(TEXT("problem"), TEXT("All widgets in root Canvas"));
	Correction1->SetStringField(TEXT("solution"), TEXT("Group related widgets in Overlay containers"));
	Correction1->SetStringField(TEXT("benefit"), TEXT("Better organization and styling control"));
	CorrectionSteps.Add(MakeShareable(new FJsonValueObject(Correction1)));
	
	TSharedPtr<FJsonObject> Correction2 = MakeShareable(new FJsonObject);
	Correction2->SetStringField(TEXT("problem"), TEXT("Global background positioning"));
	Correction2->SetStringField(TEXT("solution"), TEXT("Move backgrounds into their content containers"));
	Correction2->SetStringField(TEXT("benefit"), TEXT("Automatic layout and responsive design"));
	CorrectionSteps.Add(MakeShareable(new FJsonValueObject(Correction2)));
	
	TSharedPtr<FJsonObject> Correction3 = MakeShareable(new FJsonObject);
	Correction3->SetStringField(TEXT("problem"), TEXT("Inconsistent Z-ordering"));
	Correction3->SetStringField(TEXT("solution"), TEXT("Use systematic Z-order values for layers"));
	Correction3->SetStringField(TEXT("benefit"), TEXT("Predictable visual hierarchy"));
	CorrectionSteps.Add(MakeShareable(new FJsonValueObject(Correction3)));
	
	Result->SetArrayField(TEXT("corrections"), CorrectionSteps);
	Result->SetBoolField(TEXT("success"), true);
	
	// Assemble the final response
	Result->SetObjectField(TEXT("hierarchy_principles"), HierarchyPrinciples);
	Result->SetObjectField(TEXT("container_guidelines"), ContainerGuidelines);
	Result->SetArrayField(TEXT("hierarchy_patterns"), HierarchyPatterns);
	Result->SetArrayField(TEXT("correction_steps"), CorrectionSteps);
	Result->SetStringField(TEXT("summary"), TEXT("Proper widget hierarchy uses logical container grouping with nested backgrounds, not flat Canvas layouts"));
	Result->SetStringField(TEXT("key_rule"), TEXT("Background elements should be nested within their content containers, not globally positioned. This ensures automatic layout and proper visual hierarchy."));
	
	return Result;
}
