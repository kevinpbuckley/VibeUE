// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "UI/SVibeUETaskList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

// Colors matching VibeUEColors from SAIChatWindow.cpp
namespace TaskListColors
{
    const FLinearColor BackgroundCard(0.10f, 0.10f, 0.14f, 1.0f);
    const FLinearColor TextPrimary(0.78f, 0.78f, 0.82f, 1.0f);
    const FLinearColor TextSecondary(0.55f, 0.55f, 0.60f, 1.0f);
    const FLinearColor TextMuted(0.38f, 0.38f, 0.42f, 1.0f);
    const FLinearColor Orange(0.95f, 0.6f, 0.15f, 1.0f);
    const FLinearColor Green(0.2f, 0.8f, 0.4f, 1.0f);
}

// Braille spinner frames for in-progress animation
static const TCHAR* SpinnerFrames[] = {
    TEXT("\u280B"), TEXT("\u2819"), TEXT("\u2839"), TEXT("\u2838"),
    TEXT("\u283C"), TEXT("\u2834"), TEXT("\u2826"), TEXT("\u2827"),
    TEXT("\u2807"), TEXT("\u280F")
};
static constexpr int32 NumSpinnerFrames = UE_ARRAY_COUNT(SpinnerFrames);

void SVibeUETaskList::Construct(const FArguments& InArgs)
{
    CurrentTaskList = InArgs._TaskList;

    UE_LOG(LogTemp, Log, TEXT("SVibeUETaskList::Construct - Initial task list size: %d"), CurrentTaskList.Num());

    ChildSlot
    [
        SNew(SBorder)
        .BorderBackgroundColor(TaskListColors::BackgroundCard)
        .Padding(FMargin(12, 8))
        [
            SNew(SVerticalBox)

            // Header row
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 6)
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ContentPadding(FMargin(0))
                .OnClicked(this, &SVibeUETaskList::OnHeaderClicked)
                [
                    SNew(SHorizontalBox)

                    // Header text
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SAssignNew(HeaderText, STextBlock)
                        .Text(this, &SVibeUETaskList::GetHeaderText)
                        .ColorAndOpacity(FSlateColor(TaskListColors::TextPrimary))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                    ]

                    // Collapse chevron
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SAssignNew(ChevronText, STextBlock)
                        .Text_Lambda([this]() { return FText::FromString(bIsCollapsed ? TEXT("\u25B6") : TEXT("\u25BC")); })
                        .ColorAndOpacity(FSlateColor(TaskListColors::TextSecondary))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                    ]
                ]
            ]

            // Separator line
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 4)
            [
                SNew(SBorder)
                .BorderBackgroundColor(TaskListColors::TextMuted * FLinearColor(1, 1, 1, 0.3f))
                .Padding(0)
                [
                    SNew(SSpacer)
                    .Size(FVector2D(1, 1))
                ]
            ]

            // Task items (rebuilt dynamically)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SAssignNew(ItemsContainer, SVerticalBox)
                .Visibility_Lambda([this]() { return bIsCollapsed ? EVisibility::Collapsed : EVisibility::Visible; })
            ]
        ]
    ];

    RebuildItems();
}

void SVibeUETaskList::UpdateTaskList(const TArray<FVibeUETaskItem>& NewTaskList)
{
    CurrentTaskList = NewTaskList;
    RebuildItems();
}

void SVibeUETaskList::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    if (bHasInProgressItems)
    {
        SpinnerTimer += InDeltaTime;
        if (SpinnerTimer >= SpinnerInterval)
        {
            SpinnerTimer = 0.0f;
            SpinnerFrame = (SpinnerFrame + 1) % NumSpinnerFrames;
        }
    }
}

void SVibeUETaskList::RebuildItems()
{
    if (!ItemsContainer.IsValid())
    {
        return;
    }

    ItemsContainer->ClearChildren();
    bHasInProgressItems = false;

    for (const FVibeUETaskItem& Item : CurrentTaskList)
    {
        if (Item.Status == EVibeUETaskStatus::InProgress)
        {
            bHasInProgressItems = true;
        }

        // Capture status by value for lambdas
        EVibeUETaskStatus ItemStatus = Item.Status;

        ItemsContainer->AddSlot()
        .AutoHeight()
        .Padding(0, 2)
        [
            SNew(SHorizontalBox)

            // Status icon
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0, 0, 8, 0)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text_Lambda([this, ItemStatus]()
                {
                    return GetStatusIcon(ItemStatus);
                })
                .ColorAndOpacity_Lambda([this, ItemStatus]()
                {
                    return GetStatusColor(ItemStatus);
                })
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
            ]

            // Title text
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Item.Title))
                .ColorAndOpacity(GetTitleColor(Item.Status))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
            ]
        ];
    }
}

FReply SVibeUETaskList::OnHeaderClicked()
{
    bIsCollapsed = !bIsCollapsed;
    return FReply::Handled();
}

FText SVibeUETaskList::GetStatusIcon(EVibeUETaskStatus Status) const
{
    switch (Status)
    {
        case EVibeUETaskStatus::NotStarted:
            return FText::FromString(TEXT("\u25CB")); // ○ empty circle
        case EVibeUETaskStatus::InProgress:
            return FText::FromString(SpinnerFrames[SpinnerFrame]);
        case EVibeUETaskStatus::Completed:
            return FText::FromString(TEXT("\u2713")); // ✓ checkmark
        default:
            return FText::FromString(TEXT("\u25CB"));
    }
}

FSlateColor SVibeUETaskList::GetStatusColor(EVibeUETaskStatus Status) const
{
    switch (Status)
    {
        case EVibeUETaskStatus::NotStarted:  return FSlateColor(TaskListColors::TextMuted);
        case EVibeUETaskStatus::InProgress:  return FSlateColor(TaskListColors::Orange);
        case EVibeUETaskStatus::Completed:   return FSlateColor(TaskListColors::Green);
        default:                             return FSlateColor(TaskListColors::TextSecondary);
    }
}

FSlateColor SVibeUETaskList::GetTitleColor(EVibeUETaskStatus Status) const
{
    switch (Status)
    {
        case EVibeUETaskStatus::Completed:   return FSlateColor(TaskListColors::TextSecondary);
        case EVibeUETaskStatus::InProgress:  return FSlateColor(TaskListColors::TextPrimary);
        default:                             return FSlateColor(TaskListColors::TextPrimary);
    }
}

FText SVibeUETaskList::GetHeaderText() const
{
    int32 Total = CurrentTaskList.Num();
    int32 Done = 0;
    for (const auto& Item : CurrentTaskList)
    {
        if (Item.Status == EVibeUETaskStatus::Completed)
        {
            Done++;
        }
    }
    return FText::FromString(FString::Printf(TEXT("Tasks (%d/%d completed)"), Done, Total));
}
