// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Block types produced by markdown parsing.
 * Each block maps to a distinct Slate widget in SMarkdownTextBlock.
 */
enum class EMarkdownBlockType : uint8
{
    Paragraph,
    Header,
    BulletItem,
    NumberedItem,
    CodeBlock,
    Table,
    Blockquote,
    HorizontalRule,
    EmptyLine
};

/**
 * A single parsed markdown block element.
 * Produced by FMarkdownToRichText::ParseBlocks and consumed by SMarkdownTextBlock.
 */
struct VIBEUE_API FMarkdownBlock
{
    EMarkdownBlockType Type = EMarkdownBlockType::Paragraph;
    FString Content;                        // Text content (for inline formatting)
    int32 Level = 0;                        // Header level (1-3) or blockquote depth
    int32 Number = 0;                       // Numbered list item number
    FString Language;                       // Code block language tag
    TArray<TArray<FString>> TableRows;      // Table cell data
    int32 TableSeparatorRow = -1;           // Index of separator row in table
    bool bIsStreaming = false;              // True for unclosed streaming code blocks
};

/**
 * Utility class to parse markdown text into structured blocks and rich text XML.
 * Handles streaming gracefully - incomplete markdown during typing won't break rendering.
 *
 * Supported markdown features:
 *  Headers (# through ######) with inline formatting preserved
 *  Bold, Italic, Bold+Italic
 *  Inline code and fenced code blocks with language tags
 *  Bullet lists (- or *) and numbered lists (1. 2. etc.)
 *  Tables (pipe-delimited with separator rows)
 *  Blockquotes (> text, nested >> text)
 *  Horizontal rules (three or more dashes, asterisks, or underscores)
 *  Links ([text](url))
 */
class VIBEUE_API FMarkdownToRichText
{
public:
    /**
     * Parse markdown into block-level elements for SMarkdownTextBlock.
     * Each block represents a distinct visual element (paragraph, header, code block, etc.)
     * @param MarkdownText - The raw markdown text to parse
     * @param bIsStreaming - If true, handle unclosed code blocks gracefully
     * @return Array of parsed markdown blocks
     */
    static TArray<FMarkdownBlock> ParseBlocks(const FString& MarkdownText, bool bIsStreaming = false);

    /**
     * Convert markdown text to rich text XML format for a single SRichTextBlock.
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

    /** Process inline formatting (bold, italic, code, links) within a line. Returns rich text XML. */
    static FString ProcessInlineFormatting(const FString& Line);

    /** Strip markdown formatting markers, returning plain text */
    static FString StripMarkdownFormatting(const FString& Text);

private:
    /** Get header level from line (0 if not a header, 1-3 for h1-h3) */
    static int32 GetHeaderLevel(const FString& Line, FString& OutHeaderText);

    /** Check if line is a bullet list item (- or *) */
    static bool IsBulletListItem(const FString& Line, FString& OutItemText);

    /** Check if line is a numbered list item (1. 2. etc) */
    static bool IsNumberedListItem(const FString& Line, FString& OutItemText, int32& OutNumber);

    /** Check if line is a horizontal rule (---, ***, ___) */
    static bool IsHorizontalRule(const FString& Line);

    /** Check if line is a blockquote (> text) and return the depth and content */
    static bool IsBlockquote(const FString& Line, FString& OutContent, int32& OutDepth);

    /** Check if line is a table row (|col1|col2|) */
    static bool IsTableRow(const FString& Line, TArray<FString>& OutCells);

    /** Check if line is a table separator (|---|---|) */
    static bool IsTableSeparator(const FString& Line);

    /** Format a table as aligned monospace text block (legacy, used by Convert) */
    static FString FormatTable(const TArray<TArray<FString>>& Rows, int32 SeparatorRowIndex);
};
