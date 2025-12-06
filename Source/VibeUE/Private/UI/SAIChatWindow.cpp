// Copyright 2025 Vibe AI. All Rights Reserved.

#include "UI/SAIChatWindow.h"
#include "Chat/AIChatCommands.h"
#include "Chat/MCPClient.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
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

DEFINE_LOG_CATEGORY(LogAIChatWindow);

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
    
    // Text colors
    const FLinearColor TextPrimary(0.9f, 0.9f, 0.95f, 1.0f);       // Main text
    const FLinearColor TextSecondary(0.6f, 0.6f, 0.65f, 1.0f);     // Secondary/muted text
    const FLinearColor TextMuted(0.4f, 0.4f, 0.45f, 1.0f);         // Very muted
    
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
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
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
                SNew(SBorder)
                .BorderBackgroundColor(VibeUEColors::Border)
                .Padding(4)
                [
                    // Text input (multi-line, 3 lines visible)
                    // Press Enter to send, Shift+Enter for new line
                    SNew(SBox)
                    .MinDesiredHeight(54.0f)  // ~3 lines at default font size
                    .MaxDesiredHeight(54.0f)
                    [
                        SAssignNew(InputTextBox, SMultiLineEditableTextBox)
                        .HintText(FText::FromString(TEXT("Type a message... (Enter to send, Shift+Enter for new line)")))
                        .AutoWrapText(true)
                        .OnKeyDownHandler(this, &SAIChatWindow::OnInputKeyDown)
                    ]
                ]
            ]
        ]
    ];
    
    // Rebuild message list from history
    RebuildMessageList();
    
    // Fetch models
    ChatSession->FetchAvailableModels(FOnModelsFetched::CreateSP(this, &SAIChatWindow::HandleModelsFetched));
    
    // Initialize MCP - auto-detect mode based on what's installed
    // Priority: Saved preference (if that mode is available) > Local mode > Engine mode
    bool bHasSavedPreference = false;
    bool bSavedEngineMode = false;
    bool bEngineMode = FMCPClient::DetermineDefaultMode(bHasSavedPreference, bSavedEngineMode);
    ChatSession->InitializeMCP(bEngineMode);
    
    // Check API key
    if (!ChatSession->HasApiKey())
    {
        SetStatusText(TEXT("Please set your OpenRouter API key in Settings"));
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
    
    UE_LOG(LogAIChatWindow, Log, TEXT("AI Chat window opened"));
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
    
    UE_LOG(LogAIChatWindow, Log, TEXT("AI Chat window closed"));
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
    
    const TArray<FChatMessage>& Messages = ChatSession->GetMessages();
    for (int32 i = 0; i < Messages.Num(); ++i)
    {
        AddMessageWidget(Messages[i], i);
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
    bool bIsToolRelated = bIsToolCall || bIsToolResponse;
    
    // For tool responses, check if success or failure
    bool bToolSuccess = true;
    if (bIsToolResponse)
    {
        // Check if the content indicates an error
        if (Message.Content.Contains(TEXT("\"error\"")) || 
            Message.Content.Contains(TEXT("\"status\": \"error\"")) ||
            Message.Content.Contains(TEXT("\"success\": false")) ||
            Message.Content.Contains(TEXT("\"success\":false")))
        {
            bToolSuccess = false;
        }
    }
    
    if (Message.Role == TEXT("user"))
    {
        BackgroundColor = VibeUEColors::UserMessage;
        BorderColor = VibeUEColors::Gray;  // Gray border for user
    }
    else if (bIsToolCall)
    {
        BackgroundColor = VibeUEColors::UserMessage;  // Gray background like other messages
        BorderColor = VibeUEColors::Orange;  // Orange left border for tool calls
    }
    else if (bIsToolResponse)
    {
        BackgroundColor = VibeUEColors::UserMessage;  // Gray background like other messages
        BorderColor = bToolSuccess ? VibeUEColors::Green : VibeUEColors::Red;  // Green/Red left border
    }
    else if (Message.Role == TEXT("assistant"))
    {
        BackgroundColor = VibeUEColors::AssistantMessage;
        BorderColor = VibeUEColors::Blue;  // Blue border for assistant
    }
    else
    {
        BackgroundColor = VibeUEColors::SystemMessage;
        BorderColor = VibeUEColors::TextSecondary;
    }
    
    // Create a solid color brush for borders
    static FSlateBrush SolidBrush;
    SolidBrush.DrawAs = ESlateBrushDrawType::Box;
    SolidBrush.TintColor = FSlateColor(FLinearColor::White);
    
    // For tool-related messages, create collapsible widget
    if (bIsToolRelated)
    {
        AddToolMessageWidget(Message, Index, BackgroundColor, BorderColor, bIsToolCall);
        return;
    }
    
    FString DisplayText = Message.Content;
    if (Message.bIsStreaming && DisplayText.IsEmpty())
    {
        DisplayText = TEXT("...");
    }
    
    // Create the message content text block and store reference for streaming updates
    TSharedPtr<STextBlock> ContentTextBlock;
    
    // Create the message with colored left border (no role label)
    TSharedRef<SWidget> MessageContent = 
        SNew(SHorizontalBox)
        
        // Colored left border strip
        + SHorizontalBox::Slot()
        .AutoWidth()
        [
            SNew(SBorder)
            .BorderImage(&SolidBrush)
            .BorderBackgroundColor(BorderColor)
            .Padding(FMargin(3, 0, 0, 0))  // 3px wide colored strip
            [
                SNew(SSpacer)
                .Size(FVector2D(0, 0))
            ]
        ]
        
        // Message content area
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        [
            SNew(SBorder)
            .BorderImage(&SolidBrush)
            .BorderBackgroundColor(BackgroundColor)
            .Padding(10)
            [
                SNew(SVerticalBox)
                
                // Message content
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SAssignNew(ContentTextBlock, STextBlock)
                    .Text(FText::FromString(DisplayText))
                    .AutoWrapText(true)
                    .ColorAndOpacity(FSlateColor(TextColor))
                ]
                
                // Copy button - subtle styling
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Right)
                .Padding(0, 6, 0, 0)
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
            ]
        ];
    
    // User messages: right-aligned with max width
    // Assistant messages: left-aligned, fill available width
    if (Message.Role == TEXT("user"))
    {
        MessageScrollBox->AddSlot()
        .Padding(4)
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
        .Padding(4)
        [
            MessageContent
        ];
    }
    
    // Store reference for streaming updates
    MessageTextBlocks.Add(Index, ContentTextBlock);
}

void SAIChatWindow::AddToolMessageWidget(const FChatMessage& Message, int32 Index, 
    const FLinearColor& BackgroundColor, const FLinearColor& BorderColor, bool bIsToolCall)
{
    // Create a solid color brush for borders
    static FSlateBrush SolidBrush;
    SolidBrush.DrawAs = ESlateBrushDrawType::Box;
    SolidBrush.TintColor = FSlateColor(FLinearColor::White);
    
    // Extract tool name and action from message
    FString ToolName;
    FString ActionName;
    FString FullJson = Message.Content;
    FString ToolStatusText;
    
    if (bIsToolCall && Message.ToolCalls.Num() > 0)
    {
        // Tool call - get name from ToolCalls array
        ToolName = Message.ToolCalls[0].Name;
        FullJson = Message.ToolCalls[0].Arguments;
        ToolStatusText = TEXT("CALL");
        
        // Try to extract action from arguments JSON
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FullJson);
        if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
        {
            JsonObject->TryGetStringField(TEXT("action"), ActionName);
        }
    }
    else
    {
        // Tool response - try to parse and find success status
        ToolStatusText = TEXT("RESULT");
        
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message.Content);
        if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
        {
            bool bSuccess = false;
            if (JsonObject->TryGetBoolField(TEXT("success"), bSuccess))
            {
                ToolStatusText = bSuccess ? TEXT("SUCCESS") : TEXT("FAILED");
            }
        }
        
        // Tool name comes from the ToolCallId if available
        ToolName = Message.ToolCallId.IsEmpty() ? TEXT("Tool") : TEXT("Response");
    }
    
    // Build summary text
    FString SummaryText;
    if (!ToolName.IsEmpty())
    {
        SummaryText = FString::Printf(TEXT("[%s] %s"), *ToolStatusText, *ToolName);
        if (!ActionName.IsEmpty())
        {
            SummaryText += FString::Printf(TEXT(" → %s"), *ActionName);
        }
    }
    else
    {
        SummaryText = FString::Printf(TEXT("[%s]"), *ToolStatusText);
    }
    
    // Create expand/collapse state - use shared ptr to track state
    TSharedPtr<bool> bIsExpanded = MakeShared<bool>(false);
    TSharedPtr<STextBlock> JsonTextBlock;
    
    // Pre-create the JSON container so we can reference it in the button click handler
    TSharedRef<SBox> JsonContainer = SNew(SBox)
        .Visibility(EVisibility::Collapsed)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SAssignNew(JsonTextBlock, STextBlock)
                .Text(FText::FromString(FullJson.Len() > 2000 ? FullJson.Left(2000) + TEXT("\n... (truncated)") : FullJson))
                .AutoWrapText(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
                .ColorAndOpacity(FSlateColor(VibeUEColors::TextPrimary))  // Standard text color on gray background
            ]
            
            // Copy button
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Right)
            .Padding(0, 4, 0, 0)
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
    
    // Capture the container as TSharedRef for the lambda
    TWeakPtr<SBox> WeakJsonContainer = JsonContainer;
    
    // Create the collapsible tool message widget
    TSharedRef<SWidget> ToolWidget = 
        SNew(SHorizontalBox)
        
        // Colored left border strip
        + SHorizontalBox::Slot()
        .AutoWidth()
        [
            SNew(SBorder)
            .BorderImage(&SolidBrush)
            .BorderBackgroundColor(BorderColor)
            .Padding(FMargin(3, 0, 0, 0))
            [
                SNew(SSpacer)
                .Size(FVector2D(0, 0))
            ]
        ]
        
        // Tool message content area
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        [
            SNew(SBorder)
            .BorderImage(&SolidBrush)
            .BorderBackgroundColor(BackgroundColor)
            .Padding(8)
            [
                SNew(SVerticalBox)
                
                // Header row with expand button and summary
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SHorizontalBox)
                    
                    // Expand/Collapse button
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0, 0, 6, 0)
                    [
                        SNew(SButton)
                        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                        .OnClicked_Lambda([bIsExpanded, WeakJsonContainer]() -> FReply
                        {
                            *bIsExpanded = !(*bIsExpanded);
                            if (TSharedPtr<SBox> Container = WeakJsonContainer.Pin())
                            {
                                Container->SetVisibility(*bIsExpanded ? EVisibility::Visible : EVisibility::Collapsed);
                            }
                            return FReply::Handled();
                        })
                        [
                            SNew(STextBlock)
                            .Text_Lambda([bIsExpanded]() { return FText::FromString(*bIsExpanded ? TEXT("-") : TEXT("+")); })
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                            .ColorAndOpacity(FSlateColor(BorderColor))  // Match border color for +/- button
                        ]
                    ]
                    
                    // Summary text
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(SummaryText))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                        .ColorAndOpacity(FSlateColor(VibeUEColors::TextPrimary))  // Standard text color on gray background
                    ]
                ]
                
                // JSON content (collapsed by default) - use pre-created container
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 6, 0, 0)
                [
                    JsonContainer
                ]
            ]
        ];
    
    // Add to scroll box
    MessageScrollBox->AddSlot()
    .Padding(2)
    [
        ToolWidget
    ];
    
    // Store reference for potential updates
    MessageTextBlocks.Add(Index, JsonTextBlock);
}

void SAIChatWindow::UpdateMessageWidget(int32 Index, const FChatMessage& Message)
{
    // Check if this message now has tool calls or is a tool response - these need special rendering
    bool bIsToolCall = Message.Role == TEXT("assistant") && Message.ToolCalls.Num() > 0;
    bool bIsToolResponse = Message.Role == TEXT("tool");
    
    // If message has tool calls or is a tool response, we need to rebuild to show collapsible widget
    if (bIsToolCall || bIsToolResponse)
    {
        RebuildMessageList();
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
        // Clear any previous error message before sending new request
        SetStatusText(TEXT(""));
        
        InputTextBox->SetText(FText::GetEmpty());
        ChatSession->SendMessage(Message);
    }
    return FReply::Handled();
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
        .ClientSize(FVector2D(450, 320))
        .SupportsMinimize(false)
        .SupportsMaximize(false);
    
    TSharedPtr<SEditableTextBox> ApiKeyInput;
    TSharedPtr<SCheckBox> EngineModeCheckBox;
    TSharedPtr<SCheckBox> DebugModeCheckBox;
    
    // Determine current mode using the same logic as initialization
    bool bHasSavedPreference = false;
    bool bSavedEngineMode = false;
    bool bCurrentEngineMode = FMCPClient::DetermineDefaultMode(bHasSavedPreference, bSavedEngineMode);
    
    bool bCurrentDebugMode = FChatSession::IsDebugModeEnabled();
    
    SettingsWindow->SetContent(
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("OpenRouter API Key:")))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 0)
        [
            SAssignNew(ApiKeyInput, SEditableTextBox)
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
                    .Text(FText::FromString(TEXT("Get your API key at openrouter.ai")))
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
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
            .AutoWrapText(true)
            .ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Right)
        .Padding(8, 16, 8, 8)
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("Save")))
            .OnClicked_Lambda([this, ApiKeyInput, EngineModeCheckBox, DebugModeCheckBox, SettingsWindow]() -> FReply
            {
                // Save API key
                FString NewApiKey = ApiKeyInput->GetText().ToString();
                ChatSession->SetApiKey(NewApiKey);
                
                // Save and apply MCP mode
                bool bNewEngineMode = EngineModeCheckBox->IsChecked();
                GConfig->SetBool(TEXT("VibeUE"), TEXT("MCPEngineMode"), bNewEngineMode, GEditorPerProjectIni);
                
                // Save debug mode
                bool bNewDebugMode = DebugModeCheckBox->IsChecked();
                FChatSession::SetDebugModeEnabled(bNewDebugMode);
                
                GConfig->Flush(false, GEditorPerProjectIni);
                
                // Reinitialize MCP with new mode (this properly shuts down, clears state, and rediscovers tools)
                ChatSession->ReinitializeMCP(bNewEngineMode);
                
                SetStatusText(TEXT("Settings saved"));
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
    if (CommitType == ETextCommit::OnEnter)
    {
        OnSendClicked();
    }
}

FReply SAIChatWindow::OnInputKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    // Enter without Shift sends the message
    // Shift+Enter inserts a new line (default behavior)
    if (InKeyEvent.GetKey() == EKeys::Enter && !InKeyEvent.IsShiftDown())
    {
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
        UE_LOG(LogAIChatWindow, Log, TEXT("Selected model: %s"), *NewSelection->Id);
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
    
    AddMessageWidget(Message, ChatSession->GetMessages().Num() - 1);
    ScrollToBottom();
    UpdateUIState();
}

void SAIChatWindow::HandleMessageUpdated(int32 Index, const FChatMessage& Message)
{
    // Check if this message has a widget yet (it may have been skipped as empty streaming)
    TSharedPtr<STextBlock>* TextBlockPtr = MessageTextBlocks.Find(Index);
    if (!TextBlockPtr)
    {
        // Widget doesn't exist - add it now that we have content or tool calls
        AddMessageWidget(Message, Index);
    }
    else
    {
        UpdateMessageWidget(Index, Message);
    }
    
    // When streaming finishes and debug mode is enabled, show usage stats
    if (!Message.bIsStreaming && Message.Role == TEXT("assistant") && FChatSession::IsDebugModeEnabled())
    {
        const FLLMUsageStats& Stats = ChatSession->GetUsageStats();
        if (Stats.RequestCount > 0)
        {
            SetStatusText(FString::Printf(TEXT("Requests: %d | Tokens: %d prompt, %d completion | Session: %d total"),
                Stats.RequestCount, Stats.TotalPromptTokens, Stats.TotalCompletionTokens,
                Stats.TotalPromptTokens + Stats.TotalCompletionTokens));
        }
    }
    
    ScrollToBottom();
    UpdateUIState();
}

void SAIChatWindow::HandleChatReset()
{
    RebuildMessageList();
    UpdateUIState();
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
        
        UE_LOG(LogAIChatWindow, Log, TEXT("Filtered to %d models with tool support (from %d total)"), 
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
        
        UE_LOG(LogAIChatWindow, Log, TEXT("Loaded %d models with tool support (from %d total)"), 
            AvailableModels.Num(), Models.Num());
    }
    else
    {
        SetStatusText(TEXT("Failed to fetch models"));
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
            UE_LOG(LogAIChatWindow, Log, TEXT("MCP tools ready: %d tools available"), ToolCount);
        }
        else
        {
            MCPToolsText->SetText(FText::FromString(TEXT("Tools: 0")));
            MCPToolsText->SetColorAndOpacity(FSlateColor(VibeUEColors::TextMuted)); // Muted for no tools
            UE_LOG(LogAIChatWindow, Log, TEXT("MCP tools: none available"));
        }
    }
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

void SAIChatWindow::CopyMessageToClipboard(int32 MessageIndex)
{
    const TArray<FChatMessage>& Messages = ChatSession->GetMessages();
    if (Messages.IsValidIndex(MessageIndex))
    {
        FPlatformApplicationMisc::ClipboardCopy(*Messages[MessageIndex].Content);
        SetStatusText(TEXT("Copied to clipboard"));
    }
}
