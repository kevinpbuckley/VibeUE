// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/UMGCommands.h"
#include "Commands/CommonUtils.h"
#include "Core/ServiceContext.h"
#include "Services/UMG/WidgetDiscoveryService.h"
#include "Services/UMG/WidgetLifecycleService.h"
#include "Services/UMG/WidgetComponentService.h"
#include "Services/UMG/WidgetPropertyService.h"
#include "Services/UMG/WidgetStyleService.h"
#include "Services/UMG/WidgetEventService.h"
#include "Services/UMG/WidgetReflectionService.h"
#include "WidgetBlueprint.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "Blueprint/WidgetTree.h"
#include "JsonObjectConverter.h"

// ============================================================================
// Constructor
// ============================================================================

FUMGCommands::FUMGCommands()
{
	// Initialize service context
	ServiceContext = MakeShared<FServiceContext>();
	
	// Initialize all UMG services
	DiscoveryService = MakeShared<FWidgetDiscoveryService>(ServiceContext);
	LifecycleService = MakeShared<FWidgetLifecycleService>(ServiceContext);
	ComponentService = MakeShared<FWidgetComponentService>(ServiceContext);
	PropertyService = MakeShared<FWidgetPropertyService>(ServiceContext);
	StyleService = MakeShared<FWidgetStyleService>(ServiceContext);
	EventService = MakeShared<FWidgetEventService>(ServiceContext);
	ReflectionService = MakeShared<FWidgetReflectionService>(ServiceContext);
}

// ============================================================================
// Helper Methods
// ============================================================================

TSharedPtr<FJsonObject> FUMGCommands::CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	if (Data.IsValid())
	{
		// Merge Data fields into Response
		for (const auto& Pair : Data->Values)
		{
			Response->SetField(Pair.Key, Pair.Value);
		}
	}
	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::CreateErrorResponse(const FString& ErrorCode, const FString& ErrorMessage)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), false);
	Response->SetStringField(TEXT("error_code"), ErrorCode);
	Response->SetStringField(TEXT("error"), ErrorMessage);
	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::ComponentToJson(UWidget* Component)
{
	TSharedPtr<FJsonObject> ComponentObj = MakeShared<FJsonObject>();
	if (Component)
	{
		ComponentObj->SetStringField(TEXT("name"), Component->GetName());
		ComponentObj->SetStringField(TEXT("type"), Component->GetClass()->GetName());
	}
	return ComponentObj;
}

// Helper to convert array of strings to JSON array
TArray<TSharedPtr<FJsonValue>> FUMGCommands::StringArrayToJson(const TArray<FString>& Strings)
{
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	for (const FString& Str : Strings)
	{
		JsonArray.Add(MakeShared<FJsonValueString>(Str));
	}
	return JsonArray;
}

// Helper to handle generic component addition (used by all add_* handlers)
TSharedPtr<FJsonObject> FUMGCommands::HandleAddComponentGeneric(
	const TSharedPtr<FJsonObject>& Params,
	const FString& ComponentType)
{
	FString BlueprintName, WidgetName, ParentName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName) ||
		!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), 
			TEXT("Missing required parameters"));
	}
	
	// parent_name is optional for some handlers
	Params->TryGetStringField(TEXT("parent_name"), ParentName);
	
	TResult<UWidgetBlueprint*> WidgetResult = DiscoveryService->FindWidget(BlueprintName);
	if (WidgetResult.IsError())
	{
		return CreateErrorResponse(WidgetResult.GetErrorCode(), WidgetResult.GetErrorMessage());
	}
	
	TResult<UWidget*> ComponentResult = ComponentService->AddComponent(
		WidgetResult.GetValue(), ComponentType, WidgetName, ParentName);
	
	if (ComponentResult.IsError())
	{
		return CreateErrorResponse(ComponentResult.GetErrorCode(), ComponentResult.GetErrorMessage());
	}
	
	return CreateSuccessResponse(ComponentToJson(ComponentResult.GetValue()));
}

// Helper to convert FWidgetComponentInfo to JSON
TSharedPtr<FJsonObject> FUMGCommands::ComponentInfoToJson(const FWidgetComponentInfo& Info)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("name"), Info.Name);
	Obj->SetStringField(TEXT("type"), Info.Type);
	Obj->SetStringField(TEXT("parent"), Info.ParentName);
	Obj->SetBoolField(TEXT("is_variable"), Info.bIsVariable);
	Obj->SetArrayField(TEXT("children"), StringArrayToJson(Info.Children));
	return Obj;
}

// Helper to convert FPropertyInfo to JSON
TSharedPtr<FJsonObject> FUMGCommands::PropertyInfoToJson(const FPropertyInfo& Info)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("name"), Info.PropertyName);
	Obj->SetStringField(TEXT("type"), Info.PropertyType);
	Obj->SetStringField(TEXT("value"), Info.CurrentValue);
	Obj->SetBoolField(TEXT("editable"), Info.bIsEditable);
	Obj->SetStringField(TEXT("category"), Info.Category);
	Obj->SetStringField(TEXT("tooltip"), Info.Tooltip);
	
	if (!Info.MinValue.IsEmpty())
	{
		Obj->SetStringField(TEXT("min_value"), Info.MinValue);
	}
	if (!Info.MaxValue.IsEmpty())
	{
		Obj->SetStringField(TEXT("max_value"), Info.MaxValue);
	}
	if (Info.EnumValues.Num() > 0)
	{
		Obj->SetArrayField(TEXT("enum_values"), StringArrayToJson(Info.EnumValues));
	}
	
	return Obj;
}

// ============================================================================
// Command Router
// ============================================================================

TSharedPtr<FJsonObject> FUMGCommands::HandleCommand(const FString& CommandName, const TSharedPtr<FJsonObject>& Params)
{
	// Original UMG Commands
	if (CommandName == TEXT("create_umg_widget_blueprint"))
		return HandleCreateUMGWidgetBlueprint(Params);
	else if (CommandName == TEXT("add_text_block_to_widget"))
		return HandleAddTextBlockToWidget(Params);
	else if (CommandName == TEXT("add_button_to_widget"))
		return HandleAddButtonToWidget(Params);
	
	// UMG Discovery Commands
	else if (CommandName == TEXT("search_items"))
		return HandleSearchItems(Params);
	else if (CommandName == TEXT("get_widget_blueprint_info"))
		return HandleGetWidgetBlueprintInfo(Params);
	else if (CommandName == TEXT("list_widget_components"))
		return HandleListWidgetComponents(Params);
	else if (CommandName == TEXT("get_widget_component_properties"))
		return HandleGetWidgetComponentProperties(Params);
	else if (CommandName == TEXT("get_available_widget_types"))
		return HandleGetAvailableWidgetTypes(Params);
	else if (CommandName == TEXT("validate_widget_hierarchy"))
		return HandleValidateWidgetHierarchy(Params);
	
	// UMG Component Commands
	else if (CommandName == TEXT("add_editable_text"))
		return HandleAddEditableText(Params);
	else if (CommandName == TEXT("add_editable_text_box"))
		return HandleAddEditableTextBox(Params);
	else if (CommandName == TEXT("add_rich_text_block"))
		return HandleAddRichTextBlock(Params);
	else if (CommandName == TEXT("add_check_box"))
		return HandleAddCheckBox(Params);
	else if (CommandName == TEXT("add_slider"))
		return HandleAddSlider(Params);
	else if (CommandName == TEXT("add_progress_bar"))
		return HandleAddProgressBar(Params);
	else if (CommandName == TEXT("add_image"))
		return HandleAddImage(Params);
	else if (CommandName == TEXT("add_spacer"))
		return HandleAddSpacer(Params);
	
	// UMG Layout Commands
	else if (CommandName == TEXT("add_canvas_panel"))
		return HandleAddCanvasPanel(Params);
	else if (CommandName == TEXT("add_size_box"))
		return HandleAddSizeBox(Params);
	else if (CommandName == TEXT("add_overlay"))
		return HandleAddOverlay(Params);
	else if (CommandName == TEXT("add_horizontal_box"))
		return HandleAddHorizontalBox(Params);
	else if (CommandName == TEXT("add_vertical_box"))
		return HandleAddVerticalBox(Params);
	else if (CommandName == TEXT("add_scroll_box"))
		return HandleAddScrollBox(Params);
	else if (CommandName == TEXT("add_grid_panel"))
		return HandleAddGridPanel(Params);
	else if (CommandName == TEXT("add_widget_switcher"))
		return HandleAddWidgetSwitcher(Params);
	else if (CommandName == TEXT("add_widget_switcher_slot"))
		return HandleAddWidgetSwitcherSlot(Params);
	else if (CommandName == TEXT("add_child_to_panel"))
		return HandleAddChildToPanel(Params);
	else if (CommandName == TEXT("remove_umg_component"))
		return HandleRemoveUMGComponent(Params);
	else if (CommandName == TEXT("set_widget_slot_properties"))
		return HandleSetWidgetSlotProperties(Params);
	
	// Property Management Commands
	else if (CommandName == TEXT("set_widget_property"))
		return HandleSetWidgetProperty(Params);
	else if (CommandName == TEXT("get_widget_property"))
		return HandleGetWidgetProperty(Params);
	else if (CommandName == TEXT("list_widget_properties"))
		return HandleListWidgetProperties(Params);
	
	// Event Commands
	else if (CommandName == TEXT("bind_input_events"))
		return HandleBindInputEvents(Params);
	else if (CommandName == TEXT("get_available_events"))
		return HandleGetAvailableEvents(Params);
	
	// Deletion Commands
	else if (CommandName == TEXT("delete_widget_blueprint"))
		return HandleDeleteWidgetBlueprint(Params);

	return CreateErrorResponse(TEXT("UNKNOWN_COMMAND"), 
		FString::Printf(TEXT("Unknown UMG command: %s"), *CommandName));
}

// ============================================================================
// Lifecycle Commands
// ============================================================================

TSharedPtr<FJsonObject> FUMGCommands::HandleCreateUMGWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing 'name' parameter"));
	}

	FString PackagePath = TEXT("/Game/UI/");
	Params->TryGetStringField(TEXT("path"), PackagePath);
	
	TResult<TPair<UWidgetBlueprint*, FWidgetInfo>> Result = 
		LifecycleService->CreateWidget(BlueprintName, PackagePath);
	
	if (Result.IsError())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}
	
	const FWidgetInfo& Info = Result.GetValue().Value;
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Info.Name);
	Data->SetStringField(TEXT("path"), Info.Path);
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleDeleteWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing 'widget_name' parameter"));
	}

	bool CheckReferences = Params->HasField(TEXT("check_references")) ? 
		Params->GetBoolField(TEXT("check_references")) : true;

	TResult<UWidgetBlueprint*> WidgetResult = DiscoveryService->FindWidget(WidgetName);
	if (WidgetResult.IsError())
	{
		return CreateErrorResponse(WidgetResult.GetErrorCode(), WidgetResult.GetErrorMessage());
	}

	UWidgetBlueprint* WidgetBlueprint = WidgetResult.GetValue();
	FString AssetPath = WidgetBlueprint->GetPathName();
	
	int32 ReferenceCount = 0;
	TResult<void> DeleteResult = LifecycleService->DeleteWidget(
		WidgetBlueprint, CheckReferences, &ReferenceCount);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("widget_name"), WidgetName);
	Data->SetStringField(TEXT("asset_path"), AssetPath);
	Data->SetNumberField(TEXT("reference_count"), ReferenceCount);
	Data->SetBoolField(TEXT("references_checked"), CheckReferences);
	
	if (DeleteResult.IsError())
	{
		Data->SetBoolField(TEXT("deletion_blocked"), true);
		return CreateErrorResponse(DeleteResult.GetErrorCode(), DeleteResult.GetErrorMessage());
	}

	Data->SetBoolField(TEXT("deletion_blocked"), false);
	Data->SetStringField(TEXT("message"), FString::Printf(
		TEXT("Widget Blueprint '%s' successfully deleted from project"), *WidgetName));

	if (ReferenceCount > 0)
	{
		Data->SetStringField(TEXT("warning"), FString::Printf(
			TEXT("Widget was referenced by %d other assets - those references may now be broken"), ReferenceCount));
	}
	
	return CreateSuccessResponse(Data);
}

// ============================================================================
// Discovery Commands
// ============================================================================

TSharedPtr<FJsonObject> FUMGCommands::HandleSearchItems(const TSharedPtr<FJsonObject>& Params)
{
	FString SearchTerm = Params->GetStringField(TEXT("search_term"));
	int32 MaxResults = Params->HasField(TEXT("max_results")) ? 
		Params->GetIntegerField(TEXT("max_results")) : 100;
	
	TResult<TArray<FAssetData>> Result = DiscoveryService->SearchWidgets(SearchTerm, MaxResults);
	
	if (Result.IsError())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}
	
	TArray<TSharedPtr<FJsonValue>> ItemsArray;
	for (const FAssetData& AssetData : Result.GetValue())
	{
		TSharedPtr<FJsonObject> ItemObj = MakeShared<FJsonObject>();
		ItemObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		ItemObj->SetStringField(TEXT("path"), AssetData.ObjectPath.ToString());
		ItemObj->SetStringField(TEXT("type"), TEXT("WidgetBlueprint"));
		ItemsArray.Add(MakeShared<FJsonValueObject>(ItemObj));
	}
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("items"), ItemsArray);
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleGetWidgetBlueprintInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing 'widget_name' parameter"));
	}
	
	TResult<UWidgetBlueprint*> WidgetResult = DiscoveryService->FindWidget(WidgetName);
	if (WidgetResult.IsError())
	{
		return CreateErrorResponse(WidgetResult.GetErrorCode(), WidgetResult.GetErrorMessage());
	}
	
	UWidgetBlueprint* Widget = WidgetResult.GetValue();
	TResult<FWidgetInfo> InfoResult = LifecycleService->GetWidgetInfo(Widget);
	
	if (InfoResult.IsError())
	{
		return CreateErrorResponse(InfoResult.GetErrorCode(), InfoResult.GetErrorMessage());
	}
	
	const FWidgetInfo& Info = InfoResult.GetValue();
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Info.Name);
	Data->SetStringField(TEXT("path"), Info.Path);
	Data->SetStringField(TEXT("parent_class"), Info.ParentClass);
	Data->SetStringField(TEXT("widget_type"), Info.WidgetType);
	
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleListWidgetComponents(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing 'widget_name' parameter"));
	}
	
	TResult<UWidgetBlueprint*> WidgetResult = DiscoveryService->FindWidget(WidgetName);
	if (WidgetResult.IsError())
	{
		return CreateErrorResponse(WidgetResult.GetErrorCode(), WidgetResult.GetErrorMessage());
	}
	
	TResult<TArray<FWidgetComponentInfo>> ComponentsResult = 
		ComponentService->ListComponents(WidgetResult.GetValue());
	
	if (ComponentsResult.IsError())
	{
		return CreateErrorResponse(ComponentsResult.GetErrorCode(), ComponentsResult.GetErrorMessage());
	}
	
	TArray<TSharedPtr<FJsonValue>> ComponentsArray;
	for (const FWidgetComponentInfo& CompInfo : ComponentsResult.GetValue())
	{
		ComponentsArray.Add(MakeShared<FJsonValueObject>(ComponentInfoToJson(CompInfo)));
	}
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("components"), ComponentsArray);
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleGetWidgetComponentProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName, ComponentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), 
			TEXT("Missing 'widget_name' or 'component_name' parameter"));
	}
	
	TResult<UWidgetBlueprint*> WidgetResult = DiscoveryService->FindWidget(WidgetName);
	if (WidgetResult.IsError())
	{
		return CreateErrorResponse(WidgetResult.GetErrorCode(), WidgetResult.GetErrorMessage());
	}
	
	TResult<TArray<FPropertyInfo>> PropsResult = 
		PropertyService->ListProperties(WidgetResult.GetValue(), ComponentName);
	
	if (PropsResult.IsError())
	{
		return CreateErrorResponse(PropsResult.GetErrorCode(), PropsResult.GetErrorMessage());
	}
	
	TArray<TSharedPtr<FJsonValue>> PropertiesArray;
	for (const FPropertyInfo& PropInfo : PropsResult.GetValue())
	{
		PropertiesArray.Add(MakeShared<FJsonValueObject>(PropertyInfoToJson(PropInfo)));
	}
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("properties"), PropertiesArray);
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleGetAvailableWidgetTypes(const TSharedPtr<FJsonObject>& Params)
{
	TResult<TArray<FString>> TypesResult = ReflectionService->GetAvailableWidgetTypes();
	
	if (TypesResult.IsError())
	{
		return CreateErrorResponse(TypesResult.GetErrorCode(), TypesResult.GetErrorMessage());
	}
	
	TArray<TSharedPtr<FJsonValue>> TypesArray;
	for (const FString& Type : TypesResult.GetValue())
	{
		TypesArray.Add(MakeShared<FJsonValueString>(Type));
	}
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("widget_types"), TypesArray);
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleValidateWidgetHierarchy(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing 'widget_name' parameter"));
	}
	
	TResult<UWidgetBlueprint*> WidgetResult = DiscoveryService->FindWidget(WidgetName);
	if (WidgetResult.IsError())
	{
		return CreateErrorResponse(WidgetResult.GetErrorCode(), WidgetResult.GetErrorMessage());
	}
	
	TResult<TArray<FString>> ValidationResult = LifecycleService->ValidateWidget(WidgetResult.GetValue());
	
	if (ValidationResult.IsError())
	{
		return CreateErrorResponse(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}
	
	TArray<TSharedPtr<FJsonValue>> ErrorsArray;
	for (const FString& Error : ValidationResult.GetValue())
	{
		ErrorsArray.Add(MakeShared<FJsonValueString>(Error));
	}
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("is_valid"), ErrorsArray.Num() == 0);
	Data->SetArrayField(TEXT("errors"), ErrorsArray);
	return CreateSuccessResponse(Data);
}

// ============================================================================
// Component Add Commands (Delegated to ComponentService)
// ============================================================================

TSharedPtr<FJsonObject> FUMGCommands::HandleAddTextBlockToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return HandleAddComponentGeneric(Params, TEXT("TextBlock"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddButtonToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return HandleAddComponentGeneric(Params, TEXT("Button"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddEditableText(const TSharedPtr<FJsonObject>& Params)
{
	return HandleAddComponentGeneric(Params, TEXT("EditableText"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddEditableTextBox(const TSharedPtr<FJsonObject>& Params)
{
	return HandleAddComponentGeneric(Params, TEXT("EditableTextBox"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddRichTextBlock(const TSharedPtr<FJsonObject>& Params)
{
	return HandleAddComponentGeneric(Params, TEXT("RichTextBlock"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddCheckBox(const TSharedPtr<FJsonObject>& Params)
{
	return HandleAddComponentGeneric(Params, TEXT("CheckBox"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddSlider(const TSharedPtr<FJsonObject>& Params)
{
	return HandleAddComponentGeneric(Params, TEXT("Slider"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddProgressBar(const TSharedPtr<FJsonObject>& Params)
{
	return HandleAddComponentGeneric(Params, TEXT("ProgressBar"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddImage(const TSharedPtr<FJsonObject>& Params)
{
	return HandleAddComponentGeneric(Params, TEXT("Image"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddSpacer(const TSharedPtr<FJsonObject>& Params)
{
	return HandleAddComponentGeneric(Params, TEXT("Spacer"));
}

// ============================================================================
// Layout Component Commands
// ============================================================================

TSharedPtr<FJsonObject> FUMGCommands::HandleAddCanvasPanel(const TSharedPtr<FJsonObject>& Params)
{
	return HandleAddComponentGeneric(Params, TEXT("CanvasPanel"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddSizeBox(const TSharedPtr<FJsonObject>& Params)
{
	return HandleAddComponentGeneric(Params, TEXT("SizeBox"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddOverlay(const TSharedPtr<FJsonObject>& Params)
{
	return HandleAddComponentGeneric(Params, TEXT("Overlay"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddHorizontalBox(const TSharedPtr<FJsonObject>& Params)
{
	return HandleAddComponentGeneric(Params, TEXT("HorizontalBox"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddVerticalBox(const TSharedPtr<FJsonObject>& Params)
{
	return HandleAddComponentGeneric(Params, TEXT("VerticalBox"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddScrollBox(const TSharedPtr<FJsonObject>& Params)
{
	return HandleAddComponentGeneric(Params, TEXT("ScrollBox"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddGridPanel(const TSharedPtr<FJsonObject>& Params)
{
	return HandleAddComponentGeneric(Params, TEXT("GridPanel"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddWidgetSwitcher(const TSharedPtr<FJsonObject>& Params)
{
	return HandleAddComponentGeneric(Params, TEXT("WidgetSwitcher"));
}


TSharedPtr<FJsonObject> FUMGCommands::HandleAddWidgetSwitcherSlot(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName, SwitcherName, ChildWidgetName;
	int32 SlotIndex = -1;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName) ||
		!Params->TryGetStringField(TEXT("switcher_name"), SwitcherName) ||
		!Params->TryGetStringField(TEXT("child_widget_name"), ChildWidgetName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing required parameters"));
	}
	
	Params->TryGetNumberField(TEXT("slot_index"), SlotIndex);
	
	TResult<UWidgetBlueprint*> WidgetResult = DiscoveryService->FindWidget(WidgetBlueprintName);
	if (WidgetResult.IsError())
	{
		return CreateErrorResponse(WidgetResult.GetErrorCode(), WidgetResult.GetErrorMessage());
	}
	
	int32 ActualSlotIndex = 0;
	TResult<void> AddSlotResult = ComponentService->AddWidgetSwitcherSlot(
		WidgetResult.GetValue(), SwitcherName, ChildWidgetName, SlotIndex, &ActualSlotIndex);
	
	if (AddSlotResult.IsError())
	{
		return CreateErrorResponse(AddSlotResult.GetErrorCode(), AddSlotResult.GetErrorMessage());
	}
	
	// Get the switcher to report total slots
	UWidgetBlueprint* WidgetBlueprint = WidgetResult.GetValue();
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	int32 TotalSlots = 0;
	
	if (WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		WidgetTree->GetAllWidgets(AllWidgets);
		
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && Widget->GetName() == SwitcherName && Widget->IsA<UWidgetSwitcher>())
			{
				UWidgetSwitcher* WidgetSwitcher = Cast<UWidgetSwitcher>(Widget);
				TotalSlots = WidgetSwitcher->GetNumWidgets();
				break;
			}
		}
	}
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Data->SetStringField(TEXT("switcher_name"), SwitcherName);
	Data->SetStringField(TEXT("child_widget_name"), ChildWidgetName);
	Data->SetNumberField(TEXT("slot_index"), ActualSlotIndex);
	Data->SetNumberField(TEXT("total_slots"), TotalSlots);
	
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddChildToPanel(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName, ChildName, ParentName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName) ||
		!Params->TryGetStringField(TEXT("child_name"), ChildName) ||
		!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing required parameters"));
	}
	
	TResult<UWidgetBlueprint*> WidgetResult = DiscoveryService->FindWidget(BlueprintName);
	if (WidgetResult.IsError())
	{
		return CreateErrorResponse(WidgetResult.GetErrorCode(), WidgetResult.GetErrorMessage());
	}
	
	TResult<void> SetParentResult = ComponentService->SetParent(
		WidgetResult.GetValue(), ChildName, ParentName);
	
	if (SetParentResult.IsError())
	{
		return CreateErrorResponse(SetParentResult.GetErrorCode(), SetParentResult.GetErrorMessage());
	}
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Child added to panel successfully"));
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleRemoveUMGComponent(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName, ComponentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing required parameters"));
	}
	
	bool bRemoveChildren = Params->HasField(TEXT("remove_children")) ?
		Params->GetBoolField(TEXT("remove_children")) : true;
	
	TResult<UWidgetBlueprint*> WidgetResult = DiscoveryService->FindWidget(WidgetName);
	if (WidgetResult.IsError())
	{
		return CreateErrorResponse(WidgetResult.GetErrorCode(), WidgetResult.GetErrorMessage());
	}
	
	TResult<void> RemoveResult = ComponentService->RemoveComponent(
		WidgetResult.GetValue(), ComponentName, bRemoveChildren);
	
	if (RemoveResult.IsError())
	{
		return CreateErrorResponse(RemoveResult.GetErrorCode(), RemoveResult.GetErrorMessage());
	}
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Component removed successfully"));
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleSetWidgetSlotProperties(const TSharedPtr<FJsonObject>& Params)
{
	// NOTE: Slot property setting requires complex slot-specific logic
	// For now, use PropertyService with "Slot." prefix
	FString WidgetName, ComponentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing required parameters"));
	}
	
	const TSharedPtr<FJsonObject>* SlotPropsObj;
	if (!Params->TryGetObjectField(TEXT("slot_properties"), SlotPropsObj))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing 'slot_properties' parameter"));
	}
	
	TResult<UWidgetBlueprint*> WidgetResult = DiscoveryService->FindWidget(WidgetName);
	if (WidgetResult.IsError())
	{
		return CreateErrorResponse(WidgetResult.GetErrorCode(), WidgetResult.GetErrorMessage());
	}
	
	// Set each slot property using PropertyService
	for (const auto& Pair : (*SlotPropsObj)->Values)
	{
		FString PropertyPath = FString::Printf(TEXT("Slot.%s"), *Pair.Key);
		FString Value;
		if (Pair.Value->Type == EJson::String)
		{
			Value = Pair.Value->AsString();
		}
		else if (Pair.Value->Type == EJson::Number)
		{
			Value = FString::SanitizeFloat(Pair.Value->AsNumber());
		}
		else
		{
			// Convert to JSON string for complex types
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Value);
			FJsonSerializer::Serialize(Pair.Value, TEXT(""), Writer);
		}
		
		TResult<void> SetResult = PropertyService->SetProperty(
			WidgetResult.GetValue(), ComponentName, PropertyPath, Value);
		
		if (SetResult.IsError())
		{
			return CreateErrorResponse(SetResult.GetErrorCode(), SetResult.GetErrorMessage());
		}
	}
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Slot properties set successfully"));
	return CreateSuccessResponse(Data);
}

// ============================================================================
// Property Commands
// ============================================================================

TSharedPtr<FJsonObject> FUMGCommands::HandleSetWidgetProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName, ComponentName, PropertyName, Value;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("component_name"), ComponentName) ||
		!Params->TryGetStringField(TEXT("property_name"), PropertyName) ||
		!Params->TryGetStringField(TEXT("value"), Value))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing required parameters"));
	}
	
	TResult<UWidgetBlueprint*> WidgetResult = DiscoveryService->FindWidget(WidgetName);
	if (WidgetResult.IsError())
	{
		return CreateErrorResponse(WidgetResult.GetErrorCode(), WidgetResult.GetErrorMessage());
	}
	
	TResult<void> SetResult = PropertyService->SetProperty(
		WidgetResult.GetValue(), ComponentName, PropertyName, Value);
	
	if (SetResult.IsError())
	{
		return CreateErrorResponse(SetResult.GetErrorCode(), SetResult.GetErrorMessage());
	}
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Property set successfully"));
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleGetWidgetProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName, ComponentName, PropertyName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("component_name"), ComponentName) ||
		!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing required parameters"));
	}
	
	TResult<UWidgetBlueprint*> WidgetResult = DiscoveryService->FindWidget(WidgetName);
	if (WidgetResult.IsError())
	{
		return CreateErrorResponse(WidgetResult.GetErrorCode(), WidgetResult.GetErrorMessage());
	}
	
	TResult<FString> GetResult = PropertyService->GetProperty(
		WidgetResult.GetValue(), ComponentName, PropertyName);
	
	if (GetResult.IsError())
	{
		return CreateErrorResponse(GetResult.GetErrorCode(), GetResult.GetErrorMessage());
	}
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("value"), GetResult.GetValue());
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleListWidgetProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName, ComponentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing required parameters"));
	}
	
	TResult<UWidgetBlueprint*> WidgetResult = DiscoveryService->FindWidget(WidgetName);
	if (WidgetResult.IsError())
	{
		return CreateErrorResponse(WidgetResult.GetErrorCode(), WidgetResult.GetErrorMessage());
	}
	
	TResult<TArray<FPropertyInfo>> PropsResult = 
		PropertyService->ListProperties(WidgetResult.GetValue(), ComponentName);
	
	if (PropsResult.IsError())
	{
		return CreateErrorResponse(PropsResult.GetErrorCode(), PropsResult.GetErrorMessage());
	}
	
	TArray<TSharedPtr<FJsonValue>> PropertiesArray;
	for (const FPropertyInfo& PropInfo : PropsResult.GetValue())
	{
		PropertiesArray.Add(MakeShared<FJsonValueObject>(PropertyInfoToJson(PropInfo)));
	}
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("properties"), PropertiesArray);
	return CreateSuccessResponse(Data);
}

// ============================================================================
// Event Commands
// ============================================================================

TSharedPtr<FJsonObject> FUMGCommands::HandleBindInputEvents(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName, ComponentName, EventName, FunctionName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("component_name"), ComponentName) ||
		!Params->TryGetStringField(TEXT("event_name"), EventName) ||
		!Params->TryGetStringField(TEXT("function_name"), FunctionName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing required parameters"));
	}
	
	TResult<UWidgetBlueprint*> WidgetResult = DiscoveryService->FindWidget(WidgetName);
	if (WidgetResult.IsError())
	{
		return CreateErrorResponse(WidgetResult.GetErrorCode(), WidgetResult.GetErrorMessage());
	}
	
	TResult<void> BindResult = EventService->BindEvent(
		WidgetResult.GetValue(), ComponentName, EventName, FunctionName);
	
	if (BindResult.IsError())
	{
		return CreateErrorResponse(BindResult.GetErrorCode(), BindResult.GetErrorMessage());
	}
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Event bound successfully"));
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleGetAvailableEvents(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName, ComponentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing required parameters"));
	}
	
	TResult<UWidgetBlueprint*> WidgetResult = DiscoveryService->FindWidget(WidgetName);
	if (WidgetResult.IsError())
	{
		return CreateErrorResponse(WidgetResult.GetErrorCode(), WidgetResult.GetErrorMessage());
	}
	
	TResult<TArray<FString>> EventsResult = 
		EventService->GetAvailableEvents(WidgetResult.GetValue(), ComponentName);
	
	if (EventsResult.IsError())
	{
		return CreateErrorResponse(EventsResult.GetErrorCode(), EventsResult.GetErrorMessage());
	}
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("events"), StringArrayToJson(EventsResult.GetValue()));
	return CreateSuccessResponse(Data);
}
