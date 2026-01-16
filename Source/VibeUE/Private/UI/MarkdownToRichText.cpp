// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "UI/MarkdownToRichText.h"
#include "Internationalization/Regex.h"

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

    // Handle AI-generated XML-style tags that got escaped
    // Convert &lt;bold&gt;text&lt;/bold&gt; or &lt;bold&gt;text&lt;/&gt; back to <bold>text</>
    Result = Result.Replace(TEXT("&lt;bold&gt;"), TEXT("<bold>"));
    Result = Result.Replace(TEXT("&lt;/bold&gt;"), TEXT("</>"));
    Result = Result.Replace(TEXT("&lt;italic&gt;"), TEXT("<italic>"));
    Result = Result.Replace(TEXT("&lt;/italic&gt;"), TEXT("</>"));
    Result = Result.Replace(TEXT("&lt;code&gt;"), TEXT("<code>"));
    Result = Result.Replace(TEXT("&lt;/code&gt;"), TEXT("</>"));
    Result = Result.Replace(TEXT("&lt;/&gt;"), TEXT("</>"));  // Generic close tag

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
            // Content already escaped - just wrap in tag
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
            // Content already escaped - just wrap in tag
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
            // Content already escaped - just wrap in tag
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
            // Content already escaped - just wrap in tag
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
            // Content already escaped - just wrap in tag
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
            // Link text already escaped, URL needs quote escaping for attribute
            FString Replacement = FString::Printf(TEXT("<a id=\"%s\" style=\"link\">%s</>"), *LinkUrl.Replace(TEXT("\""), TEXT("&quot;")), *LinkText);
            Replacements.Add(TPair<FString, FString>(FullMatch, Replacement));
        }

        for (const auto& Rep : Replacements)
        {
            Result = Result.Replace(*Rep.Key, *Rep.Value);
        }
    }

    return Result;
}

FString FMarkdownToRichText::StripMarkdownFormatting(const FString& Text)
{
    FString Result = Text;
    
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

    if (TrimmedLine.StartsWith(TEXT("- ")) || TrimmedLine.StartsWith(TEXT("* ")))
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

    for (int32 i = 0; i < Lines.Num(); i++)
    {
        const FString& Line = Lines[i];

        // Check for code block markers (```)
        if (Line.TrimStart().StartsWith(TEXT("```")))
        {
            if (!bInCodeBlock)
            {
                // Starting code block
                bInCodeBlock = true;
                CodeBlockLanguage = Line.TrimStart().Mid(3).TrimStartAndEnd();
                CodeBlockContent.Empty();
            }
            else
            {
                // Ending code block
                bInCodeBlock = false;
                // Output the code block
                if (!CodeBlockContent.IsEmpty())
                {
                    // Remove trailing newline if present
                    if (CodeBlockContent.EndsWith(TEXT("\n")))
                    {
                        CodeBlockContent = CodeBlockContent.LeftChop(1);
                    }
                    Result += FString::Printf(TEXT("<codeblock>%s</>\n"), *EscapeXML(CodeBlockContent));
                }
                CodeBlockContent.Empty();
            }
            continue;
        }

        // Inside code block - accumulate content
        if (bInCodeBlock)
        {
            CodeBlockContent += Line + TEXT("\n");
            continue;
        }

        // Check for headers
        // NOTE: SRichTextBlock does NOT support nested tags!
        // Headers are already styled (h1/h2/h3), so we strip any inline formatting
        // from header text to avoid nesting issues.
        FString HeaderText;
        int32 HeaderLevel = GetHeaderLevel(Line, HeaderText);
        if (HeaderLevel > 0)
        {
            FString StyleName = FString::Printf(TEXT("h%d"), HeaderLevel);
            // Strip markdown formatting from header text (headers already have styling)
            FString CleanHeaderText = StripMarkdownFormatting(HeaderText);
            // Add extra line before headers for spacing
            Result += FString::Printf(TEXT("\n<%s>%s</>\n"), *StyleName, *EscapeXML(CleanHeaderText));
            continue;
        }

        // Check for bullet list items
        // NOTE: SRichTextBlock does NOT support nested tags! 
        // We cannot use <listitem><bold>text</></> - the inner tags become literal text.
        // So we apply inline formatting directly without wrapping in listitem tags.
        FString ItemText;
        if (IsBulletListItem(Line, ItemText))
        {
            // Use bullet character with inline formatting (no outer listitem tag)
            Result += FString::Printf(TEXT("• %s\n"), *ProcessInlineFormatting(ItemText));
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

    // Handle unclosed code block (streaming case)
    if (bInCodeBlock && !CodeBlockContent.IsEmpty())
    {
        if (bIsStreaming)
        {
            // Show partial code block with indicator
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
