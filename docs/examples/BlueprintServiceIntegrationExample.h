/**
 * @file BlueprintServiceIntegrationExample.h
 * @brief Example showing how to integrate BlueprintDiscoveryService with command handlers
 * 
 * This file demonstrates the intended usage pattern for the BlueprintDiscoveryService
 * in command handler classes. This is NOT part of the active codebase but serves as
 * documentation for future migration work in Phase 2.
 * 
 * When migrating BlueprintCommands to use services, the pattern would be:
 * 
 * 1. Create service instance in command handler constructor
 * 2. Call service methods from command handlers
 * 3. Convert TResult<T> to JSON responses for MCP
 * 
 * Example usage:
 * @code
 * // In BlueprintCommandHandler constructor:
 * DiscoveryService = MakeShared<FBlueprintDiscoveryService>(Context);
 * 
 * // In HandleFindBlueprint:
 * TResult<UBlueprint*> Result = DiscoveryService->FindBlueprint(BlueprintName);
 * if (Result.IsError())
 * {
 *     return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
 * }
 * return CreateSuccessResponseWithBlueprint(Result.GetValue());
 * @endcode
 * 
 * @note This is a documentation file only and should not be included in builds
 */

#pragma once

#include "CoreMinimal.h"
#include "Services/Blueprint/BlueprintDiscoveryService.h"
#include "Commands/CommonUtils.h"
#include "Json.h"

/**
 * Example command handler using BlueprintDiscoveryService
 * This demonstrates the service integration pattern
 */
class FBlueprintCommandHandlerExample
{
public:
    FBlueprintCommandHandlerExample()
    {
        // Create service with default context
        TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
        DiscoveryService = MakeShared<FBlueprintDiscoveryService>(Context);
    }

    /**
     * Example: Handle find blueprint command
     * Demonstrates converting from TResult to JSON response
     */
    TSharedPtr<FJsonObject> HandleFindBlueprintExample(const TSharedPtr<FJsonObject>& Params)
    {
        // Extract parameter
        FString BlueprintName;
        if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
        }

        // Call service method
        TResult<UBlueprint*> Result = DiscoveryService->FindBlueprint(BlueprintName);
        
        // Convert result to JSON
        if (Result.IsError())
        {
            return FCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("%s: %s"), 
                    *Result.GetErrorCode(), 
                    *Result.GetErrorMessage())
            );
        }

        // Success - create response with blueprint data
        UBlueprint* Blueprint = Result.GetValue();
        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("name"), Blueprint->GetName());
        Data->SetStringField(TEXT("path"), Blueprint->GetPathName());
        
        return FCommonUtils::CreateSuccessResponse(Data);
    }

    /**
     * Example: Handle search blueprints command
     * Demonstrates converting TArray results to JSON
     */
    TSharedPtr<FJsonObject> HandleSearchBlueprintsExample(const TSharedPtr<FJsonObject>& Params)
    {
        FString SearchTerm;
        if (!Params->TryGetStringField(TEXT("search_term"), SearchTerm))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'search_term' parameter"));
        }

        int32 MaxResults = 100;
        Params->TryGetNumberField(TEXT("max_results"), MaxResults);

        // Call service method
        TResult<TArray<FBlueprintInfo>> Result = DiscoveryService->SearchBlueprints(SearchTerm, MaxResults);
        
        if (Result.IsError())
        {
            return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
        }

        // Convert array of FBlueprintInfo to JSON array
        TArray<TSharedPtr<FJsonValue>> JsonArray;
        for (const FBlueprintInfo& Info : Result.GetValue())
        {
            TSharedPtr<FJsonObject> InfoObj = MakeShared<FJsonObject>();
            InfoObj->SetStringField(TEXT("name"), Info.Name);
            InfoObj->SetStringField(TEXT("path"), Info.Path);
            InfoObj->SetStringField(TEXT("parent_class"), Info.ParentClass);
            InfoObj->SetBoolField(TEXT("is_widget"), Info.bIsWidgetBlueprint);
            
            JsonArray.Add(MakeShared<FJsonValueObject>(InfoObj));
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetArrayField(TEXT("blueprints"), JsonArray);
        Data->SetNumberField(TEXT("count"), JsonArray.Num());
        
        return FCommonUtils::CreateSuccessResponse(Data);
    }

    /**
     * Example: Handle blueprint exists check
     * Demonstrates simple bool result handling
     */
    TSharedPtr<FJsonObject> HandleBlueprintExistsExample(const TSharedPtr<FJsonObject>& Params)
    {
        FString BlueprintName;
        if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
        }

        TResult<bool> Result = DiscoveryService->BlueprintExists(BlueprintName);
        
        if (Result.IsError())
        {
            return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetBoolField(TEXT("exists"), Result.GetValue());
        Data->SetStringField(TEXT("blueprint_name"), BlueprintName);
        
        return FCommonUtils::CreateSuccessResponse(Data);
    }

private:
    TSharedPtr<FBlueprintDiscoveryService> DiscoveryService;
};
