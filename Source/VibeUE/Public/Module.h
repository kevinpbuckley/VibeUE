// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Called before engine exit to cleanup Python references */
	void OnPreExit();

	/** Register VibeUE service toolsets with UE 5.8's ToolsetRegistry (exposes them on Epic's MCP endpoint) */
	void RegisterToolsets();
	/** Unregister VibeUE service toolsets */
	void UnregisterToolsets();

	/**
	 * Bind / unbind the forced-exit cleanup on FCoreDelegates::GetApplicationWillTerminateDelegate(). A forced
	 * exit (the Automation "Quit" of -ExecCmds="Automation RunTests ...; Quit", any RequestExit(true)) ends in
	 * TerminateProcess with neither OnPreExit nor ShutdownModule, and WillTerminate is the one callback it still
	 * broadcasts. Both are idempotent. Register runs in StartupModule, Unregister in ShutdownModule.
	 */
	void RegisterWillTerminateHook();
	void UnregisterWillTerminateHook();
	bool IsWillTerminateHookRegistered() const { return OnWillTerminateHandle.IsValid(); }

	/**
	 * The WillTerminate handler: removes this process's readiness and health signal files. Runs on whatever thread
	 * requested the exit; off the game thread (or mid-crash) it never joins the health writer. Public so the
	 * automation tests can run it from a worker thread without broadcasting the engine delegate.
	 */
	void OnApplicationWillTerminate();

	static inline FModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FModule>("VibeUE");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("VibeUE");
	}

private:
	/** Re-adds the bridged VibeUE tools after ModelContextProtocol.RefreshTools drops every registered MCP tool */
	void HandleMCPRefreshTools();

	// False when running as a commandlet (cook, etc.) — services are skipped to
	// avoid keeping UnrealEditor-Cmd.exe alive after the commandlet finishes.
	bool bServicesInitialized = false;

	FDelegateHandle OnRefreshToolsHandle;

	// Re-publishes the readiness signal on map open so its currentMap field stays fresh (issue #554).
	FDelegateHandle OnMapOpenedHandle;

	// FCoreDelegates::GetApplicationWillTerminateDelegate() binding; removed in ShutdownModule.
	FDelegateHandle OnWillTerminateHandle;
};
