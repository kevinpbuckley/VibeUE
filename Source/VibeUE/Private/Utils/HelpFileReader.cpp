// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Utils/HelpFileReader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/FileManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHelpFileReader, Log, All);

FString FHelpFileReader::GetHelpBasePath()
{
	// Get plugin content directory
	FString PluginDir = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("VibeUE"));
	FString HelpPath = FPaths::Combine(PluginDir, TEXT("Content"), TEXT("Help"));
	return HelpPath;
}

bool FHelpFileReader::ReadHelpFile(const FString& FilePath, FString& OutContent)
{
	if (!FPaths::FileExists(FilePath))
	{
		UE_LOG(LogHelpFileReader, Warning, TEXT("Help file not found: %s"), *FilePath);
		return false;
	}

	if (!FFileHelper::LoadFileToString(OutContent, *FilePath))
	{
		UE_LOG(LogHelpFileReader, Error, TEXT("Failed to read help file: %s"), *FilePath);
		return false;
	}

	return true;
}

TSharedPtr<FJsonObject> FHelpFileReader::CreateErrorResponse(const FString& Code, const FString& Message)
{
	TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
	Response->SetBoolField(TEXT("success"), false);
	Response->SetStringField(TEXT("error_code"), Code);
	Response->SetStringField(TEXT("error"), Message);
	return Response;
}

TSharedPtr<FJsonObject> FHelpFileReader::CreateSuccessResponse()
{
	TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
	Response->SetBoolField(TEXT("success"), true);
	return Response;
}

TSharedPtr<FJsonObject> FHelpFileReader::ParseMarkdownToJson(const FString& Content, const FString& HelpType)
{
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("help_type"), HelpType);
	
	// Include the raw markdown content - LLMs can parse markdown well
	Response->SetStringField(TEXT("content"), Content);

	// Extract title (first # line)
	TArray<FString> Lines;
	Content.ParseIntoArray(Lines, TEXT("\n"), false);
	
	for (const FString& Line : Lines)
	{
		FString TrimmedLine = Line.TrimStartAndEnd();
		if (TrimmedLine.StartsWith(TEXT("# ")))
		{
			Response->SetStringField(TEXT("title"), TrimmedLine.Mid(2).TrimStartAndEnd());
			break;
		}
	}

	// Extract sections for structured access
	TSharedPtr<FJsonObject> Sections = MakeShareable(new FJsonObject);
	FString CurrentSection;
	FString CurrentContent;

	for (const FString& Line : Lines)
	{
		FString TrimmedLine = Line.TrimStartAndEnd();
		
		if (TrimmedLine.StartsWith(TEXT("## ")))
		{
			// Save previous section
			if (!CurrentSection.IsEmpty())
			{
				Sections->SetStringField(CurrentSection.ToLower().Replace(TEXT(" "), TEXT("_")), CurrentContent.TrimStartAndEnd());
			}
			
			CurrentSection = TrimmedLine.Mid(3).TrimStartAndEnd();
			CurrentContent.Empty();
		}
		else if (!CurrentSection.IsEmpty())
		{
			CurrentContent += Line + TEXT("\n");
		}
	}
	
	// Save last section
	if (!CurrentSection.IsEmpty())
	{
		Sections->SetStringField(CurrentSection.ToLower().Replace(TEXT(" "), TEXT("_")), CurrentContent.TrimStartAndEnd());
	}

	Response->SetObjectField(TEXT("sections"), Sections);

	return Response;
}

TArray<FString> FHelpFileReader::GetAvailableHelpActions(const FString& ToolName)
{
	TArray<FString> Actions;
	FString HelpDir = FPaths::Combine(GetHelpBasePath(), ToolName);
	
	// Find all .md files in the tool's help directory (except help.md which is the overview)
	IFileManager& FileManager = IFileManager::Get();
	TArray<FString> Files;
	FileManager.FindFiles(Files, *FPaths::Combine(HelpDir, TEXT("*.md")), true, false);
	
	for (const FString& File : Files)
	{
		FString ActionName = FPaths::GetBaseFilename(File);
		if (ActionName.ToLower() != TEXT("help"))
		{
			Actions.Add(ActionName);
		}
	}
	
	return Actions;
}

TSharedPtr<FJsonObject> FHelpFileReader::GetToolHelp(const FString& ToolName)
{
	FString HelpPath = FPaths::Combine(GetHelpBasePath(), ToolName, TEXT("help.md"));
	
	FString Content;
	if (!ReadHelpFile(HelpPath, Content))
	{
		return CreateErrorResponse(TEXT("HELP_NOT_FOUND"), 
			FString::Printf(TEXT("Help file not found for tool: %s. Expected at: %s"), *ToolName, *HelpPath));
	}

	TSharedPtr<FJsonObject> Response = ParseMarkdownToJson(Content, TEXT("tool_overview"));
	Response->SetStringField(TEXT("tool"), ToolName);
	Response->SetStringField(TEXT("usage"), FString::Printf(TEXT("For action help: %s(action='help', help_action='action_name')"), *ToolName));

	// Add list of available help actions
	TArray<FString> AvailableActions = GetAvailableHelpActions(ToolName);
	TArray<TSharedPtr<FJsonValue>> ActionsArray;
	for (const FString& Action : AvailableActions)
	{
		ActionsArray.Add(MakeShareable(new FJsonValueString(Action)));
	}
	Response->SetArrayField(TEXT("available_help_actions"), ActionsArray);
	
	return Response;
}

TSharedPtr<FJsonObject> FHelpFileReader::GetActionHelp(const FString& ToolName, const FString& ActionName)
{
	FString HelpPath = FPaths::Combine(GetHelpBasePath(), ToolName, ActionName + TEXT(".md"));
	
	FString Content;
	if (!ReadHelpFile(HelpPath, Content))
	{
		return CreateErrorResponse(TEXT("ACTION_HELP_NOT_FOUND"), 
			FString::Printf(TEXT("Help file not found for action '%s' in tool '%s'. Expected at: %s"), *ActionName, *ToolName, *HelpPath));
	}

	TSharedPtr<FJsonObject> Response = ParseMarkdownToJson(Content, TEXT("action_detail"));
	Response->SetStringField(TEXT("tool"), ToolName);
	Response->SetStringField(TEXT("action"), ActionName);

	return Response;
}

TSharedPtr<FJsonObject> FHelpFileReader::HandleHelp(const FString& ToolName, const TSharedPtr<FJsonObject>& Params)
{
	FString HelpAction;
	if (Params.IsValid() && Params->TryGetStringField(TEXT("help_action"), HelpAction))
	{
		// Action-specific help requested
		return GetActionHelp(ToolName, HelpAction.ToLower());
	}

	// Tool overview help
	return GetToolHelp(ToolName);
}
