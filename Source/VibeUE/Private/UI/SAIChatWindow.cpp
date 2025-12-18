// Copyright 2025 Vibe AI. All Rights Reserved.

#include "UI/SAIChatWindow.h"
#include "Chat/AIChatCommands.h"
#include "Chat/ChatSession.h"
#include "Chat/MCPClient.h"
#include "Chat/ILLMClient.h"
#include "Chat/VibeUEAPIClient.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY(LogAIChatWindow);

// Helper to sanitize strings for logging (remove NUL and control characters)
static FString SanitizeForLog(const FString& Input)
{
    FString Output;
    Output.Reserve(Input.Len());
    for (TCHAR Char : Input)
    {
        // Skip NUL and other problematic control characters, keep tab/newline/CR
        if (Char == 0 || (Char < 32 && Char != 9 && Char != 10 && Char != 13))
        {
            continue;
        }
        Output.AppendChar(Char);
    }
    return Output;
}

// Helper to write logs to dedicated file
void FChatWindowLogger::LogToFile(const FString& Level, const FString& Message)
{
    FString LogFilePath = GetLogFilePath();
    FString Timestamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));
    FString SanitizedMessage = SanitizeForLog(Message);
    FString LogLine = FString::Printf(TEXT("[%s] [%s] %s\n"), *Timestamp, *Level, *SanitizedMessage);
    // Use ForceAnsi to avoid UTF-16 null bytes when appending
    FFileHelper::SaveStringToFile(LogLine, *LogFilePath, FFileHelper::EEncodingOptions::ForceAnsi, &IFileManager::Get(), FILEWRITE_Append);
}

FString FChatWindowLogger::GetLogFilePath()
{
    return FPaths::ProjectSavedDir() / TEXT("Logs") / TEXT("VibeUE_Chat.log");
}

// Macro to log to both UE output and dedicated file
#define CHAT_LOG(Level, Format, ...) \
    do { \
        UE_LOG(LogAIChatWindow, Level, Format, ##__VA_ARGS__); \
        FChatWindowLogger::LogToFile(TEXT(#Level), FString::Printf(Format, ##__VA_ARGS__)); \
    } while(0)

TWeakPtr<SWindow> SAIChatWindow::WindowInstance;
TSharedPtr<SAIChatWindow> SAIChatWindow::WidgetInstance;

// VibeUE Brand Colors
namespace VibeUEColors
{
    // Primary colors from website
    const FLinearColor Background(0.05f, 0.05f, 0.08f, 1.0f);      // Very dark blue-black
    const FLinearColor BackgroundLight(0.08f, 0.08f, 0.12f, 1.0f); // Slightly lighter for panels
    const FLinearColor BackgroundCard(0.10f, 0.10f, 0.14f, 1.0f);  // Card/message background
    
    // Role accent colors (borders)
    const FLinearColor Gray(0.5f, 0.5f, 0.55f, 1.0f);              // Gray - user messages
    const FLinearColor Blue(0.3f, 0.5f, 0.9f, 1.0f);               // Blue - assistant messages
    const FLinearColor Orange(0.95f, 0.6f, 0.15f, 1.0f);           // Bright Orange - tool calls (sending)
    const FLinearColor Green(0.2f, 0.8f, 0.4f, 1.0f);              // Bright Green - tool success
    const FLinearColor Red(0.9f, 0.25f, 0.25f, 1.0f);              // Bright Red - tool failures
    
    // Legacy/additional colors
    const FLinearColor Cyan(0.0f, 0.9f, 0.9f, 1.0f);               // Cyan accent
    const FLinearColor Magenta(0.85f, 0.2f, 0.65f, 1.0f);          // Magenta/pink accent
    const FLinearColor MagentaDark(0.7f, 0.5f, 1.0f, 1.0f);        // Bright purple for JSON text
    
    // Text colors - softer grays for readability
    const FLinearColor TextPrimary(0.78f, 0.78f, 0.82f, 1.0f);     // Main text - soft gray (not pure white)
    const FLinearColor TextSecondary(0.55f, 0.55f, 0.60f, 1.0f);   // Secondary/muted text
    const FLinearColor TextMuted(0.38f, 0.38f, 0.42f, 1.0f);       // Very muted
    const FLinearColor TextCode(0.72f, 0.82f, 0.72f, 1.0f);        // Code/JSON text - slight green tint
    
    // Message background colors  
    const FLinearColor UserMessage(0.14f, 0.14f, 0.16f, 1.0f);     // User messages - neutral dark gray
    const FLinearColor AssistantMessage(0.10f, 0.12f, 0.18f, 1.0f);// Assistant - dark blue tint
    const FLinearColor ToolMessage(0.12f, 0.12f, 0.12f, 1.0f);     // Tool - dark gray
    const FLinearColor SystemMessage(0.25f, 0.15f, 0.1f, 1.0f);    // System - dark orange
    
    // Border/highlight
    const FLinearColor Border(0.2f, 0.2f, 0.25f, 1.0f);
    const FLinearColor BorderHighlight(0.0f, 0.7f, 0.7f, 0.5f);    // Cyan highlight border
}

void SAIChatWindow::Construct(const FArguments& InArgs)
{
    // Create chat session
    ChatSession = MakeShared<FChatSession>();
    ChatSession->Initialize();
    
    // Bind callbacks
    ChatSession->OnMessageAdded.BindSP(this, &SAIChatWindow::HandleMessageAdded);
    ChatSession->OnMessageUpdated.BindSP(this, &SAIChatWindow::HandleMessageUpdated);
    ChatSession->OnChatReset.BindSP(this, &SAIChatWindow::HandleChatReset);
    ChatSession->OnChatError.BindSP(this, &SAIChatWindow::HandleChatError);
    ChatSession->OnMCPToolsReady.BindSP(this, &SAIChatWindow::HandleMCPToolsReady);
    ChatSession->OnSummarizationStarted.BindSP(this, &SAIChatWindow::HandleSummarizationStarted);
    ChatSession->OnSummarizationComplete.BindSP(this, &SAIChatWindow::HandleSummarizationComplete);
    ChatSession->OnTokenBudgetUpdated.BindSP(this, &SAIChatWindow::HandleTokenBudgetUpdated);
    ChatSession->OnToolIterationLimitReached.BindSP(this, &SAIChatWindow::HandleToolIterationLimitReached);
    ChatSession->OnThinkingStatusChanged.BindSP(this, &SAIChatWindow::HandleThinkingStatusChanged);
    ChatSession->OnToolPreparing.BindSP(this, &SAIChatWindow::HandleToolPreparing);
    
    // Build UI with VibeUE branding
    ChildSlot
    [
        SNew(SBorder)
        .BorderBackgroundColor(VibeUEColors::Background)
        .Padding(0)
        [
            SNew(SVerticalBox)
            
            // Toolbar with gradient-like header
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBorder)
                .BorderBackgroundColor(VibeUEColors::BackgroundLight)
                .Padding(8)
                [
                    SNew(SHorizontalBox)
                    
                    // Model selector
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .Padding(0, 0, 8, 0)
                    [
                        SAssignNew(ModelComboBox, SComboBox<TSharedPtr<FOpenRouterModel>>)
                        .OptionsSource(&AvailableModels)
                        .OnSelectionChanged(this, &SAIChatWindow::OnModelSelectionChanged)
                        .OnGenerateWidget(this, &SAIChatWindow::GenerateModelComboItem)
                        .Content()
                        [
                            SNew(STextBlock)
                            .Text(this, &SAIChatWindow::GetSelectedModelText)
                            .ColorAndOpacity(FSlateColor(VibeUEColors::TextPrimary))
                        ]
                    ]
                    
                    // MCP Tools indicator with cyan accent
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0, 0, 12, 0)
                    [
                        SAssignNew(MCPToolsText, STextBlock)
                        .Text(FText::FromString(TEXT("Tools: --")))
                        .ToolTipText(FText::FromString(TEXT("Available MCP tools")))
                        .ColorAndOpacity(FSlateColor(VibeUEColors::Cyan))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                    ]
                    
                    // Token budget indicator
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0, 0, 12, 0)
                    [
                        SAssignNew(TokenBudgetText, STextBlock)
                        .Text(FText::FromString(TEXT("Context: --")))
                        .ToolTipText(FText::FromString(TEXT("Context token usage (current / budget)")))
                        .ColorAndOpacity(FSlateColor(VibeUEColors::Green))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                    ]
                    
                    // Reset button
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(0, 0, 4, 0)
                    [
                        SNew(SButton)
                        .Text(FText::FromString(TEXT("Reset")))
                        .ToolTipText(FText::FromString(TEXT("Clear conversation history")))
                        .OnClicked(this, &SAIChatWindow::OnResetClicked)
                    ]
                    
                    // Settings button
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SButton)
                        .Text(FText::FromString(TEXT("Settings")))
                        .ToolTipText(FText::FromString(TEXT("Configure API key and preferences")))
                        .OnClicked(this, &SAIChatWindow::OnSettingsClicked)
                    ]
                ]
            ]
            
            // Status bar with magenta accent for errors
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(8, 4)
            [
                SAssignNew(StatusText, STextBlock)
                .Text(FText::GetEmpty())
                .ColorAndOpacity(FSlateColor(VibeUEColors::Magenta))
            ]
            
            // Message list area
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(4)
            [
                SNew(SBorder)
                .BorderBackgroundColor(VibeUEColors::BackgroundCard)
                .Padding(4)
                [
                    SAssignNew(MessageScrollBox, SScrollBox)
                ]
            ]
            
            // Input area with styled border
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(8, 4, 8, 8)
            [
                SNew(SHorizontalBox)
                
                // Text input (multi-line, 3 lines visible)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(SBorder)
                    .BorderBackgroundColor(VibeUEColors::Border)
                    .Padding(4)
                    [
                        // Press Enter to send, Shift+Enter for new line
                        SNew(SBox)
                        .MinDesiredHeight(54.0f)  // ~3 lines at default font size
                        .MaxDesiredHeight(54.0f)
                        [
                            SAssignNew(InputTextBox, SMultiLineEditableTextBox)
                            .HintText(this, &SAIChatWindow::GetInputHintText)
                            .AutoWrapText(true)
                            .IsReadOnly(this, &SAIChatWindow::IsInputReadOnly)
                            .OnKeyDownHandler(this, &SAIChatWindow::OnInputKeyDown)
                        ]
                    ]
                ]
                
                // Stop button (only visible when request in progress)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(4, 0, 0, 0)
                .VAlign(VAlign_Center)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Stop")))
                    .ToolTipText(FText::FromString(TEXT("Stop the current AI response")))
                    .Visibility(this, &SAIChatWindow::GetStopButtonVisibility)
                    .OnClicked(this, &SAIChatWindow::OnStopClicked)
                    .ButtonColorAndOpacity(FLinearColor(0.8f, 0.2f, 0.2f, 1.0f))
                ]
            ]
        ]
    ];
    
    // Rebuild message list from history
    RebuildMessageList();
    
    // Update model dropdown based on current provider
    UpdateModelDropdownForProvider();
    
    // Initialize MCP - auto-detect mode based on what's installed
    // Priority: Saved preference (if that mode is available) > Local mode > Engine mode
    bool bHasSavedPreference = false;
    bool bSavedEngineMode = false;
    bool bEngineMode = FMCPClient::DetermineDefaultMode(bHasSavedPreference, bSavedEngineMode);
    ChatSession->InitializeMCP(bEngineMode);
    
    // Check API key
    if (!ChatSession->HasApiKey())
    {
        FLLMProviderInfo ProviderInfo = ChatSession->GetCurrentProviderInfo();
        SetStatusText(FString::Printf(TEXT("Please set your %s API key in Settings"), *ProviderInfo.DisplayName));
    }
}

SAIChatWindow::~SAIChatWindow()
{
    if (ChatSession.IsValid())
    {
        ChatSession->Shutdown();
    }
}

void SAIChatWindow::OpenWindow()
{
    if (WindowInstance.IsValid())
    {
        // Window already exists, bring to front
        TSharedPtr<SWindow> Window = WindowInstance.Pin();
        if (Window.IsValid())
        {
            Window->BringToFront();
            return;
        }
    }
    
    // Create widget
    WidgetInstance = SNew(SAIChatWindow);
    
    // Create window
    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(FText::FromString(TEXT("VibeUE AI Chat")))
        .ClientSize(FVector2D(500, 700))
        .SupportsMinimize(true)
        .SupportsMaximize(true)
        [
            WidgetInstance.ToSharedRef()
        ];
    
    WindowInstance = Window;
    
    FSlateApplication::Get().AddWindow(Window);
    
    CHAT_LOG(Log, TEXT("AI Chat window opened"));
}

void SAIChatWindow::CloseWindow()
{
    if (WindowInstance.IsValid())
    {
        TSharedPtr<SWindow> Window = WindowInstance.Pin();
        if (Window.IsValid())
        {
            Window->RequestDestroyWindow();
        }
    }
    WindowInstance.Reset();
    WidgetInstance.Reset();
    
    CHAT_LOG(Log, TEXT("AI Chat window closed"));
}

void SAIChatWindow::ToggleWindow()
{
    if (IsWindowOpen())
    {
        CloseWindow();
    }
    else
    {
        OpenWindow();
    }
}

bool SAIChatWindow::IsWindowOpen()
{
    return WindowInstance.IsValid() && WindowInstance.Pin().IsValid();
}

void SAIChatWindow::RebuildMessageList()
{
    MessageScrollBox->ClearChildren();
    MessageTextBlocks.Empty();
    ToolCallWidgets.Empty();  // Clear tool call widget references
    PendingToolCallKeys.Empty();  // Clear pending tool call queue
    
    const TArray<FChatMessage>& Messages = ChatSession->GetMessages();
    
    // Show empty state if no messages
    if (Messages.Num() == 0)
    {
        // Check if user has a VibeUE API key
        bool bHasVibeUEApiKey = !FChatSession::GetVibeUEApiKeyFromConfig().IsEmpty();
        
        // Always recreate the empty state widget to reflect current API key status
        TSharedPtr<SVerticalBox> EmptyStateContent;
        
        EmptyStateWidget = SNew(SBox)
            .Padding(FMargin(20, 40))
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            [
                SAssignNew(EmptyStateContent, SVerticalBox)
                
                // Welcome message
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Center)
                .Padding(0, 0, 0, 12)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Welcome to VibeUE AI Chat")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                    .ColorAndOpacity(FSlateColor(VibeUEColors::TextPrimary))
                ]
                
                // Disclaimer
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Center)
                .Padding(0, 0, 0, 8)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("AI responses may be inaccurate.")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 11))
                    .ColorAndOpacity(FSlateColor(VibeUEColors::TextSecondary))
                ]
                
                // Hint
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Center)
                .Padding(0, 0, 0, 12)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Always verify important information.")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                    .ColorAndOpacity(FSlateColor(VibeUEColors::TextMuted))
                ]
            ];
        
        // Add API key link if user doesn't have one
        if (!bHasVibeUEApiKey)
        {
            EmptyStateContent->AddSlot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            .Padding(0, 8, 0, 0)
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .OnClicked_Lambda([]() -> FReply {
                    FPlatformProcess::LaunchURL(TEXT("https://www.vibeue.com/login"), nullptr, nullptr);
                    return FReply::Handled();
                })
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Get a free API key at vibeue.com")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                    .ColorAndOpacity(FSlateColor(VibeUEColors::Cyan))
                ]
            ];
        }
        
        MessageScrollBox->AddSlot()
        [
            EmptyStateWidget.ToSharedRef()
        ];
    }
    else
    {
        for (int32 i = 0; i < Messages.Num(); ++i)
        {
            AddMessageWidget(Messages[i], i);
        }
    }
    
    ScrollToBottom();
}

void SAIChatWindow::AddMessageWidget(const FChatMessage& Message, int32 Index)
{
    // Determine styling based on role
    FLinearColor BackgroundColor;
    FLinearColor BorderColor;
    FLinearColor TextColor = VibeUEColors::TextPrimary;
    
    // Check if this is a tool call (assistant message with tool calls) or tool response
    bool bIsToolCall = Message.Role == TEXT("assistant") && Message.ToolCalls.Num() > 0;
    bool bIsToolResponse = Message.Role == TEXT("tool");
    
    // For tool calls, create paired widgets for each tool call
    if (bIsToolCall)
    {
        for (int32 ToolIdx = 0; ToolIdx < Message.ToolCalls.Num(); ToolIdx++)
        {
            AddToolCallWidget(Message.ToolCalls[ToolIdx], Index, ToolIdx);
        }
        return;
    }
    
    // For tool responses, update the corresponding tool call widget
    if (bIsToolResponse)
    {
        // Parse the response to check success/failure
        bool bSuccess = true;
        if (Message.Content.Contains(TEXT("\"error\"")) || 
            Message.Content.Contains(TEXT("\"status\": \"error\"")) ||
            Message.Content.Contains(TEXT("\"success\": false")) ||
            Message.Content.Contains(TEXT("\"success\":false")))
        {
            bSuccess = false;
        }
        
        // Update the existing tool call widget with this response
        UpdateToolCallWithResponse(Message.ToolCallId, Message.Content, bSuccess);
        return;
    }
    
    // Regular message styling
    if (Message.Role == TEXT("user"))
    {
        BackgroundColor = VibeUEColors::UserMessage;
        BorderColor = VibeUEColors::Gray;
    }
    else if (Message.Role == TEXT("assistant"))
    {
        BackgroundColor = VibeUEColors::AssistantMessage;
        BorderColor = VibeUEColors::Blue;
    }
    else
    {
        BackgroundColor = VibeUEColors::SystemMessage;
        BorderColor = VibeUEColors::TextSecondary;
    }
    
    // Create rounded brush for bubble effect
    static FSlateBrush RoundedBrush;
    RoundedBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
    RoundedBrush.TintColor = FSlateColor(FLinearColor::White);
    RoundedBrush.OutlineSettings.CornerRadii = FVector4(4.0f, 4.0f, 4.0f, 4.0f);
    RoundedBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
    
    // Create thin border strip brush (not rounded)
    static FSlateBrush BorderStripBrush;
    BorderStripBrush.DrawAs = ESlateBrushDrawType::Box;
    BorderStripBrush.TintColor = FSlateColor(FLinearColor::White);
    
    FString DisplayText = Message.Content;
    if (Message.bIsStreaming && DisplayText.IsEmpty())
    {
        DisplayText = TEXT("...");
    }
    
    // Create the message content text block and store reference for streaming updates
    TSharedPtr<STextBlock> ContentTextBlock;
    
    // Create the message bubble with rounded corners
    TSharedRef<SWidget> MessageContent = 
        SNew(SBorder)
        .BorderImage(&RoundedBrush)
        .BorderBackgroundColor(BackgroundColor)
        .Padding(FMargin(8, 4, 8, 4))
        [
            SNew(SHorizontalBox)
            
            // Colored accent line (left side)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(FMargin(0, 0, 6, 0))
            [
                SNew(SBorder)
                .BorderImage(&BorderStripBrush)
                .BorderBackgroundColor(BorderColor)
                .Padding(FMargin(2, 0, 0, 0))
                [
                    SNew(SSpacer)
                    .Size(FVector2D(0, 0))
                ]
            ]
            
            // Message content - fills available space
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SAssignNew(ContentTextBlock, STextBlock)
                .Text(FText::FromString(DisplayText))
                .AutoWrapText(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                .ColorAndOpacity(FSlateColor(TextColor))
            ]
            
            // Copy button - on same line, right side
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Top)
            .Padding(FMargin(6, 0, 0, 0))
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("Copy")))
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .OnClicked_Lambda([this, Index]() -> FReply
                {
                    CopyMessageToClipboard(Index);
                    return FReply::Handled();
                })
            ]
        ];
    
    // User messages: right-aligned with max width
    // Assistant messages: left-aligned, fill available width
    if (Message.Role == TEXT("user"))
    {
        MessageScrollBox->AddSlot()
        .Padding(2)
        .HAlign(HAlign_Right)
        [
            SNew(SBox)
            .MaxDesiredWidth(350.0f)
            [
                MessageContent
            ]
        ];
    }
    else
    {
        // Assistant/system messages fill width
        MessageScrollBox->AddSlot()
        .Padding(2)
        [
            MessageContent
        ];
    }
    
    // Store reference for streaming updates
    MessageTextBlocks.Add(Index, ContentTextBlock);
}

void SAIChatWindow::AddToolCallWidget(const FChatToolCall& ToolCall, int32 MessageIndex, int32 ToolIndex)
{
    // Generate a unique key that includes message index and tool index
    // This handles the case where vLLM/Qwen returns the same ID (call_0) for all tool calls
    FString UniqueKey = FString::Printf(TEXT("%d_%d_%s"), MessageIndex, ToolIndex, *ToolCall.Id);
    
    // Check if widget already exists for this tool call (prevents duplicates)
    if (ToolCallWidgets.Contains(UniqueKey))
    {
        if (FChatSession::IsDebugModeEnabled())
        {
            CHAT_LOG(Warning, TEXT("[UI] AddToolCallWidget: Widget already exists for key %s, skipping"), *UniqueKey);
        }
        return;
    }
    
    // Update status to show tool execution
    SetStatusText(FString::Printf(TEXT("Executing: %s"), *ToolCall.Name));
    
    // Create a solid color brush for borders
    static FSlateBrush SolidBrush;
    SolidBrush.DrawAs = ESlateBrushDrawType::Box;
    SolidBrush.TintColor = FSlateColor(FLinearColor::White);
    
    // Extract action name from arguments if available
    FString ActionName;
    TSharedPtr<FJsonObject> ArgsJson;
    TSharedRef<TJsonReader<>> ArgsReader = TJsonReaderFactory<>::Create(ToolCall.Arguments);
    if (FJsonSerializer::Deserialize(ArgsReader, ArgsJson) && ArgsJson.IsValid())
    {
        ArgsJson->TryGetStringField(TEXT("action"), ActionName);
    }
    
    // Build compact summary text (like Copilot: "tool_name → action")
    FString CallSummary = ToolCall.Name;
    if (!ActionName.IsEmpty())
    {
        CallSummary += FString::Printf(TEXT(" → %s"), *ActionName);
    }
    
    // Create widget data struct
    FToolCallWidgetData WidgetData;
    WidgetData.bExpanded = MakeShared<bool>(false);
    WidgetData.CallJson = ToolCall.Arguments;
    WidgetData.bResponseReceived = false;
    
    // Truncate JSON for display
    FString TruncatedCallJson = WidgetData.CallJson.Len() > 1000 ? WidgetData.CallJson.Left(1000) + TEXT("\n...") : WidgetData.CallJson;
    
    // Capture for copy lambdas
    FString CapturedCallJson = ToolCall.Arguments;
    TSharedPtr<FString> CapturedResponseJson = MakeShared<FString>();
    WidgetData.ResponseJsonPtr = CapturedResponseJson;
    
    // Create expandable details container (hidden by default)
    TSharedRef<SBox> DetailsContainer = SNew(SBox)
        .Visibility(EVisibility::Collapsed)
        .Padding(FMargin(12, 4, 0, 0))
        [
            SNew(SVerticalBox)
            // Call arguments section
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Arguments:")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                        .ColorAndOpacity(FSlateColor(VibeUEColors::TextSecondary))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                        .Text(FText::FromString(TEXT("Copy")))
                        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                        .OnClicked_Lambda([CapturedCallJson]() -> FReply
                        {
                            FPlatformApplicationMisc::ClipboardCopy(*CapturedCallJson);
                            return FReply::Handled();
                        })
                    ]
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 2, 0, 0)
                [
                    SNew(SBorder)
                    .BorderImage(&SolidBrush)
                    .BorderBackgroundColor(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f))
                    .Padding(4)
                    [
                        SAssignNew(WidgetData.CallJsonText, STextBlock)
                        .Text(FText::FromString(TruncatedCallJson))
                        .AutoWrapText(true)
                        .Font(FCoreStyle::GetDefaultFontStyle("Mono", 10))
                        .ColorAndOpacity(FSlateColor(VibeUEColors::TextCode))
                    ]
                ]
            ]
            // Response section (will be populated when response arrives)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 8, 0, 0)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Response:")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                        .ColorAndOpacity(FSlateColor(VibeUEColors::TextSecondary))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                        .Text(FText::FromString(TEXT("Copy")))
                        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                        .OnClicked_Lambda([CapturedResponseJson]() -> FReply
                        {
                            FPlatformApplicationMisc::ClipboardCopy(**CapturedResponseJson);
                            return FReply::Handled();
                        })
                    ]
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 2, 0, 0)
                [
                    SNew(SBorder)
                    .BorderImage(&SolidBrush)
                    .BorderBackgroundColor(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f))
                    .Padding(4)
                    [
                        SAssignNew(WidgetData.ResponseJsonText, STextBlock)
                        .Text(FText::GetEmpty())
                        .AutoWrapText(true)
                        .Font(FCoreStyle::GetDefaultFontStyle("Mono", 10))
                        .ColorAndOpacity(FSlateColor(VibeUEColors::TextCode))
                    ]
                ]
            ]
        ];
    
    WidgetData.DetailsContainer = DetailsContainer;
    TWeakPtr<SBox> WeakDetailsContainer = DetailsContainer;
    
    // Create compact single-line widget (Copilot style)
    TSharedRef<SWidget> CompactWidget = 
        SNew(SVerticalBox)
        // Main header row
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(2, 0)
        [
            SNew(SHorizontalBox)
            
            // Chevron expand button
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0, 0, 4, 0)
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ContentPadding(FMargin(0))
                .OnClicked_Lambda([bExpanded = WidgetData.bExpanded, WeakDetailsContainer]() -> FReply
                {
                    *bExpanded = !(*bExpanded);
                    if (TSharedPtr<SBox> Container = WeakDetailsContainer.Pin())
                    {
                        Container->SetVisibility(*bExpanded ? EVisibility::Visible : EVisibility::Collapsed);
                    }
                    return FReply::Handled();
                })
                [
                    SAssignNew(WidgetData.ChevronText, STextBlock)
                    .Text_Lambda([bExpanded = WidgetData.bExpanded]() { return FText::FromString(*bExpanded ? TEXT("▼") : TEXT("▶")); })
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                    .ColorAndOpacity(FSlateColor(VibeUEColors::TextSecondary))
                ]
            ]
            
            // Status indicator (arrow while pending, then ✓ or ✗)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0, 0, 6, 0)
            [
                SAssignNew(WidgetData.StatusText, STextBlock)
                .Text(FText::FromString(TEXT("→")))  // Right arrow = running
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                .ColorAndOpacity(FSlateColor(VibeUEColors::Orange))
            ]
            
            // Tool call summary
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SAssignNew(WidgetData.SummaryText, STextBlock)
                .Text(FText::FromString(CallSummary))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                .ColorAndOpacity(FSlateColor(VibeUEColors::TextPrimary))
            ]
        ]
        
        // Expandable details (collapsed by default)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            DetailsContainer
        ];
    
    // Store widget data keyed by unique key for later update
    ToolCallWidgets.Add(UniqueKey, WidgetData);
    
    // Add to pending queue (FIFO - responses come in same order as calls)
    PendingToolCallKeys.Add(UniqueKey);
    
    // Add to scroll box
    MessageScrollBox->AddSlot()
    .Padding(FMargin(2, 0, 2, 0))
    [
        CompactWidget
    ];
    
    ScrollToBottom();
}

void SAIChatWindow::UpdateToolCallWithResponse(const FString& ToolCallId, const FString& ResponseJson, bool bSuccess)
{
    // Update status to show tool completed
    SetStatusText(bSuccess ? TEXT("Tool completed successfully") : TEXT("Tool execution failed"));
    
    // Find the first pending widget that hasn't received a response yet
    // We use a queue because vLLM/Qwen may return the same ID (call_0) for all tool calls
    FString UniqueKey;
    for (const FString& Key : PendingToolCallKeys)
    {
        FToolCallWidgetData* Widget = ToolCallWidgets.Find(Key);
        if (Widget && !Widget->bResponseReceived)
        {
            UniqueKey = Key;
            break;
        }
    }
    
    if (UniqueKey.IsEmpty())
    {
        UE_LOG(LogAIChatWindow, Warning, TEXT("Could not find pending tool call widget for ID: %s"), *ToolCallId);
        return;
    }
    
    FToolCallWidgetData* WidgetData = ToolCallWidgets.Find(UniqueKey);
    if (!WidgetData)
    {
        return;
    }
    
    // Mark response received and store JSON for copy button
    WidgetData->bResponseReceived = true;
    WidgetData->ResponseJson = ResponseJson;
    if (WidgetData->ResponseJsonPtr.IsValid())
    {
        *WidgetData->ResponseJsonPtr = ResponseJson;
    }
    
    // Update status indicator to checkmark or X
    if (WidgetData->StatusText.IsValid())
    {
        FString StatusIcon = bSuccess ? TEXT("✓") : TEXT("✗");
        FLinearColor StatusColor = bSuccess ? VibeUEColors::Green : VibeUEColors::Red;
        
        WidgetData->StatusText->SetText(FText::FromString(StatusIcon));
        WidgetData->StatusText->SetColorAndOpacity(FSlateColor(StatusColor));
    }
    
    // Update response JSON text in the details section
    if (WidgetData->ResponseJsonText.IsValid())
    {
        FString TruncatedJson = ResponseJson.Len() > 1000 ? ResponseJson.Left(1000) + TEXT("\n...") : ResponseJson;
        FLinearColor TextColor = bSuccess ? VibeUEColors::Green : VibeUEColors::Red;
        
        WidgetData->ResponseJsonText->SetText(FText::FromString(TruncatedJson));
        WidgetData->ResponseJsonText->SetColorAndOpacity(FSlateColor(TextColor));
    }
    
    ScrollToBottom();
}

void SAIChatWindow::UpdateMessageWidget(int32 Index, const FChatMessage& Message)
{
    // Tool calls are handled by AddToolCallWidget which creates widgets immediately
    // Tool responses are handled by UpdateToolCallWithResponse which updates in place
    // Neither need rebuilding the whole list
    
    bool bIsToolCall = Message.Role == TEXT("assistant") && Message.ToolCalls.Num() > 0;
    bool bIsToolResponse = Message.Role == TEXT("tool");
    
    // Tool messages are handled by their dedicated functions, skip here
    if (bIsToolCall || bIsToolResponse)
    {
        return;
    }
    
    // Try to update just the text block instead of rebuilding
    TSharedPtr<STextBlock>* TextBlockPtr = MessageTextBlocks.Find(Index);
    if (TextBlockPtr && TextBlockPtr->IsValid())
    {
        FString DisplayText = Message.Content;
        if (Message.bIsStreaming && DisplayText.IsEmpty())
        {
            DisplayText = TEXT("...");
        }
        (*TextBlockPtr)->SetText(FText::FromString(DisplayText));
    }
    else
    {
        // Fallback to rebuild if we don't have a reference
        RebuildMessageList();
    }
}

void SAIChatWindow::ScrollToBottom()
{
    MessageScrollBox->ScrollToEnd();
}

FReply SAIChatWindow::OnSendClicked()
{
    FString Message = InputTextBox->GetText().ToString();
    if (!Message.IsEmpty())
    {
        if (FChatSession::IsDebugModeEnabled())
        {
            CHAT_LOG(Log, TEXT("[UI EVENT] Send button clicked - Message: %s"), *Message.Left(100));
        }
        
        // Clear any previous error message before sending new request
        SetStatusText(TEXT("Sending request..."));
        
        InputTextBox->SetText(FText::GetEmpty());
        
        // Check if user typed "continue" to resume after iteration limit
        if (Message.TrimStartAndEnd().ToLower() == TEXT("continue"))
        {
            ChatSession->ContinueAfterIterationLimit();
        }
        else
        {
            ChatSession->SendMessage(Message);
        }
    }
    return FReply::Handled();
}

FReply SAIChatWindow::OnStopClicked()
{
    if (ChatSession.IsValid() && ChatSession->IsRequestInProgress())
    {
        if (FChatSession::IsDebugModeEnabled())
        {
            CHAT_LOG(Log, TEXT("[UI EVENT] Stop button clicked - Cancelling request"));
        }
        ChatSession->CancelRequest();
        SetStatusText(TEXT("Request cancelled"));
    }
    return FReply::Handled();
}

EVisibility SAIChatWindow::GetStopButtonVisibility() const
{
    if (ChatSession.IsValid() && ChatSession->IsRequestInProgress())
    {
        return EVisibility::Visible;
    }
    return EVisibility::Collapsed;
}

FReply SAIChatWindow::OnResetClicked()
{
    ChatSession->ResetChat();
    SetStatusText(TEXT(""));
    return FReply::Handled();
}

FReply SAIChatWindow::OnSettingsClicked()
{
    // Show API key input dialog
    TSharedRef<SWindow> SettingsWindow = SNew(SWindow)
        .Title(FText::FromString(TEXT("VibeUE AI Chat Settings")))
        .ClientSize(FVector2D(500, 720))
        .SupportsMinimize(false)
        .SupportsMaximize(false);
    
    TSharedPtr<SEditableTextBox> VibeUEApiKeyInput;
    TSharedPtr<SEditableTextBox> OpenRouterApiKeyInput;
    TSharedPtr<SCheckBox> EngineModeCheckBox;
    TSharedPtr<SCheckBox> DebugModeCheckBox;
    TSharedPtr<SCheckBox> ParallelToolCallsCheckBox;
    TSharedPtr<SSpinBox<float>> TemperatureSpinBox;
    TSharedPtr<SSpinBox<float>> TopPSpinBox;
    TSharedPtr<SSpinBox<int32>> MaxTokensSpinBox;
    TSharedPtr<SSpinBox<int32>> MaxToolIterationsSpinBox;
    
    // Load current LLM parameter values
    float CurrentTemperature = FChatSession::GetTemperatureFromConfig();
    float CurrentTopP = FChatSession::GetTopPFromConfig();
    int32 CurrentMaxTokens = FChatSession::GetMaxTokensFromConfig();
    bool bCurrentParallelToolCalls = FChatSession::GetParallelToolCallsFromConfig();
    int32 CurrentMaxToolIterations = FChatSession::GetMaxToolCallIterationsFromConfig();
    
    // Get available providers for the dropdown
    TArray<FLLMProviderInfo> AvailableProvidersList = FChatSession::GetAvailableProviders();
    TSharedPtr<TArray<TSharedPtr<FString>>> ProviderOptions = MakeShared<TArray<TSharedPtr<FString>>>();
    for (const FLLMProviderInfo& ProviderInfo : AvailableProvidersList)
    {
        ProviderOptions->Add(MakeShared<FString>(ProviderInfo.DisplayName));
    }
    
    // Current selection - find the matching item from the options array
    ELLMProvider CurrentProvider = FChatSession::GetProviderFromConfig();
    FString CurrentProviderName = CurrentProvider == ELLMProvider::VibeUE ? TEXT("VibeUE") : TEXT("OpenRouter");
    TSharedPtr<FString> SelectedProvider;
    for (const TSharedPtr<FString>& Option : *ProviderOptions)
    {
        if (Option.IsValid() && *Option == CurrentProviderName)
        {
            SelectedProvider = Option;
            break;
        }
    }
    // Fallback to first option if not found
    if (!SelectedProvider.IsValid() && ProviderOptions->Num() > 0)
    {
        SelectedProvider = (*ProviderOptions)[0];
    }
    TSharedPtr<TSharedPtr<FString>> SelectedProviderPtr = MakeShared<TSharedPtr<FString>>(SelectedProvider);
    
    // Determine current mode using the same logic as initialization
    bool bHasSavedPreference = false;
    bool bSavedEngineMode = false;
    bool bCurrentEngineMode = FMCPClient::DetermineDefaultMode(bHasSavedPreference, bSavedEngineMode);
    
    bool bCurrentDebugMode = FChatSession::IsDebugModeEnabled();
    
    SettingsWindow->SetContent(
        SNew(SVerticalBox)
        // Provider Selection (Dropdown)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("LLM Provider:")))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 4)
        [
            SNew(SComboBox<TSharedPtr<FString>>)
            .OptionsSource(ProviderOptions.Get())
            .InitiallySelectedItem(SelectedProvider)
            .OnSelectionChanged_Lambda([SelectedProviderPtr, ProviderOptions](TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
            {
                if (NewSelection.IsValid())
                {
                    *SelectedProviderPtr = NewSelection;
                }
            })
            .OnGenerateWidget_Lambda([ProviderOptions](TSharedPtr<FString> Item) -> TSharedRef<SWidget>
            {
                return SNew(STextBlock)
                    .Text(Item.IsValid() ? FText::FromString(*Item) : FText::FromString(TEXT("Invalid")));
            })
            .Content()
            [
                SNew(STextBlock)
                .Text_Lambda([SelectedProviderPtr, ProviderOptions]() -> FText
                {
                    return SelectedProviderPtr->IsValid() ? FText::FromString(**SelectedProviderPtr) : FText::FromString(TEXT("Select Provider"));
                })
            ]
        ]
        // VibeUE API Key
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 12, 8, 0)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("VibeUE API Key:")))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 0)
        [
            SAssignNew(VibeUEApiKeyInput, SEditableTextBox)
            .Text(FText::FromString(FChatSession::GetVibeUEApiKeyFromConfig()))
            .IsPassword(true)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 4, 8, 0)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .OnClicked_Lambda([]() -> FReply {
                    FPlatformProcess::LaunchURL(TEXT("https://www.vibeue.com/login"), nullptr, nullptr);
                    return FReply::Handled();
                })
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Get VibeUE API key at vibeue.com")))
                    .ColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.5f, 1.0f)))
                ]
            ]
        ]
        // OpenRouter API Key
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 12, 8, 0)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("OpenRouter API Key:")))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 0)
        [
            SAssignNew(OpenRouterApiKeyInput, SEditableTextBox)
            .Text(FText::FromString(FChatSession::GetApiKeyFromConfig()))
            .IsPassword(true)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 4, 8, 0)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .OnClicked_Lambda([]() -> FReply {
                    FPlatformProcess::LaunchURL(TEXT("https://openrouter.ai/keys"), nullptr, nullptr);
                    return FReply::Handled();
                })
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Get OpenRouter API key at openrouter.ai")))
                    .ColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.5f, 1.0f)))
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 16, 8, 4)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("MCP Server Mode:")))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 4)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SAssignNew(EngineModeCheckBox, SCheckBox)
                .IsChecked(bCurrentEngineMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
            ]
            + SHorizontalBox::Slot()
            .Padding(4, 0, 0, 0)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Engine Mode (FAB install)")))
                .ToolTipText(FText::FromString(TEXT("OFF = Use Project/Plugins/VibeUE (development)\nON = Use Engine/Plugins/Marketplace/VibeUE (testing FAB install)")))
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 12, 8, 4)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SAssignNew(DebugModeCheckBox, SCheckBox)
                .IsChecked(bCurrentDebugMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
            ]
            + SHorizontalBox::Slot()
            .Padding(4, 0, 0, 0)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Debug Mode")))
                .ToolTipText(FText::FromString(TEXT("Show request count and token usage in the status bar.")))
            ]
        ]
        // ============ LLM Generation Parameters (VibeUE only) ============
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 16, 8, 4)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("LLM Generation Parameters (VibeUE only):")))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
        ]
        // Temperature
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 4)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(0.4f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Temperature:")))
                .ToolTipText(FText::FromString(TEXT("Lower = more deterministic (better for code). Range: 0.0-2.0. Default: 0.2")))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(0.6f)
            [
                SAssignNew(TemperatureSpinBox, SSpinBox<float>)
                .MinValue(0.0f)
                .MaxValue(2.0f)
                .Delta(0.05f)
                .Value(CurrentTemperature)
                .MinDesiredWidth(100)
            ]
        ]
        // Top P
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 4)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(0.4f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Top P:")))
                .ToolTipText(FText::FromString(TEXT("Nucleus sampling. Range: 0.0-1.0. Default: 0.95")))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(0.6f)
            [
                SAssignNew(TopPSpinBox, SSpinBox<float>)
                .MinValue(0.0f)
                .MaxValue(1.0f)
                .Delta(0.05f)
                .Value(CurrentTopP)
                .MinDesiredWidth(100)
            ]
        ]
        // Max Tokens
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 4)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(0.4f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Max Tokens:")))
                .ToolTipText(FText::FromString(TEXT("Maximum response length. Range: 256-16384. Default: 8192")))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(0.6f)
            [
                SAssignNew(MaxTokensSpinBox, SSpinBox<int32>)
                .MinValue(256)
                .MaxValue(16384)
                .Delta(256)
                .Value(CurrentMaxTokens)
                .MinDesiredWidth(100)
            ]
        ]
        // Max Tool Iterations (like Copilot's maxRequests)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 4)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(0.4f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Max Tool Iterations:")))
                .ToolTipText(FText::FromString(TEXT("Max tool call rounds before confirmation prompt. Range: 10-500. Default: 200 (like Copilot)")))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(0.6f)
            [
                SAssignNew(MaxToolIterationsSpinBox, SSpinBox<int32>)
                .MinValue(10)
                .MaxValue(500)
                .Delta(10)
                .Value(CurrentMaxToolIterations)
                .MinDesiredWidth(100)
            ]
        ]
        // Parallel Tool Calls
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 12, 8, 4)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SAssignNew(ParallelToolCallsCheckBox, SCheckBox)
                .IsChecked(bCurrentParallelToolCalls ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
            ]
            + SHorizontalBox::Slot()
            .Padding(4, 0, 0, 0)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Parallel Tool Calls")))
                .ToolTipText(FText::FromString(TEXT("ON = LLM can make multiple tool calls at once (faster)\nOFF = One tool call at a time (shows progress between calls)")))
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 8, 8, 0)
        [
            SNew(STextBlock)
            .Text_Lambda([]() -> FText {
                FString LocalPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir() / TEXT("VibeUE") / TEXT("Content") / TEXT("Python"));
                FString EnginePath = FMCPClient::GetEngineVibeUEPythonPath();
                if (EnginePath.IsEmpty())
                {
                    EnginePath = TEXT("(VibeUE not found in Engine Marketplace)");
                }
                return FText::FromString(FString::Printf(TEXT("Local: %s\nEngine: %s"), *LocalPath, *EnginePath));
            })
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
            .AutoWrapText(true)
            .ColorAndOpacity(FSlateColor(VibeUEColors::TextMuted))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Right)
        .Padding(8, 16, 8, 8)
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("Save")))
            .OnClicked_Lambda([this, VibeUEApiKeyInput, OpenRouterApiKeyInput, SelectedProviderPtr, EngineModeCheckBox, DebugModeCheckBox, ParallelToolCallsCheckBox, TemperatureSpinBox, TopPSpinBox, MaxTokensSpinBox, MaxToolIterationsSpinBox, SettingsWindow]() -> FReply
            {
                // Save VibeUE API key
                FString NewVibeUEApiKey = VibeUEApiKeyInput->GetText().ToString();
                ChatSession->SetVibeUEApiKey(NewVibeUEApiKey);
                
                // Save OpenRouter API key
                FString NewOpenRouterApiKey = OpenRouterApiKeyInput->GetText().ToString();
                ChatSession->SetApiKey(NewOpenRouterApiKey);
                
                // Save provider selection from dropdown
                ELLMProvider NewProvider = ELLMProvider::VibeUE;  // Default
                if (SelectedProviderPtr->IsValid() && **SelectedProviderPtr == TEXT("OpenRouter"))
                {
                    NewProvider = ELLMProvider::OpenRouter;
                }
                ChatSession->SetCurrentProvider(NewProvider);
                
                // Save and apply MCP mode
                bool bNewEngineMode = EngineModeCheckBox->IsChecked();
                GConfig->SetBool(TEXT("VibeUE"), TEXT("MCPEngineMode"), bNewEngineMode, GEditorPerProjectIni);
                
                // Save debug mode
                bool bNewDebugMode = DebugModeCheckBox->IsChecked();
                FChatSession::SetDebugModeEnabled(bNewDebugMode);
                
                // Save LLM generation parameters
                FChatSession::SaveTemperatureToConfig(TemperatureSpinBox->GetValue());
                FChatSession::SaveTopPToConfig(TopPSpinBox->GetValue());
                FChatSession::SaveMaxTokensToConfig(MaxTokensSpinBox->GetValue());
                FChatSession::SaveMaxToolCallIterationsToConfig(MaxToolIterationsSpinBox->GetValue());
                FChatSession::SaveParallelToolCallsToConfig(ParallelToolCallsCheckBox->IsChecked());
                
                // Apply max tool iterations to current session
                ChatSession->SetMaxToolCallIterations(MaxToolIterationsSpinBox->GetValue());
                
                // Apply the new LLM parameters to the client
                ChatSession->ApplyLLMParametersToClient();
                
                GConfig->Flush(false, GEditorPerProjectIni);
                
                // Reinitialize MCP with new mode (this properly shuts down, clears state, and rediscovers tools)
                ChatSession->ReinitializeMCP(bNewEngineMode);
                
                // Update the model dropdown based on new provider
                UpdateModelDropdownForProvider();
                
                SetStatusText(FString::Printf(TEXT("Settings saved - Using %s"), 
                    NewProvider == ELLMProvider::VibeUE ? TEXT("VibeUE API") : TEXT("OpenRouter")));
                SettingsWindow->RequestDestroyWindow();
                return FReply::Handled();
            })
        ]
    );
    
    FSlateApplication::Get().AddWindow(SettingsWindow);
    
    return FReply::Handled();
}

void SAIChatWindow::OnInputTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    // NOTE: We intentionally do NOT handle OnEnter here.
    // The OnInputKeyDown handler already handles Enter key presses.
    // Handling it here too would cause duplicate message sends.
    // OnUserInteraction is handled there instead.
    if (CommitType == ETextCommit::OnUserMovedFocus)
    {
        // Optional: could send on focus loss if desired
    }
}

FReply SAIChatWindow::OnInputKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    // Block input while a request is in progress
    if (ChatSession.IsValid() && ChatSession->IsRequestInProgress())
    {
        if (FChatSession::IsDebugModeEnabled())
        {
            CHAT_LOG(Verbose, TEXT("[UI EVENT] Key press blocked - Request in progress"));
        }
        return FReply::Handled(); // Consume the key press but don't do anything
    }
    
    // Enter without Shift sends the message
    // Shift+Enter inserts a new line (default behavior)
    if (InKeyEvent.GetKey() == EKeys::Enter && !InKeyEvent.IsShiftDown())
    {
        if (FChatSession::IsDebugModeEnabled())
        {
            CHAT_LOG(Log, TEXT("[UI EVENT] Enter key pressed - Sending message"));
        }
        OnSendClicked();
        return FReply::Handled();
    }
    
    return FReply::Unhandled();
}

void SAIChatWindow::OnModelSelectionChanged(TSharedPtr<FOpenRouterModel> NewSelection, ESelectInfo::Type SelectInfo)
{
    if (NewSelection.IsValid())
    {
        SelectedModel = NewSelection;
        ChatSession->SetCurrentModel(NewSelection->Id);
        CHAT_LOG(Log, TEXT("Selected model: %s"), *NewSelection->Id);
    }
}

TSharedRef<SWidget> SAIChatWindow::GenerateModelComboItem(TSharedPtr<FOpenRouterModel> Model)
{
    return SNew(STextBlock)
        .Text(FText::FromString(Model.IsValid() ? Model->GetDisplayString() : TEXT("Unknown")));
}

FText SAIChatWindow::GetSelectedModelText() const
{
    if (SelectedModel.IsValid())
    {
        return FText::FromString(SelectedModel->GetDisplayString());
    }
    
    // Show current model from session
    FString CurrentModel = ChatSession.IsValid() ? ChatSession->GetCurrentModel() : TEXT("Loading...");
    return FText::FromString(CurrentModel);
}

void SAIChatWindow::HandleMessageAdded(const FChatMessage& Message)
{
    // Don't add empty streaming assistant messages - wait for content or tool call
    // This prevents the "..." flash before tool calls
    if (Message.Role == TEXT("assistant") && Message.bIsStreaming && Message.Content.IsEmpty() && Message.ToolCalls.Num() == 0)
    {
        // Skip adding - HandleMessageUpdated will add it when content arrives
        return;
    }
    
    int32 MessageIndex = ChatSession->GetMessages().Num() - 1;
    
    // Remove empty state widget if this is the first message
    if (MessageIndex == 0 && EmptyStateWidget.IsValid())
    {
        MessageScrollBox->ClearChildren();
    }
    
    // Check if widget already exists for this index (prevents duplicates)
    if (MessageTextBlocks.Contains(MessageIndex))
    {
        if (FChatSession::IsDebugModeEnabled())
        {
            CHAT_LOG(Warning, TEXT("[UI] HandleMessageAdded: Widget already exists for index %d, skipping"), MessageIndex);
        }
        return;
    }
    
    AddMessageWidget(Message, MessageIndex);
    ScrollToBottom();
    UpdateUIState();
}

void SAIChatWindow::HandleMessageUpdated(int32 Index, const FChatMessage& Message)
{
    // For tool calls, check if widgets already exist via ToolCallWidgets map
    bool bIsToolCall = Message.Role == TEXT("assistant") && Message.ToolCalls.Num() > 0;
    if (bIsToolCall)
    {
        // Check if any of the tool calls already have widgets (using unique key)
        bool bAllToolsHaveWidgets = true;
        for (int32 ToolIdx = 0; ToolIdx < Message.ToolCalls.Num(); ToolIdx++)
        {
            const FChatToolCall& ToolCall = Message.ToolCalls[ToolIdx];
            FString UniqueKey = FString::Printf(TEXT("%d_%d_%s"), Index, ToolIdx, *ToolCall.Id);
            if (!ToolCallWidgets.Contains(UniqueKey))
            {
                bAllToolsHaveWidgets = false;
                break;
            }
        }
        
        if (bAllToolsHaveWidgets)
        {
            // All tools already have widgets, nothing to do
            return;
        }
        
        // Some tools don't have widgets yet - add them
        for (int32 ToolIdx = 0; ToolIdx < Message.ToolCalls.Num(); ToolIdx++)
        {
            const FChatToolCall& ToolCall = Message.ToolCalls[ToolIdx];
            FString UniqueKey = FString::Printf(TEXT("%d_%d_%s"), Index, ToolIdx, *ToolCall.Id);
            if (!ToolCallWidgets.Contains(UniqueKey))
            {
                AddToolCallWidget(ToolCall, Index, ToolIdx);
            }
        }
        return;
    }
    
    // For tool responses, just update - AddMessageWidget handles this correctly
    bool bIsToolResponse = Message.Role == TEXT("tool");
    if (bIsToolResponse)
    {
        AddMessageWidget(Message, Index);  // This calls UpdateToolCallWithResponse internally
        return;
    }
    
    // Check if this message has a widget yet (it may have been skipped as empty streaming)
    TSharedPtr<STextBlock>* TextBlockPtr = MessageTextBlocks.Find(Index);
    if (!TextBlockPtr)
    {
        // Widget doesn't exist - add it now that we have content
        AddMessageWidget(Message, Index);
    }
    else
    {
        UpdateMessageWidget(Index, Message);
    }
    
    // When streaming finishes for assistant message, update status
    if (!Message.bIsStreaming && Message.Role == TEXT("assistant"))
    {
        if (FChatSession::IsDebugModeEnabled())
        {
            // Show usage stats in debug mode
            const FLLMUsageStats& Stats = ChatSession->GetUsageStats();
            if (Stats.RequestCount > 0)
            {
                SetStatusText(FString::Printf(TEXT("Requests: %d | Tokens: %d prompt, %d completion | Session: %d total"),
                    Stats.RequestCount, Stats.TotalPromptTokens, Stats.TotalCompletionTokens,
                    Stats.TotalPromptTokens + Stats.TotalCompletionTokens));
            }
        }
        else
        {
            // Clear any error message on successful response completion
            SetStatusText(TEXT(""));
        }
        
        // Update token budget display after assistant response completes
        UpdateTokenBudgetDisplay();
    }
    
    ScrollToBottom();
    UpdateUIState();
}

void SAIChatWindow::HandleChatReset()
{
    RebuildMessageList();
    UpdateUIState();
    UpdateTokenBudgetDisplay();
}

void SAIChatWindow::HandleChatError(const FString& ErrorMessage)
{
    SetStatusText(ErrorMessage);
    UpdateUIState();
}

void SAIChatWindow::HandleModelsFetched(bool bSuccess, const TArray<FOpenRouterModel>& Models)
{
    if (bSuccess)
    {
        AvailableModels.Empty();
        SelectedModel.Reset();  // Clear old selection when fetching new models
        
        // Filter to only models that support tools, then sort
        TArray<FOpenRouterModel> FilteredModels;
        for (const FOpenRouterModel& Model : Models)
        {
            if (Model.bSupportsTools)
            {
                FilteredModels.Add(Model);
            }
        }
        
        // Sort: free models first, then by name
        FilteredModels.Sort([](const FOpenRouterModel& A, const FOpenRouterModel& B)
        {
            // Free models come first
            if (A.IsFree() != B.IsFree())
            {
                return A.IsFree();
            }
            // Then sort by name
            return A.Name < B.Name;
        });
        
        CHAT_LOG(Log, TEXT("Filtered to %d models with tool support (from %d total)"), 
            FilteredModels.Num(), Models.Num());
        
        for (const FOpenRouterModel& Model : FilteredModels)
        {
            TSharedPtr<FOpenRouterModel> ModelPtr = MakeShared<FOpenRouterModel>(Model);
            AvailableModels.Add(ModelPtr);
            
            // Set selected model if it matches current
            if (Model.Id == ChatSession->GetCurrentModel())
            {
                SelectedModel = ModelPtr;
            }
        }
        
        // If no model selected yet, pick first free model with tool support
        if (!SelectedModel.IsValid() && AvailableModels.Num() > 0)
        {
            for (const TSharedPtr<FOpenRouterModel>& ModelPtr : AvailableModels)
            {
                if (ModelPtr->IsFree())
                {
                    SelectedModel = ModelPtr;
                    ChatSession->SetCurrentModel(ModelPtr->Id);
                    break;
                }
            }
            // If no free model found, use first available
            if (!SelectedModel.IsValid())
            {
                SelectedModel = AvailableModels[0];
                ChatSession->SetCurrentModel(SelectedModel->Id);
            }
        }
        
        ModelComboBox->RefreshOptions();
        
        if (SelectedModel.IsValid())
        {
            ModelComboBox->SetSelectedItem(SelectedModel);
        }
        
        CHAT_LOG(Log, TEXT("Loaded %d models with tool support (from %d total)"), 
            AvailableModels.Num(), Models.Num());
    }
    else
    {
        SetStatusText(TEXT("Failed to fetch models"));
    }
}

void SAIChatWindow::UpdateModelDropdownForProvider()
{
    if (!ChatSession.IsValid())
    {
        return;
    }
    
    // Check if provider supports model selection
    if (ChatSession->SupportsModelSelection())
    {
        // OpenRouter - fetch models
        ChatSession->FetchAvailableModels(FOnModelsFetched::CreateSP(this, &SAIChatWindow::HandleModelsFetched));
    }
    else
    {
        // VibeUE - show single "VibeUE" option
        AvailableModels.Empty();
        SelectedModel.Reset();
        
        // Create a single "VibeUE" model entry with default values
        TSharedPtr<FOpenRouterModel> VibeUEModelPtr = MakeShared<FOpenRouterModel>();
        VibeUEModelPtr->Id = TEXT("vibeue");
        VibeUEModelPtr->Name = TEXT("VibeUE");
        VibeUEModelPtr->bSupportsTools = true;
        VibeUEModelPtr->ContextLength = 131072; // Default, will be updated from API
        
        AvailableModels.Add(VibeUEModelPtr);
        SelectedModel = VibeUEModelPtr;
        
        // Fetch actual model info from API to get real context length
        if (ChatSession.IsValid())
        {
            TSharedPtr<FVibeUEAPIClient> VibeUEClient = ChatSession->GetVibeUEClient();
            if (VibeUEClient.IsValid())
            {
                // Capture weak pointers for the lambda
                TWeakPtr<FOpenRouterModel> WeakModel = VibeUEModelPtr;
                TWeakPtr<SComboBox<TSharedPtr<FOpenRouterModel>>> WeakComboBox = ModelComboBox;
                
                VibeUEClient->FetchModelInfo([WeakModel, WeakComboBox](bool bSuccess, int32 ContextLength, const FString& ModelId)
                {
                    // Must run on game thread since we're updating UI
                    AsyncTask(ENamedThreads::GameThread, [WeakModel, WeakComboBox, bSuccess, ContextLength, ModelId]()
                    {
                        if (TSharedPtr<FOpenRouterModel> Model = WeakModel.Pin())
                        {
                            if (bSuccess && ContextLength > 0)
                            {
                                Model->ContextLength = ContextLength;
                                UE_LOG(LogAIChatWindow, Log, TEXT("Updated VibeUE model context length to %d from API"), ContextLength);
                            }
                            
                            // Refresh the combo box to show updated info
                            if (TSharedPtr<SComboBox<TSharedPtr<FOpenRouterModel>>> ComboBox = WeakComboBox.Pin())
                            {
                                ComboBox->RefreshOptions();
                            }
                        }
                    });
                });
            }
        }
        
        if (ModelComboBox.IsValid())
        {
            ModelComboBox->RefreshOptions();
            ModelComboBox->SetSelectedItem(SelectedModel);
        }
        
        CHAT_LOG(Log, TEXT("Provider changed to VibeUE - model dropdown shows single option"));
    }
}

void SAIChatWindow::HandleMCPToolsReady(bool bSuccess, int32 ToolCount)
{
    if (MCPToolsText.IsValid())
    {
        if (bSuccess && ToolCount > 0)
        {
            MCPToolsText->SetText(FText::FromString(FString::Printf(TEXT("Tools: %d"), ToolCount)));
            MCPToolsText->SetColorAndOpacity(FSlateColor(VibeUEColors::Green)); // Green for connected
            CHAT_LOG(Log, TEXT("MCP tools ready: %d tools available"), ToolCount);
        }
        else
        {
            MCPToolsText->SetText(FText::FromString(TEXT("Tools: 0")));
            MCPToolsText->SetColorAndOpacity(FSlateColor(VibeUEColors::TextMuted)); // Muted for no tools
            CHAT_LOG(Log, TEXT("MCP tools: none available"));
        }
    }
    
    // Update token budget display initially
    UpdateTokenBudgetDisplay();
}

void SAIChatWindow::HandleSummarizationStarted(const FString& Reason)
{
    CHAT_LOG(Log, TEXT("Summarization started: %s"), *Reason);
    SetStatusText(FString::Printf(TEXT("📋 Summarizing conversation... (%s)"), *Reason));
    
    // Update token budget display color to indicate summarization
    if (TokenBudgetText.IsValid())
    {
        TokenBudgetText->SetColorAndOpacity(FSlateColor(VibeUEColors::Orange));
    }
}

void SAIChatWindow::HandleSummarizationComplete(bool bSuccess, const FString& Summary)
{
    if (bSuccess)
    {
        CHAT_LOG(Log, TEXT("Summarization complete: %d chars"), Summary.Len());
        SetStatusText(TEXT("✅ Conversation summarized to save context space."));
        
        // Show summary preview in a system message
        FString PreviewText = Summary.Left(200);
        if (Summary.Len() > 200) PreviewText += TEXT("...");
        CHAT_LOG(Log, TEXT("Summary preview: %s"), *PreviewText);
    }
    else
    {
        CHAT_LOG(Warning, TEXT("Summarization failed"));
        SetStatusText(TEXT("⚠️ Failed to summarize conversation."));
    }
    
    // Update token budget display
    UpdateTokenBudgetDisplay();
    
    // Clear status after a delay (would need timer, for now just leave it)
}

void SAIChatWindow::HandleTokenBudgetUpdated(int32 CurrentTokens, int32 MaxTokens, float UtilizationPercent)
{
    if (!TokenBudgetText.IsValid()) return;
    
    // Format the display: "Context: 12.5K / 117K (10%)"
    auto FormatTokens = [](int32 Tokens) -> FString
    {
        if (Tokens >= 1000)
        {
            return FString::Printf(TEXT("%.1fK"), Tokens / 1000.0f);
        }
        return FString::Printf(TEXT("%d"), Tokens);
    };
    
    FString TokenText = FString::Printf(TEXT("Context: %s / %s (%.0f%%)"),
        *FormatTokens(CurrentTokens), 
        *FormatTokens(MaxTokens), 
        UtilizationPercent * 100.f);
    
    TokenBudgetText->SetText(FText::FromString(TokenText));
    
    // Color based on utilization
    FLinearColor Color;
    if (UtilizationPercent < 0.6f)
    {
        Color = VibeUEColors::Green; // Plenty of room
    }
    else if (UtilizationPercent < 0.8f)
    {
        Color = FLinearColor(1.0f, 0.8f, 0.0f, 1.0f); // Yellow - getting full
    }
    else
    {
        Color = VibeUEColors::Red; // Near limit
    }
    TokenBudgetText->SetColorAndOpacity(FSlateColor(Color));
}

void SAIChatWindow::HandleToolIterationLimitReached(int32 CurrentIteration, int32 MaxIterations)
{
    CHAT_LOG(Warning, TEXT("Tool iteration limit reached: %d/%d"), CurrentIteration, MaxIterations);
    
    // Calculate what the new limit will be (50% increase, like Copilot)
    int32 NewLimit = FMath::RoundToInt(MaxIterations * 1.5f);
    NewLimit = FMath::Clamp(NewLimit, 10, 500);
    
    // Show a system message asking if user wants to continue
    FString Message = FString::Printf(
        TEXT("⚠️ Tool iteration limit reached (%d/%d). The AI has been working and may need more iterations.\n\nType 'continue' to increase the limit to %d, or send a new message to start fresh."),
        CurrentIteration, MaxIterations, NewLimit);
    
    SetStatusText(FString::Printf(TEXT("Tool limit reached (%d/%d) - type 'continue' (new limit: %d) or new message"), CurrentIteration, MaxIterations, NewLimit));
    
    // Add a system message to the chat
    FChatMessage SystemMsg(TEXT("system"), Message);
    SystemMsg.Role = TEXT("system");
    if (ChatSession.IsValid())
    {
        // We need to add this as a visual-only message, not to the actual conversation
        // For now, just show it in the status and let user type 'continue'
    }
}

void SAIChatWindow::HandleThinkingStatusChanged(bool bIsThinking)
{
    if (bIsThinking)
    {
        SetStatusText(TEXT("AI is thinking..."));
    }
    else
    {
        // Transitioning from thinking to generating
        SetStatusText(TEXT("Generating response..."));
    }
}

void SAIChatWindow::HandleToolPreparing(const FString& ToolName)
{
    SetStatusText(FString::Printf(TEXT("Preparing tool: %s"), *ToolName));
}

void SAIChatWindow::UpdateTokenBudgetDisplay()
{
    if (!ChatSession.IsValid()) return;
    
    int32 CurrentTokens = ChatSession->GetEstimatedTokenCount();
    int32 MaxTokens = ChatSession->GetTokenBudget();
    float Utilization = ChatSession->GetContextUtilization();
    
    HandleTokenBudgetUpdated(CurrentTokens, MaxTokens, Utilization);
}

void SAIChatWindow::UpdateUIState()
{
    // UI state updates handled by IsSendEnabled and other callbacks
}

void SAIChatWindow::SetStatusText(const FString& Text)
{
    if (StatusText.IsValid())
    {
        StatusText->SetText(FText::FromString(Text));
    }
}

bool SAIChatWindow::IsSendEnabled() const
{
    return ChatSession.IsValid() && 
           ChatSession->HasApiKey() && 
           !ChatSession->IsRequestInProgress();
}

bool SAIChatWindow::IsInputReadOnly() const
{
    // Make input read-only while a request is in progress
    return ChatSession.IsValid() && ChatSession->IsRequestInProgress();
}

FText SAIChatWindow::GetInputHintText() const
{
    if (ChatSession.IsValid() && ChatSession->IsRequestInProgress())
    {
        return FText::FromString(TEXT("Waiting for AI response..."));
    }
    return FText::FromString(TEXT("Type a message... (Enter to send, Shift+Enter for new line)"));
}

void SAIChatWindow::CopyMessageToClipboard(int32 MessageIndex)
{
    const TArray<FChatMessage>& Messages = ChatSession->GetMessages();
    if (Messages.IsValidIndex(MessageIndex))
    {
        FPlatformApplicationMisc::ClipboardCopy(*Messages[MessageIndex].Content);
        SetStatusText(TEXT("Copied to clipboard"));
    }
}
