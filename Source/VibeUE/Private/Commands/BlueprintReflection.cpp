#include "Commands/BlueprintReflection.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Self.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "KismetCompiler.h"
#include "Framework/Commands/UIAction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"  
#include "EdGraphSchema_K2.h"
#include "BlueprintActionDatabase.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "Kismet2/BlueprintEditorUtils.h"

// Declare the log category
DEFINE_LOG_CATEGORY_STATIC(LogVibeUEReflection, Log, All);

// Static member initialization - simplified
TArray<FBlueprintReflection::FNodeCategory> FBlueprintReflection::CachedNodeCategories;
bool FBlueprintReflection::bCategoriesInitialized = false;
TMap<FString, UClass*> FBlueprintReflection::NodeTypeMap;

FBlueprintReflection::FBlueprintReflection()
{
    if (!bCategoriesInitialized)
    {
        PopulateNodeCategories();
        bCategoriesInitialized = true;
    }
}

TSharedPtr<FJsonObject> FBlueprintReflection::GetAvailableBlueprintNodes(UBlueprint* Blueprint, const FString& Category, const FString& Context)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShareable(new FJsonObject);
    
    if (!Blueprint)
    {
        ResponseObject->SetBoolField(TEXT("success"), false);
        ResponseObject->SetStringField(TEXT("error"), TEXT("Blueprint not found"));
        return ResponseObject;
    }

    try
    {
        UE_LOG(LogVibeUEReflection, Log, TEXT("*** GetAvailableBlueprintNodes called for: %s with Category: '%s', Context: '%s' ***"), 
               *Blueprint->GetName(), *Category, *Context);
        
        // Get filtered Blueprint actions using the improved filtering system
        TArray<TSharedPtr<FEdGraphSchemaAction>> AllActions;
        GetBlueprintActionMenuItems(Blueprint, AllActions);
        
        UE_LOG(LogVibeUEReflection, Warning, TEXT("*** DEBUG: Retrieved %d FILTERED actions from GetBlueprintActionMenuItems ***"), AllActions.Num());
        
        if (AllActions.Num() == 0)
        {
            UE_LOG(LogVibeUEReflection, Error, TEXT("*** ERROR: No actions retrieved from Blueprint Action Database ***"));
            
            // Return error result to see if this path is taken
            TSharedPtr<FJsonObject> CategoriesObject = MakeShareable(new FJsonObject);
            TArray<TSharedPtr<FJsonValue>> ErrorNodes;
            
            TSharedPtr<FJsonObject> ErrorNode = MakeShareable(new FJsonObject);
            ErrorNode->SetStringField(TEXT("name"), TEXT("*** ERROR: No Blueprint Actions Found ***"));
            ErrorNode->SetStringField(TEXT("category"), TEXT("Error"));
            ErrorNode->SetStringField(TEXT("description"), TEXT("Blueprint Action Database returned 0 actions"));
            ErrorNode->SetStringField(TEXT("keywords"), TEXT("error debug"));
            ErrorNode->SetStringField(TEXT("section_id"), TEXT("0"));
            ErrorNode->SetStringField(TEXT("action_class"), TEXT("ErrorAction"));
            ErrorNode->SetStringField(TEXT("type"), TEXT("node"));
            
            ErrorNodes.Add(MakeShareable(new FJsonValueObject(ErrorNode)));
            CategoriesObject->SetArrayField(TEXT("Error"), ErrorNodes);
            
            ResponseObject->SetBoolField(TEXT("success"), true);
            ResponseObject->SetObjectField(TEXT("categories"), CategoriesObject);
            ResponseObject->SetNumberField(TEXT("total_nodes"), 1);
            ResponseObject->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());
            
            return ResponseObject;
        }
        
        TSharedPtr<FJsonObject> CategoriesObject = MakeShareable(new FJsonObject);
        TMap<FString, TArray<TSharedPtr<FJsonValue>>> CategoryMap;
        
        // Process filtered actions with additional search filtering
        int32 TotalActionCount = 0;
        for (const TSharedPtr<FEdGraphSchemaAction>& Action : AllActions)
        {
            if (Action.IsValid())
            {
                TSharedPtr<FJsonObject> ActionObject = MakeShareable(new FJsonObject);
                
                FString ActionName = Action->GetMenuDescription().ToString();
                FString ActionCategory = Action->GetCategory().ToString();
                FString ActionTooltip = Action->GetTooltipDescription().ToString();
                FString ActionKeywords = Action->GetKeywords().ToString();
                
                if (ActionName.IsEmpty())
                {
                    ActionName = FString::Printf(TEXT("Unknown Action %d"), TotalActionCount);
                }
                
                if (ActionCategory.IsEmpty())
                {
                    ActionCategory = TEXT("Blueprint");
                }
                
                // Apply search filtering similar to Unreal Editor
                bool bShouldInclude = true;
                
                // Category filtering - exact match or contains
                if (!Category.IsEmpty() && Category != TEXT("all") && Category != TEXT("*"))
                {
                    bShouldInclude = ActionCategory.Contains(Category) || 
                                   ActionCategory.Equals(Category, ESearchCase::IgnoreCase);
                }
                
                // Context/Search term filtering - searches in name, keywords, and tooltip
                if (bShouldInclude && !Context.IsEmpty() && Context != TEXT("all") && Context != TEXT("*"))
                {
                    FString SearchText = (ActionName + TEXT(" ") + ActionKeywords + TEXT(" ") + ActionTooltip).ToLower();
                    FString SearchTerm = Context.ToLower();
                    
                    bShouldInclude = SearchText.Contains(SearchTerm) ||
                                   ActionName.Contains(Context) ||
                                   ActionKeywords.Contains(Context) ||
                                   ActionTooltip.Contains(Context);
                }
                
                if (bShouldInclude)
                {
                    ActionObject->SetStringField(TEXT("name"), ActionName);
                    ActionObject->SetStringField(TEXT("category"), ActionCategory);
                    ActionObject->SetStringField(TEXT("description"), ActionTooltip);
                    ActionObject->SetStringField(TEXT("keywords"), ActionKeywords);
                    ActionObject->SetStringField(TEXT("section_id"), TEXT("0"));
                    ActionObject->SetStringField(TEXT("action_class"), TEXT("FEdGraphSchemaAction"));
                    ActionObject->SetStringField(TEXT("type"), TEXT("node"));
                    
                    // Add search relevance scoring for better results
                    int32 RelevanceScore = CalculateSearchRelevance(ActionName, ActionKeywords, ActionTooltip, Context);
                    ActionObject->SetNumberField(TEXT("relevance_score"), RelevanceScore);
                    
                    // Add to category map
                    if (!CategoryMap.Contains(ActionCategory))
                    {
                        CategoryMap.Add(ActionCategory, TArray<TSharedPtr<FJsonValue>>());
                    }
                    CategoryMap[ActionCategory].Add(MakeShareable(new FJsonValueObject(ActionObject)));
                    TotalActionCount++;
                }
            }
        }
        
        // Build categories object
        for (const auto& CategoryPair : CategoryMap)
        {
            CategoriesObject->SetArrayField(CategoryPair.Key, CategoryPair.Value);
        }
        
        ResponseObject->SetBoolField(TEXT("success"), true);
        ResponseObject->SetObjectField(TEXT("categories"), CategoriesObject);
        ResponseObject->SetNumberField(TEXT("total_nodes"), TotalActionCount);
        ResponseObject->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());
        ResponseObject->SetStringField(TEXT("blueprint_class"), Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetName() : TEXT("Unknown"));
        ResponseObject->SetStringField(TEXT("filter_applied"), Category.IsEmpty() ? TEXT("none") : Category);
        ResponseObject->SetStringField(TEXT("search_term"), Context.IsEmpty() ? TEXT("none") : Context);
        
        UE_LOG(LogVibeUEReflection, Warning, TEXT("*** DEBUG: Returning %d FILTERED Blueprint actions for: %s (Category: %s, Search: %s) ***"), 
               TotalActionCount, *Blueprint->GetName(), *Category, *Context);
        
    }
    catch (const std::exception& e)
    {
        FString ErrorMessage = FString::Printf(TEXT("Error discovering Blueprint nodes: %s"), UTF8_TO_TCHAR(e.what()));
        UE_LOG(LogVibeUEReflection, Error, TEXT("%s"), *ErrorMessage);
        
        ResponseObject->SetBoolField(TEXT("success"), false);
        ResponseObject->SetStringField(TEXT("error"), ErrorMessage);
    }
    
    return ResponseObject;
}

TSharedPtr<FJsonObject> FBlueprintReflection::CreateBlueprintNode(UBlueprint* Blueprint, const FString& NodeType, const TSharedPtr<FJsonObject>& NodeParams)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShareable(new FJsonObject);
    
    if (!Blueprint)
    {
        ResponseObject->SetBoolField(TEXT("success"), false);
        ResponseObject->SetStringField(TEXT("error"), TEXT("Blueprint not found"));
        return ResponseObject;
    }

    try
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("CreateBlueprintNode: Creating '%s' node in Blueprint '%s'"), *NodeType, *Blueprint->GetName());
        
        // Find the event graph
        UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
        if (!EventGraph)
        {
            ResponseObject->SetBoolField(TEXT("success"), false);
            ResponseObject->SetStringField(TEXT("error"), TEXT("Could not find event graph in Blueprint"));
            return ResponseObject;
        }
        
        // ENHANCED: Try direct node type mapping first (fast path)
        UClass** NodeClassPtr = NodeTypeMap.Find(NodeType);
        if (NodeClassPtr && *NodeClassPtr)
        {
            UClass* NodeClass = *NodeClassPtr;
            
            // Create the new node instance
            UK2Node* NewNode = NewObject<UK2Node>(EventGraph, NodeClass);
            if (NewNode)
            {
                // Set node position from parameters
                FVector2D NodePosition(200.0f, 200.0f); // Default position
                if (NodeParams.IsValid())
                {
                    const TArray<TSharedPtr<FJsonValue>>* PosArray;
                    if (NodeParams->TryGetArrayField(TEXT("position"), PosArray) && PosArray->Num() >= 2)
                    {
                        NodePosition.X = (*PosArray)[0]->AsNumber();
                        NodePosition.Y = (*PosArray)[1]->AsNumber();
                    }
                }
                
                NewNode->NodePosX = NodePosition.X;
                NewNode->NodePosY = NodePosition.Y;
                
                // Add node to the graph
                EventGraph->AddNode(NewNode, true, true);
                
                // Allocate default pins
                NewNode->AllocateDefaultPins();
                
                // Configure node-specific properties if provided
                if (NodeParams.IsValid())
                {
                    FBlueprintReflection::ConfigureNodeFromParameters(NewNode, NodeParams);
                }
                
                // Reconstruct the node to ensure proper setup
                NewNode->ReconstructNode();
                
                // Mark blueprint as modified
                FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
                
                // Return success response with node details
                FString NodeId = FString::Printf(TEXT("%s_%s"), *NodeType.ToLower(), *LexToString(NewNode->NodeGuid));
                ResponseObject->SetBoolField(TEXT("success"), true);
                ResponseObject->SetStringField(TEXT("node_id"), NodeId);
                ResponseObject->SetStringField(TEXT("node_type"), NodeType);
                ResponseObject->SetStringField(TEXT("display_name"), NewNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
                ResponseObject->SetNumberField(TEXT("position_x"), NodePosition.X);
                ResponseObject->SetNumberField(TEXT("position_y"), NodePosition.Y);
                ResponseObject->SetNumberField(TEXT("pin_count"), NewNode->Pins.Num());
                
                UE_LOG(LogVibeUEReflection, Log, TEXT("Successfully created node %s (ID: %s) via direct mapping in Blueprint %s"), 
                       *NodeType, *NodeId, *Blueprint->GetName());
                return ResponseObject;
            }
        }
        
        // ENHANCED: Blueprint Action Database approach (like GitHub reference)
        UE_LOG(LogVibeUEReflection, Warning, TEXT("Direct mapping failed for '%s', trying Blueprint Action Database approach"), *NodeType);
        
        FBlueprintActionDatabase& ActionDatabase = FBlueprintActionDatabase::Get();
        const FBlueprintActionDatabase::FActionRegistry& ActionRegistry = ActionDatabase.GetAllActions();
        
        // Search through the action registry for matching node types
        for (auto const& ActionEntry : ActionRegistry)
        {
            for (UBlueprintNodeSpawner* NodeSpawner : ActionEntry.Value)
            {
                if (NodeSpawner && NodeSpawner->NodeClass)
                {
                    FString SpawnerClassName = NodeSpawner->NodeClass->GetName();
                    
                    // Get menu description from DefaultMenuSignature instead of GetMenuName()
                    FString MenuDescription = NodeSpawner->DefaultMenuSignature.MenuName.ToString();
                    if (MenuDescription.IsEmpty())
                    {
                        MenuDescription = SpawnerClassName; // Fallback to class name
                    }
                
                // Check if this spawner matches our requested node type
                bool bIsMatch = false;
                
                // Exact match in menu description (highest priority)
                if (MenuDescription.Equals(NodeType, ESearchCase::IgnoreCase))
                {
                    bIsMatch = true;
                    UE_LOG(LogVibeUEReflection, Warning, TEXT("Found exact menu match: '%s' for requested '%s'"), *MenuDescription, *NodeType);
                }
                // Partial match in menu description
                else if (MenuDescription.Contains(NodeType, ESearchCase::IgnoreCase) && NodeType.Len() > 3)
                {
                    bIsMatch = true;
                    UE_LOG(LogVibeUEReflection, Warning, TEXT("Found partial menu match: '%s' contains '%s'"), *MenuDescription, *NodeType);
                }
                // Match in class name (fallback)
                else if (SpawnerClassName.Contains(NodeType, ESearchCase::IgnoreCase) && NodeType.Len() > 3)
                {
                    bIsMatch = true;
                    UE_LOG(LogVibeUEReflection, Warning, TEXT("Found class name match: '%s' contains '%s'"), *SpawnerClassName, *NodeType);
                }                    if (bIsMatch)
                    {
                        // Found a potential match! Try to spawn it
                        FVector2D SpawnLocation(200.0f, 200.0f);
                        if (NodeParams.IsValid() && NodeParams->HasField(TEXT("position")))
                        {
                            const TArray<TSharedPtr<FJsonValue>>* PositionArray;
                            if (NodeParams->TryGetArrayField(TEXT("position"), PositionArray) && PositionArray->Num() >= 2)
                            {
                                SpawnLocation.X = (*PositionArray)[0]->AsNumber();
                                SpawnLocation.Y = (*PositionArray)[1]->AsNumber();
                            }
                        }
                        
                        // Create the node using NodeSpawner
                        IBlueprintNodeBinder::FBindingSet Bindings;
                        UEdGraphNode* SpawnedNode = NodeSpawner->Invoke(EventGraph, Bindings, SpawnLocation);
                        
                        if (SpawnedNode)
                        {
                            FString NodeId = FString::Printf(TEXT("%s_%s"), *NodeType.ToLower(), *LexToString(SpawnedNode->NodeGuid));
                            
                            // Mark blueprint as modified
                            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
                            
                            UE_LOG(LogVibeUEReflection, Warning, TEXT("Created %s node via action database at (%.1f, %.1f) with ID: %s"), 
                                   *NodeType, SpawnLocation.X, SpawnLocation.Y, *NodeId);
                            
                            ResponseObject->SetBoolField(TEXT("success"), true);
                            ResponseObject->SetStringField(TEXT("node_type"), NodeType);
                            ResponseObject->SetStringField(TEXT("node_id"), NodeId);
                            ResponseObject->SetStringField(TEXT("spawner_class"), SpawnerClassName);
                            ResponseObject->SetStringField(TEXT("menu_description"), MenuDescription);
                            ResponseObject->SetStringField(TEXT("display_name"), SpawnedNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
                            ResponseObject->SetNumberField(TEXT("position_x"), SpawnLocation.X);
                            ResponseObject->SetNumberField(TEXT("position_y"), SpawnLocation.Y);
                            ResponseObject->SetStringField(TEXT("message"), FString::Printf(TEXT("Successfully created %s node via action database"), *NodeType));
                            return ResponseObject;
                        }
                        else
                        {
                            UE_LOG(LogVibeUEReflection, Warning, TEXT("NodeSpawner::Invoke returned null for %s"), *NodeType);
                        }
                    }
                }
            }
        }
        
        // If we get here, we couldn't create the node
        ResponseObject->SetBoolField(TEXT("success"), false);
        ResponseObject->SetStringField(TEXT("error"), FString::Printf(TEXT("Could not create node of type '%s' - not found in direct mapping or action database"), *NodeType));
        return ResponseObject;
        
    }
    catch (const std::exception& e)
    {
        FString ErrorMessage = FString::Printf(TEXT("Error creating Blueprint node: %s"), UTF8_TO_TCHAR(e.what()));
        UE_LOG(LogVibeUEReflection, Error, TEXT("%s"), *ErrorMessage);
        
        ResponseObject->SetBoolField(TEXT("success"), false);
        ResponseObject->SetStringField(TEXT("error"), ErrorMessage);
    }
    
    return ResponseObject;
}

void FBlueprintReflection::PopulateNodeCategories()
{
    CachedNodeCategories.Empty();
    NodeTypeMap.Empty();
    
    // Simplified node categories for initial implementation
    FNodeCategory FlowControl;
    FlowControl.CategoryName = TEXT("Flow Control");
    FlowControl.Description = TEXT("Control the execution flow of your Blueprint");
    FlowControl.Keywords = {TEXT("branch"), TEXT("if"), TEXT("loop"), TEXT("sequence")};
    CachedNodeCategories.Add(FlowControl);
    
    FNodeCategory Variables;
    Variables.CategoryName = TEXT("Variables");
    Variables.Description = TEXT("Get and set variable values");
    Variables.Keywords = {TEXT("get"), TEXT("set"), TEXT("variable"), TEXT("property")};
    CachedNodeCategories.Add(Variables);
    
    FNodeCategory Functions;
    Functions.CategoryName = TEXT("Functions");
    Functions.Description = TEXT("Call functions and methods");
    Functions.Keywords = {TEXT("call"), TEXT("function"), TEXT("method")};
    CachedNodeCategories.Add(Functions);
    
    // Populate simplified node type map (using UClass* instead of TSubclassOf)
    NodeTypeMap.Add(TEXT("Branch"), UK2Node_IfThenElse::StaticClass());
    NodeTypeMap.Add(TEXT("GetVariable"), UK2Node_VariableGet::StaticClass());
    NodeTypeMap.Add(TEXT("SetVariable"), UK2Node_VariableSet::StaticClass());
    NodeTypeMap.Add(TEXT("CallFunction"), UK2Node_CallFunction::StaticClass());
    NodeTypeMap.Add(TEXT("Cast"), UK2Node_DynamicCast::StaticClass());
    NodeTypeMap.Add(TEXT("Self"), UK2Node_Self::StaticClass());
    NodeTypeMap.Add(TEXT("Event"), UK2Node_Event::StaticClass());
    
    // ENHANCED: Will add more node types as we find the correct header files
    // For now, focus on the working Blueprint Action Database approach
    
    UE_LOG(LogVibeUEReflection, Log, TEXT("Populated %d enhanced node categories with %d node types"), CachedNodeCategories.Num(), NodeTypeMap.Num());
}

TArray<FBlueprintReflection::FNodeMetadata> FBlueprintReflection::DiscoverNodesForBlueprint(UBlueprint* Blueprint, const FString& Category)
{
    TArray<FNodeMetadata> DiscoveredNodes;
    
    // Simplified implementation - return basic node metadata
    FNodeMetadata BranchNode;
    BranchNode.Category = TEXT("Flow Control");
    BranchNode.NodeType = TEXT("Branch");
    BranchNode.DisplayName = TEXT("Branch");
    BranchNode.Description = TEXT("Conditional execution flow");
    BranchNode.Keywords = {TEXT("if"), TEXT("condition")};
    BranchNode.InputPins.Add(TEXT("exec"));
    BranchNode.OutputPins.Add(TEXT("true"));
    BranchNode.OutputPins.Add(TEXT("false"));
    DiscoveredNodes.Add(BranchNode);
    
    UE_LOG(LogVibeUEReflection, Log, TEXT("Discovered %d simplified nodes for Blueprint %s"), DiscoveredNodes.Num(), *Blueprint->GetName());
    return DiscoveredNodes;
}

TArray<FBlueprintReflection::FNodeCategory> FBlueprintReflection::GetNodeCategories()
{
    if (!bCategoriesInitialized)
    {
        PopulateNodeCategories();
        bCategoriesInitialized = true;
    }
    return CachedNodeCategories;
}

// === NODE CONFIGURATION SYSTEM ===

void FBlueprintReflection::ConfigureNodeFromParameters(UK2Node* Node, const TSharedPtr<FJsonObject>& NodeParams)
{
    if (!Node || !NodeParams.IsValid())
        return;
        
    UE_LOG(LogVibeUEReflection, Log, TEXT("Configuring node %s with parameters"), *Node->GetClass()->GetName());
    
    // Configure node-specific properties based on node type
    if (UK2Node_CallFunction* FunctionNode = Cast<UK2Node_CallFunction>(Node))
    {
        ConfigureFunctionNode(FunctionNode, NodeParams);
    }
    else if (UK2Node_VariableGet* VariableGetNode = Cast<UK2Node_VariableGet>(Node))
    {
        ConfigureVariableNode(VariableGetNode, NodeParams);
    }
    else if (UK2Node_VariableSet* VariableSetNode = Cast<UK2Node_VariableSet>(Node))
    {
        ConfigureVariableSetNode(VariableSetNode, NodeParams);
    }
    else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
    {
        ConfigureEventNode(EventNode, NodeParams);
    }
    
    // Apply common properties
    FString NodeComment;
    if (NodeParams->TryGetStringField(TEXT("comment"), NodeComment))
    {
        Node->NodeComment = NodeComment;
        Node->bCommentBubbleVisible = !NodeComment.IsEmpty();
    }
    
    // Set node enabled state
    bool bNodeEnabled = true;
    if (NodeParams->TryGetBoolField(TEXT("enabled"), bNodeEnabled))
    {
        Node->SetEnabledState(bNodeEnabled ? ENodeEnabledState::Enabled : ENodeEnabledState::Disabled);
    }
}

void FBlueprintReflection::ConfigureFunctionNode(UK2Node_CallFunction* FunctionNode, const TSharedPtr<FJsonObject>& NodeParams)
{
    if (!FunctionNode)
        return;
        
    FString FunctionName;
    if (NodeParams->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        // Find the function in the available functions
        if (UClass* TargetClass = FunctionNode->GetBlueprint()->GeneratedClass)
        {
            if (UFunction* Function = TargetClass->FindFunctionByName(*FunctionName))
            {
                FunctionNode->SetFromFunction(Function);
                UE_LOG(LogVibeUEReflection, Log, TEXT("Set function node to call: %s"), *FunctionName);
            }
        }
    }
    
    FString TargetClass;
    if (NodeParams->TryGetStringField(TEXT("target_class"), TargetClass))
    {
        // Handle target class specification for static functions
        UE_LOG(LogVibeUEReflection, Log, TEXT("Function node target class: %s"), *TargetClass);
    }
}

void FBlueprintReflection::ConfigureVariableNode(UK2Node_VariableGet* VariableNode, const TSharedPtr<FJsonObject>& NodeParams)
{
    if (!VariableNode)
        return;
        
    FString VariableName;
    if (NodeParams->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        // Find the variable in the Blueprint
        if (UBlueprint* Blueprint = VariableNode->GetBlueprint())
        {
            FName VarName(*VariableName);
            VariableNode->VariableReference.SetSelfMember(VarName);
            UE_LOG(LogVibeUEReflection, Log, TEXT("Set variable get node to reference: %s"), *VariableName);
        }
    }
}

void FBlueprintReflection::ConfigureVariableSetNode(UK2Node_VariableSet* VariableNode, const TSharedPtr<FJsonObject>& NodeParams)
{
    if (!VariableNode)
        return;
        
    FString VariableName;
    if (NodeParams->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        // Find the variable in the Blueprint
        if (UBlueprint* Blueprint = VariableNode->GetBlueprint())
        {
            FName VarName(*VariableName);
            VariableNode->VariableReference.SetSelfMember(VarName);
            UE_LOG(LogVibeUEReflection, Log, TEXT("Set variable set node to reference: %s"), *VariableName);
        }
    }
}

void FBlueprintReflection::ConfigureEventNode(UK2Node_Event* EventNode, const TSharedPtr<FJsonObject>& NodeParams)
{
    if (!EventNode)
        return;
        
    FString EventName;
    if (NodeParams->TryGetStringField(TEXT("event_name"), EventName))
    {
        // Configure custom event name
        EventNode->CustomFunctionName = FName(*EventName);
        UE_LOG(LogVibeUEReflection, Log, TEXT("Set event node name: %s"), *EventName);
    }
    
    // Handle event-specific parameters
    bool bOverride = false;
    if (NodeParams->TryGetBoolField(TEXT("override"), bOverride))
    {
        if (bOverride)
        {
            EventNode->bOverrideFunction = true;
            UE_LOG(LogVibeUEReflection, Log, TEXT("Set event node to override"));
        }
    }
}

// === PLACEHOLDER IMPLEMENTATIONS FOR DECLARED METHODS ===

TSharedPtr<FJsonObject> FBlueprintReflection::GetNodeProperties(UK2Node* Node)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    if (!Node)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Node is null");
        return Result;
    }
    
    // Placeholder - return basic node info
    Result->SetBoolField("success", true);
    Result->SetStringField("node_type", Node->GetClass()->GetName());
    Result->SetStringField("node_title", Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
    
    return Result;
}

TSharedPtr<FJsonObject> FBlueprintReflection::SetPinDefaultValue(UEdGraphPin* Pin, const FString& PinName, const FString& Value)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    if (!Pin)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Pin is null");
        return Result;
    }
    
    // Validate that this pin can have its default value set
    if (Pin->Direction != EGPD_Input)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", FString::Printf(TEXT("Cannot set default value on output pin '%s'"), *PinName));
        return Result;
    }
    
    if (Pin->LinkedTo.Num() > 0)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", FString::Printf(TEXT("Cannot set default value on connected pin '%s'"), *PinName));
        return Result;
    }
    
    // Store original value for comparison
    FString OriginalValue = Pin->DefaultValue;
    
    // Handle different pin types
    if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
    {
        bool bValue = Value.ToBool();
        Pin->DefaultValue = bValue ? TEXT("true") : TEXT("false");
    }
    else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
    {
        int32 IntValue = FCString::Atoi(*Value);
        Pin->DefaultValue = FString::FromInt(IntValue);
    }
    else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Real)
    {
        float FloatValue = FCString::Atof(*Value);
        Pin->DefaultValue = FString::SanitizeFloat(FloatValue);
    }
    else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_String)
    {
        Pin->DefaultValue = Value;
    }
    else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
    {
        Pin->DefaultValue = Value;
    }
    else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
    {
        Pin->DefaultValue = Value;
    }
    else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Byte && Pin->PinType.PinSubCategoryObject.IsValid())
    {
        // Handle enum values
        UEnum* EnumClass = Cast<UEnum>(Pin->PinType.PinSubCategoryObject.Get());
        if (EnumClass)
        {
            // Try to find the enum value by name
            int64 EnumValue = EnumClass->GetValueByNameString(Value);
            if (EnumValue != INDEX_NONE)
            {
                Pin->DefaultValue = Value;
            }
            else
            {
                Result->SetBoolField("success", false);
                Result->SetStringField("error", FString::Printf(TEXT("Invalid enum value '%s' for pin '%s'"), *Value, *PinName));
                return Result;
            }
        }
        else
        {
            // Regular byte value
            uint8 ByteValue = (uint8)FCString::Atoi(*Value);
            Pin->DefaultValue = FString::FromInt(ByteValue);
        }
    }
    else
    {
        // For other types, try to set the raw value
        Pin->DefaultValue = Value;
    }
    
    // Mark the node as modified
    if (UEdGraphNode* OwnerNode = Pin->GetOwningNode())
    {
        OwnerNode->ReconstructNode();
        
        if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(OwnerNode))
        {
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        }
    }
    
    // Build successful result with details
    Result->SetBoolField("success", true);
    Result->SetStringField("pin_name", PinName);
    Result->SetStringField("pin_type", Pin->PinType.PinCategory.ToString());
    Result->SetStringField("old_value", OriginalValue);
    Result->SetStringField("new_value", Pin->DefaultValue);
    Result->SetStringField("pin_subcategory", Pin->PinType.PinSubCategory.ToString());
    
    if (Pin->PinType.PinSubCategoryObject.IsValid())
    {
        Result->SetStringField("pin_subcategory_object", Pin->PinType.PinSubCategoryObject->GetName());
    }
    
    return Result;
}

TSharedPtr<FJsonObject> FBlueprintReflection::GetNodeProperty(UK2Node* Node, const FString& PropertyName)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    if (!Node)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Node is null");
        return Result;
    }
    
    UClass* NodeClass = Node->GetClass();
    if (!NodeClass)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Node class is null");
        return Result;
    }
    
    // Try to find the property on the node
    FProperty* Property = NodeClass->FindPropertyByName(*PropertyName);
    
    // If not found, try case-insensitive search
    if (!Property)
    {
        for (TFieldIterator<FProperty> PropIt(NodeClass); PropIt; ++PropIt)
        {
            if (PropIt->GetName().Equals(PropertyName, ESearchCase::IgnoreCase))
            {
                Property = *PropIt;
                break;
            }
        }
    }
    
    // If still not found, try to find a pin with this name
    if (!Property)
    {
        UEdGraphPin* TargetPin = nullptr;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->PinName.ToString().Equals(PropertyName, ESearchCase::IgnoreCase))
            {
                TargetPin = Pin;
                break;
            }
        }
        
        if (TargetPin)
        {
            // Return pin default value information
            Result->SetBoolField("success", true);
            Result->SetStringField("property_name", PropertyName);
            Result->SetStringField("property_type", "Pin");
            Result->SetStringField("value", TargetPin->DefaultValue);
            Result->SetStringField("pin_type", TargetPin->PinType.PinCategory.ToString());
            Result->SetStringField("pin_subcategory", TargetPin->PinType.PinSubCategory.ToString());
            Result->SetBoolField("is_connected", TargetPin->LinkedTo.Num() > 0);
            Result->SetStringField("pin_direction", (TargetPin->Direction == EGPD_Input) ? "Input" : "Output");
            
            if (TargetPin->PinType.PinSubCategoryObject.IsValid())
            {
                Result->SetStringField("pin_subcategory_object", TargetPin->PinType.PinSubCategoryObject->GetName());
            }
            
            return Result;
        }
        
        // Property not found at all
        Result->SetBoolField("success", false);
        Result->SetStringField("error", FString::Printf(TEXT("Property '%s' not found on node class '%s'"), 
            *PropertyName, *NodeClass->GetName()));
        
        // Provide available properties and pins as suggestions
        TArray<TSharedPtr<FJsonValue>> AvailableProps;
        for (TFieldIterator<FProperty> PropIt(NodeClass); PropIt; ++PropIt)
        {
            FProperty* Prop = *PropIt;
            if (Prop->HasAnyPropertyFlags(CPF_Edit))
            {
                AvailableProps.Add(MakeShared<FJsonValueString>(Prop->GetName()));
            }
        }
        
        TArray<TSharedPtr<FJsonValue>> AvailablePins;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Input && !Pin->PinType.bIsReference)
            {
                AvailablePins.Add(MakeShared<FJsonValueString>(Pin->PinName.ToString()));
            }
        }
        
        Result->SetArrayField("available_properties", AvailableProps);
        Result->SetArrayField("available_pins", AvailablePins);
        return Result;
    }
    
    // Found node property - get its value
    FString PropertyValue;
    bool bSuccess = true;
    
    if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
    {
        bool bValue = BoolProp->GetPropertyValue_InContainer(Node);
        PropertyValue = bValue ? TEXT("true") : TEXT("false");
    }
    else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
    {
        int32 IntValue = IntProp->GetPropertyValue_InContainer(Node);
        PropertyValue = FString::FromInt(IntValue);
    }
    else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
    {
        float FloatValue = FloatProp->GetPropertyValue_InContainer(Node);
        PropertyValue = FString::SanitizeFloat(FloatValue);
    }
    else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
    {
        FString StrValue = StrProp->GetPropertyValue_InContainer(Node);
        PropertyValue = StrValue;
    }
    else if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
    {
        FText TextValue = TextProp->GetPropertyValue_InContainer(Node);
        PropertyValue = TextValue.ToString();
    }
    else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
    {
        FName NameValue = NameProp->GetPropertyValue_InContainer(Node);
        PropertyValue = NameValue.ToString();
    }
    else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
    {
        int64 EnumValue = EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(EnumProp->ContainerPtrToValuePtr<void>(Node));
        if (UEnum* EnumClass = EnumProp->GetEnum())
        {
            PropertyValue = EnumClass->GetNameStringByValue(EnumValue);
        }
        else
        {
            PropertyValue = FString::FromInt(EnumValue);
        }
    }
    else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
    {
        uint8 ByteValue = ByteProp->GetPropertyValue_InContainer(Node);
        if (ByteProp->Enum)
        {
            PropertyValue = ByteProp->Enum->GetNameStringByValue(ByteValue);
        }
        else
        {
            PropertyValue = FString::FromInt(ByteValue);
        }
    }
    else
    {
        // Try to export as string for other property types
        FString ExportedValue;
        Property->ExportTextItem_Direct(ExportedValue, Property->ContainerPtrToValuePtr<uint8>(Node), nullptr, Node, PPF_None);
        PropertyValue = ExportedValue;
    }
    
    if (bSuccess)
    {
        Result->SetBoolField("success", true);
        Result->SetStringField("property_name", PropertyName);
        Result->SetStringField("property_type", "Node");
        Result->SetStringField("value", PropertyValue);
        Result->SetStringField("cpp_type", Property->GetCPPType());
        Result->SetBoolField("is_editable", Property->HasAnyPropertyFlags(CPF_Edit));
        Result->SetBoolField("is_blueprint_visible", Property->HasAnyPropertyFlags(CPF_BlueprintVisible));
    }
    else
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", FString::Printf(TEXT("Failed to get value for property '%s'"), *PropertyName));
    }
    
    return Result;
}

TSharedPtr<FJsonObject> FBlueprintReflection::SetNodeProperty(UK2Node* Node, const FString& PropertyName, const FString& PropertyValue)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    if (!Node)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Node is null");
        return Result;
    }
    
    UE_LOG(LogVibeUEReflection, Warning, TEXT("SetNodeProperty: Node=%s, Property=%s, Value=%s"), 
        *Node->GetName(), *PropertyName, *PropertyValue);
    
    // Enhanced: Use reflection to find and set properties
    UClass* NodeClass = Node->GetClass();
    FProperty* Property = NodeClass->FindPropertyByName(*PropertyName);
    
    if (!Property)
    {
        // Try searching for properties with similar names (case-insensitive)
        for (TFieldIterator<FProperty> PropIt(NodeClass); PropIt; ++PropIt)
        {
            if (PropIt->GetName().Equals(PropertyName, ESearchCase::IgnoreCase))
            {
                Property = *PropIt;
                break;
            }
        }
        
        if (!Property)
        {
            // Enhanced: If no node property found, try to find a pin with this name
            UEdGraphPin* TargetPin = nullptr;
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin && Pin->PinName.ToString().Equals(PropertyName, ESearchCase::IgnoreCase))
                {
                    TargetPin = Pin;
                    break;
                }
            }
            
            if (TargetPin)
            {
                // Handle pin default value setting
                return SetPinDefaultValue(TargetPin, PropertyName, PropertyValue);
            }
            
            Result->SetBoolField("success", false);
            Result->SetStringField("error", FString::Printf(TEXT("Property '%s' not found on node class '%s'"), 
                *PropertyName, *NodeClass->GetName()));
            
            // Enhanced: Provide available properties and pin names as suggestions
            TArray<TSharedPtr<FJsonValue>> AvailableProps;
            for (TFieldIterator<FProperty> PropIt(NodeClass); PropIt; ++PropIt)
            {
                FProperty* Prop = *PropIt;
                if (Prop->HasAnyPropertyFlags(CPF_Edit))
                {
                    AvailableProps.Add(MakeShared<FJsonValueString>(Prop->GetName()));
                }
            }
            
            TArray<TSharedPtr<FJsonValue>> AvailablePins;
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin && Pin->Direction == EGPD_Input && !Pin->PinType.bIsReference)
                {
                    AvailablePins.Add(MakeShared<FJsonValueString>(Pin->PinName.ToString()));
                }
            }
            
            Result->SetArrayField("available_properties", AvailableProps);
            Result->SetArrayField("available_pins", AvailablePins);
            return Result;
        }
    }
    
    // Enhanced: Check if property is editable
    if (!Property->HasAnyPropertyFlags(CPF_Edit))
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", FString::Printf(TEXT("Property '%s' is not editable"), *PropertyName));
        Result->SetStringField("property_flags", FString::Printf(TEXT("0x%08X"), (uint32)Property->PropertyFlags));
        return Result;
    }
    
    // Enhanced: Set property value based on type
    bool bSetSuccess = false;
    FString ErrorMessage;
    
    void* PropertyPtr = Property->ContainerPtrToValuePtr<void>(Node);
    if (!PropertyPtr)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Failed to get property pointer");
        return Result;
    }
    
    // Enhanced: Handle different property types
    if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
    {
        bool bValue = PropertyValue.ToBool() || PropertyValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
        BoolProp->SetPropertyValue(PropertyPtr, bValue);
        bSetSuccess = true;
        UE_LOG(LogVibeUEReflection, Warning, TEXT("Set bool property %s = %s"), *PropertyName, bValue ? TEXT("true") : TEXT("false"));
    }
    else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
    {
        int32 IntValue = FCString::Atoi(*PropertyValue);
        IntProp->SetPropertyValue(PropertyPtr, IntValue);
        bSetSuccess = true;
        UE_LOG(LogVibeUEReflection, Warning, TEXT("Set int property %s = %d"), *PropertyName, IntValue);
    }
    else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
    {
        float FloatValue = FCString::Atof(*PropertyValue);
        FloatProp->SetPropertyValue(PropertyPtr, FloatValue);
        bSetSuccess = true;
        UE_LOG(LogVibeUEReflection, Warning, TEXT("Set float property %s = %f"), *PropertyName, FloatValue);
    }
    else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
    {
        StrProp->SetPropertyValue(PropertyPtr, PropertyValue);
        bSetSuccess = true;
        UE_LOG(LogVibeUEReflection, Warning, TEXT("Set string property %s = %s"), *PropertyName, *PropertyValue);
    }
    else if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
    {
        FText TextValue = FText::FromString(PropertyValue);
        TextProp->SetPropertyValue(PropertyPtr, TextValue);
        bSetSuccess = true;
        UE_LOG(LogVibeUEReflection, Warning, TEXT("Set text property %s = %s"), *PropertyName, *PropertyValue);
    }
    else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
    {
        FName NameValue(*PropertyValue);
        NameProp->SetPropertyValue(PropertyPtr, NameValue);
        bSetSuccess = true;
        UE_LOG(LogVibeUEReflection, Warning, TEXT("Set name property %s = %s"), *PropertyName, *PropertyValue);
    }
    else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
    {
        uint8 ByteValue = (uint8)FCString::Atoi(*PropertyValue);
        ByteProp->SetPropertyValue(PropertyPtr, ByteValue);
        bSetSuccess = true;
        UE_LOG(LogVibeUEReflection, Warning, TEXT("Set byte property %s = %d"), *PropertyName, ByteValue);
    }
    else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
    {
        // Enhanced: Handle enum properties by name or value
        if (UEnum* Enum = EnumProp->GetEnum())
        {
            int64 EnumValue = Enum->GetValueByName(*PropertyValue);
            if (EnumValue == INDEX_NONE)
            {
                // Try to parse as integer
                EnumValue = FCString::Atoi64(*PropertyValue);
            }
            EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(PropertyPtr, EnumValue);
            bSetSuccess = true;
            UE_LOG(LogVibeUEReflection, Warning, TEXT("Set enum property %s = %s (%lld)"), *PropertyName, *PropertyValue, EnumValue);
        }
        else
        {
            ErrorMessage = TEXT("Failed to get enum definition");
        }
    }
    else
    {
        ErrorMessage = FString::Printf(TEXT("Unsupported property type: %s"), *Property->GetClass()->GetName());
        UE_LOG(LogVibeUEReflection, Warning, TEXT("Unsupported property type for %s: %s"), *PropertyName, *Property->GetClass()->GetName());
    }
    
    if (bSetSuccess)
    {
        // Enhanced: Mark the blueprint as modified
        if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(Node))
        {
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        }
        
        Result->SetBoolField("success", true);
        Result->SetStringField("property_name", PropertyName);
        Result->SetStringField("property_value", PropertyValue);
        Result->SetStringField("property_type", Property->GetClass()->GetName());
        Result->SetStringField("message", "Property set successfully");
    }
    else
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", ErrorMessage.IsEmpty() ? TEXT("Failed to set property") : ErrorMessage);
        Result->SetStringField("property_name", PropertyName);
        Result->SetStringField("property_type", Property->GetClass()->GetName());
    }
    
    return Result;
}

TSharedPtr<FJsonObject> FBlueprintReflection::GetNodePinDetails(UK2Node* Node)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    if (!Node)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Node is null");
        return Result;
    }
    
    // Placeholder - return basic pin info
    Result->SetBoolField("success", true);
    Result->SetNumberField("input_pin_count", Node->Pins.Num());
    Result->SetNumberField("output_pin_count", Node->Pins.Num());
    
    return Result;
}

// === PRIVATE HELPER METHOD IMPLEMENTATIONS ===

bool FBlueprintReflection::ValidateNodeCreation(UBlueprint* Blueprint, const FString& NodeType, const TSharedPtr<FJsonObject>& NodeParams)
{
    if (!Blueprint)
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("Blueprint is not valid"));
        return false;
    }
    
    // Additional validation could be added here based on node type and parameters
    return true;
}

TSharedPtr<FJsonObject> FBlueprintReflection::ReflectNodeProperties(UK2Node* Node)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    if (!Node)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Node is null");
        return Result;
    }
    
    // Placeholder - simplified property reflection
    Result->SetBoolField("success", true);
    Result->SetStringField("node_type", Node->GetClass()->GetName());
    
    return Result;
}

TSharedPtr<FJsonObject> FBlueprintReflection::AnalyzeNodePins(UK2Node* Node)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    if (!Node)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Node is null");
        return Result;
    }
    
    // Placeholder - simplified pin analysis
    Result->SetBoolField("success", true);
    Result->SetNumberField("total_pins", Node->Pins.Num());
    
    return Result;
}

FString FBlueprintReflection::GetPinTypeDescription(const FEdGraphPinType& PinType)
{
    FString TypeDescription = PinType.PinCategory.ToString();
    
    if (PinType.PinSubCategory != NAME_None)
    {
        TypeDescription += FString::Printf(TEXT(" (%s)"), *PinType.PinSubCategory.ToString());
    }
    
    return TypeDescription;
}

// === BLUEPRINT NODE DISCOVERY SYSTEM ===

void FBlueprintReflection::GetBlueprintActionMenuItems(UBlueprint* Blueprint, TArray<TSharedPtr<FEdGraphSchemaAction>>& Actions)
{
    if (!Blueprint || !Blueprint->UbergraphPages.Num()) 
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("Invalid Blueprint or no UbergraphPages"));
        return;
    }
    
    UE_LOG(LogVibeUEReflection, Log, TEXT("Getting filtered Blueprint actions from Action Database for: %s"), *Blueprint->GetName());
    
    // Get the Blueprint Action Database
    FBlueprintActionDatabase& ActionDatabase = FBlueprintActionDatabase::Get();
    const FBlueprintActionDatabase::FActionRegistry& AllActions = ActionDatabase.GetAllActions();
    
    // Define priority categories and limits - EXPANDED for comprehensive discovery
    TMap<FString, int32> CategoryLimits;
    CategoryLimits.Add(TEXT("Flow Control"), 50);          // Branch, Sequence, ForEach, etc. (doubled)
    CategoryLimits.Add(TEXT("Math"), 60);                  // Add, Multiply, etc. (doubled)
    CategoryLimits.Add(TEXT("Utilities"), 40);             // Print String, Delay, etc. (doubled)
    CategoryLimits.Add(TEXT("Input"), 30);                 // Input actions (doubled)
    CategoryLimits.Add(TEXT("Variables"), 30);             // Variable nodes (doubled)
    CategoryLimits.Add(TEXT("Functions"), 40);             // Function calls (doubled)
    CategoryLimits.Add(TEXT("Events"), 30);                // Event nodes (doubled)
    CategoryLimits.Add(TEXT("Components"), 30);            // Component actions (doubled)
    CategoryLimits.Add(TEXT("Actor"), 30);                 // Actor-related nodes (doubled)
    CategoryLimits.Add(TEXT("Gameplay"), 30);              // Gameplay nodes (doubled)
    CategoryLimits.Add(TEXT("String"), 25);                // String operations (new)
    CategoryLimits.Add(TEXT("Array"), 25);                 // Array operations (new)
    CategoryLimits.Add(TEXT("Object"), 25);                // Object operations (new)
    CategoryLimits.Add(TEXT("Blueprint"), 25);             // Blueprint operations (new)
    CategoryLimits.Add(TEXT("Development"), 20);           // Debug/Development (new)
    
    // Track category counts
    TMap<FString, int32> CategoryCounts;
    
    // Essential node types to prioritize - EXPANDED for comprehensive coverage
    TSet<FString> EssentialNodeTypes = {
        // Core Flow Control
        TEXT("K2Node_IfThenElse"),              // Branch
        TEXT("K2Node_ExecutionSequence"),       // Sequence  
        TEXT("K2Node_ForEach"),                 // For Each Loop
        TEXT("K2Node_WhileLoop"),               // While Loop
        TEXT("K2Node_DoOnceMultiInput"),        // Do Once
        TEXT("K2Node_MultiGate"),               // Multi Gate
        TEXT("K2Node_Select"),                  // Select
        TEXT("K2Node_Switch"),                  // Switch
        TEXT("K2Node_SwitchString"),            // Switch on String
        TEXT("K2Node_SwitchInteger"),           // Switch on Int
        TEXT("K2Node_SwitchEnum"),              // Switch on Enum
        
        // Functions and Calls
        TEXT("K2Node_CallFunction"),            // Function Call
        TEXT("K2Node_CallFunctionOnMember"),    // Call Function on Member
        TEXT("K2Node_CallParentFunction"),      // Call Parent Function
        TEXT("K2Node_MacroInstance"),           // Macro Instance
        
        // Variables and Data
        TEXT("K2Node_VariableGet"),             // Get Variable
        TEXT("K2Node_VariableSet"),             // Set Variable
        TEXT("K2Node_Literal"),                 // Literals
        TEXT("K2Node_MakeStruct"),              // Make Struct
        TEXT("K2Node_BreakStruct"),             // Break Struct
        TEXT("K2Node_MakeArray"),               // Make Array
        TEXT("K2Node_GetArrayItem"),            // Get Array Item
        TEXT("K2Node_SetArrayItem"),            // Set Array Item
        
        // Events
        TEXT("K2Node_Event"),                   // Events
        TEXT("K2Node_CustomEvent"),             // Custom Events
        TEXT("K2Node_InputAction"),             // Input Action
        TEXT("K2Node_InputAxisEvent"),          // Input Axis
        TEXT("K2Node_InputKey"),                // Input Key
        TEXT("K2Node_InputTouch"),              // Input Touch
        
        // Object Operations
        TEXT("K2Node_DynamicCast"),             // Cast
        TEXT("K2Node_ClassDynamicCast"),        // Class Cast
        TEXT("K2Node_SpawnActor"),              // Spawn Actor
        TEXT("K2Node_SpawnActorFromClass"),     // Spawn Actor from Class
        TEXT("K2Node_DestroyActor"),            // Destroy Actor
        TEXT("K2Node_CreateDelegate"),          // Create Delegate
        
        // Utilities
        TEXT("K2Node_Timeline"),                // Timeline
        TEXT("K2Node_Delay"),                   // Delay
        TEXT("K2Node_DelayUntilNextTick"),      // Delay Until Next Tick
        TEXT("K2Node_RetriggeredDelay"),        // Retriggered Delay
        
        // Math Operations
        TEXT("K2Node_MathExpression"),          // Math Expression
        TEXT("K2Node_GetEnumeratorValue"),      // Get Enum Value
        TEXT("K2Node_GetEnumeratorName"),       // Get Enum Name
        
        // String Operations  
        TEXT("K2Node_FormatText"),              // Format Text
        TEXT("K2Node_GetSubstring"),            // Get Substring
        
        // Component Operations
        TEXT("K2Node_ComponentBoundEvent"),     // Component Bound Event
        TEXT("K2Node_AddComponent"),            // Add Component
        TEXT("K2Node_GetComponentsByClass"),    // Get Components by Class
        TEXT("K2Node_GetComponentsByTag"),      // Get Components by Tag
    };
    
    // High-priority search terms that should always be included - GREATLY EXPANDED
    TSet<FString> HighPriorityKeywords = {
        // Debug and Logging
        TEXT("print"), TEXT("log"), TEXT("debug"), TEXT("string"), TEXT("display"), TEXT("warning"), TEXT("error"),
        
        // Flow Control
        TEXT("branch"), TEXT("if"), TEXT("condition"), TEXT("else"), TEXT("then"), TEXT("gate"), TEXT("flip"), TEXT("flop"),
        TEXT("loop"), TEXT("for"), TEXT("while"), TEXT("each"), TEXT("sequence"), TEXT("multi"), TEXT("select"), TEXT("switch"),
        TEXT("delay"), TEXT("timer"), TEXT("retriggerable"), TEXT("do"), TEXT("once"), TEXT("retriggered"),
        
        // Math Operations
        TEXT("add"), TEXT("subtract"), TEXT("multiply"), TEXT("divide"), TEXT("power"), TEXT("sqrt"), TEXT("abs"), TEXT("min"), TEXT("max"),
        TEXT("sin"), TEXT("cos"), TEXT("tan"), TEXT("atan"), TEXT("atan2"), TEXT("floor"), TEXT("ceil"), TEXT("round"), TEXT("fmod"),
        TEXT("clamp"), TEXT("lerp"), TEXT("alpha"), TEXT("normalize"), TEXT("dot"), TEXT("cross"), TEXT("distance"), TEXT("length"),
        TEXT("greater"), TEXT("less"), TEXT("equal"), TEXT("not"), TEXT("and"), TEXT("or"), TEXT("xor"), TEXT("nand"),
        
        // Data Types
        TEXT("string"), TEXT("text"), TEXT("name"), TEXT("number"), TEXT("int"), TEXT("integer"), TEXT("float"), TEXT("byte"), TEXT("bool"), TEXT("boolean"),
        TEXT("vector"), TEXT("vector2d"), TEXT("vector4"), TEXT("rotator"), TEXT("transform"), TEXT("location"), TEXT("rotation"), TEXT("scale"),
        TEXT("color"), TEXT("linear"), TEXT("struct"), TEXT("object"), TEXT("class"), TEXT("enum"), TEXT("array"), TEXT("map"), TEXT("set"),
        
        // Input
        TEXT("input"), TEXT("key"), TEXT("mouse"), TEXT("button"), TEXT("axis"), TEXT("action"), TEXT("touch"), TEXT("gesture"),
        TEXT("pressed"), TEXT("released"), TEXT("up"), TEXT("down"), TEXT("click"), TEXT("double"), TEXT("hold"),
        
        // Events 
        TEXT("event"), TEXT("tick"), TEXT("begin"), TEXT("start"), TEXT("end"), TEXT("finish"), TEXT("complete"), TEXT("overlap"),
        TEXT("hit"), TEXT("collision"), TEXT("trigger"), TEXT("custom"), TEXT("bind"), TEXT("delegate"), TEXT("dispatch"),
        TEXT("notify"), TEXT("broadcast"), TEXT("multicast"), TEXT("call"), TEXT("execute"),
        
        // Object Lifecycle
        TEXT("spawn"), TEXT("destroy"), TEXT("create"), TEXT("construct"), TEXT("destruct"), TEXT("delete"), TEXT("remove"),
        TEXT("instantiate"), TEXT("clone"), TEXT("duplicate"), TEXT("copy"), TEXT("reference"),
        
        // Variables and Properties
        TEXT("get"), TEXT("set"), TEXT("variable"), TEXT("property"), TEXT("value"), TEXT("data"), TEXT("field"), TEXT("member"),
        TEXT("increment"), TEXT("decrement"), TEXT("append"), TEXT("prepend"), TEXT("insert"), TEXT("clear"), TEXT("empty"),
        
        // Components and Actors
        TEXT("component"), TEXT("actor"), TEXT("pawn"), TEXT("character"), TEXT("controller"), TEXT("widget"), TEXT("scene"),
        TEXT("mesh"), TEXT("static"), TEXT("skeletal"), TEXT("primitive"), TEXT("collision"), TEXT("physics"), TEXT("movement"),
        TEXT("camera"), TEXT("light"), TEXT("audio"), TEXT("particle"), TEXT("material"), TEXT("texture"),
        
        // Gameplay
        TEXT("damage"), TEXT("health"), TEXT("score"), TEXT("level"), TEXT("game"), TEXT("mode"), TEXT("state"), TEXT("save"),
        TEXT("load"), TEXT("pause"), TEXT("resume"), TEXT("restart"), TEXT("quit"), TEXT("exit"), TEXT("menu"),
        TEXT("inventory"), TEXT("item"), TEXT("pickup"), TEXT("weapon"), TEXT("ammo"), TEXT("power"), TEXT("ability"),
        
        // Utility Functions
        TEXT("format"), TEXT("convert"), TEXT("parse"), TEXT("split"), TEXT("join"), TEXT("contains"), TEXT("find"), TEXT("replace"),
        TEXT("substring"), TEXT("length"), TEXT("size"), TEXT("count"), TEXT("index"), TEXT("valid"), TEXT("null"), TEXT("none"),
        TEXT("random"), TEXT("seed"), TEXT("probability"), TEXT("chance"), TEXT("range"), TEXT("map"), TEXT("remap"),
        
        // Animation and Timeline
        TEXT("timeline"), TEXT("curve"), TEXT("animate"), TEXT("tween"), TEXT("ease"), TEXT("smooth"), TEXT("interpolate"),
        TEXT("keyframe"), TEXT("track"), TEXT("montage"), TEXT("blend"), TEXT("transition"),
        
        // Networking 
        TEXT("replicate"), TEXT("server"), TEXT("client"), TEXT("authority"), TEXT("remote"), TEXT("rpc"), TEXT("reliable"),
        TEXT("multicast"), TEXT("owning"), TEXT("connection"), TEXT("session"),
        
        // File and Data
        TEXT("file"), TEXT("save"), TEXT("load"), TEXT("read"), TEXT("write"), TEXT("json"), TEXT("config"), TEXT("settings"),
        TEXT("serialize"), TEXT("deserialize"), TEXT("export"), TEXT("import")
    };
    
    // Process each action from the database with smart filtering
    int32 ActionCount = 0;
    int32 TotalProcessed = 0;
    const int32 MAX_ACTIONS = 3000;        // Increased from 300 for comprehensive coverage
    const int32 MAX_PROCESS = 5000;       // Increased from 2000 for full Blueprint database access
    
    for (auto& ActionEntry : AllActions)
    {
        // PERFORMANCE OPTIMIZATION: Stop processing if we have enough actions or processed too many
        if (ActionCount >= MAX_ACTIONS || TotalProcessed >= MAX_PROCESS)
        {
            UE_LOG(LogVibeUEReflection, Warning, TEXT("Early exit: ActionCount=%d, TotalProcessed=%d"), ActionCount, TotalProcessed);
            break;
        }
        
        const FBlueprintActionDatabase::FActionList& ActionList = ActionEntry.Value;
        
        for (UBlueprintNodeSpawner* NodeSpawner : ActionList)
        {
            TotalProcessed++;
            
            // Additional safety check within inner loop
            if (ActionCount >= MAX_ACTIONS || TotalProcessed >= MAX_PROCESS)
            {
                break;
            }
            if (NodeSpawner && NodeSpawner->NodeClass)
            {
                FString NodeClassName = NodeSpawner->NodeClass->GetName();
                FString DisplayName = NodeClassName;
                FString Category = TEXT("Other");
                FString Tooltip = FString::Printf(TEXT("Blueprint node: %s"), *NodeClassName);
                FString Keywords = TEXT("");
                
                // Try to get better display name from spawner
                if (NodeSpawner->DefaultMenuSignature.MenuName.ToString().Len() > 0)
                {
                    DisplayName = NodeSpawner->DefaultMenuSignature.MenuName.ToString();
                }
                
                // Try to get better category from spawner
                if (NodeSpawner->DefaultMenuSignature.Category.ToString().Len() > 0)
                {
                    Category = NodeSpawner->DefaultMenuSignature.Category.ToString();
                }
                
                // Try to get better tooltip from spawner
                if (NodeSpawner->DefaultMenuSignature.Tooltip.ToString().Len() > 0)
                {
                    Tooltip = NodeSpawner->DefaultMenuSignature.Tooltip.ToString();
                }
                
                // Try to get keywords from spawner
                if (NodeSpawner->DefaultMenuSignature.Keywords.ToString().Len() > 0)
                {
                    Keywords = NodeSpawner->DefaultMenuSignature.Keywords.ToString();
                }
                
                // Smart filtering based on category, node type, and keywords
                bool bShouldInclude = false;
                
                // Always include essential node types
                if (EssentialNodeTypes.Contains(NodeClassName))
                {
                    bShouldInclude = true;
                }
                // Include if it contains high-priority keywords
                else if (ContainsHighPriorityKeywords(DisplayName, Keywords, HighPriorityKeywords))
                {
                    bShouldInclude = true;
                }
                // Check category limits
                else if (CategoryLimits.Contains(Category))
                {
                    int32 CurrentCount = CategoryCounts.FindOrAdd(Category, 0);
                    if (CurrentCount < CategoryLimits[Category])
                    {
                        bShouldInclude = true;
                    }
                }
                // Include a few from other categories to maintain variety
                else
                {
                    int32 OtherCount = CategoryCounts.FindOrAdd(TEXT("Other"), 0);
                    if (OtherCount < 20)  // Max 20 miscellaneous nodes
                    {
                        bShouldInclude = true;
                        int32& Count = CategoryCounts.FindOrAdd(TEXT("Other"), 0);
                        Count++;
                    }
                }
                
                if (bShouldInclude)
                {
                    TSharedPtr<FEdGraphSchemaAction> NewAction = MakeShared<FEdGraphSchemaAction>(
                        FText::FromString(Category),           // Category
                        FText::FromString(DisplayName),        // MenuDescription  
                        FText::FromString(Tooltip),           // TooltipDescription
                        0,                                     // Grouping
                        FText::FromString(Keywords)           // Keywords (properly converted to FText)
                    );
                    
                    Actions.Add(NewAction);
                    ActionCount++;
                    
                    // Update category count - use FindOrAdd for safe access
                    if (CategoryLimits.Contains(Category))
                    {
                        int32& Count = CategoryCounts.FindOrAdd(Category, 0);
                        Count++;
                    }
                }
            }
        }
    }
    
    UE_LOG(LogVibeUEReflection, Warning, TEXT("Filtered Blueprint actions: %d selected from %d total for: %s"), ActionCount, TotalProcessed, *Blueprint->GetName());
}

// Helper function to check for high-priority keywords
bool FBlueprintReflection::ContainsHighPriorityKeywords(const FString& DisplayName, const FString& Keywords, const TSet<FString>& HighPriorityKeywords)
{
    FString SearchText = DisplayName.ToLower() + TEXT(" ") + Keywords.ToLower();
    
    for (const FString& Keyword : HighPriorityKeywords)
    {
        if (SearchText.Contains(Keyword.ToLower()))
        {
            return true;
        }
    }
    return false;
}

// Helper function to calculate search relevance score like the Unreal Editor
int32 FBlueprintReflection::CalculateSearchRelevance(const FString& ActionName, const FString& Keywords, const FString& Tooltip, const FString& SearchTerm)
{
    if (SearchTerm.IsEmpty())
    {
        return 50; // Default relevance for no search
    }
    
    int32 Score = 0;
    FString LowerSearchTerm = SearchTerm.ToLower();
    
    // Exact match in name gets highest score
    if (ActionName.ToLower().Equals(LowerSearchTerm))
    {
        Score += 100;
    }
    // Starts with search term gets high score
    else if (ActionName.ToLower().StartsWith(LowerSearchTerm))
    {
        Score += 80;
    }
    // Contains search term gets medium score
    else if (ActionName.ToLower().Contains(LowerSearchTerm))
    {
        Score += 60;
    }
    
    // Keywords match
    if (Keywords.ToLower().Contains(LowerSearchTerm))
    {
        Score += 40;
    }
    
    // Tooltip match
    if (Tooltip.ToLower().Contains(LowerSearchTerm))
    {
        Score += 20;
    }
    
    return Score;
}

UK2Node* FBlueprintReflection::CreateNodeFromIdentifier(UBlueprint* Blueprint, const FString& NodeIdentifier, const TSharedPtr<FJsonObject>& Config)
{
    if (!Blueprint)
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("CreateNodeFromIdentifier: Invalid Blueprint"));
        return nullptr;
    }
    
    UEdGraph* EventGraph = Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
    if (!EventGraph)
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("CreateNodeFromIdentifier: No EventGraph found"));
        return nullptr;
    }
    
    UK2Node* NewNode = nullptr;
    
    UE_LOG(LogVibeUEReflection, Log, TEXT("Creating node from identifier: %s"), *NodeIdentifier);
    
    // ENHANCED: Use Blueprint Action Database to find and create the exact node requested
    TArray<TSharedPtr<FEdGraphSchemaAction>> AllActions;
    GetBlueprintActionMenuItems(Blueprint, AllActions);
    
    // Search for matching action by name
    TSharedPtr<FEdGraphSchemaAction> MatchedAction;
    for (const auto& Action : AllActions)
    {
        if (Action.IsValid())
        {
            FString ActionName = Action->GetMenuDescription().ToString();
            
            // Try exact match first
            if (ActionName.Equals(NodeIdentifier, ESearchCase::IgnoreCase))
            {
                MatchedAction = Action;
                UE_LOG(LogVibeUEReflection, Log, TEXT("Found exact match for node: %s"), *ActionName);
                break;
            }
            // Try contains match
            else if (ActionName.Contains(NodeIdentifier) || NodeIdentifier.Contains(ActionName))
            {
                MatchedAction = Action;
                UE_LOG(LogVibeUEReflection, Log, TEXT("Found partial match for node: %s -> %s"), *NodeIdentifier, *ActionName);
            }
        }
    }
    
    // If we found a matching action, try to spawn it
    if (MatchedAction.IsValid())
    {
        // Skip complex action spawning for now - use fallback creation
        UE_LOG(LogVibeUEReflection, Log, TEXT("Found action but using fallback creation for: %s"), *NodeIdentifier);
    }
    
    // FALLBACK: Use pattern-based creation for common cases
    if (NodeIdentifier.Contains(TEXT("Print")) || NodeIdentifier.Contains(TEXT("String")))
    {
        // Create a function call node for Print String
        UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(EventGraph);
        UFunction* PrintStringFunc = UKismetSystemLibrary::StaticClass()->FindFunctionByName("PrintString");
        if (PrintStringFunc)
        {
            FuncNode->SetFromFunction(PrintStringFunc);
        }
        NewNode = FuncNode;
    }
    else if (NodeIdentifier.StartsWith(TEXT("Get ")) || NodeIdentifier.StartsWith(TEXT("Set ")))
    {
        // Variable nodes
        bool bIsGetter = NodeIdentifier.StartsWith(TEXT("Get "));
        if (bIsGetter)
        {
            NewNode = NewObject<UK2Node_VariableGet>(EventGraph);
        }
        else
        {
            NewNode = NewObject<UK2Node_VariableSet>(EventGraph);
        }
    }
    else if (NodeIdentifier.Contains(TEXT("Branch")) || NodeIdentifier.Contains(TEXT("If")))
    {
        // Branch node
        NewNode = NewObject<UK2Node_IfThenElse>(EventGraph);
    }
    else if (NodeIdentifier.Contains(TEXT("Sequence")))
    {
        // Sequence node - commenting out for now due to missing header
        // NewNode = NewObject<UK2Node_ExecutionSequence>(EventGraph);
        UE_LOG(LogVibeUEReflection, Warning, TEXT("Sequence node creation not implemented yet"));
    }
    else if (NodeIdentifier.Contains(TEXT("Event")))
    {
        // Custom event node
        NewNode = NewObject<UK2Node_Event>(EventGraph);
    }
    else
    {
        // Default to creating a basic function call node
        NewNode = NewObject<UK2Node_CallFunction>(EventGraph);
    }
    
    if (NewNode)
    {
        EventGraph->AddNode(NewNode, true);
        UE_LOG(LogVibeUEReflection, Log, TEXT("Successfully created node: %s"), *NewNode->GetClass()->GetName());
    }
    
    return NewNode;
}

TSharedPtr<FJsonObject> FBlueprintReflection::ProcessActionToJson(TSharedPtr<FEdGraphSchemaAction> Action)
{
    if (!Action.IsValid())
    {
        return nullptr;
    }
    
    TSharedPtr<FJsonObject> ActionInfo = MakeShared<FJsonObject>();
    
    // Basic action information - only use methods we know exist
    ActionInfo->SetStringField(TEXT("name"), Action->GetMenuDescription().ToString());
    ActionInfo->SetStringField(TEXT("category"), Action->GetCategory().ToString());
    ActionInfo->SetStringField(TEXT("description"), Action->GetTooltipDescription().ToString());
    ActionInfo->SetStringField(TEXT("keywords"), Action->GetKeywords().ToString());
    ActionInfo->SetStringField(TEXT("section_id"), FString::Printf(TEXT("%d"), Action->GetSectionID()));
    
    // Get action class name - use generic since GetClass() is not accessible
    ActionInfo->SetStringField(TEXT("action_class"), TEXT("FEdGraphSchemaAction"));
    
    // Try to determine action type from menu description only
    FString MenuDescription = Action->GetMenuDescription().ToString();
    
    if (MenuDescription.Contains(TEXT("(")))
    {
        ActionInfo->SetStringField(TEXT("type"), TEXT("function"));
    }
    else if (MenuDescription.StartsWith(TEXT("Get ")) || MenuDescription.StartsWith(TEXT("Set ")))
    {
        ActionInfo->SetStringField(TEXT("type"), TEXT("variable"));
        ActionInfo->SetBoolField(TEXT("is_getter"), MenuDescription.StartsWith(TEXT("Get ")));
        
        // Extract variable name
        FString VarName = MenuDescription;
        if (VarName.StartsWith(TEXT("Get ")) || VarName.StartsWith(TEXT("Set ")))
        {
            VarName = VarName.Mid(4); // Remove "Get " or "Set "
        }
        ActionInfo->SetStringField(TEXT("variable_name"), VarName);
    }
    else if (MenuDescription.Contains(TEXT("Event")))
    {
        ActionInfo->SetStringField(TEXT("type"), TEXT("event"));
    }
    else
    {
        ActionInfo->SetStringField(TEXT("type"), TEXT("node"));
    }
    
    return ActionInfo;
}

TSharedPtr<FJsonObject> FBlueprintReflectionCommands::HandleGetAvailableBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogVibeUEReflection, Log, TEXT("HandleGetAvailableBlueprintNodes called"));
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    
    // Extract parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Missing blueprint_name parameter"));
        return Result;
    }
    
    FString Category = Params->GetStringField(TEXT("category"));
    FString SearchTerm = Params->GetStringField(TEXT("search_term")); 
    int32 MaxResults = Params->GetIntegerField(TEXT("max_results"));
    if (MaxResults <= 0) MaxResults = 50; // Default limit to prevent timeouts
    
    UE_LOG(LogVibeUEReflection, Log, TEXT("Search params - Category: '%s', SearchTerm: '%s', MaxResults: %d"), *Category, *SearchTerm, MaxResults);
    
    // Find the Blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
        return Result;
    }
    
    // NEW OPTIMIZED SEARCH: Only search when we have a search term, limit results
    TArray<TSharedPtr<FEdGraphSchemaAction>> FoundActions;
    
    if (!SearchTerm.IsEmpty())
    {
        // Use targeted search instead of loading everything
        FBlueprintReflection::GetFilteredBlueprintActions(Blueprint, SearchTerm, Category, MaxResults, FoundActions);
    }
    else
    {
        // When no search term, return a curated list of common actions
        FBlueprintReflection::GetCommonBlueprintActions(Blueprint, Category, MaxResults, FoundActions);
    }
    
    UE_LOG(LogVibeUEReflection, Log, TEXT("Found %d actions for search term '%s'"), FoundActions.Num(), *SearchTerm);
    
    // Organize actions by category
    TMap<FString, TArray<TSharedPtr<FJsonValue>>> CategoryMap;
    int32 TotalNodes = 0;
    
    for (auto& Action : FoundActions)
    {
        if (!Action.IsValid()) continue;
        
        // Process action to JSON
        TSharedPtr<FJsonObject> ActionJson = FBlueprintReflection::ProcessActionToJson(Action);
        if (!ActionJson.IsValid()) continue;
        
        FString ActionCategory = ActionJson->GetStringField(TEXT("category"));
        FString ActionName = ActionJson->GetStringField(TEXT("name"));
        
        // Add to appropriate category
        if (!CategoryMap.Contains(ActionCategory))
        {
            CategoryMap.Add(ActionCategory, TArray<TSharedPtr<FJsonValue>>());
        }
        
        CategoryMap[ActionCategory].Add(MakeShared<FJsonValueObject>(ActionJson));
        TotalNodes++;
        
        UE_LOG(LogVibeUEReflection, VeryVerbose, TEXT("Added action: %s in category: %s"), *ActionName, *ActionCategory);
    }
    
    // Build result structure
    TSharedPtr<FJsonObject> Categories = MakeShared<FJsonObject>();
    for (auto& CategoryPair : CategoryMap)
    {
        Categories->SetArrayField(CategoryPair.Key, CategoryPair.Value);
    }
    
    Result->SetObjectField(TEXT("categories"), Categories);
    Result->SetNumberField(TEXT("total_nodes"), TotalNodes);
    Result->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Result->SetBoolField(TEXT("success"), true);
    
    UE_LOG(LogVibeUEReflection, Log, TEXT("Discovered %d nodes in %d categories for Blueprint: %s"), TotalNodes, CategoryMap.Num(), *BlueprintName);
    
    return Result;
}

// === BLUEPRINT REFLECTION COMMANDS IMPLEMENTATION ===

FBlueprintReflectionCommands::FBlueprintReflectionCommands()
{
    // Constructor - no initialization needed for now
}

TSharedPtr<FJsonObject> FBlueprintReflectionCommands::HandleAddBlueprintNode(const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogVibeUEReflection, Warning, TEXT("HandleAddBlueprintNode called - using enhanced reflection system"));
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    
    // Extract parameters with better validation and guidance
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        UE_LOG(LogVibeUEReflection, Error, TEXT("Missing blueprint_name parameter"));
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Missing blueprint_name parameter. Use full asset path like '/Game/Blueprints/Actors/BP_Heart.BP_Heart'"));
        Result->SetStringField(TEXT("usage_hint"), TEXT("Blueprint name should be a full asset path, not just a simple name"));
        return Result;
    }
    UE_LOG(LogVibeUEReflection, Warning, TEXT("Blueprint path: %s"), *BlueprintName);
    
    FString NodeIdentifier;
    if (!Params->TryGetStringField(TEXT("node_type"), NodeIdentifier))
    {
        UE_LOG(LogVibeUEReflection, Error, TEXT("Missing node_type parameter"));
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Missing node_type parameter. Use node types like 'Branch', 'Print String', 'GetVariable', 'SetVariable', 'Self', etc."));
        Result->SetStringField(TEXT("usage_hint"), TEXT("Node type should be a descriptive name like 'Branch' or 'Print String'"));
        return Result;
    }
    UE_LOG(LogVibeUEReflection, Warning, TEXT("Node type: %s"), *NodeIdentifier);
    
    // Extract position parameters with better defaults
    float PosX = 500.0f, PosY = 500.0f;  // Default position
    const TSharedPtr<FJsonObject>* NodeParamsPtr;
    if (Params->TryGetObjectField(TEXT("node_params"), NodeParamsPtr) && NodeParamsPtr)
    {
        const TArray<TSharedPtr<FJsonValue>>* PositionArrayPtr;
        if ((*NodeParamsPtr)->TryGetArrayField(TEXT("position"), PositionArrayPtr) && PositionArrayPtr && PositionArrayPtr->Num() >= 2)
        {
            PosX = (*PositionArrayPtr)[0]->AsNumber();
            PosY = (*PositionArrayPtr)[1]->AsNumber();
            UE_LOG(LogVibeUEReflection, Warning, TEXT("Position: (%f, %f)"), PosX, PosY);
        }
    }
    // Also check for direct position array parameter
    else 
    {
        const TArray<TSharedPtr<FJsonValue>>* DirectPositionPtr;
        if (Params->TryGetArrayField(TEXT("position"), DirectPositionPtr) && DirectPositionPtr && DirectPositionPtr->Num() >= 2)
        {
            PosX = (*DirectPositionPtr)[0]->AsNumber();
            PosY = (*DirectPositionPtr)[1]->AsNumber();
            UE_LOG(LogVibeUEReflection, Warning, TEXT("Direct position: (%f, %f)"), PosX, PosY);
        }
    }
    
    // Try to load the Blueprint with better path handling
    UBlueprint* Blueprint = nullptr;
    
    // Handle both simple names and full paths with better guidance
    FString AssetPath = BlueprintName;
    
    // PREFER FULL PATHS: If it looks like a full path, use it directly
    if (BlueprintName.Contains(TEXT("/Game/")))
    {
        // Full path provided - use directly
        UE_LOG(LogVibeUEReflection, Log, TEXT("Using provided full path: %s"), *AssetPath);
        Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
    }
    else if (!BlueprintName.Contains(TEXT("/")) && !BlueprintName.Contains(TEXT(".")))
    {
        // Simple name - try to find it in common locations (DISCOURAGED)
        UE_LOG(LogVibeUEReflection, Warning, TEXT("Using simple name '%s' - recommend using full asset paths instead"), *BlueprintName);
        
        TArray<FString> SearchPaths = {
            FString::Printf(TEXT("/Game/Blueprints/Characters/%s.%s"), *BlueprintName, *BlueprintName),
            FString::Printf(TEXT("/Game/Blueprints/Actors/%s.%s"), *BlueprintName, *BlueprintName),
            FString::Printf(TEXT("/Game/Blueprints/%s.%s"), *BlueprintName, *BlueprintName),
            FString::Printf(TEXT("/Game/%s.%s"), *BlueprintName, *BlueprintName)
        };
        
        for (const FString& SearchPath : SearchPaths)
        {
            UE_LOG(LogVibeUEReflection, Log, TEXT("Trying to load Blueprint at: %s"), *SearchPath);
            Blueprint = LoadObject<UBlueprint>(nullptr, *SearchPath);
            if (Blueprint)
            {
                AssetPath = SearchPath;
                UE_LOG(LogVibeUEReflection, Warning, TEXT("Found Blueprint at: %s"), *AssetPath);
                break;
            }
        }
    }
    else
    {
        // Partial path provided - try to use it
        UE_LOG(LogVibeUEReflection, Log, TEXT("Trying to load Blueprint with partial path: %s"), *AssetPath);
        Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
    }
    
    if (!Blueprint)
    {
        FString ErrorMsg = FString::Printf(TEXT("Could not load Blueprint: %s"), *AssetPath);
        UE_LOG(LogVibeUEReflection, Error, TEXT("%s"), *ErrorMsg);
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), ErrorMsg);
        Result->SetStringField(TEXT("suggestion"), TEXT("Use full asset path like '/Game/Blueprints/Actors/BP_Heart.BP_Heart'"));
        Result->SetStringField(TEXT("usage_hint"), TEXT("Search for available Blueprints first using search_items with asset_type='Blueprint'"));
        return Result;
    }
    
    UE_LOG(LogVibeUEReflection, Warning, TEXT("Blueprint loaded successfully: %s"), *Blueprint->GetName());
    
    // Get the event graph
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
        UE_LOG(LogVibeUEReflection, Error, TEXT("Could not find EventGraph in Blueprint: %s"), *Blueprint->GetName());
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Could not find EventGraph in Blueprint"));
        return Result;
    }
    
    UE_LOG(LogVibeUEReflection, Warning, TEXT("EventGraph found: %s"), *EventGraph->GetName());
    
    // ENHANCED: Prioritize reflection system over hardcoded node creation
    UEdGraphNode* NewNode = nullptr;
    FString NodeId;
    
    UE_LOG(LogVibeUEReflection, Warning, TEXT("Creating node '%s' - trying reflection system first"), *NodeIdentifier);
    
    try 
    {
        // FIRST: Try the true reflection-based creation system for ALL node types
        TSharedPtr<FJsonObject> NodeParamsJson = NodeParamsPtr ? *NodeParamsPtr : MakeShareable(new FJsonObject);
        
        // Add position to node params for reflection system
        TArray<TSharedPtr<FJsonValue>> PositionArray;
        PositionArray.Add(MakeShared<FJsonValueNumber>(PosX));
        PositionArray.Add(MakeShared<FJsonValueNumber>(PosY));
        NodeParamsJson->SetArrayField(TEXT("position"), PositionArray);
        
        TSharedPtr<FJsonObject> ReflectionResult = FBlueprintReflection::CreateBlueprintNode(Blueprint, NodeIdentifier, NodeParamsJson);
        
        if (ReflectionResult.IsValid() && ReflectionResult->GetBoolField(TEXT("success")))
        {
            // Success! Extract the created node info
            FString ReflectionNodeId = ReflectionResult->GetStringField(TEXT("node_id"));
            UE_LOG(LogVibeUEReflection, Warning, TEXT("Successfully created node via REFLECTION SYSTEM: %s (ID: %s)"), *NodeIdentifier, *ReflectionNodeId);
            
            Result->SetBoolField(TEXT("success"), true);
            Result->SetStringField(TEXT("node_type"), NodeIdentifier);
            Result->SetStringField(TEXT("node_id"), ReflectionNodeId);
            Result->SetStringField(TEXT("creation_method"), TEXT("reflection_system"));
            Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Successfully created %s node via reflection system in Blueprint %s"), *NodeIdentifier, *Blueprint->GetName()));
            return Result;
        }
        
        // FALLBACK: Use hardcoded creation for common node types if reflection fails
        UE_LOG(LogVibeUEReflection, Warning, TEXT("Reflection system failed for '%s', trying hardcoded fallback"), *NodeIdentifier);
        
        if (NodeIdentifier == TEXT("Branch"))
        {
            // Create a Branch node
            UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(EventGraph);
            if (BranchNode)
            {
                BranchNode->NodePosX = PosX;
                BranchNode->NodePosY = PosY;
                BranchNode->AllocateDefaultPins();
                EventGraph->AddNode(BranchNode, true);
                NewNode = BranchNode;
                NodeId = FString::Printf(TEXT("branch_%s"), *LexToString(BranchNode->NodeGuid));
                UE_LOG(LogVibeUEReflection, Warning, TEXT("Created Branch node at (%f, %f) with GUID: %s"), PosX, PosY, *LexToString(BranchNode->NodeGuid));
            }
        }
        else if (NodeIdentifier == TEXT("Print String"))
        {
            // Create a Print String node
            UK2Node_CallFunction* PrintNode = NewObject<UK2Node_CallFunction>(EventGraph);
            if (PrintNode)
            {
                PrintNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString), UKismetSystemLibrary::StaticClass());
                PrintNode->NodePosX = PosX;
                PrintNode->NodePosY = PosY;
                PrintNode->AllocateDefaultPins();
                EventGraph->AddNode(PrintNode, true);
                NewNode = PrintNode;
                NodeId = FString::Printf(TEXT("print_%s"), *LexToString(PrintNode->NodeGuid));
                UE_LOG(LogVibeUEReflection, Warning, TEXT("Created Print String node at (%f, %f) with GUID: %s"), PosX, PosY, *LexToString(PrintNode->NodeGuid));
            }
        }
        else if (NodeIdentifier == TEXT("Cast To Object"))
        {
            // Create a Cast node (generic cast)
            UK2Node_DynamicCast* CastNode = NewObject<UK2Node_DynamicCast>(EventGraph);
            if (CastNode)
            {
                // Set target class to UObject as default
                CastNode->TargetType = UObject::StaticClass();
                CastNode->NodePosX = PosX;
                CastNode->NodePosY = PosY;
                CastNode->AllocateDefaultPins();
                EventGraph->AddNode(CastNode, true);
                NewNode = CastNode;
                NodeId = FString::Printf(TEXT("cast_%s"), *LexToString(CastNode->NodeGuid));
                UE_LOG(LogVibeUEReflection, Warning, TEXT("Created Cast To Object node at (%f, %f) with GUID: %s"), PosX, PosY, *LexToString(CastNode->NodeGuid));
            }
        }
        else if (NodeIdentifier == TEXT("GetVariable"))
        {
            // Create a Get Variable node
            UK2Node_VariableGet* GetVariableNode = NewObject<UK2Node_VariableGet>(EventGraph);
            if (GetVariableNode)
            {
                GetVariableNode->NodePosX = PosX;
                GetVariableNode->NodePosY = PosY;
                
                // Configure the variable reference using existing NodeParamsPtr
                if (NodeParamsPtr)
                {
                    FBlueprintReflection::ConfigureVariableNode(GetVariableNode, *NodeParamsPtr);
                }
                
                GetVariableNode->AllocateDefaultPins();
                EventGraph->AddNode(GetVariableNode, true);
                NewNode = GetVariableNode;
                NodeId = FString::Printf(TEXT("getvar_%s"), *LexToString(GetVariableNode->NodeGuid));
                UE_LOG(LogVibeUEReflection, Warning, TEXT("Created GetVariable node at (%f, %f) with GUID: %s"), PosX, PosY, *LexToString(GetVariableNode->NodeGuid));
            }
        }
        else if (NodeIdentifier == TEXT("SetVariable"))
        {
            // Create a Set Variable node
            UK2Node_VariableSet* SetVariableNode = NewObject<UK2Node_VariableSet>(EventGraph);
            if (SetVariableNode)
            {
                SetVariableNode->NodePosX = PosX;
                SetVariableNode->NodePosY = PosY;
                
                // Configure the variable reference using existing NodeParamsPtr
                if (NodeParamsPtr)
                {
                    FBlueprintReflection::ConfigureVariableSetNode(SetVariableNode, *NodeParamsPtr);
                }
                
                SetVariableNode->AllocateDefaultPins();
                EventGraph->AddNode(SetVariableNode, true);
                NewNode = SetVariableNode;
                NodeId = FString::Printf(TEXT("setvar_%s"), *LexToString(SetVariableNode->NodeGuid));
                UE_LOG(LogVibeUEReflection, Warning, TEXT("Created SetVariable node at (%f, %f) with GUID: %s"), PosX, PosY, *LexToString(SetVariableNode->NodeGuid));
            }
        }
        else if (NodeIdentifier == TEXT("Self"))
        {
            // Create a Self reference node (K2Node_Self)
            UK2Node_Self* SelfNode = NewObject<UK2Node_Self>(EventGraph);
            if (SelfNode)
            {
                SelfNode->NodePosX = PosX;
                SelfNode->NodePosY = PosY;
                
                SelfNode->AllocateDefaultPins();
                EventGraph->AddNode(SelfNode, true);
                NewNode = SelfNode;
                NodeId = FString::Printf(TEXT("self_%s"), *LexToString(SelfNode->NodeGuid));
                UE_LOG(LogVibeUEReflection, Warning, TEXT("Created Self reference node at (%f, %f) with GUID: %s"), PosX, PosY, *LexToString(SelfNode->NodeGuid));
            }
        }
        else
        {
            // No hardcoded fallback available
            FString ErrorMsg = FString::Printf(TEXT("Node type '%s' not implemented in hardcoded fallbacks and reflection system failed"), *NodeIdentifier);
            UE_LOG(LogVibeUEReflection, Warning, TEXT("%s"), *ErrorMsg);
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), ErrorMsg);
            Result->SetStringField(TEXT("suggestion"), TEXT("Try using exact node names from get_available_blueprint_nodes"));
            return Result;
        }
        
        if (NewNode)
        {
            // Mark Blueprint as modified
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
            
            // Refresh and compile the Blueprint
            FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
            FKismetEditorUtilities::CompileBlueprint(Blueprint);
            
            UE_LOG(LogVibeUEReflection, Warning, TEXT("Successfully created and added node '%s' to Blueprint '%s' via HARDCODED FALLBACK - NodeId: %s"), *NodeIdentifier, *Blueprint->GetName(), *NodeId);
            
            Result->SetBoolField(TEXT("success"), true);
            Result->SetStringField(TEXT("node_type"), NodeIdentifier);
            Result->SetStringField(TEXT("node_id"), NodeId);
            Result->SetStringField(TEXT("creation_method"), TEXT("hardcoded_fallback"));
            Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Successfully created %s node via hardcoded fallback in Blueprint %s at position (%f, %f)"), *NodeIdentifier, *Blueprint->GetName(), PosX, PosY));
        }
        else
        {
            FString ErrorMsg = FString::Printf(TEXT("Failed to create node of type: %s"), *NodeIdentifier);
            UE_LOG(LogVibeUEReflection, Error, TEXT("%s"), *ErrorMsg);
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), ErrorMsg);
            Result->SetStringField(TEXT("suggestion"), TEXT("Check available node types using get_available_blueprint_nodes"));
        }
    }
    catch (const std::exception& e)
    {
        FString ErrorMsg = FString::Printf(TEXT("Exception during node creation: %s"), ANSI_TO_TCHAR(e.what()));
        UE_LOG(LogVibeUEReflection, Error, TEXT("%s"), *ErrorMsg);
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), ErrorMsg);
        Result->SetStringField(TEXT("suggestion"), TEXT("Check Blueprint path and node type parameters"));
    }
    catch (...)
    {
        FString ErrorMsg = TEXT("Unknown exception during node creation");
        UE_LOG(LogVibeUEReflection, Error, TEXT("%s"), *ErrorMsg);
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), ErrorMsg);
        Result->SetStringField(TEXT("suggestion"), TEXT("Verify Blueprint asset path and node type are correct"));
    }
    
    return Result;
}

TSharedPtr<FJsonObject> FBlueprintReflectionCommands::HandleSetBlueprintNodeProperty(const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return CreateErrorResponse("Invalid parameters provided");
    }
    
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return CreateErrorResponse("Missing blueprint_name parameter");
    }
    
    FString NodeId;
    if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
    {
        return CreateErrorResponse("Missing node_id parameter");
    }
    
    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return CreateErrorResponse("Missing property_name parameter");
    }
    
    FString PropertyValue;
    if (!Params->TryGetStringField(TEXT("property_value"), PropertyValue))
    {
        return CreateErrorResponse("Missing property_value parameter");
    }
    
    UBlueprint* Blueprint = FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
    }
    
    UK2Node* Node = FindNodeInBlueprint(Blueprint, NodeId);
    if (!Node)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Node '%s' not found in blueprint"), *NodeId));
    }
    
    // Use the reflection system to set property
    return FBlueprintReflection::SetNodeProperty(Node, PropertyName, PropertyValue);
}

TSharedPtr<FJsonObject> FBlueprintReflectionCommands::HandleGetBlueprintNodeProperty(const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return CreateErrorResponse(TEXT("Invalid parameters"));
    }
    
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }
    
    FString NodeId;
    if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
    {
        return CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
    }
    
    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }
    
    UBlueprint* Blueprint = FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
    }
    
    UK2Node* Node = FindNodeInBlueprint(Blueprint, NodeId);
    if (!Node)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Node '%s' not found in blueprint"), *NodeId));
    }
    
    // Use the reflection system to get property
    return FBlueprintReflection::GetNodeProperty(Node, PropertyName);
}

TSharedPtr<FJsonObject> FBlueprintReflectionCommands::HandleGetEnhancedNodeDetails(const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return CreateErrorResponse("Invalid parameters provided");
    }
    
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return CreateErrorResponse("Missing blueprint_name parameter");
    }
    
    FString NodeId;
    if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
    {
        return CreateErrorResponse("Missing node_id parameter");
    }
    
    UBlueprint* Blueprint = FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
    }
    
    UK2Node* Node = FindNodeInBlueprint(Blueprint, NodeId);
    if (!Node)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Node '%s' not found in blueprint"), *NodeId));
    }
    
    // Get comprehensive node details using reflection
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    // Basic node info
    TSharedPtr<FJsonObject> NodeInfo = FBlueprintReflection::GetNodeProperties(Node);
    TSharedPtr<FJsonObject> PinInfo = FBlueprintReflection::GetNodePinDetails(Node);
    
    Result->SetBoolField("success", true);
    Result->SetObjectField("node_properties", NodeInfo);
    Result->SetObjectField("pin_details", PinInfo);
    Result->SetStringField("node_id", NodeId);
    Result->SetStringField("blueprint_name", BlueprintName);
    
    return Result;
}

// === HELPER METHODS ===

UBlueprint* FBlueprintReflectionCommands::FindBlueprint(const FString& BlueprintName)
{
    // Use the working implementation from CommonUtils
    return FCommonUtils::FindBlueprint(BlueprintName);
}

UK2Node* FBlueprintReflectionCommands::FindNodeInBlueprint(UBlueprint* Blueprint, const FString& NodeId)
{
    if (!Blueprint)
        return nullptr;
        
    // Node IDs in our system are NodeGuid strings (hex format), not integer UniqueIDs
    // Use the same approach as other commands - check the event graph first
    UEdGraph* EventGraph = FCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (EventGraph)
    {
        for (UEdGraphNode* Node : EventGraph->Nodes)
        {
            if (UK2Node* K2Node = Cast<UK2Node>(Node))
            {
                if (K2Node->NodeGuid.ToString() == NodeId)
                {
                    return K2Node;
                }
            }
        }
    }
    
    // Also search through all other graphs in the blueprint (function graphs, etc.)
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph == EventGraph) continue; // Skip the event graph since we already checked it
        
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (UK2Node* K2Node = Cast<UK2Node>(Node))
            {
                if (K2Node->NodeGuid.ToString() == NodeId)
                {
                    return K2Node;
                }
            }
        }
    }
    
    return nullptr;
}

// NEW: Optimized filtered search that only searches specific terms
void FBlueprintReflection::GetFilteredBlueprintActions(UBlueprint* Blueprint, const FString& SearchTerm, const FString& Category, int32 MaxResults, TArray<TSharedPtr<FEdGraphSchemaAction>>& OutActions)
{
    if (!Blueprint || SearchTerm.IsEmpty())
    {
        return;
    }
    
    UE_LOG(LogVibeUEReflection, Log, TEXT("Performing targeted search for: '%s'"), *SearchTerm);
    
    // For now, let's use a simple approach - check if we have known self-reference nodes
    FString SearchTermLower = SearchTerm.ToLower();
    
    // Create simple mock actions for key search terms
    if (SearchTermLower.Contains(TEXT("self")))
    {
        // Create a simple self-reference action
        TSharedPtr<FEdGraphSchemaAction> SelfAction = MakeShareable(new FEdGraphSchemaAction(
            FText::FromString(TEXT("Self")),
            FText::FromString(TEXT("Get a reference to self")),
            FText::FromString(TEXT("Returns a reference to this actor instance")),
            0
        ));
        OutActions.Add(SelfAction);
        UE_LOG(LogVibeUEReflection, Log, TEXT("Added self-reference action"));
    }
    
    if (SearchTermLower.Contains(TEXT("branch")))
    {
        TSharedPtr<FEdGraphSchemaAction> BranchAction = MakeShareable(new FEdGraphSchemaAction(
            FText::FromString(TEXT("Flow Control")),
            FText::FromString(TEXT("Branch")),
            FText::FromString(TEXT("Branches execution flow based on a boolean condition")),
            0
        ));
        OutActions.Add(BranchAction);
        UE_LOG(LogVibeUEReflection, Log, TEXT("Added branch action"));
    }
    
    if (SearchTermLower.Contains(TEXT("print")))
    {
        TSharedPtr<FEdGraphSchemaAction> PrintAction = MakeShareable(new FEdGraphSchemaAction(
            FText::FromString(TEXT("Utilities")),
            FText::FromString(TEXT("Print String")),
            FText::FromString(TEXT("Prints a string to the screen and log")),
            0
        ));
        OutActions.Add(PrintAction);
        UE_LOG(LogVibeUEReflection, Log, TEXT("Added print action"));
    }
    
    UE_LOG(LogVibeUEReflection, Log, TEXT("Filtered search found %d actions for '%s'"), OutActions.Num(), *SearchTerm);
}

// NEW: Get common Blueprint actions when no search term provided
void FBlueprintReflection::GetCommonBlueprintActions(UBlueprint* Blueprint, const FString& Category, int32 MaxResults, TArray<TSharedPtr<FEdGraphSchemaAction>>& OutActions)
{
    if (!Blueprint) return;
    
    // Define common action types that users frequently need
    TArray<FString> CommonSearchTerms = {
        TEXT("self"),           // Get reference to self
        TEXT("branch"),         // If/Then/Else
        TEXT("print"),          // Print String
        TEXT("delay"),          // Delay
        TEXT("sequence"),       // Sequence
        TEXT("cast"),          // Cast To
        TEXT("get"),           // Variable getters
        TEXT("set"),           // Variable setters
        TEXT("add"),           // Math operations
        TEXT("multiply"),      // Math operations
        TEXT("event"),         // Event nodes
        TEXT("tick"),          // Tick event
        TEXT("beginplay"),     // Begin Play
        TEXT("spawn"),         // Spawn Actor
        TEXT("destroy"),       // Destroy Actor
    };
    
    int32 ResultsPerTerm = FMath::Max(1, MaxResults / CommonSearchTerms.Num());
    
    for (const FString& SearchTerm : CommonSearchTerms)
    {
        if (OutActions.Num() >= MaxResults) break;
        
        TArray<TSharedPtr<FEdGraphSchemaAction>> TermResults;
        GetFilteredBlueprintActions(Blueprint, SearchTerm, Category, ResultsPerTerm, TermResults);
        
        for (const auto& Action : TermResults)
        {
            if (OutActions.Num() >= MaxResults) break;
            OutActions.Add(Action);
        }
    }
    
    UE_LOG(LogVibeUEReflection, Log, TEXT("Common actions search found %d results"), OutActions.Num());
}

TSharedPtr<FJsonObject> FBlueprintReflectionCommands::CreateErrorResponse(const FString& Message)
{
    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField("success", false);
    Response->SetStringField("error", Message);
    return Response;
}

TSharedPtr<FJsonObject> FBlueprintReflectionCommands::CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data)
{
    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField("success", true);
    
    if (Data.IsValid())
    {
        for (auto& Elem : Data->Values)
        {
            Response->SetField(Elem.Key, Elem.Value);
        }
    }
    
    return Response;
}
