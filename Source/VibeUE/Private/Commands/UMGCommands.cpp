#include "Commands/UMGCommands.h"
#include "Commands/CommonUtils.h"
#include "Services/UMG/WidgetLifecycleService.h"
#include "Services/UMG/WidgetPropertyService.h"
#include "Services/UMG/WidgetComponentService.h"
#include "Services/UMG/WidgetHierarchyService.h"
#include "Services/UMG/WidgetBlueprintInfoService.h"
#include "Services/UMG/WidgetDiscoveryService.h"
#include "Services/UMG/WidgetEventService.h"
#include "Services/UMG/WidgetAssetService.h"
#include "Core/ServiceContext.h"
// TODO: Issue #188 skipped - discovery handlers already well-structured
#include "Editor.h"
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
#include "Components/SizeBox.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UObject/UnrealType.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "UObject/Class.h"
#include "UObject/TopLevelAssetPath.h"

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

FUMGCommands::FUMGCommands(TSharedPtr<FServiceContext> InServiceContext)
{
	ServiceContext = InServiceContext.IsValid() ? InServiceContext : MakeShared<FServiceContext>();

	// Initialize services using shared context
	LifecycleService = MakeShared<FWidgetLifecycleService>(ServiceContext);
	PropertyService = MakeShared<FWidgetPropertyService>(ServiceContext);
	ComponentService = MakeShared<FWidgetComponentService>(ServiceContext);
	HierarchyService = MakeShared<FWidgetHierarchyService>(ServiceContext);
	BlueprintInfoService = MakeShared<FWidgetBlueprintInfoService>(ServiceContext);
	DiscoveryService = MakeShared<FWidgetDiscoveryService>(ServiceContext);
	EventService = MakeShared<FWidgetEventService>(ServiceContext);
	AssetService = MakeShared<FWidgetAssetService>(ServiceContext);
	// TODO: Issue #188 skipped - discovery handlers already well-structured
}

// Static member definition
// Static variables for UMG commands

TSharedPtr<FJsonObject> FUMGCommands::HandleCommand(const FString& CommandName, const TSharedPtr<FJsonObject>& Params)
{
	// Original UMG Commands
	if (CommandName == TEXT("create_umg_widget_blueprint"))
	{
		return HandleCreateUMGWidgetBlueprint(Params);
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
	// UMG Hierarchy Commands
	else if (CommandName == TEXT("add_child_to_panel"))
	{
		return HandleAddChildToPanel(Params);
	}
	else if (CommandName == TEXT("remove_umg_component"))
	{
		return HandleRemoveUMGComponent(Params);
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
	// set_widget_transform/set_widget_visibility/set_widget_z_order removed
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

	return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown UMG command: %s"), *CommandName));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleCreateUMGWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	// 1. Extract and validate parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	// Get optional parameters
	FString PackagePath = TEXT("/Game/UI/");
	Params->TryGetStringField(TEXT("path"), PackagePath);
	
	FString ParentClass = TEXT("UserWidget");
	Params->TryGetStringField(TEXT("parent_class"), ParentClass);

	// 2. Call service method
	auto Result = LifecycleService->CreateWidgetBlueprint(BlueprintName, PackagePath, ParentClass);
	
	// 3. Handle result
	if (Result.IsError())
	{
		return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
	}

	UWidgetBlueprint* WidgetBlueprint = Result.GetValue();
	
	// 4. Create success response
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("name"), BlueprintName);
	ResultObj->SetStringField(TEXT("path"), WidgetBlueprint->GetPathName());
	return ResultObj;
}




// ===================================================================
// UMG Discovery Methods Implementation
// ===================================================================

TSharedPtr<FJsonObject> FUMGCommands::HandleSearchItems(const TSharedPtr<FJsonObject>& Params)
{
	FString SearchTerm;
	if (!Params->TryGetStringField(TEXT("search_term"), SearchTerm))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing 'search_term' parameter"));
	}

	FString AssetType;
	Params->TryGetStringField(TEXT("asset_type"), AssetType);

	FString Path = TEXT("/Game");
	Params->TryGetStringField(TEXT("path"), Path);

	bool bCaseSensitive = false;
	Params->TryGetBoolField(TEXT("case_sensitive"), bCaseSensitive);

	bool bIncludeEngineContent = false;
	Params->TryGetBoolField(TEXT("include_engine_content"), bIncludeEngineContent);

	int32 MaxResults = 100;
	double MaxResultsValue = 0.0;
	if (Params->TryGetNumberField(TEXT("max_results"), MaxResultsValue))
	{
		MaxResults = FMath::Max(0, static_cast<int32>(MaxResultsValue));
	}

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(*Path);
	if (bIncludeEngineContent)
	{
		Filter.PackagePaths.Add(TEXT("/Engine"));
	}
	if (!AssetType.IsEmpty())
	{
		FTopLevelAssetPath AssetClassPath = UClass::TryConvertShortTypeNameToPathName<UClass>(AssetType, ELogVerbosity::NoLogging);
		if (AssetClassPath.IsNull())
		{
			if (AssetType.Contains(TEXT("/")))
			{
				AssetClassPath = FTopLevelAssetPath(*AssetType);
			}
			else if (UClass* AssetClass = FindFirstObjectSafe<UClass>(*AssetType))
			{
				AssetClassPath = AssetClass->GetClassPathName();
			}
		}

		if (!AssetClassPath.IsNull())
		{
			Filter.ClassPaths.Add(AssetClassPath);
		}
	}

	// Use ServiceContext to get AssetRegistry instead of loading module directly
	IAssetRegistry* AssetRegistry = ServiceContext->GetAssetRegistry();
	if (!AssetRegistry)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Failed to get Asset Registry"));
	}

	// If discovery service is available and the requested asset type appears to be a widget
	const bool bLooksLikeWidgetSearch = AssetType.IsEmpty() || AssetType.Contains(TEXT("Widget"), ESearchCase::IgnoreCase) || AssetType.Contains(TEXT("WidgetBlueprint"), ESearchCase::IgnoreCase);
	if (DiscoveryService.IsValid() && bLooksLikeWidgetSearch)
	{
		// Delegate to WidgetDiscoveryService which provides richer widget blueprint metadata
		auto SearchResult = DiscoveryService->SearchWidgetBlueprints(SearchTerm, MaxResults);
		if (SearchResult.IsError())
		{
			return FCommonUtils::CreateErrorResponse(SearchResult.GetErrorMessage());
		}

		const TArray<FWidgetBlueprintInfo>& Found = SearchResult.GetValue();
		TArray<TSharedPtr<FJsonValue>> ItemArrayLocal;
		for (const FWidgetBlueprintInfo& Info : Found)
		{
			TSharedPtr<FJsonObject> ItemObj = MakeShared<FJsonObject>();
			ItemObj->SetStringField(TEXT("asset_name"), Info.Name);
			ItemObj->SetStringField(TEXT("object_path"), Info.Path);
			ItemObj->SetStringField(TEXT("package_name"), Info.PackagePath);
			ItemObj->SetStringField(TEXT("class_name"), TEXT("WidgetBlueprint"));
			ItemArrayLocal.Add(MakeShared<FJsonValueObject>(ItemObj));
		}

		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetBoolField(TEXT("success"), true);
		Response->SetArrayField(TEXT("items"), ItemArrayLocal);
		Response->SetNumberField(TEXT("count"), ItemArrayLocal.Num());

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
	
	TArray<FAssetData> Assets;
	AssetRegistry->GetAssets(Filter, Assets);

	TArray<TSharedPtr<FJsonValue>> ItemArray;
	const ESearchCase::Type SearchCase = bCaseSensitive ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase;

	for (const FAssetData& Asset : Assets)
	{
		const FString AssetName = Asset.AssetName.ToString();
		if (!SearchTerm.IsEmpty() && !AssetName.Contains(SearchTerm, SearchCase))
		{
			continue;
		}

		TSharedPtr<FJsonObject> ItemObj = MakeShared<FJsonObject>();
		ItemObj->SetStringField(TEXT("asset_name"), AssetName);
		ItemObj->SetStringField(TEXT("object_path"), Asset.GetObjectPathString());
		ItemObj->SetStringField(TEXT("package_name"), Asset.PackageName.ToString());
		ItemObj->SetStringField(TEXT("class_name"), Asset.AssetClassPath.ToString());

		ItemArray.Add(MakeShared<FJsonValueObject>(ItemObj));

		if (MaxResults > 0 && ItemArray.Num() >= MaxResults)
		{
			break;
		}
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetArrayField(TEXT("items"), ItemArray);
	Response->SetNumberField(TEXT("count"), ItemArray.Num());

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

TSharedPtr<FJsonObject> FUMGCommands::HandleSetWidgetProperty(const TSharedPtr<FJsonObject>& Params)
{
	if (!PropertyService.IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetPropertyService not available"));
	}

	FString WidgetBlueprintName;
	FString WidgetName;
	FString PropertyName;

	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}

	if (!Params->TryGetStringField(TEXT("component_name"), WidgetName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}

	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing property_name parameter"));
	}

	FWidgetPropertySetRequest Request;
	Request.PropertyPath = PropertyName;

	FString PropertyValueString;
	const bool bHasStringValue = Params->TryGetStringField(TEXT("property_value"), PropertyValueString);
	if (bHasStringValue)
	{
		Request.Value.SetString(PropertyValueString);
	}
	else
	{
		const TSharedPtr<FJsonValue>* PropertyValueField = Params->Values.Find(TEXT("property_value"));
		if (PropertyValueField && PropertyValueField->IsValid())
		{
			Request.Value.SetJson(*PropertyValueField);
		}
		else
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Missing property_value parameter"));
		}
	}

	FString CollectionOp;
	if (Params->TryGetStringField(TEXT("collection_op"), CollectionOp) && !CollectionOp.IsEmpty())
	{
		FWidgetCollectionOperation Operation;
		Operation.Operation = CollectionOp;
		double IndexValue = 0.0;
		if (Params->TryGetNumberField(TEXT("index"), IndexValue))
		{
			Operation.Index = static_cast<int32>(IndexValue);
		}
		Request.CollectionOperation = Operation;
	}

	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	const auto Result = PropertyService->SetWidgetProperty(WidgetBlueprint, WidgetName, Request);
	if (Result.IsError())
	{
		return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
	}

	const FWidgetPropertySetResult& Payload = Result.GetValue();
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Response->SetStringField(TEXT("component_name"), WidgetName);
	Response->SetStringField(TEXT("property_name"), PropertyName);

	if (Payload.bChildOrderUpdated)
	{
		Response->SetNumberField(TEXT("property_value"), Payload.ChildOrderValue);
	}
	else if (Payload.AppliedValue.HasJson())
	{
		Response->SetField(TEXT("property_value"), Payload.AppliedValue.JsonValue);
	}
	else if (Payload.AppliedValue.HasString())
	{
		Response->SetStringField(TEXT("property_value"), Payload.AppliedValue.StringValue);
	}
	else
	{
		Response->SetStringField(TEXT("property_value"), TEXT(""));
	}

	if (!Payload.CollectionOperation.IsEmpty())
	{
		Response->SetStringField(TEXT("collection_op"), Payload.CollectionOperation);
	}

	Response->SetStringField(TEXT("note"), Payload.Note.IsEmpty() ? TEXT("Property set successfully") : Payload.Note);

	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleGetWidgetBlueprintInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		Params->TryGetStringField(TEXT("widget_path"), WidgetName);
		if (WidgetName.IsEmpty())
		{
			Params->TryGetStringField(TEXT("object_path"), WidgetName);
		}
		if (WidgetName.IsEmpty())
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter (accepts name or full path)"));
		}
	}

	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint not found for '%s'"), *WidgetName));
	}

	if (!BlueprintInfoService.IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetBlueprintInfoService not available"));
	}

	const auto Result = BlueprintInfoService->GetWidgetBlueprintInfo(WidgetBlueprint);
	if (Result.IsError())
	{
		return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
	}

	const FWidgetBlueprintInfo& Info = Result.GetValue();
	TSharedPtr<FJsonObject> WidgetInfo = MakeShared<FJsonObject>();
	WidgetInfo->SetStringField(TEXT("name"), Info.Name);
	WidgetInfo->SetStringField(TEXT("path"), Info.Path);
	WidgetInfo->SetStringField(TEXT("package_path"), Info.PackagePath);
	WidgetInfo->SetStringField(TEXT("parent_class"), Info.ParentClass);
	WidgetInfo->SetStringField(TEXT("root_widget_name"), Info.RootWidget.IsEmpty() ? TEXT("") : Info.RootWidget);

	TArray<TSharedPtr<FJsonValue>> ComponentArray;
	for (const FWidgetInfo& W : Info.Components)
	{
		TSharedPtr<FJsonObject> ComponentObj = MakeShared<FJsonObject>();
		ComponentObj->SetStringField(TEXT("name"), W.Name);
		ComponentObj->SetStringField(TEXT("type"), W.Type);
		ComponentObj->SetBoolField(TEXT("is_variable"), W.bIsVariable);
		if (!W.ParentName.IsEmpty()) ComponentObj->SetStringField(TEXT("parent"), W.ParentName);
		if (W.Children.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ChildrenArray;
			for (const FString& C : W.Children)
			{
				ChildrenArray.Add(MakeShared<FJsonValueString>(C));
			}
			ComponentObj->SetArrayField(TEXT("children"), ChildrenArray);
		}
		ComponentArray.Add(MakeShared<FJsonValueObject>(ComponentObj));
	}
	WidgetInfo->SetArrayField(TEXT("components"), ComponentArray);
	WidgetInfo->SetNumberField(TEXT("component_count"), ComponentArray.Num());

	TArray<TSharedPtr<FJsonValue>> VariableArray;
	for (const FString& Var : Info.VariableNames)
	{
		VariableArray.Add(MakeShared<FJsonValueString>(Var));
	}
	WidgetInfo->SetArrayField(TEXT("variables"), VariableArray);
	WidgetInfo->SetNumberField(TEXT("variable_count"), VariableArray.Num());

	TArray<TSharedPtr<FJsonValue>> EventArray;
	for (const FString& E : Info.EventNames)
	{
		EventArray.Add(MakeShared<FJsonValueString>(E));
	}
	WidgetInfo->SetArrayField(TEXT("events"), EventArray);
	WidgetInfo->SetNumberField(TEXT("event_count"), EventArray.Num());

	TArray<TSharedPtr<FJsonValue>> AnimationArray;
	for (const FString& A : Info.AnimationNames)
	{
		AnimationArray.Add(MakeShared<FJsonValueString>(A));
	}
	WidgetInfo->SetArrayField(TEXT("animations"), AnimationArray);
	WidgetInfo->SetNumberField(TEXT("animation_count"), AnimationArray.Num());

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetObjectField(TEXT("widget_info"), WidgetInfo);
	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleListWidgetComponents(const TSharedPtr<FJsonObject>& Params)
{
	if (!HierarchyService.IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetHierarchyService not available"));
	}

	FString WidgetIdentifier;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetIdentifier))
	{
		Params->TryGetStringField(TEXT("widget_path"), WidgetIdentifier);
		if (WidgetIdentifier.IsEmpty())
		{
			Params->TryGetStringField(TEXT("object_path"), WidgetIdentifier);
		}
		if (WidgetIdentifier.IsEmpty())
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter (accepts name or full path)"));
		}
	}

	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetIdentifier);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint not found for '%s'"), *WidgetIdentifier));
	}

	const auto Result = HierarchyService->ListWidgetComponents(WidgetBlueprint);
	if (Result.IsError())
	{
		return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
	}

	const TArray<FWidgetInfo>& Components = Result.GetValue();
	TArray<TSharedPtr<FJsonValue>> ComponentArray;
	ComponentArray.Reserve(Components.Num());

	for (const FWidgetInfo& Info : Components)
	{
		TSharedPtr<FJsonObject> ComponentInfo = MakeShared<FJsonObject>();
		ComponentInfo->SetStringField(TEXT("name"), Info.Name);
		ComponentInfo->SetStringField(TEXT("type"), Info.Type);
		ComponentInfo->SetBoolField(TEXT("is_variable"), Info.bIsVariable);

		if (!Info.ParentName.IsEmpty())
		{
			ComponentInfo->SetStringField(TEXT("parent_name"), Info.ParentName);
		}

		if (Info.Children.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ChildrenArray;
			ChildrenArray.Reserve(Info.Children.Num());
			for (const FString& ChildName : Info.Children)
			{
				ChildrenArray.Add(MakeShared<FJsonValueString>(ChildName));
			}
			ComponentInfo->SetArrayField(TEXT("children"), ChildrenArray);
		}

		ComponentArray.Add(MakeShared<FJsonValueObject>(ComponentInfo));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetArrayField(TEXT("components"), ComponentArray);
	Response->SetStringField(TEXT("widget_path"), WidgetBlueprint->GetPathName());
	Response->SetNumberField(TEXT("count"), ComponentArray.Num());
	Response->SetStringField(TEXT("usage"), TEXT("Use 'widget_name' as name, package path, or full object path to target a widget blueprint."));
	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleGetWidgetComponentProperties(const TSharedPtr<FJsonObject>& Params)
{
	// Parse parameters
	FString WidgetName, ComponentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) || !Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' or 'component_name' parameter"));
	}

	if (WidgetName.IsEmpty())
	{
		Params->TryGetStringField(TEXT("widget_path"), WidgetName);
		if (WidgetName.IsEmpty())
		{
			Params->TryGetStringField(TEXT("object_path"), WidgetName);
		}
	}

	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint not found for '%s'"), *WidgetName));
	}

	if (!ComponentService.IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetComponentService not available"));
	}

	const auto Result = ComponentService->GetWidgetComponentInfo(WidgetBlueprint, ComponentName, true);
	if (Result.IsError())
	{
		return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
	}

	const FWidgetComponentInfo& Info = Result.GetValue();
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("component_name"), Info.Name);
	Response->SetStringField(TEXT("component_type"), Info.Type);
	Response->SetBoolField(TEXT("is_variable"), Info.bIsVariable);
	Response->SetBoolField(TEXT("is_enabled"), Info.bIsEnabled);
	Response->SetStringField(TEXT("visibility"), Info.Visibility);
	Response->SetStringField(TEXT("widget_path"), WidgetBlueprint->GetPathName());

	if (Info.SlotInfo.IsSet())
	{
		const FWidgetSlotInfo& S = Info.SlotInfo.GetValue();
		TSharedPtr<FJsonObject> SlotObj = MakeShared<FJsonObject>();
		SlotObj->SetStringField(TEXT("slot_type"), S.SlotType);
		for (const auto& Pair : S.Properties)
		{
			SlotObj->SetStringField(Pair.Key, Pair.Value);
		}
		Response->SetObjectField(TEXT("slot_info"), SlotObj);
	}

	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleGetAvailableWidgetTypes(const TSharedPtr<FJsonObject>& Params)
{
	// Prefer discovery service list for consistency
	if (DiscoveryService.IsValid())
	{
		auto Result = DiscoveryService->GetAvailableWidgetTypes();
		if (Result.IsError())
		{
			return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
		}

		const TArray<FString>& WidgetTypes = Result.GetValue();
		TArray<TSharedPtr<FJsonValue>> TypeArray;
		for (const FString& Type : WidgetTypes)
		{
			TypeArray.Add(MakeShared<FJsonValueString>(Type));
		}

		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetBoolField(TEXT("success"), true);
		Response->SetArrayField(TEXT("widget_types"), TypeArray);
		Response->SetNumberField(TEXT("count"), WidgetTypes.Num());
		return Response;
	}

	// Fallback: local list (kept for compatibility)
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    
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

TSharedPtr<FJsonObject> FUMGCommands::HandleValidateWidgetHierarchy(const TSharedPtr<FJsonObject>& Params)
{
	if (!HierarchyService.IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetHierarchyService not available"));
	}

	FString WidgetIdentifier;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetIdentifier))
	{
		Params->TryGetStringField(TEXT("widget_path"), WidgetIdentifier);
		if (WidgetIdentifier.IsEmpty())
		{
			Params->TryGetStringField(TEXT("object_path"), WidgetIdentifier);
		}
		if (WidgetIdentifier.IsEmpty())
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
		}
	}

	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetIdentifier);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetIdentifier));
	}

	const auto Result = HierarchyService->ValidateWidgetHierarchy(WidgetBlueprint);
	if (Result.IsError())
	{
		return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
	}

	const TArray<FString>& Errors = Result.GetValue();
	const bool bIsValid = Errors.Num() == 0;

	TArray<TSharedPtr<FJsonValue>> ErrorArray;
	ErrorArray.Reserve(Errors.Num());
	for (const FString& ErrorMessage : Errors)
	{
		ErrorArray.Add(MakeShared<FJsonValueString>(ErrorMessage));
	}

	FString ValidationMessage;
	if (bIsValid)
	{
		ValidationMessage = TEXT("Widget hierarchy is valid");
	}
	else if (Errors.Num() > 0)
	{
		ValidationMessage = Errors[0];
	}
	else
	{
		ValidationMessage = TEXT("Invalid widget hierarchy");
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetBoolField(TEXT("is_valid"), bIsValid);
	Response->SetStringField(TEXT("validation_message"), ValidationMessage);
	Response->SetStringField(TEXT("widget_path"), WidgetBlueprint->GetPathName());
	Response->SetNumberField(TEXT("error_count"), Errors.Num());
	if (Errors.Num() > 0)
	{
		Response->SetArrayField(TEXT("errors"), ErrorArray);
	}

	return Response;
}

// ===================================================================
// UMG Component Methods Implementation
// ===================================================================

TSharedPtr<FJsonObject> FUMGCommands::HandleAddChildToPanel(const TSharedPtr<FJsonObject>& Params)
{
	if (!ComponentService.IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetComponentService not available"));
	}

	FString WidgetBlueprintName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}

	FString ChildName;
	if (!Params->TryGetStringField(TEXT("child_name"), ChildName))
	{
		if (!Params->TryGetStringField(TEXT("component_name"), ChildName) &&
			!Params->TryGetStringField(TEXT("widget_component_name"), ChildName))
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Missing child_name parameter"));
		}
	}

	FString ParentName;
	const bool bHasParentName = Params->TryGetStringField(TEXT("parent_name"), ParentName) ||
		Params->TryGetStringField(TEXT("panel_name"), ParentName) ||
		Params->TryGetStringField(TEXT("parent_component_name"), ParentName);

	FString ParentType = TEXT("CanvasPanel");
	Params->TryGetStringField(TEXT("parent_type"), ParentType);

	bool bReparentIfExists = true;
	Params->TryGetBoolField(TEXT("reparent_if_exists"), bReparentIfExists);

	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	double InsertIndexValue = 0.0;
	int32 DesiredIndex = INDEX_NONE;
	if (Params->TryGetNumberField(TEXT("child_index"), InsertIndexValue) ||
		Params->TryGetNumberField(TEXT("insert_index"), InsertIndexValue))
	{
		DesiredIndex = FMath::Max(0, static_cast<int32>(InsertIndexValue));
	}

	TSharedPtr<FJsonObject> SlotProperties;
	if (Params->HasTypedField<EJson::Object>(TEXT("slot_properties")))
	{
		SlotProperties = Params->GetObjectField(TEXT("slot_properties"));
	}

	FWidgetAddChildRequest Request;
	Request.WidgetBlueprintName = WidgetBlueprintName;
	Request.ChildName = ChildName;
	Request.ParentName = bHasParentName ? ParentName : TEXT("");
	Request.ParentType = ParentType;
	Request.bReparentIfExists = bReparentIfExists;
	if (DesiredIndex != INDEX_NONE)
	{
		Request.InsertIndex = DesiredIndex;
	}
	Request.SlotProperties = SlotProperties;

	const auto Result = ComponentService->AddChildToPanel(WidgetBlueprint, Request);
	if (Result.IsError())
	{
		return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
	}

	const FWidgetAddChildResult& Payload = Result.GetValue();

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Response->SetStringField(TEXT("child_name"), Payload.ChildName);
	Response->SetStringField(TEXT("parent_name"), Payload.ParentName);
	Response->SetStringField(TEXT("parent_type"), Payload.ParentType);
	Response->SetBoolField(TEXT("reparented"), Payload.bReparented);
	Response->SetBoolField(TEXT("slot_properties_applied"), Payload.bSlotPropertiesApplied);
	Response->SetBoolField(TEXT("structure_changed"), Payload.bStructureChanged);
	if (Payload.ChildIndex.IsSet())
	{
		Response->SetNumberField(TEXT("child_index"), Payload.ChildIndex.GetValue());
	}
	Response->SetStringField(TEXT("note"), TEXT("Child widget successfully attached to parent panel"));
	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleRemoveUMGComponent(const TSharedPtr<FJsonObject>& Params)
{
	if (!ComponentService.IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetComponentService not available"));
	}

	FString WidgetBlueprintName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}

	FString ComponentName;
	if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		// Fallbacks used by some callers
		if (!Params->TryGetStringField(TEXT("widget_component_name"), ComponentName))
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
		}
	}

	bool bRemoveChildren = false;
	Params->TryGetBoolField(TEXT("remove_children"), bRemoveChildren);

	bool bRemoveFromVariables = false;
	Params->TryGetBoolField(TEXT("remove_from_variables"), bRemoveFromVariables);

	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	FWidgetRemoveComponentRequest Request;
	Request.ComponentName = ComponentName;
	Request.bRemoveChildren = bRemoveChildren;
	Request.bRemoveFromVariables = bRemoveFromVariables;

	const auto Result = ComponentService->RemoveComponent(WidgetBlueprint, Request);
	if (Result.IsError())
	{
		return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
	}

	const FWidgetRemoveComponentResult& Payload = Result.GetValue();

	TArray<TSharedPtr<FJsonValue>> RemovedComponents;
	for (const FWidgetComponentRecord& Record : Payload.RemovedComponents)
	{
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Record.Name);
		Entry->SetStringField(TEXT("type"), Record.Type);
		RemovedComponents.Add(MakeShared<FJsonValueObject>(Entry));
	}

	TArray<TSharedPtr<FJsonValue>> OrphanedChildren;
	for (const FWidgetComponentRecord& Record : Payload.OrphanedChildren)
	{
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Record.Name);
		Entry->SetStringField(TEXT("type"), Record.Type);
		OrphanedChildren.Add(MakeShared<FJsonValueObject>(Entry));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Response->SetStringField(TEXT("component_name"), ComponentName);
	Response->SetArrayField(TEXT("removed_components"), RemovedComponents);
	Response->SetArrayField(TEXT("orphaned_children"), OrphanedChildren);
	Response->SetBoolField(TEXT("variable_cleanup"), Payload.bVariableCleanupPerformed);
	Response->SetBoolField(TEXT("structure_changed"), Payload.bStructureChanged);

	TSharedPtr<FJsonObject> ParentInfo = MakeShared<FJsonObject>();
	ParentInfo->SetStringField(TEXT("name"), Payload.ParentName);
	ParentInfo->SetStringField(TEXT("type"), Payload.ParentType);
	Response->SetObjectField(TEXT("parent_info"), ParentInfo);

	Response->SetStringField(TEXT("note"), FString::Printf(
		TEXT("Universal component removal completed. Removed %d components, orphaned %d children"),
		RemovedComponents.Num(),
		OrphanedChildren.Num()));

	return Response;
}









// ===================================================================
// UMG Layout Methods Implementation (Stub implementations)
// ===================================================================








TSharedPtr<FJsonObject> FUMGCommands::HandleGetWidgetProperty(const TSharedPtr<FJsonObject>& Params)
{
	if (!PropertyService.IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetPropertyService not available"));
	}

	FString WidgetBlueprintName;
	FString WidgetName;
	FString PropertyName;

	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}

	if (!Params->TryGetStringField(TEXT("component_name"), WidgetName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}

	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing property_name parameter"));
	}

	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	const auto Result = PropertyService->GetWidgetProperty(WidgetBlueprint, WidgetName, PropertyName);
	if (Result.IsError())
	{
		return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
	}

	const FWidgetPropertyGetResult& Payload = Result.GetValue();
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Response->SetStringField(TEXT("component_name"), WidgetName);
	Response->SetStringField(TEXT("property_name"), PropertyName);

	if (Payload.bIsChildOrder)
	{
		Response->SetNumberField(TEXT("property_value"), Payload.ChildOrderValue);
	}
	else if (Payload.Value.HasJson())
	{
		Response->SetField(TEXT("property_value"), Payload.Value.JsonValue);
	}
	else if (Payload.Value.HasString())
	{
		Response->SetStringField(TEXT("property_value"), Payload.Value.StringValue);
	}
	else
	{
		Response->SetStringField(TEXT("property_value"), TEXT(""));
	}

	Response->SetStringField(TEXT("property_type"), Payload.PropertyType);
	Response->SetBoolField(TEXT("editable"), Payload.bIsEditable);

	TSharedPtr<FJsonObject> Constraints = Payload.Constraints.IsValid() ? Payload.Constraints : MakeShared<FJsonObject>();
	Response->SetObjectField(TEXT("constraints"), Constraints);

	TSharedPtr<FJsonObject> Schema = Payload.Schema.IsValid() ? Payload.Schema : MakeShared<FJsonObject>();
	Response->SetObjectField(TEXT("schema"), Schema);

	TSharedPtr<FJsonObject> AdapterInfo = MakeShared<FJsonObject>();
	AdapterInfo->SetStringField(TEXT("component_kind"), TEXT("UMG"));
	AdapterInfo->SetStringField(TEXT("slot_class"), Payload.SlotClass);
	Response->SetObjectField(TEXT("adapter_info"), AdapterInfo);

	return Response;
}
TSharedPtr<FJsonObject> FUMGCommands::HandleSetWidgetSlotProperties(const TSharedPtr<FJsonObject>& Params)
{
	if (!ComponentService.IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetComponentService not available"));
	}

	FString WidgetBlueprintName;
	FString WidgetName;
	FString SlotType;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("target_widget_name"), WidgetName))
	{
		// Try alternative parameter name
		if (!Params->TryGetStringField(TEXT("widget_component_name"), WidgetName))
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Missing target_widget_name or widget_component_name parameter"));
		}
	}
	
	Params->TryGetStringField(TEXT("slot_type"), SlotType);
	TSharedPtr<FJsonObject> SlotProperties;
	if (Params->HasTypedField<EJson::Object>(TEXT("slot_properties")))
	{
		SlotProperties = Params->GetObjectField(TEXT("slot_properties"));
	}

	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	FWidgetSlotUpdateRequest Request;
	Request.WidgetBlueprintName = WidgetBlueprintName;
	Request.WidgetName = WidgetName;
	Request.SlotTypeOverride = SlotType;
	Request.SlotProperties = SlotProperties;

	const auto Result = ComponentService->SetSlotProperties(WidgetBlueprint, Request);
	if (Result.IsError())
	{
		return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
	}

	const FWidgetSlotUpdateResult& Payload = Result.GetValue();

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Response->SetStringField(TEXT("target_widget_name"), Payload.WidgetName);
	Response->SetStringField(TEXT("slot_type"), Payload.SlotType);
	Response->SetBoolField(TEXT("applied"), Payload.bApplied);
	if (Payload.AppliedProperties.IsValid())
	{
		Response->SetObjectField(TEXT("slot_properties"), Payload.AppliedProperties);
	}
	Response->SetStringField(TEXT("note"), TEXT("Slot properties updated"));
	return Response;
}

// ===================================================================
// UMG Styling Methods Implementation (Stub implementations)
// ===================================================================

TSharedPtr<FJsonObject> FUMGCommands::HandleListWidgetProperties(const TSharedPtr<FJsonObject>& Params)
{
	if (!PropertyService.IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetPropertyService not available"));
	}

	FString WidgetBlueprintName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("component_name"), WidgetName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}

	bool bIncludeSlotProperties = true;
	Params->TryGetBoolField(TEXT("include_slot_properties"), bIncludeSlotProperties);

	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	const auto Result = PropertyService->ListWidgetProperties(WidgetBlueprint, WidgetName, bIncludeSlotProperties);
	if (Result.IsError())
	{
		return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
	}

	const TArray<FWidgetPropertyInfo>& Properties = Result.GetValue();

	TArray<TSharedPtr<FJsonValue>> PropertiesJson;
	PropertiesJson.Reserve(Properties.Num());

	for (const FWidgetPropertyInfo& Info : Properties)
	{
		TSharedPtr<FJsonObject> PropertyObj = MakeShared<FJsonObject>();
		PropertyObj->SetStringField(TEXT("name"), Info.PropertyName);
		PropertyObj->SetStringField(TEXT("type"), Info.PropertyType);
		PropertyObj->SetStringField(TEXT("value"), Info.CurrentValue.IsEmpty() ? TEXT("") : Info.CurrentValue);

		if (!Info.DefaultValue.IsEmpty())
		{
			PropertyObj->SetStringField(TEXT("default_value"), Info.DefaultValue);
		}

		if (!Info.Category.IsEmpty())
		{
			PropertyObj->SetStringField(TEXT("category"), Info.Category);
		}

		PropertyObj->SetBoolField(TEXT("editable"), Info.bIsEditable);
		PropertyObj->SetBoolField(TEXT("blueprint_visible"), Info.bIsBlueprintVisible);

		PropertiesJson.Add(MakeShared<FJsonValueObject>(PropertyObj));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Response->SetStringField(TEXT("component_name"), WidgetName);
	Response->SetArrayField(TEXT("properties"), PropertiesJson);
	Response->SetNumberField(TEXT("count"), PropertiesJson.Num());
	Response->SetBoolField(TEXT("include_slot_properties"), bIncludeSlotProperties);

	return Response;
}


// ===================================================================
// UMG Event Methods Implementation (Stub implementations)
// ===================================================================

TSharedPtr<FJsonObject> FUMGCommands::HandleBindInputEvents(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	TArray<TSharedPtr<FJsonValue>> InputMappings;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	const TArray<TSharedPtr<FJsonValue>>* InputMappingsArray;
	if (!Params->TryGetArrayField(TEXT("input_mappings"), InputMappingsArray))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing input_mappings parameter"));
	}
	
	InputMappings = *InputMappingsArray;
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	if (!EventService.IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetEventService not available"));
	}

	// Convert JSON input mappings to DTOs
	TArray<FWidgetInputMapping> Mappings;
	for (const TSharedPtr<FJsonValue>& MappingValue : InputMappings)
	{
		if (MappingValue->Type == EJson::Object)
		{
			TSharedPtr<FJsonObject> MappingObj = MappingValue->AsObject();
			FWidgetInputMapping Map;
			MappingObj->TryGetStringField(TEXT("event_name"), Map.EventName);
			MappingObj->TryGetStringField(TEXT("function_name"), Map.FunctionName);
			if (!Map.EventName.IsEmpty() && !Map.FunctionName.IsEmpty())
			{
				Mappings.Add(Map);
			}
		}
	}

	const auto ResultT = EventService->BindInputEvents(WidgetBlueprint, Mappings);
	if (ResultT.IsError())
	{
		return FCommonUtils::CreateErrorResponse(ResultT.GetErrorMessage());
	}

	int32 BoundCount = ResultT.GetValue();
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetArrayField(TEXT("input_mappings"), InputMappings);
	Result->SetNumberField(TEXT("bindings_count"), BoundCount);
	Result->SetStringField(TEXT("note"), TEXT("Input events bound to widget functions successfully"));
	return Result;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleGetAvailableEvents(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString WidgetType;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	Params->TryGetStringField(TEXT("widget_type"), WidgetType);
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	if (!EventService.IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetEventService not available"));
	}

	const auto ResultT = EventService->GetAvailableEvents(WidgetBlueprint, WidgetType);
	if (ResultT.IsError())
	{
		return FCommonUtils::CreateErrorResponse(ResultT.GetErrorMessage());
	}

	const TArray<FWidgetEventInfo>& EventsInfo = ResultT.GetValue();
	TArray<TSharedPtr<FJsonValue>> EventsJson;
	for (const FWidgetEventInfo& E : EventsInfo)
	{
		TSharedPtr<FJsonObject> EventObj = MakeShared<FJsonObject>();
		EventObj->SetStringField(TEXT("name"), E.Name);
		EventObj->SetStringField(TEXT("type"), E.Type);
		EventObj->SetStringField(TEXT("description"), E.Description);
		EventsJson.Add(MakeShared<FJsonValueObject>(EventObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("widget_type"), WidgetType);
	Result->SetArrayField(TEXT("available_events"), EventsJson);
	return Result;
}




// ============================================================================
// NEW BULK OPERATIONS AND IMPROVED FUNCTIONALITY - Added based on Issues Report
// ============================================================================

