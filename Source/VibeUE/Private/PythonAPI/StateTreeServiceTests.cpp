// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UStateTreeService.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/StateTreeEvaluatorBlueprintBase.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "UObject/Package.h"

// Regression coverage for the Blueprint skeleton-class crash in add_task / add_global_task /
// add_evaluator. A Blueprint's skeleton class (SKEL_<Name>_C) passed the old "child of the Blueprint
// base" check, so the loaded-class listers returned SKEL_ names and an agent that copied one into
// add_task made the service NewObject the skeleton class, which crashed the editor.
//
// ORDER MATTERS: each test checks the lister FIRST and returns before any add_* call if the lister
// still shows a SKEL_ name. On a build without the fix the test fails at that first assertion and
// never reaches the crashing NewObject path.
//
// Headless-safe: the Blueprints live in the transient package and the StateTree is an in-memory,
// never-saved /Game asset (the service resolves trees by path). Nothing is rooted; cleanup drops the
// standalone flags so GC reclaims everything.

namespace VibeStateTreeServiceTests
{
	static const EAutomationTestFlags kTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	static const TCHAR* kStaleBlueprintMessage = TEXT("Compile the Blueprint first");

	static FString UniqueSuffix()
	{
		return FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
	}

	/** Creates and compiles a transient Blueprint of ParentClass. */
	static UBlueprint* CreateCompiledBlueprint(UClass* ParentClass, const FString& NamePrefix)
	{
		const FName Name(*FString::Printf(TEXT("%s_%s"), *NamePrefix, *UniqueSuffix()));
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass, GetTransientPackage(), Name,
			BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("StateTreeServiceTests"));
		if (Blueprint)
		{
			Blueprint->SetFlags(RF_Transient);
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
		}
		return Blueprint;
	}

	static void ReleaseBlueprint(UBlueprint* Blueprint)
	{
		if (Blueprint)
		{
			Blueprint->ClearFlags(RF_Standalone | RF_Public);
		}
	}

	/** Creates an in-memory StateTree through the service; returns it and its asset path. */
	static UStateTree* CreateServiceStateTree(FAutomationTestBase& Test, FString& OutAssetPath)
	{
		const TArray<FString> Schemas = UStateTreeService::ListStateTreeSchemas();
		if (!Test.TestTrue(TEXT("at least one StateTree schema is available"), Schemas.Num() > 0))
		{
			return nullptr;
		}
		const FString Schema = Schemas.Contains(TEXT("StateTreeComponentSchema"))
			? FString(TEXT("StateTreeComponentSchema"))
			: Schemas[0];

		const FString AssetName = FString::Printf(TEXT("ST_SkelGuard_%s"), *UniqueSuffix());
		OutAssetPath = FString(TEXT("/Game/__VibeUETest/")) + AssetName;
		if (!Test.TestTrue(TEXT("CreateStateTree succeeded"), UStateTreeService::CreateStateTree(OutAssetPath, Schema)))
		{
			return nullptr;
		}

		const FString ObjectPath = OutAssetPath + TEXT(".") + AssetName;
		UStateTree* Tree = FindObject<UStateTree>(nullptr, *ObjectPath);
		Test.TestNotNull(TEXT("the created StateTree is in memory"), Tree);
		return Tree;
	}

	static void ReleaseStateTree(UStateTree* Tree)
	{
		if (Tree)
		{
			FAssetRegistryModule::AssetDeleted(Tree);
			Tree->GetOutermost()->SetDirtyFlag(false);
			Tree->ClearFlags(RF_Standalone | RF_Public);
		}
	}

	static UStateTreeEditorData* GetTreeEditorData(UStateTree* Tree)
	{
		return Tree ? Cast<UStateTreeEditorData>(Tree->EditorData) : nullptr;
	}

	static UStateTreeState* FindSubTree(UStateTree* Tree, const FName Name)
	{
		if (UStateTreeEditorData* EditorData = GetTreeEditorData(Tree))
		{
			for (UStateTreeState* SubTree : EditorData->SubTrees)
			{
				if (SubTree && SubTree->Name == Name)
				{
					return SubTree;
				}
			}
		}
		return nullptr;
	}

	/**
	 * Lister checks shared by both tests. Returns false when the lister still exposes the skeleton
	 * class (the unfixed behaviour), so the caller stops before any add_* call.
	 */
	static bool CheckListerHidesSkeleton(FAutomationTestBase& Test, const TArray<FString>& Types,
		const FString& SkeletonName, const FString& GeneratedName)
	{
		const bool bListsSkeleton = Types.Contains(SkeletonName);
		Test.TestFalse(FString::Printf(TEXT("lister does not list the skeleton class '%s'"), *SkeletonName), bListsSkeleton);

		int32 GeneratedCount = 0;
		for (const FString& Type : Types)
		{
			GeneratedCount += Type == GeneratedName ? 1 : 0;
			Test.TestFalse(FString::Printf(TEXT("lister entry '%s' is not an internal Blueprint class"), *Type),
				Type.StartsWith(TEXT("SKEL_")) || Type.StartsWith(TEXT("REINST_")) ||
				Type.StartsWith(TEXT("TRASHCLASS_")) || Type.StartsWith(TEXT("HOTRELOADED_")));
		}
		Test.TestEqual(FString::Printf(TEXT("lister lists the generated class '%s' exactly once"), *GeneratedName), GeneratedCount, 1);

		return !bListsSkeleton && GeneratedCount == 1;
	}

	static bool CheckTaskNode(FAutomationTestBase& Test, const TCHAR* What, const FStateTreeEditorNode& Node, UClass* ExpectedClass)
	{
		const FStateTreeBlueprintTaskWrapper* Wrapper = Node.Node.GetPtr<FStateTreeBlueprintTaskWrapper>();
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s: node is a Blueprint task wrapper"), What), Wrapper))
		{
			return false;
		}
		Test.TestTrue(*FString::Printf(TEXT("%s: wrapper TaskClass is the generated class"), What), Wrapper->TaskClass.Get() == ExpectedClass);
		Test.TestTrue(*FString::Printf(TEXT("%s: instance object is of the generated class"), What),
			Node.InstanceObject && Node.InstanceObject->GetClass() == ExpectedClass);
		return true;
	}

	static bool CheckEvaluatorNode(FAutomationTestBase& Test, const TCHAR* What, const FStateTreeEditorNode& Node, UClass* ExpectedClass)
	{
		const FStateTreeBlueprintEvaluatorWrapper* Wrapper = Node.Node.GetPtr<FStateTreeBlueprintEvaluatorWrapper>();
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s: node is a Blueprint evaluator wrapper"), What), Wrapper))
		{
			return false;
		}
		Test.TestTrue(*FString::Printf(TEXT("%s: wrapper EvaluatorClass is the generated class"), What), Wrapper->EvaluatorClass.Get() == ExpectedClass);
		Test.TestTrue(*FString::Printf(TEXT("%s: instance object is of the generated class"), What),
			Node.InstanceObject && Node.InstanceObject->GetClass() == ExpectedClass);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeStateTreeBlueprintTaskSkeletonClassTest,
	"VibeUE.StateTreeService.BlueprintTask.SkeletonClassNeverInstantiated",
	VibeStateTreeServiceTests::kTestFlags)
bool FVibeStateTreeBlueprintTaskSkeletonClassTest::RunTest(const FString&)
{
	using namespace VibeStateTreeServiceTests;

	UBlueprint* Blueprint = CreateCompiledBlueprint(UStateTreeTaskBlueprintBase::StaticClass(), TEXT("STT_SkelGuard"));
	ON_SCOPE_EXIT { ReleaseBlueprint(Blueprint); };
	if (!TestNotNull(TEXT("created a Blueprint task"), Blueprint))
	{
		return false;
	}

	UClass* SkeletonClass = Blueprint->SkeletonGeneratedClass.Get();
	UClass* GeneratedClass = Blueprint->GeneratedClass.Get();
	if (!TestNotNull(TEXT("the Blueprint has a skeleton class"), SkeletonClass) ||
		!TestNotNull(TEXT("the Blueprint has a generated class"), GeneratedClass) ||
		!TestTrue(TEXT("skeleton and generated classes differ"), SkeletonClass != GeneratedClass) ||
		!TestTrue(TEXT("the Blueprint compiled up to date"), Blueprint->IsUpToDate()))
	{
		return false;
	}
	const FString SkeletonName = SkeletonClass->GetName();
	const FString GeneratedName = GeneratedClass->GetName();
	TestTrue(TEXT("skeleton class name has the SKEL_ prefix"), SkeletonName.StartsWith(TEXT("SKEL_")));

	// (1) The lister. On an unfixed build this is the first failure, and we stop here.
	if (!CheckListerHidesSkeleton(*this, UStateTreeService::GetAvailableTaskTypes(), SkeletonName, GeneratedName))
	{
		return false;
	}

	FString TreePath;
	UStateTree* Tree = CreateServiceStateTree(*this, TreePath);
	ON_SCOPE_EXIT { ReleaseStateTree(Tree); };
	if (!Tree)
	{
		return false;
	}
	const FName StateName(TEXT("GuardRoot"));
	TestTrue(TEXT("AddState created the subtree"), UStateTreeService::AddState(TreePath, TEXT(""), StateName.ToString()));
	UStateTreeState* State = FindSubTree(Tree, StateName);
	UStateTreeEditorData* EditorData = GetTreeEditorData(Tree);
	if (!TestNotNull(TEXT("the subtree exists"), State) || !TestNotNull(TEXT("the tree has editor data"), EditorData))
	{
		return false;
	}

	// (2) A copied SKEL_ name resolves to the compiled generated class; the skeleton is never instantiated.
	const int32 TasksBefore = State->Tasks.Num();
	TestTrue(TEXT("AddTask with the SKEL_ name resolves to the generated class"),
		UStateTreeService::AddTask(TreePath, StateName.ToString(), SkeletonName));
	if (TestEqual(TEXT("AddTask with the SKEL_ name added exactly one task"), State->Tasks.Num(), TasksBefore + 1))
	{
		CheckTaskNode(*this, TEXT("SKEL_ name"), State->Tasks.Last(), GeneratedClass);
	}

	// (3) The real _C name works as before.
	TestTrue(TEXT("AddTask with the generated _C name succeeds"),
		UStateTreeService::AddTask(TreePath, StateName.ToString(), GeneratedName));
	if (TestEqual(TEXT("AddTask with the _C name added exactly one task"), State->Tasks.Num(), TasksBefore + 2))
	{
		CheckTaskNode(*this, TEXT("_C name"), State->Tasks.Last(), GeneratedClass);
	}

	// The global-task path shares the resolver.
	const int32 GlobalBefore = EditorData->GlobalTasks.Num();
	TestTrue(TEXT("AddGlobalTask with the SKEL_ name resolves to the generated class"),
		UStateTreeService::AddGlobalTask(TreePath, SkeletonName));
	if (TestEqual(TEXT("AddGlobalTask added exactly one task"), EditorData->GlobalTasks.Num(), GlobalBefore + 1))
	{
		CheckTaskNode(*this, TEXT("global SKEL_ name"), EditorData->GlobalTasks.Last(), GeneratedClass);
	}

	// (4) An edited-but-uncompiled Blueprint is refused with the compile-first error, by either name,
	// and drops out of the lister. Status is set directly: it is exactly what the editor sets on edit
	// (FBlueprintEditorUtils::MarkBlueprintAsModified), without triggering a skeleton regeneration.
	Blueprint->Status = BS_Dirty;
	AddExpectedMessagePlain(kStaleBlueprintMessage, ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 3);
	const int32 TasksBeforeStale = State->Tasks.Num();
	const int32 GlobalBeforeStale = EditorData->GlobalTasks.Num();
	TestFalse(TEXT("AddTask refuses an uncompiled Blueprint by its _C name"),
		UStateTreeService::AddTask(TreePath, StateName.ToString(), GeneratedName));
	TestFalse(TEXT("AddTask refuses an uncompiled Blueprint by its SKEL_ name"),
		UStateTreeService::AddTask(TreePath, StateName.ToString(), SkeletonName));
	TestFalse(TEXT("AddGlobalTask refuses an uncompiled Blueprint"),
		UStateTreeService::AddGlobalTask(TreePath, GeneratedName));
	TestEqual(TEXT("refused adds left the state's tasks unchanged"), State->Tasks.Num(), TasksBeforeStale);
	TestEqual(TEXT("refused adds left the global tasks unchanged"), EditorData->GlobalTasks.Num(), GlobalBeforeStale);
	TestFalse(TEXT("lister drops an uncompiled Blueprint task"), UStateTreeService::GetAvailableTaskTypes().Contains(GeneratedName));

	// (5) A Blueprint with compile errors is refused with its own message, not the compile-first advice.
	Blueprint->Status = BS_Error;
	AddExpectedMessagePlain(TEXT("has compile errors; fix them and recompile"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1);
	TestFalse(TEXT("AddTask refuses a Blueprint with compile errors"),
		UStateTreeService::AddTask(TreePath, StateName.ToString(), GeneratedName));
	TestEqual(TEXT("the refused add left the state's tasks unchanged"), State->Tasks.Num(), TasksBeforeStale);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeStateTreeBlueprintEvaluatorSkeletonClassTest,
	"VibeUE.StateTreeService.BlueprintEvaluator.SkeletonClassNeverInstantiated",
	VibeStateTreeServiceTests::kTestFlags)
bool FVibeStateTreeBlueprintEvaluatorSkeletonClassTest::RunTest(const FString&)
{
	using namespace VibeStateTreeServiceTests;

	UBlueprint* Blueprint = CreateCompiledBlueprint(UStateTreeEvaluatorBlueprintBase::StaticClass(), TEXT("STE_SkelGuard"));
	ON_SCOPE_EXIT { ReleaseBlueprint(Blueprint); };
	if (!TestNotNull(TEXT("created a Blueprint evaluator"), Blueprint))
	{
		return false;
	}

	UClass* SkeletonClass = Blueprint->SkeletonGeneratedClass.Get();
	UClass* GeneratedClass = Blueprint->GeneratedClass.Get();
	if (!TestNotNull(TEXT("the Blueprint has a skeleton class"), SkeletonClass) ||
		!TestNotNull(TEXT("the Blueprint has a generated class"), GeneratedClass) ||
		!TestTrue(TEXT("skeleton and generated classes differ"), SkeletonClass != GeneratedClass) ||
		!TestTrue(TEXT("the Blueprint compiled up to date"), Blueprint->IsUpToDate()))
	{
		return false;
	}
	const FString SkeletonName = SkeletonClass->GetName();
	const FString GeneratedName = GeneratedClass->GetName();

	// (1) The lister. On an unfixed build this is the first failure, and we stop here.
	if (!CheckListerHidesSkeleton(*this, UStateTreeService::GetAvailableEvaluatorTypes(), SkeletonName, GeneratedName))
	{
		return false;
	}

	FString TreePath;
	UStateTree* Tree = CreateServiceStateTree(*this, TreePath);
	ON_SCOPE_EXIT { ReleaseStateTree(Tree); };
	UStateTreeEditorData* EditorData = GetTreeEditorData(Tree);
	if (!TestNotNull(TEXT("the tree has editor data"), EditorData))
	{
		return false;
	}

	// (2) A copied SKEL_ name resolves to the compiled generated class.
	const int32 EvaluatorsBefore = EditorData->Evaluators.Num();
	TestTrue(TEXT("AddEvaluator with the SKEL_ name resolves to the generated class"),
		UStateTreeService::AddEvaluator(TreePath, SkeletonName));
	if (TestEqual(TEXT("AddEvaluator with the SKEL_ name added exactly one evaluator"), EditorData->Evaluators.Num(), EvaluatorsBefore + 1))
	{
		CheckEvaluatorNode(*this, TEXT("SKEL_ name"), EditorData->Evaluators.Last(), GeneratedClass);
	}

	// (3) The real _C name works as before.
	TestTrue(TEXT("AddEvaluator with the generated _C name succeeds"),
		UStateTreeService::AddEvaluator(TreePath, GeneratedName));
	if (TestEqual(TEXT("AddEvaluator with the _C name added exactly one evaluator"), EditorData->Evaluators.Num(), EvaluatorsBefore + 2))
	{
		CheckEvaluatorNode(*this, TEXT("_C name"), EditorData->Evaluators.Last(), GeneratedClass);
	}

	// (4) An edited-but-uncompiled Blueprint is refused and drops out of the lister.
	Blueprint->Status = BS_Dirty;
	AddExpectedMessagePlain(kStaleBlueprintMessage, ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 2);
	const int32 EvaluatorsBeforeStale = EditorData->Evaluators.Num();
	TestFalse(TEXT("AddEvaluator refuses an uncompiled Blueprint by its _C name"),
		UStateTreeService::AddEvaluator(TreePath, GeneratedName));
	TestFalse(TEXT("AddEvaluator refuses an uncompiled Blueprint by its SKEL_ name"),
		UStateTreeService::AddEvaluator(TreePath, SkeletonName));
	TestEqual(TEXT("refused adds left the evaluators unchanged"), EditorData->Evaluators.Num(), EvaluatorsBeforeStale);
	TestFalse(TEXT("lister drops an uncompiled Blueprint evaluator"), UStateTreeService::GetAvailableEvaluatorTypes().Contains(GeneratedName));

	return true;
}

#endif // WITH_AUTOMATION_TESTS
