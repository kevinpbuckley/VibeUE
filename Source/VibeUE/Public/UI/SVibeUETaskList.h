// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Chat/ChatTypes.h"

class SVerticalBox;
class STextBlock;

/**
 * Slate widget that renders an inline task list checklist in the chat window.
 * Shows task status with icons, supports collapse/expand, and animates in-progress items.
 */
class VIBEUE_API SVibeUETaskList : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SVibeUETaskList) {}
        SLATE_ARGUMENT(TArray<FVibeUETaskItem>, TaskList)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Update the displayed task list (called when OnTaskListUpdated fires) */
    void UpdateTaskList(const TArray<FVibeUETaskItem>& NewTaskList);

    /** Tick for spinner animation */
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
    /** Rebuild all task item widgets */
    void RebuildItems();

    /** Toggle collapsed/expanded state */
    FReply OnHeaderClicked();

    /** Get the appropriate icon for a task status */
    FText GetStatusIcon(EVibeUETaskStatus Status) const;

    /** Get the appropriate color for a task status icon */
    FSlateColor GetStatusColor(EVibeUETaskStatus Status) const;

    /** Get text color for a task title based on status */
    FSlateColor GetTitleColor(EVibeUETaskStatus Status) const;

    /** Get progress header text (e.g., "Tasks (2/5 completed)") */
    FText GetHeaderText() const;

    TArray<FVibeUETaskItem> CurrentTaskList;
    TSharedPtr<SVerticalBox> ItemsContainer;
    TSharedPtr<STextBlock> HeaderText;
    TSharedPtr<STextBlock> ChevronText;
    bool bIsCollapsed = false;

    // Spinner animation for in-progress items
    int32 SpinnerFrame = 0;
    float SpinnerTimer = 0.0f;
    static constexpr float SpinnerInterval = 0.3f;
    bool bHasInProgressItems = false;
};
