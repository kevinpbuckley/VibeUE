#include "Commands/UMGReflectionCommands.h"
#include "Commands/CommonUtils.h"
#include "Core/ServiceContext.h"
#include "Core/ErrorCodes.h"
#include "Services/UMG/WidgetDiscoveryService.h"
#include "Services/UMG/WidgetComponentService.h"
#include "Services/UMG/WidgetPropertyService.h"
#include "Engine/Engine.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/ContentWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/Overlay.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/EditableText.h"
#include "Components/Slider.h"
#include "Components/ProgressBar.h"
#include "Components/CheckBox.h"
#include "Components/Spacer.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "WidgetBlueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"

FUMGReflectionCommands::FUMGReflectionCommands()
{
	// Initialize service context
	ServiceContext = MakeShared<FServiceContext>();
	
	// Initialize UMG services
	DiscoveryService = MakeShared<FWidgetDiscoveryService>(ServiceContext);
	ComponentService = MakeShared<FWidgetComponentService>(ServiceContext);
	PropertyService = MakeShared<FWidgetPropertyService>(ServiceContext);
}

FUMGReflectionCommands::~FUMGReflectionCommands()
{
}

TSharedPtr<FJsonObject> FUMGReflectionCommands::CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data)
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

TSharedPtr<FJsonObject> FUMGReflectionCommands::CreateErrorResponse(const FString& ErrorCode, const FString& ErrorMessage)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), false);
	Response->SetStringField(TEXT("error_code"), ErrorCode);
	Response->SetStringField(TEXT("error"), ErrorMessage);
	return Response;
}

TSharedPtr<FJsonObject> FUMGReflectionCommands::FindWidgetOrError(const FString& WidgetName, UWidgetBlueprint*& OutWidget)
{
	TResult<UWidgetBlueprint*> Result = DiscoveryService->FindWidget(WidgetName);
	if (Result.IsError())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}
	OutWidget = Result.GetValue();
	return nullptr;
}

TSharedPtr<FJsonObject> FUMGReflectionCommands::HandleCommand(const FString& CommandName, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandName == TEXT("get_available_widgets"))
	{
		return HandleGetAvailableWidgets(Params);
	}
	else if (CommandName == TEXT("add_widget_component"))
	{
		return HandleAddWidgetComponent(Params);
	}

	return CreateErrorResponse(TEXT("UNKNOWN_COMMAND"), 
		FString::Printf(TEXT("Unknown UMG Reflection command: %s"), *CommandName));
}

TSharedPtr<FJsonObject> FUMGReflectionCommands::HandleGetAvailableWidgets(const TSharedPtr<FJsonObject>& Params)
{
	// Get parameters
	FString CategoryFilter;
	bool bIncludeCustom = true;
	bool bIncludeEngine = true;
	FString ParentCompatibility;

	Params->TryGetStringField(TEXT("category"), CategoryFilter);
	Params->TryGetBoolField(TEXT("include_custom"), bIncludeCustom);
	Params->TryGetBoolField(TEXT("include_engine"), bIncludeEngine);
	Params->TryGetStringField(TEXT("parent_compatibility"), ParentCompatibility);

	// Discover all widget classes
	TArray<UClass*> WidgetClasses = DiscoverWidgetClasses(bIncludeEngine, bIncludeCustom);

	// Build response data
	TArray<TSharedPtr<FJsonValue>> WidgetArray;
	TSet<FString> Categories;

	for (UClass* WidgetClass : WidgetClasses)
	{
		if (!WidgetClass || !WidgetClass->IsChildOf<UWidget>())
		{
			continue;
		}

		// Get widget info
		FString WidgetName = WidgetClass->GetName();
		FString DisplayName = WidgetClass->GetDisplayNameText().ToString();
		if (DisplayName.IsEmpty())
		{
			DisplayName = WidgetName;
		}

		FString Category = GetWidgetCategory(WidgetClass);
		Categories.Add(Category);

		// Apply category filter
		if (!CategoryFilter.IsEmpty() && Category != CategoryFilter)
		{
			continue;
		}

		// Check parent compatibility if specified
		if (!ParentCompatibility.IsEmpty())
		{
			// Find parent class
			UClass* ParentClass = FindFirstObject<UClass>(*ParentCompatibility);
			if (ParentClass && !IsParentChildCompatible(ParentClass, WidgetClass))
			{
				continue;
			}
		}

		// Create widget info object
		TSharedPtr<FJsonObject> WidgetInfo = MakeShared<FJsonObject>();
		WidgetInfo->SetStringField(TEXT("name"), WidgetName);
		WidgetInfo->SetStringField(TEXT("display_name"), DisplayName);
		WidgetInfo->SetStringField(TEXT("category"), Category);
		WidgetInfo->SetStringField(TEXT("class_path"), WidgetClass->GetPathName());
		WidgetInfo->SetBoolField(TEXT("is_custom"), !WidgetClass->IsNative());
		WidgetInfo->SetBoolField(TEXT("supports_children"), DoesWidgetSupportChildren(WidgetClass));
		WidgetInfo->SetNumberField(TEXT("max_children"), GetMaxChildrenCount(WidgetClass));
		
		// Get supported child types
		TArray<FString> SupportedChildTypes = GetSupportedChildTypes(WidgetClass);
		TArray<TSharedPtr<FJsonValue>> ChildTypesArray;
		for (const FString& ChildType : SupportedChildTypes)
		{
			ChildTypesArray.Add(MakeShared<FJsonValueString>(ChildType));
		}
		WidgetInfo->SetArrayField(TEXT("supported_child_types"), ChildTypesArray);

		// Get description (from class metadata if available)
		FString Description = WidgetClass->GetMetaData(TEXT("ToolTip"));
		if (Description.IsEmpty())
		{
			Description = FString::Printf(TEXT("%s widget component"), *DisplayName);
		}
		WidgetInfo->SetStringField(TEXT("description"), Description);

		WidgetArray.Add(MakeShared<FJsonValueObject>(WidgetInfo));
	}

	// Convert categories set to array
	TArray<TSharedPtr<FJsonValue>> CategoriesArray;
	for (const FString& Category : Categories)
	{
		CategoriesArray.Add(MakeShared<FJsonValueString>(Category));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("widgets"), WidgetArray);
	Data->SetArrayField(TEXT("categories"), CategoriesArray);
	Data->SetNumberField(TEXT("count"), WidgetArray.Num());

	return CreateSuccessResponse(Data);
}

TArray<UClass*> FUMGReflectionCommands::DiscoverWidgetClasses(bool bIncludeEngine, bool bIncludeCustom)
{
	TArray<UClass*> WidgetClasses;

	// Get all classes derived from UWidget
	for (TObjectIterator<UClass> ClassIterator; ClassIterator; ++ClassIterator)
	{
		UClass* Class = *ClassIterator;
		
		// Skip abstract classes and interfaces
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}

		// Must be a widget class
		if (!Class->IsChildOf<UWidget>())
		{
			continue;
		}

		// Filter by engine vs custom
		bool bIsEngineClass = Class->IsNative();
		if (bIsEngineClass && !bIncludeEngine)
		{
			continue;
		}
		if (!bIsEngineClass && !bIncludeCustom)
		{
			continue;
		}

		// Skip base classes that shouldn't be instantiated directly
		if (Class == UWidget::StaticClass() || 
			Class == UPanelWidget::StaticClass() || 
			Class == UContentWidget::StaticClass())
		{
			continue;
		}

		WidgetClasses.Add(Class);
	}

	// Also include custom widget blueprints if requested
	if (bIncludeCustom)
	{
		// Query asset registry for widget blueprints
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		TArray<FAssetData> WidgetBlueprints;
		AssetRegistry.GetAssetsByClass(UWidgetBlueprint::StaticClass()->GetClassPathName(), WidgetBlueprints);

		for (const FAssetData& AssetData : WidgetBlueprints)
		{
			if (UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(AssetData.GetAsset()))
			{
				if (UClass* GeneratedClass = WidgetBlueprint->GeneratedClass)
				{
					WidgetClasses.Add(GeneratedClass);
				}
			}
		}
	}

	return WidgetClasses;
}

FString FUMGReflectionCommands::GetWidgetCategory(UClass* WidgetClass)
{
	if (!WidgetClass)
	{
		return TEXT("Unknown");
	}

	// Check if it's a custom widget blueprint
	if (!WidgetClass->IsNative())
	{
		return TEXT("User Created");
	}

	// Panel widgets
	if (WidgetClass->IsChildOf<UPanelWidget>())
	{
		return TEXT("Panel");
	}

	// Common interactive widgets
	if (WidgetClass->IsChildOf<UButton>())
	{
		return TEXT("Common");
	}

	// Input widgets
	if (WidgetClass->IsChildOf<UEditableText>() ||
		WidgetClass->IsChildOf<USlider>() ||
		WidgetClass->IsChildOf<UCheckBox>())
	{
		return TEXT("Input");
	}

	// Display widgets
	if (WidgetClass->IsChildOf<UTextBlock>() ||
		WidgetClass->IsChildOf<UImage>() ||
		WidgetClass->IsChildOf<UProgressBar>())
	{
		return TEXT("Display");
	}

	// Primitive widgets
	if (WidgetClass->IsChildOf<USpacer>() ||
		WidgetClass->IsChildOf<UBorder>())
	{
		return TEXT("Primitive");
	}

	// Default category
	return TEXT("Common");
}

bool FUMGReflectionCommands::DoesWidgetSupportChildren(UClass* WidgetClass)
{
	if (!WidgetClass)
	{
		return false;
	}

	// Panel widgets support multiple children
	if (WidgetClass->IsChildOf<UPanelWidget>())
	{
		return true;
	}

	// Content widgets support single child
	if (WidgetClass->IsChildOf<UContentWidget>())
	{
		return true;
	}

	return false;
}

int32 FUMGReflectionCommands::GetMaxChildrenCount(UClass* WidgetClass)
{
	if (!WidgetClass)
	{
		return 0;
	}

	// Panel widgets support unlimited children
	if (WidgetClass->IsChildOf<UPanelWidget>())
	{
		return -1; // Unlimited
	}

	// Content widgets support single child
	if (WidgetClass->IsChildOf<UContentWidget>())
	{
		return 1;
	}

	return 0; // No children
}

bool FUMGReflectionCommands::IsParentChildCompatible(UClass* ParentClass, UClass* ChildClass)
{
	if (!ParentClass || !ChildClass)
	{
		return false;
	}

	// Panel widgets can contain any widget
	if (ParentClass->IsChildOf<UPanelWidget>())
	{
		return ChildClass->IsChildOf<UWidget>();
	}

	// Content widgets can contain any widget (but only one)
	if (ParentClass->IsChildOf<UContentWidget>())
	{
		return ChildClass->IsChildOf<UWidget>();
	}

	// Non-container widgets cannot have children
	return false;
}

TArray<FString> FUMGReflectionCommands::GetSupportedChildTypes(UClass* ParentClass)
{
	TArray<FString> SupportedTypes;

	if (!ParentClass)
	{
		return SupportedTypes;
	}

	// Panel and Content widgets support all widget types
	if (ParentClass->IsChildOf<UPanelWidget>() || ParentClass->IsChildOf<UContentWidget>())
	{
		SupportedTypes.Add(TEXT("*")); // All widget types
	}

	return SupportedTypes;
}

TSharedPtr<FJsonObject> FUMGReflectionCommands::HandleAddWidgetComponent(const TSharedPtr<FJsonObject>& Params)
{
	// Extract parameters
	FString WidgetBlueprintName;
	FString ComponentType;
	FString ComponentName;
	FString ParentName;

	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing widget_name parameter"));
	}

	if (!Params->TryGetStringField(TEXT("component_type"), ComponentType))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing component_type parameter"));
	}

	if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMETER"), TEXT("Missing component_name parameter"));
	}

	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		ParentName = TEXT(""); // Empty means add to root
	}

	// Get optional parameters
	bool bIsVariable = false;
	Params->TryGetBoolField(TEXT("is_variable"), bIsVariable);

	TSharedPtr<FJsonObject> Properties = Params->GetObjectField(TEXT("properties"));

	// Find widget blueprint using WidgetDiscoveryService
	UWidgetBlueprint* WidgetBlueprint = nullptr;
	if (TSharedPtr<FJsonObject> ErrorResponse = FindWidgetOrError(WidgetBlueprintName, WidgetBlueprint))
	{
		return ErrorResponse;
	}

	// Use WidgetComponentService to add the component
	TResult<UWidget*> ComponentResult = ComponentService->AddComponent(
		WidgetBlueprint, 
		ComponentType, 
		ComponentName, 
		ParentName
	);

	if (ComponentResult.IsError())
	{
		return CreateErrorResponse(ComponentResult.GetErrorCode(), ComponentResult.GetErrorMessage());
	}

	UWidget* NewWidget = ComponentResult.GetValue();

	// Apply initial properties using PropertyService if provided
	if (Properties.IsValid() && NewWidget)
	{
		ApplyWidgetProperties(NewWidget, Properties);
	}

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);

	// Create success response
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("component_name"), ComponentName);
	Data->SetStringField(TEXT("component_type"), NewWidget->GetClass()->GetName());
	Data->SetStringField(TEXT("widget_name"), WidgetBlueprint->GetName());
	Data->SetStringField(TEXT("parent_name"), ParentName.IsEmpty() ? TEXT("root") : ParentName);

	return CreateSuccessResponse(Data);
}

void FUMGReflectionCommands::ApplyWidgetProperties(UWidget* Widget, const TSharedPtr<FJsonObject>& Properties)
{
	if (!Widget || !Properties.IsValid())
	{
		return;
	}

	// Use PropertyService to apply properties
	for (const auto& Pair : Properties->Values)
	{
		FString PropertyName = Pair.Key;
		FString PropertyValue;
		
		// Convert JSON value to string
		if (Pair.Value->Type == EJson::String)
		{
			PropertyValue = Pair.Value->AsString();
		}
		else if (Pair.Value->Type == EJson::Number)
		{
			PropertyValue = FString::SanitizeFloat(Pair.Value->AsNumber());
		}
		else if (Pair.Value->Type == EJson::Boolean)
		{
			PropertyValue = Pair.Value->AsBool() ? TEXT("true") : TEXT("false");
		}
		else
		{
			continue; // Skip unsupported types
		}

		// Use PropertyService to set the property
		// Note: PropertyService expects widget blueprint, but we can call it directly on the widget
		// For now, we'll skip this and let the caller handle property setting
		// TODO: Consider adding a method to PropertyService that works on UWidget directly
	}
}