#include "Commands/BlueprintNodeCommands.h"
#include "Commands/BlueprintReflection.h"
#include "Commands/CommonUtils.h"
#include "Commands/ComponentEventBinder.h"
#include "Commands/InputKeyEnumerator.h"
#include "Core/ServiceContext.h"
#include "Services/Blueprint/BlueprintDiscoveryService.h"
#include "Services/Blueprint/BlueprintLifecycleService.h"
#include "Services/Blueprint/BlueprintPropertyService.h"
#include "Services/Blueprint/BlueprintComponentService.h"
#include "Services/Blueprint/BlueprintFunctionService.h"
#include "Services/Blueprint/BlueprintNodeService.h"
#include "Services/Blueprint/BlueprintGraphService.h"
#include "Services/Blueprint/BlueprintReflectionService.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Containers/Set.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Knot.h"  // NEW (Oct 6, 2025): Reroute node multi-connection support
// #include "K2Node_ForEachLoop.h"  // Commented out - header not found
#include "K2Node_Timeline.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_VariableSet.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_InputKey.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "UObject/StrongObjectPtr.h"
#include "GameFramework/InputSettings.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "EdGraphSchema_K2.h"
#include "ScopedTransaction.h"
#include "Dom/JsonValue.h"
#include "Misc/DefaultValueHelper.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Math/Vector4.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Math/Color.h"
#include "UObject/UnrealType.h"
#include "Internationalization/Text.h"
#include "Templates/UniquePtr.h"

// Declare the log category
DEFINE_LOG_CATEGORY_STATIC(LogVibeUE, Log, All);

namespace VibeUENodeIntrospection
{
static FString NormalizeGuid(const FGuid& Guid);
static FString BuildPinIdentifier(const UEdGraphNode* Node, const UEdGraphPin* Pin);
static FString DescribeGraphScope(const UBlueprint* Blueprint, const UEdGraph* Graph);
static FString DescribeExecState(const UEdGraphNode* Node);
static bool IsPureK2Node(const UEdGraphNode* Node);
static TSharedPtr<FJsonObject> BuildPinDescriptor(const UBlueprint* Blueprint, const UEdGraphNode* OwningNode, const UEdGraphPin* Pin);
}

namespace
{
static void AppendStringIfValid(const FString& InValue, TArray<FString>& OutValues)
{
    FString Trimmed = InValue;
    Trimmed.TrimStartAndEndInline();
    if (!Trimmed.IsEmpty())
    {
        OutValues.AddUnique(Trimmed);
    }
}

static void CollectStringValues(const TSharedPtr<FJsonObject>& Source, const TArray<FString>& FieldNames, TArray<FString>& OutValues)
{
    if (!Source.IsValid())
    {
        return;
    }

    for (const FString& FieldName : FieldNames)
    {
        FString SingleValue;
        if (Source->TryGetStringField(FieldName, SingleValue))
        {
            AppendStringIfValid(SingleValue, OutValues);
        }

        const TArray<TSharedPtr<FJsonValue>>* ArrayPtr = nullptr;
        if (Source->TryGetArrayField(FieldName, ArrayPtr) && ArrayPtr)
        {
            for (const TSharedPtr<FJsonValue>& Entry : *ArrayPtr)
            {
                if (!Entry.IsValid())
                {
                    continue;
                }

                if (Entry->Type == EJson::String)
                {
                    AppendStringIfValid(Entry->AsString(), OutValues);
                }
                else if (Entry->Type == EJson::Object)
                {
                    const TSharedPtr<FJsonObject>* EntryObject = nullptr;
                    if (Entry->TryGetObject(EntryObject) && EntryObject)
                    {
                        FString NestedValue;
                        if ((*EntryObject)->TryGetStringField(TEXT("pin_name"), NestedValue))
                        {
                            AppendStringIfValid(NestedValue, OutValues);
                        }
                    }
                }
            }
        }
    }
}

static UEdGraphPin* FindPinForOperation(UEdGraphNode* Node, const FString& RawPinName)
{
    if (!Node)
    {
        return nullptr;
    }

    FString PinName = RawPinName;
    PinName.TrimStartAndEndInline();
    if (PinName.IsEmpty())
    {
        return nullptr;
    }

    auto MatchesPinName = [&PinName](UEdGraphPin* Pin) -> bool
    {
        if (!Pin)
        {
            return false;
        }

        if (Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
        {
            return true;
        }

        const FString PinDisplay = Pin->GetDisplayName().ToString();
        return !PinDisplay.IsEmpty() && PinDisplay.Equals(PinName, ESearchCase::IgnoreCase);
    };

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->ParentPin == nullptr && MatchesPinName(Pin))
        {
            return Pin;
        }
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (!Pin)
        {
            continue;
        }

        if (MatchesPinName(Pin))
        {
            return Pin->ParentPin ? Pin->ParentPin : Pin;
        }
    }

    int32 SeparatorIndex = INDEX_NONE;
    if (PinName.FindChar(TEXT('_'), SeparatorIndex))
    {
        const FString BaseName = PinName.Left(SeparatorIndex);
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->ParentPin == nullptr && Pin->PinName.ToString().Equals(BaseName, ESearchCase::IgnoreCase))
            {
                return Pin;
            }
        }
    }

    return nullptr;
}
}

FBlueprintNodeCommands::FBlueprintNodeCommands()
{
    // Initialize reflection system
    ReflectionCommands = MakeShareable(new FBlueprintReflectionCommands());
    
    // Initialize service context
    TSharedPtr<FServiceContext> ServiceContext = MakeShared<FServiceContext>();
    
    // Initialize Blueprint services
    DiscoveryService = MakeShared<FBlueprintDiscoveryService>(ServiceContext);
    LifecycleService = MakeShared<FBlueprintLifecycleService>(ServiceContext);
    PropertyService = MakeShared<FBlueprintPropertyService>(ServiceContext);
    ComponentService = MakeShared<FBlueprintComponentService>(ServiceContext);
    FunctionService = MakeShared<FBlueprintFunctionService>(ServiceContext);
    NodeService = MakeShared<FBlueprintNodeService>(ServiceContext);
    GraphService = MakeShared<FBlueprintGraphService>(ServiceContext);
    ReflectionService = MakeShared<FBlueprintReflectionService>(ServiceContext);
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogVibeUE, Warning, TEXT("MCP: BlueprintNodeCommands::HandleCommand called with CommandType: %s"), *CommandType);
    if (CommandType == TEXT("manage_blueprint_node"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleManageBlueprintNode"));
        return HandleManageBlueprintNode(Params);
    }
    if (CommandType == TEXT("manage_blueprint_function"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleManageBlueprintFunction"));
        return HandleManageBlueprintFunction(Params);
    }
    if (CommandType == TEXT("get_available_blueprint_nodes"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleGetAvailableBlueprintNodes"));
        return HandleGetAvailableBlueprintNodes(Params);
    }
    if (CommandType == TEXT("discover_nodes_with_descriptors"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleDiscoverNodesWithDescriptors"));
        return HandleDiscoverNodesWithDescriptors(Params);
    }

    const FString ErrorMessage = FString::Printf(TEXT("Unknown command: %s. Use manage_blueprint_node, manage_blueprint_function, get_available_blueprint_nodes, or discover_nodes_with_descriptors."), *CommandType);
    return FCommonUtils::CreateErrorResponse(ErrorMessage);
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleConnectBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Invalid connection payload"));
    }

    TSharedPtr<FJsonObject> ForwardParams = MakeShared<FJsonObject>();
    ForwardParams->Values = Params->Values;

    const TArray<TSharedPtr<FJsonValue>>* ExistingConnections = nullptr;
    if (!Params->TryGetArrayField(TEXT("connections"), ExistingConnections))
    {
        TSharedPtr<FJsonObject> Connection = MakeShared<FJsonObject>();

        auto CopyStringField = [&](const FString& SourceField, const FString& TargetField)
        {
            FString Value;
            if (Params->TryGetStringField(SourceField, Value))
            {
                Value.TrimStartAndEndInline();
                if (!Value.IsEmpty())
                {
                    Connection->SetStringField(TargetField, Value);
                }
            }
        };

        CopyStringField(TEXT("source_pin_id"), TEXT("source_pin_id"));
        CopyStringField(TEXT("target_pin_id"), TEXT("target_pin_id"));
        CopyStringField(TEXT("source_node_id"), TEXT("source_node_id"));
        CopyStringField(TEXT("target_node_id"), TEXT("target_node_id"));
        CopyStringField(TEXT("source_pin"), TEXT("source_pin"));
        CopyStringField(TEXT("source_pin_name"), TEXT("source_pin_name"));
        CopyStringField(TEXT("target_pin"), TEXT("target_pin"));
        CopyStringField(TEXT("target_pin_name"), TEXT("target_pin_name"));

        TArray<TSharedPtr<FJsonValue>> ConnectionArray;
        ConnectionArray.Add(MakeShared<FJsonValueObject>(Connection));
        ForwardParams->SetArrayField(TEXT("connections"), ConnectionArray);
    }

    return HandleConnectPins(ForwardParams);
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleConnectPins(const TSharedPtr<FJsonObject>& Params)
{
    // Extract blueprint name
    FString BlueprintName;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Find Blueprint using DiscoveryService
    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return FCommonUtils::CreateErrorResponse(FindResult.GetErrorMessage());
    }
    UBlueprint* Blueprint = FindResult.GetValue();

    // Call service method with full params
    auto Result = NodeService->ConnectPinsAdvanced(Blueprint, Params);
    
    // Return result directly (already in JSON format)
    if (Result.IsError())
    {
        return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
    }
    
    return Result.GetValue();
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleDisconnectPins(const TSharedPtr<FJsonObject>& Params)
{
    // Extract blueprint name
    FString BlueprintName;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Find Blueprint using DiscoveryService
    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return FCommonUtils::CreateErrorResponse(FindResult.GetErrorMessage());
    }
    UBlueprint* Blueprint = FindResult.GetValue();

    // Call service method with full params
    auto Result = NodeService->DisconnectPinsAdvanced(Blueprint, Params);
    
    // Return result directly (already in JSON format)
    if (Result.IsError())
    {
        return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
    }
    
    return Result.GetValue();
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleAddBlueprintEvent(const TSharedPtr<FJsonObject>& Params)
{
    // 1. Extract and validate parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString EventName;
    if (!Params->TryGetStringField(TEXT("event_name"), EventName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'event_name' parameter"));
    }

    FString GraphName;
    Params->TryGetStringField(TEXT("graph_name"), GraphName);

    // 2. Find Blueprint using DiscoveryService
    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return FCommonUtils::CreateErrorResponse(FindResult.GetErrorMessage());
    }
    UBlueprint* Blueprint = FindResult.GetValue();

    // 3. Call service method - business logic lives in the service
    auto Result = NodeService->CreateEventNode(Blueprint, EventName, GraphName);
    if (Result.IsError())
    {
        return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
    }

    const FNodeInfo& NodeInfo = Result.GetValue();

    // 4. Build JSON response
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetStringField(TEXT("node_id"), NodeInfo.NodeId);
    return Response;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleAddBlueprintInputActionNode(const TSharedPtr<FJsonObject>& Params)
{
    // 1. Extract and validate parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ActionName;
    if (!Params->TryGetStringField(TEXT("action_name"), ActionName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'action_name' parameter"));
    }

    FString GraphName;
    Params->TryGetStringField(TEXT("graph_name"), GraphName);

    // 2. Find Blueprint using DiscoveryService
    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return FCommonUtils::CreateErrorResponse(FindResult.GetErrorMessage());
    }
    UBlueprint* Blueprint = FindResult.GetValue();

    // 3. Call service method - business logic lives in the service
    auto Result = NodeService->CreateInputActionNode(Blueprint, ActionName, GraphName);
    if (Result.IsError())
    {
        return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
    }

    const FNodeInfo& NodeInfo = Result.GetValue();

    // 4. Build JSON response
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetStringField(TEXT("node_id"), NodeInfo.NodeId);
    return Response;
}


TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleFindBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    // 1. Extract and validate parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString NodeType;
    if (!Params->TryGetStringField(TEXT("node_type"), NodeType))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'node_type' parameter"));
    }

    // 2. Find Blueprint using DiscoveryService
    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return FCommonUtils::CreateErrorResponse(FindResult.GetErrorMessage());
    }
    UBlueprint* Blueprint = FindResult.GetValue();

    // 3. Extract optional graph name
    FString GraphName;
    Params->TryGetStringField(TEXT("graph_name"), GraphName);

    // 4. Call service method - business logic lives in the service
    auto Result = NodeService->FindNodes(Blueprint, NodeType, GraphName);
    
    // 5. Convert TResult to JSON response
    if (Result.IsError())
    {
        return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
    }

    const TArray<FString>& NodeGuids = Result.GetValue();
    
    // Build JSON array of node GUIDs
    TArray<TSharedPtr<FJsonValue>> NodeGuidArray;
    for (const FString& Guid : NodeGuids)
    {
        NodeGuidArray.Add(MakeShared<FJsonValueString>(Guid));
    }
    
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

namespace VibeUENodeIntrospection
{
static FString NormalizeGuid(const FGuid& Guid)
{
    return Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces);
}

static FString DescribeGraphScope(const UBlueprint* Blueprint, const UEdGraph* Graph)
{
    if (!Blueprint || !Graph)
    {
        return TEXT("unknown");
    }

    UEdGraph* MutableGraph = const_cast<UEdGraph*>(Graph);
    if (Blueprint->UbergraphPages.Contains(MutableGraph))
    {
        return TEXT("event");
    }
    if (Blueprint->FunctionGraphs.Contains(MutableGraph))
    {
        return TEXT("function");
    }
    if (Blueprint->MacroGraphs.Contains(MutableGraph))
    {
        return TEXT("macro");
    }
    if (Blueprint->IntermediateGeneratedGraphs.Contains(MutableGraph))
    {
        return TEXT("intermediate");
    }

    return TEXT("unknown");
}

static FString DescribeExecState(const UEdGraphNode* Node)
{
    if (!Node)
    {
        return TEXT("unknown");
    }

    if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
    {
        if (const UFunction* TargetFunction = CallNode->GetTargetFunction())
        {
            static const FName LatentMeta(TEXT("Latent"));
            if (TargetFunction->HasMetaData(LatentMeta))
            {
                return TEXT("latent");
            }
        }
    }

    if (Node->IsA<UK2Node_Timeline>())
    {
        return TEXT("timeline");
    }

    if (const UK2Node* K2Node = Cast<UK2Node>(Node))
    {
        if (K2Node->IsNodePure())
        {
            return TEXT("pure");
        }
    }

    return TEXT("normal");
}

static bool IsPureK2Node(const UEdGraphNode* Node)
{
    if (const UK2Node* K2Node = Cast<UK2Node>(Node))
    {
        return K2Node->IsNodePure();
    }

    return false;
}

static FString DescribePinDirection(EEdGraphPinDirection Direction)
{
    return Direction == EGPD_Input ? TEXT("input") : TEXT("output");
}

static FString DescribeContainerType(EPinContainerType ContainerType)
{
    switch (ContainerType)
    {
        case EPinContainerType::Array: return TEXT("array");
        case EPinContainerType::Set:   return TEXT("set");
        case EPinContainerType::Map:   return TEXT("map");
        default:                       return TEXT("none");
    }
}

static TSharedPtr<FJsonValue> ConvertLiteralToJson(const FString& Literal)
{
    if (Literal.IsEmpty())
    {
        return MakeShared<FJsonValueNull>();
    }

    if (Literal.Equals(TEXT("true"), ESearchCase::IgnoreCase))
    {
        return MakeShared<FJsonValueBoolean>(true);
    }

    if (Literal.Equals(TEXT("false"), ESearchCase::IgnoreCase))
    {
        return MakeShared<FJsonValueBoolean>(false);
    }

    double NumericValue = 0.0;
    if (FDefaultValueHelper::ParseDouble(Literal, NumericValue))
    {
        return MakeShared<FJsonValueNumber>(NumericValue);
    }

    return MakeShared<FJsonValueString>(Literal);
}

static TSharedPtr<FJsonValue> BuildDefaultValueJson(const UEdGraphPin* Pin)
{
    if (!Pin)
    {
        return MakeShared<FJsonValueNull>();
    }

    if (Pin->DefaultObject)
    {
        return MakeShared<FJsonValueString>(Pin->DefaultObject->GetPathName());
    }

    if (!Pin->DefaultTextValue.IsEmpty())
    {
        return MakeShared<FJsonValueString>(Pin->DefaultTextValue.ToString());
    }

    if (!Pin->DefaultValue.IsEmpty())
    {
        return ConvertLiteralToJson(Pin->DefaultValue);
    }

    return MakeShared<FJsonValueNull>();
}

static FString DescribePinCategory(const FEdGraphPinType& PinType)
{
    return PinType.PinCategory.ToString();
}

static FString DescribePinSubCategory(const FEdGraphPinType& PinType)
{
    return PinType.PinSubCategory.ToString();
}

static FString DescribePinTypePath(const FEdGraphPinType& PinType)
{
    if (PinType.PinSubCategoryObject.IsValid())
    {
        return PinType.PinSubCategoryObject->GetPathName();
    }
    return FString();
}

static FString BuildPinIdentifier(const UEdGraphNode* Node, const UEdGraphPin* Pin)
{
    if (!Node || !Pin)
    {
        return FString();
    }

    if (Pin->PersistentGuid.IsValid())
    {
        return NormalizeGuid(Pin->PersistentGuid);
    }

    return FString::Printf(TEXT("%s:%s"), *Node->NodeGuid.ToString(), *Pin->PinName.ToString());
}

static TSharedPtr<FJsonObject> BuildPinDescriptor(const UBlueprint* Blueprint, const UEdGraphNode* OwningNode, const UEdGraphPin* Pin)
{
    TSharedPtr<FJsonObject> PinObject = MakeShared<FJsonObject>();
    PinObject->SetStringField(TEXT("pin_id"), BuildPinIdentifier(OwningNode, Pin));
    PinObject->SetStringField(TEXT("name"), Pin->PinName.ToString());
    PinObject->SetStringField(TEXT("direction"), DescribePinDirection(Pin->Direction));
    PinObject->SetStringField(TEXT("category"), DescribePinCategory(Pin->PinType));
    PinObject->SetStringField(TEXT("subcategory"), DescribePinSubCategory(Pin->PinType));
    const FString TypePath = DescribePinTypePath(Pin->PinType);
    if (!TypePath.IsEmpty())
    {
        PinObject->SetStringField(TEXT("pin_type_path"), TypePath);
    }

    PinObject->SetStringField(TEXT("container"), DescribeContainerType(Pin->PinType.ContainerType));
    PinObject->SetBoolField(TEXT("is_const"), Pin->PinType.bIsConst);
    PinObject->SetBoolField(TEXT("is_reference"), Pin->PinType.bIsReference);
    PinObject->SetBoolField(TEXT("is_array"), Pin->PinType.ContainerType == EPinContainerType::Array);
    PinObject->SetBoolField(TEXT("is_set"), Pin->PinType.ContainerType == EPinContainerType::Set);
    PinObject->SetBoolField(TEXT("is_map"), Pin->PinType.ContainerType == EPinContainerType::Map);
    PinObject->SetBoolField(TEXT("is_hidden"), Pin->bHidden);
    PinObject->SetBoolField(TEXT("is_advanced"), Pin->bAdvancedView);
    PinObject->SetBoolField(TEXT("is_connected"), Pin->LinkedTo.Num() > 0);

    if (!Pin->PinToolTip.IsEmpty())
    {
        PinObject->SetStringField(TEXT("tooltip"), Pin->PinToolTip);
    }

    if (!Pin->DefaultValue.IsEmpty())
    {
        PinObject->SetStringField(TEXT("default_value"), Pin->DefaultValue);
    }
    if (!Pin->DefaultTextValue.IsEmpty())
    {
        PinObject->SetStringField(TEXT("default_text"), Pin->DefaultTextValue.ToString());
    }
    if (Pin->DefaultObject)
    {
        PinObject->SetStringField(TEXT("default_object_path"), Pin->DefaultObject->GetPathName());
    }

    PinObject->SetField(TEXT("default_value_json"), BuildDefaultValueJson(Pin));

    TArray<TSharedPtr<FJsonValue>> LinkArray;
    for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
    {
        if (!LinkedPin)
        {
            continue;
        }

        const UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
        TSharedPtr<FJsonObject> Link = MakeShared<FJsonObject>();
        if (LinkedNode)
        {
            Link->SetStringField(TEXT("node_id"), NormalizeGuid(LinkedNode->NodeGuid));
        }
        Link->SetStringField(TEXT("pin_id"), BuildPinIdentifier(LinkedNode, LinkedPin));
        Link->SetStringField(TEXT("pin_name"), LinkedPin->PinName.ToString());
        LinkArray.Add(MakeShared<FJsonValueObject>(Link));
    }
    PinObject->SetArrayField(TEXT("links"), LinkArray);

    return PinObject;
}

static TSharedPtr<FJsonObject> BuildNodeDescriptorJson(UBlueprint* Blueprint, UK2Node* Node, TSharedPtr<FJsonObject>& OutNodeParams, FString& OutSpawnerKey)
{
    OutNodeParams.Reset();
    OutSpawnerKey.Reset();

    if (!Node)
    {
        return nullptr;
    }

    using FDescriptor = FBlueprintReflection::FNodeSpawnerDescriptor;

    if (UK2Node_CallFunction* FuncNode = Cast<UK2Node_CallFunction>(Node))
    {
        if (UFunction* TargetFunction = FuncNode->GetTargetFunction())
        {
            TStrongObjectPtr<UBlueprintFunctionNodeSpawner> TempSpawner(UBlueprintFunctionNodeSpawner::Create(TargetFunction));
            if (TempSpawner.IsValid())
            {
                FDescriptor Descriptor = FBlueprintReflection::ExtractDescriptorFromSpawner(TempSpawner.Get(), Blueprint);
                if (!Descriptor.SpawnerKey.IsEmpty())
                {
                    OutSpawnerKey = Descriptor.SpawnerKey;
                }

                OutNodeParams = MakeShared<FJsonObject>();
                OutNodeParams->SetStringField(TEXT("spawner_key"), Descriptor.SpawnerKey);
                OutNodeParams->SetStringField(TEXT("function_name"), Descriptor.FunctionName);
                if (!Descriptor.FunctionClassPath.IsEmpty())
                {
                    OutNodeParams->SetStringField(TEXT("function_class"), Descriptor.FunctionClassPath);
                }
                else if (!Descriptor.FunctionClassName.IsEmpty())
                {
                    OutNodeParams->SetStringField(TEXT("function_class"), Descriptor.FunctionClassName);
                }
                OutNodeParams->SetBoolField(TEXT("is_static"), Descriptor.bIsStatic);

                return Descriptor.ToJson();
            }
        }
    }
    else if (UK2Node_VariableGet* VarGetNode = Cast<UK2Node_VariableGet>(Node))
    {
        const FName VariableName = VarGetNode->GetVarName();
        if (!VariableName.IsNone())
        {
            FDescriptor Descriptor;
            Descriptor.NodeType = TEXT("variable_get");
            Descriptor.DisplayName = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
            Descriptor.NodeClassName = Node->GetClass()->GetName();
            Descriptor.NodeClassPath = Node->GetClass()->GetPathName();
            Descriptor.VariableName = VariableName.ToString();
            Descriptor.SpawnerKey = FString::Printf(TEXT("GET %s"), *Descriptor.VariableName);

            if (UEdGraphPin* ValuePin = VarGetNode->GetValuePin())
            {
                Descriptor.VariableType = ValuePin->PinType.PinCategory.ToString();
                if (ValuePin->PinType.PinSubCategoryObject.IsValid())
                {
                    Descriptor.VariableTypePath = ValuePin->PinType.PinSubCategoryObject->GetPathName();
                }
            }

            FBlueprintReflection::ExtractPinDescriptorsFromNode(VarGetNode, Descriptor.Pins);
            Descriptor.ExpectedPinCount = Descriptor.Pins.Num();

            OutSpawnerKey = Descriptor.SpawnerKey;

            OutNodeParams = MakeShared<FJsonObject>();
            OutNodeParams->SetStringField(TEXT("variable_name"), Descriptor.VariableName);
            OutNodeParams->SetStringField(TEXT("operation"), TEXT("get"));

            return Descriptor.ToJson();
        }
    }
    else if (UK2Node_VariableSet* VarSetNode = Cast<UK2Node_VariableSet>(Node))
    {
        const FName VariableName = VarSetNode->GetVarName();
        if (!VariableName.IsNone())
        {
            FDescriptor Descriptor;
            Descriptor.NodeType = TEXT("variable_set");
            Descriptor.DisplayName = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
            Descriptor.NodeClassName = Node->GetClass()->GetName();
            Descriptor.NodeClassPath = Node->GetClass()->GetPathName();
            Descriptor.VariableName = VariableName.ToString();
            Descriptor.SpawnerKey = FString::Printf(TEXT("SET %s"), *Descriptor.VariableName);

            UEdGraphPin* ValuePin = VarSetNode->FindPin(VariableName, EGPD_Input);
            if (!ValuePin)
            {
                for (UEdGraphPin* Pin : VarSetNode->Pins)
                {
                    if (Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
                    {
                        ValuePin = Pin;
                        break;
                    }
                }
            }

            if (ValuePin)
            {
                Descriptor.VariableType = ValuePin->PinType.PinCategory.ToString();
                if (ValuePin->PinType.PinSubCategoryObject.IsValid())
                {
                    Descriptor.VariableTypePath = ValuePin->PinType.PinSubCategoryObject->GetPathName();
                }
            }

            FBlueprintReflection::ExtractPinDescriptorsFromNode(VarSetNode, Descriptor.Pins);
            Descriptor.ExpectedPinCount = Descriptor.Pins.Num();

            OutSpawnerKey = Descriptor.SpawnerKey;

            OutNodeParams = MakeShared<FJsonObject>();
            OutNodeParams->SetStringField(TEXT("variable_name"), Descriptor.VariableName);
            OutNodeParams->SetStringField(TEXT("operation"), TEXT("set"));

            return Descriptor.ToJson();
        }
    }
    else if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
    {
        if (UClass* TargetClass = CastNode->TargetType)
        {
            FDescriptor Descriptor;
            Descriptor.NodeType = TEXT("dynamic_cast");
            Descriptor.DisplayName = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
            Descriptor.NodeClassName = Node->GetClass()->GetName();
            Descriptor.NodeClassPath = Node->GetClass()->GetPathName();
            Descriptor.TargetClassName = TargetClass->GetName();
            Descriptor.TargetClassPath = TargetClass->GetPathName();
            Descriptor.SpawnerKey = FString::Printf(TEXT("Cast To %s"), *Descriptor.TargetClassName);

            FBlueprintReflection::ExtractPinDescriptorsFromNode(CastNode, Descriptor.Pins);
            Descriptor.ExpectedPinCount = Descriptor.Pins.Num();

            OutSpawnerKey = Descriptor.SpawnerKey;

            OutNodeParams = MakeShared<FJsonObject>();
            OutNodeParams->SetStringField(TEXT("cast_target"), Descriptor.TargetClassPath);

            return Descriptor.ToJson();
        }
    }

    return nullptr;
}
} // namespace VibeUENodeIntrospection

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleDescribeBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    // Extract and validate blueprint_name
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Find Blueprint using DiscoveryService
    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return FCommonUtils::CreateErrorResponse(FindResult.GetErrorMessage());
    }
    UBlueprint* Blueprint = FindResult.GetValue();

    // Extract pin inclusion parameters
    bool bIncludePins = true;
    Params->TryGetBoolField(TEXT("include_pins"), bIncludePins);

    bool bIncludeInternalPins = false;
    Params->TryGetBoolField(TEXT("include_internal"), bIncludeInternalPins);

    // Call service method
    auto Result = NodeService->DescribeNodesAdvanced(Blueprint, Params, bIncludePins, bIncludeInternalPins);

    if (Result.IsError())
    {
        return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
    }

    // Build success response
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);

    // Convert array of JSON objects to array of JSON values
    TArray<TSharedPtr<FJsonValue>> NodesArray;
    for (const TSharedPtr<FJsonObject>& NodeObj : Result.GetValue())
    {
        NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
    }
    Response->SetArrayField(TEXT("nodes"), NodesArray);

    // Add stats
    double OffsetValue = 0.0;
    int32 Offset = 0;
    if (Params->TryGetNumberField(TEXT("offset"), OffsetValue))
    {
        Offset = FMath::Max(0, static_cast<int32>(OffsetValue));
    }

    double LimitValue = -1.0;
    int32 Limit = -1;
    if (Params->TryGetNumberField(TEXT("limit"), LimitValue))
    {
        Limit = static_cast<int32>(LimitValue);
    }

    TSharedPtr<FJsonObject> Stats = MakeShared<FJsonObject>();
    Stats->SetNumberField(TEXT("offset"), Offset);
    if (Limit >= 0)
    {
        Stats->SetNumberField(TEXT("limit"), Limit);
    }
    Stats->SetNumberField(TEXT("returned"), NodesArray.Num());
    Response->SetObjectField(TEXT("stats"), Stats);

    return Response;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleListEventGraphNodes(const TSharedPtr<FJsonObject>& Params)
{
    // 1. Extract and validate parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    bool bIncludeFunctions = true, bIncludeMacros = true, bIncludeTimeline = true;
    Params->TryGetBoolField(TEXT("include_functions"), bIncludeFunctions);
    Params->TryGetBoolField(TEXT("include_macros"), bIncludeMacros);
    Params->TryGetBoolField(TEXT("include_timeline"), bIncludeTimeline);

    // 2. Find Blueprint using DiscoveryService
    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return FCommonUtils::CreateErrorResponse(FindResult.GetErrorMessage());
    }
    UBlueprint* Blueprint = FindResult.GetValue();

    // 3. Extract optional graph name
    FString GraphName;
    Params->TryGetStringField(TEXT("graph_name"), GraphName);

    // 4. Call service method to list nodes
    auto Result = NodeService->ListNodes(Blueprint, GraphName);
    if (Result.IsError())
    {
        return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
    }

    // 5. Convert TResult to JSON response with filtering
    const TArray<FNodeSummary>& Summaries = Result.GetValue();
    
    TArray<TSharedPtr<FJsonValue>> NodeArray;
    for (const FNodeSummary& Summary : Summaries)
    {
        // Apply filters
        if (!bIncludeFunctions && Summary.NodeType == TEXT("FunctionCall")) continue;
        if (!bIncludeMacros && Summary.NodeType == TEXT("MacroInstance")) continue;
        if (!bIncludeTimeline && Summary.NodeType == TEXT("Timeline")) continue;

        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("id"), Summary.NodeId);
        Obj->SetStringField(TEXT("node_type"), Summary.NodeType);
        Obj->SetStringField(TEXT("title"), Summary.Title);
        
        // Convert TArray<TSharedPtr<FJsonObject>> to TArray<TSharedPtr<FJsonValue>>
        TArray<TSharedPtr<FJsonValue>> PinValues;
        for (const auto& PinObj : Summary.Pins)
        {
            PinValues.Add(MakeShared<FJsonValueObject>(PinObj));
        }
        Obj->SetArrayField(TEXT("pins"), PinValues);
        NodeArray.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("nodes"), NodeArray);
    return ResultObj;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleGetNodeDetails(const TSharedPtr<FJsonObject>& Params)
{
	UE_LOG(LogVibeUE, Warning, TEXT("MCP: HandleGetNodeDetails called"));
	
	// Extract and validate parameters
	FString BlueprintName, NodeId;
	
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		UE_LOG(LogVibeUE, Error, TEXT("MCP: HandleGetNodeDetails - Missing blueprint_name parameter"));
		return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
	{
		UE_LOG(LogVibeUE, Error, TEXT("MCP: HandleGetNodeDetails - Missing node_id parameter"));
		return FCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
	}
	
	UE_LOG(LogVibeUE, Warning, TEXT("MCP: HandleGetNodeDetails - Blueprint: %s, NodeId: %s"), *BlueprintName, *NodeId);
	
	// Find Blueprint using DiscoveryService
	auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
	if (FindResult.IsError())
	{
		UE_LOG(LogVibeUE, Error, TEXT("MCP: HandleGetNodeDetails - %s"), *FindResult.GetErrorMessage());
		return FCommonUtils::CreateErrorResponse(FindResult.GetErrorMessage());
	}
	
	UBlueprint* Blueprint = FindResult.GetValue();
	UE_LOG(LogVibeUE, Warning, TEXT("MCP: HandleGetNodeDetails - Found Blueprint: %s"), *Blueprint->GetName());
	
	// Call service method to get detailed node information
	auto Result = NodeService->GetNodeDetailsAdvanced(Blueprint, NodeId, Params);
	
	if (Result.IsError())
	{
		UE_LOG(LogVibeUE, Error, TEXT("MCP: HandleGetNodeDetails - %s"), *Result.GetErrorMessage());
		return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
	}
	
	UE_LOG(LogVibeUE, Warning, TEXT("MCP: HandleGetNodeDetails - Success"));
	return Result.GetValue();
}

// --- Unified Function Management (Phase 1) ---
TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleManageBlueprintFunction(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName; FString Action;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    if (!Params->TryGetStringField(TEXT("action"), Action))
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'action' parameter"));

    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));

    const FString NormalizedAction = Action.ToLower();

    // Core CRUD
    if (NormalizedAction == TEXT("list"))
    {
        return BuildFunctionSummary(Blueprint);
    }
    if (NormalizedAction == TEXT("get"))
    {
        FString FunctionName; if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
        }
        return BuildSingleFunctionInfo(Blueprint, FunctionName);
    }
    if (NormalizedAction == TEXT("create"))
    {
        FString FunctionName; if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
        }
        return CreateFunctionGraph(Blueprint, FunctionName);
    }
    if (NormalizedAction == TEXT("delete"))
    {
        FString FunctionName; if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
        }
        FString Err; if (!RemoveFunctionGraph(Blueprint, FunctionName, Err))
        {
            return FCommonUtils::CreateErrorResponse(Err);
        }
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        R->SetBoolField(TEXT("success"), true);
        R->SetStringField(TEXT("function_name"), FunctionName);
        return R;
    }

    // Parameter operations
    if (NormalizedAction == TEXT("list_params"))
    {
        FString FunctionName; if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' for list_params"));
        }
        UEdGraph* Graph = nullptr; if (!FindUserFunctionGraph(Blueprint, FunctionName, Graph))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Function not found"));
        }
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        R->SetBoolField(TEXT("success"), true);
        R->SetStringField(TEXT("function_name"), FunctionName);
        TArray<TSharedPtr<FJsonValue>> ParamsArray = ListFunctionParameters(Blueprint, Graph);
        R->SetArrayField(TEXT("parameters"), ParamsArray);
        R->SetNumberField(TEXT("count"), ParamsArray.Num());
        return R;
    }
    if (NormalizedAction == TEXT("add_param"))
    {
        FString FunctionName, ParamName, TypeDesc, Direction;
        if (!Params->TryGetStringField(TEXT("function_name"), FunctionName)) return FCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name'"));
        if (!Params->TryGetStringField(TEXT("param_name"), ParamName)) return FCommonUtils::CreateErrorResponse(TEXT("Missing 'param_name'"));
        if (!Params->TryGetStringField(TEXT("type"), TypeDesc)) return FCommonUtils::CreateErrorResponse(TEXT("Missing 'type'"));
        if (!Params->TryGetStringField(TEXT("direction"), Direction)) Direction = TEXT("input");
        UEdGraph* Graph = nullptr; if (!FindUserFunctionGraph(Blueprint, FunctionName, Graph))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Function not found"));
        }
        return AddFunctionParameter(Blueprint, Graph, ParamName, TypeDesc, Direction);
    }
    if (NormalizedAction == TEXT("remove_param"))
    {
        FString FunctionName, ParamName, Direction;
        if (!Params->TryGetStringField(TEXT("function_name"), FunctionName)) return FCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name'"));
        if (!Params->TryGetStringField(TEXT("param_name"), ParamName)) return FCommonUtils::CreateErrorResponse(TEXT("Missing 'param_name'"));
        if (!Params->TryGetStringField(TEXT("direction"), Direction)) Direction = TEXT("input");
        UEdGraph* Graph = nullptr; if (!FindUserFunctionGraph(Blueprint, FunctionName, Graph))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Function not found"));
        }
        return RemoveFunctionParameter(Blueprint, Graph, ParamName, Direction);
    }
    if (NormalizedAction == TEXT("update_param"))
    {
        FString FunctionName, ParamName, Direction, NewType, NewName;
        if (!Params->TryGetStringField(TEXT("function_name"), FunctionName)) return FCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name'"));
        if (!Params->TryGetStringField(TEXT("param_name"), ParamName)) return FCommonUtils::CreateErrorResponse(TEXT("Missing 'param_name'"));
        if (!Params->TryGetStringField(TEXT("direction"), Direction)) Direction = TEXT("input");
        Params->TryGetStringField(TEXT("new_type"), NewType);
        Params->TryGetStringField(TEXT("new_name"), NewName);
        UEdGraph* Graph = nullptr; if (!FindUserFunctionGraph(Blueprint, FunctionName, Graph))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Function not found"));
        }
        return UpdateFunctionParameter(Blueprint, Graph, ParamName, Direction, NewType, NewName);
    }
    if (NormalizedAction == TEXT("update_properties"))
    {
        FString FunctionName; if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name'"));
        }
        UEdGraph* Graph = nullptr; if (!FindUserFunctionGraph(Blueprint, FunctionName, Graph))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Function not found"));
        }
        return UpdateFunctionProperties(Blueprint, Graph, Params);
    }

    // Local variable operations
    if (NormalizedAction == TEXT("list_locals") || NormalizedAction == TEXT("locals") || NormalizedAction == TEXT("list_local_vars"))
    {
        FString FunctionName; if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' for list_locals"));
        }
        UEdGraph* Graph = nullptr; if (!FindUserFunctionGraph(Blueprint, FunctionName, Graph))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Function not found"));
        }
        TArray<TSharedPtr<FJsonValue>> Locals = ListFunctionLocalVariables(Blueprint, Graph);
        TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("function_name"), FunctionName);
        Response->SetArrayField(TEXT("locals"), Locals);
        Response->SetNumberField(TEXT("count"), Locals.Num());
        return Response;
    }
    if (NormalizedAction == TEXT("add_local") || NormalizedAction == TEXT("add_local_var"))
    {
        FString FunctionName;
        if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name'"));
        }
        FString LocalName;
        if (!Params->TryGetStringField(TEXT("local_name"), LocalName))
        {
            if (!Params->TryGetStringField(TEXT("variable_name"), LocalName) && !Params->TryGetStringField(TEXT("name"), LocalName))
            {
                return FCommonUtils::CreateErrorResponse(TEXT("Missing 'local_name' parameter"));
            }
        }
        FString TypeDesc;
        if (!Params->TryGetStringField(TEXT("type"), TypeDesc))
        {
            if (!Params->TryGetStringField(TEXT("local_type"), TypeDesc) && !Params->TryGetStringField(TEXT("variable_type"), TypeDesc))
            {
                return FCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter for local variable"));
            }
        }
        UEdGraph* Graph = nullptr; if (!FindUserFunctionGraph(Blueprint, FunctionName, Graph))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Function not found"));
        }
        return AddFunctionLocalVariable(Blueprint, Graph, LocalName, TypeDesc, Params);
    }
    if (NormalizedAction == TEXT("remove_local") || NormalizedAction == TEXT("remove_local_var"))
    {
        FString FunctionName;
        if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name'"));
        }
        FString LocalName;
        if (!Params->TryGetStringField(TEXT("local_name"), LocalName))
        {
            if (!Params->TryGetStringField(TEXT("variable_name"), LocalName))
            {
                return FCommonUtils::CreateErrorResponse(TEXT("Missing 'local_name' parameter"));
            }
        }
        UEdGraph* Graph = nullptr; if (!FindUserFunctionGraph(Blueprint, FunctionName, Graph))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Function not found"));
        }
        return RemoveFunctionLocalVariable(Blueprint, Graph, LocalName);
    }
    if (NormalizedAction == TEXT("update_local") || NormalizedAction == TEXT("update_local_var"))
    {
        FString FunctionName;
        if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name'"));
        }
        FString LocalName;
        if (!Params->TryGetStringField(TEXT("local_name"), LocalName))
        {
            if (!Params->TryGetStringField(TEXT("variable_name"), LocalName))
            {
                return FCommonUtils::CreateErrorResponse(TEXT("Missing 'local_name' parameter"));
            }
        }
        UEdGraph* Graph = nullptr; if (!FindUserFunctionGraph(Blueprint, FunctionName, Graph))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Function not found"));
        }
        return UpdateFunctionLocalVariable(Blueprint, Graph, LocalName, Params);
    }
    if (NormalizedAction == TEXT("get_available_local_types") || NormalizedAction == TEXT("list_local_types"))
    {
        return BuildAvailableLocalVariableTypes();
    }

    return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown function action: %s"), *Action));
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleManageBlueprintNode(const TSharedPtr<FJsonObject>& Params)
{
    FString Action;
    if (!Params->TryGetStringField(TEXT("action"), Action))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'action' parameter"));
    }

    const FString NormalizedAction = Action.ToLower();

    if (NormalizedAction == TEXT("list") || NormalizedAction == TEXT("list_nodes") || NormalizedAction == TEXT("enumerate"))
    {
        return HandleListEventGraphNodes(Params);
    }
    if (NormalizedAction == TEXT("find") || NormalizedAction == TEXT("search") || NormalizedAction == TEXT("locate"))
    {
        return HandleFindBlueprintNodes(Params);
    }
    if (NormalizedAction == TEXT("add") || NormalizedAction == TEXT("create") || NormalizedAction == TEXT("spawn"))
    {
        return HandleAddBlueprintNode(Params);
    }
    if (NormalizedAction == TEXT("delete") || NormalizedAction == TEXT("remove") || NormalizedAction == TEXT("destroy"))
    {
        return HandleDeleteBlueprintNode(Params);
    }
    if (NormalizedAction == TEXT("connect") || NormalizedAction == TEXT("link") || NormalizedAction == TEXT("wire") ||
        NormalizedAction == TEXT("connect_pins") || NormalizedAction == TEXT("wire_pins"))
    {
        return HandleConnectPins(Params);
    }
    if (NormalizedAction == TEXT("disconnect") || NormalizedAction == TEXT("disconnect_pins") ||
        NormalizedAction == TEXT("break") || NormalizedAction == TEXT("break_links"))
    {
        return HandleDisconnectPins(Params);
    }
    if (NormalizedAction == TEXT("move") || NormalizedAction == TEXT("reposition") || NormalizedAction == TEXT("translate") || NormalizedAction == TEXT("set_position"))
    {
        return HandleMoveBlueprintNode(Params);
    }
    if (NormalizedAction == TEXT("details") || NormalizedAction == TEXT("get") || NormalizedAction == TEXT("info"))
    {
        return HandleGetNodeDetails(Params);
    }
    if (NormalizedAction == TEXT("describe") || NormalizedAction == TEXT("describe_nodes") || NormalizedAction == TEXT("introspect"))
    {
        return HandleDescribeBlueprintNodes(Params);
    }
    if (NormalizedAction == TEXT("available") || NormalizedAction == TEXT("catalog") || NormalizedAction == TEXT("palette"))
    {
        return HandleGetAvailableBlueprintNodes(Params);
    }
    if (NormalizedAction == TEXT("set_property") || NormalizedAction == TEXT("update_property"))
    {
        return HandleSetBlueprintNodeProperty(Params);
    }
    if (NormalizedAction == TEXT("reset_pin_defaults") || NormalizedAction == TEXT("reset_pin_default") ||
        NormalizedAction == TEXT("reset_pin") || NormalizedAction == TEXT("reset_pins") ||
        NormalizedAction == TEXT("reset_defaults"))
    {
        return HandleResetPinDefaults(Params);
    }
    if (NormalizedAction == TEXT("configure") || NormalizedAction == TEXT("configure_node") || NormalizedAction == TEXT("configure_pins"))
    {
        return HandleConfigureBlueprintNode(Params);
    }
    if (NormalizedAction == TEXT("split") || NormalizedAction == TEXT("split_pin") || NormalizedAction == TEXT("split_pins"))
    {
        return HandleSplitOrRecombinePins(Params, true);
    }
    if (NormalizedAction == TEXT("recombine") || NormalizedAction == TEXT("recombine_pin") || NormalizedAction == TEXT("recombine_pins") ||
        NormalizedAction == TEXT("unsplit") || NormalizedAction == TEXT("unsplit_pins"))
    {
        return HandleSplitOrRecombinePins(Params, false);
    }
    if (NormalizedAction == TEXT("list_custom_events") || NormalizedAction == TEXT("events"))
    {
        return HandleListCustomEvents(Params);
    }
    if (NormalizedAction == TEXT("refresh_node") || NormalizedAction == TEXT("refreshnode") ||
        NormalizedAction == TEXT("reconstruct") || NormalizedAction == TEXT("reconstruct_node"))
    {
        return HandleRefreshBlueprintNode(Params);
    }
    if (NormalizedAction == TEXT("refresh_nodes") || NormalizedAction == TEXT("refreshall") ||
        NormalizedAction == TEXT("refresh_blueprint") || NormalizedAction == TEXT("refreshgraph"))
    {
        return HandleRefreshBlueprintNodes(Params);
    }
    
    // Component Event Actions (Oct 6, 2025)
    if (NormalizedAction == TEXT("create_component_event") || NormalizedAction == TEXT("component_event"))
    {
        return HandleCreateComponentEvent(Params);
    }
    if (NormalizedAction == TEXT("discover_component_events") || NormalizedAction == TEXT("get_component_events") ||
        NormalizedAction == TEXT("component_events") || NormalizedAction == TEXT("list_component_events"))
    {
        return HandleGetComponentEvents(Params);
    }
    
    // Input Key Actions (Oct 6, 2025)
    if (NormalizedAction == TEXT("discover_input_keys") || NormalizedAction == TEXT("get_input_keys") ||
        NormalizedAction == TEXT("get_all_input_keys") || NormalizedAction == TEXT("input_keys") ||
        NormalizedAction == TEXT("list_input_keys"))
    {
        return HandleGetAllInputKeys(Params);
    }
    if (NormalizedAction == TEXT("create_input_key") || NormalizedAction == TEXT("input_key") ||
        NormalizedAction == TEXT("create_input_key_node"))
    {
        return HandleCreateInputKeyNode(Params);
    }

    return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown node action: %s"), *Action));
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::BuildFunctionSummary(UBlueprint* Blueprint)
{
    TArray<TSharedPtr<FJsonValue>> Funcs;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (!Graph) continue;
        TSharedPtr<FJsonObject> F = MakeShared<FJsonObject>();
        F->SetStringField(TEXT("name"), Graph->GetName());
        F->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
        Funcs.Add(MakeShared<FJsonValueObject>(F));
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("functions"), Funcs);
    return Result;
}

bool FBlueprintNodeCommands::FindUserFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName, UEdGraph*& OutGraph) const
{
    OutGraph = nullptr; if (!Blueprint) return false;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (Graph && Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase)) { OutGraph = Graph; return true; }
    }
    return false;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::BuildSingleFunctionInfo(UBlueprint* Blueprint, const FString& FunctionName)
{
    UEdGraph* Graph = nullptr; if (!FindUserFunctionGraph(Blueprint, FunctionName, Graph))
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Function not found: %s"), *FunctionName));
    TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();
    Info->SetStringField(TEXT("name"), FunctionName);
    Info->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
    Info->SetStringField(TEXT("graph_guid"), Graph->GraphGuid.ToString());
    return Info;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::CreateFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName)
{
    UEdGraph* Existing = nullptr; if (FindUserFunctionGraph(Blueprint, FunctionName, Existing))
        return FCommonUtils::CreateErrorResponse(TEXT("Function already exists"));
    // Create new graph then add as function
    UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FName(*FunctionName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    if (!NewGraph)
        return FCommonUtils::CreateErrorResponse(TEXT("Failed to allocate new function graph"));
    // Use non-templated form: explicitly specify nullptr as UFunction signature (template parameter cannot deduce from nullptr directly)
    FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, NewGraph, /*bIsUserCreated*/ true, (UFunction*)nullptr);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    TSharedPtr<FJsonObject> Res = MakeShared<FJsonObject>();
    Res->SetBoolField(TEXT("success"), true);
    Res->SetStringField(TEXT("function_name"), FunctionName);
    Res->SetStringField(TEXT("graph_guid"), NewGraph->GraphGuid.ToString());
    return Res;
}

bool FBlueprintNodeCommands::RemoveFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName, FString& OutError)
{
    UEdGraph* Graph = nullptr; if (!FindUserFunctionGraph(Blueprint, FunctionName, Graph)) { OutError = TEXT("Function not found"); return false; }
    FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph, EGraphRemoveFlags::Recompile);
    return true;
}

TArray<TSharedPtr<FJsonValue>> FBlueprintNodeCommands::ListFunctionParameters(UBlueprint* Blueprint, UEdGraph* FunctionGraph) const
{
    TArray<TSharedPtr<FJsonValue>> Result;
    if (!Blueprint || !FunctionGraph) return Result;
    UK2Node_FunctionEntry* EntryNode = nullptr;
    TArray<UK2Node_FunctionResult*> ResultNodes;
    for (UEdGraphNode* Node : FunctionGraph->Nodes)
    {
        if (UK2Node_FunctionEntry* AsEntry = Cast<UK2Node_FunctionEntry>(Node)) { EntryNode = AsEntry; }
        else if (UK2Node_FunctionResult* AsRes = Cast<UK2Node_FunctionResult>(Node)) { ResultNodes.Add(AsRes); }
    }
    if (!EntryNode) return Result; // malformed function graph

    auto SerializePin = [](const UEdGraphPin* Pin, const FString& Dir)->TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
        P->SetStringField(TEXT("name"), Pin->GetFName().ToString());
        P->SetStringField(TEXT("direction"), Dir);
        FString TypeStr = Pin->PinType.PinCategory.ToString();
        if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object && Pin->PinType.PinSubCategoryObject.IsValid())
        { TypeStr = FString::Printf(TEXT("object:%s"), *Pin->PinType.PinSubCategoryObject->GetName()); }
        else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && Pin->PinType.PinSubCategoryObject.IsValid())
        { TypeStr = FString::Printf(TEXT("struct:%s"), *Pin->PinType.PinSubCategoryObject->GetName()); }
        if (Pin->PinType.ContainerType == EPinContainerType::Array)
        { TypeStr = FString::Printf(TEXT("array<%s>"), *TypeStr); }
        P->SetStringField(TEXT("type"), TypeStr);
        return P;
    };

    // Inputs (entry node outputs)
    for (UEdGraphPin* Pin : EntryNode->Pins)
    {
        if (Pin->Direction == EGPD_Output && Pin->PinName != UEdGraphSchema_K2::PN_Then)
        {
            Result.Add(MakeShared<FJsonValueObject>(SerializePin(Pin, TEXT("input"))));
        }
    }
    // Return / out params (result node inputs)
    for (UK2Node_FunctionResult* RNode : ResultNodes)
    {
        for (UEdGraphPin* Pin : RNode->Pins)
        {
            if (Pin->Direction == EGPD_Input && Pin->PinName != UEdGraphSchema_K2::PN_Then)
            {
                const bool bIsReturn = (Pin->PinName == UEdGraphSchema_K2::PN_ReturnValue);
                Result.Add(MakeShared<FJsonValueObject>(SerializePin(Pin, bIsReturn ? TEXT("return") : TEXT("out"))));
            }
        }
    }
    return Result;
}

TArray<TSharedPtr<FJsonValue>> FBlueprintNodeCommands::ListFunctionLocalVariables(UBlueprint* Blueprint, UEdGraph* FunctionGraph) const
{
    TArray<TSharedPtr<FJsonValue>> Result;
    if (!Blueprint || !FunctionGraph)
    {
        return Result;
    }

    UK2Node_FunctionEntry* Entry = FindFunctionEntry(FunctionGraph);
    if (!Entry)
    {
        return Result;
    }

    for (const FBPVariableDescription& VarDesc : Entry->LocalVariables)
    {
        TSharedPtr<FJsonObject> VarObject = MakeShared<FJsonObject>();
        VarObject->SetStringField(TEXT("name"), VarDesc.VarName.ToString());
        VarObject->SetStringField(TEXT("friendly_name"), VarDesc.FriendlyName);
        VarObject->SetStringField(TEXT("type"), DescribePinType(VarDesc.VarType));
        VarObject->SetStringField(TEXT("display_type"), UEdGraphSchema_K2::TypeToText(VarDesc.VarType).ToString());
        VarObject->SetStringField(TEXT("default_value"), VarDesc.DefaultValue);
        VarObject->SetStringField(TEXT("category"), VarDesc.Category.ToString());
        VarObject->SetStringField(TEXT("pin_category"), VarDesc.VarType.PinCategory.ToString());
        VarObject->SetStringField(TEXT("guid"), VarDesc.VarGuid.ToString());
        VarObject->SetBoolField(TEXT("is_const"), VarDesc.VarType.bIsConst || ((VarDesc.PropertyFlags & CPF_BlueprintReadOnly) != 0));
        VarObject->SetBoolField(TEXT("is_reference"), VarDesc.VarType.bIsReference);
        VarObject->SetBoolField(TEXT("is_editable"), (VarDesc.PropertyFlags & CPF_Edit) != 0);
        VarObject->SetBoolField(TEXT("is_array"), VarDesc.VarType.ContainerType == EPinContainerType::Array);
        VarObject->SetBoolField(TEXT("is_set"), VarDesc.VarType.ContainerType == EPinContainerType::Set);
        VarObject->SetBoolField(TEXT("is_map"), VarDesc.VarType.ContainerType == EPinContainerType::Map);
        Result.Add(MakeShared<FJsonValueObject>(VarObject));
    }

    return Result;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::AddFunctionLocalVariable(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& VarName, const FString& TypeDesc, const TSharedPtr<FJsonObject>& Params)
{
    if (!Blueprint || !FunctionGraph)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Invalid blueprint or function graph"));
    }

    if (VarName.TrimStartAndEnd().IsEmpty())
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Local variable name cannot be empty"));
    }

    UK2Node_FunctionEntry* Entry = FindFunctionEntry(FunctionGraph);
    if (!Entry)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Function entry node not found"));
    }

    for (const FBPVariableDescription& Local : Entry->LocalVariables)
    {
        if (Local.VarName.ToString().Equals(VarName, ESearchCase::IgnoreCase))
        {
            return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Local variable '%s' already exists"), *VarName));
        }
    }

    FEdGraphPinType PinType;
    FString TypeError;
    if (!ParseTypeDescriptor(TypeDesc, PinType, TypeError))
    {
        return FCommonUtils::CreateErrorResponse(TypeError);
    }

    bool bIsReference = false;
    const bool bHasReference = Params.IsValid() && Params->TryGetBoolField(TEXT("is_reference"), bIsReference);
    bool bIsConst = false;
    const bool bHasConst = Params.IsValid() && Params->TryGetBoolField(TEXT("is_const"), bIsConst);
    bool bIsEditable = false;
    const bool bHasEditable = Params.IsValid() && Params->TryGetBoolField(TEXT("is_editable"), bIsEditable);

    if (bHasReference)
    {
        PinType.bIsReference = bIsReference;
    }
    if (bHasConst)
    {
        PinType.bIsConst = bIsConst;
    }

    FString DefaultValue;
    bool bHasDefaultValue = false;
    if (Params.IsValid() && Params->HasField(TEXT("default_value")))
    {
        bHasDefaultValue = true;
        if (!Params->TryGetStringField(TEXT("default_value"), DefaultValue))
        {
            bool BoolVal = false;
            if (Params->TryGetBoolField(TEXT("default_value"), BoolVal))
            {
                DefaultValue = BoolVal ? TEXT("true") : TEXT("false");
            }
            else
            {
                double NumberVal = 0.0;
                if (Params->TryGetNumberField(TEXT("default_value"), NumberVal))
                {
                    DefaultValue = FString::SanitizeFloat(NumberVal);
                }
                else
                {
                    return FCommonUtils::CreateErrorResponse(TEXT("default_value must be a string, boolean, or number"));
                }
            }
        }
    }

    if (!bHasDefaultValue)
    {
        DefaultValue.Reset();
    }

    if (!FBlueprintEditorUtils::AddLocalVariable(Blueprint, FunctionGraph, FName(*VarName), PinType, DefaultValue))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Failed to add local variable"));
    }

    Entry = FindFunctionEntry(FunctionGraph);
    if (Entry)
    {
        Entry->Modify();
        for (FBPVariableDescription& Local : Entry->LocalVariables)
        {
            if (Local.VarName.ToString().Equals(VarName, ESearchCase::IgnoreCase))
            {
                if (bHasConst)
                {
                    if (bIsConst)
                    {
                        Local.PropertyFlags |= CPF_BlueprintReadOnly;
                        Local.VarType.bIsConst = true;
                    }
                    else
                    {
                        Local.PropertyFlags &= ~CPF_BlueprintReadOnly;
                        Local.VarType.bIsConst = false;
                    }
                }
                if (bHasReference)
                {
                    Local.VarType.bIsReference = bIsReference;
                }
                if (bHasEditable)
                {
                    if (bIsEditable)
                    {
                        Local.PropertyFlags |= CPF_Edit;
                        Local.PropertyFlags |= CPF_BlueprintVisible;
                    }
                    else
                    {
                        Local.PropertyFlags &= ~CPF_Edit;
                    }
                }
                break;
            }
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    TArray<TSharedPtr<FJsonValue>> Locals = ListFunctionLocalVariables(Blueprint, FunctionGraph);
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("local_name"), VarName);
    Response->SetStringField(TEXT("type"), DescribePinType(PinType));
    Response->SetArrayField(TEXT("locals"), Locals);
    Response->SetNumberField(TEXT("count"), Locals.Num());
    return Response;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::RemoveFunctionLocalVariable(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& VarName)
{
    if (!Blueprint || !FunctionGraph)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Invalid blueprint or function graph"));
    }

    FName VarFName(*VarName);
    UK2Node_FunctionEntry* Entry = nullptr;
    FBPVariableDescription* Existing = FBlueprintEditorUtils::FindLocalVariable(Blueprint, FunctionGraph, VarFName, &Entry);
    if (!Existing || !Entry)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Local variable '%s' not found"), *VarName));
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

    TArray<TSharedPtr<FJsonValue>> Locals = ListFunctionLocalVariables(Blueprint, FunctionGraph);
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("removed_local"), VarName);
    Response->SetArrayField(TEXT("locals"), Locals);
    Response->SetNumberField(TEXT("count"), Locals.Num());
    return Response;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::UpdateFunctionLocalVariable(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& VarName, const TSharedPtr<FJsonObject>& Params)
{
    if (!Blueprint || !FunctionGraph)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Invalid blueprint or function graph"));
    }

    if (!Params.IsValid())
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing update parameters"));
    }

    FName CurrentName(*VarName);
    UK2Node_FunctionEntry* Entry = nullptr;
    FBPVariableDescription* VarDesc = FBlueprintEditorUtils::FindLocalVariable(Blueprint, FunctionGraph, CurrentName, &Entry);
    if (!VarDesc || !Entry)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Local variable '%s' not found"), *VarName));
    }

    const UStruct* Scope = ResolveFunctionScopeStruct(Blueprint, FunctionGraph);

    Entry->Modify();

    FString NewName;
    Params->TryGetStringField(TEXT("new_name"), NewName);

    FString NewTypeDesc;
    Params->TryGetStringField(TEXT("new_type"), NewTypeDesc);

    FString DefaultValue;
    bool bHasDefaultValue = false;
    if (Params->HasField(TEXT("default_value")))
    {
        bHasDefaultValue = true;
        if (!Params->TryGetStringField(TEXT("default_value"), DefaultValue))
        {
            bool BoolVal = false;
            if (Params->TryGetBoolField(TEXT("default_value"), BoolVal))
            {
                DefaultValue = BoolVal ? TEXT("true") : TEXT("false");
            }
            else
            {
                double NumberVal = 0.0;
                if (Params->TryGetNumberField(TEXT("default_value"), NumberVal))
                {
                    DefaultValue = FString::SanitizeFloat(NumberVal);
                }
                else
                {
                    return FCommonUtils::CreateErrorResponse(TEXT("default_value must be a string, boolean, or number"));
                }
            }
        }
    }

    bool bIsReference = VarDesc->VarType.bIsReference;
    const bool bHasReference = Params->TryGetBoolField(TEXT("is_reference"), bIsReference);
    bool bIsConst = VarDesc->VarType.bIsConst || ((VarDesc->PropertyFlags & CPF_BlueprintReadOnly) != 0);
    const bool bHasConst = Params->TryGetBoolField(TEXT("is_const"), bIsConst);
    bool bIsEditable = (VarDesc->PropertyFlags & CPF_Edit) != 0;
    const bool bHasEditable = Params->TryGetBoolField(TEXT("is_editable"), bIsEditable);

    bool bStructuralChange = false;

    if (!NewTypeDesc.IsEmpty())
    {
        FEdGraphPinType NewPinType;
        FString TypeError;
        if (!ParseTypeDescriptor(NewTypeDesc, NewPinType, TypeError))
        {
            return FCommonUtils::CreateErrorResponse(TypeError);
        }
        if (bHasReference)
        {
            NewPinType.bIsReference = bIsReference;
        }
        if (bHasConst)
        {
            NewPinType.bIsConst = bIsConst;
        }

        if (Scope)
        {
            FBlueprintEditorUtils::ChangeLocalVariableType(Blueprint, Scope, CurrentName, NewPinType);
            bStructuralChange = true;
        }
        else
        {
            Entry->Modify();
            VarDesc->VarType = NewPinType;
            VarDesc->DefaultValue.Empty();
            bStructuralChange = true;
        }
    }
    else if (bHasReference || bHasConst)
    {
        Entry->Modify();
        VarDesc->VarType.bIsReference = bIsReference;
        VarDesc->VarType.bIsConst = bIsConst;
        bStructuralChange = true;
    }

    if (!NewName.IsEmpty() && !NewName.Equals(VarName, ESearchCase::CaseSensitive))
    {
        if (Scope)
        {
            FBlueprintEditorUtils::RenameLocalVariable(Blueprint, Scope, CurrentName, FName(*NewName));
        }
        else
        {
            Entry->Modify();
            VarDesc->VarName = FName(*NewName);
            VarDesc->FriendlyName = FName::NameToDisplayString(NewName, VarDesc->VarType.PinCategory == UEdGraphSchema_K2::PC_Boolean);
        }
        CurrentName = FName(*NewName);
        bStructuralChange = true;
    }

    Entry = nullptr;
    VarDesc = FBlueprintEditorUtils::FindLocalVariable(Blueprint, FunctionGraph, CurrentName, &Entry);
    if (!VarDesc || !Entry)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Local variable could not be resolved after update"));
    }

    Entry->Modify();

    if (bHasDefaultValue)
    {
        VarDesc->DefaultValue = DefaultValue;
        bStructuralChange = true;
    }

    if (bHasConst)
    {
        if (bIsConst)
        {
            VarDesc->PropertyFlags |= CPF_BlueprintReadOnly;
            VarDesc->VarType.bIsConst = true;
        }
        else
        {
            VarDesc->PropertyFlags &= ~CPF_BlueprintReadOnly;
            VarDesc->VarType.bIsConst = false;
        }
        bStructuralChange = true;
    }

    if (bHasReference)
    {
        VarDesc->VarType.bIsReference = bIsReference;
        bStructuralChange = true;
    }

    if (bHasEditable)
    {
        if (bIsEditable)
        {
            VarDesc->PropertyFlags |= CPF_Edit;
            VarDesc->PropertyFlags |= CPF_BlueprintVisible;
        }
        else
        {
            VarDesc->PropertyFlags &= ~CPF_Edit;
        }
        bStructuralChange = true;
    }

    if (bStructuralChange)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    }

    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    TArray<TSharedPtr<FJsonValue>> Locals = ListFunctionLocalVariables(Blueprint, FunctionGraph);
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("local_name"), CurrentName.ToString());
    Response->SetArrayField(TEXT("locals"), Locals);
    Response->SetNumberField(TEXT("count"), Locals.Num());
    return Response;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::BuildAvailableLocalVariableTypes() const
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);

    TArray<TSharedPtr<FJsonValue>> Types;
    auto AddType = [&Types](const FString& Descriptor, const FString& DisplayName, const FString& Category, const FString& Notes)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("descriptor"), Descriptor);
        Obj->SetStringField(TEXT("display_name"), DisplayName);
        Obj->SetStringField(TEXT("category"), Category);
        if (!Notes.IsEmpty())
        {
            Obj->SetStringField(TEXT("notes"), Notes);
        }
        Types.Add(MakeShared<FJsonValueObject>(Obj));
    };

    // Basic types
    AddType(TEXT("bool"), TEXT("Boolean"), TEXT("basic"), TEXT("True/false value"));
    AddType(TEXT("byte"), TEXT("Byte"), TEXT("basic"), TEXT("Unsigned 0-255"));
    AddType(TEXT("int"), TEXT("Integer"), TEXT("basic"), TEXT("32-bit signed integer"));
    AddType(TEXT("int64"), TEXT("Integer64"), TEXT("basic"), TEXT("64-bit signed integer"));
    AddType(TEXT("float"), TEXT("Float"), TEXT("basic"), TEXT("Single-precision floating point"));
    AddType(TEXT("double"), TEXT("Double"), TEXT("basic"), TEXT("Double-precision floating point"));
    AddType(TEXT("string"), TEXT("String"), TEXT("basic"), TEXT("UTF-16 string value"));
    AddType(TEXT("name"), TEXT("Name"), TEXT("basic"), TEXT("Name identifier"));
    AddType(TEXT("text"), TEXT("Text"), TEXT("basic"), TEXT("Localized text"));

    // Struct types
    AddType(TEXT("struct:Vector"), TEXT("Vector"), TEXT("struct"), TEXT("3D vector (X,Y,Z)"));
    AddType(TEXT("struct:Vector2D"), TEXT("Vector2D"), TEXT("struct"), TEXT("2D vector (X,Y)"));
    AddType(TEXT("struct:Vector4"), TEXT("Vector4"), TEXT("struct"), TEXT("4-component vector"));
    AddType(TEXT("struct:Rotator"), TEXT("Rotator"), TEXT("struct"), TEXT("Pitch/Yaw/Roll"));
    AddType(TEXT("struct:Transform"), TEXT("Transform"), TEXT("struct"), TEXT("Location, rotation, scale"));
    AddType(TEXT("struct:Color"), TEXT("Color"), TEXT("struct"), TEXT("RGBA 0-255"));
    AddType(TEXT("struct:LinearColor"), TEXT("LinearColor"), TEXT("struct"), TEXT("RGBA 0-1"));

    // Object/class types
    AddType(TEXT("object:Actor"), TEXT("Actor"), TEXT("object"), TEXT("Reference to AActor"));
    AddType(TEXT("object:Pawn"), TEXT("Pawn"), TEXT("object"), TEXT("Reference to APawn"));
    AddType(TEXT("object:Character"), TEXT("Character"), TEXT("object"), TEXT("Reference to ACharacter"));
    AddType(TEXT("object:PlayerController"), TEXT("PlayerController"), TEXT("object"), TEXT("Reference to APlayerController"));
    AddType(TEXT("object:StaticMeshComponent"), TEXT("StaticMeshComponent"), TEXT("object"), TEXT("Reference to UStaticMeshComponent"));
    AddType(TEXT("object:StaticMesh"), TEXT("StaticMesh"), TEXT("object"), TEXT("Reference to UStaticMesh asset"));
    AddType(TEXT("object:Material"), TEXT("Material"), TEXT("object"), TEXT("Reference to UMaterial"));
    AddType(TEXT("object:Texture2D"), TEXT("Texture2D"), TEXT("object"), TEXT("Reference to UTexture2D"));
    AddType(TEXT("class:Actor"), TEXT("Actor Class"), TEXT("class"), TEXT("TSubclassOf<AActor> reference"));
    AddType(TEXT("interface:YourInterface"), TEXT("Interface"), TEXT("interface"), TEXT("Replace 'YourInterface' with the interface class (e.g., interface:MyBlueprintInterface)"));

    Response->SetArrayField(TEXT("types"), Types);
    Response->SetNumberField(TEXT("count"), Types.Num());
    Response->SetStringField(TEXT("usage"), TEXT("Use descriptors directly or wrap with array<...> for arrays."));
    return Response;
}

FString FBlueprintNodeCommands::DescribePinType(const FEdGraphPinType& PinType) const
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
        if (Category == UEdGraphSchema_K2::PC_SoftObject && SubObject)
        {
            return FString::Printf(TEXT("soft_object:%s"), *SubObject->GetName());
        }
        if (Category == UEdGraphSchema_K2::PC_SoftClass && SubObject)
        {
            return FString::Printf(TEXT("soft_class:%s"), *SubObject->GetName());
        }
        if (Category == UEdGraphSchema_K2::PC_Interface && SubObject)
        {
            return FString::Printf(TEXT("interface:%s"), *SubObject->GetName());
        }
        if (Category == UEdGraphSchema_K2::PC_Enum && SubObject)
        {
            return FString::Printf(TEXT("enum:%s"), *SubObject->GetName());
        }
        if (Category == UEdGraphSchema_K2::PC_Wildcard) return TEXT("wildcard");
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
        FString ValueDesc = DescribeCategory(PinType.PinValueType.TerminalCategory, PinType.PinValueType.TerminalSubCategory, PinType.PinValueType.TerminalSubCategoryObject.Get());
        return FString::Printf(TEXT("map<%s,%s>"), *Base, *ValueDesc);
    }
    return Base;
}

const UStruct* FBlueprintNodeCommands::ResolveFunctionScopeStruct(UBlueprint* Blueprint, UEdGraph* FunctionGraph) const
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

static UK2Node_FunctionEntry* FindFunctionEntry(UEdGraph* Graph)
{
    for (UEdGraphNode* Node : Graph->Nodes)
    { if (UK2Node_FunctionEntry* E = Cast<UK2Node_FunctionEntry>(Node)) return E; }
    return nullptr;
}
static UK2Node_FunctionResult* FindOrCreateResultNode(UBlueprint* Blueprint, UEdGraph* Graph)
{
    for (UEdGraphNode* Node : Graph->Nodes) if (UK2Node_FunctionResult* R = Cast<UK2Node_FunctionResult>(Node)) return R;
    FGraphNodeCreator<UK2Node_FunctionResult> Creator(*Graph); UK2Node_FunctionResult* NewNode = Creator.CreateNode(); Creator.Finalize();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    return NewNode;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::AddFunctionParameter(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& ParamName, const FString& TypeDesc, const FString& Direction)
{
    if (!Blueprint || !FunctionGraph) return FCommonUtils::CreateErrorResponse(TEXT("Invalid blueprint/graph"));
    FString DirLower = Direction.ToLower();
    if (!(DirLower == TEXT("input") || DirLower == TEXT("out") || DirLower == TEXT("return")))
        return FCommonUtils::CreateErrorResponse(TEXT("Invalid direction (expected input|out|return)"));

    // Check existing
    auto Existing = ListFunctionParameters(Blueprint, FunctionGraph);
    for (auto& V : Existing)
    {
        const TSharedPtr<FJsonObject>* ObjPtr; if (V->TryGetObject(ObjPtr))
        { if ((*ObjPtr)->GetStringField(TEXT("name")).Equals(ParamName, ESearchCase::IgnoreCase)) return FCommonUtils::CreateErrorResponse(TEXT("Parameter already exists")); }
    }

    FEdGraphPinType PinType; FString TypeErr; if (!ParseTypeDescriptor(TypeDesc, PinType, TypeErr)) return FCommonUtils::CreateErrorResponse(TypeErr);

    UK2Node_FunctionEntry* Entry = FindFunctionEntry(FunctionGraph);
    if (!Entry) return FCommonUtils::CreateErrorResponse(TEXT("Function entry node not found"));

    if (DirLower == TEXT("input"))
    {
        UEdGraphPin* NewPin = Entry->CreateUserDefinedPin(FName(*ParamName), PinType, EGPD_Output, false);
        if (!NewPin) return FCommonUtils::CreateErrorResponse(TEXT("Failed to create input pin"));
    }
    else // out or return
    {
        UK2Node_FunctionResult* ResultNode = FindOrCreateResultNode(Blueprint, FunctionGraph);
        if (!ResultNode) return FCommonUtils::CreateErrorResponse(TEXT("Failed to resolve/create result node"));
        FName NewPinName = (DirLower == TEXT("return")) ? UEdGraphSchema_K2::PN_ReturnValue : FName(*ParamName);
        if (DirLower == TEXT("return"))
        {
            // If return already exists, error
            for (UEdGraphPin* P : ResultNode->Pins)
            { if (P->PinName == UEdGraphSchema_K2::PN_ReturnValue) return FCommonUtils::CreateErrorResponse(TEXT("Return value already exists")); }
        }
        UEdGraphPin* NewPin = ResultNode->CreateUserDefinedPin(NewPinName, PinType, EGPD_Input, false);
        if (!NewPin) return FCommonUtils::CreateErrorResponse(TEXT("Failed to create result pin"));
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetArrayField(TEXT("parameters"), ListFunctionParameters(Blueprint, FunctionGraph));
    return R;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::RemoveFunctionParameter(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& ParamName, const FString& Direction)
{
    if (!Blueprint || !FunctionGraph) return FCommonUtils::CreateErrorResponse(TEXT("Invalid blueprint/graph"));
    FString DirLower = Direction.ToLower();
    bool bFound = false;
    if (DirLower == TEXT("input"))
    {
        if (UK2Node_FunctionEntry* Entry = FindFunctionEntry(FunctionGraph))
        {
            for (int32 i=Entry->Pins.Num()-1;i>=0;--i)
            {
                UEdGraphPin* P = Entry->Pins[i];
                if (P->Direction==EGPD_Output && P->PinName.ToString().Equals(ParamName, ESearchCase::IgnoreCase))
                { P->BreakAllPinLinks(); Entry->Pins.RemoveAt(i); bFound=true; }
            }
        }
    }
    else // out or return
    {
        for (UEdGraphNode* Node : FunctionGraph->Nodes)
        {
            if (UK2Node_FunctionResult* RNode = Cast<UK2Node_FunctionResult>(Node))
            {
                for (int32 i=RNode->Pins.Num()-1;i>=0;--i)
                {
                    UEdGraphPin* P = RNode->Pins[i];
                    if (P->Direction==EGPD_Input)
                    {
                        bool bNameMatch = false;
                        if (DirLower==TEXT("return")) bNameMatch = (P->PinName == UEdGraphSchema_K2::PN_ReturnValue);
                        else bNameMatch = P->PinName.ToString().Equals(ParamName, ESearchCase::IgnoreCase);
                        if (bNameMatch)
                        { P->BreakAllPinLinks(); RNode->Pins.RemoveAt(i); bFound=true; }
                    }
                }
            }
        }
    }
    if (!bFound) return FCommonUtils::CreateErrorResponse(TEXT("Parameter not found"));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetArrayField(TEXT("parameters"), ListFunctionParameters(Blueprint, FunctionGraph));
    return R;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::UpdateFunctionParameter(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& ParamName, const FString& Direction, const FString& NewType, const FString& NewName)
{
    if (!Blueprint || !FunctionGraph) return FCommonUtils::CreateErrorResponse(TEXT("Invalid blueprint/graph"));
    FString DirLower = Direction.ToLower();
    FEdGraphPinType NewPinType; bool bTypeChange=false; if (!NewType.IsEmpty()) { FString Err; if (!ParseTypeDescriptor(NewType, NewPinType, Err)) return FCommonUtils::CreateErrorResponse(Err); bTypeChange=true; }
    bool bModified=false;

    auto ApplyChanges = [&](UEdGraphPin* P)
    {
        if (bTypeChange) P->PinType = NewPinType;
        if (!NewName.IsEmpty() && P->PinName.ToString() != NewName && P->PinName != UEdGraphSchema_K2::PN_ReturnValue)
        { P->PinName = FName(*NewName); }
        bModified=true;
    };

    if (DirLower==TEXT("input"))
    {
        if (UK2Node_FunctionEntry* Entry = FindFunctionEntry(FunctionGraph))
        {
            for (UEdGraphPin* P : Entry->Pins)
            {
                if (P->Direction==EGPD_Output && P->PinName.ToString().Equals(ParamName, ESearchCase::IgnoreCase)) { ApplyChanges(P); }
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
                    if (P->Direction==EGPD_Input)
                    {
                        bool bMatch=false;
                        if (DirLower==TEXT("return")) bMatch = (P->PinName==UEdGraphSchema_K2::PN_ReturnValue);
                        else bMatch = P->PinName.ToString().Equals(ParamName, ESearchCase::IgnoreCase);
                        if (bMatch) { ApplyChanges(P); }
                    }
                }
            }
        }
    }
    if (!bModified) return FCommonUtils::CreateErrorResponse(TEXT("Parameter not found"));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>(); R->SetBoolField(TEXT("success"), true); R->SetArrayField(TEXT("parameters"), ListFunctionParameters(Blueprint, FunctionGraph)); return R;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::UpdateFunctionProperties(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const TSharedPtr<FJsonObject>& Params)
{
    if (!Blueprint || !FunctionGraph) return FCommonUtils::CreateErrorResponse(TEXT("Invalid blueprint or function graph"));
    // Only 'is_pure' supported currently; others require metadata API adaptation
    bool bPure=false; bool bHas=false; bHas = Params->TryGetBoolField(TEXT("is_pure"), bPure);
    if (bHas)
    {
        // Pure flag requires locating entry node & adjusting function flags post-compile; stub for now
        return FCommonUtils::CreateErrorResponse(TEXT("Setting is_pure not yet implemented"));
    }
    return FCommonUtils::CreateErrorResponse(TEXT("No supported properties provided"));
}

bool FBlueprintNodeCommands::ParseTypeDescriptor(const FString& TypeDesc, FEdGraphPinType& OutType, FString& OutError) const
{
    FString Lower = TypeDesc.ToLower();
    OutType.ResetToDefaults();

    if (Lower.StartsWith(TEXT("array<")) && Lower.EndsWith(TEXT(">")))
    {
        FString Inner = TypeDesc.Mid(6, TypeDesc.Len() - 7);
        Inner.TrimStartAndEndInline();
        FEdGraphPinType InnerType; FString Err;
        if (!ParseTypeDescriptor(Inner, InnerType, Err)) { OutError = Err; return false; }
        OutType = InnerType; OutType.ContainerType = EPinContainerType::Array; return true;
    }
    if (Lower.StartsWith(TEXT("set<")) && Lower.EndsWith(TEXT(">")))
    {
        FString Inner = TypeDesc.Mid(4, TypeDesc.Len() - 5);
        Inner.TrimStartAndEndInline();
        FEdGraphPinType InnerType; FString Err;
        if (!ParseTypeDescriptor(Inner, InnerType, Err)) { OutError = Err; return false; }
        OutType = InnerType; OutType.ContainerType = EPinContainerType::Set; return true;
    }
    if (Lower.StartsWith(TEXT("map<")) && Lower.EndsWith(TEXT(">")))
    {
        FString Inner = TypeDesc.Mid(4, TypeDesc.Len() - 5);
        Inner.TrimStartAndEndInline();
        FString KeyDesc, ValueDesc;
        if (!Inner.Split(TEXT(","), &KeyDesc, &ValueDesc))
        {
            OutError = TEXT("Map descriptors must use the format map<key,value>");
            return false;
        }
        KeyDesc.TrimStartAndEndInline();
        ValueDesc.TrimStartAndEndInline();

        FEdGraphPinType KeyType; FString Err;
        if (!ParseTypeDescriptor(KeyDesc, KeyType, Err)) { OutError = Err; return false; }
        FEdGraphPinType ValueType;
        if (!ParseTypeDescriptor(ValueDesc, ValueType, Err)) { OutError = Err; return false; }

        OutType = KeyType;
        OutType.ContainerType = EPinContainerType::Map;
        OutType.PinValueType.TerminalCategory = ValueType.PinCategory;
        OutType.PinValueType.TerminalSubCategory = ValueType.PinSubCategory;
        OutType.PinValueType.TerminalSubCategoryObject = ValueType.PinSubCategoryObject;
        OutType.PinValueType.bTerminalIsConst = ValueType.bIsConst;
        OutType.PinValueType.bTerminalIsWeakPointer = ValueType.bIsWeakPointer;
        OutType.PinValueType.bTerminalIsUObjectWrapper = ValueType.bIsUObjectWrapper;
        return true;
    }
    if (Lower == TEXT("bool")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Boolean; return true; }
    if (Lower == TEXT("byte")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Byte; return true; }
    if (Lower == TEXT("int") || Lower == TEXT("int32")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Int; return true; }
    if (Lower == TEXT("int64") || Lower == TEXT("integer64")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Int64; return true; }
    if (Lower == TEXT("float")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Float; return true; }
    if (Lower == TEXT("double")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Double; return true; }
    if (Lower == TEXT("string")) { OutType.PinCategory = UEdGraphSchema_K2::PC_String; return true; }
    if (Lower == TEXT("name")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Name; return true; }
    if (Lower == TEXT("text")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Text; return true; }
    if (Lower == TEXT("vector")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Struct; OutType.PinSubCategoryObject = TBaseStructure<FVector>::Get(); return true; }
    if (Lower == TEXT("vector2d")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Struct; OutType.PinSubCategoryObject = TBaseStructure<FVector2D>::Get(); return true; }
    if (Lower == TEXT("vector4")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Struct; OutType.PinSubCategoryObject = TBaseStructure<FVector4>::Get(); return true; }
    if (Lower == TEXT("rotator")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Struct; OutType.PinSubCategoryObject = TBaseStructure<FRotator>::Get(); return true; }
    if (Lower == TEXT("transform")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Struct; OutType.PinSubCategoryObject = TBaseStructure<FTransform>::Get(); return true; }
    if (Lower == TEXT("color")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Struct; OutType.PinSubCategoryObject = TBaseStructure<FColor>::Get(); return true; }
    if (Lower == TEXT("linearcolor")) { OutType.PinCategory = UEdGraphSchema_K2::PC_Struct; OutType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get(); return true; }
    if (Lower.StartsWith(TEXT("enum:")))
    {
        FString EnumName = TypeDesc.Mid(5);
        UEnum* EnumObj = FindFirstObject<UEnum>(*EnumName);
        if (!EnumObj)
        {
            OutError = FString::Printf(TEXT("Enum '%s' not found"), *EnumName);
            return false;
        }
        OutType.PinCategory = UEdGraphSchema_K2::PC_Enum;
        OutType.PinSubCategoryObject = EnumObj;
        return true;
    }
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
    if (Lower.StartsWith(TEXT("class:")))
    {
        FString ClassName = TypeDesc.Mid(6);
        UClass* C = FindFirstObject<UClass>(*ClassName);
        if (!C)
        {
            OutError = FString::Printf(TEXT("Class '%s' not found"), *ClassName);
            return false;
        }
        OutType.PinCategory = UEdGraphSchema_K2::PC_Class;
        OutType.PinSubCategoryObject = C;
        return true;
    }
    if (Lower.StartsWith(TEXT("soft_object:")))
    {
        FString ClassName = TypeDesc.Mid(12);
        UClass* C = FindFirstObject<UClass>(*ClassName);
        if (!C)
        {
            OutError = FString::Printf(TEXT("Class '%s' not found"), *ClassName);
            return false;
        }
        OutType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
        OutType.PinSubCategoryObject = C;
        return true;
    }
    if (Lower.StartsWith(TEXT("soft_class:")))
    {
        FString ClassName = TypeDesc.Mid(11);
        UClass* C = FindFirstObject<UClass>(*ClassName);
        if (!C)
        {
            OutError = FString::Printf(TEXT("Class '%s' not found"), *ClassName);
            return false;
        }
        OutType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
        OutType.PinSubCategoryObject = C;
        return true;
    }
    if (Lower.StartsWith(TEXT("interface:")))
    {
        FString ClassName = TypeDesc.Mid(10);
        UClass* C = FindFirstObject<UClass>(*ClassName);
        if (!C)
        {
            OutError = FString::Printf(TEXT("Interface '%s' not found"), *ClassName);
            return false;
        }
        OutType.PinCategory = UEdGraphSchema_K2::PC_Interface;
        OutType.PinSubCategoryObject = C;
        return true;
    }
    if (Lower.StartsWith(TEXT("struct:")))
    {
        FString StructName = TypeDesc.Mid(7);
        UScriptStruct* S = FindFirstObject<UScriptStruct>(*StructName);
        if (!S)
        {
            OutError = FString::Printf(TEXT("Struct '%s' not found"), *StructName);
            return false;
        }
        OutType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        OutType.PinSubCategoryObject = S;
        return true;
    }
    OutError = FString::Printf(TEXT("Unsupported type descriptor '%s'"), *TypeDesc);
    return false;
}

UEdGraph* FBlueprintNodeCommands::ResolveTargetGraph(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Params, FString& OutError) const
{
    OutError.Reset();
    if (!Blueprint)
    {
        OutError = TEXT("Invalid blueprint");
        return nullptr;
    }

    FString GraphGuidString;
    if (Params->TryGetStringField(TEXT("graph_guid"), GraphGuidString) && !GraphGuidString.IsEmpty())
    {
        FGuid DesiredGuid;
        if (!FGuid::Parse(GraphGuidString, DesiredGuid))
        {
            OutError = FString::Printf(TEXT("Invalid graph_guid '%s'"), *GraphGuidString);
            return nullptr;
        }

        TArray<UEdGraph*> AllGraphs;
        GatherCandidateGraphs(Blueprint, nullptr, AllGraphs);
        for (UEdGraph* Graph : AllGraphs)
        {
            if (Graph && Graph->GraphGuid == DesiredGuid)
            {
                return Graph;
            }
        }

        OutError = FString::Printf(TEXT("Graph with guid '%s' not found"), *GraphGuidString);
        return nullptr;
    }

    FString Scope;
    Params->TryGetStringField(TEXT("graph_scope"), Scope);
    Scope.TrimStartAndEndInline();

    FString NamedGraph;
    Params->TryGetStringField(TEXT("graph_name"), NamedGraph);
    NamedGraph.TrimStartAndEndInline();

    if (Scope.IsEmpty() || Scope.Equals(TEXT("event"), ESearchCase::IgnoreCase))
    {
        if (!NamedGraph.IsEmpty())
        {
            for (UEdGraph* Graph : Blueprint->UbergraphPages)
            {
                if (Graph && Graph->GetName().Equals(NamedGraph, ESearchCase::IgnoreCase))
                {
                    return Graph;
                }
            }
            OutError = FString::Printf(TEXT("Event graph '%s' not found"), *NamedGraph);
            return nullptr;
        }

        return FCommonUtils::FindOrCreateEventGraph(Blueprint);
    }

    if (Scope.Equals(TEXT("function"), ESearchCase::IgnoreCase))
    {
        FString FunctionName;
        if (!Params->TryGetStringField(TEXT("function_name"), FunctionName) || FunctionName.IsEmpty())
        {
            if (!NamedGraph.IsEmpty())
            {
                FunctionName = NamedGraph;
            }
        }

        if (FunctionName.IsEmpty())
        {
            OutError = TEXT("Missing 'function_name' for function scope");
            return nullptr;
        }

        UEdGraph* FunctionGraph = nullptr;
        if (!FindUserFunctionGraph(Blueprint, FunctionName, FunctionGraph))
        {
            OutError = FString::Printf(TEXT("Function not found: %s"), *FunctionName);
            return nullptr;
        }
        return FunctionGraph;
    }

    if (Scope.Equals(TEXT("macro"), ESearchCase::IgnoreCase))
    {
        FString MacroName;
        if (!Params->TryGetStringField(TEXT("macro_name"), MacroName) || MacroName.IsEmpty())
        {
            MacroName = NamedGraph;
        }

        if (MacroName.IsEmpty())
        {
            OutError = TEXT("Missing 'macro_name' for macro scope");
            return nullptr;
        }

        for (UEdGraph* Graph : Blueprint->MacroGraphs)
        {
            if (Graph && Graph->GetName().Equals(MacroName, ESearchCase::IgnoreCase))
            {
                return Graph;
            }
        }

        OutError = FString::Printf(TEXT("Macro graph '%s' not found"), *MacroName);
        return nullptr;
    }

    OutError = FString::Printf(TEXT("Unsupported graph_scope '%s'"), *Scope);
    return nullptr;
}

void FBlueprintNodeCommands::GatherCandidateGraphs(UBlueprint* Blueprint, UEdGraph* PreferredGraph, TArray<UEdGraph*>& OutGraphs) const
{
    OutGraphs.Reset();

    if (!Blueprint)
    {
        return;
    }

    TSet<UEdGraph*> Seen;
    auto AddGraph = [&OutGraphs, &Seen](UEdGraph* Graph)
    {
        if (Graph && !Seen.Contains(Graph))
        {
            Seen.Add(Graph);
            OutGraphs.Add(Graph);
        }
    };

    AddGraph(PreferredGraph);

    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        AddGraph(Graph);
    }
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        AddGraph(Graph);
    }
    for (UEdGraph* Graph : Blueprint->MacroGraphs)
    {
        AddGraph(Graph);
    }
    for (UEdGraph* Graph : Blueprint->IntermediateGeneratedGraphs)
    {
        AddGraph(Graph);
    }
}

bool FBlueprintNodeCommands::ResolvePinByIdentifier(const TArray<UEdGraph*>& Graphs, const FString& Identifier, FResolvedPinReference& OutPin) const
{
    OutPin = FResolvedPinReference();

    FString Trimmed = Identifier;
    Trimmed.TrimStartAndEndInline();
    if (Trimmed.IsEmpty())
    {
        return false;
    }

    FGuid ParsedGuid;
    const bool bHasGuid = FGuid::Parse(Trimmed, ParsedGuid);

    FString LowerIdentifier = Trimmed;
    LowerIdentifier.ToLowerInline();

    FString NodePart;
    FString PinPart;
    const bool bHasNodePinPair = Trimmed.Split(TEXT(":"), &NodePart, &PinPart, ESearchCase::IgnoreCase, ESearchDir::FromStart);

    FGuid NodeGuid;
    const bool bNodeGuidValid = bHasNodePinPair && FGuid::Parse(NodePart, NodeGuid);
    FString PinNameLower = PinPart;
    if (bHasNodePinPair)
    {
        PinNameLower.TrimStartAndEndInline();
        PinNameLower.ToLowerInline();
    }

    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph)
        {
            continue;
        }

        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node)
            {
                continue;
            }

            if (bNodeGuidValid && Node->NodeGuid != NodeGuid)
            {
                continue;
            }

            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin)
                {
                    continue;
                }

                if (bHasGuid)
                {
                    if ((Pin->PersistentGuid.IsValid() && Pin->PersistentGuid == ParsedGuid) || Pin->PinId == ParsedGuid)
                    {
                        OutPin.Pin = Pin;
                        OutPin.Node = Node;
                        OutPin.Graph = Graph;
                        OutPin.Identifier = VibeUENodeIntrospection::BuildPinIdentifier(Node, Pin);
                        return true;
                    }
                }

                FString Candidate = VibeUENodeIntrospection::BuildPinIdentifier(Node, Pin);
                FString CandidateLower = Candidate;
                CandidateLower.ToLowerInline();
                if (!CandidateLower.IsEmpty() && CandidateLower == LowerIdentifier)
                {
                    OutPin.Pin = Pin;
                    OutPin.Node = Node;
                    OutPin.Graph = Graph;
                    OutPin.Identifier = Candidate;
                    return true;
                }

                if (bHasNodePinPair)
                {
                    FString PinDisplayName = Pin->PinName.ToString();
                    PinDisplayName.ToLowerInline();
                    if (PinDisplayName == PinNameLower)
                    {
                        OutPin.Pin = Pin;
                        OutPin.Node = Node;
                        OutPin.Graph = Graph;
                        OutPin.Identifier = VibeUENodeIntrospection::BuildPinIdentifier(Node, Pin);
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

bool FBlueprintNodeCommands::ResolvePinByNodeAndName(const TArray<UEdGraph*>& Graphs, const FString& NodeIdentifier, const FString& PinName, EEdGraphPinDirection DesiredDirection, FResolvedPinReference& OutPin, FString& OutError) const
{
    OutPin = FResolvedPinReference();
    OutError.Reset();

    FString TrimmedNode = NodeIdentifier;
    TrimmedNode.TrimStartAndEndInline();
    if (TrimmedNode.IsEmpty())
    {
        OutError = TEXT("Missing node identifier");
        return false;
    }

    FString TrimmedPin = PinName;
    TrimmedPin.TrimStartAndEndInline();
    if (TrimmedPin.IsEmpty())
    {
        OutError = TEXT("Missing pin name");
        return false;
    }

    UEdGraphNode* Node = nullptr;
    UEdGraph* Graph = nullptr;
    if (!ResolveNodeIdentifier(TrimmedNode, Graphs, Node, Graph) || !Node)
    {
        OutError = FString::Printf(TEXT("Node '%s' not found"), *TrimmedNode);
        return false;
    }

    UEdGraphPin* Pin = FCommonUtils::FindPin(Node, TrimmedPin, DesiredDirection);
    if (!Pin && DesiredDirection != EGPD_MAX)
    {
        Pin = FCommonUtils::FindPin(Node, TrimmedPin, EGPD_MAX);
    }
    if (!Pin)
    {
        OutError = FString::Printf(TEXT("Pin '%s' not found on node '%s'"), *TrimmedPin, *TrimmedNode);
        return false;
    }

    OutPin.Pin = Pin;
    OutPin.Node = Node;
    OutPin.Graph = Graph ? Graph : Node->GetGraph();
    OutPin.Identifier = VibeUENodeIntrospection::BuildPinIdentifier(Node, Pin);
    return true;
}

bool FBlueprintNodeCommands::ResolvePinFromPayload(const TSharedPtr<FJsonObject>& Payload, const TArray<FString>& RolePrefixes, EEdGraphPinDirection DesiredDirection, const TArray<UEdGraph*>& Graphs, FResolvedPinReference& OutPin, FString& OutError) const
{
    OutPin = FResolvedPinReference();
    OutError.Reset();

    if (!Payload.IsValid())
    {
        OutError = TEXT("Invalid connection payload");
        return false;
    }

    auto GatherKeys = [](const TArray<FString>& Prefixes, const TArray<FString>& BaseNames)
    {
        TArray<FString> Keys;
        TSet<FString> Seen;

        for (const FString& Base : BaseNames)
        {
            if (Base.IsEmpty())
            {
                continue;
            }

            if (!Seen.Contains(Base))
            {
                Seen.Add(Base);
                Keys.Add(Base);
            }
        }

        for (const FString& Prefix : Prefixes)
        {
            if (Prefix.IsEmpty())
            {
                continue;
            }

            for (const FString& Base : BaseNames)
            {
                if (Base.IsEmpty())
                {
                    continue;
                }

                const FString Key = Prefix + TEXT("_") + Base;
                if (!Seen.Contains(Key))
                {
                    Seen.Add(Key);
                    Keys.Add(Key);
                }
            }
        }

        return Keys;
    };

    auto TryGetStringFromKeys = [&](const TArray<FString>& Keys, FString& OutValue) -> bool
    {
        for (const FString& Key : Keys)
        {
            if (Key.IsEmpty())
            {
                continue;
            }

            if (Payload->TryGetStringField(Key, OutValue))
            {
                OutValue.TrimStartAndEndInline();
                if (!OutValue.IsEmpty())
                {
                    return true;
                }
            }
        }
        return false;
    };

    FString PinIdentifier;
    const TArray<FString> IdentifierBaseNames = {TEXT("pin_id"), TEXT("pin_guid"), TEXT("pin_identifier")};
    TArray<FString> IdentifierKeys = GatherKeys(RolePrefixes, IdentifierBaseNames);
    if (TryGetStringFromKeys(IdentifierKeys, PinIdentifier))
    {
        if (ResolvePinByIdentifier(Graphs, PinIdentifier, OutPin))
        {
            return true;
        }
    }

    FString NodeIdentifier;
    const TArray<FString> NodeBaseNames = {TEXT("node_id"), TEXT("node_guid"), TEXT("node")};
    TryGetStringFromKeys(GatherKeys(RolePrefixes, NodeBaseNames), NodeIdentifier);

    FString PinName;
    const TArray<FString> PinBaseNames = {TEXT("pin_name"), TEXT("pin"), TEXT("pin_display_name")};
    TryGetStringFromKeys(GatherKeys(RolePrefixes, PinBaseNames), PinName);

    if (!NodeIdentifier.IsEmpty() && !PinName.IsEmpty())
    {
        if (ResolvePinByNodeAndName(Graphs, NodeIdentifier, PinName, DesiredDirection, OutPin, OutError))
        {
            return true;
        }
        return false;
    }

    if (!PinIdentifier.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Pin identifier '%s' not found"), *PinIdentifier);
        return false;
    }

    if (!NodeIdentifier.IsEmpty())
    {
        const FString DisplayPin = PinName.IsEmpty() ? TEXT("<unspecified>") : PinName;
        OutError = FString::Printf(TEXT("Pin '%s' not found on node '%s'"), *DisplayPin, *NodeIdentifier);
        return false;
    }

    OutError = TEXT("No pin identifier or node/pin name provided");
    return false;
}

bool FBlueprintNodeCommands::ResolveNodeIdentifier(const FString& Identifier, const TArray<UEdGraph*>& Graphs, UEdGraphNode*& OutNode, UEdGraph*& OutGraph) const
{
    OutNode = nullptr;
    OutGraph = nullptr;

    if (Identifier.IsEmpty())
    {
        return false;
    }

    FString NormalizedIdentifier = Identifier;
    NormalizedIdentifier.ReplaceInline(TEXT("{"), TEXT(""));
    NormalizedIdentifier.ReplaceInline(TEXT("}"), TEXT(""));
    FString HyphenlessIdentifier = NormalizedIdentifier;
    HyphenlessIdentifier.ReplaceInline(TEXT("-"), TEXT(""));

    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph)
        {
            continue;
        }

        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node)
            {
                continue;
            }

            FString GuidString = Node->NodeGuid.ToString();
            GuidString.ReplaceInline(TEXT("{"), TEXT(""));
            GuidString.ReplaceInline(TEXT("}"), TEXT(""));
            FString HyphenlessGuid = GuidString;
            HyphenlessGuid.ReplaceInline(TEXT("-"), TEXT(""));

            if (GuidString.Equals(NormalizedIdentifier, ESearchCase::IgnoreCase) ||
                HyphenlessGuid.Equals(HyphenlessIdentifier, ESearchCase::IgnoreCase))
            {
                OutNode = Node;
                OutGraph = Graph;
                return true;
            }

            FString LexGuidString = LexToString(Node->NodeGuid);
            FString HyphenlessLexGuid = LexGuidString;
            HyphenlessLexGuid.ReplaceInline(TEXT("-"), TEXT(""));

            if (LexGuidString.Equals(NormalizedIdentifier, ESearchCase::IgnoreCase) ||
                HyphenlessLexGuid.Equals(HyphenlessIdentifier, ESearchCase::IgnoreCase))
            {
                OutNode = Node;
                OutGraph = Graph;
                return true;
            }

            const FString NodeName = Node->GetName();
            if (NodeName.Equals(NormalizedIdentifier, ESearchCase::IgnoreCase))
            {
                OutNode = Node;
                OutGraph = Graph;
                return true;
            }

            const FString UniqueIdString = FString::FromInt(Node->GetUniqueID());
            if (UniqueIdString.Equals(NormalizedIdentifier, ESearchCase::IgnoreCase))
            {
                OutNode = Node;
                OutGraph = Graph;
                return true;
            }

            const FString TitleString = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
            if (TitleString.Equals(NormalizedIdentifier, ESearchCase::IgnoreCase))
            {
                OutNode = Node;
                OutGraph = Graph;
                return true;
            }
        }
    }

    return false;
}

FString FBlueprintNodeCommands::DescribeAvailableNodes(const TArray<UEdGraph*>& Graphs) const
{
    FString Description;

    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph)
        {
            continue;
        }

        const FString GraphName = Graph->GetName();
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node)
            {
                continue;
            }

            if (!Description.IsEmpty())
            {
                Description += TEXT(" | ");
            }

            Description += FString::Printf(TEXT("%s (Graph=%s, Guid=%s, Name=%s, UniqueId=%d)"),
                                           *Node->GetNodeTitle(ENodeTitleType::ListView).ToString(),
                                           *GraphName,
                                           *Node->NodeGuid.ToString(),
                                           *Node->GetName(),
                                           Node->GetUniqueID());
        }
    }

    return Description;
}

bool FBlueprintNodeCommands::ResolveNodeContext(const TSharedPtr<FJsonObject>& Params,
    UBlueprint*& OutBlueprint,
    UEdGraphNode*& OutNode,
    UEdGraph*& OutGraph,
    TArray<UEdGraph*>& OutCandidateGraphs,
    FString& OutBlueprintName,
    FString& OutNodeIdentifier,
    FString& OutError) const
{
    OutBlueprint = nullptr;
    OutNode = nullptr;
    OutGraph = nullptr;
    OutCandidateGraphs.Reset();
    OutBlueprintName.Reset();
    OutNodeIdentifier.Reset();
    OutError.Reset();

    if (!Params.IsValid())
    {
        OutError = TEXT("Invalid parameter payload");
        return false;
    }

    auto TryLoadBlueprintName = [&](const FString& FieldName) -> bool
    {
        FString Value;
        if (Params->TryGetStringField(FieldName, Value))
        {
            Value.TrimStartAndEndInline();
            if (!Value.IsEmpty())
            {
                OutBlueprintName = Value;
                return true;
            }
        }
        return false;
    };

    if (!TryLoadBlueprintName(TEXT("blueprint_name")))
    {
        TryLoadBlueprintName(TEXT("blueprint"));
    }
    if (OutBlueprintName.IsEmpty())
    {
        OutError = TEXT("Missing 'blueprint_name' parameter");
        return false;
    }

    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(OutBlueprintName);
    if (!Blueprint)
    {
        OutError = FString::Printf(TEXT("Blueprint not found: %s"), *OutBlueprintName);
        return false;
    }

    FString GraphError;
    UEdGraph* PreferredGraph = ResolveTargetGraph(Blueprint, Params, GraphError);
    if (!PreferredGraph && !GraphError.IsEmpty())
    {
        OutError = GraphError;
        return false;
    }

    GatherCandidateGraphs(Blueprint, PreferredGraph, OutCandidateGraphs);
    if (OutCandidateGraphs.Num() == 0)
    {
        GatherCandidateGraphs(Blueprint, nullptr, OutCandidateGraphs);
    }

    if (OutCandidateGraphs.Num() == 0)
    {
        OutError = TEXT("No graphs available for blueprint");
        return false;
    }

    auto CollectNodeIdentifiers = [&](const TSharedPtr<FJsonObject>& Source, TArray<FString>& OutIdentifiers)
    {
        if (!Source.IsValid())
        {
            return;
        }

        const TArray<FString> NodeFields = {
            TEXT("node_id"), TEXT("node_guid"), TEXT("node_identifier"), TEXT("node"), TEXT("node_name"),
            TEXT("node_title"), TEXT("target_node_id"), TEXT("target_node"), TEXT("source_node_id"),
            TEXT("node_ids"), TEXT("node_identifiers"), TEXT("nodes")
        };
        CollectStringValues(Source, NodeFields, OutIdentifiers);
    };

    TArray<FString> NodeIdentifiers;
    CollectNodeIdentifiers(Params, NodeIdentifiers);

    const TSharedPtr<FJsonObject>* ExtraObject = nullptr;
    if (Params->TryGetObjectField(TEXT("extra"), ExtraObject) && ExtraObject)
    {
        CollectNodeIdentifiers(*ExtraObject, NodeIdentifiers);
    }

    const TSharedPtr<FJsonObject>* ConfigObject = nullptr;
    if (Params->TryGetObjectField(TEXT("node_config"), ConfigObject) && ConfigObject)
    {
        CollectNodeIdentifiers(*ConfigObject, NodeIdentifiers);
    }

    if (NodeIdentifiers.Num() == 0)
    {
        FString DirectNodeId;
        if (Params->TryGetStringField(TEXT("node_id"), DirectNodeId))
        {
            DirectNodeId.TrimStartAndEndInline();
            if (!DirectNodeId.IsEmpty())
            {
                NodeIdentifiers.AddUnique(DirectNodeId);
            }
        }
    }

    if (NodeIdentifiers.Num() == 0)
    {
        OutError = TEXT("Missing node identifier");
        return false;
    }

    for (const FString& Identifier : NodeIdentifiers)
    {
        FString Trimmed = Identifier;
        Trimmed.TrimStartAndEndInline();
        if (Trimmed.IsEmpty())
        {
            continue;
        }

        UEdGraphNode* Node = nullptr;
        UEdGraph* Graph = nullptr;
        if (ResolveNodeIdentifier(Trimmed, OutCandidateGraphs, Node, Graph) && Node)
        {
            OutBlueprint = Blueprint;
            OutNode = Node;
            OutGraph = Graph ? Graph : Node->GetGraph();
            OutNodeIdentifier = Trimmed;
            OutError.Reset();
            return true;
        }

        if (OutNodeIdentifier.IsEmpty())
        {
            OutNodeIdentifier = Trimmed;
        }
    }

    FString AvailableNodes = DescribeAvailableNodes(OutCandidateGraphs);
    if (OutNodeIdentifier.IsEmpty())
    {
        OutError = TEXT("Node not found");
    }
    else
    {
        OutError = FString::Printf(TEXT("Node '%s' not found"), *OutNodeIdentifier);
    }
    if (!AvailableNodes.IsEmpty())
    {
        OutError += FString::Printf(TEXT(". Available nodes: %s"), *AvailableNodes);
    }
    return false;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::ApplyPinTransform(
    UBlueprint* Blueprint,
    UEdGraphNode* Node,
    const FString& BlueprintName,
    const FString& NodeIdentifier,
    const TArray<FString>& PinNames,
    bool bSplitPins) const
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Result->SetStringField(TEXT("node_id"), NodeIdentifier);
    Result->SetStringField(TEXT("action"), bSplitPins ? TEXT("split_pins") : TEXT("recombine_pins"));
    Result->SetNumberField(TEXT("requested_count"), PinNames.Num());

    TArray<TSharedPtr<FJsonValue>> PinReports;
    int32 FailureCount = 0;
    int32 ChangedCount = 0;

    if (!Blueprint || !Node)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), TEXT("Invalid blueprint or node"));
        Result->SetArrayField(TEXT("pins"), PinReports);
        Result->SetNumberField(TEXT("failure_count"), PinNames.Num());
        return Result;
    }

    UEdGraph* Graph = Node->GetGraph();
    Result->SetStringField(TEXT("graph_name"), Graph ? Graph->GetName() : TEXT(""));

    const UEdGraphSchema_K2* Schema = Graph ? Cast<UEdGraphSchema_K2>(Graph->GetSchema()) : nullptr;
    if (!Schema)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), TEXT("Graph schema is not K2"));
        Result->SetArrayField(TEXT("pins"), PinReports);
        Result->SetNumberField(TEXT("failure_count"), PinNames.Num());
        return Result;
    }

    TUniquePtr<FScopedTransaction> Transaction;
    TSet<FString> SeenPins;

    auto EnsureTransaction = [&]()
    {
        if (!Transaction.IsValid())
        {
            const FText TransactionText = bSplitPins
                ? NSLOCTEXT("VibeUE", "SplitPinsTransaction", "MCP Split Blueprint Pins")
                : NSLOCTEXT("VibeUE", "RecombinePinsTransaction", "MCP Recombine Blueprint Pins");
            Transaction = MakeUnique<FScopedTransaction>(TransactionText);
            if (Graph)
            {
                Graph->Modify();
            }
            Node->Modify();
        }
    };

    for (const FString& RawName : PinNames)
    {
        FString PinName = RawName;
        PinName.TrimStartAndEndInline();
        if (PinName.IsEmpty())
        {
            continue;
        }

        if (SeenPins.Contains(PinName))
        {
            continue;
        }
        SeenPins.Add(PinName);

        TSharedPtr<FJsonObject> PinReport = MakeShared<FJsonObject>();
        PinReport->SetStringField(TEXT("pin_name"), PinName);
        PinReport->SetStringField(TEXT("action"), bSplitPins ? TEXT("split") : TEXT("recombine"));

        UEdGraphPin* Pin = FindPinForOperation(Node, PinName);
        if (!Pin)
        {
            ++FailureCount;
            PinReport->SetStringField(TEXT("status"), TEXT("failed"));
            PinReport->SetStringField(TEXT("message"), TEXT("Pin not found"));
            PinReports.Add(MakeShared<FJsonValueObject>(PinReport));
            continue;
        }

        PinReport->SetStringField(TEXT("pin_id"), VibeUENodeIntrospection::BuildPinIdentifier(Node, Pin));

        const bool bAlreadySplit = Pin->SubPins.Num() > 0;
        const bool bCanSplit = Node->CanSplitPin(Pin);

        if (bSplitPins)
        {
            if (bAlreadySplit)
            {
                PinReport->SetStringField(TEXT("status"), TEXT("noop"));
                PinReport->SetStringField(TEXT("message"), TEXT("Pin already split"));
            }
            else if (!bCanSplit)
            {
                ++FailureCount;
                PinReport->SetStringField(TEXT("status"), TEXT("failed"));
                PinReport->SetStringField(TEXT("message"), TEXT("Pin cannot be split"));
            }
            else
            {
                EnsureTransaction();
                Schema->SplitPin(Pin);
                ++ChangedCount;
                PinReport->SetStringField(TEXT("status"), TEXT("applied"));
                PinReport->SetStringField(TEXT("message"), TEXT("Pin split into sub-pins"));
            }
        }
        else
        {
            UEdGraphPin* ParentPin = Pin->ParentPin ? Pin->ParentPin : Pin;
            if (ParentPin->SubPins.Num() == 0)
            {
                PinReport->SetStringField(TEXT("status"), TEXT("noop"));
                PinReport->SetStringField(TEXT("message"), TEXT("Pin is already recombined"));
            }
            else
            {
                EnsureTransaction();
                Schema->RecombinePin(ParentPin);
                ++ChangedCount;
                PinReport->SetStringField(TEXT("status"), TEXT("applied"));
                PinReport->SetStringField(TEXT("message"), TEXT("Pin recombined"));
            }
        }

        PinReports.Add(MakeShared<FJsonValueObject>(PinReport));
    }

    if (Transaction.IsValid() && ChangedCount > 0)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    }

    const bool bSuccess = FailureCount == 0;
    Result->SetBoolField(TEXT("success"), bSuccess);
    Result->SetArrayField(TEXT("pins"), PinReports);
    Result->SetNumberField(TEXT("changed_count"), ChangedCount);
    Result->SetNumberField(TEXT("failure_count"), FailureCount);
    Result->SetStringField(TEXT("message"), bSuccess ? TEXT("Pin operation completed") : TEXT("Some pins could not be processed"));
    return Result;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleListCustomEvents(const TSharedPtr<FJsonObject>& Params)
{
    // 1. Extract and validate parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }
    
    // 2. Find Blueprint using DiscoveryService
    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return FCommonUtils::CreateErrorResponse(FindResult.GetErrorMessage());
    }
    UBlueprint* Blueprint = FindResult.GetValue();
    
    // 3. Call service method - business logic lives in GraphService
    auto Result = GraphService->ListCustomEvents(Blueprint);
    if (Result.IsError())
    {
        return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
    }
    
    // 4. Convert TResult to JSON response
    const TArray<FString>& EventNames = Result.GetValue();
    
    TArray<TSharedPtr<FJsonValue>> Events;
    for (const FString& EventName : EventNames)
    {
        TSharedPtr<FJsonObject> Evt = MakeShared<FJsonObject>();
        Evt->SetStringField(TEXT("name"), EventName);
        Events.Add(MakeShared<FJsonValueObject>(Evt));
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("events"), Events);
    return ResultObj;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleRefreshBlueprintNode(const TSharedPtr<FJsonObject>& Params)
{
    // 1. Extract and validate parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString NodeId;
    if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
    }

    bool bCompile = true;
    Params->TryGetBoolField(TEXT("compile"), bCompile);

    // 2. Find Blueprint using DiscoveryService
    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return FCommonUtils::CreateErrorResponse(FindResult.GetErrorMessage());
    }
    UBlueprint* Blueprint = FindResult.GetValue();

    // 3. Call service method - business logic lives in the service
    auto Result = NodeService->RefreshNode(Blueprint, NodeId);
    if (Result.IsError())
    {
        return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
    }

    // 4. Compile if requested
    if (bCompile)
    {
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
    }

    // 5. Build JSON response
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Response->SetStringField(TEXT("node_id"), NodeId);
    Response->SetBoolField(TEXT("compiled"), bCompile);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Node '%s' refreshed in Blueprint '%s'"), *NodeId, *BlueprintName));
    return Response;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleRefreshBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    // 1. Extract and validate parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    bool bCompile = true;
    Params->TryGetBoolField(TEXT("compile"), bCompile);

    // 2. Find Blueprint using DiscoveryService
    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return FCommonUtils::CreateErrorResponse(FindResult.GetErrorMessage());
    }
    UBlueprint* Blueprint = FindResult.GetValue();

    // 3. Call service method - business logic lives in the service
    auto Result = NodeService->RefreshAllNodes(Blueprint);
    if (Result.IsError())
    {
        return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
    }

    // 4. Compile if requested
    if (bCompile)
    {
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
    }

    // 5. Build JSON response with graph summaries
    TArray<UEdGraph*> Graphs;
    TArray<TSharedPtr<FJsonValue>> GraphSummaries;
    int32 TotalNodes = 0;

    // Gather graphs for summary
    if (Blueprint->UbergraphPages.Num() > 0)
    {
        Graphs.Append(Blueprint->UbergraphPages);
    }
    Graphs.Append(Blueprint->FunctionGraphs);
    Graphs.Append(Blueprint->MacroGraphs);

    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph)
        {
            continue;
        }

        TotalNodes += Graph->Nodes.Num();

        TSharedPtr<FJsonObject> GraphInfo = MakeShared<FJsonObject>();
        GraphInfo->SetStringField(TEXT("graph_name"), Graph->GetName());
        GraphInfo->SetStringField(TEXT("graph_guid"), Graph->GraphGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
        GraphInfo->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
        GraphSummaries.Add(MakeShared<FJsonValueObject>(GraphInfo));
    }

    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Response->SetNumberField(TEXT("graph_count"), Graphs.Num());
    Response->SetNumberField(TEXT("node_count"), TotalNodes);
    Response->SetBoolField(TEXT("compiled"), bCompile);
    Response->SetArrayField(TEXT("graphs"), GraphSummaries);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Refreshed %d graphs (%d nodes) in Blueprint '%s'"), Graphs.Num(), TotalNodes, *BlueprintName));
    return Response;
}

// NEW: Reflection-based command implementations
TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleGetAvailableBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    if (ReflectionCommands.IsValid())
    {
        return ReflectionCommands->HandleGetAvailableBlueprintNodes(Params);
    }
    return FCommonUtils::CreateErrorResponse(TEXT("Reflection system not initialized"));
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleDiscoverNodesWithDescriptors(const TSharedPtr<FJsonObject>& Params)
{
    if (ReflectionCommands.IsValid())
    {
        return ReflectionCommands->HandleDiscoverNodesWithDescriptors(Params);
    }
    return FCommonUtils::CreateErrorResponse(TEXT("Reflection system not initialized"));
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleAddBlueprintNode(const TSharedPtr<FJsonObject>& Params)
{
    if (ReflectionCommands.IsValid())
    {
        return ReflectionCommands->HandleAddBlueprintNode(Params);
    }
    return FCommonUtils::CreateErrorResponse(TEXT("Reflection system not initialized"));
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleSetBlueprintNodeProperty(const TSharedPtr<FJsonObject>& Params)
{
    if (ReflectionCommands.IsValid())
    {
        return ReflectionCommands->HandleSetBlueprintNodeProperty(Params);
    }
    return FCommonUtils::CreateErrorResponse(TEXT("Reflection system not initialized"));
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleSplitOrRecombinePins(const TSharedPtr<FJsonObject>& Params, bool bSplitPins)
{
    UBlueprint* Blueprint = nullptr;
    UEdGraphNode* Node = nullptr;
    UEdGraph* Graph = nullptr;
    TArray<UEdGraph*> CandidateGraphs;
    FString BlueprintName;
    FString NodeIdentifier;
    FString Error;

    if (!ResolveNodeContext(Params, Blueprint, Node, Graph, CandidateGraphs, BlueprintName, NodeIdentifier, Error))
    {
        return FCommonUtils::CreateErrorResponse(Error);
    }

    auto GatherPins = [&](const TSharedPtr<FJsonObject>& Source, TArray<FString>& OutPins)
    {
        if (!Source.IsValid())
        {
            return;
        }

        const TArray<FString> PinFields = {
            TEXT("pin"), TEXT("pin_name"), TEXT("pin_names"), TEXT("pins"), TEXT("pin_display_name"),
            TEXT("pin_identifier"), TEXT("pin_identifiers"), TEXT("pin_ids")
        };
        CollectStringValues(Source, PinFields, OutPins);

        const TArray<TSharedPtr<FJsonValue>>* PinOperations = nullptr;
        if (Source->TryGetArrayField(TEXT("pin_operations"), PinOperations) && PinOperations)
        {
            for (const TSharedPtr<FJsonValue>& Value : *PinOperations)
            {
                const TSharedPtr<FJsonObject>* OperationObject = nullptr;
                if (!Value.IsValid() || !Value->TryGetObject(OperationObject) || !OperationObject)
                {
                    continue;
                }

                FString Action;
                if ((*OperationObject)->TryGetStringField(TEXT("action"), Action))
                {
                    Action.TrimStartAndEndInline();
                    const bool bActionMatches = bSplitPins
                        ? Action.Equals(TEXT("split"), ESearchCase::IgnoreCase)
                        : (Action.Equals(TEXT("recombine"), ESearchCase::IgnoreCase) || Action.Equals(TEXT("unsplit"), ESearchCase::IgnoreCase));
                    if (!bActionMatches)
                    {
                        continue;
                    }
                }

                const TArray<FString> OperationFields = {TEXT("pin"), TEXT("pin_name"), TEXT("name")};
                CollectStringValues(*OperationObject, OperationFields, OutPins);
            }
        }
    };

    TArray<FString> PinNames;
    GatherPins(Params, PinNames);

    const TSharedPtr<FJsonObject>* Extra = nullptr;
    if (Params->TryGetObjectField(TEXT("extra"), Extra) && Extra)
    {
        GatherPins(*Extra, PinNames);
    }

    const TSharedPtr<FJsonObject>* NodeConfig = nullptr;
    if (Params->TryGetObjectField(TEXT("node_config"), NodeConfig) && NodeConfig)
    {
        GatherPins(*NodeConfig, PinNames);
    }

    if (PinNames.Num() == 0)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("No pin names provided for operation"));
    }

    return ApplyPinTransform(Blueprint, Node, BlueprintName, NodeIdentifier, PinNames, bSplitPins);
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleResetPinDefaults(const TSharedPtr<FJsonObject>& Params)
{
	// Extract and validate parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}
	
	FString NodeId;
	if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
	}
	
	// Find Blueprint using DiscoveryService
	auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
	if (FindResult.IsError())
	{
		return FCommonUtils::CreateErrorResponse(FindResult.GetErrorMessage());
	}
	
	UBlueprint* Blueprint = FindResult.GetValue();
	
	// Call service method to reset pin defaults
	auto Result = NodeService->ResetPinDefaultsAdvanced(Blueprint, NodeId, Params);
	
	if (Result.IsError())
	{
		return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
	}
	
	// Add blueprint name to response
	TSharedPtr<FJsonObject> Response = Result.GetValue();
	Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
	
	return Response;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleConfigureBlueprintNode(const TSharedPtr<FJsonObject>& Params)
{
	// Extract and validate parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}
	
	FString NodeId;
	if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
	}
	
	// Find Blueprint using DiscoveryService
	auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
	if (FindResult.IsError())
	{
		return FCommonUtils::CreateErrorResponse(FindResult.GetErrorMessage());
	}
	
	UBlueprint* Blueprint = FindResult.GetValue();
	
	// Call service method to configure node
	auto Result = NodeService->ConfigureNodeAdvanced(Blueprint, NodeId, Params);
	
	if (Result.IsError())
	{
		return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
	}
	
	// Add blueprint name to response
	TSharedPtr<FJsonObject> Response = Result.GetValue();
	Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
	
	return Response;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleDeleteBlueprintNode(const TSharedPtr<FJsonObject>& Params)
{
    // 1. Extract and validate parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }
    
    FString NodeID;
    if (!Params->TryGetStringField(TEXT("node_id"), NodeID))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
    }
    
    FString GraphName;
    Params->TryGetStringField(TEXT("graph_name"), GraphName);
    
    // 2. Find Blueprint using DiscoveryService
    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return FCommonUtils::CreateErrorResponse(FindResult.GetErrorMessage());
    }
    UBlueprint* Blueprint = FindResult.GetValue();
    
    // 3. Call service method - business logic lives in the service
    auto Result = NodeService->DeleteNode(Blueprint, NodeID, GraphName);
    if (Result.IsError())
    {
        return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
    }
    
    // 4. Compile blueprint
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    
    // 5. Build JSON response
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Response->SetStringField(TEXT("node_guid"), NodeID);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Node '%s' successfully deleted from Blueprint '%s'"), *NodeID, *BlueprintName));
    return Response;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleMoveBlueprintNode(const TSharedPtr<FJsonObject>& Params)
{
    // 1. Extract and validate parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString NodeID;
    if (!Params->TryGetStringField(TEXT("node_id"), NodeID))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
    }

    // Parse position from various possible field formats
    FVector2D NewPosition(0.0f, 0.0f);
    bool bHasPosition = false;

    auto TryLoadPosition = [&](const FString& FieldName) -> bool
    {
        if (Params->HasField(FieldName))
        {
            NewPosition = FCommonUtils::GetVector2DFromJson(Params, FieldName);
            return true;
        }
        return false;
    };

    bHasPosition = TryLoadPosition(TEXT("position")) ||
                   TryLoadPosition(TEXT("node_position")) ||
                   TryLoadPosition(TEXT("new_position"));

    if (!bHasPosition)
    {
        double PosX = 0.0;
        double PosY = 0.0;
        const bool bHasX = Params->TryGetNumberField(TEXT("x"), PosX) || Params->TryGetNumberField(TEXT("pos_x"), PosX);
        const bool bHasY = Params->TryGetNumberField(TEXT("y"), PosY) || Params->TryGetNumberField(TEXT("pos_y"), PosY);

        if (bHasX && bHasY)
        {
            NewPosition.X = static_cast<float>(PosX);
            NewPosition.Y = static_cast<float>(PosY);
            bHasPosition = true;
        }
    }

    if (!bHasPosition)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'position' (array) or 'x'/'y' fields for node move"));
    }

    // 2. Find Blueprint using DiscoveryService
    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return FCommonUtils::CreateErrorResponse(FindResult.GetErrorMessage());
    }
    UBlueprint* Blueprint = FindResult.GetValue();

    // 3. Call service method - business logic lives in the service
    const int32 RoundedX = FMath::RoundToInt(NewPosition.X);
    const int32 RoundedY = FMath::RoundToInt(NewPosition.Y);
    
    auto Result = NodeService->MoveNode(Blueprint, NodeID, RoundedX, RoundedY);
    if (Result.IsError())
    {
        return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
    }

    // 4. Build JSON response
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Response->SetStringField(TEXT("node_id"), NodeID);
    Response->SetNumberField(TEXT("new_x"), RoundedX);
    Response->SetNumberField(TEXT("new_y"), RoundedY);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Node '%s' moved to (%d, %d)"), *NodeID, RoundedX, RoundedY));

    return Response;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleDeleteBlueprintEventNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    FString EventName;
    bool RemoveCustomEventsOnly = true;
    
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing blueprint_name parameter"));
    }
    
    if (!Params->TryGetStringField(TEXT("event_name"), EventName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing event_name parameter"));
    }
    
    // Optional parameter with default
    Params->TryGetBoolField(TEXT("remove_custom_events_only"), RemoveCustomEventsOnly);
    
    // Find the Blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
    }
    
    // Find the Event Graph
    UEdGraph* EventGraph = nullptr;
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph && Graph->GetFName() == "EventGraph")
        {
            EventGraph = Graph;
            break;
        }
    }
    
    if (!EventGraph)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("EventGraph not found in Blueprint"));
    }
    
    // Find the event node
    UK2Node_Event* EventNode = nullptr;
    UK2Node_CustomEvent* CustomEventNode = nullptr;
    FString EventType = TEXT("Unknown");
    
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        if (UK2Node_Event* Event = Cast<UK2Node_Event>(Node))
        {
            FString NodeEventName = Event->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
            if (NodeEventName.Contains(EventName) || Event->EventReference.GetMemberName().ToString() == EventName)
            {
                EventNode = Event;
                
                // Check if it's a custom event
                if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Event))
                {
                    CustomEventNode = CustomEvent;
                    EventType = TEXT("Custom");
                }
                else
                {
                    EventType = TEXT("Engine");
                }
                break;
            }
        }
    }
    
    if (!EventNode)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Event '%s' not found in Blueprint"), *EventName));
    }
    
    // Safety check: Protect engine events if safety is enabled
    if (RemoveCustomEventsOnly && EventType == TEXT("Engine"))
    {
        // Check for protected engine events
        FString EventMemberName = EventNode->EventReference.GetMemberName().ToString();
        if (EventMemberName == TEXT("ReceiveBeginPlay") || 
            EventMemberName == TEXT("ReceiveConstruct") || 
            EventMemberName == TEXT("ReceiveTick") ||
            EventMemberName == TEXT("ReceiveEndPlay") ||
            EventMemberName.StartsWith(TEXT("InputAction")) ||
            EventMemberName.StartsWith(TEXT("InputAxis")))
        {
            return FCommonUtils::CreateErrorResponse(FString::Printf(
                TEXT("Cannot delete protected engine event '%s'. Use remove_custom_events_only=false to override (not recommended)"), 
                *EventName
            ));
        }
    }
    
    // Safety check: Can the user delete this node?
    if (!EventNode->CanUserDeleteNode())
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Event node '%s' cannot be deleted (protected)"), *EventName));
    }
    
    // Collect information about connected nodes
    TArray<TSharedPtr<FJsonValue>> ConnectedNodes;
    
    for (UEdGraphPin* Pin : EventNode->Pins)
    {
        if (Pin && Pin->LinkedTo.Num() > 0)
        {
            for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
            {
                TSharedPtr<FJsonObject> NodeInfo = MakeShareable(new FJsonObject);
                NodeInfo->SetStringField(TEXT("connected_node"), LinkedPin->GetOwningNode()->GetName());
                NodeInfo->SetStringField(TEXT("connected_node_type"), LinkedPin->GetOwningNode()->GetClass()->GetName());
                NodeInfo->SetStringField(TEXT("pin_name"), LinkedPin->PinName.ToString());
                ConnectedNodes.Add(MakeShareable(new FJsonValueObject(NodeInfo)));
            }
            
            // Break all connections
            Pin->BreakAllPinLinks();
        }
    }
    
    // Remove the event node from the graph
    EventGraph->RemoveNode(EventNode, true);
    
    // Mark Blueprint as modified and recompile
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    
    // Create success response
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Result->SetStringField(TEXT("event_name"), EventName);
    Result->SetStringField(TEXT("event_type"), EventType);
    Result->SetBoolField(TEXT("protection_active"), RemoveCustomEventsOnly);
    Result->SetArrayField(TEXT("connected_nodes"), ConnectedNodes);
    Result->SetNumberField(TEXT("connected_nodes_count"), ConnectedNodes.Num());
    Result->SetStringField(TEXT("message"), FString::Printf(
        TEXT("%s event '%s' successfully deleted from Blueprint '%s'"), 
        *EventType, *EventName, *BlueprintName
    ));
    
    // Add safety information
    TSharedPtr<FJsonObject> SafetyInfo = MakeShareable(new FJsonObject);
    SafetyInfo->SetBoolField(TEXT("custom_events_only"), RemoveCustomEventsOnly);
    SafetyInfo->SetBoolField(TEXT("is_custom_event"), EventType == TEXT("Custom"));
    SafetyInfo->SetBoolField(TEXT("is_protected_event"), false);  // If we got here, it wasn't protected
    Result->SetObjectField(TEXT("safety_info"), SafetyInfo);
    
    return Result;
}

UK2Node_FunctionEntry* FBlueprintNodeCommands::FindFunctionEntry(UEdGraph* FunctionGraph) const
{
    if (!FunctionGraph)
    {
        return nullptr;
    }
    
    for (UEdGraphNode* Node : FunctionGraph->Nodes)
    {
        if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
        {
            return Entry;
        }
    }
    
    return nullptr;
}

// ============================================================================
// Component Event Support (Oct 6, 2025) - Reflection-Based Implementation
// ============================================================================

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleCreateComponentEvent(const TSharedPtr<FJsonObject>& Params)
{
    // Extract parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    FString DelegateName;
    
    // Support both direct parameters and nested node_params
    const TSharedPtr<FJsonObject>* NodeParamsPtr = nullptr;
    if (Params->TryGetObjectField(TEXT("node_params"), NodeParamsPtr) && NodeParamsPtr && (*NodeParamsPtr).IsValid())
    {
        const TSharedPtr<FJsonObject>* ComponentEventPtr = nullptr;
        if ((*NodeParamsPtr)->TryGetObjectField(TEXT("component_event"), ComponentEventPtr) && ComponentEventPtr && (*ComponentEventPtr).IsValid())
        {
            (*ComponentEventPtr)->TryGetStringField(TEXT("component_name"), ComponentName);
            (*ComponentEventPtr)->TryGetStringField(TEXT("delegate_name"), DelegateName);
        }
    }
    
    // Fallback to direct parameters
    if (ComponentName.IsEmpty())
    {
        Params->TryGetStringField(TEXT("component_name"), ComponentName);
    }
    if (DelegateName.IsEmpty())
    {
        Params->TryGetStringField(TEXT("delegate_name"), DelegateName);
    }

    if (ComponentName.IsEmpty() || DelegateName.IsEmpty())
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' or 'delegate_name' in node_params.component_event"));
    }

    // Parse position
    FVector2D Position(0.0, 0.0);
    const TArray<TSharedPtr<FJsonValue>>* PositionArray = nullptr;
    if (Params->TryGetArrayField(TEXT("position"), PositionArray) && PositionArray && PositionArray->Num() >= 2)
    {
        Position.X = (*PositionArray)[0]->AsNumber();
        Position.Y = (*PositionArray)[1]->AsNumber();
    }

    // Find Blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Create component event using reflection-based binder
    FString Error;
    UK2Node_ComponentBoundEvent* EventNode = FComponentEventBinder::CreateComponentEvent(
        Blueprint,
        ComponentName,
        DelegateName,
        Position,
        Error
    );

    if (!EventNode)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("Failed to create component event: %s"), *Error));
    }

    // Build success response
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("node_id"), EventNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
    Result->SetStringField(TEXT("component_name"), ComponentName);
    Result->SetStringField(TEXT("delegate_name"), DelegateName);
    Result->SetNumberField(TEXT("pin_count"), EventNode->Pins.Num());
    
    // Add position info
    TArray<TSharedPtr<FJsonValue>> PosArray;
    PosArray.Add(MakeShared<FJsonValueNumber>(EventNode->NodePosX));
    PosArray.Add(MakeShared<FJsonValueNumber>(EventNode->NodePosY));
    Result->SetArrayField(TEXT("position"), PosArray);

    UE_LOG(LogVibeUE, Log, TEXT("Successfully created component event: %s::%s"), *ComponentName, *DelegateName);

    return Result;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleGetComponentEvents(const TSharedPtr<FJsonObject>& Params)
{
    // Extract parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentNameFilter;
    Params->TryGetStringField(TEXT("component_name"), ComponentNameFilter);

    // Find Blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Discover component events using reflection
    TArray<FComponentEventInfo> Events;
    if (!FComponentEventBinder::GetAvailableComponentEvents(Blueprint, ComponentNameFilter, Events))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Failed to enumerate component events"));
    }

    // Build response
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("count"), Events.Num());

    // Group events by component
    TMap<FString, TArray<FComponentEventInfo>> EventsByComponent;
    for (const FComponentEventInfo& EventInfo : Events)
    {
        if (!EventsByComponent.Contains(EventInfo.ComponentName))
        {
            EventsByComponent.Add(EventInfo.ComponentName, TArray<FComponentEventInfo>());
        }
        EventsByComponent[EventInfo.ComponentName].Add(EventInfo);
    }

    // Build components array
    TArray<TSharedPtr<FJsonValue>> ComponentsArray;
    for (const auto& Pair : EventsByComponent)
    {
        TSharedPtr<FJsonObject> ComponentObj = MakeShared<FJsonObject>();
        ComponentObj->SetStringField(TEXT("component_name"), Pair.Key);
        
        if (Pair.Value.Num() > 0)
        {
            ComponentObj->SetStringField(TEXT("component_class"), Pair.Value[0].ComponentClassName);
        }

        // Build events array for this component
        TArray<TSharedPtr<FJsonValue>> EventsArray;
        for (const FComponentEventInfo& EventInfo : Pair.Value)
        {
            TSharedPtr<FJsonObject> EventObj = MakeShared<FJsonObject>();
            EventObj->SetStringField(TEXT("delegate_name"), EventInfo.DelegateName);
            EventObj->SetStringField(TEXT("display_name"), EventInfo.DisplayName);
            EventObj->SetStringField(TEXT("signature"), EventInfo.Signature);

            // Add parameters
            TArray<TSharedPtr<FJsonValue>> ParamsArray;
            for (const FParameterInfo& ParamInfo : EventInfo.Parameters)
            {
                TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
                ParamObj->SetStringField(TEXT("name"), ParamInfo.Name);
                ParamObj->SetStringField(TEXT("type"), ParamInfo.Type);
                ParamObj->SetStringField(TEXT("cpp_type"), ParamInfo.CPPType);
                ParamObj->SetStringField(TEXT("direction"), ParamInfo.Direction);
                ParamsArray.Add(MakeShared<FJsonValueObject>(ParamObj));
            }
            EventObj->SetArrayField(TEXT("parameters"), ParamsArray);

            EventsArray.Add(MakeShared<FJsonValueObject>(EventObj));
        }
        ComponentObj->SetArrayField(TEXT("events"), EventsArray);

        ComponentsArray.Add(MakeShared<FJsonValueObject>(ComponentObj));
    }

    Result->SetArrayField(TEXT("components"), ComponentsArray);

    UE_LOG(LogVibeUE, Log, TEXT("Discovered %d component events across %d components"), 
        Events.Num(), EventsByComponent.Num());

    return Result;
}

// ============================================================================
// Input Key Discovery Support (Oct 6, 2025) - Reflection-Based Implementation
// ============================================================================

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleGetAllInputKeys(const TSharedPtr<FJsonObject>& Params)
{
    // Extract parameters
    FString Category = TEXT("All");
    Params->TryGetStringField(TEXT("category"), Category);

    bool bIncludeDeprecated = false;
    Params->TryGetBoolField(TEXT("include_deprecated"), bIncludeDeprecated);

    // Get input keys using reflection
    TArray<FInputKeyInfo> Keys;
    int32 Count = 0;
    
    if (Category == TEXT("All"))
    {
        Count = FInputKeyEnumerator::GetAllInputKeys(Keys, bIncludeDeprecated);
    }
    else
    {
        Count = FInputKeyEnumerator::GetInputKeysByCategory(Category, Keys);
    }

    // Build response
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("count"), Count);
    Result->SetStringField(TEXT("category"), Category);

    // Build keys array
    TArray<TSharedPtr<FJsonValue>> KeysArray;
    for (const FInputKeyInfo& KeyInfo : Keys)
    {
        TSharedPtr<FJsonObject> KeyObj = MakeShared<FJsonObject>();
        KeyObj->SetStringField(TEXT("key_name"), KeyInfo.KeyName);
        KeyObj->SetStringField(TEXT("display_name"), KeyInfo.DisplayName);
        KeyObj->SetStringField(TEXT("menu_category"), KeyInfo.MenuCategory);
        KeyObj->SetStringField(TEXT("category"), KeyInfo.Category);
        KeyObj->SetBoolField(TEXT("is_gamepad"), KeyInfo.bIsGamepadKey);
        KeyObj->SetBoolField(TEXT("is_mouse"), KeyInfo.bIsMouseButton);
        KeyObj->SetBoolField(TEXT("is_keyboard"), KeyInfo.bIsKeyboard);
        KeyObj->SetBoolField(TEXT("is_modifier"), KeyInfo.bIsModifierKey);
        KeyObj->SetBoolField(TEXT("is_digital"), KeyInfo.bIsDigital);
        KeyObj->SetBoolField(TEXT("is_analog"), KeyInfo.bIsAnalog);
        KeyObj->SetBoolField(TEXT("is_bindable"), KeyInfo.bIsBindableInBlueprints);

        KeysArray.Add(MakeShared<FJsonValueObject>(KeyObj));
    }
    Result->SetArrayField(TEXT("keys"), KeysArray);

    // Add category statistics
    TSharedPtr<FJsonObject> StatsObj = MakeShared<FJsonObject>();
    int32 KeyboardCount = 0, MouseCount = 0, GamepadCount = 0, OtherCount = 0;
    for (const FInputKeyInfo& KeyInfo : Keys)
    {
        if (KeyInfo.bIsGamepadKey) GamepadCount++;
        else if (KeyInfo.bIsMouseButton) MouseCount++;
        else if (KeyInfo.bIsKeyboard) KeyboardCount++;
        else OtherCount++;
    }
    StatsObj->SetNumberField(TEXT("keyboard_keys"), KeyboardCount);
    StatsObj->SetNumberField(TEXT("mouse_keys"), MouseCount);
    StatsObj->SetNumberField(TEXT("gamepad_keys"), GamepadCount);
    StatsObj->SetNumberField(TEXT("other_keys"), OtherCount);
    Result->SetObjectField(TEXT("statistics"), StatsObj);

    UE_LOG(LogVibeUE, Log, TEXT("Discovered %d input keys via reflection (Category: %s)"), Count, *Category);

    return Result;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleCreateInputKeyNode(const TSharedPtr<FJsonObject>& Params)
{
    // Extract parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString KeyName;
    if (!Params->TryGetStringField(TEXT("key_name"), KeyName))
    {
        // Try node_params.spawner_key as fallback
        const TSharedPtr<FJsonObject>* NodeParamsPtr = nullptr;
        if (Params->TryGetObjectField(TEXT("node_params"), NodeParamsPtr) && NodeParamsPtr && (*NodeParamsPtr).IsValid())
        {
            (*NodeParamsPtr)->TryGetStringField(TEXT("spawner_key"), KeyName);
        }
    }

    if (KeyName.IsEmpty())
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'key_name' parameter"));
    }

    // Parse position
    FVector2D Position(0.0, 0.0);
    const TArray<TSharedPtr<FJsonValue>>* PositionArray = nullptr;
    if (Params->TryGetArrayField(TEXT("position"), PositionArray) && PositionArray && PositionArray->Num() >= 2)
    {
        Position.X = (*PositionArray)[0]->AsNumber();
        Position.Y = (*PositionArray)[1]->AsNumber();
    }

    // Find Blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Find the key using reflection
    FInputKeyInfo KeyInfo;
    if (!FInputKeyEnumerator::FindInputKey(KeyName, KeyInfo))
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("Input key '%s' not found. Use get_all_input_keys to discover available keys."), *KeyName));
    }

    // Create input key node
    FString Error;
    UK2Node_InputKey* InputKeyNode = FInputKeyEnumerator::CreateInputKeyNode(
        Blueprint,
        KeyInfo.Key,
        Position,
        Error
    );

    if (!InputKeyNode)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("Failed to create input key node: %s"), *Error));
    }

    // Build success response
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("node_id"), InputKeyNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
    Result->SetStringField(TEXT("key_name"), KeyInfo.KeyName);
    Result->SetStringField(TEXT("display_name"), KeyInfo.DisplayName);
    Result->SetNumberField(TEXT("pin_count"), InputKeyNode->Pins.Num());
    
    // Add position info
    TArray<TSharedPtr<FJsonValue>> PosArray;
    PosArray.Add(MakeShared<FJsonValueNumber>(InputKeyNode->NodePosX));
    PosArray.Add(MakeShared<FJsonValueNumber>(InputKeyNode->NodePosY));
    Result->SetArrayField(TEXT("position"), PosArray);

    UE_LOG(LogVibeUE, Log, TEXT("Successfully created input key node for key: %s"), *KeyInfo.KeyName);

    return Result;
}
