// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UBlackboardService.h"
#include "AIServiceTestFixture.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Decorators/BTDecorator_BlackboardBase.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectHash.h"

static const EAutomationTestFlags kBBTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

static const TCHAR* kBBTestDir = TEXT("/Game/Developers/VibeUEBTTests");

// ResetFixtureAsset / FScopedFixtureReset live in AIServiceTestFixture.h — the Behavior Tree
// suite writes into the same fixture directory (NOT reliably gitignored; see the note there)
// and needs identical semantics.
using VibeAITest::FScopedFixtureReset;

// Regression test for two Critical review findings on RenameBlackboardKey:
//  - the BT sweep must inspect decorators (FBTCompositeChild::Decorators), not just tasks and
//    services — UBTDecorator_BlackboardBase::BlackboardKey is the base for every Blackboard-
//    driven condition, and is exactly where a real project's key selectors live.
//  - a rejected rename must be distinguishable from a successful rename nothing referenced.
// Builds a real runtime BT node tree (UBTCompositeNode / FBTCompositeChild), not an editor
// EdGraph, because that is what RenameBlackboardKey's sweep actually walks.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBBKeyRenameTest,
	"VibeUE.Blackboard.Keys.Rename", kBBTestFlags)
bool FVibeBBKeyRenameTest::RunTest(const FString&)
{
	const FString BBPath = FString(kBBTestDir) / TEXT("BB_VibeRenameTest");
	const FString BTPath = FString(kBBTestDir) / TEXT("BT_VibeRenameTest");
	// BT_VibeRenameTest is never saved to disk (see the comment below), but it IS registered
	// with the asset registry and left resident in memory — a same-process rerun would collide
	// with it just as surely as BBPath collides with a previous process's saved .uasset.
	FScopedFixtureReset ResetBB(BBPath);
	FScopedFixtureReset ResetBT(BTPath);

	TestTrue(TEXT("blackboard created"), UBlackboardService::CreateBlackboard(BBPath, FString()));
	TestEqual(TEXT("add Target key"),
		UBlackboardService::AddBlackboardKey(BBPath, TEXT("Target"), TEXT("Object"), false), FString());

	UBlackboardData* Board = LoadObject<UBlackboardData>(nullptr, *BBPath);
	if (!TestNotNull(TEXT("board loaded"), Board))
	{
		return false;
	}

	// Sequence root -> one task, gated by a Blackboard decorator bound to "Target".
	UPackage* BTPackage = CreatePackage(*BTPath);
	if (!TestNotNull(TEXT("BT package created"), BTPackage))
	{
		return false;
	}
	UBehaviorTree* Tree = NewObject<UBehaviorTree>(
		BTPackage, TEXT("BT_VibeRenameTest"), RF_Public | RF_Standalone | RF_Transactional);
	Tree->BlackboardAsset = Board;

	UBTComposite_Sequence* Root = NewObject<UBTComposite_Sequence>(Tree, NAME_None, RF_Transactional);
	UBTTask_Wait* Task = NewObject<UBTTask_Wait>(Tree, NAME_None, RF_Transactional);
	UBTDecorator_Blackboard* Decorator = NewObject<UBTDecorator_Blackboard>(Tree, NAME_None, RF_Transactional);

	// BlackboardKey is `protected` on UBTDecorator_BlackboardBase — set it through the same
	// reflection path the service's own sweep reads it through, rather than a friend/accessor,
	// so this exercises the real access pattern against a real engine decorator class.
	FStructProperty* KeyProp = CastField<FStructProperty>(
		UBTDecorator_BlackboardBase::StaticClass()->FindPropertyByName(TEXT("BlackboardKey")));
	if (!TestNotNull(TEXT("BlackboardKey property found via reflection"), KeyProp))
	{
		return false;
	}
	FBlackboardKeySelector* Selector = KeyProp->ContainerPtrToValuePtr<FBlackboardKeySelector>(Decorator);
	Selector->SelectedKeyName = TEXT("Target");

	FBTCompositeChild Child;
	Child.ChildTask = Task;
	Child.Decorators.Add(Decorator);
	Root->Children.Add(Child);
	Tree->RootNode = Root;

	FAssetRegistryModule::AssetCreated(Tree);
	Tree->MarkPackageDirty();

	// Rename: the decorator's selector must show up in the affected-reference list. If the
	// sweep only checked tasks/services (the pre-fix bug), this list comes back empty.
	TArray<FString> Affected =
		UBlackboardService::RenameBlackboardKey(BBPath, TEXT("Target"), TEXT("TargetActor"));
	TestTrue(TEXT("rename reports the decorator's reference"),
		Affected.ContainsByPredicate([&BTPath](const FString& Ref)
		{
			return Ref.StartsWith(BTPath) && Ref.Contains(TEXT("BlackboardKey"));
		}));
	TestFalse(TEXT("a successful rename never returns the ERROR sentinel"),
		Affected.ContainsByPredicate([](const FString& Ref){ return Ref.StartsWith(TEXT("ERROR:")); }));

	// The rename actually landed on the Blackboard asset.
	TArray<FBBKeyInfo> KeysAfterRename = UBlackboardService::GetBlackboardKeys(BBPath);
	TestTrue(TEXT("TargetActor present after rename"),
		KeysAfterRename.ContainsByPredicate([](const FBBKeyInfo& K){ return K.Name == TEXT("TargetActor"); }));
	TestFalse(TEXT("Target gone after rename"),
		KeysAfterRename.ContainsByPredicate([](const FBBKeyInfo& K){ return K.Name == TEXT("Target"); }));

	// A rejected rename (unknown key) is distinguishable from a successful-but-unreferenced
	// one: it returns the single-entry ERROR sentinel, not a plain empty array.
	TArray<FString> MissingKeyResult =
		UBlackboardService::RenameBlackboardKey(BBPath, TEXT("NoSuchKey"), TEXT("Whatever"));
	TestEqual(TEXT("rejected rename (missing key) returns exactly one entry"), MissingKeyResult.Num(), 1);
	if (MissingKeyResult.Num() == 1)
	{
		TestTrue(TEXT("missing-key rejection is the ERROR sentinel"),
			MissingKeyResult[0].StartsWith(TEXT("ERROR:")));
	}

	// A rejected rename via name collision is likewise distinguishable.
	TestEqual(TEXT("add second key"),
		UBlackboardService::AddBlackboardKey(BBPath, TEXT("Other"), TEXT("Bool"), false), FString());
	TArray<FString> CollisionResult =
		UBlackboardService::RenameBlackboardKey(BBPath, TEXT("Other"), TEXT("TargetActor"));
	TestEqual(TEXT("rejected rename (collision) returns exactly one entry"), CollisionResult.Num(), 1);
	if (CollisionResult.Num() == 1)
	{
		TestTrue(TEXT("collision rejection is the ERROR sentinel"),
			CollisionResult[0].StartsWith(TEXT("ERROR:")));
	}

	// A successful rename that nothing references returns a genuine empty array — not the
	// ERROR sentinel — proving the two failure/success-with-nothing-to-report cases don't alias.
	TArray<FString> UnreferencedResult =
		UBlackboardService::RenameBlackboardKey(BBPath, TEXT("Other"), TEXT("OtherRenamed"));
	TestEqual(TEXT("unreferenced-but-successful rename returns nothing to report"),
		UnreferencedResult.Num(), 0);

	return true;
}
#endif // WITH_AUTOMATION_TESTS
