// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "PythonAPI/UWidgetService.h"
#include "WidgetBlueprint.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"

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

	UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(WidgetPath));
	if (!WidgetBP)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::GetHierarchy: Failed to load Widget Blueprint: %s"), *WidgetPath);
		return Hierarchy;
	}

	UWidgetTree* WidgetTree = WidgetBP->WidgetTree;
	if (!WidgetTree)
	{
		return Hierarchy;
	}

	UWidget* RootWidget = WidgetTree->RootWidget;
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

		Hierarchy.Add(Info);

		// If it's a panel widget, recurse into children
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
	UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(WidgetPath));
	if (!WidgetBP || !WidgetBP->WidgetTree || !WidgetBP->WidgetTree->RootWidget)
	{
		return FString();
	}

	return WidgetBP->WidgetTree->RootWidget->GetName();
}
