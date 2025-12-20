// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Tools/ToolMacros.h"
#include "ExampleTools.generated.h"

/**
 * Example tool class demonstrating the reflection-based tool system
 * This serves as a reference implementation for creating new tools
 */
UCLASS(meta = (
	ToolCategory = "Example",
	ToolDescription = "Example tools for testing the reflection system"
))
class VIBEUE_API UExampleTools : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Example tool: Echo a message
	 * Demonstrates basic string parameter handling
	 */
	UFUNCTION(meta = (
		ToolName = "echo",
		ToolDescription = "Echoes back the input message",
		ToolCategory = "Example",
		ToolExamples = "echo(message=\"Hello, World!\")"
	))
	static FString Echo(const FString& Message);

	/**
	 * Example tool: Add two numbers
	 * Demonstrates numeric parameter handling
	 */
	UFUNCTION(meta = (
		ToolName = "add_numbers",
		ToolDescription = "Adds two numbers together",
		ToolCategory = "Example",
		ToolExamples = "add_numbers(a=5, b=3)"
	))
	static FString AddNumbers(int32 A, int32 B);

	/**
	 * Example tool: Get system info
	 * Demonstrates tool with no parameters
	 */
	UFUNCTION(meta = (
		ToolName = "get_system_info",
		ToolDescription = "Returns system information",
		ToolCategory = "Example",
		ToolExamples = "get_system_info()"
	))
	static FString GetSystemInfo();

	/**
	 * System tool: Check Unreal connection
	 * Verifies connection to Unreal Engine
	 */
	UFUNCTION(meta = (
		ToolName = "check_unreal_connection",
		ToolDescription = "Test Unreal Engine connection and plugin status",
		ToolCategory = "System",
		ToolExamples = "check_unreal_connection()"
	))
	static FString CheckUnrealConnection();
};

