// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/ServiceBase.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLogReaderService, Log, All);

/**
 * Information about a log file
 */
struct VIBEUE_API FLogFileInfo
{
	FString Name;
	FString FullPath;
	FString RelativePath;
	int64 SizeBytes;
	FDateTime ModifiedTime;
	int32 LineCount;
	FString Category;  // e.g., "System", "Blueprint", "Niagara", "VibeUE"
};

/**
 * Result of a log read operation
 */
struct VIBEUE_API FLogReadResult
{
	bool bSuccess = false;
	FString Content;
	int32 StartLine = 0;
	int32 EndLine = 0;
	int32 TotalLines = 0;
	int32 MatchCount = 0;  // For filter operations
	FString ErrorMessage;
};

/**
 * @class FLogReaderService
 * @brief Service for reading and filtering Unreal Engine log files
 *
 * Provides functionality similar to PowerShell's Get-Content including:
 * - Reading entire files or specific line ranges
 * - Tailing (reading last N lines)
 * - Filtering by regex pattern
 * - Waiting for new content (change detection)
 * - Supporting all UE log types (system, Blueprint, Niagara, etc.)
 */
class VIBEUE_API FLogReaderService : public FServiceBase
{
public:
	explicit FLogReaderService(TSharedPtr<FServiceContext> InContext = nullptr);
	virtual ~FLogReaderService() = default;

	virtual void Initialize() override;
	virtual FString GetServiceName() const override { return TEXT("LogReaderService"); }

	//-------------------------------------------------------------------------
	// Log Discovery
	//-------------------------------------------------------------------------

	/**
	 * List all available log files
	 * @param Category Optional category filter (System, Blueprint, Niagara, VibeUE, etc.)
	 * @return Array of log file information
	 */
	TArray<FLogFileInfo> ListLogFiles(const FString& Category = TEXT(""));

	/**
	 * Get known log file paths by type
	 */
	FString GetMainLogPath() const;
	FString GetVibeUEChatLogPath() const;
	FString GetVibeUERawLLMLogPath() const;
	FString GetBlueprintCompileLogPath() const;
	FString GetNiagaraLogPath() const;

	//-------------------------------------------------------------------------
	// File Reading Operations
	//-------------------------------------------------------------------------

	/**
	 * Read entire file content
	 * @param FilePath Path to the log file
	 * @param MaxLines Maximum lines to read (0 = unlimited, default 2000)
	 * @return Read result with content and metadata
	 */
	FLogReadResult ReadFile(const FString& FilePath, int32 MaxLines = 2000);

	/**
	 * Read specific line range from file
	 * @param FilePath Path to the log file
	 * @param Offset Starting line (0-based)
	 * @param Limit Number of lines to read
	 * @return Read result with content and metadata
	 */
	FLogReadResult ReadLines(const FString& FilePath, int32 Offset, int32 Limit);

	/**
	 * Read last N lines from file (tail)
	 * @param FilePath Path to the log file
	 * @param LineCount Number of lines from the end
	 * @return Read result with content and metadata
	 */
	FLogReadResult TailFile(const FString& FilePath, int32 LineCount = 50);

	/**
	 * Read first N lines from file (head)
	 * @param FilePath Path to the log file
	 * @param LineCount Number of lines from the start
	 * @return Read result with content and metadata
	 */
	FLogReadResult HeadFile(const FString& FilePath, int32 LineCount = 50);

	//-------------------------------------------------------------------------
	// Filtering Operations
	//-------------------------------------------------------------------------

	/**
	 * Filter file content by regex pattern
	 * @param FilePath Path to the log file
	 * @param Pattern Regex pattern to match
	 * @param bCaseSensitive Whether pattern matching is case-sensitive
	 * @param ContextLines Number of lines before/after each match to include
	 * @param MaxMatches Maximum number of matches to return (0 = unlimited)
	 * @return Read result with filtered content
	 */
	FLogReadResult FilterByPattern(
		const FString& FilePath,
		const FString& Pattern,
		bool bCaseSensitive = false,
		int32 ContextLines = 0,
		int32 MaxMatches = 100
	);

	/**
	 * Filter for specific log levels (Error, Warning, etc.)
	 * @param FilePath Path to the log file
	 * @param LevelFilter Log level to filter for (Error, Warning, Display, Log, Verbose)
	 * @param MaxMatches Maximum number of matches to return
	 * @return Read result with filtered content
	 */
	FLogReadResult FilterByLogLevel(
		const FString& FilePath,
		const FString& LevelFilter,
		int32 MaxMatches = 100
	);

	//-------------------------------------------------------------------------
	// File Information
	//-------------------------------------------------------------------------

	/**
	 * Get detailed information about a log file
	 * @param FilePath Path to the log file
	 * @return File information struct
	 */
	FLogFileInfo GetFileInfo(const FString& FilePath);

	/**
	 * Count total lines in a file
	 * @param FilePath Path to the log file
	 * @return Line count (-1 if file not found)
	 */
	int32 CountLines(const FString& FilePath);

	//-------------------------------------------------------------------------
	// Change Detection (Simulated Watch)
	//-------------------------------------------------------------------------

	/**
	 * Get new content since a specific line
	 * @param FilePath Path to the log file
	 * @param LastKnownLine Last line that was read (0-based)
	 * @return Read result with new content since that line
	 */
	FLogReadResult GetNewContent(const FString& FilePath, int32 LastKnownLine);

	/**
	 * Check if file has been modified since a given time
	 * @param FilePath Path to the log file
	 * @param SinceTime Time to check against
	 * @return True if file was modified after SinceTime
	 */
	bool HasFileChanged(const FString& FilePath, const FDateTime& SinceTime);

	//-------------------------------------------------------------------------
	// JSON Response Helpers
	//-------------------------------------------------------------------------

	/**
	 * Convert log file info array to JSON
	 */
	static FString LogFileInfoArrayToJson(const TArray<FLogFileInfo>& Files);

	/**
	 * Convert read result to JSON
	 */
	static FString LogReadResultToJson(const FLogReadResult& Result);

	/**
	 * Convert file info to JSON
	 */
	static FString LogFileInfoToJson(const FLogFileInfo& Info);

private:
	/**
	 * Load file into lines array
	 */
	bool LoadFileLines(const FString& FilePath, TArray<FString>& OutLines, FString& OutError);

	/**
	 * Determine category from file name/path
	 */
	FString DetermineLogCategory(const FString& FilePath) const;

	/**
	 * Get the logs directory
	 */
	FString GetLogsDirectory() const;

	/**
	 * Resolve a file path (handle relative paths, aliases)
	 */
	FString ResolveFilePath(const FString& FilePath) const;
};
