// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Core/ToolRegistry.h"
#include "Utils/VibeUEPaths.h"
#include "Tools/PythonTools.h"
#include "Tools/PythonDiscoveryService.h"
#include "Tools/PythonTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Internationalization/Regex.h"

DEFINE_LOG_CATEGORY_STATIC(LogSkillsTools, Log, All);

/**
 * Extract a COMMON_MISTAKES section from skill markdown content.
 * Looks for headings: "## COMMON_MISTAKES" or "### ⚠️ Common Mistakes to Avoid"
 * Returns the section content up to the next heading of equal or higher level, or empty string if not found.
 */
static FString ExtractCommonMistakes(const FString& SkillContent)
{
	// Try multiple heading patterns
	const TArray<FString> Patterns = {
		TEXT("## COMMON_MISTAKES"),
		TEXT("### COMMON_MISTAKES"),
		TEXT("### ⚠️ Common Mistakes to Avoid"),
		TEXT("### Common Mistakes to Avoid"),
		TEXT("## Common Mistakes")
	};

	int32 SectionStart = INDEX_NONE;
	int32 HeadingLevel = 0; // number of '#' chars in the matched heading

	for (const FString& Pattern : Patterns)
	{
		SectionStart = SkillContent.Find(Pattern, ESearchCase::IgnoreCase);
		if (SectionStart != INDEX_NONE)
		{
			// Count '#' chars to determine heading level
			HeadingLevel = 0;
			for (int32 i = 0; i < Pattern.Len() && Pattern[i] == '#'; i++)
			{
				HeadingLevel++;
			}

			// Move past the heading line
			int32 LineEnd = SkillContent.Find(TEXT("\n"), ESearchCase::IgnoreCase, ESearchDir::FromStart, SectionStart);
			if (LineEnd != INDEX_NONE)
			{
				SectionStart = LineEnd + 1;
			}
			break;
		}
	}

	if (SectionStart == INDEX_NONE)
	{
		return FString();
	}

	// Find the end of this section (next heading of equal or higher level)
	int32 SectionEnd = SkillContent.Len();
	FString HeadingPrefix;
	for (int32 i = 0; i < HeadingLevel; i++)
	{
		HeadingPrefix += TEXT("#");
	}
	HeadingPrefix += TEXT(" ");

	// Search for next heading at same or higher level
	int32 SearchPos = SectionStart;
	while (SearchPos < SkillContent.Len())
	{
		int32 NextNewline = SkillContent.Find(TEXT("\n"), ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchPos);
		if (NextNewline == INDEX_NONE)
		{
			break;
		}

		int32 NextLineStart = NextNewline + 1;
		if (NextLineStart < SkillContent.Len())
		{
			// Check if next line starts with a heading of equal or higher level
			bool bIsHeading = true;
			int32 HashCount = 0;
			for (int32 i = NextLineStart; i < SkillContent.Len() && SkillContent[i] == '#'; i++)
			{
				HashCount++;
			}
			if (HashCount > 0 && HashCount <= HeadingLevel)
			{
				SectionEnd = NextNewline;
				break;
			}
		}

		SearchPos = NextLineStart;
	}

	FString Section = SkillContent.Mid(SectionStart, SectionEnd - SectionStart).TrimStartAndEnd();
	return Section;
}

// Helper function to extract a field from ParamsJson
static FString ExtractParamFromJson(const TMap<FString, FString>& Params, const FString& FieldName)
{
	// First check if parameter exists directly (case-insensitive check for action/Action)
	const FString* DirectParam = Params.Find(FieldName);
	if (DirectParam)
	{
		return *DirectParam;
	}
	
	// Also check capitalized version (MCP server capitalizes 'action' to 'Action')
	FString CapitalizedField = FieldName;
	if (CapitalizedField.Len() > 0)
	{
		CapitalizedField[0] = FChar::ToUpper(CapitalizedField[0]);
	}
	DirectParam = Params.Find(CapitalizedField);
	if (DirectParam)
	{
		return *DirectParam;
	}

	// Otherwise, try to extract from ParamsJson
	const FString* ParamsJsonStr = Params.Find(TEXT("ParamsJson"));
	if (!ParamsJsonStr)
	{
		return FString();
	}

	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(*ParamsJsonStr);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		return FString();
	}

	FString Value;
	if (JsonObj->TryGetStringField(FieldName, Value))
	{
		return Value;
	}

	return FString();
}

/**
 * Parse YAML frontmatter from a markdown file
 * Returns a JSON object representing the frontmatter, or nullptr if no frontmatter found
 */
static TSharedPtr<FJsonObject> ParseYAMLFrontmatter(const FString& MarkdownContent)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	// Check if file starts with ---
	if (!MarkdownContent.StartsWith(TEXT("---")))
	{
		return nullptr;
	}

	// Find the closing ---
	int32 StartIndex = 3; // Skip first ---
	int32 EndIndex = MarkdownContent.Find(TEXT("---"), ESearchCase::IgnoreCase, ESearchDir::FromStart, StartIndex);

	if (EndIndex == INDEX_NONE)
	{
		return nullptr;
	}

	// Extract frontmatter content
	FString Frontmatter = MarkdownContent.Mid(StartIndex, EndIndex - StartIndex).TrimStartAndEnd();

	// Parse YAML (simple key: value parser)
	TArray<FString> Lines;
	Frontmatter.ParseIntoArrayLines(Lines);

	TArray<FString> CurrentArrayKey;

	for (const FString& Line : Lines)
	{
		FString TrimmedLine = Line.TrimStartAndEnd();

		if (TrimmedLine.IsEmpty() || TrimmedLine.StartsWith(TEXT("#")))
		{
			continue; // Skip empty lines and comments
		}

		// Check if this is an array item (starts with -)
		if (TrimmedLine.StartsWith(TEXT("- ")))
		{
			FString ArrayValue = TrimmedLine.RightChop(2).TrimStartAndEnd();

			if (!CurrentArrayKey.IsEmpty())
			{
				// Add to the current array
				FString Key = CurrentArrayKey.Last();
				TArray<TSharedPtr<FJsonValue>> Array;

				if (Result->HasField(Key))
				{
					const TArray<TSharedPtr<FJsonValue>>* ExistingArray = nullptr;
					if (Result->TryGetArrayField(Key, ExistingArray) && ExistingArray)
					{
						Array = *ExistingArray;
					}
				}

				Array.Add(MakeShared<FJsonValueString>(ArrayValue));
				Result->SetArrayField(Key, Array);
			}
			continue;
		}

		// Check if this is a key: value pair
		int32 ColonIndex;
		if (TrimmedLine.FindChar(TEXT(':'), ColonIndex))
		{
			FString Key = TrimmedLine.Left(ColonIndex).TrimStartAndEnd();
			FString Value = TrimmedLine.RightChop(ColonIndex + 1).TrimStartAndEnd();

			if (Value.IsEmpty())
			{
				// This key starts an array
				CurrentArrayKey.Empty();
				CurrentArrayKey.Add(Key);
				Result->SetArrayField(Key, TArray<TSharedPtr<FJsonValue>>());
			}
			else
			{
				// Simple key-value
				CurrentArrayKey.Empty();
				Result->SetStringField(Key, Value);
			}
		}
	}

	return Result;
}

/**
 * Format a Python class info as a JSON object for the response
 */
static TSharedPtr<FJsonObject> FormatClassInfoAsJson(const VibeUE::FPythonClassInfo& ClassInfo)
{
	TSharedPtr<FJsonObject> ClassObj = MakeShared<FJsonObject>();
	ClassObj->SetStringField(TEXT("name"), ClassInfo.Name);
	ClassObj->SetStringField(TEXT("full_path"), ClassInfo.FullPath);
	ClassObj->SetStringField(TEXT("docstring"), ClassInfo.Docstring);
	ClassObj->SetBoolField(TEXT("is_abstract"), ClassInfo.bIsAbstract);

	// Base classes
	TArray<TSharedPtr<FJsonValue>> BaseClassesArray;
	for (const FString& BaseClass : ClassInfo.BaseClasses)
	{
		BaseClassesArray.Add(MakeShared<FJsonValueString>(BaseClass));
	}
	ClassObj->SetArrayField(TEXT("base_classes"), BaseClassesArray);

	// Methods
	TArray<TSharedPtr<FJsonValue>> MethodsArray;
	for (const VibeUE::FPythonFunctionInfo& Method : ClassInfo.Methods)
	{
		TSharedPtr<FJsonObject> MethodObj = MakeShared<FJsonObject>();
		MethodObj->SetStringField(TEXT("name"), Method.Name);
		MethodObj->SetStringField(TEXT("signature"), Method.Signature);
		MethodObj->SetStringField(TEXT("docstring"), Method.Docstring);
		MethodObj->SetStringField(TEXT("return_type"), Method.ReturnType);
		MethodObj->SetBoolField(TEXT("is_static"), Method.bIsStatic);

		// Parameters
		TArray<TSharedPtr<FJsonValue>> ParamsArray;
		for (int32 i = 0; i < Method.Parameters.Num(); i++)
		{
			TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
			ParamObj->SetStringField(TEXT("name"), Method.Parameters[i]);
			if (i < Method.ParamTypes.Num())
			{
				ParamObj->SetStringField(TEXT("type"), Method.ParamTypes[i]);
			}
			ParamsArray.Add(MakeShared<FJsonValueObject>(ParamObj));
		}
		MethodObj->SetArrayField(TEXT("parameters"), ParamsArray);

		MethodsArray.Add(MakeShared<FJsonValueObject>(MethodObj));
	}
	ClassObj->SetArrayField(TEXT("methods"), MethodsArray);

	// Properties
	TArray<TSharedPtr<FJsonValue>> PropertiesArray;
	for (const FString& Property : ClassInfo.Properties)
	{
		PropertiesArray.Add(MakeShared<FJsonValueString>(Property));
	}
	ClassObj->SetArrayField(TEXT("properties"), PropertiesArray);

	return ClassObj;
}

/**
 * Discover all services for a skill and return as JSON array
 * Uses IncludeInherited=false to avoid bloating response with base class methods
 */
static TArray<TSharedPtr<FJsonValue>> DiscoverServicesForSkill(const TArray<FString>& ServiceNames)
{
	TArray<TSharedPtr<FJsonValue>> DiscoveryResults;

	auto DiscoveryService = UPythonTools::GetDiscoveryService();
	if (!DiscoveryService.IsValid())
	{
		UE_LOG(LogSkillsTools, Warning, TEXT("PythonDiscoveryService not available"));
		return DiscoveryResults;
	}

	for (const FString& ServiceName : ServiceNames)
	{
		UE_LOG(LogSkillsTools, Log, TEXT("Discovering service: %s"), *ServiceName);

		// Try with unreal. prefix first
		// IncludeInherited=false to exclude base Object methods (acquire_editor_element_handle, etc.)
		// This dramatically reduces response size and keeps only service-specific methods
		FString FullClassName = FString::Printf(TEXT("unreal.%s"), *ServiceName);
		auto Result = DiscoveryService->DiscoverClass(
			FullClassName,
			FString(),  // No method filter
			0,          // No max methods limit
			false,      // IncludeInherited = false - CRITICAL for token reduction
			false       // IncludePrivate = false
		);

		if (Result.IsError())
		{
			// Try without prefix
			Result = DiscoveryService->DiscoverClass(
				ServiceName,
				FString(),
				0,
				false,  // IncludeInherited = false
				false
			);
		}

		if (Result.IsSuccess())
		{
			TSharedPtr<FJsonObject> ClassJson = FormatClassInfoAsJson(Result.GetValue());
			DiscoveryResults.Add(MakeShared<FJsonValueObject>(ClassJson));
			UE_LOG(LogSkillsTools, Log, TEXT("  Discovered %d methods for %s"), Result.GetValue().Methods.Num(), *ServiceName);
		}
		else
		{
			UE_LOG(LogSkillsTools, Warning, TEXT("  Failed to discover service %s: %s"), *ServiceName, *Result.GetErrorMessage());
			// Add error entry
			TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
			ErrorObj->SetStringField(TEXT("name"), ServiceName);
			ErrorObj->SetStringField(TEXT("error"), Result.GetErrorMessage());
			DiscoveryResults.Add(MakeShared<FJsonValueObject>(ErrorObj));
		}
	}

	return DiscoveryResults;
}

/**
 * Scan Skills directory and return metadata for all skills
 */
static FString ListSkills()
{
	FString SkillsDir = FVibeUEPaths::GetPluginContentDir() / TEXT("Skills");

	UE_LOG(LogSkillsTools, Log, TEXT("Scanning skills directory: %s"), *SkillsDir);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Check if Skills directory exists
	if (!PlatformFile.DirectoryExists(*SkillsDir))
	{
		UE_LOG(LogSkillsTools, Warning, TEXT("Skills directory does not exist: %s"), *SkillsDir);

		// Return empty array with success
		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetBoolField(TEXT("success"), true);
		ResultObj->SetArrayField(TEXT("skills"), TArray<TSharedPtr<FJsonValue>>());

		FString ResultJson;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultJson);
		FJsonSerializer::Serialize(ResultObj.ToSharedRef(), Writer);
		return ResultJson;
	}

	// Iterate through subdirectories
	TArray<TSharedPtr<FJsonValue>> SkillsArray;

	PlatformFile.IterateDirectory(*SkillsDir, [&](const TCHAR* FilenameOrDirectory, bool bIsDirectory) -> bool
	{
		if (!bIsDirectory)
		{
			return true; // Skip files in root Skills directory
		}

		FString SkillDirPath = FString(FilenameOrDirectory);
		FString SkillName = FPaths::GetCleanFilename(SkillDirPath);
		FString SkillMdPath = SkillDirPath / TEXT("skill.md");

		UE_LOG(LogSkillsTools, Verbose, TEXT("Found skill directory: %s"), *SkillName);

		// Check if skill.md exists
		if (!PlatformFile.FileExists(*SkillMdPath))
		{
			UE_LOG(LogSkillsTools, Warning, TEXT("Skill '%s' missing skill.md, skipping"), *SkillName);
			return true;
		}

		// Read skill.md
		FString SkillMdContent;
		if (!FFileHelper::LoadFileToString(SkillMdContent, *SkillMdPath))
		{
			UE_LOG(LogSkillsTools, Warning, TEXT("Failed to read skill.md for '%s'"), *SkillName);
			return true;
		}

		// Parse frontmatter
		TSharedPtr<FJsonObject> Frontmatter = ParseYAMLFrontmatter(SkillMdContent);
		if (!Frontmatter.IsValid())
		{
			UE_LOG(LogSkillsTools, Warning, TEXT("Skill '%s' has no valid YAML frontmatter"), *SkillName);
			return true;
		}

		// Count markdown files in skill directory (excluding skill.md)
		int32 FileCount = 0;
		PlatformFile.IterateDirectoryRecursively(*SkillDirPath, [&](const TCHAR* FileOrDir, bool bIsDir) -> bool
		{
			if (!bIsDir && FString(FileOrDir).EndsWith(TEXT(".md")))
			{
				FileCount++;
			}
			return true;
		});

		// Build skill info object
		TSharedPtr<FJsonObject> SkillInfo = MakeShared<FJsonObject>();

		// Copy fields from frontmatter
		for (const auto& Pair : Frontmatter->Values)
		{
			SkillInfo->SetField(Pair.Key, Pair.Value);
		}

		// Add computed fields
		SkillInfo->SetNumberField(TEXT("file_count"), FileCount);

		// Estimate token count (rough: 100 tokens per file, adjusted by file count)
		int32 EstimatedTokens = FileCount * 800; // Average ~800 tokens per content file
		SkillInfo->SetNumberField(TEXT("estimated_tokens"), EstimatedTokens);

		SkillsArray.Add(MakeShared<FJsonValueObject>(SkillInfo));

		UE_LOG(LogSkillsTools, Log, TEXT("Loaded skill metadata: %s (%d files)"), *SkillName, FileCount);

		return true; // Continue iteration
	});

	// Build result JSON
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetArrayField(TEXT("skills"), SkillsArray);

	FString ResultJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultJson);
	FJsonSerializer::Serialize(ResultObj.ToSharedRef(), Writer);

	UE_LOG(LogSkillsTools, Log, TEXT("Listed %d skills"), SkillsArray.Num());

	return ResultJson;
}

/**
 * Suggest skills based on a query string matching against keywords
 * Returns skills sorted by relevance (number of keyword matches)
 */
static FString SuggestSkills(const FString& Query)
{
	FString SkillsDir = FVibeUEPaths::GetPluginContentDir() / TEXT("Skills");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (!PlatformFile.DirectoryExists(*SkillsDir))
	{
		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetBoolField(TEXT("success"), true);
		ResultObj->SetArrayField(TEXT("suggested_skills"), TArray<TSharedPtr<FJsonValue>>());

		FString ResultJson;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultJson);
		FJsonSerializer::Serialize(ResultObj.ToSharedRef(), Writer);
		return ResultJson;
	}

	// Tokenize query into lowercase words
	TArray<FString> QueryWords;
	Query.ToLower().ParseIntoArray(QueryWords, TEXT(" "), true);

	// Store skill matches with scores
	TArray<TPair<int32, TSharedPtr<FJsonObject>>> ScoredSkills;

	PlatformFile.IterateDirectory(*SkillsDir, [&](const TCHAR* FilenameOrDirectory, bool bIsDirectory) -> bool
	{
		if (!bIsDirectory)
		{
			return true;
		}

		FString SkillDirPath = FString(FilenameOrDirectory);
		FString SkillName = FPaths::GetCleanFilename(SkillDirPath);
		FString SkillMdPath = SkillDirPath / TEXT("skill.md");

		if (!PlatformFile.FileExists(*SkillMdPath))
		{
			return true;
		}

		FString SkillMdContent;
		if (!FFileHelper::LoadFileToString(SkillMdContent, *SkillMdPath))
		{
			return true;
		}

		TSharedPtr<FJsonObject> Frontmatter = ParseYAMLFrontmatter(SkillMdContent);
		if (!Frontmatter.IsValid())
		{
			return true;
		}

		// Collect all keywords from the skill
		TArray<FString> AllKeywords;

		// Add name and display_name as implicit keywords
		FString Name, DisplayName, Description;
		if (Frontmatter->TryGetStringField(TEXT("name"), Name))
		{
			AllKeywords.Add(Name.ToLower());
		}
		if (Frontmatter->TryGetStringField(TEXT("display_name"), DisplayName))
		{
			// Split display name into words
			TArray<FString> DisplayWords;
			DisplayName.ToLower().ParseIntoArray(DisplayWords, TEXT(" "), true);
			AllKeywords.Append(DisplayWords);
		}
		if (Frontmatter->TryGetStringField(TEXT("description"), Description))
		{
			// Split description into words
			TArray<FString> DescWords;
			Description.ToLower().ParseIntoArray(DescWords, TEXT(" "), true);
			AllKeywords.Append(DescWords);
		}

		// Add explicit keywords
		const TArray<TSharedPtr<FJsonValue>>* KeywordsArray = nullptr;
		if (Frontmatter->TryGetArrayField(TEXT("keywords"), KeywordsArray) && KeywordsArray)
		{
			for (const TSharedPtr<FJsonValue>& Value : *KeywordsArray)
			{
				FString Keyword;
				if (Value->TryGetString(Keyword))
				{
					AllKeywords.Add(Keyword.ToLower());
				}
			}
		}

		// Add vibeue_classes and unreal_classes as keywords
		const TArray<TSharedPtr<FJsonValue>>* ClassesArray = nullptr;
		if (Frontmatter->TryGetArrayField(TEXT("vibeue_classes"), ClassesArray) && ClassesArray)
		{
			for (const TSharedPtr<FJsonValue>& Value : *ClassesArray)
			{
				FString ClassName;
				if (Value->TryGetString(ClassName))
				{
					AllKeywords.Add(ClassName.ToLower());
				}
			}
		}
		if (Frontmatter->TryGetArrayField(TEXT("unreal_classes"), ClassesArray) && ClassesArray)
		{
			for (const TSharedPtr<FJsonValue>& Value : *ClassesArray)
			{
				FString ClassName;
				if (Value->TryGetString(ClassName))
				{
					AllKeywords.Add(ClassName.ToLower());
				}
			}
		}

		// Calculate match score
		int32 Score = 0;
		for (const FString& QueryWord : QueryWords)
		{
			for (const FString& Keyword : AllKeywords)
			{
				// Exact match worth 3 points
				if (Keyword.Equals(QueryWord))
				{
					Score += 3;
				}
				// Contains match worth 1 point
				else if (Keyword.Contains(QueryWord) || QueryWord.Contains(Keyword))
				{
					Score += 1;
				}
			}
		}

		if (Score > 0)
		{
			TSharedPtr<FJsonObject> SkillInfo = MakeShared<FJsonObject>();
			SkillInfo->SetStringField(TEXT("name"), Name.IsEmpty() ? SkillName : Name);
			if (!DisplayName.IsEmpty())
			{
				SkillInfo->SetStringField(TEXT("display_name"), DisplayName);
			}
			if (!Description.IsEmpty())
			{
				SkillInfo->SetStringField(TEXT("description"), Description);
			}
			SkillInfo->SetNumberField(TEXT("relevance_score"), Score);

			ScoredSkills.Add(TPair<int32, TSharedPtr<FJsonObject>>(Score, SkillInfo));
		}

		return true;
	});

	// Sort by score descending
	ScoredSkills.Sort([](const TPair<int32, TSharedPtr<FJsonObject>>& A, const TPair<int32, TSharedPtr<FJsonObject>>& B)
	{
		return A.Key > B.Key;
	});

	// Build result
	TArray<TSharedPtr<FJsonValue>> SuggestedArray;
	for (const auto& ScoredSkill : ScoredSkills)
	{
		SuggestedArray.Add(MakeShared<FJsonValueObject>(ScoredSkill.Value));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("query"), Query);
	ResultObj->SetArrayField(TEXT("suggested_skills"), SuggestedArray);

	if (SuggestedArray.Num() > 0)
	{
		ResultObj->SetStringField(TEXT("hint"), TEXT("Use manage_skills(action='load', skill_name='<name>') to load a skill"));
	}
	else
	{
		ResultObj->SetStringField(TEXT("hint"), TEXT("No matching skills found. Use manage_skills(action='list') to see all available skills"));
	}

	FString ResultJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultJson);
	FJsonSerializer::Serialize(ResultObj.ToSharedRef(), Writer);

	UE_LOG(LogSkillsTools, Log, TEXT("Suggested %d skills for query '%s'"), SuggestedArray.Num(), *Query);

	return ResultJson;
}

/**
 * Resolve skill name to directory path
 * Supports: directory name, `name` field, or `display_name` field (case-insensitive)
 */
static FString ResolveSkillDirectory(const FString& SkillName)
{
	FString SkillsDir = FVibeUEPaths::GetPluginContentDir() / TEXT("Skills");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// 1. Try as directory name first
	FString DirectPath = SkillsDir / SkillName;
	if (PlatformFile.DirectoryExists(*DirectPath))
	{
		UE_LOG(LogSkillsTools, Verbose, TEXT("Resolved '%s' to directory: %s"), *SkillName, *DirectPath);
		return DirectPath;
	}

	// 2. Scan all skill.md files and match on name or display_name
	FString ResolvedPath;

	PlatformFile.IterateDirectory(*SkillsDir, [&](const TCHAR* FilenameOrDirectory, bool bIsDirectory) -> bool
	{
		if (!bIsDirectory)
		{
			return true;
		}

		FString SkillDirPath = FString(FilenameOrDirectory);
		FString SkillMdPath = SkillDirPath / TEXT("skill.md");

		if (!PlatformFile.FileExists(*SkillMdPath))
		{
			return true;
		}

		// Read and parse skill.md
		FString SkillMdContent;
		if (!FFileHelper::LoadFileToString(SkillMdContent, *SkillMdPath))
		{
			return true;
		}

		TSharedPtr<FJsonObject> Frontmatter = ParseYAMLFrontmatter(SkillMdContent);
		if (!Frontmatter.IsValid())
		{
			return true;
		}

		// Check `name` field (exact match)
		FString Name;
		if (Frontmatter->TryGetStringField(TEXT("name"), Name) && Name.Equals(SkillName))
		{
			ResolvedPath = SkillDirPath;
			return false; // Stop iteration
		}

		// Check `display_name` field (case-insensitive)
		FString DisplayName;
		if (Frontmatter->TryGetStringField(TEXT("display_name"), DisplayName) && DisplayName.Equals(SkillName, ESearchCase::IgnoreCase))
		{
			ResolvedPath = SkillDirPath;
			return false; // Stop iteration
		}

		return true; // Continue iteration
	});

	if (!ResolvedPath.IsEmpty())
	{
		UE_LOG(LogSkillsTools, Log, TEXT("Resolved '%s' via skill.md metadata: %s"), *SkillName, *ResolvedPath);
	}
	else
	{
		UE_LOG(LogSkillsTools, Warning, TEXT("Failed to resolve skill name: %s"), *SkillName);
	}

	return ResolvedPath;
}

/**
 * Helper struct to collect skill data before building response
 */
struct FSkillData
{
	FString SkillName;
	FString SkillDir;
	TArray<FString> VibeUEClassNames;
	TArray<FString> UnrealClassNames;
	TArray<FString> MarkdownFiles;
};

/**
 * Load skill data from a directory (classes and file list, but not content yet)
 */
static bool LoadSkillData(const FString& SkillName, FSkillData& OutData)
{
	OutData.SkillName = SkillName;
	OutData.SkillDir = ResolveSkillDirectory(SkillName);
	
	if (OutData.SkillDir.IsEmpty())
	{
		return false;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Read skill.md to get the class lists
	FString SkillMdPath = OutData.SkillDir / TEXT("skill.md");

	if (PlatformFile.FileExists(*SkillMdPath))
	{
		FString SkillMdContent;
		if (FFileHelper::LoadFileToString(SkillMdContent, *SkillMdPath))
		{
			TSharedPtr<FJsonObject> SkillFrontmatter = ParseYAMLFrontmatter(SkillMdContent);
			if (SkillFrontmatter.IsValid())
			{
				// Extract vibeue_classes array
				const TArray<TSharedPtr<FJsonValue>>* VibeUEArray = nullptr;
				if (SkillFrontmatter->TryGetArrayField(TEXT("vibeue_classes"), VibeUEArray) && VibeUEArray)
				{
					for (const TSharedPtr<FJsonValue>& ClassValue : *VibeUEArray)
					{
						FString ClassName;
						if (ClassValue->TryGetString(ClassName))
						{
							OutData.VibeUEClassNames.Add(ClassName);
						}
					}
				}

				// Extract unreal_classes array
				const TArray<TSharedPtr<FJsonValue>>* UnrealArray = nullptr;
				if (SkillFrontmatter->TryGetArrayField(TEXT("unreal_classes"), UnrealArray) && UnrealArray)
				{
					for (const TSharedPtr<FJsonValue>& ClassValue : *UnrealArray)
					{
						FString ClassName;
						if (ClassValue->TryGetString(ClassName))
						{
							OutData.UnrealClassNames.Add(ClassName);
						}
					}
				}
			}
		}
	}

	// Collect all .md files in the skill directory
	PlatformFile.IterateDirectoryRecursively(*OutData.SkillDir, [&](const TCHAR* FileOrDir, bool bIsDir) -> bool
	{
		if (!bIsDir)
		{
			FString FilePath = FString(FileOrDir);
			if (FilePath.EndsWith(TEXT(".md")))
			{
				OutData.MarkdownFiles.Add(FilePath);
			}
		}
		return true;
	});

	OutData.MarkdownFiles.Sort();
	return true;
}

/**
 * Load multiple skills with deduplicated discovery
 */
static FString LoadMultipleSkills(const TArray<FString>& SkillNames)
{
	UE_LOG(LogSkillsTools, Log, TEXT("Loading %d skills with deduplication"), SkillNames.Num());

	// Collect data from all skills
	TArray<FSkillData> AllSkillData;
	TArray<FString> FailedSkills;
	
	for (const FString& SkillName : SkillNames)
	{
		FSkillData Data;
		if (LoadSkillData(SkillName, Data))
		{
			AllSkillData.Add(MoveTemp(Data));
			UE_LOG(LogSkillsTools, Log, TEXT("  Loaded skill data: %s"), *SkillName);
		}
		else
		{
			FailedSkills.Add(SkillName);
			UE_LOG(LogSkillsTools, Warning, TEXT("  Failed to load skill: %s"), *SkillName);
		}
	}

	if (AllSkillData.Num() == 0)
	{
		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetBoolField(TEXT("success"), false);
		ErrorObj->SetStringField(TEXT("error"), TEXT("No skills could be loaded"));
		
		TArray<TSharedPtr<FJsonValue>> FailedArray;
		for (const FString& Name : FailedSkills)
		{
			FailedArray.Add(MakeShared<FJsonValueString>(Name));
		}
		ErrorObj->SetArrayField(TEXT("failed_skills"), FailedArray);

		FString ErrorJson;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ErrorJson);
		FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
		return ErrorJson;
	}

	// Deduplicate classes across all skills
	TSet<FString> UniqueVibeUEClasses;
	TSet<FString> UniqueUnrealClasses;
	
	for (const FSkillData& Data : AllSkillData)
	{
		for (const FString& ClassName : Data.VibeUEClassNames)
		{
			UniqueVibeUEClasses.Add(ClassName);
		}
		for (const FString& ClassName : Data.UnrealClassNames)
		{
			UniqueUnrealClasses.Add(ClassName);
		}
	}

	TArray<FString> MergedVibeUEClasses = UniqueVibeUEClasses.Array();
	TArray<FString> MergedUnrealClasses = UniqueUnrealClasses.Array();

	UE_LOG(LogSkillsTools, Log, TEXT("Skill classes: %d VibeUE, %d Unreal (AI will discover methods as needed)"), 
		MergedVibeUEClasses.Num(), MergedUnrealClasses.Num());

	// NOTE: We no longer auto-discover methods here
	// The AI should call discover_python_class with a method_filter to get what it needs
	// This produces better results than dumping 80+ methods that the AI ignores

	// Concatenate content from all skills with separators
	FString ConcatenatedContent;
	TArray<FString> FilesLoaded;
	TArray<FString> LoadedSkillNames;
	int32 TotalTokens = 0;

	for (const FSkillData& Data : AllSkillData)
	{
		LoadedSkillNames.Add(FPaths::GetCleanFilename(Data.SkillDir));

		// Add skill separator
		if (!ConcatenatedContent.IsEmpty())
		{
			ConcatenatedContent += TEXT("\n\n========================================\n");
			ConcatenatedContent += FString::Printf(TEXT("# SKILL: %s\n"), *FPaths::GetCleanFilename(Data.SkillDir));
			ConcatenatedContent += TEXT("========================================\n\n");
		}
		else
		{
			ConcatenatedContent += FString::Printf(TEXT("# SKILL: %s\n\n"), *FPaths::GetCleanFilename(Data.SkillDir));
		}

		// Add files from this skill
		FString SkillsDir = FVibeUEPaths::GetPluginContentDir() / TEXT("Skills");
		for (const FString& FilePath : Data.MarkdownFiles)
		{
			FString FileContent;
			if (FFileHelper::LoadFileToString(FileContent, *FilePath))
			{
				FString RelativePath = FilePath;
				RelativePath.RemoveFromStart(SkillsDir + TEXT("/"));
				RelativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
				FilesLoaded.Add(RelativePath);

				ConcatenatedContent += TEXT("\n---\n\n");
				ConcatenatedContent += FileContent;
			}
		}
	}

	TotalTokens = ConcatenatedContent.Len() / 4;

	// Build result JSON
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	
	// List skills loaded
	TArray<TSharedPtr<FJsonValue>> SkillsArray;
	for (const FString& Name : LoadedSkillNames)
	{
		SkillsArray.Add(MakeShared<FJsonValueString>(Name));
	}
	ResultObj->SetArrayField(TEXT("skills_loaded"), SkillsArray);

	// Instructions for AI to discover methods
	ResultObj->SetStringField(TEXT("IMPORTANT"), 
		TEXT("BEFORE writing code, call discover_python_class to get method signatures. "
		     "Example: discover_python_class('unreal.BlueprintService', method_filter='variable') "
		     "to find all variable-related methods. The 'content' below has workflows and gotchas."));

	// Extract COMMON_MISTAKES from skill markdown content (skill-specific, not hardcoded)
	FString ExtractedMistakes = ExtractCommonMistakes(ConcatenatedContent);
	if (!ExtractedMistakes.IsEmpty())
	{
		ResultObj->SetStringField(TEXT("COMMON_MISTAKES"), ExtractedMistakes);
	}

	// Add class lists - AI should call discover_python_class on these
	TArray<TSharedPtr<FJsonValue>> VibeUEClassesArray;
	for (const FString& ClassName : MergedVibeUEClasses)
	{
		VibeUEClassesArray.Add(MakeShared<FJsonValueString>(ClassName));
	}
	ResultObj->SetArrayField(TEXT("vibeue_classes"), VibeUEClassesArray);
	ResultObj->SetStringField(TEXT("vibeue_classes_usage"), 
		TEXT("Call discover_python_class('unreal.ClassName', method_filter='keyword') to get methods"));

	TArray<TSharedPtr<FJsonValue>> UnrealClassesArray;
	for (const FString& ClassName : MergedUnrealClasses)
	{
		UnrealClassesArray.Add(MakeShared<FJsonValueString>(ClassName));
	}
	ResultObj->SetArrayField(TEXT("unreal_classes"), UnrealClassesArray);

	// Prepend instruction to use discovery tools
	FString ContentWithWarning = TEXT("## How to Use This Skill\n\n")
		TEXT("1. Call `discover_python_class('unreal.ClassName', method_filter='keyword')` to find methods\n")
		TEXT("2. Read the COMMON_MISTAKES section above to avoid wrong method names\n")
		TEXT("3. The workflows below show patterns but USE DISCOVERED SIGNATURES for exact syntax\n\n")
		TEXT("4. WidgetService does NOT have create_widget() - use BlueprintService with 'UserWidget' parent\n\n")
		+ ConcatenatedContent;
	ResultObj->SetStringField(TEXT("content"), ContentWithWarning);

	TArray<TSharedPtr<FJsonValue>> FilesArray;
	for (const FString& File : FilesLoaded)
	{
		FilesArray.Add(MakeShared<FJsonValueString>(File));
	}
	ResultObj->SetArrayField(TEXT("files_loaded"), FilesArray);
	ResultObj->SetNumberField(TEXT("token_count"), TotalTokens);

	// Add failed skills if any
	if (FailedSkills.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> FailedArray;
		for (const FString& Name : FailedSkills)
		{
			FailedArray.Add(MakeShared<FJsonValueString>(Name));
		}
		ResultObj->SetArrayField(TEXT("failed_skills"), FailedArray);
	}

	FString ResultJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultJson);
	FJsonSerializer::Serialize(ResultObj.ToSharedRef(), Writer);

	UE_LOG(LogSkillsTools, Log, TEXT("Loaded %d skills: %d files, ~%d tokens, %d VibeUE + %d Unreal classes (deduplicated)"), 
		LoadedSkillNames.Num(), FilesLoaded.Num(), TotalTokens, MergedVibeUEClasses.Num(), MergedUnrealClasses.Num());

	return ResultJson;
}

/**
 * Load a single skill (streamlined output format)
 */
static FString LoadSingleSkill(const FString& SkillName)
{
	UE_LOG(LogSkillsTools, Log, TEXT("Loading skill: %s"), *SkillName);

	// Use the common LoadSkillData helper
	FSkillData SkillData;
	if (!LoadSkillData(SkillName, SkillData))
	{
		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetBoolField(TEXT("success"), false);
		ErrorObj->SetStringField(TEXT("error"), FString::Printf(TEXT("Skill not found: %s"), *SkillName));

		FString ErrorJson;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ErrorJson);
		FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
		return ErrorJson;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Concatenate all files
	FString ConcatenatedContent;
	TArray<FString> FilesLoaded;

	for (const FString& FilePath : SkillData.MarkdownFiles)
	{
		FString FileContent;
		if (FFileHelper::LoadFileToString(FileContent, *FilePath))
		{
			// Get relative path from Skills directory
			FString RelativePath = FilePath;
			FString SkillsDir = FVibeUEPaths::GetPluginContentDir() / TEXT("Skills");
			RelativePath.RemoveFromStart(SkillsDir + TEXT("/"));
			RelativePath.ReplaceInline(TEXT("\\"), TEXT("/"));

			FilesLoaded.Add(RelativePath);

			// Add separator between files
			if (!ConcatenatedContent.IsEmpty())
			{
				ConcatenatedContent += TEXT("\n\n---\n\n");
			}

			ConcatenatedContent += FileContent;
		}
		else
		{
			UE_LOG(LogSkillsTools, Warning, TEXT("Failed to read file: %s"), *FilePath);
		}
	}

	// Get skill name from directory
	FString ActualSkillName = FPaths::GetCleanFilename(SkillData.SkillDir);

	// NOTE: We no longer auto-discover methods here
	// The AI should call discover_python_class with a method_filter to get what it needs
	// This produces better results than dumping 80+ methods that the AI ignores

	// Build result JSON
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("skill_name"), ActualSkillName);
	
	// Instructions for AI to discover methods
	ResultObj->SetStringField(TEXT("IMPORTANT"), 
		TEXT("BEFORE writing code, call discover_python_class to get method signatures. ")
		TEXT("Example: discover_python_class('unreal.BlueprintService', method_filter='variable') ")
		TEXT("to find all variable-related methods. The 'content' below has workflows and gotchas."));

	// Extract COMMON_MISTAKES from skill markdown content (skill-specific, not hardcoded)
	FString ExtractedMistakesSingle = ExtractCommonMistakes(ConcatenatedContent);
	if (!ExtractedMistakesSingle.IsEmpty())
	{
		ResultObj->SetStringField(TEXT("COMMON_MISTAKES"), ExtractedMistakesSingle);
	}

	// Add class lists - AI should call discover_python_class on these
	TArray<TSharedPtr<FJsonValue>> VibeUEClassesArray;
	for (const FString& ClassName : SkillData.VibeUEClassNames)
	{
		VibeUEClassesArray.Add(MakeShared<FJsonValueString>(ClassName));
	}
	ResultObj->SetArrayField(TEXT("vibeue_classes"), VibeUEClassesArray);
	ResultObj->SetStringField(TEXT("vibeue_classes_usage"), 
		TEXT("Call discover_python_class('unreal.ClassName', method_filter='keyword') to get methods"));

	TArray<TSharedPtr<FJsonValue>> UnrealClassesArray;
	for (const FString& ClassName : SkillData.UnrealClassNames)
	{
		UnrealClassesArray.Add(MakeShared<FJsonValueString>(ClassName));
	}
	ResultObj->SetArrayField(TEXT("unreal_classes"), UnrealClassesArray);

	// Content LAST - workflows and gotchas only, not method signatures
	// Prepend critical instruction to content so AI CANNOT miss it
	FString ContentWithWarning = TEXT("## How to Use This Skill\n\n")
		TEXT("1. Call discover_python_class('unreal.ClassName', method_filter='keyword') to find methods\n")
		TEXT("2. Read COMMON_MISTAKES above to avoid common errors\n")
		TEXT("3. Use the workflows below for common patterns\n\n")
		+ ConcatenatedContent;
	ResultObj->SetStringField(TEXT("content"), ContentWithWarning);

	TArray<TSharedPtr<FJsonValue>> FilesArray;
	for (const FString& File : FilesLoaded)
	{
		FilesArray.Add(MakeShared<FJsonValueString>(File));
	}
	ResultObj->SetArrayField(TEXT("files_loaded"), FilesArray);

	// Rough token count estimate (1 token ≈ 4 characters)
	int32 TokenCount = ConcatenatedContent.Len() / 4;
	ResultObj->SetNumberField(TEXT("token_count"), TokenCount);

	FString ResultJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultJson);
	FJsonSerializer::Serialize(ResultObj.ToSharedRef(), Writer);

	UE_LOG(LogSkillsTools, Log, TEXT("Loaded skill '%s': %d files, ~%d tokens, %d VibeUE + %d Unreal classes"), 
		*ActualSkillName, FilesLoaded.Num(), TokenCount, SkillData.VibeUEClassNames.Num(), SkillData.UnrealClassNames.Num());

	return ResultJson;
}

/**
 * Load one or more skills with deduplicated discovery
 * Unified function that handles both single and multiple skill loading
 */
static FString LoadSkills(const TArray<FString>& SkillNames)
{
	if (SkillNames.Num() == 0)
	{
		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetBoolField(TEXT("success"), false);
		ErrorObj->SetStringField(TEXT("error"), TEXT("No skill names provided"));

		FString ErrorJson;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ErrorJson);
		FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
		return ErrorJson;
	}

	// For single skill, use streamlined output format
	if (SkillNames.Num() == 1)
	{
		return LoadSingleSkill(SkillNames[0]);
	}

	// Multiple skills - use batch loading with deduplication
	return LoadMultipleSkills(SkillNames);
}

// Helper to parse skill_names array from JSON
static TArray<FString> ExtractSkillNamesArray(const TMap<FString, FString>& Params)
{
	TArray<FString> Result;
	
	// Try to get skill_names from ParamsJson
	const FString* ParamsJsonStr = Params.Find(TEXT("ParamsJson"));
	if (!ParamsJsonStr)
	{
		return Result;
	}

	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(*ParamsJsonStr);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		return Result;
	}

	// Check for skill_names array (case variations)
	const TArray<TSharedPtr<FJsonValue>>* SkillNamesArray = nullptr;
	if (!JsonObj->TryGetArrayField(TEXT("skill_names"), SkillNamesArray))
	{
		JsonObj->TryGetArrayField(TEXT("Skill_names"), SkillNamesArray);
	}

	if (SkillNamesArray)
	{
		for (const TSharedPtr<FJsonValue>& Value : *SkillNamesArray)
		{
			FString Name;
			if (Value->TryGetString(Name))
			{
				Result.Add(Name);
			}
		}
	}

	return Result;
}

// Helper struct and function for sanitizing malformed manage_skills action parameters.
// Extracted outside REGISTER_VIBEUE_TOOL macro because TMap<K,V> template commas
// confuse the C preprocessor's argument parsing.
struct FSanitizedAction
{
	FString Action;
	TMap<FString, FString> RecoveredParams;
};

static FSanitizedAction SanitizeManageSkillsAction(const FString& RawAction)
{
	FSanitizedAction Result;
	Result.Action = RawAction;

	// Some LLMs (e.g. Gemini Flash) produce malformed JSON escaping that
	// merges all parameters into the "action" value, e.g.:
	//   {"action":"load\", \"skill_name\": \"landscape"}
	// After JSON parsing this becomes Action = "load", "skill_name": "landscape"
	// We sanitize by:
	// 1. Extracting embedded parameters from the corrupted string
	// 2. Truncating action at first quote/comma to get the real action
	if (Result.Action.Contains(TEXT("\"")))
	{
		// Try to parse embedded key-value pairs from the corrupted action string
		// Pattern: load", "skill_name": "landscape"  →  skill_name=landscape
		FRegexPattern Pattern(TEXT("\"(\\w+)\"\\s*:\\s*\"([^\"]+)\""));
		FRegexMatcher Matcher(Pattern, Result.Action);
		while (Matcher.FindNext())
		{
			FString Key = Matcher.GetCaptureGroup(1);
			FString Value = Matcher.GetCaptureGroup(2);
			Result.RecoveredParams.Add(Key, Value);
			UE_LOG(LogTemp, Warning, TEXT("manage_skills: Recovered embedded param '%s'='%s' from malformed action string"), *Key, *Value);
		}

		// Truncate action at first quote/comma
		int32 QuoteIdx = INDEX_NONE;
		Result.Action.FindChar(TEXT('"'), QuoteIdx);
		int32 CommaIdx = INDEX_NONE;
		Result.Action.FindChar(TEXT(','), CommaIdx);
		int32 TruncateAt = INDEX_NONE;
		if (QuoteIdx != INDEX_NONE && CommaIdx != INDEX_NONE)
		{
			TruncateAt = FMath::Min(QuoteIdx, CommaIdx);
		}
		else if (QuoteIdx != INDEX_NONE)
		{
			TruncateAt = QuoteIdx;
		}
		else if (CommaIdx != INDEX_NONE)
		{
			TruncateAt = CommaIdx;
		}
		if (TruncateAt != INDEX_NONE)
		{
			Result.Action = Result.Action.Left(TruncateAt).TrimEnd();
		}
		UE_LOG(LogTemp, Warning, TEXT("manage_skills: Sanitized malformed action to '%s' (recovered %d embedded params)"), *Result.Action, Result.RecoveredParams.Num());
	}

	return Result;
}

// Register manage_skills tool
REGISTER_VIBEUE_TOOL(manage_skills,
	"Discover and load domain-specific knowledge skills. Use 'list' to see available skills, 'suggest' to find skills matching a query, 'load' to load a skill by name or display_name. Use 'skill_names' array to load multiple skills with deduplicated discovery.",
	"Skills",
	TOOL_PARAMS(
		TOOL_PARAM("action", "Action to perform: 'list', 'suggest', or 'load'", "string", true),
		TOOL_PARAM("query", "Query string to match against skill keywords (for 'suggest' action)", "string", false),
		TOOL_PARAM("skill_name", "Name of a single skill to load (for 'load' action). Can be directory name, 'name' field, or 'display_name' field from skill.md", "string", false),
		TOOL_PARAM("skill_names", "Array of skill names to load together with deduplicated discovery (for 'load' action). More efficient when loading multiple related skills.", "array", false)
	),
	{
		FString RawAction = ExtractParamFromJson(Params, TEXT("action"));
		FSanitizedAction Sanitized = SanitizeManageSkillsAction(RawAction);
		FString Action = Sanitized.Action;

		if (Action.Equals(TEXT("list"), ESearchCase::IgnoreCase))
		{
			return ListSkills();
		}
		else if (Action.Equals(TEXT("suggest"), ESearchCase::IgnoreCase))
		{
			FString Query = ExtractParamFromJson(Params, TEXT("query"));
			// Fall back to recovered params from malformed JSON
			if (Query.IsEmpty() && Sanitized.RecoveredParams.Contains(TEXT("query")))
			{
				Query = Sanitized.RecoveredParams.FindRef(TEXT("query"));
			}
			if (Query.IsEmpty())
			{
				TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
				ErrorObj->SetBoolField(TEXT("success"), false);
				ErrorObj->SetStringField(TEXT("error"), TEXT("'query' parameter required for 'suggest' action"));

				FString ErrorJson;
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ErrorJson);
				FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
				return ErrorJson;
			}
			return SuggestSkills(Query);
		}
		else if (Action.Equals(TEXT("load"), ESearchCase::IgnoreCase))
		{
			// Build skill names list from either parameter
			TArray<FString> SkillNames = ExtractSkillNamesArray(Params);
			
			// If no array provided, check for single skill_name
			if (SkillNames.Num() == 0)
			{
				FString SkillName = ExtractParamFromJson(Params, TEXT("skill_name"));
				// Fall back to recovered params from malformed JSON
				if (SkillName.IsEmpty() && Sanitized.RecoveredParams.Contains(TEXT("skill_name")))
				{
					SkillName = Sanitized.RecoveredParams.FindRef(TEXT("skill_name"));
				}
				if (!SkillName.IsEmpty())
				{
					SkillNames.Add(SkillName);
				}
			}
			
			if (SkillNames.Num() == 0)
			{
				TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
				ErrorObj->SetBoolField(TEXT("success"), false);
				ErrorObj->SetStringField(TEXT("error"), TEXT("Either 'skill_name' or 'skill_names' parameter required for 'load' action"));

				FString ErrorJson;
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ErrorJson);
				FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
				return ErrorJson;
			}

			return LoadSkills(SkillNames);
		}
		else
		{
			TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
			ErrorObj->SetBoolField(TEXT("success"), false);
			ErrorObj->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown action: %s. Must be 'list', 'suggest', or 'load'"), *Action));

			FString ErrorJson;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ErrorJson);
			FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
			return ErrorJson;
		}
	}
);
