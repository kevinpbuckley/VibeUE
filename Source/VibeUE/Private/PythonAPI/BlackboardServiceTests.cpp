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

// =================================================================================================
//  Coverage added by the post-review fixes (fix/bt-service-review).
// =================================================================================================

// The three key types the original suite never touched (Class, Enum, NativeEnum), the enum arm
// of SetBlackboardKeyObjectClass, its error paths, and the post-creation editing the Blackboard
// editor has always allowed and the service previously did not: bInstanceSynced, category,
// description.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBBKeyTypesAndMetadataTest,
	"VibeUE.Blackboard.Keys.TypesAndMetadata", kBBTestFlags)
bool FVibeBBKeyTypesAndMetadataTest::RunTest(const FString&)
{
	const FString Path = FString(kBBTestDir) / TEXT("BB_TypesAndMetadata");
	FScopedFixtureReset ResetFixture(Path);

	TestTrue(TEXT("created"), UBlackboardService::CreateBlackboard(Path, FString()));

	for (const TCHAR* Type : { TEXT("Class"), TEXT("Enum"), TEXT("NativeEnum") })
	{
		TestEqual(FString::Printf(TEXT("add %s key"), Type),
			UBlackboardService::AddBlackboardKey(Path, FString::Printf(TEXT("Key%s"), Type), Type, false),
			FString());
	}

	// Class key takes a base class; Enum key takes a UEnum by path.
	TestEqual(TEXT("class key base class"),
		UBlackboardService::SetBlackboardKeyObjectClass(Path, TEXT("KeyClass"), TEXT("/Script/Engine.Pawn")),
		FString());
	TestEqual(TEXT("enum key enum type"),
		UBlackboardService::SetBlackboardKeyObjectClass(Path, TEXT("KeyEnum"),
			TEXT("/Script/Engine.ECollisionChannel")),
		FString());

	TArray<FBBKeyInfo> Keys = UBlackboardService::GetBlackboardKeys(Path);
	const FBBKeyInfo* ClassKey = Keys.FindByPredicate(
		[](const FBBKeyInfo& K){ return K.Name == TEXT("KeyClass"); });
	if (TestNotNull(TEXT("class key present"), ClassKey))
	{
		TestEqual(TEXT("class key type"), ClassKey->Type, FString(TEXT("Class")));
		TestTrue(TEXT("class key base reported"), ClassKey->ObjectClassPath.Contains(TEXT("Pawn")));
	}
	const FBBKeyInfo* EnumKey = Keys.FindByPredicate(
		[](const FBBKeyInfo& K){ return K.Name == TEXT("KeyEnum"); });
	if (TestNotNull(TEXT("enum key present"), EnumKey))
	{
		TestTrue(TEXT("enum key enum reported"),
			EnumKey->ObjectClassPath.Contains(TEXT("ECollisionChannel")));
	}

	// Error paths: missing key, a key kind that takes neither, a bogus path.
	TestTrue(TEXT("missing key errors"),
		!UBlackboardService::SetBlackboardKeyObjectClass(Path, TEXT("NoSuchKey"), TEXT("/Script/Engine.Pawn")).IsEmpty());
	TestEqual(TEXT("add bool key"),
		UBlackboardService::AddBlackboardKey(Path, TEXT("KeyBool"), TEXT("Bool"), false), FString());
	TestTrue(TEXT("a Bool key takes no class"),
		!UBlackboardService::SetBlackboardKeyObjectClass(Path, TEXT("KeyBool"), TEXT("/Script/Engine.Pawn")).IsEmpty());
	TestTrue(TEXT("a bogus class path errors"),
		!UBlackboardService::SetBlackboardKeyObjectClass(Path, TEXT("KeyClass"), TEXT("/Script/Engine.NoSuchClass_X")).IsEmpty());

	// bInstanceSynced is editable AFTER creation now, both directions.
	TestEqual(TEXT("sync on"),
		UBlackboardService::SetBlackboardKeyInstanceSynced(Path, TEXT("KeyBool"), true), FString());
	Keys = UBlackboardService::GetBlackboardKeys(Path);
	const FBBKeyInfo* Synced = Keys.FindByPredicate(
		[](const FBBKeyInfo& K){ return K.Name == TEXT("KeyBool"); });
	if (TestNotNull(TEXT("bool key present"), Synced))
	{
		TestTrue(TEXT("synced after set"), Synced->bInstanceSynced);
	}
	TestEqual(TEXT("sync off"),
		UBlackboardService::SetBlackboardKeyInstanceSynced(Path, TEXT("KeyBool"), false), FString());

	// Category and description: set, read back, clear.
	TestEqual(TEXT("metadata set"),
		UBlackboardService::SetBlackboardKeyMetadata(Path, TEXT("KeyBool"),
			TEXT("Combat"), TEXT("Whether the pawn is alerted.")), FString());
	Keys = UBlackboardService::GetBlackboardKeys(Path);
	const FBBKeyInfo* Documented = Keys.FindByPredicate(
		[](const FBBKeyInfo& K){ return K.Name == TEXT("KeyBool"); });
	if (TestNotNull(TEXT("documented key present"), Documented))
	{
		TestEqual(TEXT("category read back"), Documented->Category, FString(TEXT("Combat")));
		TestEqual(TEXT("description read back"), Documented->Description,
			FString(TEXT("Whether the pawn is alerted.")));
	}
	TestEqual(TEXT("metadata cleared"),
		UBlackboardService::SetBlackboardKeyMetadata(Path, TEXT("KeyBool"), FString(), FString()),
		FString());

	return true;
}

// CreateBlackboard must never overwrite: the pre-fix version happily "re-created" over an
// existing asset, wiping its keys on disk (empirically confirmed during review), and would
// fatally collide with a loaded object of another class.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBBCreateGuardsTest,
	"VibeUE.Blackboard.Asset.CreateGuards", kBBTestFlags)
bool FVibeBBCreateGuardsTest::RunTest(const FString&)
{
	const FString Path = FString(kBBTestDir) / TEXT("BB_CreateGuards");
	FScopedFixtureReset ResetFixture(Path);

	TestTrue(TEXT("created"), UBlackboardService::CreateBlackboard(Path, FString()));
	TestEqual(TEXT("key added"),
		UBlackboardService::AddBlackboardKey(Path, TEXT("Precious"), TEXT("Float"), false), FString());

	// The second create on the same path must be refused — and the key must survive.
	TestFalse(TEXT("a second create on the same path is refused"),
		UBlackboardService::CreateBlackboard(Path, FString()));
	TestTrue(TEXT("the existing key survived the refused create"),
		UBlackboardService::GetBlackboardKeys(Path).ContainsByPredicate(
			[](const FBBKeyInfo& K){ return K.Name == TEXT("Precious"); }));

	// Path validation: engine/script roots and over-long names are refused, not attempted.
	TestFalse(TEXT("creating under /Engine is refused"),
		UBlackboardService::CreateBlackboard(TEXT("/Engine/VibeUEShouldNeverExist"), FString()));
	TestFalse(TEXT("creating under /Script is refused"),
		UBlackboardService::CreateBlackboard(TEXT("/Script/VibeUEShouldNeverExist"), FString()));
	TestFalse(TEXT("an over-long path is refused, not a fatal FName"),
		UBlackboardService::CreateBlackboard(TEXT("/Game/") + FString::ChrN(2000, TEXT('x')), FString()));

	// Over-long key names are errors on every entry point that takes one.
	const FString LongKey = FString::ChrN(2000, TEXT('k'));
	TestTrue(TEXT("over-long key name refused on add"),
		!UBlackboardService::AddBlackboardKey(Path, LongKey, TEXT("Bool"), false).IsEmpty());
	TestTrue(TEXT("over-long key name refused on remove"),
		!UBlackboardService::RemoveBlackboardKey(Path, LongKey).IsEmpty());

	return true;
}

// The blackboard mutators must hold the same write guards the BT service holds. The pre-fix
// service had none: a key mutation during PIE went straight through (empirically confirmed),
// re-indexing Keys under live UBlackboardComponents. Same FScopedPlayWorld pattern as
// Asset.PlaySessionGuard: PlayWorld is pointed at the editor world for exactly one call.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBBWriteGuardsTest,
	"VibeUE.Blackboard.Asset.WriteGuards", kBBTestFlags)
bool FVibeBBWriteGuardsTest::RunTest(const FString&)
{
	const FString Path = FString(kBBTestDir) / TEXT("BB_WriteGuards");
	FScopedFixtureReset ResetFixture(Path);

	TestTrue(TEXT("created"), UBlackboardService::CreateBlackboard(Path, FString()));

	UWorld* const StandInWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("an editor world is available to stand in for a play world"), StandInWorld))
	{
		return false;
	}

	UWorld* const PlayWorldBefore = ToRawPtr(GEditor->PlayWorld);
	FString DuringPIE;
	{
		struct FScopedPlayWorld
		{
			UWorld* Previous;
			explicit FScopedPlayWorld(UWorld* World) : Previous(ToRawPtr(GEditor->PlayWorld))
			{
				GEditor->PlayWorld = World;
			}
			~FScopedPlayWorld() { GEditor->PlayWorld = Previous; }
		} PretendPIE(StandInWorld);

		DuringPIE = UBlackboardService::AddBlackboardKey(Path, TEXT("DuringPIE"), TEXT("Bool"), false);
	}
	TestEqual(TEXT("PlayWorld restored"), ToRawPtr(GEditor->PlayWorld), PlayWorldBefore);
	TestTrue(TEXT("a key mutation during a play session is refused"), !DuringPIE.IsEmpty());
	TestTrue(TEXT("...naming the session as the reason"), DuringPIE.Contains(TEXT("Play In Editor")));
	TestFalse(TEXT("the refused key was not added"),
		UBlackboardService::GetBlackboardKeys(Path).ContainsByPredicate(
			[](const FBBKeyInfo& K){ return K.Name == TEXT("DuringPIE"); }));

	// The refusal is a condition, not a latch.
	TestEqual(TEXT("the same add goes through once the play world is gone"),
		UBlackboardService::AddBlackboardKey(Path, TEXT("DuringPIE"), TEXT("Bool"), false), FString());

	// SelfActor is engine-persistent on a parent-less board: removal would read back undone after
	// the next reload, so it is refused up front.
	const FString SelfError = UBlackboardService::RemoveBlackboardKey(Path, TEXT("SelfActor"));
	TestTrue(TEXT("removing SelfActor is refused"), !SelfError.IsEmpty());
	TestTrue(TEXT("...as engine-persistent"), SelfError.Contains(TEXT("persistent")));

	return true;
}

// SetBlackboardParent: attach, inherited-key reporting, detach, cycle refusal, shadowing
// refusal — the re-parenting the Blackboard editor has always allowed and the service lacked.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBBReparentTest,
	"VibeUE.Blackboard.Keys.Reparent", kBBTestFlags)
bool FVibeBBReparentTest::RunTest(const FString&)
{
	const FString ParentPath = FString(kBBTestDir) / TEXT("BB_ReparentParent");
	const FString ChildPath = FString(kBBTestDir) / TEXT("BB_ReparentChild");
	FScopedFixtureReset ResetParent(ParentPath);
	FScopedFixtureReset ResetChild(ChildPath);

	TestTrue(TEXT("parent created"), UBlackboardService::CreateBlackboard(ParentPath, FString()));
	TestEqual(TEXT("parent key added"),
		UBlackboardService::AddBlackboardKey(ParentPath, TEXT("SharedTarget"), TEXT("Object"), false), FString());
	TestTrue(TEXT("child created (parentless)"), UBlackboardService::CreateBlackboard(ChildPath, FString()));
	TestEqual(TEXT("child key added"),
		UBlackboardService::AddBlackboardKey(ChildPath, TEXT("OwnKey"), TEXT("Int"), false), FString());

	// Attach: the parent's key appears on the child as inherited.
	TestEqual(TEXT("reparent onto the parent"),
		UBlackboardService::SetBlackboardParent(ChildPath, ParentPath), FString());
	TArray<FBBKeyInfo> Keys = UBlackboardService::GetBlackboardKeys(ChildPath);
	const FBBKeyInfo* Inherited = Keys.FindByPredicate(
		[](const FBBKeyInfo& K){ return K.Name == TEXT("SharedTarget"); });
	if (TestNotNull(TEXT("inherited key visible on the child"), Inherited))
	{
		TestTrue(TEXT("...flagged as inherited"), Inherited->bInherited);
	}
	const FBBKeyInfo* Own = Keys.FindByPredicate(
		[](const FBBKeyInfo& K){ return K.Name == TEXT("OwnKey"); });
	if (TestNotNull(TEXT("own key still visible"), Own))
	{
		TestFalse(TEXT("...not flagged as inherited"), Own->bInherited);
	}

	// An inherited key is refused by the per-key mutators, naming the right fix.
	TestTrue(TEXT("editing an inherited key is refused"),
		UBlackboardService::SetBlackboardKeyInstanceSynced(ChildPath, TEXT("SharedTarget"), true)
			.Contains(TEXT("inherited")));

	// Cycle refusal: the parent cannot adopt its own child.
	const FString CycleError = UBlackboardService::SetBlackboardParent(ParentPath, ChildPath);
	TestTrue(TEXT("a parent cycle is refused"), !CycleError.IsEmpty());
	TestTrue(TEXT("...named as a cycle"), CycleError.Contains(TEXT("cycle")));

	// Shadowing refusal: a child owning a same-named key cannot adopt the parent that defines it.
	TestEqual(TEXT("detach"),
		UBlackboardService::SetBlackboardParent(ChildPath, FString()), FString());
	TestEqual(TEXT("add colliding key"),
		UBlackboardService::AddBlackboardKey(ChildPath, TEXT("SharedTarget"), TEXT("Bool"), false), FString());
	const FString ShadowError = UBlackboardService::SetBlackboardParent(ChildPath, ParentPath);
	TestTrue(TEXT("a shadowing reparent is refused"), !ShadowError.IsEmpty());
	TestTrue(TEXT("...naming the colliding key"), ShadowError.Contains(TEXT("SharedTarget")));

	// Detach worked: the inherited key is gone from the listing.
	TestFalse(TEXT("after detach the parent's key is no longer listed"),
		UBlackboardService::GetBlackboardKeys(ChildPath).ContainsByPredicate(
			[](const FBBKeyInfo& K){ return K.Name == TEXT("SharedTarget") && K.bInherited; }));

	return true;
}

#endif // WITH_AUTOMATION_TESTS
