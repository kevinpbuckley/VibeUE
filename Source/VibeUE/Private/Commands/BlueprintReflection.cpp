#include "Commands/BlueprintReflection.h"
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
TMap<FString, UBlueprintNodeSpawner*> FBlueprintReflection::CachedNodeSpawners;

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
                    CachedNodeSpawners.Add(NodeType, NodeSpawner);
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
                            CachedNodeSpawners.Add(NodeType, NodeSpawner);
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

    // ENHANCED: Try to find the function node using the BlueprintActionDatabase first
    // This integrates the discovery system with the creation system
    FString NodeTypeName;
    if (NodeParams->TryGetStringField(TEXT("node_type_name"), NodeTypeName))
    {
        UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Attempting to configure using discovered node type: %s"), *NodeTypeName);
        
        // ENHANCED: First check cached spawners for performance
        if (UBlueprintNodeSpawner** CachedSpawner = CachedNodeSpawners.Find(NodeTypeName))
        {
            if (UBlueprintFunctionNodeSpawner* FunctionSpawner = Cast<UBlueprintFunctionNodeSpawner>(*CachedSpawner))
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
                        // Cache this spawner for future use
                        CachedNodeSpawners.Add(NodeTypeName, NodeSpawner);
                        
                        // Found matching spawner - extract function information
                        if (UBlueprintFunctionNodeSpawner* FunctionSpawner = Cast<UBlueprintFunctionNodeSpawner>(NodeSpawner))
                        {
                            if (const UFunction* FoundFunction = FunctionSpawner->GetFunction())
                            {
                                UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Found function via spawner: %s::%s"), 
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
        UE_LOG(LogVibeUEReflection, Warning, TEXT("ConfigureFunctionNode: Could not find spawner for node type: %s"), *NodeTypeName);
    }

    // FALLBACK: Original parameter-based configuration
    FString FunctionName;
    if (!NodeParams->TryGetStringField(TEXT("function_name"), FunctionName))
    {
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
        
    // Find the variable in the Blueprint
    if (UBlueprint* Blueprint = VariableNode->GetBlueprint())
    {
        FName VarName(*VariableName);
        VariableNode->VariableReference.SetSelfMember(VarName);
        VariableNode->AllocateDefaultPins();
        VariableNode->ReconstructNode();
        UE_LOG(LogVibeUEReflection, Log, TEXT("Set variable get node to reference: %s"), *VariableName);
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
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    
    // Extract parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Missing blueprint_name parameter"));
        return Result;
    }
    
    FString Category;
    Params->TryGetStringField(TEXT("category"), Category);

    FString SearchTerm;
    if (!Params->TryGetStringField(TEXT("search_term"), SearchTerm))
    {
        if (!Params->TryGetStringField(TEXT("searchTerm"), SearchTerm))
        {
            Params->TryGetStringField(TEXT("searchterm"), SearchTerm);
        }
    }

    // Normalize user inputs for consistent filtering
    Category.TrimStartAndEndInline();
    SearchTerm.TrimStartAndEndInline();

    FString CategoryLower = Category.ToLower();
    FString SearchTermLower = SearchTerm.ToLower();

    bool bIncludeFunctions = true;
    bool bIncludeVariables = true;
    bool bIncludeEvents = true;

    Params->TryGetBoolField(TEXT("include_functions"), bIncludeFunctions);
    Params->TryGetBoolField(TEXT("includeFunctions"), bIncludeFunctions);
    Params->TryGetBoolField(TEXT("include_variables"), bIncludeVariables);
    Params->TryGetBoolField(TEXT("includeVariables"), bIncludeVariables);
    Params->TryGetBoolField(TEXT("include_events"), bIncludeEvents);
    Params->TryGetBoolField(TEXT("includeEvents"), bIncludeEvents);

    int32 MaxResults = 100;
    double ParsedMaxResults = 0.0;
    if (Params->TryGetNumberField(TEXT("max_results"), ParsedMaxResults) ||
        Params->TryGetNumberField(TEXT("maxResults"), ParsedMaxResults))
    {
        MaxResults = FMath::Max(1, static_cast<int32>(ParsedMaxResults));
    }

    UE_LOG(LogVibeUEReflection, Log, TEXT("Search params - Category: '%s', SearchTerm: '%s', IncludeFunctions=%s, IncludeVariables=%s, IncludeEvents=%s, MaxResults=%d"),
        *Category, *SearchTerm,
        bIncludeFunctions ? TEXT("true") : TEXT("false"),
        bIncludeVariables ? TEXT("true") : TEXT("false"),
        bIncludeEvents ? TEXT("true") : TEXT("false"),
        MaxResults);
    
    // Find the Blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
        return Result;
    }
    
    // Discover all available actions using Unreal's reflection system
    TArray<TSharedPtr<FEdGraphSchemaAction>> AllActions;
    FBlueprintReflection::GetBlueprintActionMenuItems(Blueprint, AllActions);
    
    // Organize actions by category
    TMap<FString, TArray<TSharedPtr<FJsonValue>>> CategoryMap;
    int32 TotalNodes = 0;
    
    TArray<FString> FilterTerms;
    TArray<FString> SanitizedFilterTerms;

    if (!SearchTermLower.IsEmpty())
    {
        SearchTermLower.ParseIntoArray(FilterTerms, TEXT(" "), true);
        if (FilterTerms.Num() == 0)
        {
            FilterTerms.Add(SearchTermLower);
        }

        for (FString& Term : FilterTerms)
        {
            Term.TrimStartAndEndInline();
            if (!Term.IsEmpty())
            {
                FString SanitizedDisplay = FName::NameToDisplayString(Term, false);
                SanitizedDisplay.ReplaceInline(TEXT(" "), TEXT(""));
                SanitizedDisplay.ReplaceInline(TEXT("_"), TEXT(""));
                SanitizedFilterTerms.Add(SanitizedDisplay.ToLower());
            }
            else
            {
                SanitizedFilterTerms.Add(TEXT(""));
            }
        }
    }

    bool bTruncated = false;

    for (auto& Action : AllActions)
    {
        if (!Action.IsValid()) continue;
        
        // Process action to JSON
        TSharedPtr<FJsonObject> ActionJson = FBlueprintReflection::ProcessActionToJson(Action);
        if (!ActionJson.IsValid()) continue;
        
        FString ActionCategory = ActionJson->GetStringField(TEXT("category"));
        FString ActionName = ActionJson->GetStringField(TEXT("name"));
        FString ActionDescription = ActionJson->GetStringField(TEXT("description"));
        FString ActionKeywords = ActionJson->GetStringField(TEXT("keywords"));
        FString ActionType = ActionJson->GetStringField(TEXT("type"));
        
        // Apply filters
        if (!CategoryLower.IsEmpty() && CategoryLower != TEXT("all") && CategoryLower != TEXT("*"))
        {
            FString ActionCategoryLower = ActionCategory.ToLower();
            if (!ActionCategoryLower.Contains(CategoryLower))
            {
                continue;
            }
        }

        // Enhanced search term filtering - match all terms across name, description, and keywords (case-insensitive)
        if (FilterTerms.Num() > 0)
        {
            FString ActionNameLower = ActionName.ToLower();
            FString ActionDescriptionLower = ActionDescription.ToLower();
            FString ActionKeywordsLower = ActionKeywords.ToLower();

            const FString CombinedSearchText = ActionNameLower + TEXT(" ") + ActionDescriptionLower + TEXT(" ") + ActionKeywordsLower;
            FString CombinedSanitized = CombinedSearchText;
            CombinedSanitized.ReplaceInline(TEXT(" "), TEXT(""));
            CombinedSanitized.ReplaceInline(TEXT("_"), TEXT(""));

            bool bMatchesAllTerms = true;
            for (int32 TermIndex = 0; TermIndex < FilterTerms.Num() && bMatchesAllTerms; ++TermIndex)
            {
                const FString& Term = FilterTerms[TermIndex];
                const FString& SanitizedTerm = SanitizedFilterTerms.IsValidIndex(TermIndex) ? SanitizedFilterTerms[TermIndex] : Term;

                if (!Term.IsEmpty())
                {
                    if (!CombinedSearchText.Contains(Term))
                    {
                        if (!CombinedSanitized.Contains(SanitizedTerm))
                        {
                            bMatchesAllTerms = false;
                        }
                    }
                }
            }

            if (!bMatchesAllTerms)
            {
                continue;
            }
        }
        
        // Apply type filters
        if (!bIncludeFunctions && ActionType == TEXT("function")) continue;
        if (!bIncludeVariables && ActionType == TEXT("variable")) continue;
        if (!bIncludeEvents && ActionType == TEXT("event")) continue;
        
        // Add to appropriate category
        if (!CategoryMap.Contains(ActionCategory))
        {
            CategoryMap.Add(ActionCategory, TArray<TSharedPtr<FJsonValue>>());
        }
        
        CategoryMap[ActionCategory].Add(MakeShared<FJsonValueObject>(ActionJson));
        TotalNodes++;

        if (TotalNodes >= MaxResults)
        {
            bTruncated = true;
            break;
        }
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
    Result->SetBoolField(TEXT("truncated"), bTruncated);
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
    bool bHasNodeType = Params->TryGetStringField(TEXT("node_type"), NodeIdentifier);
    if (!bHasNodeType)
    {
        if (Params->TryGetStringField(TEXT("node_identifier"), NodeIdentifier))
        {
            UE_LOG(LogVibeUEReflection, Warning, TEXT("Legacy 'node_identifier' parameter received, using it as node type"));
        }
        else
        {
            UE_LOG(LogVibeUEReflection, Error, TEXT("Missing node_type parameter"));
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Missing node_type parameter. Use node types like 'Branch', 'Print String', 'GetVariable', 'SetVariable', 'Self', etc."));
            Result->SetStringField(TEXT("usage_hint"), TEXT("Node type should be a descriptive name like 'Branch' or 'Print String'"));
            return Result;
        }
    }
    UE_LOG(LogVibeUEReflection, Warning, TEXT("Node type: %s"), *NodeIdentifier);

    // Extract node parameters (supports legacy names)
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
    const FJsonObject* NodeParamsObj = NodeParamsShared.Get();

    // Extract position parameters with better defaults
    float PosX = 500.0f, PosY = 500.0f;  // Default position
    bool bPositionSet = false;
    if (NodeParamsObj)
    {
        const TArray<TSharedPtr<FJsonValue>>* PositionArrayPtr = nullptr;
        if (NodeParamsObj->TryGetArrayField(TEXT("position"), PositionArrayPtr) && PositionArrayPtr && PositionArrayPtr->Num() >= 2)
        {
            PosX = (*PositionArrayPtr)[0]->AsNumber();
            PosY = (*PositionArrayPtr)[1]->AsNumber();
            bPositionSet = true;
            UE_LOG(LogVibeUEReflection, Warning, TEXT("Position (node_params.position): (%f, %f)"), PosX, PosY);
        }
        else if (NodeParamsObj->TryGetArrayField(TEXT("node_position"), PositionArrayPtr) && PositionArrayPtr && PositionArrayPtr->Num() >= 2)
        {
            PosX = (*PositionArrayPtr)[0]->AsNumber();
            PosY = (*PositionArrayPtr)[1]->AsNumber();
            bPositionSet = true;
            UE_LOG(LogVibeUEReflection, Warning, TEXT("Position (node_params.node_position): (%f, %f)"), PosX, PosY);
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* DirectPositionPtr = nullptr;
    if (!bPositionSet && Params->TryGetArrayField(TEXT("position"), DirectPositionPtr) && DirectPositionPtr && DirectPositionPtr->Num() >= 2)
    {
        PosX = (*DirectPositionPtr)[0]->AsNumber();
        PosY = (*DirectPositionPtr)[1]->AsNumber();
        bPositionSet = true;
        UE_LOG(LogVibeUEReflection, Warning, TEXT("Direct position parameter: (%f, %f)"), PosX, PosY);
    }
    if (!bPositionSet && Params->TryGetArrayField(TEXT("node_position"), DirectPositionPtr) && DirectPositionPtr && DirectPositionPtr->Num() >= 2)
    {
        PosX = (*DirectPositionPtr)[0]->AsNumber();
        PosY = (*DirectPositionPtr)[1]->AsNumber();
        bPositionSet = true;
        UE_LOG(LogVibeUEReflection, Warning, TEXT("Direct node_position parameter: (%f, %f)"), PosX, PosY);
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
                Result->SetStringField(TEXT("usage_hint"), TEXT("Provide the exact function name when graph_scope='function'"));
                return Result;
            }

            const FName FunctionGraphName(*FunctionName);

            // Search all graphs to handle editor-created and rebuilt graphs consistently
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
                Result->SetStringField(TEXT("suggestion"), TEXT("Verify the function exists and the name matches exactly"));
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

    // Default to event graph only when no explicit function scope was requested
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

    // ENHANCED: Prioritize reflection system over hardcoded node creation
    UEdGraphNode* NewNode = nullptr;
    FString NodeId;

    UE_LOG(LogVibeUEReflection, Warning, TEXT("Creating node '%s' - trying reflection system first"), *NodeIdentifier);

    try
    {
        // FIRST: Try the true reflection-based creation system for ALL node types
        TSharedPtr<FJsonObject> NodeParamsJson = NodeParamsShared.IsValid()
            ? MakeShareable(new FJsonObject(*NodeParamsShared))
            : MakeShareable(new FJsonObject);

        // Add position to node params for reflection system
        TArray<TSharedPtr<FJsonValue>> PositionArray;
        PositionArray.Add(MakeShared<FJsonValueNumber>(PosX));
        PositionArray.Add(MakeShared<FJsonValueNumber>(PosY));
        NodeParamsJson->SetArrayField(TEXT("position"), PositionArray);

    TSharedPtr<FJsonObject> ReflectionResult = CreateBlueprintNodeInGraph(Blueprint, NodeIdentifier, NodeParamsJson, TargetGraph);

        if (ReflectionResult.IsValid() && ReflectionResult->GetBoolField(TEXT("success")))
        {
            // Success! Extract the created node info
            FString ReflectionNodeId = ReflectionResult->GetStringField(TEXT("node_id"));
            UE_LOG(LogVibeUEReflection, Warning, TEXT("Successfully created node via REFLECTION SYSTEM: %s (ID: %s)"), *NodeIdentifier, *ReflectionNodeId);

            Result->SetBoolField(TEXT("success"), true);
            Result->SetStringField(TEXT("node_type"), NodeIdentifier);
            Result->SetStringField(TEXT("node_id"), ReflectionNodeId);
            Result->SetStringField(TEXT("creation_method"), TEXT("reflection_system"));
            Result->SetStringField(TEXT("graph_name"), TargetGraph ? TargetGraph->GetName() : TEXT("unknown"));
            Result->SetStringField(TEXT("graph_scope"), bExplicitFunctionScope ? TEXT("function") : TEXT("event"));
            Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Successfully created %s node via reflection system in Blueprint %s"), *NodeIdentifier, *Blueprint->GetName()));
            Result->SetObjectField(TEXT("reflection_result"), ReflectionResult);
            return Result;
        }

        // FALLBACK: Use hardcoded creation for common node types if reflection fails
        UE_LOG(LogVibeUEReflection, Warning, TEXT("Reflection system failed for '%s', trying hardcoded fallback"), *NodeIdentifier);

        if (NodeIdentifier == TEXT("Branch"))
        {
            // Create a Branch node
            UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(TargetGraph);
            if (BranchNode)
            {
                BranchNode->CreateNewGuid();
                BranchNode->NodePosX = PosX;
                BranchNode->NodePosY = PosY;
                BranchNode->AllocateDefaultPins();
                TargetGraph->AddNode(BranchNode, true);
                NewNode = BranchNode;
                NodeId = BranchNode->NodeGuid.ToString();
                UE_LOG(LogVibeUEReflection, Warning, TEXT("Created Branch node at (%f, %f) with GUID: %s"), PosX, PosY, *LexToString(BranchNode->NodeGuid));
            }
        }
        else if (NodeIdentifier == TEXT("Print String"))
        {
            // Create a Print String node
            UK2Node_CallFunction* PrintNode = NewObject<UK2Node_CallFunction>(TargetGraph);
            if (PrintNode)
            {
                PrintNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString), UKismetSystemLibrary::StaticClass());
                PrintNode->CreateNewGuid();
                PrintNode->NodePosX = PosX;
                PrintNode->NodePosY = PosY;
                PrintNode->AllocateDefaultPins();
                TargetGraph->AddNode(PrintNode, true);
                NewNode = PrintNode;
                NodeId = PrintNode->NodeGuid.ToString();
                UE_LOG(LogVibeUEReflection, Warning, TEXT("Created Print String node at (%f, %f) with GUID: %s"), PosX, PosY, *LexToString(PrintNode->NodeGuid));
            }
        }
        else if (NodeIdentifier == TEXT("Cast To Object"))
        {
            // Create a Cast node (generic cast)
            UK2Node_DynamicCast* CastNode = NewObject<UK2Node_DynamicCast>(TargetGraph);
            if (CastNode)
            {
                // Set target class to UObject as default
                CastNode->TargetType = UObject::StaticClass();
                CastNode->CreateNewGuid();
                CastNode->NodePosX = PosX;
                CastNode->NodePosY = PosY;
                CastNode->AllocateDefaultPins();
                TargetGraph->AddNode(CastNode, true);
                NewNode = CastNode;
                NodeId = CastNode->NodeGuid.ToString();
                UE_LOG(LogVibeUEReflection, Warning, TEXT("Created Cast To Object node at (%f, %f) with GUID: %s"), PosX, PosY, *LexToString(CastNode->NodeGuid));
            }
        }
        else if (NodeIdentifier == TEXT("GetVariable"))
        {
            // Create a Get Variable node
            UK2Node_VariableGet* GetVariableNode = NewObject<UK2Node_VariableGet>(TargetGraph);
            if (GetVariableNode)
            {
                GetVariableNode->CreateNewGuid();
                GetVariableNode->NodePosX = PosX;
                GetVariableNode->NodePosY = PosY;

                // Configure the variable reference using provided node params
                if (NodeParamsShared.IsValid())
                {
                    FBlueprintReflection::ConfigureVariableNode(GetVariableNode, NodeParamsShared);
                }

                GetVariableNode->AllocateDefaultPins();
                TargetGraph->AddNode(GetVariableNode, true);
                NewNode = GetVariableNode;
                NodeId = GetVariableNode->NodeGuid.ToString();
                UE_LOG(LogVibeUEReflection, Warning, TEXT("Created GetVariable node at (%f, %f) with GUID: %s"), PosX, PosY, *LexToString(GetVariableNode->NodeGuid));
            }
        }
        else if (NodeIdentifier == TEXT("SetVariable"))
        {
            // Create a Set Variable node
            UK2Node_VariableSet* SetVariableNode = NewObject<UK2Node_VariableSet>(TargetGraph);
            if (SetVariableNode)
            {
                SetVariableNode->CreateNewGuid();
                SetVariableNode->NodePosX = PosX;
                SetVariableNode->NodePosY = PosY;

                // Configure the variable reference using provided node params
                if (NodeParamsShared.IsValid())
                {
                    FBlueprintReflection::ConfigureVariableSetNode(SetVariableNode, NodeParamsShared);
                }

                SetVariableNode->AllocateDefaultPins();
                TargetGraph->AddNode(SetVariableNode, true);
                NewNode = SetVariableNode;
                NodeId = SetVariableNode->NodeGuid.ToString();
                UE_LOG(LogVibeUEReflection, Warning, TEXT("Created SetVariable node at (%f, %f) with GUID: %s"), PosX, PosY, *LexToString(SetVariableNode->NodeGuid));
            }
        }
        else if (NodeIdentifier == TEXT("Self"))
        {
            // Create a Self reference node (K2Node_Self)
            UK2Node_Self* SelfNode = NewObject<UK2Node_Self>(TargetGraph);
            if (SelfNode)
            {
                SelfNode->CreateNewGuid();
                SelfNode->NodePosX = PosX;
                SelfNode->NodePosY = PosY;

                SelfNode->AllocateDefaultPins();
                TargetGraph->AddNode(SelfNode, true);
                NewNode = SelfNode;
                NodeId = SelfNode->NodeGuid.ToString();
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
            Result->SetStringField(TEXT("graph_name"), TargetGraph ? TargetGraph->GetName() : TEXT("unknown"));
            Result->SetStringField(TEXT("graph_scope"), bExplicitFunctionScope ? TEXT("function") : TEXT("event"));
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
