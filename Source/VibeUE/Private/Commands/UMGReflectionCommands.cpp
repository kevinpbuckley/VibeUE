#include "Commands/UMGReflectionCommands.h"
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
#include "Core/ServiceContext.h"
#include "../../Public/Commands/CommonUtils.h"

FUMGReflectionCommands::FUMGReflectionCommands()
	: ServiceContext(MakeShared<FServiceContext>())
{
}

FUMGReflectionCommands::FUMGReflectionCommands(TSharedPtr<FServiceContext> InServiceContext)
	: ServiceContext(InServiceContext.IsValid() ? InServiceContext : MakeShared<FServiceContext>())
{
}

FUMGReflectionCommands::~FUMGReflectionCommands()
{
}

TSharedPtr<FJsonObject> FUMGReflectionCommands::HandleCommand(const FString& CommandName, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandName == TEXT("add_widget_component"))
	{
		return HandleAddWidgetComponent(Params);
	}

	return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown UMG Reflection command: %s"), *CommandName));
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
		// Use ServiceContext to get AssetRegistry instead of loading module directly
		IAssetRegistry* AssetRegistry = ServiceContext->GetAssetRegistry();
		if (!AssetRegistry)
		{
			// If AssetRegistry is not available, log warning and continue with engine widgets only
			UE_LOG(LogTemp, Warning, TEXT("Failed to get Asset Registry, custom widget blueprints will not be included"));
		}
		else
		{
			TArray<FAssetData> WidgetBlueprints;
			AssetRegistry->GetAssetsByClass(UWidgetBlueprint::StaticClass()->GetClassPathName(), WidgetBlueprints);

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
	// Get required parameters
	FString WidgetBlueprintName;
	FString ComponentType;
	FString ComponentName;
	FString ParentName;

	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}

	if (!Params->TryGetStringField(TEXT("component_type"), ComponentType))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing component_type parameter"));
	}

	if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}

	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		ParentName = TEXT("root");
	}

	// Get optional parameters
	bool bIsVariable = false;
	Params->TryGetBoolField(TEXT("is_variable"), bIsVariable);

	TSharedPtr<FJsonObject> Properties = Params->GetObjectField(TEXT("properties"));

	// Find widget blueprint
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	// Find component class
	UClass* ComponentClass = FindFirstObject<UClass>(*ComponentType);
	if (!ComponentClass)
	{
		// Try with U prefix for engine classes
		FString ClassNameWithPrefix = FString::Printf(TEXT("U%s"), *ComponentType);
		ComponentClass = FindFirstObject<UClass>(*ClassNameWithPrefix);
	}

	if (!ComponentClass || !ComponentClass->IsChildOf<UWidget>())
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget component type '%s' not found or not a valid widget class"), *ComponentType));
	}

	// Validate creation parameters
	TSharedPtr<FJsonObject> ValidationResult = ValidateWidgetCreation(WidgetBlueprint, ComponentClass, ParentName);
	if (ValidationResult && !ValidationResult->GetBoolField(TEXT("success")))
	{
		return ValidationResult;
	}

	// Create and add the widget component
	return CreateAndAddWidgetComponent(WidgetBlueprint, ComponentClass, ComponentName, ParentName, bIsVariable, Properties);
}

TSharedPtr<FJsonObject> FUMGReflectionCommands::ValidateWidgetCreation(UWidgetBlueprint* WidgetBlueprint, UClass* ComponentClass, const FString& ParentName)
{
	if (!WidgetBlueprint || !ComponentClass)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Invalid widget blueprint or component class"));
	}

	// If parent is "root", check if we can add to root
	if (ParentName == TEXT("root"))
	{
		// Root widget must be a panel that can contain children
		if (!DoesWidgetSupportChildren(ComponentClass))
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Root widget must be a panel that can contain children"));
		}
		return nullptr; // Valid
	}

	// Find parent widget in the widget tree
	UWidget* ParentWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*ParentName));
	if (!ParentWidget)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Parent widget '%s' not found"), *ParentName));
	}

	// Check if parent supports children
	UClass* ParentClass = ParentWidget->GetClass();
	if (!DoesWidgetSupportChildren(ParentClass))
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Parent widget '%s' does not support children"), *ParentName));
	}

	// Check parent-child compatibility
	if (!IsParentChildCompatible(ParentClass, ComponentClass))
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component type '%s' is not compatible with parent '%s'"), *ComponentClass->GetName(), *ParentClass->GetName()));
	}

	// Check child count limits for content widgets
	if (ParentClass->IsChildOf<UContentWidget>())
	{
		if (UContentWidget* ContentParent = Cast<UContentWidget>(ParentWidget))
		{
			if (ContentParent->GetContent() != nullptr)
			{
				return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Content widget '%s' already has a child"), *ParentName));
			}
		}
	}

	return nullptr; // Valid
}

TSharedPtr<FJsonObject> FUMGReflectionCommands::CreateAndAddWidgetComponent(UWidgetBlueprint* WidgetBlueprint, UClass* ComponentClass, const FString& ComponentName, const FString& ParentName, bool bIsVariable, const TSharedPtr<FJsonObject>& Properties)
{
	if (!WidgetBlueprint || !ComponentClass)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Invalid parameters for widget creation"));
	}

	// Create the widget component
	// Add to widget tree
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Widget Blueprint has no widget tree"));
	}

	// Create widget using proper NewObject pattern for widget blueprint editing
	UWidget* NewWidget = nullptr;
	try
	{
		NewWidget = NewObject<UWidget>(WidgetTree, ComponentClass, FName(*ComponentName));
		UE_LOG(LogTemp, Warning, TEXT("Widget creation attempt completed, NewWidget: %s"), NewWidget ? TEXT("Valid") : TEXT("Null"));
	}
	catch (...)
	{
		UE_LOG(LogTemp, Error, TEXT("Exception during widget creation"));
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Exception during widget creation of type '%s'"), *ComponentClass->GetName()));
	}
	
	if (!NewWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create widget component of type '%s'"), *ComponentClass->GetName());
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create widget component of type '%s'"), *ComponentClass->GetName()));
	}

	// Add to parent or set as root
	if (ParentName == TEXT("root"))
	{
		WidgetTree->RootWidget = NewWidget;
	}
	else
	{
		UWidget* ParentWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*ParentName));
		if (!ParentWidget)
		{
			return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Parent widget '%s' not found"), *ParentName));
		}

		// Add to appropriate parent type
		if (UPanelWidget* PanelParent = Cast<UPanelWidget>(ParentWidget))
		{
			PanelParent->AddChild(NewWidget);
		}
		else if (UContentWidget* ContentParent = Cast<UContentWidget>(ParentWidget))
		{
			ContentParent->SetContent(NewWidget);
		}
		else
		{
			return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Parent widget '%s' cannot contain children"), *ParentName));
		}
	}

	// Set as variable if requested
	if (bIsVariable)
	{
		// This would require blueprint compilation - simplified for now
		UE_LOG(LogTemp, Warning, TEXT("Variable creation not fully implemented yet"));
	}

	// Apply initial properties
	if (Properties.IsValid())
	{
		ApplyWidgetProperties(NewWidget, Properties);
	}

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);

	// Create success response
	TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
	ResponseObj->SetBoolField(TEXT("success"), true);
	ResponseObj->SetStringField(TEXT("component_name"), ComponentName);
	ResponseObj->SetStringField(TEXT("component_type"), ComponentClass->GetName());
	ResponseObj->SetStringField(TEXT("widget_name"), WidgetBlueprint->GetName());
	ResponseObj->SetStringField(TEXT("parent_name"), ParentName);

	// Add validation info
	TSharedPtr<FJsonObject> ValidationInfo = MakeShared<FJsonObject>();
	ValidationInfo->SetBoolField(TEXT("parent_supports_children"), true);
	ValidationInfo->SetBoolField(TEXT("child_count_valid"), true);
	ValidationInfo->SetBoolField(TEXT("type_compatibility"), true);
	ResponseObj->SetObjectField(TEXT("validation"), ValidationInfo);

	return ResponseObj;
}

void FUMGReflectionCommands::ApplyWidgetProperties(UWidget* Widget, const TSharedPtr<FJsonObject>& Properties)
{
	if (!Widget || !Properties.IsValid())
	{
		return;
	}

	// Apply common properties
	if (Properties->HasField(TEXT("visibility")))
	{
		FString VisibilityStr;
		if (Properties->TryGetStringField(TEXT("visibility"), VisibilityStr))
		{
			// Convert string to ESlateVisibility enum
			// Implementation would depend on specific visibility values
		}
	}

	// Apply size if specified
	const TArray<TSharedPtr<FJsonValue>>* SizeArray;
	if (Properties->TryGetArrayField(TEXT("size"), SizeArray) && SizeArray->Num() >= 2)
	{
		float Width = (*SizeArray)[0]->AsNumber();
		float Height = (*SizeArray)[1]->AsNumber();
		// Apply size - implementation depends on widget type
	}

	// Add more property applications as needed
}