// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Core/ToolRegistry.h"
#include "PythonAPI/UStateTreeService.h"
#include "Json.h"
#include "JsonUtilities.h"

DEFINE_LOG_CATEGORY_STATIC(LogStateTreeTools, Log, All);

// -----------------------------------------------------------------------
// Local param extraction helper (consistent with other tool files)
// -----------------------------------------------------------------------
static FString ExtractParamFromJson(const TMap<FString, FString>& Params, const FString& Key)
{
	const FString* Found = Params.Find(Key);
	return Found ? *Found : FString();
}

// -----------------------------------------------------------------------
// Helper: serialize FStateTreeInfo to JSON string
// -----------------------------------------------------------------------
static FString StateTreeInfoToJson(const FStateTreeInfo& Info)
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("assetPath"), Info.AssetPath);
	Root->SetStringField(TEXT("assetName"), Info.AssetName);
	Root->SetStringField(TEXT("schemaClass"), Info.SchemaClass);
	Root->SetBoolField(TEXT("isCompiled"), Info.bIsCompiled);
	Root->SetStringField(TEXT("lastCompileStatus"), Info.LastCompileStatus);

	auto NodeInfoArray = [](const TArray<FStateTreeNodeInfo>& Nodes) -> TArray<TSharedPtr<FJsonValue>>
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FStateTreeNodeInfo& N : Nodes)
		{
			TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("name"), N.Name);
			Obj->SetStringField(TEXT("structType"), N.StructType);
			Obj->SetBoolField(TEXT("enabled"), N.bEnabled);
			Arr.Add(MakeShared<FJsonValueObject>(Obj));
		}
		return Arr;
	};

	Root->SetArrayField(TEXT("evaluators"), NodeInfoArray(Info.Evaluators));
	Root->SetArrayField(TEXT("globalTasks"), NodeInfoArray(Info.GlobalTasks));

	TArray<TSharedPtr<FJsonValue>> StatesArr;
	for (const FStateTreeStateInfo& State : Info.AllStates)
	{
		TSharedPtr<FJsonObject> StateObj = MakeShared<FJsonObject>();
		StateObj->SetStringField(TEXT("name"), State.Name);
		StateObj->SetStringField(TEXT("path"), State.Path);
		StateObj->SetStringField(TEXT("type"), State.StateType);
		StateObj->SetStringField(TEXT("selectionBehavior"), State.SelectionBehavior);
		StateObj->SetBoolField(TEXT("enabled"), State.bEnabled);
		StateObj->SetArrayField(TEXT("tasks"), NodeInfoArray(State.Tasks));
		StateObj->SetArrayField(TEXT("enterConditions"), NodeInfoArray(State.EnterConditions));

		TArray<TSharedPtr<FJsonValue>> TransArr;
		for (const FStateTreeTransitionInfo& T : State.Transitions)
		{
			TSharedPtr<FJsonObject> TObj = MakeShared<FJsonObject>();
			TObj->SetStringField(TEXT("trigger"), T.Trigger);
			TObj->SetStringField(TEXT("transitionType"), T.TransitionType);
			TObj->SetStringField(TEXT("targetPath"), T.TargetStatePath);
			TObj->SetStringField(TEXT("targetName"), T.TargetStateName);
			TObj->SetStringField(TEXT("priority"), T.Priority);
			TObj->SetBoolField(TEXT("enabled"), T.bEnabled);
			TransArr.Add(MakeShared<FJsonValueObject>(TObj));
		}
		StateObj->SetArrayField(TEXT("transitions"), TransArr);

		TArray<TSharedPtr<FJsonValue>> ChildArr;
		for (const FString& ChildPath : State.ChildPaths)
		{
			ChildArr.Add(MakeShared<FJsonValueString>(ChildPath));
		}
		StateObj->SetArrayField(TEXT("childPaths"), ChildArr);

		StatesArr.Add(MakeShared<FJsonValueObject>(StateObj));
	}
	Root->SetArrayField(TEXT("allStates"), StatesArr);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Output;
}

// -----------------------------------------------------------------------
// Helper: serialize FStateTreeCompileResult to JSON
// -----------------------------------------------------------------------
static FString CompileResultToJson(const FStateTreeCompileResult& Result)
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("success"), Result.bSuccess);

	TArray<TSharedPtr<FJsonValue>> Errors;
	for (const FString& E : Result.Errors)
	{
		Errors.Add(MakeShared<FJsonValueString>(E));
	}
	Root->SetArrayField(TEXT("errors"), Errors);

	TArray<TSharedPtr<FJsonValue>> Warnings;
	for (const FString& W : Result.Warnings)
	{
		Warnings.Add(MakeShared<FJsonValueString>(W));
	}
	Root->SetArrayField(TEXT("warnings"), Warnings);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Output;
}

// ============================================================
// Tool: list_state_trees
// ============================================================
REGISTER_VIBEUE_TOOL(list_state_trees,
	"List all StateTree assets under a content directory. "
	"Returns an array of asset paths that can be used with other state tree tools.",
	"StateTree",
	TOOL_PARAMS(
		TOOL_PARAM("directory", "Content directory to search (default: /Game)", "string", false)
	),
	{
		const FString Dir = ExtractParamFromJson(Params, TEXT("directory"));
		const FString SearchPath = Dir.IsEmpty() ? TEXT("/Game") : Dir;

		TArray<FString> StateTrees = UStateTreeService::ListStateTrees(SearchPath);

		TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FString& Path : StateTrees)
		{
			Arr.Add(MakeShared<FJsonValueString>(Path));
		}
		Root->SetArrayField(TEXT("stateTrees"), Arr);
		Root->SetNumberField(TEXT("count"), StateTrees.Num());

		FString Output;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
		return Output;
	}
);

// ============================================================
// Tool: get_state_tree_info
// ============================================================
REGISTER_VIBEUE_TOOL(get_state_tree_info,
	"Get detailed structural info about a StateTree asset: schema, evaluators, global tasks, "
	"and all states with their tasks, conditions, transitions, and children.",
	"StateTree",
	TOOL_PARAMS(
		TOOL_PARAM("assetPath", "Content path to the StateTree asset (e.g. /Game/AI/MyStateTree)", "string", true)
	),
	{
		const FString AssetPath = ExtractParamFromJson(Params, TEXT("assetPath"));
		FStateTreeInfo Info;
		const bool bOk = UStateTreeService::GetStateTreeInfo(AssetPath, Info);
		if (!bOk)
		{
			return FString::Printf(TEXT("{\"error\": \"Failed to load StateTree: %s\"}"), *AssetPath);
		}
		return StateTreeInfoToJson(Info);
	}
);

// ============================================================
// Tool: create_state_tree
// ============================================================
REGISTER_VIBEUE_TOOL(create_state_tree,
	"Create a new StateTree asset at the given content path. "
	"After creation, use add_state to add states and compile_state_tree to compile. "
	"Schema defaults to StateTreeComponentSchema. Other options: StateTreeAIComponentSchema, BrainComponentStateTreeSchema.",
	"StateTree",
	TOOL_PARAMS(
		TOOL_PARAM("assetPath", "Full content path including asset name (e.g. /Game/AI/MyBehavior)", "string", true),
		TOOL_PARAM("schemaClass", "Schema class name (default: StateTreeComponentSchema). Shorthand accepted: Component, AIComponent", "string", false)
	),
	{
		const FString AssetPath = ExtractParamFromJson(Params, TEXT("assetPath"));
		const FString SchemaClass = ExtractParamFromJson(Params, TEXT("schemaClass"));
		const bool bOk = UStateTreeService::CreateStateTree(AssetPath, SchemaClass);
		return FString::Printf(TEXT("{\"success\": %s, \"assetPath\": \"%s\"}"),
		                       bOk ? TEXT("true") : TEXT("false"), *AssetPath);
	}
);

// ============================================================
// Tool: add_state
// ============================================================
REGISTER_VIBEUE_TOOL(add_state,
	"Add a state to a StateTree. "
	"Leave parentPath empty to create a new top-level subtree (like 'Root'). "
	"Use 'Root' as parentPath to add a direct child of Root. "
	"State types: State (default), Group, Subtree, Linked, LinkedAsset. "
	"After adding states, you must call compile_state_tree.",
	"StateTree",
	TOOL_PARAMS(
		TOOL_PARAM("assetPath", "Content path to the StateTree", "string", true),
		TOOL_PARAM("parentPath", "Path of parent state (e.g. 'Root') or empty for a top-level subtree", "string", false),
		TOOL_PARAM("stateName", "Name for the new state", "string", true),
		TOOL_PARAM("stateType", "State type: State, Group, Subtree, Linked, LinkedAsset (default: State)", "string", false)
	),
	{
		const FString AssetPath = ExtractParamFromJson(Params, TEXT("assetPath"));
		const FString ParentPath = ExtractParamFromJson(Params, TEXT("parentPath"));
		const FString StateName = ExtractParamFromJson(Params, TEXT("stateName"));
		const FString StateType = ExtractParamFromJson(Params, TEXT("stateType"));

		const bool bOk = UStateTreeService::AddState(AssetPath, ParentPath, StateName,
		                                              StateType.IsEmpty() ? TEXT("State") : StateType);
		return FString::Printf(TEXT("{\"success\": %s, \"stateName\": \"%s\", \"parentPath\": \"%s\"}"),
		                       bOk ? TEXT("true") : TEXT("false"), *StateName, *ParentPath);
	}
);

// ============================================================
// Tool: remove_state
// ============================================================
REGISTER_VIBEUE_TOOL(remove_state,
	"Remove a state (and all its children) from a StateTree by path. "
	"Example path: 'Root/Walking' removes the Walking state and all its children.",
	"StateTree",
	TOOL_PARAMS(
		TOOL_PARAM("assetPath", "Content path to the StateTree", "string", true),
		TOOL_PARAM("statePath", "Path of the state to remove (e.g. 'Root/Walking')", "string", true)
	),
	{
		const FString AssetPath = ExtractParamFromJson(Params, TEXT("assetPath"));
		const FString StatePath = ExtractParamFromJson(Params, TEXT("statePath"));
		const bool bOk = UStateTreeService::RemoveState(AssetPath, StatePath);
		return FString::Printf(TEXT("{\"success\": %s, \"statePath\": \"%s\"}"),
		                       bOk ? TEXT("true") : TEXT("false"), *StatePath);
	}
);

// ============================================================
// Tool: set_state_enabled
// ============================================================
REGISTER_VIBEUE_TOOL(set_state_enabled,
	"Enable or disable a state in a StateTree. Disabled states are skipped during selection.",
	"StateTree",
	TOOL_PARAMS(
		TOOL_PARAM("assetPath", "Content path to the StateTree", "string", true),
		TOOL_PARAM("statePath", "Path of the state (e.g. 'Root/Idle')", "string", true),
		TOOL_PARAM("enabled", "true to enable, false to disable", "bool", true)
	),
	{
		const FString AssetPath = ExtractParamFromJson(Params, TEXT("assetPath"));
		const FString StatePath = ExtractParamFromJson(Params, TEXT("statePath"));
		const FString EnabledStr = ExtractParamFromJson(Params, TEXT("enabled"));
		const bool bEnabled = EnabledStr == TEXT("true") || EnabledStr == TEXT("1");

		const bool bOk = UStateTreeService::SetStateEnabled(AssetPath, StatePath, bEnabled);
		return FString::Printf(TEXT("{\"success\": %s, \"statePath\": \"%s\", \"enabled\": %s}"),
		                       bOk ? TEXT("true") : TEXT("false"), *StatePath,
		                       bEnabled ? TEXT("true") : TEXT("false"));
	}
);

// ============================================================
// Tool: add_task_to_state
// ============================================================
REGISTER_VIBEUE_TOOL(add_task_to_state,
	"Add a task to a specific state in a StateTree. "
	"Use get_available_task_types to see what task structs are available. "
	"Common built-in tasks: FStateTreeDelayTask (wait for duration), "
	"FStateTreeRunSubtreeTask (run a subtree), FStateTreeSetParameterTask (set a parameter). "
	"After adding tasks, you must call compile_state_tree.",
	"StateTree",
	TOOL_PARAMS(
		TOOL_PARAM("assetPath", "Content path to the StateTree", "string", true),
		TOOL_PARAM("statePath", "Path of the target state (e.g. 'Root/Walking')", "string", true),
		TOOL_PARAM("taskStructName", "C++ struct name of the task (e.g. 'FStateTreeDelayTask')", "string", true)
	),
	{
		const FString AssetPath = ExtractParamFromJson(Params, TEXT("assetPath"));
		const FString StatePath = ExtractParamFromJson(Params, TEXT("statePath"));
		const FString TaskName = ExtractParamFromJson(Params, TEXT("taskStructName"));

		const bool bOk = UStateTreeService::AddTask(AssetPath, StatePath, TaskName);
		return FString::Printf(TEXT("{\"success\": %s, \"task\": \"%s\", \"state\": \"%s\"}"),
		                       bOk ? TEXT("true") : TEXT("false"), *TaskName, *StatePath);
	}
);

// ============================================================
// Tool: add_evaluator
// ============================================================
REGISTER_VIBEUE_TOOL(add_evaluator,
	"Add a global evaluator to a StateTree. Evaluators run every tick and provide data "
	"accessible by all states. Use get_available_evaluator_types to see available evaluator types.",
	"StateTree",
	TOOL_PARAMS(
		TOOL_PARAM("assetPath", "Content path to the StateTree", "string", true),
		TOOL_PARAM("evaluatorStructName", "C++ struct name of the evaluator", "string", true)
	),
	{
		const FString AssetPath = ExtractParamFromJson(Params, TEXT("assetPath"));
		const FString EvalName = ExtractParamFromJson(Params, TEXT("evaluatorStructName"));

		const bool bOk = UStateTreeService::AddEvaluator(AssetPath, EvalName);
		return FString::Printf(TEXT("{\"success\": %s, \"evaluator\": \"%s\"}"),
		                       bOk ? TEXT("true") : TEXT("false"), *EvalName);
	}
);

// ============================================================
// Tool: add_global_task
// ============================================================
REGISTER_VIBEUE_TOOL(add_global_task,
	"Add a global task to a StateTree. Global tasks run as long as the StateTree is active, "
	"regardless of which state is active.",
	"StateTree",
	TOOL_PARAMS(
		TOOL_PARAM("assetPath", "Content path to the StateTree", "string", true),
		TOOL_PARAM("taskStructName", "C++ struct name of the task (e.g. 'FStateTreeDelayTask')", "string", true)
	),
	{
		const FString AssetPath = ExtractParamFromJson(Params, TEXT("assetPath"));
		const FString TaskName = ExtractParamFromJson(Params, TEXT("taskStructName"));

		const bool bOk = UStateTreeService::AddGlobalTask(AssetPath, TaskName);
		return FString::Printf(TEXT("{\"success\": %s, \"task\": \"%s\"}"),
		                       bOk ? TEXT("true") : TEXT("false"), *TaskName);
	}
);

// ============================================================
// Tool: add_transition
// ============================================================
REGISTER_VIBEUE_TOOL(add_transition,
	"Add a transition to a state in a StateTree. "
	"Triggers: OnStateCompleted, OnStateSucceeded, OnStateFailed, OnTick, OnEvent. "
	"Transition types: GotoState (requires targetPath), Succeeded, Failed, NextState, NextSelectableState. "
	"Priorities: Low, Normal (default), Medium, High, Critical.",
	"StateTree",
	TOOL_PARAMS(
		TOOL_PARAM("assetPath", "Content path to the StateTree", "string", true),
		TOOL_PARAM("statePath", "Path of the state to add the transition to (e.g. 'Root/Walking')", "string", true),
		TOOL_PARAM("trigger", "When to trigger: OnStateCompleted, OnStateSucceeded, OnStateFailed, OnTick, OnEvent", "string", true),
		TOOL_PARAM("transitionType", "What to do: GotoState, Succeeded, Failed, NextState, NextSelectableState", "string", true),
		TOOL_PARAM("targetPath", "Path of target state (only for GotoState, e.g. 'Root/Idle')", "string", false),
		TOOL_PARAM("priority", "Priority: Low, Normal, Medium, High, Critical (default: Normal)", "string", false)
	),
	{
		const FString AssetPath    = ExtractParamFromJson(Params, TEXT("assetPath"));
		const FString StatePath    = ExtractParamFromJson(Params, TEXT("statePath"));
		const FString Trigger      = ExtractParamFromJson(Params, TEXT("trigger"));
		const FString TransType    = ExtractParamFromJson(Params, TEXT("transitionType"));
		const FString TargetPath   = ExtractParamFromJson(Params, TEXT("targetPath"));
		const FString Priority     = ExtractParamFromJson(Params, TEXT("priority"));

		const bool bOk = UStateTreeService::AddTransition(AssetPath, StatePath, Trigger, TransType,
		                                                    TargetPath,
		                                                    Priority.IsEmpty() ? TEXT("Normal") : Priority);
		return FString::Printf(TEXT("{\"success\": %s, \"state\": \"%s\", \"trigger\": \"%s\", \"type\": \"%s\"}"),
		                       bOk ? TEXT("true") : TEXT("false"), *StatePath, *Trigger, *TransType);
	}
);

// ============================================================
// Tool: compile_state_tree
// ============================================================
REGISTER_VIBEUE_TOOL(compile_state_tree,
	"Compile a StateTree asset. Must be called after adding states, tasks, evaluators, or transitions. "
	"Returns success status along with any errors or warnings from the compiler.",
	"StateTree",
	TOOL_PARAMS(
		TOOL_PARAM("assetPath", "Content path to the StateTree", "string", true)
	),
	{
		const FString AssetPath = ExtractParamFromJson(Params, TEXT("assetPath"));
		const FStateTreeCompileResult Result = UStateTreeService::CompileStateTree(AssetPath);
		return CompileResultToJson(Result);
	}
);

// ============================================================
// Tool: save_state_tree
// ============================================================
REGISTER_VIBEUE_TOOL(save_state_tree,
	"Save a StateTree asset to disk. Call this after compiling to persist changes.",
	"StateTree",
	TOOL_PARAMS(
		TOOL_PARAM("assetPath", "Content path to the StateTree", "string", true)
	),
	{
		const FString AssetPath = ExtractParamFromJson(Params, TEXT("assetPath"));
		const bool bOk = UStateTreeService::SaveStateTree(AssetPath);
		return FString::Printf(TEXT("{\"success\": %s, \"assetPath\": \"%s\"}"),
		                       bOk ? TEXT("true") : TEXT("false"), *AssetPath);
	}
);

// ============================================================
// Tool: get_available_task_types
// ============================================================
REGISTER_VIBEUE_TOOL(get_available_task_types,
	"List all registered StateTree task struct types. "
	"These are the struct names you can use with add_task_to_state and add_global_task.",
	"StateTree",
	TOOL_PARAMS(),
	{
		TArray<FString> Types = UStateTreeService::GetAvailableTaskTypes();

		TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FString& T : Types)
		{
			Arr.Add(MakeShared<FJsonValueString>(T));
		}
		Root->SetArrayField(TEXT("taskTypes"), Arr);
		Root->SetNumberField(TEXT("count"), Types.Num());

		FString Output;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
		return Output;
	}
);

// ============================================================
// Tool: get_available_evaluator_types
// ============================================================
REGISTER_VIBEUE_TOOL(get_available_evaluator_types,
	"List all registered StateTree evaluator struct types. "
	"These are the struct names you can use with add_evaluator.",
	"StateTree",
	TOOL_PARAMS(),
	{
		TArray<FString> Types = UStateTreeService::GetAvailableEvaluatorTypes();

		TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FString& T : Types)
		{
			Arr.Add(MakeShared<FJsonValueString>(T));
		}
		Root->SetArrayField(TEXT("evaluatorTypes"), Arr);
		Root->SetNumberField(TEXT("count"), Types.Num());

		FString Output;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
		return Output;
	}
);
