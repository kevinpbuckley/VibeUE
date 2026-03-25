// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Chat/AIChatCommands.h"
#include "UI/SAIChatWindow.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Application/SlateApplication.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "LevelEditor.h"
#include "Editor.h"
#include "Widgets/Docking/SDockTab.h"
#include "StatusBarSubsystem.h"

#define LOCTEXT_NAMESPACE "AIChatCommands"

// Static members
TSharedPtr<FUICommandList> FAIChatCommands::CommandList;
FDelegateHandle FAIChatCommands::MenuExtensionHandle;
FDelegateHandle FAIChatCommands::PanelDrawerSummonHandle;
const FName FAIChatCommands::AIChatTabName("VibeUEAIChat");

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
        "Open the VibeUE AI Chat panel",
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
    
    // Register tab spawner
    RegisterTabSpawner();
    
    // Register menus via startup callback to ensure UToolMenus is fully initialized
    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&FAIChatCommands::RegisterMenus));
    
    // Register status bar panel drawer (after editor is ready)
    if (GEditor)
    {
        RegisterStatusBarPanelDrawer();
    }
    else
    {
        FCoreDelegates::OnPostEngineInit.AddStatic(&FAIChatCommands::RegisterStatusBarPanelDrawer);
    }
    
    UE_LOG(LogTemp, Log, TEXT("AI Chat commands initialized with panel drawer support"));
}

void FAIChatCommands::Shutdown()
{
    UnregisterStatusBarPanelDrawer();
    UnregisterTabSpawner();
    UnregisterMenus();
    
    CommandList.Reset();
    
    FAIChatCommands::Unregister();
    
    UE_LOG(LogTemp, Log, TEXT("AI Chat commands shutdown"));
}

void FAIChatCommands::RegisterTabSpawner()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        AIChatTabName,
        FOnSpawnTab::CreateStatic(&FAIChatCommands::SpawnAIChatTab))
        .SetDisplayName(LOCTEXT("AIChatTabTitle", "VibeUE AI Chat"))
        .SetMenuType(ETabSpawnerMenuType::Hidden)
        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Comment"))
        .SetCanSidebarTab(false);  // Panel drawer tabs don't work well as sidebar tabs
    
    UE_LOG(LogTemp, Log, TEXT("AI Chat tab spawner registered"));
}

void FAIChatCommands::UnregisterTabSpawner()
{
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AIChatTabName);
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
void FAIChatCommands::RegisterStatusBarPanelDrawer()
{
    if (GEditor)
    {
        // Make sure StatusBar module is loaded
        FModuleManager::Get().LoadModuleChecked(TEXT("StatusBar"));
        
        if (UStatusBarSubsystem* StatusBarSubsystem = GEditor->GetEditorSubsystem<UStatusBarSubsystem>())
        {
            PanelDrawerSummonHandle = StatusBarSubsystem->RegisterPanelDrawerSummon(
                UStatusBarSubsystem::FRegisterPanelDrawerSummonDelegate::FDelegate::CreateStatic(
                    &FAIChatCommands::GeneratePanelDrawerSummon)
            );
            
            UE_LOG(LogTemp, Log, TEXT("AI Chat panel drawer registered in status bar"));
        }
    }
}

void FAIChatCommands::UnregisterStatusBarPanelDrawer()
{
    if (GEditor && PanelDrawerSummonHandle.IsValid())
    {
        if (UStatusBarSubsystem* StatusBarSubsystem = GEditor->GetEditorSubsystem<UStatusBarSubsystem>())
        {
            StatusBarSubsystem->UnregisterPanelDrawerSummon(PanelDrawerSummonHandle);
        }
        PanelDrawerSummonHandle.Reset();
    }
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

TSharedRef<SDockTab> FAIChatCommands::SpawnAIChatTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(LOCTEXT("AIChatTabLabel", "VibeUE AI Chat"))
        [
            SNew(SAIChatWindow)
        ];
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
void FAIChatCommands::GeneratePanelDrawerSummon(
    TArray<UStatusBarSubsystem::FTabIdAndButtonLabel>& OutTabIdsAndLabels,
    const TSharedRef<SDockTab>& InParentTab)
{
    // Add the "VibeUE" button to the status bar
    OutTabIdsAndLabels.Emplace(AIChatTabName, LOCTEXT("StatusBarVibeUE", "VibeUE"));
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FAIChatCommands::RegisterMenus()
{
    UToolMenus* ToolMenus = UToolMenus::Get();
    if (!ToolMenus)
    {
        return;
    }

    // Owner will be used for cleanup in call to UToolMenus::UnregisterOwner()
    FToolMenuOwnerScoped OwnerScoped("AIChatCommands");

    // Add to Window menu under Assistance section (alongside Epic AI Assistant)
    {
        UToolMenu* WindowMenu = ToolMenus->ExtendMenu("MainFrame.MainMenu.Window");

        FToolMenuSection& Section = WindowMenu->FindOrAddSection("Assistance");

        Section.AddEntry(FToolMenuEntry::InitMenuEntry(
            "VibeUEAIChat",
            LOCTEXT("OpenAIChatLabel", "VibeUE AI Chat"),
            LOCTEXT("OpenAIChatTooltip", "Open the VibeUE AI Chat panel (Ctrl+Shift+V)"),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Comment"),
            FUIAction(
                FExecuteAction::CreateStatic(&FAIChatCommands::HandleOpenAIChat),
                FCanExecuteAction::CreateStatic(&FAIChatCommands::CanOpenAIChat)
            )
        ));
    }

    ToolMenus->RefreshAllWidgets();
}

void FAIChatCommands::UnregisterMenus()
{
    // Use UnregisterOwner to remove only our entries, not the entire section
    // (the Assistance section is shared with Epic's AI Assistant)
    UToolMenus::UnregisterOwner("AIChatCommands");
}

void FAIChatCommands::HandleOpenAIChat()
{
    // Try to toggle the tab in the panel drawer (right-side slide-in panel)
    // This matches how Unreal's AI Assistant behaves
    
    // Get the widget path under the cursor to find the appropriate window
    FWidgetPath WidgetPath = FSlateApplication::Get().LocateWindowUnderMouse(
        FSlateApplication::Get().GetCursorPos(),
        FSlateApplication::Get().GetInteractiveTopLevelWindows());
    
    TSharedPtr<FTabManager> TabManager;
    if (WidgetPath.IsValid())
    {
        TabManager = FGlobalTabmanager::Get()->GetSubTabManagerForWindow(WidgetPath.GetWindow());
    }
    
    if (TabManager)
    {
        // Toggle the tab in the panel drawer
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        TabManager->TryToggleTabInPanelDrawer(AIChatTabName, {});
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
    }
    else
    {
        // Fallback: just invoke the tab normally
        FGlobalTabmanager::Get()->TryInvokeTab(AIChatTabName);
    }
}

bool FAIChatCommands::CanOpenAIChat()
{
    return true;
}

#undef LOCTEXT_NAMESPACE
