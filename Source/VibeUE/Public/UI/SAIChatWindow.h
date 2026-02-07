// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Chat/ChatSession.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

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

    /** Static method to attach an image from a file path (for AI tool use) */
    static bool AttachImageFromPath(const FString& FilePath);

    /** Static method to check if an image is currently attached */
    static bool HasImageAttached();

    /** Static method to clear any attached image */
    static void ClearImageAttachment();

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
    
    /** Cached model ratings from VibeUE website API */
    TMap<FString, FString> ModelRatings;
    
    /** Whether model ratings have been fetched */
    bool bModelRatingsFetched = false;
    
    /** Status text block */
    TSharedPtr<STextBlock> StatusText;
    
    /** MCP Tools count text block */
    TSharedPtr<STextBlock> ToolsCountText;
    
    /** Token budget text block */
    TSharedPtr<STextBlock> TokenBudgetText;
    
    /** Empty state container shown when no messages */
    TSharedPtr<SWidget> EmptyStateWidget;
    
    /** Map of message index to rich text block for streaming updates */
    TMap<int32, TSharedPtr<SRichTextBlock>> MessageTextBlocks;

    // ============ Voice Input UI Components ============

    /** Microphone button for voice input */
    TSharedPtr<SButton> MicrophoneButton;

    /** Voice input enabled checkbox (for settings) */
    TSharedPtr<SCheckBox> VoiceInputEnabledCheckBox;

    /** Auto-send after recording checkbox (for settings) */
    TSharedPtr<SCheckBox> AutoSendAfterRecordingCheckBox;

    /** ElevenLabs API key input (for settings) */
    TSharedPtr<SEditableTextBox> ElevenLabsApiKeyInput;

    /** Voice input active state */
    bool bIsVoiceInputActive = false;

    // ============ Image Attachment ============

    /** Attachment button (paperclip icon) */
    TSharedPtr<SButton> AttachmentButton;

    /** Image preview container (shown when image is attached) */
    TSharedPtr<SBox> ImagePreviewContainer;

    /** Image preview widget */
    TSharedPtr<SImage> ImagePreviewWidget;

    /** Remove attachment button (X button on preview) */
    TSharedPtr<SButton> RemoveAttachmentButton;

    /** Currently attached image as base64 data URL */
    FString AttachedImageDataUrl;

    /** Brush for the attached image preview */
    TSharedPtr<FSlateBrush> AttachedImageBrush;

    /** Texture for the attached image */
    UTexture2D* AttachedImageTexture = nullptr;

    /** Handle attachment button clicked */
    FReply OnAttachmentClicked();

    /** Handle remove attachment button clicked */
    FReply OnRemoveAttachmentClicked();

    /** Open file dialog to select an image */
    void OpenImageFileDialog();

    /** Process and attach an image from file path */
    void AttachImageFromFile(const FString& FilePath);

    /** Process and attach an image from clipboard */
    void AttachImageFromClipboard();

    /** Set the attached image preview */
    void SetAttachedImagePreview(const TArray<uint8>& ImageData, const FString& MimeType);

    /** Clear the attached image */
    void ClearAttachedImage();

    /** Check if there's an attached image */
    bool HasAttachedImage() const { return !AttachedImageDataUrl.IsEmpty(); }

    // ============ Thinking Indicator (Phase 1) ============

    /** Container widget for the thinking indicator */
    TSharedPtr<SWidget> ThinkingIndicatorWidget;

    /** Text block for animated thinking text */
    TSharedPtr<STextBlock> ThinkingTextBlock;

    /** Timer handle for thinking indicator animation */
    FTimerHandle ThinkingAnimationTimerHandle;

    /** Current animation frame for thinking indicator */
    int32 ThinkingAnimationFrame = 0;

    /** Whether thinking indicator is currently visible in scroll box */
    bool bThinkingIndicatorVisible = false;

    /** Current vibing word for the thinking indicator (randomly selected) */
    FString CurrentVibingWord;

    /** Get vibing words from config (or defaults) */
    static TArray<FString> GetVibingWordsFromConfig();

    /** Save vibing words to config */
    static void SaveVibingWordsToConfig(const TArray<FString>& Words);

    /** Handle LLM thinking started */
    void HandleLLMThinkingStarted();

    /** Handle LLM thinking complete */
    void HandleLLMThinkingComplete();

    /** Show/hide the thinking indicator */
    void ShowThinkingIndicator(bool bShow);

    /** Animate the thinking indicator (called by timer) */
    void AnimateThinkingIndicator();

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
        FTimerHandle StatusAnimationTimer;          // Timer handle for status animation (Phase 2)
        int32 AnimationFrame = 0;                   // Current animation frame for status spinner
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

    /** Start animated spinner for tool status (Phase 2) */
    void StartToolStatusAnimation(const FString& UniqueKey);

    /** Stop animated spinner for tool status (Phase 2) */
    void StopToolStatusAnimation(const FString& UniqueKey);

    /** Update a message widget (for streaming) */
    void UpdateMessageWidget(int32 Index, const FChatMessage& Message);
    
    /** Add a system notification to the chat UI (not stored in conversation history) */
    void AddSystemNotification(const FString& Message);
    
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
    
    /** Handle tools button clicked */
    FReply OnToolsClicked();
    
    /** Build an icon button for the toolbar */
    TSharedRef<SWidget> MakeToolbarIconButton(
        const FSlateBrush* IconBrush,
        const FText& ToolTip,
        FOnClicked OnClicked
    );
    
    /** Close the tools popup */
    void CloseToolsPopup();
    
    /** Tools popup window */
    TWeakPtr<SWindow> ToolsPopupWindow;
    
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
    
    /** Fetch model ratings from VibeUE website API and apply to available models */
    void FetchModelRatings();
    
    /** Handle model ratings fetch response */
    void HandleModelRatingsFetched(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
    
    /** Apply cached ratings to available models, re-sort, and refresh UI */
    void ApplyModelRatings();
    
    /** Get the color for a model's rating */
    static FLinearColor GetRatingColor(const FString& Rating);
    
    /** Handle MCP tools ready callback */
    void HandleToolsReady(bool bSuccess, int32 ToolCount);
    
    /** Handle summarization started callback */
    void HandleSummarizationStarted(const FString& Reason);
    
    /** Handle summarization complete callback */
    void HandleSummarizationComplete(bool bSuccess, const FString& Summary);
    
    /** Handle token budget updated callback */
    void HandleTokenBudgetUpdated(int32 CurrentTokens, int32 MaxTokens, float UtilizationPercent);
    
    /** Handle tool iteration limit reached callback */
    void HandleToolIterationLimitReached(int32 CurrentIteration, int32 MaxIterations);

    // ============ Voice Input Handlers ============

    /** Handle microphone button pressed (start recording) */
    void OnMicrophonePressed();

    /** Handle microphone button released (stop recording) */
    void OnMicrophoneReleased();

    /** Get microphone button text (emoji) */
    FText GetMicrophoneButtonText() const;

    /** Get microphone tooltip */
    FText GetMicrophoneTooltip() const;

    /** Check if microphone is enabled */
    bool IsMicrophoneEnabled() const;

    /** Handle voice input started */
    void OnVoiceInputStarted(bool bSuccess);

    /** Handle voice input text (partial or final) */
    void OnVoiceInputText(const FString& Text, bool bIsFinal);

    /** Handle voice input stopped */
    void OnVoiceInputStopped();

    /** Handle voice input auto-sent (clear input) */
    void OnVoiceInputAutoSent();

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

    /** Handle hyperlink clicked in rich text */
    void HandleHyperlinkClicked(const FSlateHyperlinkRun::FMetadata& Metadata);

    /** Static window instance */
    static TWeakPtr<SWindow> WindowInstance;
    
    /** Static widget instance */
    static TSharedPtr<SAIChatWindow> WidgetInstance;
};
