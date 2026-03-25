// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Tools/LogReaderService.h"
#include "Core/ToolRegistry.h"
#include "Json.h"

// Helper function to extract a field from Params
static FString ExtractLogParam(const TMap<FString, FString>& Params, const FString& FieldName)
{
	// First check if parameter exists directly
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

static FString ExtractLogParamWithDefault(const TMap<FString, FString>& Params, const FString& FieldName, const FString& DefaultValue)
{
	FString Value = ExtractLogParam(Params, FieldName);
	return Value.IsEmpty() ? DefaultValue : Value;
}

static bool ExtractLogBoolParam(const TMap<FString, FString>& Params, const FString& FieldName, bool DefaultValue)
{
	FString Value = ExtractLogParam(Params, FieldName);
	if (Value.IsEmpty())
	{
		return DefaultValue;
	}
	return Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("1"));
}

static int32 ExtractLogIntParam(const TMap<FString, FString>& Params, const FString& FieldName, int32 DefaultValue)
{
	FString Value = ExtractLogParam(Params, FieldName);
	if (Value.IsEmpty())
	{
		return DefaultValue;
	}
	return FCString::Atoi(*Value);
}

static FString BuildLogErrorResponse(const FString& ErrorCode, const FString& ErrorMessage)
{
	TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
	ErrorObj->SetBoolField(TEXT("success"), false);
	ErrorObj->SetStringField(TEXT("error_code"), ErrorCode);
	ErrorObj->SetStringField(TEXT("error"), ErrorMessage);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
	return JsonString;
}

static FString GetLogReaderHelp()
{
	TSharedPtr<FJsonObject> HelpObj = MakeShared<FJsonObject>();
	HelpObj->SetBoolField(TEXT("success"), true);
	HelpObj->SetStringField(TEXT("tool"), TEXT("read_logs"));
	HelpObj->SetStringField(TEXT("description"), TEXT("Read and filter Unreal Engine log files. Supports tailing, filtering, and paginated reading similar to PowerShell Get-Content."));

	// Actions
	TArray<TSharedPtr<FJsonValue>> ActionsArray;

	auto AddAction = [&ActionsArray](const FString& Name, const FString& Desc, const FString& ParamsDesc)
	{
		TSharedPtr<FJsonObject> ActionObj = MakeShared<FJsonObject>();
		ActionObj->SetStringField(TEXT("action"), Name);
		ActionObj->SetStringField(TEXT("description"), Desc);
		ActionObj->SetStringField(TEXT("parameters"), ParamsDesc);
		ActionsArray.Add(MakeShared<FJsonValueObject>(ActionObj));
	};

	AddAction(TEXT("list"), TEXT("List available log files"), TEXT("category (optional): Filter by category (System, Blueprint, Niagara, VibeUE)"));
	AddAction(TEXT("info"), TEXT("Get detailed information about a log file"), TEXT("file (required): File path or alias (main, chat, llm)"));
	AddAction(TEXT("read"), TEXT("Read log content with pagination"), TEXT("file (required), offset (default 0), limit (default 2000)"));
	AddAction(TEXT("tail"), TEXT("Read last N lines from log"), TEXT("file (required), lines (default 50)"));
	AddAction(TEXT("head"), TEXT("Read first N lines from log"), TEXT("file (required), lines (default 50)"));
	AddAction(TEXT("filter"), TEXT("Filter log by regex pattern"), TEXT("file (required), pattern (required), case_sensitive (default false), context_lines (default 0), max_matches (default 100)"));
	AddAction(TEXT("errors"), TEXT("Find all errors in log"), TEXT("file (required), max_matches (default 100)"));
	AddAction(TEXT("warnings"), TEXT("Find all warnings in log"), TEXT("file (required), max_matches (default 100)"));
	AddAction(TEXT("since"), TEXT("Get new content since a specific line"), TEXT("file (required), last_line (required): Last line number you read"));
	AddAction(TEXT("help"), TEXT("Show this help message"), TEXT("None"));

	HelpObj->SetArrayField(TEXT("actions"), ActionsArray);

	// File aliases
	TSharedPtr<FJsonObject> AliasesObj = MakeShared<FJsonObject>();
	AliasesObj->SetStringField(TEXT("main/system/project"), TEXT("Main Unreal Engine log (ProjectName.log)"));
	AliasesObj->SetStringField(TEXT("chat/vibeue"), TEXT("VibeUE chat session log"));
	AliasesObj->SetStringField(TEXT("llm/rawllm"), TEXT("Raw LLM API request/response log"));
	HelpObj->SetObjectField(TEXT("file_aliases"), AliasesObj);

	// Examples
	TArray<TSharedPtr<FJsonValue>> ExamplesArray;
	auto AddExample = [&ExamplesArray](const FString& Example)
	{
		ExamplesArray.Add(MakeShared<FJsonValueString>(Example));
	};

	AddExample(TEXT("action=list"));
	AddExample(TEXT("action=tail, file=main, lines=100"));
	AddExample(TEXT("action=filter, file=main, pattern=Blueprint.*Error"));
	AddExample(TEXT("action=errors, file=main"));
	AddExample(TEXT("action=read, file=chat, offset=0, limit=500"));

	HelpObj->SetArrayField(TEXT("examples"), ExamplesArray);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(HelpObj.ToSharedRef(), Writer);
	return JsonString;
}

// Register the read_logs tool
REGISTER_VIBEUE_TOOL(read_logs,
	"Read and filter Unreal Engine log files. Actions: list (browse logs), info (file details), read (paginated content), tail (last N lines), head (first N lines), filter (regex search), errors (find errors), warnings (find warnings), since (new content since line), help (documentation). File aliases: main/system (project log), chat/vibeue (chat log), llm (raw API log).",
	"Logs",
	TOOL_PARAMS(
		TOOL_PARAM("action", "Operation: list, info, read, tail, head, filter, errors, warnings, since, help", "string", true),
		TOOL_PARAM("file", "Log file path or alias (main, chat, llm). Required for most actions.", "string", false),
		TOOL_PARAM("category", "[list] Filter by category: System, Blueprint, Niagara, VibeUE", "string", false),
		TOOL_PARAM("offset", "[read] Starting line number (0-based, default 0)", "number", false),
		TOOL_PARAM("limit", "[read] Number of lines to read (default 2000, 0=unlimited)", "number", false),
		TOOL_PARAM("lines", "[tail/head] Number of lines (default 50)", "number", false),
		TOOL_PARAM("pattern", "[filter] Regex pattern to search for", "string", false),
		TOOL_PARAM("case_sensitive", "[filter] Case-sensitive matching (default false)", "boolean", false),
		TOOL_PARAM("context_lines", "[filter] Lines of context around matches (default 0)", "number", false),
		TOOL_PARAM("max_matches", "[filter/errors/warnings] Maximum matches to return (default 100)", "number", false),
		TOOL_PARAM("last_line", "[since] Last line number you read (for getting new content)", "number", false)
	),
	{
		FString Action = ExtractLogParam(Params, TEXT("action")).ToLower();

		if (Action.IsEmpty())
		{
			return BuildLogErrorResponse(TEXT("MISSING_ACTION"), TEXT("The 'action' parameter is required. Use action=help for documentation."));
		}

		// Create service with context (required by FServiceBase)
		auto ServiceContext = MakeShared<FServiceContext>();
		auto Service = MakeShared<FLogReaderService>(ServiceContext);

		// Handle each action
		if (Action == TEXT("help"))
		{
			return GetLogReaderHelp();
		}

		if (Action == TEXT("list"))
		{
			FString Category = ExtractLogParam(Params, TEXT("category"));
			TArray<FLogFileInfo> Files = Service->ListLogFiles(Category);
			return FLogReaderService::LogFileInfoArrayToJson(Files);
		}

		// All other actions require a file parameter
		FString File = ExtractLogParam(Params, TEXT("file"));
		if (File.IsEmpty())
		{
			return BuildLogErrorResponse(TEXT("MISSING_FILE"), TEXT("The 'file' parameter is required for this action. Use file aliases: main, chat, llm, or provide a path."));
		}

		if (Action == TEXT("info"))
		{
			FLogFileInfo Info = Service->GetFileInfo(File);
			if (Info.SizeBytes == 0 && Info.ModifiedTime == FDateTime::MinValue())
			{
				return BuildLogErrorResponse(TEXT("FILE_NOT_FOUND"), FString::Printf(TEXT("Log file not found: %s"), *File));
			}
			return FLogReaderService::LogFileInfoToJson(Info);
		}

		if (Action == TEXT("read"))
		{
			int32 Offset = ExtractLogIntParam(Params, TEXT("offset"), 0);
			int32 Limit = ExtractLogIntParam(Params, TEXT("limit"), 2000);
			FLogReadResult Result = Service->ReadLines(File, Offset, Limit);
			return FLogReaderService::LogReadResultToJson(Result);
		}

		if (Action == TEXT("tail"))
		{
			int32 Lines = ExtractLogIntParam(Params, TEXT("lines"), 50);
			FLogReadResult Result = Service->TailFile(File, Lines);
			return FLogReaderService::LogReadResultToJson(Result);
		}

		if (Action == TEXT("head"))
		{
			int32 Lines = ExtractLogIntParam(Params, TEXT("lines"), 50);
			FLogReadResult Result = Service->HeadFile(File, Lines);
			return FLogReaderService::LogReadResultToJson(Result);
		}

		if (Action == TEXT("filter"))
		{
			FString Pattern = ExtractLogParam(Params, TEXT("pattern"));
			if (Pattern.IsEmpty())
			{
				return BuildLogErrorResponse(TEXT("MISSING_PATTERN"), TEXT("The 'pattern' parameter is required for filter action."));
			}
			bool bCaseSensitive = ExtractLogBoolParam(Params, TEXT("case_sensitive"), false);
			int32 ContextLines = ExtractLogIntParam(Params, TEXT("context_lines"), 0);
			int32 MaxMatches = ExtractLogIntParam(Params, TEXT("max_matches"), 100);

			FLogReadResult Result = Service->FilterByPattern(File, Pattern, bCaseSensitive, ContextLines, MaxMatches);
			return FLogReaderService::LogReadResultToJson(Result);
		}

		if (Action == TEXT("errors"))
		{
			int32 MaxMatches = ExtractLogIntParam(Params, TEXT("max_matches"), 100);
			FLogReadResult Result = Service->FilterByLogLevel(File, TEXT("Error"), MaxMatches);
			return FLogReaderService::LogReadResultToJson(Result);
		}

		if (Action == TEXT("warnings"))
		{
			int32 MaxMatches = ExtractLogIntParam(Params, TEXT("max_matches"), 100);
			FLogReadResult Result = Service->FilterByLogLevel(File, TEXT("Warning"), MaxMatches);
			return FLogReaderService::LogReadResultToJson(Result);
		}

		if (Action == TEXT("since"))
		{
			FString LastLineStr = ExtractLogParam(Params, TEXT("last_line"));
			if (LastLineStr.IsEmpty())
			{
				return BuildLogErrorResponse(TEXT("MISSING_LAST_LINE"), TEXT("The 'last_line' parameter is required for 'since' action."));
			}
			int32 LastLine = FCString::Atoi(*LastLineStr);
			FLogReadResult Result = Service->GetNewContent(File, LastLine);
			return FLogReaderService::LogReadResultToJson(Result);
		}

		// Unknown action
		return BuildLogErrorResponse(TEXT("UNKNOWN_ACTION"), FString::Printf(TEXT("Unknown action: %s. Use action=help for documentation."), *Action));
	}
);
