// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Utility class for reading help documentation from Content/Help folder.
 * Provides file-based help system for all VibeUE tools.
 */
class VIBEUE_API FHelpFileReader
{
public:
	/**
	 * Get tool overview help (reads help.md from tool folder).
	 * 
	 * @param ToolName - Name of the tool (e.g., "manage_material")
	 * @return JSON response with tool help or error
	 */
	static TSharedPtr<FJsonObject> GetToolHelp(const FString& ToolName);

	/**
	 * Get action-specific help (reads [action].md from tool folder).
	 * 
	 * @param ToolName - Name of the tool (e.g., "manage_material")
	 * @param ActionName - Name of the action (e.g., "create")
	 * @return JSON response with action help or error
	 */
	static TSharedPtr<FJsonObject> GetActionHelp(const FString& ToolName, const FString& ActionName);

	/**
	 * Handle a help request - automatically routes to tool or action help.
	 * 
	 * @param ToolName - Name of the tool
	 * @param Params - Parameters that may contain "help_action"
	 * @return JSON response with appropriate help
	 */
	static TSharedPtr<FJsonObject> HandleHelp(const FString& ToolName, const TSharedPtr<FJsonObject>& Params);

private:
	/**
	 * Get the path to the Help content folder.
	 */
	static FString GetHelpBasePath();

	/**
	 * Get list of available help actions for a tool.
	 */
	static TArray<FString> GetAvailableHelpActions(const FString& ToolName);

	/**
	 * Read a markdown file and return its contents.
	 */
	static bool ReadHelpFile(const FString& FilePath, FString& OutContent);

	/**
	 * Parse markdown content into JSON response.
	 */
	static TSharedPtr<FJsonObject> ParseMarkdownToJson(const FString& Content, const FString& HelpType);

	/**
	 * Create an error response.
	 */
	static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& Code, const FString& Message);

	/**
	 * Create a success response.
	 */
	static TSharedPtr<FJsonObject> CreateSuccessResponse();
};
