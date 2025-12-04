// Copyright 2025 Vibe AI. All Rights Reserved.

#include "Chat/AIChatCommands.h"
#include "UI/SAIChatWindow.h"
#include "Framework/Docking/TabManager.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "LevelEditor.h"

#define LOCTEXT_NAMESPACE "AIChatCommands"

TSharedPtr<FUICommandList> FAIChatCommands::CommandList;
FDelegateHandle FAIChatCommands::MenuExtensionHandle;

FAIChatCommands::FAIChatCommands()
    : TCommands<FAIChatCommands>(
        TEXT("AIChatCommands"),
        LOCTEXT("AIChatCommands", "AI Chat Commands"),
        NAME_None,
        FAppStyle::GetAppStyleSetName())
{
}

void FAIChatCommands::RegisterCommands()
{
    UI_COMMAND(
        OpenAIChat,
        "Open AI Chat",
        "Open the VibeUE AI Chat window",
        EUserInterfaceActionType::Button,
        FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::V)
    );
}

void FAIChatCommands::Initialize()
{
    // Register commands
    FAIChatCommands::Register();
    
    // Create command list
    CommandList = MakeShared<FUICommandList>();
    
    // Map commands to actions
    CommandList->MapAction(
        Get().OpenAIChat,
        FExecuteAction::CreateStatic(&FAIChatCommands::HandleOpenAIChat),
        FCanExecuteAction::CreateStatic(&FAIChatCommands::CanOpenAIChat)
    );
    
    // Bind to Level Editor's global actions so keyboard shortcuts work
    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
    LevelEditorModule.GetGlobalLevelEditorActions()->Append(CommandList.ToSharedRef());
    
    // Register menus
    RegisterMenus();
    
    UE_LOG(LogTemp, Log, TEXT("AI Chat commands initialized"));
}

void FAIChatCommands::Shutdown()
{
    UnregisterMenus();
    
    CommandList.Reset();
    
    FAIChatCommands::Unregister();
    
    UE_LOG(LogTemp, Log, TEXT("AI Chat commands shutdown"));
}

void FAIChatCommands::RegisterMenus()
{
    UToolMenus* ToolMenus = UToolMenus::Get();
    if (!ToolMenus)
    {
        return;
    }
    
    // Add to Tools menu
    UToolMenu* ToolsMenu = ToolMenus->ExtendMenu("LevelEditor.MainMenu.Tools");
    if (ToolsMenu)
    {
        FToolMenuSection& Section = ToolsMenu->FindOrAddSection("VibeUE");
        Section.Label = LOCTEXT("VibeUESection", "VibeUE");
        
        Section.AddMenuEntryWithCommandList(
            Get().OpenAIChat,
            CommandList,
            LOCTEXT("OpenAIChatLabel", "AI Chat"),
            LOCTEXT("OpenAIChatTooltip", "Open the VibeUE AI Chat window (Ctrl+Shift+V)"),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Comment")
        );
    }
    
    ToolMenus->RefreshAllWidgets();
}

void FAIChatCommands::UnregisterMenus()
{
    UToolMenus* ToolMenus = UToolMenus::Get();
    if (ToolMenus)
    {
        ToolMenus->RemoveMenu("LevelEditor.MainMenu.Tools");
    }
}

void FAIChatCommands::HandleOpenAIChat()
{
    SAIChatWindow::ToggleWindow();
}

bool FAIChatCommands::CanOpenAIChat()
{
    return true;
}

#undef LOCTEXT_NAMESPACE
