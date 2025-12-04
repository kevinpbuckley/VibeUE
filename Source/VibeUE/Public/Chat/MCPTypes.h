// Copyright 2025 Vibe AI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

/**
 * MCP Server configuration (from mcp.json)
 */
struct VIBEUE_API FMCPServerConfig
{
    /** Server identifier */
    FString Name;
    
    /** Transport type: "stdio" or "http" */
    FString Type;
    
    /** Command to execute (for stdio transport) */
    FString Command;
    
    /** Command arguments */
    TArray<FString> Args;
    
    /** Environment variables */
    TMap<FString, FString> Environment;
    
    /** Working directory */
    FString WorkingDirectory;
    
    /** HTTP URL (for http transport) */
    FString Url;
    
    /** Whether this server is enabled */
    bool bEnabled = true;
    
    /** Parse from JSON object */
    static FMCPServerConfig FromJson(const FString& InName, TSharedPtr<FJsonObject> JsonObj)
    {
        FMCPServerConfig Config;
        Config.Name = InName;
        
        JsonObj->TryGetStringField(TEXT("type"), Config.Type);
        JsonObj->TryGetStringField(TEXT("command"), Config.Command);
        JsonObj->TryGetStringField(TEXT("url"), Config.Url);
        JsonObj->TryGetStringField(TEXT("cwd"), Config.WorkingDirectory);
        
        // Parse args array
        const TArray<TSharedPtr<FJsonValue>>* ArgsArray;
        if (JsonObj->TryGetArrayField(TEXT("args"), ArgsArray))
        {
            for (const auto& Arg : *ArgsArray)
            {
                Config.Args.Add(Arg->AsString());
            }
        }
        
        // Parse environment variables
        const TSharedPtr<FJsonObject>* EnvObj;
        if (JsonObj->TryGetObjectField(TEXT("env"), EnvObj))
        {
            for (const auto& Pair : (*EnvObj)->Values)
            {
                Config.Environment.Add(Pair.Key, Pair.Value->AsString());
            }
        }
        
        return Config;
    }
};

/**
 * MCP Tool parameter schema
 */
struct VIBEUE_API FMCPToolParameter
{
    FString Name;
    FString Type;
    FString Description;
    bool bRequired = false;
    TSharedPtr<FJsonValue> Default;
};

/**
 * MCP Tool definition
 */
struct VIBEUE_API FMCPTool
{
    /** Tool name (unique identifier) */
    FString Name;
    
    /** Human-readable description */
    FString Description;
    
    /** Input parameters schema (JSON Schema format) */
    TSharedPtr<FJsonObject> InputSchema;
    
    /** Which server provides this tool */
    FString ServerName;
    
    /** Get list of required parameter names */
    TArray<FString> GetRequiredParameters() const
    {
        TArray<FString> Required;
        if (InputSchema.IsValid())
        {
            const TArray<TSharedPtr<FJsonValue>>* RequiredArray;
            if (InputSchema->TryGetArrayField(TEXT("required"), RequiredArray))
            {
                for (const auto& Val : *RequiredArray)
                {
                    Required.Add(Val->AsString());
                }
            }
        }
        return Required;
    }
    
    /** Parse from JSON object (tools/list response) */
    static FMCPTool FromJson(TSharedPtr<FJsonObject> JsonObj, const FString& InServerName)
    {
        FMCPTool Tool;
        Tool.ServerName = InServerName;
        
        JsonObj->TryGetStringField(TEXT("name"), Tool.Name);
        JsonObj->TryGetStringField(TEXT("description"), Tool.Description);
        
        const TSharedPtr<FJsonObject>* SchemaObj;
        if (JsonObj->TryGetObjectField(TEXT("inputSchema"), SchemaObj))
        {
            Tool.InputSchema = *SchemaObj;
        }
        
        return Tool;
    }
    
    /** Convert to JSON for OpenRouter tool definition */
    TSharedPtr<FJsonObject> ToOpenRouterJson() const
    {
        TSharedPtr<FJsonObject> ToolJson = MakeShared<FJsonObject>();
        ToolJson->SetStringField(TEXT("type"), TEXT("function"));
        
        TSharedPtr<FJsonObject> FunctionJson = MakeShared<FJsonObject>();
        FunctionJson->SetStringField(TEXT("name"), Name);
        FunctionJson->SetStringField(TEXT("description"), Description);
        
        if (InputSchema.IsValid())
        {
            FunctionJson->SetObjectField(TEXT("parameters"), InputSchema);
        }
        else
        {
            // Empty parameters object
            TSharedPtr<FJsonObject> EmptyParams = MakeShared<FJsonObject>();
            EmptyParams->SetStringField(TEXT("type"), TEXT("object"));
            EmptyParams->SetObjectField(TEXT("properties"), MakeShared<FJsonObject>());
            FunctionJson->SetObjectField(TEXT("parameters"), EmptyParams);
        }
        
        ToolJson->SetObjectField(TEXT("function"), FunctionJson);
        return ToolJson;
    }
};

/**
 * MCP Tool call request (from LLM)
 */
struct VIBEUE_API FMCPToolCall
{
    /** Unique ID for this tool call */
    FString Id;
    
    /** Tool name to invoke */
    FString ToolName;
    
    /** Arguments as JSON object */
    TSharedPtr<FJsonObject> Arguments;
    
    /** Raw arguments JSON string (for streaming accumulation) */
    FString ArgumentsJson;
    
    /** Parse from OpenRouter tool_calls array element */
    static FMCPToolCall FromOpenRouterJson(TSharedPtr<FJsonObject> JsonObj)
    {
        FMCPToolCall Call;
        
        JsonObj->TryGetStringField(TEXT("id"), Call.Id);
        
        const TSharedPtr<FJsonObject>* FunctionObj;
        if (JsonObj->TryGetObjectField(TEXT("function"), FunctionObj))
        {
            (*FunctionObj)->TryGetStringField(TEXT("name"), Call.ToolName);
            
            FString ArgumentsStr;
            if ((*FunctionObj)->TryGetStringField(TEXT("arguments"), ArgumentsStr))
            {
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ArgumentsStr);
                TSharedPtr<FJsonObject> ArgsObj;
                if (FJsonSerializer::Deserialize(Reader, ArgsObj))
                {
                    Call.Arguments = ArgsObj;
                }
            }
        }
        
        return Call;
    }
};

/**
 * MCP Tool call result
 */
struct VIBEUE_API FMCPToolResult
{
    /** Tool call ID this result is for */
    FString ToolCallId;
    
    /** Whether the call succeeded */
    bool bSuccess = false;
    
    /** Result content (can be text or structured data) */
    FString Content;
    
    /** Error message if failed */
    FString ErrorMessage;
    
    /** Convert to OpenRouter tool result message */
    TSharedPtr<FJsonObject> ToOpenRouterJson() const
    {
        TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
        ResultJson->SetStringField(TEXT("role"), TEXT("tool"));
        ResultJson->SetStringField(TEXT("tool_call_id"), ToolCallId);
        ResultJson->SetStringField(TEXT("content"), bSuccess ? Content : FString::Printf(TEXT("Error: %s"), *ErrorMessage));
        return ResultJson;
    }
};

/**
 * MCP Configuration containing all server configs
 */
struct VIBEUE_API FMCPConfiguration
{
    /** Map of server name to config */
    TMap<FString, FMCPServerConfig> Servers;
    
    /** Parse from mcp.json content */
    static FMCPConfiguration FromJsonString(const FString& JsonContent)
    {
        FMCPConfiguration Config;
        
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
        TSharedPtr<FJsonObject> RootObj;
        
        if (FJsonSerializer::Deserialize(Reader, RootObj) && RootObj.IsValid())
        {
            const TSharedPtr<FJsonObject>* ServersObj;
            if (RootObj->TryGetObjectField(TEXT("servers"), ServersObj))
            {
                for (const auto& Pair : (*ServersObj)->Values)
                {
                    const TSharedPtr<FJsonObject>* ServerObj;
                    if ((*ServersObj)->TryGetObjectField(Pair.Key, ServerObj))
                    {
                        Config.Servers.Add(Pair.Key, FMCPServerConfig::FromJson(Pair.Key, *ServerObj));
                    }
                }
            }
        }
        
        return Config;
    }
};
