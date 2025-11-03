#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"
#include "Math/Color.h"
#include "Layout/Margin.h"
#include "Fonts/SlateFontInfo.h"
#include "Types/SlateEnums.h"

// Forward declarations
class UWidgetBlueprint;
class UWidget;

/**
 * @struct FWidgetStyle
 * @brief Structure defining a complete widget style
 * 
 * Contains style information including colors, fonts, padding, and alignment
 * that can be applied to UMG widgets.
 */
struct VIBEUE_API FWidgetStyle
{
	/** Primary color for the widget */
	FLinearColor PrimaryColor;
	
	/** Secondary/background color */
	FLinearColor SecondaryColor;
	
	/** Font information */
	FSlateFontInfo Font;
	
	/** Padding around the widget */
	FMargin Padding;
	
	/** Horizontal alignment */
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;
	
	/** Vertical alignment */
	TEnumAsByte<EVerticalAlignment> VerticalAlignment;

	FWidgetStyle()
		: PrimaryColor(FLinearColor::White)
		, SecondaryColor(FLinearColor::Gray)
		, Padding(0.0f)
		, HorizontalAlignment(HAlign_Fill)
		, VerticalAlignment(VAlign_Fill)
	{}
};

/**
 * @class FWidgetStyleService
 * @brief Service responsible for UMG widget styling operations
 * 
 * This service provides focused widget styling functionality extracted from
 * UMGCommands.cpp. It handles applying colors, fonts, padding, and alignment
 * to UMG widgets, and provides modern UI color palettes and style presets.
 * 
 * All methods return TResult<T> for type-safe error handling, avoiding the need
 * for runtime JSON parsing in the service layer.
 * 
 * @note This is part of Phase 3 refactoring (Task 14) to extract UMG styling
 * operations into a focused service as per CPP_REFACTORING_DESIGN.md
 * 
 * @see TResult
 * @see FServiceBase
 * @see Issue #35
 */
class VIBEUE_API FWidgetStyleService : public FServiceBase
{
public:
	/**
	 * @brief Constructor
	 * @param Context Service context for shared state
	 */
	explicit FWidgetStyleService(TSharedPtr<FServiceContext> Context);

	// ========================================
	// Style Application
	// ========================================

	/**
	 * @brief Apply a complete style to a widget component
	 * 
	 * @param Widget Widget blueprint containing the component
	 * @param ComponentName Name of the component to style
	 * @param Style Style configuration to apply
	 * @return TResult indicating success or error details
	 */
	TResult<void> ApplyStyle(UWidgetBlueprint* Widget, const FString& ComponentName, const FWidgetStyle& Style);

	/**
	 * @brief Apply a predefined style set to a widget component
	 * 
	 * @param Widget Widget blueprint containing the component
	 * @param ComponentName Name of the component to style
	 * @param StyleSetName Name of the style set (e.g., "Modern", "Material", "Minimal")
	 * @return TResult indicating success or error details
	 */
	TResult<void> ApplyStyleSet(UWidgetBlueprint* Widget, const FString& ComponentName, const FString& StyleSetName);

	// ========================================
	// Color Management
	// ========================================

	/**
	 * @brief Set the color of a widget component
	 * 
	 * @param Widget Widget blueprint containing the component
	 * @param ComponentName Name of the component
	 * @param Color Color to apply
	 * @return TResult indicating success or error details
	 */
	TResult<void> SetColor(UWidgetBlueprint* Widget, const FString& ComponentName, const FLinearColor& Color);

	/**
	 * @brief Set both color and opacity of a widget component
	 * 
	 * @param Widget Widget blueprint containing the component
	 * @param ComponentName Name of the component
	 * @param Color Color with opacity to apply
	 * @return TResult indicating success or error details
	 */
	TResult<void> SetColorAndOpacity(UWidgetBlueprint* Widget, const FString& ComponentName, const FLinearColor& Color);

	// ========================================
	// Font Management
	// ========================================

	/**
	 * @brief Set the font of a text widget component
	 * 
	 * @param Widget Widget blueprint containing the component
	 * @param ComponentName Name of the text component
	 * @param Font Font configuration to apply
	 * @return TResult indicating success or error details
	 */
	TResult<void> SetFont(UWidgetBlueprint* Widget, const FString& ComponentName, const FSlateFontInfo& Font);

	/**
	 * @brief Set the font size of a text widget component
	 * 
	 * @param Widget Widget blueprint containing the component
	 * @param ComponentName Name of the text component
	 * @param Size Font size in points
	 * @return TResult indicating success or error details
	 */
	TResult<void> SetFontSize(UWidgetBlueprint* Widget, const FString& ComponentName, int32 Size);

	// ========================================
	// Layout
	// ========================================

	/**
	 * @brief Set padding for a widget component
	 * 
	 * @param Widget Widget blueprint containing the component
	 * @param ComponentName Name of the component
	 * @param Padding Padding values (left, top, right, bottom)
	 * @return TResult indicating success or error details
	 */
	TResult<void> SetPadding(UWidgetBlueprint* Widget, const FString& ComponentName, const FMargin& Padding);

	/**
	 * @brief Set alignment for a widget component
	 * 
	 * @param Widget Widget blueprint containing the component
	 * @param ComponentName Name of the component
	 * @param HAlign Horizontal alignment
	 * @param VAlign Vertical alignment
	 * @return TResult indicating success or error details
	 */
	TResult<void> SetAlignment(UWidgetBlueprint* Widget, const FString& ComponentName,
	                          EHorizontalAlignment HAlign, EVerticalAlignment VAlign);

	// ========================================
	// Style Presets
	// ========================================

	/**
	 * @brief Get list of available predefined style sets
	 * 
	 * @return TResult containing array of style set names
	 */
	TResult<TArray<FString>> GetAvailableStyleSets();

	/**
	 * @brief Get a predefined style set configuration
	 * 
	 * @param StyleSetName Name of the style set to retrieve
	 * @return TResult containing the style configuration
	 */
	TResult<FWidgetStyle> GetStyleSet(const FString& StyleSetName);

protected:
	/**
	 * @brief Gets the service name for logging
	 * @return The service name
	 */
	virtual FString GetServiceName() const override { return TEXT("WidgetStyleService"); }

private:
	/**
	 * @brief Find a widget component by name in a widget blueprint
	 * 
	 * @param Widget Widget blueprint to search
	 * @param ComponentName Name of the component
	 * @return Pointer to the widget component or nullptr if not found
	 */
	UWidget* FindWidgetComponent(UWidgetBlueprint* Widget, const FString& ComponentName);

	/**
	 * @brief Initialize predefined style sets
	 */
	void InitializeStyleSets();

	/** Map of predefined style sets */
	TMap<FString, FWidgetStyle> StyleSets;
};
