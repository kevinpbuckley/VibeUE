// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Tools/LogReaderService.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Json.h"
#include "Internationalization/Regex.h"

DEFINE_LOG_CATEGORY(LogLogReaderService);

// LogPython introspection dumps put entire class docstrings on a single log
// line (tens of KB each); cap per-line output so one line can't flood the
// caller's LLM context.
static constexpr int32 MaxOutputCharsPerLine = 2000;

static FString ClampLogLine(const FString& Line)
{
	if (Line.Len() <= MaxOutputCharsPerLine)
	{
		return Line;
	}
	return Line.Left(MaxOutputCharsPerLine) + FString::Printf(TEXT(" ...[line truncated; %d chars total]"), Line.Len());
}

// File stat times are UTC; the chat log and the user's clock are local.
// Report both so timestamps aren't misread as local time.
static FString FormatLocalTime(const FDateTime& UtcTime)
{
	const FTimespan UtcToLocal = FDateTime::Now() - FDateTime::UtcNow();
	return (UtcTime + UtcToLocal).ToString();
}

FLogReaderService::FLogReaderService(TSharedPtr<FServiceContext> InContext)
	: FServiceBase(InContext)
{
}

void FLogReaderService::Initialize()
{
	LogInfo(TEXT("LogReaderService initialized"));
}

//-----------------------------------------------------------------------------
// Path Helpers
//-----------------------------------------------------------------------------

// Collapse a path to absolute form and strip ".." segments stranded at the
// drive root ("E:/../../foo" -> "E:/foo") which ConvertRelativePathToFull can
// leave behind when a relative path has more ".." components than the base
// directory has segments.
static FString NormalizeLogPath(const FString& InPath)
{
	FString Path = FPaths::ConvertRelativePathToFull(InPath);
	if (Path.Len() > 2 && Path[1] == TEXT(':') && Path[2] == TEXT('/'))
	{
		while (Path.Mid(3, 3) == TEXT("../"))
		{
			Path.RemoveAt(3, 3);
		}
	}
	return Path;
}

FString FLogReaderService::GetLogsDirectory() const
{
	return NormalizeLogPath(FPaths::ProjectSavedDir() / TEXT("Logs"));
}

FString FLogReaderService::GetMainLogPath() const
{
	// Main project log - named after the project
	FString ProjectName = FApp::GetProjectName();
	return GetLogsDirectory() / (ProjectName + TEXT(".log"));
}

FString FLogReaderService::GetVibeUEChatLogPath() const
{
	return GetLogsDirectory() / TEXT("VibeUE_Chat.log");
}

FString FLogReaderService::GetVibeUERawLLMLogPath() const
{
	return GetLogsDirectory() / TEXT("VibeUE_RawLLM.log");
}

FString FLogReaderService::GetBlueprintCompileLogPath() const
{
	// Blueprint compile logs are in the main log, but we can also check for specific files
	return GetMainLogPath();
}

FString FLogReaderService::GetNiagaraLogPath() const
{
	// Niagara compile logs are typically in the main log
	return GetMainLogPath();
}

FString FLogReaderService::ResolveFilePath(const FString& FilePath) const
{
	// Handle aliases
	if (FilePath.Equals(TEXT("main"), ESearchCase::IgnoreCase) ||
		FilePath.Equals(TEXT("system"), ESearchCase::IgnoreCase) ||
		FilePath.Equals(TEXT("project"), ESearchCase::IgnoreCase))
	{
		return GetMainLogPath();
	}
	if (FilePath.Equals(TEXT("chat"), ESearchCase::IgnoreCase) ||
		FilePath.Equals(TEXT("vibeue"), ESearchCase::IgnoreCase))
	{
		return GetVibeUEChatLogPath();
	}
	if (FilePath.Equals(TEXT("llm"), ESearchCase::IgnoreCase) ||
		FilePath.Equals(TEXT("rawllm"), ESearchCase::IgnoreCase))
	{
		return GetVibeUERawLLMLogPath();
	}

	// Handle absolute paths - check if file exists at given path
	// An absolute path on Windows starts with drive letter (e.g., C:) or on Unix starts with /
	bool bIsAbsolutePath = (FilePath.Len() > 1 && FilePath[1] == TEXT(':')) || FilePath.StartsWith(TEXT("/"));
	if (bIsAbsolutePath)
	{
		// Normalize the absolute path to resolve any .. or . components
		// Return normalized path even if file doesn't exist yet (for write operations)
		return NormalizeLogPath(FilePath);
	}

	// Try as relative to logs directory
	FString ResolvedPath = NormalizeLogPath(GetLogsDirectory() / FilePath);
	if (FPaths::FileExists(ResolvedPath))
	{
		return ResolvedPath;
	}

	// Try as relative to project directory
	ResolvedPath = NormalizeLogPath(FPaths::ProjectDir() / FilePath);
	if (FPaths::FileExists(ResolvedPath))
	{
		return ResolvedPath;
	}

	// Return normalized path based on logs directory (will fail later with appropriate error)
	return NormalizeLogPath(GetLogsDirectory() / FilePath);
}

FString FLogReaderService::DetermineLogCategory(const FString& FilePath) const
{
	FString FileName = FPaths::GetCleanFilename(FilePath).ToLower();

	if (FileName.Contains(TEXT("vibeue_chat")))
	{
		return TEXT("VibeUE");
	}
	if (FileName.Contains(TEXT("vibeue_rawllm")))
	{
		return TEXT("VibeUE");
	}
	if (FileName.Contains(TEXT("niagara")))
	{
		return TEXT("Niagara");
	}
	if (FileName.Contains(TEXT("blueprint")))
	{
		return TEXT("Blueprint");
	}
	if (FileName.Contains(TEXT("shader")))
	{
		return TEXT("Shader");
	}
	if (FileName.Contains(TEXT("cook")))
	{
		return TEXT("Cook");
	}
	if (FileName.EndsWith(TEXT(".log")))
	{
		return TEXT("System");
	}

	return TEXT("Other");
}

//-----------------------------------------------------------------------------
// Log Discovery
//-----------------------------------------------------------------------------

TArray<FLogFileInfo> FLogReaderService::ListLogFiles(const FString& Category)
{
	TArray<FLogFileInfo> Results;
	FString LogsDir = GetLogsDirectory();

	// Find all log files
	TArray<FString> FoundFiles;
	IFileManager::Get().FindFilesRecursive(FoundFiles, *LogsDir, TEXT("*.log"), true, false);

	for (const FString& FilePath : FoundFiles)
	{
		FLogFileInfo Info = GetFileInfo(FilePath);

		// Apply category filter if specified
		if (!Category.IsEmpty() && !Info.Category.Equals(Category, ESearchCase::IgnoreCase))
		{
			continue;
		}

		Results.Add(Info);
	}

	// Sort by modified time (most recent first)
	Results.Sort([](const FLogFileInfo& A, const FLogFileInfo& B)
	{
		return A.ModifiedTime > B.ModifiedTime;
	});

	return Results;
}

//-----------------------------------------------------------------------------
// File Information
//-----------------------------------------------------------------------------

FLogFileInfo FLogReaderService::GetFileInfo(const FString& FilePath)
{
	// First resolve any aliases (main, chat, llm, etc.)
	// This must happen BEFORE converting to absolute path
	FString ResolvedPath = ResolveFilePath(FilePath);

	// Then convert to absolute path to handle any relative paths
	ResolvedPath = NormalizeLogPath(ResolvedPath);

	FLogFileInfo Info;
	Info.FullPath = ResolvedPath;
	Info.Name = FPaths::GetCleanFilename(ResolvedPath);
	Info.RelativePath = Info.Name; // Default to just the filename

	// Make relative path from logs directory
	FString LogsDir = GetLogsDirectory();
	if (ResolvedPath.StartsWith(LogsDir))
	{
		Info.RelativePath = ResolvedPath.RightChop(LogsDir.Len() + 1);
	}

	Info.Category = DetermineLogCategory(ResolvedPath);

	// Get file stats
	FFileStatData StatData = IFileManager::Get().GetStatData(*ResolvedPath);
	if (StatData.bIsValid)
	{
		Info.SizeBytes = StatData.FileSize;
		Info.ModifiedTime = StatData.ModificationTime;
	}
	else
	{
		Info.SizeBytes = 0;
		Info.ModifiedTime = FDateTime::MinValue();
	}

	// Count lines (can be slow for large files)
	Info.LineCount = CountLines(ResolvedPath);

	return Info;
}

int32 FLogReaderService::CountLines(const FString& FilePath)
{
	FString ResolvedPath = ResolveFilePath(FilePath);

	if (!FPaths::FileExists(ResolvedPath))
	{
		return -1;
	}

	// Use CreateFileReader with FILEREAD_AllowWrite flag to read files that are open for writing
	TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*ResolvedPath, FILEREAD_AllowWrite));
	if (!Reader)
	{
		return -1;
	}

	int32 LineCount = 0;
	const int32 BufferSize = 65536;
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(BufferSize);
	uint8 LastByte = '\n';

	while (!Reader->AtEnd())
	{
		int64 BytesToRead = FMath::Min<int64>(BufferSize, Reader->TotalSize() - Reader->Tell());
		Reader->Serialize(Buffer.GetData(), BytesToRead);

		for (int32 i = 0; i < BytesToRead; ++i)
		{
			if (Buffer[i] == '\n')
			{
				LineCount++;
			}
		}
		if (BytesToRead > 0)
		{
			LastByte = Buffer[BytesToRead - 1];
		}
	}

	// A trailing newline terminates the last line rather than starting a new
	// one; only count a final unterminated line.
	if (Reader->TotalSize() > 0 && LastByte != '\n')
	{
		LineCount++;
	}

	return LineCount;
}

//-----------------------------------------------------------------------------
// File Reading
//-----------------------------------------------------------------------------

bool FLogReaderService::LoadFileLines(const FString& FilePath, TArray<FString>& OutLines, FLogReadResult& OutResult)
{
	FString ResolvedPath = ResolveFilePath(FilePath);

	if (!FPaths::FileExists(ResolvedPath))
	{
		OutResult.ErrorCode = TEXT("FILE_NOT_FOUND");
		OutResult.ErrorMessage = FString::Printf(TEXT("File not found: %s. Use action=list to see available log files."), *ResolvedPath);
		return false;
	}

	// Use CreateFileReader with FILEREAD_AllowWrite flag to read files that are open for writing
	// This is necessary for reading the main log file while Unreal Engine has it open
	TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*ResolvedPath, FILEREAD_AllowWrite));
	if (!Reader)
	{
		OutResult.ErrorCode = TEXT("FILE_OPEN_FAILED");
		OutResult.ErrorMessage = FString::Printf(TEXT("Failed to open file: %s"), *ResolvedPath);
		return false;
	}

	// Read the entire file into a string
	TArray<uint8> FileData;
	FileData.SetNumUninitialized(Reader->TotalSize());
	
	if (Reader->TotalSize() > 0)
	{
		Reader->Serialize(FileData.GetData(), Reader->TotalSize());
	}
	
	Reader->Close();

	// Convert to string (assuming UTF-8 encoding)
	FString Content;
	FFileHelper::BufferToString(Content, FileData.GetData(), FileData.Num());

	// Split into lines, preserving empty lines
	Content.ParseIntoArray(OutLines, TEXT("\n"), false);

	// Remove trailing \r from each line (Windows line endings)
	for (FString& Line : OutLines)
	{
		if (Line.EndsWith(TEXT("\r")))
		{
			Line.RemoveAt(Line.Len() - 1);
		}
	}

	// A trailing newline produces a phantom empty element; drop it so tail/read
	// return the last real line and counts match CountLines()
	if (OutLines.Num() > 0 && OutLines.Last().IsEmpty())
	{
		OutLines.RemoveAt(OutLines.Num() - 1);
	}

	return true;
}

FLogReadResult FLogReaderService::ReadFile(const FString& FilePath, int32 MaxLines)
{
	FLogReadResult Result;

	TArray<FString> Lines;
	if (!LoadFileLines(FilePath, Lines, Result))
	{
		return Result;
	}

	Result.TotalLines = Lines.Num();
	Result.StartLine = 0;

	int32 LinesToRead = (MaxLines <= 0) ? Lines.Num() : FMath::Min(MaxLines, Lines.Num());
	Result.EndLine = LinesToRead - 1;

	TArray<FString> SelectedLines;
	for (int32 i = 0; i < LinesToRead; ++i)
	{
		SelectedLines.Add(ClampLogLine(Lines[i]));
	}

	Result.Content = FString::Join(SelectedLines, TEXT("\n"));
	Result.bSuccess = true;

	return Result;
}

FLogReadResult FLogReaderService::ReadLines(const FString& FilePath, int32 Offset, int32 Limit)
{
	FLogReadResult Result;

	TArray<FString> Lines;
	if (!LoadFileLines(FilePath, Lines, Result))
	{
		return Result;
	}

	Result.TotalLines = Lines.Num();

	// Clamp offset
	int32 StartLine = FMath::Clamp(Offset, 0, Lines.Num());
	int32 EndLine = (Limit <= 0) ? Lines.Num() : FMath::Min(StartLine + Limit, Lines.Num());

	Result.StartLine = StartLine;
	Result.EndLine = EndLine - 1;

	TArray<FString> SelectedLines;
	for (int32 i = StartLine; i < EndLine; ++i)
	{
		SelectedLines.Add(ClampLogLine(Lines[i]));
	}

	Result.Content = FString::Join(SelectedLines, TEXT("\n"));
	Result.bSuccess = true;

	return Result;
}

FLogReadResult FLogReaderService::TailFile(const FString& FilePath, int32 LineCount)
{
	FLogReadResult Result;

	TArray<FString> Lines;
	if (!LoadFileLines(FilePath, Lines, Result))
	{
		return Result;
	}

	Result.TotalLines = Lines.Num();

	int32 StartLine = FMath::Max(0, Lines.Num() - LineCount);
	Result.StartLine = StartLine;
	Result.EndLine = Lines.Num() - 1;

	TArray<FString> SelectedLines;
	for (int32 i = StartLine; i < Lines.Num(); ++i)
	{
		SelectedLines.Add(ClampLogLine(Lines[i]));
	}

	Result.Content = FString::Join(SelectedLines, TEXT("\n"));
	Result.bSuccess = true;

	return Result;
}

FLogReadResult FLogReaderService::HeadFile(const FString& FilePath, int32 LineCount)
{
	return ReadLines(FilePath, 0, LineCount);
}

//-----------------------------------------------------------------------------
// Filtering
//-----------------------------------------------------------------------------

FLogReadResult FLogReaderService::FilterByPattern(
	const FString& FilePath,
	const FString& Pattern,
	bool bCaseSensitive,
	int32 ContextLines,
	int32 MaxMatches)
{
	FLogReadResult Result;

	TArray<FString> Lines;
	if (!LoadFileLines(FilePath, Lines, Result))
	{
		return Result;
	}

	Result.TotalLines = Lines.Num();

	// Build regex
	FRegexPattern RegexPattern(Pattern, bCaseSensitive ? ERegexPatternFlags::None : ERegexPatternFlags::CaseInsensitive);

	TArray<FString> MatchedContent;
	TSet<int32> IncludedLines;
	int32 MatchCount = 0;

	for (int32 i = 0; i < Lines.Num() && (MaxMatches <= 0 || MatchCount < MaxMatches); ++i)
	{
		FRegexMatcher Matcher(RegexPattern, Lines[i]);
		if (Matcher.FindNext())
		{
			MatchCount++;

			// Include context lines
			int32 ContextStart = FMath::Max(0, i - ContextLines);
			int32 ContextEnd = FMath::Min(Lines.Num() - 1, i + ContextLines);

			for (int32 j = ContextStart; j <= ContextEnd; ++j)
			{
				if (!IncludedLines.Contains(j))
				{
					IncludedLines.Add(j);
				}
			}
		}
	}

	// Build output maintaining line order
	TArray<int32> SortedLines = IncludedLines.Array();
	SortedLines.Sort();

	int32 LastLine = -2;
	for (int32 LineNum : SortedLines)
	{
		// Add separator if there's a gap
		if (LastLine >= 0 && LineNum > LastLine + 1)
		{
			MatchedContent.Add(TEXT("---"));
		}
		MatchedContent.Add(FString::Printf(TEXT("%d: %s"), LineNum + 1, *ClampLogLine(Lines[LineNum])));
		LastLine = LineNum;
	}

	Result.Content = FString::Join(MatchedContent, TEXT("\n"));
	Result.MatchCount = MatchCount;
	Result.bIsFilterResult = true;
	Result.bTruncated = (MaxMatches > 0 && MatchCount >= MaxMatches);
	Result.bSuccess = true;

	return Result;
}

FLogReadResult FLogReaderService::FilterByLogLevel(
	const FString& FilePath,
	const FString& LevelFilter,
	int32 MaxMatches)
{
	// Build pattern for UE log format: LogCategory: Level: Message
	// or simpler: just look for the level keyword
	FString Pattern;

	if (LevelFilter.Equals(TEXT("Error"), ESearchCase::IgnoreCase))
	{
		Pattern = TEXT("\\bError\\b|\\bFatal\\b");
	}
	else if (LevelFilter.Equals(TEXT("Warning"), ESearchCase::IgnoreCase))
	{
		Pattern = TEXT("\\bWarning\\b");
	}
	else if (LevelFilter.Equals(TEXT("Display"), ESearchCase::IgnoreCase))
	{
		Pattern = TEXT("\\bDisplay\\b");
	}
	else if (LevelFilter.Equals(TEXT("Log"), ESearchCase::IgnoreCase))
	{
		Pattern = TEXT(": Log:");
	}
	else if (LevelFilter.Equals(TEXT("Verbose"), ESearchCase::IgnoreCase))
	{
		Pattern = TEXT("\\bVerbose\\b|\\bVeryVerbose\\b");
	}
	else
	{
		// Use the level as-is
		Pattern = LevelFilter;
	}

	return FilterByPattern(FilePath, Pattern, false, 0, MaxMatches);
}

//-----------------------------------------------------------------------------
// Change Detection
//-----------------------------------------------------------------------------

FLogReadResult FLogReaderService::GetNewContent(const FString& FilePath, int32 LastKnownLine)
{
	FLogReadResult Result;

	TArray<FString> Lines;
	if (!LoadFileLines(FilePath, Lines, Result))
	{
		return Result;
	}

	Result.TotalLines = Lines.Num();

	if (LastKnownLine >= Lines.Num())
	{
		// No new content
		Result.bSuccess = true;
		Result.StartLine = Lines.Num();
		Result.EndLine = Lines.Num() - 1;
		Result.Content = TEXT("");
		return Result;
	}

	Result.StartLine = LastKnownLine;
	Result.EndLine = Lines.Num() - 1;

	TArray<FString> NewLines;
	for (int32 i = LastKnownLine; i < Lines.Num(); ++i)
	{
		NewLines.Add(ClampLogLine(Lines[i]));
	}

	Result.Content = FString::Join(NewLines, TEXT("\n"));
	Result.bSuccess = true;

	return Result;
}

bool FLogReaderService::HasFileChanged(const FString& FilePath, const FDateTime& SinceTime)
{
	FString ResolvedPath = ResolveFilePath(FilePath);
	FFileStatData StatData = IFileManager::Get().GetStatData(*ResolvedPath);

	if (!StatData.bIsValid)
	{
		return false;
	}

	return StatData.ModificationTime > SinceTime;
}

//-----------------------------------------------------------------------------
// JSON Conversion
//-----------------------------------------------------------------------------

FString FLogReaderService::LogFileInfoArrayToJson(const TArray<FLogFileInfo>& Files)
{
	TSharedPtr<FJsonObject> RootObj = MakeShared<FJsonObject>();
	RootObj->SetBoolField(TEXT("success"), true);
	RootObj->SetNumberField(TEXT("count"), Files.Num());

	TArray<TSharedPtr<FJsonValue>> FilesArray;
	for (const FLogFileInfo& File : Files)
	{
		TSharedPtr<FJsonObject> FileObj = MakeShared<FJsonObject>();
		FileObj->SetStringField(TEXT("name"), File.Name);
		FileObj->SetStringField(TEXT("path"), File.RelativePath);
		FileObj->SetStringField(TEXT("full_path"), File.FullPath);
		FileObj->SetStringField(TEXT("category"), File.Category);
		FileObj->SetNumberField(TEXT("size_bytes"), File.SizeBytes);
		FileObj->SetStringField(TEXT("size_human"),
			File.SizeBytes < 1024 ? FString::Printf(TEXT("%lld B"), File.SizeBytes) :
			File.SizeBytes < 1024 * 1024 ? FString::Printf(TEXT("%.1f KB"), File.SizeBytes / 1024.0) :
			FString::Printf(TEXT("%.1f MB"), File.SizeBytes / (1024.0 * 1024.0)));
		FileObj->SetStringField(TEXT("modified"), FormatLocalTime(File.ModifiedTime) + TEXT(" (local)"));
		FileObj->SetStringField(TEXT("modified_utc"), File.ModifiedTime.ToString());
		FileObj->SetNumberField(TEXT("line_count"), File.LineCount);
		FilesArray.Add(MakeShared<FJsonValueObject>(FileObj));
	}

	RootObj->SetArrayField(TEXT("files"), FilesArray);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer);
	return JsonString;
}

FString FLogReaderService::LogReadResultToJson(const FLogReadResult& Result)
{
	TSharedPtr<FJsonObject> RootObj = MakeShared<FJsonObject>();
	RootObj->SetBoolField(TEXT("success"), Result.bSuccess);

	if (!Result.bSuccess)
	{
		if (!Result.ErrorCode.IsEmpty())
		{
			RootObj->SetStringField(TEXT("error_code"), Result.ErrorCode);
		}
		RootObj->SetStringField(TEXT("error"), Result.ErrorMessage);
	}
	else if (Result.bIsFilterResult)
	{
		// Filter results are matched lines (each prefixed with its 1-based line
		// number), not a contiguous range — line-offset pagination doesn't apply.
		RootObj->SetStringField(TEXT("content"), Result.Content);
		RootObj->SetNumberField(TEXT("match_count"), Result.MatchCount);
		RootObj->SetNumberField(TEXT("total_lines"), Result.TotalLines);
		RootObj->SetBoolField(TEXT("has_more"), Result.bTruncated);
		if (Result.bTruncated)
		{
			RootObj->SetStringField(TEXT("note"), TEXT("Match limit reached; raise max_matches or narrow the pattern to see more."));
		}
		if (Result.MatchCount == 0)
		{
			RootObj->SetStringField(TEXT("note"), TEXT("No matches found."));
		}
	}
	else
	{
		RootObj->SetStringField(TEXT("content"), Result.Content);
		RootObj->SetNumberField(TEXT("start_line"), Result.StartLine);
		RootObj->SetNumberField(TEXT("end_line"), Result.EndLine);
		RootObj->SetNumberField(TEXT("total_lines"), Result.TotalLines);
		RootObj->SetNumberField(TEXT("lines_returned"), FMath::Max(0, Result.EndLine - Result.StartLine + 1));

		// Pagination hints
		if (Result.EndLine < Result.TotalLines - 1)
		{
			RootObj->SetBoolField(TEXT("has_more"), true);
			RootObj->SetNumberField(TEXT("next_offset"), Result.EndLine + 1);
		}
		else
		{
			RootObj->SetBoolField(TEXT("has_more"), false);
		}
	}

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer);
	return JsonString;
}

FString FLogReaderService::LogFileInfoToJson(const FLogFileInfo& Info)
{
	TSharedPtr<FJsonObject> RootObj = MakeShared<FJsonObject>();
	RootObj->SetBoolField(TEXT("success"), true);
	RootObj->SetStringField(TEXT("name"), Info.Name);
	RootObj->SetStringField(TEXT("path"), Info.RelativePath);
	RootObj->SetStringField(TEXT("full_path"), Info.FullPath);
	RootObj->SetStringField(TEXT("category"), Info.Category);
	RootObj->SetNumberField(TEXT("size_bytes"), Info.SizeBytes);
	RootObj->SetStringField(TEXT("size_human"),
		Info.SizeBytes < 1024 ? FString::Printf(TEXT("%lld B"), Info.SizeBytes) :
		Info.SizeBytes < 1024 * 1024 ? FString::Printf(TEXT("%.1f KB"), Info.SizeBytes / 1024.0) :
		FString::Printf(TEXT("%.1f MB"), Info.SizeBytes / (1024.0 * 1024.0)));
	RootObj->SetStringField(TEXT("modified"), FormatLocalTime(Info.ModifiedTime) + TEXT(" (local)"));
	RootObj->SetStringField(TEXT("modified_utc"), Info.ModifiedTime.ToString());
	RootObj->SetNumberField(TEXT("line_count"), Info.LineCount);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer);
	return JsonString;
}
