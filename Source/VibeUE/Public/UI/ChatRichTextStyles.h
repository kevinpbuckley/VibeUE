// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/**
 * Manages rich text styles for the AI Chat window.
 * Creates and maintains an FSlateStyleSet with styles for markdown elements.
 */
class VIBEUE_API FChatRichTextStyles
{
public:
    /** Initialize the style set - call once at module startup */
    static void Initialize();

    /** Shutdown and release styles - call at module shutdown */
    static void Shutdown();

    /** Get the style set for use with SRichTextBlock */
    static const ISlateStyle& Get();

    /** Get the style set name */
    static FName GetStyleSetName();

    // Style name constants
    static const FName Style_Default;
    static const FName Style_Bold;
    static const FName Style_Italic;
    static const FName Style_BoldItalic;
    static const FName Style_Code;
    static const FName Style_CodeBlock;
    static const FName Style_H1;
    static const FName Style_H2;
    static const FName Style_H3;
    static const FName Style_ListItem;
    static const FName Style_Link;

private:
    /** Create the style set with all text styles */
    static TSharedRef<FSlateStyleSet> Create();

    /** The actual style set instance */
    static TSharedPtr<FSlateStyleSet> StyleSet;
};
