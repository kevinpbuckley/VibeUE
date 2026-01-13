// Copyright Buckley Builds LLC 2025 All Rights Reserved.

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

DEFINE_LOG_CATEGORY_STATIC(LogSkillsTools, Log, All);

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
		FString FullClassName = FString::Printf(TEXT("unreal.%s"), *ServiceName);
		auto Result = DiscoveryService->DiscoverClass(FullClassName);

		if (Result.IsError())
		{
			// Try without prefix
			Result = DiscoveryService->DiscoverClass(ServiceName);
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
 * Load all markdown files from a skill directory, run discovery for services, and return combined result
 */
static FString LoadSkill(const FString& SkillName)
{
	UE_LOG(LogSkillsTools, Log, TEXT("Loading skill: %s"), *SkillName);

	// Resolve skill name to directory
	FString SkillDir = ResolveSkillDirectory(SkillName);
	if (SkillDir.IsEmpty())
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

	// First, read skill.md to get the class lists
	FString SkillMdPath = SkillDir / TEXT("skill.md");
	TArray<FString> VibeUEClassNames;
	TArray<FString> UnrealClassNames;
	TSharedPtr<FJsonObject> SkillFrontmatter;

	if (PlatformFile.FileExists(*SkillMdPath))
	{
		FString SkillMdContent;
		if (FFileHelper::LoadFileToString(SkillMdContent, *SkillMdPath))
		{
			SkillFrontmatter = ParseYAMLFrontmatter(SkillMdContent);
			if (SkillFrontmatter.IsValid())
			{
				// Extract vibeue_classes array (primary - VibeUE services)
				const TArray<TSharedPtr<FJsonValue>>* VibeUEArray = nullptr;
				if (SkillFrontmatter->TryGetArrayField(TEXT("vibeue_classes"), VibeUEArray) && VibeUEArray)
				{
					for (const TSharedPtr<FJsonValue>& ClassValue : *VibeUEArray)
					{
						FString ClassName;
						if (ClassValue->TryGetString(ClassName))
						{
							VibeUEClassNames.Add(ClassName);
						}
					}
				}

				// Extract unreal_classes array (fallback - native UE classes)
				const TArray<TSharedPtr<FJsonValue>>* UnrealArray = nullptr;
				if (SkillFrontmatter->TryGetArrayField(TEXT("unreal_classes"), UnrealArray) && UnrealArray)
				{
					for (const TSharedPtr<FJsonValue>& ClassValue : *UnrealArray)
					{
						FString ClassName;
						if (ClassValue->TryGetString(ClassName))
						{
							UnrealClassNames.Add(ClassName);
						}
					}
				}
			}
		}
	}

	// Collect all .md files in the skill directory
	TArray<FString> MarkdownFiles;

	PlatformFile.IterateDirectoryRecursively(*SkillDir, [&](const TCHAR* FileOrDir, bool bIsDir) -> bool
	{
		if (!bIsDir)
		{
			FString FilePath = FString(FileOrDir);
			if (FilePath.EndsWith(TEXT(".md")))
			{
				MarkdownFiles.Add(FilePath);
			}
		}
		return true;
	});

	// Sort files alphabetically (skill.md will be first due to naming)
	MarkdownFiles.Sort();

	// Concatenate all files
	FString ConcatenatedContent;
	TArray<FString> FilesLoaded;

	for (const FString& FilePath : MarkdownFiles)
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
	FString ActualSkillName = FPaths::GetCleanFilename(SkillDir);

	// Run discovery for VibeUE classes first (primary APIs)
	TArray<TSharedPtr<FJsonValue>> VibeUEDiscoveryResults;
	if (VibeUEClassNames.Num() > 0)
	{
		UE_LOG(LogSkillsTools, Log, TEXT("Running discovery for %d VibeUE classes..."), VibeUEClassNames.Num());
		VibeUEDiscoveryResults = DiscoverServicesForSkill(VibeUEClassNames);
	}

	// Run discovery for Unreal classes (fallback/advanced)
	TArray<TSharedPtr<FJsonValue>> UnrealDiscoveryResults;
	if (UnrealClassNames.Num() > 0)
	{
		UE_LOG(LogSkillsTools, Log, TEXT("Running discovery for %d Unreal classes..."), UnrealClassNames.Num());
		UnrealDiscoveryResults = DiscoverServicesForSkill(UnrealClassNames);
	}

	// Build result JSON
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("skill_name"), ActualSkillName);
	ResultObj->SetStringField(TEXT("content"), ConcatenatedContent);

	TArray<TSharedPtr<FJsonValue>> FilesArray;
	for (const FString& File : FilesLoaded)
	{
		FilesArray.Add(MakeShared<FJsonValueString>(File));
	}
	ResultObj->SetArrayField(TEXT("files_loaded"), FilesArray);

	// Add VibeUE classes list (primary)
	TArray<TSharedPtr<FJsonValue>> VibeUEClassesArray;
	for (const FString& ClassName : VibeUEClassNames)
	{
		VibeUEClassesArray.Add(MakeShared<FJsonValueString>(ClassName));
	}
	ResultObj->SetArrayField(TEXT("vibeue_classes"), VibeUEClassesArray);

	// Add VibeUE discovery results (the actual API info - USE THESE FIRST)
	ResultObj->SetArrayField(TEXT("vibeue_apis"), VibeUEDiscoveryResults);

	// Add Unreal classes list (fallback)
	TArray<TSharedPtr<FJsonValue>> UnrealClassesArray;
	for (const FString& ClassName : UnrealClassNames)
	{
		UnrealClassesArray.Add(MakeShared<FJsonValueString>(ClassName));
	}
	ResultObj->SetArrayField(TEXT("unreal_classes"), UnrealClassesArray);

	// Add Unreal discovery results (fallback APIs - only use when VibeUE doesn't cover)
	ResultObj->SetArrayField(TEXT("unreal_apis"), UnrealDiscoveryResults);

	// Rough token count estimate (1 token ≈ 4 characters)
	int32 TokenCount = ConcatenatedContent.Len() / 4;
	ResultObj->SetNumberField(TEXT("token_count"), TokenCount);

	FString ResultJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultJson);
	FJsonSerializer::Serialize(ResultObj.ToSharedRef(), Writer);

	UE_LOG(LogSkillsTools, Log, TEXT("Loaded skill '%s': %d files, ~%d tokens, %d VibeUE + %d Unreal classes discovered"), 
		*ActualSkillName, FilesLoaded.Num(), TokenCount, VibeUEDiscoveryResults.Num(), UnrealDiscoveryResults.Num());

	return ResultJson;
}

// Register manage_skills tool
REGISTER_VIBEUE_TOOL(manage_skills,
	"Discover and load domain-specific knowledge skills. Use 'list' to see available skills, 'load' to load a skill by name or display_name.",
	"Skills",
	TOOL_PARAMS(
		TOOL_PARAM("action", "Action to perform: 'list' or 'load'", "string", true),
		TOOL_PARAM("skill_name", "Name of skill to load (only for 'load' action). Can be directory name, 'name' field, or 'display_name' field from skill.md", "string", false)
	),
	{
		FString Action = ExtractParamFromJson(Params, TEXT("action"));

		if (Action.Equals(TEXT("list"), ESearchCase::IgnoreCase))
		{
			return ListSkills();
		}
		else if (Action.Equals(TEXT("load"), ESearchCase::IgnoreCase))
		{
			FString SkillName = ExtractParamFromJson(Params, TEXT("skill_name"));
			if (SkillName.IsEmpty())
			{
				TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
				ErrorObj->SetBoolField(TEXT("success"), false);
				ErrorObj->SetStringField(TEXT("error"), TEXT("skill_name parameter required for 'load' action"));

				FString ErrorJson;
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ErrorJson);
				FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
				return ErrorJson;
			}

			return LoadSkill(SkillName);
		}
		else
		{
			TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
			ErrorObj->SetBoolField(TEXT("success"), false);
			ErrorObj->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown action: %s. Must be 'list' or 'load'"), *Action));

			FString ErrorJson;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ErrorJson);
			FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
			return ErrorJson;
		}
	}
);
