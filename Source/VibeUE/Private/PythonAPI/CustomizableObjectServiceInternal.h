// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"
#include "PythonAPI/UCustomizableObjectService.h"

class UEdGraph;
class UEdGraphNode;
class FProperty;

/**
 * Reflection-only access to the Mutable plugin for UCustomizableObjectService.
 *
 * Nothing here includes a Mutable header or links a Mutable module. Mutable is an optional Beta
 * plugin a project may disable; declaring it as a dependency would force it on (or break loading)
 * in every project that installs VibeUE, and its node classes are Private (unexported) anyway. Everything is found by /Script path and
 * driven through UObject reflection and the generic UEdGraph / UEdGraphNode / UEdGraphSchema
 * virtuals, which Mutable's own editor also relies on.
 */
namespace VibeCO
{
	// ---- Class lookup ------------------------------------------------------------------------

	/** /Script/CustomizableObject.CustomizableObject, or null when Mutable is not loaded. */
	UClass* GetObjectClass();

	/** /Script/CustomizableObjectEditor.CustomizableObjectNode — the base of every graph node. */
	UClass* GetNodeBaseClass();

	/** Empty when Mutable is usable, else a caller-facing reason. */
	FString CheckAvailable();

	/**
	 * Resolve a node class from a caller-supplied name: "CONodeSwitch", "UCONodeSwitch", a full
	 * /Script path, or a legacy name the engine redirects (e.g. "CustomizableObjectNodeMaterial").
	 * Must derive from CustomizableObjectNode and not be abstract.
	 */
	UClass* ResolveNodeClass(const FString& Name, FString& OutError);

	/**
	 * Why a node class cannot be placed in a Customizable Object graph, or empty if it can. Tunnel
	 * nodes are a Macro Library's inputs/outputs: building their pins dereferences the owning macro,
	 * which a Customizable Object graph does not have, so constructing one crashes the editor.
	 */
	FString WhyNotPlaceable(const UClass* Class);

	// ---- Assets ------------------------------------------------------------------------------

	FString CheckNameLength(const FString& Value, const TCHAR* What);
	FString CheckWritableAssetPath(const FString& AssetPath);

	/** "/Game/A/CO_X" -> "/Game/A/CO_X.CO_X"; object paths pass through. */
	FString ToObjectPath(const FString& AssetPath);

	/** Load a Customizable Object and its Source graph. Empty on success. Graph may be null only if bAllowNoGraph. */
	FString OpenObject(const FString& AssetPath, UObject*& OutObject, UEdGraph*& OutGraph, bool bAllowNoGraph = false);

	/** OpenObject plus the write rules (not /Engine, has a graph). */
	FString OpenForWrite(const FString& AssetPath, UObject*& OutObject, UEdGraph*& OutGraph);

	/** Reads UCustomizableObject::Source through reflection. */
	UEdGraph* GetSourceGraph(UObject* Object);

	/**
	 * Finish a write: bump the object's VersionId (the key Mutable's compiled-data cache uses, which
	 * only an open CO editor or a reloaded asset would otherwise refresh), notify the graph so an
	 * open editor redraws, and save. Empty on success.
	 */
	FString CommitEdit(UObject* Object, UEdGraph* Graph);

	/** Give the object a fresh VersionId, so its compiled-data cache key changes. */
	void BumpVersionId(UObject* Object);

	/** Package path (no object name) of any asset object. */
	FString PackagePathOf(const UObject* Object);

	// ---- Nodes & pins ------------------------------------------------------------------------

	FString NodeIdOf(const UEdGraphNode* Node);
	bool IsMutableNode(const UEdGraphNode* Node);

	/** By NodeGuid (any FGuid text format) or node object name. */
	UEdGraphNode* FindNode(UEdGraph* Graph, const FString& NodeId, FString& OutError);

	/** The graph's Base Object node (CustomizableObjectNodeObject with bIsBase). */
	UEdGraphNode* FindBaseObjectNode(UEdGraph* Graph);

	/**
	 * By PinName (exact, then case-insensitive), then by display name. Direction EGPD_MAX means
	 * either; an ambiguous match is an error that lists the candidates.
	 */
	UEdGraphPin* FindPin(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction, FString& OutError);

	FVibeCOPinInfo DescribePin(const UEdGraphPin* Pin);
	TArray<FVibeCOPinInfo> DescribePins(const UEdGraphNode* Node);
	FVibeCONodeInfo DescribeNode(const UEdGraphNode* Node);

	/** Mutable's own creation sequence (EdGraphSchema_CustomizableObject CreateNode), minus the transaction. */
	void FinishNewNode(UEdGraph* Graph, UEdGraphNode* Node, int32 PosX, int32 PosY);

	/** ReconstructNode on every CONodeSwitch linked to Node's outputs, whose option pins follow an enum. */
	void RefreshDependentSwitches(UEdGraphNode* Node);

	// ---- Properties --------------------------------------------------------------------------

	/**
	 * A property callers may read or write on a node: declared by a Mutable node class (not by
	 * UEdGraphNode itself — pins, position, GUID have their own methods), not transient, not
	 * deprecated.
	 */
	bool IsNodeProperty(const FProperty* Property);

	FProperty* FindNodeProperty(UObject* Node, const FString& PropertyName, FString& OutError);

	FString ExportValue(const FProperty* Property, const UObject* Container);

	/** ImportText into Container with value-level rollback on failure. Empty on success. */
	FString ImportValue(FProperty* Property, UObject* Container, const FString& Value);

	TArray<FVibeCOPropertyInfo> DescribeProperties(const UObject* Container);

	/** Read a top-level property as text, or empty if absent. */
	FString ReadPropertyText(const UObject* Container, const TCHAR* PropertyName);

	/** Read a top-level object/soft-object property as a path, or empty. */
	FString ReadObjectPath(const UObject* Container, const TCHAR* PropertyName);

	// ---- Reflected calls ---------------------------------------------------------------------

	/**
	 * Call a UFUNCTION by name. Args fill the input parameters in declaration order, from text.
	 * OutReturn receives the return value as text (enums by name). False with OutError on any
	 * mismatch — never a partial call.
	 */
	bool CallFunction(UObject* Target, const TCHAR* FunctionName, const TArray<FString>& Args,
		FString* OutReturn, FString& OutError);

	/** Convenience: CallFunction returning the text, or Fallback on failure. */
	FString CallForText(UObject* Target, const TCHAR* FunctionName, const TArray<FString>& Args = {},
		const FString& Fallback = FString());

	// ---- Compile -----------------------------------------------------------------------------

	/**
	 * Synchronous compile via UCustomizableObject::Compile(FCompileParams) with bAsync=false and
	 * bSkipIfNotOutOfDate=false, capturing Mutable's log output. Fills everything but DurationSeconds.
	 */
	void Compile(UObject* Object, const FString& OptimizationLevel, const FString& TextureCompression,
		FVibeCOCompileResult& OutResult);
}
