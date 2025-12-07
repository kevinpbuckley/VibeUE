// Copyright 2025 Vibe AI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboBox.h"
#include "Chat/ChatSession.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAIChatWindow, Log, All);

class SScrollBox;
class SEditableTextBox;
class SMultiLineEditableTextBox;
class STextBlock;
class SButton;

/**
 * Slate widget for the AI Chat window
 */
class VIBEUE_API SAIChatWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SAIChatWindow) {}
    SLATE_END_ARGS()
    
    /** Construct the widget */
    void Construct(const FArguments& InArgs);
    
    /** Destructor */
    virtual ~SAIChatWindow();
    
    /** Static method to open the chat window */
    static void OpenWindow();
    
    /** Static method to close the chat window */
    static void CloseWindow();
    
    /** Static method to toggle the chat window */
    static void ToggleWindow();
    
    /** Check if window is currently open */
    static bool IsWindowOpen();
    
private:
    /** The chat session managing conversation state */
    TSharedPtr<FChatSession> ChatSession;
    
    /** Scroll box containing message widgets */
    TSharedPtr<SScrollBox> MessageScrollBox;
    
    /** Input text box for typing messages */
    TSharedPtr<SMultiLineEditableTextBox> InputTextBox;
    
    /** Model selection combo box */
    TSharedPtr<SComboBox<TSharedPtr<FOpenRouterModel>>> ModelComboBox;
    
    /** Currently selected model */
    TSharedPtr<FOpenRouterModel> SelectedModel;
    
    /** Available models for the combo box */
    TArray<TSharedPtr<FOpenRouterModel>> AvailableModels;
    
    /** Status text block */
    TSharedPtr<STextBlock> StatusText;
    
    /** MCP Tools count text block */
    TSharedPtr<STextBlock> MCPToolsText;
    
    /** Map of message index to text block for streaming updates */
    TMap<int32, TSharedPtr<STextBlock>> MessageTextBlocks;
    
    /** Build the message list UI */
    void RebuildMessageList();
    
    /** Add a message widget to the scroll box */
    void AddMessageWidget(const FChatMessage& Message, int32 Index);
    
    /** Add a collapsible tool message widget (for tool calls and results) */
    void AddToolMessageWidget(const FChatMessage& Message, int32 Index, 
        const FLinearColor& BackgroundColor, const FLinearColor& BorderColor, bool bIsToolCall);
    
    /** Update a message widget (for streaming) */
    void UpdateMessageWidget(int32 Index, const FChatMessage& Message);
    
    /** Scroll to the bottom of the message list */
    void ScrollToBottom();
    
    /** Handle send button clicked */
    FReply OnSendClicked();
    
    /** Handle stop button clicked */
    FReply OnStopClicked();
    
    /** Get stop button visibility */
    EVisibility GetStopButtonVisibility() const;
    
    /** Handle reset button clicked */
    FReply OnResetClicked();
    
    /** Handle settings button clicked */
    FReply OnSettingsClicked();
    
    /** Handle input text committed (Enter pressed) */
    void OnInputTextCommitted(const FText& Text, ETextCommit::Type CommitType);
    
    /** Handle key down in input box (Enter to send, Shift+Enter for new line) */
    FReply OnInputKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);
    
    /** Handle model selection changed */
    void OnModelSelectionChanged(TSharedPtr<FOpenRouterModel> NewSelection, ESelectInfo::Type SelectInfo);
    
    /** Generate widget for model combo box item */
    TSharedRef<SWidget> GenerateModelComboItem(TSharedPtr<FOpenRouterModel> Model);
    
    /** Get the currently selected model text */
    FText GetSelectedModelText() const;
    
    /** Update the model dropdown based on the current LLM provider */
    void UpdateModelDropdownForProvider();
    
    /** Handle message added callback */
    void HandleMessageAdded(const FChatMessage& Message);
    
    /** Handle message updated callback (streaming) */
    void HandleMessageUpdated(int32 Index, const FChatMessage& Message);
    
    /** Handle chat reset callback */
    void HandleChatReset();
    
    /** Handle chat error callback */
    void HandleChatError(const FString& ErrorMessage);
    
    /** Handle models fetched callback */
    void HandleModelsFetched(bool bSuccess, const TArray<FOpenRouterModel>& Models);
    
    /** Handle MCP tools ready callback */
    void HandleMCPToolsReady(bool bSuccess, int32 ToolCount);
    
    /** Update UI enabled state based on session state */
    void UpdateUIState();
    
    /** Set status text */
    void SetStatusText(const FString& Text);
    
    /** Check if send is enabled */
    bool IsSendEnabled() const;
    
    /** Copy message content to clipboard */
    void CopyMessageToClipboard(int32 MessageIndex);
    
    /** Static window instance */
    static TWeakPtr<SWindow> WindowInstance;
    
    /** Static widget instance */
    static TSharedPtr<SAIChatWindow> WidgetInstance;
};
