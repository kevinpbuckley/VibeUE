// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "Commands/FileSystemCommands.h"
#include "Core/ErrorCodes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/ScopeExit.h"
#include "Internationalization/Regex.h"
#include "Interfaces/IPluginManager.h"

using namespace VibeUE;

FFileSystemCommands::FFileSystemCommands()
{
}

TSharedPtr<FJsonObject> FFileSystemCommands::HandleCommand(
	const FString& ToolName,
	const TSharedPtr<FJsonObject>& Params)
{
	if (ToolName == TEXT("read_file"))
	{
		return HandleReadFile(Params);
	}
	else if (ToolName == TEXT("list_dir"))
	{
		return HandleListDir(Params);
	}
	else if (ToolName == TEXT("file_search"))
	{
		return HandleFileSearch(Params);
	}
	else if (ToolName == TEXT("grep_search"))
	{
		return HandleGrepSearch(Params);
	}
	else if (ToolName == TEXT("get_directories"))
	{
		return HandleGetDirectories(Params);
	}

	return CreateErrorResponse(
		ErrorCodes::PARAM_INVALID,
		FString::Printf(TEXT("Unknown filesystem tool: %s"), *ToolName)
	);
}

TSharedPtr<FJsonObject> FFileSystemCommands::HandleReadFile(const TSharedPtr<FJsonObject>& Params)
{
	// Get file path
	FString FilePath;
	if (!Params->TryGetStringField(TEXT("filePath"), FilePath))
	{
		return CreateErrorResponse(ErrorCodes::PARAM_MISSING, TEXT("Missing required parameter: filePath"));
	}

	// Validate and normalize path
	FString Error;
	if (!ValidateAndNormalizePath(FilePath, Error))
	{
		return CreateErrorResponse(ErrorCodes::PARAM_INVALID, Error);
	}

	// Get optional line range parameters (1-indexed like VSCode)
	int32 StartLine = 1;
	int32 EndLine = -1; // -1 means end of file
	double StartLineDouble, EndLineDouble;
	if (Params->TryGetNumberField(TEXT("startLine"), StartLineDouble))
	{
		StartLine = FMath::Max(1, (int32)StartLineDouble);
	}
	if (Params->TryGetNumberField(TEXT("endLine"), EndLineDouble))
	{
		EndLine = (int32)EndLineDouble;
	}

	// Check if file exists
	if (!FPaths::FileExists(FilePath))
	{
		return CreateErrorResponse(
			ErrorCodes::FILE_NOT_FOUND,
			FString::Printf(TEXT("File not found: %s"), *FilePath)
		);
	}

	// Load file into line array
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *FilePath))
	{
		return CreateErrorResponse(
			ErrorCodes::FILE_READ_ERROR,
			FString::Printf(TEXT("Failed to read file: %s"), *FilePath)
		);
	}

	// Determine actual end line
	int32 TotalLines = Lines.Num();
	if (EndLine < 0 || EndLine > TotalLines)
	{
		EndLine = TotalLines;
	}

	// Validate range
	if (StartLine > TotalLines)
	{
		return CreateErrorResponse(
			ErrorCodes::PARAM_INVALID,
			FString::Printf(TEXT("startLine (%d) exceeds file length (%d lines)"), StartLine, TotalLines)
		);
	}

	// Build content string (convert to 0-indexed for array access)
	TArray<FString> SelectedLines;
	for (int32 i = StartLine - 1; i < EndLine; i++)
	{
		if (i >= 0 && i < Lines.Num())
		{
			SelectedLines.Add(Lines[i]);
		}
	}

	FString Content = FString::Join(SelectedLines, TEXT("\n"));

	// Build response
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("filePath"), FilePath);
	Data->SetNumberField(TEXT("startLine"), StartLine);
	Data->SetNumberField(TEXT("endLine"), EndLine);
	Data->SetNumberField(TEXT("totalLines"), TotalLines);
	Data->SetStringField(TEXT("content"), Content);
	Response->SetObjectField(TEXT("data"), Data);

	return Response;
}

TSharedPtr<FJsonObject> FFileSystemCommands::HandleListDir(const TSharedPtr<FJsonObject>& Params)
{
	// Get directory path
	FString Path;
	if (!Params->TryGetStringField(TEXT("path"), Path))
	{
		return CreateErrorResponse(ErrorCodes::PARAM_MISSING, TEXT("Missing required parameter: path"));
	}

	// Validate and normalize path
	FString Error;
	if (!ValidateAndNormalizePath(Path, Error))
	{
		return CreateErrorResponse(ErrorCodes::PARAM_INVALID, Error);
	}

	// Check if directory exists
	if (!FPaths::DirectoryExists(Path))
	{
		return CreateErrorResponse(
			ErrorCodes::FILE_NOT_FOUND,
			FString::Printf(TEXT("Directory not found: %s"), *Path)
		);
	}

	// List directory contents
	TArray<FString> Files;
	TArray<FString> Directories;

	IFileManager& FileManager = IFileManager::Get();

	// Find all files
	FileManager.FindFiles(Files, *(Path / TEXT("*")), true, false);

	// Find all directories
	FileManager.FindFiles(Directories, *(Path / TEXT("*")), false, true);

	// Build response
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), Path);

	TArray<TSharedPtr<FJsonValue>> EntriesArray;

	// Add directories
	for (const FString& Dir : Directories)
	{
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Dir + TEXT("/"));
		Entry->SetStringField(TEXT("type"), TEXT("directory"));
		EntriesArray.Add(MakeShared<FJsonValueObject>(Entry));
	}

	// Add files
	for (const FString& File : Files)
	{
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), File);
		Entry->SetStringField(TEXT("type"), TEXT("file"));

		// Get file size
		FString FullPath = Path / File;
		int64 FileSize = FileManager.FileSize(*FullPath);
		if (FileSize >= 0)
		{
			Entry->SetNumberField(TEXT("size"), (double)FileSize);
		}

		EntriesArray.Add(MakeShared<FJsonValueObject>(Entry));
	}

	Data->SetArrayField(TEXT("entries"), EntriesArray);
	Data->SetNumberField(TEXT("fileCount"), Files.Num());
	Data->SetNumberField(TEXT("directoryCount"), Directories.Num());
	Response->SetObjectField(TEXT("data"), Data);

	return Response;
}

TSharedPtr<FJsonObject> FFileSystemCommands::HandleFileSearch(const TSharedPtr<FJsonObject>& Params)
{
	// Get search pattern
	FString Query;
	if (!Params->TryGetStringField(TEXT("query"), Query))
	{
		return CreateErrorResponse(ErrorCodes::PARAM_MISSING, TEXT("Missing required parameter: query"));
	}

	// Get optional max results
	int32 MaxResults = 100;
	double MaxResultsDouble;
	if (Params->TryGetNumberField(TEXT("maxResults"), MaxResultsDouble))
	{
		MaxResults = FMath::Max(1, (int32)MaxResultsDouble);
	}

	// Start from project root or plugin root
	FString SearchRoot = FPaths::ProjectDir();

	// Find matching files
	TArray<FString> MatchingFiles;
	FindFilesRecursive(SearchRoot, Query, MatchingFiles, MaxResults);

	// Build response
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("query"), Query);
	Data->SetNumberField(TEXT("totalResults"), MatchingFiles.Num());

	TArray<TSharedPtr<FJsonValue>> FilesArray;
	for (const FString& File : MatchingFiles)
	{
		FilesArray.Add(MakeShared<FJsonValueString>(File));
	}

	Data->SetArrayField(TEXT("files"), FilesArray);
	Response->SetObjectField(TEXT("data"), Data);

	return Response;
}

TSharedPtr<FJsonObject> FFileSystemCommands::HandleGrepSearch(const TSharedPtr<FJsonObject>& Params)
{
	// Get search query
	FString Query;
	if (!Params->TryGetStringField(TEXT("query"), Query))
	{
		return CreateErrorResponse(ErrorCodes::PARAM_MISSING, TEXT("Missing required parameter: query"));
	}

	// Get optional parameters
	bool bIsRegexp = false;
	Params->TryGetBoolField(TEXT("isRegexp"), bIsRegexp);

	FString IncludePattern;
	Params->TryGetStringField(TEXT("includePattern"), IncludePattern);

	bool bIncludeIgnoredFiles = false;
	Params->TryGetBoolField(TEXT("includeIgnoredFiles"), bIncludeIgnoredFiles);

	int32 MaxResults = 50;
	double MaxResultsDouble;
	if (Params->TryGetNumberField(TEXT("maxResults"), MaxResultsDouble))
	{
		MaxResults = FMath::Max(1, (int32)MaxResultsDouble);
	}

	// Determine search root
	FString SearchRoot = FPaths::ProjectDir();

	// Find files to search
	TArray<FString> FilesToSearch;
	FString SearchPattern = IncludePattern.IsEmpty() ? TEXT("**/*.{cpp,h,cs,py,ini,json,md,txt,uproject,uplugin}") : IncludePattern;
	FindFilesRecursive(SearchRoot, SearchPattern, FilesToSearch, 10000); // High limit for grep

	// Search through files
	TArray<FGrepMatch> AllMatches;
	int32 MatchCount = 0;

	for (const FString& File : FilesToSearch)
	{
		if (MatchCount >= MaxResults)
		{
			break;
		}

		// Skip ignored files unless explicitly included
		if (!bIncludeIgnoredFiles)
		{
			FString Filename = FPaths::GetCleanFilename(File);
			FString Directory = FPaths::GetPath(File);

			// Skip common build/cache directories
			if (Directory.Contains(TEXT("/Intermediate/")) ||
				Directory.Contains(TEXT("/Binaries/")) ||
				Directory.Contains(TEXT("/DerivedDataCache/")) ||
				Directory.Contains(TEXT("/.git/")) ||
				Directory.Contains(TEXT("/node_modules/")))
			{
				continue;
			}
		}

		GrepSearchInFile(File, Query, bIsRegexp, AllMatches, MaxResults);
		MatchCount = AllMatches.Num();
	}

	// Build response
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("query"), Query);
	Data->SetBoolField(TEXT("isRegexp"), bIsRegexp);
	Data->SetNumberField(TEXT("totalMatches"), AllMatches.Num());

	TArray<TSharedPtr<FJsonValue>> MatchesArray;
	for (const FGrepMatch& Match : AllMatches)
	{
		TSharedPtr<FJsonObject> MatchObj = MakeShared<FJsonObject>();
		MatchObj->SetStringField(TEXT("file"), Match.FilePath);
		MatchObj->SetNumberField(TEXT("lineNumber"), Match.LineNumber);
		MatchObj->SetStringField(TEXT("line"), Match.LineText);

		TArray<TSharedPtr<FJsonValue>> RangesArray;
		for (const auto& Range : Match.MatchRanges)
		{
			TSharedPtr<FJsonObject> RangeObj = MakeShared<FJsonObject>();
			RangeObj->SetNumberField(TEXT("start"), Range.Key);
			RangeObj->SetNumberField(TEXT("length"), Range.Value);
			RangesArray.Add(MakeShared<FJsonValueObject>(RangeObj));
		}
		MatchObj->SetArrayField(TEXT("ranges"), RangesArray);

		MatchesArray.Add(MakeShared<FJsonValueObject>(MatchObj));
	}

	Data->SetArrayField(TEXT("matches"), MatchesArray);
	Response->SetObjectField(TEXT("data"), Data);

	return Response;
}

TSharedPtr<FJsonObject> FFileSystemCommands::HandleGetDirectories(const TSharedPtr<FJsonObject>& Params)
{
	// Get important project and engine directories
	FString EngineDir = FPaths::EngineDir();
	FString GameDir = FPaths::ProjectDir();
	FString PluginDir = FPaths::Combine(GameDir, TEXT("Plugins"), TEXT("VibeUE"));
	
	// Determine platform-specific paths
#if PLATFORM_WINDOWS
	FString PlatformDir = TEXT("Win64");
#elif PLATFORM_MAC
	FString PlatformDir = TEXT("Mac");
#elif PLATFORM_LINUX
	FString PlatformDir = TEXT("Linux");
#else
	FString PlatformDir = TEXT("Win64"); // Default fallback
#endif

	// Build Python paths
	FString PythonIncludeDir = FPaths::Combine(EngineDir, TEXT("Source"), TEXT("ThirdParty"), TEXT("Python3"), PlatformDir, TEXT("include"));
	FString PythonLibDir = FPaths::Combine(EngineDir, TEXT("Source"), TEXT("ThirdParty"), TEXT("Python3"), PlatformDir, TEXT("Lib"));
	FString PythonSitePackagesDir = FPaths::Combine(EngineDir, TEXT("Plugins"), TEXT("Experimental"), TEXT("PythonScriptPlugin"), TEXT("Content"), TEXT("Python"));
	
	// Normalize paths
	FPaths::NormalizeDirectoryName(GameDir);
	FPaths::NormalizeDirectoryName(PluginDir);
	FPaths::NormalizeDirectoryName(PythonIncludeDir);
	FPaths::NormalizeDirectoryName(PythonLibDir);
	FPaths::NormalizeDirectoryName(PythonSitePackagesDir);

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	
	// Add all directory paths
	Data->SetStringField(TEXT("gameDir"), GameDir);
	Data->SetStringField(TEXT("pluginDir"), PluginDir);
	Data->SetStringField(TEXT("pythonIncludeDir"), PythonIncludeDir);
	Data->SetStringField(TEXT("pythonLibDir"), PythonLibDir);
	Data->SetStringField(TEXT("pythonSitePackagesDir"), PythonSitePackagesDir);
	Data->SetStringField(TEXT("engineDir"), EngineDir);
	Data->SetStringField(TEXT("platform"), PlatformDir);
	Data->SetStringField(TEXT("description"), TEXT("Important project directories: game, plugin, and UE Python API paths"));
	
	// Check which directories actually exist
	TArray<TSharedPtr<FJsonValue>> ExistingDirsArray;
	if (FPaths::DirectoryExists(GameDir))
	{
		ExistingDirsArray.Add(MakeShared<FJsonValueString>(GameDir));
	}
	if (FPaths::DirectoryExists(PluginDir))
	{
		ExistingDirsArray.Add(MakeShared<FJsonValueString>(PluginDir));
	}
	if (FPaths::DirectoryExists(PythonIncludeDir))
	{
		ExistingDirsArray.Add(MakeShared<FJsonValueString>(PythonIncludeDir));
	}
	if (FPaths::DirectoryExists(PythonLibDir))
	{
		ExistingDirsArray.Add(MakeShared<FJsonValueString>(PythonLibDir));
	}
	if (FPaths::DirectoryExists(PythonSitePackagesDir))
	{
		ExistingDirsArray.Add(MakeShared<FJsonValueString>(PythonSitePackagesDir));
	}
	Data->SetArrayField(TEXT("existingDirectories"), ExistingDirsArray);
	
	Response->SetObjectField(TEXT("data"), Data);

	return Response;
}

// Helper Methods

TSharedPtr<FJsonObject> FFileSystemCommands::CreateSuccessResponse() const
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	return Response;
}

TSharedPtr<FJsonObject> FFileSystemCommands::CreateErrorResponse(
	const FString& ErrorCode,
	const FString& ErrorMessage) const
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), false);

	TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
	Error->SetStringField(TEXT("code"), ErrorCode);
	Error->SetStringField(TEXT("message"), ErrorMessage);

	Response->SetObjectField(TEXT("error"), Error);

	return Response;
}

bool FFileSystemCommands::ValidateAndNormalizePath(FString& InOutPath, FString& OutError) const
{
	// Convert to absolute path if relative
	if (FPaths::IsRelative(InOutPath))
	{
		InOutPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), InOutPath);
	}

	// Normalize path
	FPaths::NormalizeFilename(InOutPath);
	FPaths::CollapseRelativeDirectories(InOutPath);

	// Check for path traversal attempts
	if (InOutPath.Contains(TEXT("..")))
	{
		OutError = TEXT("Path traversal (..) not allowed");
		return false;
	}

	// Ensure path is within allowed directories
	if (!IsPathAllowed(InOutPath))
	{
		OutError = FString::Printf(TEXT("Access denied: Path outside project directory: %s"), *InOutPath);
		return false;
	}

	return true;
}

FString FFileSystemCommands::GetPluginSourceRoot() const
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("VibeUE"));
	if (Plugin.IsValid())
	{
		return Plugin->GetContentDir() / TEXT("Python");
	}
	return FPaths::ProjectPluginsDir() / TEXT("VibeUE") / TEXT("Content") / TEXT("Python");
}

bool FFileSystemCommands::IsPathAllowed(const FString& Path) const
{
	// Allow access to project directory and subdirectories
	FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeFilename(ProjectDir);

	FString TestPath = Path;
	FPaths::NormalizeFilename(TestPath);

	// Check if path starts with project directory
	if (TestPath.StartsWith(ProjectDir))
	{
		return true;
	}

	// Also allow plugin directories
	FString PluginsDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir());
	FPaths::NormalizeFilename(PluginsDir);

	if (TestPath.StartsWith(PluginsDir))
	{
		return true;
	}

	// Allow access to entire Unreal Engine installation directory
	FString EngineDir = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
	FPaths::NormalizeFilename(EngineDir);
	
	if (TestPath.StartsWith(EngineDir))
	{
		return true;
	}

	return false;
}

bool FFileSystemCommands::MatchesGlobPattern(const FString& Path, const FString& Pattern) const
{
	// Simple glob matching for common patterns
	// Supports: *, **, ?, [abc], {a,b,c}

	// Convert glob to regex-like matching
	FString RegexPattern = Pattern;

	// Escape regex special characters except our glob characters
	RegexPattern = RegexPattern.Replace(TEXT("."), TEXT("\\."));
	RegexPattern = RegexPattern.Replace(TEXT("+"), TEXT("\\+"));
	RegexPattern = RegexPattern.Replace(TEXT("^"), TEXT("\\^"));
	RegexPattern = RegexPattern.Replace(TEXT("$"), TEXT("\\$"));
	RegexPattern = RegexPattern.Replace(TEXT("("), TEXT("\\("));
	RegexPattern = RegexPattern.Replace(TEXT(")"), TEXT("\\)"));
	RegexPattern = RegexPattern.Replace(TEXT("|"), TEXT("\\|"));

	// Convert glob wildcards to regex
	RegexPattern = RegexPattern.Replace(TEXT("**"), TEXT("<!DOUBLESTAR!>"));
	RegexPattern = RegexPattern.Replace(TEXT("*"), TEXT("[^/\\\\]*"));
	RegexPattern = RegexPattern.Replace(TEXT("<!DOUBLESTAR!>"), TEXT(".*"));
	RegexPattern = RegexPattern.Replace(TEXT("?"), TEXT("."));

	// Handle {a,b,c} patterns
	RegexPattern = RegexPattern.Replace(TEXT("{"), TEXT("("));
	RegexPattern = RegexPattern.Replace(TEXT("}"), TEXT(")"));
	RegexPattern = RegexPattern.Replace(TEXT(","), TEXT("|"));

	// Create regex pattern
	FRegexPattern Regex(RegexPattern, ERegexPatternFlags::CaseInsensitive);
	FRegexMatcher Matcher(Regex, Path);

	return Matcher.FindNext();
}

void FFileSystemCommands::FindFilesRecursive(
	const FString& Directory,
	const FString& Pattern,
	TArray<FString>& OutFiles,
	int32 MaxResults) const
{
	if (OutFiles.Num() >= MaxResults)
	{
		return;
	}

	IFileManager& FileManager = IFileManager::Get();

	// Handle glob patterns
	TArray<FString> AllFiles;
	FileManager.FindFilesRecursive(AllFiles, *Directory, TEXT("*"), true, false);

	for (const FString& File : AllFiles)
	{
		if (OutFiles.Num() >= MaxResults)
		{
			break;
		}

		// Get relative path for pattern matching
		FString RelativePath = File;
		FPaths::MakePathRelativeTo(RelativePath, *Directory);

		if (MatchesGlobPattern(File, Pattern))
		{
			OutFiles.Add(File);
		}
	}
}

void FFileSystemCommands::GrepSearchInFile(
	const FString& FilePath,
	const FString& Pattern,
	bool bIsRegex,
	TArray<FGrepMatch>& OutMatches,
	int32 MaxResults) const
{
	if (OutMatches.Num() >= MaxResults)
	{
		return;
	}

	// Load file into lines
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *FilePath))
	{
		return;
	}

	// Search each line
	for (int32 i = 0; i < Lines.Num(); i++)
	{
		if (OutMatches.Num() >= MaxResults)
		{
			break;
		}

		const FString& Line = Lines[i];

		if (bIsRegex)
		{
			// Regex search
			FRegexPattern Regex(Pattern, ERegexPatternFlags::CaseInsensitive);
			FRegexMatcher Matcher(Regex, Line);

			if (Matcher.FindNext())
			{
				FGrepMatch Match;
				Match.FilePath = FilePath;
				Match.LineNumber = i + 1; // 1-indexed
				Match.LineText = Line;

				// Get match position
				int32 Start = Matcher.GetMatchBeginning();
				int32 End = Matcher.GetMatchEnding();
				Match.MatchRanges.Add(TPair<int32, int32>(Start, End - Start));

				OutMatches.Add(Match);
			}
		}
		else
		{
			// Simple case-insensitive substring search
			int32 FindIndex = Line.Find(Pattern, ESearchCase::IgnoreCase);
			if (FindIndex != INDEX_NONE)
			{
				FGrepMatch Match;
				Match.FilePath = FilePath;
				Match.LineNumber = i + 1; // 1-indexed
				Match.LineText = Line;
				Match.MatchRanges.Add(TPair<int32, int32>(FindIndex, Pattern.Len()));

				OutMatches.Add(Match);
			}
		}
	}
}
