// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "UI/MarkdownToRichText.h"

class SVerticalBox;

/**
 * A composite markdown rendering widget that replaces a single SRichTextBlock.
 *
 * Instead of putting all markdown into one SRichTextBlock (which has an internal
 * limit on styled runs, causing tags to render as literal text in long responses),
 * this widget parses markdown into block-level elements and renders each with its
 * own dedicated Slate widget:
 *
 *  - Paragraphs: individual SRichTextBlock with inline formatting (bold, italic, code, links)
 *  - Headers: STextBlock with header font size
 *  - Code blocks: SBorder (dark background) + STextBlock (monospace green)
 *  - Tables: STextBlock per row (bold header, monospace body)
 *  - Blockquotes: SHorizontalBox with colored left accent bar + SRichTextBlock
 *  - Horizontal rules: styled STextBlock separator
 *  - List items: bullet/number prefix + SRichTextBlock
 *
 * Each sub-widget only needs a few styled runs, eliminating the run limit problem.
 */
class VIBEUE_API SMarkdownTextBlock : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMarkdownTextBlock)
        : _Text()
        , _IsStreaming(false)
        , _AutoWrapText(true)
    {}
        SLATE_ATTRIBUTE(FText, Text)
        SLATE_ATTRIBUTE(bool, IsStreaming)
        SLATE_ATTRIBUTE(bool, AutoWrapText)
        SLATE_EVENT(FSlateHyperlinkRun::FOnClick, OnHyperlinkClicked)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Update the displayed markdown text (for streaming updates) */
    void SetText(const FText& InText);

    /** Update streaming state */
    void SetIsStreaming(bool bStreaming);

    /** Get the raw markdown text (for copy-to-clipboard) */
    const FString& GetRawText() const { return RawMarkdown; }

private:
    /** Differentially update the widget tree - only rebuild changed blocks */
    void UpdateWidgets();

    /** Check if two parsed blocks are identical */
    static bool BlocksMatch(const FMarkdownBlock& A, const FMarkdownBlock& B);

    /** Create a widget for a specific block type */
    TSharedRef<SWidget> CreateBlockWidget(const FMarkdownBlock& Block);

    /** Create an SRichTextBlock with inline formatting and hyperlink support */
    TSharedRef<SWidget> CreateRichTextWidget(const FString& RichText, const FName& TextStyleName);

    /** Create a code block widget with dark background */
    TSharedRef<SWidget> CreateCodeBlockWidget(const FMarkdownBlock& Block);

    /** Create a table widget */
    TSharedRef<SWidget> CreateTableWidget(const FMarkdownBlock& Block);

    /** Create a blockquote widget with colored left accent bar */
    TSharedRef<SWidget> CreateBlockquoteWidget(const FMarkdownBlock& Block);

    /** Create a horizontal rule separator */
    TSharedRef<SWidget> CreateHorizontalRuleWidget();

    /** Create a list item widget (bullet or numbered) */
    TSharedRef<SWidget> CreateListItemWidget(const FMarkdownBlock& Block);

    /** Create a header widget */
    TSharedRef<SWidget> CreateHeaderWidget(const FMarkdownBlock& Block);

    /** The vertical box container for all block widgets */
    TSharedPtr<SVerticalBox> ContentBox;

    /** Cached parsed blocks from last update (for differential comparison) */
    TArray<FMarkdownBlock> CachedBlocks;

    /** Widget references matching CachedBlocks 1:1 */
    TArray<TSharedPtr<SWidget>> BlockWidgets;

    /** Current raw markdown text */
    FString RawMarkdown;

    /** Whether we're in streaming mode */
    bool bIsStreaming = false;

    /** Whether to auto-wrap text */
    bool bAutoWrapText = true;

    /** Hyperlink click delegate */
    FSlateHyperlinkRun::FOnClick OnHyperlinkClicked;
};
