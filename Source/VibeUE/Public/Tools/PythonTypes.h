// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace VibeUE
{

/**
 * Information about a Python function/method
 */
struct VIBEUE_API FPythonFunctionInfo
{
	/** Function name */
	FString Name;

	/** Full signature (e.g., "load_asset(path: str) -> Object") */
	FString Signature;

	/** Function docstring */
	FString Docstring;

	/** Parameter names */
	TArray<FString> Parameters;

	/** Parameter type hints */
	TArray<FString> ParamTypes;

	/** Return type hint */
	FString ReturnType;

	/** Is this a method (vs standalone function) */
	bool bIsMethod = false;

	/** Is this a static method */
	bool bIsStatic = false;

	/** Is this a class method */
	bool bIsClassMethod = false;
};

/**
 * Information about a Python class
 */
struct VIBEUE_API FPythonClassInfo
{
	/** Class name */
	FString Name;

	/** Full path (e.g., "unreal.EditorActorSubsystem") */
	FString FullPath;

	/** Class docstring */
	FString Docstring;

	/** Base class names */
	TArray<FString> BaseClasses;

	/** Class methods */
	TArray<FPythonFunctionInfo> Methods;

	/** Property names */
	TArray<FString> Properties;

	/** Is this an abstract class */
	bool bIsAbstract = false;
};

/**
 * Module discovery result
 */
struct VIBEUE_API FPythonModuleInfo
{
	/** Module name (e.g., "unreal") */
	FString ModuleName;

	/** List of class names in module */
	TArray<FString> Classes;

	/** List of function names in module */
	TArray<FString> Functions;

	/** List of constant names in module */
	TArray<FString> Constants;

	/** Total number of members discovered */
	int32 TotalMembers = 0;
};

/**
 * Code execution result
 */
struct VIBEUE_API FPythonExecutionResult
{
	/** Was execution successful */
	bool bSuccess = false;

	/** Per-process sequential run id assigned by FPythonExecutionService::ExecuteCode (B2). 0 = unset. */
	int64 RunId = 0;

	/** stdout from print statements */
	FString Output;

	/** Return value (for EvaluateStatement mode) */
	FString Result;

	/** Exception traceback (if error occurred) */
	FString ErrorMessage;

	/** Captured log output */
	TArray<FString> LogMessages;

	/** Execution time in milliseconds */
	float ExecutionTimeMs = 0.0f;

	/**
	 * True when the run completed successfully but overran the client timeout (see ExecuteCode).
	 * The payload is still valid — a client whose own timeout is longer, or a retry, can use it.
	 */
	bool bTimedOut = false;

	/**
	 * Whether the pre-execution auto-save sweep ACTUALLY RAN for this call — not merely whether the
	 * caller asked for it. The caller already knows what it passed in execute_python_code's auto_save
	 * argument; what it cannot otherwise tell is whether its unsaved editor edits were flushed to
	 * disk, which is the only thing that matters downstream. False means nothing was written, and
	 * AutoSaveNote says why. Defaults true to match the historical behaviour.
	 */
	bool bAutoSave = true;

	/**
	 * Empty when the sweep ran and completed cleanly. Otherwise a short machine-readable reason:
	 * "opted_out", "previous_run_crashed", "editor_unavailable", "pie_active", or "save_failed"
	 * (the sweep ran but the package save reported errors, so SavedPackages lists what it attempted).
	 */
	FString AutoSaveNote;

	/** Names of the packages written to disk by the pre-execution auto-save sweep (empty if none). */
	TArray<FString> SavedPackages;

	/**
	 * Map worlds left resident in memory besides the open level, as full object paths. A map opened
	 * as an asset stays loaded, and any level load while one is resident fails the engine's stale-world
	 * check and FATALS the editor ("World Memory Leaks", EditorServer.cpp). Non-empty is a warning that
	 * the next level change will crash until the references are dropped and garbage is collected.
	 */
	TArray<FString> ResidentMaps;
};

/**
 * Example script structure
 */
struct VIBEUE_API FPythonExampleScript
{
	/** Example title */
	FString Title;

	/** Example description */
	FString Description;

	/** Category (e.g., "Asset Management", "Blueprint Editing") */
	FString Category;

	/** Python code */
	FString Code;

	/** Tags for filtering */
	TArray<FString> Tags;
};

/**
 * Source code search result
 */
struct VIBEUE_API FSourceSearchResult
{
	/** Relative path from plugin source root */
	FString FilePath;

	/** Line number where match was found */
	int32 LineNumber = 0;

	/** Content of the matching line */
	FString LineContent;

	/** Lines before match (for context) */
	TArray<FString> ContextBefore;

	/** Lines after match (for context) */
	TArray<FString> ContextAfter;
};

} // namespace VibeUE
