// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Core/ToolRegistry.h"
#include "Chat/ChatSession.h"
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

// Forward declarations for helpers used by ListSkills before they are defined further down.
static TArray<TSharedPtr<FJsonValue>> EnumerateSections(const FString& SkillDir);
static TArray<TSharedPtr<FJsonValue>> EnumerateResources(const FString& SkillDir);
static void ParseSkillSpec(const FString& RawSkillName, FString& OutFolder, FString& OutSubDoc);

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
		FString SkillMdPath = SkillDirPath / TEXT("SKILL.md");

		UE_LOG(LogSkillsTools, Verbose, TEXT("Found skill directory: %s"), *SkillName);

		// Check if SKILL.md exists
		if (!PlatformFile.FileExists(*SkillMdPath))
		{
			UE_LOG(LogSkillsTools, Warning, TEXT("Skill '%s' missing SKILL.md, skipping"), *SkillName);
			return true;
		}

		// Read SKILL.md
		FString SkillMdContent;
		if (!FFileHelper::LoadFileToString(SkillMdContent, *SkillMdPath))
		{
			UE_LOG(LogSkillsTools, Warning, TEXT("Failed to read SKILL.md for '%s'"), *SkillName);
			return true;
		}

		// Parse frontmatter
		TSharedPtr<FJsonObject> Frontmatter = ParseYAMLFrontmatter(SkillMdContent);
		if (!Frontmatter.IsValid())
		{
			UE_LOG(LogSkillsTools, Warning, TEXT("Skill '%s' has no valid YAML frontmatter"), *SkillName);
			return true;
		}

		// Enumerate sibling sub-doc files so list callers can see at a glance
		// what additional sections are loadable for this skill.
		TArray<TSharedPtr<FJsonValue>> SectionsArray = EnumerateSections(SkillDirPath);
		TArray<TSharedPtr<FJsonValue>> ResourcesArray = EnumerateResources(SkillDirPath);

		// Build skill info object
		TSharedPtr<FJsonObject> SkillInfo = MakeShared<FJsonObject>();

		// Copy fields from frontmatter
		for (const auto& Pair : Frontmatter->Values)
		{
			SkillInfo->SetField(Pair.Key, Pair.Value);
		}

		// Always include `sections` (empty array when no sub-docs exist) so the
		// agent can rely on the field being present and decide which sub-doc
		// to load via `vibeue-skills-manager(action="load", skill_name="<skill>/<section>")`.
		SkillInfo->SetArrayField(TEXT("sections"), SectionsArray);
		SkillInfo->SetNumberField(TEXT("section_count"), SectionsArray.Num());
		SkillInfo->SetArrayField(TEXT("resources"), ResourcesArray);
		SkillInfo->SetNumberField(TEXT("resource_count"), ResourcesArray.Num());

		SkillsArray.Add(MakeShared<FJsonValueObject>(SkillInfo));

		UE_LOG(LogSkillsTools, Log, TEXT("Loaded skill metadata: %s (%d sub-docs)"), *SkillName, SectionsArray.Num());

		return true; // Continue iteration
	});

	// Build result JSON
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetArrayField(TEXT("skills"), SkillsArray);
	ResultObj->SetStringField(TEXT("usage"),
		TEXT("Each skill entry has a `sections` array. Load the index with "
			 "`vibeue-skills-manager(action='load', skill_name='<skill>')` to get workflows and gotchas. "
			 "Load a specific sub-doc with `skill_name='<skill>/<section>'` to pull deeper reference material on demand."));

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
		FString SkillMdPath = SkillDirPath / TEXT("SKILL.md");

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
		ResultObj->SetStringField(TEXT("hint"), TEXT("Use vibeue-skills-manager(action='load', skill_name='<name>') to load a skill"));
	}
	else
	{
		ResultObj->SetStringField(TEXT("hint"), TEXT("No matching skills found. Use vibeue-skills-manager(action='list') to see all available skills"));
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

	// 2. Scan all SKILL.md files and match on name or display_name
	FString ResolvedPath;

	PlatformFile.IterateDirectory(*SkillsDir, [&](const TCHAR* FilenameOrDirectory, bool bIsDirectory) -> bool
	{
		if (!bIsDirectory)
		{
			return true;
		}

		FString SkillDirPath = FString(FilenameOrDirectory);
		FString SkillMdPath = SkillDirPath / TEXT("SKILL.md");

		if (!PlatformFile.FileExists(*SkillMdPath))
		{
			return true;
		}

		// Read and parse SKILL.md
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
		UE_LOG(LogSkillsTools, Log, TEXT("Resolved '%s' via SKILL.md metadata: %s"), *SkillName, *ResolvedPath);
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
	FString RequestedName;                                   // Original input — e.g. "state-trees" or "state-trees/transitions"
	FString SkillName;                                       // Resolved folder name — e.g. "state-trees"
	FString SkillDir;                                        // Absolute path to skill folder
	FString SubDocName;                                      // Empty for index load; otherwise the sub-doc filename without .md
	TArray<FString> VibeUEClassNames;
	TArray<FString> UnrealClassNames;
	TArray<FString> MarkdownFiles;                           // Files whose contents will be returned (1 element after split refactor)
	TArray<TSharedPtr<FJsonValue>> AvailableSections;        // {name, description} for sibling .md files (excluding SKILL.md)
	TArray<TSharedPtr<FJsonValue>> AvailableResources;       // {name, type, description} for bundled scripts + reference files
};

/**
 * Split a skill spec like "state-trees" or "state-trees/transitions" into
 * its folder portion and optional sub-doc portion. Accepts both forward and
 * back slashes; only the FIRST separator is honored, so nested folders are
 * treated as one sub-doc name (we don't currently support nested subfolders).
 */
static void ParseSkillSpec(const FString& RawSkillName, FString& OutFolder, FString& OutSubDoc)
{
	int32 SlashIdx = INDEX_NONE;
	if (!RawSkillName.FindChar(TEXT('/'), SlashIdx))
	{
		RawSkillName.FindChar(TEXT('\\'), SlashIdx);
	}

	if (SlashIdx != INDEX_NONE)
	{
		OutFolder = RawSkillName.Left(SlashIdx).TrimStartAndEnd();
		OutSubDoc = RawSkillName.Mid(SlashIdx + 1).TrimStartAndEnd();
		// Strip a trailing .md if the caller included it
		if (OutSubDoc.EndsWith(TEXT(".md"), ESearchCase::IgnoreCase))
		{
			OutSubDoc = OutSubDoc.Left(OutSubDoc.Len() - 3);
		}
	}
	else
	{
		OutFolder = RawSkillName.TrimStartAndEnd();
		OutSubDoc.Empty();
	}
}

/**
 * Enumerate sibling .md files inside a skill directory (excluding SKILL.md itself).
 * Each entry is a JSON object with `name` (bare filename without .md) and, when
 * the sub-doc has its own YAML frontmatter `description:`, a `description` field.
 * Sub-docs are the loadable sections returned when the agent requests
 * `skill_name="<folder>/<section>"`.
 */
static TArray<TSharedPtr<FJsonValue>> EnumerateSections(const FString& SkillDir)
{
	TArray<TSharedPtr<FJsonValue>> Sections;
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (SkillDir.IsEmpty() || !PlatformFile.DirectoryExists(*SkillDir))
	{
		return Sections;
	}

	PlatformFile.IterateDirectory(*SkillDir, [&](const TCHAR* FilenameOrDirectory, bool bIsDirectory) -> bool
	{
		if (bIsDirectory)
		{
			return true;
		}

		FString FilePath = FString(FilenameOrDirectory);
		FString FileName = FPaths::GetCleanFilename(FilePath);

		if (!FileName.EndsWith(TEXT(".md"), ESearchCase::IgnoreCase))
		{
			return true;
		}
		if (FileName.Equals(TEXT("SKILL.md"), ESearchCase::IgnoreCase))
		{
			return true;
		}

		TSharedPtr<FJsonObject> SectionObj = MakeShared<FJsonObject>();
		SectionObj->SetStringField(TEXT("name"), FPaths::GetBaseFilename(FileName));

		// Pull description from sub-doc frontmatter if present, so list / load responses
		// can show the agent what each sub-doc covers without loading them all.
		FString FileContent;
		if (FFileHelper::LoadFileToString(FileContent, *FilePath))
		{
			TSharedPtr<FJsonObject> Frontmatter = ParseYAMLFrontmatter(FileContent);
			if (Frontmatter.IsValid())
			{
				FString Desc;
				if (Frontmatter->TryGetStringField(TEXT("description"), Desc))
				{
					SectionObj->SetStringField(TEXT("description"), Desc);
				}
			}
		}

		Sections.Add(MakeShared<FJsonValueObject>(SectionObj));
		return true;
	});

	// Stable alphabetical order so the agent sees a consistent menu
	Sections.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		FString NameA, NameB;
		if (A->AsObject().IsValid()) A->AsObject()->TryGetStringField(TEXT("name"), NameA);
		if (B->AsObject().IsValid()) B->AsObject()->TryGetStringField(TEXT("name"), NameB);
		return NameA < NameB;
	});

	return Sections;
}

/**
 * Enumerate bundled resources for a skill — every file under the skill folder EXCEPT `SKILL.md`
 * and the top-level `.md` sub-docs (those are surfaced separately as `sections`). This covers the
 * three Agent-Skills bundled-content kinds beyond the index: runnable Code (scripts/*.pyx|.py),
 * additional reference docs (e.g. reference/*.md), and Resources (schemas, templates, data, examples).
 *
 * Each entry is `{name, type, description}`:
 *   - `name`        loadable spec suffix = the file's path relative to the skill folder (forward
 *                   slashes), WITH extension — pass it as skill_name="<skill>/<name>".
 *   - `type`        "script" for .py/.pyx (run via execute_python_code) else "reference" (read as
 *                   reference material).
 *   - `description` for scripts, the first comment line; for .md, the frontmatter description; else omitted.
 */
static TArray<TSharedPtr<FJsonValue>> EnumerateResources(const FString& SkillDir)
{
	TArray<TSharedPtr<FJsonValue>> Resources;
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (SkillDir.IsEmpty() || !PlatformFile.DirectoryExists(*SkillDir))
	{
		return Resources;
	}

	FString NormalizedDir = SkillDir;
	FPaths::NormalizeDirectoryName(NormalizedDir);

	PlatformFile.IterateDirectoryRecursively(*SkillDir, [&](const TCHAR* FilenameOrDirectory, bool bIsDirectory) -> bool
	{
		if (bIsDirectory)
		{
			return true;
		}

		FString FilePath = FString(FilenameOrDirectory);
		const FString FileName = FPaths::GetCleanFilename(FilePath);

		// Relative path (forward slashes) used as the loadable spec suffix.
		FString RelPath = FilePath;
		FPaths::MakePathRelativeTo(RelPath, *(NormalizedDir / TEXT("")));
		RelPath.ReplaceInline(TEXT("\\"), TEXT("/"));

		// SKILL.md is the index; top-level .md files are advertised as `sections`.
		const bool bIsMarkdown = FileName.EndsWith(TEXT(".md"), ESearchCase::IgnoreCase);
		const bool bTopLevel = !RelPath.Contains(TEXT("/"));
		if (FileName.Equals(TEXT("SKILL.md"), ESearchCase::IgnoreCase) || (bIsMarkdown && bTopLevel))
		{
			return true;
		}

		const bool bIsScript = FileName.EndsWith(TEXT(".pyx"), ESearchCase::IgnoreCase) ||
			FileName.EndsWith(TEXT(".py"), ESearchCase::IgnoreCase);

		TSharedPtr<FJsonObject> ResObj = MakeShared<FJsonObject>();
		ResObj->SetStringField(TEXT("name"), RelPath);
		ResObj->SetStringField(TEXT("type"), bIsScript ? TEXT("script") : TEXT("reference"));

		FString FileContent;
		if (FFileHelper::LoadFileToString(FileContent, *FilePath))
		{
			if (bIsScript)
			{
				// Description = first descriptive comment line ("# <file> — <desc>").
				TArray<FString> Lines;
				FileContent.ParseIntoArrayLines(Lines);
				for (const FString& Line : Lines)
				{
					FString Trimmed = Line.TrimStartAndEnd();
					if (Trimmed.IsEmpty() || Trimmed.StartsWith(TEXT("#!")))
					{
						continue;
					}
					if (Trimmed.StartsWith(TEXT("#")))
					{
						Trimmed = Trimmed.RightChop(1).TrimStart();
					}
					if (!Trimmed.IsEmpty())
					{
						ResObj->SetStringField(TEXT("description"), Trimmed);
					}
					break;
				}
			}
			else if (bIsMarkdown)
			{
				// Description from the sub-doc's YAML frontmatter, when present.
				TSharedPtr<FJsonObject> Frontmatter = ParseYAMLFrontmatter(FileContent);
				FString Desc;
				if (Frontmatter.IsValid() && Frontmatter->TryGetStringField(TEXT("description"), Desc))
				{
					ResObj->SetStringField(TEXT("description"), Desc);
				}
			}
		}

		Resources.Add(MakeShared<FJsonValueObject>(ResObj));
		return true;
	});

	Resources.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		FString NameA, NameB;
		if (A->AsObject().IsValid()) A->AsObject()->TryGetStringField(TEXT("name"), NameA);
		if (B->AsObject().IsValid()) B->AsObject()->TryGetStringField(TEXT("name"), NameB);
		return NameA < NameB;
	});

	return Resources;
}

/**
 * Load skill data from a directory.
 *
 * `RawSkillName` may be either a bare skill folder name (e.g. "state-trees")
 * or a sub-doc path (e.g. "state-trees/transitions"). In both cases the root
 * `SKILL.md` is consulted for the class metadata (`vibeue_classes` /
 * `unreal_classes`). For an index load only `SKILL.md` is selected as the
 * file to return; for a sub-doc load only `<folder>/<subdoc>.md` is selected.
 *
 * We deliberately do NOT recursively concatenate every .md in the folder
 * anymore — large skills were blowing past the MCP tool-output token cap.
 * Sibling .md files are surfaced as `AvailableSections` so the agent can
 * decide whether to load any of them explicitly.
 */
static bool LoadSkillData(const FString& RawSkillName, FSkillData& OutData)
{
	OutData.RequestedName = RawSkillName;
	ParseSkillSpec(RawSkillName, OutData.SkillName, OutData.SubDocName);

	OutData.SkillDir = ResolveSkillDirectory(OutData.SkillName);
	if (OutData.SkillDir.IsEmpty())
	{
		return false;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Always read root SKILL.md for the class lists — they live on the index
	// regardless of which sub-doc the agent ultimately loads.
	FString SkillMdPath = OutData.SkillDir / TEXT("SKILL.md");
	if (PlatformFile.FileExists(*SkillMdPath))
	{
		FString SkillMdContent;
		if (FFileHelper::LoadFileToString(SkillMdContent, *SkillMdPath))
		{
			TSharedPtr<FJsonObject> SkillFrontmatter = ParseYAMLFrontmatter(SkillMdContent);
			if (SkillFrontmatter.IsValid())
			{
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

	// Select which file(s) to return based on whether a sub-doc / script was requested.
	if (!OutData.SubDocName.IsEmpty())
	{
		// Resolve in order: <sub>.md (reference sub-doc), <sub>.pyx / <sub>.py (runnable script,
		// e.g. "scripts/create_widget"), then the path exactly as given (if it already has an ext).
		const TArray<FString> Candidates = {
			OutData.SkillDir / (OutData.SubDocName + TEXT(".md")),
			OutData.SkillDir / (OutData.SubDocName + TEXT(".pyx")),
			OutData.SkillDir / (OutData.SubDocName + TEXT(".py")),
			OutData.SkillDir / OutData.SubDocName,
		};

		FString ResolvedPath;
		for (const FString& Candidate : Candidates)
		{
			if (PlatformFile.FileExists(*Candidate))
			{
				ResolvedPath = Candidate;
				break;
			}
		}

		if (ResolvedPath.IsEmpty())
		{
			UE_LOG(LogSkillsTools, Warning, TEXT("Sub-doc/script not found: '%s' in skill '%s'"),
				*OutData.SubDocName, *OutData.SkillName);
			return false;
		}
		OutData.MarkdownFiles.Add(ResolvedPath);
	}
	else if (PlatformFile.FileExists(*SkillMdPath))
	{
		OutData.MarkdownFiles.Add(SkillMdPath);
	}
	else
	{
		// No SKILL.md at all — nothing usable in this folder
		UE_LOG(LogSkillsTools, Warning, TEXT("Skill folder has no SKILL.md: %s"), *OutData.SkillDir);
		return false;
	}

	// Always populate the section + resource menus so the response can advertise
	// other sub-docs and bundled resources (scripts, schemas, templates) to load next.
	OutData.AvailableSections = EnumerateSections(OutData.SkillDir);
	OutData.AvailableResources = EnumerateResources(OutData.SkillDir);

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
	TArray<TSharedPtr<FJsonValue>> PerSkillSections;
	int32 TotalTokens = 0;

	for (const FSkillData& Data : AllSkillData)
	{
		// Reported name reflects what the agent asked for: "<skill>" or "<skill>/<subdoc>".
		FString FolderName = FPaths::GetCleanFilename(Data.SkillDir);
		FString ReportedName = Data.SubDocName.IsEmpty()
			? FolderName
			: (FolderName + TEXT("/") + Data.SubDocName);
		LoadedSkillNames.Add(ReportedName);

		// Add skill separator
		if (!ConcatenatedContent.IsEmpty())
		{
			ConcatenatedContent += TEXT("\n\n========================================\n");
			ConcatenatedContent += FString::Printf(TEXT("# SKILL: %s\n"), *ReportedName);
			ConcatenatedContent += TEXT("========================================\n\n");
		}
		else
		{
			ConcatenatedContent += FString::Printf(TEXT("# SKILL: %s\n\n"), *ReportedName);
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

		// Per-skill section menu so the agent can decide to follow up with
		// `vibeue-skills-manager(action='load', skill_name='<folder>/<section>')`.
		TSharedPtr<FJsonObject> SkillSectionsObj = MakeShared<FJsonObject>();
		SkillSectionsObj->SetStringField(TEXT("skill"), FolderName);
		SkillSectionsObj->SetArrayField(TEXT("sections"), Data.AvailableSections);
		SkillSectionsObj->SetArrayField(TEXT("resources"), Data.AvailableResources);
		PerSkillSections.Add(MakeShared<FJsonValueObject>(SkillSectionsObj));
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
		     "Batch keywords with '|' to cover several topics in ONE call: "
		     "discover_python_class('unreal.BlueprintService', method_filter='variable|component|compile'). "
		     "Multiple classes per call: class_name='unreal.A, unreal.B'. "
		     "The 'content' below has workflows and gotchas."));

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
		TEXT("Call discover_python_class('unreal.ClassName', method_filter='keyword1|keyword2') to get methods — '|' ORs keywords, comma-separated class_name fetches several classes in one call"));

	TArray<TSharedPtr<FJsonValue>> UnrealClassesArray;
	for (const FString& ClassName : MergedUnrealClasses)
	{
		UnrealClassesArray.Add(MakeShared<FJsonValueString>(ClassName));
	}
	ResultObj->SetArrayField(TEXT("unreal_classes"), UnrealClassesArray);

	// Per-skill sub-doc menu so the agent can follow up with targeted
	// `skill_name="<skill>/<section>"` loads without re-listing the catalogue.
	ResultObj->SetArrayField(TEXT("available_sections"), PerSkillSections);
	ResultObj->SetStringField(TEXT("available_sections_usage"),
		TEXT("Each entry has a `skill` (folder name), `sections` (loadable .md sub-docs), and "
			 "`resources` (bundled scripts + reference files, each with a `type`). Load a sub-doc with "
			 "vibeue-skills-manager(action='load', skill_name='<skill>/<section>'); load a resource with "
			 "skill_name='<skill>/<name>' (type='script' → run via execute_python_code; "
			 "type='reference' → read-only)."));

	// Prepend instruction to use discovery tools
	FString ContentWithWarning = TEXT("## How to Use This Skill\n\n")
		TEXT("1. Call `discover_python_class('unreal.A, unreal.B', method_filter='kw1|kw2')` to find methods — batch classes (comma) and keywords ('|') into ONE call\n")
		TEXT("2. Read the COMMON_MISTAKES section above to avoid wrong method names\n")
		TEXT("3. The workflows below show patterns but USE DISCOVERED SIGNATURES for exact syntax\n")
		TEXT("4. Load sub-docs from `available_sections` only when you need deeper reference material\n\n")
		TEXT("5. WidgetService does NOT have create_widget() - use BlueprintService with 'UserWidget' parent\n\n")
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

	// Skill folder name (without any sub-doc suffix), used as the dedup/injection key
	FString ActualSkillName = FPaths::GetCleanFilename(SkillData.SkillDir);

	// Reported name — includes "<folder>/<subdoc>" when a sub-doc was loaded, so
	// the agent and dedup keying both reflect what was actually pulled in.
	FString ReportedName = SkillData.SubDocName.IsEmpty()
		? ActualSkillName
		: (ActualSkillName + TEXT("/") + SkillData.SubDocName);

	// NOTE: We no longer auto-discover methods here
	// The AI should call discover_python_class with a method_filter to get what it needs
	// This produces better results than dumping 80+ methods that the AI ignores

	// Build result JSON
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("skill_name"), ReportedName);

	if (!SkillData.SubDocName.IsEmpty())
	{
		ResultObj->SetStringField(TEXT("loaded_section"), SkillData.SubDocName);
	}

	// Instructions for AI to discover methods
	ResultObj->SetStringField(TEXT("IMPORTANT"),
		TEXT("BEFORE writing code, call discover_python_class to get method signatures. ")
		TEXT("Batch keywords with '|' to cover several topics in ONE call: ")
		TEXT("discover_python_class('unreal.BlueprintService', method_filter='variable|component|compile'). ")
		TEXT("Multiple classes per call: class_name='unreal.A, unreal.B'. ")
		TEXT("The 'content' below has workflows and gotchas."));

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
		TEXT("Call discover_python_class('unreal.ClassName', method_filter='keyword1|keyword2') to get methods — '|' ORs keywords, comma-separated class_name fetches several classes in one call"));

	TArray<TSharedPtr<FJsonValue>> UnrealClassesArray;
	for (const FString& ClassName : SkillData.UnrealClassNames)
	{
		UnrealClassesArray.Add(MakeShared<FJsonValueString>(ClassName));
	}
	ResultObj->SetArrayField(TEXT("unreal_classes"), UnrealClassesArray);

	// Sub-doc menu so the agent knows what else is loadable for this skill
	// without needing to re-list the catalogue.
	ResultObj->SetArrayField(TEXT("available_sections"), SkillData.AvailableSections);
	if (SkillData.AvailableSections.Num() > 0)
	{
		ResultObj->SetStringField(TEXT("available_sections_usage"),
			FString::Printf(TEXT("Load a sub-doc with vibeue-skills-manager(action='load', skill_name='%s/<section>'). Available sections listed above."),
				*ActualSkillName));
	}

	// Bundled resources: runnable scripts (type="script") + reference files (type="reference").
	ResultObj->SetArrayField(TEXT("available_resources"), SkillData.AvailableResources);
	if (SkillData.AvailableResources.Num() > 0)
	{
		ResultObj->SetStringField(TEXT("available_resources_usage"),
			FString::Printf(TEXT("Bundled resources for this skill. Load one with vibeue-skills-manager(action='load', skill_name='%s/<name>'). type='script' = a runnable example: edit the variables at the top, then run via execute_python_code. type='reference' = read-only reference material."),
				*ActualSkillName));
	}

	// Content LAST - workflows and gotchas only, not method signatures
	// Prepend critical instruction to content so AI CANNOT miss it
	FString ContentWithWarning = TEXT("## How to Use This Skill\n\n")
		TEXT("1. Call discover_python_class('unreal.A, unreal.B', method_filter='kw1|kw2') to find methods — batch classes (comma) and keywords ('|') into ONE call\n")
		TEXT("2. Read COMMON_MISTAKES above to avoid common errors\n")
		TEXT("3. Use the workflows below for common patterns\n")
		TEXT("4. Load a sub-doc (if listed in available_sections) when you need deeper reference material\n\n")
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

	UE_LOG(LogSkillsTools, Log, TEXT("Loaded skill '%s': %d files, ~%d tokens, %d VibeUE + %d Unreal classes, %d sub-docs available"),
		*ReportedName, FilesLoaded.Num(), TokenCount, SkillData.VibeUEClassNames.Num(), SkillData.UnrealClassNames.Num(), SkillData.AvailableSections.Num());

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
	// True when the raw action value was malformed (embedded params, escaped quotes, etc.)
	bool bWasMalformed = false;
	// Keys detected with no associated value (e.g. skill_name: with no value)
	TArray<FString> KeysWithNoValue;
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
	if (Result.Action.Contains(TEXT("\"")) || Result.Action.Contains(TEXT(":")))
	{
		Result.bWasMalformed = true;

		// Pattern 1: quoted key + quoted value — e.g. "skill_name": "landscape"
		FRegexPattern QuotedPattern(TEXT("\"(\\w+)\"\\s*:\\s*\"([^\"]+)\""));
		FRegexMatcher QuotedMatcher(QuotedPattern, Result.Action);
		while (QuotedMatcher.FindNext())
		{
			FString Key = QuotedMatcher.GetCaptureGroup(1);
			FString Value = QuotedMatcher.GetCaptureGroup(2);
			Result.RecoveredParams.Add(Key, Value);
			UE_LOG(LogTemp, Warning, TEXT("manage_skills: Recovered embedded param '%s'='%s' from malformed action string"), *Key, *Value);
		}

		// Pattern 2: unquoted key with no useful value — e.g. skill_name: (end of string or empty quotes)
		// Handles: {"action":"load\",skill_name:"} → action value = load",skill_name:
		FRegexPattern UnquotedPattern(TEXT("(\\w+):\\s*(?:\"\")?\\s*$"));
		FRegexMatcher UnquotedMatcher(UnquotedPattern, Result.Action);
		while (UnquotedMatcher.FindNext())
		{
			FString Key = UnquotedMatcher.GetCaptureGroup(1);
			if (!Result.RecoveredParams.Contains(Key))
			{
				Result.KeysWithNoValue.AddUnique(Key);
				UE_LOG(LogTemp, Warning, TEXT("manage_skills: Detected key '%s' with no value in malformed action string"), *Key);
			}
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
		UE_LOG(LogTemp, Warning, TEXT("manage_skills: Sanitized malformed action to '%s' (recovered %d embedded params, %d keys-without-value)"),
			*Result.Action, Result.RecoveredParams.Num(), Result.KeysWithNoValue.Num());
	}

	return Result;
}

// Register the vibeue-skills-manager tool (formerly manage_skills).
// Registered directly instead of via REGISTER_VIBEUE_TOOL because the hyphenated
// name (chosen for parity with the external unreal-engine-skills-manager tool)
// cannot be expressed as a C++ identifier token in that macro.
static FToolAutoRegistrar AutoRegister_vibeue_skills_manager(
	FToolRegistration{
		TEXT("vibeue-skills-manager"),
		TEXT("Discover and load VibeUE editor-automation skills (workflows, gotchas, property formats for VibeUE's editor-control tools). "
		"Skills are organized as an INDEX (SKILL.md — concise workflows + gotchas) plus optional SUB-DOCS (siblings — deeper reference material loaded on demand). "
		"Actions: "
		"'list' — return every skill with its description, classes, and a `sections` array naming each loadable sub-doc. Call this once to discover what's available without loading content. "
		"'suggest' — keyword search across skill names/descriptions/keywords. Use when you know the domain but not the skill name. "
		"'load' — load a skill's index (just `SKILL.md`) OR a specific sub-doc. Use `skill_name='<skill>'` for the index, `skill_name='<skill>/<section>'` for a sub-doc (e.g. 'state-trees/transitions'). The response includes `available_sections` so you can decide whether you need to load any sub-doc. "
		"Use `skill_names` (array) to batch-load multiple in one call with deduplicated class lists."),
		TEXT("Skills"),
		TOOL_PARAMS(
			TOOL_PARAM("action", "Action to perform: 'list', 'suggest', or 'load'", "string", true),
			TOOL_PARAM("query", "Query string to match against skill keywords (for 'suggest' action)", "string", false),
			TOOL_PARAM("skill_name", "Skill to load (for 'load' action). Accepts: directory name (e.g. 'state-trees'), `name`/`display_name` field from frontmatter, or a sub-doc path like 'state-trees/transitions' to load only that sibling .md file. Index loads (no slash) return just SKILL.md; sub-doc loads return only the requested file. Class metadata (vibeue_classes/unreal_classes) always comes from the index.", "string", false),
			TOOL_PARAM("skill_names", "Array of skills to load together (for 'load' action). Each entry follows the same syntax as `skill_name`. Use this when you need several related skills in one call.", "array", false)
		),
		[](const TMap<FString, FString>& Params) -> FString
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

				// Give a corrective message when the LLM generated malformed JSON
				// (e.g. {"action":"load\",skill_name:"}) so it self-corrects instead
				// of retrying the same broken arguments and entering an infinite loop.
				if (Sanitized.bWasMalformed && Sanitized.KeysWithNoValue.Contains(TEXT("skill_name")))
				{
					ErrorObj->SetStringField(TEXT("error"),
						TEXT("MALFORMED JSON: 'skill_name' key was detected but had no value. "
							 "Your JSON arguments were malformed (likely an escaped quote issue). "
							 "Retry with properly formatted JSON: {\"action\": \"load\", \"skill_name\": \"<name>\"}"));
				}
				else if (Sanitized.bWasMalformed)
				{
					ErrorObj->SetStringField(TEXT("error"),
						TEXT("MALFORMED JSON: vibeue-skills-manager arguments were malformed — parameters were embedded in the action value. "
							 "Retry with properly formatted JSON: {\"action\": \"load\", \"skill_name\": \"<name>\"}"));
				}
				else
				{
					ErrorObj->SetStringField(TEXT("error"), TEXT("Either 'skill_name' or 'skill_names' parameter required for 'load' action"));
				}

				FString ErrorJson;
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ErrorJson);
				FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
				return ErrorJson;
			}

			// In-editor chat: inject skill content into the system prompt instead of
			// returning it as a large tool result (which degrades over long conversations).
			// MCP callers (external tools) get the full content as before.
			FChatSession* Session = FToolRegistry::Get().GetCurrentSession();
			if (Session != nullptr)
			{
				// Filter out skills already in the system prompt (best-effort by input name)
				TArray<FString> NewSkillNames;
				TArray<FString> AlreadyLoadedNames;
				for (const FString& Name : SkillNames)
				{
					if (Session->IsSkillLoaded(Name))
					{
						AlreadyLoadedNames.Add(Name);
					}
					else
					{
						NewSkillNames.Add(Name);
					}
				}

				if (NewSkillNames.IsEmpty())
				{
					// All requested skills are already injected
					TSharedPtr<FJsonObject> ConfirmObj = MakeShared<FJsonObject>();
					ConfirmObj->SetBoolField(TEXT("success"), true);
					ConfirmObj->SetStringField(TEXT("message"), FString::Printf(
						TEXT("All requested skills are already loaded in the system prompt: %s"),
						*FString::Join(AlreadyLoadedNames, TEXT(", "))));

					TArray<TSharedPtr<FJsonValue>> InPromptArray;
					for (const FString& Name : Session->GetLoadedSkillNames())
					{
						InPromptArray.Add(MakeShared<FJsonValueString>(Name));
					}
					ConfirmObj->SetArrayField(TEXT("skills_in_system_prompt"), InPromptArray);

					FString ConfirmJson;
					TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ConfirmJson);
					FJsonSerializer::Serialize(ConfirmObj.ToSharedRef(), Writer);
					return ConfirmJson;
				}

				// Load only the skills that aren't already in the prompt
				FString FullResult = LoadSkills(NewSkillNames);

				TSharedPtr<FJsonObject> ResultObj;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FullResult);
				if (FJsonSerializer::Deserialize(Reader, ResultObj) && ResultObj.IsValid()
					&& ResultObj->GetBoolField(TEXT("success")))
				{
					// Collect actual resolved directory names from the result.
					// LoadSingleSkill uses "skill_name" (string); LoadMultipleSkills uses "skills_loaded" (array).
					TArray<FString> ActuallyLoadedNames;
					const TArray<TSharedPtr<FJsonValue>>* SkillsLoadedArray;
					if (ResultObj->TryGetArrayField(TEXT("skills_loaded"), SkillsLoadedArray))
					{
						for (const TSharedPtr<FJsonValue>& V : *SkillsLoadedArray)
						{
							ActuallyLoadedNames.Add(V->AsString());
						}
					}
					else
					{
						// Single-skill format: "skill_name" string field
						FString SingleName;
						if (ResultObj->TryGetStringField(TEXT("skill_name"), SingleName) && !SingleName.IsEmpty())
						{
							ActuallyLoadedNames.Add(SingleName);
						}
					}

					// Inject content into system prompt (persisted to disk)
					FString SkillContent = ResultObj->GetStringField(TEXT("content"));
					Session->InjectSkillIntoSystemPrompt(ActuallyLoadedNames, SkillContent);

					// Return a lightweight confirmation — full docs are now in the system prompt
					TSharedPtr<FJsonObject> ConfirmObj = MakeShared<FJsonObject>();
					ConfirmObj->SetBoolField(TEXT("success"), true);
					ConfirmObj->SetBoolField(TEXT("injected_into_system_prompt"), true);

					// skills_loaded array (normalised — works for both single and multiple skill formats)
					TArray<TSharedPtr<FJsonValue>> LoadedArr;
					for (const FString& Name : ActuallyLoadedNames)
					{
						LoadedArr.Add(MakeShared<FJsonValueString>(Name));
					}
					ConfirmObj->SetArrayField(TEXT("skills_loaded"), LoadedArr);
					ConfirmObj->SetStringField(TEXT("message"),
						TEXT("Skill documentation has been injected into the system prompt. "
							 "It will persist for this conversation and survive editor restarts. "
							 "Use discover_python_class to get exact method signatures before writing code "
							 "(batch it: class_name='unreal.A, unreal.B' and method_filter='kw1|kw2' cover several classes/topics in ONE call)."));
					ConfirmObj->SetStringField(TEXT("IMPORTANT"), ResultObj->GetStringField(TEXT("IMPORTANT")));

					// Keep COMMON_MISTAKES and class lists for immediate AI reference
					FString CommonMistakes;
					if (ResultObj->TryGetStringField(TEXT("COMMON_MISTAKES"), CommonMistakes))
					{
						ConfirmObj->SetStringField(TEXT("COMMON_MISTAKES"), CommonMistakes);
					}

					const TArray<TSharedPtr<FJsonValue>>* VibeUEClassesArray;
					if (ResultObj->TryGetArrayField(TEXT("vibeue_classes"), VibeUEClassesArray))
					{
						ConfirmObj->SetArrayField(TEXT("vibeue_classes"), *VibeUEClassesArray);
					}
					FString VibeUEClassesUsage;
					if (ResultObj->TryGetStringField(TEXT("vibeue_classes_usage"), VibeUEClassesUsage))
					{
						ConfirmObj->SetStringField(TEXT("vibeue_classes_usage"), VibeUEClassesUsage);
					}

					const TArray<TSharedPtr<FJsonValue>>* UnrealClassesArray;
					if (ResultObj->TryGetArrayField(TEXT("unreal_classes"), UnrealClassesArray))
					{
						ConfirmObj->SetArrayField(TEXT("unreal_classes"), *UnrealClassesArray);
					}

					// Pass through the sub-doc + resource menus so the in-editor chat can discover and
					// load deeper material (workflows/reference sub-docs, runnable scripts, reference files).
					for (const TCHAR* MenuField : { TEXT("available_sections"), TEXT("available_resources") })
					{
						const TArray<TSharedPtr<FJsonValue>>* MenuArray = nullptr;
						if (ResultObj->TryGetArrayField(MenuField, MenuArray) && MenuArray)
						{
							ConfirmObj->SetArrayField(MenuField, *MenuArray);
						}
					}
					for (const TCHAR* UsageField : { TEXT("available_sections_usage"), TEXT("available_resources_usage") })
					{
						FString UsageStr;
						if (ResultObj->TryGetStringField(UsageField, UsageStr))
						{
							ConfirmObj->SetStringField(UsageField, UsageStr);
						}
					}

					// For a sub-doc / script / resource load (name contains a '/'), the requested body is
					// small and specific — return it directly in the tool result so the agent can use it
					// immediately, instead of only relying on the system-prompt injection.
					bool bLoadedSubResource = false;
					for (const FString& Name : ActuallyLoadedNames)
					{
						if (Name.Contains(TEXT("/")))
						{
							bLoadedSubResource = true;
							break;
						}
					}
					if (bLoadedSubResource)
					{
						FString SubContent;
						if (ResultObj->TryGetStringField(TEXT("content"), SubContent))
						{
							ConfirmObj->SetStringField(TEXT("content"), SubContent);
						}
						const TArray<TSharedPtr<FJsonValue>>* FilesLoadedArray = nullptr;
						if (ResultObj->TryGetArrayField(TEXT("files_loaded"), FilesLoadedArray) && FilesLoadedArray)
						{
							ConfirmObj->SetArrayField(TEXT("files_loaded"), *FilesLoadedArray);
						}
					}

					// Report all skills now in the system prompt
					TArray<TSharedPtr<FJsonValue>> AllInPromptArray;
					for (const FString& Name : Session->GetLoadedSkillNames())
					{
						AllInPromptArray.Add(MakeShared<FJsonValueString>(Name));
					}
					ConfirmObj->SetArrayField(TEXT("all_skills_in_system_prompt"), AllInPromptArray);

					if (!AlreadyLoadedNames.IsEmpty())
					{
						TArray<TSharedPtr<FJsonValue>> AlreadyArray;
						for (const FString& Name : AlreadyLoadedNames)
						{
							AlreadyArray.Add(MakeShared<FJsonValueString>(Name));
						}
						ConfirmObj->SetArrayField(TEXT("already_loaded"), AlreadyArray);
					}

					FString ConfirmJson;
					TSharedRef<TJsonWriter<>> ConfirmWriter = TJsonWriterFactory<>::Create(&ConfirmJson);
					FJsonSerializer::Serialize(ConfirmObj.ToSharedRef(), ConfirmWriter);
					return ConfirmJson;
				}

				// Parse failed or skill not found — return original error
				return FullResult;
			}

			// MCP caller: return full content as before
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
	}
);
