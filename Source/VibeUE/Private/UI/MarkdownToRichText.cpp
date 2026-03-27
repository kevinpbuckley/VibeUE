// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "UI/MarkdownToRichText.h"
#include "Internationalization/Regex.h"

static FString SplitNestedTagsInRun(const FString& Content, const FString& OuterTag)
{
    FString Result = Content;
    FRegexPattern Pattern(TEXT("<([\\w\\d\\.-]+)(?: (?:[\\w\\d\\.-]+=(?>\".*?\")))+?>.*?</>"));
    FRegexMatcher Matcher(Pattern, Result);

    TArray<TPair<FString, FString>> Replacements;
    while (Matcher.FindNext())
    {
        FString FullMatch = Matcher.GetCaptureGroup(0);
        FString Replacement = FString::Printf(TEXT("</><%s>%s</><%s>"), *OuterTag, *FullMatch, *OuterTag);
        Replacements.Add(TPair<FString, FString>(FullMatch, Replacement));
    }

    for (const auto& Rep : Replacements)
    {
        Result = Result.Replace(*Rep.Key, *Rep.Value);
    }

    return Result;
}

static FString FlattenNestedRuns(const FString& Input, const FString& OuterTag)
{
    FString Result = Input;
    const FString PatternString = FString::Printf(TEXT("<%s>(.*?)</>"), *OuterTag);
    FRegexPattern Pattern(PatternString);
    FRegexMatcher Matcher(Pattern, Result);

    TArray<TPair<FString, FString>> Replacements;
    while (Matcher.FindNext())
    {
        FString FullMatch = Matcher.GetCaptureGroup(0);
        FString Content = Matcher.GetCaptureGroup(1);
        FString Flattened = SplitNestedTagsInRun(Content, OuterTag);
        FString Replacement = FString::Printf(TEXT("<%s>%s</>"), *OuterTag, *Flattened);
        Replacements.Add(TPair<FString, FString>(FullMatch, Replacement));
    }

    for (const auto& Rep : Replacements)
    {
        Result = Result.Replace(*Rep.Key, *Rep.Value);
    }

    return Result;
}

static FString SplitNestedPair(const FString& Input, const FString& OuterTag, const FString& InnerTag, const FString& MixedTag)
{
    FString Result = Input;
    const FString PatternString = FString::Printf(
        TEXT("<%s>([\\s\\S]*?)<%s>([\\s\\S]*?)</>([\\s\\S]*?)</>"),
        *OuterTag,
        *InnerTag);
    FRegexPattern Pattern(PatternString);

    bool bMatched = true;
    while (bMatched)
    {
        bMatched = false;
        FRegexMatcher Matcher(Pattern, Result);
        TArray<TPair<FString, FString>> Replacements;

        while (Matcher.FindNext())
        {
            bMatched = true;
            FString FullMatch = Matcher.GetCaptureGroup(0);
            FString Prefix = Matcher.GetCaptureGroup(1);
            FString Inner = Matcher.GetCaptureGroup(2);
            FString Suffix = Matcher.GetCaptureGroup(3);

            FString Replacement = FString::Printf(
                TEXT("<%s>%s</><%s>%s</><%s>%s</>"),
                *OuterTag, *Prefix,
                *MixedTag, *Inner,
                *OuterTag, *Suffix);
            Replacements.Add(TPair<FString, FString>(FullMatch, Replacement));
        }

        for (const auto& Rep : Replacements)
        {
            Result = Result.Replace(*Rep.Key, *Rep.Value);
        }
    }

    return Result;
}

FString FMarkdownToRichText::EscapeXML(const FString& Text)
{
    FString Result = Text;
    // Order matters: escape & first since other escapes contain &
    Result = Result.Replace(TEXT("&"), TEXT("&amp;"));
    Result = Result.Replace(TEXT("<"), TEXT("&lt;"));
    Result = Result.Replace(TEXT(">"), TEXT("&gt;"));
    Result = Result.Replace(TEXT("\""), TEXT("&quot;"));
    return Result;
}

FString FMarkdownToRichText::ProcessInlineFormatting(const FString& Line)
{
    // FIRST: Escape all XML special characters in the entire line
    // This ensures any literal <, >, & in the text won't break XML parsing
    // Markdown markers (*, _, `, #, -, [, ]) are NOT affected by XML escaping
    FString Result = EscapeXML(Line);

    // Handle double-escaped AI tags like &amp;lt;bold&amp;gt; that should become rich text tags
    Result = Result.Replace(TEXT("&amp;lt;bold&amp;gt;"), TEXT("<bold>"));
    Result = Result.Replace(TEXT("&amp;lt;/bold&amp;gt;"), TEXT("</>"));
    Result = Result.Replace(TEXT("&amp;lt;italic&amp;gt;"), TEXT("<italic>"));
    Result = Result.Replace(TEXT("&amp;lt;/italic&amp;gt;"), TEXT("</>"));
    Result = Result.Replace(TEXT("&amp;lt;code&amp;gt;"), TEXT("<code>"));
    Result = Result.Replace(TEXT("&amp;lt;/code&amp;gt;"), TEXT("</>"));
    Result = Result.Replace(TEXT("&amp;lt;/&amp;gt;"), TEXT("</>"));
    Result = Result.Replace(TEXT("&amp;lt;a "), TEXT("<a "));
    Result = Result.Replace(TEXT("&amp;lt;a&gt;"), TEXT("<a>"));

    // Handle AI-generated XML-style tags that got escaped
    // Convert &lt;bold&gt;text&lt;/bold&gt; or &lt;bold&gt;text&lt;/&gt; back to <bold>text</>
    Result = Result.Replace(TEXT("&lt;bold&gt;"), TEXT("<bold>"));
    Result = Result.Replace(TEXT("&lt;/bold&gt;"), TEXT("</>"));
    Result = Result.Replace(TEXT("&lt;italic&gt;"), TEXT("<italic>"));
    Result = Result.Replace(TEXT("&lt;/italic&gt;"), TEXT("</>"));
    Result = Result.Replace(TEXT("&lt;code&gt;"), TEXT("<code>"));
    Result = Result.Replace(TEXT("&lt;/code&gt;"), TEXT("</>"));
    Result = Result.Replace(TEXT("&lt;/&gt;"), TEXT("</>"));  // Generic close tag
    Result = Result.Replace(TEXT("&lt;a "), TEXT("<a "));
    Result = Result.Replace(TEXT("&lt;a&gt;"), TEXT("<a>"));

    // Normalize raw AI-generated <a id="url"> tags into our expected hyperlink format
    {
        FRegexPattern Pattern(TEXT("<a id=\"([^\"]+)\""));
        FRegexMatcher Matcher(Pattern, Result);

        TArray<TPair<FString, FString>> Replacements;
        while (Matcher.FindNext())
        {
            FString FullMatch = Matcher.GetCaptureGroup(0);
            FString Url = Matcher.GetCaptureGroup(1);
            if (!FullMatch.Contains(TEXT("href=")))
            {
                FString Replacement = FString::Printf(TEXT("<a id=\"link\" href=\"%s\" style=\"Hyperlink\" textstyle=\"link\""), *Url);
                Replacements.Add(TPair<FString, FString>(FullMatch, Replacement));
            }
        }

        for (const auto& Rep : Replacements)
        {
            Result = Result.Replace(*Rep.Key, *Rep.Value);
        }
    }

    // Process inline code first (to protect code content from other formatting)
    // Match `code` - single backticks
    // Note: Content is already escaped, don't double-escape
    {
        FRegexPattern Pattern(TEXT("`([^`]+)`"));
        FRegexMatcher Matcher(Pattern, Result);

        TArray<TPair<FString, FString>> Replacements;
        while (Matcher.FindNext())
        {
            FString FullMatch = Matcher.GetCaptureGroup(0);
            FString CodeContent = Matcher.GetCaptureGroup(1);
            // Content already escaped - just wrap in tag
            FString Replacement = FString::Printf(TEXT("<code>%s</>"), *CodeContent);
            Replacements.Add(TPair<FString, FString>(FullMatch, Replacement));
        }

        for (const auto& Rep : Replacements)
        {
            Result = Result.Replace(*Rep.Key, *Rep.Value);
        }
    }

    // Bold+Italic: ***text*** (must be before bold and italic)
    {
        FRegexPattern Pattern(TEXT("\\*\\*\\*([^*]+)\\*\\*\\*"));
        FRegexMatcher Matcher(Pattern, Result);

        TArray<TPair<FString, FString>> Replacements;
        while (Matcher.FindNext())
        {
            FString FullMatch = Matcher.GetCaptureGroup(0);
            FString Content = Matcher.GetCaptureGroup(1);
            FString Replacement = FString::Printf(TEXT("<bolditalic>%s</>"), *Content);
            Replacements.Add(TPair<FString, FString>(FullMatch, Replacement));
        }

        for (const auto& Rep : Replacements)
        {
            Result = Result.Replace(*Rep.Key, *Rep.Value);
        }
    }

    // Bold: **text** or __text__
    {
        FRegexPattern Pattern(TEXT("\\*\\*([^*]+)\\*\\*"));
        FRegexMatcher Matcher(Pattern, Result);

        TArray<TPair<FString, FString>> Replacements;
        while (Matcher.FindNext())
        {
            FString FullMatch = Matcher.GetCaptureGroup(0);
            FString Content = Matcher.GetCaptureGroup(1);
            FString Replacement = FString::Printf(TEXT("<bold>%s</>"), *Content);
            Replacements.Add(TPair<FString, FString>(FullMatch, Replacement));
        }

        for (const auto& Rep : Replacements)
        {
            Result = Result.Replace(*Rep.Key, *Rep.Value);
        }
    }

    // Bold with underscores: __text__
    {
        FRegexPattern Pattern(TEXT("__([^_]+)__"));
        FRegexMatcher Matcher(Pattern, Result);

        TArray<TPair<FString, FString>> Replacements;
        while (Matcher.FindNext())
        {
            FString FullMatch = Matcher.GetCaptureGroup(0);
            FString Content = Matcher.GetCaptureGroup(1);
            FString Replacement = FString::Printf(TEXT("<bold>%s</>"), *Content);
            Replacements.Add(TPair<FString, FString>(FullMatch, Replacement));
        }

        for (const auto& Rep : Replacements)
        {
            Result = Result.Replace(*Rep.Key, *Rep.Value);
        }
    }

    // Italic: *text* (but not **)
    {
        FRegexPattern Pattern(TEXT("(?<!\\*)\\*([^*]+)\\*(?!\\*)"));
        FRegexMatcher Matcher(Pattern, Result);

        TArray<TPair<FString, FString>> Replacements;
        while (Matcher.FindNext())
        {
            FString FullMatch = Matcher.GetCaptureGroup(0);
            FString Content = Matcher.GetCaptureGroup(1);
            FString Replacement = FString::Printf(TEXT("<italic>%s</>"), *Content);
            Replacements.Add(TPair<FString, FString>(FullMatch, Replacement));
        }

        for (const auto& Rep : Replacements)
        {
            Result = Result.Replace(*Rep.Key, *Rep.Value);
        }
    }

    // Italic with underscores: _text_ (not inside words)
    {
        FRegexPattern Pattern(TEXT("(?<![\\w])_([^_]+)_(?![\\w])"));
        FRegexMatcher Matcher(Pattern, Result);

        TArray<TPair<FString, FString>> Replacements;
        while (Matcher.FindNext())
        {
            FString FullMatch = Matcher.GetCaptureGroup(0);
            FString Content = Matcher.GetCaptureGroup(1);
            FString Replacement = FString::Printf(TEXT("<italic>%s</>"), *Content);
            Replacements.Add(TPair<FString, FString>(FullMatch, Replacement));
        }

        for (const auto& Rep : Replacements)
        {
            Result = Result.Replace(*Rep.Key, *Rep.Value);
        }
    }

    // Links: [text](url)
    {
        FRegexPattern Pattern(TEXT("\\[([^\\]]+)\\]\\(([^)]+)\\)"));
        FRegexMatcher Matcher(Pattern, Result);

        TArray<TPair<FString, FString>> Replacements;
        while (Matcher.FindNext())
        {
            FString FullMatch = Matcher.GetCaptureGroup(0);
            FString LinkText = Matcher.GetCaptureGroup(1);
            FString LinkUrl = Matcher.GetCaptureGroup(2);
            FString Replacement = FString::Printf(TEXT("<a id=\"link\" href=\"%s\" style=\"Hyperlink\" textstyle=\"link\">%s</>"), *LinkUrl.Replace(TEXT("\""), TEXT("&quot;")), *LinkText);
            Replacements.Add(TPair<FString, FString>(FullMatch, Replacement));
        }

        for (const auto& Rep : Replacements)
        {
            Result = Result.Replace(*Rep.Key, *Rep.Value);
        }
    }

    Result = SplitNestedPair(Result, TEXT("italic"), TEXT("bold"), TEXT("bolditalic"));
    Result = SplitNestedPair(Result, TEXT("bold"), TEXT("italic"), TEXT("bolditalic"));
    Result = FlattenNestedRuns(Result, TEXT("italic"));
    Result = FlattenNestedRuns(Result, TEXT("bold"));
    Result = FlattenNestedRuns(Result, TEXT("bolditalic"));

    return Result;
}

FString FMarkdownToRichText::StripMarkdownFormatting(const FString& Text)
{
    FString Result = Text;
    
    // Remove bold+italic markers: ***text*** -> text (must be first)
    {
        FRegexPattern Pattern(TEXT("\\*\\*\\*([^*]+)\\*\\*\\*"));
        FRegexMatcher Matcher(Pattern, Result);
        TArray<TPair<FString, FString>> Replacements;
        while (Matcher.FindNext())
        {
            Replacements.Add(TPair<FString, FString>(Matcher.GetCaptureGroup(0), Matcher.GetCaptureGroup(1)));
        }
        for (const auto& Rep : Replacements)
        {
            Result = Result.Replace(*Rep.Key, *Rep.Value);
        }
    }
    
    // Remove bold markers: **text** -> text
    {
        FRegexPattern Pattern(TEXT("\\*\\*([^*]+)\\*\\*"));
        FRegexMatcher Matcher(Pattern, Result);
        TArray<TPair<FString, FString>> Replacements;
        while (Matcher.FindNext())
        {
            Replacements.Add(TPair<FString, FString>(Matcher.GetCaptureGroup(0), Matcher.GetCaptureGroup(1)));
        }
        for (const auto& Rep : Replacements)
        {
            Result = Result.Replace(*Rep.Key, *Rep.Value);
        }
    }
    
    // Remove underscore bold: __text__ -> text
    {
        FRegexPattern Pattern(TEXT("__([^_]+)__"));
        FRegexMatcher Matcher(Pattern, Result);
        TArray<TPair<FString, FString>> Replacements;
        while (Matcher.FindNext())
        {
            Replacements.Add(TPair<FString, FString>(Matcher.GetCaptureGroup(0), Matcher.GetCaptureGroup(1)));
        }
        for (const auto& Rep : Replacements)
        {
            Result = Result.Replace(*Rep.Key, *Rep.Value);
        }
    }
    
    // Remove italic markers: *text* -> text (but not **)
    {
        FRegexPattern Pattern(TEXT("(?<!\\*)\\*([^*]+)\\*(?!\\*)"));
        FRegexMatcher Matcher(Pattern, Result);
        TArray<TPair<FString, FString>> Replacements;
        while (Matcher.FindNext())
        {
            Replacements.Add(TPair<FString, FString>(Matcher.GetCaptureGroup(0), Matcher.GetCaptureGroup(1)));
        }
        for (const auto& Rep : Replacements)
        {
            Result = Result.Replace(*Rep.Key, *Rep.Value);
        }
    }
    
    // Remove underscore italic: _text_ -> text
    {
        FRegexPattern Pattern(TEXT("(?<![\\w])_([^_]+)_(?![\\w])"));
        FRegexMatcher Matcher(Pattern, Result);
        TArray<TPair<FString, FString>> Replacements;
        while (Matcher.FindNext())
        {
            Replacements.Add(TPair<FString, FString>(Matcher.GetCaptureGroup(0), Matcher.GetCaptureGroup(1)));
        }
        for (const auto& Rep : Replacements)
        {
            Result = Result.Replace(*Rep.Key, *Rep.Value);
        }
    }
    
    // Remove inline code: `code` -> code
    {
        FRegexPattern Pattern(TEXT("`([^`]+)`"));
        FRegexMatcher Matcher(Pattern, Result);
        TArray<TPair<FString, FString>> Replacements;
        while (Matcher.FindNext())
        {
            Replacements.Add(TPair<FString, FString>(Matcher.GetCaptureGroup(0), Matcher.GetCaptureGroup(1)));
        }
        for (const auto& Rep : Replacements)
        {
            Result = Result.Replace(*Rep.Key, *Rep.Value);
        }
    }
    
    return Result;
}

int32 FMarkdownToRichText::GetHeaderLevel(const FString& Line, FString& OutHeaderText)
{
    FString TrimmedLine = Line.TrimStart();

    // Check h4-h6 first (most specific) - treat as h3
    if (TrimmedLine.StartsWith(TEXT("###### ")))
    {
        OutHeaderText = TrimmedLine.Mid(7).TrimStartAndEnd();
        return 3;
    }
    if (TrimmedLine.StartsWith(TEXT("##### ")))
    {
        OutHeaderText = TrimmedLine.Mid(6).TrimStartAndEnd();
        return 3;
    }
    if (TrimmedLine.StartsWith(TEXT("#### ")))
    {
        OutHeaderText = TrimmedLine.Mid(5).TrimStartAndEnd();
        return 3;
    }
    if (TrimmedLine.StartsWith(TEXT("### ")))
    {
        OutHeaderText = TrimmedLine.Mid(4).TrimStartAndEnd();
        return 3;
    }
    if (TrimmedLine.StartsWith(TEXT("## ")))
    {
        OutHeaderText = TrimmedLine.Mid(3).TrimStartAndEnd();
        return 2;
    }
    if (TrimmedLine.StartsWith(TEXT("# ")))
    {
        OutHeaderText = TrimmedLine.Mid(2).TrimStartAndEnd();
        return 1;
    }

    return 0;
}

bool FMarkdownToRichText::IsBulletListItem(const FString& Line, FString& OutItemText)
{
    FString TrimmedLine = Line.TrimStart();

    // Make sure "---", "***" aren't treated as bullet items
    if (TrimmedLine.StartsWith(TEXT("- ")) && !IsHorizontalRule(Line))
    {
        OutItemText = TrimmedLine.Mid(2);
        return true;
    }
    if (TrimmedLine.StartsWith(TEXT("* ")) && !IsHorizontalRule(Line))
    {
        OutItemText = TrimmedLine.Mid(2);
        return true;
    }

    return false;
}

bool FMarkdownToRichText::IsNumberedListItem(const FString& Line, FString& OutItemText, int32& OutNumber)
{
    FString TrimmedLine = Line.TrimStart();

    // Match pattern like "1. " or "12. "
    FRegexPattern Pattern(TEXT("^(\\d+)\\.\\s+(.*)$"));
    FRegexMatcher Matcher(Pattern, TrimmedLine);

    if (Matcher.FindNext())
    {
        OutNumber = FCString::Atoi(*Matcher.GetCaptureGroup(1));
        OutItemText = Matcher.GetCaptureGroup(2);
        return true;
    }

    return false;
}

bool FMarkdownToRichText::IsHorizontalRule(const FString& Line)
{
    FString TrimmedLine = Line.TrimStartAndEnd();
    
    // Standard markdown horizontal rules: three or more of -, *, or _
    // May have spaces between them
    
    // Remove all spaces for checking
    FString Compact = TrimmedLine.Replace(TEXT(" "), TEXT(""));
    
    if (Compact.Len() < 3)
    {
        return false;
    }
    
    // Check if all characters are the same dash/star/underscore
    TCHAR FirstChar = Compact[0];
    if (FirstChar != TEXT('-') && FirstChar != TEXT('*') && FirstChar != TEXT('_'))
    {
        return false;
    }
    
    for (int32 i = 1; i < Compact.Len(); i++)
    {
        if (Compact[i] != FirstChar)
        {
            return false;
        }
    }
    
    return true;
}

bool FMarkdownToRichText::IsBlockquote(const FString& Line, FString& OutContent, int32& OutDepth)
{
    FString TrimmedLine = Line.TrimStart();
    OutDepth = 0;
    
    // Count leading > characters (with optional spaces between them)
    int32 Index = 0;
    while (Index < TrimmedLine.Len())
    {
        if (TrimmedLine[Index] == TEXT('>'))
        {
            OutDepth++;
            Index++;
            // Skip optional space after >
            if (Index < TrimmedLine.Len() && TrimmedLine[Index] == TEXT(' '))
            {
                Index++;
            }
        }
        else
        {
            break;
        }
    }
    
    if (OutDepth > 0)
    {
        OutContent = TrimmedLine.Mid(Index);
        return true;
    }
    
    return false;
}

bool FMarkdownToRichText::IsTableRow(const FString& Line, TArray<FString>& OutCells)
{
    FString TrimmedLine = Line.TrimStartAndEnd();
    
    // Table rows must start and end with |
    if (!TrimmedLine.StartsWith(TEXT("|")))
    {
        return false;
    }
    
    // Split by | and trim each cell
    OutCells.Empty();
    
    // Remove leading and trailing |
    FString Inner = TrimmedLine;
    if (Inner.StartsWith(TEXT("|")))
    {
        Inner = Inner.Mid(1);
    }
    if (Inner.EndsWith(TEXT("|")))
    {
        Inner = Inner.LeftChop(1);
    }
    
    // Split by |
    TArray<FString> RawCells;
    Inner.ParseIntoArray(RawCells, TEXT("|"), false);
    
    for (const FString& Cell : RawCells)
    {
        OutCells.Add(Cell.TrimStartAndEnd());
    }
    
    return OutCells.Num() > 0;
}

bool FMarkdownToRichText::IsTableSeparator(const FString& Line)
{
    TArray<FString> Cells;
    if (!IsTableRow(Line, Cells))
    {
        return false;
    }
    
    // Each cell in the separator row should be only dashes (with optional colons for alignment)
    // Patterns: ---, :---, ---:, :---:
    FRegexPattern Pattern(TEXT("^:?-{1,}:?$"));
    
    for (const FString& Cell : Cells)
    {
        if (Cell.IsEmpty())
        {
            continue;
        }
        
        FRegexMatcher Matcher(Pattern, Cell);
        if (!Matcher.FindNext())
        {
            return false;
        }
    }
    
    return true;
}

FString FMarkdownToRichText::FormatTable(const TArray<TArray<FString>>& Rows, int32 SeparatorRowIndex)
{
    if (Rows.Num() == 0)
    {
        return FString();
    }
    
    // Calculate max column widths
    int32 NumCols = 0;
    for (const auto& Row : Rows)
    {
        NumCols = FMath::Max(NumCols, Row.Num());
    }
    
    TArray<int32> ColWidths;
    ColWidths.SetNumZeroed(NumCols);
    
    for (const auto& Row : Rows)
    {
        for (int32 Col = 0; Col < Row.Num(); Col++)
        {
            ColWidths[Col] = FMath::Max(ColWidths[Col], Row[Col].Len());
        }
    }
    
    // Ensure minimum column width of 3
    for (int32& Width : ColWidths)
    {
        Width = FMath::Max(Width, 3);
    }
    
    FString Result;
    
    for (int32 RowIdx = 0; RowIdx < Rows.Num(); RowIdx++)
    {
        // Skip the separator row (we render our own visual separator)
        if (RowIdx == SeparatorRowIndex)
        {
            // Render separator as a dashed line - NO tag to save run budget
            FString SepLine;
            for (int32 Col = 0; Col < NumCols; Col++)
            {
                if (Col > 0) SepLine += TEXT("   ");
                for (int32 d = 0; d < ColWidths[Col]; d++)
                {
                    SepLine += TEXT("\u2500"); // Box-drawing horizontal line
                }
            }
            Result += EscapeXML(SepLine) + TEXT("\n");
            continue;
        }
        
        const auto& Row = Rows[RowIdx];
        bool bIsHeader = (SeparatorRowIndex > 0 && RowIdx < SeparatorRowIndex);
        
        // Build padded row
        FString FormattedRow;
        for (int32 Col = 0; Col < NumCols; Col++)
        {
            FString CellText = (Col < Row.Num()) ? Row[Col] : FString();
            
            // Right-pad to column width
            while (CellText.Len() < ColWidths[Col])
            {
                CellText += TEXT(" ");
            }
            
            if (Col > 0)
            {
                FormattedRow += TEXT("   "); // Column spacing
            }
            FormattedRow += CellText;
        }
        
        if (bIsHeader)
        {
            // Use bold tag for header rows - one of our core supported tags
            Result += FString::Printf(TEXT("<bold>%s</>\n"), *EscapeXML(FormattedRow));
        }
        else
        {
            // Plain text for body rows - no tag needed, saves run budget
            Result += EscapeXML(FormattedRow) + TEXT("\n");
        }
    }
    
    return Result;
}

FString FMarkdownToRichText::Convert(const FString& MarkdownText, bool bIsStreaming)
{
    if (MarkdownText.IsEmpty())
    {
        return FString();
    }

    FString Result;
    TArray<FString> Lines;
    MarkdownText.ParseIntoArrayLines(Lines);

    bool bInCodeBlock = false;
    FString CodeBlockContent;
    FString CodeBlockLanguage;
    
    // Table accumulation state
    bool bInTable = false;
    TArray<TArray<FString>> TableRows;
    int32 TableSeparatorRow = -1;

    for (int32 i = 0; i < Lines.Num(); i++)
    {
        const FString& Line = Lines[i];

        // Check for code block markers (```)
        if (Line.TrimStart().StartsWith(TEXT("```")))
        {
            if (!bInCodeBlock)
            {
                // Flush any pending table before starting code block
                if (bInTable)
                {
                    Result += FormatTable(TableRows, TableSeparatorRow);
                    TableRows.Empty();
                    TableSeparatorRow = -1;
                    bInTable = false;
                }
                
                // Starting code block
                bInCodeBlock = true;
                CodeBlockLanguage = Line.TrimStart().Mid(3).TrimStartAndEnd();
                CodeBlockContent.Empty();
            }
            else
            {
                // Ending code block
                bInCodeBlock = false;
                // Output the code block with optional language label
                if (!CodeBlockContent.IsEmpty())
                {
                    // Remove trailing newline if present
                    if (CodeBlockContent.EndsWith(TEXT("\n")))
                    {
                        CodeBlockContent = CodeBlockContent.LeftChop(1);
                    }
                    // Language label as plain text (no tag - saves run budget)
                    if (!CodeBlockLanguage.IsEmpty())
                    {
                        Result += FString::Printf(TEXT("  %s\n"), *EscapeXML(CodeBlockLanguage));
                    }
                    // Code content
                    Result += FString::Printf(TEXT("<codeblock>%s</>\n"), *EscapeXML(CodeBlockContent));
                }
                CodeBlockContent.Empty();
                CodeBlockLanguage.Empty();
            }
            continue;
        }

        // Inside code block - accumulate content
        if (bInCodeBlock)
        {
            CodeBlockContent += Line + TEXT("\n");
            continue;
        }

        // Check for horizontal rule BEFORE bullet list items (--- would match "- --")
        if (IsHorizontalRule(Line))
        {
            // Flush any pending table
            if (bInTable)
            {
                Result += FormatTable(TableRows, TableSeparatorRow);
                TableRows.Empty();
                TableSeparatorRow = -1;
                bInTable = false;
            }
            
            // Render horizontal rule as plain Unicode line (no tag - saves run budget)
            Result += TEXT("\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n");
            continue;
        }

        // Check for table rows
        TArray<FString> TableCells;
        if (IsTableRow(Line, TableCells))
        {
            // Flush any pending table if we're not currently in one
            // (this handles consecutive tables separated by non-table content)
            
            if (IsTableSeparator(Line))
            {
                if (bInTable)
                {
                    TableSeparatorRow = TableRows.Num();
                    TableRows.Add(TableCells);
                }
                // Separator outside table context - ignore
                continue;
            }
            
            bInTable = true;
            TableRows.Add(TableCells);
            continue;
        }
        else if (bInTable)
        {
            // Non-table line encountered - flush the accumulated table
            Result += FormatTable(TableRows, TableSeparatorRow);
            TableRows.Empty();
            TableSeparatorRow = -1;
            bInTable = false;
            // Fall through to process this line normally
        }

        // Check for blockquotes
        FString BlockquoteContent;
        int32 BlockquoteDepth;
        if (IsBlockquote(Line, BlockquoteContent, BlockquoteDepth))
        {
            // Build indent prefix with plain pipe characters (no tag - saves run budget)
            FString Prefix;
            for (int32 d = 0; d < BlockquoteDepth; d++)
            {
                Prefix += TEXT("\u2502 ");
            }
            
            // Process inline formatting within blockquote content
            // NOTE: Do NOT wrap in <blockquote> tag - SRichTextBlock does not support nested tags,
            // so <blockquote><bold>text</></> would render <bold> as literal text.
            // The accent bar prefix already provides visual distinction.
            FString FormattedContent = ProcessInlineFormatting(BlockquoteContent);
            Result += FString::Printf(TEXT("%s%s\n"), *Prefix, *FormattedContent);
            continue;
        }

        // Check for headers
        // FIX: Process inline formatting WITHIN headers instead of stripping it.
        // SRichTextBlock doesn't support nested tags, so we apply ProcessInlineFormatting
        // and then wrap in the header style. Since SRichTextBlock can't nest,
        // we preserve the raw header text with inline formatting applied.
        FString HeaderText;
        int32 HeaderLevel = GetHeaderLevel(Line, HeaderText);
        if (HeaderLevel > 0)
        {
            FString StyleName = FString::Printf(TEXT("h%d"), HeaderLevel);
            // Use ProcessInlineFormatting to handle bold/italic/code within headers
            // Note: SRichTextBlock doesn't nest styles well, so the header style
            // applies to the entire line, but inline formatting markers are cleaned up
            FString CleanHeaderText = StripMarkdownFormatting(HeaderText);
            // Add extra line before headers for spacing
            Result += FString::Printf(TEXT("\n<%s>%s</>\n"), *StyleName, *EscapeXML(CleanHeaderText));
            continue;
        }

        // Check for bullet list items
        FString ItemText;
        if (IsBulletListItem(Line, ItemText))
        {
            // Use bullet character with inline formatting (no outer listitem tag)
            Result += FString::Printf(TEXT("\u2022 %s\n"), *ProcessInlineFormatting(ItemText));
            continue;
        }

        // Check for numbered list items
        int32 ItemNumber;
        if (IsNumberedListItem(Line, ItemText, ItemNumber))
        {
            // Use number with inline formatting (no outer listitem tag)
            Result += FString::Printf(TEXT("%d. %s\n"), ItemNumber, *ProcessInlineFormatting(ItemText));
            continue;
        }

        // Regular paragraph line
        if (Line.IsEmpty())
        {
            // Empty line = paragraph break, add extra spacing
            Result += TEXT("\n\n");
        }
        else
        {
            Result += ProcessInlineFormatting(Line) + TEXT("\n");
        }
    }

    // Flush any pending table at end of input
    if (bInTable)
    {
        Result += FormatTable(TableRows, TableSeparatorRow);
    }

    // Handle unclosed code block (streaming case)
    if (bInCodeBlock && !CodeBlockContent.IsEmpty())
    {
        // Language label as plain text (no tag)
        if (!CodeBlockLanguage.IsEmpty())
        {
            Result += FString::Printf(TEXT("  %s\n"), *EscapeXML(CodeBlockLanguage));
        }
        
        if (bIsStreaming)
        {
            // Show partial code block with streaming indicator
            Result += FString::Printf(TEXT("<codeblock>%s...</>\n"), *EscapeXML(CodeBlockContent));
        }
        else
        {
            // Non-streaming: render as-is
            Result += FString::Printf(TEXT("<codeblock>%s</>\n"), *EscapeXML(CodeBlockContent));
        }
    }

    // Remove trailing newline for cleaner display
    if (Result.EndsWith(TEXT("\n")))
    {
        Result = Result.LeftChop(1);
    }

    return Result;
}

TArray<FMarkdownBlock> FMarkdownToRichText::ParseBlocks(const FString& MarkdownText, bool bIsStreaming)
{
    TArray<FMarkdownBlock> Blocks;

    if (MarkdownText.IsEmpty())
    {
        return Blocks;
    }

    TArray<FString> Lines;
    MarkdownText.ParseIntoArrayLines(Lines);

    bool bInCodeBlock = false;
    FString CodeBlockContent;
    FString CodeBlockLanguage;

    // Table accumulation state
    bool bInTable = false;
    TArray<TArray<FString>> TableRows;
    int32 TableSeparatorRow = -1;

    for (int32 i = 0; i < Lines.Num(); i++)
    {
        const FString& Line = Lines[i];

        // Check for code block markers (```)
        if (Line.TrimStart().StartsWith(TEXT("```")))
        {
            if (!bInCodeBlock)
            {
                // Flush any pending table
                if (bInTable)
                {
                    FMarkdownBlock TableBlock;
                    TableBlock.Type = EMarkdownBlockType::Table;
                    TableBlock.TableRows = TableRows;
                    TableBlock.TableSeparatorRow = TableSeparatorRow;
                    Blocks.Add(TableBlock);
                    TableRows.Empty();
                    TableSeparatorRow = -1;
                    bInTable = false;
                }

                bInCodeBlock = true;
                CodeBlockLanguage = Line.TrimStart().Mid(3).TrimStartAndEnd();
                CodeBlockContent.Empty();
            }
            else
            {
                // Ending code block
                bInCodeBlock = false;
                if (!CodeBlockContent.IsEmpty())
                {
                    if (CodeBlockContent.EndsWith(TEXT("\n")))
                    {
                        CodeBlockContent = CodeBlockContent.LeftChop(1);
                    }

                    FMarkdownBlock Block;
                    Block.Type = EMarkdownBlockType::CodeBlock;
                    Block.Content = CodeBlockContent;
                    Block.Language = CodeBlockLanguage;
                    Blocks.Add(Block);
                }
                CodeBlockContent.Empty();
                CodeBlockLanguage.Empty();
            }
            continue;
        }

        // Inside code block - accumulate content
        if (bInCodeBlock)
        {
            CodeBlockContent += Line + TEXT("\n");
            continue;
        }

        // Check for horizontal rule BEFORE bullet list items
        if (IsHorizontalRule(Line))
        {
            if (bInTable)
            {
                FMarkdownBlock TableBlock;
                TableBlock.Type = EMarkdownBlockType::Table;
                TableBlock.TableRows = TableRows;
                TableBlock.TableSeparatorRow = TableSeparatorRow;
                Blocks.Add(TableBlock);
                TableRows.Empty();
                TableSeparatorRow = -1;
                bInTable = false;
            }

            FMarkdownBlock Block;
            Block.Type = EMarkdownBlockType::HorizontalRule;
            Blocks.Add(Block);
            continue;
        }

        // Check for table rows
        TArray<FString> TableCells;
        if (IsTableRow(Line, TableCells))
        {
            if (IsTableSeparator(Line))
            {
                if (bInTable)
                {
                    TableSeparatorRow = TableRows.Num();
                    TableRows.Add(TableCells);
                }
                continue;
            }

            bInTable = true;
            TableRows.Add(TableCells);
            continue;
        }
        else if (bInTable)
        {
            // Flush accumulated table
            FMarkdownBlock TableBlock;
            TableBlock.Type = EMarkdownBlockType::Table;
            TableBlock.TableRows = TableRows;
            TableBlock.TableSeparatorRow = TableSeparatorRow;
            Blocks.Add(TableBlock);
            TableRows.Empty();
            TableSeparatorRow = -1;
            bInTable = false;
            // Fall through to process this line normally
        }

        // Check for blockquotes
        FString BlockquoteContent;
        int32 BlockquoteDepth;
        if (IsBlockquote(Line, BlockquoteContent, BlockquoteDepth))
        {
            FMarkdownBlock Block;
            Block.Type = EMarkdownBlockType::Blockquote;
            Block.Content = BlockquoteContent;
            Block.Level = BlockquoteDepth;
            Blocks.Add(Block);
            continue;
        }

        // Check for headers
        FString HeaderText;
        int32 HeaderLevel = GetHeaderLevel(Line, HeaderText);
        if (HeaderLevel > 0)
        {
            FMarkdownBlock Block;
            Block.Type = EMarkdownBlockType::Header;
            Block.Content = HeaderText;
            Block.Level = HeaderLevel;
            Blocks.Add(Block);
            continue;
        }

        // Check for bullet list items
        FString ItemText;
        if (IsBulletListItem(Line, ItemText))
        {
            FMarkdownBlock Block;
            Block.Type = EMarkdownBlockType::BulletItem;
            Block.Content = ItemText;
            Blocks.Add(Block);
            continue;
        }

        // Check for numbered list items
        int32 ItemNumber;
        if (IsNumberedListItem(Line, ItemText, ItemNumber))
        {
            FMarkdownBlock Block;
            Block.Type = EMarkdownBlockType::NumberedItem;
            Block.Content = ItemText;
            Block.Number = ItemNumber;
            Blocks.Add(Block);
            continue;
        }

        // Empty line
        if (Line.IsEmpty())
        {
            FMarkdownBlock Block;
            Block.Type = EMarkdownBlockType::EmptyLine;
            Blocks.Add(Block);
            continue;
        }

        // Regular paragraph
        FMarkdownBlock Block;
        Block.Type = EMarkdownBlockType::Paragraph;
        Block.Content = Line;
        Blocks.Add(Block);
    }

    // Flush any pending table
    if (bInTable)
    {
        FMarkdownBlock TableBlock;
        TableBlock.Type = EMarkdownBlockType::Table;
        TableBlock.TableRows = TableRows;
        TableBlock.TableSeparatorRow = TableSeparatorRow;
        Blocks.Add(TableBlock);
    }

    // Handle unclosed code block (streaming case)
    if (bInCodeBlock && !CodeBlockContent.IsEmpty())
    {
        if (CodeBlockContent.EndsWith(TEXT("\n")))
        {
            CodeBlockContent = CodeBlockContent.LeftChop(1);
        }

        FMarkdownBlock Block;
        Block.Type = EMarkdownBlockType::CodeBlock;
        Block.Content = CodeBlockContent;
        Block.Language = CodeBlockLanguage;
        Block.bIsStreaming = bIsStreaming;
        Blocks.Add(Block);
    }

    return Blocks;
}
