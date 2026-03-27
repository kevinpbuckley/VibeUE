// Copyright Epic Games, Inc. All Rights Reserved.

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
