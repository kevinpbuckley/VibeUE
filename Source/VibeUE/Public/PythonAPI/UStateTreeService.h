// Copyright Buckley Builds LLC 2026

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "UStateTreeService.generated.h"

/**
 * StateTree editor helper service exposed to Python.
 *
 * Provides convenience functions for creating EditorData, adding subtrees,
 * listing subtrees, and saving the asset. Designed to be simple and safe
 * for use from Unreal Python scripts.
 */
UCLASS(BlueprintType)
class VIBEUE_API UStateTreeService : public UObject
{
    GENERATED_BODY()

public:
    /** Ensure the StateTree has EditorData; create one if missing. */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool EnsureEditorData(const FString& AssetPath);

    /** Return the EditorData class path or empty if none. */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static FString GetEditorDataClass(const FString& AssetPath);

    /** Return the GUID string for a named state in the EditorData, or empty if not found. */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static FString GetStateGuid(const FString& AssetPath, const FString& StateName);

    /** Serialize an editor state object to JSON (basic properties). */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static FString GetStateAsJson(const FString& AssetPath, const FString& StateName);

    /** Validation / compile helpers */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool ValidateStateTree(const FString& AssetPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool CompileIfChanged(const FString& AssetPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool ResetCompiled(const FString& AssetPath);

    /** UStruct JSON helpers */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static FString GetStructDefaultJson(const FString& StructPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool SetStructFromJson(const FString& StructPath, const FString& JsonString);

    /** Reinitialize a node's Instance struct to a different UScriptStruct (editor-only). */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool ReinstanceNodeInstance(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 NodeIndex, const FString& NewStructPath);

    /** Property binding management: scan nodes for binding-like fields and perform edits. */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static TArray<FString> ListPropertyBindings(const FString& AssetPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool AddPropertyBinding(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 NodeIndex, const FString& FieldName, const FString& BindingJson);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool RemovePropertyBinding(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 NodeIndex, const FString& FieldName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool RemapPropertyBindings(const FString& AssetPath, const FString& OldPath, const FString& NewPath);

    /** Transition helpers */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static int32 FindTransitionByGuid(const FString& AssetPath, const FString& GuidString);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static int32 AddTransitionWithGuid(const FString& AssetPath, const FString& StateName, const FString& GuidString);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool SetTransitionConditionsJson(const FString& AssetPath, const FString& StateName, int32 TransitionIndex, const FString& ConditionsJsonArray);

    /** Transaction wrappers (convenience) */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool BeginTransaction(const FString& Key, const FString& Reason);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool EndTransaction(const FString& Key);

    /** Export the EditorData for the asset as a JSON string for inspection or CI export. */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static FString ExportEditorDataJson(const FString& AssetPath);

    /** Add a root SubTree name to the StateTree (creates EditorData if needed). */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool AddSubTree(const FString& AssetPath, const FString& SubTreeName);

    /** Move an existing SubTree to be a child of a named state (editor-only).
     *  If a root subtree with the same name exists it will be removed after creating the child.
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool MoveSubTreeUnderState(const FString& AssetPath, const FString& SubTreeName, const FString& ParentStateName);

    /** Set a named UPROPERTY on a state object (editor-only). This sets properties on the `UStateTreeState` instance
     *  (e.g. `Description`, `bEnabled`). It does not currently modify the instanced parameter bag.
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool SetStateParameter(const FString& AssetPath, const FString& StateName, const FString& ParamName, const FString& Value);

    /** Add a Task node to the specified state. `TaskStructPath` should point to a UScriptStruct (e.g. "/Script/MyModule.MyTaskStruct").
     *  Returns true on success. Editor-only.
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool AddTaskToState(const FString& AssetPath, const FString& StateName, const FString& TaskStructPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool AddConditionToState(const FString& AssetPath, const FString& StateName, const FString& ConditionStructPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool AddConsiderationToState(const FString& AssetPath, const FString& StateName, const FString& ConsiderationStructPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool AddGlobalTask(const FString& AssetPath, const FString& TaskStructPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool LinkSubTreeToState(const FString& AssetPath, const FString& StateName, const FString& SubTreeName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static TArray<FString> ListStatesDetailed(const FString& AssetPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static TArray<FString> GetStateDetails(const FString& AssetPath, const FString& StateName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool DeleteSubTree(const FString& AssetPath, const FString& SubTreeName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool DeleteState(const FString& AssetPath, const FString& StateName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool RenameState(const FString& AssetPath, const FString& OldName, const FString& NewName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool RemoveTaskFromState(const FString& AssetPath, const FString& StateName, int32 TaskIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool MoveTaskIndex(const FString& AssetPath, const FString& StateName, int32 FromIndex, int32 ToIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool SetNodeParameter(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 NodeIndex, const FString& ParamName, const FString& Value);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static TArray<FString> DiscoverScriptStructs(const FString& Filter);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool RenameSubTree(const FString& AssetPath, const FString& OldName, const FString& NewName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool RemoveNodeFromState(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 NodeIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool MoveNodeIndex(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 FromIndex, int32 ToIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static FString GetNodeStructAsJson(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 NodeIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool SetNodeStructFromJson(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 NodeIndex, const FString& JsonString);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool DuplicateSubTree(const FString& AssetPath, const FString& SubTreeName, const FString& NewName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool DuplicateState(const FString& AssetPath, const FString& StateName, const FString& NewName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool BeginBulkEdit(const FString& AssetPath, const FString& Reason);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool EndBulkEdit(const FString& AssetPath);

    /** Property bag helpers for editing `FInstancedPropertyBag` entries (editor-only).
     *  `BagPropertyName` is the name of the property on `UStateTreeState` that contains the bag struct
     *  (usually "Parameters").
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static TArray<FString> ListPropertyBagEntries(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName = TEXT("Parameters"));

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static FString GetPropertyBagEntryAsJson(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool SetPropertyBagEntryFromJson(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName, const FString& JsonString);

    /** Convenience typed accessors for property-bag entries. These avoid JSON->UStruct conversions
     *  in Python by providing simple typed setters and a string-valued getter for the entry value.
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static FString GetPropertyBagEntryValue(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool SetPropertyBagEntryBool(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName, bool Value);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool SetPropertyBagEntryInt(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName, int32 Value);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool SetPropertyBagEntryFloat(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName, float Value);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool SetPropertyBagEntryString(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName, const FString& Value);

    /** Transition inspection and editing helpers */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static TArray<FString> ListTransitions(const FString& AssetPath, const FString& StateName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static FString GetTransitionAsJson(const FString& AssetPath, const FString& StateName, int32 TransitionIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool MoveTransitionIndex(const FString& AssetPath, const FString& StateName, int32 FromIndex, int32 ToIndex);

    /** Node / binding helpers */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static TArray<FString> ListNodeStructFields(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 NodeIndex);

    /** Move a node/editor task from one state to another (preserves node data but gives it a new GUID). */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool MoveNodeToState(const FString& AssetPath, const FString& FromStateName, const FString& NodeArray, int32 NodeIndex, const FString& ToStateName);

    /** Convenience: duplicate subtree by name to a new name (clipboard-like copy/paste). */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool CopySubTreeToNewName(const FString& AssetPath, const FString& SubTreeName, const FString& NewName);

    /** Full transition editing helpers */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool SetTransitionTarget(const FString& AssetPath, const FString& StateName, int32 TransitionIndex, const FString& TargetStateName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool SetTransitionPriority(const FString& AssetPath, const FString& StateName, int32 TransitionIndex, int32 Priority);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool SetTransitionField(const FString& AssetPath, const FString& StateName, int32 TransitionIndex, const FString& FieldName, const FString& JsonValue);

    /** Property-bag structural edits */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool AddPropertyBagEntry(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName, const FString& JsonValue);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool RemovePropertyBagEntry(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool RenamePropertyBagEntry(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& OldName, const FString& NewName);

    /** Typed getters for property-bag entries */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool GetPropertyBagEntryBool(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static int32 GetPropertyBagEntryInt(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static float GetPropertyBagEntryFloat(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static FString GetPropertyBagEntryString(const FString& AssetPath, const FString& StateName, const FString& BagPropertyName, const FString& EntryName);

    /** Simple binding helpers: list potential binding keys from a node's struct JSON, and set a top-level field in the node struct JSON. */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static TArray<FString> ListBindingsFromNode(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 NodeIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool SetNodeStructFieldAsJson(const FString& AssetPath, const FString& StateName, const FString& NodeArray, int32 NodeIndex, const FString& FieldName, const FString& JsonValue);

    /** Transition editing helpers (editor-only) */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static int32 AddTransition(const FString& AssetPath, const FString& StateName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool DeleteTransition(const FString& AssetPath, const FString& StateName, int32 TransitionIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool SetTransitionFromJson(const FString& AssetPath, const FString& StateName, int32 TransitionIndex, const FString& JsonString);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool AddTransitionCondition(const FString& AssetPath, const FString& StateName, int32 TransitionIndex, const FString& ConditionStructPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool RemoveTransitionCondition(const FString& AssetPath, const FString& StateName, int32 TransitionIndex, int32 ConditionIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool MoveTransitionConditionIndex(const FString& AssetPath, const FString& StateName, int32 TransitionIndex, int32 FromIndex, int32 ToIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static FString GetPropertyAsJson(const FString& AssetPath, const FString& StateName, const FString& PropertyName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool SetPropertyFromJson(const FString& AssetPath, const FString& StateName, const FString& PropertyName, const FString& JsonString);

    /** List root SubTree names for the StateTree. */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static TArray<FString> ListSubTrees(const FString& AssetPath);

    /** Save the StateTree asset package. */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
    static bool SaveAsset(const FString& AssetPath);
};
