// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/Result.h"
#include "Services/Common/ServiceBase.h"
#include "Services/Blueprint/Types/FunctionTypes.h"
#include "EdGraph/EdGraphPin.h"

// Forward declarations
class UBlueprint;
class UEdGraph;
struct FEdGraphPinType;

/**
 * Service for Blueprint function and parameter management
 * Extracted from BlueprintNodeCommands.cpp
 */
class VIBEUE_API FBlueprintFunctionService : public FServiceBase
{
public:
    explicit FBlueprintFunctionService(TSharedPtr<FServiceContext> Context);
    virtual ~FBlueprintFunctionService() = default;
    
    // FServiceBase interface
    virtual FString GetServiceName() const override { return TEXT("BlueprintFunctionService"); }
    
    /**
     * Create a new function in a Blueprint
     * @param Blueprint The blueprint to add the function to
     * @param FunctionName The name of the new function
     * @return Result containing the function graph on success, or error
     */
    TResult<UEdGraph*> CreateFunction(UBlueprint* Blueprint, const FString& FunctionName);
    
    /**
     * Delete a function from a Blueprint
     * @param Blueprint The blueprint containing the function
     * @param FunctionName The name of the function to delete
     * @return Success or error result
     */
    TResult<void> DeleteFunction(UBlueprint* Blueprint, const FString& FunctionName);
    
    /**
     * Get function graph GUID
     * @param Blueprint The blueprint containing the function
     * @param FunctionName The name of the function
     * @return Result containing the graph GUID string on success
     */
    TResult<FString> GetFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName);
    
    /**
     * List all functions in a Blueprint
     * @param Blueprint The blueprint to query
     * @return Result containing array of function information
     */
    TResult<TArray<FFunctionInfo>> ListFunctions(UBlueprint* Blueprint);
    
    /**
     * Add a parameter to a function
     * @param Blueprint The blueprint containing the function
     * @param FunctionName The name of the function
     * @param ParamName The name of the new parameter
     * @param ParamType The type descriptor (e.g., "int", "string", "object:Actor")
     * @param Direction The parameter direction: "input", "out", or "return"
     * @return Success or error result
     */
    TResult<void> AddParameter(UBlueprint* Blueprint, const FString& FunctionName, 
                               const FString& ParamName, const FString& ParamType, 
                               const FString& Direction);
    
    /**
     * Remove a parameter from a function
     * @param Blueprint The blueprint containing the function
     * @param FunctionName The name of the function
     * @param ParamName The name of the parameter to remove
     * @param Direction The parameter direction: "input", "out", or "return"
     * @return Success or error result
     */
    TResult<void> RemoveParameter(UBlueprint* Blueprint, const FString& FunctionName, 
                                  const FString& ParamName, const FString& Direction);
    
    /**
     * Update a parameter's type or name
     * @param Blueprint The blueprint containing the function
     * @param FunctionName The name of the function
     * @param ParamName The current name of the parameter
     * @param NewType The new type descriptor (empty to keep current)
     * @param NewName The new parameter name (empty to keep current)
     * @param Direction The parameter direction: "input", "out", or "return"
     * @return Success or error result
     */
    TResult<void> UpdateParameter(UBlueprint* Blueprint, const FString& FunctionName,
                                  const FString& ParamName, const FString& NewType,
                                  const FString& NewName, const FString& Direction);
    
    /**
     * List all parameters of a function
     * @param Blueprint The blueprint containing the function
     * @param FunctionName The name of the function
     * @return Result containing array of parameter information
     */
    TResult<TArray<FFunctionParameterInfo>> ListParameters(UBlueprint* Blueprint, const FString& FunctionName);
    
    /**
     * Add a local variable to a function
     * @param Blueprint The blueprint containing the function
     * @param FunctionName The name of the function
     * @param VarName The name of the local variable
     * @param VarType The type descriptor
     * @param DefaultValue The default value (optional)
     * @param bIsConst Whether the variable is const
     * @param bIsReference Whether the variable is a reference
     * @return Success or error result
     */
    TResult<void> AddLocalVariable(UBlueprint* Blueprint, const FString& FunctionName,
                                   const FString& VarName, const FString& VarType,
                                   const FString& DefaultValue = FString(),
                                   bool bIsConst = false, bool bIsReference = false);
    
    /**
     * Remove a local variable from a function
     * @param Blueprint The blueprint containing the function
     * @param FunctionName The name of the function
     * @param VarName The name of the local variable to remove
     * @return Success or error result
     */
    TResult<void> RemoveLocalVariable(UBlueprint* Blueprint, const FString& FunctionName,
                                      const FString& VarName);
    
    /**
     * List all local variables in a function
     * @param Blueprint The blueprint containing the function
     * @param FunctionName The name of the function
     * @return Result containing array of local variable information
     */
    TResult<TArray<FLocalVariableInfo>> ListLocalVariables(UBlueprint* Blueprint, const FString& FunctionName);

private:
    // Helper methods
    bool FindUserFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName, UEdGraph*& OutGraph) const;
    bool ParseTypeDescriptor(const FString& TypeDesc, FEdGraphPinType& OutType, FString& OutError) const;
    FString DescribePinType(const FEdGraphPinType& PinType) const;
};
