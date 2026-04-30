/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#pragma once

#include "CoreMinimal.h"

class FConnectedGraph;
class FFormatterNode;
class FFormatterGraph;

// VibeUE: Vendored from howaajin/graphformatter. UENUM/UMETA macros stripped
// because we do not expose these enums to Blueprints/UHT in this plugin.
enum class EGraphFormatterPositioningAlgorithm : uint8
{
    EEvenlyInLayer,
    EFastAndSimpleMethodTop,
    EFastAndSimpleMethodMedian,
    ELayerSweep,
};

enum class EFormatterPinDirection : uint8
{
    In,
    Out,
    InOut,
};

class FFormatterPin
{
public:
    // Guid for this Pin, used to mapping different instances to one instance
    // For example, in a duplicated graph, needs to know the original pin
    FGuid Guid;

    // User-defined pointer used as a mapping key from a child graph pin to the original graph pin
    void* OriginalPin{nullptr};
    
    EFormatterPinDirection Direction{EFormatterPinDirection::In};

    // Owner of the pin
    FFormatterNode* OwningNode{nullptr};

    // Position relate to Owner
    FVector2D NodeOffset;
    
    // When the "Owner" is assigned to a certain layer, this pin's index within that layer
    // Can be used to detect intersections
    int32 IndexInLayer{-1};
};

// An edge is defined here as belonging to a specific node,
// meaning the connection between two nodes, with each edge storing information about the connection between the two nodes
class FFormatterEdge
{
public:
    // The owner's pin the edge linked to, it can be either an "In" or "Out" direction
    FFormatterPin* From;

    // Indicates which pin this edge is connected to, it can be either an "In" or "Out" direction
    FFormatterPin* To;
    float Weight = 1;
    bool IsCrossing(const FFormatterEdge* Edge) const;
    bool IsInnerSegment() const;
};

class FFormatterNode
{
public:
    static FFormatterNode* CreateDummy();
    static TArray<FFormatterEdge*> GetEdgeBetweenTwoLayer(const TArray<FFormatterNode*>& LowerLayer, const TArray<FFormatterNode*>& UpperLayer, const FFormatterNode* ExcludedNode = nullptr);
    static TArray<FFormatterNode*> GetSuccessorsForNodes(TSet<FFormatterNode*> Nodes);
    static void CalculatePinsIndexInLayer(const TArray<FFormatterNode*>& Layer);
    static void CalculatePinsIndex(const TArray<TArray<FFormatterNode*>>& Order);
    static int32 CalculateCrossing(const TArray<TArray<FFormatterNode*>>& Order);

    // Guid for this node, used to mapping different instances to one instance
    // For example, in a duplicated graph, needs to know the original pin
    FGuid Guid;

    // Custom pointer for user-defined nodes tracking
    void* OriginalNode = nullptr;

    // Indicating whether this node is a subgraph
    FFormatterGraph* SubGraph = nullptr;

    // Representing the size of the node or the bounding box size of a subgraph
    FVector2D Size;

    // Edges with an "In" direction "From" pin
    TArray<FFormatterEdge*> InEdges;
    
    // Edges with an "Out" direction "From" pin
    TArray<FFormatterEdge*> OutEdges;

    // All pins with an "In" direction
    TArray<FFormatterPin*> InPins;
    
    // All pins with an "Out" direction
    TArray<FFormatterPin*> OutPins;

    // Path depth used in the longest path layering algorithm 
    int32 PathDepth;
    
    // Positioning priority used in priority positioning algorithm 
    int32 PositioningPriority;
    
    FFormatterNode(const FFormatterNode& Other);
    FFormatterNode();

    ~FFormatterNode();
    void Connect(FFormatterPin* SourcePin, FFormatterPin* TargetPin, float Weight = 1);
    void Disconnect(FFormatterPin* SourcePin, FFormatterPin* TargetPin);
    void AddPin(FFormatterPin* Pin);
    TArray<FFormatterNode*> GetSuccessors() const;
    TArray<FFormatterNode*> GetPredecessors() const;
    bool IsSource() const;
    bool IsSink() const;
    bool AnySuccessorPathDepthEqu0() const;
    bool IsCrossingInnerSegment(const TArray<FFormatterNode*>& LowerLayer, const TArray<FFormatterNode*>& UpperLayer) const;
    FFormatterNode* GetMedianUpper() const;
    FFormatterNode* GetMedianLower() const;
    TArray<FFormatterNode*> GetUppers() const;
    TArray<FFormatterNode*> GetLowers() const;
    int32 GetInputPinCount() const;
    int32 GetInputPinIndex(FFormatterPin* InputPin) const;
    int32 GetOutputPinCount() const;
    int32 GetOutputPinIndex(FFormatterPin* OutputPin) const;
    TArray<FFormatterEdge*> GetEdgeLinkedToLayer(const TArray<FFormatterNode*>& Layer, EEdGraphPinDirection Direction) const;
    float CalcBarycenter(const TArray<FFormatterNode*>& Layer, EEdGraphPinDirection Direction) const;
    float GetLinkedPositionToNode(const FFormatterNode* Node, EEdGraphPinDirection Direction, bool IsHorizontalDirection = true);
    float GetMaxWeight(EEdGraphPinDirection Direction);
    float GetMaxWeightToNode(const FFormatterNode* Node, EEdGraphPinDirection Direction);
    int32 CalcPriority(EEdGraphPinDirection Direction) const;
    void InitPosition(FVector2D InPosition);
    void SetPosition(FVector2D InPosition);
    FVector2D GetPosition() const;
    void SetSubGraph(FFormatterGraph* InSubGraph);
    void UpdatePinsOffset(FVector2D Border);
    friend class FConnectedGraph;

private:
    float OrderValue{0.0f};
    FVector2D Position;
};

class FFormatterGraph
{
public:
    static TArray<FBox2D> CalculateLayersBound(TArray<TArray<FFormatterNode*>>& InLayeredNodes, bool IsHorizontalDirection = true, bool IsParameterGroup = false);
    inline static int32 HorizontalSpacing = 100;
    inline static int32 VerticalSpacing = 80;
    inline static FVector2D SpacingFactorOfGroup = FVector2D(0.314);
    inline static int32 MaxLayerNodes = 5;
    inline static int32 MaxOrderingIterations = 10;
    inline static EGraphFormatterPositioningAlgorithm PositioningAlgorithm = EGraphFormatterPositioningAlgorithm::EFastAndSimpleMethodMedian;

    FFormatterGraph(const FFormatterGraph& Other);
    virtual ~FFormatterGraph();
    void DetachAndDestroy();
    virtual FFormatterGraph* Clone();

    explicit FFormatterGraph(bool InIsVerticalLayout = false, bool InIsParameterGroup = false);

    void AddNode(FFormatterNode* InNode);

    virtual TMap<void*, FVector2D> GetPinsOffset() { return {}; }

    virtual TArray<FFormatterPin*> GetInputPins() const { return {}; }

    virtual TArray<FFormatterPin*> GetOutputPins() const { return {}; }

    virtual TSet<void*> GetOriginalNodes() const { return {}; }

    virtual void Format() { }
    
    virtual void OffsetBy(const FVector2D& InOffset) { }
    virtual void SetPosition(const FVector2D& Position);

    virtual TMap<void*, FBox2D> GetBoundMap() { return {}; }

    FBox2D GetTotalBound() const { return TotalBound; }

    void SetBorder(float Left, float Top, float Right, float Bottom);
    FBox2D GetBorder() const;
    void SetIsParameterGroup(bool InIsParameterGroup);
    bool GetIsParameterGroup() const;
    bool GetIsVerticalLayout() const;
    void SetIsVerticalLayout(bool InIsVerticalLayout);
    bool (*NodeComparer)(const FFormatterNode& A, const FFormatterNode& B) = nullptr;
    TMap<void*, FFormatterPin*> OriginalPinsMap;
    TArray<FFormatterNode*> Nodes;

protected:
    TMap<FGuid, FFormatterNode*> NodesMap;
    TMap<FGuid, FFormatterGraph*> SubGraphs;
    TMap<FGuid, FFormatterPin*> PinsMap;
    
    FBox2D TotalBound = FBox2D(ForceInit);
    FBox2D Border = {FVector2D(ForceInitToZero), FVector2D(ForceInitToZero)};
    bool IsParameterGroup = false;
    bool IsVerticalLayout = false;
private:
    bool IsNodeDetached = false;
};

class FDisconnectedGraph : public FFormatterGraph
{
    TArray<FFormatterGraph*> ConnectedGraphs;

public:
    void AddGraph(FFormatterGraph* Graph);
    virtual ~FDisconnectedGraph() override;
    virtual TMap<void*, FVector2D> GetPinsOffset() override;
    virtual TArray<FFormatterPin*> GetInputPins() const override;
    virtual TArray<FFormatterPin*> GetOutputPins() const override;
    virtual TSet<void*> GetOriginalNodes() const override;
    virtual void Format() override;
    virtual void OffsetBy(const FVector2D& InOffset) override;
    virtual TMap<void*, FBox2D> GetBoundMap() override;
};

class FConnectedGraph : public FFormatterGraph
{
public:
    FConnectedGraph(bool InIsVerticalLayout = false, bool InIsParameterGroup = false);
    FConnectedGraph(const FConnectedGraph& Other);
    virtual FFormatterGraph* Clone() override;
    void RemoveNode(FFormatterNode* NodeToRemove);
    virtual void Format() override;
    virtual TMap<void*, FBox2D> GetBoundMap() override;
    virtual void OffsetBy(const FVector2D& InOffset) override;
    virtual TMap<void*, FVector2D> GetPinsOffset() override;
    virtual TArray<FFormatterPin*> GetInputPins() const override;
    virtual TArray<FFormatterPin*> GetOutputPins() const override;
    virtual TSet<void*> GetOriginalNodes() const override;

private:
    
    TArray<FFormatterNode*> GetNodesGreaterThan(int32 i, TSet<FFormatterNode*>& Excluded);

    void DoLayering();
    void RemoveCycle();
    void AddDummyNodes();
    void SortInLayer(TArray<TArray<FFormatterNode*>>& Order, EEdGraphPinDirection Direction);
    void DoOrderingSweep();
    void DoPositioning();
    FFormatterNode* FindSourceNode() const;
    FFormatterNode* FindSinkNode() const;
    FFormatterNode* FindMaxInOutWeightDiffNode() const;
    TArray<FFormatterNode*> GetLeavesWithPathDepth0() const;
    int32 AssignPathDepthForNodes() const;

    TArray<TArray<FFormatterNode*>> LayeredList;
};

