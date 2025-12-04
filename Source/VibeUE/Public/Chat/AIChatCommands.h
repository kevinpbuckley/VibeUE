// Copyright 2025 Vibe AI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"

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
    
private:
    /** Register menu extensions */
    static void RegisterMenus();
    
    /** Unregister menu extensions */
    static void UnregisterMenus();
    
    /** Handle open AI chat command */
    static void HandleOpenAIChat();
    
    /** Check if open AI chat command can execute */
    static bool CanOpenAIChat();
    
    /** Command list for AI Chat */
    static TSharedPtr<FUICommandList> CommandList;
    
    /** Menu extension handle */
    static FDelegateHandle MenuExtensionHandle;
};
