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

TSharedPtr<FJsonObject> FUMGCommands::CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	if (Data.IsValid())
	{
		for (const auto& Pair : Data->Values)
			Response->SetField(Pair.Key, Pair.Value);
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
		JsonArray.Add(MakeShared<FJsonValueString>(Str));
	return JsonArray;
}

// Helper macros for parameter validation
#define VALIDATE_PARAM(Params, Field, Variable) \
	if (!Params->TryGetStringField(TEXT(Field), Variable)) \
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing '" Field "' parameter"));

#define VALIDATE_PARAMS_2(Params, F1, V1, F2, V2) \
	if (!Params->TryGetStringField(TEXT(F1), V1) || !Params->TryGetStringField(TEXT(F2), V2)) \
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing required parameters"));

#define VALIDATE_PARAMS_3(Params, F1, V1, F2, V2, F3, V3) \
	if (!Params->TryGetStringField(TEXT(F1), V1) || !Params->TryGetStringField(TEXT(F2), V2) || !Params->TryGetStringField(TEXT(F3), V3)) \
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing required parameters"));

#define VALIDATE_PARAMS_4(Params, F1, V1, F2, V2, F3, V3, F4, V4) \
	if (!Params->TryGetStringField(TEXT(F1), V1) || !Params->TryGetStringField(TEXT(F2), V2) || !Params->TryGetStringField(TEXT(F3), V3) || !Params->TryGetStringField(TEXT(F4), V4)) \
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing required parameters"));

#define FIND_WIDGET_OR_ERROR(WidgetName, WidgetVar) \
	UWidgetBlueprint* WidgetVar = nullptr; \
	if (auto ErrorResponse = FindWidgetOrError(WidgetName, WidgetVar)) return ErrorResponse;

// Helper to find widget and return error response if not found
TSharedPtr<FJsonObject> FUMGCommands::FindWidgetOrError(const FString& WidgetName, UWidgetBlueprint*& OutWidget)
{
	TResult<UWidgetBlueprint*> Result = DiscoveryService->FindWidget(WidgetName);
	if (Result.IsError())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}
	OutWidget = Result.GetValue();
	return nullptr;
}

// Helper to handle generic component addition (used by all add_* handlers)
TSharedPtr<FJsonObject> FUMGCommands::HandleAddComponentGeneric(
	const TSharedPtr<FJsonObject>& Params,
	const FString& ComponentType)
{
	FString BlueprintName, WidgetName, ParentName;
	VALIDATE_PARAMS_2(Params, "blueprint_name", BlueprintName, "widget_name", WidgetName)
	Params->TryGetStringField(TEXT("parent_name"), ParentName);
	FIND_WIDGET_OR_ERROR(BlueprintName, Widget)
	TResult<UWidget*> ComponentResult = ComponentService->AddComponent(Widget, ComponentType, WidgetName, ParentName);
	if (ComponentResult.IsError())
		return CreateErrorResponse(ComponentResult.GetErrorCode(), ComponentResult.GetErrorMessage());
	return CreateSuccessResponse(ComponentToJson(ComponentResult.GetValue()));
}

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
		Obj->SetStringField(TEXT("min_value"), Info.MinValue);
	if (!Info.MaxValue.IsEmpty())
		Obj->SetStringField(TEXT("max_value"), Info.MaxValue);
	if (Info.EnumValues.Num() > 0)
		Obj->SetArrayField(TEXT("enum_values"), StringArrayToJson(Info.EnumValues));
	return Obj;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleCommand(const FString& CommandName, const TSharedPtr<FJsonObject>& Params)
{
	static const TMap<FString, FString> ComponentTypeMap = {
		{TEXT("add_text_block_to_widget"), TEXT("TextBlock")}, {TEXT("add_button_to_widget"), TEXT("Button")},
		{TEXT("add_editable_text"), TEXT("EditableText")}, {TEXT("add_editable_text_box"), TEXT("EditableTextBox")},
		{TEXT("add_rich_text_block"), TEXT("RichTextBlock")}, {TEXT("add_check_box"), TEXT("CheckBox")},
		{TEXT("add_slider"), TEXT("Slider")}, {TEXT("add_progress_bar"), TEXT("ProgressBar")},
		{TEXT("add_image"), TEXT("Image")}, {TEXT("add_spacer"), TEXT("Spacer")},
		{TEXT("add_canvas_panel"), TEXT("CanvasPanel")}, {TEXT("add_size_box"), TEXT("SizeBox")},
		{TEXT("add_overlay"), TEXT("Overlay")}, {TEXT("add_horizontal_box"), TEXT("HorizontalBox")},
		{TEXT("add_vertical_box"), TEXT("VerticalBox")}, {TEXT("add_scroll_box"), TEXT("ScrollBox")},
		{TEXT("add_grid_panel"), TEXT("GridPanel")}, {TEXT("add_widget_switcher"), TEXT("WidgetSwitcher")}
	};
	
	if (const FString* ComponentType = ComponentTypeMap.Find(CommandName))
		return HandleAddComponentGeneric(Params, *ComponentType);
	
	if (CommandName == TEXT("create_umg_widget_blueprint")) return HandleCreateUMGWidgetBlueprint(Params);
	if (CommandName == TEXT("delete_widget_blueprint")) return HandleDeleteWidgetBlueprint(Params);
	if (CommandName == TEXT("search_items")) return HandleSearchItems(Params);
	if (CommandName == TEXT("get_widget_blueprint_info")) return HandleGetWidgetBlueprintInfo(Params);
	if (CommandName == TEXT("list_widget_components")) return HandleListWidgetComponents(Params);
	if (CommandName == TEXT("get_widget_component_properties")) return HandleGetWidgetComponentProperties(Params);
	if (CommandName == TEXT("get_available_widget_types")) return HandleGetAvailableWidgetTypes(Params);
	if (CommandName == TEXT("validate_widget_hierarchy")) return HandleValidateWidgetHierarchy(Params);
	if (CommandName == TEXT("add_widget_switcher_slot")) return HandleAddWidgetSwitcherSlot(Params);
	if (CommandName == TEXT("add_child_to_panel")) return HandleAddChildToPanel(Params);
	if (CommandName == TEXT("remove_umg_component")) return HandleRemoveUMGComponent(Params);
	if (CommandName == TEXT("set_widget_slot_properties")) return HandleSetWidgetSlotProperties(Params);
	if (CommandName == TEXT("set_widget_property")) return HandleSetWidgetProperty(Params);
	if (CommandName == TEXT("get_widget_property")) return HandleGetWidgetProperty(Params);
	if (CommandName == TEXT("list_widget_properties")) return HandleListWidgetProperties(Params);
	if (CommandName == TEXT("bind_input_events")) return HandleBindInputEvents(Params);
	if (CommandName == TEXT("get_available_events")) return HandleGetAvailableEvents(Params);
	return CreateErrorResponse(TEXT("UNKNOWN_COMMAND"), 
		FString::Printf(TEXT("Unknown UMG command: %s"), *CommandName));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleCreateUMGWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing 'name' parameter"));
	FString PackagePath = TEXT("/Game/UI/");
	Params->TryGetStringField(TEXT("path"), PackagePath);
	TResult<TPair<UWidgetBlueprint*, FWidgetInfo>> Result = LifecycleService->CreateWidget(BlueprintName, PackagePath);
	if (Result.IsError())
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	const FWidgetInfo& Info = Result.GetValue().Value;
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Info.Name);
	Data->SetStringField(TEXT("path"), Info.Path);
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleDeleteWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName;
	VALIDATE_PARAM(Params, "widget_name", WidgetName)
	bool CheckReferences = Params->HasField(TEXT("check_references")) ? Params->GetBoolField(TEXT("check_references")) : true;
	FIND_WIDGET_OR_ERROR(WidgetName, Widget)
	FString AssetPath = Widget->GetPathName();
	int32 ReferenceCount = 0;
	TResult<void> DeleteResult = LifecycleService->DeleteWidget(Widget, CheckReferences, &ReferenceCount);
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
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Widget Blueprint '%s' successfully deleted from project"), *WidgetName));
	if (ReferenceCount > 0)
		Data->SetStringField(TEXT("warning"), FString::Printf(TEXT("Widget was referenced by %d other assets - those references may now be broken"), ReferenceCount));
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleSearchItems(const TSharedPtr<FJsonObject>& Params)
{
	FString SearchTerm = Params->GetStringField(TEXT("search_term"));
	int32 MaxResults = Params->HasField(TEXT("max_results")) ? Params->GetIntegerField(TEXT("max_results")) : 100;
	TResult<TArray<FAssetData>> Result = DiscoveryService->SearchWidgets(SearchTerm, MaxResults);
	if (Result.IsError())
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
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
	VALIDATE_PARAM(Params, "widget_name", WidgetName)
	FIND_WIDGET_OR_ERROR(WidgetName, Widget)
	
	TResult<FWidgetInfo> InfoResult = LifecycleService->GetWidgetInfo(Widget);
	if (InfoResult.IsError())
		return CreateErrorResponse(InfoResult.GetErrorCode(), InfoResult.GetErrorMessage());
	
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
	VALIDATE_PARAM(Params, "widget_name", WidgetName)
	FIND_WIDGET_OR_ERROR(WidgetName, Widget)
	
	TResult<TArray<FWidgetComponentInfo>> ComponentsResult = ComponentService->ListComponents(Widget);
	if (ComponentsResult.IsError())
		return CreateErrorResponse(ComponentsResult.GetErrorCode(), ComponentsResult.GetErrorMessage());
	
	TArray<TSharedPtr<FJsonValue>> ComponentsArray;
	for (const FWidgetComponentInfo& CompInfo : ComponentsResult.GetValue())
		ComponentsArray.Add(MakeShared<FJsonValueObject>(ComponentInfoToJson(CompInfo)));
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("components"), ComponentsArray);
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleGetWidgetComponentProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName, ComponentName;
	VALIDATE_PARAMS_2(Params, "widget_name", WidgetName, "component_name", ComponentName)
	FIND_WIDGET_OR_ERROR(WidgetName, Widget)
	
	TResult<TArray<FPropertyInfo>> PropsResult = PropertyService->ListProperties(Widget, ComponentName);
	if (PropsResult.IsError())
		return CreateErrorResponse(PropsResult.GetErrorCode(), PropsResult.GetErrorMessage());
	
	TArray<TSharedPtr<FJsonValue>> PropertiesArray;
	for (const FPropertyInfo& PropInfo : PropsResult.GetValue())
		PropertiesArray.Add(MakeShared<FJsonValueObject>(PropertyInfoToJson(PropInfo)));
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("properties"), PropertiesArray);
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleGetAvailableWidgetTypes(const TSharedPtr<FJsonObject>& Params)
{
	TResult<TArray<FString>> TypesResult = ReflectionService->GetAvailableWidgetTypes();
	if (TypesResult.IsError())
		return CreateErrorResponse(TypesResult.GetErrorCode(), TypesResult.GetErrorMessage());
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("widget_types"), StringArrayToJson(TypesResult.GetValue()));
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleValidateWidgetHierarchy(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName;
	VALIDATE_PARAM(Params, "widget_name", WidgetName)
	FIND_WIDGET_OR_ERROR(WidgetName, Widget)
	
	TResult<TArray<FString>> ValidationResult = LifecycleService->ValidateWidget(Widget);
	if (ValidationResult.IsError())
		return CreateErrorResponse(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	
	TArray<TSharedPtr<FJsonValue>> ErrorsArray = StringArrayToJson(ValidationResult.GetValue());
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("is_valid"), ErrorsArray.Num() == 0);
	Data->SetArrayField(TEXT("errors"), ErrorsArray);
	return CreateSuccessResponse(Data);
}

// All component addition now handled by HandleAddComponentGeneric via command router

TSharedPtr<FJsonObject> FUMGCommands::HandleAddWidgetSwitcherSlot(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName, SwitcherName, ChildWidgetName;
	VALIDATE_PARAMS_3(Params, "widget_name", WidgetBlueprintName, "switcher_name", SwitcherName, "child_widget_name", ChildWidgetName)
	int32 SlotIndex = -1;
	Params->TryGetNumberField(TEXT("slot_index"), SlotIndex);
	FIND_WIDGET_OR_ERROR(WidgetBlueprintName, Widget)
	int32 ActualSlotIndex = 0;
	TResult<void> AddSlotResult = ComponentService->AddWidgetSwitcherSlot(Widget, SwitcherName, ChildWidgetName, SlotIndex, &ActualSlotIndex);
	if (AddSlotResult.IsError())
		return CreateErrorResponse(AddSlotResult.GetErrorCode(), AddSlotResult.GetErrorMessage());
	int32 TotalSlots = 0;
	if (Widget->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		Widget->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* W : AllWidgets)
			if (W && W->GetName() == SwitcherName && W->IsA<UWidgetSwitcher>())
			{
				TotalSlots = Cast<UWidgetSwitcher>(W)->GetNumWidgets();
				break;
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
	VALIDATE_PARAMS_3(Params, "blueprint_name", BlueprintName, "child_name", ChildName, "parent_name", ParentName)
	FIND_WIDGET_OR_ERROR(BlueprintName, Widget)
	
	TResult<void> SetParentResult = ComponentService->SetParent(Widget, ChildName, ParentName);
	if (SetParentResult.IsError())
		return CreateErrorResponse(SetParentResult.GetErrorCode(), SetParentResult.GetErrorMessage());
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Child added to panel successfully"));
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleRemoveUMGComponent(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName, ComponentName;
	VALIDATE_PARAMS_2(Params, "widget_name", WidgetName, "component_name", ComponentName)
	
	bool bRemoveChildren = Params->HasField(TEXT("remove_children")) ?
		Params->GetBoolField(TEXT("remove_children")) : true;
	
	FIND_WIDGET_OR_ERROR(WidgetName, Widget)
	
	TResult<void> RemoveResult = ComponentService->RemoveComponent(Widget, ComponentName, bRemoveChildren);
	if (RemoveResult.IsError())
		return CreateErrorResponse(RemoveResult.GetErrorCode(), RemoveResult.GetErrorMessage());
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Component removed successfully"));
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleSetWidgetSlotProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName, ComponentName;
	VALIDATE_PARAMS_2(Params, "widget_name", WidgetName, "component_name", ComponentName)
	const TSharedPtr<FJsonObject>* SlotPropsObj;
	if (!Params->TryGetObjectField(TEXT("slot_properties"), SlotPropsObj))
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing 'slot_properties' parameter"));
	FIND_WIDGET_OR_ERROR(WidgetName, Widget)
	for (const auto& Pair : (*SlotPropsObj)->Values)
	{
		FString PropertyPath = FString::Printf(TEXT("Slot.%s"), *Pair.Key);
		FString Value;
		if (Pair.Value->Type == EJson::String)
			Value = Pair.Value->AsString();
		else if (Pair.Value->Type == EJson::Number)
			Value = FString::SanitizeFloat(Pair.Value->AsNumber());
		else
		{
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Value);
			FJsonSerializer::Serialize(Pair.Value, TEXT(""), Writer);
		}
		TResult<void> SetResult = PropertyService->SetProperty(Widget, ComponentName, PropertyPath, Value);
		if (SetResult.IsError())
			return CreateErrorResponse(SetResult.GetErrorCode(), SetResult.GetErrorMessage());
	}
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Slot properties set successfully"));
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleSetWidgetProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName, ComponentName, PropertyName, Value;
	VALIDATE_PARAMS_4(Params, "widget_name", WidgetName, "component_name", ComponentName, "property_name", PropertyName, "value", Value)
	FIND_WIDGET_OR_ERROR(WidgetName, Widget)
	
	TResult<void> SetResult = PropertyService->SetProperty(Widget, ComponentName, PropertyName, Value);
	if (SetResult.IsError())
		return CreateErrorResponse(SetResult.GetErrorCode(), SetResult.GetErrorMessage());
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Property set successfully"));
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleGetWidgetProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName, ComponentName, PropertyName;
	VALIDATE_PARAMS_3(Params, "widget_name", WidgetName, "component_name", ComponentName, "property_name", PropertyName)
	FIND_WIDGET_OR_ERROR(WidgetName, Widget)
	
	TResult<FString> GetResult = PropertyService->GetProperty(Widget, ComponentName, PropertyName);
	if (GetResult.IsError())
		return CreateErrorResponse(GetResult.GetErrorCode(), GetResult.GetErrorMessage());
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("value"), GetResult.GetValue());
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleListWidgetProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName, ComponentName;
	VALIDATE_PARAMS_2(Params, "widget_name", WidgetName, "component_name", ComponentName)
	FIND_WIDGET_OR_ERROR(WidgetName, Widget)
	
	TResult<TArray<FPropertyInfo>> PropsResult = PropertyService->ListProperties(Widget, ComponentName);
	if (PropsResult.IsError())
		return CreateErrorResponse(PropsResult.GetErrorCode(), PropsResult.GetErrorMessage());
	
	TArray<TSharedPtr<FJsonValue>> PropertiesArray;
	for (const FPropertyInfo& PropInfo : PropsResult.GetValue())
		PropertiesArray.Add(MakeShared<FJsonValueObject>(PropertyInfoToJson(PropInfo)));
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("properties"), PropertiesArray);
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleBindInputEvents(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName, ComponentName, EventName, FunctionName;
	VALIDATE_PARAMS_4(Params, "widget_name", WidgetName, "component_name", ComponentName, "event_name", EventName, "function_name", FunctionName)
	FIND_WIDGET_OR_ERROR(WidgetName, Widget)
	
	TResult<void> BindResult = EventService->BindEvent(Widget, ComponentName, EventName, FunctionName);
	if (BindResult.IsError())
		return CreateErrorResponse(BindResult.GetErrorCode(), BindResult.GetErrorMessage());
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Event bound successfully"));
	return CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FUMGCommands::HandleGetAvailableEvents(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName, ComponentName;
	VALIDATE_PARAMS_2(Params, "widget_name", WidgetName, "component_name", ComponentName)
	FIND_WIDGET_OR_ERROR(WidgetName, Widget)
	
	TResult<TArray<FString>> EventsResult = EventService->GetAvailableEvents(Widget, ComponentName);
	if (EventsResult.IsError())
		return CreateErrorResponse(EventsResult.GetErrorCode(), EventsResult.GetErrorMessage());
	
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("events"), StringArrayToJson(EventsResult.GetValue()));
	return CreateSuccessResponse(Data);
}
