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
// suite writes into the same gitignored fixture directory and needs identical semantics.
using VibeAITest::FScopedFixtureReset;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBBKeyCrudTest,
	"VibeUE.Blackboard.Keys.Crud", kBBTestFlags)
bool FVibeBBKeyCrudTest::RunTest(const FString&)
{
	const FString Path = FString(kBBTestDir) / TEXT("BB_VibeTest");
	FScopedFixtureReset ResetFixture(Path);

	TestTrue(TEXT("created"), UBlackboardService::CreateBlackboard(Path, FString()));

	// A fresh blackboard carries exactly the engine's injected self key and nothing else — a
	// service-created board must match a hand-authored one (both BB_Enemy and BB_Villager in
	// this project carry "SelfActor"), or a BT node bound to "SelfActor" silently fails to
	// resolve against it.
	TArray<FBBKeyInfo> Fresh = UBlackboardService::GetBlackboardKeys(Path);
	TestEqual(TEXT("only the engine self key on create"), Fresh.Num(), 1);
	if (Fresh.Num() == 1)
	{
		TestEqual(TEXT("self key name"), Fresh[0].Name, FString(TEXT("SelfActor")));
		TestEqual(TEXT("self key type"), Fresh[0].Type, FString(TEXT("Object")));
	}

	// One key of each scalar type.
	for (const TCHAR* Type : { TEXT("Bool"), TEXT("Int"), TEXT("Float"), TEXT("Vector"),
	                           TEXT("Name"), TEXT("String"), TEXT("Rotator") })
	{
		const FString Err = UBlackboardService::AddBlackboardKey(
			Path, FString::Printf(TEXT("Key%s"), Type), Type, false);
		TestEqual(FString::Printf(TEXT("add %s"), Type), Err, FString());
	}

	// Seven added keys plus the pre-existing self key.
	TArray<FBBKeyInfo> Keys = UBlackboardService::GetBlackboardKeys(Path);
	TestEqual(TEXT("eight keys (seven added plus the self key)"), Keys.Num(), 8);

	const FBBKeyInfo* Bool = Keys.FindByPredicate(
		[](const FBBKeyInfo& K){ return K.Name == TEXT("KeyBool"); });
	TestNotNull(TEXT("KeyBool present"), Bool);
	if (Bool)
	{
		TestEqual(TEXT("KeyBool type"), Bool->Type, FString(TEXT("Bool")));
		TestFalse(TEXT("KeyBool not synced"), Bool->bInstanceSynced);
	}

	// An unknown type is a reported error, not a silently broken key.
	const FString BadErr = UBlackboardService::AddBlackboardKey(
		Path, TEXT("KeyBogus"), TEXT("NotAType"), false);
	TestTrue(TEXT("unknown type rejected"), !BadErr.IsEmpty());
	TestEqual(TEXT("no key added (still eight)"), UBlackboardService::GetBlackboardKeys(Path).Num(), 8);

	// A duplicate name is rejected.
	TestTrue(TEXT("duplicate rejected"),
		!UBlackboardService::AddBlackboardKey(Path, TEXT("KeyBool"), TEXT("Bool"), false).IsEmpty());

	// Object key carries a base class.
	TestEqual(TEXT("add object key"),
		UBlackboardService::AddBlackboardKey(Path, TEXT("KeyActor"), TEXT("Object"), true), FString());
	TestEqual(TEXT("set base class"),
		UBlackboardService::SetBlackboardKeyObjectClass(Path, TEXT("KeyActor"), TEXT("/Script/Engine.Actor")),
		FString());
	Keys = UBlackboardService::GetBlackboardKeys(Path);
	const FBBKeyInfo* Actor = Keys.FindByPredicate(
		[](const FBBKeyInfo& K){ return K.Name == TEXT("KeyActor"); });
	TestNotNull(TEXT("KeyActor present"), Actor);
	if (Actor)
	{
		TestTrue(TEXT("KeyActor synced"), Actor->bInstanceSynced);
		TestTrue(TEXT("KeyActor base class"), Actor->ObjectClassPath.Contains(TEXT("Actor")));
	}

	// Remove: nine (eight plus KeyActor) minus KeyBool leaves eight.
	TestEqual(TEXT("remove"), UBlackboardService::RemoveBlackboardKey(Path, TEXT("KeyBool")), FString());
	TestEqual(TEXT("eight after remove (nine minus KeyBool)"),
		UBlackboardService::GetBlackboardKeys(Path).Num(), 8);
	TestTrue(TEXT("removing a missing key errors"),
		!UBlackboardService::RemoveBlackboardKey(Path, TEXT("KeyBool")).IsEmpty());

	return true;
}

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

// Regression test for the Critical finding that CreateBlackboard never fixed up FirstKeyID for
// a parented board: without recomputing it against the parent chain, a child's own key IDs
// start at 0 and collide with the parent's, corrupting GetKey/GetKeyID lookups until the asset
// is reloaded from disk (which masked the bug in-editor, since a fresh load always recomputes
// it correctly — the bug only showed on an asset created and used within the same session).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBBParentedKeyIdsTest,
	"VibeUE.Blackboard.Keys.ParentedKeyIds", kBBTestFlags)
bool FVibeBBParentedKeyIdsTest::RunTest(const FString&)
{
	const FString ParentPath = FString(kBBTestDir) / TEXT("BB_VibeParentTest");
	const FString ChildPath = FString(kBBTestDir) / TEXT("BB_VibeChildTest");
	FScopedFixtureReset ResetParent(ParentPath);
	FScopedFixtureReset ResetChild(ChildPath);

	TestTrue(TEXT("parent created"), UBlackboardService::CreateBlackboard(ParentPath, FString()));
	TestEqual(TEXT("add parent key"),
		UBlackboardService::AddBlackboardKey(ParentPath, TEXT("ParentKey"), TEXT("Bool"), false), FString());

	TestTrue(TEXT("child created"), UBlackboardService::CreateBlackboard(ChildPath, ParentPath));
	TestEqual(TEXT("add child key"),
		UBlackboardService::AddBlackboardKey(ChildPath, TEXT("ChildKey"), TEXT("Int"), false), FString());

	UBlackboardData* ParentBoard = LoadObject<UBlackboardData>(nullptr, *ParentPath);
	UBlackboardData* ChildBoard = LoadObject<UBlackboardData>(nullptr, *ChildPath);
	if (!TestNotNull(TEXT("parent loaded"), ParentBoard) || !TestNotNull(TEXT("child loaded"), ChildBoard))
	{
		return false;
	}

	const FBlackboard::FKey ParentKeyId = ParentBoard->GetKeyID(TEXT("ParentKey"));
	const FBlackboard::FKey ChildKeyId = ChildBoard->GetKeyID(TEXT("ChildKey"));
	TestTrue(TEXT("parent key resolves"), ParentKeyId != FBlackboard::InvalidKey);
	TestTrue(TEXT("child key resolves"), ChildKeyId != FBlackboard::InvalidKey);

	// If FirstKeyID were never recomputed (the bug), ChildKeyId would be a small index (0, 1, ...)
	// that lands inside the parent's own key range instead of after it.
	TestTrue(TEXT("child key ID starts after the parent's entire key range"),
		(int32)ChildKeyId >= ParentBoard->GetNumKeys());
	TestTrue(TEXT("child key ID does not collide with the parent's own key ID"),
		ChildKeyId != ParentKeyId);

	return true;
}

#endif // WITH_AUTOMATION_TESTS
