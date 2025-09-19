#include "Commands/VibeUEBlueprintNodeCommands.h"
#include "Commands/VibeUEBlueprintReflection.h"
#include "Commands/VibeUECommonUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
// #include "K2Node_ForEachLoop.h"  // Commented out - header not found
#include "K2Node_Timeline.h"
#include "K2Node_MacroInstance.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "GameFramework/InputSettings.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "EdGraphSchema_K2.h"

// Declare the log category
DEFINE_LOG_CATEGORY_STATIC(LogVibeUE, Log, All);

FVibeUEBlueprintNodeCommands::FVibeUEBlueprintNodeCommands()
{
    // Initialize reflection system
    ReflectionCommands = MakeShareable(new FVibeUEBlueprintReflectionCommands());
}

TSharedPtr<FJsonObject> FVibeUEBlueprintNodeCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogVibeUE, Warning, TEXT("MCP: BlueprintNodeCommands::HandleCommand called with CommandType: %s"), *CommandType);
    
    if (CommandType == TEXT("connect_blueprint_nodes"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleConnectBlueprintNodes"));
        return HandleConnectBlueprintNodes(Params);
    }
    else if (CommandType == TEXT("add_blueprint_get_self_component_reference"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleAddBlueprintGetSelfComponentReference"));
        return HandleAddBlueprintGetSelfComponentReference(Params);
    }
    else if (CommandType == TEXT("add_blueprint_event_node"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleAddBlueprintEvent"));
        return HandleAddBlueprintEvent(Params);
    }
    else if (CommandType == TEXT("add_blueprint_function_node"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleAddBlueprintFunctionCall"));
        return HandleAddBlueprintFunctionCall(Params);
    }
    else if (CommandType == TEXT("add_blueprint_variable"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleAddBlueprintVariable"));
        return HandleAddBlueprintVariable(Params);
    }
    else if (CommandType == TEXT("add_blueprint_input_action_node"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleAddBlueprintInputActionNode"));
        return HandleAddBlueprintInputActionNode(Params);
    }
    else if (CommandType == TEXT("add_blueprint_self_reference"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleAddBlueprintSelfReference"));
        return HandleAddBlueprintSelfReference(Params);
    }
    else if (CommandType == TEXT("find_blueprint_nodes"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleFindBlueprintNodes"));
        return HandleFindBlueprintNodes(Params);
    }
    else if (CommandType == TEXT("list_event_graph_nodes"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleListEventGraphNodes"));
        return HandleListEventGraphNodes(Params);
    }
    else if (CommandType == TEXT("get_node_details"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleGetNodeDetails"));
        return HandleGetNodeDetails(Params);
    }
    else if (CommandType == TEXT("list_blueprint_functions"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleListBlueprintFunctions"));
        return HandleListBlueprintFunctions(Params);
    }
    else if (CommandType == TEXT("list_custom_events"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleListCustomEvents"));
        return HandleListCustomEvents(Params);
    }
    // NEW: Reflection-based commands
    else if (CommandType == TEXT("get_available_blueprint_nodes"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleGetAvailableBlueprintNodes (Reflection)"));
        return HandleGetAvailableBlueprintNodes(Params);
    }
    else if (CommandType == TEXT("add_blueprint_node"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleAddBlueprintNode (Reflection)"));
        return HandleAddBlueprintNode(Params);
    }
    else if (CommandType == TEXT("set_blueprint_node_property"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleSetBlueprintNodeProperty (Reflection)"));
        return HandleSetBlueprintNodeProperty(Params);
    }
    else if (CommandType == TEXT("get_blueprint_node_property"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleGetBlueprintNodeProperty (Reflection)"));
        return HandleGetBlueprintNodeProperty(Params);
    }
    
    UE_LOG(LogVibeUE, Error, TEXT("MCP: Unknown blueprint node command: %s"), *CommandType);
    return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown blueprint node command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FVibeUEBlueprintNodeCommands::HandleConnectBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString SourceNodeId;
    if (!Params->TryGetStringField(TEXT("source_node_id"), SourceNodeId))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'source_node_id' parameter"));
    }

    FString TargetNodeId;
    if (!Params->TryGetStringField(TEXT("target_node_id"), TargetNodeId))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'target_node_id' parameter"));
    }

    FString SourcePinName;
    if (!Params->TryGetStringField(TEXT("source_pin"), SourcePinName))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'source_pin' parameter"));
    }

    FString TargetPinName;
    if (!Params->TryGetStringField(TEXT("target_pin"), TargetPinName))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'target_pin' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FVibeUECommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FVibeUECommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Find the nodes
    UEdGraphNode* SourceNode = nullptr;
    UEdGraphNode* TargetNode = nullptr;
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        if (Node->NodeGuid.ToString() == SourceNodeId)
        {
            SourceNode = Node;
        }
        else if (Node->NodeGuid.ToString() == TargetNodeId)
        {
            TargetNode = Node;
        }
    }

    if (!SourceNode || !TargetNode)
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Source or target node not found"));
    }

    // Enhanced connection with reflection-based pin discovery
    TSharedPtr<FJsonObject> ConnectionResult = FVibeUECommonUtils::ConnectGraphNodesWithReflection(
        EventGraph, SourceNode, SourcePinName, TargetNode, TargetPinName);
    
    bool bSuccess = false;
    ConnectionResult->TryGetBoolField(TEXT("success"), bSuccess);
    
    if (bSuccess)
    {
        // Mark the blueprint as modified
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        
        // Return the enhanced connection result with detailed information
        return ConnectionResult;
    }
    else
    {
        // Return detailed error with suggestions
        return ConnectionResult;
    }
}

TSharedPtr<FJsonObject> FVibeUEBlueprintNodeCommands::HandleAddBlueprintGetSelfComponentReference(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FVibeUECommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FVibeUECommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FVibeUECommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }
    
    // We'll skip component verification since the GetAllNodes API may have changed in UE5.5
    
    // Create the variable get node directly
    UK2Node_VariableGet* GetComponentNode = NewObject<UK2Node_VariableGet>(EventGraph);
    if (!GetComponentNode)
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create get component node"));
    }
    
    // Set up the variable reference properly for UE5.5
    FMemberReference& VarRef = GetComponentNode->VariableReference;
    VarRef.SetSelfMember(FName(*ComponentName));
    
    // Set node position
    GetComponentNode->NodePosX = NodePosition.X;
    GetComponentNode->NodePosY = NodePosition.Y;
    
    // Add to graph
    EventGraph->AddNode(GetComponentNode);
    GetComponentNode->CreateNewGuid();
    GetComponentNode->PostPlacedNewNode();
    GetComponentNode->AllocateDefaultPins();
    
    // Explicitly reconstruct node for UE5.5
    GetComponentNode->ReconstructNode();
    
    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), GetComponentNode->NodeGuid.ToString());
    return ResultObj;
}

TSharedPtr<FJsonObject> FVibeUEBlueprintNodeCommands::HandleAddBlueprintEvent(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString EventName;
    if (!Params->TryGetStringField(TEXT("event_name"), EventName))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'event_name' parameter"));
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FVibeUECommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FVibeUECommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FVibeUECommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Create the event node
    UK2Node_Event* EventNode = FVibeUECommonUtils::CreateEventNode(EventGraph, EventName, NodePosition);
    if (!EventNode)
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create event node"));
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), EventNode->NodeGuid.ToString());
    return ResultObj;
}

TSharedPtr<FJsonObject> FVibeUEBlueprintNodeCommands::HandleAddBlueprintFunctionCall(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FVibeUECommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Check for target parameter (optional)
    FString Target;
    Params->TryGetStringField(TEXT("target"), Target);

    // Find the blueprint
    UBlueprint* Blueprint = FVibeUECommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FVibeUECommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Find the function
    UFunction* Function = nullptr;
    UK2Node_CallFunction* FunctionNode = nullptr;
    
    // Add extensive logging for debugging
    UE_LOG(LogTemp, Display, TEXT("Looking for function '%s' in target '%s'"), 
           *FunctionName, Target.IsEmpty() ? TEXT("Blueprint") : *Target);
    
    // Check if we have a target class specified
    if (!Target.IsEmpty())
    {
        // Try to find the target class
        UClass* TargetClass = nullptr;
        
        // First try without a prefix
        TargetClass = FindFirstObject<UClass>(*Target, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("VibeUEBlueprintNodeCommands"));
        UE_LOG(LogTemp, Display, TEXT("Tried to find class '%s': %s"), 
               *Target, TargetClass ? TEXT("Found") : TEXT("Not found"));
        
        // If not found, try with U prefix (common convention for UE classes)
        if (!TargetClass && !Target.StartsWith(TEXT("U")))
        {
            FString TargetWithPrefix = FString(TEXT("U")) + Target;
            TargetClass = FindFirstObject<UClass>(*TargetWithPrefix, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("VibeUEBlueprintNodeCommands"));
            UE_LOG(LogTemp, Display, TEXT("Tried to find class '%s': %s"), 
                   *TargetWithPrefix, TargetClass ? TEXT("Found") : TEXT("Not found"));
        }
        
        // If still not found, try with common component names
        if (!TargetClass)
        {
            // Try some common component class names
            TArray<FString> PossibleClassNames;
            PossibleClassNames.Add(FString(TEXT("U")) + Target + TEXT("Component"));
            PossibleClassNames.Add(Target + TEXT("Component"));
            
            for (const FString& ClassName : PossibleClassNames)
            {
                TargetClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("VibeUEBlueprintNodeCommands"));
                if (TargetClass)
                {
                    UE_LOG(LogTemp, Display, TEXT("Found class using alternative name '%s'"), *ClassName);
                    break;
                }
            }
        }
        
        // Special case handling for common classes like UGameplayStatics
        if (!TargetClass && Target == TEXT("UGameplayStatics"))
        {
            // For UGameplayStatics, use a direct reference to known class
            TargetClass = FindFirstObject<UClass>(TEXT("UGameplayStatics"), EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("VibeUEBlueprintNodeCommands"));
            if (!TargetClass)
            {
                // Try loading it from its known package
                TargetClass = LoadObject<UClass>(nullptr, TEXT("/Script/Engine.GameplayStatics"));
                UE_LOG(LogTemp, Display, TEXT("Explicitly loading GameplayStatics: %s"), 
                       TargetClass ? TEXT("Success") : TEXT("Failed"));
            }
        }
        
        // If we found a target class, look for the function there
        if (TargetClass)
        {
            UE_LOG(LogTemp, Display, TEXT("Looking for function '%s' in class '%s'"), 
                   *FunctionName, *TargetClass->GetName());
                   
            // First try exact name
            Function = TargetClass->FindFunctionByName(*FunctionName);
            
            // If not found, try class hierarchy
            UClass* CurrentClass = TargetClass;
            while (!Function && CurrentClass)
            {
                UE_LOG(LogTemp, Display, TEXT("Searching in class: %s"), *CurrentClass->GetName());
                
                // Try exact match
                Function = CurrentClass->FindFunctionByName(*FunctionName);
                
                // Try case-insensitive match
                if (!Function)
                {
                    for (TFieldIterator<UFunction> FuncIt(CurrentClass); FuncIt; ++FuncIt)
                    {
                        UFunction* AvailableFunc = *FuncIt;
                        UE_LOG(LogTemp, Display, TEXT("  - Available function: %s"), *AvailableFunc->GetName());
                        
                        if (AvailableFunc->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
                        {
                            UE_LOG(LogTemp, Display, TEXT("  - Found case-insensitive match: %s"), *AvailableFunc->GetName());
                            Function = AvailableFunc;
                            break;
                        }
                    }
                }
                
                // Move to parent class
                CurrentClass = CurrentClass->GetSuperClass();
            }
            
            // Special handling for known functions
            if (!Function)
            {
                if (TargetClass->GetName() == TEXT("GameplayStatics") && 
                    (FunctionName == TEXT("GetActorOfClass") || FunctionName.Equals(TEXT("GetActorOfClass"), ESearchCase::IgnoreCase)))
                {
                    UE_LOG(LogTemp, Display, TEXT("Using special case handling for GameplayStatics::GetActorOfClass"));
                    
                    // Create the function node directly
                    FunctionNode = NewObject<UK2Node_CallFunction>(EventGraph);
                    if (FunctionNode)
                    {
                        // Direct setup for known function
                        FunctionNode->FunctionReference.SetExternalMember(
                            FName(TEXT("GetActorOfClass")), 
                            TargetClass
                        );
                        
                        FunctionNode->NodePosX = NodePosition.X;
                        FunctionNode->NodePosY = NodePosition.Y;
                        EventGraph->AddNode(FunctionNode);
                        FunctionNode->CreateNewGuid();
                        FunctionNode->PostPlacedNewNode();
                        FunctionNode->AllocateDefaultPins();
                        
                        UE_LOG(LogTemp, Display, TEXT("Created GetActorOfClass node directly"));
                        
                        // List all pins
                        for (UEdGraphPin* Pin : FunctionNode->Pins)
                        {
                            UE_LOG(LogTemp, Display, TEXT("  - Pin: %s, Direction: %d, Category: %s"), 
                                   *Pin->PinName.ToString(), (int32)Pin->Direction, *Pin->PinType.PinCategory.ToString());
                        }
                    }
                }
            }
        }
    }
    
    // If we still haven't found the function, try in the blueprint's class
    if (!Function && !FunctionNode)
    {
        UE_LOG(LogTemp, Display, TEXT("Trying to find function in blueprint class"));
        Function = Blueprint->GeneratedClass->FindFunctionByName(*FunctionName);
    }
    
    // Create the function call node if we found the function
    if (Function && !FunctionNode)
    {
        FunctionNode = FVibeUECommonUtils::CreateFunctionCallNode(EventGraph, Function, NodePosition);
    }
    
    if (!FunctionNode)
    {
        return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Function not found: %s in target %s"), *FunctionName, Target.IsEmpty() ? TEXT("Blueprint") : *Target));
    }

    // Set parameters if provided
    if (Params->HasField(TEXT("params")))
    {
        const TSharedPtr<FJsonObject>* ParamsObj;
        if (Params->TryGetObjectField(TEXT("params"), ParamsObj))
        {
            // Process parameters
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Param : (*ParamsObj)->Values)
            {
                const FString& ParamName = Param.Key;
                const TSharedPtr<FJsonValue>& ParamValue = Param.Value;
                
                // Find the parameter pin
                UEdGraphPin* ParamPin = FVibeUECommonUtils::FindPin(FunctionNode, ParamName, EGPD_Input);
                if (ParamPin)
                {
                    UE_LOG(LogTemp, Display, TEXT("Found parameter pin '%s' of category '%s'"), 
                           *ParamName, *ParamPin->PinType.PinCategory.ToString());
                    UE_LOG(LogTemp, Display, TEXT("  Current default value: '%s'"), *ParamPin->DefaultValue);
                    if (ParamPin->PinType.PinSubCategoryObject.IsValid())
                    {
                        UE_LOG(LogTemp, Display, TEXT("  Pin subcategory: '%s'"), 
                               *ParamPin->PinType.PinSubCategoryObject->GetName());
                    }
                    
                    // Set parameter based on type
                    if (ParamValue->Type == EJson::String)
                    {
                        FString StringVal = ParamValue->AsString();
                        UE_LOG(LogTemp, Display, TEXT("  Setting string parameter '%s' to: '%s'"), 
                               *ParamName, *StringVal);
                        
                        // Handle class reference parameters (e.g., ActorClass in GetActorOfClass)
                        if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
                        {
                            // For class references, we require the exact class name with proper prefix
                            // - Actor classes must start with 'A' (e.g., ACameraActor)
                            // - Non-actor classes must start with 'U' (e.g., UObject)
                            const FString& ClassName = StringVal;
                            
                            // Use FindFirstObject instead of deprecated ANY_PACKAGE
                            UClass* Class = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("VibeUEBlueprintNodeCommands"));

                            if (!Class)
                            {
                                Class = LoadObject<UClass>(nullptr, *ClassName);
                                UE_LOG(LogVibeUE, Display, TEXT("FindFirstObject<UClass> failed. Assuming soft path  path: %s"), *ClassName);
                            }
                            
                            // If not found, try with Engine module path
                            if (!Class)
                            {
                                FString EngineClassName = FString::Printf(TEXT("/Script/Engine.%s"), *ClassName);
                                Class = LoadObject<UClass>(nullptr, *EngineClassName);
                                UE_LOG(LogVibeUE, Display, TEXT("Trying Engine module path: %s"), *EngineClassName);
                            }
                            
                            if (!Class)
                            {
                                UE_LOG(LogVibeUE, Error, TEXT("Failed to find class '%s'. Make sure to use the exact class name with proper prefix (A for actors, U for non-actors)"), *ClassName);
                                return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find class '%s'"), *ClassName));
                            }

                            const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(EventGraph->GetSchema());
                            if (!K2Schema)
                            {
                                UE_LOG(LogVibeUE, Error, TEXT("Failed to get K2Schema"));
                                return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to get K2Schema"));
                            }

                            K2Schema->TrySetDefaultObject(*ParamPin, Class);
                            if (ParamPin->DefaultObject != Class)
                            {
                                UE_LOG(LogVibeUE, Error, TEXT("Failed to set class reference for pin '%s' to '%s'"), *ParamPin->PinName.ToString(), *ClassName);
                                return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to set class reference for pin '%s'"), *ParamPin->PinName.ToString()));
                            }

                            UE_LOG(LogVibeUE, Log, TEXT("Successfully set class reference for pin '%s' to '%s'"), *ParamPin->PinName.ToString(), *ClassName);
                            continue;
                        }
                        else if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
                        {
                            // Ensure we're using an integer value (no decimal)
                            int32 IntValue = FMath::RoundToInt(ParamValue->AsNumber());
                            ParamPin->DefaultValue = FString::FromInt(IntValue);
                            UE_LOG(LogTemp, Display, TEXT("  Set integer parameter '%s' to: %d (string: '%s')"), 
                                   *ParamName, IntValue, *ParamPin->DefaultValue);
                        }
                        else if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Float)
                        {
                            // For other numeric types
                            float FloatValue = ParamValue->AsNumber();
                            ParamPin->DefaultValue = FString::SanitizeFloat(FloatValue);
                            UE_LOG(LogTemp, Display, TEXT("  Set float parameter '%s' to: %f (string: '%s')"), 
                                   *ParamName, FloatValue, *ParamPin->DefaultValue);
                        }
                        else if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
                        {
                            bool BoolValue = ParamValue->AsBool();
                            ParamPin->DefaultValue = BoolValue ? TEXT("true") : TEXT("false");
                            UE_LOG(LogTemp, Display, TEXT("  Set boolean parameter '%s' to: %s"), 
                                   *ParamName, *ParamPin->DefaultValue);
                        }
                        else if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && ParamPin->PinType.PinSubCategoryObject == TBaseStructure<FVector>::Get())
                        {
                            // Handle array parameters - like Vector parameters
                            const TArray<TSharedPtr<FJsonValue>>* ArrayValue;
                            if (ParamValue->TryGetArray(ArrayValue))
                            {
                                // Check if this could be a vector (array of 3 numbers)
                                if (ArrayValue->Num() == 3)
                                {
                                    // Create a proper vector string: (X=0.0,Y=0.0,Z=1000.0)
                                    float X = (*ArrayValue)[0]->AsNumber();
                                    float Y = (*ArrayValue)[1]->AsNumber();
                                    float Z = (*ArrayValue)[2]->AsNumber();
                                    
                                    FString VectorString = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), X, Y, Z);
                                    ParamPin->DefaultValue = VectorString;
                                    
                                    UE_LOG(LogTemp, Display, TEXT("  Set vector parameter '%s' to: %s"), 
                                           *ParamName, *VectorString);
                                    UE_LOG(LogTemp, Display, TEXT("  Final pin value: '%s'"), 
                                           *ParamPin->DefaultValue);
                                }
                                else
                                {
                                    UE_LOG(LogTemp, Warning, TEXT("Array parameter type not fully supported yet"));
                                }
                            }
                        }
                    }
                    else if (ParamValue->Type == EJson::Number)
                    {
                        // Handle integer vs float parameters correctly
                        if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
                        {
                            // Ensure we're using an integer value (no decimal)
                            int32 IntValue = FMath::RoundToInt(ParamValue->AsNumber());
                            ParamPin->DefaultValue = FString::FromInt(IntValue);
                            UE_LOG(LogTemp, Display, TEXT("  Set integer parameter '%s' to: %d (string: '%s')"), 
                                   *ParamName, IntValue, *ParamPin->DefaultValue);
                        }
                        else
                        {
                            // For other numeric types
                            float FloatValue = ParamValue->AsNumber();
                            ParamPin->DefaultValue = FString::SanitizeFloat(FloatValue);
                            UE_LOG(LogTemp, Display, TEXT("  Set float parameter '%s' to: %f (string: '%s')"), 
                                   *ParamName, FloatValue, *ParamPin->DefaultValue);
                        }
                    }
                    else if (ParamValue->Type == EJson::Boolean)
                    {
                        bool BoolValue = ParamValue->AsBool();
                        ParamPin->DefaultValue = BoolValue ? TEXT("true") : TEXT("false");
                        UE_LOG(LogTemp, Display, TEXT("  Set boolean parameter '%s' to: %s"), 
                               *ParamName, *ParamPin->DefaultValue);
                    }
                    else if (ParamValue->Type == EJson::Array)
                    {
                        UE_LOG(LogTemp, Display, TEXT("  Processing array parameter '%s'"), *ParamName);
                        // Handle array parameters - like Vector parameters
                        const TArray<TSharedPtr<FJsonValue>>* ArrayValue;
                        if (ParamValue->TryGetArray(ArrayValue))
                        {
                            // Check if this could be a vector (array of 3 numbers)
                            if (ArrayValue->Num() == 3 && 
                                (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct) &&
                                (ParamPin->PinType.PinSubCategoryObject == TBaseStructure<FVector>::Get()))
                            {
                                // Create a proper vector string: (X=0.0,Y=0.0,Z=1000.0)
                                float X = (*ArrayValue)[0]->AsNumber();
                                float Y = (*ArrayValue)[1]->AsNumber();
                                float Z = (*ArrayValue)[2]->AsNumber();
                                
                                FString VectorString = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), X, Y, Z);
                                ParamPin->DefaultValue = VectorString;
                                
                                UE_LOG(LogTemp, Display, TEXT("  Set vector parameter '%s' to: %s"), 
                                       *ParamName, *VectorString);
                                UE_LOG(LogTemp, Display, TEXT("  Final pin value: '%s'"), 
                                       *ParamPin->DefaultValue);
                            }
                            else
                            {
                                UE_LOG(LogTemp, Warning, TEXT("Array parameter type not fully supported yet"));
                            }
                        }
                    }
                    // Add handling for other types as needed
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Parameter pin '%s' not found"), *ParamName);
                }
            }
        }
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), FunctionNode->NodeGuid.ToString());
    return ResultObj;
}

TSharedPtr<FJsonObject> FVibeUEBlueprintNodeCommands::HandleAddBlueprintVariable(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString VariableName;
    if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'variable_name' parameter"));
    }

    FString VariableType;
    if (!Params->TryGetStringField(TEXT("variable_type"), VariableType))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'variable_type' parameter"));
    }

    // Get optional parameters
    bool IsExposed = false;
    if (Params->HasField(TEXT("is_exposed")))
    {
        IsExposed = Params->GetBoolField(TEXT("is_exposed"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FVibeUECommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Create variable based on type
    FEdGraphPinType PinType;
    
    // Set up pin type based on variable_type string
    if (VariableType == TEXT("Boolean"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    }
    else if (VariableType == TEXT("Integer") || VariableType == TEXT("Int"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
    }
    else if (VariableType == TEXT("Float"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Float;
    }
    else if (VariableType == TEXT("String"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_String;
    }
    else if (VariableType == TEXT("Vector"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    }
    else
    {
        return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unsupported variable type: %s"), *VariableType));
    }

    // Create the variable
    FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VariableName), PinType);

    // Set variable properties
    FBPVariableDescription* NewVar = nullptr;
    for (FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        if (Variable.VarName == FName(*VariableName))
        {
            NewVar = &Variable;
            break;
        }
    }

    if (NewVar)
    {
        // Set exposure in editor
        if (IsExposed)
        {
            NewVar->PropertyFlags |= CPF_Edit;
        }
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("variable_name"), VariableName);
    ResultObj->SetStringField(TEXT("variable_type"), VariableType);
    return ResultObj;
}

TSharedPtr<FJsonObject> FVibeUEBlueprintNodeCommands::HandleAddBlueprintInputActionNode(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ActionName;
    if (!Params->TryGetStringField(TEXT("action_name"), ActionName))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'action_name' parameter"));
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FVibeUECommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FVibeUECommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FVibeUECommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Create the input action node
    UK2Node_InputAction* InputActionNode = FVibeUECommonUtils::CreateInputActionNode(EventGraph, ActionName, NodePosition);
    if (!InputActionNode)
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create input action node"));
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), InputActionNode->NodeGuid.ToString());
    return ResultObj;
}

TSharedPtr<FJsonObject> FVibeUEBlueprintNodeCommands::HandleAddBlueprintSelfReference(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FVibeUECommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FVibeUECommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FVibeUECommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Create the self node
    UK2Node_Self* SelfNode = FVibeUECommonUtils::CreateSelfReferenceNode(EventGraph, NodePosition);
    if (!SelfNode)
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to create self node"));
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), SelfNode->NodeGuid.ToString());
    return ResultObj;
}

TSharedPtr<FJsonObject> FVibeUEBlueprintNodeCommands::HandleFindBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString NodeType;
    if (!Params->TryGetStringField(TEXT("node_type"), NodeType))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'node_type' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FVibeUECommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FVibeUECommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Create a JSON array for the node GUIDs
    TArray<TSharedPtr<FJsonValue>> NodeGuidArray;
    
    // Filter nodes by the exact requested type
    if (NodeType == TEXT("Event"))
    {
        FString EventName;
        // Prefer 'event_name', but allow legacy 'event_type' for compatibility
        if (!Params->TryGetStringField(TEXT("event_name"), EventName))
        {
            Params->TryGetStringField(TEXT("event_type"), EventName);
        }
        if (EventName.IsEmpty())
        {
            return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'event_name' (or legacy 'event_type') parameter for Event node search"));
        }
        
        // Look for nodes with exact event name (e.g., ReceiveBeginPlay)
        for (UEdGraphNode* Node : EventGraph->Nodes)
        {
            UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
            if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
            {
                UE_LOG(LogTemp, Display, TEXT("Found event node with name %s: %s"), *EventName, *EventNode->NodeGuid.ToString());
                NodeGuidArray.Add(MakeShared<FJsonValueString>(EventNode->NodeGuid.ToString()));
            }
        }
    }
    // Add other node types as needed (InputAction, etc.)
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("node_guids"), NodeGuidArray);
    
    return ResultObj;
} 

static FString GetNodeTypeString(const UEdGraphNode* Node)
{
    if (Cast<UK2Node_Event>(Node)) return TEXT("Event");
    if (Cast<UK2Node_CallFunction>(Node)) return TEXT("FunctionCall");
    if (Cast<UK2Node_VariableGet>(Node)) return TEXT("VariableGet");
    if (Cast<UK2Node_VariableSet>(Node)) return TEXT("VariableSet");
    if (Cast<UK2Node_IfThenElse>(Node)) return TEXT("Branch");
    // if (Cast<UK2Node_ForEachLoop>(Node)) return TEXT("ForEachLoop");  // Commented out - class not available
    if (Cast<UK2Node_Timeline>(Node)) return TEXT("Timeline");
    if (Cast<UK2Node_MacroInstance>(Node)) return TEXT("MacroInstance");
    if (Cast<UK2Node_CustomEvent>(Node)) return TEXT("CustomEvent");
    return Node ? Node->GetClass()->GetName() : TEXT("Unknown");
}

static TSharedPtr<FJsonObject> MakePinJson(const UEdGraphPin* Pin)
{
    TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
    PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
    PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
    PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
    if (!Pin->DefaultValue.IsEmpty())
    {
        PinObj->SetStringField(TEXT("default"), Pin->DefaultValue);
    }
    // Connections for outputs
    if (Pin->Direction == EGPD_Output)
    {
        TArray<TSharedPtr<FJsonValue>> Conns;
        for (const UEdGraphPin* Linked : Pin->LinkedTo)
        {
            TSharedPtr<FJsonObject> C = MakeShared<FJsonObject>();
            C->SetStringField(TEXT("to_node_id"), Linked->GetOwningNode()->NodeGuid.ToString());
            C->SetStringField(TEXT("to_pin"), Linked->PinName.ToString());
            Conns.Add(MakeShared<FJsonValueObject>(C));
        }
        if (Conns.Num() > 0)
        {
            PinObj->SetArrayField(TEXT("connections"), Conns);
        }
    }
    return PinObj;
}

TSharedPtr<FJsonObject> FVibeUEBlueprintNodeCommands::HandleListEventGraphNodes(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    bool bIncludeFunctions = true, bIncludeMacros = true, bIncludeTimeline = true;
    Params->TryGetBoolField(TEXT("include_functions"), bIncludeFunctions);
    Params->TryGetBoolField(TEXT("include_macros"), bIncludeMacros);
    Params->TryGetBoolField(TEXT("include_timeline"), bIncludeTimeline);

    UBlueprint* Blueprint = FVibeUECommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FVibeUECommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    TArray<TSharedPtr<FJsonValue>> NodeArray;
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        const FString Type = GetNodeTypeString(Node);
        if (!bIncludeFunctions && Type == TEXT("FunctionCall")) continue;
        if (!bIncludeMacros && Type == TEXT("MacroInstance")) continue;
        if (!bIncludeTimeline && Type == TEXT("Timeline")) continue;

        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("id"), Node->NodeGuid.ToString());
        Obj->SetStringField(TEXT("node_type"), Type);
        Obj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

        TArray<TSharedPtr<FJsonValue>> Pins;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            Pins.Add(MakeShared<FJsonValueObject>(MakePinJson(Pin)));
        }
        Obj->SetArrayField(TEXT("pins"), Pins);
        NodeArray.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("nodes"), NodeArray);
    return Result;
}

TSharedPtr<FJsonObject> FVibeUEBlueprintNodeCommands::HandleGetNodeDetails(const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogVibeUE, Warning, TEXT("MCP: HandleGetNodeDetails called"));
    
    FString BlueprintName, NodeId;
    bool bIncludeProperties = true;
    bool bIncludePins = true;
    bool bIncludeConnections = true;
    
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        UE_LOG(LogVibeUE, Error, TEXT("MCP: HandleGetNodeDetails - Missing blueprint_name parameter"));
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
    {
        UE_LOG(LogVibeUE, Error, TEXT("MCP: HandleGetNodeDetails - Missing node_id parameter"));
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
    }
    
    // Enhanced: Support optional parameters
    Params->TryGetBoolField(TEXT("include_properties"), bIncludeProperties);
    Params->TryGetBoolField(TEXT("include_pins"), bIncludePins);
    Params->TryGetBoolField(TEXT("include_connections"), bIncludeConnections);
    
    UE_LOG(LogVibeUE, Warning, TEXT("MCP: HandleGetNodeDetails - Blueprint: %s, NodeId: %s, Props: %s, Pins: %s, Conns: %s"), 
        *BlueprintName, *NodeId, bIncludeProperties ? TEXT("true") : TEXT("false"), 
        bIncludePins ? TEXT("true") : TEXT("false"), bIncludeConnections ? TEXT("true") : TEXT("false"));
    
    UBlueprint* Blueprint = FVibeUECommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        UE_LOG(LogVibeUE, Error, TEXT("MCP: HandleGetNodeDetails - Blueprint not found: %s"), *BlueprintName);
        return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }
    UEdGraph* EventGraph = FVibeUECommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }
    UEdGraphNode* Found = nullptr;
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        if (Node->NodeGuid.ToString() == NodeId)
        {
            Found = Node; break;
        }
    }
    if (!Found)
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Node not found"));
    }
    
    // Enhanced: Create comprehensive node information
    TSharedPtr<FJsonObject> NodeInfo = MakeShared<FJsonObject>();
    NodeInfo->SetStringField(TEXT("id"), Found->NodeGuid.ToString());
    NodeInfo->SetStringField(TEXT("node_class"), Found->GetClass()->GetName());
    NodeInfo->SetStringField(TEXT("title"), Found->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
    
    // Enhanced: Add position information
    TArray<TSharedPtr<FJsonValue>> Position;
    Position.Add(MakeShared<FJsonValueNumber>(Found->NodePosX));
    Position.Add(MakeShared<FJsonValueNumber>(Found->NodePosY));
    NodeInfo->SetArrayField(TEXT("position"), Position);
    
    // Enhanced: Add category and metadata
    if (UK2Node* K2Node = Cast<UK2Node>(Found))
    {
        NodeInfo->SetStringField(TEXT("category"), K2Node->GetMenuCategory().ToString());
        NodeInfo->SetStringField(TEXT("tooltip"), K2Node->GetTooltipText().ToString());
        NodeInfo->SetStringField(TEXT("keywords"), K2Node->GetKeywords().ToString());
    }
    
    // Enhanced: Add node state information
    NodeInfo->SetBoolField(TEXT("can_user_delete_node"), Found->CanUserDeleteNode());
    // Note: Comment node checking requires specific include - simplified for now
    NodeInfo->SetStringField(TEXT("node_class_simple"), Found->GetClass()->GetName().Contains(TEXT("Comment")) ? TEXT("Comment") : TEXT("Other"));
    
    // Enhanced: Try to get additional node properties safely
    if (UK2Node* K2Node = Cast<UK2Node>(Found))
    {
        // Color information might not be available on all node types
        NodeInfo->SetStringField(TEXT("node_type"), TEXT("Blueprint"));
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetObjectField(TEXT("node_info"), NodeInfo);
    
    // Enhanced: Include detailed pins if requested
    if (bIncludePins)
    {
        TArray<TSharedPtr<FJsonValue>> InputPins;
        TArray<TSharedPtr<FJsonValue>> OutputPins;
        
        for (UEdGraphPin* Pin : Found->Pins)
        {
            TSharedPtr<FJsonObject> PinInfo = MakeShared<FJsonObject>();
            PinInfo->SetStringField(TEXT("name"), Pin->PinName.ToString());
            PinInfo->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
            PinInfo->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
            PinInfo->SetBoolField(TEXT("is_hidden"), Pin->bHidden);
            PinInfo->SetBoolField(TEXT("is_connected"), Pin->LinkedTo.Num() > 0);
            
            // Enhanced: Add default value information
            if (!Pin->DefaultValue.IsEmpty())
            {
                PinInfo->SetStringField(TEXT("default_value"), Pin->DefaultValue);
            }
            if (Pin->DefaultObject)
            {
                PinInfo->SetStringField(TEXT("default_object"), Pin->DefaultObject->GetName());
            }
            if (!Pin->DefaultTextValue.IsEmpty())
            {
                PinInfo->SetStringField(TEXT("default_text"), Pin->DefaultTextValue.ToString());
            }
            
            // Enhanced: Add connection information if requested
            if (bIncludeConnections && Pin->LinkedTo.Num() > 0)
            {
                TArray<TSharedPtr<FJsonValue>> Connections;
                for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                {
                    if (LinkedPin && LinkedPin->GetOwningNode())
                    {
                        TSharedPtr<FJsonObject> Connection = MakeShared<FJsonObject>();
                        Connection->SetStringField(TEXT("to_node_id"), LinkedPin->GetOwningNode()->NodeGuid.ToString());
                        Connection->SetStringField(TEXT("to_pin"), LinkedPin->PinName.ToString());
                        Connections.Add(MakeShared<FJsonValueObject>(Connection));
                    }
                }
                PinInfo->SetArrayField(TEXT("connections"), Connections);
            }
            
            // Enhanced: Add pin type details
            if (Pin->PinType.PinSubCategory.IsNone() == false)
            {
                PinInfo->SetStringField(TEXT("sub_category"), Pin->PinType.PinSubCategory.ToString());
            }
            if (Pin->PinType.PinSubCategoryObject.IsValid())
            {
                PinInfo->SetStringField(TEXT("sub_category_object"), Pin->PinType.PinSubCategoryObject->GetName());
            }
            PinInfo->SetBoolField(TEXT("is_array"), Pin->PinType.IsArray());
            PinInfo->SetBoolField(TEXT("is_reference"), Pin->PinType.bIsReference);
            
            if (Pin->Direction == EGPD_Input)
            {
                InputPins.Add(MakeShared<FJsonValueObject>(PinInfo));
            }
            else
            {
                OutputPins.Add(MakeShared<FJsonValueObject>(PinInfo));
            }
        }
        
        TSharedPtr<FJsonObject> PinsInfo = MakeShared<FJsonObject>();
        PinsInfo->SetArrayField(TEXT("input_pins"), InputPins);
        PinsInfo->SetArrayField(TEXT("output_pins"), OutputPins);
        Result->SetObjectField(TEXT("pins"), PinsInfo);
    }
    
    // Enhanced: Include node properties if requested
    if (bIncludeProperties)
    {
        TArray<TSharedPtr<FJsonValue>> Properties;
        
        // Get reflection-based properties
        UClass* NodeClass = Found->GetClass();
        for (TFieldIterator<FProperty> PropIt(NodeClass); PropIt; ++PropIt)
        {
            FProperty* Property = *PropIt;
            if (!Property || Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient))
            {
                continue;
            }
            
            // Enhanced: Include editable properties
            if (Property->HasAnyPropertyFlags(CPF_Edit))
            {
                TSharedPtr<FJsonObject> PropInfo = MakeShared<FJsonObject>();
                PropInfo->SetStringField(TEXT("name"), Property->GetName());
                PropInfo->SetStringField(TEXT("type"), Property->GetClass()->GetName());
                PropInfo->SetBoolField(TEXT("editable"), true);
                
                // Enhanced: Try to get current property value as string
                FString PropertyValue;
                if (const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Found))
                {
                    // Use simpler text export method
                    PropertyValue = Property->GetNameCPP();
                    PropInfo->SetStringField(TEXT("current_value"), PropertyValue);
                }
                
                // Enhanced: Add property metadata
                if (Property->HasMetaData(TEXT("Tooltip")))
                {
                    PropInfo->SetStringField(TEXT("tooltip"), Property->GetMetaData(TEXT("Tooltip")));
                }
                if (Property->HasMetaData(TEXT("Category")))
                {
                    PropInfo->SetStringField(TEXT("category"), Property->GetMetaData(TEXT("Category")));
                }
                
                Properties.Add(MakeShared<FJsonValueObject>(PropInfo));
            }
        }
        
        Result->SetArrayField(TEXT("properties"), Properties);
    }
    
    return Result;
}

TSharedPtr<FJsonObject> FVibeUEBlueprintNodeCommands::HandleListBlueprintFunctions(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    bool bIncludeOverrides = true;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }
    Params->TryGetBoolField(TEXT("include_overrides"), bIncludeOverrides);

    UBlueprint* Blueprint = FVibeUECommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    TArray<TSharedPtr<FJsonValue>> Funcs;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (!Graph) continue;
        TSharedPtr<FJsonObject> F = MakeShared<FJsonObject>();
        F->SetStringField(TEXT("name"), Graph->GetName());

        // Gather entry pins
        TArray<FString> Inputs, Outputs;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            // TODO: Fix UK2Node_FunctionEntry compilation issue
            /*
            if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
            {
                for (const FEdGraphPinType& PinType : Entry->UserDefinedPins)
                {
                    Inputs.Add(PinType.PinCategory.ToString());
                }
            }
            */
        }
        // Attempt to find result node types
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node->GetClass()->GetName().Contains(TEXT("FunctionResult")))
            {
                for (UEdGraphPin* Pin : Node->Pins)
                {
                    if (Pin->Direction == EGPD_Input)
                    {
                        Outputs.Add(Pin->PinType.PinCategory.ToString());
                    }
                }
            }
        }
        if (Inputs.Num() > 0)
        {
            TArray<TSharedPtr<FJsonValue>> InVals; for (const FString& S : Inputs) InVals.Add(MakeShared<FJsonValueString>(S));
            F->SetArrayField(TEXT("inputs"), InVals);
        }
        if (Outputs.Num() > 0)
        {
            TArray<TSharedPtr<FJsonValue>> OutVals; for (const FString& S : Outputs) OutVals.Add(MakeShared<FJsonValueString>(S));
            F->SetArrayField(TEXT("outputs"), OutVals);
        }
        Funcs.Add(MakeShared<FJsonValueObject>(F));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("functions"), Funcs);
    return Result;
}

TSharedPtr<FJsonObject> FVibeUEBlueprintNodeCommands::HandleListCustomEvents(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }
    UBlueprint* Blueprint = FVibeUECommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FVibeUECommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }
    UEdGraph* EventGraph = FVibeUECommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FVibeUECommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }
    TArray<TSharedPtr<FJsonValue>> Events;
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        if (UK2Node_CustomEvent* CE = Cast<UK2Node_CustomEvent>(Node))
        {
            TSharedPtr<FJsonObject> Evt = MakeShared<FJsonObject>();
            Evt->SetStringField(TEXT("name"), CE->CustomFunctionName.ToString());
            Events.Add(MakeShared<FJsonValueObject>(Evt));
        }
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("events"), Events);
    return Result;
}

// NEW: Reflection-based command implementations
TSharedPtr<FJsonObject> FVibeUEBlueprintNodeCommands::HandleGetAvailableBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    if (ReflectionCommands.IsValid())
    {
        return ReflectionCommands->HandleGetAvailableBlueprintNodes(Params);
    }
    return FVibeUECommonUtils::CreateErrorResponse(TEXT("Reflection system not initialized"));
}

TSharedPtr<FJsonObject> FVibeUEBlueprintNodeCommands::HandleAddBlueprintNode(const TSharedPtr<FJsonObject>& Params)
{
    if (ReflectionCommands.IsValid())
    {
        return ReflectionCommands->HandleAddBlueprintNode(Params);
    }
    return FVibeUECommonUtils::CreateErrorResponse(TEXT("Reflection system not initialized"));
}

TSharedPtr<FJsonObject> FVibeUEBlueprintNodeCommands::HandleSetBlueprintNodeProperty(const TSharedPtr<FJsonObject>& Params)
{
    if (ReflectionCommands.IsValid())
    {
        return ReflectionCommands->HandleSetBlueprintNodeProperty(Params);
    }
    return FVibeUECommonUtils::CreateErrorResponse(TEXT("Reflection system not initialized"));
}

TSharedPtr<FJsonObject> FVibeUEBlueprintNodeCommands::HandleGetBlueprintNodeProperty(const TSharedPtr<FJsonObject>& Params)
{
    if (ReflectionCommands.IsValid())
    {
        return ReflectionCommands->HandleGetBlueprintNodeProperty(Params);
    }
    return FVibeUECommonUtils::CreateErrorResponse(TEXT("Reflection system not initialized"));
}
