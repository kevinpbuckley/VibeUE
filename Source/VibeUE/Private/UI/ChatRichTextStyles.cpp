// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "UI/ChatRichTextStyles.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleRegistry.h"

// Style name constants
const FName FChatRichTextStyles::Style_Default = TEXT("default");
const FName FChatRichTextStyles::Style_Bold = TEXT("bold");
const FName FChatRichTextStyles::Style_Italic = TEXT("italic");
const FName FChatRichTextStyles::Style_BoldItalic = TEXT("bolditalic");
const FName FChatRichTextStyles::Style_Code = TEXT("code");
const FName FChatRichTextStyles::Style_CodeBlock = TEXT("codeblock");
const FName FChatRichTextStyles::Style_H1 = TEXT("h1");
const FName FChatRichTextStyles::Style_H2 = TEXT("h2");
const FName FChatRichTextStyles::Style_H3 = TEXT("h3");
const FName FChatRichTextStyles::Style_ListItem = TEXT("listitem");
const FName FChatRichTextStyles::Style_Link = TEXT("link");

TSharedPtr<FSlateStyleSet> FChatRichTextStyles::StyleSet = nullptr;

// VibeUE Brand Colors (matching SAIChatWindow.cpp)
namespace ChatColors
{
    const FLinearColor TextPrimary(0.78f, 0.78f, 0.82f, 1.0f);     // Main text - soft gray
    const FLinearColor TextCode(0.72f, 0.82f, 0.72f, 1.0f);        // Code text - slight green tint
    const FLinearColor Cyan(0.0f, 0.9f, 0.9f, 1.0f);               // Links - cyan accent
}

void FChatRichTextStyles::Initialize()
{
    if (!StyleSet.IsValid())
    {
        StyleSet = Create();
        FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
    }
}

void FChatRichTextStyles::Shutdown()
{
    if (StyleSet.IsValid())
    {
        FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
        StyleSet.Reset();
    }
}

const ISlateStyle& FChatRichTextStyles::Get()
{
    if (!StyleSet.IsValid())
    {
        Initialize();
    }
    return *StyleSet;
}

FName FChatRichTextStyles::GetStyleSetName()
{
    static FName StyleSetName(TEXT("ChatRichTextStyles"));
    return StyleSetName;
}

TSharedRef<FSlateStyleSet> FChatRichTextStyles::Create()
{
    TSharedRef<FSlateStyleSet> NewStyleSet = MakeShareable(new FSlateStyleSet(GetStyleSetName()));

    // Base font settings - size 12 for readability
    const FSlateFontInfo RegularFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);
    const FSlateFontInfo BoldFont = FCoreStyle::GetDefaultFontStyle("Bold", 12);
    const FSlateFontInfo ItalicFont = FCoreStyle::GetDefaultFontStyle("Italic", 12);
    const FSlateFontInfo BoldItalicFont = FCoreStyle::GetDefaultFontStyle("Bold", 12); // Fallback if BoldItalic unavailable
    const FSlateFontInfo MonoFont = FCoreStyle::GetDefaultFontStyle("Mono", 11);

    // Default text style
    FTextBlockStyle DefaultStyle;
    DefaultStyle.SetFont(RegularFont);
    DefaultStyle.SetColorAndOpacity(FSlateColor(ChatColors::TextPrimary));
    NewStyleSet->Set(Style_Default, DefaultStyle);

    // Bold
    FTextBlockStyle BoldStyle = DefaultStyle;
    BoldStyle.SetFont(BoldFont);
    NewStyleSet->Set(Style_Bold, BoldStyle);

    // Italic
    FTextBlockStyle ItalicStyle = DefaultStyle;
    ItalicStyle.SetFont(ItalicFont);
    NewStyleSet->Set(Style_Italic, ItalicStyle);

    // Bold + Italic (using bold as fallback)
    FTextBlockStyle BoldItalicStyle = DefaultStyle;
    BoldItalicStyle.SetFont(BoldItalicFont);
    NewStyleSet->Set(Style_BoldItalic, BoldItalicStyle);

    // Inline code - monospace with green tint
    FTextBlockStyle CodeStyle = DefaultStyle;
    CodeStyle.SetFont(MonoFont);
    CodeStyle.SetColorAndOpacity(FSlateColor(ChatColors::TextCode));
    NewStyleSet->Set(Style_Code, CodeStyle);

    // Code block - same as inline code
    FTextBlockStyle CodeBlockStyle = CodeStyle;
    NewStyleSet->Set(Style_CodeBlock, CodeBlockStyle);

    // Header 1
    FTextBlockStyle H1Style = DefaultStyle;
    H1Style.SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 18));
    NewStyleSet->Set(Style_H1, H1Style);

    // Header 2
    FTextBlockStyle H2Style = DefaultStyle;
    H2Style.SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 15));
    NewStyleSet->Set(Style_H2, H2Style);

    // Header 3
    FTextBlockStyle H3Style = DefaultStyle;
    H3Style.SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 13));
    NewStyleSet->Set(Style_H3, H3Style);

    // List item - same as default
    NewStyleSet->Set(Style_ListItem, DefaultStyle);

    // Link style - cyan color
    FTextBlockStyle LinkStyle = DefaultStyle;
    LinkStyle.SetColorAndOpacity(FSlateColor(ChatColors::Cyan));
    NewStyleSet->Set(Style_Link, LinkStyle);

    return NewStyleSet;
}
