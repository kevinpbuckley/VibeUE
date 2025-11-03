#include "Services/Blueprint/BlueprintFunctionService.h"
#include "Core/ErrorCodes.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "GraphEditorSettings.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Math/Vector4.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Math/Color.h"

// Helper functions (namespace to avoid polluting global scope)
namespace
{
    UK2Node_FunctionEntry* FindFunctionEntry(UEdGraph* Graph)
    {
        if (!Graph) return nullptr;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
            {
                return Entry;
            }
        }
        return nullptr;
    }

    UK2Node_FunctionResult* FindOrCreateResultNode(UBlueprint* Blueprint, UEdGraph* Graph)
    {
        if (!Graph) return nullptr;
        
        // Try to find existing result node
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (UK2Node_FunctionResult* Result = Cast<UK2Node_FunctionResult>(Node))
            {
                return Result;
            }
        }
        
        // Create new result node
        FGraphNodeCreator<UK2Node_FunctionResult> Creator(*Graph);
        UK2Node_FunctionResult* NewNode = Creator.CreateNode();
        Creator.Finalize();
        
        if (Blueprint)
        {
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        }
        
        return NewNode;
    }

    const UStruct* ResolveFunctionScopeStruct(UBlueprint* Blueprint, UEdGraph* FunctionGraph)
    {
        if (!Blueprint || !FunctionGraph)
        {
            return nullptr;
        }

        auto FindScope = [FunctionGraph](UClass* InClass) -> const UStruct*
        {
            if (!InClass)
            {
                return nullptr;
            }
            return InClass->FindFunctionByName(FunctionGraph->GetFName());
        };

        if (const UStruct* Scope = FindScope(Blueprint->SkeletonGeneratedClass))
        {
            return Scope;
        }
        if (const UStruct* Scope = FindScope(Blueprint->GeneratedClass))
        {
            return Scope;
        }

        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        if (const UStruct* Scope = FindScope(Blueprint->SkeletonGeneratedClass))
        {
            return Scope;
        }
        return FindScope(Blueprint->GeneratedClass);
    }
}

FBlueprintFunctionService::FBlueprintFunctionService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

TResult<UEdGraph*> FBlueprintFunctionService::CreateFunction(UBlueprint* Blueprint, const FString& FunctionName)
{
    using namespace VibeUE::ErrorCodes;
    
    auto ValidResult = ValidateNotNull(Blueprint, BLUEPRINT_NOT_FOUND, TEXT("Blueprint cannot be null"));
    if (ValidResult.IsError())
    {
        return TResult<UEdGraph*>::Error(ValidResult.GetErrorCode(), ValidResult.GetErrorMessage());
    }
    
    auto StringResult = ValidateString(FunctionName, TEXT("FunctionName"));
    if (StringResult.IsError())
    {
        return TResult<UEdGraph*>::Error(StringResult.GetErrorCode(), StringResult.GetErrorMessage());
    }
    
    // Check if function already exists
    UEdGraph* Existing = nullptr;
    if (FindUserFunctionGraph(Blueprint, FunctionName, Existing))
    {
        return TResult<UEdGraph*>::Error(FUNCTION_ALREADY_EXISTS, 
            FString::Printf(TEXT("Function '%s' already exists"), *FunctionName));
    }
    
    // Create new graph
    UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint, 
        FName(*FunctionName), 
        UEdGraph::StaticClass(), 
        UEdGraphSchema_K2::StaticClass()
    );
    
    if (!NewGraph)
    {
        return TResult<UEdGraph*>::Error(FUNCTION_CREATE_FAILED, 
            TEXT("Failed to allocate new function graph"));
    }
    
    // Add as function graph
    FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, NewGraph, true, nullptr);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    
    LogInfo(FString::Printf(TEXT("Created function '%s' with GUID %s"), *FunctionName, *NewGraph->GraphGuid.ToString()));
    
    return TResult<UEdGraph*>::Success(NewGraph);
}

TResult<void> FBlueprintFunctionService::DeleteFunction(UBlueprint* Blueprint, const FString& FunctionName)
{
    using namespace VibeUE::ErrorCodes;
    
    auto ValidResult = ValidateNotNull(Blueprint, BLUEPRINT_NOT_FOUND, TEXT("Blueprint cannot be null"));
    if (ValidResult.IsError())
    {
        return TResult<void>::Error(ValidResult.GetErrorCode(), ValidResult.GetErrorMessage());
    }
    
    UEdGraph* Graph = nullptr;
    if (!FindUserFunctionGraph(Blueprint, FunctionName, Graph))
    {
        return TResult<void>::Error(FUNCTION_NOT_FOUND, 
            FString::Printf(TEXT("Function '%s' not found"), *FunctionName));
    }
    
    FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph, EGraphRemoveFlags::Recompile);
    
    LogInfo(FString::Printf(TEXT("Deleted function '%s'"), *FunctionName));
    
    return TResult<void>::Success();
}

TResult<FString> FBlueprintFunctionService::GetFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName)
{
    using namespace VibeUE::ErrorCodes;
    
    auto ValidResult = ValidateNotNull(Blueprint, BLUEPRINT_NOT_FOUND, TEXT("Blueprint cannot be null"));
    if (ValidResult.IsError())
    {
        return TResult<FString>::Error(ValidResult.GetErrorCode(), ValidResult.GetErrorMessage());
    }
    
    UEdGraph* Graph = nullptr;
    if (!FindUserFunctionGraph(Blueprint, FunctionName, Graph))
    {
        return TResult<FString>::Error(FUNCTION_NOT_FOUND, 
            FString::Printf(TEXT("Function '%s' not found"), *FunctionName));
    }
    
    return TResult<FString>::Success(Graph->GraphGuid.ToString());
}

TResult<TArray<FFunctionInfo>> FBlueprintFunctionService::ListFunctions(UBlueprint* Blueprint)
{
    using namespace VibeUE::ErrorCodes;
    
    auto ValidResult = ValidateNotNull(Blueprint, BLUEPRINT_NOT_FOUND, TEXT("Blueprint cannot be null"));
    if (ValidResult.IsError())
    {
        return TResult<TArray<FFunctionInfo>>::Error(ValidResult.GetErrorCode(), ValidResult.GetErrorMessage());
    }
    
    TArray<FFunctionInfo> Functions;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (Graph)
        {
            FFunctionInfo Info(Graph->GetName(), Graph->GraphGuid.ToString(), Graph->Nodes.Num());
            Functions.Add(Info);
        }
    }
    
    return TResult<TArray<FFunctionInfo>>::Success(MoveTemp(Functions));
}

TResult<void> FBlueprintFunctionService::AddParameter(UBlueprint* Blueprint, const FString& FunctionName,
    const FString& ParamName, const FString& ParamType, const FString& Direction)
{
    using namespace VibeUE::ErrorCodes;
    
    auto ValidResult = ValidateNotNull(Blueprint, BLUEPRINT_NOT_FOUND, TEXT("Blueprint cannot be null"));
    if (ValidResult.IsError())
    {
        return TResult<void>::Error(ValidResult.GetErrorCode(), ValidResult.GetErrorMessage());
    }
    
    UEdGraph* FunctionGraph = nullptr;
    if (!FindUserFunctionGraph(Blueprint, FunctionName, FunctionGraph))
    {
        return TResult<void>::Error(FUNCTION_NOT_FOUND, 
            FString::Printf(TEXT("Function '%s' not found"), *FunctionName));
    }
    
    FString DirLower = Direction.ToLower();
    if (!(DirLower == TEXT("input") || DirLower == TEXT("out") || DirLower == TEXT("return")))
    {
        return TResult<void>::Error(PARAMETER_INVALID_DIRECTION, 
            TEXT("Invalid direction (expected input|out|return)"));
    }
    
    // Check if parameter already exists
    auto ListResult = ListParameters(Blueprint, FunctionName);
    if (ListResult.IsSuccess())
    {
        for (const FFunctionParameterInfo& Param : ListResult.GetValue())
        {
            if (Param.Name.Equals(ParamName, ESearchCase::IgnoreCase))
            {
                return TResult<void>::Error(PARAMETER_ALREADY_EXISTS, 
                    FString::Printf(TEXT("Parameter '%s' already exists"), *ParamName));
            }
        }
    }
    
    // Parse type
    FEdGraphPinType PinType;
    FString TypeError;
    if (!ParseTypeDescriptor(ParamType, PinType, TypeError))
    {
        return TResult<void>::Error(PARAMETER_TYPE_INVALID, TypeError);
    }
    
    UK2Node_FunctionEntry* Entry = FindFunctionEntry(FunctionGraph);
    if (!Entry)
    {
        return TResult<void>::Error(FUNCTION_ENTRY_NOT_FOUND, TEXT("Function entry node not found"));
    }
    
    if (DirLower == TEXT("input"))
    {
        UEdGraphPin* NewPin = Entry->CreateUserDefinedPin(FName(*ParamName), PinType, EGPD_Output, false);
        if (!NewPin)
        {
            return TResult<void>::Error(PARAMETER_CREATE_FAILED, TEXT("Failed to create input pin"));
        }
    }
    else // out or return
    {
        UK2Node_FunctionResult* ResultNode = FindOrCreateResultNode(Blueprint, FunctionGraph);
        if (!ResultNode)
        {
            return TResult<void>::Error(FUNCTION_RESULT_CREATE_FAILED, 
                TEXT("Failed to resolve/create result node"));
        }
        
        FName NewPinName = (DirLower == TEXT("return")) ? UEdGraphSchema_K2::PN_ReturnValue : FName(*ParamName);
        
        if (DirLower == TEXT("return"))
        {
            // Check if return already exists
            for (UEdGraphPin* P : ResultNode->Pins)
            {
                if (P->PinName == UEdGraphSchema_K2::PN_ReturnValue)
                {
                    return TResult<void>::Error(PARAMETER_ALREADY_EXISTS, TEXT("Return value already exists"));
                }
            }
        }
        
        UEdGraphPin* NewPin = ResultNode->CreateUserDefinedPin(NewPinName, PinType, EGPD_Input, false);
        if (!NewPin)
        {
            return TResult<void>::Error(PARAMETER_CREATE_FAILED, TEXT("Failed to create result pin"));
        }
    }
    
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    
    LogInfo(FString::Printf(TEXT("Added %s parameter '%s' to function '%s'"), *Direction, *ParamName, *FunctionName));
    
    return TResult<void>::Success();
}

TResult<void> FBlueprintFunctionService::RemoveParameter(UBlueprint* Blueprint, const FString& FunctionName,
    const FString& ParamName, const FString& Direction)
{
    using namespace VibeUE::ErrorCodes;
    
    auto ValidResult = ValidateNotNull(Blueprint, BLUEPRINT_NOT_FOUND, TEXT("Blueprint cannot be null"));
    if (ValidResult.IsError())
    {
        return TResult<void>::Error(ValidResult.GetErrorCode(), ValidResult.GetErrorMessage());
    }
    
    UEdGraph* FunctionGraph = nullptr;
    if (!FindUserFunctionGraph(Blueprint, FunctionName, FunctionGraph))
    {
        return TResult<void>::Error(FUNCTION_NOT_FOUND, 
            FString::Printf(TEXT("Function '%s' not found"), *FunctionName));
    }
    
    FString DirLower = Direction.ToLower();
    bool bFound = false;
    
    if (DirLower == TEXT("input"))
    {
        if (UK2Node_FunctionEntry* Entry = FindFunctionEntry(FunctionGraph))
        {
            for (int32 i = Entry->Pins.Num() - 1; i >= 0; --i)
            {
                UEdGraphPin* P = Entry->Pins[i];
                if (P->Direction == EGPD_Output && P->PinName.ToString().Equals(ParamName, ESearchCase::IgnoreCase))
                {
                    P->BreakAllPinLinks();
                    Entry->Pins.RemoveAt(i);
                    bFound = true;
                    break;
                }
            }
        }
    }
    else // out or return
    {
        for (UEdGraphNode* Node : FunctionGraph->Nodes)
        {
            if (UK2Node_FunctionResult* RNode = Cast<UK2Node_FunctionResult>(Node))
            {
                for (int32 i = RNode->Pins.Num() - 1; i >= 0; --i)
                {
                    UEdGraphPin* P = RNode->Pins[i];
                    if (P->Direction == EGPD_Input)
                    {
                        bool bNameMatch = false;
                        if (DirLower == TEXT("return"))
                        {
                            bNameMatch = (P->PinName == UEdGraphSchema_K2::PN_ReturnValue);
                        }
                        else
                        {
                            bNameMatch = P->PinName.ToString().Equals(ParamName, ESearchCase::IgnoreCase);
                        }
                        
                        if (bNameMatch)
                        {
                            P->BreakAllPinLinks();
                            RNode->Pins.RemoveAt(i);
                            bFound = true;
                            break;
                        }
                    }
                }
                if (bFound) break;
            }
        }
    }
    
    if (!bFound)
    {
        return TResult<void>::Error(PARAMETER_NOT_FOUND, 
            FString::Printf(TEXT("Parameter '%s' not found"), *ParamName));
    }
    
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    
    LogInfo(FString::Printf(TEXT("Removed %s parameter '%s' from function '%s'"), *Direction, *ParamName, *FunctionName));
    
    return TResult<void>::Success();
}

TResult<void> FBlueprintFunctionService::UpdateParameter(UBlueprint* Blueprint, const FString& FunctionName,
    const FString& ParamName, const FString& NewType, const FString& NewName, const FString& Direction)
{
    using namespace VibeUE::ErrorCodes;
    
    auto ValidResult = ValidateNotNull(Blueprint, BLUEPRINT_NOT_FOUND, TEXT("Blueprint cannot be null"));
    if (ValidResult.IsError())
    {
        return TResult<void>::Error(ValidResult.GetErrorCode(), ValidResult.GetErrorMessage());
    }
    
    UEdGraph* FunctionGraph = nullptr;
    if (!FindUserFunctionGraph(Blueprint, FunctionName, FunctionGraph))
    {
        return TResult<void>::Error(FUNCTION_NOT_FOUND, 
            FString::Printf(TEXT("Function '%s' not found"), *FunctionName));
    }
    
    FString DirLower = Direction.ToLower();
    FEdGraphPinType NewPinType;
    bool bTypeChange = false;
    
    if (!NewType.IsEmpty())
    {
        FString Err;
        if (!ParseTypeDescriptor(NewType, NewPinType, Err))
        {
            return TResult<void>::Error(PARAMETER_TYPE_INVALID, Err);
        }
        bTypeChange = true;
    }
    
    bool bModified = false;
    
    auto ApplyChanges = [&](UEdGraphPin* P)
    {
        if (bTypeChange)
        {
            P->PinType = NewPinType;
        }
        if (!NewName.IsEmpty() && P->PinName.ToString() != NewName && P->PinName != UEdGraphSchema_K2::PN_ReturnValue)
        {
            P->PinName = FName(*NewName);
        }
        bModified = true;
    };
    
    if (DirLower == TEXT("input"))
    {
        if (UK2Node_FunctionEntry* Entry = FindFunctionEntry(FunctionGraph))
        {
            for (UEdGraphPin* P : Entry->Pins)
            {
                if (P->Direction == EGPD_Output && P->PinName.ToString().Equals(ParamName, ESearchCase::IgnoreCase))
                {
                    ApplyChanges(P);
                    break;
                }
            }
        }
    }
    else
    {
        for (UEdGraphNode* Node : FunctionGraph->Nodes)
        {
            if (UK2Node_FunctionResult* RNode = Cast<UK2Node_FunctionResult>(Node))
            {
                for (UEdGraphPin* P : RNode->Pins)
                {
                    if (P->Direction == EGPD_Input)
                    {
                        bool bMatch = false;
                        if (DirLower == TEXT("return"))
                        {
                            bMatch = (P->PinName == UEdGraphSchema_K2::PN_ReturnValue);
                        }
                        else
                        {
                            bMatch = P->PinName.ToString().Equals(ParamName, ESearchCase::IgnoreCase);
                        }
                        
                        if (bMatch)
                        {
                            ApplyChanges(P);
                            break;
                        }
                    }
                }
                if (bModified) break;
            }
        }
    }
    
    if (!bModified)
    {
        return TResult<void>::Error(PARAMETER_NOT_FOUND, 
            FString::Printf(TEXT("Parameter '%s' not found"), *ParamName));
    }
    
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    
    LogInfo(FString::Printf(TEXT("Updated parameter '%s' in function '%s'"), *ParamName, *FunctionName));
    
    return TResult<void>::Success();
}

TResult<TArray<FFunctionParameterInfo>> FBlueprintFunctionService::ListParameters(UBlueprint* Blueprint, const FString& FunctionName)
{
    using namespace VibeUE::ErrorCodes;
    
    auto ValidResult = ValidateNotNull(Blueprint, BLUEPRINT_NOT_FOUND, TEXT("Blueprint cannot be null"));
    if (ValidResult.IsError())
    {
        return TResult<TArray<FFunctionParameterInfo>>::Error(ValidResult.GetErrorCode(), ValidResult.GetErrorMessage());
    }
    
    UEdGraph* FunctionGraph = nullptr;
    if (!FindUserFunctionGraph(Blueprint, FunctionName, FunctionGraph))
    {
        return TResult<TArray<FFunctionParameterInfo>>::Error(FUNCTION_NOT_FOUND, 
            FString::Printf(TEXT("Function '%s' not found"), *FunctionName));
    }
    
    TArray<FFunctionParameterInfo> Result;
    
    UK2Node_FunctionEntry* EntryNode = FindFunctionEntry(FunctionGraph);
    if (!EntryNode)
    {
        return TResult<TArray<FFunctionParameterInfo>>::Success(MoveTemp(Result));
    }
    
    // Inputs (entry node outputs)
    for (UEdGraphPin* Pin : EntryNode->Pins)
    {
        if (Pin->Direction == EGPD_Output && Pin->PinName != UEdGraphSchema_K2::PN_Then)
        {
            FString TypeStr = DescribePinType(Pin->PinType);
            Result.Add(FFunctionParameterInfo(Pin->GetFName().ToString(), TEXT("input"), TypeStr));
        }
    }
    
    // Return / out params (result node inputs)
    for (UEdGraphNode* Node : FunctionGraph->Nodes)
    {
        if (UK2Node_FunctionResult* RNode = Cast<UK2Node_FunctionResult>(Node))
        {
            for (UEdGraphPin* Pin : RNode->Pins)
            {
                if (Pin->Direction == EGPD_Input && Pin->PinName != UEdGraphSchema_K2::PN_Then)
                {
                    const bool bIsReturn = (Pin->PinName == UEdGraphSchema_K2::PN_ReturnValue);
                    FString TypeStr = DescribePinType(Pin->PinType);
                    FString Dir = bIsReturn ? TEXT("return") : TEXT("out");
                    Result.Add(FFunctionParameterInfo(Pin->GetFName().ToString(), Dir, TypeStr));
                }
            }
        }
    }
    
    return TResult<TArray<FFunctionParameterInfo>>::Success(MoveTemp(Result));
}

TResult<void> FBlueprintFunctionService::AddLocalVariable(UBlueprint* Blueprint, const FString& FunctionName,
    const FString& VarName, const FString& VarType, const FString& DefaultValue, bool bIsConst, bool bIsReference)
{
    using namespace VibeUE::ErrorCodes;
    
    auto ValidResult = ValidateNotNull(Blueprint, BLUEPRINT_NOT_FOUND, TEXT("Blueprint cannot be null"));
    if (ValidResult.IsError())
    {
        return TResult<void>::Error(ValidResult.GetErrorCode(), ValidResult.GetErrorMessage());
    }
    
    UEdGraph* FunctionGraph = nullptr;
    if (!FindUserFunctionGraph(Blueprint, FunctionName, FunctionGraph))
    {
        return TResult<void>::Error(FUNCTION_NOT_FOUND, 
            FString::Printf(TEXT("Function '%s' not found"), *FunctionName));
    }
    
    if (VarName.TrimStartAndEnd().IsEmpty())
    {
        return TResult<void>::Error(PARAM_INVALID, TEXT("Local variable name cannot be empty"));
    }
    
    UK2Node_FunctionEntry* Entry = FindFunctionEntry(FunctionGraph);
    if (!Entry)
    {
        return TResult<void>::Error(FUNCTION_ENTRY_NOT_FOUND, TEXT("Function entry node not found"));
    }
    
    // Check if variable already exists
    for (const FBPVariableDescription& Local : Entry->LocalVariables)
    {
        if (Local.VarName.ToString().Equals(VarName, ESearchCase::IgnoreCase))
        {
            return TResult<void>::Error(VARIABLE_ALREADY_EXISTS, 
                FString::Printf(TEXT("Local variable '%s' already exists"), *VarName));
        }
    }
    
    // Parse type
    FEdGraphPinType PinType;
    FString TypeError;
    if (!ParseTypeDescriptor(VarType, PinType, TypeError))
    {
        return TResult<void>::Error(VARIABLE_TYPE_INVALID, TypeError);
    }
    
    PinType.bIsReference = bIsReference;
    PinType.bIsConst = bIsConst;
    
    // Add the local variable
    if (!FBlueprintEditorUtils::AddLocalVariable(Blueprint, FunctionGraph, FName(*VarName), PinType, DefaultValue))
    {
        return TResult<void>::Error(VARIABLE_CREATE_FAILED, TEXT("Failed to add local variable"));
    }
    
    // Update properties
    Entry = FindFunctionEntry(FunctionGraph);
    if (Entry)
    {
        Entry->Modify();
        for (FBPVariableDescription& Local : Entry->LocalVariables)
        {
            if (Local.VarName.ToString().Equals(VarName, ESearchCase::IgnoreCase))
            {
                if (bIsConst)
                {
                    Local.PropertyFlags |= CPF_BlueprintReadOnly;
                    Local.VarType.bIsConst = true;
                }
                if (bIsReference)
                {
                    Local.VarType.bIsReference = true;
                }
                break;
            }
        }
    }
    
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    
    LogInfo(FString::Printf(TEXT("Added local variable '%s' to function '%s'"), *VarName, *FunctionName));
    
    return TResult<void>::Success();
}

TResult<void> FBlueprintFunctionService::RemoveLocalVariable(UBlueprint* Blueprint, const FString& FunctionName,
    const FString& VarName)
{
    using namespace VibeUE::ErrorCodes;
    
    auto ValidResult = ValidateNotNull(Blueprint, BLUEPRINT_NOT_FOUND, TEXT("Blueprint cannot be null"));
    if (ValidResult.IsError())
    {
        return TResult<void>::Error(ValidResult.GetErrorCode(), ValidResult.GetErrorMessage());
    }
    
    UEdGraph* FunctionGraph = nullptr;
    if (!FindUserFunctionGraph(Blueprint, FunctionName, FunctionGraph))
    {
        return TResult<void>::Error(FUNCTION_NOT_FOUND, 
            FString::Printf(TEXT("Function '%s' not found"), *FunctionName));
    }
    
    FName VarFName(*VarName);
    UK2Node_FunctionEntry* Entry = nullptr;
    FBPVariableDescription* Existing = FBlueprintEditorUtils::FindLocalVariable(Blueprint, FunctionGraph, VarFName, &Entry);
    
    if (!Existing || !Entry)
    {
        return TResult<void>::Error(VARIABLE_NOT_FOUND, 
            FString::Printf(TEXT("Local variable '%s' not found"), *VarName));
    }
    
    const UStruct* Scope = ResolveFunctionScopeStruct(Blueprint, FunctionGraph);
    if (Scope)
    {
        FBlueprintEditorUtils::RemoveLocalVariable(Blueprint, Scope, VarFName);
    }
    else
    {
        Entry->Modify();
        for (int32 Index = 0; Index < Entry->LocalVariables.Num(); ++Index)
        {
            if (Entry->LocalVariables[Index].VarName == VarFName)
            {
                Entry->LocalVariables.RemoveAt(Index);
                break;
            }
        }
        FBlueprintEditorUtils::RemoveVariableNodes(Blueprint, VarFName, true, FunctionGraph);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    }
    
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    
    LogInfo(FString::Printf(TEXT("Removed local variable '%s' from function '%s'"), *VarName, *FunctionName));
    
    return TResult<void>::Success();
}

TResult<TArray<FLocalVariableInfo>> FBlueprintFunctionService::ListLocalVariables(UBlueprint* Blueprint, const FString& FunctionName)
{
    using namespace VibeUE::ErrorCodes;
    
    auto ValidResult = ValidateNotNull(Blueprint, BLUEPRINT_NOT_FOUND, TEXT("Blueprint cannot be null"));
    if (ValidResult.IsError())
    {
        return TResult<TArray<FLocalVariableInfo>>::Error(ValidResult.GetErrorCode(), ValidResult.GetErrorMessage());
    }
    
    UEdGraph* FunctionGraph = nullptr;
    if (!FindUserFunctionGraph(Blueprint, FunctionName, FunctionGraph))
    {
        return TResult<TArray<FLocalVariableInfo>>::Error(FUNCTION_NOT_FOUND, 
            FString::Printf(TEXT("Function '%s' not found"), *FunctionName));
    }
    
    TArray<FLocalVariableInfo> Result;
    
    UK2Node_FunctionEntry* Entry = FindFunctionEntry(FunctionGraph);
    if (!Entry)
    {
        return TResult<TArray<FLocalVariableInfo>>::Success(MoveTemp(Result));
    }
    
    for (const FBPVariableDescription& VarDesc : Entry->LocalVariables)
    {
        FLocalVariableInfo Info;
        Info.Name = VarDesc.VarName.ToString();
        Info.FriendlyName = VarDesc.FriendlyName;
        Info.Type = DescribePinType(VarDesc.VarType);
        Info.DisplayType = UEdGraphSchema_K2::TypeToText(VarDesc.VarType).ToString();
        Info.DefaultValue = VarDesc.DefaultValue;
        Info.Category = VarDesc.Category.ToString();
        Info.PinCategory = VarDesc.VarType.PinCategory.ToString();
        Info.Guid = VarDesc.VarGuid.ToString();
        Info.bIsConst = VarDesc.VarType.bIsConst || ((VarDesc.PropertyFlags & CPF_BlueprintReadOnly) != 0);
        Info.bIsReference = VarDesc.VarType.bIsReference;
        
        Result.Add(Info);
    }
    
    return TResult<TArray<FLocalVariableInfo>>::Success(MoveTemp(Result));
}

bool FBlueprintFunctionService::FindUserFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName, UEdGraph*& OutGraph) const
{
    OutGraph = nullptr;
    if (!Blueprint)
    {
        return false;
    }
    
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (Graph && Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
        {
            OutGraph = Graph;
            return true;
        }
    }
    
    return false;
}

FString FBlueprintFunctionService::DescribePinType(const FEdGraphPinType& PinType) const
{
    auto DescribeCategory = [](const FName& Category, const FName& SubCategory, UObject* SubObject) -> FString
    {
        if (Category == UEdGraphSchema_K2::PC_Boolean) return TEXT("bool");
        if (Category == UEdGraphSchema_K2::PC_Byte)
        {
            if (SubObject)
            {
                return FString::Printf(TEXT("enum:%s"), *SubObject->GetName());
            }
            return TEXT("byte");
        }
        if (Category == UEdGraphSchema_K2::PC_Int) return TEXT("int");
        if (Category == UEdGraphSchema_K2::PC_Int64) return TEXT("int64");
        if (Category == UEdGraphSchema_K2::PC_Float) return TEXT("float");
        if (Category == UEdGraphSchema_K2::PC_Double) return TEXT("double");
        if (Category == UEdGraphSchema_K2::PC_String) return TEXT("string");
        if (Category == UEdGraphSchema_K2::PC_Name) return TEXT("name");
        if (Category == UEdGraphSchema_K2::PC_Text) return TEXT("text");
        if (Category == UEdGraphSchema_K2::PC_Struct && SubObject)
        {
            return FString::Printf(TEXT("struct:%s"), *SubObject->GetName());
        }
        if (Category == UEdGraphSchema_K2::PC_Object && SubObject)
        {
            return FString::Printf(TEXT("object:%s"), *SubObject->GetName());
        }
        if (Category == UEdGraphSchema_K2::PC_Class && SubObject)
        {
            return FString::Printf(TEXT("class:%s"), *SubObject->GetName());
        }
        return Category.ToString();
    };

    FString Base = DescribeCategory(PinType.PinCategory, PinType.PinSubCategory, PinType.PinSubCategoryObject.Get());

    if (PinType.ContainerType == EPinContainerType::Array)
    {
        return FString::Printf(TEXT("array<%s>"), *Base);
    }
    if (PinType.ContainerType == EPinContainerType::Set)
    {
        return FString::Printf(TEXT("set<%s>"), *Base);
    }
    if (PinType.ContainerType == EPinContainerType::Map)
    {
        FString ValueDesc = DescribeCategory(PinType.PinValueType.TerminalCategory, 
            PinType.PinValueType.TerminalSubCategory, PinType.PinValueType.TerminalSubCategoryObject.Get());
        return FString::Printf(TEXT("map<%s,%s>"), *Base, *ValueDesc);
    }
    
    return Base;
}

bool FBlueprintFunctionService::ParseTypeDescriptor(const FString& TypeDesc, FEdGraphPinType& OutType, FString& OutError) const
{
    FString Lower = TypeDesc.ToLower();
    OutType.ResetToDefaults();

    // Handle containers
    if (Lower.StartsWith(TEXT("array<")) && Lower.EndsWith(TEXT(">")))
    {
        FString Inner = TypeDesc.Mid(6, TypeDesc.Len() - 7);
        Inner.TrimStartAndEndInline();
        FEdGraphPinType InnerType;
        FString Err;
        if (!ParseTypeDescriptor(Inner, InnerType, Err))
        {
            OutError = Err;
            return false;
        }
        OutType = InnerType;
        OutType.ContainerType = EPinContainerType::Array;
        return true;
    }
    
    // Simple types
    if (Lower == TEXT("bool")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Boolean; return true; }
    if (Lower == TEXT("byte")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Byte; return true; }
    if (Lower == TEXT("int") || Lower == TEXT("int32")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Int; return true; }
    if (Lower == TEXT("int64")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Int64; return true; }
    if (Lower == TEXT("float")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Float; return true; }
    if (Lower == TEXT("double")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Double; return true; }
    if (Lower == TEXT("string")) { OutType.PinCategory = UEdGraphSchema_K2::PC_String; return true; }
    if (Lower == TEXT("name")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Name; return true; }
    if (Lower == TEXT("text")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Text; return true; }
    
    // Common structs
    if (Lower == TEXT("vector"))
    {
        OutType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        OutType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        return true;
    }
    if (Lower == TEXT("vector2d"))
    {
        OutType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        OutType.PinSubCategoryObject = TBaseStructure<FVector2D>::Get();
        return true;
    }
    if (Lower == TEXT("rotator"))
    {
        OutType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        OutType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
        return true;
    }
    if (Lower == TEXT("transform"))
    {
        OutType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        OutType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
        return true;
    }
    
    // Object types
    if (Lower.StartsWith(TEXT("object:")))
    {
        FString ClassName = TypeDesc.Mid(7);
        UClass* C = FindFirstObject<UClass>(*ClassName);
        if (!C)
        {
            OutError = FString::Printf(TEXT("Class '%s' not found"), *ClassName);
            return false;
        }
        OutType.PinCategory = UEdGraphSchema_K2::PC_Object;
        OutType.PinSubCategoryObject = C;
        return true;
    }
    
    OutError = FString::Printf(TEXT("Unknown type descriptor: %s"), *TypeDesc);
    return false;
}
