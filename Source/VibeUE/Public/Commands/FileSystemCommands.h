// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for filesystem operations like file reading, searching, and directory listing
 */
class VIBEUE_API FFileSystemCommands
{
public:
	FFileSystemCommands();

	/**
	 * Main command handler for filesystem tools
	 */
	TSharedPtr<FJsonObject> HandleCommand(
		const FString& ToolName,
		const TSharedPtr<FJsonObject>& Params
	);

private:
	// Tool handlers
	TSharedPtr<FJsonObject> HandleReadFile(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListDir(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleFileSearch(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGrepSearch(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetDirectories(const TSharedPtr<FJsonObject>& Params);

	// Helper methods
	TSharedPtr<FJsonObject> CreateSuccessResponse() const;
	TSharedPtr<FJsonObject> CreateErrorResponse(
		const FString& ErrorCode,
		const FString& ErrorMessage
	) const;

	// Path validation and helpers
	bool ValidateAndNormalizePath(FString& InOutPath, FString& OutError) const;
	FString GetPluginSourceRoot() const;
	bool IsPathAllowed(const FString& Path) const;
	bool MatchesGlobPattern(const FString& Path, const FString& Pattern) const;
	void FindFilesRecursive(
		const FString& Directory,
		const FString& Pattern,
		TArray<FString>& OutFiles,
		int32 MaxResults
	) const;

	// Grep search helpers
	struct FGrepMatch
	{
		FString FilePath;
		int32 LineNumber;
		FString LineText;
		TArray<TPair<int32, int32>> MatchRanges; // Start, Length pairs
	};

	void GrepSearchInFile(
		const FString& FilePath,
		const FString& Pattern,
		bool bIsRegex,
		TArray<FGrepMatch>& OutMatches,
		int32 MaxResults
	) const;
};
