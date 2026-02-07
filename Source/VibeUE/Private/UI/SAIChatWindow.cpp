// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "UI/SAIChatWindow.h"
#include "UI/ChatRichTextStyles.h"
#include "UI/MarkdownToRichText.h"
#include "Chat/AIChatCommands.h"
#include "Chat/ChatSession.h"
#include "Chat/MCPClient.h"
#include "MCP/MCPServer.h"
#include "Core/ToolRegistry.h"
#include "Chat/ILLMClient.h"
#include "Chat/VibeUEAPIClient.h"
#include "Speech/SpeechToTextService.h"
#include "Speech/ElevenLabsSpeechProvider.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
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
#include "Misc/Base64.h"
#include "HAL/PlatformTime.h"
#include "Editor.h"
#include "DesktopPlatformModule.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Engine/Texture2D.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

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
    const FLinearColor UserMessage(0.055f, 0.094f, 0.102f, 1.0f);  // User messages - #0E181A
    const FLinearColor AssistantMessage(0.0f, 0.0f, 0.0f, 0.0f);   // Assistant - transparent (no background)
    const FLinearColor ToolMessage(0.12f, 0.12f, 0.12f, 1.0f);     // Tool - dark gray
    const FLinearColor SystemMessage(0.22f, 0.08f, 0.08f, 1.0f);   // System/Error - dark red
    
    // Border/highlight
    const FLinearColor Border(0.2f, 0.2f, 0.25f, 1.0f);
    const FLinearColor BorderHighlight(0.0f, 0.7f, 0.7f, 0.5f);    // Cyan highlight border
    
    // Model rating colors (matching website)
    const FLinearColor RatingGreat(0.13f, 0.55f, 0.13f, 1.0f);     // Green-700 for "great"
    const FLinearColor RatingGood(0.2f, 0.72f, 0.35f, 1.0f);       // Green-500 for "good"
    const FLinearColor RatingModerate(0.85f, 0.75f, 0.1f, 1.0f);   // Yellow-500 for "moderate"
    const FLinearColor RatingBad(0.86f, 0.2f, 0.2f, 1.0f);         // Red-600 for "bad"
    const FLinearColor Gold(1.0f, 0.84f, 0.0f, 1.0f);              // Gold for star icon
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
    ChatSession->OnToolsReady.BindSP(this, &SAIChatWindow::HandleToolsReady);
    ChatSession->OnSummarizationStarted.BindSP(this, &SAIChatWindow::HandleSummarizationStarted);
    ChatSession->OnSummarizationComplete.BindSP(this, &SAIChatWindow::HandleSummarizationComplete);
    ChatSession->OnTokenBudgetUpdated.BindSP(this, &SAIChatWindow::HandleTokenBudgetUpdated);
    ChatSession->OnToolIterationLimitReached.BindSP(this, &SAIChatWindow::HandleToolIterationLimitReached);
    ChatSession->OnLLMThinkingStarted.BindSP(this, &SAIChatWindow::HandleLLMThinkingStarted);
    ChatSession->OnLLMThinkingComplete.BindSP(this, &SAIChatWindow::HandleLLMThinkingComplete);

    // Voice input delegates
    ChatSession->OnVoiceInputStarted.BindSP(this, &SAIChatWindow::OnVoiceInputStarted);
    ChatSession->OnVoiceInputText.BindSP(this, &SAIChatWindow::OnVoiceInputText);
    ChatSession->OnVoiceInputStopped.BindSP(this, &SAIChatWindow::OnVoiceInputStopped);
    ChatSession->OnVoiceInputAutoSent.BindSP(this, &SAIChatWindow::OnVoiceInputAutoSent);

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
                        SAssignNew(ToolsCountText, STextBlock)
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
                    
                    // Reset button (icon) - First
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0, 0, 4, 0)
                    [
                        SNew(SButton)
                        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                        .ContentPadding(FMargin(4))
                        .ToolTipText(FText::FromString(TEXT("Reset - Clear conversation history")))
                        .OnClicked(this, &SAIChatWindow::OnResetClicked)
                        [
                            SNew(SImage)
                            .Image(FAppStyle::GetBrush("Icons.Refresh"))
                            .ColorAndOpacity(FSlateColor(VibeUEColors::TextSecondary))
                            .DesiredSizeOverride(FVector2D(16, 16))
                        ]
                    ]
                    
                    // Tools button (icon) - puzzle piece / plug icon
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0, 0, 4, 0)
                    [
                        SNew(SButton)
                        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                        .ContentPadding(FMargin(4))
                        .ToolTipText(FText::FromString(TEXT("Manage Tools - Enable/Disable AI tools")))
                        .OnClicked(this, &SAIChatWindow::OnToolsClicked)
                        [
                            SNew(SImage)
                            .Image(FAppStyle::GetBrush("Icons.Package"))
                            .ColorAndOpacity(FSlateColor(VibeUEColors::TextSecondary))
                            .DesiredSizeOverride(FVector2D(16, 16))
                        ]
                    ]
                    
                    // Settings button (icon) - gear
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                        .ContentPadding(FMargin(4))
                        .ToolTipText(FText::FromString(TEXT("Settings - Configure API key and preferences")))
                        .OnClicked(this, &SAIChatWindow::OnSettingsClicked)
                        [
                            SNew(SImage)
                            .Image(FAppStyle::GetBrush("Icons.Toolbar.Settings"))
                            .ColorAndOpacity(FSlateColor(VibeUEColors::TextSecondary))
                            .DesiredSizeOverride(FVector2D(16, 16))
                        ]
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
                SNew(SVerticalBox)

                // Image preview (shown when image is attached)
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 4)
                [
                    SAssignNew(ImagePreviewContainer, SBox)
                    .Visibility(EVisibility::Collapsed)
                    [
                        SNew(SBorder)
                        .BorderBackgroundColor(VibeUEColors::Border)
                        .Padding(4)
                        [
                            SNew(SHorizontalBox)

                            // Image thumbnail
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SBox)
                                .WidthOverride(64.0f)
                                .HeightOverride(64.0f)
                                [
                                    SAssignNew(ImagePreviewWidget, SImage)
                                ]
                            ]

                            // "Image attached" label
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .VAlign(VAlign_Center)
                            .Padding(8, 0, 0, 0)
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Image attached")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                                .ColorAndOpacity(FSlateColor(VibeUEColors::TextSecondary))
                            ]

                            // Remove button (X)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Top)
                            [
                                SAssignNew(RemoveAttachmentButton, SButton)
                                .Text(FText::FromString(TEXT("✕")))
                                .ToolTipText(FText::FromString(TEXT("Remove attached image")))
                                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                                .OnClicked(this, &SAIChatWindow::OnRemoveAttachmentClicked)
                            ]
                        ]
                    ]
                ]

                // Input row
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SHorizontalBox)

                    // Attachment button (paperclip)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(0, 0, 4, 0)
                    .VAlign(VAlign_Center)
                    [
                        SAssignNew(AttachmentButton, SButton)
                        .Text(FText::FromString(TEXT("📎")))
                        .ToolTipText(FText::FromString(TEXT("Attach an image (or paste with Ctrl+V)")))
                        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                        .OnClicked(this, &SAIChatWindow::OnAttachmentClicked)
                    ]

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

                    // Microphone button for voice input (push-to-talk)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(4, 0, 0, 0)
                    .VAlign(VAlign_Center)
                    [
                        SNew(SBox)
                        [
                            SAssignNew(MicrophoneButton, SButton)
                            .Text(this, &SAIChatWindow::GetMicrophoneButtonText)
                            .ToolTipText(this, &SAIChatWindow::GetMicrophoneTooltip)
                            .IsEnabled(this, &SAIChatWindow::IsMicrophoneEnabled)
                            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                            .OnPressed(this, &SAIChatWindow::OnMicrophonePressed)
                            .OnReleased(this, &SAIChatWindow::OnMicrophoneReleased)
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
        ]
    ];
    
    // Rebuild message list from history
    RebuildMessageList();
    
    // Update model dropdown based on current provider
    UpdateModelDropdownForProvider();
    
    // Initialize MCP
    ChatSession->InitializeMCP();
    
    // Check API key
    if (!ChatSession->HasApiKey())
    {
        FLLMProviderInfo ProviderInfo = ChatSession->GetCurrentProviderInfo();
        AddSystemNotification(FString::Printf(TEXT("⚠️ Please set your %s API key in Settings"), *ProviderInfo.DisplayName));
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

bool SAIChatWindow::AttachImageFromPath(const FString& FilePath)
{
    if (!WidgetInstance.IsValid())
    {
        UE_LOG(LogAIChatWindow, Warning, TEXT("Cannot attach image - chat window not open"));
        return false;
    }

    // Verify file exists
    if (!FPaths::FileExists(FilePath))
    {
        UE_LOG(LogAIChatWindow, Warning, TEXT("Cannot attach image - file not found: %s"), *FilePath);
        return false;
    }

    // Verify it's a supported image format
    FString Extension = FPaths::GetExtension(FilePath).ToLower();
    if (Extension != TEXT("png") && Extension != TEXT("jpg") && Extension != TEXT("jpeg") && Extension != TEXT("bmp"))
    {
        UE_LOG(LogAIChatWindow, Warning, TEXT("Cannot attach image - unsupported format: %s"), *Extension);
        return false;
    }

    // Attach the image
    WidgetInstance->AttachImageFromFile(FilePath);
    return WidgetInstance->HasAttachedImage();
}

bool SAIChatWindow::HasImageAttached()
{
    if (!WidgetInstance.IsValid())
    {
        return false;
    }
    return WidgetInstance->HasAttachedImage();
}

void SAIChatWindow::ClearImageAttachment()
{
    if (WidgetInstance.IsValid())
    {
        WidgetInstance->ClearAttachedImage();
    }
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

    // For tool calls, display the message content first (if any), then create tool call widgets
    if (bIsToolCall)
    {
        // If there's text content before the tool calls, display it first
        if (!Message.Content.IsEmpty())
        {
            // Continue below to create the message widget with the content
            // Don't return yet - we'll add tool calls after the message
        }
        else
        {
            // No text content, just create tool call widgets
            for (int32 ToolIdx = 0; ToolIdx < Message.ToolCalls.Num(); ToolIdx++)
            {
                AddToolCallWidget(Message.ToolCalls[ToolIdx], Index, ToolIdx);
            }
            return;
        }
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
        BorderColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);  // Transparent - no accent line
    }
    else if (Message.Role == TEXT("assistant"))
    {
        BackgroundColor = VibeUEColors::AssistantMessage;
        BorderColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);  // Transparent - no accent line
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

    // Convert markdown to rich text format
    FString RichText = FMarkdownToRichText::Convert(DisplayText, Message.bIsStreaming);

    // Create the message content rich text block and store reference for streaming updates
    TSharedPtr<SRichTextBlock> ContentRichTextBlock;

    // Create the message bubble with rounded corners
    TSharedRef<SWidget> MessageContent =
        SNew(SBorder)
        .BorderImage(&RoundedBrush)
        .BorderBackgroundColor(BackgroundColor)
        .Padding(FMargin(12, 10, 12, 10))
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

            // Message content - fills available space with markdown rendering
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SAssignNew(ContentRichTextBlock, SRichTextBlock)
                .Text(FText::FromString(RichText))
                .AutoWrapText(true)
                .DecoratorStyleSet(&FChatRichTextStyles::Get())
                .TextStyle(&FChatRichTextStyles::Get(), FChatRichTextStyles::Style_Default)
                .LineHeightPercentage(1.5f)  // 50% more line height for readability
                + SRichTextBlock::HyperlinkDecorator(TEXT("a"), FSlateHyperlinkRun::FOnClick::CreateSP(this, &SAIChatWindow::HandleHyperlinkClicked))
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
                .OnClicked_Lambda([this, Index, MessageContent = Message.Content]() -> FReply
                {
                    // For system notifications (negative index), copy the captured content
                    // For regular messages, use the message index to get current content
                    if (Index < 0)
                    {
                        FPlatformApplicationMisc::ClipboardCopy(*MessageContent);
                    }
                    else
                    {
                        CopyMessageToClipboard(Index);
                    }
                    return FReply::Handled();
                })
            ]
        ];
    
    // User messages: right-aligned with max width
    // Assistant messages: left-aligned, fill available width
    if (Message.Role == TEXT("user"))
    {
        MessageScrollBox->AddSlot()
        .Padding(10)
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
        .Padding(10)
        [
            MessageContent
        ];
    }
    
    // Store reference for streaming updates
    MessageTextBlocks.Add(Index, ContentRichTextBlock);

    // If this was a tool call message with content, now add the tool call widgets after the message
    if (bIsToolCall && !Message.Content.IsEmpty())
    {
        for (int32 ToolIdx = 0; ToolIdx < Message.ToolCalls.Num(); ToolIdx++)
        {
            AddToolCallWidget(Message.ToolCalls[ToolIdx], Index, ToolIdx);
        }
    }
}

void SAIChatWindow::AddSystemNotification(const FString& Message)
{
    // Create a simple system notification that appears in chat but isn't part of conversation
    FChatMessage SystemMsg(TEXT("system"), Message);
    SystemMsg.bIsStreaming = false;
    
    // Use a negative index to avoid conflicts with real messages
    static int32 NotificationCounter = -1000;
    int32 NotificationIndex = NotificationCounter--;
    
    AddMessageWidget(SystemMsg, NotificationIndex);
    ScrollToBottom();
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
                        .Text(FText::FromString(WidgetData.CallJson))
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
    
    // Force Slate to invalidate and redraw the widget immediately
    // This ensures the yellow arrow (→) is visible before tool execution completes
    CompactWidget->Invalidate(EInvalidateWidgetReason::Paint);

    // Start the rotating arrow animation for this tool call (Phase 2)
    StartToolStatusAnimation(UniqueKey);

    ScrollToBottom();
}

void SAIChatWindow::UpdateToolCallWithResponse(const FString& ToolCallId, const FString& ResponseJson, bool bSuccess)
{
    
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

    // Stop the rotating arrow animation (Phase 2)
    StopToolStatusAnimation(UniqueKey);

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
        
        // Force immediate repaint to show the status change
        WidgetData->StatusText->Invalidate(EInvalidateWidgetReason::Paint);
    }
    
    // Update response JSON text in the details section
    if (WidgetData->ResponseJsonText.IsValid())
    {
        FLinearColor TextColor = bSuccess ? VibeUEColors::Green : VibeUEColors::Red;
        
        WidgetData->ResponseJsonText->SetText(FText::FromString(ResponseJson));
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
    
    // Try to update just the rich text block instead of rebuilding
    TSharedPtr<SRichTextBlock>* RichTextBlockPtr = MessageTextBlocks.Find(Index);
    if (RichTextBlockPtr && RichTextBlockPtr->IsValid())
    {
        FString DisplayText = Message.Content;
        if (Message.bIsStreaming && DisplayText.IsEmpty())
        {
            DisplayText = TEXT("...");
        }
        // Convert markdown to rich text format
        FString RichText = FMarkdownToRichText::Convert(DisplayText, Message.bIsStreaming);
        (*RichTextBlockPtr)->SetText(FText::FromString(RichText));
    }
    else
    {
        // Fallback to rebuild if we don't have a reference
        RebuildMessageList();
    }
}

void SAIChatWindow::ScrollToBottom()
{
    // If thinking indicator is visible, move it to the bottom
    // This ensures it always appears below the latest content
    if (bThinkingIndicatorVisible && ThinkingIndicatorWidget.IsValid())
    {
        MessageScrollBox->RemoveSlot(ThinkingIndicatorWidget.ToSharedRef());
        MessageScrollBox->AddSlot()
        .Padding(FMargin(2, 4))
        [
            ThinkingIndicatorWidget.ToSharedRef()
        ];
    }

    MessageScrollBox->ScrollToEnd();
}

FReply SAIChatWindow::OnSendClicked()
{
    FString Message = InputTextBox->GetText().ToString();
    bool bHasImage = HasAttachedImage();

    // Allow sending if there's text OR an attached image
    if (!Message.IsEmpty() || bHasImage)
    {
        if (FChatSession::IsDebugModeEnabled())
        {
            CHAT_LOG(Log, TEXT("[UI EVENT] Send button clicked - Message: %s, HasImage: %s"),
                *Message.Left(100), bHasImage ? TEXT("Yes") : TEXT("No"));
        }

        // Clear any previous error message before sending new request
        // Status now shown via streaming indicator in chat

        InputTextBox->SetText(FText::GetEmpty());

        // Check if user typed "continue" to resume after iteration limit
        // Only intercept if we're actually waiting - otherwise pass through as normal message
        if (Message.TrimStartAndEnd().ToLower() == TEXT("continue") && ChatSession->IsWaitingForUserToContinue())
        {
            ChatSession->ContinueAfterIterationLimit();
            // Status animation will start when user message is added to UI
        }
        else if (bHasImage)
        {
            // Send message with attached image
            ChatSession->SendMessageWithImage(Message, AttachedImageDataUrl);
            ClearAttachedImage();
            // Status animation will start when user message is added to UI
        }
        else
        {
            ChatSession->SendMessage(Message);
            // Status animation will start when user message is added to UI
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
        // Cancellation reflected in chat UI
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

void SAIChatWindow::CloseToolsPopup()
{
    if (TSharedPtr<SWindow> Window = ToolsPopupWindow.Pin())
    {
        Window->RequestDestroyWindow();
    }
    ToolsPopupWindow.Reset();
}

FReply SAIChatWindow::OnToolsClicked()
{
    // Close existing popup if open
    CloseToolsPopup();
    
    // Get all tools from registry
    FToolRegistry& Registry = FToolRegistry::Get();
    const TArray<FToolMetadata>& AllTools = Registry.GetAllTools();
    
    // Get MCP tools directly from MCP client (not merged list)
    // IMPORTANT: Get ALL MCP tools regardless of enabled state for display in popup
    TArray<FMCPTool> MCPTools;
    if (ChatSession.IsValid())
    {
        bool bMCPInitialized = ChatSession->IsMCPInitialized();
        UE_LOG(LogAIChatWindow, Log, TEXT("Tools popup: MCP initialized = %s"), bMCPInitialized ? TEXT("YES") : TEXT("NO"));
        
        if (bMCPInitialized && ChatSession->GetMCPClient().IsValid())
        {
            // Get MCP tools directly from client (external server tools only)
            // Note: GetMCPTools() returns only MCP tools, not internal tools
            const TArray<FMCPTool>& AllMCPTools = ChatSession->GetMCPClient()->GetMCPTools();
            MCPTools = AllMCPTools;
            UE_LOG(LogAIChatWindow, Log, TEXT("Tools popup: Found %d MCP tools from client"), MCPTools.Num());
            
            // Log each MCP tool for debugging
            for (const FMCPTool& Tool : MCPTools)
            {
                bool bEnabled = Registry.IsToolEnabled(Tool.Name);
                UE_LOG(LogAIChatWindow, Log, TEXT("  MCP Tool: %s (Server: %s, Enabled: %s)"), 
                    *Tool.Name, *Tool.ServerName, bEnabled ? TEXT("YES") : TEXT("NO"));
            }
        }
        else
        {
            UE_LOG(LogAIChatWindow, Warning, TEXT("Tools popup: MCP not initialized or client invalid, no MCP tools available"));
        }
    }
    
    // Count enabled internal tools
    int32 EnabledInternalCount = 0;
    for (const FToolMetadata& Tool : AllTools)
    {
        if (Registry.IsToolEnabled(Tool.Name))
        {
            EnabledInternalCount++;
        }
    }
    
    // Create a shared map to track pending tool state changes
    // This is modified by checkboxes and applied when Save is clicked
    TSharedPtr<TMap<FString, bool>> PendingToolStates = MakeShared<TMap<FString, bool>>();
    
    // Initialize with current states
    for (const FToolMetadata& Tool : AllTools)
    {
        PendingToolStates->Add(Tool.Name, Registry.IsToolEnabled(Tool.Name));
    }
    for (const FMCPTool& Tool : MCPTools)
    {
        if (Tool.ServerName != TEXT("Internal"))
        {
            PendingToolStates->Add(Tool.Name, Registry.IsToolEnabled(Tool.Name));
        }
    }
    
    // Create the tools popup window
    TSharedRef<SWindow> PopupWindow = SNew(SWindow)
        .Title(FText::FromString(TEXT("VibeUE Tools")))
        .ClientSize(FVector2D(500, 600))
        .SupportsMinimize(false)
        .SupportsMaximize(false)
        .IsTopmostWindow(true);
    
    // Build tools list content
    TSharedRef<SVerticalBox> ToolsListBox = SNew(SVerticalBox);
    
    // Shared pointer for updating tool count text
    TSharedPtr<STextBlock> ToolCountText;
    
    // Internal tools section
    if (AllTools.Num() > 0)
    {
        ToolsListBox->AddSlot()
        .AutoHeight()
        .Padding(8, 8, 8, 4)
        [
            SAssignNew(ToolCountText, STextBlock)
            .Text(FText::FromString(FString::Printf(TEXT("Internal Tools (%d/%d enabled)"), EnabledInternalCount, AllTools.Num())))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
            .ColorAndOpacity(FSlateColor(VibeUEColors::Cyan))
        ];
        
        for (const FToolMetadata& Tool : AllTools)
        {
            // Capture tool name for lambda
            FString ToolName = Tool.Name;
            bool bIsEnabled = Registry.IsToolEnabled(ToolName);
            
            // Create checkbox with dynamic state
            TSharedPtr<SCheckBox> ToolCheckBox;
            
            ToolsListBox->AddSlot()
            .AutoHeight()
            .Padding(12, 4, 8, 4)
            [
                SNew(SBorder)
                .BorderBackgroundColor(bIsEnabled ? FLinearColor(0.1f, 0.12f, 0.14f, 1.0f) : FLinearColor(0.08f, 0.08f, 0.08f, 1.0f))
                .Padding(6)
                [
                    SNew(SHorizontalBox)
                    
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0, 0, 8, 0)
                    [
                        SAssignNew(ToolCheckBox, SCheckBox)
                        .IsChecked(bIsEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
                        .ToolTipText(FText::FromString(FString::Printf(TEXT("Enable/Disable %s"), *Tool.Name)))
                        .OnCheckStateChanged_Lambda([ToolName, PendingToolStates](ECheckBoxState NewState)
                        {
                            bool bNewEnabled = (NewState == ECheckBoxState::Checked);
                            UE_LOG(LogAIChatWindow, Log, TEXT("Checkbox changed for tool '%s': bNewEnabled=%s (pending)"), 
                                *ToolName, bNewEnabled ? TEXT("true") : TEXT("false"));
                            PendingToolStates->Add(ToolName, bNewEnabled);
                        })
                    ]
                    
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(SVerticalBox)
                        
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(Tool.Name))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                            .ColorAndOpacity(FSlateColor(bIsEnabled ? VibeUEColors::TextPrimary : VibeUEColors::TextMuted))
                        ]
                        
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(Tool.Description.Left(100) + (Tool.Description.Len() > 100 ? TEXT("...") : TEXT(""))))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                            .ColorAndOpacity(FSlateColor(VibeUEColors::TextMuted))
                            .AutoWrapText(true)
                        ]
                    ]
                    
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(8, 0, 0, 0)
                    [
                        SNew(SBorder)
                        .BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.18f, 1.0f))
                        .Padding(FMargin(6, 2))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(Tool.Category))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                            .ColorAndOpacity(FSlateColor(VibeUEColors::TextSecondary))
                        ]
                    ]
                ]
            ];
        }
    }
    
    // MCP tools section (tools from external servers)
    int32 ExternalToolCount = 0;
    for (const FMCPTool& Tool : MCPTools)
    {
        if (Tool.ServerName != TEXT("Internal")) // Skip internal tools
        {
            if (ExternalToolCount == 0)
            {
                ToolsListBox->AddSlot()
                .AutoHeight()
                .Padding(8, 16, 8, 4)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("MCP Tools (External)")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                    .ColorAndOpacity(FSlateColor(VibeUEColors::Green))
                ];
            }
            
            ExternalToolCount++;
            FString MCPToolName = Tool.Name;
            bool bMCPEnabled = Registry.IsToolEnabled(MCPToolName);
            
            ToolsListBox->AddSlot()
            .AutoHeight()
            .Padding(12, 4, 8, 4)
            [
                SNew(SBorder)
                .BorderBackgroundColor(bMCPEnabled ? FLinearColor(0.1f, 0.12f, 0.14f, 1.0f) : FLinearColor(0.08f, 0.08f, 0.08f, 1.0f))
                .Padding(6)
                [
                    SNew(SHorizontalBox)
                    
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0, 0, 8, 0)
                    [
                        SNew(SCheckBox)
                        .IsChecked(bMCPEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
                        .ToolTipText(FText::FromString(FString::Printf(TEXT("Enable/Disable %s"), *Tool.Name)))
                        .OnCheckStateChanged_Lambda([MCPToolName, PendingToolStates](ECheckBoxState NewState)
                        {
                            bool bNewEnabled = (NewState == ECheckBoxState::Checked);
                            UE_LOG(LogAIChatWindow, Log, TEXT("MCP Checkbox changed for tool '%s': bNewEnabled=%s (pending)"), 
                                *MCPToolName, bNewEnabled ? TEXT("true") : TEXT("false"));
                            PendingToolStates->Add(MCPToolName, bNewEnabled);
                        })
                    ]
                    
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(SVerticalBox)
                        
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(Tool.Name))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                            .ColorAndOpacity(FSlateColor(bMCPEnabled ? VibeUEColors::TextPrimary : VibeUEColors::TextMuted))
                        ]
                        
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(Tool.Description.Left(100) + (Tool.Description.Len() > 100 ? TEXT("...") : TEXT(""))))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                            .ColorAndOpacity(FSlateColor(VibeUEColors::TextMuted))
                            .AutoWrapText(true)
                        ]
                    ]
                    
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(8, 0, 0, 0)
                    [
                        SNew(SBorder)
                        .BorderBackgroundColor(FLinearColor(0.1f, 0.15f, 0.1f, 1.0f))
                        .Padding(FMargin(6, 2))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(Tool.ServerName))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                            .ColorAndOpacity(FSlateColor(VibeUEColors::Green))
                        ]
                    ]
                ]
            ];
        }
    }
    
    // Capture this pointer for Save button lambda
    SAIChatWindow* Self = this;
    TWeakPtr<SWindow> WeakPopupWindow = PopupWindow;
    
    // Build the popup content
    PopupWindow->SetContent(
        SNew(SBorder)
        .BorderBackgroundColor(VibeUEColors::Background)
        .Padding(0)
        [
            SNew(SVerticalBox)
            
            // Header
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBorder)
                .BorderBackgroundColor(VibeUEColors::BackgroundLight)
                .Padding(12)
                [
                    SNew(SHorizontalBox)
                    
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(FString::Printf(TEXT("Tool Manager - %d Internal, %d MCP"), AllTools.Num(), ExternalToolCount)))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                        .ColorAndOpacity(FSlateColor(VibeUEColors::TextPrimary))
                    ]
                ]
            ]
            
            // Scrollable tools list
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SNew(SScrollBox)
                + SScrollBox::Slot()
                [
                    ToolsListBox
                ]
            ]
            
            // Footer with Save button
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(8)
            [
                SNew(SVerticalBox)
                
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 8)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Click Save to apply changes. Disabled tools won't be used by AI.")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                    .ColorAndOpacity(FSlateColor(VibeUEColors::TextMuted))
                ]
                
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SHorizontalBox)
                    
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SButton)
                        .Text(FText::FromString(TEXT("Save & Close")))
                        .OnClicked_Lambda([Self, PendingToolStates, WeakPopupWindow]() -> FReply
                        {
                            UE_LOG(LogAIChatWindow, Log, TEXT("=== SAVE BUTTON CLICKED ==="));
                            
                            // Build the new disabled tools set directly
                            TSet<FString> NewDisabledTools;
                            for (const auto& Pair : *PendingToolStates)
                            {
                                UE_LOG(LogAIChatWindow, Log, TEXT("  Tool '%s' -> %s"), 
                                    *Pair.Key, Pair.Value ? TEXT("ENABLED") : TEXT("DISABLED"));
                                if (!Pair.Value) // If NOT enabled, add to disabled set
                                {
                                    NewDisabledTools.Add(Pair.Key);
                                }
                            }
                            
                            // Use the new bulk update method that bypasses change detection
                            FToolRegistry& Reg = FToolRegistry::Get();
                            Reg.SetDisabledToolsAndSave(NewDisabledTools);
                            
                            UE_LOG(LogAIChatWindow, Log, TEXT("Applied %d tool states, %d now disabled"), 
                                PendingToolStates->Num(), NewDisabledTools.Num());
                            
                            // Update the main chat window's tool count
                            if (Self && Self->ChatSession.IsValid())
                            {
                                Self->HandleToolsReady(true, Self->ChatSession->GetEnabledToolCount());
                            }
                            
                            // Close the popup
                            if (TSharedPtr<SWindow> StrongWindow = WeakPopupWindow.Pin())
                            {
                                StrongWindow->RequestDestroyWindow();
                            }
                            
                            return FReply::Handled();
                        })
                    ]
                    
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(8, 0, 0, 0)
                    [
                        SNew(SButton)
                        .Text(FText::FromString(TEXT("Cancel")))
                        .OnClicked_Lambda([WeakPopupWindow]() -> FReply
                        {
                            if (TSharedPtr<SWindow> StrongWindow = WeakPopupWindow.Pin())
                            {
                                StrongWindow->RequestDestroyWindow();
                            }
                            return FReply::Handled();
                        })
                    ]
                ]
            ]
        ]
    );
    
    // Show the popup
    FSlateApplication::Get().AddWindow(PopupWindow);
    ToolsPopupWindow = PopupWindow;
    
    return FReply::Handled();
}

FReply SAIChatWindow::OnSettingsClicked()
{
    // Show API key input dialog
    TSharedRef<SWindow> SettingsWindow = SNew(SWindow)
        .Title(FText::FromString(TEXT("VibeUE AI Chat Settings")))
        .ClientSize(FVector2D(500, 900))
        .SupportsMinimize(false)
        .SupportsMaximize(false);
    
    TSharedPtr<SEditableTextBox> VibeUEApiKeyInput;
    TSharedPtr<SEditableTextBox> OpenRouterApiKeyInput;
    TSharedPtr<SCheckBox> DebugModeCheckBox;
    TSharedPtr<SCheckBox> AutoSaveBeforePythonCheckBox;
    TSharedPtr<SCheckBox> ParallelToolCallsCheckBox;
    TSharedPtr<SSpinBox<float>> TemperatureSpinBox;
    TSharedPtr<SSpinBox<float>> TopPSpinBox;
    TSharedPtr<SSpinBox<int32>> MaxTokensSpinBox;
    TSharedPtr<SSpinBox<int32>> MaxToolIterationsSpinBox;

    // MCP Server settings widgets
    TSharedPtr<SCheckBox> MCPServerEnabledCheckBox;
    TSharedPtr<SSpinBox<int32>> MCPServerPortSpinBox;
    TSharedPtr<SEditableTextBox> MCPServerApiKeyInput;
    
    // Load current LLM parameter values
    float CurrentTemperature = FChatSession::GetTemperatureFromConfig();
    float CurrentTopP = FChatSession::GetTopPFromConfig();
    int32 CurrentMaxTokens = FChatSession::GetMaxTokensFromConfig();
    bool bCurrentParallelToolCalls = FChatSession::GetParallelToolCallsFromConfig();
    int32 CurrentMaxToolIterations = FChatSession::GetMaxToolCallIterationsFromConfig();

    // Load current MCP Server settings
    bool bMCPServerEnabled = FMCPServer::GetEnabledFromConfig();
    int32 MCPServerPort = FMCPServer::GetPortFromConfig();
    FString MCPServerApiKey = FMCPServer::GetApiKeyFromConfig();

    // Load current auto-save setting
    bool bCurrentAutoSaveBeforePython = FChatSession::IsAutoSaveBeforePythonExecutionEnabled();
    
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

        // Auto-save before Python execution
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 8, 8, 4)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SAssignNew(AutoSaveBeforePythonCheckBox, SCheckBox)
                .IsChecked(bCurrentAutoSaveBeforePython ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
            ]
            + SHorizontalBox::Slot()
            .Padding(4, 0, 0, 0)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Auto Save Before Python Execution")))
                .ToolTipText(FText::FromString(TEXT("Automatically save all dirty packages before executing Python code to protect against crashes.")))
            ]
        ]

        // ============ Voice Input Settings ============
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 16, 8, 4)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Voice Input Settings:")))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
        ]

        // Enable voice input checkbox
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 8, 8, 4)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SAssignNew(VoiceInputEnabledCheckBox, SCheckBox)
                .IsChecked(FSpeechToTextService::GetVoiceInputEnabledFromConfig()
                    ? ECheckBoxState::Checked
                    : ECheckBoxState::Unchecked)
            ]
            + SHorizontalBox::Slot()
            .Padding(4, 0, 0, 0)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Enable Voice Input")))
            ]
        ]

        // Auto-send after recording checkbox
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 8, 8, 4)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SAssignNew(AutoSendAfterRecordingCheckBox, SCheckBox)
                .IsChecked(ChatSession.IsValid() && ChatSession->IsAutoSendAfterRecordingEnabled()
                    ? ECheckBoxState::Checked
                    : ECheckBoxState::Unchecked)
            ]
            + SHorizontalBox::Slot()
            .Padding(4, 0, 0, 0)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Auto Send After Recording")))
                .ToolTipText(FText::FromString(TEXT("Automatically send transcribed text to AI without review")))
            ]
        ]

        // ElevenLabs API Key
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 8, 8, 0)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("ElevenLabs API Key:")))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 0)
        [
            SAssignNew(ElevenLabsApiKeyInput, SEditableTextBox)
            .Text(FText::FromString(FElevenLabsSpeechProvider::GetApiKeyFromConfig()))
            .IsPassword(true)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 4, 8, 0)
        [
            SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .OnClicked_Lambda([]() -> FReply {
                FPlatformProcess::LaunchURL(TEXT("https://elevenlabs.io/app/settings/api-keys"), nullptr, nullptr);
                return FReply::Handled();
            })
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Get ElevenLabs API key at elevenlabs.io")))
                .ColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.5f, 1.0f)))
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
        // ============ MCP Server Settings ============
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 16, 8, 4)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("MCP Server (Expose Tools to External Clients):")))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
        ]
        // MCP Server Enabled
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 4)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SAssignNew(MCPServerEnabledCheckBox, SCheckBox)
                .IsChecked(bMCPServerEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
            ]
            + SHorizontalBox::Slot()
            .Padding(4, 0, 0, 0)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Enable MCP Server")))
                .ToolTipText(FText::FromString(TEXT("Expose internal tools via Streamable HTTP for VS Code, Cursor, Claude Desktop, etc.")))
            ]
        ]
        // MCP Server Port
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
                .Text(FText::FromString(TEXT("Port:")))
                .ToolTipText(FText::FromString(TEXT("Port for the MCP HTTP server. Default: 8080")))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(0.6f)
            [
                SAssignNew(MCPServerPortSpinBox, SSpinBox<int32>)
                .MinValue(1024)
                .MaxValue(65535)
                .Delta(1)
                .Value(MCPServerPort)
                .MinDesiredWidth(100)
            ]
        ]
        // MCP Server API Key
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 4, 8, 0)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("API Key (optional):")))
            .ToolTipText(FText::FromString(TEXT("Require this API key in requests. Leave empty for no authentication.")))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 0)
        [
            SAssignNew(MCPServerApiKeyInput, SEditableTextBox)
            .Text(FText::FromString(MCPServerApiKey))
            .IsPassword(true)
            .HintText(FText::FromString(TEXT("Leave empty for no auth")))
        ]
        // MCP Server Status
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8, 8, 8, 0)
        [
            SNew(STextBlock)
            .Text_Lambda([]() -> FText {
                FMCPServer& Server = FMCPServer::Get();
                if (Server.IsRunning())
                {
                    return FText::FromString(FString::Printf(TEXT("Status: Running at %s"), *Server.GetServerUrl()));
                }
                return FText::FromString(TEXT("Status: Not running"));
            })
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
            .ColorAndOpacity_Lambda([]() -> FSlateColor {
                return FMCPServer::Get().IsRunning() 
                    ? FSlateColor(FLinearColor(0.2f, 0.8f, 0.2f)) 
                    : FSlateColor(VibeUEColors::TextMuted);
            })
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Right)
        .Padding(8, 16, 8, 8)
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("Save")))
            .OnClicked_Lambda([this, VibeUEApiKeyInput, OpenRouterApiKeyInput, SelectedProviderPtr, DebugModeCheckBox, AutoSaveBeforePythonCheckBox, ParallelToolCallsCheckBox, TemperatureSpinBox, TopPSpinBox, MaxTokensSpinBox, MaxToolIterationsSpinBox, MCPServerEnabledCheckBox, MCPServerPortSpinBox, MCPServerApiKeyInput, SettingsWindow]() -> FReply
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
                
                // Save debug mode
                bool bNewDebugMode = DebugModeCheckBox->IsChecked();
                FChatSession::SetDebugModeEnabled(bNewDebugMode);

                // Save auto-save before Python execution setting
                bool bNewAutoSaveBeforePython = AutoSaveBeforePythonCheckBox->IsChecked();
                FChatSession::SetAutoSaveBeforePythonExecutionEnabled(bNewAutoSaveBeforePython);

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
                
                // Save MCP Server settings
                bool bNewMCPServerEnabled = MCPServerEnabledCheckBox->IsChecked();
                int32 NewMCPServerPort = MCPServerPortSpinBox->GetValue();
                FString NewMCPServerApiKey = MCPServerApiKeyInput->GetText().ToString();
                
                FMCPServer::SaveEnabledToConfig(bNewMCPServerEnabled);
                FMCPServer::SavePortToConfig(NewMCPServerPort);
                FMCPServer::SaveApiKeyToConfig(NewMCPServerApiKey);
                
                // Restart MCP Server if settings changed
                FMCPServer& MCPServer = FMCPServer::Get();
                MCPServer.LoadConfig();
                if (bNewMCPServerEnabled && !MCPServer.IsRunning())
                {
                    MCPServer.Start();
                }
                else if (!bNewMCPServerEnabled && MCPServer.IsRunning())
                {
                    MCPServer.StopServer();
                }

                // Save Voice Input settings
                bool bNewVoiceInputEnabled = VoiceInputEnabledCheckBox->IsChecked();
                bool bNewAutoSendAfterRecording = AutoSendAfterRecordingCheckBox->IsChecked();
                FString NewElevenLabsApiKey = ElevenLabsApiKeyInput->GetText().ToString();

                FSpeechToTextService::SaveVoiceInputEnabledToConfig(bNewVoiceInputEnabled);
                if (ChatSession.IsValid())
                {
                    ChatSession->SetAutoSendAfterRecordingEnabled(bNewAutoSendAfterRecording);
                }
                FElevenLabsSpeechProvider::SaveApiKeyToConfig(NewElevenLabsApiKey);

                // Update the provider with the new API key immediately
                if (ChatSession.IsValid())
                {
                    TSharedPtr<FSpeechToTextService> SpeechService = ChatSession->GetSpeechService();
                    if (SpeechService.IsValid())
                    {
                        TSharedPtr<ISpeechProvider> Provider = SpeechService->GetActiveProvider();
                        if (Provider.IsValid())
                        {
                            TSharedPtr<FElevenLabsSpeechProvider> ElevenLabsProvider = StaticCastSharedPtr<FElevenLabsSpeechProvider>(Provider);
                            if (ElevenLabsProvider.IsValid())
                            {
                                ElevenLabsProvider->SetApiKey(NewElevenLabsApiKey);
                            }
                        }
                    }
                }

                GConfig->Flush(false, GEditorPerProjectIni);
                
                // Update the model dropdown based on new provider
                UpdateModelDropdownForProvider();
                
                AddSystemNotification(FString::Printf(TEXT("✅ Settings saved - Using %s"), 
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

    // Ctrl+V - check for image in clipboard
    if (InKeyEvent.GetKey() == EKeys::V && InKeyEvent.IsControlDown())
    {
        // Try to paste image from clipboard
        AttachImageFromClipboard();
        // Don't return Handled - let the text paste continue if there's no image
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
    if (!Model.IsValid())
    {
        return SNew(STextBlock).Text(FText::FromString(TEXT("Unknown")));
    }
    
    FLinearColor TextColor = GetRatingColor(Model->Rating);
    
    // Build display string without star (star gets its own gold-colored text)
    FString DisplayStr;
    if (Model->IsFree())
    {
        DisplayStr = FString::Printf(TEXT("[FREE] %s (%dK)"), *Model->Name, Model->ContextLength / 1024);
    }
    else
    {
        DisplayStr = FString::Printf(TEXT("%s (%dK) $%.2f/1M"), *Model->Name, Model->ContextLength / 1024, Model->PricingPrompt);
    }
    
    if (Model->Rating == TEXT("great"))
    {
        // Gold star + colored model text
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0, 0, 4, 0)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("\u2B50")))
                .ColorAndOpacity(FSlateColor(VibeUEColors::Gold))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(DisplayStr))
                .ColorAndOpacity(FSlateColor(TextColor))
            ];
    }
    
    // Non-great models: just colored text
    return SNew(STextBlock)
        .Text(FText::FromString(DisplayStr))
        .ColorAndOpacity(FSlateColor(TextColor));
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
    UE_LOG(LogAIChatWindow, Log, TEXT("[HandleMessageAdded] Role: %s, Content length: %d"), *Message.Role, Message.Content.Len());
    
    // Don't process empty streaming assistant messages - they're just placeholders
    if (Message.Role == TEXT("assistant") && Message.bIsStreaming && Message.Content.IsEmpty() && Message.ToolCalls.Num() == 0)
    {
        // Skip - HandleMessageUpdated will handle it when content arrives
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
        // FIRST: Handle any text content the assistant provided alongside tool calls
        // The AI often explains what it's doing before/after tool calls
        if (!Message.Content.IsEmpty())
        {
            TSharedPtr<SRichTextBlock>* TextBlockPtr = MessageTextBlocks.Find(Index);
            if (!TextBlockPtr)
            {
                // Widget doesn't exist yet - add it for the content
                AddMessageWidget(Message, Index);
            }
            else
            {
                // Widget exists - update the content
                UpdateMessageWidget(Index, Message);
            }
        }
        
        // THEN: Handle tool call widgets
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
        
        if (!bAllToolsHaveWidgets)
        {
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
    TSharedPtr<SRichTextBlock>* TextBlockPtr = MessageTextBlocks.Find(Index);
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
    // Add error message to chat window
    AddSystemNotification(FString::Printf(TEXT("❌ Error: %s"), *ErrorMessage));
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
        
        // Fetch model ratings from VibeUE website to color-code and sort
        FetchModelRatings();
    }
    else
    {
        AddSystemNotification(TEXT("❌ Failed to fetch models"));
    }
}

void SAIChatWindow::FetchModelRatings()
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(TEXT("https://vibeue.com/api/models/ratings"));
    Request->SetVerb(TEXT("GET"));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    
    // Use BindSP with SharedThis to match the pattern that works in OpenRouterClient
    Request->OnProcessRequestComplete().BindSP(
        StaticCastSharedRef<SAIChatWindow>(AsShared()),
        &SAIChatWindow::HandleModelRatingsFetched);
    
    Request->ProcessRequest();
    CHAT_LOG(Log, TEXT("Fetching model ratings from VibeUE..."));
}

void SAIChatWindow::HandleModelRatingsFetched(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
    if (!bConnectedSuccessfully || !Response.IsValid())
    {
        CHAT_LOG(Warning, TEXT("Failed to fetch model ratings from VibeUE"));
        return;
    }
    
    int32 ResponseCode = Response->GetResponseCode();
    if (ResponseCode != 200)
    {
        CHAT_LOG(Warning, TEXT("Model ratings request failed with code %d"), ResponseCode);
        return;
    }
    
    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
    
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        CHAT_LOG(Warning, TEXT("Failed to parse model ratings JSON"));
        return;
    }
    
    const TSharedPtr<FJsonObject>* RatingsObject;
    if (!RootObject->TryGetObjectField(TEXT("ratings"), RatingsObject))
    {
        CHAT_LOG(Warning, TEXT("Model ratings response missing 'ratings' object"));
        return;
    }
    
    // Cache all ratings
    ModelRatings.Empty();
    for (const auto& Pair : (*RatingsObject)->Values)
    {
        FString RatingValue;
        if (Pair.Value->TryGetString(RatingValue))
        {
            ModelRatings.Add(Pair.Key, RatingValue);
        }
    }
    
    bModelRatingsFetched = true;
    CHAT_LOG(Log, TEXT("Fetched %d model ratings from VibeUE"), ModelRatings.Num());
    
    // Apply ratings to available models
    ApplyModelRatings();
}

void SAIChatWindow::ApplyModelRatings()
{
    if (!bModelRatingsFetched || AvailableModels.Num() == 0)
    {
        return;
    }
    
    // Apply ratings to each model
    int32 RatedCount = 0;
    for (const TSharedPtr<FOpenRouterModel>& ModelPtr : AvailableModels)
    {
        if (ModelPtr.IsValid())
        {
            const FString* FoundRating = ModelRatings.Find(ModelPtr->Id);
            if (FoundRating)
            {
                ModelPtr->Rating = *FoundRating;
                RatedCount++;
            }
            else
            {
                ModelPtr->Rating = TEXT(""); // Unrated
            }
        }
    }
    
    // Remember current selection
    FString SelectedModelId;
    if (SelectedModel.IsValid())
    {
        SelectedModelId = SelectedModel->Id;
    }
    
    // Re-sort: rated models first (great > good > moderate > bad), then free, then alphabetical
    AvailableModels.Sort([](const TSharedPtr<FOpenRouterModel>& A, const TSharedPtr<FOpenRouterModel>& B)
    {
        int32 TierA = A->GetRatingTier();
        int32 TierB = B->GetRatingTier();
        
        // Rated models come first (higher tier = better)
        if (TierA != TierB)
        {
            return TierA > TierB;
        }
        
        // Within same tier: free models first
        if (A->IsFree() != B->IsFree())
        {
            return A->IsFree();
        }
        
        // Then alphabetical
        return A->Name < B->Name;
    });
    
    // Restore selection
    SelectedModel.Reset();
    for (const TSharedPtr<FOpenRouterModel>& ModelPtr : AvailableModels)
    {
        if (ModelPtr->Id == SelectedModelId)
        {
            SelectedModel = ModelPtr;
            break;
        }
    }
    
    // Refresh UI
    ModelComboBox->RefreshOptions();
    if (SelectedModel.IsValid())
    {
        ModelComboBox->SetSelectedItem(SelectedModel);
    }
    
    CHAT_LOG(Log, TEXT("Applied ratings to %d/%d models, re-sorted dropdown"), RatedCount, AvailableModels.Num());
}

FLinearColor SAIChatWindow::GetRatingColor(const FString& Rating)
{
    if (Rating == TEXT("great"))    return VibeUEColors::RatingGreat;
    if (Rating == TEXT("good"))     return VibeUEColors::RatingGood;
    if (Rating == TEXT("moderate")) return VibeUEColors::RatingModerate;
    if (Rating == TEXT("bad"))      return VibeUEColors::RatingBad;
    return VibeUEColors::TextPrimary; // Unrated = white/default
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

void SAIChatWindow::HandleToolsReady(bool bSuccess, int32 ToolCount)
{
    if (ToolsCountText.IsValid())
    {
        if (bSuccess && ToolCount > 0)
        {
            ToolsCountText->SetText(FText::FromString(FString::Printf(TEXT("Tools: %d"), ToolCount)));
            ToolsCountText->SetColorAndOpacity(FSlateColor(VibeUEColors::Green)); // Green for connected
            CHAT_LOG(Log, TEXT("MCP tools ready: %d tools available"), ToolCount);
        }
        else
        {
            ToolsCountText->SetText(FText::FromString(TEXT("Tools: 0")));
            ToolsCountText->SetColorAndOpacity(FSlateColor(VibeUEColors::TextMuted)); // Muted for no tools
            CHAT_LOG(Log, TEXT("MCP tools: none available"));
        }
    }
    
    // Update token budget display initially
    UpdateTokenBudgetDisplay();
}

void SAIChatWindow::HandleSummarizationStarted(const FString& Reason)
{
    CHAT_LOG(Log, TEXT("Summarization started: %s"), *Reason);
    AddSystemNotification(FString::Printf(TEXT("📋 Summarizing conversation... (%s)"), *Reason));
    
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
        AddSystemNotification(TEXT("✅ Conversation summarized to save context space."));
        
        // Show summary preview in a system message
        FString PreviewText = Summary.Left(200);
        if (Summary.Len() > 200) PreviewText += TEXT("...");
        CHAT_LOG(Log, TEXT("Summary preview: %s"), *PreviewText);
    }
    else
    {
        CHAT_LOG(Warning, TEXT("Summarization failed"));
        AddSystemNotification(TEXT("⚠️ Failed to summarize conversation."));
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
        if (Tokens >= 1024)
        {
            return FString::Printf(TEXT("%.1fK"), Tokens / 1024.0f);
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
    
    // Add the message to chat (system message is the primary notification)
    AddSystemNotification(Message);
    FChatMessage SystemMsg(TEXT("system"), Message);
    SystemMsg.Role = TEXT("system");
    if (ChatSession.IsValid())
    {
        // We need to add this as a visual-only message, not to the actual conversation
        // For now, just show it in the status and let user type 'continue'
    }
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
        // Status animation now shows in chat, keep input hint simple
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
        // Transient clipboard notification not needed - user knows they copied
    }
}

// ============================================================================
// Voice Input Handlers
// ============================================================================

void SAIChatWindow::OnMicrophonePressed()
{
    if (!ChatSession.IsValid())
    {
        return;
    }

    // Start recording when button is pressed down
    double CurrentTime = FPlatformTime::Seconds();
    UE_LOG(LogTemp, Warning, TEXT("[VOICE DEBUG] Microphone button PRESSED at time %.3f"), CurrentTime);
    ChatSession->StartVoiceInput();
}

void SAIChatWindow::OnMicrophoneReleased()
{
    if (!ChatSession.IsValid())
    {
        return;
    }

    // Stop recording when button is released
    double CurrentTime = FPlatformTime::Seconds();
    UE_LOG(LogTemp, Warning, TEXT("[VOICE DEBUG] Microphone button RELEASED at time %.3f"), CurrentTime);
    ChatSession->StopVoiceInput();
}

FText SAIChatWindow::GetMicrophoneButtonText() const
{
    if (bIsVoiceInputActive)
    {
        // Red circle (recording)
        return FText::FromString(TEXT("\U0001F534"));
    }
    else
    {
        // Microphone emoji
        return FText::FromString(TEXT("\U0001F3A4"));
    }
}

FText SAIChatWindow::GetMicrophoneTooltip() const
{
    if (!ChatSession.IsValid() || !ChatSession->IsVoiceInputAvailable())
    {
        return FText::FromString(TEXT("Voice input not configured. Add ElevenLabs API key in settings."));
    }

    if (bIsVoiceInputActive)
    {
        return FText::FromString(TEXT("Recording... (release to stop)"));
    }
    else
    {
        return FText::FromString(TEXT("Hold to record voice input"));
    }
}

bool SAIChatWindow::IsMicrophoneEnabled() const
{
    return ChatSession.IsValid() && ChatSession->IsVoiceInputAvailable();
}

void SAIChatWindow::OnVoiceInputStarted(bool bSuccess)
{
    bIsVoiceInputActive = bSuccess;
    if (bSuccess)
    {
        InputTextBox->SetText(FText::FromString(TEXT("Listening...")));
    }
}

void SAIChatWindow::OnVoiceInputText(const FString& Text, bool bIsFinal)
{
    if (bIsFinal)
    {
        // Final transcript - set in input box for user to edit/send
        InputTextBox->SetText(FText::FromString(Text));
        bIsVoiceInputActive = false;
    }
    else
    {
        // Partial transcript - show as preview
        InputTextBox->SetText(FText::FromString(Text));
    }
}

void SAIChatWindow::OnVoiceInputStopped()
{
    bIsVoiceInputActive = false;
}

void SAIChatWindow::OnVoiceInputAutoSent()
{
    // Clear input box after auto-sending
    InputTextBox->SetText(FText::FromString(TEXT("")));
}

void SAIChatWindow::HandleHyperlinkClicked(const FSlateHyperlinkRun::FMetadata& Metadata)
{
    // Get URL from the "id" attribute we set in markdown conversion
    const FString* URL = Metadata.Find(TEXT("id"));
    if (URL && !URL->IsEmpty())
    {
        FPlatformProcess::LaunchURL(**URL, nullptr, nullptr);
    }
}

// ============================================================================
// Phase 1: Thinking Indicator
// ============================================================================

void SAIChatWindow::HandleLLMThinkingStarted()
{
    ShowThinkingIndicator(true);
}

void SAIChatWindow::HandleLLMThinkingComplete()
{
    ShowThinkingIndicator(false);
}

void SAIChatWindow::ShowThinkingIndicator(bool bShow)
{
    if (bShow)
    {
        // Select a random vibing word for this thinking session
        TArray<FString> VibingWords = GetVibingWordsFromConfig();
        if (VibingWords.Num() > 0)
        {
            int32 RandomIndex = FMath::RandRange(0, VibingWords.Num() - 1);
            CurrentVibingWord = VibingWords[RandomIndex];
        }
        else
        {
            CurrentVibingWord = TEXT("Vibing");
        }

        // Create the thinking indicator widget if it doesn't exist
        if (!ThinkingIndicatorWidget.IsValid())
        {
            ThinkingIndicatorWidget =
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(8, 4)
                [
                    SAssignNew(ThinkingTextBlock, STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("● %s·"), *CurrentVibingWord)))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                    .ColorAndOpacity(FSlateColor(VibeUEColors::TextSecondary))
                ];
        }
        else
        {
            // Update the text with the new vibing word
            ThinkingTextBlock->SetText(FText::FromString(FString::Printf(TEXT("● %s·"), *CurrentVibingWord)));
        }

        // Remove first if already visible (to re-add at bottom)
        if (bThinkingIndicatorVisible)
        {
            MessageScrollBox->RemoveSlot(ThinkingIndicatorWidget.ToSharedRef());
        }

        // Add to scroll box at the bottom
        MessageScrollBox->AddSlot()
        .Padding(FMargin(2, 4))
        [
            ThinkingIndicatorWidget.ToSharedRef()
        ];

        bThinkingIndicatorVisible = true;

        // Start the animation timer (only if not already running)
        if (!ThinkingAnimationTimerHandle.IsValid())
        {
            ThinkingAnimationFrame = 0;
            if (GEditor)
            {
                GEditor->GetTimerManager()->SetTimer(
                    ThinkingAnimationTimerHandle,
                    FTimerDelegate::CreateSP(this, &SAIChatWindow::AnimateThinkingIndicator),
                    0.3f,  // 300ms per frame for subtle animation
                    true   // Loop
                );
            }
        }

        // Note: Don't call ScrollToBottom() here to avoid recursion
        MessageScrollBox->ScrollToEnd();
    }
    else
    {
        // Stop the animation timer
        if (GEditor)
        {
            GEditor->GetTimerManager()->ClearTimer(ThinkingAnimationTimerHandle);
        }

        // Remove the thinking indicator from scroll box
        if (ThinkingIndicatorWidget.IsValid() && bThinkingIndicatorVisible)
        {
            MessageScrollBox->RemoveSlot(ThinkingIndicatorWidget.ToSharedRef());
            bThinkingIndicatorVisible = false;
        }
    }
}

void SAIChatWindow::AnimateThinkingIndicator()
{
    if (!ThinkingTextBlock.IsValid())
    {
        return;
    }

    // Animate with growing/shrinking dots: ·  ··  ···  ··
    static const TCHAR* ThinkingFrames[] = { TEXT("·"), TEXT("··"), TEXT("···"), TEXT("··") };
    ThinkingAnimationFrame = (ThinkingAnimationFrame + 1) % 4;

    // Use the current vibing word (selected when thinking started)
    FString Word = CurrentVibingWord.IsEmpty() ? TEXT("Vibing") : CurrentVibingWord;
    FString AnimatedText = FString::Printf(TEXT("● %s%s"), *Word, ThinkingFrames[ThinkingAnimationFrame]);
    ThinkingTextBlock->SetText(FText::FromString(AnimatedText));
}

TArray<FString> SAIChatWindow::GetVibingWordsFromConfig()
{
    TArray<FString> Words;

    // Try to load from config
    FString WordsString;
    GConfig->GetString(TEXT("VibeUE.UI"), TEXT("VibingWords"), WordsString, GEditorPerProjectIni);

    if (!WordsString.IsEmpty())
    {
        WordsString.ParseIntoArray(Words, TEXT(","), true);
        // Trim whitespace from each word
        for (FString& Word : Words)
        {
            Word.TrimStartAndEndInline();
        }
    }

    // If no config or empty, use defaults (33 vibing-related words)
    if (Words.Num() == 0)
    {
        Words = {
            TEXT("Vibing"),
            TEXT("Grooving"),
            TEXT("Flowing"),
            TEXT("Syncing"),
            TEXT("Tuning"),
            TEXT("Jamming"),
            TEXT("Chilling"),
            TEXT("Cruising"),
            TEXT("Gliding"),
            TEXT("Drifting"),
            TEXT("Floating"),
            TEXT("Buzzing"),
            TEXT("Humming"),
            TEXT("Pulsing"),
            TEXT("Resonating"),
            TEXT("Harmonizing"),
            TEXT("Radiating"),
            TEXT("Channeling"),
            TEXT("Aligning"),
            TEXT("Synergizing"),
            TEXT("Manifesting"),
            TEXT("Cultivating"),
            TEXT("Nurturing"),
            TEXT("Brewing"),
            TEXT("Conjuring"),
            TEXT("Crafting"),
            TEXT("Weaving"),
            TEXT("Spinning"),
            TEXT("Cooking"),
            TEXT("Stirring"),
            TEXT("Mixing"),
            TEXT("Blending"),
            TEXT("Composing")
        };

        // Save defaults to config for user customization
        SaveVibingWordsToConfig(Words);
    }

    return Words;
}

void SAIChatWindow::SaveVibingWordsToConfig(const TArray<FString>& Words)
{
    FString WordsString = FString::Join(Words, TEXT(","));
    GConfig->SetString(TEXT("VibeUE.UI"), TEXT("VibingWords"), *WordsString, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

// ============================================================================
// Phase 2: Tool Status Animation (Rotating Arrow Spinner)
// ============================================================================

void SAIChatWindow::StartToolStatusAnimation(const FString& UniqueKey)
{
    FToolCallWidgetData* WidgetData = ToolCallWidgets.Find(UniqueKey);
    if (!WidgetData)
    {
        return;
    }

    // Don't start if already completed
    if (WidgetData->bResponseReceived)
    {
        return;
    }

    // Use editor timer manager for animation
    if (GEditor)
    {
        GEditor->GetTimerManager()->SetTimer(
            WidgetData->StatusAnimationTimer,
            FTimerDelegate::CreateLambda([this, UniqueKey]()
            {
                FToolCallWidgetData* Data = ToolCallWidgets.Find(UniqueKey);
                if (!Data || Data->bResponseReceived)
                {
                    // Stop animation if completed or widget gone
                    StopToolStatusAnimation(UniqueKey);
                    return;
                }

                // Rotating arrow spinner frames
                static const TCHAR* SpinnerFrames[] = {
                    TEXT("→"), TEXT("↗"), TEXT("↑"), TEXT("↖"),
                    TEXT("←"), TEXT("↙"), TEXT("↓"), TEXT("↘")
                };

                Data->AnimationFrame = (Data->AnimationFrame + 1) % 8;

                if (Data->StatusText.IsValid())
                {
                    Data->StatusText->SetText(FText::FromString(SpinnerFrames[Data->AnimationFrame]));
                }
            }),
            0.1f,  // 100ms per frame for smooth rotation
            true   // Loop
        );
    }
}

void SAIChatWindow::StopToolStatusAnimation(const FString& UniqueKey)
{
    FToolCallWidgetData* WidgetData = ToolCallWidgets.Find(UniqueKey);
    if (!WidgetData)
    {
        return;
    }

    if (GEditor)
    {
        GEditor->GetTimerManager()->ClearTimer(WidgetData->StatusAnimationTimer);
    }
}

// ============================================================================
// Image Attachment
// ============================================================================

FReply SAIChatWindow::OnAttachmentClicked()
{
    OpenImageFileDialog();
    return FReply::Handled();
}

FReply SAIChatWindow::OnRemoveAttachmentClicked()
{
    ClearAttachedImage();
    return FReply::Handled();
}

void SAIChatWindow::OpenImageFileDialog()
{
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform)
    {
        return;
    }

    TArray<FString> OutFiles;
    const FString FileTypes = TEXT("Image Files (*.png;*.jpg;*.jpeg;*.bmp)|*.png;*.jpg;*.jpeg;*.bmp");

    bool bOpened = DesktopPlatform->OpenFileDialog(
        FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
        TEXT("Select Image"),
        FPaths::ProjectDir(),
        TEXT(""),
        FileTypes,
        EFileDialogFlags::None,
        OutFiles
    );

    if (bOpened && OutFiles.Num() > 0)
    {
        AttachImageFromFile(OutFiles[0]);
    }
}

void SAIChatWindow::AttachImageFromFile(const FString& FilePath)
{
    // Load the image file
    TArray<uint8> ImageData;
    if (!FFileHelper::LoadFileToArray(ImageData, *FilePath))
    {
        UE_LOG(LogAIChatWindow, Warning, TEXT("Failed to load image file: %s"), *FilePath);
        return;
    }

    // Determine MIME type from extension
    FString Extension = FPaths::GetExtension(FilePath).ToLower();
    FString MimeType;
    if (Extension == TEXT("png"))
    {
        MimeType = TEXT("image/png");
    }
    else if (Extension == TEXT("jpg") || Extension == TEXT("jpeg"))
    {
        MimeType = TEXT("image/jpeg");
    }
    else if (Extension == TEXT("bmp"))
    {
        MimeType = TEXT("image/bmp");
    }
    else
    {
        UE_LOG(LogAIChatWindow, Warning, TEXT("Unsupported image format: %s"), *Extension);
        return;
    }

    SetAttachedImagePreview(ImageData, MimeType);
}

void SAIChatWindow::AttachImageFromClipboard()
{
    // Try to get image from Windows clipboard
    // Note: Slate's clipboard API is text-only, so we need to use platform-specific code
#if PLATFORM_WINDOWS
    if (!OpenClipboard(nullptr))
    {
        return;
    }

    // Check if clipboard contains a bitmap
    if (IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(CF_DIBV5))
    {
        HANDLE hData = GetClipboardData(CF_DIB);
        if (hData)
        {
            BITMAPINFO* pBMI = (BITMAPINFO*)GlobalLock(hData);
            if (pBMI)
            {
                int32 Width = pBMI->bmiHeader.biWidth;
                int32 Height = FMath::Abs(pBMI->bmiHeader.biHeight);
                int32 BitCount = pBMI->bmiHeader.biBitCount;

                // Get pointer to pixel data
                uint8* pPixels = (uint8*)pBMI + pBMI->bmiHeader.biSize;
                if (pBMI->bmiHeader.biCompression == BI_BITFIELDS)
                {
                    pPixels += 12; // Skip color masks
                }

                // Convert to PNG using ImageWrapper
                IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

                // Create raw BGRA data
                TArray<uint8> RawData;
                int32 RowPitch = ((Width * BitCount + 31) / 32) * 4;
                RawData.SetNum(Width * Height * 4);

                bool bBottomUp = pBMI->bmiHeader.biHeight > 0;

                for (int32 y = 0; y < Height; ++y)
                {
                    int32 SrcY = bBottomUp ? (Height - 1 - y) : y;
                    uint8* SrcRow = pPixels + SrcY * RowPitch;
                    uint8* DstRow = RawData.GetData() + y * Width * 4;

                    for (int32 x = 0; x < Width; ++x)
                    {
                        if (BitCount == 32)
                        {
                            DstRow[x * 4 + 0] = SrcRow[x * 4 + 2]; // R
                            DstRow[x * 4 + 1] = SrcRow[x * 4 + 1]; // G
                            DstRow[x * 4 + 2] = SrcRow[x * 4 + 0]; // B
                            DstRow[x * 4 + 3] = SrcRow[x * 4 + 3]; // A
                        }
                        else if (BitCount == 24)
                        {
                            DstRow[x * 4 + 0] = SrcRow[x * 3 + 2]; // R
                            DstRow[x * 4 + 1] = SrcRow[x * 3 + 1]; // G
                            DstRow[x * 4 + 2] = SrcRow[x * 3 + 0]; // B
                            DstRow[x * 4 + 3] = 255;               // A
                        }
                    }
                }

                GlobalUnlock(hData);
                CloseClipboard();

                // Convert to PNG for storage
                TSharedPtr<IImageWrapper> PngWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
                if (PngWrapper->SetRaw(RawData.GetData(), RawData.Num(), Width, Height, ERGBFormat::RGBA, 8))
                {
                    TArray64<uint8> CompressedPng = PngWrapper->GetCompressed(90);
                    if (CompressedPng.Num() > 0)
                    {
                        TArray<uint8> PngArray;
                        PngArray.Append(CompressedPng.GetData(), CompressedPng.Num());
                        SetAttachedImagePreview(PngArray, TEXT("image/png"));
                        return;
                    }
                }

                return;
            }
            GlobalUnlock(hData);
        }
    }

    CloseClipboard();
#endif
}

void SAIChatWindow::SetAttachedImagePreview(const TArray<uint8>& ImageData, const FString& MimeType)
{
    // Encode to base64 data URL
    FString Base64Data = FBase64::Encode(ImageData);
    AttachedImageDataUrl = FString::Printf(TEXT("data:%s;base64,%s"), *MimeType, *Base64Data);

    // Create texture for preview
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    EImageFormat ImageFormat = EImageFormat::PNG;
    if (MimeType.Contains(TEXT("jpeg")) || MimeType.Contains(TEXT("jpg")))
    {
        ImageFormat = EImageFormat::JPEG;
    }
    else if (MimeType.Contains(TEXT("bmp")))
    {
        ImageFormat = EImageFormat::BMP;
    }

    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
    if (ImageWrapper->SetCompressed(ImageData.GetData(), ImageData.Num()))
    {
        TArray64<uint8> RawData;
        if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
        {
            int32 Width = ImageWrapper->GetWidth();
            int32 Height = ImageWrapper->GetHeight();

            // Clean up old texture
            if (AttachedImageTexture)
            {
                AttachedImageTexture->RemoveFromRoot();
                AttachedImageTexture = nullptr;
            }

            // Create new texture
            AttachedImageTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
            if (AttachedImageTexture)
            {
                AttachedImageTexture->AddToRoot(); // Prevent GC

                // Copy data to texture
                void* TextureData = AttachedImageTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
                FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
                AttachedImageTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
                AttachedImageTexture->UpdateResource();

                // Create brush
                AttachedImageBrush = MakeShared<FSlateBrush>();
                AttachedImageBrush->SetResourceObject(AttachedImageTexture);
                AttachedImageBrush->ImageSize = FVector2D(64, 64);

                // Set the image
                if (ImagePreviewWidget.IsValid())
                {
                    ImagePreviewWidget->SetImage(AttachedImageBrush.Get());
                }
            }
        }
    }

    // Show the preview container
    if (ImagePreviewContainer.IsValid())
    {
        ImagePreviewContainer->SetVisibility(EVisibility::Visible);
    }

    UE_LOG(LogAIChatWindow, Log, TEXT("Image attached: %s, size: %d bytes"), *MimeType, ImageData.Num());
}

void SAIChatWindow::ClearAttachedImage()
{
    AttachedImageDataUrl.Empty();

    // Hide the preview container
    if (ImagePreviewContainer.IsValid())
    {
        ImagePreviewContainer->SetVisibility(EVisibility::Collapsed);
    }

    // Clear the image widget
    if (ImagePreviewWidget.IsValid())
    {
        ImagePreviewWidget->SetImage(nullptr);
    }

    // Clean up brush
    AttachedImageBrush.Reset();

    // Clean up texture
    if (AttachedImageTexture)
    {
        AttachedImageTexture->RemoveFromRoot();
        AttachedImageTexture = nullptr;
    }

    UE_LOG(LogAIChatWindow, Log, TEXT("Attached image cleared"));
}
