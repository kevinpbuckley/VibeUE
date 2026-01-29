// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/Blueprint.h"
#include "UBlueprintService.generated.h"

/**
 * Information about a blueprint variable
 */
USTRUCT(BlueprintType)
struct FBlueprintVariableInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString VariableName;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString VariableType;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Category;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsPublic = false;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsExposed = false;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DefaultValue;
};

/**
 * Detailed information about a blueprint variable (for get_info action)
 */
USTRUCT(BlueprintType)
struct FBlueprintVariableDetailedInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString VariableName;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString VariableType;

	/** Full type path (e.g., "/Script/CoreUObject.FloatProperty") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString TypePath;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Category;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Tooltip;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DefaultValue;

	/** Whether the variable can be edited per instance in Details panel */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsInstanceEditable = false;

	/** Whether the variable is exposed on spawn */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsExposeOnSpawn = false;

	/** Whether the variable is private */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsPrivate = false;

	/** Whether the variable is read-only in Blueprints */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsBlueprintReadOnly = false;

	/** Whether the variable is exposed to cinematics/Sequencer */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsExposeToCinematics = false;

	/** Replication condition: "None", "Replicated", or "RepNotify" */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ReplicationCondition;

	/** Whether this is an array type */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsArray = false;

	/** Whether this is a set type */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsSet = false;

	/** Whether this is a map type */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsMap = false;
};

/**
 * Search result for variable types
 */
USTRUCT(BlueprintType)
struct FVariableTypeInfo
{
	GENERATED_BODY()

	/** Type name (e.g., "Vector", "Actor") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString TypeName;

	/** Full type path (e.g., "/Script/CoreUObject.Vector") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString TypePath;

	/** Category (e.g., "Structure", "Object", "Enum") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Category;

	/** Description of the type */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Description;
};

/**
 * Information about a blueprint function parameter
 */
USTRUCT(BlueprintType)
struct FBlueprintFunctionParameterInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ParameterName;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ParameterType;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsOutput = false;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsReference = false;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DefaultValue;
};

/**
 * Information about a blueprint function
 */
USTRUCT(BlueprintType)
struct FBlueprintFunctionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString FunctionName;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ReturnType;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FString> Parameters;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsOverride = false;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsPure = false;
};

/**
 * Information about a function local variable
 */
USTRUCT(BlueprintType)
struct FBlueprintLocalVariableInfo
{
	GENERATED_BODY()

	/** Variable name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString VariableName;

	/** Friendly display name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString FriendlyName;

	/** Type descriptor (e.g., "float", "struct:Vector", "object:Actor") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString VariableType;

	/** Human-readable type string */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DisplayType;

	/** Default value as string */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DefaultValue;

	/** Category */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Category;

	/** Variable GUID */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Guid;

	/** Whether variable is const/read-only */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsConst = false;

	/** Whether variable is a reference */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsReference = false;

	/** Whether variable is an array */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsArray = false;

	/** Whether variable is a set */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsSet = false;

	/** Whether variable is a map */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsMap = false;
};

/**
 * Detailed information about a blueprint function (for get_info action)
 */
USTRUCT(BlueprintType)
struct FBlueprintFunctionDetailedInfo
{
	GENERATED_BODY()

	/** Function name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString FunctionName;

	/** Graph GUID as string */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString GraphGuid;

	/** Number of nodes in the function graph */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 NodeCount = 0;

	/** Whether this is a pure function */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsPure = false;

	/** Whether this is an override */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsOverride = false;

	/** Input parameters */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FBlueprintFunctionParameterInfo> InputParameters;

	/** Output parameters */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FBlueprintFunctionParameterInfo> OutputParameters;

	/** Local variables */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FBlueprintLocalVariableInfo> LocalVariables;
};

/**
 * Information about a blueprint component
 */
USTRUCT(BlueprintType)
struct FBlueprintComponentInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ComponentName;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ComponentClass;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString AttachParent;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsRootComponent = false;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsSceneComponent = false;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FString> Children;
};

/**
 * Information about an available component type
 */
USTRUCT(BlueprintType)
struct FComponentTypeInfo
{
	GENERATED_BODY()

	/** Component class name (e.g., "StaticMeshComponent") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Name;

	/** Display name for UI */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DisplayName;

	/** Full class path (e.g., "/Script/Engine.StaticMeshComponent") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ClassPath;

	/** Category (e.g., "Rendering", "Physics") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Category;

	/** Whether this is a scene component (can have transforms) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsSceneComponent = false;

	/** Whether this is a primitive component (can render) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsPrimitiveComponent = false;

	/** Whether this is an abstract class */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsAbstract = false;

	/** Base class name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString BaseClass;
};

/**
 * Detailed information about a component type
 */
USTRUCT(BlueprintType)
struct FComponentDetailedInfo
{
	GENERATED_BODY()

	/** Component class name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Name;

	/** Display name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DisplayName;

	/** Full class path */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ClassPath;

	/** Category */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Category;

	/** Parent class name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ParentClass;

	/** Whether this is a scene component */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsSceneComponent = false;

	/** Whether this is a primitive component */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsPrimitiveComponent = false;

	/** Number of editable properties */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 PropertyCount = 0;

	/** Number of callable functions */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 FunctionCount = 0;
};

/**
 * Information about a component property
 */
USTRUCT(BlueprintType)
struct FComponentPropertyInfo
{
	GENERATED_BODY()

	/** Property name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString PropertyName;

	/** Property type (e.g., "float", "FVector", "UStaticMesh*") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString PropertyType;

	/** Property category */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Category;

	/** Current value as string */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Value;

	/** Whether the property is editable */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsEditable = true;

	/** Whether the property is inherited */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsInherited = false;
};

/**
 * Information about a pin on a blueprint node
 */
USTRUCT(BlueprintType)
struct FBlueprintPinInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString PinName;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString PinType;  // exec, bool, float, int, string, object, etc.

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsInput = true;  // True for input, false for output

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsConnected = false;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DefaultValue;
};

/**
 * Information about a connection between two pins
 */
USTRUCT(BlueprintType)
struct FBlueprintConnectionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString SourceNodeId;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString SourceNodeTitle;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString SourcePinName;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString TargetNodeId;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString TargetNodeTitle;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString TargetPinName;
};

/**
 * Information about a blueprint node
 */
USTRUCT(BlueprintType)
struct FBlueprintNodeInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString NodeId;  // Unique identifier (GUID)

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString NodeType;  // K2Node class name

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString NodeTitle;  // Display title

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	float PosX = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	float PosY = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FString> PinNames;  // Names of all pins on this node (for quick reference)

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FBlueprintPinInfo> Pins;  // Detailed pin information
};

/**
 * Detailed pin information including connections
 */
USTRUCT(BlueprintType)
struct FBlueprintPinDetailedInfo
{
	GENERATED_BODY()

	/** Pin name (internal) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString PinName;

	/** Pin display name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DisplayName;

	/** Pin type category (exec, bool, float, int, string, object, struct, etc.) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString PinCategory;

	/** Pin type subcategory or object/struct path */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString PinSubCategory;

	/** Full type path for struct/object types */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString TypePath;

	/** Whether this is an input pin */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsInput = true;

	/** Whether the pin is connected */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsConnected = false;

	/** Whether the pin is hidden */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsHidden = false;

	/** Whether this is an array type */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsArray = false;

	/** Whether this is a reference type */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsReference = false;

	/** Whether the pin can be split (struct pins) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bCanSplit = false;

	/** Whether the pin is currently split */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsSplit = false;

	/** Default value */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DefaultValue;

	/** Tooltip */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Tooltip;

	/** Connected node IDs and pin names (format: "NodeId:PinName") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FString> Connections;
};

/**
 * Detailed node information (for details action)
 */
USTRUCT(BlueprintType)
struct FBlueprintNodeDetailedInfo
{
	GENERATED_BODY()

	/** Node GUID */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString NodeId;

	/** Node class name (e.g., K2Node_CallFunction) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString NodeClass;

	/** Display title */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString NodeTitle;

	/** Full title */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString FullTitle;

	/** Graph name this node belongs to */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString GraphName;

	/** Graph scope (event, function, macro) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString GraphScope;

	/** Node category */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Category;

	/** Tooltip/description */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Tooltip;

	/** Position X */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	float PosX = 0.0f;

	/** Position Y */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	float PosY = 0.0f;

	/** Whether this is a pure node (no exec pins) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsPure = false;

	/** Whether this node has latent execution */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsLatent = false;

	/** For function calls: target function name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString FunctionName;

	/** For function calls: owning class */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString FunctionClass;

	/** For variable nodes: variable name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString VariableName;

	/** Input pins */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FBlueprintPinDetailedInfo> InputPins;

	/** Output pins */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FBlueprintPinDetailedInfo> OutputPins;
};

/**
 * Information about a discoverable node type
 */
USTRUCT(BlueprintType)
struct FBlueprintNodeTypeInfo
{
	GENERATED_BODY()

	/** Display name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DisplayName;

	/** Node type category (e.g., "Math", "Flow Control") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Category;

	/** Spawner key for creating this node */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString SpawnerKey;

	/** Node class name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString NodeClass;

	/** Tooltip/description */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Tooltip;

	/** Whether this is a pure function (no exec pins) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsPure = false;

	/** Whether this is a latent action */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsLatent = false;

	/** Keywords for searching */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FString> Keywords;
};

/**
 * Comprehensive blueprint information
 */
USTRUCT(BlueprintType)
struct FBlueprintDetailedInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString BlueprintName;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString BlueprintPath;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ParentClass;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsWidgetBlueprint = false;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FBlueprintVariableInfo> Variables;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FBlueprintFunctionInfo> Functions;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FBlueprintComponentInfo> Components;
};

/**
 * Blueprint service exposed directly to Python.
 *
 * This service provides blueprint introspection and analysis with native
 * Unreal Engine types.
 *
 * Python Usage:
 *   import unreal
 *
 *   # Get blueprint info (returns BlueprintDetailedInfo or None)
 *   info = unreal.BlueprintService.get_blueprint_info("/Game/Blueprints/BP_Player_Test")
 *   if info:
 *       print(f"Parent: {info.parent_class}")
 *       for var in info.variables:
 *           print(f"  {var.variable_name}: {var.variable_type}")
 *
 *   # List variables
 *   variables = unreal.BlueprintService.list_variables("/Game/BP_Player_Test")
 *
 *   # List components
 *   components = unreal.BlueprintService.list_components("/Game/BP_Player_Test")
 *
 * @note All methods are static and thread-safe
 * @note C++ out parameters become Python return values
 */
UCLASS(BlueprintType)
class VIBEUE_API UBlueprintService : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Get comprehensive blueprint information.
	 *
	 * @param BlueprintPath - Full path to the blueprint (e.g., "/Game/Blueprints/BP_Player_Test")
	 * @param OutInfo - Structure containing all blueprint details (C++ only)
	 * @return True if successful, false if blueprint not found or invalid
	 *
	 * Python Usage (out params become return values):
	 *   info = unreal.BlueprintService.get_blueprint_info("/Game/BP_Player_Test")
	 *   if info:
	 *       print(f"Found {len(info.variables)} variables")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool GetBlueprintInfo(const FString& BlueprintPath, FBlueprintDetailedInfo& OutInfo);

	/**
	 * List all variables in a blueprint.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @return Array of variable information
	 *
	 * Example:
	 *   vars = unreal.BlueprintService.list_variables("/Game/BP_Player_Test")
	 *   for var in vars:
	 *       print(f"{var.variable_name}: {var.variable_type}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static TArray<FBlueprintVariableInfo> ListVariables(const FString& BlueprintPath);

	/**
	 * List all functions in a blueprint.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @return Array of function information
	 *
	 * Example:
	 *   funcs = unreal.BlueprintService.list_functions("/Game/BP_Player_Test")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static TArray<FBlueprintFunctionInfo> ListFunctions(const FString& BlueprintPath);

	/**
	 * List all components in a blueprint.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @return Array of component information
	 *
	 * Example:
	 *   comps = unreal.BlueprintService.list_components("/Game/BP_Player_Test")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static TArray<FBlueprintComponentInfo> ListComponents(const FString& BlueprintPath);

	/**
	 * Get the component hierarchy as a flat list with parent information.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @return Array of components with hierarchy information
	 *
	 * Example:
	 *   hierarchy = unreal.BlueprintService.get_component_hierarchy("/Game/BP_Player_Test")
	 *   for comp in hierarchy:
	 *       indent = "  " if comp.attach_parent else ""
	 *       print(f"{indent}{comp.component_name} ({comp.component_class})")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static TArray<FBlueprintComponentInfo> GetComponentHierarchy(const FString& BlueprintPath);

	// ============================================================================
	// COMPONENT MANAGEMENT (manage_blueprint_component actions)
	// ============================================================================

	/**
	 * Get available component types that can be added to blueprints.
	 * Use this to discover what components are available before adding them.
	 *
	 * @param SearchFilter - Optional filter to search by name (partial match, case-insensitive)
	 * @param MaxResults - Maximum number of results to return (default 50)
	 * @return Array of available component types
	 *
	 * Example - Get all available components:
	 *   types = unreal.BlueprintService.get_available_components()
	 *   for t in types:
	 *       print(f"{t.name} ({t.category})")
	 *
	 * Example - Search for mesh components:
	 *   types = unreal.BlueprintService.get_available_components("Mesh")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Components")
	static TArray<FComponentTypeInfo> GetAvailableComponents(
		const FString& SearchFilter = TEXT(""),
		int32 MaxResults = 50
	);

	/**
	 * Get detailed information about a component type.
	 *
	 * @param ComponentType - Component class name (e.g., "StaticMeshComponent")
	 * @param OutInfo - Detailed component type information
	 * @return True if successful
	 *
	 * Example:
	 *   success, info = unreal.BlueprintService.get_component_info("StaticMeshComponent")
	 *   if success:
	 *       print(f"Properties: {info.property_count}, Functions: {info.function_count}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Components")
	static bool GetComponentInfo(
		const FString& ComponentType,
		FComponentDetailedInfo& OutInfo
	);

	/**
	 * Add a component to a blueprint.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param ComponentType - Component class name (e.g., "StaticMeshComponent", "PointLightComponent")
	 * @param ComponentName - Name for the new component
	 * @param ParentName - Optional name of parent component (for scene components)
	 * @return True if successful
	 *
	 * Example - Add a static mesh:
	 *   unreal.BlueprintService.add_component("/Game/BP_Player", "StaticMeshComponent", "Body")
	 *
	 * Example - Add with parent:
	 *   unreal.BlueprintService.add_component("/Game/BP_Player", "SpotLightComponent", "HeadLight", "Head")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Components")
	static bool AddComponent(
		const FString& BlueprintPath,
		const FString& ComponentType,
		const FString& ComponentName,
		const FString& ParentName = TEXT("")
	);

	/**
	 * Remove a component from a blueprint.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param ComponentName - Name of the component to remove
	 * @param bRemoveChildren - Whether to also remove child components (default: true)
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.remove_component("/Game/BP_Player", "OldMesh")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Components")
	static bool RemoveComponent(
		const FString& BlueprintPath,
		const FString& ComponentName,
		bool bRemoveChildren = true
	);

	/**
	 * Get a property value from a component in a blueprint.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param ComponentName - Name of the component
	 * @param PropertyName - Name of the property to get
	 * @param OutValue - Property value as a string
	 * @return True if successful
	 *
	 * Example:
	 *   success, value = unreal.BlueprintService.get_component_property("/Game/BP_Player", "Mesh", "RelativeLocation")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Components")
	static bool GetComponentProperty(
		const FString& BlueprintPath,
		const FString& ComponentName,
		const FString& PropertyName,
		FString& OutValue
	);

	/**
	 * Set a property value on a component in a blueprint.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param ComponentName - Name of the component
	 * @param PropertyName - Name of the property to set
	 * @param PropertyValue - Value to set as a string
	 * @return True if successful
	 *
	 * Example - Set relative location:
	 *   unreal.BlueprintService.set_component_property("/Game/BP_Player", "Mesh", "RelativeLocation", "(X=0,Y=0,Z=50)")
	 *
	 * Example - Set visibility:
	 *   unreal.BlueprintService.set_component_property("/Game/BP_Player", "Mesh", "bVisible", "true")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Components")
	static bool SetComponentProperty(
		const FString& BlueprintPath,
		const FString& ComponentName,
		const FString& PropertyName,
		const FString& PropertyValue
	);

	/**
	 * Get all properties of a component in a blueprint.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param ComponentName - Name of the component
	 * @param bIncludeInherited - Whether to include inherited properties (default: true)
	 * @return Array of property information
	 *
	 * Example:
	 *   props = unreal.BlueprintService.get_all_component_properties("/Game/BP_Player", "Mesh")
	 *   for prop in props:
	 *       print(f"{prop.property_name}: {prop.property_type} = {prop.value}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Components")
	static TArray<FComponentPropertyInfo> GetAllComponentProperties(
		const FString& BlueprintPath,
		const FString& ComponentName,
		bool bIncludeInherited = true
	);

	/**
	 * Reparent a component to a new parent in the hierarchy.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param ComponentName - Name of the component to reparent
	 * @param NewParentName - Name of the new parent component
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.reparent_component("/Game/BP_Player", "Light", "NewRoot")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Components")
	static bool ReparentComponent(
		const FString& BlueprintPath,
		const FString& ComponentName,
		const FString& NewParentName
	);

	/**
	 * Get the parent class of a blueprint.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @return Parent class name, or empty string if not found
	 *
	 * Example:
	 *   parent = unreal.BlueprintService.get_parent_class("/Game/BP_Player_Test")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static FString GetParentClass(const FString& BlueprintPath);

	/**
	 * Check if a blueprint is a Widget Blueprint.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @return True if it's a Widget Blueprint
	 *
	 * Example:
	 *   is_widget = unreal.BlueprintService.is_widget_blueprint("/Game/UI/WBP_MainMenu")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool IsWidgetBlueprint(const FString& BlueprintPath);

	// ============================================================================
	// VARIABLE MANAGEMENT (Phase 1)
	// ============================================================================

	/**
	 * Add a new variable to a blueprint.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param VariableName - Name of the variable
	 * @param VariableType - Type string (e.g., "float", "FVector", "AActor", "TSubclassOf<AActor>")
	 * @param DefaultValue - Default value as a string (optional)
	 * @param bIsArray - Whether this is an array type
	 * @param ContainerType - Container type: "Array", "Set", "Map", or empty
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.add_variable("/Game/BP_Player", "Health", "float", "100.0")
	 *   unreal.BlueprintService.add_variable("/Game/BP_Player", "Location", "FVector", "(X=0,Y=0,Z=100)")
	 *   unreal.BlueprintService.add_variable("/Game/BP_Player", "Items", "AActor", "", bIsArray=True, ContainerType="Array")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool AddVariable(
		const FString& BlueprintPath,
		const FString& VariableName,
		const FString& VariableType,
		const FString& DefaultValue = TEXT(""),
		bool bIsArray = false,
		const FString& ContainerType = TEXT("")
	);

	/**
	 * Set the default value of an existing variable.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param VariableName - Name of the variable
	 * @param DefaultValue - New default value as a string
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.set_variable_default_value("/Game/BP_Player", "Health", "150.0")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool SetVariableDefaultValue(
		const FString& BlueprintPath,
		const FString& VariableName,
		const FString& DefaultValue
	);

	/**
	 * Remove a variable from a blueprint.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param VariableName - Name of the variable to remove
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.remove_variable("/Game/BP_Player", "OldVariable")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool RemoveVariable(
		const FString& BlueprintPath,
		const FString& VariableName
	);

	/**
	 * Get detailed information about a specific variable.
	 * Use this to discover all properties that can be modified before calling ModifyVariable.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param VariableName - Name of the variable
	 * @param OutInfo - Detailed variable information (C++ only, becomes return value in Python)
	 * @return True if successful
	 *
	 * Example:
	 *   success, info = unreal.BlueprintService.get_variable_info("/Game/BP_Player", "Health")
	 *   if success:
	 *       print(f"Type: {info.variable_type}, Category: {info.category}")
	 *       print(f"Replication: {info.replication_condition}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool GetVariableInfo(
		const FString& BlueprintPath,
		const FString& VariableName,
		FBlueprintVariableDetailedInfo& OutInfo
	);

	/**
	 * Modify properties of an existing variable.
	 * All fields returned by GetVariableInfo can be modified. Pass empty string to keep current value.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param VariableName - Name of the variable to modify
	 * @param NewName - New name for the variable (empty to keep current)
	 * @param NewCategory - New category (empty to keep current)
	 * @param NewTooltip - New tooltip (empty to keep current)
	 * @param NewDefaultValue - New default value (empty to keep current)
	 * @param bSetInstanceEditable - If >= 0, sets instance editable flag (0=false, 1=true, -1=unchanged)
	 * @param bSetExposeOnSpawn - If >= 0, sets expose on spawn flag
	 * @param bSetPrivate - If >= 0, sets private flag
	 * @param bSetBlueprintReadOnly - If >= 0, sets read-only flag
	 * @param NewReplicationCondition - "None", "Replicated", "RepNotify", or empty to keep current
	 * @return True if successful
	 *
	 * Example - Rename variable:
	 *   unreal.BlueprintService.modify_variable("/Game/BP_Player", "HP", NewName="Health")
	 *
	 * Example - Make instance editable with category:
	 *   unreal.BlueprintService.modify_variable("/Game/BP_Enemy", "Damage",
	 *       NewCategory="Combat", bSetInstanceEditable=1, NewTooltip="Base damage value")
	 *
	 * Example - Enable replication:
	 *   unreal.BlueprintService.modify_variable("/Game/BP_Player", "Score",
	 *       NewReplicationCondition="Replicated")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool ModifyVariable(
		const FString& BlueprintPath,
		const FString& VariableName,
		const FString& NewName = TEXT(""),
		const FString& NewCategory = TEXT(""),
		const FString& NewTooltip = TEXT(""),
		const FString& NewDefaultValue = TEXT(""),
		int32 bSetInstanceEditable = -1,
		int32 bSetExposeOnSpawn = -1,
		int32 bSetPrivate = -1,
		int32 bSetBlueprintReadOnly = -1,
		const FString& NewReplicationCondition = TEXT("")
	);

	/**
	 * Search for available variable types.
	 * Use this to discover valid type names/paths before creating variables.
	 *
	 * @param SearchTerm - Search term to filter types (partial match, case-insensitive)
	 * @param Category - Filter by category: "Basic", "Structure", "Object", "Enum" (empty for all)
	 * @param MaxResults - Maximum number of results to return (default 20)
	 * @return Array of matching type information
	 *
	 * Example - Search for Vector types:
	 *   types = unreal.BlueprintService.search_variable_types("Vector")
	 *   for t in types:
	 *       print(f"{t.type_name}: {t.type_path}")
	 *
	 * Example - Get all Structure types:
	 *   types = unreal.BlueprintService.search_variable_types("", "Structure")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static TArray<FVariableTypeInfo> SearchVariableTypes(
		const FString& SearchTerm = TEXT(""),
		const FString& Category = TEXT(""),
		int32 MaxResults = 20
	);

	// ============================================================================
	// FUNCTION MANAGEMENT (Phase 2)
	// ============================================================================

	/**
	 * Create a new function in a blueprint (idempotent - won't create duplicates).
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param FunctionName - Name of the function
	 * @param bIsPure - Whether this is a pure function (no exec pins)
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.create_function("/Game/BP_Player", "ApplyDamage")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool CreateFunction(
		const FString& BlueprintPath,
		const FString& FunctionName,
		bool bIsPure = false
	);

	/**
	 * Add a parameter to a function.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param FunctionName - Name of the function
	 * @param ParameterName - Name of the parameter
	 * @param ParameterType - Type string (same format as AddVariable)
	 * @param bIsOutput - Whether this is an output parameter
	 * @param bIsReference - Whether this is passed by reference
	 * @param DefaultValue - Default value as a string (optional)
	 * @param bIsArray - Whether this is an array type
	 * @param ContainerType - Container type: "Array", "Set", "Map", or empty
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.add_function_parameter("/Game/BP_Player", "ApplyDamage", "Amount", "float")
	 *   unreal.BlueprintService.add_function_parameter("/Game/BP_Player", "ApplyDamage", "WasKilled", "bool", bIsOutput=True)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool AddFunctionParameter(
		const FString& BlueprintPath,
		const FString& FunctionName,
		const FString& ParameterName,
		const FString& ParameterType,
		bool bIsOutput = false,
		bool bIsReference = false,
		const FString& DefaultValue = TEXT(""),
		bool bIsArray = false,
		const FString& ContainerType = TEXT("")
	);

	/**
	 * Add a local variable to a function.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param FunctionName - Name of the function
	 * @param VariableName - Name of the local variable
	 * @param VariableType - Type string (same format as AddVariable)
	 * @param DefaultValue - Default value as a string (optional)
	 * @param bIsArray - Whether this is an array type
	 * @param ContainerType - Container type: "Array", "Set", "Map", or empty
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.add_function_local_variable("/Game/BP_Player", "ApplyDamage", "TempDamage", "float", "0.0")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool AddFunctionLocalVariable(
		const FString& BlueprintPath,
		const FString& FunctionName,
		const FString& VariableName,
		const FString& VariableType,
		const FString& DefaultValue = TEXT(""),
		bool bIsArray = false,
		const FString& ContainerType = TEXT("")
	);

	/**
	 * Get detailed parameter information for a function.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param FunctionName - Name of the function
	 * @return Array of parameter information
	 *
	 * Example:
	 *   params = unreal.BlueprintService.get_function_parameters("/Game/BP_Player", "ApplyDamage")
	 *   for param in params:
	 *       print(f"{param.parameter_name}: {param.parameter_type} (output={param.b_is_output})")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static TArray<FBlueprintFunctionParameterInfo> GetFunctionParameters(
		const FString& BlueprintPath,
		const FString& FunctionName
	);

	/**
	 * Delete a function from a blueprint.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param FunctionName - Name of the function to delete
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.delete_function("/Game/BP_Player", "OldFunction")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Functions")
	static bool DeleteFunction(
		const FString& BlueprintPath,
		const FString& FunctionName
	);

	/**
	 * Get detailed information about a specific function.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param FunctionName - Name of the function
	 * @param OutInfo - Detailed function information (C++ only, becomes return value in Python)
	 * @return True if successful
	 *
	 * Example:
	 *   success, info = unreal.BlueprintService.get_function_info("/Game/BP_Player", "ApplyDamage")
	 *   if success:
	 *       print(f"Nodes: {info.node_count}, Pure: {info.b_is_pure}")
	 *       for param in info.input_parameters:
	 *           print(f"  Input: {param.parameter_name}: {param.parameter_type}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Functions")
	static bool GetFunctionInfo(
		const FString& BlueprintPath,
		const FString& FunctionName,
		FBlueprintFunctionDetailedInfo& OutInfo
	);

	/**
	 * Add an input parameter to a function (convenience method for add_input action).
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param FunctionName - Name of the function
	 * @param ParameterName - Name of the parameter
	 * @param ParameterType - Type string (e.g., "float", "FVector", "AActor")
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.add_function_input("/Game/BP_Player", "ApplyDamage", "Amount", "float")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Functions")
	static bool AddFunctionInput(
		const FString& BlueprintPath,
		const FString& FunctionName,
		const FString& ParameterName,
		const FString& ParameterType
	);

	/**
	 * Add an output parameter to a function (convenience method for add_output action).
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param FunctionName - Name of the function
	 * @param ParameterName - Name of the parameter
	 * @param ParameterType - Type string (e.g., "float", "bool", "FVector")
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.add_function_output("/Game/BP_Player", "ApplyDamage", "WasKilled", "bool")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Functions")
	static bool AddFunctionOutput(
		const FString& BlueprintPath,
		const FString& FunctionName,
		const FString& ParameterName,
		const FString& ParameterType
	);

	/**
	 * Remove a parameter from a function.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param FunctionName - Name of the function
	 * @param ParameterName - Name of the parameter to remove
	 * @param bIsOutput - Whether this is an output parameter (default: false for input)
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.remove_function_parameter("/Game/BP_Player", "ApplyDamage", "OldParam")
	 *   unreal.BlueprintService.remove_function_parameter("/Game/BP_Player", "ApplyDamage", "OldOutput", bIsOutput=True)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Functions")
	static bool RemoveFunctionParameter(
		const FString& BlueprintPath,
		const FString& FunctionName,
		const FString& ParameterName,
		bool bIsOutput = false
	);

	/**
	 * Remove a local variable from a function.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param FunctionName - Name of the function
	 * @param VariableName - Name of the local variable to remove
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.remove_function_local_variable("/Game/BP_Player", "ApplyDamage", "TempVar")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Functions")
	static bool RemoveFunctionLocalVariable(
		const FString& BlueprintPath,
		const FString& FunctionName,
		const FString& VariableName
	);

	/**
	 * Update a local variable in a function.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param FunctionName - Name of the function
	 * @param VariableName - Current name of the local variable
	 * @param NewName - New name for the variable (empty to keep current)
	 * @param NewType - New type for the variable (empty to keep current)
	 * @param NewDefaultValue - New default value (empty to keep current)
	 * @return True if successful
	 *
	 * Example - Rename local variable:
	 *   unreal.BlueprintService.update_function_local_variable("/Game/BP_Player", "ApplyDamage", "TempVar", NewName="FinalDamage")
	 *
	 * Example - Change type:
	 *   unreal.BlueprintService.update_function_local_variable("/Game/BP_Player", "ApplyDamage", "Counter", NewType="int64")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Functions")
	static bool UpdateFunctionLocalVariable(
		const FString& BlueprintPath,
		const FString& FunctionName,
		const FString& VariableName,
		const FString& NewName = TEXT(""),
		const FString& NewType = TEXT(""),
		const FString& NewDefaultValue = TEXT("")
	);

	/**
	 * List all local variables in a function.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param FunctionName - Name of the function
	 * @return Array of local variable information
	 *
	 * Example:
	 *   locals = unreal.BlueprintService.list_function_local_variables("/Game/BP_Player", "ApplyDamage")
	 *   for var in locals:
	 *       print(f"{var.variable_name}: {var.variable_type} = {var.default_value}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Functions")
	static TArray<FBlueprintLocalVariableInfo> ListFunctionLocalVariables(
		const FString& BlueprintPath,
		const FString& FunctionName
	);

	// ============================================================================
	// NODE MANAGEMENT (Phase 3)
	// ============================================================================

	/**
	 * Add a variable getter node to a graph.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph ("EventGraph", function name, etc.)
	 * @param VariableName - Name of the variable to get
	 * @param PosX - X position in the graph
	 * @param PosY - Y position in the graph
	 * @return Node ID (GUID) if successful, empty string otherwise
	 *
	 * Example:
	 *   node_id = unreal.BlueprintService.add_get_variable_node("/Game/BP_Player", "ApplyDamage", "Health", 200, 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static FString AddGetVariableNode(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& VariableName,
		float PosX = 0.0f,
		float PosY = 0.0f
	);

	/**
	 * Add a variable setter node to a graph.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph ("EventGraph", function name, etc.)
	 * @param VariableName - Name of the variable to set
	 * @param PosX - X position in the graph
	 * @param PosY - Y position in the graph
	 * @return Node ID (GUID) if successful, empty string otherwise
	 *
	 * Example:
	 *   node_id = unreal.BlueprintService.add_set_variable_node("/Game/BP_Player", "ApplyDamage", "Health", 400, 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static FString AddSetVariableNode(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& VariableName,
		float PosX = 0.0f,
		float PosY = 0.0f
	);

	/**
	 * Add a branch node to a graph.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @param PosX - X position in the graph
	 * @param PosY - Y position in the graph
	 * @return Node ID (GUID) if successful, empty string otherwise
	 *
	 * Example:
	 *   node_id = unreal.BlueprintService.add_branch_node("/Game/BP_Player", "ApplyDamage", 300, 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static FString AddBranchNode(
		const FString& BlueprintPath,
		const FString& GraphName,
		float PosX = 0.0f,
		float PosY = 0.0f
	);

	/**
	 * Add a print string node to a graph.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @param PosX - X position in the graph
	 * @param PosY - Y position in the graph
	 * @return Node ID (GUID) if successful, empty string otherwise
	 *
	 * Example:
	 *   node_id = unreal.BlueprintService.add_print_string_node("/Game/BP_Player", "EventGraph", 100, 100)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static FString AddPrintStringNode(
		const FString& BlueprintPath,
		const FString& GraphName,
		float PosX = 0.0f,
		float PosY = 0.0f
	);

	/**
	 * Connect two nodes by their pins.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @param SourceNodeId - GUID of the source node
	 * @param SourcePinName - Name of the output pin on the source node
	 * @param TargetNodeId - GUID of the target node
	 * @param TargetPinName - Name of the input pin on the target node
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.connect_nodes("/Game/BP_Player", "ApplyDamage", entry_id, "then", branch_id, "execute")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool ConnectNodes(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& SourceNodeId,
		const FString& SourcePinName,
		const FString& TargetNodeId,
		const FString& TargetPinName
	);

	/**
	 * Get all nodes in a graph.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @return Array of node information
	 *
	 * Example:
	 *   nodes = unreal.BlueprintService.get_nodes_in_graph("/Game/BP_Player", "ApplyDamage")
	 *   for node in nodes:
	 *       print(f"{node.node_title} at ({node.pos_x}, {node.pos_y})")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static TArray<FBlueprintNodeInfo> GetNodesInGraph(
		const FString& BlueprintPath,
		const FString& GraphName
	);

	/**
	 * Add a cast node to a graph.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @param TargetClass - Class to cast to (e.g., "Character", "Pawn", "Actor")
	 * @param PosX - X position in the graph
	 * @param PosY - Y position in the graph
	 * @return Node ID (GUID) if successful, empty string otherwise
	 *
	 * Example:
	 *   node_id = unreal.BlueprintService.add_cast_node("/Game/ABP_Character", "EventGraph", "Character", 200, 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static FString AddCastNode(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& TargetClass,
		float PosX = 0.0f,
		float PosY = 0.0f
	);

	/**
	 * Add an event node to a graph (e.g., Event BeginPlay, Event Tick).
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @param EventName - Name of the event (e.g., "ReceiveBeginPlay", "ReceiveTick", "BlueprintInitializeAnimation")
	 * @param PosX - X position in the graph
	 * @param PosY - Y position in the graph
	 * @return Node ID (GUID) if successful, empty string otherwise
	 *
	 * Example:
	 *   node_id = unreal.BlueprintService.add_event_node("/Game/BP_Player", "EventGraph", "ReceiveBeginPlay", 0, 0)
	 *   node_id = unreal.BlueprintService.add_event_node("/Game/ABP_Character", "EventGraph", "BlueprintInitializeAnimation", 0, 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static FString AddEventNode(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& EventName,
		float PosX = 0.0f,
		float PosY = 0.0f
	);

	// ============================================================================
	// ADVANCED NODE OPERATIONS (Phase 4)
	// ============================================================================

	/**
	 * Add a function call node to a graph. This is the most versatile node creation method.
	 * Use this to add any blueprint-callable function including math operations.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @param FunctionOwnerClass - Class that owns the function (e.g., "KismetMathLibrary", "KismetSystemLibrary")
	 * @param FunctionName - Name of the function (e.g., "Greater_FloatFloat", "Clamp", "Add_FloatFloat")
	 * @param PosX - X position in the graph
	 * @param PosY - Y position in the graph
	 * @return Node ID (GUID) if successful, empty string otherwise
	 *
	 * Example - Add Greater Than comparison:
	 *   node_id = unreal.BlueprintService.add_function_call_node("/Game/BP_Player", "ApplyDamage", "KismetMathLibrary", "Greater_FloatFloat", 200, 100)
	 *
	 * Example - Add Clamp node:
	 *   node_id = unreal.BlueprintService.add_function_call_node("/Game/BP_Player", "ApplyDamage", "KismetMathLibrary", "Clamp", 400, 100)
	 *
	 * Example - Add Subtract node:
	 *   node_id = unreal.BlueprintService.add_function_call_node("/Game/BP_Player", "ApplyDamage", "KismetMathLibrary", "Subtract_FloatFloat", 300, 100)
	 *
	 * Common Classes:
	 *   - KismetMathLibrary: Math operations (Add, Subtract, Multiply, Divide, Clamp, Greater, Less, etc.)
	 *   - KismetSystemLibrary: System functions (PrintString, Delay, etc.)
	 *   - KismetStringLibrary: String operations
	 *   - KismetArrayLibrary: Array operations
	 *   - GameplayStatics: Game functions (GetPlayerController, SpawnActor, etc.)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static FString AddFunctionCallNode(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& FunctionOwnerClass,
		const FString& FunctionName,
		float PosX = 0.0f,
		float PosY = 0.0f
	);

	/**
	 * Add a comparison node to a graph (convenience method).
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @param ComparisonType - Type of comparison: "Greater", "Less", "GreaterEqual", "LessEqual", "Equal", "NotEqual"
	 * @param ValueType - Type to compare: "Float", "Int", "Double" (default: "Float")
	 *                    Note: UE 5.7 normalizes "Float" to "Double" internally
	 * @param PosX - X position in the graph
	 * @param PosY - Y position in the graph
	 * @return Node ID (GUID) if successful, empty string otherwise
	 *
	 * Example:
	 *   node_id = unreal.BlueprintService.add_comparison_node("/Game/BP_Player", "ApplyDamage", "Greater", "Float", 200, 100)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static FString AddComparisonNode(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& ComparisonType,
		const FString& ValueType = TEXT("Float"),
		float PosX = 0.0f,
		float PosY = 0.0f
	);

	/**
	 * Add a math operation node to a graph (convenience method).
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @param MathOperation - Operation: "Add", "Subtract", "Multiply", "Divide", "Clamp", "Min", "Max", "Abs"
	 * @param ValueType - Type for operation: "Float", "Int", "Double", "Vector" (default: "Float")
	 *                    Note: UE 5.7 normalizes "Float" to "Double" internally
	 * @param PosX - X position in the graph
	 * @param PosY - Y position in the graph
	 * @return Node ID (GUID) if successful, empty string otherwise
	 *
	 * Example:
	 *   node_id = unreal.BlueprintService.add_math_node("/Game/BP_Player", "ApplyDamage", "Subtract", "Float", 300, 100)
	 *   node_id = unreal.BlueprintService.add_math_node("/Game/BP_Player", "ApplyDamage", "Clamp", "Float", 400, 100)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static FString AddMathNode(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& MathOperation,
		const FString& ValueType = TEXT("Float"),
		float PosX = 0.0f,
		float PosY = 0.0f
	);

	/**
	 * Get all connections in a graph.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @return Array of connection information
	 *
	 * Example:
	 *   connections = unreal.BlueprintService.get_connections("/Game/BP_Player", "ApplyDamage")
	 *   for conn in connections:
	 *       print(f"{conn.source_node_title}.{conn.source_pin_name} -> {conn.target_node_title}.{conn.target_pin_name}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static TArray<FBlueprintConnectionInfo> GetConnections(
		const FString& BlueprintPath,
		const FString& GraphName
	);

	/**
	 * Get detailed pin information for a specific node.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @param NodeId - GUID of the node
	 * @return Array of pin information
	 *
	 * Example:
	 *   pins = unreal.BlueprintService.get_node_pins("/Game/BP_Player", "ApplyDamage", "45CC026642D99D1D713EDCA5C483E490")
	 *   for pin in pins:
	 *       print(f"{pin.pin_name} ({pin.pin_type}) - {'input' if pin.is_input else 'output'}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static TArray<FBlueprintPinInfo> GetNodePins(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& NodeId
	);

	/**
	 * Disconnect a pin from all its connections.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @param NodeId - GUID of the node
	 * @param PinName - Name of the pin to disconnect
	 * @return True if any connections were broken
	 *
	 * Example:
	 *   unreal.BlueprintService.disconnect_pin("/Game/BP_Player", "ApplyDamage", node_id, "then")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool DisconnectPin(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& NodeId,
		const FString& PinName
	);

	/**
	 * Delete a node from a graph.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @param NodeId - GUID of the node to delete
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.delete_node("/Game/BP_Player", "ApplyDamage", node_id)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool DeleteNode(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& NodeId
	);

	/**
	 * Set the position of a node in a graph.
	 * Use this to reposition Entry/Result nodes for clean layouts.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @param NodeId - GUID of the node to reposition
	 * @param PosX - New X position in the graph
	 * @param PosY - New Y position in the graph
	 * @return True if successful
	 *
	 * Example - Reposition Result node to end of function:
	 *   unreal.BlueprintService.set_node_position("/Game/BP_Player", "ApplyDamage", result_id, 800, 0)
	 *
	 * Example - Separate stacked Entry/Result nodes:
	 *   nodes = unreal.BlueprintService.get_nodes_in_graph(path, func)
	 *   for node in nodes:
	 *       if "FunctionEntry" in node.node_type:
	 *           unreal.BlueprintService.set_node_position(path, func, node.node_id, 0, 0)
	 *       elif "FunctionResult" in node.node_type:
	 *           unreal.BlueprintService.set_node_position(path, func, node.node_id, 800, 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool SetNodePosition(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& NodeId,
		float PosX = 0.0f,
		float PosY = 0.0f
	);

	// ============================================================================
	// LIFECYCLE & PROPERTY MANAGEMENT (Missing manage_blueprint Actions)
	// ============================================================================

	/**
	 * Create a new blueprint from a parent class.
	 *
	 * @param BlueprintName - Name of the blueprint to create
	 * @param ParentClass - Parent class name or path (e.g., "Actor", "Character", "/Script/Engine.Actor")
	 * @param BlueprintPath - Directory path where blueprint will be created (e.g., "/Game/Blueprints")
	 * @return Full path to created blueprint, or empty string on failure
	 *
	 * Example:
	 *   path = unreal.BlueprintService.create_blueprint("BP_MyActor", "Actor", "/Game/Blueprints")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static FString CreateBlueprint(
		const FString& BlueprintName,
		const FString& ParentClass,
		const FString& BlueprintPath
	);

	/**
	 * Compile a blueprint.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.compile_blueprint("/Game/BP_Player")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool CompileBlueprint(const FString& BlueprintPath);

	/**
	 * Get a property value from a blueprint's Class Default Object (CDO).
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param PropertyName - Name of the property to get
	 * @param OutValue - Property value as a string (C++ only, becomes return value in Python)
	 * @return True if successful
	 *
	 * Python Usage (out params become return values):
	 *   success, value = unreal.BlueprintService.get_property("/Game/BP_Player", "Health")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool GetProperty(
		const FString& BlueprintPath,
		const FString& PropertyName,
		FString& OutValue
	);

	/**
	 * Set a property value on a blueprint's Class Default Object (CDO).
	 * WARNING: Modifying CDO can cause instability. Prefer using variables and defaults instead.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param PropertyName - Name of the property to set
	 * @param PropertyValue - Value to set as a string
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.set_property("/Game/BP_Player", "Health", "150.0")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool SetProperty(
		const FString& BlueprintPath,
		const FString& PropertyName,
		const FString& PropertyValue
	);

	/**
	 * Change the parent class of a blueprint (reparent).
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param NewParentClass - New parent class name or path
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.reparent_blueprint("/Game/BP_Player", "Character")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool ReparentBlueprint(
		const FString& BlueprintPath,
		const FString& NewParentClass
	);

	/**
	 * Compare two blueprints and return differences as string.
	 * Compares variables, functions, components, and parent classes.
	 *
	 * @param BlueprintPathA - Full path to first blueprint
	 * @param BlueprintPathB - Full path to second blueprint
	 * @param OutDifferences - Description of differences (C++ only, becomes return value in Python)
	 * @return True if differences found, false if identical
	 *
	 * Python Usage (out params become return values):
	 *   has_diff, diff_text = unreal.BlueprintService.diff_blueprints("/Game/BP_Player", "/Game/BP_Enemy")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool DiffBlueprints(
		const FString& BlueprintPathA,
		const FString& BlueprintPathB,
		FString& OutDifferences
	);

	// ============================================================================
	// NODE MANAGEMENT - Advanced Operations (manage_blueprint_node actions)
	// ============================================================================

	/**
	 * Discover available node types that can be created in a blueprint.
	 * Mimics the Blueprint editor's "Add Node" context menu.
	 *
	 * @param BlueprintPath - Full path to the blueprint (for context-aware suggestions)
	 * @param SearchTerm - Search term to filter nodes (partial match)
	 * @param Category - Optional category filter (e.g., "Math", "Flow Control")
	 * @param MaxResults - Maximum number of results (default 20)
	 * @return Array of available node types
	 *
	 * Example - Search for print nodes:
	 *   nodes = unreal.BlueprintService.discover_nodes("/Game/BP_Player", "Print")
	 *
	 * Example - Get math nodes:
	 *   nodes = unreal.BlueprintService.discover_nodes("/Game/BP_Player", "", "Math")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Nodes")
	static TArray<FBlueprintNodeTypeInfo> DiscoverNodes(
		const FString& BlueprintPath,
		const FString& SearchTerm = TEXT(""),
		const FString& Category = TEXT(""),
		int32 MaxResults = 20
	);

	/**
	 * Get detailed information about a specific node in a graph.
	 * Returns complete pin information including connections.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @param NodeId - GUID of the node
	 * @param OutInfo - Detailed node information (C++ only)
	 * @return True if successful
	 *
	 * Example:
	 *   success, info = unreal.BlueprintService.get_node_details("/Game/BP_Player", "EventGraph", node_id)
	 *   if success:
	 *       for pin in info.input_pins:
	 *           print(f"  {pin.pin_name}: {pin.pin_category}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Nodes")
	static bool GetNodeDetails(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& NodeId,
		FBlueprintNodeDetailedInfo& OutInfo
	);

	/**
	 * Set a pin's default value on a node.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @param NodeId - GUID of the node
	 * @param PinName - Name of the pin to set
	 * @param Value - Value to set as string
	 * @return True if successful
	 *
	 * Example - Set string value:
	 *   unreal.BlueprintService.set_node_pin_value("/Game/BP_Player", "EventGraph", node_id, "InString", "Hello World")
	 *
	 * Example - Set numeric value:
	 *   unreal.BlueprintService.set_node_pin_value("/Game/BP_Player", "ApplyDamage", node_id, "B", "2.5")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Nodes")
	static bool SetNodePinValue(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& NodeId,
		const FString& PinName,
		const FString& Value
	);

	/**
	 * Split a struct pin into individual member pins.
	 * Works on struct types like FVector, FRotator, FTransform, etc.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @param NodeId - GUID of the node
	 * @param PinName - Name of the struct pin to split
	 * @return True if successful
	 *
	 * Example - Split a Vector output:
	 *   unreal.BlueprintService.split_pin("/Game/BP_Player", "EventGraph", node_id, "ReturnValue")
	 *   # Now you can connect to ReturnValue_X, ReturnValue_Y, ReturnValue_Z
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Nodes")
	static bool SplitPin(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& NodeId,
		const FString& PinName
	);

	/**
	 * Recombine a previously split pin back into a single struct pin.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @param NodeId - GUID of the node
	 * @param PinName - Base name of the split pin to recombine
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.recombine_pin("/Game/BP_Player", "EventGraph", node_id, "ReturnValue")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Nodes")
	static bool RecombinePin(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& NodeId,
		const FString& PinName
	);

	/**
	 * Refresh/reconstruct a node to update its pins and connections.
	 * Useful after modifying a function signature or when pins are out of date.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @param NodeId - GUID of the node to refresh
	 * @param bCompile - Whether to compile the blueprint after refresh (default: true)
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.refresh_node("/Game/BP_Player", "EventGraph", node_id)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Nodes")
	static bool RefreshNode(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& NodeId,
		bool bCompile = true
	);

	/**
	 * Configure node-specific settings. This is for setting internal node properties
	 * that are not exposed as pins (like class selection on spawn nodes).
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @param NodeId - GUID of the node to configure
	 * @param PropertyName - Name of the property to set
	 * @param Value - Value to set as string
	 * @return True if successful
	 *
	 * Example - Configure a SpawnActorFromClass node:
	 *   unreal.BlueprintService.configure_node("/Game/BP_Spawner", "SpawnEnemy", node_id, "ActorClass", "/Game/BP_Enemy")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Nodes")
	static bool ConfigureNode(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& NodeId,
		const FString& PropertyName,
		const FString& Value
	);

	/**
	 * Create a node by spawner key (discovered via discover_nodes).
	 * This is the most flexible node creation method.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @param SpawnerKey - Spawner key from discover_nodes (e.g., "UK2Node_CallFunction /Script/Engine.KismetMathLibrary:Clamp")
	 * @param PosX - X position in the graph
	 * @param PosY - Y position in the graph
	 * @return Node ID (GUID) if successful, empty string otherwise
	 *
	 * Example:
	 *   # First discover the node
	 *   nodes = unreal.BlueprintService.discover_nodes("/Game/BP_Player", "Clamp")
	 *   # Then create it using the spawner_key
	 *   node_id = unreal.BlueprintService.create_node_by_key("/Game/BP_Player", "EventGraph", nodes[0].spawner_key, 100, 100)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Nodes")
	static FString CreateNodeByKey(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& SpawnerKey,
		float PosX = 0.0f,
		float PosY = 0.0f
	);

	// ============================================================================
	// COMPONENT OPERATIONS - Extended API
	// ============================================================================

	/**
	 * List all properties of a component in a blueprint.
	 * This is an alias for GetAllComponentProperties with a more intuitive name.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param ComponentName - Name of the component
	 * @param bIncludeInherited - Whether to include inherited properties (default: true)
	 * @return Array of property information
	 *
	 * Example:
	 *   props = unreal.BlueprintService.list_component_properties("/Game/BP_Player", "Mesh")
	 *   for prop in props:
	 *       print(f"{prop.property_name}: {prop.property_type} = {prop.value}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Components")
	static TArray<FComponentPropertyInfo> ListComponentProperties(
		const FString& BlueprintPath,
		const FString& ComponentName,
		bool bIncludeInherited = true
	);

	/**
	 * Set a component as the root component of the blueprint.
	 * The component must be a SceneComponent and must exist in the blueprint.
	 * The previous root's children will be reparented to the new root.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param ComponentName - Name of the component to make root
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.BlueprintService.set_root_component("/Game/BP_Player", "MyNewRoot")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Components")
	static bool SetRootComponent(
		const FString& BlueprintPath,
		const FString& ComponentName
	);

	/**
	 * Compare properties of two components and return the differences.
	 * Components can be in the same or different blueprints.
	 *
	 * @param BlueprintPathA - Full path to the first blueprint
	 * @param ComponentNameA - Name of the first component
	 * @param BlueprintPathB - Full path to the second blueprint (or same as A)
	 * @param ComponentNameB - Name of the second component
	 * @param OutDifferences - String containing the differences
	 * @return True if comparison succeeded (even if no differences)
	 *
	 * Example:
	 *   success, diff = unreal.BlueprintService.compare_components(
	 *       "/Game/BP_Player", "Mesh",
	 *       "/Game/BP_Enemy", "Mesh"
	 *   )
	 *   print(diff)  # Shows property differences
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Components")
	static bool CompareComponents(
		const FString& BlueprintPathA,
		const FString& ComponentNameA,
		const FString& BlueprintPathB,
		const FString& ComponentNameB,
		FString& OutDifferences
	);

	// ============================================================================
	// EXISTENCE CHECKS - Fast boolean checks before creation (Idempotency)
	// ============================================================================

	/**
	 * Check if a blueprint exists at the given path.
	 *
	 * @param BlueprintPath - Full path to the blueprint (e.g., "/Game/Blueprints/BP_Player")
	 * @return True if blueprint exists
	 *
	 * Example:
	 *   if not unreal.BlueprintService.blueprint_exists("/Game/Blueprints/BP_Enemy"):
	 *       unreal.BlueprintService.create_blueprint("BP_Enemy", "Actor", "/Game/Blueprints")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Exists")
	static bool BlueprintExists(const FString& BlueprintPath);

	/**
	 * Check if a variable exists in a blueprint.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param VariableName - Name of the variable (case-insensitive)
	 * @return True if variable exists
	 *
	 * Example:
	 *   if not unreal.BlueprintService.variable_exists(bp_path, "Health"):
	 *       unreal.BlueprintService.add_variable(bp_path, "Health", "float")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Exists")
	static bool VariableExists(const FString& BlueprintPath, const FString& VariableName);

	/**
	 * Check if a function exists in a blueprint.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param FunctionName - Name of the function (case-insensitive)
	 * @return True if function exists
	 *
	 * Example:
	 *   if not unreal.BlueprintService.function_exists(bp_path, "ApplyDamage"):
	 *       unreal.BlueprintService.create_function(bp_path, "ApplyDamage")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Exists")
	static bool FunctionExists(const FString& BlueprintPath, const FString& FunctionName);

	/**
	 * Check if a component exists in a blueprint.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param ComponentName - Name of the component (case-insensitive)
	 * @return True if component exists
	 *
	 * Example:
	 *   if not unreal.BlueprintService.component_exists(bp_path, "Mesh"):
	 *       unreal.BlueprintService.add_component(bp_path, "StaticMeshComponent", "Mesh")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Exists")
	static bool ComponentExists(const FString& BlueprintPath, const FString& ComponentName);

	/**
	 * Check if a local variable exists in a function.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param FunctionName - Name of the function
	 * @param VariableName - Name of the local variable (case-insensitive)
	 * @return True if local variable exists
	 *
	 * Example:
	 *   if not unreal.BlueprintService.local_variable_exists(bp_path, "ApplyDamage", "TempValue"):
	 *       unreal.BlueprintService.add_function_local_variable(bp_path, "ApplyDamage", "TempValue", "float")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Exists")
	static bool LocalVariableExists(
		const FString& BlueprintPath,
		const FString& FunctionName,
		const FString& VariableName
	);

	/**
	 * Check if a node with the given title exists in a graph.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph ("EventGraph", function name, etc.)
	 * @param NodeTitle - Node title to search for (case-insensitive)
	 * @return True if a node with matching title exists
	 *
	 * Example:
	 *   if not unreal.BlueprintService.node_exists(bp_path, "EventGraph", "Event BeginPlay"):
	 *       # Add BeginPlay event
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Exists")
	static bool NodeExists(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& NodeTitle
	);

	/**
	 * Check if a function call node exists in a graph (calls a specific function).
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph
	 * @param FunctionName - Function name being called (case-insensitive)
	 * @return True if a call to that function exists
	 *
	 * Example:
	 *   if not unreal.BlueprintService.function_call_exists(bp_path, "EventGraph", "PrintString"):
	 *       unreal.BlueprintService.add_print_string_node(bp_path, "EventGraph", 400, 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints|Exists")
	static bool FunctionCallExists(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& FunctionName
	);

private:
	/** Helper to load blueprint from path */
	static UBlueprint* LoadBlueprint(const FString& BlueprintPath);

	/** Helper to find a graph by name */
	static UEdGraph* FindGraph(UBlueprint* Blueprint, const FString& GraphName);

	/** Helper to find a node by ID in a graph */
	static UEdGraphNode* FindNodeById(UEdGraph* Graph, const FString& NodeId);
};
