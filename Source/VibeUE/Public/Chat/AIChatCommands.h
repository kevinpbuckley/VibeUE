// Copyright 2025 Vibe AI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "StatusBarSubsystem.h"

class SDockTab;

/**
 * Commands for the AI Chat feature
 */
class VIBEUE_API FAIChatCommands : public TCommands<FAIChatCommands>
{
public:
    FAIChatCommands();
    
    /** Register all commands */
    virtual void RegisterCommands() override;
    
    /** Open AI Chat window command */
    TSharedPtr<FUICommandInfo> OpenAIChat;
    
    /** Get the command list */
    static TSharedPtr<FUICommandList> GetCommandList() { return CommandList; }
    
    /** Initialize and register with editor */
    static void Initialize();
    
    /** Shutdown and unregister */
    static void Shutdown();
    
    /** Tab name for the AI Chat */
    static const FName AIChatTabName;
    
private:
    /** Register menu extensions */
    static void RegisterMenus();
    
    /** Unregister menu extensions */
    static void UnregisterMenus();
    
    /** Register the nomad tab spawner */
    static void RegisterTabSpawner();
    
    /** Unregister the nomad tab spawner */
    static void UnregisterTabSpawner();
    
    /** Register the status bar panel drawer summon */
    static void RegisterStatusBarPanelDrawer();
    
    /** Unregister the status bar panel drawer summon */
    static void UnregisterStatusBarPanelDrawer();
    
    /** Spawn the AI Chat tab */
    static TSharedRef<SDockTab> SpawnAIChatTab(const FSpawnTabArgs& Args);
    
    /** Generate the panel drawer summon button */
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    static void GeneratePanelDrawerSummon(TArray<UStatusBarSubsystem::FTabIdAndButtonLabel>& OutTabIdsAndLabels, const TSharedRef<SDockTab>& InParentTab);
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    
    /** Handle open AI chat command - toggles panel drawer */
    static void HandleOpenAIChat();
    
    /** Check if open AI chat command can execute */
    static bool CanOpenAIChat();
    
    /** Command list for AI Chat */
    static TSharedPtr<FUICommandList> CommandList;
    
    /** Menu extension handle */
    static FDelegateHandle MenuExtensionHandle;
    
    /** Panel drawer summon handle */
    static FDelegateHandle PanelDrawerSummonHandle;
};
