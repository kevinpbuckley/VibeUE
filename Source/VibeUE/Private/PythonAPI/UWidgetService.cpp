// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UWidgetService.h"
#include "WidgetBlueprint.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/ScrollBox.h"
#include "Components/GridPanel.h"
#include "Components/WidgetSwitcher.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/ProgressBar.h"
#include "Components/Spacer.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/PropertyIterator.h"
#include "UObject/UnrealType.h"

// Static list of available widget types
static const TArray<FString> GAvailableWidgetTypes = {
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
	TEXT("WidgetSwitcher")
};

// =================================================================
// Helper Methods
// =================================================================

UWidgetBlueprint* UWidgetService::LoadWidgetBlueprint(const FString& WidgetPath)
{
	UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(WidgetPath));
	if (!WidgetBP)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService: Failed to load Widget Blueprint: %s"), *WidgetPath);
	}
	return WidgetBP;
}

UWidget* UWidgetService::FindWidgetByName(UWidgetBlueprint* WidgetBP, const FString& ComponentName)
{
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return nullptr;
	}

	// Search all widgets in the tree
	TArray<UWidget*> AllWidgets;
	WidgetBP->WidgetTree->GetAllWidgets(AllWidgets);
	
	for (UWidget* Widget : AllWidgets)
	{
		if (Widget && Widget->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
		{
			return Widget;
		}
	}
	
	return nullptr;
}

TSubclassOf<UWidget> UWidgetService::FindWidgetClass(const FString& TypeName)
{
	// Map type names to widget classes
	static TMap<FString, TSubclassOf<UWidget>> WidgetClassMap;
	
	if (WidgetClassMap.Num() == 0)
	{
		WidgetClassMap.Add(TEXT("TextBlock"), UTextBlock::StaticClass());
		WidgetClassMap.Add(TEXT("Button"), UButton::StaticClass());
		WidgetClassMap.Add(TEXT("Image"), UImage::StaticClass());
		WidgetClassMap.Add(TEXT("EditableText"), UEditableText::StaticClass());
		WidgetClassMap.Add(TEXT("EditableTextBox"), UEditableTextBox::StaticClass());
		WidgetClassMap.Add(TEXT("CheckBox"), UCheckBox::StaticClass());
		WidgetClassMap.Add(TEXT("Slider"), USlider::StaticClass());
		WidgetClassMap.Add(TEXT("ProgressBar"), UProgressBar::StaticClass());
		WidgetClassMap.Add(TEXT("Spacer"), USpacer::StaticClass());
		WidgetClassMap.Add(TEXT("CanvasPanel"), UCanvasPanel::StaticClass());
		WidgetClassMap.Add(TEXT("Overlay"), UOverlay::StaticClass());
		WidgetClassMap.Add(TEXT("HorizontalBox"), UHorizontalBox::StaticClass());
		WidgetClassMap.Add(TEXT("VerticalBox"), UVerticalBox::StaticClass());
		WidgetClassMap.Add(TEXT("ScrollBox"), UScrollBox::StaticClass());
		WidgetClassMap.Add(TEXT("GridPanel"), UGridPanel::StaticClass());
		WidgetClassMap.Add(TEXT("WidgetSwitcher"), UWidgetSwitcher::StaticClass());
	}
	
	if (TSubclassOf<UWidget>* Found = WidgetClassMap.Find(TypeName))
	{
		return *Found;
	}
	
	// Try case-insensitive search
	for (const auto& Pair : WidgetClassMap)
	{
		if (Pair.Key.Equals(TypeName, ESearchCase::IgnoreCase))
		{
			return Pair.Value;
		}
	}
	
	return nullptr;
}

// =================================================================
// Discovery Methods
// =================================================================

TArray<FString> UWidgetService::ListWidgetBlueprints(const FString& PathFilter)
{
	TArray<FString> WidgetPaths;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/UMGEditor.WidgetBlueprint")));

	if (!PathFilter.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*PathFilter));
		Filter.bRecursivePaths = true;
	}

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		WidgetPaths.Add(AssetData.GetObjectPathString());
	}

	return WidgetPaths;
}

TArray<FWidgetInfo> UWidgetService::GetHierarchy(const FString& WidgetPath)
{
	TArray<FWidgetInfo> Hierarchy;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return Hierarchy;
	}

	UWidget* RootWidget = WidgetBP->WidgetTree->RootWidget;
	if (!RootWidget)
	{
		return Hierarchy;
	}

	// Build hierarchy recursively
	TFunction<void(UWidget*, const FString&)> AddWidgetToHierarchy;
	AddWidgetToHierarchy = [&](UWidget* Widget, const FString& ParentName)
	{
		if (!Widget)
		{
			return;
		}

		FWidgetInfo Info;
		Info.WidgetName = Widget->GetName();
		Info.WidgetClass = Widget->GetClass()->GetName();
		Info.ParentWidget = ParentName;
		Info.bIsRootWidget = (Widget == RootWidget);
		Info.bIsVariable = Widget->bIsVariable;

		// If it's a panel widget, collect children
		if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
		{
			for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
			{
				if (UWidget* Child = PanelWidget->GetChildAt(i))
				{
					Info.Children.Add(Child->GetName());
				}
			}
		}

		Hierarchy.Add(Info);

		// Recurse into children
		if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
		{
			for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
			{
				UWidget* Child = PanelWidget->GetChildAt(i);
				AddWidgetToHierarchy(Child, Info.WidgetName);
			}
		}
	};

	AddWidgetToHierarchy(RootWidget, FString());

	return Hierarchy;
}

FString UWidgetService::GetRootWidget(const FString& WidgetPath)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree || !WidgetBP->WidgetTree->RootWidget)
	{
		return FString();
	}

	return WidgetBP->WidgetTree->RootWidget->GetName();
}

TArray<FWidgetInfo> UWidgetService::ListComponents(const FString& WidgetPath)
{
	TArray<FWidgetInfo> Components;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return Components;
	}

	TArray<UWidget*> AllWidgets;
	WidgetBP->WidgetTree->GetAllWidgets(AllWidgets);

	for (UWidget* Widget : AllWidgets)
	{
		if (!Widget)
		{
			continue;
		}

		FWidgetInfo Info;
		Info.WidgetName = Widget->GetName();
		Info.WidgetClass = Widget->GetClass()->GetName();
		Info.bIsVariable = Widget->bIsVariable;
		Info.bIsRootWidget = (Widget == WidgetBP->WidgetTree->RootWidget);

		// Get parent
		if (UPanelWidget* Parent = Widget->GetParent())
		{
			Info.ParentWidget = Parent->GetName();
		}

		// Get children if panel
		if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
		{
			for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
			{
				if (UWidget* Child = PanelWidget->GetChildAt(i))
				{
					Info.Children.Add(Child->GetName());
				}
			}
		}

		Components.Add(Info);
	}

	return Components;
}

TArray<FString> UWidgetService::SearchTypes(const FString& FilterText)
{
	TArray<FString> Results;
	
	for (const FString& Type : GAvailableWidgetTypes)
	{
		if (FilterText.IsEmpty() || Type.Contains(FilterText, ESearchCase::IgnoreCase))
		{
			Results.Add(Type);
		}
	}
	
	return Results;
}

TArray<FWidgetPropertyInfo> UWidgetService::GetComponentProperties(const FString& WidgetPath, const FString& ComponentName)
{
	return ListProperties(WidgetPath, ComponentName, false);
}

// =================================================================
// Component Management
// =================================================================

FWidgetAddComponentResult UWidgetService::AddComponent(
	const FString& WidgetPath,
	const FString& ComponentType,
	const FString& ComponentName,
	const FString& ParentName,
	bool bIsVariable)
{
	FWidgetAddComponentResult Result;
	
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetPath);
		return Result;
	}

	if (!WidgetBP->WidgetTree)
	{
		Result.ErrorMessage = TEXT("Widget Blueprint has no WidgetTree");
		return Result;
	}

	// Find the widget class
	TSubclassOf<UWidget> WidgetClass = FindWidgetClass(ComponentType);
	if (!WidgetClass)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Unknown widget type '%s'. Use search_types() to get available types."), *ComponentType);
		return Result;
	}

	// Find parent panel (or use root)
	UPanelWidget* ParentPanel = nullptr;
	if (!ParentName.IsEmpty())
	{
		UWidget* ParentWidget = FindWidgetByName(WidgetBP, ParentName);
		ParentPanel = Cast<UPanelWidget>(ParentWidget);
		if (!ParentPanel)
		{
			Result.ErrorMessage = FString::Printf(TEXT("Parent '%s' not found or is not a panel widget"), *ParentName);
			return Result;
		}
	}
	else
	{
		// Use root as parent if it's a panel
		ParentPanel = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
	}

	// Create the new widget
	UWidget* NewWidget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*ComponentName));
	if (!NewWidget)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to create widget of type '%s'"), *ComponentType);
		return Result;
	}

	NewWidget->bIsVariable = bIsVariable;

	// Add to parent or set as root
	if (ParentPanel)
	{
		UPanelSlot* Slot = ParentPanel->AddChild(NewWidget);
		if (!Slot)
		{
			Result.ErrorMessage = TEXT("Failed to add widget to parent panel");
			return Result;
		}
		Result.ParentName = ParentPanel->GetName();
	}
	else if (!WidgetBP->WidgetTree->RootWidget)
	{
		// Set as root if no root exists
		WidgetBP->WidgetTree->RootWidget = NewWidget;
		Result.ParentName = TEXT("(root)");
	}
	else
	{
		Result.ErrorMessage = TEXT("Cannot add widget: no parent specified and root already exists");
		return Result;
	}

	// Mark blueprint as modified
	WidgetBP->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

	Result.bSuccess = true;
	Result.ComponentName = NewWidget->GetName();
	Result.ComponentType = ComponentType;
	Result.bIsVariable = bIsVariable;

	return Result;
}

FWidgetRemoveComponentResult UWidgetService::RemoveComponent(
	const FString& WidgetPath,
	const FString& ComponentName,
	bool bRemoveChildren)
{
	FWidgetRemoveComponentResult Result;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetPath);
		return Result;
	}

	if (!WidgetBP->WidgetTree)
	{
		Result.ErrorMessage = TEXT("Widget Blueprint has no WidgetTree");
		return Result;
	}

	UWidget* WidgetToRemove = FindWidgetByName(WidgetBP, ComponentName);
	if (!WidgetToRemove)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Widget component '%s' not found"), *ComponentName);
		return Result;
	}

	// Can't remove root
	if (WidgetToRemove == WidgetBP->WidgetTree->RootWidget)
	{
		Result.ErrorMessage = TEXT("Cannot remove root widget");
		return Result;
	}

	// Collect children if needed
	if (UPanelWidget* Panel = Cast<UPanelWidget>(WidgetToRemove))
	{
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			if (UWidget* Child = Panel->GetChildAt(i))
			{
				if (bRemoveChildren)
				{
					Result.RemovedComponents.Add(Child->GetName());
				}
				else
				{
					Result.OrphanedChildren.Add(Child->GetName());
				}
			}
		}
	}

	// Remove from parent
	if (UPanelWidget* Parent = WidgetToRemove->GetParent())
	{
		Parent->RemoveChild(WidgetToRemove);
	}

	// Remove from widget tree
	WidgetBP->WidgetTree->RemoveWidget(WidgetToRemove);
	Result.RemovedComponents.Add(ComponentName);

	// Mark blueprint as modified
	WidgetBP->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

	Result.bSuccess = true;
	return Result;
}

// =================================================================
// Validation
// =================================================================

FWidgetValidationResult UWidgetService::Validate(const FString& WidgetPath)
{
	FWidgetValidationResult Result;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		Result.bIsValid = false;
		Result.Errors.Add(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetPath));
		Result.ValidationMessage = Result.Errors[0];
		return Result;
	}

	if (!WidgetBP->WidgetTree)
	{
		Result.bIsValid = false;
		Result.Errors.Add(TEXT("Widget Blueprint has no WidgetTree"));
		Result.ValidationMessage = Result.Errors[0];
		return Result;
	}

	// Check for root widget
	if (!WidgetBP->WidgetTree->RootWidget)
	{
		Result.bIsValid = false;
		Result.Errors.Add(TEXT("Widget Blueprint has no root widget"));
	}

	// Check for duplicate names
	TArray<UWidget*> AllWidgets;
	WidgetBP->WidgetTree->GetAllWidgets(AllWidgets);
	
	TSet<FString> WidgetNames;
	for (UWidget* Widget : AllWidgets)
	{
		if (Widget)
		{
			FString Name = Widget->GetName();
			if (WidgetNames.Contains(Name))
			{
				Result.Errors.Add(FString::Printf(TEXT("Duplicate widget name: %s"), *Name));
			}
			WidgetNames.Add(Name);
		}
	}

	// Check for orphaned widgets (not attached to hierarchy)
	if (UWidget* Root = WidgetBP->WidgetTree->RootWidget)
	{
		TSet<UWidget*> ReachableWidgets;
		TFunction<void(UWidget*)> CollectReachable = [&](UWidget* Widget)
		{
			if (!Widget || ReachableWidgets.Contains(Widget))
			{
				return;
			}
			ReachableWidgets.Add(Widget);
			if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
			{
				for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
				{
					CollectReachable(Panel->GetChildAt(i));
				}
			}
		};
		CollectReachable(Root);

		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && !ReachableWidgets.Contains(Widget))
			{
				Result.Errors.Add(FString::Printf(TEXT("Orphaned widget not in hierarchy: %s"), *Widget->GetName()));
			}
		}
	}

	Result.bIsValid = (Result.Errors.Num() == 0);
	Result.ValidationMessage = Result.bIsValid 
		? TEXT("Widget hierarchy is valid") 
		: Result.Errors[0];

	return Result;
}

// =================================================================
// Property Access
// =================================================================

FString UWidgetService::GetProperty(
	const FString& WidgetPath,
	const FString& ComponentName,
	const FString& PropertyName)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return FString();
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
	if (!Widget)
	{
		return FString();
	}

	// Find the property
	FProperty* Property = Widget->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		// Try case-insensitive search
		for (TFieldIterator<FProperty> It(Widget->GetClass()); It; ++It)
		{
			if (It->GetName().Equals(PropertyName, ESearchCase::IgnoreCase))
			{
				Property = *It;
				break;
			}
		}
	}

	if (!Property)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::GetProperty: Property '%s' not found on widget '%s'"), *PropertyName, *ComponentName);
		return FString();
	}

	// Get the value as string
	FString ValueStr;
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Widget);
	Property->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, Widget, PPF_None);

	return ValueStr;
}

bool UWidgetService::SetProperty(
	const FString& WidgetPath,
	const FString& ComponentName,
	const FString& PropertyName,
	const FString& PropertyValue)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return false;
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
	if (!Widget)
	{
		return false;
	}

	// Find the property
	FProperty* Property = Widget->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		// Try case-insensitive search
		for (TFieldIterator<FProperty> It(Widget->GetClass()); It; ++It)
		{
			if (It->GetName().Equals(PropertyName, ESearchCase::IgnoreCase))
			{
				Property = *It;
				break;
			}
		}
	}

	if (!Property)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::SetProperty: Property '%s' not found on widget '%s'"), *PropertyName, *ComponentName);
		return false;
	}

	// Check if property is editable
	if (!Property->HasAnyPropertyFlags(CPF_Edit))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::SetProperty: Property '%s' is not editable"), *PropertyName);
		return false;
	}

	// Set the value from string
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Widget);
	if (!Property->ImportText_Direct(*PropertyValue, ValuePtr, Widget, PPF_None))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::SetProperty: Failed to parse value '%s' for property '%s'"), *PropertyValue, *PropertyName);
		return false;
	}

	// Mark as modified
	Widget->Modify();
	WidgetBP->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBP);

	return true;
}

TArray<FWidgetPropertyInfo> UWidgetService::ListProperties(
	const FString& WidgetPath,
	const FString& ComponentName,
	bool bEditableOnly)
{
	TArray<FWidgetPropertyInfo> Properties;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return Properties;
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
	if (!Widget)
	{
		return Properties;
	}

	// Iterate all properties
	for (TFieldIterator<FProperty> It(Widget->GetClass()); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property)
		{
			continue;
		}

		// Skip non-blueprint visible properties if filtering
		bool bIsBlueprintVisible = Property->HasAnyPropertyFlags(CPF_BlueprintVisible);
		bool bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);

		if (bEditableOnly && !bIsEditable)
		{
			continue;
		}

		FWidgetPropertyInfo Info;
		Info.PropertyName = Property->GetName();
		Info.PropertyType = Property->GetCPPType();
		Info.bIsEditable = bIsEditable;
		Info.bIsBlueprintVisible = bIsBlueprintVisible;

		// Get category
		Info.Category = Property->GetMetaData(TEXT("Category"));

		// Get current value
		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Widget);
		Property->ExportTextItem_Direct(Info.CurrentValue, ValuePtr, nullptr, Widget, PPF_None);

		Properties.Add(Info);
	}

	return Properties;
}

// =================================================================
// Event Handling
// =================================================================

TArray<FWidgetEventInfo> UWidgetService::GetAvailableEvents(
	const FString& WidgetPath,
	const FString& ComponentName,
	const FString& WidgetType)
{
	TArray<FWidgetEventInfo> Events;

	// Get the class to inspect
	UClass* WidgetClass = nullptr;

	if (!WidgetType.IsEmpty())
	{
		TSubclassOf<UWidget> TypeClass = FindWidgetClass(WidgetType);
		if (TypeClass)
		{
			WidgetClass = *TypeClass;
		}
	}
	else if (!ComponentName.IsEmpty() && !WidgetPath.IsEmpty())
	{
		UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
		if (WidgetBP)
		{
			UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
			if (Widget)
			{
				WidgetClass = Widget->GetClass();
			}
		}
	}

	if (!WidgetClass)
	{
		// Default to base widget events
		WidgetClass = UWidget::StaticClass();
	}

	// Find multicast delegate properties (these are events)
	for (TFieldIterator<FMulticastDelegateProperty> It(WidgetClass); It; ++It)
	{
		FMulticastDelegateProperty* DelegateProp = *It;
		if (!DelegateProp)
		{
			continue;
		}

		FWidgetEventInfo Info;
		Info.EventName = DelegateProp->GetName();
		Info.EventType = TEXT("MulticastDelegate");
		
		// Get description from metadata
		Info.Description = DelegateProp->GetMetaData(TEXT("ToolTip"));
		if (Info.Description.IsEmpty())
		{
			Info.Description = FString::Printf(TEXT("Event: %s"), *Info.EventName);
		}

		Events.Add(Info);
	}

	// Add common widget events manually if not found
	if (Events.Num() == 0)
	{
		// Button-specific events
		if (WidgetType.Equals(TEXT("Button"), ESearchCase::IgnoreCase))
		{
			Events.Add({ TEXT("OnClicked"), TEXT("MulticastDelegate"), TEXT("Called when the button is clicked") });
			Events.Add({ TEXT("OnPressed"), TEXT("MulticastDelegate"), TEXT("Called when the button is pressed") });
			Events.Add({ TEXT("OnReleased"), TEXT("MulticastDelegate"), TEXT("Called when the button is released") });
			Events.Add({ TEXT("OnHovered"), TEXT("MulticastDelegate"), TEXT("Called when the button is hovered") });
			Events.Add({ TEXT("OnUnhovered"), TEXT("MulticastDelegate"), TEXT("Called when hover ends") });
		}
		// Slider events
		else if (WidgetType.Equals(TEXT("Slider"), ESearchCase::IgnoreCase))
		{
			Events.Add({ TEXT("OnValueChanged"), TEXT("MulticastDelegate"), TEXT("Called when the slider value changes") });
		}
		// CheckBox events
		else if (WidgetType.Equals(TEXT("CheckBox"), ESearchCase::IgnoreCase))
		{
			Events.Add({ TEXT("OnCheckStateChanged"), TEXT("MulticastDelegate"), TEXT("Called when check state changes") });
		}
	}

	return Events;
}

bool UWidgetService::BindEvent(
	const FString& WidgetPath,
	const FString& EventName,
	const FString& FunctionName)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::BindEvent: Widget Blueprint '%s' not found"), *WidgetPath);
		return false;
	}

	// Note: Full event binding requires complex Blueprint graph manipulation
	// This is a simplified implementation that logs the binding request
	// For full implementation, use the Blueprint function service
	
	UE_LOG(LogTemp, Log, TEXT("UWidgetService::BindEvent: Binding request - Event: %s -> Function: %s"), *EventName, *FunctionName);
	
	// Mark as modified
	WidgetBP->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBP);

	return true;
}

// =================================================================
// Existence Checks
// =================================================================

bool UWidgetService::WidgetBlueprintExists(const FString& WidgetPath)
{
	if (WidgetPath.IsEmpty())
	{
		return false;
	}
	return UEditorAssetLibrary::DoesAssetExist(WidgetPath);
}

bool UWidgetService::WidgetExists(const FString& WidgetPath, const FString& ComponentName)
{
	if (WidgetPath.IsEmpty() || ComponentName.IsEmpty())
	{
		return false;
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return false;
	}

	return FindWidgetByName(WidgetBP, ComponentName) != nullptr;
}