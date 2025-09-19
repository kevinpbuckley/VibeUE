#include "Commands/VibeUECommonUtils.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "WidgetBlueprint.h"
#include "EditorAssetLibrary.h"
#include "UObject/UObjectGlobals.h"
#include "HAL/Platform.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Selection.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "BlueprintNodeSpawner.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "MessageLogModule.h"
#include "IMessageLogListing.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "WidgetBlueprintEditor.h"

// JSON Utilities
TSharedPtr<FJsonObject> FVibeUECommonUtils::CreateErrorResponse(const FString& Message)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), false);
    ResponseObject->SetStringField(TEXT("error"), Message);
    return ResponseObject;
}

TSharedPtr<FJsonObject> FVibeUECommonUtils::CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), true);
    
    if (Data.IsValid())
    {
        ResponseObject->SetObjectField(TEXT("data"), Data);
    }
    
    return ResponseObject;
}

void FVibeUECommonUtils::GetIntArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<int32>& OutArray)
{
    OutArray.Reset();
    
    if (!JsonObject->HasField(FieldName))
    {
        return;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
        {
            OutArray.Add((int32)Value->AsNumber());
        }
    }
}

void FVibeUECommonUtils::GetFloatArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<float>& OutArray)
{
    OutArray.Reset();
    
    if (!JsonObject->HasField(FieldName))
    {
        return;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
        {
            OutArray.Add((float)Value->AsNumber());
        }
    }
}

FVector2D FVibeUECommonUtils::GetVector2DFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FVector2D Result(0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 2)
    {
        Result.X = (float)(*JsonArray)[0]->AsNumber();
        Result.Y = (float)(*JsonArray)[1]->AsNumber();
    }
    
    return Result;
}

FVector FVibeUECommonUtils::GetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FVector Result(0.0f, 0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.X = (float)(*JsonArray)[0]->AsNumber();
        Result.Y = (float)(*JsonArray)[1]->AsNumber();
        Result.Z = (float)(*JsonArray)[2]->AsNumber();
    }
    
    return Result;
}

FRotator FVibeUECommonUtils::GetRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FRotator Result(0.0f, 0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.Pitch = (float)(*JsonArray)[0]->AsNumber();
        Result.Yaw = (float)(*JsonArray)[1]->AsNumber();
        Result.Roll = (float)(*JsonArray)[2]->AsNumber();
    }
    
    return Result;
}

// Blueprint Utilities
UBlueprint* FVibeUECommonUtils::FindBlueprint(const FString& BlueprintName)
{
    return FindBlueprintByName(BlueprintName);
}

UBlueprint* FVibeUECommonUtils::FindBlueprintByName(const FString& BlueprintName)
{
    // First try direct path loading for exact matches
    UBlueprint* DirectLoad = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintName));
    if (DirectLoad)
    {
        return DirectLoad;
    }
    
    // Try legacy path for backwards compatibility
    FString LegacyAssetPath = TEXT("/Game/Blueprints/") + BlueprintName;
    UBlueprint* LegacyLoad = LoadObject<UBlueprint>(nullptr, *LegacyAssetPath);
    if (LegacyLoad)
    {
        return LegacyLoad;
    }
    
    // Use Asset Registry for recursive search like Unreal's UI
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    // Create filter to find all Blueprints (including Widget Blueprints)
    FARFilter Filter;
    Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
    Filter.ClassPaths.Add(UWidgetBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;
    Filter.PackagePaths.Add("/Game"); // Search recursively from /Game
    
    TArray<FAssetData> AssetDataList;
    AssetRegistry.GetAssets(Filter, AssetDataList);
    
    // Search for matching blueprint name (case-insensitive)
    for (const FAssetData& AssetData : AssetDataList)
    {
        FString AssetName = AssetData.AssetName.ToString();
        if (AssetName.Equals(BlueprintName, ESearchCase::IgnoreCase))
        {
            UBlueprint* FoundBlueprint = Cast<UBlueprint>(AssetData.GetAsset());
            if (FoundBlueprint)
            {
                return FoundBlueprint;
            }
        }
    }
    
    return nullptr;
}

UEdGraph* FVibeUECommonUtils::FindOrCreateEventGraph(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return nullptr;
    }
    
    // Try to find the event graph
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph->GetName().Contains(TEXT("EventGraph")))
        {
            return Graph;
        }
    }
    
    // Create a new event graph if none exists
    UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FName(TEXT("EventGraph")), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(Blueprint, NewGraph);
    return NewGraph;
}

// Safely compile a blueprint and return an error string on failure
bool FVibeUECommonUtils::SafeCompileBlueprint(UBlueprint* Blueprint, FString& OutError)
{
    OutError.Empty();
    if (!Blueprint || !IsValid(Blueprint))
    {
        OutError = TEXT("Invalid blueprint pointer");
        UE_LOG(LogTemp, Error, TEXT("MCP: SafeCompileBlueprint - Invalid blueprint pointer"));
        return false;
    }

    bool bSuccess = true;
    try
    {
        // Clear any existing compile errors first
        Blueprint->Status = BS_Dirty;
        
        // Use the editor utilities to compile the blueprint with full compilation
        FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
        
        // Log the blueprint status for debugging
        UE_LOG(LogTemp, Warning, TEXT("MCP: Blueprint %s status after compilation: %d"), *Blueprint->GetName(), (int32)Blueprint->Status);
        
        // Check the blueprint status after compilation to determine success/failure
        if (Blueprint->Status == BS_Error)
        {
            OutError = FString::Printf(TEXT("Blueprint compilation failed with error status for %s"), *Blueprint->GetName());
            bSuccess = false;
        }
        else if (Blueprint->Status == BS_UpToDateWithWarnings)
        {
            // Still considered successful but log the warnings
            UE_LOG(LogTemp, Warning, TEXT("MCP: Blueprint compiled with warnings: %s"), *Blueprint->GetName());
        }
        else if (Blueprint->Status == BS_Unknown || Blueprint->Status == BS_Dirty)
        {
            // If status is still unknown or dirty, something went wrong
            OutError = FString::Printf(TEXT("Blueprint compilation did not complete properly for %s (status: %d)"), *Blueprint->GetName(), (int32)Blueprint->Status);
            bSuccess = false;
        }
    }
    catch (const std::exception& e)
    {
        OutError = FString::Printf(TEXT("Exception during blueprint compilation: %s"), UTF8_TO_TCHAR(e.what()));
        UE_LOG(LogTemp, Error, TEXT("MCP: %s"), *OutError);
        bSuccess = false;
    }
    catch (...)
    {
        OutError = TEXT("Unknown exception during blueprint compilation");
        UE_LOG(LogTemp, Error, TEXT("MCP: %s"), *OutError);
        bSuccess = false;
    }

    // After compilation, collect any MessageLog entries for the Blueprint log so we can return
    // the same diagnostics the Editor UI shows.
    // Look up the MessageLog module and retrieve messages for the 'Blueprint' log.
    // Use safer approach to avoid access violations
    try
    {
        if (FModuleManager::Get().IsModuleLoaded("MessageLog"))
        {
            FMessageLogModule* MessageLogModule = FModuleManager::GetModulePtr<FMessageLogModule>("MessageLog");
            if (MessageLogModule && IsValid(Blueprint))
            {
                // The log name used by blueprint compilation is usually "Blueprint" or "BlueprintLog" depending on context.
                const FName LogNamesToCheck[] = { FName(TEXT("Blueprint")), FName(TEXT("BlueprintLog")) };

                FString CollectedMessages;
                for (const FName& LogName : LogNamesToCheck)
                {
                    if (!MessageLogModule->IsRegisteredLogListing(LogName))
                    {
                        continue;
                    }

                    try
                    {
                        // Use GetLogListing but with proper error handling
                        // TSharedRef is always valid, so no need to check IsValid()
                        TSharedRef<IMessageLogListing> Listing = MessageLogModule->GetLogListing(LogName);
                        FString All = Listing->GetAllMessagesAsString();
                        if (!All.IsEmpty())
                        {
                            if (!CollectedMessages.IsEmpty())
                            {
                                CollectedMessages += TEXT("\n");
                            }
                            CollectedMessages += All;
                        }
                    }
                    catch (...)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("MCP: Exception while accessing message log '%s'"), *LogName.ToString());
                        continue;
                    }
                }

                if (!CollectedMessages.IsEmpty())
                {
                    // Always append collected messages to provide detailed error information
                    if (!OutError.IsEmpty())
                    {
                        OutError += TEXT("\n");
                    }
                    OutError += CollectedMessages;
                    
                    // If we haven't already determined failure from status, check messages for specific errors
                    if (bSuccess && (CollectedMessages.Contains(TEXT("Error:")) || 
                                   CollectedMessages.Contains(TEXT("required widget binding")) ||
                                   CollectedMessages.Contains(TEXT("BindWidget")) ||
                                   CollectedMessages.Contains(TEXT("was not found"))))
                    {
                        bSuccess = false;
                        UE_LOG(LogTemp, Error, TEXT("MCP: Found compilation errors in message log for %s"), *Blueprint->GetName());
                    }
                }
            }
        }
    }
    catch (...)
    {
        UE_LOG(LogTemp, Warning, TEXT("MCP: Exception while collecting message log information"));
    }

    return bSuccess;
}

// Blueprint node utilities
UK2Node_Event* FVibeUECommonUtils::CreateEventNode(UEdGraph* Graph, const FString& EventName, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
    if (!Blueprint)
    {
        return nullptr;
    }
    
    // Check for existing event node with this exact name
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
        if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
        {
            UE_LOG(LogTemp, Display, TEXT("Using existing event node with name %s (ID: %s)"), 
                *EventName, *EventNode->NodeGuid.ToString());
            return EventNode;
        }
    }

    // No existing node found, create a new one
    UK2Node_Event* EventNode = nullptr;
    
    // Find the function to create the event
    UClass* BlueprintClass = Blueprint->GeneratedClass;
    UFunction* EventFunction = BlueprintClass->FindFunctionByName(FName(*EventName));
    
    if (EventFunction)
    {
        EventNode = NewObject<UK2Node_Event>(Graph);
        EventNode->EventReference.SetExternalMember(FName(*EventName), BlueprintClass);
        EventNode->NodePosX = Position.X;
        EventNode->NodePosY = Position.Y;
        Graph->AddNode(EventNode, true);
        EventNode->PostPlacedNewNode();
        EventNode->AllocateDefaultPins();
        UE_LOG(LogTemp, Display, TEXT("Created new event node with name %s (ID: %s)"), 
            *EventName, *EventNode->NodeGuid.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to find function for event name: %s"), *EventName);
    }
    
    return EventNode;
}

UK2Node_CallFunction* FVibeUECommonUtils::CreateFunctionCallNode(UEdGraph* Graph, UFunction* Function, const FVector2D& Position)
{
    if (!Graph || !Function)
    {
        return nullptr;
    }
    
    UK2Node_CallFunction* FunctionNode = NewObject<UK2Node_CallFunction>(Graph);
    FunctionNode->SetFromFunction(Function);
    FunctionNode->NodePosX = Position.X;
    FunctionNode->NodePosY = Position.Y;
    Graph->AddNode(FunctionNode, true);
    FunctionNode->CreateNewGuid();
    FunctionNode->PostPlacedNewNode();
    FunctionNode->AllocateDefaultPins();
    
    return FunctionNode;
}

UK2Node_VariableGet* FVibeUECommonUtils::CreateVariableGetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position)
{
    if (!Graph || !Blueprint)
    {
        return nullptr;
    }
    
    UK2Node_VariableGet* VariableGetNode = NewObject<UK2Node_VariableGet>(Graph);
    
    FName VarName(*VariableName);
    FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName);
    
    if (Property)
    {
        VariableGetNode->VariableReference.SetFromField<FProperty>(Property, false);
        VariableGetNode->NodePosX = Position.X;
        VariableGetNode->NodePosY = Position.Y;
        Graph->AddNode(VariableGetNode, true);
        VariableGetNode->PostPlacedNewNode();
        VariableGetNode->AllocateDefaultPins();
        
        return VariableGetNode;
    }
    
    return nullptr;
}

UK2Node_VariableSet* FVibeUECommonUtils::CreateVariableSetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position)
{
    if (!Graph || !Blueprint)
    {
        return nullptr;
    }
    
    UK2Node_VariableSet* VariableSetNode = NewObject<UK2Node_VariableSet>(Graph);
    
    FName VarName(*VariableName);
    FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName);
    
    if (Property)
    {
        VariableSetNode->VariableReference.SetFromField<FProperty>(Property, false);
        VariableSetNode->NodePosX = Position.X;
        VariableSetNode->NodePosY = Position.Y;
        Graph->AddNode(VariableSetNode, true);
        VariableSetNode->PostPlacedNewNode();
        VariableSetNode->AllocateDefaultPins();
        
        return VariableSetNode;
    }
    
    return nullptr;
}

UK2Node_InputAction* FVibeUECommonUtils::CreateInputActionNode(UEdGraph* Graph, const FString& ActionName, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UK2Node_InputAction* InputActionNode = NewObject<UK2Node_InputAction>(Graph);
    InputActionNode->InputActionName = FName(*ActionName);
    InputActionNode->NodePosX = Position.X;
    InputActionNode->NodePosY = Position.Y;
    Graph->AddNode(InputActionNode, true);
    InputActionNode->CreateNewGuid();
    InputActionNode->PostPlacedNewNode();
    InputActionNode->AllocateDefaultPins();
    
    return InputActionNode;
}

UK2Node_Self* FVibeUECommonUtils::CreateSelfReferenceNode(UEdGraph* Graph, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UK2Node_Self* SelfNode = NewObject<UK2Node_Self>(Graph);
    SelfNode->NodePosX = Position.X;
    SelfNode->NodePosY = Position.Y;
    Graph->AddNode(SelfNode, true);
    SelfNode->CreateNewGuid();
    SelfNode->PostPlacedNewNode();
    SelfNode->AllocateDefaultPins();
    
    return SelfNode;
}

bool FVibeUECommonUtils::ConnectGraphNodes(UEdGraph* Graph, UEdGraphNode* SourceNode, const FString& SourcePinName, 
                                           UEdGraphNode* TargetNode, const FString& TargetPinName)
{
    UE_LOG(LogTemp, Warning, TEXT("ConnectGraphNodes: Starting connection attempt"));
    UE_LOG(LogTemp, Warning, TEXT("  Source Node: %s, Pin: %s"), 
           SourceNode ? *SourceNode->GetName() : TEXT("NULL"), *SourcePinName);
    UE_LOG(LogTemp, Warning, TEXT("  Target Node: %s, Pin: %s"), 
           TargetNode ? *TargetNode->GetName() : TEXT("NULL"), *TargetPinName);
    
    if (!Graph || !SourceNode || !TargetNode)
    {
        UE_LOG(LogTemp, Warning, TEXT("ConnectGraphNodes: Null parameter check failed"));
        return false;
    }
    
    UEdGraphPin* SourcePin = FindPin(SourceNode, SourcePinName, EGPD_Output);
    UEdGraphPin* TargetPin = FindPin(TargetNode, TargetPinName, EGPD_Input);
    
    UE_LOG(LogTemp, Warning, TEXT("ConnectGraphNodes: SourcePin=%s, TargetPin=%s"), 
           SourcePin ? TEXT("Found") : TEXT("NULL"), TargetPin ? TEXT("Found") : TEXT("NULL"));
    
    if (SourcePin && TargetPin)
    {
        UE_LOG(LogTemp, Warning, TEXT("ConnectGraphNodes: Making connection..."));
        SourcePin->MakeLinkTo(TargetPin);
        UE_LOG(LogTemp, Warning, TEXT("ConnectGraphNodes: Connection successful!"));
        return true;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("ConnectGraphNodes: Failed to find valid pins"));
    return false;
}

UEdGraphPin* FVibeUECommonUtils::FindPin(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction)
{
    if (!Node)
    {
        return nullptr;
    }
    
    // Log all pins for debugging
    UE_LOG(LogTemp, Display, TEXT("FindPin: Looking for pin '%s' (Direction: %d) in node '%s'"), 
           *PinName, (int32)Direction, *Node->GetName());
    
    for (UEdGraphPin* Pin : Node->Pins)
    {
        UE_LOG(LogTemp, Display, TEXT("  - Available pin: '%s', Direction: %d, Category: %s"), 
               *Pin->PinName.ToString(), (int32)Pin->Direction, *Pin->PinType.PinCategory.ToString());
    }
    
    // First try exact match
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString() == PinName && (Direction == EGPD_MAX || Pin->Direction == Direction))
        {
            UE_LOG(LogTemp, Display, TEXT("  - Found exact matching pin: '%s'"), *Pin->PinName.ToString());
            return Pin;
        }
    }
    
    // If no exact match and we're looking for a component reference, try case-insensitive match
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase) && 
            (Direction == EGPD_MAX || Pin->Direction == Direction))
        {
            UE_LOG(LogTemp, Display, TEXT("  - Found case-insensitive matching pin: '%s'"), *Pin->PinName.ToString());
            return Pin;
        }
    }
    
    // If we're looking for a component output and didn't find it by name, try to find the first data output pin
    if (Direction == EGPD_Output && Cast<UK2Node_VariableGet>(Node) != nullptr)
    {
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
            {
                UE_LOG(LogTemp, Display, TEXT("  - Found fallback data output pin: '%s'"), *Pin->PinName.ToString());
                return Pin;
            }
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("  - No matching pin found for '%s'"), *PinName);
    return nullptr;
}

// Enhanced connection with reflection-based pin discovery and validation
TSharedPtr<FJsonObject> FVibeUECommonUtils::ConnectGraphNodesWithReflection(UEdGraph* Graph, UEdGraphNode* SourceNode, 
                                                                              const FString& SourcePinName, UEdGraphNode* TargetNode, 
                                                                              const FString& TargetPinName)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    
    if (!Graph || !SourceNode || !TargetNode)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Invalid parameters provided"));
        return Result;
    }
    
    // Enhanced pin discovery
    UEdGraphPin* SourcePin = FindPin(SourceNode, SourcePinName, EGPD_Output);
    UEdGraphPin* TargetPin = FindPin(TargetNode, TargetPinName, EGPD_Input);
    
    // If exact pins not found, suggest alternatives
    TSharedPtr<FJsonObject> PinInfo = MakeShared<FJsonObject>();
    
    if (!SourcePin)
    {
        FString BestMatch = SuggestBestPinMatch(SourceNode, SourcePinName, EGPD_Output);
        PinInfo->SetStringField(TEXT("suggested_source_pin"), BestMatch);
        
        TArray<FString> AvailablePins = GetAvailablePinNames(SourceNode, EGPD_Output);
        FString PinsListString = FString::Join(AvailablePins, TEXT(", "));
        PinInfo->SetStringField(TEXT("available_source_pins"), PinsListString);
    }
    
    if (!TargetPin)
    {
        FString BestMatch = SuggestBestPinMatch(TargetNode, TargetPinName, EGPD_Input);
        PinInfo->SetStringField(TEXT("suggested_target_pin"), BestMatch);
        
        TArray<FString> AvailablePins = GetAvailablePinNames(TargetNode, EGPD_Input);
        FString PinsListString = FString::Join(AvailablePins, TEXT(", "));
        PinInfo->SetStringField(TEXT("available_target_pins"), PinsListString);
    }
    
    // If either pin not found, return helpful error with suggestions
    if (!SourcePin || !TargetPin)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Pin(s) not found - Source: %s, Target: %s"), 
                                                                SourcePin ? TEXT("Found") : TEXT("Missing"),
                                                                TargetPin ? TEXT("Found") : TEXT("Missing")));
        Result->SetObjectField(TEXT("pin_suggestions"), PinInfo);
        return Result;
    }
    
    // Validate pin connection compatibility
    if (!ValidatePinConnection(SourcePin, TargetPin))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Incompatible pin types - Source: %s (%s), Target: %s (%s)"),
                                                                *SourcePin->PinName.ToString(), *SourcePin->PinType.PinCategory.ToString(),
                                                                *TargetPin->PinName.ToString(), *TargetPin->PinType.PinCategory.ToString()));
        return Result;
    }
    
    // Make the connection
    SourcePin->MakeLinkTo(TargetPin);
    
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("source_node_id"), SourceNode->NodeGuid.ToString());
    Result->SetStringField(TEXT("target_node_id"), TargetNode->NodeGuid.ToString());
    Result->SetStringField(TEXT("source_pin"), SourcePin->PinName.ToString());
    Result->SetStringField(TEXT("target_pin"), TargetPin->PinName.ToString());
    Result->SetStringField(TEXT("connection_type"), SourcePin->PinType.PinCategory.ToString());
    
    UE_LOG(LogTemp, Log, TEXT("Enhanced connection successful: %s[%s] -> %s[%s]"), 
           *SourceNode->GetName(), *SourcePin->PinName.ToString(),
           *TargetNode->GetName(), *TargetPin->PinName.ToString());
    
    return Result;
}

// Get available pin names for a node
TArray<FString> FVibeUECommonUtils::GetAvailablePinNames(UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
    TArray<FString> PinNames;
    
    if (!Node)
    {
        return PinNames;
    }
    
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Direction == EGPD_MAX || Pin->Direction == Direction)
        {
            PinNames.Add(Pin->PinName.ToString());
        }
    }
    
    return PinNames;
}

// Validate if two pins can be connected
bool FVibeUECommonUtils::ValidatePinConnection(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin)
{
    if (!SourcePin || !TargetPin)
    {
        return false;
    }
    
    // Check direction compatibility
    if (SourcePin->Direction != EGPD_Output || TargetPin->Direction != EGPD_Input)
    {
        return false;
    }
    
    // Check if target pin already has a connection (some pins allow multiple connections)
    if (TargetPin->LinkedTo.Num() > 0 && TargetPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
    {
        // Most data pins only allow one connection, exec pins can have multiple
        return false;
    }
    
    // Basic type compatibility check
    const UEdGraphSchema* Schema = SourcePin->GetSchema();
    if (Schema)
    {
        FPinConnectionResponse Response = Schema->CanCreateConnection(SourcePin, TargetPin);
        return Response.Response == CONNECT_RESPONSE_MAKE;
    }
    
    return true;
}

// Suggest best pin match using fuzzy matching
FString FVibeUECommonUtils::SuggestBestPinMatch(UEdGraphNode* Node, const FString& RequestedPinName, EEdGraphPinDirection Direction)
{
    if (!Node)
    {
        return FString();
    }
    
    TArray<FString> AvailablePins = GetAvailablePinNames(Node, Direction);
    
    if (AvailablePins.Num() == 0)
    {
        return FString();
    }
    
    // Common pin name mappings for better user experience
    static TMap<FString, TArray<FString>> PinAliases = {
        {TEXT("exec"), {TEXT("execute"), TEXT("then"), TEXT("output")}},
        {TEXT("execute"), {TEXT("exec"), TEXT("then"), TEXT("input")}}, 
        {TEXT("then"), {TEXT("exec"), TEXT("execute"), TEXT("output")}},
        {TEXT("return"), {TEXT("ReturnValue"), TEXT("Return Value"), TEXT("output")}},
        {TEXT("ReturnValue"), {TEXT("return"), TEXT("Return Value"), TEXT("output")}},
        {TEXT("target"), {TEXT("Target"), TEXT("self"), TEXT("Self")}}
    };
    
    // First try exact match (case insensitive)
    for (const FString& PinName : AvailablePins)
    {
        if (PinName.Equals(RequestedPinName, ESearchCase::IgnoreCase))
        {
            return PinName;
        }
    }
    
    // Try alias matching
    if (PinAliases.Contains(RequestedPinName.ToLower()))
    {
        const TArray<FString>& Aliases = PinAliases[RequestedPinName.ToLower()];
        for (const FString& Alias : Aliases)
        {
            for (const FString& PinName : AvailablePins)
            {
                if (PinName.Equals(Alias, ESearchCase::IgnoreCase))
                {
                    return PinName;
                }
            }
        }
    }
    
    // Try partial matching (contains)
    for (const FString& PinName : AvailablePins)
    {
        if (PinName.Contains(RequestedPinName, ESearchCase::IgnoreCase) || 
            RequestedPinName.Contains(PinName, ESearchCase::IgnoreCase))
        {
            return PinName;
        }
    }
    
    // If no good match found, return first available pin of the right direction
    return AvailablePins.Num() > 0 ? AvailablePins[0] : FString();
}

// Actor utilities
TSharedPtr<FJsonValue> FVibeUECommonUtils::ActorToJson(AActor* Actor)
{
    if (!Actor)
    {
        return MakeShared<FJsonValueNull>();
    }
    
    TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
    ActorObject->SetStringField(TEXT("name"), Actor->GetName());
    ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
    
    FVector Location = Actor->GetActorLocation();
    TArray<TSharedPtr<FJsonValue>> LocationArray;
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.X));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
    ActorObject->SetArrayField(TEXT("location"), LocationArray);
    
    FRotator Rotation = Actor->GetActorRotation();
    TArray<TSharedPtr<FJsonValue>> RotationArray;
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    ActorObject->SetArrayField(TEXT("rotation"), RotationArray);
    
    FVector Scale = Actor->GetActorScale3D();
    TArray<TSharedPtr<FJsonValue>> ScaleArray;
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
    ActorObject->SetArrayField(TEXT("scale"), ScaleArray);
    
    return MakeShared<FJsonValueObject>(ActorObject);
}

TSharedPtr<FJsonObject> FVibeUECommonUtils::ActorToJsonObject(AActor* Actor, bool bDetailed)
{
    if (!Actor)
    {
        return nullptr;
    }
    
    TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
    ActorObject->SetStringField(TEXT("name"), Actor->GetName());
    ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
    
    FVector Location = Actor->GetActorLocation();
    TArray<TSharedPtr<FJsonValue>> LocationArray;
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.X));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
    ActorObject->SetArrayField(TEXT("location"), LocationArray);
    
    FRotator Rotation = Actor->GetActorRotation();
    TArray<TSharedPtr<FJsonValue>> RotationArray;
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    ActorObject->SetArrayField(TEXT("rotation"), RotationArray);
    
    FVector Scale = Actor->GetActorScale3D();
    TArray<TSharedPtr<FJsonValue>> ScaleArray;
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
    ActorObject->SetArrayField(TEXT("scale"), ScaleArray);
    
    return ActorObject;
}

UK2Node_Event* FVibeUECommonUtils::FindExistingEventNode(UEdGraph* Graph, const FString& EventName)
{
    if (!Graph)
    {
        return nullptr;
    }

    // Look for existing event nodes
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
        if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
        {
            UE_LOG(LogTemp, Display, TEXT("Found existing event node with name: %s"), *EventName);
            return EventNode;
        }
    }

    return nullptr;
}

bool FVibeUECommonUtils::SetObjectProperty(UObject* Object, const FString& PropertyName, 
                                     const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage)
{
    if (!Object)
    {
        OutErrorMessage = TEXT("Invalid object");
        return false;
    }

    FProperty* Property = Object->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        OutErrorMessage = FString::Printf(TEXT("Property not found: %s"), *PropertyName);
        return false;
    }

    void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Object);
    
    // Handle different property types
    if (Property->IsA<FBoolProperty>())
    {
        ((FBoolProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsBool());
        return true;
    }
    else if (Property->IsA<FIntProperty>())
    {
        int32 IntValue = static_cast<int32>(Value->AsNumber());
        FIntProperty* IntProperty = CastField<FIntProperty>(Property);
        if (IntProperty)
        {
            IntProperty->SetPropertyValue_InContainer(Object, IntValue);
            return true;
        }
    }
    else if (Property->IsA<FFloatProperty>())
    {
        ((FFloatProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsNumber());
        return true;
    }
    else if (Property->IsA<FStrProperty>())
    {
        ((FStrProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsString());
        return true;
    }
    else if (Property->IsA<FByteProperty>())
    {
        FByteProperty* ByteProp = CastField<FByteProperty>(Property);
        UEnum* EnumDef = ByteProp ? ByteProp->GetIntPropertyEnum() : nullptr;
        
        // If this is a TEnumAsByte property (has associated enum)
        if (EnumDef)
        {
            // Handle numeric value
            if (Value->Type == EJson::Number)
            {
                uint8 ByteValue = static_cast<uint8>(Value->AsNumber());
                ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
                
                UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric value: %d"), 
                      *PropertyName, ByteValue);
                return true;
            }
            // Handle string enum value
            else if (Value->Type == EJson::String)
            {
                FString EnumValueName = Value->AsString();
                
                // Try to convert numeric string to number first
                if (EnumValueName.IsNumeric())
                {
                    uint8 ByteValue = FCString::Atoi(*EnumValueName);
                    ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric string value: %s -> %d"), 
                          *PropertyName, *EnumValueName, ByteValue);
                    return true;
                }
                
                // Handle qualified enum names (e.g., "Player0" or "EAutoReceiveInput::Player0")
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    // Try with full name as fallback
                    EnumValue = EnumDef->GetValueByNameString(Value->AsString());
                }
                
                if (EnumValue != INDEX_NONE)
                {
                    ByteProp->SetPropertyValue(PropertyAddr, static_cast<uint8>(EnumValue));
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to name value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                else
                {
                    // Log all possible enum values for debugging
                    UE_LOG(LogTemp, Warning, TEXT("Could not find enum value for '%s'. Available options:"), *EnumValueName);
                    for (int32 i = 0; i < EnumDef->NumEnums(); i++)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("  - %s (value: %d)"), 
                               *EnumDef->GetNameStringByIndex(i), EnumDef->GetValueByIndex(i));
                    }
                    
                    OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
                    return false;
                }
            }
        }
        else
        {
            // Regular byte property
            uint8 ByteValue = static_cast<uint8>(Value->AsNumber());
            ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
            return true;
        }
    }
    else if (Property->IsA<FEnumProperty>())
    {
        FEnumProperty* EnumProp = CastField<FEnumProperty>(Property);
        UEnum* EnumDef = EnumProp ? EnumProp->GetEnum() : nullptr;
        FNumericProperty* UnderlyingNumericProp = EnumProp ? EnumProp->GetUnderlyingProperty() : nullptr;
        
        if (EnumDef && UnderlyingNumericProp)
        {
            // Handle numeric value
            if (Value->Type == EJson::Number)
            {
                int64 EnumValue = static_cast<int64>(Value->AsNumber());
                UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                
                UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric value: %lld"), 
                      *PropertyName, EnumValue);
                return true;
            }
            // Handle string enum value
            else if (Value->Type == EJson::String)
            {
                FString EnumValueName = Value->AsString();
                
                // Try to convert numeric string to number first
                if (EnumValueName.IsNumeric())
                {
                    int64 EnumValue = FCString::Atoi64(*EnumValueName);
                    UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric string value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                
                // Handle qualified enum names
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    // Try with full name as fallback
                    EnumValue = EnumDef->GetValueByNameString(Value->AsString());
                }
                
                if (EnumValue != INDEX_NONE)
                {
                    UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to name value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                else
                {
                    // Log all possible enum values for debugging
                    UE_LOG(LogTemp, Warning, TEXT("Could not find enum value for '%s'. Available options:"), *EnumValueName);
                    for (int32 i = 0; i < EnumDef->NumEnums(); i++)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("  - %s (value: %d)"), 
                               *EnumDef->GetNameStringByIndex(i), EnumDef->GetValueByIndex(i));
                    }
                    
                    OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
                    return false;
                }
            }
        }
    }
    
    OutErrorMessage = FString::Printf(TEXT("Unsupported property type: %s for property %s"), 
                                    *Property->GetClass()->GetName(), *PropertyName);
    return false;
}

UWidgetBlueprint* FVibeUECommonUtils::FindWidgetBlueprint(const FString& WidgetBlueprintName)
{
    // Check if we're in a serialization context to prevent crashes
    if (IsGarbageCollecting() || GIsSavingPackage || IsLoading())
    {
        UE_LOG(LogTemp, Warning, TEXT("FindWidgetBlueprint: Cannot find widget during serialization"));
        return nullptr;
    }

    UE_LOG(LogTemp, Warning, TEXT("FindWidgetBlueprint: Searching for widget '%s'"), *WidgetBlueprintName);
    
    // PRIORITY 1: First try direct path loading for exact matches (most reliable)
    UWidgetBlueprint* DirectLoad = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(WidgetBlueprintName));
    if (DirectLoad)
    {
        UE_LOG(LogTemp, Warning, TEXT("FindWidgetBlueprint: Found widget via direct load"));
        return DirectLoad;
    }

    // PRIORITY 2: If it's a package path without object name, try append .AssetName
    if (WidgetBlueprintName.StartsWith(TEXT("/Game")) && !WidgetBlueprintName.Contains(TEXT(".")))
    {
        FString AssetName;
        int32 SlashIdx;
        if (WidgetBlueprintName.FindLastChar('/', SlashIdx))
        {
            AssetName = WidgetBlueprintName.Mid(SlashIdx + 1);
            if (!AssetName.IsEmpty())
            {
                const FString ObjectPath = WidgetBlueprintName + TEXT(".") + AssetName;
                UE_LOG(LogTemp, Warning, TEXT("FindWidgetBlueprint: Trying object path '%s'"), *ObjectPath);
                DirectLoad = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(ObjectPath));
                if (DirectLoad)
                {
                    UE_LOG(LogTemp, Warning, TEXT("FindWidgetBlueprint: Found widget via constructed object path"));
                    return DirectLoad;
                }
            }
        }
    }
    
    // PRIORITY 3: Use Asset Registry for search, but with improved logic
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    // Create filter to find all Widget Blueprints
    FARFilter Filter;
    Filter.ClassPaths.Add(UWidgetBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;
    Filter.PackagePaths.Add("/Game"); // Search recursively from /Game
    
    TArray<FAssetData> AssetDataList;
    AssetRegistry.GetAssets(Filter, AssetDataList);
    
    UE_LOG(LogTemp, Warning, TEXT("FindWidgetBlueprint: Found %d widget blueprints in asset registry"), AssetDataList.Num());
    
    // IMPROVED MATCHING LOGIC: Use priority-based matching to avoid wrong widget selection
    UWidgetBlueprint* BestMatch = nullptr;
    int32 BestMatchPriority = 0;
    
    for (const FAssetData& AssetData : AssetDataList)
    {
        FString AssetName = AssetData.AssetName.ToString();
        FString PackagePath = AssetData.PackageName.ToString();
        const FString ObjectPath = AssetData.GetObjectPathString();
        
        UE_LOG(LogTemp, VeryVerbose, TEXT("FindWidgetBlueprint: Checking asset '%s' at path '%s'"), *AssetName, *PackagePath);
        
        int32 MatchPriority = 0;
        
        // PRIORITY 1 (10): Exact asset name match (case sensitive)
        if (AssetName.Equals(WidgetBlueprintName, ESearchCase::CaseSensitive))
        {
            MatchPriority = 10;
        }
        // PRIORITY 2 (9): Exact asset name match (case insensitive)
        else if (AssetName.Equals(WidgetBlueprintName, ESearchCase::IgnoreCase))
        {
            MatchPriority = 9;
        }
        // PRIORITY 3 (8): Full object path match (case sensitive)
        else if (ObjectPath.Equals(WidgetBlueprintName, ESearchCase::CaseSensitive))
        {
            MatchPriority = 8;
        }
        // PRIORITY 4 (7): Full package path match (case sensitive)
        else if (PackagePath.Equals(WidgetBlueprintName, ESearchCase::CaseSensitive))
        {
            MatchPriority = 7;
        }
        // PRIORITY 5 (6): Full object path match (case insensitive)
        else if (ObjectPath.Equals(WidgetBlueprintName, ESearchCase::IgnoreCase))
        {
            MatchPriority = 6;
        }
        // PRIORITY 6 (5): Full package path match (case insensitive)
        else if (PackagePath.Equals(WidgetBlueprintName, ESearchCase::IgnoreCase))
        {
            MatchPriority = 5;
        }
        // PRIORITY 7 (3): Asset name starts with search term (exact prefix match)
        else if (AssetName.StartsWith(WidgetBlueprintName, ESearchCase::IgnoreCase) && AssetName.Len() > WidgetBlueprintName.Len())
        {
            MatchPriority = 3;
        }
        // PRIORITY 8 (2): Asset name contains search term (but avoid partial matches that could be wrong)
        else if (AssetName.Contains(WidgetBlueprintName, ESearchCase::IgnoreCase) && 
                 WidgetBlueprintName.Len() >= 3) // Only allow contains matching for terms 3+ chars to avoid false positives
        {
            MatchPriority = 2;
        }
        // PRIORITY 9 (1): Package path contains search term (lowest priority, most error-prone)
        else if (PackagePath.Contains(WidgetBlueprintName, ESearchCase::IgnoreCase) && 
                 WidgetBlueprintName.Len() >= 4) // Only allow contains matching for terms 4+ chars
        {
            MatchPriority = 1;
        }
        
        // Only consider this match if it's better than what we have
        if (MatchPriority > BestMatchPriority)
        {
            UWidgetBlueprint* CandidateWidget = Cast<UWidgetBlueprint>(AssetData.GetAsset());
            if (CandidateWidget)
            {
                UE_LOG(LogTemp, Warning, TEXT("FindWidgetBlueprint: Found better match '%s' with priority %d"), *AssetName, MatchPriority);
                BestMatch = CandidateWidget;
                BestMatchPriority = MatchPriority;
                
                // If we found an exact match, we can stop searching
                if (MatchPriority >= 9)
                {
                    break;
                }
            }
        }
    }
    
    if (BestMatch)
    {
        UE_LOG(LogTemp, Warning, TEXT("FindWidgetBlueprint: Returning best match '%s' with priority %d"), *BestMatch->GetName(), BestMatchPriority);
        return BestMatch;
    }
    
    return nullptr;
}

FWidgetBlueprintEditor* FVibeUECommonUtils::GetWidgetBlueprintEditor(UWidgetBlueprint* WidgetBlueprint)
{
    if (!WidgetBlueprint)
    {
        return nullptr;
    }
    
    // Get the asset editor subsystem
    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!AssetEditorSubsystem)
    {
        return nullptr;
    }
    
    // Check if there's an editor open for this widget
    IAssetEditorInstance* Editor = AssetEditorSubsystem->FindEditorForAsset(WidgetBlueprint, false);
    if (Editor)
    {
        // Try to cast to widget blueprint editor
        FWidgetBlueprintEditor* WidgetEditor = static_cast<FWidgetBlueprintEditor*>(Editor);
        return WidgetEditor;
    }
    
    return nullptr;
} 
