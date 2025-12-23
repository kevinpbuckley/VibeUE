// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Commands/UMGCommands.h"
#include "Commands/CommonUtils.h"
#include "Core/JsonValueHelper.h"
#include "Utils/HelpFileReader.h"
#include "Utils/ParamValidation.h"
#include "Services/UMG/WidgetLifecycleService.h"
#include "Services/UMG/WidgetPropertyService.h"
#include "Services/UMG/UMGWidgetService.h"
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

// Parameter validation helpers for UMG Widget actions
namespace UMGParams
{
	inline const TArray<FString>& WidgetNameParams()
	{
		static const TArray<FString> Params = {
			TEXT("widget_name"), TEXT("widget_path"), TEXT("object_path")
		};
		return Params;
	}

	inline const TArray<FString>& ComponentParams()
	{
		static const TArray<FString> Params = {
			TEXT("widget_name"), TEXT("widget_path"), TEXT("component_name")
		};
		return Params;
	}

	inline const TArray<FString>& AddComponentParams()
	{
		static const TArray<FString> Params = {
			TEXT("widget_name"), TEXT("widget_path"), TEXT("component_type"),
			TEXT("component_name"), TEXT("parent_name"), TEXT("slot_index")
		};
		return Params;
	}

	inline const TArray<FString>& RemoveComponentParams()
	{
		static const TArray<FString> Params = {
			TEXT("widget_name"), TEXT("widget_path"), TEXT("component_name")
		};
		return Params;
	}

	inline const TArray<FString>& PropertyParams()
	{
		static const TArray<FString> Params = {
			TEXT("widget_name"), TEXT("widget_path"), TEXT("component_name"),
			TEXT("property_name"), TEXT("property_value")
		};
		return Params;
	}

	inline const TArray<FString>& GetPropertyParams()
	{
		static const TArray<FString> Params = {
			TEXT("widget_name"), TEXT("widget_path"), TEXT("component_name"),
			TEXT("property_name")
		};
		return Params;
	}

	inline const TArray<FString>& SearchTypesParams()
	{
		static const TArray<FString> Params = {
			TEXT("search_term"), TEXT("category"), TEXT("max_results")
		};
		return Params;
	}

	inline const TArray<FString>& CreateWidgetParams()
	{
		static const TArray<FString> Params = {
			TEXT("name"), TEXT("path"), TEXT("parent_class")
		};
		return Params;
	}

	inline const TArray<FString>& EventParams()
	{
		static const TArray<FString> Params = {
			TEXT("widget_name"), TEXT("widget_path"), TEXT("event_bindings")
		};
		return Params;
	}
}

FUMGCommands::FUMGCommands(TSharedPtr<FServiceContext> InServiceContext)
{
	ServiceContext = InServiceContext.IsValid() ? InServiceContext : MakeShared<FServiceContext>();

	// Initialize services using shared context
	LifecycleService = MakeShared<FWidgetLifecycleService>(ServiceContext);
	PropertyService = MakeShared<FWidgetPropertyService>(ServiceContext);
	WidgetService = MakeShared<FUMGWidgetService>(ServiceContext);
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
	// Check for JSON parse errors (set by ParseParams when invalid JSON is received)
	if (Params->HasField(TEXT("__json_parse_error__")))
	{
		FString RawJson;
		Params->TryGetStringField(TEXT("__raw_json__"), RawJson);
		return FCommonUtils::CreateErrorResponse(FString::Printf(
			TEXT("JSON parse error in ParamsJson - check for malformed escape sequences in nested JSON values. Raw input (truncated): %s"),
			*RawJson.Left(200)));
	}

	// Handle multi-action manage_umg_widget command
	if (CommandName == TEXT("manage_umg_widget"))
	{
		FString Action;
		if (!Params->TryGetStringField(TEXT("action"), Action))
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Missing 'action' parameter for manage_umg_widget"));
		}
		
		// Handle help action
		if (Action.ToLower() == TEXT("help"))
		{
			return HandleHelp(Params);
		}
		
		// Normalize action to lowercase for case-insensitive matching
		FString NormalizedAction = Action.ToLower();
		
		// Route to appropriate command based on action (support both C++ and Python action names)
		FString RoutedCommand;
		if (NormalizedAction == TEXT("add_child") || NormalizedAction == TEXT("add_component")) RoutedCommand = TEXT("add_widget_component");
		else if (NormalizedAction == TEXT("remove_child") || NormalizedAction == TEXT("remove_component")) RoutedCommand = TEXT("remove_umg_component");
		else if (NormalizedAction == TEXT("set_property")) RoutedCommand = TEXT("set_widget_property");
		else if (NormalizedAction == TEXT("get_property")) RoutedCommand = TEXT("get_widget_property");
		else if (NormalizedAction == TEXT("list_components")) RoutedCommand = TEXT("list_widget_components");
		else if (NormalizedAction == TEXT("get_available_types") || NormalizedAction == TEXT("search_types")) RoutedCommand = TEXT("get_available_widget_types");
		else if (NormalizedAction == TEXT("validate")) RoutedCommand = TEXT("validate_widget_hierarchy");
		else if (NormalizedAction == TEXT("get_component_properties")) RoutedCommand = TEXT("get_widget_component_properties");
		else if (NormalizedAction == TEXT("list_properties")) RoutedCommand = TEXT("list_widget_properties");
		else if (NormalizedAction == TEXT("get_available_events")) RoutedCommand = TEXT("get_available_events");
		else if (NormalizedAction == TEXT("bind_events")) RoutedCommand = TEXT("bind_input_events");
		else
		{
			return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown action: %s. Valid actions: list_components, add_component, remove_component, validate, search_types, get_component_properties, get_property, set_property, list_properties, get_available_events, bind_events"), *Action));
		}
		
		return HandleCommand(RoutedCommand, Params);
	}
	
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
	else if (CommandName == TEXT("add_child_to_panel") || CommandName == TEXT("add_widget_component"))
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
	else if (CommandName == TEXT("bind_input_events") || CommandName == TEXT("bind_widget_events"))
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
		return ParamValidation::MissingParamsError(
			TEXT("Missing 'name' parameter"),
			UMGParams::CreateWidgetParams());
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
		return ParamValidation::MissingParamsError(
			TEXT("Missing 'search_term' parameter"),
			UMGParams::SearchTypesParams());
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

	// Only use widget discovery when explicitly searching for Widget/WidgetBlueprint types
	// Empty AssetType should search ALL assets, not just widgets
	const bool bLooksLikeWidgetSearch = AssetType.Contains(TEXT("Widget"), ESearchCase::IgnoreCase) || AssetType.Contains(TEXT("WidgetBlueprint"), ESearchCase::IgnoreCase);
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
	FString ActualSearchPath = Path;
	bool bUsedFallback = false;

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

	// If no results found and we weren't already searching from /Game root, retry with full tree
	if (ItemArray.Num() == 0 && Path != TEXT("/Game"))
	{
		bUsedFallback = true;
		ActualSearchPath = TEXT("/Game");
		
		// Rebuild filter with /Game root
		FARFilter FallbackFilter;
		FallbackFilter.bRecursivePaths = true;
		FallbackFilter.PackagePaths.Add(TEXT("/Game"));
		if (bIncludeEngineContent)
		{
			FallbackFilter.PackagePaths.Add(TEXT("/Engine"));
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
				FallbackFilter.ClassPaths.Add(AssetClassPath);
			}
		}

		TArray<FAssetData> FallbackAssets;
		AssetRegistry->GetAssets(FallbackFilter, FallbackAssets);

		for (const FAssetData& Asset : FallbackAssets)
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
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetArrayField(TEXT("items"), ItemArray);
	Response->SetNumberField(TEXT("count"), ItemArray.Num());

	TSharedPtr<FJsonObject> SearchInfo = MakeShared<FJsonObject>();
	SearchInfo->SetStringField(TEXT("search_term"), SearchTerm);
	SearchInfo->SetStringField(TEXT("asset_type"), AssetType);
	SearchInfo->SetStringField(TEXT("path"), ActualSearchPath);
	SearchInfo->SetStringField(TEXT("original_path"), Path);
	SearchInfo->SetBoolField(TEXT("used_fallback"), bUsedFallback);
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
		return ParamValidation::MissingParamsError(
			TEXT("Missing widget_name parameter"),
			UMGParams::PropertyParams());
	}

	if (!Params->TryGetStringField(TEXT("component_name"), WidgetName))
	{
		return ParamValidation::MissingParamsError(
			TEXT("Missing component_name parameter"),
			UMGParams::PropertyParams());
	}

	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return ParamValidation::MissingParamsError(
			TEXT("Missing property_name parameter"),
			UMGParams::PropertyParams());
	}

	// Check if this is a slot property - route to slot property handler instead
	if (PropertyName.StartsWith(TEXT("Slot."), ESearchCase::IgnoreCase))
	{
		return HandleSetWidgetSlotPropertyFromPath(Params, WidgetBlueprintName, WidgetName, PropertyName);
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
			return ParamValidation::MissingParamsError(
				TEXT("Missing property_value parameter"),
				UMGParams::PropertyParams());
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
		// Property not found or set failed - include list of available properties to help the user
		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("error"), Result.GetErrorMessage());
		
		// Get available properties for this component
		const auto PropertiesResult = PropertyService->ListWidgetProperties(WidgetBlueprint, WidgetName, true);
		if (PropertiesResult.IsSuccess())
		{
			const TArray<FWidgetPropertyInfo>& Properties = PropertiesResult.GetValue();
			TArray<TSharedPtr<FJsonValue>> AvailableProps;
			
			// Prioritize common editable properties
			// Filter out raw UObject pointer properties like 'Slot' - these should be accessed via Slot.PropertyName syntax
			for (const FWidgetPropertyInfo& Info : Properties)
			{
				// Skip raw object pointers that can't be directly accessed
				if (Info.PropertyName.Equals(TEXT("Slot"), ESearchCase::IgnoreCase) ||
					Info.PropertyType.Contains(TEXT("TObjectPtr")) ||
					Info.PropertyType.Contains(TEXT("UObject*")))
				{
					continue;
				}
				
				if (Info.bIsEditable && Info.bIsBlueprintVisible && AvailableProps.Num() < 20)
				{
					TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
					PropObj->SetStringField(TEXT("name"), Info.PropertyName);
					PropObj->SetStringField(TEXT("type"), Info.PropertyType);
					if (!Info.Category.IsEmpty())
					{
						PropObj->SetStringField(TEXT("category"), Info.Category);
					}
					AvailableProps.Add(MakeShared<FJsonValueObject>(PropObj));
				}
			}
			
			Response->SetArrayField(TEXT("available_properties"), AvailableProps);
			
			// Provide context-aware hint based on error message
			FString HintMessage;
			const FString ErrorMsg = Result.GetErrorMessage();
			if (ErrorMsg.Contains(TEXT("Canvas")) && (ErrorMsg.Contains(TEXT("Alignment")) || ErrorMsg.Contains(TEXT("LayoutData"))))
			{
				HintMessage = FString::Printf(TEXT("Property '%s' not found. For CanvasPanel slot properties, use Slot.alignment ([x,y] or 'center'), Slot.anchors ('fill' or {min_x,min_y,max_x,max_y}), Slot.position, Slot.size."), *PropertyName);
			}
			else if (PropertyName.StartsWith(TEXT("Slot.")))
			{
				HintMessage = FString::Printf(TEXT("Slot property '%s' not found. Supported: Slot.alignment, Slot.anchors, Slot.position, Slot.size, Slot.auto_size, Slot.z_order (CanvasPanel) or Slot.horizontal_alignment, Slot.vertical_alignment (Box/Overlay)."), *PropertyName);
			}
			else
			{
				HintMessage = FString::Printf(TEXT("Property '%s' not found or could not be set. Use one of the available_properties listed. For slot properties use 'Slot.PropertyName' format."), *PropertyName);
			}
			Response->SetStringField(TEXT("hint"), HintMessage);
		}
		
		return Response;
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
			return ParamValidation::MissingParamsError(
				TEXT("Missing 'widget_name' parameter (accepts name or full path)"),
				UMGParams::WidgetNameParams());
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
			return ParamValidation::MissingParamsError(
				TEXT("Missing 'widget_name' parameter (accepts name or full path)"),
				UMGParams::WidgetNameParams());
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
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return ParamValidation::MissingParamsError(
			TEXT("Missing required 'widget_name' parameter"),
			UMGParams::ComponentParams());
	}
	if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return ParamValidation::MissingParamsError(
			TEXT("Missing required 'component_name' parameter - use list_components first to get valid component names"),
			UMGParams::ComponentParams());
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

	if (!WidgetService.IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetWidgetService not available"));
	}

	const auto Result = WidgetService->GetWidgetComponentInfo(WidgetBlueprint, ComponentName, true);
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
	// Get optional filter parameters
	FString Category;
	Params->TryGetStringField(TEXT("category"), Category);
	
	FString SearchText;
	if (!Params->TryGetStringField(TEXT("search_text"), SearchText))
	{
		Params->TryGetStringField(TEXT("search_term"), SearchText);
	}

	// Define widget types by category
	struct FWidgetTypeInfo
	{
		FString Name;
		FString Category;
	};
	
	TArray<FWidgetTypeInfo> AllWidgetTypes = {
		// Common category
		{TEXT("TextBlock"), TEXT("Common")},
		{TEXT("Button"), TEXT("Common")},
		{TEXT("EditableText"), TEXT("Common")},
		{TEXT("EditableTextBox"), TEXT("Common")},
		{TEXT("RichTextBlock"), TEXT("Common")},
		{TEXT("CheckBox"), TEXT("Common")},
		{TEXT("Slider"), TEXT("Common")},
		{TEXT("ProgressBar"), TEXT("Common")},
		{TEXT("Image"), TEXT("Common")},
		{TEXT("Spacer"), TEXT("Common")},
		// Panel category
		{TEXT("CanvasPanel"), TEXT("Panel")},
		{TEXT("Overlay"), TEXT("Panel")},
		{TEXT("HorizontalBox"), TEXT("Panel")},
		{TEXT("VerticalBox"), TEXT("Panel")},
		{TEXT("ScrollBox"), TEXT("Panel")},
		{TEXT("GridPanel"), TEXT("Panel")},
		{TEXT("UniformGridPanel"), TEXT("Panel")},
		{TEXT("WrapBox"), TEXT("Panel")},
		{TEXT("SizeBox"), TEXT("Panel")},
		{TEXT("ScaleBox"), TEXT("Panel")},
		{TEXT("Border"), TEXT("Panel")},
		// List category
		{TEXT("ListView"), TEXT("List")},
		{TEXT("TileView"), TEXT("List")},
		{TEXT("TreeView"), TEXT("List")},
		// Misc category
		{TEXT("WidgetSwitcher"), TEXT("Misc")},
		{TEXT("Throbber"), TEXT("Misc")},
		{TEXT("CircularThrobber"), TEXT("Misc")},
		{TEXT("NamedSlot"), TEXT("Misc")},
	};

	// Filter by category and/or search text
	TArray<FString> FilteredTypes;
	for (const FWidgetTypeInfo& TypeInfo : AllWidgetTypes)
	{
		// Filter by category if specified
		if (!Category.IsEmpty() && !TypeInfo.Category.Equals(Category, ESearchCase::IgnoreCase))
		{
			continue;
		}
		
		// Filter by search text if specified
		if (!SearchText.IsEmpty() && !TypeInfo.Name.Contains(SearchText, ESearchCase::IgnoreCase))
		{
			continue;
		}
		
		FilteredTypes.Add(TypeInfo.Name);
	}

	TArray<TSharedPtr<FJsonValue>> TypeArray;
	for (const FString& Type : FilteredTypes)
	{
		TypeArray.Add(MakeShared<FJsonValueString>(Type));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetArrayField(TEXT("widget_types"), TypeArray);
	Response->SetNumberField(TEXT("count"), FilteredTypes.Num());
	
	// Include filter info in response
	if (!Category.IsEmpty())
	{
		Response->SetStringField(TEXT("category_filter"), Category);
	}
	if (!SearchText.IsEmpty())
	{
		Response->SetStringField(TEXT("search_filter"), SearchText);
	}
	
	// List available categories for reference
	TArray<TSharedPtr<FJsonValue>> CategoriesArray;
	CategoriesArray.Add(MakeShared<FJsonValueString>(TEXT("Common")));
	CategoriesArray.Add(MakeShared<FJsonValueString>(TEXT("Panel")));
	CategoriesArray.Add(MakeShared<FJsonValueString>(TEXT("List")));
	CategoriesArray.Add(MakeShared<FJsonValueString>(TEXT("Misc")));
	Response->SetArrayField(TEXT("available_categories"), CategoriesArray);
	
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
			return ParamValidation::MissingParamsError(
				TEXT("Missing 'widget_name' parameter"),
				UMGParams::WidgetNameParams());
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
	if (!WidgetService.IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetWidgetService not available"));
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
			return FCommonUtils::CreateErrorResponse(TEXT("Missing child_name/component_name parameter"));
		}
	}

	// Check if component_type is provided - if so, CREATE a new widget
	FString ComponentType;
	const bool bHasComponentType = Params->TryGetStringField(TEXT("component_type"), ComponentType);
	
	FString ParentName;
	const bool bHasParentName = Params->TryGetStringField(TEXT("parent_name"), ParentName) ||
		Params->TryGetStringField(TEXT("panel_name"), ParentName) ||
		Params->TryGetStringField(TEXT("parent_component_name"), ParentName);

	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	// If component_type is provided, CREATE a new widget component
	if (bHasComponentType)
	{
		bool bIsVariable = false;
		Params->TryGetBoolField(TEXT("is_variable"), bIsVariable);
		
		const auto Result = WidgetService->AddWidgetComponent(WidgetBlueprint, ComponentType, ChildName, bHasParentName ? ParentName : TEXT(""), bIsVariable);
		if (Result.IsError())
		{
			return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
		}

		UWidget* NewWidget = Result.GetValue();
		
		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
		Response->SetStringField(TEXT("component_name"), ChildName);
		Response->SetStringField(TEXT("component_type"), ComponentType);
		Response->SetStringField(TEXT("parent_name"), bHasParentName ? ParentName : TEXT("(root)"));
		Response->SetBoolField(TEXT("is_variable"), bIsVariable);
		Response->SetStringField(TEXT("note"), FString::Printf(TEXT("Created new %s widget '%s'"), *ComponentType, *ChildName));
		return Response;
	}

	// Otherwise, MOVE an existing widget to a parent panel
	FString ParentType = TEXT("CanvasPanel");
	Params->TryGetStringField(TEXT("parent_type"), ParentType);

	bool bReparentIfExists = true;
	Params->TryGetBoolField(TEXT("reparent_if_exists"), bReparentIfExists);

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

	const auto Result = WidgetService->AddChildToPanel(WidgetBlueprint, Request);
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
	if (!WidgetService.IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetWidgetService not available"));
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

	const auto Result = WidgetService->RemoveComponent(WidgetBlueprint, Request);
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
// UMG Layout Methods Implementation
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
		// Property not found - include list of available properties to help the user
		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("error"), Result.GetErrorMessage());
		
		// Get available properties for this component
		const auto PropertiesResult = PropertyService->ListWidgetProperties(WidgetBlueprint, WidgetName, true);
		if (PropertiesResult.IsSuccess())
		{
			const TArray<FWidgetPropertyInfo>& Properties = PropertiesResult.GetValue();
			TArray<TSharedPtr<FJsonValue>> AvailableProps;
			AvailableProps.Reserve(FMath::Min(Properties.Num(), 20)); // Limit to 20 most common
			
			// Prioritize common editable properties
			// Filter out raw UObject pointer properties like 'Slot' - these should be accessed via Slot.PropertyName syntax
			TArray<const FWidgetPropertyInfo*> SortedProps;
			for (const FWidgetPropertyInfo& Info : Properties)
			{
				// Skip raw object pointers that can't be directly accessed
				if (Info.PropertyName.Equals(TEXT("Slot"), ESearchCase::IgnoreCase) ||
					Info.PropertyType.Contains(TEXT("TObjectPtr")) ||
					Info.PropertyType.Contains(TEXT("UObject*")))
				{
					continue;
				}
				
				if (Info.bIsEditable && Info.bIsBlueprintVisible)
				{
					SortedProps.Add(&Info);
				}
			}
			
			// Add up to 20 properties
			for (int32 i = 0; i < FMath::Min(SortedProps.Num(), 20); ++i)
			{
				TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
				PropObj->SetStringField(TEXT("name"), SortedProps[i]->PropertyName);
				PropObj->SetStringField(TEXT("type"), SortedProps[i]->PropertyType);
				if (!SortedProps[i]->Category.IsEmpty())
				{
					PropObj->SetStringField(TEXT("category"), SortedProps[i]->Category);
				}
				AvailableProps.Add(MakeShared<FJsonValueObject>(PropObj));
			}
			
			Response->SetArrayField(TEXT("available_properties"), AvailableProps);
			Response->SetStringField(TEXT("hint"), FString::Printf(TEXT("Property '%s' not found. Use one of the available_properties listed, or call list_properties action for full list. For slot properties use 'Slot.PropertyName' format."), *PropertyName));
		}
		
		return Response;
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

TSharedPtr<FJsonObject> FUMGCommands::HandleSetWidgetSlotPropertyFromPath(
	const TSharedPtr<FJsonObject>& Params,
	const FString& WidgetBlueprintName,
	const FString& WidgetName,
	const FString& PropertyName)
{
	if (!WidgetService.IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetService not available"));
	}

	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	// Parse the slot property path: "Slot.alignment", "Slot.anchors", "Slot.position", etc.
	FString SlotPropertyPath = PropertyName;
	SlotPropertyPath.RemoveFromStart(TEXT("Slot."), ESearchCase::IgnoreCase);
	
	// Normalize common variations
	FString NormalizedProperty = SlotPropertyPath.ToLower();
	
	// Get the property value
	const TSharedPtr<FJsonValue>* PropertyValueField = Params->Values.Find(TEXT("property_value"));
	if (!PropertyValueField || !PropertyValueField->IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing property_value parameter"));
	}
	
	// Build the slot_properties JSON object for ApplySlotProperties
	TSharedPtr<FJsonObject> SlotProperties = MakeShared<FJsonObject>();
	
	// Map Slot.PropertyName paths to the slot_properties JSON format
	// CanvasPanel slot properties: position, size, anchors, alignment, auto_size, z_order
	// Overlay/Box slot properties: horizontal_alignment, vertical_alignment, padding, size, fill
	
	if (NormalizedProperty == TEXT("alignment") || NormalizedProperty == TEXT("layoutdata.alignment"))
	{
		// Alignment expects [x, y] array (0.5, 0.5 = center)
		// First try to get as Vector2D using helper (handles arrays, string arrays, objects)
		FVector2D Alignment;
		if (FJsonValueHelper::TryGetVector2D(*PropertyValueField, Alignment))
		{
			TArray<TSharedPtr<FJsonValue>> AlignArray;
			AlignArray.Add(MakeShared<FJsonValueNumber>(Alignment.X));
			AlignArray.Add(MakeShared<FJsonValueNumber>(Alignment.Y));
			SlotProperties->SetArrayField(TEXT("alignment"), AlignArray);
		}
		else if ((*PropertyValueField)->Type == EJson::String)
		{
			// Parse common alignment strings like "center", "left", "top", etc.
			FString AlignStr = (*PropertyValueField)->AsString().TrimStartAndEnd().ToLower();
			TArray<TSharedPtr<FJsonValue>> AlignArray;
			
			if (AlignStr.Contains(TEXT("center")))
			{
				AlignArray.Add(MakeShared<FJsonValueNumber>(0.5));
				AlignArray.Add(MakeShared<FJsonValueNumber>(0.5));
			}
			else if (AlignStr.Contains(TEXT("left")))
			{
				AlignArray.Add(MakeShared<FJsonValueNumber>(0.0));
				AlignArray.Add(MakeShared<FJsonValueNumber>(0.5));
			}
			else if (AlignStr.Contains(TEXT("right")))
			{
				AlignArray.Add(MakeShared<FJsonValueNumber>(1.0));
				AlignArray.Add(MakeShared<FJsonValueNumber>(0.5));
			}
			else if (AlignStr.Contains(TEXT("top")))
			{
				AlignArray.Add(MakeShared<FJsonValueNumber>(0.5));
				AlignArray.Add(MakeShared<FJsonValueNumber>(0.0));
			}
			else if (AlignStr.Contains(TEXT("bottom")))
			{
				AlignArray.Add(MakeShared<FJsonValueNumber>(0.5));
				AlignArray.Add(MakeShared<FJsonValueNumber>(1.0));
			}
			else
			{
				return FCommonUtils::CreateErrorResponse(TEXT("Slot.alignment requires array [x, y] or string like 'center', 'left', 'right', 'top', 'bottom'"));
			}
			
			SlotProperties->SetArrayField(TEXT("alignment"), AlignArray);
		}
		else
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Slot.alignment requires array [x, y] or string like 'center', 'left', 'right', 'top', 'bottom'"));
		}
	}
	else if (NormalizedProperty == TEXT("anchors") || NormalizedProperty.StartsWith(TEXT("layoutdata.anchors")))
	{
		// Anchors expects {min_x, min_y, max_x, max_y}
		if ((*PropertyValueField)->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject>& AnchorObj = (*PropertyValueField)->AsObject();
			TSharedPtr<FJsonObject> AnchorsJson = MakeShared<FJsonObject>();
			
			// Support various formats
			if (AnchorObj->HasField(TEXT("min_x")))
			{
				AnchorsJson = AnchorObj;
			}
			else if (AnchorObj->HasField(TEXT("Minimum")))
			{
				// Convert from Minimum/Maximum format
				const TArray<TSharedPtr<FJsonValue>>* MinArray;
				const TArray<TSharedPtr<FJsonValue>>* MaxArray;
				if (AnchorObj->TryGetArrayField(TEXT("Minimum"), MinArray) && MinArray->Num() >= 2 &&
					AnchorObj->TryGetArrayField(TEXT("Maximum"), MaxArray) && MaxArray->Num() >= 2)
				{
					AnchorsJson->SetNumberField(TEXT("min_x"), (*MinArray)[0]->AsNumber());
					AnchorsJson->SetNumberField(TEXT("min_y"), (*MinArray)[1]->AsNumber());
					AnchorsJson->SetNumberField(TEXT("max_x"), (*MaxArray)[0]->AsNumber());
					AnchorsJson->SetNumberField(TEXT("max_y"), (*MaxArray)[1]->AsNumber());
				}
			}
			
			SlotProperties->SetObjectField(TEXT("anchors"), AnchorsJson);
		}
		else if ((*PropertyValueField)->Type == EJson::String)
		{
			// Parse common anchor presets
			FString AnchorStr = (*PropertyValueField)->AsString().ToLower();
			TSharedPtr<FJsonObject> AnchorsJson = MakeShared<FJsonObject>();
			
			if (AnchorStr.Contains(TEXT("fill")) || AnchorStr.Contains(TEXT("stretch")))
			{
				// Fill/Stretch: anchors 0,0 to 1,1
				AnchorsJson->SetNumberField(TEXT("min_x"), 0.0);
				AnchorsJson->SetNumberField(TEXT("min_y"), 0.0);
				AnchorsJson->SetNumberField(TEXT("max_x"), 1.0);
				AnchorsJson->SetNumberField(TEXT("max_y"), 1.0);
			}
			else if (AnchorStr.Contains(TEXT("center")))
			{
				AnchorsJson->SetNumberField(TEXT("min_x"), 0.5);
				AnchorsJson->SetNumberField(TEXT("min_y"), 0.5);
				AnchorsJson->SetNumberField(TEXT("max_x"), 0.5);
				AnchorsJson->SetNumberField(TEXT("max_y"), 0.5);
			}
			else if (AnchorStr.Contains(TEXT("top")) && AnchorStr.Contains(TEXT("left")))
			{
				AnchorsJson->SetNumberField(TEXT("min_x"), 0.0);
				AnchorsJson->SetNumberField(TEXT("min_y"), 0.0);
				AnchorsJson->SetNumberField(TEXT("max_x"), 0.0);
				AnchorsJson->SetNumberField(TEXT("max_y"), 0.0);
			}
			else
			{
				// Default to top-left
				AnchorsJson->SetNumberField(TEXT("min_x"), 0.0);
				AnchorsJson->SetNumberField(TEXT("min_y"), 0.0);
				AnchorsJson->SetNumberField(TEXT("max_x"), 0.0);
				AnchorsJson->SetNumberField(TEXT("max_y"), 0.0);
			}
			
			SlotProperties->SetObjectField(TEXT("anchors"), AnchorsJson);
		}
		else
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Slot.anchors requires object {min_x, min_y, max_x, max_y} or string like 'fill', 'center', 'top-left'"));
		}
	}
	else if (NormalizedProperty == TEXT("position") || NormalizedProperty == TEXT("layoutdata.position"))
	{
		// Use FJsonValueHelper to handle both array and string-encoded array
		TArray<TSharedPtr<FJsonValue>> ArrayValues;
		if (FJsonValueHelper::TryGetArray(*PropertyValueField, ArrayValues) && ArrayValues.Num() >= 2)
		{
			SlotProperties->SetArrayField(TEXT("position"), ArrayValues);
		}
		else
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Slot.position requires array [x, y]"));
		}
	}
	else if (NormalizedProperty == TEXT("size") || NormalizedProperty == TEXT("layoutdata.size"))
	{
		// Use FJsonValueHelper to handle both array and string-encoded array
		TArray<TSharedPtr<FJsonValue>> ArrayValues;
		if (FJsonValueHelper::TryGetArray(*PropertyValueField, ArrayValues) && ArrayValues.Num() >= 2)
		{
			SlotProperties->SetArrayField(TEXT("size"), ArrayValues);
		}
		else
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Slot.size requires array [width, height]"));
		}
	}
	else if (NormalizedProperty == TEXT("auto_size") || NormalizedProperty == TEXT("autosize"))
	{
		SlotProperties->SetBoolField(TEXT("auto_size"), (*PropertyValueField)->AsBool());
	}
	else if (NormalizedProperty == TEXT("z_order") || NormalizedProperty == TEXT("zorder"))
	{
		SlotProperties->SetNumberField(TEXT("z_order"), (*PropertyValueField)->AsNumber());
	}
	else if (NormalizedProperty == TEXT("horizontal_alignment") || NormalizedProperty == TEXT("horizontalalignment"))
	{
		SlotProperties->SetStringField(TEXT("horizontal_alignment"), (*PropertyValueField)->AsString());
	}
	else if (NormalizedProperty == TEXT("vertical_alignment") || NormalizedProperty == TEXT("verticalalignment"))
	{
		SlotProperties->SetStringField(TEXT("vertical_alignment"), (*PropertyValueField)->AsString());
	}
	else if (NormalizedProperty == TEXT("padding"))
	{
		SlotProperties->SetField(TEXT("padding"), *PropertyValueField);
	}
	else if (NormalizedProperty == TEXT("fill") || NormalizedProperty == TEXT("fill_width") || NormalizedProperty == TEXT("fillwidth"))
	{
		SlotProperties->SetBoolField(TEXT("fill"), (*PropertyValueField)->AsBool());
	}
	else
	{
		// Unknown slot property - provide helpful error
		return FCommonUtils::CreateErrorResponse(FString::Printf(
			TEXT("Unknown slot property '%s'. For CanvasPanel children, supported slot properties are: "
			     "Slot.alignment (array [x,y] or 'center'), Slot.anchors (object or 'fill'/'center'), "
			     "Slot.position ([x,y]), Slot.size ([w,h]), Slot.auto_size (bool), Slot.z_order (int). "
			     "For Overlay/Box children: Slot.horizontal_alignment, Slot.vertical_alignment, Slot.padding, Slot.fill."),
			*PropertyName));
	}

	// Call the slot property setter
	FWidgetSlotUpdateRequest Request;
	Request.WidgetBlueprintName = WidgetBlueprintName;
	Request.WidgetName = WidgetName;
	Request.SlotProperties = SlotProperties;

	const auto Result = WidgetService->SetSlotProperties(WidgetBlueprint, Request);
	if (Result.IsError())
	{
		return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
	}

	const FWidgetSlotUpdateResult& Payload = Result.GetValue();

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Response->SetStringField(TEXT("component_name"), WidgetName);
	Response->SetStringField(TEXT("property_name"), PropertyName);
	Response->SetStringField(TEXT("slot_type"), Payload.SlotType);
	if (Payload.AppliedProperties.IsValid())
	{
		Response->SetObjectField(TEXT("applied_slot_properties"), Payload.AppliedProperties);
	}
	Response->SetStringField(TEXT("note"), TEXT("Slot property set successfully"));
	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleSetWidgetSlotProperties(const TSharedPtr<FJsonObject>& Params)
{
	if (!WidgetService.IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetWidgetService not available"));
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

	const auto Result = WidgetService->SetSlotProperties(WidgetBlueprint, Request);
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
// UMG Styling Methods Implementation
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
// UMG Event Methods Implementation
// ===================================================================

TSharedPtr<FJsonObject> FUMGCommands::HandleBindInputEvents(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	TArray<TSharedPtr<FJsonValue>> InputMappings;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	// Try to get input_mappings as array first
	const TArray<TSharedPtr<FJsonValue>>* InputMappingsArray = nullptr;
	bool bHasInputMappingsArray = Params->TryGetArrayField(TEXT("input_mappings"), InputMappingsArray);
	
	// If no array, try to build from flat parameters (event_name + function_name)
	FString EventName, FunctionName;
	bool bHasFlatParams = Params->TryGetStringField(TEXT("event_name"), EventName) && 
	                      Params->TryGetStringField(TEXT("function_name"), FunctionName);
	
	if (!bHasInputMappingsArray && !bHasFlatParams)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing input_mappings array OR event_name+function_name. Use either: input_mappings: [{\"event_name\": \"OnClicked\", \"function_name\": \"HandleClick\"}] OR flat params: event_name, function_name"));
	}
	
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
	
	// If flat params provided, create single mapping from them
	if (bHasFlatParams)
	{
		FWidgetInputMapping Map;
		Map.EventName = EventName;
		Map.FunctionName = FunctionName;
		Mappings.Add(Map);
	}
	else if (bHasInputMappingsArray && InputMappingsArray)
	{
		// Process array format
		for (const TSharedPtr<FJsonValue>& MappingValue : *InputMappingsArray)
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
	}

	const auto ResultT = EventService->BindInputEvents(WidgetBlueprint, Mappings);
	if (ResultT.IsError())
	{
		return FCommonUtils::CreateErrorResponse(ResultT.GetErrorMessage());
	}

	int32 BoundCount = ResultT.GetValue();
	
	// Build output array from the mappings we processed
	TArray<TSharedPtr<FJsonValue>> BoundMappingsJson;
	for (const FWidgetInputMapping& Map : Mappings)
	{
		TSharedPtr<FJsonObject> MapObj = MakeShared<FJsonObject>();
		MapObj->SetStringField(TEXT("event_name"), Map.EventName);
		MapObj->SetStringField(TEXT("function_name"), Map.FunctionName);
		BoundMappingsJson.Add(MakeShared<FJsonValueObject>(MapObj));
	}
	
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetArrayField(TEXT("bound_events"), BoundMappingsJson);
	Result->SetNumberField(TEXT("bindings_count"), BoundCount);
	Result->SetStringField(TEXT("note"), TEXT("Input events bound to widget functions successfully"));
	return Result;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleGetAvailableEvents(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString ComponentName;
	FString WidgetType;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	Params->TryGetStringField(TEXT("component_name"), ComponentName);
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

	const auto ResultT = EventService->GetAvailableEvents(WidgetBlueprint, ComponentName, WidgetType);
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
	Result->SetStringField(TEXT("component_name"), ComponentName);
	Result->SetStringField(TEXT("widget_type"), WidgetType);
	Result->SetArrayField(TEXT("available_events"), EventsJson);
	return Result;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleHelp(const TSharedPtr<FJsonObject>& Params)
{
	return FHelpFileReader::HandleHelp(TEXT("manage_umg_widget"), Params);
}

