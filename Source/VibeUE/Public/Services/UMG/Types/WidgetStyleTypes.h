#pragma once

#include "CoreMinimal.h"
#include "Math/Color.h"
#include "Layout/Margin.h"
#include "Fonts/SlateFontInfo.h"
#include "Types/SlateEnums.h"

/**
 * UMG Widget Style Type Definitions
 * 
 * This header contains data structures related to UMG widget styling.
 */

/**
 * @struct FVibeWidgetStyle
 * @brief Structure defining a complete widget style
 * 
 * Contains style information including colors, fonts, padding, and alignment
 * that can be applied to UMG widgets.
 */
struct VIBEUE_API FVibeWidgetStyle
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

	FVibeWidgetStyle()
		: PrimaryColor(FLinearColor::White)
		, SecondaryColor(FLinearColor::Gray)
		, Padding(0.0f)
		, HorizontalAlignment(HAlign_Fill)
		, VerticalAlignment(VAlign_Fill)
	{}
};
