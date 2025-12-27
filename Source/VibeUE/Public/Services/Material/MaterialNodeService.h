// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"

class UMaterial;
class UMaterialExpression;
class UMaterialGraph;
class UMaterialGraphNode;
struct FExpressionInput;
struct FExpressionOutput;

/**
 * @struct FMaterialExpressionInfo
 * @brief Information about a material expression (node) in the material graph
 */
struct FMaterialExpressionInfo
{
    /** Unique identifier (object path or generated ID) */
    FString Id;
    
    /** Expression class name (e.g., "MaterialExpressionAdd") */
    FString ClassName;
    
    /** Display name for the expression */
    FString DisplayName;
    
    /** Expression category */
    FString Category;
    
    /** Position in the graph editor */
    int32 PosX = 0;
    int32 PosY = 0;
    
    /** Description/tooltip */
    FString Description;
    
    /** Whether this is a parameter expression */
    bool bIsParameter = false;
    
    /** Parameter name (if applicable) */
    FString ParameterName;
    
    /** Input pin information */
    TArray<FString> InputNames;
    
    /** Output pin information */
    TArray<FString> OutputNames;
};

/**
 * @struct FMaterialExpressionTypeInfo
 * @brief Information about an available material expression type for discovery
 */
struct FMaterialExpressionTypeInfo
{
    /** Expression class name */
    FString ClassName;
    
    /** Display name */
    FString DisplayName;
    
    /** Category for grouping */
    FString Category;
    
    /** Description/tooltip */
    FString Description;
    
    /** Keywords for search */
    TArray<FString> Keywords;
    
    /** Whether this creates a parameter */
    bool bIsParameter = false;
    
    /** Input pin names */
    TArray<FString> InputNames;
    
    /** Output pin names */
    TArray<FString> OutputNames;
};

/**
 * @struct FMaterialPinInfo
 * @brief Information about an input or output pin on a material expression
 */
struct FMaterialPinInfo
{
    /** Pin name */
    FString Name;
    
    /** Pin index */
    int32 Index = 0;
    
    /** Pin direction (input/output) */
    FString Direction;
    
    /** Value type (Float1, Float2, Float3, Float4, Texture2D, etc.) */
    FString ValueType;
    
    /** Whether the pin is connected */
    bool bIsConnected = false;
    
    /** Connected expression ID (if connected) */
    FString ConnectedExpressionId;
    
    /** Connected output index (if this is an input) */
    int32 ConnectedOutputIndex = -1;
    
    /** Default value as string (if applicable) */
    FString DefaultValue;
};

/**
 * @struct FMaterialConnectionInfo
 * @brief Information about a connection between material expressions
 */
struct FMaterialConnectionInfo
{
    /** Source expression ID */
    FString SourceExpressionId;
    
    /** Source output name or index */
    FString SourceOutput;
    
    /** Target expression ID */
    FString TargetExpressionId;
    
    /** Target input name or index */
    FString TargetInput;
};

/**
 * @class FMaterialNodeService
 * @brief Service for material graph node operations
 * 
 * Provides functionality for:
 * - Discovering available material expression types
 * - Creating and deleting material expressions
 * - Connecting and disconnecting expressions
 * - Getting and setting expression properties
 * - Promoting pins to parameters
 * - Managing the material graph
 */
class VIBEUE_API FMaterialNodeService : public FServiceBase
{
public:
    explicit FMaterialNodeService(TSharedPtr<FServiceContext> Context);
    virtual ~FMaterialNodeService() = default;

    // FServiceBase interface
    virtual FString GetServiceName() const override { return TEXT("MaterialNodeService"); }

    // ============================================================================
    // Expression Discovery
    // ============================================================================

    /**
     * Discover available material expression types
     * @param Category Optional category filter
     * @param SearchTerm Optional search term
     * @param MaxResults Maximum results to return
     * @return Array of expression type info
     */
    TResult<TArray<FMaterialExpressionTypeInfo>> DiscoverExpressionTypes(
        const FString& Category = TEXT(""),
        const FString& SearchTerm = TEXT(""),
        int32 MaxResults = 100
    );

    /**
     * Get expression categories
     * @return Array of category names
     */
    TResult<TArray<FString>> GetExpressionCategories();

    // ============================================================================
    // Expression Lifecycle
    // ============================================================================

    /**
     * Create a new material expression
     * @param Material Target material
     * @param ExpressionClassName Class name of expression to create
     * @param PosX X position in graph
     * @param PosY Y position in graph
     * @return Info about created expression
     */
    TResult<FMaterialExpressionInfo> CreateExpression(
        UMaterial* Material,
        const FString& ExpressionClassName,
        int32 PosX = 0,
        int32 PosY = 0
    );

    /**
     * Delete a material expression
     * @param Material Target material
     * @param ExpressionId Expression to delete
     * @return Success/failure
     */
    TResult<void> DeleteExpression(UMaterial* Material, const FString& ExpressionId);

    /**
     * Move an expression to a new position
     * @param Material Target material
     * @param ExpressionId Expression to move
     * @param PosX New X position
     * @param PosY New Y position
     * @return Success/failure
     */
    TResult<void> MoveExpression(
        UMaterial* Material,
        const FString& ExpressionId,
        int32 PosX,
        int32 PosY
    );

    // ============================================================================
    // Expression Information
    // ============================================================================

    /**
     * List all expressions in a material
     * @param Material Target material
     * @return Array of expression info
     */
    TResult<TArray<FMaterialExpressionInfo>> ListExpressions(UMaterial* Material);

    /**
     * Get detailed information about an expression
     * @param Material Target material
     * @param ExpressionId Expression ID
     * @return Expression info with full pin details
     */
    TResult<FMaterialExpressionInfo> GetExpressionDetails(
        UMaterial* Material,
        const FString& ExpressionId
    );

    /**
     * Get all pins for an expression
     * @param Material Target material
     * @param ExpressionId Expression ID
     * @return Array of pin info
     */
    TResult<TArray<FMaterialPinInfo>> GetExpressionPins(
        UMaterial* Material,
        const FString& ExpressionId
    );

    // ============================================================================
    // Connections
    // ============================================================================

    /**
     * Connect two expressions
     * @param Material Target material
     * @param SourceExpressionId Source expression
     * @param SourceOutputName Output name (or empty for first output)
     * @param TargetExpressionId Target expression
     * @param TargetInputName Input name
     * @return Success/failure
     */
    TResult<void> ConnectExpressions(
        UMaterial* Material,
        const FString& SourceExpressionId,
        const FString& SourceOutputName,
        const FString& TargetExpressionId,
        const FString& TargetInputName
    );

    /**
     * Disconnect an input
     * @param Material Target material
     * @param ExpressionId Expression with the input
     * @param InputName Input to disconnect
     * @return Success/failure
     */
    TResult<void> DisconnectInput(
        UMaterial* Material,
        const FString& ExpressionId,
        const FString& InputName
    );

    /**
     * Connect an expression output to a material property (e.g., BaseColor)
     * @param Material Target material
     * @param ExpressionId Source expression
     * @param OutputName Output name (or empty for first output)
     * @param MaterialProperty Property name (BaseColor, Metallic, Roughness, etc.)
     * @return Success/failure
     */
    TResult<void> ConnectToMaterialProperty(
        UMaterial* Material,
        const FString& ExpressionId,
        const FString& OutputName,
        const FString& MaterialProperty
    );

    /**
     * Disconnect a material property input
     * @param Material Target material
     * @param MaterialProperty Property to disconnect
     * @return Success/failure
     */
    TResult<void> DisconnectMaterialProperty(UMaterial* Material, const FString& MaterialProperty);

    /**
     * List all connections in the material
     * @param Material Target material
     * @return Array of connection info
     */
    TResult<TArray<FMaterialConnectionInfo>> ListConnections(UMaterial* Material);

    // ============================================================================
    // Expression Properties
    // ============================================================================

    /**
     * Get a property value from an expression
     * @param Material Target material
     * @param ExpressionId Expression ID
     * @param PropertyName Property name
     * @return Property value as string
     */
    TResult<FString> GetExpressionProperty(
        UMaterial* Material,
        const FString& ExpressionId,
        const FString& PropertyName
    );

    /**
     * Set a property value on an expression
     * @param Material Target material
     * @param ExpressionId Expression ID
     * @param PropertyName Property name
     * @param Value Value as string
     * @return Success/failure
     */
    TResult<void> SetExpressionProperty(
        UMaterial* Material,
        const FString& ExpressionId,
        const FString& PropertyName,
        const FString& Value
    );

    /**
     * List all editable properties on an expression
     * @param Material Target material
     * @param ExpressionId Expression ID
     * @return Array of property names and current values
     */
    TResult<TArray<TPair<FString, FString>>> ListExpressionProperties(
        UMaterial* Material,
        const FString& ExpressionId
    );

    // ============================================================================
    // Parameter Operations
    // ============================================================================

    /**
     * Promote a constant expression to a parameter
     * @param Material Target material
     * @param ExpressionId Expression to promote (must be Constant, Constant2/3/4Vector, or TextureSample)
     * @param ParameterName Name for the new parameter
     * @param GroupName Optional parameter group
     * @return Info about the new parameter expression
     */
    TResult<FMaterialExpressionInfo> PromoteToParameter(
        UMaterial* Material,
        const FString& ExpressionId,
        const FString& ParameterName,
        const FString& GroupName = TEXT("")
    );

    /**
     * Create a parameter expression directly
     * @param Material Target material
     * @param ParameterType Type (Scalar, Vector, Texture, StaticBool)
     * @param ParameterName Parameter name
     * @param GroupName Optional group
     * @param DefaultValue Default value as string
     * @param PosX X position
     * @param PosY Y position
     * @return Info about created parameter
     */
    TResult<FMaterialExpressionInfo> CreateParameter(
        UMaterial* Material,
        const FString& ParameterType,
        const FString& ParameterName,
        const FString& GroupName = TEXT(""),
        const FString& DefaultValue = TEXT(""),
        int32 PosX = 0,
        int32 PosY = 0
    );

    /**
     * Set parameter metadata (group, sort priority, slider range for scalars)
     * @param Material Target material
     * @param ExpressionId Parameter expression ID
     * @param GroupName New group name
     * @param SortPriority Sort priority within group
     * @return Success/failure
     */
    TResult<void> SetParameterMetadata(
        UMaterial* Material,
        const FString& ExpressionId,
        const FString& GroupName,
        int32 SortPriority = 0
    );

    // ============================================================================
    // Material Output Properties
    // ============================================================================

    /**
     * Get available material output properties
     * @param Material Target material
     * @return Array of property names that can be connected (BaseColor, Metallic, etc.)
     */
    TResult<TArray<FString>> GetMaterialOutputProperties(UMaterial* Material);

    /**
     * Get current connections to material outputs
     * @param Material Target material
     * @return Map of property name to connected expression info
     */
    TResult<TMap<FString, FString>> GetMaterialOutputConnections(UMaterial* Material);

private:
    // Helper methods
    UMaterialExpression* FindExpressionById(UMaterial* Material, const FString& ExpressionId);
    FString GetExpressionId(UMaterialExpression* Expression);
    FExpressionInput* FindInputByName(UMaterialExpression* Expression, const FString& InputName);
    int32 FindOutputIndexByName(UMaterialExpression* Expression, const FString& OutputName);
    TArray<FString> GetExpressionInputNames(UMaterialExpression* Expression);
    TArray<FString> GetExpressionOutputNames(UMaterialExpression* Expression);
    UClass* ResolveExpressionClass(const FString& ClassName);
    FMaterialExpressionInfo BuildExpressionInfo(UMaterialExpression* Expression);
    EMaterialProperty StringToMaterialProperty(const FString& PropertyName);
    void RefreshMaterialGraph(UMaterial* Material);
};
