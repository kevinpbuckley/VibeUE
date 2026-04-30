/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "FormatterGraph.h"

#include "EvenlyPlaceStrategy.h"
#include "FastAndSimplePositioningStrategy.h"
#include "PriorityPositioningStrategy.h"

bool FFormatterEdge::IsCrossing(const FFormatterEdge* Edge) const
{
    return (From->IndexInLayer < Edge->From->IndexInLayer && To->IndexInLayer > Edge->To->IndexInLayer) || (From->IndexInLayer > Edge->From->IndexInLayer && To->IndexInLayer < Edge->To->IndexInLayer);
}

bool FFormatterEdge::IsInnerSegment() const
{
    return From->OwningNode->OriginalNode == nullptr && To->OwningNode->OriginalNode == nullptr;
}

FFormatterNode::FFormatterNode(const FFormatterNode& Other)
    : Guid(Other.Guid)
    , OriginalNode(Other.OriginalNode)
    , Size(Other.Size)
    , PathDepth(Other.PathDepth)
    , Position(Other.Position)
{
    if (Other.SubGraph != nullptr)
    {
        SubGraph = Other.SubGraph->Clone();
    }
    else
    {
        SubGraph = nullptr;
    }
    for (auto Pin : Other.InPins)
    {
        auto NewPin = new FFormatterPin;
        NewPin->Guid = Pin->Guid;
        NewPin->OriginalPin = Pin->OriginalPin;
        NewPin->Direction = Pin->Direction;
        NewPin->OwningNode = this;
        NewPin->NodeOffset = Pin->NodeOffset;
        InPins.Add(NewPin);
    }
    for (auto Pin : Other.OutPins)
    {
        auto NewPin = new FFormatterPin;
        NewPin->Guid = Pin->Guid;
        NewPin->OriginalPin = Pin->OriginalPin;
        NewPin->Direction = Pin->Direction;
        NewPin->OwningNode = this;
        NewPin->NodeOffset = Pin->NodeOffset;
        OutPins.Add(NewPin);
    }
}

FFormatterNode::FFormatterNode()
    : Guid(FGuid::NewGuid())
    , Size(FVector2D(1, 1))
    , PathDepth(0)
    , PositioningPriority(INT_MAX)
    , Position(FVector2D::ZeroVector)
{
}

FFormatterNode* FFormatterNode::CreateDummy()
{
    auto DummyNode = new FFormatterNode();
    auto InPin = new FFormatterPin;
    InPin->Guid = FGuid::NewGuid();
    InPin->OriginalPin = nullptr;
    InPin->Direction = EFormatterPinDirection::In;
    InPin->OwningNode = DummyNode;
    InPin->NodeOffset = FVector2D::ZeroVector;

    auto OutPin = new FFormatterPin;
    OutPin->Guid = FGuid::NewGuid();
    OutPin->OriginalPin = nullptr;
    OutPin->Direction = EFormatterPinDirection::Out;
    OutPin->OwningNode = DummyNode;
    OutPin->NodeOffset = FVector2D::ZeroVector;

    DummyNode->InPins.Add(InPin);
    DummyNode->OutPins.Add(OutPin);
    return DummyNode;
}

void FFormatterNode::Connect(FFormatterPin* SourcePin, FFormatterPin* TargetPin, float Weight)
{
    const auto Edge = new FFormatterEdge;
    Edge->From = SourcePin;
    Edge->To = TargetPin;
    Edge->Weight = Weight;
    if (SourcePin->Direction == EFormatterPinDirection::Out)
    {
        OutEdges.Add(Edge);
    }
    else
    {
        InEdges.Add(Edge);
    }
}

void FFormatterNode::Disconnect(FFormatterPin* SourcePin, FFormatterPin* TargetPin)
{
    const auto Predicate = [SourcePin, TargetPin](const FFormatterEdge* Edge)
    {
        return Edge->From == SourcePin && Edge->To == TargetPin;
    };
    if (SourcePin->Direction == EFormatterPinDirection::Out)
    {
        const auto Index = OutEdges.IndexOfByPredicate(Predicate);
        if (Index != INDEX_NONE)
        {
            delete OutEdges[Index];
            OutEdges.RemoveAt(Index);
        }
    }
    else
    {
        const auto Index = InEdges.IndexOfByPredicate(Predicate);
        if (Index != INDEX_NONE)
        {
            delete InEdges[Index];
            InEdges.RemoveAt(Index);
        }
    }
}

void FFormatterNode::AddPin(FFormatterPin* Pin)
{
    if (Pin->Direction == EFormatterPinDirection::In)
    {
        InPins.Add(Pin);
    }
    else
    {
        OutPins.Add(Pin);
    }
}

TArray<FFormatterNode*> FFormatterNode::GetSuccessors() const
{
    TArray<FFormatterNode*> Result;
    for (auto Edge : OutEdges)
    {
        Result.Add(Edge->To->OwningNode);
    }
    return Result;
}

TArray<FFormatterNode*> FFormatterNode::GetPredecessors() const
{
    TArray<FFormatterNode*> Result;
    for (auto Edge : InEdges)
    {
        Result.Add(Edge->To->OwningNode);
    }
    return Result;
}

bool FFormatterNode::IsSource() const
{
    return InEdges.Num() == 0;
}

bool FFormatterNode::IsSink() const
{
    return OutEdges.Num() == 0;
}

bool FFormatterNode::AnySuccessorPathDepthEqu0() const
{
    for (auto OutEdge : OutEdges)
    {
        if (OutEdge->To->OwningNode->PathDepth == 0)
        {
            return true;
        }
    }
    return false;
}

float FFormatterNode::GetLinkedPositionToNode(const FFormatterNode* Node, EEdGraphPinDirection Direction, bool IsHorizontalDirection)
{
    auto& Edges = Direction == EGPD_Input ? InEdges : OutEdges;
    float MedianPosition = 0.0f;
    int32 Count = 0;
    for (auto Edge : Edges)
    {
        if (Edge->To->OwningNode == Node)
        {
            if (IsHorizontalDirection)
            {
                MedianPosition += Edge->From->NodeOffset.Y;
            }
            else
            {
                MedianPosition += Edge->From->NodeOffset.X;
            }
            ++Count;
        }
    }
    if (Count == 0)
    {
        return 0.0f;
    }
    return MedianPosition / Count;
}

float FFormatterNode::GetMaxWeight(EEdGraphPinDirection Direction)
{
    auto& Edges = Direction == EGPD_Input ? InEdges : OutEdges;
    float MaxWeight = 0.0f;
    for (auto Edge : Edges)
    {
        if (MaxWeight < Edge->Weight)
        {
            MaxWeight = Edge->Weight;
        }
    }
    return MaxWeight;
}

float FFormatterNode::GetMaxWeightToNode(const FFormatterNode* Node, EEdGraphPinDirection Direction)
{
    auto& Edges = Direction == EGPD_Input ? InEdges : OutEdges;
    float MaxWeight = 0.0f;
    for (auto Edge : Edges)
    {
        if (Edge->To->OwningNode == Node && MaxWeight < Edge->Weight)
        {
            MaxWeight = Edge->Weight;
        }
    }
    return MaxWeight;
}

bool FFormatterNode::IsCrossingInnerSegment(const TArray<FFormatterNode*>& LowerLayer, const TArray<FFormatterNode*>& UpperLayer) const
{
    auto EdgesLinkedToUpper = GetEdgeLinkedToLayer(UpperLayer, EGPD_Input);
    auto EdgesBetweenTwoLayers = GetEdgeBetweenTwoLayer(LowerLayer, UpperLayer, this);
    for (auto EdgeLinkedToUpper : EdgesLinkedToUpper)
    {
        for (auto EdgeBetweenTwoLayers : EdgesBetweenTwoLayers)
        {
            if (EdgeBetweenTwoLayers->IsInnerSegment() && EdgeLinkedToUpper->IsCrossing(EdgeBetweenTwoLayers))
            {
                return true;
            }
        }
    }
    return false;
}

FFormatterNode* FFormatterNode::GetMedianUpper() const
{
    TArray<FFormatterNode*> UpperNodes;
    for (auto InEdge : InEdges)
    {
        if (!UpperNodes.Contains(InEdge->To->OwningNode))
        {
            UpperNodes.Add(InEdge->To->OwningNode);
        }
    }
    if (UpperNodes.Num() > 0)
    {
        const int32 m = UpperNodes.Num() / 2;
        return UpperNodes[m];
    }
    return nullptr;
}

FFormatterNode* FFormatterNode::GetMedianLower() const
{
    TArray<FFormatterNode*> LowerNodes;
    for (auto OutEdge : OutEdges)
    {
        if (!LowerNodes.Contains(OutEdge->To->OwningNode))
        {
            LowerNodes.Add(OutEdge->To->OwningNode);
        }
    }
    if (LowerNodes.Num() > 0)
    {
        const int32 m = LowerNodes.Num() / 2;
        return LowerNodes[m];
    }
    return nullptr;
}

TArray<FFormatterNode*> FFormatterNode::GetUppers() const
{
    TSet<FFormatterNode*> UpperNodes;
    for (auto InEdge : InEdges)
    {
        UpperNodes.Add(InEdge->To->OwningNode);
    }
    return UpperNodes.Array();
}

TArray<FFormatterNode*> FFormatterNode::GetLowers() const
{
    TSet<FFormatterNode*> LowerNodes;
    for (auto OutEdge : OutEdges)
    {
        LowerNodes.Add(OutEdge->To->OwningNode);
    }
    return LowerNodes.Array();
}

int32 FFormatterNode::GetInputPinCount() const
{
    return InPins.Num();
}

int32 FFormatterNode::GetInputPinIndex(FFormatterPin* InputPin) const
{
    return InPins.Find(InputPin);
}

int32 FFormatterNode::GetOutputPinCount() const
{
    return OutPins.Num();
}

int32 FFormatterNode::GetOutputPinIndex(FFormatterPin* OutputPin) const
{
    return OutPins.Find(OutputPin);
}

TArray<FFormatterEdge*> FFormatterNode::GetEdgeLinkedToLayer(const TArray<FFormatterNode*>& Layer, EEdGraphPinDirection Direction) const
{
    TArray<FFormatterEdge*> Result;
    const TArray<FFormatterEdge*>& Edges = Direction == EGPD_Output ? OutEdges : InEdges;
    for (auto Edge : Edges)
    {
        for (auto NextLayerNode : Layer)
        {
            if (Edge->To->OwningNode == NextLayerNode)
            {
                Result.Add(Edge);
            }
        }
    }
    return Result;
}

float FFormatterNode::CalcBarycenter(const TArray<FFormatterNode*>& Layer, EEdGraphPinDirection Direction) const
{
    auto Edges = GetEdgeLinkedToLayer(Layer, Direction);
    if (Edges.Num() == 0)
    {
        return 0.0f;
    }
    float Sum = 0.0f;
    for (auto Edge : Edges)
    {
        Sum += Edge->To->IndexInLayer;
    }
    return Sum / Edges.Num();
}

int32 FFormatterNode::CalcPriority(EEdGraphPinDirection Direction) const
{
    if (OriginalNode == nullptr)
    {
        return 0;
    }
    return Direction == EGPD_Output ? OutEdges.Num() : InEdges.Num();
}

FFormatterNode::~FFormatterNode()
{
    for (auto Edge : InEdges)
    {
        delete Edge;
    }
    for (auto Edge : OutEdges)
    {
        delete Edge;
    }
    for (auto Pin : InPins)
    {
        delete Pin;
    }
    for (auto Pin : OutPins)
    {
        delete Pin;
    }
    delete SubGraph;
}

void FFormatterNode::InitPosition(FVector2D InPosition)
{
    Position = InPosition;
}

void FFormatterNode::SetPosition(FVector2D InPosition)
{
    const FVector2D Offset = InPosition - Position;
    Position = InPosition;
    if (SubGraph != nullptr)
    {
        SubGraph->OffsetBy(Offset);
    }
}

FVector2D FFormatterNode::GetPosition() const
{
    return Position;
}

void FFormatterNode::SetSubGraph(FFormatterGraph* InSubGraph)
{
    SubGraph = InSubGraph;
    auto SubGraphInPins = SubGraph->GetInputPins();
    auto SubGraphOutPins = SubGraph->GetOutputPins();
    for (auto Pin : SubGraphInPins)
    {
        auto NewPin = new FFormatterPin();
        NewPin->Guid = Pin->Guid;
        NewPin->OwningNode = this;
        NewPin->Direction = Pin->Direction;
        NewPin->NodeOffset = Pin->NodeOffset;
        NewPin->OriginalPin = Pin->OriginalPin;
        InPins.Add(NewPin);
    }
    for (auto Pin : SubGraphOutPins)
    {
        auto NewPin = new FFormatterPin();
        NewPin->Guid = Pin->Guid;
        NewPin->OwningNode = this;
        NewPin->Direction = Pin->Direction;
        NewPin->NodeOffset = Pin->NodeOffset;
        NewPin->OriginalPin = Pin->OriginalPin;
        OutPins.Add(NewPin);
    }
}

void FFormatterNode::UpdatePinsOffset(FVector2D Border)
{
    if (SubGraph != nullptr)
    {
        auto PinsOffset = SubGraph->GetPinsOffset();
        for (auto Pin : InPins)
        {
            if (PinsOffset.Contains(Pin->OriginalPin))
            {
                Pin->NodeOffset = PinsOffset[Pin->OriginalPin] + Border;
            }
        }
        for (auto Pin : OutPins)
        {
            if (PinsOffset.Contains(Pin->OriginalPin))
            {
                Pin->NodeOffset = PinsOffset[Pin->OriginalPin] + Border;
            }
        }
        InPins.Sort([](const FFormatterPin& A, const FFormatterPin& B)
                    { return A.NodeOffset.Y < B.NodeOffset.Y; });
        OutPins.Sort([](const FFormatterPin& A, const FFormatterPin& B)
                     { return A.NodeOffset.Y < B.NodeOffset.Y; });
    }
}

TArray<FFormatterNode*> FFormatterNode::GetSuccessorsForNodes(TSet<FFormatterNode*> Nodes)
{
    TArray<FFormatterNode*> Result;
    for (auto Node : Nodes)
    {
        for (auto OutEdge : Node->OutEdges)
        {
            if (!Nodes.Contains(OutEdge->To->OwningNode))
            {
                Result.Add(OutEdge->To->OwningNode);
            }
        }
    }
    return Result;
}

void FFormatterNode::CalculatePinsIndexInLayer(const TArray<FFormatterNode*>& Layer)
{
    int32 InPinStartIndex = 0, OutPinStartIndex = 0;
    for (int32 j = 0; j < Layer.Num(); j++)
    {
        for (auto InPin : Layer[j]->InPins)
        {
            InPin->IndexInLayer = InPinStartIndex + Layer[j]->GetInputPinIndex(InPin);
        }
        for (auto OutPin : Layer[j]->OutPins)
        {
            OutPin->IndexInLayer = OutPinStartIndex + Layer[j]->GetOutputPinIndex(OutPin);
        }
        OutPinStartIndex += Layer[j]->GetOutputPinCount();
        InPinStartIndex += Layer[j]->GetInputPinCount();
    }
}

void FFormatterNode::CalculatePinsIndex(const TArray<TArray<FFormatterNode*>>& Order)
{
    for (int32 i = 0; i < Order.Num(); i++)
    {
        auto& Layer = Order[i];
        CalculatePinsIndexInLayer(Layer);
    }
}

TArray<FFormatterNode*> FConnectedGraph::GetNodesGreaterThan(int32 i, TSet<FFormatterNode*>& Excluded)
{
    TArray<FFormatterNode*> Result;
    for (auto Node : Nodes)
    {
        if (!Excluded.Contains(Node) && Node->PathDepth >= i)
        {
            Result.Add(Node);
        }
    }
    return Result;
}

void FConnectedGraph::RemoveNode(FFormatterNode* NodeToRemove)
{
    TArray<FFormatterEdge*> Edges = NodeToRemove->InEdges;
    for (auto Edge : Edges)
    {
        Edge->To->OwningNode->Disconnect(Edge->To, Edge->From);
    }
    Edges = NodeToRemove->OutEdges;
    for (auto Edge : Edges)
    {
        Edge->To->OwningNode->Disconnect(Edge->To, Edge->From);
    }
    Nodes.Remove(NodeToRemove);
    NodesMap.Remove(NodeToRemove->Guid);
    SubGraphs.Remove(NodeToRemove->Guid);
    for (auto Pin : NodeToRemove->InPins)
    {
        OriginalPinsMap.Remove(Pin->OriginalPin);
        PinsMap.Remove(Pin->Guid);
    }
    for (auto Pin : NodeToRemove->OutPins)
    {
        OriginalPinsMap.Remove(Pin->OriginalPin);
        PinsMap.Remove(Pin->Guid);
    }
    delete NodeToRemove;
}

void FConnectedGraph::RemoveCycle()
{
    auto ClonedGraph = new FConnectedGraph(*this);
    while (auto SourceNode = ClonedGraph->FindSourceNode())
    {
        ClonedGraph->RemoveNode(SourceNode);
    }
    while (auto SinkNode = ClonedGraph->FindSinkNode())
    {
        ClonedGraph->RemoveNode(SinkNode);
    }
    while (auto MedianNode = ClonedGraph->FindMaxInOutWeightDiffNode())
    {
        for (auto Edge : MedianNode->InEdges)
        {
            FFormatterPin* From = PinsMap[Edge->From->Guid];
            FFormatterPin* To = PinsMap[Edge->To->Guid];
            NodesMap[MedianNode->Guid]->Disconnect(From, To);
            To->OwningNode->Disconnect(To, From);
        }
        ClonedGraph->RemoveNode(MedianNode);
    }
    delete ClonedGraph;
}

FFormatterNode* FConnectedGraph::FindSourceNode() const
{
    for (auto Node : Nodes)
    {
        if (Node->IsSource())
        {
            return Node;
        }
    }
    return nullptr;
}

FFormatterNode* FConnectedGraph::FindSinkNode() const
{
    for (auto Node : Nodes)
    {
        if (Node->IsSink())
        {
            return Node;
        }
    }
    return nullptr;
}

FFormatterNode* FConnectedGraph::FindMaxInOutWeightDiffNode() const
{
    auto EdgesWeightSum = [](const TArray<FFormatterEdge*>& Edges)
    {
        float Sum = 0;
        for (const auto Edge : Edges)
        {
            Sum += Edge->Weight;
        }
        return Sum;
    };
    FFormatterNode* Result = nullptr;
    int32 MaxWeight = -INT32_MAX;
    for (const auto Node : Nodes)
    {
        const int32 Diff = EdgesWeightSum(Node->OutEdges) - EdgesWeightSum(Node->InEdges);
        if (Diff > MaxWeight)
        {
            MaxWeight = Diff;
            Result = Node;
        }
    }
    return Result;
}

TArray<FBox2D> FFormatterGraph::CalculateLayersBound(TArray<TArray<FFormatterNode*>>& InLayeredNodes, bool IsHorizontalDirection, bool IsParameterGroup)
{
    TArray<FBox2D> LayersBound;
    FBox2D TotalBound(ForceInit);
    FVector2D Spacing;
    if (IsHorizontalDirection)
    {
        Spacing = FVector2D(HorizontalSpacing, 0);
    }
    else
    {
        Spacing = FVector2D(0, VerticalSpacing);
    }
    if (IsParameterGroup)
    {
        Spacing *= SpacingFactorOfGroup.X;
    }
    for (int32 i = 0; i < InLayeredNodes.Num(); i++)
    {
        const auto& Layer = InLayeredNodes[i];
        FBox2D Bound(ForceInit);
        FVector2D Position;
        if (TotalBound.bIsValid)
        {
            Position = TotalBound.Max + Spacing;
        }
        else
        {
            Position = FVector2D(0, 0);
        }
        for (auto Node : Layer)
        {
            if (Bound.bIsValid)
            {
                Bound += FBox2D(Position, Position + Node->Size);
            }
            else
            {
                Bound = FBox2D(Position, Position + Node->Size);
            }
        }
        LayersBound.Add(Bound);
        if (TotalBound.bIsValid)
        {
            TotalBound += Bound;
        }
        else
        {
            TotalBound = Bound;
        }
    }
    return LayersBound;
}

FFormatterGraph::FFormatterGraph(bool InIsVerticalLayout, bool InIsParameterGroup)
    : IsParameterGroup(InIsParameterGroup)
    , IsVerticalLayout(InIsVerticalLayout)
{
}

void FFormatterGraph::AddNode(FFormatterNode* InNode)
{
    Nodes.Add(InNode);
    NodesMap.Add(InNode->Guid, InNode);
    if (InNode->SubGraph != nullptr)
    {
        SubGraphs.Add(InNode->Guid, InNode->SubGraph);
    }
    for (auto Pin : InNode->InPins)
    {
        if (Pin->OriginalPin != nullptr)
        {
            OriginalPinsMap.Add(Pin->OriginalPin, Pin);
        }
        PinsMap.Add(Pin->Guid, Pin);
    }
    for (auto Pin : InNode->OutPins)
    {
        if (Pin->OriginalPin != nullptr)
        {
            OriginalPinsMap.Add(Pin->OriginalPin, Pin);
        }
        PinsMap.Add(Pin->Guid, Pin);
    }
}

void FFormatterGraph::SetPosition(const FVector2D& Position)
{
    const FVector2D Offset = Position - TotalBound.Min;
    OffsetBy(Offset);
}

void FFormatterGraph::SetBorder(float Left, float Top, float Right, float Bottom)
{
    this->Border = FBox2D(FVector2D(Left, Top), FVector2D(Right, Bottom));
}

FBox2D FFormatterGraph::GetBorder() const
{
    return Border;
}

void FFormatterGraph::SetIsParameterGroup(bool InIsParameterGroup)
{
    IsParameterGroup = InIsParameterGroup;
}

bool FFormatterGraph::GetIsParameterGroup() const
{
    return IsParameterGroup;
}

bool FFormatterGraph::GetIsVerticalLayout() const
{
    return IsVerticalLayout;
}

void FFormatterGraph::SetIsVerticalLayout(bool InIsVerticalLayout)
{
    IsVerticalLayout = InIsVerticalLayout;
}

FFormatterGraph::FFormatterGraph(const FFormatterGraph& Other)
{
    for (auto Node : Other.Nodes)
    {
        FFormatterNode* Cloned = new FFormatterNode(*Node);
        AddNode(Cloned);
    }
    for (auto Node : Other.Nodes)
    {
        for (auto Edge : Node->InEdges)
        {
            FFormatterPin* From = PinsMap[Edge->From->Guid];
            FFormatterPin* To = PinsMap[Edge->To->Guid];
            NodesMap[Node->Guid]->Connect(From, To, Edge->Weight);
        }
        for (auto Edge : Node->OutEdges)
        {
            FFormatterPin* From = PinsMap[Edge->From->Guid];
            FFormatterPin* To = PinsMap[Edge->To->Guid];
            NodesMap[Node->Guid]->Connect(From, To, Edge->Weight);
        }
    }

    TotalBound = Other.TotalBound;
    Border = Other.Border;
    IsParameterGroup = Other.IsParameterGroup;
    IsVerticalLayout = Other.IsVerticalLayout;
}

FFormatterGraph::~FFormatterGraph()
{
    if (IsNodeDetached)
    {
        return;
    }
 
    for (auto Node : Nodes)
    {
        delete Node;
    }
}

void FFormatterGraph::DetachAndDestroy()
{
    IsNodeDetached = true;
    delete this;
}

FFormatterGraph* FFormatterGraph::Clone()
{
    return nullptr;
}

void FDisconnectedGraph::AddGraph(FFormatterGraph* Graph)
{
    ConnectedGraphs.Push(Graph);
}

FDisconnectedGraph::~FDisconnectedGraph()
{
    for (auto Graph : ConnectedGraphs)
    {
        delete Graph;
    }
}

TMap<void*, FVector2D> FDisconnectedGraph::GetPinsOffset()
{
    TMap<void*, FVector2D> Result;
    for (auto Graph : ConnectedGraphs)
    {
        auto SubBound = Graph->GetTotalBound();
        auto Offset = SubBound.Min - GetTotalBound().Min;
        auto SubOffsets = Graph->GetPinsOffset();
        for (auto& SubOffsetPair : SubOffsets)
        {
            SubOffsetPair.Value = SubOffsetPair.Value + Offset;
        }
        Result.Append(SubOffsets);
    }
    return Result;
}

TArray<FFormatterPin*> FDisconnectedGraph::GetInputPins() const
{
    TSet<FFormatterPin*> Result;
    for (auto Graph : ConnectedGraphs)
    {
        Result.Append(Graph->GetInputPins());
    }
    return Result.Array();
}

TArray<FFormatterPin*> FDisconnectedGraph::GetOutputPins() const
{
    TSet<FFormatterPin*> Result;
    for (auto Graph : ConnectedGraphs)
    {
        Result.Append(Graph->GetOutputPins());
    }
    return Result.Array();
}

TSet<void*> FDisconnectedGraph::GetOriginalNodes() const
{
    TSet<void*> Result;
    for (auto Graph : ConnectedGraphs)
    {
        Result.Append(Graph->GetOriginalNodes());
    }
    return Result;
}

void FDisconnectedGraph::Format()
{
    FBox2D PreBound(ForceInit);
    for (auto Graph : ConnectedGraphs)
    {
        Graph->Format();

        if (PreBound.bIsValid)
        {
            FVector2D TopRight = FVector2D(PreBound.Max.X, PreBound.Min.Y);
            FVector2D BottomLeft = FVector2D(PreBound.Min.X, PreBound.Max.Y);
            FVector2D StartCorner = IsVerticalLayout ? TopRight : BottomLeft;
            Graph->SetPosition(StartCorner);
        }
        auto Bound = Graph->GetTotalBound();
        if (TotalBound.bIsValid)
        {
            TotalBound += Bound;
        }
        else
        {
            TotalBound = Bound;
        }

        FVector2D Offset = IsVerticalLayout ? FVector2D(VerticalSpacing, 0) : FVector2D(0, VerticalSpacing);
        PreBound = TotalBound.ShiftBy(Offset);
    }
}

void FDisconnectedGraph::OffsetBy(const FVector2D& InOffset)
{
    for (auto Graph : ConnectedGraphs)
    {
        Graph->OffsetBy(InOffset);
    }
}

TMap<void*, FBox2D> FDisconnectedGraph::GetBoundMap()
{
    TMap<void*, FBox2D> Result;
    for (auto Graph : ConnectedGraphs)
    {
        Result.Append(Graph->GetBoundMap());
    }
    return Result;
}

FConnectedGraph::FConnectedGraph(bool InIsVerticalLayout, bool InIsParameterGroup)
    : FFormatterGraph(InIsVerticalLayout, InIsParameterGroup)
{
}

FConnectedGraph::FConnectedGraph(const FConnectedGraph& Other)
    : FFormatterGraph(Other)
{
}

int32 FConnectedGraph::AssignPathDepthForNodes() const
{
    int32 LongestPath = 1;
    while (true)
    {
        auto Leaves = GetLeavesWithPathDepth0();
        if (Leaves.Num() == 0)
        {
            break;
        }
        for (auto leaf : Leaves)
        {
            leaf->PathDepth = LongestPath;
        }
        LongestPath++;
    }
    LongestPath--;
    return LongestPath;
}

FFormatterGraph* FConnectedGraph::Clone()
{
    return new FConnectedGraph(*this);
}

TArray<FFormatterNode*> FConnectedGraph::GetLeavesWithPathDepth0() const
{
    TArray<FFormatterNode*> Result;
    for (auto Node : Nodes)
    {
        if (Node->PathDepth != 0 || Node->AnySuccessorPathDepthEqu0())
        {
            continue;
        }
        Result.Add(Node);
    }
    return Result;
}

void FConnectedGraph::DoLayering()
{
    LayeredList.Empty();
    TSet<FFormatterNode*> Set;
    for (int32 i = AssignPathDepthForNodes(); i != 0; i--)
    {
        TSet<FFormatterNode*> Layer;
        auto Successors = FFormatterNode::GetSuccessorsForNodes(Set);
        auto NodesToProcess = GetNodesGreaterThan(i, Set);
        NodesToProcess.Append(Successors);
        for (auto Node : NodesToProcess)
        {
            auto Predecessors = Node->GetPredecessors();
            bool bPredecessorsFinished = true;
            for (auto Predecessor : Predecessors)
            {
                if (!Set.Contains(Predecessor))
                {
                    bPredecessorsFinished = false;
                    break;
                }
            }
            if (bPredecessorsFinished)
            {
                Layer.Add(Node);
            }
        }
        Set.Append(Layer);
        TArray<FFormatterNode*> Array = Layer.Array();
        if (MaxLayerNodes)
        {
            TArray<FFormatterNode*> SubLayer;
            for (int j = 0; j != Array.Num(); j++)
            {
                SubLayer.Add(Array[j]);
                if (SubLayer.Num() == MaxLayerNodes || j == Array.Num() - 1)
                {
                    if (NodeComparer)
                    {
                        SubLayer.Sort(NodeComparer);
                    }
                    LayeredList.Add(SubLayer);
                    SubLayer.Reset();
                }
            }
        }
        else
        {
            if (NodeComparer)
            {
                Array.Sort(NodeComparer);
            }
            LayeredList.Add(Array);
        }
    }
}

void FConnectedGraph::AddDummyNodes()
{
    for (int i = 0; i < LayeredList.Num() - 1; i++)
    {
        auto& Layer = LayeredList[i];
        auto& NextLayer = LayeredList[i + 1];
        for (auto Node : Layer)
        {
            TArray<FFormatterEdge*> LongEdges;
            for (auto Edge : Node->OutEdges)
            {
                if (!NextLayer.Contains(Edge->To->OwningNode))
                {
                    LongEdges.Add(Edge);
                }
            }
            for (auto Edge : LongEdges)
            {
                auto dummyNode = FFormatterNode::CreateDummy();
                AddNode(dummyNode);
                Node->Connect(Edge->From, dummyNode->InPins[0], Edge->Weight);
                dummyNode->Connect(dummyNode->InPins[0], Edge->From, Edge->Weight);
                dummyNode->Connect(dummyNode->OutPins[0], Edge->To, Edge->Weight);
                Edge->To->OwningNode->Disconnect(Edge->To, Edge->From);
                Edge->To->OwningNode->Connect(Edge->To, dummyNode->OutPins[0], Edge->Weight);
                Node->Disconnect(Edge->From, Edge->To);
                NextLayer.Add(dummyNode);
            }
        }
    }
}

void FConnectedGraph::SortInLayer(TArray<TArray<FFormatterNode*>>& Order, EEdGraphPinDirection Direction)
{
    if (Order.Num() < 2)
    {
        return;
    }
    const int Start = Direction == EGPD_Output ? Order.Num() - 2 : 1;
    const int End = Direction == EGPD_Output ? -1 : Order.Num();
    const int Step = Direction == EGPD_Output ? -1 : 1;
    for (int i = Start; i != End; i += Step)
    {
        auto& FixedLayer = Order[i - Step];
        auto& FreeLayer = Order[i];
        for (FFormatterNode* Node : FreeLayer)
        {
            Node->OrderValue = Node->CalcBarycenter(FixedLayer, Direction);
        }
        FreeLayer.StableSort([](const FFormatterNode& A, const FFormatterNode& B) -> bool
                             { return A.OrderValue < B.OrderValue; });
        FFormatterNode::CalculatePinsIndexInLayer(FreeLayer);
    }
}

TArray<FFormatterEdge*> FFormatterNode::GetEdgeBetweenTwoLayer(const TArray<FFormatterNode*>& LowerLayer, const TArray<FFormatterNode*>& UpperLayer, const FFormatterNode* ExcludedNode)
{
    TArray<FFormatterEdge*> Result;
    for (auto Node : LowerLayer)
    {
        if (ExcludedNode == Node)
        {
            continue;
        }
        Result += Node->GetEdgeLinkedToLayer(UpperLayer, EGPD_Input);
    }
    return Result;
}

int32 FFormatterNode::CalculateCrossing(const TArray<TArray<FFormatterNode*>>& Order)
{
    CalculatePinsIndex(Order);
    int32 CrossingValue = 0;
    for (int i = 1; i < Order.Num(); i++)
    {
        const auto& UpperLayer = Order[i - 1];
        const auto& LowerLayer = Order[i];
        TArray<FFormatterEdge*> NodeEdges = GetEdgeBetweenTwoLayer(LowerLayer, UpperLayer);
        while (NodeEdges.Num() != 0)
        {
            const auto Edge1 = NodeEdges.Pop();
            for (const auto Edge2 : NodeEdges)
            {
                if (Edge1->IsCrossing(Edge2))
                {
                    CrossingValue++;
                }
            }
        }
    }
    return CrossingValue;
}

void FConnectedGraph::DoOrderingSweep()
{
    auto Best = LayeredList;
    auto Order = LayeredList;
    int32 BestCrossing = INT_MAX;
    for (int i = 0; i < MaxOrderingIterations; i++)
    {
        SortInLayer(Order, i % 2 == 0 ? EGPD_Input : EGPD_Output);
        const int32 NewCrossing = FFormatterNode::CalculateCrossing(Order);
        if (NewCrossing < BestCrossing)
        {
            Best = Order;
            BestCrossing = NewCrossing;
        }
    }
    LayeredList = Best;
}

void FConnectedGraph::DoPositioning()
{
    if (IsVerticalLayout)
    {
        FFastAndSimplePositioningStrategy FastAndSimplePositioningStrategy(LayeredList, false, IsParameterGroup);
        TotalBound = FastAndSimplePositioningStrategy.GetTotalBound();
        return;
    }

    if (PositioningAlgorithm == EGraphFormatterPositioningAlgorithm::EEvenlyInLayer)
    {
        FEvenlyPlaceStrategy LeftToRightPositioningStrategy(LayeredList);
        TotalBound = LeftToRightPositioningStrategy.GetTotalBound();
    }
    else if (PositioningAlgorithm == EGraphFormatterPositioningAlgorithm::EFastAndSimpleMethodMedian || PositioningAlgorithm == EGraphFormatterPositioningAlgorithm::EFastAndSimpleMethodTop)
    {
        FFastAndSimplePositioningStrategy FastAndSimplePositioningStrategy(LayeredList, true, IsParameterGroup);
        TotalBound = FastAndSimplePositioningStrategy.GetTotalBound();
    }
    else if (PositioningAlgorithm == EGraphFormatterPositioningAlgorithm::ELayerSweep)
    {
        FPriorityPositioningStrategy PriorityPositioningStrategy(LayeredList);
        TotalBound = PriorityPositioningStrategy.GetTotalBound();
    }
}

TMap<void*, FVector2D> FConnectedGraph::GetPinsOffset()
{
    TMap<void*, FVector2D> Result;
    for (auto Node : Nodes)
    {
        for (auto OutPin : Node->OutPins)
        {
            FVector2D PinOffset = Node->Position + OutPin->NodeOffset - TotalBound.Min;
            Result.Add(OutPin->OriginalPin, PinOffset);
        }
        for (auto InPin : Node->InPins)
        {
            FVector2D PinOffset = Node->Position + InPin->NodeOffset - TotalBound.Min;
            Result.Add(InPin->OriginalPin, PinOffset);
        }
    }
    return Result;
}

TArray<FFormatterPin*> FConnectedGraph::GetInputPins() const
{
    TSet<FFormatterPin*> Result;
    for (auto Node : Nodes)
    {
        for (auto Pin : Node->InPins)
        {
            Result.Add(Pin);
        }
    }
    return Result.Array();
}

TArray<FFormatterPin*> FConnectedGraph::GetOutputPins() const
{
    TSet<FFormatterPin*> Result;
    for (auto Node : Nodes)
    {
        for (auto Pin : Node->OutPins)
        {
            Result.Add(Pin);
        }
    }
    return Result.Array();
}

TSet<void*> FConnectedGraph::GetOriginalNodes() const
{
    TSet<void*> Result;
    for (auto Node : Nodes)
    {
        if (SubGraphs.Contains(Node->Guid))
        {
            Result.Append(SubGraphs[Node->Guid]->GetOriginalNodes());
        }
        if (Node->OriginalNode != nullptr)
        {
            Result.Add(Node->OriginalNode);
        }
    }
    return Result;
}

void FConnectedGraph::Format()
{
    for (auto [Key, SubGraph] : SubGraphs)
    {
        auto Node = NodesMap[Key];
        SubGraph->Format();
        auto SubGraphBorder = SubGraph->GetBorder();
        Node->UpdatePinsOffset(SubGraphBorder.Min);
        auto Bound = SubGraph->GetTotalBound();
        Node->InitPosition(Bound.Min - SubGraphBorder.Min);
        Node->Size = SubGraph->GetTotalBound().GetSize() + FVector2D(SubGraphBorder.Min.X + SubGraphBorder.Max.X, SubGraphBorder.Min.Y + SubGraphBorder.Max.Y);
    }
    if (Nodes.Num() > 0)
    {
        RemoveCycle();
        DoLayering();
        AddDummyNodes();
        if (!NodeComparer)
        {
            DoOrderingSweep();
        }
        DoPositioning();
    }
}

void FConnectedGraph::OffsetBy(const FVector2D& InOffset)
{
    for (auto Node : Nodes)
    {
        Node->SetPosition(Node->GetPosition() + InOffset);
    }
    TotalBound = TotalBound.ShiftBy(InOffset);
}

TMap<void*, FBox2D> FConnectedGraph::GetBoundMap()
{
    TMap<void*, FBox2D> Result;
    for (auto Node : Nodes)
    {
        if (Node->OriginalNode == nullptr)
        {
            continue;
        }
        Result.Add(Node->OriginalNode, FBox2D(Node->GetPosition(), Node->GetPosition() + Node->Size));
        if (SubGraphs.Contains(Node->Guid))
        {
            Result.Append(SubGraphs[Node->Guid]->GetBoundMap());
        }
    }
    return Result;
}

