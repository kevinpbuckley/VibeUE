// Copyright Buckley Builds LLC 2026

#include "PythonAPI/UStateTreeService.h"
#if WITH_EDITOR
#include "StateTreeEditorData.h"
#include "StateTreeEditingSubsystem.h"
#include "Editor.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/DefaultValueHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "JsonObjectConverter.h"
#include "UObject/Package.h"
#endif

FString UStateTreeService::GetEditorDataClass(const FString& AssetPath)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty()) return FString();
    UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *AssetPath));
    if (!StateTree) return FString();
    if (UObject* ED = StateTree->EditorData) return ED->GetClass()->GetPathName();
    return FString();
#else
    return FString();
#endif
}

FString UStateTreeService::GetStateGuid(const FString& AssetPath, const FString& StateName)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || StateName.IsEmpty()) return FString();
    UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *AssetPath));
    if (!StateTree) return FString();
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return FString();
    TFunction<UStateTreeState*(UStateTreeState*)> FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found) return Found->ID.ToString();
    }
    return FString();
#else
    return FString();
#endif
}

FString UStateTreeService::GetStateAsJson(const FString& AssetPath, const FString& StateName)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || StateName.IsEmpty()) return FString();
    UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *AssetPath));
    if (!StateTree) return FString();
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return FString();

    TFunction<UStateTreeState*(UStateTreeState*)> FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* Target = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found) { Target = Found; break; }
    }
    if (!Target) return FString();

    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("Name"), Target->Name.ToString());
    Obj->SetStringField(TEXT("Description"), Target->Description);
    Obj->SetStringField(TEXT("Tag"), Target->Tag.ToString());
    Obj->SetBoolField(TEXT("Enabled"), Target->bEnabled);
    Obj->SetNumberField(TEXT("ChildrenCount"), Target->Children.Num());
    Obj->SetNumberField(TEXT("TasksCount"), Target->Tasks.Num());
    Obj->SetNumberField(TEXT("EnterConditionsCount"), Target->EnterConditions.Num());
    Obj->SetNumberField(TEXT("ConsiderationsCount"), Target->Considerations.Num());
    Obj->SetStringField(TEXT("LinkedSubtreeName"), Target->LinkedSubtree.Name.IsNone() ? TEXT("") : Target->LinkedSubtree.Name.ToString());
    Obj->SetStringField(TEXT("ID"), Target->ID.ToString());

    // children names
    TArray<TSharedPtr<FJsonValue>> ChildArr;
    for (TObjectPtr<UStateTreeState> C : Target->Children)
    {
        if (C) ChildArr.Add(MakeShared<FJsonValueString>(C->Name.ToString()));
    }
    Obj->SetArrayField(TEXT("Children"), ChildArr);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
    return Out;
#else
    return FString();
#endif
}

bool UStateTreeService::ValidateStateTree(const FString& AssetPath)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *AssetPath));
    if (!StateTree) return false;
    if (!GEditor) return false;
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
        return true;
    }
    return false;
#else
    return false;
#endif
}

bool UStateTreeService::CompileIfChanged(const FString& AssetPath)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *AssetPath));
    if (!StateTree) return false;
    StateTree->CompileIfChanged();
    return true;
#else
    return false;
#endif
}

bool UStateTreeService::ResetCompiled(const FString& AssetPath)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *AssetPath));
    if (!StateTree) return false;
    StateTree->ResetCompiled();
    return true;
#else
    return false;
#endif
}

FString UStateTreeService::GetStructDefaultJson(const FString& StructPath)
{
#if WITH_EDITOR
    if (StructPath.IsEmpty()) return FString();
    UScriptStruct* SS = FindObject<UScriptStruct>(nullptr, *StructPath);
    if (!SS) SS = LoadObject<UScriptStruct>(nullptr, *StructPath);
    if (!SS) return FString();

    int32 Size = SS->GetStructureSize();
    void* Mem = FMemory::Malloc(Size);
    FMemory::Memzero(Mem, Size);
    SS->InitializeStruct(Mem);
    FString Json;
    if (!FJsonObjectConverter::UStructToJsonObjectString(SS, Mem, Json, 0, 0))
    {
        SS->DestroyStruct(Mem);
        FMemory::Free(Mem);
        return FString();
    }
    SS->DestroyStruct(Mem);
    FMemory::Free(Mem);
    return Json;
#else
    return FString();
#endif
}

bool UStateTreeService::SetStructFromJson(const FString& StructPath, const FString& JsonString)
{
#if WITH_EDITOR
    if (StructPath.IsEmpty() || JsonString.IsEmpty()) return false;
    UScriptStruct* SS = FindObject<UScriptStruct>(nullptr, *StructPath);
    if (!SS) SS = LoadObject<UScriptStruct>(nullptr, *StructPath);
    if (!SS) return false;

    int32 Size = SS->GetStructureSize();
    void* Mem = FMemory::Malloc(Size);
    FMemory::Memzero(Mem, Size);
    SS->InitializeStruct(Mem);

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        SS->DestroyStruct(Mem);
        FMemory::Free(Mem);
        return false;
    }
    bool bOk = FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), SS, Mem, 0, 0);
    SS->DestroyStruct(Mem);
    FMemory::Free(Mem);
    return bOk;
#else
    return false;
#endif
}

bool UStateTreeService::ReinstanceNodeInstance(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 NodeIndex, const FString& NewStructPath)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || NodeArray.IsEmpty() || NodeIndex < 0 || NewStructPath.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    // locate node array
    TArray<FStateTreeEditorNode>* NodeArrayPtr = nullptr;
    UStateTreeState* TargetState = nullptr;
    if (NodeArray.Equals(TEXT("GlobalTasks"), ESearchCase::IgnoreCase))
    {
        NodeArrayPtr = &EditorData->GlobalTasks;
    }
    else
    {
        TFunction<UStateTreeState*(UStateTreeState*)> FindInState = [&](UStateTreeState* Node)->UStateTreeState*
        {
            if (!Node) return nullptr;
            if (Node->Name.ToString() == StateName) return Node;
            for (TObjectPtr<UStateTreeState> Child : Node->Children)
            {
                UStateTreeState* Found = FindInState(Child);
                if (Found) return Found;
            }
            return nullptr;
        };
        for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
        {
            UStateTreeState* Found = FindInState(ST);
            if (Found) { TargetState = Found; break; }
        }
        if (!TargetState) return false;
        if (NodeArray.Equals(TEXT("Tasks"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->Tasks;
        else if (NodeArray.Equals(TEXT("EnterConditions"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->EnterConditions;
        else if (NodeArray.Equals(TEXT("Considerations"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->Considerations;
        else return false;
    }
    if (!NodeArrayPtr) return false;
    if (NodeIndex < 0 || NodeIndex >= NodeArrayPtr->Num()) return false;

    UScriptStruct* SS = FindObject<UScriptStruct>(nullptr, *NewStructPath);
    if (!SS) SS = LoadObject<UScriptStruct>(nullptr, *NewStructPath);
    if (!SS) return false;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::ReinstanceNodeInstance")));
    (*NodeArrayPtr)[NodeIndex].Instance.InitializeAs(SS);
    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
#else
    return false;
#endif
}

#if WITH_EDITOR
TArray<FString> UStateTreeService::ListPropertyBindings(const FString& AssetPath)
{
    TArray<FString> Out;
    if (AssetPath.IsEmpty()) return Out;
    UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *AssetPath));
    if (!StateTree) return Out;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return Out;

    // Walk all nodes (roots and children)
    TFunction<void(UStateTreeState*, const FString&)> DFS = [&](UStateTreeState* Node, const FString& Path)
    {
        if (!Node) return;
        FString MyPath = Path.IsEmpty() ? Node->Name.ToString() : (Path + TEXT("/") + Node->Name.ToString());
        auto InspectNodeArray = [&](TArray<FStateTreeEditorNode>& Arr, const FString& ArrName)
        {
            for (int32 i = 0; i < Arr.Num(); ++i)
            {
            FStateTreeEditorNode& N = Arr[i];
            const UScriptStruct* SS = N.Instance.GetScriptStruct();
            void* Mem = N.Instance.GetMutableMemory();
            if (!Mem) { SS = N.Node.GetScriptStruct(); Mem = N.Node.GetMutableMemory(); }
                if (!SS || !Mem) continue;
                FString Json;
                if (!FJsonObjectConverter::UStructToJsonObjectString(SS, Mem, Json, 0, 0)) continue;
                TSharedPtr<FJsonObject> Obj;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
                if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) continue;
                for (auto& Pair : Obj->Values)
                {
                    const TSharedPtr<FJsonValue>& V = Pair.Value;
                    if (V.IsValid() && V->Type == EJson::Object)
                    {
                        TSharedPtr<FJsonObject> Sub = V->AsObject();
                        if (Sub->HasField(TEXT("Path")) || Sub->HasField(TEXT("Property")) || Sub->HasField(TEXT("Binding")))
                        {
                            FString Entry = FString::Printf(TEXT("%s|%s|%s|%d"), *MyPath, *ArrName, *Pair.Key, i);
                            Out.Add(Entry);
                        }
                    }
                }
            }
        };

        InspectNodeArray(Node->Tasks, TEXT("Tasks"));
        InspectNodeArray(Node->EnterConditions, TEXT("EnterConditions"));
        InspectNodeArray(Node->Considerations, TEXT("Considerations"));
        for (TObjectPtr<UStateTreeState> C : Node->Children) DFS(C, MyPath);
    };

    for (TObjectPtr<UStateTreeState> Root : EditorData->SubTrees)
    {
        DFS(Root, TEXT(""));
    }

    // Also include global tasks
    for (int32 i = 0; i < EditorData->GlobalTasks.Num(); ++i)
    {
        FStateTreeEditorNode& N = EditorData->GlobalTasks[i];
        const UScriptStruct* SS = N.Instance.GetScriptStruct();
        void* Mem = N.Instance.GetMutableMemory();
        if (!Mem) { SS = N.Node.GetScriptStruct(); Mem = N.Node.GetMutableMemory(); }
        if (!SS || !Mem) continue;
        FString Json;
        if (!FJsonObjectConverter::UStructToJsonObjectString(SS, Mem, Json, 0, 0)) continue;
        TSharedPtr<FJsonObject> Obj;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) continue;
        for (auto& Pair : Obj->Values)
        {
            const TSharedPtr<FJsonValue>& V = Pair.Value;
            if (V.IsValid() && V->Type == EJson::Object)
            {
                TSharedPtr<FJsonObject> Sub = V->AsObject();
                if (Sub->HasField(TEXT("Path")) || Sub->HasField(TEXT("Property")) || Sub->HasField(TEXT("Binding")))
                {
                    FString Entry = FString::Printf(TEXT("<Global>|GlobalTasks|%s|%d"), *Pair.Key, i);
                    Out.Add(Entry);
                }
            }
        }
    }

    return Out;
}

bool UStateTreeService::AddPropertyBinding(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 NodeIndex, const FString& FieldName, const FString& BindingJson)
{
    if (AssetPath.IsEmpty() || FieldName.IsEmpty()) return false;
    FString Current = GetNodeStructAsJson(AssetPath, StateName, NodeArray, NodeIndex);
    if (Current.IsEmpty()) return false;
    TSharedPtr<FJsonObject> Obj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Current);
    if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) return false;

    // Parse binding json value
    TSharedPtr<FJsonValue> NewVal;
    TSharedRef<TJsonReader<>> R2 = TJsonReaderFactory<>::Create(BindingJson);
    TSharedPtr<FJsonObject> TempObj;
    if (FJsonSerializer::Deserialize(R2, TempObj) && TempObj.IsValid()) NewVal = MakeShared<FJsonValueObject>(TempObj);
    else
    {
        double Num; if (FDefaultValueHelper::ParseDouble(BindingJson, Num)) NewVal = MakeShared<FJsonValueNumber>(Num);
        else if (BindingJson.Equals(TEXT("true"), ESearchCase::IgnoreCase)) NewVal = MakeShared<FJsonValueBoolean>(true);
        else if (BindingJson.Equals(TEXT("false"), ESearchCase::IgnoreCase)) NewVal = MakeShared<FJsonValueBoolean>(false);
        else NewVal = MakeShared<FJsonValueString>(BindingJson);
    }
    if (!NewVal.IsValid()) return false;
    Obj->SetField(FieldName, NewVal);
    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
    return SetNodeStructFromJson(AssetPath, StateName, NodeArray, NodeIndex, Out);
}

bool UStateTreeService::RemovePropertyBinding(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 NodeIndex, const FString& FieldName)
{
    if (AssetPath.IsEmpty() || FieldName.IsEmpty()) return false;
    FString Current = GetNodeStructAsJson(AssetPath, StateName, NodeArray, NodeIndex);
    if (Current.IsEmpty()) return false;
    TSharedPtr<FJsonObject> Obj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Current);
    if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) return false;
    Obj->RemoveField(FieldName);
    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
    return SetNodeStructFromJson(AssetPath, StateName, NodeArray, NodeIndex, Out);
}

bool UStateTreeService::RemapPropertyBindings(const FString& AssetPath, const FString& OldPath, const FString& NewPath)
{
    if (AssetPath.IsEmpty() || OldPath.IsEmpty() || NewPath.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    auto ProcessNode = [&](UStateTreeState* State, const TArray<FStateTreeEditorNode>& Arr, const FString& ArrName)
    {
        for (int32 i = 0; i < Arr.Num(); ++i)
        {
            FString Json = GetNodeStructAsJson(AssetPath, State ? State->Name.ToString() : TEXT(""), ArrName, i);
            if (Json.IsEmpty()) continue;
            TSharedPtr<FJsonObject> Obj;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
            if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) continue;
            bool bChanged = false;
            for (auto& Pair : Obj->Values)
            {
                const TSharedPtr<FJsonValue>& V = Pair.Value;
                if (V.IsValid() && V->Type == EJson::Object)
                {
                    TSharedPtr<FJsonObject> Sub = V->AsObject();
                    if (Sub->HasField(TEXT("Path")))
                    {
                        FString P = Sub->GetStringField(TEXT("Path"));
                        if (P == OldPath) { Sub->SetStringField(TEXT("Path"), NewPath); bChanged = true; }
                    }
                }
            }
            if (bChanged)
            {
                FString Out;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
                FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
                SetNodeStructFromJson(AssetPath, State ? State->Name.ToString() : TEXT(""), ArrName, i, Out);
            }
        }
    };

    // Walk tree
    TFunction<void(UStateTreeState*)> DFS = [&](UStateTreeState* Node)
    {
        if (!Node) return;
        ProcessNode(Node, Node->Tasks, TEXT("Tasks"));
        ProcessNode(Node, Node->EnterConditions, TEXT("EnterConditions"));
        ProcessNode(Node, Node->Considerations, TEXT("Considerations"));
        for (TObjectPtr<UStateTreeState> C : Node->Children) DFS(C);
    };

    for (TObjectPtr<UStateTreeState> Root : EditorData->SubTrees)
    {
        DFS(Root);
    }

    // global tasks
    ProcessNode(nullptr, EditorData->GlobalTasks, TEXT("GlobalTasks"));

    return true;
}

int32 UStateTreeService::FindTransitionByGuid(const FString& AssetPath, const FString& GuidString)
{
    if (AssetPath.IsEmpty() || GuidString.IsEmpty()) return -1;
    UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *AssetPath));
    if (!StateTree) return -1;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return -1;

    FGuid G; if (!FGuid::Parse(GuidString, G)) return -1;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        // recursive search
        TFunction<int32(UStateTreeState*)> SearchState;
        SearchState = [&](UStateTreeState* Node)->int32
        {
            if (!Node) return -1;
            for (int32 i = 0; i < Node->Transitions.Num(); ++i)
            {
                if (Node->Transitions[i].ID == G) return i;
            }
            for (TObjectPtr<UStateTreeState> C : Node->Children)
            {
                int32 R = SearchState(C);
                if (R >= 0) return R;
            }
            return -1;
        };
        int32 Found = SearchState(ST);
        if (Found >= 0) return Found;
    }
    return -1;
}

int32 UStateTreeService::AddTransitionWithGuid(const FString& AssetPath, const FString& StateName, const FString& GuidString)
{
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || GuidString.IsEmpty()) return -1;
    UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *AssetPath));
    if (!StateTree) return -1;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return -1;

    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found) { TargetState = Found; break; }
    }
    if (!TargetState) return -1;

    FGuid G; if (!FGuid::Parse(GuidString, G)) G = FGuid::NewGuid();
    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::AddTransitionWithGuid")));
    TargetState->Modify(false);
    int32 NewIndex = TargetState->Transitions.AddDefaulted();
    TargetState->Transitions[NewIndex].ID = G;
    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>()) EditingSubsystem->ValidateStateTree(StateTree);
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return NewIndex;
}

bool UStateTreeService::SetTransitionConditionsJson(const FString& AssetPath, const FString& StateName, int32 TransitionIndex, const FString& ConditionsJsonArray)
{
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || TransitionIndex < 0 || ConditionsJsonArray.IsEmpty()) return false;
    // Build a wrapper object: { "Conditions": <array> }
    FString Wrapper = FString::Printf(TEXT("{\"Conditions\":%s}"), *ConditionsJsonArray);
    return SetTransitionFromJson(AssetPath, StateName, TransitionIndex, Wrapper);
}

bool UStateTreeService::BeginTransaction(const FString& Key, const FString& Reason)
{
    return BeginBulkEdit(Key.IsEmpty() ? FString() : Key, Reason);
}

bool UStateTreeService::EndTransaction(const FString& Key)
{
    return EndBulkEdit(Key.IsEmpty() ? FString() : Key);
}
#if WITH_EDITOR
FString UStateTreeService::ExportEditorDataJson(const FString& AssetPath)
{
    if (AssetPath.IsEmpty()) return FString();
    UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *AssetPath));
    if (!StateTree) return FString();
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return FString();

    // Simple export: iterate editor data fields we know and emit JSON
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    // Subtrees
    TArray<TSharedPtr<FJsonValue>> SubArr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        if (!ST) continue;
        TSharedPtr<FJsonObject> SObj = MakeShared<FJsonObject>();
        SObj->SetStringField(TEXT("Name"), ST->Name.ToString());
        SObj->SetStringField(TEXT("ID"), ST->ID.ToString());
        SObj->SetStringField(TEXT("Description"), ST->Description);
        SObj->SetNumberField(TEXT("ChildrenCount"), ST->Children.Num());
        SubArr.Add(MakeShared<FJsonValueObject>(SObj));
    }
    Root->SetArrayField(TEXT("SubTrees"), SubArr);

    // Global tasks count
    Root->SetNumberField(TEXT("GlobalTasks"), EditorData->GlobalTasks.Num());

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    return Out;
}
#endif
#endif

#if WITH_EDITOR
bool UStateTreeService::SetTransitionTarget(const FString& AssetPath, const FString& StateName, int32 TransitionIndex, const FString& TargetStateName)
{
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || TargetStateName.IsEmpty() || TransitionIndex < 0) return false;
    UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    // find target state object by name
    UStateTreeState* TargetStateObj = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        if (ST && ST->Name.ToString() == TargetStateName) { TargetStateObj = ST; break; }
    }
    if (!TargetStateObj)
    {
        // search deeper
        TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
        FindInState = [&](UStateTreeState* Node)->UStateTreeState*
        {
            if (!Node) return nullptr;
            if (Node->Name.ToString() == TargetStateName) return Node;
            for (TObjectPtr<UStateTreeState> Child : Node->Children)
            {
                UStateTreeState* Found = FindInState(Child);
                if (Found) return Found;
            }
            return nullptr;
        };
        for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
        {
            UStateTreeState* Found = FindInState(ST);
            if (Found) { TargetStateObj = Found; break; }
        }
    }
    if (!TargetStateObj) return false;

    // find the source state and set transition
    TFunction<UStateTreeState*(UStateTreeState*)> FindSrc;
    FindSrc = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindSrc(Child);
            if (Found) return Found;
        }
        return nullptr;
    };
    UStateTreeState* SrcState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindSrc(ST);
        if (Found) { SrcState = Found; break; }
    }
    if (!SrcState) return false;
    if (TransitionIndex >= SrcState->Transitions.Num()) return false;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::SetTransitionTarget")));
    SrcState->Modify(false);
    FStateTreeTransition& T = SrcState->Transitions[TransitionIndex];
    T.State.Name = TargetStateObj->Name;
    T.State.ID = TargetStateObj->ID;
    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
}

bool UStateTreeService::SetTransitionPriority(const FString& AssetPath, const FString& StateName, int32 TransitionIndex, int32 Priority)
{
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || TransitionIndex < 0) return false;
    UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    TFunction<UStateTreeState*(UStateTreeState*)> FindSrc;
    FindSrc = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindSrc(Child);
            if (Found) return Found;
        }
        return nullptr;
    };
    UStateTreeState* SrcState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindSrc(ST);
        if (Found) { SrcState = Found; break; }
    }
    if (!SrcState) return false;
    if (TransitionIndex >= SrcState->Transitions.Num()) return false;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::SetTransitionPriority")));
    SrcState->Modify(false);
    FStateTreeTransition& T = SrcState->Transitions[TransitionIndex];
    T.Priority = static_cast<EStateTreeTransitionPriority>(Priority);
    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
}

bool UStateTreeService::SetTransitionField(const FString& AssetPath, const FString& StateName, int32 TransitionIndex, const FString& FieldName, const FString& JsonValue)
{
    // Generic setter: deserialize transition to JSON, set field, then write back
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || FieldName.IsEmpty() || TransitionIndex < 0) return false;
    FString Json = GetTransitionAsJson(AssetPath, StateName, TransitionIndex);
    if (Json.IsEmpty()) return false;
    TSharedPtr<FJsonObject> Obj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) return false;

    // Parse JsonValue as a JSON value
    TSharedPtr<FJsonValue> NewVal;
    {
        TSharedRef<TJsonReader<>> R2 = TJsonReaderFactory<>::Create(JsonValue);
        TSharedPtr<FJsonObject> TempObj;
        if (FJsonSerializer::Deserialize(R2, TempObj) && TempObj.IsValid())
        {
            NewVal = MakeShared<FJsonValueObject>(TempObj);
        }
        else
        {
            // Try parse as primitive
            double Num;
            if (JsonValue.Equals(TEXT("true"), ESearchCase::IgnoreCase)) { NewVal = MakeShared<FJsonValueBoolean>(true); }
            else if (JsonValue.Equals(TEXT("false"), ESearchCase::IgnoreCase)) { NewVal = MakeShared<FJsonValueBoolean>(false); }
            else if (FDefaultValueHelper::ParseDouble(JsonValue, Num)) { NewVal = MakeShared<FJsonValueNumber>(Num); }
            else { NewVal = MakeShared<FJsonValueString>(JsonValue); }
        }
    }
    if (!NewVal.IsValid()) return false;
    Obj->SetField(FieldName, NewVal);

    FString OutJson;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
    FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
    return SetTransitionFromJson(AssetPath, StateName, TransitionIndex, OutJson);
}

bool UStateTreeService::AddPropertyBagEntry(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName, const FString& JsonValue)
{
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || BagPropertyName.IsEmpty() || EntryName.IsEmpty()) return false;
    FString Current = GetPropertyAsJson(AssetPath, StateName, BagPropertyName);
    if (Current.IsEmpty()) return false;
    TSharedPtr<FJsonObject> Obj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Current);
    if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) return false;

    // Parse new value
    TSharedPtr<FJsonValue> NewVal;
    TSharedRef<TJsonReader<>> R2 = TJsonReaderFactory<>::Create(JsonValue);
    TSharedPtr<FJsonObject> TempObj;
    if (FJsonSerializer::Deserialize(R2, TempObj) && TempObj.IsValid()) NewVal = MakeShared<FJsonValueObject>(TempObj);
    else {
        double Num; if (FDefaultValueHelper::ParseDouble(JsonValue, Num)) NewVal = MakeShared<FJsonValueNumber>(Num);
        else if (JsonValue.Equals(TEXT("true"), ESearchCase::IgnoreCase)) NewVal = MakeShared<FJsonValueBoolean>(true);
        else if (JsonValue.Equals(TEXT("false"), ESearchCase::IgnoreCase)) NewVal = MakeShared<FJsonValueBoolean>(false);
        else NewVal = MakeShared<FJsonValueString>(JsonValue);
    }
    Obj->SetField(EntryName, NewVal);
    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
    return SetPropertyFromJson(AssetPath, StateName, BagPropertyName, Out);
}

bool UStateTreeService::RemovePropertyBagEntry(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName)
{
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || BagPropertyName.IsEmpty() || EntryName.IsEmpty()) return false;
    FString Current = GetPropertyAsJson(AssetPath, StateName, BagPropertyName);
    if (Current.IsEmpty()) return false;
    TSharedPtr<FJsonObject> Obj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Current);
    if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) return false;
    Obj->RemoveField(EntryName);
    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
    return SetPropertyFromJson(AssetPath, StateName, BagPropertyName, Out);
}

bool UStateTreeService::RenamePropertyBagEntry(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& OldName, const FString& NewName)
{
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || BagPropertyName.IsEmpty() || OldName.IsEmpty() || NewName.IsEmpty()) return false;
    FString Current = GetPropertyAsJson(AssetPath, StateName, BagPropertyName);
    if (Current.IsEmpty()) return false;
    TSharedPtr<FJsonObject> Obj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Current);
    if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) return false;
    TSharedPtr<FJsonValue> Val = Obj->TryGetField(OldName);
    if (!Val.IsValid()) return false;
    Obj->SetField(NewName, Val);
    Obj->RemoveField(OldName);
    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
    return SetPropertyFromJson(AssetPath, StateName, BagPropertyName, Out);
}

bool UStateTreeService::GetPropertyBagEntryBool(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName)
{
    FString Json = GetPropertyBagEntryAsJson(AssetPath, StateName, BagPropertyName, EntryName);
    if (Json.IsEmpty()) return false;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    TSharedPtr<FJsonValue> Val;
    if (FJsonSerializer::Deserialize(Reader, Val) && Val.IsValid())
    {
        bool B = false; if (Val->TryGetBool(B)) return B;
        FString S; if (Val->TryGetString(S)) return (S.Equals(TEXT("true"), ESearchCase::IgnoreCase));
        double N; if (Val->TryGetNumber(N)) return (N != 0.0);
    }
    return false;
}

int32 UStateTreeService::GetPropertyBagEntryInt(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName)
{
    FString Json = GetPropertyBagEntryAsJson(AssetPath, StateName, BagPropertyName, EntryName);
    if (Json.IsEmpty()) return 0;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    TSharedPtr<FJsonValue> Val;
    if (FJsonSerializer::Deserialize(Reader, Val) && Val.IsValid())
    {
        double N; if (Val->TryGetNumber(N)) return (int32)N;
        FString S; if (Val->TryGetString(S)) return FCString::Atoi(*S);
    }
    return 0;
}

float UStateTreeService::GetPropertyBagEntryFloat(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName)
{
    FString Json = GetPropertyBagEntryAsJson(AssetPath, StateName, BagPropertyName, EntryName);
    if (Json.IsEmpty()) return 0.0f;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    TSharedPtr<FJsonValue> Val;
    if (FJsonSerializer::Deserialize(Reader, Val) && Val.IsValid())
    {
        double N; if (Val->TryGetNumber(N)) return (float)N;
        FString S; if (Val->TryGetString(S)) return FCString::Atof(*S);
    }
    return 0.0f;
}

FString UStateTreeService::GetPropertyBagEntryString(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName)
{
    FString Json = GetPropertyBagEntryAsJson(AssetPath, StateName, BagPropertyName, EntryName);
    if (Json.IsEmpty()) return FString();
    // If JSON is a quoted string, parse it
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    TSharedPtr<FJsonValue> Val;
    if (FJsonSerializer::Deserialize(Reader, Val) && Val.IsValid())
    {
        FString S; if (Val->TryGetString(S)) return S;
        // fallback: return raw JSON
        return Json;
    }
    return FString();
}

TArray<FString> UStateTreeService::ListBindingsFromNode(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 NodeIndex)
{
    TArray<FString> Out;
    FString Json = GetNodeStructAsJson(AssetPath, StateName, NodeArray, NodeIndex);
    if (Json.IsEmpty()) return Out;
    TSharedPtr<FJsonObject> Obj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) return Out;
    for (auto& Pair : Obj->Values)
    {
        const TSharedPtr<FJsonValue>& V = Pair.Value;
        if (V.IsValid() && V->Type == EJson::Object)
        {
            TSharedPtr<FJsonObject> Sub = V->AsObject();
            if (Sub->HasField(TEXT("Path")) || Sub->HasField(TEXT("Property")) || Sub->HasField(TEXT("Binding")))
            {
                Out.Add(Pair.Key);
            }
        }
    }
    return Out;
}

bool UStateTreeService::SetNodeStructFieldAsJson(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 NodeIndex, const FString& FieldName, const FString& JsonValue)
{
    if (AssetPath.IsEmpty() || FieldName.IsEmpty()) return false;
    FString Json = GetNodeStructAsJson(AssetPath, StateName, NodeArray, NodeIndex);
    if (Json.IsEmpty()) return false;
    TSharedPtr<FJsonObject> Obj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) return false;

    // parse JsonValue to FJsonValue
    TSharedPtr<FJsonValue> NewVal;
    TSharedRef<TJsonReader<>> R2 = TJsonReaderFactory<>::Create(JsonValue);
    TSharedPtr<FJsonObject> TempObj;
    if (FJsonSerializer::Deserialize(R2, TempObj) && TempObj.IsValid()) NewVal = MakeShared<FJsonValueObject>(TempObj);
    else {
        double Num; if (FDefaultValueHelper::ParseDouble(JsonValue, Num)) NewVal = MakeShared<FJsonValueNumber>(Num);
        else if (JsonValue.Equals(TEXT("true"), ESearchCase::IgnoreCase)) NewVal = MakeShared<FJsonValueBoolean>(true);
        else if (JsonValue.Equals(TEXT("false"), ESearchCase::IgnoreCase)) NewVal = MakeShared<FJsonValueBoolean>(false);
        else NewVal = MakeShared<FJsonValueString>(JsonValue);
    }
    Obj->SetField(FieldName, NewVal);
    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
    return SetNodeStructFromJson(AssetPath, StateName, NodeArray, NodeIndex, Out);
}
#endif

#if WITH_EDITOR
TArray<FString> UStateTreeService::ListTransitions(const FString& AssetPath, const FString& StateName)
{
    TArray<FString> Out;
    if (AssetPath.IsEmpty() || StateName.IsEmpty()) return Out;
    UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *AssetPath));
    if (!StateTree) return Out;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return Out;

    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found) { TargetState = Found; break; }
    }
    if (!TargetState) return Out;

    for (int32 i = 0; i < TargetState->Transitions.Num(); ++i)
    {
        const FStateTreeTransition& T = TargetState->Transitions[i];
        FString Json;
        if (FJsonObjectConverter::UStructToJsonObjectString(FStateTreeTransition::StaticStruct(), (void*)&T, Json, 0, 0))
        {
            Out.Add(Json);
        }
        else
        {
            Out.Add(FString::Printf(TEXT("{\"index\":%d}"), i));
        }
    }
    return Out;
}

FString UStateTreeService::GetTransitionAsJson(const FString& AssetPath, const FString& StateName, int32 TransitionIndex)
{
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || TransitionIndex < 0) return FString();
    UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *AssetPath));
    if (!StateTree) return FString();
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return FString();

    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found) { TargetState = Found; break; }
    }
    if (!TargetState) return FString();
    if (TransitionIndex >= TargetState->Transitions.Num()) return FString();

    FString Json;
    const FStateTreeTransition& T = TargetState->Transitions[TransitionIndex];
    if (FJsonObjectConverter::UStructToJsonObjectString(FStateTreeTransition::StaticStruct(), (void*)&T, Json, 0, 0))
    {
        return Json;
    }
    return FString();
}

bool UStateTreeService::MoveTransitionIndex(const FString& AssetPath, const FString& StateName, int32 FromIndex, int32 ToIndex)
{
    if (AssetPath.IsEmpty() || StateName.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found) { TargetState = Found; break; }
    }
    if (!TargetState) return false;

    int32 Count = TargetState->Transitions.Num();
    if (FromIndex < 0 || FromIndex >= Count || ToIndex < 0 || ToIndex >= Count) return false;
    if (FromIndex == ToIndex) return true;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::MoveTransitionIndex")));
    TargetState->Modify(false);
    FStateTreeTransition Copy = TargetState->Transitions[FromIndex];
    TargetState->Transitions.RemoveAt(FromIndex);
    TargetState->Transitions.Insert(Copy, ToIndex);
    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
}

TArray<FString> UStateTreeService::ListNodeStructFields(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 NodeIndex)
{
    TArray<FString> Out;
    if (AssetPath.IsEmpty() || NodeArray.IsEmpty()) return Out;
    UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *AssetPath));
    if (!StateTree) return Out;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return Out;

    TArray<FStateTreeEditorNode>* NodeArrayPtr = nullptr;
    UStateTreeState* TargetState = nullptr;
    if (NodeArray.Equals(TEXT("GlobalTasks"), ESearchCase::IgnoreCase))
    {
        NodeArrayPtr = &EditorData->GlobalTasks;
    }
    else
    {
        if (StateName.IsEmpty()) return Out;
        TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
        FindInState = [&](UStateTreeState* Node)->UStateTreeState*
        {
            if (!Node) return nullptr;
            if (Node->Name.ToString() == StateName) return Node;
            for (TObjectPtr<UStateTreeState> Child : Node->Children)
            {
                UStateTreeState* Found = FindInState(Child);
                if (Found) return Found;
            }
            return nullptr;
        };
        for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
        {
            UStateTreeState* Found = FindInState(ST);
            if (Found) { TargetState = Found; break; }
        }
        if (!TargetState) return Out;
        if (NodeArray.Equals(TEXT("Tasks"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->Tasks;
        else if (NodeArray.Equals(TEXT("EnterConditions"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->EnterConditions;
        else if (NodeArray.Equals(TEXT("Considerations"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->Considerations;
        else return Out;
    }

    if (!NodeArrayPtr) return Out;
    if (NodeIndex < 0 || NodeIndex >= NodeArrayPtr->Num()) return Out;

    FStateTreeEditorNode& Node = (*NodeArrayPtr)[NodeIndex];
    const UScriptStruct* SS = Node.Instance.GetScriptStruct();
    if (!SS) SS = Node.Node.GetScriptStruct();
    if (!SS) return Out;

    for (TFieldIterator<FProperty> It((UStruct*)SS); It; ++It)
    {
        FProperty* P = *It;
        if (P) Out.Add(P->GetName());
    }
    return Out;
}

bool UStateTreeService::MoveNodeToState(const FString& AssetPath, const FString& FromStateName, const FString& NodeArray, int32 NodeIndex, const FString& ToStateName)
{
    if (AssetPath.IsEmpty() || FromStateName.IsEmpty() || ToStateName.IsEmpty() || NodeArray.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    auto FindState = [&](const FString& Name)->UStateTreeState*
    {
        TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
        FindInState = [&](UStateTreeState* Node)->UStateTreeState*
        {
            if (!Node) return nullptr;
            if (Node->Name.ToString() == Name) return Node;
            for (TObjectPtr<UStateTreeState> Child : Node->Children)
            {
                UStateTreeState* Found = FindInState(Child);
                if (Found) return Found;
            }
            return nullptr;
        };
        for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
        {
            UStateTreeState* Found = FindInState(ST);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* FromState = FindState(FromStateName);
    UStateTreeState* ToState = FindState(ToStateName);
    if (!FromState || !ToState) return false;

    TArray<FStateTreeEditorNode>* FromArray = nullptr;
    if (NodeArray.Equals(TEXT("Tasks"), ESearchCase::IgnoreCase)) FromArray = &FromState->Tasks;
    else if (NodeArray.Equals(TEXT("EnterConditions"), ESearchCase::IgnoreCase)) FromArray = &FromState->EnterConditions;
    else if (NodeArray.Equals(TEXT("Considerations"), ESearchCase::IgnoreCase)) FromArray = &FromState->Considerations;
    else return false;

    if (NodeIndex < 0 || NodeIndex >= FromArray->Num()) return false;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::MoveNodeToState")));
    FromState->Modify(false);
    ToState->Modify(false);

    FStateTreeEditorNode NodeCopy = (*FromArray)[NodeIndex];
    NodeCopy.ID = FGuid::NewGuid();
    // Append to destination
    if (NodeArray.Equals(TEXT("Tasks"), ESearchCase::IgnoreCase)) ToState->Tasks.Add(NodeCopy);
    else if (NodeArray.Equals(TEXT("EnterConditions"), ESearchCase::IgnoreCase)) ToState->EnterConditions.Add(NodeCopy);
    else if (NodeArray.Equals(TEXT("Considerations"), ESearchCase::IgnoreCase)) ToState->Considerations.Add(NodeCopy);

    // Remove from source
    FromArray->RemoveAt(NodeIndex);

    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
}

bool UStateTreeService::CopySubTreeToNewName(const FString& AssetPath, const FString& SubTreeName, const FString& NewName)
{
    // Simple wrapper around DuplicateSubTree which duplicates and renames the subtree.
    return DuplicateSubTree(AssetPath, SubTreeName, NewName);
}
#endif
#include "EditorAssetLibrary.h"
#include "JsonObjectConverter.h"
#include "UObject/UnrealType.h"
#include "StateTreeState.h"
#if WITH_EDITOR
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonTypes.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializerMacros.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UObjectIterator.h"
#include "ScopedTransaction.h"
#endif

bool UStateTreeService::EnsureEditorData(const FString& AssetPath)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree)
    {
        UE_LOG(LogTemp, Warning, TEXT("UStateTreeService: Failed to load StateTree: %s"), *AssetPath);
        return false;
    }

    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData)
    {
        EditorData = NewObject<UStateTreeEditorData>(StateTree, NAME_None, RF_Transactional);
        if (!EditorData)
        {
            UE_LOG(LogTemp, Warning, TEXT("UStateTreeService: Failed to create EditorData for %s"), *AssetPath);
            return false;
        }
        StateTree->EditorData = EditorData;
    }
    return true;
#else
    UE_LOG(LogTemp, Warning, TEXT("UStateTreeService::EnsureEditorData is editor-only"));
    return false;
#endif
}

bool UStateTreeService::AddSubTree(const FString& AssetPath, const FString& SubTreeName)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || SubTreeName.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree)
    {
        UE_LOG(LogTemp, Warning, TEXT("UStateTreeService: Failed to load StateTree: %s"), *AssetPath);
        return false;
    }

    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData)
    {
        EditorData = NewObject<UStateTreeEditorData>(StateTree, NAME_None, RF_Transactional);
        StateTree->EditorData = EditorData;
    }

    if (GEditor)
    {
        GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::AddSubTree")));
    }

    EditorData->Modify(false);
    EditorData->AddSubTree(FName(*SubTreeName));

    if (GEditor)
    {
        GEditor->EndTransaction();
    }

    // Try to validate via editing subsystem if available
    if (GEditor)
    {
        if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
        {
            EditingSubsystem->ValidateStateTree(StateTree);
        }
    }

    // Mark package dirty
    UPackage* Package = StateTree->GetOutermost();
    if (Package)
    {
        Package->SetDirtyFlag(true);
    }

    return true;
#else
    UE_LOG(LogTemp, Warning, TEXT("UStateTreeService::AddSubTree is editor-only"));
    return false;
#endif
}

TArray<FString> UStateTreeService::ListSubTrees(const FString& AssetPath)
{
    TArray<FString> Out;
#if WITH_EDITOR
    if (AssetPath.IsEmpty()) return Out;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return Out;
    if (UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData))
    {
        for (const auto& ST : EditorData->SubTrees)
        {
            Out.Add(ST->Name.ToString());
        }
    }
#else
    UE_LOG(LogTemp, Warning, TEXT("UStateTreeService::ListSubTrees is editor-only"));
#endif
    return Out;
}

bool UStateTreeService::SaveAsset(const FString& AssetPath)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty()) return false;
    UObject* Obj = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Obj) return false;
    UPackage* Package = Obj->GetOutermost();
    if (!Package) return false;

    Package->SetDirtyFlag(true);
    FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());

    // Backup existing file
    if (FPaths::FileExists(PackageFileName))
    {
        int64 Now = FDateTime::Now().ToUnixTimestamp();
        FString BackupName = FString::Printf(TEXT("%s.bak.%lld"), *PackageFileName, Now);
        IFileManager::Get().Copy(*BackupName, *PackageFileName);
    }

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Standalone;
    SaveArgs.Error = GError;
    bool bSaved = UPackage::SavePackage(Package, Obj, *PackageFileName, SaveArgs);
    return bSaved;
#else
    UE_LOG(LogTemp, Warning, TEXT("UStateTreeService::SaveAsset is editor-only"));
    return false;
#endif
}

bool UStateTreeService::MoveSubTreeUnderState(const FString& AssetPath, const FString& SubTreeName, const FString& ParentStateName)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || SubTreeName.IsEmpty() || ParentStateName.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree)
    {
        UE_LOG(LogTemp, Warning, TEXT("UStateTreeService: Failed to load StateTree: %s"), *AssetPath);
        return false;
    }

    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData)
    {
        EditorData = NewObject<UStateTreeEditorData>(StateTree, NAME_None, RF_Transactional);
        StateTree->EditorData = EditorData;
    }

    if (GEditor)
    {
        GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::MoveSubTreeUnderState")));
    }

    EditorData->Modify(false);

    // Helper: recursive find state by name in the subtree tree
    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == ParentStateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* ParentState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found)
        {
            ParentState = Found;
            break;
        }
    }

    if (!ParentState)
    {
        UE_LOG(LogTemp, Warning, TEXT("UStateTreeService: Could not find parent state '%s'"), *ParentStateName);
        if (GEditor) GEditor->EndTransaction();
        return false;
    }

    // Create a new child under the parent using the same name.
    ParentState->Modify(false);
    ParentState->AddChildState(FName(*SubTreeName));

    // Remove any existing root subtree with the same name to avoid duplicate root entries.
    for (int32 i = EditorData->SubTrees.Num() - 1; i >= 0; --i)
    {
        UStateTreeState* ST = EditorData->SubTrees[i];
        if (ST && ST->Name.ToString() == SubTreeName)
        {
            EditorData->SubTrees.RemoveAt(i);
        }
    }

    if (GEditor)
    {
        GEditor->EndTransaction();
    }

    // Try to validate via editing subsystem if available
    if (GEditor)
    {
        if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
        {
            EditingSubsystem->ValidateStateTree(StateTree);
        }
    }

    // Mark package dirty
    UPackage* Package = StateTree->GetOutermost();
    if (Package)
    {
        Package->SetDirtyFlag(true);
    }

    return true;
#else
    UE_LOG(LogTemp, Warning, TEXT("UStateTreeService::MoveSubTreeUnderState is editor-only"));
    return false;
#endif
}

bool UStateTreeService::SetStateParameter(const FString& AssetPath, const FString& StateName, const FString& ParamName, const FString& Value)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || ParamName.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree)
    {
        UE_LOG(LogTemp, Warning, TEXT("UStateTreeService: Failed to load StateTree: %s"), *AssetPath);
        return false;
    }

    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData)
    {
        UE_LOG(LogTemp, Warning, TEXT("UStateTreeService::SetStateParameter: no EditorData on %s"), *AssetPath);
        return false;
    }

    // recursive find state
    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found)
        {
            TargetState = Found;
            break;
        }
    }

    if (!TargetState)
    {
        UE_LOG(LogTemp, Warning, TEXT("UStateTreeService: Could not find state '%s'"), *StateName);
        return false;
    }

    if (GEditor)
    {
        GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::SetStateParameter")));
    }

    TargetState->Modify(false);

    // Find property on UStateTreeState
    FProperty* Prop = FindFProperty<FProperty>(UStateTreeState::StaticClass(), *ParamName);
    if (!Prop)
    {
        UE_LOG(LogTemp, Warning, TEXT("UStateTreeService: Property %s not found on UStateTreeState"), *ParamName);
        if (GEditor) GEditor->EndTransaction();
        return false;
    }

    void* Dest = Prop->ContainerPtrToValuePtr<void>(TargetState);
    if (!Dest)
    {
        UE_LOG(LogTemp, Warning, TEXT("UStateTreeService: Failed to get pointer to property %s"), *ParamName);
        if (GEditor) GEditor->EndTransaction();
        return false;
    }

    // Support common property types by parsing the string and assigning via typed setters.
    if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
    {
        bool b = (Value.ToLower() == TEXT("true") || Value == TEXT("1"));
        BoolProp->SetPropertyValue_InContainer(TargetState, b);
    }
    else if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
    {
        int32 V = FCString::Atoi(*Value);
        IntProp->SetPropertyValue_InContainer(TargetState, V);
    }
    else if (FInt64Property* Int64Prop = CastField<FInt64Property>(Prop))
    {
        int64 V = FCString::Atoi64(*Value);
        Int64Prop->SetPropertyValue_InContainer(TargetState, V);
    }
    else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
    {
        float V = FCString::Atof(*Value);
        FloatProp->SetPropertyValue_InContainer(TargetState, V);
    }
    else if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
    {
        FName N(*Value);
        NameProp->SetPropertyValue_InContainer(TargetState, N);
    }
    else if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
    {
        StrProp->SetPropertyValue_InContainer(TargetState, Value);
    }
    else if (FTextProperty* TextProp = CastField<FTextProperty>(Prop))
    {
        TextProp->SetPropertyValue_InContainer(TargetState, FText::FromString(Value));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("UStateTreeService: Unsupported property type for %s"), *ParamName);
        if (GEditor) GEditor->EndTransaction();
        return false;
    }

    // Mark package dirty and validate
    UPackage* Package = StateTree->GetOutermost();
    if (Package)
    {
        Package->SetDirtyFlag(true);
    }

    if (GEditor)
    {
        GEditor->EndTransaction();
    }

    if (GEditor)
    {
        if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
        {
            EditingSubsystem->ValidateStateTree(StateTree);
        }
    }

    return true;
#else
    UE_LOG(LogTemp, Warning, TEXT("UStateTreeService::SetStateParameter is editor-only"));
    return false;
#endif
}

bool UStateTreeService::AddTaskToState(const FString& AssetPath, const FString& StateName, const FString& TaskStructPath)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || TaskStructPath.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree)
    {
        UE_LOG(LogTemp, Warning, TEXT("UStateTreeService: Failed to load StateTree: %s"), *AssetPath);
        return false;
    }

    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData)
    {
        EditorData = NewObject<UStateTreeEditorData>(StateTree, NAME_None, RF_Transactional);
        StateTree->EditorData = EditorData;
    }

    // Find state by name
    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found)
        {
            TargetState = Found;
            break;
        }
    }

    if (!TargetState)
    {
        UE_LOG(LogTemp, Warning, TEXT("UStateTreeService: Could not find state '%s'"), *StateName);
        return false;
    }

    if (GEditor)
    {
        GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::AddTaskToState")));
    }

    TargetState->Modify(false);

    // Resolve the script struct for the task
    const FString& Path = TaskStructPath;
    UScriptStruct* TaskStruct = FindObject<UScriptStruct>(nullptr, *Path);
    if (!TaskStruct)
    {
        TaskStruct = LoadObject<UScriptStruct>(nullptr, *Path);
    }
    if (!TaskStruct)
    {
        UE_LOG(LogTemp, Warning, TEXT("UStateTreeService: Could not find UScriptStruct at path '%s'"), *Path);
        if (GEditor) GEditor->EndTransaction();
        return false;
    }

    // Append new editor node
    FStateTreeEditorNode& NewNode = TargetState->Tasks.AddDefaulted_GetRef();
    NewNode.ID = FGuid::NewGuid();
    NewNode.Node.InitializeAs(TaskStruct);

    // Initialize instance and execution runtime data if the node declares them
    const FStateTreeNodeBase& NodeBase = NewNode.Node.GetMutable<FStateTreeNodeBase>();
    if (const UStruct* InstanceType = NodeBase.GetInstanceDataType())
    {
        if (const UScriptStruct* InstSS = Cast<const UScriptStruct>(InstanceType))
        {
            NewNode.Instance.InitializeAs(InstSS);
        }
    }
    if (const UStruct* ExecType = NodeBase.GetExecutionRuntimeDataType())
    {
        if (const UScriptStruct* ExecSS = Cast<const UScriptStruct>(ExecType))
        {
            NewNode.ExecutionRuntimeData.InitializeAs(ExecSS);
        }
    }

    // Mark package dirty, validate and save
    UPackage* Package = StateTree->GetOutermost();
    if (Package)
    {
        Package->SetDirtyFlag(true);
    }

    if (GEditor)
    {
        GEditor->EndTransaction();
    }

    if (GEditor)
    {
        if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
        {
            EditingSubsystem->ValidateStateTree(StateTree);
        }
    }

    // Save asset
    SaveAsset(AssetPath);

    return true;
#else
    UE_LOG(LogTemp, Warning, TEXT("UStateTreeService::AddTaskToState is editor-only"));
    return false;
#endif
}

bool UStateTreeService::AddConditionToState(const FString& AssetPath, const FString& StateName, const FString& ConditionStructPath)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || ConditionStructPath.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;

    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found)
        {
            TargetState = Found;
            break;
        }
    }
    if (!TargetState) return false;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::AddConditionToState")));
    TargetState->Modify(false);

    UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *ConditionStructPath);
    if (!Struct) Struct = LoadObject<UScriptStruct>(nullptr, *ConditionStructPath);
    if (!Struct)
    {
        if (GEditor) GEditor->EndTransaction();
        return false;
    }

    FStateTreeEditorNode& NewNode = TargetState->EnterConditions.AddDefaulted_GetRef();
    NewNode.ID = FGuid::NewGuid();
    NewNode.Node.InitializeAs(Struct);
    const FStateTreeNodeBase& NodeBase = NewNode.Node.GetMutable<FStateTreeNodeBase>();
    if (const UStruct* InstanceType = NodeBase.GetInstanceDataType())
    {
        if (const UScriptStruct* InstSS = Cast<const UScriptStruct>(InstanceType))
        {
            NewNode.Instance.InitializeAs(InstSS);
        }
    }
    if (const UStruct* ExecType = NodeBase.GetExecutionRuntimeDataType())
    {
        if (const UScriptStruct* ExecSS = Cast<const UScriptStruct>(ExecType))
        {
            NewNode.ExecutionRuntimeData.InitializeAs(ExecSS);
        }
    }

    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    UPackage* Package = StateTree->GetOutermost(); if (Package) Package->SetDirtyFlag(true);
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
#else
    return false;
#endif
}

bool UStateTreeService::AddConsiderationToState(const FString& AssetPath, const FString& StateName, const FString& ConsiderationStructPath)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || ConsiderationStructPath.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;

    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found)
        {
            TargetState = Found;
            break;
        }
    }
    if (!TargetState) return false;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::AddConsiderationToState")));
    TargetState->Modify(false);

    UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *ConsiderationStructPath);
    if (!Struct) Struct = LoadObject<UScriptStruct>(nullptr, *ConsiderationStructPath);
    if (!Struct)
    {
        if (GEditor) GEditor->EndTransaction();
        return false;
    }

    FStateTreeEditorNode& NewNode = TargetState->Considerations.AddDefaulted_GetRef();
    NewNode.ID = FGuid::NewGuid();
    NewNode.Node.InitializeAs(Struct);
    const FStateTreeNodeBase& NodeBase = NewNode.Node.GetMutable<FStateTreeNodeBase>();
    if (const UStruct* InstanceType = NodeBase.GetInstanceDataType())
    {
        if (const UScriptStruct* InstSS = Cast<const UScriptStruct>(InstanceType))
        {
            NewNode.Instance.InitializeAs(InstSS);
        }
    }
    if (const UStruct* ExecType = NodeBase.GetExecutionRuntimeDataType())
    {
        if (const UScriptStruct* ExecSS = Cast<const UScriptStruct>(ExecType))
        {
            NewNode.ExecutionRuntimeData.InitializeAs(ExecSS);
        }
    }

    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    UPackage* Package = StateTree->GetOutermost(); if (Package) Package->SetDirtyFlag(true);
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
#else
    return false;
#endif
}

bool UStateTreeService::AddGlobalTask(const FString& AssetPath, const FString& TaskStructPath)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || TaskStructPath.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;

    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData)
    {
        EditorData = NewObject<UStateTreeEditorData>(StateTree, NAME_None, RF_Transactional);
        StateTree->EditorData = EditorData;
    }

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::AddGlobalTask")));
    EditorData->Modify(false);

    UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *TaskStructPath);
    if (!Struct) Struct = LoadObject<UScriptStruct>(nullptr, *TaskStructPath);
    if (!Struct)
    {
        if (GEditor) GEditor->EndTransaction();
        return false;
    }

    FStateTreeEditorNode& NewNode = EditorData->GlobalTasks.AddDefaulted_GetRef();
    NewNode.ID = FGuid::NewGuid();
    NewNode.Node.InitializeAs(Struct);
    const FStateTreeNodeBase& NodeBase = NewNode.Node.GetMutable<FStateTreeNodeBase>();
    if (const UStruct* InstanceType = NodeBase.GetInstanceDataType())
    {
        if (const UScriptStruct* InstSS = Cast<const UScriptStruct>(InstanceType))
        {
            NewNode.Instance.InitializeAs(InstSS);
        }
    }
    if (const UStruct* ExecType = NodeBase.GetExecutionRuntimeDataType())
    {
        if (const UScriptStruct* ExecSS = Cast<const UScriptStruct>(ExecType))
        {
            NewNode.ExecutionRuntimeData.InitializeAs(ExecSS);
        }
    }

    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    UPackage* Package = StateTree->GetOutermost(); if (Package) Package->SetDirtyFlag(true);
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
#else
    return false;
#endif
}

bool UStateTreeService::LinkSubTreeToState(const FString& AssetPath, const FString& StateName, const FString& SubTreeName)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || SubTreeName.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    // find parent state
    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found)
        {
            TargetState = Found;
            break;
        }
    }
    if (!TargetState) return false;

    // find subtree by name
    UStateTreeState* SubTreeState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        if (ST && ST->Name.ToString() == SubTreeName)
        {
            SubTreeState = ST;
            break;
        }
    }
    if (!SubTreeState) return false;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::LinkSubTreeToState")));
    TargetState->Modify(false);
    TargetState->LinkedSubtree.Name = SubTreeState->Name;
    TargetState->LinkedSubtree.ID = SubTreeState->ID;

    UPackage* Package = StateTree->GetOutermost(); if (Package) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
#else
    return false;
#endif
}

TArray<FString> UStateTreeService::ListStatesDetailed(const FString& AssetPath)
{
    TArray<FString> Out;
#if WITH_EDITOR
    if (AssetPath.IsEmpty()) return Out;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return Out;
    if (UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData))
    {
        TFunction<void(UStateTreeState*, const FString&)> DFS = [&](UStateTreeState* Node, const FString& Path)
        {
            if (!Node) return;
            FString MyPath = Path.IsEmpty() ? Node->Name.ToString() : (Path + TEXT("/") + Node->Name.ToString());
            Out.Add(MyPath);
            for (TObjectPtr<UStateTreeState> Child : Node->Children)
            {
                DFS(Child, MyPath);
            }
        };
        for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
        {
            DFS(ST, TEXT(""));
        }
    }
#endif
    return Out;
}

TArray<FString> UStateTreeService::GetStateDetails(const FString& AssetPath, const FString& StateName)
{
    TArray<FString> Out;
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || StateName.IsEmpty()) return Out;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return Out;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return Out;

    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found)
        {
            TargetState = Found;
            break;
        }
    }
    if (!TargetState) return Out;

    Out.Add(FString::Printf(TEXT("Name=%s"), *TargetState->Name.ToString()));
    Out.Add(FString::Printf(TEXT("Description=%s"), *TargetState->Description));
    Out.Add(FString::Printf(TEXT("Tag=%s"), *TargetState->Tag.ToString()));
    Out.Add(FString::Printf(TEXT("Enabled=%d"), TargetState->bEnabled ? 1 : 0));
    Out.Add(FString::Printf(TEXT("Children=%d"), TargetState->Children.Num()));
    Out.Add(FString::Printf(TEXT("Tasks=%d"), TargetState->Tasks.Num()));
    Out.Add(FString::Printf(TEXT("EnterConditions=%d"), TargetState->EnterConditions.Num()));
    Out.Add(FString::Printf(TEXT("Considerations=%d"), TargetState->Considerations.Num()));
    FString Linked = TargetState->LinkedSubtree.Name.IsNone() ? TEXT("(none)") : TargetState->LinkedSubtree.Name.ToString();
    Out.Add(FString::Printf(TEXT("LinkedSubtree=%s"), *Linked));
#endif
    return Out;
}

bool UStateTreeService::DeleteSubTree(const FString& AssetPath, const FString& SubTreeName)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || SubTreeName.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::DeleteSubTree")));
    EditorData->Modify(false);
    for (int32 i = EditorData->SubTrees.Num() - 1; i >= 0; --i)
    {
        UStateTreeState* ST = EditorData->SubTrees[i];
        if (ST && ST->Name.ToString() == SubTreeName)
        {
            EditorData->SubTrees.RemoveAt(i);
        }
    }

    if (GEditor) GEditor->EndTransaction();
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    SaveAsset(AssetPath);
    return true;
#else
    return false;
#endif
}

bool UStateTreeService::DeleteState(const FString& AssetPath, const FString& StateName)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || StateName.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::DeleteState")));
    EditorData->Modify(false);

    // Helper to remove child with name from a parent node
    TFunction<bool(UStateTreeState*)> RemoveFromTree;
    RemoveFromTree = [&](UStateTreeState* Node)->bool
    {
        if (!Node) return false;
        for (int32 i = Node->Children.Num() - 1; i >= 0; --i)
        {
            if (Node->Children[i] && Node->Children[i]->Name.ToString() == StateName)
            {
                Node->Modify(false);
                Node->Children.RemoveAt(i);
                return true;
            }
            if (RemoveFromTree(Node->Children[i])) return true;
        }
        return false;
    };

    // Try remove from roots first
    for (int32 i = EditorData->SubTrees.Num() - 1; i >= 0; --i)
    {
        UStateTreeState* ST = EditorData->SubTrees[i];
        if (ST && ST->Name.ToString() == StateName)
        {
            EditorData->SubTrees.RemoveAt(i);
            if (GEditor) GEditor->EndTransaction();
            if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
            {
                EditingSubsystem->ValidateStateTree(StateTree);
            }
            if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
            SaveAsset(AssetPath);
            return true;
        }
        if (RemoveFromTree(ST))
        {
            if (GEditor) GEditor->EndTransaction();
            if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
            {
                EditingSubsystem->ValidateStateTree(StateTree);
            }
            if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
            SaveAsset(AssetPath);
            return true;
        }
    }

    if (GEditor) GEditor->EndTransaction();
    return false;
#else
    return false;
#endif
}

bool UStateTreeService::RenameState(const FString& AssetPath, const FString& OldName, const FString& NewName)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || OldName.IsEmpty() || NewName.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == OldName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found)
        {
            TargetState = Found;
            break;
        }
    }
    if (!TargetState) return false;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::RenameState")));
    TargetState->Modify(false);
    TargetState->Name = FName(*NewName);
    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
#else
    return false;
#endif
}

bool UStateTreeService::RemoveTaskFromState(const FString& AssetPath, const FString& StateName, int32 TaskIndex)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || TaskIndex < 0) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found)
        {
            TargetState = Found;
            break;
        }
    }
    if (!TargetState) return false;

    if (TaskIndex >= TargetState->Tasks.Num()) return false;
    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::RemoveTaskFromState")));
    TargetState->Modify(false);
    TargetState->Tasks.RemoveAt(TaskIndex);
    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
#else
    return false;
#endif
}

bool UStateTreeService::MoveTaskIndex(const FString& AssetPath, const FString& StateName, int32 FromIndex, int32 ToIndex)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || StateName.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found)
        {
            TargetState = Found;
            break;
        }
    }
    if (!TargetState) return false;

    int32 Count = TargetState->Tasks.Num();
    if (FromIndex < 0 || FromIndex >= Count || ToIndex < 0 || ToIndex >= Count) return false;
    if (FromIndex == ToIndex) return true;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::MoveTaskIndex")));
    TargetState->Modify(false);
    FStateTreeEditorNode NodeCopy = TargetState->Tasks[FromIndex];
    TargetState->Tasks.RemoveAt(FromIndex);
    TargetState->Tasks.Insert(NodeCopy, ToIndex);

    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
#else
    return false;
#endif
}

bool UStateTreeService::SetNodeParameter(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 NodeIndex, const FString& ParamName, const FString& Value)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || ParamName.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    // locate node reference array
    TArray<FStateTreeEditorNode>* NodeArrayPtr = nullptr;
    UStateTreeState* TargetState = nullptr;
    if (NodeArray.Equals(TEXT("GlobalTasks"), ESearchCase::IgnoreCase))
    {
        NodeArrayPtr = &EditorData->GlobalTasks;
    }
    else
    {
        if (StateName.IsEmpty()) return false;
        TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
        FindInState = [&](UStateTreeState* Node)->UStateTreeState*
        {
            if (!Node) return nullptr;
            if (Node->Name.ToString() == StateName) return Node;
            for (TObjectPtr<UStateTreeState> Child : Node->Children)
            {
                UStateTreeState* Found = FindInState(Child);
                if (Found) return Found;
            }
            return nullptr;
        };
        for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
        {
            UStateTreeState* Found = FindInState(ST);
            if (Found)
            {
                TargetState = Found;
                break;
            }
        }
        if (!TargetState) return false;
        if (NodeArray.Equals(TEXT("Tasks"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->Tasks;
        else if (NodeArray.Equals(TEXT("EnterConditions"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->EnterConditions;
        else if (NodeArray.Equals(TEXT("Considerations"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->Considerations;
        else return false;
    }

    if (!NodeArrayPtr) return false;
    if (NodeIndex < 0 || NodeIndex >= NodeArrayPtr->Num()) return false;

    FStateTreeEditorNode& Node = (*NodeArrayPtr)[NodeIndex];

    // Attempt to set property on Instance (if present) otherwise Node.Node (struct)
    const UScriptStruct* SS = Node.Instance.GetScriptStruct();
    void* ContainerPtr = Node.Instance.GetMutableMemory();
    if (!ContainerPtr)
    {
        // fallback to node struct
        SS = Node.Node.GetScriptStruct();
        ContainerPtr = Node.Node.GetMutableMemory();
    }
    if (!SS || !ContainerPtr)
    {
        return false;
    }

    FProperty* Prop = FindFProperty<FProperty>(SS, *ParamName);
    if (!Prop) return false;

    if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
    {
        bool b = (Value.ToLower() == TEXT("true") || Value == TEXT("1"));
        BoolProp->SetPropertyValue_InContainer(ContainerPtr, b);
    }
    else if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
    {
        int32 V = FCString::Atoi(*Value);
        IntProp->SetPropertyValue_InContainer(ContainerPtr, V);
    }
    else if (FInt64Property* Int64Prop = CastField<FInt64Property>(Prop))
    {
        int64 V = FCString::Atoi64(*Value);
        Int64Prop->SetPropertyValue_InContainer(ContainerPtr, V);
    }
    else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
    {
        float V = FCString::Atof(*Value);
        FloatProp->SetPropertyValue_InContainer(ContainerPtr, V);
    }
    else if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
    {
        FName N(*Value);
        NameProp->SetPropertyValue_InContainer(ContainerPtr, N);
    }
    else if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
    {
        StrProp->SetPropertyValue_InContainer(ContainerPtr, Value);
    }
    else if (FTextProperty* TextProp = CastField<FTextProperty>(Prop))
    {
        TextProp->SetPropertyValue_InContainer(ContainerPtr, FText::FromString(Value));
    }
    else
    {
        return false;
    }

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::SetNodeParameter")));
    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
#else
    return false;
#endif
}

TArray<FString> UStateTreeService::DiscoverScriptStructs(const FString& Filter)
{
    TArray<FString> Out;
#if WITH_EDITOR
    for (TObjectIterator<UScriptStruct> It; It; ++It)
    {
        UScriptStruct* SS = *It;
        if (!SS) continue;
        FString Path = SS->GetPathName();
        if (Filter.IsEmpty() || Path.Contains(Filter))
        {
            Out.Add(Path);
        }
    }
#endif
    return Out;
}

FString UStateTreeService::GetNodeStructAsJson(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 NodeIndex)
{
#if WITH_EDITOR
    FString Empty;
    if (AssetPath.IsEmpty() || NodeArray.IsEmpty()) return Empty;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return Empty;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return Empty;

    TArray<FStateTreeEditorNode>* NodeArrayPtr = nullptr;
    UStateTreeState* TargetState = nullptr;
    if (NodeArray.Equals(TEXT("GlobalTasks"), ESearchCase::IgnoreCase))
    {
        NodeArrayPtr = &EditorData->GlobalTasks;
    }
    else
    {
        if (StateName.IsEmpty()) return Empty;
        TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
        FindInState = [&](UStateTreeState* Node)->UStateTreeState*
        {
            if (!Node) return nullptr;
            if (Node->Name.ToString() == StateName) return Node;
            for (TObjectPtr<UStateTreeState> Child : Node->Children)
            {
                UStateTreeState* Found = FindInState(Child);
                if (Found) return Found;
            }
            return nullptr;
        };
        for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
        {
            UStateTreeState* Found = FindInState(ST);
            if (Found)
            {
                TargetState = Found;
                break;
            }
        }
        if (!TargetState) return Empty;
        if (NodeArray.Equals(TEXT("Tasks"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->Tasks;
        else if (NodeArray.Equals(TEXT("EnterConditions"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->EnterConditions;
        else if (NodeArray.Equals(TEXT("Considerations"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->Considerations;
        else return Empty;
    }

    if (!NodeArrayPtr) return Empty;
    if (NodeIndex < 0 || NodeIndex >= NodeArrayPtr->Num()) return Empty;

    FStateTreeEditorNode& Node = (*NodeArrayPtr)[NodeIndex];

    // Prefer Instance then Node struct
    const UScriptStruct* SS = Node.Instance.GetScriptStruct();
    void* ContainerPtr = Node.Instance.GetMutableMemory();
    if (!ContainerPtr)
    {
        SS = Node.Node.GetScriptStruct();
        ContainerPtr = Node.Node.GetMutableMemory();
    }
    if (!SS || !ContainerPtr) return Empty;

    FString JsonString;
    if (!FJsonObjectConverter::UStructToJsonObjectString(SS, ContainerPtr, JsonString, 0, 0))
    {
        return Empty;
    }
    return JsonString;
#else
    return FString();
#endif
}

bool UStateTreeService::SetNodeStructFromJson(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 NodeIndex, const FString& JsonString)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || NodeArray.IsEmpty() || JsonString.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    TArray<FStateTreeEditorNode>* NodeArrayPtr = nullptr;
    UStateTreeState* TargetState = nullptr;
    if (NodeArray.Equals(TEXT("GlobalTasks"), ESearchCase::IgnoreCase))
    {
        NodeArrayPtr = &EditorData->GlobalTasks;
    }
    else
    {
        if (StateName.IsEmpty()) return false;
        TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
        FindInState = [&](UStateTreeState* Node)->UStateTreeState*
        {
            if (!Node) return nullptr;
            if (Node->Name.ToString() == StateName) return Node;
            for (TObjectPtr<UStateTreeState> Child : Node->Children)
            {
                UStateTreeState* Found = FindInState(Child);
                if (Found) return Found;
            }
            return nullptr;
        };
        for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
        {
            UStateTreeState* Found = FindInState(ST);
            if (Found)
            {
                TargetState = Found;
                break;
            }
        }
        if (!TargetState) return false;
        if (NodeArray.Equals(TEXT("Tasks"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->Tasks;
        else if (NodeArray.Equals(TEXT("EnterConditions"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->EnterConditions;
        else if (NodeArray.Equals(TEXT("Considerations"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->Considerations;
        else return false;
    }

    if (!NodeArrayPtr) return false;
    if (NodeIndex < 0 || NodeIndex >= NodeArrayPtr->Num()) return false;

    FStateTreeEditorNode& Node = (*NodeArrayPtr)[NodeIndex];
    const UScriptStruct* SS = Node.Instance.GetScriptStruct();
    void* ContainerPtr = Node.Instance.GetMutableMemory();
    if (!ContainerPtr)
    {
        SS = Node.Node.GetScriptStruct();
        ContainerPtr = Node.Node.GetMutableMemory();
    }
    if (!SS || !ContainerPtr) return false;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::SetNodeStructFromJson")));
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        if (GEditor) GEditor->EndTransaction();
        return false;
    }
    bool bOk = FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), SS, ContainerPtr, 0, 0);
    if (!bOk)
    {
        if (GEditor) GEditor->EndTransaction();
        return false;
    }

    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
#else
    return false;
#endif
}

FString UStateTreeService::GetPropertyAsJson(const FString& AssetPath, const FString& StateName, const FString& PropertyName)
{
#if WITH_EDITOR
    FString Empty;
    if (AssetPath.IsEmpty() || PropertyName.IsEmpty()) return Empty;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return Empty;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return Empty;

    // find state by name
    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    if (!StateName.IsEmpty())
    {
        for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
        {
            UStateTreeState* Found = FindInState(ST);
            if (Found)
            {
                TargetState = Found;
                break;
            }
        }
    }
    if (!TargetState) return Empty;

    FProperty* Prop = FindFProperty<FProperty>(UStateTreeState::StaticClass(), *PropertyName);
    if (!Prop) return Empty;
    if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
    {
        void* Dest = StructProp->ContainerPtrToValuePtr<void>(TargetState);
        if (!Dest) return Empty;
        FString JsonString;
        if (FJsonObjectConverter::UStructToJsonObjectString(StructProp->Struct, Dest, JsonString, 0, 0))
        {
            return JsonString;
        }
    }
    return Empty;
#else
    return FString();
#endif
}

bool UStateTreeService::SetPropertyFromJson(const FString& AssetPath, const FString& StateName, const FString& PropertyName, const FString& JsonString)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || PropertyName.IsEmpty() || JsonString.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    // find state by name
    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    if (!StateName.IsEmpty())
    {
        for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
        {
            UStateTreeState* Found = FindInState(ST);
            if (Found)
            {
                TargetState = Found;
                break;
            }
        }
    }
    if (!TargetState) return false;

    FProperty* Prop = FindFProperty<FProperty>(UStateTreeState::StaticClass(), *PropertyName);
    if (!Prop) return false;
    if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
    {
        void* Dest = StructProp->ContainerPtrToValuePtr<void>(TargetState);
        if (!Dest) return false;

        if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::SetPropertyFromJson")));
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
        if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
        {
            if (GEditor) GEditor->EndTransaction();
            return false;
        }
        bool bOk = FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), StructProp->Struct, Dest, 0, 0);
        if (!bOk)
        {
            if (GEditor) GEditor->EndTransaction();
            return false;
        }

        if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
        if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
        {
            EditingSubsystem->ValidateStateTree(StateTree);
        }
        if (GEditor) GEditor->EndTransaction();
        SaveAsset(AssetPath);
        return true;
    }
    return false;
#else
    return false;
#endif
}

bool UStateTreeService::DuplicateSubTree(const FString& AssetPath, const FString& SubTreeName, const FString& NewName)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || SubTreeName.IsEmpty() || NewName.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        if (ST && ST->Name.ToString() == SubTreeName)
        {
            if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::DuplicateSubTree")));
            UStateTreeState* Copy = DuplicateObject<UStateTreeState>(ST, EditorData);
            if (!Copy) { if (GEditor) GEditor->EndTransaction(); return false; }
            Copy->Name = FName(*NewName);
            EditorData->Modify(false);
            EditorData->SubTrees.Add(Copy);
            if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
            if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
            {
                EditingSubsystem->ValidateStateTree(StateTree);
            }
            if (GEditor) GEditor->EndTransaction();
            SaveAsset(AssetPath);
            return true;
        }
    }
    return false;
#else
    return false;
#endif
}

bool UStateTreeService::DuplicateState(const FString& AssetPath, const FString& StateName, const FString& NewName)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || NewName.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    // find node and parent
    UStateTreeState* Parent = nullptr;
    UStateTreeState* FoundNode = nullptr;
    TFunction<bool(UStateTreeState*, UStateTreeState*)> FindWithParent;
    FindWithParent = [&](UStateTreeState* Node, UStateTreeState* ParentNode)->bool
    {
        if (!Node) return false;
        if (Node->Name.ToString() == StateName) { FoundNode = Node; Parent = ParentNode; return true; }
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            if (FindWithParent(Child, Node)) return true;
        }
        return false;
    };

    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        if (FindWithParent(ST, nullptr)) break;
    }
    if (!FoundNode) return false;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::DuplicateState")));
    UStateTreeState* Copy = DuplicateObject<UStateTreeState>(FoundNode, EditorData);
    if (!Copy) { if (GEditor) GEditor->EndTransaction(); return false; }
    Copy->Name = FName(*NewName);
    if (Parent)
    {
        Parent->Modify(false);
        Parent->Children.Add(Copy);
    }
    else
    {
        EditorData->Modify(false);
        EditorData->SubTrees.Add(Copy);
    }

    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
#else
    return false;
#endif
}

bool UStateTreeService::BeginBulkEdit(const FString& AssetPath, const FString& Reason)
{
#if WITH_EDITOR
    static TMap<FString, TUniquePtr<FScopedTransaction>> Transactions;
    if (!GEditor) return false;
    FString Key = AssetPath.IsEmpty() ? TEXT("__GLOBAL__") : AssetPath;
    if (Transactions.Contains(Key)) return true;
    FText Txt = FText::FromString(Reason.IsEmpty() ? TEXT("UStateTreeService::BulkEdit") : Reason);
    Transactions.Add(Key, MakeUnique<FScopedTransaction>(Txt));
    return true;
#else
    return false;
#endif
}

bool UStateTreeService::EndBulkEdit(const FString& AssetPath)
{
#if WITH_EDITOR
    static TMap<FString, TUniquePtr<FScopedTransaction>> Transactions;
    if (!GEditor) return false;
    FString Key = AssetPath.IsEmpty() ? TEXT("__GLOBAL__") : AssetPath;
    if (Transactions.Contains(Key))
    {
        Transactions.Remove(Key);
    }
    UStateTree* StateTree = nullptr;
    if (!AssetPath.IsEmpty()) StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (StateTree)
    {
        if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
        {
            EditingSubsystem->ValidateStateTree(StateTree);
        }
        if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
        SaveAsset(AssetPath);
    }
    return true;
#else
    return false;
#endif
}

bool UStateTreeService::RenameSubTree(const FString& AssetPath, const FString& OldName, const FString& NewName)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || OldName.IsEmpty() || NewName.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        if (ST && ST->Name.ToString() == OldName)
        {
            if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::RenameSubTree")));
            ST->Modify(false);
            ST->Name = FName(*NewName);
            if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
            if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
            {
                EditingSubsystem->ValidateStateTree(StateTree);
            }
            if (GEditor) GEditor->EndTransaction();
            SaveAsset(AssetPath);
            return true;
        }
    }
    return false;
#else
    return false;
#endif
}

bool UStateTreeService::RemoveNodeFromState(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 NodeIndex)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || NodeArray.IsEmpty() || NodeIndex < 0) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    TArray<FStateTreeEditorNode>* NodeArrayPtr = nullptr;
    UStateTreeState* TargetState = nullptr;
    if (NodeArray.Equals(TEXT("GlobalTasks"), ESearchCase::IgnoreCase))
    {
        NodeArrayPtr = &EditorData->GlobalTasks;
    }
    else
    {
        if (StateName.IsEmpty()) return false;
        TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
        FindInState = [&](UStateTreeState* Node)->UStateTreeState*
        {
            if (!Node) return nullptr;
            if (Node->Name.ToString() == StateName) return Node;
            for (TObjectPtr<UStateTreeState> Child : Node->Children)
            {
                UStateTreeState* Found = FindInState(Child);
                if (Found) return Found;
            }
            return nullptr;
        };
        for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
        {
            UStateTreeState* Found = FindInState(ST);
            if (Found)
            {
                TargetState = Found;
                break;
            }
        }
        if (!TargetState) return false;
        if (NodeArray.Equals(TEXT("Tasks"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->Tasks;
        else if (NodeArray.Equals(TEXT("EnterConditions"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->EnterConditions;
        else if (NodeArray.Equals(TEXT("Considerations"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->Considerations;
        else return false;
    }

    if (!NodeArrayPtr) return false;
    if (NodeIndex < 0 || NodeIndex >= NodeArrayPtr->Num()) return false;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::RemoveNodeFromState")));
    if (TargetState) TargetState->Modify(false); else EditorData->Modify(false);
    NodeArrayPtr->RemoveAt(NodeIndex);
    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
#else
    return false;
#endif
}

bool UStateTreeService::MoveNodeIndex(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 FromIndex, int32 ToIndex)
{
#if WITH_EDITOR
    if (AssetPath.IsEmpty() || NodeArray.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    TArray<FStateTreeEditorNode>* NodeArrayPtr = nullptr;
    UStateTreeState* TargetState = nullptr;
    if (NodeArray.Equals(TEXT("GlobalTasks"), ESearchCase::IgnoreCase))
    {
        NodeArrayPtr = &EditorData->GlobalTasks;
    }
    else
    {
        if (StateName.IsEmpty()) return false;
        TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
        FindInState = [&](UStateTreeState* Node)->UStateTreeState*
        {
            if (!Node) return nullptr;
            if (Node->Name.ToString() == StateName) return Node;
            for (TObjectPtr<UStateTreeState> Child : Node->Children)
            {
                UStateTreeState* Found = FindInState(Child);
                if (Found) return Found;
            }
            return nullptr;
        };
        for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
        {
            UStateTreeState* Found = FindInState(ST);
            if (Found)
            {
                TargetState = Found;
                break;
            }
        }
        if (!TargetState) return false;
        if (NodeArray.Equals(TEXT("Tasks"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->Tasks;
        else if (NodeArray.Equals(TEXT("EnterConditions"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->EnterConditions;
        else if (NodeArray.Equals(TEXT("Considerations"), ESearchCase::IgnoreCase)) NodeArrayPtr = &TargetState->Considerations;
        else return false;
    }

    if (!NodeArrayPtr) return false;
    int32 Count = NodeArrayPtr->Num();
    if (FromIndex < 0 || FromIndex >= Count || ToIndex < 0 || ToIndex >= Count) return false;
    if (FromIndex == ToIndex) return true;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::MoveNodeIndex")));
    if (TargetState) TargetState->Modify(false); else EditorData->Modify(false);
    FStateTreeEditorNode Copy = (*NodeArrayPtr)[FromIndex];
    NodeArrayPtr->RemoveAt(FromIndex);
    NodeArrayPtr->Insert(Copy, ToIndex);
    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
#else
    return false;
#endif
}

#if WITH_EDITOR
TArray<FString> UStateTreeService::ListPropertyBagEntries(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName)
{
    TArray<FString> Out;
    if (AssetPath.IsEmpty() || BagPropertyName.IsEmpty()) return Out;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return Out;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return Out;

    // find state by name
    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    if (!StateName.IsEmpty())
    {
        for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
        {
            UStateTreeState* Found = FindInState(ST);
            if (Found)
            {
                TargetState = Found;
                break;
            }
        }
    }
    if (!TargetState) return Out;

    FProperty* Prop = FindFProperty<FProperty>(UStateTreeState::StaticClass(), *BagPropertyName);
    if (!Prop) return Out;
    if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
    {
        void* Dest = StructProp->ContainerPtrToValuePtr<void>(TargetState);
        if (!Dest) return Out;
        FString JsonString;
        if (!FJsonObjectConverter::UStructToJsonObjectString(StructProp->Struct, Dest, JsonString, 0, 0)) return Out;
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
        if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid()) return Out;
        for (const auto& Pair : JsonObject->Values)
        {
            Out.Add(Pair.Key);
        }
    }
    return Out;
}

FString UStateTreeService::GetPropertyBagEntryAsJson(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName)
{
    FString Empty;
    if (AssetPath.IsEmpty() || BagPropertyName.IsEmpty() || EntryName.IsEmpty()) return Empty;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return Empty;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return Empty;

    // find state by name
    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    if (!StateName.IsEmpty())
    {
        for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
        {
            UStateTreeState* Found = FindInState(ST);
            if (Found)
            {
                TargetState = Found;
                break;
            }
        }
    }
    if (!TargetState) return Empty;

    FProperty* Prop = FindFProperty<FProperty>(UStateTreeState::StaticClass(), *BagPropertyName);
    if (!Prop) return Empty;
    if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
    {
        void* Dest = StructProp->ContainerPtrToValuePtr<void>(TargetState);
        if (!Dest) return Empty;
        FString JsonString;
        if (!FJsonObjectConverter::UStructToJsonObjectString(StructProp->Struct, Dest, JsonString, 0, 0)) return Empty;
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
        if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid()) return Empty;
        TSharedPtr<FJsonValue> Val = JsonObject->TryGetField(EntryName);
        if (!Val.IsValid()) return Empty;

        // Serialize the value into a standalone JSON string by wrapping it and then extracting
        // the substring between the first ':' and the final closing brace. This is robust
        // for nested objects/arrays and avoids leaving stray braces.
        TSharedPtr<FJsonObject> TempObj = MakeShared<FJsonObject>();
        TempObj->SetField(TEXT("v"), Val);
        FString OutString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutString);
        FJsonSerializer::Serialize(TempObj.ToSharedRef(), Writer);
        // OutString should be like: {"v":<value>}
        int32 ColonPos = OutString.Find(TEXT(":"));
        int32 LastBrace = INDEX_NONE;
        OutString.FindLastChar('}', LastBrace);
        if (ColonPos == INDEX_NONE || LastBrace == INDEX_NONE || LastBrace <= ColonPos) return Empty;
        FString Sub = OutString.Mid(ColonPos + 1, LastBrace - (ColonPos + 1));
        return Sub.TrimStartAndEnd();
    }
    return Empty;
}

bool UStateTreeService::SetPropertyBagEntryFromJson(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName, const FString& JsonString)
{
    if (AssetPath.IsEmpty() || BagPropertyName.IsEmpty() || EntryName.IsEmpty() || JsonString.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    // find state by name
    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    if (!StateName.IsEmpty())
    {
        for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
        {
            UStateTreeState* Found = FindInState(ST);
            if (Found)
            {
                TargetState = Found;
                break;
            }
        }
    }
    if (!TargetState) return false;

    FProperty* Prop = FindFProperty<FProperty>(UStateTreeState::StaticClass(), *BagPropertyName);
    if (!Prop) return false;
    if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
    {
        void* Dest = StructProp->ContainerPtrToValuePtr<void>(TargetState);
        if (!Dest) return false;

        if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::SetPropertyBagEntryFromJson")));

        // Build a JSON wrapper: { "EntryName": <JsonString> }
        FString Trimmed = JsonString;
        Trimmed.TrimStartAndEndInline();

        // Basic sanitization: if the provided JSON has extra trailing closing braces/brackets,
        // trim them until braces/brackets are balanced. This accepts values that were
        // serialized with an extra closing brace by prior helper code.
        auto CountChars = [](const FString& S, TCHAR C) -> int32
        {
            int32 Count = 0;
            for (TCHAR ch : S) if (ch == C) ++Count;
            return Count;
        };
        int32 OpenCurly = CountChars(Trimmed, TEXT('{'));
        int32 CloseCurly = CountChars(Trimmed, TEXT('}'));
        while (CloseCurly > OpenCurly && Trimmed.Len() > 0 && Trimmed.EndsWith(TEXT("}")))
        {
            Trimmed.LeftChopInline(1);
            --CloseCurly;
        }
        int32 OpenSquare = CountChars(Trimmed, TEXT('['));
        int32 CloseSquare = CountChars(Trimmed, TEXT(']'));
        while (CloseSquare > OpenSquare && Trimmed.Len() > 0 && Trimmed.EndsWith(TEXT("]")))
        {
            Trimmed.LeftChopInline(1);
            --CloseSquare;
        }

        FString Wrapper = FString::Printf(TEXT("{\"%s\":%s}"), *EntryName, *Trimmed);
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Wrapper);
        if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
        {
            if (GEditor) GEditor->EndTransaction();
            return false;
        }
        bool bOk = FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), StructProp->Struct, Dest, 0, 0);
        if (!bOk)
        {
            if (GEditor) GEditor->EndTransaction();
            return false;
        }

        if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
        if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
        {
            EditingSubsystem->ValidateStateTree(StateTree);
        }
        if (GEditor) GEditor->EndTransaction();
        SaveAsset(AssetPath);
        return true;
    }
    return false;
}

#if WITH_EDITOR
FString UStateTreeService::GetPropertyBagEntryValue(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName)
{
    FString Json = GetPropertyBagEntryAsJson(AssetPath, StateName, BagPropertyName, EntryName);
    if (Json.IsEmpty()) return FString();
    // Parse the JSON string and extract possible value fields: "value" or direct object
    TSharedPtr<FJsonValue> RootValue;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    TSharedPtr<FJsonObject> Obj;
    if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
    {
        // Try common keys
        static const TArray<FString> Keys = { TEXT("value"), TEXT("Value"), TEXT("v") };
        for (const FString& K : Keys)
        {
            const TSharedPtr<FJsonValue>* ValPtr = Obj->Values.Find(K);
            if (ValPtr && ValPtr->IsValid())
            {
                TSharedPtr<FJsonValue> V = *ValPtr;
                FString Out;
                if (V->TryGetString(Out)) return Out;
                double Num; if (V->TryGetNumber(Num)) return FString::SanitizeFloat((float)Num);
                bool B; if (V->TryGetBool(B)) return B ? TEXT("true") : TEXT("false");
                // Fallback: stringify the value
                FString Serialized;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
                FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
                return Serialized;
            }
        }
        // If no known key, return whole object
        FString Full;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Full);
        FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
        return Full;
    }
    return FString();
}

bool UStateTreeService::SetPropertyBagEntryBool(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName, bool Value)
{
    FString Val = Value ? TEXT("true") : TEXT("false");
    FString Json = FString::Printf(TEXT("{\"%s\":%s}"), *EntryName, *Val);
    return SetPropertyBagEntryFromJson(AssetPath, StateName, BagPropertyName, EntryName, Json);
}

bool UStateTreeService::SetPropertyBagEntryInt(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName, int32 Value)
{
    FString Val = FString::FromInt(Value);
    FString Json = FString::Printf(TEXT("{\"%s\":%s}"), *EntryName, *Val);
    return SetPropertyBagEntryFromJson(AssetPath, StateName, BagPropertyName, EntryName, Json);
}

bool UStateTreeService::SetPropertyBagEntryFloat(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName, float Value)
{
    FString Val = FString::SanitizeFloat(Value);
    FString Json = FString::Printf(TEXT("{\"%s\":%s}"), *EntryName, *Val);
    return SetPropertyBagEntryFromJson(AssetPath, StateName, BagPropertyName, EntryName, Json);
}

bool UStateTreeService::SetPropertyBagEntryString(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName, const FString& Value)
{
    // Ensure string is properly quoted in JSON
    FString Escaped = Value.ReplaceCharWithEscapedChar();
    FString Json = FString::Printf(TEXT("{\"%s\":\"%s\"}"), *EntryName, *Escaped);
    return SetPropertyBagEntryFromJson(AssetPath, StateName, BagPropertyName, EntryName, Json);
}
#endif

int32 UStateTreeService::AddTransition(const FString& AssetPath, const FString& StateName)
{
    if (AssetPath.IsEmpty() || StateName.IsEmpty()) return -1;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return -1;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return -1;

    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found)
        {
            TargetState = Found;
            break;
        }
    }
    if (!TargetState) return -1;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::AddTransition")));
    TargetState->Modify(false);
    int32 NewIndex = TargetState->Transitions.AddDefaulted();
    TargetState->Transitions[NewIndex].ID = FGuid::NewGuid();

    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return NewIndex;
}

bool UStateTreeService::DeleteTransition(const FString& AssetPath, const FString& StateName, int32 TransitionIndex)
{
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || TransitionIndex < 0) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found)
        {
            TargetState = Found;
            break;
        }
    }
    if (!TargetState) return false;
    if (TransitionIndex >= TargetState->Transitions.Num()) return false;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::DeleteTransition")));
    TargetState->Modify(false);
    TargetState->Transitions.RemoveAt(TransitionIndex);
    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
}

bool UStateTreeService::SetTransitionFromJson(const FString& AssetPath, const FString& StateName, int32 TransitionIndex, const FString& JsonString)
{
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || TransitionIndex < 0 || JsonString.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found)
        {
            TargetState = Found;
            break;
        }
    }
    if (!TargetState) return false;
    if (TransitionIndex >= TargetState->Transitions.Num()) return false;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::SetTransitionFromJson")));
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid()) { if (GEditor) GEditor->EndTransaction(); return false; }
    bool bOk = FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), FStateTreeTransition::StaticStruct(), &TargetState->Transitions[TransitionIndex], 0, 0);
    if (!bOk) { if (GEditor) GEditor->EndTransaction(); return false; }

    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
}

bool UStateTreeService::AddTransitionCondition(const FString& AssetPath, const FString& StateName, int32 TransitionIndex, const FString& ConditionStructPath)
{
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || ConditionStructPath.IsEmpty() || TransitionIndex < 0) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found)
        {
            TargetState = Found;
            break;
        }
    }
    if (!TargetState) return false;
    if (TransitionIndex >= TargetState->Transitions.Num()) return false;

    UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *ConditionStructPath);
    if (!Struct) Struct = LoadObject<UScriptStruct>(nullptr, *ConditionStructPath);
    if (!Struct) return false;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::AddTransitionCondition")));
    TargetState->Modify(false);
    FStateTreeEditorNode& NewNode = TargetState->Transitions[TransitionIndex].Conditions.AddDefaulted_GetRef();
    NewNode.ID = FGuid::NewGuid();
    NewNode.Node.InitializeAs(Struct);
    const FStateTreeNodeBase& NodeBase = NewNode.Node.GetMutable<FStateTreeNodeBase>();
    if (const UStruct* InstanceType = NodeBase.GetInstanceDataType())
    {
        if (const UScriptStruct* InstSS = Cast<const UScriptStruct>(InstanceType))
        {
            NewNode.Instance.InitializeAs(InstSS);
        }
    }
    if (const UStruct* ExecType = NodeBase.GetExecutionRuntimeDataType())
    {
        if (const UScriptStruct* ExecSS = Cast<const UScriptStruct>(ExecType))
        {
            NewNode.ExecutionRuntimeData.InitializeAs(ExecSS);
        }
    }

    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
}

bool UStateTreeService::RemoveTransitionCondition(const FString& AssetPath, const FString& StateName, int32 TransitionIndex, int32 ConditionIndex)
{
    if (AssetPath.IsEmpty() || StateName.IsEmpty() || TransitionIndex < 0 || ConditionIndex < 0) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found)
        {
            TargetState = Found;
            break;
        }
    }
    if (!TargetState) return false;
    if (TransitionIndex >= TargetState->Transitions.Num()) return false;
    FStateTreeTransition& Trans = TargetState->Transitions[TransitionIndex];
    if (ConditionIndex >= Trans.Conditions.Num()) return false;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::RemoveTransitionCondition")));
    TargetState->Modify(false);
    Trans.Conditions.RemoveAt(ConditionIndex);
    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
}

bool UStateTreeService::MoveTransitionConditionIndex(const FString& AssetPath, const FString& StateName, int32 TransitionIndex, int32 FromIndex, int32 ToIndex)
{
    if (AssetPath.IsEmpty() || StateName.IsEmpty()) return false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!StateTree) return false;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData) return false;

    TFunction<UStateTreeState*(UStateTreeState*)> FindInState;
    FindInState = [&](UStateTreeState* Node)->UStateTreeState*
    {
        if (!Node) return nullptr;
        if (Node->Name.ToString() == StateName) return Node;
        for (TObjectPtr<UStateTreeState> Child : Node->Children)
        {
            UStateTreeState* Found = FindInState(Child);
            if (Found) return Found;
        }
        return nullptr;
    };

    UStateTreeState* TargetState = nullptr;
    for (TObjectPtr<UStateTreeState> ST : EditorData->SubTrees)
    {
        UStateTreeState* Found = FindInState(ST);
        if (Found)
        {
            TargetState = Found;
            break;
        }
    }
    if (!TargetState) return false;
    if (TransitionIndex >= TargetState->Transitions.Num()) return false;
    FStateTreeTransition& Trans = TargetState->Transitions[TransitionIndex];

    int32 Count = Trans.Conditions.Num();
    if (FromIndex < 0 || FromIndex >= Count || ToIndex < 0 || ToIndex >= Count) return false;
    if (FromIndex == ToIndex) return true;

    if (GEditor) GEditor->BeginTransaction(FText::FromString(TEXT("UStateTreeService::MoveTransitionConditionIndex")));
    TargetState->Modify(false);
    FStateTreeEditorNode Copy = Trans.Conditions[FromIndex];
    Trans.Conditions.RemoveAt(FromIndex);
    Trans.Conditions.Insert(Copy, ToIndex);
    if (UPackage* Package = StateTree->GetOutermost()) Package->SetDirtyFlag(true);
    if (UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>())
    {
        EditingSubsystem->ValidateStateTree(StateTree);
    }
    if (GEditor) GEditor->EndTransaction();
    SaveAsset(AssetPath);
    return true;
}
#endif
