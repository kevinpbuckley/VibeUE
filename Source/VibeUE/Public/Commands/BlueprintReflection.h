#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "K2Node.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"

// Forward declarations
class UK2Node_CallFunction;
class UK2Node_VariableGet;
class UK2Node_VariableSet;
class UK2Node_Event;

/**
 * Blueprint reflection helper for dynamic node discovery and manipulation
 * Uses Unreal's native reflection system to provide comprehensive Blueprint node access
 */
class VIBEUE_API FBlueprintReflection
{
public:
    FBlueprintReflection();

    // Core reflection methods
    static TSharedPtr<FJsonObject> GetAvailableBlueprintNodes(UBlueprint* Blueprint, const FString& Category = TEXT(""), const FString& Context = TEXT(""));
    static TSharedPtr<FJsonObject> CreateBlueprintNode(UBlueprint* Blueprint, const FString& NodeType, const TSharedPtr<FJsonObject>& NodeParams = nullptr);
    static TSharedPtr<FJsonObject> GetNodeProperties(UK2Node* Node);
    static TSharedPtr<FJsonObject> SetNodeProperty(UK2Node* Node, const FString& PropertyName, const FString& PropertyValue);
    static TSharedPtr<FJsonObject> GetNodeProperty(UK2Node* Node, const FString& PropertyName);
    static TSharedPtr<FJsonObject> SetPinDefaultValue(UEdGraphPin* Pin, const FString& PinName, const FString& Value);
    static TSharedPtr<FJsonObject> GetNodePinDetails(UK2Node* Node);

    // Node discovery categories (simplified)
    struct FNodeCategory
    {
        FString CategoryName;
        FString Description;
        TArray<FString> Keywords;
    };

    // Node metadata structure
    struct FNodeMetadata
    {
        FString NodeType;
        FString DisplayName;
        FString Description;
        FString Category;
        TArray<FString> Keywords;
        TMap<FString, FString> Properties;
        TArray<FString> InputPins;
        TArray<FString> OutputPins;
    };

public:
    // Public node discovery system methods
    static void GetBlueprintActionMenuItems(UBlueprint* Blueprint, TArray<TSharedPtr<FEdGraphSchemaAction>>& Actions);
    static TSharedPtr<FJsonObject> ProcessActionToJson(TSharedPtr<FEdGraphSchemaAction> Action);
    static UK2Node* CreateNodeFromIdentifier(UBlueprint* Blueprint, const FString& NodeIdentifier, const TSharedPtr<FJsonObject>& Config);
    static void ConfigureNodeFromParameters(UK2Node* Node, const TSharedPtr<FJsonObject>& NodeParams);
    
    // NEW: Optimized search methods
    static void GetFilteredBlueprintActions(UBlueprint* Blueprint, const FString& SearchTerm, const FString& Category, int32 MaxResults, TArray<TSharedPtr<FEdGraphSchemaAction>>& OutActions);
    static void GetCommonBlueprintActions(UBlueprint* Blueprint, const FString& Category, int32 MaxResults, TArray<TSharedPtr<FEdGraphSchemaAction>>& OutActions);
    
    // Node configuration helpers (moved to public for reflection system access)
    static void ConfigureFunctionNode(UK2Node_CallFunction* FunctionNode, const TSharedPtr<FJsonObject>& NodeParams);
    static void ConfigureVariableNode(UK2Node_VariableGet* VariableNode, const TSharedPtr<FJsonObject>& NodeParams);
    static void ConfigureVariableSetNode(UK2Node_VariableSet* VariableNode, const TSharedPtr<FJsonObject>& NodeParams);
    static void ConfigureEventNode(UK2Node_Event* EventNode, const TSharedPtr<FJsonObject>& NodeParams);

private:
    // Internal reflection helpers (simplified)
    static void PopulateNodeCategories();
    static TArray<FNodeCategory> GetNodeCategories();
    static TArray<FNodeMetadata> DiscoverNodesForBlueprint(UBlueprint* Blueprint, const FString& Category);
    static bool ValidateNodeCreation(UBlueprint* Blueprint, const FString& NodeType, const TSharedPtr<FJsonObject>& NodeParams);
    
    // Node discovery system using Unreal's action menu
    static UK2Node* CreateNodeFromBlueprintAction(UBlueprint* Blueprint, TSharedPtr<FEdGraphSchemaAction> Action);
    
    // Property reflection helpers (basic implementation)
    static TSharedPtr<FJsonObject> ReflectNodeProperties(UK2Node* Node);
    
    // Pin analysis helpers
    static TSharedPtr<FJsonObject> AnalyzeNodePins(UK2Node* Node);
    static FString GetPinTypeDescription(const FEdGraphPinType& PinType);
    
    // Search and filtering helpers
    static bool ContainsHighPriorityKeywords(const FString& DisplayName, const FString& Keywords, const TSet<FString>& HighPriorityKeywords);
    static int32 CalculateSearchRelevance(const FString& ActionName, const FString& Keywords, const FString& Tooltip, const FString& SearchTerm);
    
    // Cache for performance
    static TArray<FNodeCategory> CachedNodeCategories;
    static bool bCategoriesInitialized;
    
    // Simplified node type mappings
    static TMap<FString, UClass*> NodeTypeMap;
};

/**
 * Enhanced Blueprint node commands with reflection support
 * Extends existing functionality with dynamic node discovery
 */
class VIBEUE_API FBlueprintReflectionCommands
{
public:
    FBlueprintReflectionCommands();

    // Enhanced MCP command handlers
    TSharedPtr<FJsonObject> HandleGetAvailableBlueprintNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetBlueprintNodeProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetBlueprintNodeProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetEnhancedNodeDetails(const TSharedPtr<FJsonObject>& Params);

private:
    // Helper methods
    UBlueprint* FindBlueprint(const FString& BlueprintName);
    UK2Node* FindNodeInBlueprint(UBlueprint* Blueprint, const FString& NodeId);
    TSharedPtr<FJsonObject> CreateErrorResponse(const FString& Message);
    TSharedPtr<FJsonObject> CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data = nullptr);
};
