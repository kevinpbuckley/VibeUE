// Copyright Buckley Builds LLC 2026 All Rights Reserved.

/**
 * Editor Chat Testing Tools - automation control of the VibeUE editor chat.
 *
 * Registers the manage_editor_chat tool, which lets an external MCP client
 * (e.g. Claude Code, VS Code) drive the in-editor chat for automated testing:
 * send prompts, poll status, read the transcript, reset/stop the chat, and
 * manage the chat log file.
 *
 * The tool is only exposed via MCP when Chat Editor Testing mode is enabled
 * (Settings -> General -> Chat Editor Testing, the [VibeUE] ChatEditorTesting
 * config key, or the -VibeUEChatTesting command-line switch). It is never
 * offered to the in-editor chat AI itself.
 */

#include "Core/ToolRegistry.h"
#include "Chat/ChatSession.h"
#include "Chat/AIChatCommands.h"
#include "UI/SAIChatWindow.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Json.h"

DECLARE_LOG_CATEGORY_EXTERN(LogEditorChatTesting, Log, All);
DEFINE_LOG_CATEGORY(LogEditorChatTesting);

namespace EditorChatTestingHelpers
{
    // Extract a parameter checking direct key, capitalized key, then ParamsJson blob
    FString ExtractParam(const TMap<FString, FString>& Params, const FString& FieldName)
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

    bool ExtractBoolParam(const TMap<FString, FString>& Params, const FString& FieldName, bool DefaultValue)
    {
        FString Value = ExtractParam(Params, FieldName);
        if (Value.IsEmpty())
        {
            return DefaultValue;
        }
        return Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("1"));
    }

    int32 ExtractIntParam(const TMap<FString, FString>& Params, const FString& FieldName, int32 DefaultValue)
    {
        FString Value = ExtractParam(Params, FieldName);
        if (Value.IsEmpty())
        {
            return DefaultValue;
        }
        return FCString::Atoi(*Value);
    }

    FString SerializeJson(const TSharedPtr<FJsonObject>& JsonObj)
    {
        FString JsonString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
        FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
        return JsonString;
    }

    FString BuildError(const FString& ErrorCode, const FString& ErrorMessage)
    {
        TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
        ErrorObj->SetBoolField(TEXT("success"), false);
        ErrorObj->SetStringField(TEXT("error_code"), ErrorCode);
        ErrorObj->SetStringField(TEXT("error"), ErrorMessage);
        return SerializeJson(ErrorObj);
    }

    FString BuildSuccess(const FString& Message)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetBoolField(TEXT("success"), true);
        Obj->SetStringField(TEXT("message"), Message);
        return SerializeJson(Obj);
    }

    FString GetRawLLMLogPath()
    {
        return FPaths::ProjectSavedDir() / TEXT("Logs") / TEXT("VibeUE_RawLLM.log");
    }

    void AddLogFileInfo(const TSharedPtr<FJsonObject>& Parent, const FString& FieldName, const FString& InPath)
    {
        // External clients need absolute paths
        const FString Path = FPaths::ConvertRelativePathToFull(InPath);
        TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();
        Info->SetStringField(TEXT("path"), Path);
        const bool bExists = IFileManager::Get().FileExists(*Path);
        Info->SetBoolField(TEXT("exists"), bExists);
        Info->SetNumberField(TEXT("size_bytes"), bExists ? (double)IFileManager::Get().FileSize(*Path) : 0.0);
        Parent->SetObjectField(FieldName, Info);
    }

    /** Move the chat log to Saved/Logs/VibeUE_ChatArchive/VibeUE_Chat_<timestamp>[_label].log */
    FString ArchiveChatLog(const FString& Label)
    {
        FString SourcePath = FPaths::ConvertRelativePathToFull(FChatWindowLogger::GetLogFilePath());

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        if (!IFileManager::Get().FileExists(*SourcePath))
        {
            Result->SetBoolField(TEXT("success"), true);
            Result->SetBoolField(TEXT("archived"), false);
            Result->SetStringField(TEXT("message"), TEXT("No chat log file exists yet - nothing to archive"));
            Result->SetStringField(TEXT("log_path"), SourcePath);
            return SerializeJson(Result);
        }

        FString ArchiveDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Logs") / TEXT("VibeUE_ChatArchive"));
        IFileManager::Get().MakeDirectory(*ArchiveDir, true);

        FString Suffix = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
        if (!Label.IsEmpty())
        {
            // Sanitize label for use in a filename
            FString CleanLabel = Label;
            for (TCHAR& Char : CleanLabel)
            {
                if (!FChar::IsAlnum(Char) && Char != TEXT('-') && Char != TEXT('_'))
                {
                    Char = TEXT('_');
                }
            }
            Suffix += TEXT("_") + CleanLabel;
        }

        FString DestPath = ArchiveDir / FString::Printf(TEXT("VibeUE_Chat_%s.log"), *Suffix);
        if (!IFileManager::Get().Move(*DestPath, *SourcePath))
        {
            return BuildError(TEXT("ARCHIVE_FAILED"),
                FString::Printf(TEXT("Failed to move chat log from %s to %s"), *SourcePath, *DestPath));
        }

        Result->SetBoolField(TEXT("success"), true);
        Result->SetBoolField(TEXT("archived"), true);
        Result->SetStringField(TEXT("archived_path"), DestPath);
        Result->SetStringField(TEXT("log_path"), SourcePath);
        return SerializeJson(Result);
    }

    bool IsSessionIdle(const TSharedPtr<FChatSession>& Session)
    {
        return !Session->IsRequestInProgress()
            && !Session->IsWaitingForToolApproval()
            && !Session->IsWaitingForUserToContinue()
            && !Session->IsSummarizationInProgress();
    }

    /** Open the chat as the docked tab / panel drawer (the user's main chat), not a floating popup */
    TSharedPtr<FChatSession> OpenMainChat()
    {
        TSharedPtr<FChatSession> Session = SAIChatWindow::GetChatSession();
        if (Session.IsValid())
        {
            return Session;
        }
        FGlobalTabmanager::Get()->TryInvokeTab(FAIChatCommands::AIChatTabName);
        return SAIChatWindow::GetChatSession();
    }

    FString BuildStatus()
    {
        TSharedPtr<FJsonObject> Status = MakeShared<FJsonObject>();
        Status->SetBoolField(TEXT("success"), true);
        Status->SetBoolField(TEXT("testing_enabled"), FChatSession::IsChatEditorTestingEnabled());
        AddLogFileInfo(Status, TEXT("chat_log"), FChatWindowLogger::GetLogFilePath());

        TSharedPtr<FChatSession> Session = SAIChatWindow::GetChatSession();
        // Any live chat UI counts: docked tab, panel drawer, or floating window
        Status->SetBoolField(TEXT("window_open"), Session.IsValid());
        Status->SetBoolField(TEXT("session_active"), Session.IsValid());
        Status->SetBoolField(TEXT("docked_tab_open"),
            FGlobalTabmanager::Get()->FindExistingLiveTab(FTabId(FAIChatCommands::AIChatTabName)).IsValid());
        Status->SetBoolField(TEXT("floating_window_open"), SAIChatWindow::IsWindowOpen());
        if (!Session.IsValid())
        {
            Status->SetStringField(TEXT("hint"), TEXT("Chat window is not open - use action=open_chat first"));
            return SerializeJson(Status);
        }

        Status->SetNumberField(TEXT("message_count"), Session->GetMessages().Num());
        Status->SetBoolField(TEXT("is_request_in_progress"), Session->IsRequestInProgress());
        Status->SetBoolField(TEXT("is_waiting_for_tool_approval"), Session->IsWaitingForToolApproval());
        Status->SetBoolField(TEXT("is_waiting_for_user_continue"), Session->IsWaitingForUserToContinue());
        Status->SetBoolField(TEXT("is_summarizing"), Session->IsSummarizationInProgress());
        Status->SetBoolField(TEXT("is_idle"), IsSessionIdle(Session));
        Status->SetStringField(TEXT("current_model"), Session->GetCurrentModel());
        Status->SetStringField(TEXT("provider"),
            Session->GetCurrentProvider() == ELLMProvider::OpenRouter ? TEXT("OpenRouter") : TEXT("VibeUE"));
        Status->SetBoolField(TEXT("yolo_mode"), FChatSession::IsYoloModeEnabled());
        Status->SetNumberField(TEXT("estimated_tokens"), Session->GetEstimatedTokenCount());
        Status->SetNumberField(TEXT("context_utilization_percent"), Session->GetContextUtilization() * 100.0f);

        if (Session->IsWaitingForToolApproval())
        {
            const TOptional<FMCPToolCall>& Pending = Session->GetPendingApprovalToolCall();
            if (Pending.IsSet())
            {
                TSharedPtr<FJsonObject> Approval = MakeShared<FJsonObject>();
                Approval->SetStringField(TEXT("tool_call_id"), Pending->Id);
                Approval->SetStringField(TEXT("tool_name"), Pending->ToolName);
                Status->SetObjectField(TEXT("pending_approval"), Approval);
            }
        }

        const FLLMUsageStats& Usage = Session->GetUsageStats();
        TSharedPtr<FJsonObject> UsageObj = MakeShared<FJsonObject>();
        UsageObj->SetNumberField(TEXT("request_count"), Usage.RequestCount);
        UsageObj->SetNumberField(TEXT("total_prompt_tokens"), Usage.TotalPromptTokens);
        UsageObj->SetNumberField(TEXT("total_completion_tokens"), Usage.TotalCompletionTokens);
        Status->SetObjectField(TEXT("usage"), UsageObj);

        TArray<TSharedPtr<FJsonValue>> SkillsArray;
        for (const FString& SkillName : Session->GetLoadedSkillNames())
        {
            SkillsArray.Add(MakeShared<FJsonValueString>(SkillName));
        }
        Status->SetArrayField(TEXT("loaded_skills"), SkillsArray);

        return SerializeJson(Status);
    }

    TSharedPtr<FJsonObject> MessageToJson(const FChatMessage& Message, int32 Index, int32 MaxContentChars)
    {
        TSharedPtr<FJsonObject> MsgObj = MakeShared<FJsonObject>();
        MsgObj->SetNumberField(TEXT("index"), Index);
        MsgObj->SetStringField(TEXT("role"), Message.Role);
        MsgObj->SetStringField(TEXT("timestamp"), Message.Timestamp.ToString());
        MsgObj->SetBoolField(TEXT("is_streaming"), Message.bIsStreaming);

        FString Content = Message.Content;
        if (MaxContentChars > 0 && Content.Len() > MaxContentChars)
        {
            Content = Content.Left(MaxContentChars);
            MsgObj->SetBoolField(TEXT("content_truncated"), true);
            MsgObj->SetNumberField(TEXT("content_full_length"), Message.Content.Len());
        }
        MsgObj->SetStringField(TEXT("content"), Content);

        if (!Message.ModelUsed.IsEmpty())
        {
            MsgObj->SetStringField(TEXT("model_used"), Message.ModelUsed);
        }
        if (!Message.ToolCallId.IsEmpty())
        {
            MsgObj->SetStringField(TEXT("tool_call_id"), Message.ToolCallId);
        }
        if (Message.ToolCalls.Num() > 0)
        {
            TArray<TSharedPtr<FJsonValue>> ToolCallsArray;
            for (const FChatToolCall& ToolCall : Message.ToolCalls)
            {
                TSharedPtr<FJsonObject> CallObj = MakeShared<FJsonObject>();
                CallObj->SetStringField(TEXT("id"), ToolCall.Id);
                CallObj->SetStringField(TEXT("name"), ToolCall.Name);
                FString Args = ToolCall.Arguments;
                if (MaxContentChars > 0 && Args.Len() > MaxContentChars)
                {
                    Args = Args.Left(MaxContentChars);
                    CallObj->SetBoolField(TEXT("arguments_truncated"), true);
                }
                CallObj->SetStringField(TEXT("arguments"), Args);
                ToolCallsArray.Add(MakeShared<FJsonValueObject>(CallObj));
            }
            MsgObj->SetArrayField(TEXT("tool_calls"), ToolCallsArray);
        }
        return MsgObj;
    }

    FString GetHelp()
    {
        TSharedPtr<FJsonObject> HelpObj = MakeShared<FJsonObject>();
        HelpObj->SetBoolField(TEXT("success"), true);
        HelpObj->SetStringField(TEXT("tool"), TEXT("manage_editor_chat"));
        HelpObj->SetStringField(TEXT("description"),
            TEXT("Automation control of the VibeUE editor chat for testing. Typical flow: open_chat -> send_message -> poll check_chat_status until is_idle -> get_last_response / get_messages -> archive_chat_log -> reset_chat."));

        TArray<TSharedPtr<FJsonValue>> ActionsArray;
        auto AddAction = [&ActionsArray](const FString& Name, const FString& Desc, const FString& ParamsDesc)
        {
            TSharedPtr<FJsonObject> ActionObj = MakeShared<FJsonObject>();
            ActionObj->SetStringField(TEXT("action"), Name);
            ActionObj->SetStringField(TEXT("description"), Desc);
            ActionObj->SetStringField(TEXT("parameters"), ParamsDesc);
            ActionsArray.Add(MakeShared<FJsonValueObject>(ActionObj));
        };

        AddAction(TEXT("check_chat_status"), TEXT("Get chat state: window open, busy flags, is_idle, model, pending approval, usage, log paths"), TEXT("None"));
        AddAction(TEXT("open_chat"), TEXT("Open the main chat (docked tab / panel drawer); creates the chat session"), TEXT("None"));
        AddAction(TEXT("close_chat"), TEXT("Close the chat (docked tab and/or floating window)"), TEXT("None"));
        AddAction(TEXT("send_message"), TEXT("Send a user message to the chat (async - poll check_chat_status until is_idle, then get_last_response)"), TEXT("message (required), auto_open (default true)"));
        AddAction(TEXT("stop_chat"), TEXT("Cancel the in-progress request (like clicking Stop)"), TEXT("None"));
        AddAction(TEXT("reset_chat"), TEXT("Reset the conversation (clears history and persistence)"), TEXT("archive_log (default false): archive the chat log first"));
        AddAction(TEXT("get_messages"), TEXT("Read the structured conversation transcript"), TEXT("offset (default: tail), limit (default 20, 0=all), max_content_chars (default 4000, 0=no truncation)"));
        AddAction(TEXT("get_last_response"), TEXT("Get the most recent completed assistant message"), TEXT("max_content_chars (default 0 = full)"));
        AddAction(TEXT("get_chat_log_path"), TEXT("Get chat log and raw LLM log file paths"), TEXT("None"));
        AddAction(TEXT("archive_chat_log"), TEXT("Move the chat log to Saved/Logs/VibeUE_ChatArchive with a timestamp"), TEXT("label (optional): suffix for the archived file name"));
        AddAction(TEXT("approve_tool"), TEXT("Approve the pending tool call (when YOLO mode is off)"), TEXT("tool_call_id (optional, defaults to the pending one)"));
        AddAction(TEXT("reject_tool"), TEXT("Reject the pending tool call"), TEXT("tool_call_id (optional, defaults to the pending one)"));
        AddAction(TEXT("set_yolo_mode"), TEXT("Enable/disable auto-approval of Python execution (enable for unattended test runs)"), TEXT("enabled (required)"));
        AddAction(TEXT("set_model"), TEXT("Set the chat model"), TEXT("model_id (required)"));
        AddAction(TEXT("help"), TEXT("Show this help message"), TEXT("None"));
        HelpObj->SetArrayField(TEXT("actions"), ActionsArray);

        return SerializeJson(HelpObj);
    }
}

// Manual registration (like attach_image) because the REGISTER_VIBEUE_TOOL macro
// has no way to set bEditorTestingOnly.
static FToolAutoRegistrar AutoRegister_manage_editor_chat(
    []() {
        FToolRegistration Reg;
        Reg.Name = TEXT("manage_editor_chat");
        Reg.Description = TEXT("Automate the VibeUE editor chat for testing (only available when Chat Editor Testing mode is enabled). Actions: check_chat_status, open_chat, close_chat, send_message, stop_chat, reset_chat, get_messages, get_last_response, get_chat_log_path, archive_chat_log, approve_tool, reject_tool, set_yolo_mode, set_model, help. send_message is async: poll check_chat_status until is_idle, then call get_last_response.");
        Reg.Category = TEXT("Testing");
        Reg.Parameters = TArray<FToolParameter>({
            FToolParameter(TEXT("action"), TEXT("Operation: check_chat_status, open_chat, close_chat, send_message, stop_chat, reset_chat, get_messages, get_last_response, get_chat_log_path, archive_chat_log, approve_tool, reject_tool, set_yolo_mode, set_model, help"), TEXT("string"), true),
            FToolParameter(TEXT("message"), TEXT("[send_message] The user message to send to the chat"), TEXT("string"), false),
            FToolParameter(TEXT("auto_open"), TEXT("[send_message] Open the chat window first if it is closed (default true)"), TEXT("boolean"), false),
            FToolParameter(TEXT("archive_log"), TEXT("[reset_chat] Archive the chat log before resetting (default false)"), TEXT("boolean"), false),
            FToolParameter(TEXT("label"), TEXT("[archive_chat_log] Optional suffix for the archived file name"), TEXT("string"), false),
            FToolParameter(TEXT("offset"), TEXT("[get_messages] Starting message index (default: tail of conversation)"), TEXT("number"), false),
            FToolParameter(TEXT("limit"), TEXT("[get_messages] Number of messages to return (default 20, 0=all)"), TEXT("number"), false),
            FToolParameter(TEXT("max_content_chars"), TEXT("[get_messages/get_last_response] Truncate each message content to this many characters (0=no truncation)"), TEXT("number"), false),
            FToolParameter(TEXT("tool_call_id"), TEXT("[approve_tool/reject_tool] Tool call id (defaults to the pending approval)"), TEXT("string"), false),
            FToolParameter(TEXT("enabled"), TEXT("[set_yolo_mode] true/false"), TEXT("boolean"), false),
            FToolParameter(TEXT("model_id"), TEXT("[set_model] Model id to switch to"), TEXT("string"), false)
        });
        Reg.ExecuteFunc = [](const TMap<FString, FString>& Params) -> FString {
            using namespace EditorChatTestingHelpers;

            // Defense in depth: also blocked here in case a client calls the tool by name
            // while it is hidden from the tools list
            if (!FChatSession::IsChatEditorTestingEnabled())
            {
                return BuildError(TEXT("TESTING_DISABLED"),
                    TEXT("Chat Editor Testing mode is disabled. Enable it in VibeUE Settings -> General, set [VibeUE] ChatEditorTesting=True in EditorPerProjectUserSettings.ini, or launch the editor with -VibeUEChatTesting."));
            }

            FString Action = ExtractParam(Params, TEXT("action")).ToLower();
            if (Action.IsEmpty())
            {
                return BuildError(TEXT("MISSING_ACTION"), TEXT("The 'action' parameter is required. Use action=help for documentation."));
            }

            if (Action == TEXT("help"))
            {
                return GetHelp();
            }

            if (Action == TEXT("check_chat_status") || Action == TEXT("status"))
            {
                return BuildStatus();
            }

            if (Action == TEXT("open_chat"))
            {
                if (SAIChatWindow::GetChatSession().IsValid())
                {
                    return BuildSuccess(TEXT("Chat is already open"));
                }
                return OpenMainChat().IsValid()
                    ? BuildSuccess(TEXT("Chat opened (docked tab)"))
                    : BuildError(TEXT("OPEN_FAILED"), TEXT("Failed to open the chat tab"));
            }

            if (Action == TEXT("close_chat"))
            {
                bool bClosedAny = false;
                if (TSharedPtr<SDockTab> ChatTab = FGlobalTabmanager::Get()->FindExistingLiveTab(FTabId(FAIChatCommands::AIChatTabName)))
                {
                    ChatTab->RequestCloseTab();
                    bClosedAny = true;
                }
                if (SAIChatWindow::IsWindowOpen())
                {
                    SAIChatWindow::CloseWindow();
                    bClosedAny = true;
                }
                return BuildSuccess(bClosedAny ? TEXT("Chat closed") : TEXT("Chat is already closed"));
            }

            if (Action == TEXT("get_chat_log_path"))
            {
                TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                Result->SetBoolField(TEXT("success"), true);
                AddLogFileInfo(Result, TEXT("chat_log"), FChatWindowLogger::GetLogFilePath());
                AddLogFileInfo(Result, TEXT("raw_llm_log"), GetRawLLMLogPath());
                Result->SetStringField(TEXT("archive_directory"), FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Logs") / TEXT("VibeUE_ChatArchive")));
                return SerializeJson(Result);
            }

            if (Action == TEXT("archive_chat_log"))
            {
                return ArchiveChatLog(ExtractParam(Params, TEXT("label")));
            }

            if (Action == TEXT("set_yolo_mode"))
            {
                FString EnabledStr = ExtractParam(Params, TEXT("enabled"));
                if (EnabledStr.IsEmpty())
                {
                    return BuildError(TEXT("MISSING_ENABLED"), TEXT("The 'enabled' parameter is required for set_yolo_mode."));
                }
                bool bEnabled = ExtractBoolParam(Params, TEXT("enabled"), false);
                FChatSession::SetYoloModeEnabled(bEnabled);
                return BuildSuccess(FString::Printf(TEXT("YOLO mode %s"), bEnabled ? TEXT("enabled") : TEXT("disabled")));
            }

            // All remaining actions need a live chat session
            TSharedPtr<FChatSession> Session = SAIChatWindow::GetChatSession();

            if (Action == TEXT("send_message"))
            {
                if (!Session.IsValid() && ExtractBoolParam(Params, TEXT("auto_open"), true))
                {
                    Session = OpenMainChat();
                }
                if (!Session.IsValid())
                {
                    return BuildError(TEXT("NO_SESSION"), TEXT("Chat window is not open. Use action=open_chat first or pass auto_open=true."));
                }

                FString Message = ExtractParam(Params, TEXT("message"));
                if (Message.IsEmpty())
                {
                    return BuildError(TEXT("MISSING_MESSAGE"), TEXT("The 'message' parameter is required for send_message."));
                }
                if (!IsSessionIdle(Session))
                {
                    return BuildError(TEXT("CHAT_BUSY"),
                        TEXT("The chat is busy (request in progress, pending approval, or waiting for continue). Use stop_chat, approve_tool, or wait for is_idle."));
                }

                Session->SendMessage(Message);

                TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                Result->SetBoolField(TEXT("success"), true);
                Result->SetStringField(TEXT("message"), TEXT("Message sent. Processing is asynchronous - poll check_chat_status until is_idle=true, then call get_last_response."));
                Result->SetNumberField(TEXT("message_count"), Session->GetMessages().Num());
                return SerializeJson(Result);
            }

            if (!Session.IsValid())
            {
                return BuildError(TEXT("NO_SESSION"), TEXT("Chat window is not open. Use action=open_chat first."));
            }

            if (Action == TEXT("stop_chat"))
            {
                if (!Session->IsRequestInProgress())
                {
                    return BuildSuccess(TEXT("No request in progress - nothing to stop"));
                }
                Session->CancelRequest();
                return BuildSuccess(TEXT("Request cancelled"));
            }

            if (Action == TEXT("reset_chat"))
            {
                FString ArchiveResult;
                if (ExtractBoolParam(Params, TEXT("archive_log"), false))
                {
                    ArchiveResult = ArchiveChatLog(TEXT("reset"));
                }
                if (Session->IsRequestInProgress())
                {
                    Session->CancelRequest();
                }
                Session->ResetChat();

                TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                Result->SetBoolField(TEXT("success"), true);
                Result->SetStringField(TEXT("message"), TEXT("Chat reset - history and persistence cleared"));
                if (!ArchiveResult.IsEmpty())
                {
                    Result->SetStringField(TEXT("archive_result"), ArchiveResult);
                }
                return SerializeJson(Result);
            }

            if (Action == TEXT("get_messages"))
            {
                const TArray<FChatMessage>& Messages = Session->GetMessages();
                int32 Limit = ExtractIntParam(Params, TEXT("limit"), 20);
                if (Limit <= 0 || Limit > Messages.Num())
                {
                    Limit = Messages.Num();
                }
                int32 Offset = ExtractIntParam(Params, TEXT("offset"), FMath::Max(0, Messages.Num() - Limit));
                Offset = FMath::Clamp(Offset, 0, Messages.Num());
                int32 MaxContentChars = ExtractIntParam(Params, TEXT("max_content_chars"), 4000);

                TArray<TSharedPtr<FJsonValue>> MessagesArray;
                for (int32 Index = Offset; Index < Messages.Num() && MessagesArray.Num() < Limit; ++Index)
                {
                    MessagesArray.Add(MakeShared<FJsonValueObject>(MessageToJson(Messages[Index], Index, MaxContentChars)));
                }

                TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                Result->SetBoolField(TEXT("success"), true);
                Result->SetNumberField(TEXT("total_messages"), Messages.Num());
                Result->SetNumberField(TEXT("offset"), Offset);
                Result->SetArrayField(TEXT("messages"), MessagesArray);
                return SerializeJson(Result);
            }

            if (Action == TEXT("get_last_response"))
            {
                const TArray<FChatMessage>& Messages = Session->GetMessages();
                int32 MaxContentChars = ExtractIntParam(Params, TEXT("max_content_chars"), 0);
                for (int32 Index = Messages.Num() - 1; Index >= 0; --Index)
                {
                    const FChatMessage& Message = Messages[Index];
                    if (Message.Role == TEXT("assistant") && !Message.bIsStreaming)
                    {
                        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                        Result->SetBoolField(TEXT("success"), true);
                        Result->SetBoolField(TEXT("is_idle"), IsSessionIdle(Session));
                        Result->SetObjectField(TEXT("response"), MessageToJson(Message, Index, MaxContentChars));
                        return SerializeJson(Result);
                    }
                }
                return BuildError(TEXT("NO_RESPONSE"), TEXT("No completed assistant response in the conversation yet."));
            }

            if (Action == TEXT("approve_tool") || Action == TEXT("reject_tool"))
            {
                if (!Session->IsWaitingForToolApproval())
                {
                    return BuildError(TEXT("NO_PENDING_APPROVAL"), TEXT("No tool call is waiting for approval."));
                }
                FString ToolCallId = ExtractParam(Params, TEXT("tool_call_id"));
                if (ToolCallId.IsEmpty())
                {
                    const TOptional<FMCPToolCall>& Pending = Session->GetPendingApprovalToolCall();
                    ToolCallId = Pending.IsSet() ? Pending->Id : FString();
                }
                if (Action == TEXT("approve_tool"))
                {
                    Session->ApproveToolCall(ToolCallId);
                    return BuildSuccess(FString::Printf(TEXT("Approved tool call %s"), *ToolCallId));
                }
                Session->RejectToolCall(ToolCallId);
                return BuildSuccess(FString::Printf(TEXT("Rejected tool call %s"), *ToolCallId));
            }

            if (Action == TEXT("set_model"))
            {
                FString ModelId = ExtractParam(Params, TEXT("model_id"));
                if (ModelId.IsEmpty())
                {
                    return BuildError(TEXT("MISSING_MODEL_ID"), TEXT("The 'model_id' parameter is required for set_model."));
                }
                Session->SetCurrentModel(ModelId);
                return BuildSuccess(FString::Printf(TEXT("Model set to %s"), *ModelId));
            }

            return BuildError(TEXT("UNKNOWN_ACTION"), FString::Printf(TEXT("Unknown action: %s. Use action=help for documentation."), *Action));
        };
        Reg.bEditorTestingOnly = true;
        return Reg;
    }()
);
