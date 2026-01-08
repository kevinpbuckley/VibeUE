// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UWidgetService.generated.h"

/**
 * Information about a widget in a Widget Blueprint
 */
USTRUCT(BlueprintType)
struct FWidgetInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString WidgetName;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString WidgetClass;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	FString ParentWidget;

	UPROPERTY(BlueprintReadWrite, Category = "Widget")
	bool bIsRootWidget = false;
};

/**
 * Widget service exposed directly to Python.
 *
 * Python Usage:
 *   import unreal
 *
 *   # List all Widget Blueprints
 *   widgets = unreal.WidgetService.list_widget_blueprints()
 *
 *   # Get widget hierarchy
 *   hierarchy = unreal.WidgetService.get_hierarchy("/Game/UI/WBP_MainMenu")
 *
 * @note This replaces the JSON-based manage_widget MCP tool
 */
UCLASS(BlueprintType)
class VIBEUE_API UWidgetService : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * List all Widget Blueprint assets.
	 *
	 * @param PathFilter - Optional path filter
	 * @return Array of Widget Blueprint paths
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets")
	static TArray<FString> ListWidgetBlueprints(const FString& PathFilter = TEXT("/Game"));

	/**
	 * Get widget hierarchy for a Widget Blueprint.
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @return Array of widget information in hierarchy order
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets")
	static TArray<FWidgetInfo> GetHierarchy(const FString& WidgetPath);

	/**
	 * Get the root widget of a Widget Blueprint.
	 *
	 * @param WidgetPath - Full path to the Widget Blueprint
	 * @return Name of the root widget, or empty if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets")
	static FString GetRootWidget(const FString& WidgetPath);
};
