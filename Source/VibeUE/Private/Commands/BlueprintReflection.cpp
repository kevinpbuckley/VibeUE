// Copyright VibeUE 2025

#include "Commands/BlueprintReflection.h"
#include "Services/Blueprint/BlueprintDiscoveryService.h"
#include "Services/Blueprint/BlueprintNodeService.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_Self.h"
#include "K2Node_Knot.h"  // NEW (Oct 6, 2025): Reroute node support
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
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
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintVariableNodeSpawner.h"
#include "BlueprintActionMenuBuilder.h"
#include "BlueprintActionMenuUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Commands/CommonUtils.h"
#include "UObject/SoftObjectPath.h"

// Declare the log category
DEFINE_LOG_CATEGORY_STATIC(LogVibeUEReflection, Log, All);

// Static member initialization - simplified
TArray<FBlueprintReflection::FNodeCategory> FBlueprintReflection::CachedNodeCategories;
bool FBlueprintReflection::bCategoriesInitialized = false;
TMap<FString, UClass*> FBlueprintReflection::NodeTypeMap;
TMap<FString, TWeakObjectPtr<UBlueprintNodeSpawner>> FBlueprintReflection::CachedNodeSpawners;

namespace
{
    UClass* ResolveClassDescriptor(const FString& Descriptor)
    {
        FString Trimmed = Descriptor;
        Trimmed.TrimStartAndEndInline();
        Trimmed.TrimQuotesInline();

        if (Trimmed.IsEmpty())
        {
            return nullptr;
        }

        auto TryLoadClass = [](const FString& Path) -> UClass*
        {
            if (Path.IsEmpty())
            {
                return nullptr;
            }

            if (UClass* Existing = FindObject<UClass>(nullptr, *Path))
            {
                return Existing;
            }

            if (UClass* Loaded = LoadObject<UClass>(nullptr, *Path))
            {
                return Loaded;
            }

            return nullptr;
        };

        // Direct attempts (raw string, potential BlueprintGeneratedClass tokens)
        if (UClass* Direct = TryLoadClass(Trimmed))
        {
            return Direct;
        }

        // Soft class path support
        const FSoftClassPath SoftClassPath(Trimmed);
        if (SoftClassPath.IsValid())
        {
            if (UClass* SoftClass = SoftClassPath.TryLoadClass<UObject>())
            {
                return SoftClass;
            }
        }

        // Extract inner path if wrapped in type quotes (e.g. Class'/Script/Engine.GameplayStatics')
        if (Trimmed.Contains(TEXT("'")))
        {
            int32 FirstQuoteIndex;
            if (Trimmed.FindChar('\'', FirstQuoteIndex))
            {
                const FString AfterQuote = Trimmed.Mid(FirstQuoteIndex + 1);
                int32 SecondQuoteIndex;
                if (AfterQuote.FindChar('\'', SecondQuoteIndex))
                {
                    const FString InnerPath = AfterQuote.Left(SecondQuoteIndex);
                    if (UClass* FromInner = TryLoadClass(InnerPath))
                    {
                        return FromInner;
                    }
                }
            }
        }

        // Blueprint paths – add explicit type prefixes so StaticLoad can resolve them
        if (Trimmed.Contains(TEXT("/")))
        {
            const FString ClassQualified = FString::Printf(TEXT("Class'%s'"), *Trimmed);
            if (UClass* FromClassQualified = TryLoadClass(ClassQualified))
            {
                return FromClassQualified;
            }

            const FString BlueprintQualified = FString::Printf(TEXT("BlueprintGeneratedClass'%s'"), *Trimmed);
            if (UClass* FromBlueprintQualified = TryLoadClass(BlueprintQualified))
            {
                return FromBlueprintQualified;
            }
        }

        // Script classes without explicit path
        if (Trimmed.StartsWith(TEXT("/Script/")))
        {
            const FString ClassQualified = FString::Printf(TEXT("Class'%s'"), *Trimmed);
            if (UClass* FromScript = TryLoadClass(ClassQualified))
            {
                return FromScript;
            }
        }

        // Add common suffix/prefix variants
        if (!Trimmed.EndsWith(TEXT("_C")))
        {
            const FString WithSuffix = Trimmed + TEXT("_C");
            if (UClass* WithSuffixClass = TryLoadClass(WithSuffix))
            {
                return WithSuffixClass;
            }
        }

        if (!Trimmed.StartsWith(TEXT("U")) && !Trimmed.StartsWith(TEXT("A")))
        {
            const FString UPrefixed = TEXT("U") + Trimmed;
            if (UClass* PrefixedClass = TryLoadClass(UPrefixed))
            {
                return PrefixedClass;
            }
        }

        UE_LOG(LogVibeUEReflection, Warning, TEXT("ResolveClassDescriptor: failed to resolve class from '%s'"), *Descriptor);
        return nullptr;
    }
}

FBlueprintReflection::FBlueprintReflection()
{
    if (!bCategoriesInitialized)
    {
        PopulateNodeCategories();
        bCategoriesInitialized = true;
    }
}

TSharedPtr<FJsonObject> FBlueprintReflection::GetAvailableBlueprintNodes(UBlueprint* Blueprint, const FString& Category, const FString& SearchTerm, const FString& Context)
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
        UE_LOG(LogVibeUEReflection, Log, TEXT("*** GetAvailableBlueprintNodes called for: %s with Category: '%s', SearchTerm: '%s', Context: '%s' ***"), 
               *Blueprint->GetName(), *Category, *SearchTerm, *Context);
        
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
                if (bShouldInclude && !SearchTerm.IsEmpty() && SearchTerm != TEXT("all") && SearchTerm != TEXT("*"))
                {
                    FString SearchText = (ActionName + TEXT(" ") + ActionKeywords + TEXT(" ") + ActionTooltip).ToLower();
                    FString SearchTermLower = SearchTerm.ToLower();
                    
                    bShouldInclude = SearchText.Contains(SearchTermLower) ||
                                   ActionName.ToLower().Contains(SearchTermLower) ||
                                   ActionKeywords.ToLower().Contains(SearchTermLower) ||
                                   ActionTooltip.ToLower().Contains(SearchTermLower);
                }
                
                // Additional Context filtering if provided
                if (bShouldInclude && !Context.IsEmpty() && Context != TEXT("all") && Context != TEXT("*"))
                {
                    FString SearchText = (ActionName + TEXT(" ") + ActionKeywords + TEXT(" ") + ActionTooltip).ToLower();
                    FString ContextLower = Context.ToLower();
                    
                    bShouldInclude = SearchText.Contains(ContextLower) ||
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
                    int32 RelevanceScore = CalculateSearchRelevance(ActionName, ActionKeywords, ActionTooltip, SearchTerm.IsEmpty() ? Context : SearchTerm);
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
        ResponseObject->SetStringField(TEXT("search_term"), SearchTerm.IsEmpty() ? TEXT("none") : SearchTerm);
        ResponseObject->SetStringField(TEXT("context"), Context.IsEmpty() ? TEXT("none") : Context);
        
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
namespace
{
    TSharedPtr<FJsonObject> CreateBlueprintNodeInGraph(UBlueprint* Blueprint, const FString& NodeType, const TSharedPtr<FJsonObject>& NodeParams, UEdGraph* TargetGraph)
    {
        TSharedPtr<FJsonObject> ResponseObject = MakeShareable(new FJsonObject);

        if (!Blueprint)
        {
            ResponseObject->SetBoolField(TEXT("success"), false);
            ResponseObject->SetStringField(TEXT("error"), TEXT("Blueprint not found"));
            return ResponseObject;
        }

        if (!TargetGraph)
        {
            ResponseObject->SetBoolField(TEXT("success"), false);
            ResponseObject->SetStringField(TEXT("error"), TEXT("Target graph not provided"));
            return ResponseObject;
        }

        try
        {
            UClass* NodeClass = FBlueprintReflection::ResolveNodeClass(NodeType);
            if (!NodeClass)
            {
                ResponseObject->SetBoolField(TEXT("success"), false);
                ResponseObject->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown node type: %s"), *NodeType));
                return ResponseObject;
            }

            // Validate that the resolved class is actually a K2Node
            if (!NodeClass->IsChildOf(UK2Node::StaticClass()))
            {
                ResponseObject->SetBoolField(TEXT("success"), false);
                ResponseObject->SetStringField(TEXT("error"), FString::Printf(TEXT("Resolved class %s is not a K2Node"), *NodeClass->GetName()));
                return ResponseObject;
            }

            UK2Node* NewNode = NewObject<UK2Node>(TargetGraph, NodeClass);
            if (!NewNode)
            {
                ResponseObject->SetBoolField(TEXT("success"), false);
                ResponseObject->SetStringField(TEXT("error"), TEXT("Failed to create node instance"));
                return ResponseObject;
            }

            FVector2D NodePosition(200.0f, 200.0f);
            if (NodeParams.IsValid())
            {
                const TArray<TSharedPtr<FJsonValue>>* PosArray;
                if (NodeParams->TryGetArrayField(TEXT("position"), PosArray) && PosArray->Num() >= 2)
                {
                    NodePosition.X = (*PosArray)[0]->AsNumber();
                    NodePosition.Y = (*PosArray)[1]->AsNumber();
                }
            }

            // Match standard graph spawning behavior so nodes fully initialize their state
            NewNode->SetFlags(RF_Transactional);
            TargetGraph->AddNode(NewNode, true, true);

            // Ensure the node has a deterministic GUID so downstream tooling can locate it
            NewNode->CreateNewGuid();

            NewNode->NodePosX = NodePosition.X;
            NewNode->NodePosY = NodePosition.Y;

            const bool bIsCallFunctionNode = NewNode->IsA<UK2Node_CallFunction>();
            const bool bIsSpawnActorNode = NewNode->IsA<UK2Node_SpawnActorFromClass>();

            if (!bIsCallFunctionNode && !bIsSpawnActorNode)
            {
                // Allow node classes to perform any post-placement initialization they need
                NewNode->PostPlacedNewNode();
            }

            // Allocate baseline pins for most nodes so they start with expected default layout.
            // Function call nodes and SpawnActor nodes defer pin allocation until after configuration
            // to ensure the correct signature/class is available when pins are created.
            if (!bIsCallFunctionNode && !bIsSpawnActorNode)
            {
                NewNode->AllocateDefaultPins();
            }

            if (NodeParams.IsValid())
            {
                // ENHANCED: Pass the original node type for discovery system integration
                NodeParams->SetStringField(TEXT("node_type_name"), NodeType);
                FBlueprintReflection::ConfigureNodeFromParameters(NewNode, NodeParams);
            }

            if (bIsCallFunctionNode || bIsSpawnActorNode)
            {
                // Function and SpawnActor nodes need their post-placement logic executed after 
                // configuration so they can finish initializing pin defaults.
                NewNode->PostPlacedNewNode();
            }

            // Function call nodes and SpawnActor nodes allocate pins during configuration.
            // No further action needed here.

            NewNode->ReconstructNode();

            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

            ResponseObject->SetBoolField(TEXT("success"), true);
            ResponseObject->SetStringField(TEXT("node_id"), NewNode->NodeGuid.ToString());
            ResponseObject->SetStringField(TEXT("node_type"), NodeType);
            ResponseObject->SetStringField(TEXT("display_name"), NewNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
            ResponseObject->SetNumberField(TEXT("position_x"), NodePosition.X);
            ResponseObject->SetNumberField(TEXT("position_y"), NodePosition.Y);
            ResponseObject->SetNumberField(TEXT("pin_count"), NewNode->Pins.Num());

            UE_LOG(LogVibeUEReflection, Log, TEXT("Successfully created node %s (ID: %d) in graph %s for Blueprint %s"),
                   *NodeType, NewNode->GetUniqueID(), *TargetGraph->GetName(), *Blueprint->GetName());
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
}

TSharedPtr<FJsonObject> FBlueprintReflection::CreateBlueprintNode(UBlueprint* Blueprint, const FString& NodeType, const TSharedPtr<FJsonObject>& NodeParams)
{
    if (!Blueprint)
    {
        TSharedPtr<FJsonObject> ResponseObject = MakeShareable(new FJsonObject);
        ResponseObject->SetBoolField(TEXT("success"), false);
        ResponseObject->SetStringField(TEXT("error"), TEXT("Blueprint not found"));
        return ResponseObject;
    }

    UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
    if (!EventGraph)
    {
        TSharedPtr<FJsonObject> ResponseObject = MakeShareable(new FJsonObject);
        ResponseObject->SetBoolField(TEXT("success"), false);
        ResponseObject->SetStringField(TEXT("error"), TEXT("Could not find event graph in Blueprint"));
        return ResponseObject;
    }

    return CreateBlueprintNodeInGraph(Blueprint, NodeType, NodeParams, EventGraph);
}

TSharedPtr<FJsonObject> FBlueprintReflection::CreateBlueprintNode(UBlueprint* Blueprint, const FString& NodeType, const TSharedPtr<FJsonObject>& NodeParams, UEdGraph* TargetGraph)
{
    return CreateBlueprintNodeInGraph(Blueprint, NodeType, NodeParams, TargetGraph);
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
    
    UE_LOG(LogVibeUEReflection, Log, TEXT("Populated %d simplified node categories"), CachedNodeCategories.Num());
}

UClass* FBlueprintReflection::ResolveNodeClass(const FString& NodeType)
{
    if (!bCategoriesInitialized)
    {
        PopulateNodeCategories();
        bCategoriesInitialized = true;
    }

    // First check the simplified NodeTypeMap for common types (performance optimization)
    if (UClass** NodeClassPtr = NodeTypeMap.Find(NodeType))
    {
        return *NodeClassPtr;
    }

    // If not found in simplified map, use full reflection system via BlueprintActionDatabase
    UE_LOG(LogVibeUEReflection, Log, TEXT("Resolving node type '%s' via full reflection system"), *NodeType);
    
    FBlueprintActionDatabase& ActionDatabase = FBlueprintActionDatabase::Get();
    const FBlueprintActionDatabase::FActionRegistry& AllActions = ActionDatabase.GetAllActions();
    
    // Search through all available node spawners
    for (auto& ActionEntry : AllActions)
    {
        const FBlueprintActionDatabase::FActionList& ActionList = ActionEntry.Value;
        
        for (UBlueprintNodeSpawner* NodeSpawner : ActionList)
        {
            if (NodeSpawner && NodeSpawner->NodeClass)
            {
                // Only consider K2Node classes for Blueprint graphs
                if (!NodeSpawner->NodeClass->IsChildOf(UK2Node::StaticClass()))
                {
                    continue;
                }

                FString NodeClassName = NodeSpawner->NodeClass->GetName();
                FString DisplayName = NodeSpawner->DefaultMenuSignature.MenuName.ToString();
                
                // ENHANCED: Check if this spawner matches the discovered node type
                // Support multiple matching patterns from the discovery system:
                // 1. Exact display name match (e.g., "Play Sound at Location")
                // 2. Exact class name match (e.g., "K2Node_CallFunction")
                // 3. Simplified name match (e.g., "FunctionResult" matches "K2Node_FunctionResult")
                // 4. Keywords match for function call nodes
                
                if (DisplayName == NodeType ||
                    NodeClassName == NodeType ||
                    NodeClassName.EndsWith(NodeType) ||
                    (NodeType == TEXT("Return") && NodeClassName.Contains(TEXT("FunctionResult"))) ||
                    (NodeType == TEXT("FunctionResult") && NodeClassName.Contains(TEXT("FunctionResult"))))
                {
                    UE_LOG(LogVibeUEReflection, Log, TEXT("Found K2Node class %s for type '%s' via reflection (Display: %s)"), 
                           *NodeClassName, *NodeType, *DisplayName);
                    
                    // Store the spawner for later use in configuration
                    CacheSpawner(NodeType, NodeSpawner);
                    return NodeSpawner->NodeClass;
                }
                
                // ENHANCED: For function call nodes, also check if the node type matches function names
                if (UBlueprintFunctionNodeSpawner* FunctionSpawner = Cast<UBlueprintFunctionNodeSpawner>(NodeSpawner))
                {
                    if (FunctionSpawner->GetFunction())
                    {
                        FString FunctionName = FunctionSpawner->GetFunction()->GetName();
                        FString QualifiedName = FString::Printf(TEXT("%s::%s"), 
                                                              *FunctionSpawner->GetFunction()->GetOwnerClass()->GetName(),
                                                              *FunctionName);
                        
                        // Match function names like "PlaySoundAtLocation" or "UGameplayStatics::PlaySoundAtLocation"
                        if (DisplayName.Contains(NodeType) || FunctionName.Contains(NodeType) || 
                            QualifiedName.Contains(NodeType))
                        {
                            UE_LOG(LogVibeUEReflection, Log, TEXT("Found function node %s for type '%s' (Function: %s)"), 
                                   *NodeClassName, *NodeType, *QualifiedName);
                            
                            // Store the spawner for configuration
                            CacheSpawner(NodeType, NodeSpawner);
                            return NodeSpawner->NodeClass;
                        }
                    }
                }
            }
        }
    }
    
    UE_LOG(LogVibeUEReflection, Warning, TEXT("Could not resolve node type: %s"), *NodeType);
    return nullptr;
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
        
    UE_LOG(LogVibeUEReflection, Log, TEXT("Configuring node %s with parameters [Enhanced Reflection]"), *Node->GetClass()->GetName());
    
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
    else if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
    {
        ConfigureDynamicCastNode(CastNode, NodeParams);
    }
    else if (UK2Node_SpawnActorFromClass* SpawnActorNode = Cast<UK2Node_SpawnActorFromClass>(Node))
    {
        SpawnActorNode->Modify();

        // Ensure baseline pins exist before we attempt to configure defaults
        if (SpawnActorNode->Pins.Num() == 0)
        {
            SpawnActorNode->AllocateDefaultPins();
        }

        FString ClassDescriptor;
        const TCHAR* ClassKeys[] = { TEXT("class"), TEXT("class_path"), TEXT("Class"), TEXT("actor_class") };
        for (const TCHAR* Key : ClassKeys)
        {
            if (NodeParams->TryGetStringField(Key, ClassDescriptor) && !ClassDescriptor.IsEmpty())
            {
                break;
            }
            ClassDescriptor.Reset();
        }

        if (!ClassDescriptor.IsEmpty())
        {
            if (UClass* TargetClass = ResolveClassDescriptor(ClassDescriptor))
            {
                if (UEdGraphPin* ClassPin = SpawnActorNode->GetClassPin())
                {
                    ClassPin->DefaultObject = TargetClass;
                    ClassPin->DefaultValue = TargetClass->GetPathName();
                    ClassPin->PinType.PinSubCategoryObject = TargetClass;
                    UE_LOG(LogVibeUEReflection, Log, TEXT("Set SpawnActor class to: %s"), *TargetClass->GetName());
                }
                else
                {
                    UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureNodeFromParameters: SpawnActor node missing class pin after allocation"));
                }
            }
            else
            {
                UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureNodeFromParameters: Failed to resolve SpawnActor class descriptor '%s'"), *ClassDescriptor);
            }
        }

        // Refresh pins so the class selection is reflected in the node title and spawned pin set
        SpawnActorNode->ReconstructNode();
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

    // 🔍 DEBUG: Log all parameters being passed
    UE_LOG(LogVibeUEReflection, Warning, TEXT("════════ ConfigureFunctionNode ENTRY ════════"));
    FString DebugJson;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&DebugJson);
    FJsonSerializer::Serialize(NodeParams.ToSharedRef(), Writer);
    UE_LOG(LogVibeUEReflection, Warning, TEXT("NodeParams JSON: %s"), *DebugJson);

    // ═════════════════════════════════════════════════════════════════════
    // NEW DESIGN: Priority 1 - Use exact spawner_key if provided
    // ═════════════════════════════════════════════════════════════════════
    FString SpawnerKey;
    if (NodeParams->TryGetStringField(TEXT("spawner_key"), SpawnerKey) && !SpawnerKey.IsEmpty())
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Using exact spawner_key: %s"), *SpawnerKey);
        
        UBlueprintNodeSpawner* Spawner = GetSpawnerByKey(SpawnerKey);
        
        if (!Spawner)
        {
            UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Spawner not cached, will search during creation"));
            // Don't fail here - the spawner might be found during discovery
            // Fall through to legacy configuration
        }
        else if (UBlueprintFunctionNodeSpawner* FunctionSpawner = Cast<UBlueprintFunctionNodeSpawner>(Spawner))
        {
            if (const UFunction* Function = FunctionSpawner->GetFunction())
            {
                UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Found spawner, configuring node with %s::%s"),
                       *Function->GetOuterUClass()->GetName(), *Function->GetName());
                
                FunctionNode->Modify();
                FunctionNode->SetFromFunction(Function);
                FunctionNode->FunctionReference.SetFromField<UFunction>(Function, Function->HasAnyFunctionFlags(FUNC_Static));
                FunctionNode->AllocateDefaultPins();
                FunctionNode->ReconstructNode();
                
                UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: ✅ SUCCESS via spawner_key with %d pins"), 
                       FunctionNode->Pins.Num());
                return;
            }
        }
    }

    // ENHANCED: Try to find the function node using the BlueprintActionDatabase first
    // This integrates the discovery system with the creation system
    FString NodeTypeName;
    if (NodeParams->TryGetStringField(TEXT("node_type_name"), NodeTypeName))
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Attempting to configure using discovered node type: %s"), *NodeTypeName);
        
        // ENHANCED: Build cache key that includes function_class to differentiate variants
        FString CacheKey = NodeTypeName;
        FString DesiredClass;
        
        // Try to get function_class from flat NodeParams first (Python flattens it)
        if (!NodeParams->TryGetStringField(TEXT("function_class"), DesiredClass))
        {
            // Fallback: try nested node_params
            if (NodeParams->HasField(TEXT("node_params")))
            {
                const TSharedPtr<FJsonObject>* NodeParamsNested;
                if (NodeParams->TryGetObjectField(TEXT("node_params"), NodeParamsNested))
                {
                    (*NodeParamsNested)->TryGetStringField(TEXT("function_class"), DesiredClass);
                }
            }
        }
        
        if (!DesiredClass.IsEmpty())
        {
            CacheKey = FString::Printf(TEXT("%s::%s"), *NodeTypeName, *DesiredClass);
            UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Using class-specific cache key: %s"), *CacheKey);
        }
        else
        {
            UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: No function_class found, using simple cache key: %s"), *CacheKey);
        }
        
        // ENHANCED: Check cached spawners for performance (now with class-specific keys)
        if (UBlueprintNodeSpawner* CachedSpawner = GetSpawnerByKey(CacheKey))
        {
            if (UBlueprintFunctionNodeSpawner* FunctionSpawner = Cast<UBlueprintFunctionNodeSpawner>(CachedSpawner))
            {
                if (const UFunction* FoundFunction = FunctionSpawner->GetFunction())
                {
                    UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Using cached spawner for function: %s::%s"), 
                           *FoundFunction->GetOuterUClass()->GetName(), *FoundFunction->GetName());
                    
                    FunctionNode->Modify();
                    FunctionNode->SetFromFunction(FoundFunction);
                    FunctionNode->FunctionReference.SetFromField<UFunction>(FoundFunction, FoundFunction->HasAnyFunctionFlags(FUNC_Static));
                    FunctionNode->AllocateDefaultPins();
                    FunctionNode->ReconstructNode();
                    return; // Success!
                }
            }
        }
        
        // Fallback to full database search if not cached
        // SKIP simple search if we have function_class specified - use enhanced context filtering instead
        if (DesiredClass.IsEmpty())
        {
            FBlueprintActionDatabase& ActionDatabase = FBlueprintActionDatabase::Get();
            const FBlueprintActionDatabase::FActionRegistry& AllActions = ActionDatabase.GetAllActions();
            
            for (auto& ActionEntry : AllActions)
            {
                const FBlueprintActionDatabase::FActionList& ActionList = ActionEntry.Value;
                for (UBlueprintNodeSpawner* NodeSpawner : ActionList)
                {
                    if (NodeSpawner && NodeSpawner->NodeClass)
                    {
                        FString DisplayName = NodeSpawner->DefaultMenuSignature.MenuName.ToString();
                        if (DisplayName.Equals(NodeTypeName, ESearchCase::IgnoreCase) ||
                            DisplayName.Contains(NodeTypeName))
                        {
                            // Cache this spawner for future use (with class-specific key)
                            CacheSpawner(CacheKey, NodeSpawner);
                            
                            // Found matching spawner - extract function information
                            if (UBlueprintFunctionNodeSpawner* FunctionSpawner = Cast<UBlueprintFunctionNodeSpawner>(NodeSpawner))
                            {
                                if (const UFunction* FoundFunction = FunctionSpawner->GetFunction())
                                {
                                    UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Found function via simple spawner search: %s::%s"), 
                                           *FoundFunction->GetOuterUClass()->GetName(), *FoundFunction->GetName());
                                    
                                    FunctionNode->Modify();
                                    FunctionNode->SetFromFunction(FoundFunction);
                                    FunctionNode->FunctionReference.SetFromField<UFunction>(FoundFunction, FoundFunction->HasAnyFunctionFlags(FUNC_Static));
                                    FunctionNode->AllocateDefaultPins();
                                    FunctionNode->ReconstructNode();
                                    return; // Success!
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Skipping simple search, using enhanced context filtering for class: %s"), *DesiredClass);
        }
        UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Could not find spawner for node type: %s"), *NodeTypeName);
    }

    // ENHANCED: Try function_name with SPAWNER SEARCH (before fallback to manual resolution)
    FString FunctionName;
    if (!NodeParams->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        const TSharedPtr<FJsonObject>* FunctionReference = nullptr;
        if (NodeParams->TryGetObjectField(TEXT("FunctionReference"), FunctionReference) && FunctionReference && (*FunctionReference)->TryGetStringField(TEXT("MemberName"), FunctionName))
        {
            // extracted from nested reference
        }
    }
    
    // NEW: If we have a function name, search for a spawner that creates this specific function
    if (!FunctionName.IsEmpty())
    {
        // ENHANCED: Extract function_class parameter for filtering
        FString DesiredFunctionClass;
        NodeParams->TryGetStringField(TEXT("function_class"), DesiredFunctionClass);
        
        // Also check FunctionReference.MemberParent
        if (DesiredFunctionClass.IsEmpty())
        {
            const TSharedPtr<FJsonObject>* FunctionReference = nullptr;
            if (NodeParams->TryGetObjectField(TEXT("FunctionReference"), FunctionReference) && FunctionReference)
            {
                (*FunctionReference)->TryGetStringField(TEXT("MemberParent"), DesiredFunctionClass);
            }
        }
        
        UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Searching spawners for function '%s' on class '%s'"), 
               *FunctionName, DesiredFunctionClass.IsEmpty() ? TEXT("<any>") : *DesiredFunctionClass);
        
        // Build cache key that includes both function name and class for precise matching
        FString CacheKey = DesiredFunctionClass.IsEmpty() ? FunctionName : FString::Printf(TEXT("%s::%s"), *DesiredFunctionClass, *FunctionName);
        
        // Check cache first for performance
        if (UBlueprintNodeSpawner* CachedSpawner = GetSpawnerByKey(CacheKey))
        {
            if (UBlueprintFunctionNodeSpawner* FunctionSpawner = Cast<UBlueprintFunctionNodeSpawner>(CachedSpawner))
            {
                if (const UFunction* FoundFunction = FunctionSpawner->GetFunction())
                {
                    UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Using cached spawner for function: %s::%s"), 
                           *FoundFunction->GetOuterUClass()->GetName(), *FoundFunction->GetName());
                    
                    FunctionNode->Modify();
                    FunctionNode->SetFromFunction(FoundFunction);
                    FunctionNode->FunctionReference.SetFromField<UFunction>(FoundFunction, FoundFunction->HasAnyFunctionFlags(FUNC_Static));
                    FunctionNode->AllocateDefaultPins();
                    FunctionNode->ReconstructNode();
                    return; // Success via cached spawner!
                }
            }
        }
        
        // Not cached, search the full database
        FBlueprintActionDatabase& ActionDatabase = FBlueprintActionDatabase::Get();
        const FBlueprintActionDatabase::FActionRegistry& AllActions = ActionDatabase.GetAllActions();
        
        // ENHANCED: Collect all matching functions with context-sensitive filtering
        TArray<TPair<const UFunction*, UBlueprintNodeSpawner*>> MatchingFunctions;
        TArray<TPair<const UFunction*, UBlueprintNodeSpawner*>> ContextFilteredMatches;
        
        // Create filter context for context-sensitive filtering (like Unreal's "Context Sensitive" checkbox)
        // NOTE: Using BPFILTER_NoFlags to allow global static functions (like UGameplayStatics)
        FBlueprintActionFilter Filter(FBlueprintActionFilter::BPFILTER_NoFlags);
        if (UBlueprint* Blueprint = FunctionNode->GetBlueprint())
        {
            Filter.Context.Blueprints.Add(Blueprint);
            Filter.Context.Graphs.Add(FunctionNode->GetGraph());
        }
        
        for (auto& ActionEntry : AllActions)
        {
            const FBlueprintActionDatabase::FActionList& ActionList = ActionEntry.Value;
            for (UBlueprintNodeSpawner* NodeSpawner : ActionList)
            {
                if (UBlueprintFunctionNodeSpawner* FunctionSpawner = Cast<UBlueprintFunctionNodeSpawner>(NodeSpawner))
                {
                    if (const UFunction* Function = FunctionSpawner->GetFunction())
                    {
                        // Match function name (case-insensitive)
                        if (Function->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
                        {
                            FString FunctionClass = Function->GetOuterUClass()->GetName();
                            FString FunctionPath = Function->GetOuterUClass()->GetPathName();
                            UE_LOG(LogVibeUEReflection, Warning, TEXT("  Found GetPlayerController variant: %s::%s (Path: %s, IsStatic: %d)"), 
                                   *FunctionClass, *Function->GetName(), *FunctionPath, Function->HasAnyFunctionFlags(FUNC_Static));
                            
                            MatchingFunctions.Add(TPair<const UFunction*, UBlueprintNodeSpawner*>(Function, NodeSpawner));
                            
                            // Check if this spawner passes context-sensitive filtering (like "Context Sensitive" checkbox)
                            FBlueprintActionInfo ActionInfo(Function->GetOuterUClass(), NodeSpawner);
                            if (!Filter.IsFiltered(ActionInfo))
                            {
                                ContextFilteredMatches.Add(TPair<const UFunction*, UBlueprintNodeSpawner*>(Function, NodeSpawner));
                                UE_LOG(LogVibeUEReflection, Log, TEXT("  ✓ Context-appropriate: %s::%s"), 
                                       *Function->GetOuterUClass()->GetName(), *Function->GetName());
                            }
                            else
                            {
                                UE_LOG(LogVibeUEReflection, Log, TEXT("  ✗ Context-filtered: %s::%s"), 
                                       *Function->GetOuterUClass()->GetName(), *Function->GetName());
                            }
                        }
                    }
                }
            }
        }
        
        UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Found %d total matches, %d context-appropriate for function '%s'"), 
               MatchingFunctions.Num(), ContextFilteredMatches.Num(), *FunctionName);
        
        // ENHANCED: Prioritize context-filtered matches, then apply class filtering
        const UFunction* BestMatch = nullptr;
        UBlueprintNodeSpawner* BestSpawner = nullptr;
        
        // Use context-filtered list if available, otherwise use all matches
        TArray<TPair<const UFunction*, UBlueprintNodeSpawner*>>& SearchList = 
            ContextFilteredMatches.Num() > 0 ? ContextFilteredMatches : MatchingFunctions;
        
        if (!DesiredFunctionClass.IsEmpty())
        {
            // First pass: Try exact class match in context-appropriate functions
            for (const auto& Pair : SearchList)
            {
                const UFunction* Function = Pair.Key;
                UClass* FunctionClass = Function->GetOuterUClass();
                
                // Check if class name matches (handle both short names and full paths)
                FString FunctionClassName = FunctionClass->GetName();
                FString FunctionClassPath = FunctionClass->GetPathName();
                
                if (FunctionClassName.Equals(DesiredFunctionClass, ESearchCase::IgnoreCase) ||
                    FunctionClassPath.Contains(DesiredFunctionClass, ESearchCase::IgnoreCase) ||
                    DesiredFunctionClass.Contains(FunctionClassName, ESearchCase::IgnoreCase))
                {
                    UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Found exact class match: %s::%s"), 
                           *FunctionClassName, *Function->GetName());
                    BestMatch = Function;
                    BestSpawner = Pair.Value;
                    break;
                }
            }
        }
        
        // If no class-specific match found, use first context-appropriate match (fallback)
        if (!BestMatch && SearchList.Num() > 0)
        {
            BestMatch = SearchList[0].Key;
            BestSpawner = SearchList[0].Value;
            UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Using first context-appropriate match: %s::%s"), 
                   *BestMatch->GetOuterUClass()->GetName(), *BestMatch->GetName());
        }
        
        if (BestMatch && BestSpawner)
        {
            UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Selected function '%s::%s'"), 
                   *BestMatch->GetOuterUClass()->GetName(), *BestMatch->GetName());
            
            // Cache this spawner for future use with the specific key
            CacheSpawner(CacheKey, BestSpawner);
            
            // Reconfigure the node with the correct function
            FunctionNode->Modify();
            FunctionNode->SetFromFunction(BestMatch);
            FunctionNode->FunctionReference.SetFromField<UFunction>(BestMatch, BestMatch->HasAnyFunctionFlags(FUNC_Static));
            FunctionNode->AllocateDefaultPins();
            FunctionNode->ReconstructNode();
            
            UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Successfully configured function node via spawner"));
            return; // Success via spawner search!
        }
        
        UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: No spawner found for function '%s', falling back to manual resolution"), *FunctionName);
    }

    // FALLBACK: Original parameter-based configuration (manual function resolution)
    if (FunctionName.IsEmpty())
    {
        // Try alternative parameter sources if not already extracted
        const TSharedPtr<FJsonObject>* FunctionReference = nullptr;
        if (NodeParams->TryGetObjectField(TEXT("FunctionReference"), FunctionReference) && FunctionReference && (*FunctionReference)->TryGetStringField(TEXT("MemberName"), FunctionName))
        {
            // extracted from nested reference
        }
    }

    FString ClassDescriptor;
    if (!NodeParams->TryGetStringField(TEXT("function_class"), ClassDescriptor))
    {
        const TSharedPtr<FJsonObject>* FunctionReference = nullptr;
        if (NodeParams->TryGetObjectField(TEXT("FunctionReference"), FunctionReference) && FunctionReference)
        {
            (*FunctionReference)->TryGetStringField(TEXT("MemberParent"), ClassDescriptor);
        }
    }

    if (ClassDescriptor.IsEmpty())
    {
        NodeParams->TryGetStringField(TEXT("target_class"), ClassDescriptor);
    }

    UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Requested function '%s' on descriptor '%s'"), *FunctionName, *ClassDescriptor);

    UClass* TargetClass = nullptr;
    if (!ClassDescriptor.IsEmpty())
    {
        TargetClass = ResolveClassDescriptor(ClassDescriptor);
        if (TargetClass)
        {
            UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Resolved target class '%s'"), *TargetClass->GetName());
        }
    }

    if (!TargetClass)
    {
        if (UBlueprint* Blueprint = FunctionNode->GetBlueprint())
        {
            TargetClass = Blueprint->GeneratedClass;
            UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Falling back to blueprint generated class '%s'"), TargetClass ? *TargetClass->GetName() : TEXT("<null>"));
        }
    }

    if (!TargetClass || FunctionName.IsEmpty())
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Unable to resolve target class or function (class='%s', function='%s')"), *ClassDescriptor, *FunctionName);
        return;
    }

    UFunction* ResolvedFunction = TargetClass->FindFunctionByName(*FunctionName);
    if (!ResolvedFunction)
    {
        for (UClass* ClassIt = TargetClass->GetSuperClass(); ClassIt && !ResolvedFunction; ClassIt = ClassIt->GetSuperClass())
        {
            ResolvedFunction = ClassIt->FindFunctionByName(*FunctionName);
        }
    }

    if (!ResolvedFunction)
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Failed to locate function '%s' on class '%s'"), *FunctionName, *TargetClass->GetName());
        return;
    }

    UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Binding to '%s::%s'"), *ResolvedFunction->GetOuterUClass()->GetName(), *ResolvedFunction->GetName());

    const bool bIsStaticFunction = ResolvedFunction->HasAnyFunctionFlags(FUNC_Static);

    bool bShouldUseSelfContext = bIsStaticFunction;

    if (!bIsStaticFunction)
    {
        if (UBlueprint* OwningBlueprint = FunctionNode->GetBlueprint())
        {
            UClass* SelfClass = OwningBlueprint->GeneratedClass ? OwningBlueprint->GeneratedClass->GetAuthoritativeClass() : nullptr;
            if (SelfClass && TargetClass && (SelfClass == TargetClass || SelfClass->IsChildOf(TargetClass)))
            {
                bShouldUseSelfContext = true;
            }
        }
    }

    FunctionNode->Modify();
    FunctionNode->SetFromFunction(ResolvedFunction);
    FunctionNode->FunctionReference.SetFromField<UFunction>(ResolvedFunction, bShouldUseSelfContext);
    FunctionNode->AllocateDefaultPins();
    FunctionNode->ReconstructNode();

    UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Bound node to %s::%s"), *TargetClass->GetName(), *FunctionName);
}

void FBlueprintReflection::ConfigureVariableNode(UK2Node_VariableGet* VariableNode, const TSharedPtr<FJsonObject>& NodeParams)
{
    if (!VariableNode)
        return;
        
    FString VariableName;
    
    // ENHANCED: Try multiple parameter sources for variable name
    if (!NodeParams->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        // Try VariableReference structure
        const TSharedPtr<FJsonObject>* VariableReference = nullptr;
        if (NodeParams->TryGetObjectField(TEXT("VariableReference"), VariableReference) && VariableReference)
        {
            (*VariableReference)->TryGetStringField(TEXT("MemberName"), VariableName);
        }
    }
    
    if (VariableName.IsEmpty())
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureVariableNode: No variable name provided in parameters"));
        return;
    }
    
    // ═════════════════════════════════════════════════════════════════════
    // NEW: Context-aware variable resolution (Oct 6, 2025)
    // Supports external member references via owner_class parameter
    // ═════════════════════════════════════════════════════════════════════
    
    FString OwnerDescriptor;
    bool bIsExternal = false;
    
    // Check for external owner class specification
    if (NodeParams->TryGetStringField(TEXT("owner_class"), OwnerDescriptor) ||
        NodeParams->TryGetStringField(TEXT("variable_owner"), OwnerDescriptor))
    {
        bIsExternal = true;
    }
    
    // Check explicit scope indicator
    FString MemberScope;
    if (NodeParams->TryGetStringField(TEXT("member_scope"), MemberScope))
    {
        if (MemberScope.Equals(TEXT("external"), ESearchCase::IgnoreCase))
        {
            bIsExternal = true;
        }
    }
    
    // Check is_local flag (inverse logic)
    bool bIsLocal = true;
    if (NodeParams->TryGetBoolField(TEXT("is_local"), bIsLocal))
    {
        if (!bIsLocal)
        {
            bIsExternal = true;
        }
    }
    
    FName VarName(*VariableName);
    
    if (bIsExternal && !OwnerDescriptor.IsEmpty())
    {
        // External member - resolve owner class and set external reference
        if (UClass* OwnerClass = ResolveClassDescriptor(OwnerDescriptor))
        {
            VariableNode->VariableReference.SetExternalMember(VarName, OwnerClass);
            VariableNode->AllocateDefaultPins();
            VariableNode->ReconstructNode();
            
            UE_LOG(LogVibeUEReflection, Log, 
                TEXT("ConfigureVariableNode: Set external variable '%s' from class '%s'"),
                *VariableName, *OwnerClass->GetName());
        }
        else
        {
            UE_LOG(LogVibeUEReflection, Warning,
                TEXT("ConfigureVariableNode: Failed to resolve owner class '%s' for variable '%s'"),
                *OwnerDescriptor, *VariableName);
            
            // Fallback to self member
            if (UBlueprint* Blueprint = VariableNode->GetBlueprint())
            {
                VariableNode->VariableReference.SetSelfMember(VarName);
                VariableNode->AllocateDefaultPins();
                VariableNode->ReconstructNode();
                
                UE_LOG(LogVibeUEReflection, Warning,
                    TEXT("ConfigureVariableNode: Falling back to self member for '%s'"), *VariableName);
            }
        }
    }
    else
    {
        // Self member (default behavior)
        if (UBlueprint* Blueprint = VariableNode->GetBlueprint())
        {
            VariableNode->VariableReference.SetSelfMember(VarName);
            VariableNode->AllocateDefaultPins();
            VariableNode->ReconstructNode();
            UE_LOG(LogVibeUEReflection, Log, TEXT("ConfigureVariableNode: Set self variable '%s'"), *VariableName);
        }
    }
}

void FBlueprintReflection::ConfigureVariableSetNode(UK2Node_VariableSet* VariableNode, const TSharedPtr<FJsonObject>& NodeParams)
{
    if (!VariableNode)
        return;
        
    FString VariableName;
    if (!NodeParams->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureVariableSetNode: No variable name provided"));
        return;
    }
    
    // ═════════════════════════════════════════════════════════════════════
    // NEW: Context-aware variable resolution (Oct 6, 2025)
    // Supports external member references via owner_class parameter
    // ═════════════════════════════════════════════════════════════════════
    
    FString OwnerDescriptor;
    bool bIsExternal = false;
    
    // Check for external owner class specification
    if (NodeParams->TryGetStringField(TEXT("owner_class"), OwnerDescriptor) ||
        NodeParams->TryGetStringField(TEXT("variable_owner"), OwnerDescriptor))
    {
        bIsExternal = true;
    }
    
    // Check explicit scope indicator
    FString MemberScope;
    if (NodeParams->TryGetStringField(TEXT("member_scope"), MemberScope))
    {
        if (MemberScope.Equals(TEXT("external"), ESearchCase::IgnoreCase))
        {
            bIsExternal = true;
        }
    }
    
    // Check is_local flag (inverse logic)
    bool bIsLocal = true;
    if (NodeParams->TryGetBoolField(TEXT("is_local"), bIsLocal))
    {
        if (!bIsLocal)
        {
            bIsExternal = true;
        }
    }
    
    FName VarName(*VariableName);
    
    if (bIsExternal && !OwnerDescriptor.IsEmpty())
    {
        // External member - resolve owner class and set external reference
        if (UClass* OwnerClass = ResolveClassDescriptor(OwnerDescriptor))
        {
            VariableNode->VariableReference.SetExternalMember(VarName, OwnerClass);
            VariableNode->AllocateDefaultPins();
            VariableNode->ReconstructNode();
            
            UE_LOG(LogVibeUEReflection, Log,
                TEXT("ConfigureVariableSetNode: Set external variable '%s' from class '%s'"),
                *VariableName, *OwnerClass->GetName());
        }
        else
        {
            UE_LOG(LogVibeUEReflection, Warning,
                TEXT("ConfigureVariableSetNode: Failed to resolve owner class '%s' for variable '%s'"),
                *OwnerDescriptor, *VariableName);
            
            // Fallback to self member
            if (UBlueprint* Blueprint = VariableNode->GetBlueprint())
            {
                VariableNode->VariableReference.SetSelfMember(VarName);
                VariableNode->AllocateDefaultPins();
                VariableNode->ReconstructNode();
                
                UE_LOG(LogVibeUEReflection, Warning,
                    TEXT("ConfigureVariableSetNode: Falling back to self member for '%s'"), *VariableName);
            }
        }
    }
    else
    {
        // Self member (default behavior)
        if (UBlueprint* Blueprint = VariableNode->GetBlueprint())
        {
            VariableNode->VariableReference.SetSelfMember(VarName);
            VariableNode->AllocateDefaultPins();
            VariableNode->ReconstructNode();
            UE_LOG(LogVibeUEReflection, Log, TEXT("ConfigureVariableSetNode: Set self variable '%s'"), *VariableName);
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

void FBlueprintReflection::ConfigureDynamicCastNode(UK2Node_DynamicCast* CastNode, const TSharedPtr<FJsonObject>& NodeParams)
{
    if (!CastNode)
    {
        return;
    }

    FString CastTargetDescriptor;
    if (!NodeParams->TryGetStringField(TEXT("cast_target"), CastTargetDescriptor))
    {
        NodeParams->TryGetStringField(TEXT("target_class"), CastTargetDescriptor);
    }

    if (CastTargetDescriptor.IsEmpty())
    {
        const TSharedPtr<FJsonObject>* CastTargetObject = nullptr;
        if (NodeParams->TryGetObjectField(TEXT("cast_target"), CastTargetObject) && CastTargetObject)
        {
            (*CastTargetObject)->TryGetStringField(TEXT("class"), CastTargetDescriptor);
        }
    }

    if (CastTargetDescriptor.IsEmpty())
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureDynamicCastNode: Missing cast_target descriptor"));
        return;
    }

    if (UClass* TargetClass = ResolveClassDescriptor(CastTargetDescriptor))
    {
        CastNode->TargetType = TargetClass;
        CastNode->ReconstructNode();
        UE_LOG(LogVibeUEReflection, Log, TEXT("ConfigureDynamicCastNode: Set cast target to %s"), *TargetClass->GetName());
    }
    else
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureDynamicCastNode: Failed to resolve cast target '%s'"), *CastTargetDescriptor);
    }
}

// ═════════════════════════════════════════════════════════════════════
// PIN DEFAULT CONFIGURATION SYSTEM (Oct 6, 2025)
// ═════════════════════════════════════════════════════════════════════

/**
 * Try to apply a struct default value from JSON
 */
static bool TryApplyStructDefault(UEdGraphPin* Pin, const TSharedPtr<FJsonObject>& StructValue)
{
    if (!Pin || !StructValue.IsValid())
    {
        return false;
    }
    
    // Common struct types
    if (Pin->PinType.PinSubCategoryObject.IsValid())
    {
        UScriptStruct* Struct = Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject.Get());
        if (!Struct)
        {
            return false;
        }
        
        // Handle FVector
        if (Struct->GetFName() == NAME_Vector)
        {
            double X = 0, Y = 0, Z = 0;
            StructValue->TryGetNumberField(TEXT("X"), X);
            StructValue->TryGetNumberField(TEXT("Y"), Y);
            StructValue->TryGetNumberField(TEXT("Z"), Z);
            Pin->DefaultValue = FString::Printf(TEXT("%f,%f,%f"), X, Y, Z);
            return true;
        }
        
        // Handle FRotator
        if (Struct->GetFName() == NAME_Rotator)
        {
            double Pitch = 0, Yaw = 0, Roll = 0;
            StructValue->TryGetNumberField(TEXT("Pitch"), Pitch);
            StructValue->TryGetNumberField(TEXT("Yaw"), Yaw);
            StructValue->TryGetNumberField(TEXT("Roll"), Roll);
            Pin->DefaultValue = FString::Printf(TEXT("%f,%f,%f"), Pitch, Yaw, Roll);
            return true;
        }
        
        // Handle FVector2D
        if (Struct->GetFName() == NAME_Vector2D)
        {
            double X = 0, Y = 0;
            StructValue->TryGetNumberField(TEXT("X"), X);
            StructValue->TryGetNumberField(TEXT("Y"), Y);
            Pin->DefaultValue = FString::Printf(TEXT("%f,%f"), X, Y);
            return true;
        }
        
        // Handle FLinearColor / FColor
        if (Struct->GetFName() == NAME_LinearColor || Struct->GetFName() == NAME_Color)
        {
            double R = 1, G = 1, B = 1, A = 1;
            StructValue->TryGetNumberField(TEXT("R"), R);
            StructValue->TryGetNumberField(TEXT("G"), G);
            StructValue->TryGetNumberField(TEXT("B"), B);
            StructValue->TryGetNumberField(TEXT("A"), A);
            Pin->DefaultValue = FString::Printf(TEXT("(R=%f,G=%f,B=%f,A=%f)"), R, G, B, A);
            return true;
        }
    }
    
    return false;
}

/**
 * Apply default values to node pins after creation
 * Supports primitive types, structs, and provides detailed error reporting
 */
TSharedPtr<FJsonObject> FBlueprintReflection::ApplyPinDefaults(
    UEdGraphNode* Node, 
    const TSharedPtr<FJsonObject>& PinDefaults
)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    TArray<FString> SuccessfulPins;
    TArray<FString> FailedPins;
    
    if (!Node || !PinDefaults.IsValid())
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Invalid node or pin defaults"));
        return Result;
    }
    
    // Iterate through all requested pin defaults
    for (const auto& Pair : PinDefaults->Values)
    {
        FString PinName = Pair.Key;
        const TSharedPtr<FJsonValue>& DefaultValue = Pair.Value;
        
        // Find the pin (case-insensitive)
        UEdGraphPin* Pin = nullptr;
        for (UEdGraphPin* CandidatePin : Node->Pins)
        {
            if (CandidatePin && CandidatePin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
            {
                Pin = CandidatePin;
                break;
            }
        }
        
        if (!Pin)
        {
            FailedPins.Add(FString::Printf(TEXT("%s (pin not found)"), *PinName));
            UE_LOG(LogVibeUEReflection, Warning, TEXT("ApplyPinDefaults: Pin '%s' not found on node"), *PinName);
            continue;
        }
        
        // Validate pin can accept defaults
        if (Pin->Direction != EGPD_Input)
        {
            FailedPins.Add(FString::Printf(TEXT("%s (output pin cannot have defaults)"), *PinName));
            UE_LOG(LogVibeUEReflection, Warning, TEXT("ApplyPinDefaults: Pin '%s' is output pin"), *PinName);
            continue;
        }
        
        if (Pin->LinkedTo.Num() > 0)
        {
            FailedPins.Add(FString::Printf(TEXT("%s (connected pin cannot have defaults)"), *PinName));
            UE_LOG(LogVibeUEReflection, Warning, TEXT("ApplyPinDefaults: Pin '%s' is connected"), *PinName);
            continue;
        }
        
        // Apply default based on value type
        bool bSuccess = false;
        
        if (DefaultValue->Type == EJson::String)
        {
            Pin->DefaultValue = DefaultValue->AsString();
            bSuccess = true;
        }
        else if (DefaultValue->Type == EJson::Number)
        {
            Pin->DefaultValue = FString::SanitizeFloat(DefaultValue->AsNumber());
            bSuccess = true;
        }
        else if (DefaultValue->Type == EJson::Boolean)
        {
            Pin->DefaultValue = DefaultValue->AsBool() ? TEXT("true") : TEXT("false");
            bSuccess = true;
        }
        else if (DefaultValue->Type == EJson::Object)
        {
            // Handle struct defaults (complex types)
            if (TryApplyStructDefault(Pin, DefaultValue->AsObject()))
            {
                bSuccess = true;
            }
            else
            {
                FailedPins.Add(FString::Printf(TEXT("%s (struct conversion failed)"), *PinName));
                UE_LOG(LogVibeUEReflection, Warning, 
                    TEXT("ApplyPinDefaults: Failed to convert struct default for pin '%s'"), *PinName);
            }
        }
        else
        {
            FailedPins.Add(FString::Printf(TEXT("%s (unsupported value type)"), *PinName));
            UE_LOG(LogVibeUEReflection, Warning, 
                TEXT("ApplyPinDefaults: Unsupported value type for pin '%s'"), *PinName);
        }
        
        if (bSuccess)
        {
            SuccessfulPins.Add(PinName);
            UE_LOG(LogVibeUEReflection, Log, 
                TEXT("ApplyPinDefaults: Set default '%s' = '%s'"), *PinName, *Pin->DefaultValue);
        }
    }
    
    // Build result
    Result->SetBoolField(TEXT("success"), FailedPins.Num() == 0);
    
    TArray<TSharedPtr<FJsonValue>> SuccessArray;
    for (const FString& SuccessPin : SuccessfulPins)
    {
        SuccessArray.Add(MakeShared<FJsonValueString>(SuccessPin));
    }
    Result->SetArrayField(TEXT("successful_pins"), SuccessArray);
    
    TArray<TSharedPtr<FJsonValue>> FailedArray;
    for (const FString& FailedPin : FailedPins)
    {
        FailedArray.Add(MakeShared<FJsonValueString>(FailedPin));
    }
    Result->SetArrayField(TEXT("failed_pins"), FailedArray);
    
    Result->SetNumberField(TEXT("successful_count"), SuccessfulPins.Num());
    Result->SetNumberField(TEXT("failed_count"), FailedPins.Num());
    
    return Result;
}

// ═════════════════════════════════════════════════════════════════════
// REROUTE NODE ERGONOMICS SYSTEM (Oct 6, 2025)
// ═════════════════════════════════════════════════════════════════════

/**
 * Create a reroute (knot) node at the specified position
 */
UK2Node_Knot* FBlueprintReflection::CreateRerouteNode(
    UEdGraph* Graph,
    const FVector2D& Position,
    const FEdGraphPinType* PinType
)
{
    if (!Graph)
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("CreateRerouteNode: Null graph"));
        return nullptr;
    }
    
    UK2Node_Knot* KnotNode = NewObject<UK2Node_Knot>(Graph);
    if (!KnotNode)
    {
        UE_LOG(LogVibeUEReflection, Error, TEXT("CreateRerouteNode: Failed to create knot node"));
        return nullptr;
    }
    
    KnotNode->NodePosX = Position.X;
    KnotNode->NodePosY = Position.Y;
    
    Graph->AddNode(KnotNode, true);
    KnotNode->CreateNewGuid();
    KnotNode->PostPlacedNewNode();
    KnotNode->AllocateDefaultPins();
    
    UE_LOG(LogVibeUEReflection, Log, 
        TEXT("CreateRerouteNode: Created reroute at (%f, %f)"), Position.X, Position.Y);
    
    return KnotNode;
}

/**
 * Create a reroute node between two existing pins
 * Automatically positions the reroute at the midpoint
 */
UK2Node_Knot* FBlueprintReflection::InsertRerouteNode(
    UEdGraph* Graph,
    UEdGraphPin* SourcePin,
    UEdGraphPin* TargetPin,
    const FVector2D* CustomPosition
)
{
    if (!Graph || !SourcePin || !TargetPin)
    {
        UE_LOG(LogVibeUEReflection, Warning, 
            TEXT("InsertRerouteNode: Invalid parameters (Graph=%p, Source=%p, Target=%p)"),
            Graph, SourcePin, TargetPin);
        return nullptr;
    }
    
    // Calculate position (midpoint between nodes or custom)
    FVector2D ReroutePosition;
    if (CustomPosition)
    {
        ReroutePosition = *CustomPosition;
    }
    else
    {
        UEdGraphNode* SourceNode = SourcePin->GetOwningNode();
        UEdGraphNode* TargetNode = TargetPin->GetOwningNode();
        
        if (SourceNode && TargetNode)
        {
            ReroutePosition.X = (SourceNode->NodePosX + TargetNode->NodePosX) / 2.0f;
            ReroutePosition.Y = (SourceNode->NodePosY + TargetNode->NodePosY) / 2.0f;
            
            // Grid snap (16-pixel increments)
            ReroutePosition.X = FMath::RoundToFloat(ReroutePosition.X / 16.0f) * 16.0f;
            ReroutePosition.Y = FMath::RoundToFloat(ReroutePosition.Y / 16.0f) * 16.0f;
        }
    }
    
    // Create the reroute node with matching pin type
    UK2Node_Knot* KnotNode = CreateRerouteNode(Graph, ReroutePosition, &SourcePin->PinType);
    if (!KnotNode)
    {
        return nullptr;
    }
    
    // Find the input and output pins on the knot
    UEdGraphPin* KnotInput = nullptr;
    UEdGraphPin* KnotOutput = nullptr;
    
    for (UEdGraphPin* Pin : KnotNode->Pins)
    {
        if (Pin->Direction == EGPD_Input)
        {
            KnotInput = Pin;
        }
        else if (Pin->Direction == EGPD_Output)
        {
            KnotOutput = Pin;
        }
    }
    
    if (!KnotInput || !KnotOutput)
    {
        UE_LOG(LogVibeUEReflection, Error, TEXT("InsertRerouteNode: Knot node missing pins"));
        Graph->RemoveNode(KnotNode);
        return nullptr;
    }
    
    // Wire up: Source -> Knot -> Target
    if (const UEdGraphSchema* Schema = Graph->GetSchema())
    {
        // Connect source to knot input
        if (Schema->TryCreateConnection(SourcePin, KnotInput))
        {
            UE_LOG(LogVibeUEReflection, Log, TEXT("InsertRerouteNode: Connected source to reroute"));
        }
        
        // Connect knot output to target
        if (Schema->TryCreateConnection(KnotOutput, TargetPin))
        {
            UE_LOG(LogVibeUEReflection, Log, TEXT("InsertRerouteNode: Connected reroute to target"));
        }
        
        UE_LOG(LogVibeUEReflection, Log,
            TEXT("InsertRerouteNode: Inserted reroute between %s and %s"),
            *SourcePin->GetName(), *TargetPin->GetName());
    }
    
    return KnotNode;
}

/**
 * Create a reroute path with multiple knots
 * Useful for creating clean cable routing
 */
TArray<UK2Node_Knot*> FBlueprintReflection::CreateReroutePath(
    UEdGraph* Graph,
    UEdGraphPin* SourcePin,
    UEdGraphPin* TargetPin,
    const TArray<FVector2D>& Waypoints
)
{
    TArray<UK2Node_Knot*> CreatedKnots;
    
    if (!Graph || !SourcePin || !TargetPin || Waypoints.Num() == 0)
    {
        UE_LOG(LogVibeUEReflection, Warning, 
            TEXT("CreateReroutePath: Invalid parameters or empty waypoints"));
        return CreatedKnots;
    }
    
    UEdGraphPin* CurrentOutput = SourcePin;
    
    // Create knot at each waypoint
    for (const FVector2D& Waypoint : Waypoints)
    {
        UK2Node_Knot* KnotNode = CreateRerouteNode(Graph, Waypoint, &SourcePin->PinType);
        if (!KnotNode)
        {
            UE_LOG(LogVibeUEReflection, Warning, TEXT("CreateReroutePath: Failed to create knot at waypoint"));
            continue;
        }
        
        // Find knot pins
        UEdGraphPin* KnotInput = nullptr;
        UEdGraphPin* KnotOutput = nullptr;
        
        for (UEdGraphPin* Pin : KnotNode->Pins)
        {
            if (Pin->Direction == EGPD_Input)
            {
                KnotInput = Pin;
            }
            else if (Pin->Direction == EGPD_Output)
            {
                KnotOutput = Pin;
            }
        }
        
        if (KnotInput && KnotOutput)
        {
            // Connect previous output to this knot
            if (const UEdGraphSchema* Schema = Graph->GetSchema())
            {
                Schema->TryCreateConnection(CurrentOutput, KnotInput);
            }
            
            CurrentOutput = KnotOutput;
            CreatedKnots.Add(KnotNode);
        }
    }
    
    // Connect final knot to target
    if (CurrentOutput && CreatedKnots.Num() > 0)
    {
        if (const UEdGraphSchema* Schema = Graph->GetSchema())
        {
            Schema->TryCreateConnection(CurrentOutput, TargetPin);
        }
    }
    
    UE_LOG(LogVibeUEReflection, Log,
        TEXT("CreateReroutePath: Created path with %d knots"), CreatedKnots.Num());
    
    return CreatedKnots;
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

namespace
{
    uint32 BuildDefaultContextTargetMask()
    {
        // Enable all context targets that are safe outside of the editor UI so that
        // library/global nodes, blueprint members, and related targets are discoverable.
        return EContextTargetFlags::TARGET_Blueprint |
               EContextTargetFlags::TARGET_BlueprintLibraries |
               EContextTargetFlags::TARGET_NonImportedTypes |
               EContextTargetFlags::TARGET_NodeTarget |
               EContextTargetFlags::TARGET_PinObject |
               EContextTargetFlags::TARGET_SiblingPinObjects |
               EContextTargetFlags::TARGET_SubComponents;
    }

    bool IsUtilityMenuAction(const TSharedPtr<FEdGraphSchemaAction>& Action)
    {
        if (!Action.IsValid())
        {
            return true;
        }

        const FName DummyId = FEdGraphSchemaAction_Dummy::StaticGetTypeId();
        if (Action->GetTypeId() == DummyId)
        {
            return true;
        }

        const FString MenuDescription = Action->GetMenuDescription().ToString();
        return MenuDescription.Equals(TEXT("Paste here"), ESearchCase::IgnoreCase);
    }
}

void FBlueprintReflection::GetBlueprintActionMenuItems(UBlueprint* Blueprint, TArray<TSharedPtr<FEdGraphSchemaAction>>& Actions)
{
    if (!Blueprint)
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("GetBlueprintActionMenuItems: invalid Blueprint"));
        return;
    }

    UEdGraph* TargetGraph = nullptr;
    if (Blueprint->UbergraphPages.Num() > 0)
    {
        TargetGraph = Blueprint->UbergraphPages[0];
    }
    else if (Blueprint->FunctionGraphs.Num() > 0)
    {
        TargetGraph = Blueprint->FunctionGraphs[0];
    }

    if (!TargetGraph)
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("GetBlueprintActionMenuItems: Blueprint %s has no graphs to source context from"), *Blueprint->GetName());
        return;
    }

    UE_LOG(LogVibeUEReflection, Verbose, TEXT("Building Blueprint action menu via MakeContextMenu for %s"), *Blueprint->GetName());

    FBlueprintActionContext Context;
    Context.Blueprints.Add(Blueprint);
    Context.Graphs.Add(TargetGraph);

    const bool bIsContextSensitive = true;
    const uint32 ContextTargetMask = BuildDefaultContextTargetMask();

    FBlueprintActionMenuBuilder MenuBuilder(FBlueprintActionMenuBuilder::DefaultConfig);
    FBlueprintActionMenuUtils::MakeContextMenu(Context, bIsContextSensitive, ContextTargetMask, MenuBuilder);

    const int32 NumDiscoveredActions = MenuBuilder.GetNumActions();
    Actions.Reserve(Actions.Num() + NumDiscoveredActions);

    for (int32 Index = 0; Index < NumDiscoveredActions; ++Index)
    {
        TSharedPtr<FEdGraphSchemaAction> SchemaAction = MenuBuilder.GetSchemaAction(Index);
        if (IsUtilityMenuAction(SchemaAction))
        {
            continue;
        }

        Actions.Add(SchemaAction);
    }

    UE_LOG(LogVibeUEReflection, Log, TEXT("GetBlueprintActionMenuItems: collected %d actions for %s"), Actions.Num(), *Blueprint->GetName());

    if (Actions.Num() == 0)
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("GetBlueprintActionMenuItems: no actions returned from MakeContextMenu, consider reviewing context mask"));
    }
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
    
    // Extract blueprint name
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Find Blueprint using DiscoveryService
    if (!DiscoveryService.IsValid())
    {
        return CreateErrorResponse(TEXT("DiscoveryService not initialized"));
    }

    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return CreateErrorResponse(FindResult.GetErrorMessage());
    }
    UBlueprint* Blueprint = FindResult.GetValue();

    // Call NodeService with full params (descriptor-based discovery)
    if (!NodeService.IsValid())
    {
        return CreateErrorResponse(TEXT("NodeService not initialized"));
    }

    auto Result = NodeService->GetAvailableNodes(Blueprint, Params);
    
    // Return result directly (already in JSON format)
    if (Result.IsError())
    {
        return CreateErrorResponse(Result.GetErrorMessage());
    }
    
    return Result.GetValue();
}

TSharedPtr<FJsonObject> FBlueprintReflectionCommands::HandleDiscoverNodesWithDescriptors(const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogVibeUEReflection, Log, TEXT("HandleDiscoverNodesWithDescriptors called - descriptor-based discovery"));
    
    // Extract blueprint name
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Find Blueprint using DiscoveryService
    if (!DiscoveryService.IsValid())
    {
        return CreateErrorResponse(TEXT("DiscoveryService not initialized"));
    }

    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return CreateErrorResponse(FindResult.GetErrorMessage());
    }
    UBlueprint* Blueprint = FindResult.GetValue();

    // Call NodeService with full params
    if (!NodeService.IsValid())
    {
        return CreateErrorResponse(TEXT("NodeService not initialized"));
    }

    auto Result = NodeService->DiscoverNodesWithDescriptors(Blueprint, Params);
    
    // Return result directly (already in JSON format)
    if (Result.IsError())
    {
        return CreateErrorResponse(Result.GetErrorMessage());
    }
    
    return Result.GetValue();
}

// === BLUEPRINT REFLECTION COMMANDS IMPLEMENTATION ===

FBlueprintReflectionCommands::FBlueprintReflectionCommands()
{
    // Constructor - no initialization needed for now
}

TSharedPtr<FJsonObject> FBlueprintReflectionCommands::HandleAddBlueprintNode(const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogVibeUEReflection, Warning, TEXT("HandleAddBlueprintNode called - descriptor-only pathway engaged"));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    // Extract parameters with better validation and guidance
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        UE_LOG(LogVibeUEReflection, Error, TEXT("Missing blueprint_name parameter"));
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Missing blueprint_name parameter. Use full asset path like '/Game/Blueprints/Actors/BP_MyActor.BP_MyActor'"));
        Result->SetStringField(TEXT("usage_hint"), TEXT("Blueprint name should be the exact package path (package_path from search_items)."));
        return Result;
    }
    UE_LOG(LogVibeUEReflection, Warning, TEXT("Blueprint path: %s"), *BlueprintName);

    // Extract node parameters (supports legacy names but always create an object so we can annotate it)
    TSharedPtr<FJsonObject> NodeParamsShared;
    const TSharedPtr<FJsonObject>* NodeParamsPtr = nullptr;
    if (Params->TryGetObjectField(TEXT("node_params"), NodeParamsPtr) && NodeParamsPtr && NodeParamsPtr->IsValid())
    {
        NodeParamsShared = *NodeParamsPtr;
    }
    else if (Params->TryGetObjectField(TEXT("node_config"), NodeParamsPtr) && NodeParamsPtr && NodeParamsPtr->IsValid())
    {
        NodeParamsShared = *NodeParamsPtr;
    }

    if (!NodeParamsShared.IsValid())
    {
        NodeParamsShared = MakeShared<FJsonObject>();
    }

    // Optional descriptive node identifier (retained for logging + configuration hints)
    FString NodeIdentifier;
    if (Params->TryGetStringField(TEXT("node_type"), NodeIdentifier) && !NodeIdentifier.IsEmpty())
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("Requested node_type: %s"), *NodeIdentifier);
    }
    else if (Params->TryGetStringField(TEXT("node_identifier"), NodeIdentifier) && !NodeIdentifier.IsEmpty())
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("Legacy node_identifier provided: %s"), *NodeIdentifier);
    }

    if (!NodeIdentifier.IsEmpty())
    {
        NodeParamsShared->SetStringField(TEXT("node_type_name"), NodeIdentifier);
    }

    // Extract the required spawner key (top-level or nested)
    FString SpawnerKey;
    if (!Params->TryGetStringField(TEXT("spawner_key"), SpawnerKey))
    {
        NodeParamsShared->TryGetStringField(TEXT("spawner_key"), SpawnerKey);
    }

    if (SpawnerKey.IsEmpty())
    {
        UE_LOG(LogVibeUEReflection, Error, TEXT("Missing required spawner_key parameter"));
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Missing required spawner_key. All node creation must specify node_params.spawner_key obtained from discover_nodes_with_descriptors()."));
        Result->SetStringField(TEXT("usage_hint"), TEXT("Call discover_nodes_with_descriptors() or get_available_blueprint_nodes() first, then pass node_params.spawner_key in manage_blueprint_node."));
        return Result;
    }

    NodeParamsShared->SetStringField(TEXT("spawner_key"), SpawnerKey);

    // Extract desired position
    auto ExtractPosition = [](const TSharedPtr<FJsonObject>& Source, const TCHAR* Field, float& OutX, float& OutY) -> bool
    {
        if (!Source.IsValid())
        {
            return false;
        }

        const TArray<TSharedPtr<FJsonValue>>* PositionArrayPtr = nullptr;
        if (Source->TryGetArrayField(Field, PositionArrayPtr) && PositionArrayPtr && PositionArrayPtr->Num() >= 2)
        {
            OutX = static_cast<float>((*PositionArrayPtr)[0]->AsNumber());
            OutY = static_cast<float>((*PositionArrayPtr)[1]->AsNumber());
            return true;
        }

        return false;
    };

    float PosX = 500.0f;
    float PosY = 500.0f;
    bool bPositionSet = ExtractPosition(NodeParamsShared, TEXT("position"), PosX, PosY) ||
                        ExtractPosition(NodeParamsShared, TEXT("node_position"), PosX, PosY);

    if (!bPositionSet)
    {
        const TArray<TSharedPtr<FJsonValue>>* DirectPositionPtr = nullptr;
        if (Params->TryGetArrayField(TEXT("position"), DirectPositionPtr) && DirectPositionPtr && DirectPositionPtr->Num() >= 2)
        {
            PosX = static_cast<float>((*DirectPositionPtr)[0]->AsNumber());
            PosY = static_cast<float>((*DirectPositionPtr)[1]->AsNumber());
            bPositionSet = true;
        }
        else if (Params->TryGetArrayField(TEXT("node_position"), DirectPositionPtr) && DirectPositionPtr && DirectPositionPtr->Num() >= 2)
        {
            PosX = static_cast<float>((*DirectPositionPtr)[0]->AsNumber());
            PosY = static_cast<float>((*DirectPositionPtr)[1]->AsNumber());
            bPositionSet = true;
        }
    }

    // Persist position inside node params for downstream configuration helpers
    TArray<TSharedPtr<FJsonValue>> PositionArray;
    PositionArray.Add(MakeShared<FJsonValueNumber>(PosX));
    PositionArray.Add(MakeShared<FJsonValueNumber>(PosY));
    NodeParamsShared->SetArrayField(TEXT("position"), PositionArray);

    // Try to load the Blueprint with better path handling
    UBlueprint* Blueprint = nullptr;

    FString AssetPath = BlueprintName;
    if (BlueprintName.Contains(TEXT("/Game/")))
    {
        UE_LOG(LogVibeUEReflection, Log, TEXT("Using provided full path: %s"), *AssetPath);
        Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
    }
    else if (!BlueprintName.Contains(TEXT("/")) && !BlueprintName.Contains(TEXT(".")))
    {
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
        UE_LOG(LogVibeUEReflection, Log, TEXT("Trying to load Blueprint with partial path: %s"), *AssetPath);
        Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
    }

    if (!Blueprint)
    {
        FString ErrorMsg = FString::Printf(TEXT("Could not load Blueprint: %s"), *AssetPath);
        UE_LOG(LogVibeUEReflection, Error, TEXT("%s"), *ErrorMsg);
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), ErrorMsg);
        Result->SetStringField(TEXT("suggestion"), TEXT("Use full asset path like '/Game/Blueprints/Actors/BP_MyActor.BP_MyActor'."));
        Result->SetStringField(TEXT("usage_hint"), TEXT("Use search_items(asset_type='Blueprint') to get the package_path value and pass that here."));
        return Result;
    }

    UE_LOG(LogVibeUEReflection, Warning, TEXT("Blueprint loaded successfully: %s"), *Blueprint->GetName());

    // Resolve target graph (event or function) using graph scoping
    UEdGraph* TargetGraph = nullptr;
    FString GraphScope;
    bool bExplicitFunctionScope = false;
    if (Params->TryGetStringField(TEXT("graph_scope"), GraphScope) && !GraphScope.IsEmpty())
    {
        if (GraphScope.Equals(TEXT("function"), ESearchCase::IgnoreCase))
        {
            bExplicitFunctionScope = true;

            FString FunctionName;
            if (!Params->TryGetStringField(TEXT("function_name"), FunctionName) || FunctionName.IsEmpty())
            {
                Result->SetBoolField(TEXT("success"), false);
                Result->SetStringField(TEXT("error"), TEXT("Missing 'function_name' for function scope"));
                Result->SetStringField(TEXT("usage_hint"), TEXT("Provide the exact function name when graph_scope='function'."));
                return Result;
            }

            const FName FunctionGraphName(*FunctionName);
            TArray<UEdGraph*> AllGraphs;
            Blueprint->GetAllGraphs(AllGraphs);

            for (UEdGraph* Graph : AllGraphs)
            {
                if (!Graph)
                {
                    continue;
                }

                if (Graph->GetFName() == FunctionGraphName || Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
                {
                    if (const UEdGraphSchema_K2* K2Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema()))
                    {
                        const EGraphType GraphType = K2Schema->GetGraphType(Graph);
                        if (GraphType == EGraphType::GT_Function || GraphType == EGraphType::GT_Ubergraph)
                        {
                            TargetGraph = Graph;
                            break;
                        }
                    }
                }
            }

            if (!TargetGraph)
            {
                Result->SetBoolField(TEXT("success"), false);
                Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Function graph not found: %s"), *FunctionName));
                Result->SetStringField(TEXT("suggestion"), TEXT("Verify the function exists and the name matches exactly."));
                return Result;
            }

            UE_LOG(LogVibeUEReflection, Warning, TEXT("Function graph found: %s"), *TargetGraph->GetName());
        }
        else if (!GraphScope.Equals(TEXT("event"), ESearchCase::IgnoreCase))
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Invalid graph_scope: %s (expected 'event' or 'function')"), *GraphScope));
            return Result;
        }
    }

    if (!TargetGraph)
    {
        for (UEdGraph* Graph : Blueprint->UbergraphPages)
        {
            if (Graph && Graph->GetFName() == TEXT("EventGraph"))
            {
                TargetGraph = Graph;
                break;
            }
        }

        if (!TargetGraph)
        {
            UE_LOG(LogVibeUEReflection, Error, TEXT("Could not find EventGraph in Blueprint: %s"), *Blueprint->GetName());
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Could not find EventGraph in Blueprint"));
            return Result;
        }

        UE_LOG(LogVibeUEReflection, Warning, TEXT("EventGraph found: %s"), *TargetGraph->GetName());
    }

    // ✅ NEW: Creation is ONLY allowed through spawner descriptors
    try
    {
        const FVector2D NodePosition(PosX, PosY);
        UK2Node* NewNode = FBlueprintReflection::CreateNodeFromSpawnerKey(TargetGraph, SpawnerKey, NodePosition);

        if (!NewNode)
        {
            UE_LOG(LogVibeUEReflection, Error, TEXT("CreateNodeFromSpawnerKey failed for '%s'"), *SpawnerKey);
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to create node using spawner_key '%s'. The spawner could not be resolved."), *SpawnerKey));
            Result->SetStringField(TEXT("suggestion"), TEXT("Refresh descriptors with discover_nodes_with_descriptors() and retry with a valid spawner_key."));
            return Result;
        }

        // Configure any additional parameters (variable names, casts, etc.)
        if (NodeParamsShared.IsValid())
        {
            FBlueprintReflection::ConfigureNodeFromParameters(NewNode, NodeParamsShared);
        }

        NewNode->NodePosX = FMath::RoundToInt(PosX);
        NewNode->NodePosY = FMath::RoundToInt(PosY);
        NewNode->ReconstructNode();

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        Blueprint->MarkPackageDirty();

        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("node_id"), NewNode->NodeGuid.ToString());
        Result->SetStringField(TEXT("spawner_key"), SpawnerKey);
        Result->SetStringField(TEXT("creation_method"), TEXT("exact_spawner_key"));
        Result->SetStringField(TEXT("graph_name"), TargetGraph ? TargetGraph->GetName() : TEXT("unknown"));
        Result->SetStringField(TEXT("graph_scope"), bExplicitFunctionScope ? TEXT("function") : TEXT("event"));
        Result->SetStringField(TEXT("node_class"), NewNode->GetClass()->GetPathName());
        Result->SetStringField(TEXT("node_display_name"), NewNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
        Result->SetNumberField(TEXT("pin_count"), NewNode->Pins.Num());
        Result->SetNumberField(TEXT("position_x"), PosX);
        Result->SetNumberField(TEXT("position_y"), PosY);

        if (!NodeIdentifier.IsEmpty())
        {
            Result->SetStringField(TEXT("requested_node_type"), NodeIdentifier);
        }

        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Successfully created node via spawner_key '%s'"), *SpawnerKey));

        return Result;
    }
    catch (const std::exception& e)
    {
        FString ErrorMsg = FString::Printf(TEXT("Exception during node creation: %s"), ANSI_TO_TCHAR(e.what()));
        UE_LOG(LogVibeUEReflection, Error, TEXT("%s"), *ErrorMsg);
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), ErrorMsg);
        Result->SetStringField(TEXT("suggestion"), TEXT("Ensure the Blueprint is loaded and the spawner_key is valid."));
        return Result;
    }
    catch (...)
    {
        FString ErrorMsg = TEXT("Unknown exception during descriptor-based node creation");
        UE_LOG(LogVibeUEReflection, Error, TEXT("%s"), *ErrorMsg);
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), ErrorMsg);
        Result->SetStringField(TEXT("suggestion"), TEXT("Verify Blueprint asset path and spawner_key."));
        return Result;
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
        
    auto FindNodeByGuid = [&](UEdGraph* Graph) -> UK2Node*
    {
        if (!Graph)
        {
            return nullptr;
        }

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
        return nullptr;
    };

    // Node IDs in our system are NodeGuid strings (hex format), not integer UniqueIDs
    // Use the same approach as other commands - check the event graph first
    if (UEdGraph* EventGraph = FCommonUtils::FindOrCreateEventGraph(Blueprint))
    {
        if (UK2Node* FoundInEvent = FindNodeByGuid(EventGraph))
        {
            return FoundInEvent;
        }
    }

    // Search function graphs explicitly so node property operations work in custom functions
    for (UEdGraph* FunctionGraph : Blueprint->FunctionGraphs)
    {
        if (UK2Node* FoundInFunction = FindNodeByGuid(FunctionGraph))
        {
            return FoundInFunction;
        }
    }

    // Also search through all other graphs in the blueprint (uber graphs, macros, etc.)
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (UK2Node* FoundInUber = FindNodeByGuid(Graph))
        {
            return FoundInUber;
        }
    }

    // As a final pass, iterate any additional graphs referenced by the Blueprint (e.g. delegate signature graphs)
    TArray<UEdGraph*> AllGraphs;
    Blueprint->GetAllGraphs(AllGraphs);
    for (UEdGraph* Graph : AllGraphs)
    {
        if (UK2Node* FoundNode = FindNodeByGuid(Graph))
        {
            return FoundNode;
        }
    }

    return nullptr;
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

// ═════════════════════════════════════════════════════════════════════════════
// NEW DESIGN IMPLEMENTATION: Node Descriptor System
// ═════════════════════════════════════════════════════════════════════════════

TSharedPtr<FJsonObject> FBlueprintReflection::FPinDescriptor::ToJson() const
{
    TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject);
    Json->SetStringField(TEXT("name"), Name);
    Json->SetStringField(TEXT("type"), Type);
    Json->SetStringField(TEXT("type_path"), TypePath);
    Json->SetStringField(TEXT("direction"), Direction);
    Json->SetStringField(TEXT("category"), Category);
    Json->SetBoolField(TEXT("is_array"), bIsArray);
    Json->SetBoolField(TEXT("is_reference"), bIsReference);
    Json->SetBoolField(TEXT("is_hidden"), bIsHidden);
    Json->SetBoolField(TEXT("is_advanced"), bIsAdvanced);
    Json->SetStringField(TEXT("default_value"), DefaultValue);
    Json->SetStringField(TEXT("tooltip"), Tooltip);
    return Json;
}

TSharedPtr<FJsonObject> FBlueprintReflection::FNodeSpawnerDescriptor::ToJson() const
{
    TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject);
    
    // Core identification
    Json->SetStringField(TEXT("spawner_key"), SpawnerKey);
    Json->SetStringField(TEXT("display_name"), DisplayName);
    Json->SetStringField(TEXT("node_class_name"), NodeClassName);
    Json->SetStringField(TEXT("node_class_path"), NodeClassPath);
    
    // Categorization
    Json->SetStringField(TEXT("category"), Category);
    Json->SetStringField(TEXT("description"), Description);
    Json->SetStringField(TEXT("tooltip"), Tooltip);
    Json->SetStringField(TEXT("node_type"), NodeType);
    
    TArray<TSharedPtr<FJsonValue>> KeywordsArray;
    for (const FString& Keyword : Keywords)
    {
        KeywordsArray.Add(MakeShareable(new FJsonValueString(Keyword)));
    }
    Json->SetArrayField(TEXT("keywords"), KeywordsArray);
    
    // Function metadata (if applicable)
    if (!FunctionName.IsEmpty())
    {
        TSharedPtr<FJsonObject> FunctionMeta = MakeShareable(new FJsonObject);
        FunctionMeta->SetStringField(TEXT("function_name"), FunctionName);
        FunctionMeta->SetStringField(TEXT("function_class"), FunctionClassName);
        FunctionMeta->SetStringField(TEXT("function_class_path"), FunctionClassPath);
        FunctionMeta->SetBoolField(TEXT("is_static"), bIsStatic);
        FunctionMeta->SetBoolField(TEXT("is_const"), bIsConst);
        FunctionMeta->SetBoolField(TEXT("is_pure"), bIsPure);
        FunctionMeta->SetStringField(TEXT("module"), Module);
        Json->SetObjectField(TEXT("function_metadata"), FunctionMeta);
    }
    
    // Variable metadata (if applicable)
    if (!VariableName.IsEmpty())
    {
        TSharedPtr<FJsonObject> VariableMeta = MakeShareable(new FJsonObject);
        VariableMeta->SetStringField(TEXT("variable_name"), VariableName);
        VariableMeta->SetStringField(TEXT("variable_type"), VariableType);
        VariableMeta->SetStringField(TEXT("variable_type_path"), VariableTypePath);
        Json->SetObjectField(TEXT("variable_metadata"), VariableMeta);
    }
    
    // Cast metadata (if applicable)
    if (!TargetClassName.IsEmpty())
    {
        TSharedPtr<FJsonObject> CastMeta = MakeShareable(new FJsonObject);
        CastMeta->SetStringField(TEXT("target_class"), TargetClassName);
        CastMeta->SetStringField(TEXT("target_class_path"), TargetClassPath);
        Json->SetObjectField(TEXT("cast_metadata"), CastMeta);
    }
    
    // Pin information
    TArray<TSharedPtr<FJsonValue>> PinsArray;
    for (const FPinDescriptor& Pin : Pins)
    {
        PinsArray.Add(MakeShareable(new FJsonValueObject(Pin.ToJson())));
    }
    Json->SetArrayField(TEXT("pins"), PinsArray);
    Json->SetNumberField(TEXT("expected_pin_count"), ExpectedPinCount);
    
    return Json;
}

FBlueprintReflection::FNodeSpawnerDescriptor FBlueprintReflection::FNodeSpawnerDescriptor::FromJson(const TSharedPtr<FJsonObject>& Json)
{
    FNodeSpawnerDescriptor Descriptor;
    
    if (!Json.IsValid())
        return Descriptor;
    
    // Core fields
    Json->TryGetStringField(TEXT("spawner_key"), Descriptor.SpawnerKey);
    Json->TryGetStringField(TEXT("display_name"), Descriptor.DisplayName);
    Json->TryGetStringField(TEXT("node_class_name"), Descriptor.NodeClassName);
    Json->TryGetStringField(TEXT("node_type"), Descriptor.NodeType);
    
    // Function metadata
    const TSharedPtr<FJsonObject>* FunctionMeta;
    if (Json->TryGetObjectField(TEXT("function_metadata"), FunctionMeta))
    {
        (*FunctionMeta)->TryGetStringField(TEXT("function_name"), Descriptor.FunctionName);
        (*FunctionMeta)->TryGetStringField(TEXT("function_class"), Descriptor.FunctionClassName);
        (*FunctionMeta)->TryGetStringField(TEXT("function_class_path"), Descriptor.FunctionClassPath);
        (*FunctionMeta)->TryGetBoolField(TEXT("is_static"), Descriptor.bIsStatic);
    }
    
    return Descriptor;
}

void FBlueprintReflection::ExtractPinDescriptors(const UFunction* Function, TArray<FPinDescriptor>& OutPins)
{
    if (!Function)
        return;
    
    OutPins.Empty();
    
    for (TFieldIterator<FProperty> It(Function); It; ++It)
    {
        FProperty* Param = *It;
        
        FPinDescriptor Pin;
        Pin.Name = Param->GetName();
        Pin.Type = Param->GetCPPType();
        
        // Get type path from property class
        if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(Param))
        {
            if (ObjectProp->PropertyClass)
            {
                Pin.TypePath = ObjectProp->PropertyClass->GetPathName();
            }
        }
        else if (FClassProperty* ClassProp = CastField<FClassProperty>(Param))
        {
            if (ClassProp->MetaClass)
            {
                Pin.TypePath = ClassProp->MetaClass->GetPathName();
            }
        }
        else
        {
            // For primitive types, use the property name
            Pin.TypePath = Param->GetClass()->GetName();
        }
        
        // Determine direction
        if (Param->HasAnyPropertyFlags(CPF_ReturnParm))
        {
            Pin.Direction = TEXT("output");
            Pin.Name = TEXT("ReturnValue");
        }
        else if (Param->HasAnyPropertyFlags(CPF_OutParm) && !Param->HasAnyPropertyFlags(CPF_ConstParm))
        {
            Pin.Direction = TEXT("output");
        }
        else
        {
            Pin.Direction = TEXT("input");
        }
        
        Pin.Category = TEXT(""); // Will be filled by pin type analysis
        Pin.bIsArray = Param->IsA<FArrayProperty>();
        Pin.bIsReference = Param->HasAnyPropertyFlags(CPF_ReferenceParm);
        Pin.bIsHidden = false; // Will be determined by metadata
        Pin.bIsAdvanced = Param->HasAnyPropertyFlags(CPF_AdvancedDisplay);
        Pin.DefaultValue = TEXT("");
        Pin.Tooltip = Param->GetToolTipText().ToString();
        
        OutPins.Add(Pin);
    }
}

void FBlueprintReflection::ExtractPinDescriptorsFromNode(UK2Node* Node, TArray<FPinDescriptor>& OutPins)
{
    if (!Node)
        return;
    
    OutPins.Empty();
    
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (!Pin)
            continue;
        
        FPinDescriptor Descriptor;
        Descriptor.Name = Pin->PinName.ToString();
        Descriptor.Type = Pin->PinType.PinCategory.ToString();
        
        if (Pin->PinType.PinSubCategoryObject.IsValid())
        {
            Descriptor.TypePath = Pin->PinType.PinSubCategoryObject->GetPathName();
        }
        
        Descriptor.Direction = (Pin->Direction == EGPD_Input) ? TEXT("input") : TEXT("output");
        Descriptor.Category = Pin->PinType.PinCategory.ToString();
        Descriptor.bIsArray = Pin->PinType.IsArray();
        Descriptor.bIsReference = Pin->PinType.bIsReference;
        Descriptor.bIsHidden = Pin->bHidden;
        Descriptor.bIsAdvanced = Pin->bAdvancedView;
        Descriptor.DefaultValue = Pin->DefaultValue;
        Descriptor.Tooltip = Pin->PinToolTip;
        
        OutPins.Add(Descriptor);
    }
}

FBlueprintReflection::FNodeSpawnerDescriptor FBlueprintReflection::ExtractDescriptorFromSpawner(
    UBlueprintNodeSpawner* Spawner,
    UBlueprint* Blueprint)
{
    FNodeSpawnerDescriptor Descriptor;
    
    if (!Spawner)
        return Descriptor;
    
    Descriptor.Spawner = Spawner;
    Descriptor.DisplayName = Spawner->DefaultMenuSignature.MenuName.ToString();
    Descriptor.Tooltip = Spawner->DefaultMenuSignature.Tooltip.ToString();
    Descriptor.Category = Spawner->DefaultMenuSignature.Category.ToString();
    Descriptor.NodeClassName = Spawner->NodeClass ? Spawner->NodeClass->GetName() : TEXT("");
    Descriptor.NodeClassPath = Spawner->NodeClass ? Spawner->NodeClass->GetPathName() : TEXT("");

    // Provide safe defaults so descriptors always have a valid classification/key even if
    // specialized extraction fails (e.g. unexpected spawner subclass).
    Descriptor.NodeType = TEXT("generic");
    Descriptor.SpawnerKey = Descriptor.DisplayName;
    
    // Extract function-specific metadata
    if (UBlueprintFunctionNodeSpawner* FunctionSpawner = Cast<UBlueprintFunctionNodeSpawner>(Spawner))
    {
        UClass* NodeClass = FunctionSpawner->NodeClass;
        if (!NodeClass)
        {
            UE_LOG(LogVibeUEReflection, Warning, TEXT("ExtractDescriptorFromSpawner: Function spawner '%s' has no NodeClass"), *Descriptor.DisplayName);
        }
        else if (!NodeClass->IsChildOf(UK2Node_CallFunction::StaticClass()))
        {
            UE_LOG(LogVibeUEReflection, Warning,
                TEXT("ExtractDescriptorFromSpawner: Function spawner '%s' NodeClass '%s' is not a UK2Node_CallFunction; treating as generic"),
                *Descriptor.DisplayName, *NodeClass->GetName());
        }
        else if (const UFunction* Function = FunctionSpawner->GetFunction())
        {
            const UClass* OwnerClass = Function->GetOuterUClass();

            if (!OwnerClass)
            {
                if (const UObject* OuterObj = Function->GetOuter())
                {
                    OwnerClass = Cast<UClass>(OuterObj);
                }
            }

            if (!OwnerClass)
            {
                UE_LOG(LogVibeUEReflection, Warning,
                    TEXT("ExtractDescriptorFromSpawner: Function '%s' has no owning class; treating spawner '%s' as generic"),
                    *Function->GetName(), *Descriptor.DisplayName);
            }
            else
            {
                Descriptor.NodeType = TEXT("function_call");
                Descriptor.FunctionName = Function->GetName();
                Descriptor.FunctionClassName = OwnerClass->GetName();
                Descriptor.FunctionClassPath = OwnerClass->GetPathName();
                Descriptor.bIsStatic = Function->HasAnyFunctionFlags(FUNC_Static);
                Descriptor.bIsConst = Function->HasAnyFunctionFlags(FUNC_Const);
                Descriptor.bIsPure = Function->HasAnyFunctionFlags(FUNC_BlueprintPure);

                // Build unique spawner key
                Descriptor.SpawnerKey = FString::Printf(TEXT("%s::%s"),
                    *Descriptor.FunctionClassName, *Descriptor.FunctionName);

                // Extract module name from class path
                const FString ClassPath = Descriptor.FunctionClassPath;
                if (ClassPath.StartsWith(TEXT("/Script/")))
                {
                    int32 DotIndex;
                    if (ClassPath.FindChar('.', DotIndex))
                    {
                        Descriptor.Module = ClassPath.Mid(8, DotIndex - 8); // Skip "/Script/"
                    }
                }

                // Extract pin descriptors
                ExtractPinDescriptors(Function, Descriptor.Pins);
                Descriptor.ExpectedPinCount = Descriptor.Pins.Num();
            }
        }
        else
        {
            UE_LOG(LogVibeUEReflection, Warning,
                TEXT("ExtractDescriptorFromSpawner: Function spawner '%s' returned null function; treating as generic"),
                *Descriptor.DisplayName);
        }
    }
    // Extract variable-specific metadata
    else if (UBlueprintVariableNodeSpawner* VariableSpawner = Cast<UBlueprintVariableNodeSpawner>(Spawner))
    {
        // Determine if this is a GET or SET node
        bool bIsGetter = VariableSpawner->NodeClass && VariableSpawner->NodeClass->IsChildOf(UK2Node_VariableGet::StaticClass());
        Descriptor.NodeType = bIsGetter ? TEXT("variable_get") : TEXT("variable_set");
        
        // Extract variable information
        UClass* OwnerClass = nullptr;
        FProperty const* VarProperty = VariableSpawner->GetVarProperty();
        FString VariableName;
        
        // Get variable name from property
        if (VarProperty)
        {
            VariableName = VarProperty->GetName();
            OwnerClass = VarProperty->GetOwnerClass();
        }
        // For local variables, check LocalVarDesc (if accessible)
        else if (VariableSpawner->IsLocalVariable())
        {
            // Local variable - use the local context
            // Note: LocalVarDesc is private, but we can still use the spawner
            // Just set a generic name for now - local vars aren't external anyway
            VariableName = Descriptor.DisplayName;
        }
        
        // Fallback to outer if we don't have owner class yet
        if (!OwnerClass && VariableSpawner->GetOuter())
        {
            if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(VariableSpawner->GetOuter()))
            {
                OwnerClass = BPGC;
            }
            else if (UBlueprint* OwnerBP = Cast<UBlueprint>(VariableSpawner->GetOuter()))
            {
                OwnerClass = OwnerBP->GeneratedClass;
            }
        }
        
        // Set variable name
        if (!VariableName.IsEmpty())
        {
            Descriptor.VariableName = VariableName;
            
            // Build spawner key: "GET VariableName" or "SET VariableName"
            FString Operation = bIsGetter ? TEXT("GET") : TEXT("SET");
            Descriptor.SpawnerKey = FString::Printf(TEXT("%s %s"), *Operation, *Descriptor.VariableName);
            
            // If we have owner class information, check if it's external
            if (OwnerClass)
            {
                Descriptor.OwnerClassName = OwnerClass->GetName();
                Descriptor.OwnerClassPath = OwnerClass->GetPathName();
                
                // Check if this is an external variable (not from the current Blueprint)
                if (Blueprint && OwnerClass != Blueprint->GeneratedClass)
                {
                    Descriptor.bIsExternalMember = true;
                    // Enhanced spawner key for external: "ClassName::GET VariableName"
                    Descriptor.SpawnerKey = FString::Printf(TEXT("%s::%s %s"),
                        *Descriptor.OwnerClassName, *Operation, *Descriptor.VariableName);
                }
            }
            
            UE_LOG(LogVibeUEReflection, Verbose, TEXT("  Variable node: %s (External: %s)"),
                *Descriptor.SpawnerKey, Descriptor.bIsExternalMember ? TEXT("Yes") : TEXT("No"));
        }
    }
    // Extract other node types as needed
    else
    {
        Descriptor.NodeType = TEXT("generic");
        Descriptor.SpawnerKey = Descriptor.DisplayName;
    }
    
    return Descriptor;
}

TArray<FBlueprintReflection::FNodeSpawnerDescriptor> FBlueprintReflection::DiscoverNodesWithDescriptors(
    UBlueprint* Blueprint,
    const FString& SearchTerm,
    const FString& CategoryFilter,
    const FString& ClassFilter,
    int32 MaxResults)
{
    TArray<FNodeSpawnerDescriptor> Descriptors;
    
    if (!Blueprint)
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("DiscoverNodesWithDescriptors: Blueprint is null"));
        return Descriptors;
    }
    
    UE_LOG(LogVibeUEReflection, Log, TEXT("DiscoverNodesWithDescriptors: Search='%s', Category='%s', Class='%s', Max=%d"),
           *SearchTerm, *CategoryFilter, *ClassFilter, MaxResults);
    
    FBlueprintActionDatabase& ActionDB = FBlueprintActionDatabase::Get();
    const FBlueprintActionDatabase::FActionRegistry& AllActions = ActionDB.GetAllActions();
    
    int32 Count = 0;
    
    for (auto& ActionEntry : AllActions)
    {
        if (Count >= MaxResults)
            break;
        
        const FBlueprintActionDatabase::FActionList& ActionList = ActionEntry.Value;
        
        for (UBlueprintNodeSpawner* Spawner : ActionList)
        {
            if (Count >= MaxResults)
                break;
            
            if (!Spawner)
                continue;
            
            // Extract complete descriptor
            FNodeSpawnerDescriptor Descriptor = ExtractDescriptorFromSpawner(Spawner, Blueprint);
            
            // Apply filters
            bool bPassesFilters = true;
            
            // Search term filter
            if (!SearchTerm.IsEmpty())
            {
                bPassesFilters = Descriptor.DisplayName.Contains(SearchTerm, ESearchCase::IgnoreCase) ||
                                Descriptor.FunctionName.Contains(SearchTerm, ESearchCase::IgnoreCase) ||
                                Descriptor.SpawnerKey.Contains(SearchTerm, ESearchCase::IgnoreCase);
            }
            
            // Category filter
            if (bPassesFilters && !CategoryFilter.IsEmpty())
            {
                bPassesFilters = Descriptor.Category.Contains(CategoryFilter, ESearchCase::IgnoreCase);
            }
            
            // Class filter (function class name)
            if (bPassesFilters && !ClassFilter.IsEmpty())
            {
                bPassesFilters = Descriptor.FunctionClassName.Contains(ClassFilter, ESearchCase::IgnoreCase) ||
                                Descriptor.FunctionClassPath.Contains(ClassFilter, ESearchCase::IgnoreCase);
            }
            
            if (bPassesFilters)
            {
                // Cache the spawner
                if (!Descriptor.SpawnerKey.IsEmpty())
                {
                    CacheSpawner(Descriptor.SpawnerKey, Spawner);
                }
                
                Descriptors.Add(Descriptor);
                Count++;
                
                UE_LOG(LogVibeUEReflection, Verbose, TEXT("  ✓ Added descriptor: %s (Key: %s)"),
                       *Descriptor.DisplayName, *Descriptor.SpawnerKey);
            }
        }
    }
    
    // ═════════════════════════════════════════════════════════════════════
    // NEW (Oct 6, 2025): Add synthetic descriptors for special node types
    // ═════════════════════════════════════════════════════════════════════
    
    // Add reroute node (K2Node_Knot) as a synthetic descriptor
    // Reroute nodes don't have spawners but are essential for clean Blueprint wiring
    if (Count < MaxResults && (SearchTerm.IsEmpty() || 
        FString(TEXT("Reroute")).Contains(SearchTerm, ESearchCase::IgnoreCase) ||
        FString(TEXT("Knot")).Contains(SearchTerm, ESearchCase::IgnoreCase)))
    {
        FNodeSpawnerDescriptor RerouteDescriptor;
        RerouteDescriptor.NodeType = TEXT("reroute");
        RerouteDescriptor.DisplayName = TEXT("Reroute Node");
        RerouteDescriptor.SpawnerKey = TEXT("K2Node_Knot");
        RerouteDescriptor.NodeClassName = TEXT("K2Node_Knot");
        RerouteDescriptor.NodeClassPath = TEXT("/Script/BlueprintGraph.K2Node_Knot");
        RerouteDescriptor.Category = TEXT("Utilities");
        RerouteDescriptor.Tooltip = TEXT("Creates a reroute node for cleaner wire routing. Reroute nodes are cosmetic and don't affect performance.");
        RerouteDescriptor.bIsSynthetic = true; // No real spawner
        RerouteDescriptor.ExpectedPinCount = 2; // InputPin + OutputPin
        RerouteDescriptor.Spawner = nullptr; // Handled specially
        
        Descriptors.Add(RerouteDescriptor);
        Count++;
        
        UE_LOG(LogVibeUEReflection, Verbose, TEXT("  ✓ Added synthetic descriptor: Reroute Node (K2Node_Knot)"));
    }
    
    UE_LOG(LogVibeUEReflection, Log, TEXT("DiscoverNodesWithDescriptors: Found %d descriptors (including %d synthetic)"), 
        Descriptors.Num(), Descriptors.FilterByPredicate([](const FNodeSpawnerDescriptor& D) { return D.bIsSynthetic; }).Num());
    
    return Descriptors;
}

UK2Node* FBlueprintReflection::CreateNodeFromDescriptor(
    UEdGraph* Graph,
    const FNodeSpawnerDescriptor& Descriptor,
    FVector2D Position)
{
    if (!Graph)
    {
        UE_LOG(LogVibeUEReflection, Error, TEXT("CreateNodeFromDescriptor: Graph is null"));
        return nullptr;
    }
    
    if (!Descriptor.Spawner)
    {
        UE_LOG(LogVibeUEReflection, Error, TEXT("CreateNodeFromDescriptor: Descriptor has no spawner"));
        return nullptr;
    }
    
    UE_LOG(LogVibeUEReflection, Warning, TEXT("CreateNodeFromDescriptor: Creating node from descriptor '%s' (Key: %s)"),
           *Descriptor.DisplayName, *Descriptor.SpawnerKey);
    
    // Use the spawner directly - NO SEARCHING
    FBlueprintActionContext Context;
    Context.Graphs.Add(Graph);
    if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph))
    {
        Context.Blueprints.Add(Blueprint);
    }
    
    UEdGraphNode* NewNode = Descriptor.Spawner->Invoke(Graph, IBlueprintNodeBinder::FBindingSet(), Position);
    
    if (UK2Node* K2Node = Cast<UK2Node>(NewNode))
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("CreateNodeFromDescriptor: Successfully created node with %d pins"),
               K2Node->Pins.Num());
        return K2Node;
    }
    
    UE_LOG(LogVibeUEReflection, Error, TEXT("CreateNodeFromDescriptor: Failed to create K2Node"));
    return nullptr;
}

UK2Node* FBlueprintReflection::CreateNodeFromSpawnerKey(
    UEdGraph* Graph,
    const FString& SpawnerKey,
    FVector2D Position)
{
    if (!Graph || SpawnerKey.IsEmpty())
    {
        UE_LOG(LogVibeUEReflection, Error, TEXT("CreateNodeFromSpawnerKey: Invalid parameters"));
        return nullptr;
    }
    
    UE_LOG(LogVibeUEReflection, Warning, TEXT("CreateNodeFromSpawnerKey: Looking up spawner with key '%s'"), *SpawnerKey);
    
    // ═════════════════════════════════════════════════════════════════════
    // NEW (Oct 6, 2025): Special handling for synthetic nodes
    // ═════════════════════════════════════════════════════════════════════
    
    // Handle reroute nodes (K2Node_Knot) - no spawner needed
    if (SpawnerKey.Equals(TEXT("K2Node_Knot"), ESearchCase::IgnoreCase))
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("CreateNodeFromSpawnerKey: Creating synthetic reroute node"));
        return CreateRerouteNode(Graph, Position);
    }
    
    // Try cached spawner first
    UBlueprintNodeSpawner* Spawner = GetSpawnerByKey(SpawnerKey);
    
    if (!Spawner)
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("CreateNodeFromSpawnerKey: Spawner not in cache, searching..."));
        
        // If not cached, search for it
        UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
        if (Blueprint)
        {
            TArray<FNodeSpawnerDescriptor> Descriptors = DiscoverNodesWithDescriptors(Blueprint, TEXT(""), TEXT(""), TEXT(""), 1000);
            
            for (const FNodeSpawnerDescriptor& Desc : Descriptors)
            {
                if (Desc.SpawnerKey.Equals(SpawnerKey, ESearchCase::IgnoreCase))
                {
                    Spawner = Desc.Spawner;
                    CacheSpawner(SpawnerKey, Spawner);
                    UE_LOG(LogVibeUEReflection, Warning, TEXT("CreateNodeFromSpawnerKey: Found and cached spawner"));
                    break;
                }
            }
        }
    }
    
    if (!Spawner)
    {
        UE_LOG(LogVibeUEReflection, Error, TEXT("CreateNodeFromSpawnerKey: Could not find spawner for key '%s'"), *SpawnerKey);
        return nullptr;
    }
    
    // Create descriptor and use it
    FNodeSpawnerDescriptor Descriptor = ExtractDescriptorFromSpawner(Spawner);
    return CreateNodeFromDescriptor(Graph, Descriptor, Position);
}

UBlueprintNodeSpawner* FBlueprintReflection::GetSpawnerByKey(const FString& SpawnerKey)
{
    if (SpawnerKey.IsEmpty())
    {
        return nullptr;
    }

    if (TWeakObjectPtr<UBlueprintNodeSpawner>* Found = CachedNodeSpawners.Find(SpawnerKey))
    {
        if (Found->IsValid())
        {
            return Found->Get();
        }

        // Remove stale entry to avoid future crashes from dangling pointers
        CachedNodeSpawners.Remove(SpawnerKey);
        UE_LOG(LogVibeUEReflection, Verbose, TEXT("GetSpawnerByKey: Removed stale cache entry for '%s'"), *SpawnerKey);
    }

    return nullptr;
}

void FBlueprintReflection::CacheSpawner(const FString& SpawnerKey, UBlueprintNodeSpawner* Spawner)
{
    if (!SpawnerKey.IsEmpty() && Spawner)
    {
        CachedNodeSpawners.Add(SpawnerKey, Spawner);
        UE_LOG(LogVibeUEReflection, Verbose, TEXT("CacheSpawner: Cached spawner '%s'"), *SpawnerKey);
    }
}

