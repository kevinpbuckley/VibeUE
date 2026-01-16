// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Utility class to convert markdown text to Slate RichText XML format.
 * Handles streaming gracefully - incomplete markdown during typing won't break rendering.
 */
class VIBEUE_API FMarkdownToRichText
{
public:
    /**
     * Convert markdown text to rich text XML format for SRichTextBlock.
     * @param MarkdownText - The raw markdown text to convert
     * @param bIsStreaming - If true, be more lenient with incomplete markdown
     * @return The converted rich text string with XML-style tags
     */
    static FString Convert(const FString& MarkdownText, bool bIsStreaming = false);

    /**
     * Escape XML special characters to prevent parsing issues.
     * @param Text - Raw text that may contain < > & " characters
     * @return Text with XML entities escaped
     */
    static FString EscapeXML(const FString& Text);

private:
    /** Process inline formatting (bold, italic, code, links) within a line */
    static FString ProcessInlineFormatting(const FString& Line);

    /** Strip markdown formatting markers, returning plain text (for use in contexts where nesting is not supported) */
    static FString StripMarkdownFormatting(const FString& Text);

    /** Get header level from line (0 if not a header, 1-3 for h1-h3) */
    static int32 GetHeaderLevel(const FString& Line, FString& OutHeaderText);

    /** Check if line is a bullet list item (- or *) */
    static bool IsBulletListItem(const FString& Line, FString& OutItemText);

    /** Check if line is a numbered list item (1. 2. etc) */
    static bool IsNumberedListItem(const FString& Line, FString& OutItemText, int32& OutNumber);
};
