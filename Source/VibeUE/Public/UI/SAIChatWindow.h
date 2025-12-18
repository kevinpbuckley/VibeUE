// Copyright 2025 Vibe AI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboBox.h"
#include "Chat/ChatSession.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAIChatWindow, Log, All);

/**
 * Helper class for logging chat window events to dedicated file
 */
class FChatWindowLogger
{
public:
    static void LogToFile(const FString& Level, const FString& Message);
    static FString GetLogFilePath();
};

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
    
    /** Token budget text block */
    TSharedPtr<STextBlock> TokenBudgetText;
    
    /** Empty state container shown when no messages */
    TSharedPtr<SWidget> EmptyStateWidget;
    
    /** Map of message index to text block for streaming updates */
    TMap<int32, TSharedPtr<STextBlock>> MessageTextBlocks;
    
    /** Widget components for compact Copilot-style tool call display */
    struct FToolCallWidgetData
    {
        TSharedPtr<STextBlock> SummaryText;         // Shows "tool_name → action"
        TSharedPtr<STextBlock> StatusText;          // Shows spinner, ✓, or ✗
        TSharedPtr<STextBlock> ChevronText;         // Shows ▶ or ▼
        TSharedPtr<SBox> DetailsContainer;          // Container for expandable details
        TSharedPtr<STextBlock> CallJsonText;        // Full JSON arguments
        TSharedPtr<STextBlock> ResponseJsonText;    // Full JSON response
        TSharedPtr<bool> bExpanded;                 // Expand state
        FString CallJson;                           // Cached call JSON for copy
        FString ResponseJson;                       // Cached response JSON for copy
        TSharedPtr<FString> ResponseJsonPtr;        // Shared pointer for copy lambda
        bool bResponseReceived = false;             // Track if response arrived
    };
    
    /** Map of unique key (MessageIndex_ToolIndex_ToolCallId) to widget data */
    TMap<FString, FToolCallWidgetData> ToolCallWidgets;
    
    /** Queue of pending tool call unique keys that haven't received responses yet (FIFO order) */
    TArray<FString> PendingToolCallKeys;
    
    /** Build the message list UI */
    void RebuildMessageList();
    
    /** Add a message widget to the scroll box */
    void AddMessageWidget(const FChatMessage& Message, int32 Index);
    
    /** Add a paired tool call widget (shows call immediately, updates with response later) */
    void AddToolCallWidget(const FChatToolCall& ToolCall, int32 MessageIndex, int32 ToolIndex);
    
    /** Update an existing tool widget with its response */
    void UpdateToolCallWithResponse(const FString& ToolCallId, const FString& ResponseJson, bool bSuccess);
    
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
    
    /** Handle summarization started callback */
    void HandleSummarizationStarted(const FString& Reason);
    
    /** Handle summarization complete callback */
    void HandleSummarizationComplete(bool bSuccess, const FString& Summary);
    
    /** Handle token budget updated callback */
    void HandleTokenBudgetUpdated(int32 CurrentTokens, int32 MaxTokens, float UtilizationPercent);
    
    /** Handle tool iteration limit reached callback */
    void HandleToolIterationLimitReached(int32 CurrentIteration, int32 MaxIterations);
    
    /** Handle thinking status changed (model is reasoning) */
    void HandleThinkingStatusChanged(bool bIsThinking);
    
    /** Handle tool preparing (tool name detected before full args) */
    void HandleToolPreparing(const FString& ToolName);
    
    /** Update the token budget display */
    void UpdateTokenBudgetDisplay();
    
    /** Update UI enabled state based on session state */
    void UpdateUIState();
    
    /** Set status text */
    void SetStatusText(const FString& Text);
    
    /** Check if send is enabled */
    bool IsSendEnabled() const;
    
    /** Check if input should be read-only (during request) */
    bool IsInputReadOnly() const;
    
    /** Get dynamic hint text for input box */
    FText GetInputHintText() const;
    
    /** Copy message content to clipboard */
    void CopyMessageToClipboard(int32 MessageIndex);
    
    /** Static window instance */
    static TWeakPtr<SWindow> WindowInstance;
    
    /** Static widget instance */
    static TSharedPtr<SAIChatWindow> WidgetInstance;
};
