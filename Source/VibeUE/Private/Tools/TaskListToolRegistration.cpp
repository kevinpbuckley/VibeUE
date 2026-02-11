// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Core/ToolRegistry.h"
#include "Chat/ChatSession.h"
#include "Chat/ChatTypes.h"
#include "Json.h"

// TODO: Task management tool disabled - not ready for production yet
// Uncomment the registration below when ready to enable

// Helper function to extract a field from Params map (same pattern as other tool files)
static FString ExtractParamFromJson(const TMap<FString, FString>& Params, const FString& FieldName)
{
	const FString* DirectParam = Params.Find(FieldName);
	if (DirectParam)
	{
		return *DirectParam;
	}

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

	return FString();
}

/*
REGISTER_VIBEUE_INTERNAL_TOOL(manage_tasks,
	"Manage a structured task list to track progress and plan tasks throughout your session. "
	"Use this tool for complex, multi-step work requiring planning and tracking. "
	"Provide the COMPLETE array of all task items on every call. "
	"Each item needs an id (number), title (string, 3-7 words), and status "
	"(not-started, in-progress, or completed). "
	"At most ONE item may be in-progress at a time. "
	"Mark items completed IMMEDIATELY when done - do not batch completions. "
	"Skip this tool for simple, single-step tasks.",
	"Planning",
	TOOL_PARAMS(
		TOOL_PARAM("taskList",
			"Complete JSON array of all task items. Must include ALL items - both existing and new. "
			"Each item: {\"id\": number, \"title\": \"string\", \"status\": \"not-started|in-progress|completed\"}",
			"array", true)
	),
	{
		FString TaskListJson = ExtractParamFromJson(Params, TEXT("taskList"));

		UE_LOG(LogTemp, Log, TEXT("[manage_tasks] Raw taskList param: %s"), *TaskListJson);

		if (TaskListJson.IsEmpty())
		{
			return TEXT("{\"success\": false, \"error\": \"taskList parameter is empty\"}");
		}

		// Parse JSON - try as array via FJsonValue (safer than array overload)
		TSharedPtr<FJsonValue> ParsedValue;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(TaskListJson);
		if (!FJsonSerializer::Deserialize(Reader, ParsedValue) || !ParsedValue.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("[manage_tasks] Failed to parse taskList JSON"));
			return TEXT("{\"success\": false, \"error\": \"Invalid JSON for taskList\"}");
		}

		if (ParsedValue->Type != EJson::Array)
		{
			UE_LOG(LogTemp, Error, TEXT("[manage_tasks] taskList is not a JSON array (type=%d)"), (int32)ParsedValue->Type);
			return TEXT("{\"success\": false, \"error\": \"taskList must be a JSON array\"}");
		}

		const TArray<TSharedPtr<FJsonValue>>& JsonArray = ParsedValue->AsArray();

		// Build task list and validate
		TArray<FVibeUETaskItem> NewTaskList;
		int32 InProgressCount = 0;
		for (const auto& JsonValue : JsonArray)
		{
			if (!JsonValue.IsValid() || JsonValue->Type != EJson::Object)
			{
				continue;
			}
			FVibeUETaskItem Item = FVibeUETaskItem::FromJson(JsonValue->AsObject());
			if (Item.Status == EVibeUETaskStatus::InProgress)
			{
				InProgressCount++;
			}
			NewTaskList.Add(Item);
		}

		UE_LOG(LogTemp, Log, TEXT("[manage_tasks] Parsed %d task items, %d in-progress"), NewTaskList.Num(), InProgressCount);

		// Validate: at most one in-progress
		if (InProgressCount > 1)
		{
			return TEXT("{\"success\": false, \"error\": \"At most one task may be in-progress at a time\"}");
		}

		// Update session
		FChatSession* Session = FToolRegistry::Get().GetCurrentSession();
		if (Session)
		{
			Session->UpdateTaskList(NewTaskList);
			UE_LOG(LogTemp, Log, TEXT("[manage_tasks] Updated session task list"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[manage_tasks] No current session available!"));
		}

		// Build summary
		int32 Total = NewTaskList.Num();
		int32 Done = 0;
		for (const auto& Item : NewTaskList)
		{
			if (Item.Status == EVibeUETaskStatus::Completed)
			{
				Done++;
			}
		}

		return FString::Printf(
			TEXT("{\"success\": true, \"message\": \"Task list updated: %d/%d completed\"}"),
			Done, Total);
	}
);
*/
