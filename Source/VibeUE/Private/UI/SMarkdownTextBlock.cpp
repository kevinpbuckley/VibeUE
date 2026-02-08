// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "UI/SMarkdownTextBlock.h"
#include "UI/MarkdownToRichText.h"
#include "UI/ChatRichTextStyles.h"
#include "Framework/Text/RichTextLayoutMarshaller.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Styling/AppStyle.h"

// Colors matching ChatRichTextStyles namespace
namespace MarkdownWidgetColors
{
    const FLinearColor CodeBackground(0.06f, 0.06f, 0.08f, 1.0f);
    const FLinearColor TextPrimary(0.78f, 0.78f, 0.82f, 1.0f);
    const FLinearColor TextMuted(0.38f, 0.38f, 0.42f, 1.0f);
    const FLinearColor BlockquoteAccent(0.3f, 0.5f, 0.9f, 0.6f);
    const FLinearColor HrColor(0.3f, 0.3f, 0.35f, 0.6f);
}

// Persistent brushes for SBorder usage (must outlive widgets)
namespace MarkdownBrushes
{
    static FSlateBrush BoxBrush;
    static FSlateBrush RoundedBoxBrush;
    static bool bInitialized = false;

    static void EnsureInitialized()
    {
        if (!bInitialized)
        {
            BoxBrush.DrawAs = ESlateBrushDrawType::Box;
            BoxBrush.TintColor = FSlateColor(FLinearColor::White);

            RoundedBoxBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
            RoundedBoxBrush.TintColor = FSlateColor(FLinearColor::White);

            bInitialized = true;
        }
    }
}

static TSharedRef<SWidget> CreatePlainTextWidget(const FString& Text, const FTextBlockStyle& TextStyle, bool bAutoWrapText)
{
    return SNew(SMultiLineEditableText)
        .Text(FText::FromString(Text))
        .TextStyle(&TextStyle)
        .AutoWrapText(bAutoWrapText)
        .IsReadOnly(true)
        .AllowContextMenu(true)
        .SelectAllTextWhenFocused(false)
        .ClearTextSelectionOnFocusLoss(true)
        .Margin(FMargin(0));
}

void SMarkdownTextBlock::Construct(const FArguments& InArgs)
{
    RawMarkdown = InArgs._Text.Get().ToString();
    bIsStreaming = InArgs._IsStreaming.Get();
    bAutoWrapText = InArgs._AutoWrapText.Get();
    OnHyperlinkClicked = InArgs._OnHyperlinkClicked;

    MarkdownBrushes::EnsureInitialized();

    ChildSlot
    [
        SAssignNew(ContentBox, SVerticalBox)
    ];

    UpdateWidgets();
}

void SMarkdownTextBlock::SetText(const FText& InText)
{
    FString NewText = InText.ToString();
    if (NewText != RawMarkdown)
    {
        RawMarkdown = NewText;
        UpdateWidgets();
    }
}

void SMarkdownTextBlock::SetIsStreaming(bool bStreaming)
{
    if (bIsStreaming != bStreaming)
    {
        bIsStreaming = bStreaming;
        UpdateWidgets();
    }
}

bool SMarkdownTextBlock::BlocksMatch(const FMarkdownBlock& A, const FMarkdownBlock& B)
{
    if (A.Type != B.Type) return false;
    if (A.Content != B.Content) return false;
    if (A.Level != B.Level) return false;
    if (A.Number != B.Number) return false;
    if (A.Language != B.Language) return false;
    if (A.bIsStreaming != B.bIsStreaming) return false;
    if (A.TableSeparatorRow != B.TableSeparatorRow) return false;
    if (A.TableRows.Num() != B.TableRows.Num()) return false;
    for (int32 r = 0; r < A.TableRows.Num(); r++)
    {
        if (A.TableRows[r] != B.TableRows[r]) return false;
    }
    return true;
}

void SMarkdownTextBlock::UpdateWidgets()
{
    TArray<FMarkdownBlock> NewBlocks = FMarkdownToRichText::ParseBlocks(RawMarkdown, bIsStreaming);

    // Safety: if tracking arrays are out of sync, force full rebuild
    if (BlockWidgets.Num() != CachedBlocks.Num())
    {
        ContentBox->ClearChildren();
        BlockWidgets.Empty();
        CachedBlocks.Empty();
    }

    // Find how far existing blocks still match the new blocks
    int32 FirstDiffIdx = 0;
    int32 MinCount = FMath::Min(CachedBlocks.Num(), NewBlocks.Num());
    for (int32 i = 0; i < MinCount; i++)
    {
        if (!BlocksMatch(CachedBlocks[i], NewBlocks[i]))
        {
            break;
        }
        FirstDiffIdx = i + 1;
    }

    // Remove stale widgets from FirstDiffIdx onwards
    while (BlockWidgets.Num() > FirstDiffIdx)
    {
        ContentBox->RemoveSlot(BlockWidgets.Last().ToSharedRef());
        BlockWidgets.Pop();
    }
    CachedBlocks.SetNum(FirstDiffIdx);

    // Add new/changed widgets
    for (int32 i = FirstDiffIdx; i < NewBlocks.Num(); i++)
    {
        TSharedRef<SWidget> Widget = CreateBlockWidget(NewBlocks[i]);
        BlockWidgets.Add(Widget);
        CachedBlocks.Add(NewBlocks[i]);
        ContentBox->AddSlot()
        .AutoHeight()
        [
            Widget
        ];
    }
}

TSharedRef<SWidget> SMarkdownTextBlock::CreateBlockWidget(const FMarkdownBlock& Block)
{
    switch (Block.Type)
    {
    case EMarkdownBlockType::Header:
        return CreateHeaderWidget(Block);

    case EMarkdownBlockType::CodeBlock:
        return CreateCodeBlockWidget(Block);

    case EMarkdownBlockType::Table:
        return CreateTableWidget(Block);

    case EMarkdownBlockType::Blockquote:
        return CreateBlockquoteWidget(Block);

    case EMarkdownBlockType::HorizontalRule:
        return CreateHorizontalRuleWidget();

    case EMarkdownBlockType::BulletItem:
    case EMarkdownBlockType::NumberedItem:
        return CreateListItemWidget(Block);

    case EMarkdownBlockType::EmptyLine:
        return SNew(SSpacer).Size(FVector2D(1, 6));

    case EMarkdownBlockType::Paragraph:
    default:
        return CreateRichTextWidget(
            FMarkdownToRichText::ProcessInlineFormatting(Block.Content),
            FChatRichTextStyles::Style_Default);
    }
}

TSharedRef<SWidget> SMarkdownTextBlock::CreateRichTextWidget(const FString& RichText, const FName& TextStyleName)
{
    // Each SRichTextBlock handles only ONE block's worth of inline formatting,
    // so we stay well under any styled-run limits.
    TArray<TSharedRef<ITextDecorator>> Decorators;
    Decorators.Add(SRichTextBlock::HyperlinkDecorator(TEXT("a"), OnHyperlinkClicked));

    TSharedRef<FRichTextLayoutMarshaller> Marshaller =
        FRichTextLayoutMarshaller::Create(MoveTemp(Decorators), &FChatRichTextStyles::Get());

    return SNew(SMultiLineEditableText)
        .Text(FText::FromString(RichText))
        .AutoWrapText(bAutoWrapText)
        .Marshaller(Marshaller)
        .TextStyle(&FChatRichTextStyles::Get(), TextStyleName)
        .IsReadOnly(true)
        .AllowContextMenu(true)
        .SelectAllTextWhenFocused(false)
        .ClearTextSelectionOnFocusLoss(true)
        .Margin(FMargin(0));
}

TSharedRef<SWidget> SMarkdownTextBlock::CreateHeaderWidget(const FMarkdownBlock& Block)
{
    // Headers use a larger bold font. Strip markdown formatting from header text.
    FString CleanText = FMarkdownToRichText::StripMarkdownFormatting(Block.Content);

    FName HeaderStyleName = FChatRichTextStyles::Style_H1;
    if (Block.Level == 2) HeaderStyleName = FChatRichTextStyles::Style_H2;
    else if (Block.Level >= 3) HeaderStyleName = FChatRichTextStyles::Style_H3;

    const FTextBlockStyle& HeaderStyle = FChatRichTextStyles::Get().GetWidgetStyle<FTextBlockStyle>(HeaderStyleName);

    return SNew(SBox)
    .Padding(FMargin(0, 24, 0, 4))
    [
        CreatePlainTextWidget(CleanText, HeaderStyle, bAutoWrapText)
    ];
}

TSharedRef<SWidget> SMarkdownTextBlock::CreateCodeBlockWidget(const FMarkdownBlock& Block)
{
    TSharedRef<SVerticalBox> CodeBox = SNew(SVerticalBox);

    // Language label above the code block
    if (!Block.Language.IsEmpty())
    {
        CodeBox->AddSlot()
        .AutoHeight()
        .Padding(FMargin(4, 4, 0, 2))
        [
            SNew(STextBlock)
            .Text(FText::FromString(Block.Language))
            .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
            .ColorAndOpacity(FSlateColor(MarkdownWidgetColors::TextMuted))
        ];
    }

    // Code content with dark background
    CodeBox->AddSlot()
    .AutoHeight()
    [
        SNew(SBorder)
        .BorderImage(&MarkdownBrushes::BoxBrush)
        .BorderBackgroundColor(MarkdownWidgetColors::CodeBackground)
        .Padding(FMargin(10, 8))
        [
            CreatePlainTextWidget(
                Block.Content,
                FChatRichTextStyles::Get().GetWidgetStyle<FTextBlockStyle>(FChatRichTextStyles::Style_CodeBlock),
                true)
        ]
    ];

    // Streaming indicator
    if (Block.bIsStreaming)
    {
        CodeBox->AddSlot()
        .AutoHeight()
        .Padding(FMargin(4, 2, 0, 0))
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("\u2026")))
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
            .ColorAndOpacity(FSlateColor(MarkdownWidgetColors::TextMuted))
        ];
    }

    return CodeBox;
}

TSharedRef<SWidget> SMarkdownTextBlock::CreateTableWidget(const FMarkdownBlock& Block)
{
    if (Block.TableRows.Num() == 0)
    {
        return SNew(SSpacer).Size(FVector2D(1, 1));
    }

    // Calculate max column widths
    int32 NumCols = 0;
    for (const auto& Row : Block.TableRows)
    {
        NumCols = FMath::Max(NumCols, Row.Num());
    }

    TArray<int32> ColWidths;
    ColWidths.SetNumZeroed(NumCols);
    for (const auto& Row : Block.TableRows)
    {
        for (int32 Col = 0; Col < Row.Num(); Col++)
        {
            ColWidths[Col] = FMath::Max(ColWidths[Col], Row[Col].Len());
        }
    }
    for (int32& Width : ColWidths)
    {
        Width = FMath::Max(Width, 3);
    }

    TSharedRef<SVerticalBox> TableBox = SNew(SVerticalBox);

    for (int32 RowIdx = 0; RowIdx < Block.TableRows.Num(); RowIdx++)
    {
        // Separator row: render as a dashed line
        if (RowIdx == Block.TableSeparatorRow)
        {
            FString SepLine;
            for (int32 Col = 0; Col < NumCols; Col++)
            {
                if (Col > 0) SepLine += TEXT("   ");
                for (int32 d = 0; d < ColWidths[Col]; d++)
                {
                    SepLine += TEXT("\u2500");
                }
            }

            TableBox->AddSlot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text(FText::FromString(SepLine))
                .Font(FCoreStyle::GetDefaultFontStyle("Mono", 11))
                .ColorAndOpacity(FSlateColor(MarkdownWidgetColors::TextMuted))
            ];
            continue;
        }

        const auto& Row = Block.TableRows[RowIdx];
        bool bIsHeader = (Block.TableSeparatorRow > 0 && RowIdx < Block.TableSeparatorRow);

        // Build padded row string
        FString FormattedRow;
        for (int32 Col = 0; Col < NumCols; Col++)
        {
            FString CellText = (Col < Row.Num()) ? Row[Col] : FString();
            while (CellText.Len() < ColWidths[Col])
            {
                CellText += TEXT(" ");
            }
            if (Col > 0)
            {
                FormattedRow += TEXT("   ");
            }
            FormattedRow += CellText;
        }

        if (bIsHeader)
        {
            TableBox->AddSlot()
            .AutoHeight()
            [
                CreatePlainTextWidget(
                    FormattedRow,
                    FChatRichTextStyles::Get().GetWidgetStyle<FTextBlockStyle>(FChatRichTextStyles::Style_TableHeader),
                    false)
            ];
        }
        else
        {
            TableBox->AddSlot()
            .AutoHeight()
            [
                CreatePlainTextWidget(
                    FormattedRow,
                    FChatRichTextStyles::Get().GetWidgetStyle<FTextBlockStyle>(FChatRichTextStyles::Style_Table),
                    false)
            ];
        }
    }

    return SNew(SBox)
    .Padding(FMargin(0, 4, 0, 4))
    [
        TableBox
    ];
}

TSharedRef<SWidget> SMarkdownTextBlock::CreateBlockquoteWidget(const FMarkdownBlock& Block)
{
    // Build accent bar(s) for the blockquote depth
    TSharedRef<SHorizontalBox> BarBox = SNew(SHorizontalBox);

    for (int32 d = 0; d < Block.Level; d++)
    {
        BarBox->AddSlot()
        .AutoWidth()
        .Padding(d > 0 ? FMargin(3, 0, 0, 0) : FMargin(0))
        [
            SNew(SBorder)
            .BorderImage(&MarkdownBrushes::BoxBrush)
            .BorderBackgroundColor(MarkdownWidgetColors::BlockquoteAccent)
            .Padding(FMargin(2, 0, 0, 0))
            [
                SNew(SSpacer).Size(FVector2D(0, 0))
            ]
        ];
    }

    // Process inline formatting for blockquote content
    FString InlineFormatted = FMarkdownToRichText::ProcessInlineFormatting(Block.Content);

    return SNew(SHorizontalBox)
    // Accent bars
    + SHorizontalBox::Slot()
    .AutoWidth()
    .Padding(FMargin(0, 0, 6, 0))
    [
        BarBox
    ]
    // Content with blockquote text style
    + SHorizontalBox::Slot()
    .FillWidth(1.0f)
    [
        CreateRichTextWidget(InlineFormatted, FChatRichTextStyles::Style_Blockquote)
    ];
}

TSharedRef<SWidget> SMarkdownTextBlock::CreateHorizontalRuleWidget()
{
    return SNew(SBox)
    .Padding(FMargin(0, 6, 0, 6))
    [
        SNew(STextBlock)
        .Text(FText::FromString(TEXT("\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500")))
        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 6))
        .ColorAndOpacity(FSlateColor(MarkdownWidgetColors::HrColor))
    ];
}

TSharedRef<SWidget> SMarkdownTextBlock::CreateListItemWidget(const FMarkdownBlock& Block)
{
    // Bullet prefix or number prefix
    FString Prefix;
    if (Block.Type == EMarkdownBlockType::BulletItem)
    {
        Prefix = TEXT("\u2022 ");
    }
    else
    {
        Prefix = FString::Printf(TEXT("%d. "), Block.Number);
    }

    FString InlineFormatted = FMarkdownToRichText::ProcessInlineFormatting(Block.Content);

    return SNew(SHorizontalBox)
    // Bullet/number prefix
    + SHorizontalBox::Slot()
    .AutoWidth()
    .Padding(FMargin(4, 0, 0, 0))
    [
        SNew(STextBlock)
        .Text(FText::FromString(Prefix))
        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
        .ColorAndOpacity(FSlateColor(MarkdownWidgetColors::TextPrimary))
    ]
    // Item content with inline formatting
    + SHorizontalBox::Slot()
    .FillWidth(1.0f)
    [
        CreateRichTextWidget(InlineFormatted, FChatRichTextStyles::Style_Default)
    ];
}
