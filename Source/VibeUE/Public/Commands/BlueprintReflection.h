// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"  // For FEdGraphSchemaAction
#include "Engine/Blueprint.h"
#include "UObject/WeakObjectPtrTemplates.h"

// Forward declarations
class UK2Node;
class UK2Node_CallFunction;
class UK2Node_VariableGet;
class UK2Node_VariableSet;
class UK2Node_Event;
class UK2Node_DynamicCast;

/**
 * Blueprint reflection helper for dynamic node discovery and manipulation
 * Uses Unreal's native reflection system to provide comprehensive Blueprint node access
 */
class VIBEUE_API FBlueprintReflection
{
public:
    FBlueprintReflection();

    // Core reflection methods
    static TSharedPtr<FJsonObject> GetAvailableBlueprintNodes(UBlueprint* Blueprint, const FString& Category = TEXT(""), const FString& SearchTerm = TEXT(""), const FString& Context = TEXT(""));
    static TSharedPtr<FJsonObject> CreateBlueprintNode(UBlueprint* Blueprint, const FString& NodeType, const TSharedPtr<FJsonObject>& NodeParams = nullptr);
    static TSharedPtr<FJsonObject> CreateBlueprintNode(UBlueprint* Blueprint, const FString& NodeType, const TSharedPtr<FJsonObject>& NodeParams, UEdGraph* TargetGraph);
    static UClass* ResolveNodeClass(const FString& NodeType);
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
    static void GetCommonBlueprintActions(UBlueprint* Blueprint, const FString& Category, int32 MaxResults, TArray<TSharedPtr<FJsonValue>>& OutActions);
    
    // Node configuration helpers (moved to public for reflection system access)
    static void ConfigureFunctionNode(UK2Node_CallFunction* FunctionNode, const TSharedPtr<FJsonObject>& NodeParams);
    static void ConfigureVariableNode(UK2Node_VariableGet* VariableNode, const TSharedPtr<FJsonObject>& NodeParams);
    static void ConfigureVariableSetNode(UK2Node_VariableSet* VariableNode, const TSharedPtr<FJsonObject>& NodeParams);
    static void ConfigureEventNode(UK2Node_Event* EventNode, const TSharedPtr<FJsonObject>& NodeParams);
    static void ConfigureDynamicCastNode(UK2Node_DynamicCast* CastNode, const TSharedPtr<FJsonObject>& NodeParams);
    
    // NEW (Oct 6, 2025): Pin default configuration system
    static TSharedPtr<FJsonObject> ApplyPinDefaults(UEdGraphNode* Node, const TSharedPtr<FJsonObject>& PinDefaults);
    
    // NEW (Oct 6, 2025): Reroute node ergonomics system
    static class UK2Node_Knot* CreateRerouteNode(
        UEdGraph* Graph,
        const FVector2D& Position,
        const struct FEdGraphPinType* PinType = nullptr
    );
    
    static class UK2Node_Knot* InsertRerouteNode(
        UEdGraph* Graph,
        UEdGraphPin* SourcePin,
        UEdGraphPin* TargetPin,
        const FVector2D* CustomPosition = nullptr
    );
    
    static TArray<class UK2Node_Knot*> CreateReroutePath(
        UEdGraph* Graph,
        UEdGraphPin* SourcePin,
        UEdGraphPin* TargetPin,
        const TArray<FVector2D>& Waypoints
    );

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
    
    // ENHANCED: Cache node spawners for proper configuration
    static TMap<FString, TWeakObjectPtr<UBlueprintNodeSpawner>> CachedNodeSpawners;

public:
    // ═════════════════════════════════════════════════════════════════════
    // NEW DESIGN: Node Descriptor System
    // ═════════════════════════════════════════════════════════════════════
    
    /** Pin descriptor with complete metadata */
    struct FPinDescriptor
    {
        FString Name;
        FString Type;
        FString TypePath;           // Full UObject path for exact type matching
        FString Direction;          // "input" or "output"
        FString Category;
        bool bIsArray;
        bool bIsReference;
        bool bIsHidden;
        bool bIsAdvanced;
        FString DefaultValue;
        FString Tooltip;
        
        TSharedPtr<FJsonObject> ToJson() const;
    };
    
    /** Complete node spawner descriptor with all creation metadata */
    struct FNodeSpawnerDescriptor
    {
        // Unique identification
        FString SpawnerKey;                     // "ClassName::FunctionName" - unique identifier
        FString DisplayName;                    // Human-readable name
        FString NodeClassName;                  // K2Node class name
        FString NodeClassPath;                  // Full path to node class
        
        // Categorization
        FString Category;
        TArray<FString> Keywords;
        FString Description;
        FString Tooltip;
        
        // Node type classification
        FString NodeType;                       // "function_call", "variable_get", "cast", etc.
        
        // Function-specific metadata (if applicable)
        FString FunctionName;
        FString FunctionClassName;
        FString FunctionClassPath;              // EXACT path for matching
        bool bIsStatic;
        bool bIsConst;
        bool bIsPure;
        FString Module;
        
        // Variable-specific metadata (if applicable)
        FString VariableName;
        FString VariableType;
        FString VariableTypePath;
        FString OwnerClassName;                 // Class that owns this variable
        FString OwnerClassPath;                 // Full path to owner class
        bool bIsExternalMember = false;         // True if variable is from external class
        
        // Cast-specific metadata (if applicable)
        FString TargetClassName;
        FString TargetClassPath;
        
        // Special node flags
        bool bIsSynthetic = false;              // True for nodes without real spawners (e.g., reroute)
        
        // Search relevance (for sorting results)
        int32 RelevanceScore = 50;              // Higher = better match. 100=exact, 80=prefix, 60=contains
        
        // Pin information
        TArray<FPinDescriptor> Pins;
        int32 ExpectedPinCount;
        
        // Direct spawner reference (for creation)
        UBlueprintNodeSpawner* Spawner;
        
        // Serialization
        TSharedPtr<FJsonObject> ToJson() const;
        TSharedPtr<FJsonObject> ToJsonCompact() const;  // Minimal output: spawner_key, display_name, category only
        static FNodeSpawnerDescriptor FromJson(const TSharedPtr<FJsonObject>& Json);
    };
    
    // ═════════════════════════════════════════════════════════════════════
    // NEW API: Discovery with Complete Descriptors
    // ═════════════════════════════════════════════════════════════════════
    
    /**
     * Discover all available nodes with COMPLETE creation metadata
     * @param Blueprint - Target Blueprint for context
     * @param SearchTerm - Optional search filter
     * @param CategoryFilter - Optional category filter
     * @param ClassFilter - Optional class filter (e.g., "GameplayStatics")
     * @param MaxResults - Maximum results to return
     * @return Array of complete node descriptors
     */
    static TArray<FNodeSpawnerDescriptor> DiscoverNodesWithDescriptors(
        UBlueprint* Blueprint,
        const FString& SearchTerm = TEXT(""),
        const FString& CategoryFilter = TEXT(""),
        const FString& ClassFilter = TEXT(""),
        int32 MaxResults = 100
    );
    
    /**
     * Extract complete descriptor from a spawner
     */
    static FNodeSpawnerDescriptor ExtractDescriptorFromSpawner(
        UBlueprintNodeSpawner* Spawner,
        UBlueprint* Blueprint = nullptr
    );
    
    /**
     * Extract descriptor from schema action (used by context-sensitive discovery)
     * Schema actions come from FBlueprintActionMenuUtils::MakeContextMenu() which is
     * the same API Unreal's editor uses for "Context Sensitive" filtering
     */
    static FNodeSpawnerDescriptor ExtractDescriptorFromSchemaAction(
        TSharedPtr<FEdGraphSchemaAction> SchemaAction,
        UBlueprint* Blueprint = nullptr
    );
    
    /**
     * Non-context-sensitive node discovery (fallback)
     * Only used when Blueprint has no graphs to source context from
     * Uses GetAllActions() which returns ALL actions without filtering
     */
    static TArray<FNodeSpawnerDescriptor> DiscoverNodesWithDescriptorsNonContextSensitive(
        UBlueprint* Blueprint,
        const FString& SearchTerm = TEXT(""),
        const FString& CategoryFilter = TEXT(""),
        const FString& ClassFilter = TEXT(""),
        int32 MaxResults = 100
    );
    
    /**
     * Extract pin descriptors from a function
     */
    static void ExtractPinDescriptors(
        const UFunction* Function,
        TArray<FPinDescriptor>& OutPins
    );
    
    /**
     * Extract pin descriptors from an existing node
     */
    static void ExtractPinDescriptorsFromNode(
        UK2Node* Node,
        TArray<FPinDescriptor>& OutPins
    );
    
    // ═════════════════════════════════════════════════════════════════════
    // NEW API: Creation from Exact Descriptors
    // ═════════════════════════════════════════════════════════════════════
    
    /**
     * Create node from complete descriptor (NO SEARCHING)
     * @param Graph - Target graph
     * @param Descriptor - Complete node descriptor
     * @param Position - Node position
     * @return Created node or nullptr
     */
    static UK2Node* CreateNodeFromDescriptor(
        UEdGraph* Graph,
        const FNodeSpawnerDescriptor& Descriptor,
        FVector2D Position
    );
    
    /**
     * Create node from spawner key (exact lookup)
     * @param Graph - Target graph
     * @param SpawnerKey - Unique spawner identifier (e.g., "GameplayStatics::GetPlayerController")
     * @param Position - Node position
     * @return Created node or nullptr
     */
    static UK2Node* CreateNodeFromSpawnerKey(
        UEdGraph* Graph,
        const FString& SpawnerKey,
        FVector2D Position
    );
    
    /**
     * Get spawner by exact key
     */
    static UBlueprintNodeSpawner* GetSpawnerByKey(const FString& SpawnerKey);
    
    /**
     * Cache a spawner with its key
     */
    static void CacheSpawner(const FString& SpawnerKey, UBlueprintNodeSpawner* Spawner);
};

/**
 * Enhanced Blueprint node commands with reflection support
 * Extends existing functionality with dynamic node discovery
 */
class VIBEUE_API FBlueprintReflectionCommands
{
public:
    FBlueprintReflectionCommands();

    // Service initialization
    void SetDiscoveryService(TSharedPtr<class FBlueprintDiscoveryService> InDiscoveryService) { DiscoveryService = InDiscoveryService; }
    void SetNodeService(TSharedPtr<class FBlueprintNodeService> InNodeService) { NodeService = InNodeService; }

    // Enhanced MCP command handlers
    TSharedPtr<FJsonObject> HandleGetAvailableBlueprintNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDiscoverNodesWithDescriptors(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetBlueprintNodeProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetEnhancedNodeDetails(const TSharedPtr<FJsonObject>& Params);

private:
    // Services
    TSharedPtr<class FBlueprintDiscoveryService> DiscoveryService;
    TSharedPtr<class FBlueprintNodeService> NodeService;
    
    // Helper methods
    UBlueprint* FindBlueprint(const FString& BlueprintName);
    UK2Node* FindNodeInBlueprint(UBlueprint* Blueprint, const FString& NodeId);
    TSharedPtr<FJsonObject> CreateErrorResponse(const FString& Message);
    TSharedPtr<FJsonObject> CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data = nullptr);
};
