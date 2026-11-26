// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Guid.h"
#include "PythonAPI/UBlueprintService.h"
#include "Tools/PythonTools.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "UObject/Package.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_AUTOMATION_TESTS

// Issue #614: two create_node_by_key fixes were live-verified only, and a mutation run showed the
// suite stayed green with either one reverted:
//   #610 - the qualified key "SPAWN <NodeClass>|<MenuName>|<OwnerClass>". A menu name is a property's
//          DISPLAY spelling, and a bool drops its "b" prefix (AActor::bCanBeDamaged is
//          "Get Can Be Damaged"); a lookup on the raw name alone missed every boolean, and
//          "...|Get Can Be Damaged|Actor" returned "" in a live editor while the suite was green.
//   #609 - a variable added moments earlier has no action-database spawner yet, so the natural
//          "add_member_variable, then create_node_by_key" flow returned "". The fallback binds the
//          Blueprint's own member by its normalised name.
// Each test asserts the node that comes back is bound to the right member, not only that an id came back.

namespace VibeVariableKeyTestUtil
{
	static UBlueprint* MakeVariableKeyFixture(FAutomationTestBase& Test, const TCHAR* Prefix, UClass* ParentClass, FString& OutPath)
	{
		const FString Leaf = FString::Printf(TEXT("%s_%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		OutPath = FString(TEXT("/Game/__VibeUETest/")) + Leaf;
		UPackage* Package = CreatePackage(*OutPath);
		if (!Test.TestNotNull(TEXT("created a package for the transient test Blueprint"), Package))
		{
			return nullptr;
		}
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass, Package, FName(*Leaf), BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		if (!Test.TestNotNull(TEXT("CreateBlueprint returned a Blueprint"), Blueprint))
		{
			return nullptr;
		}
		FAssetRegistryModule::AssetCreated(Blueprint);
		return Blueprint;
	}

	static void ReleaseVariableKeyFixture(UBlueprint* Blueprint)
	{
		if (Blueprint)
		{
			FAssetRegistryModule::AssetDeleted(Blueprint);
			Blueprint->ClearFlags(RF_Standalone | RF_Public);
		}
	}

	static UK2Node_Variable* FindVariableNode(UBlueprint* Blueprint, const FString& NodeId)
	{
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			if (!Graph)
			{
				continue;
			}
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node && Node->NodeGuid.ToString() == NodeId)
				{
					return Cast<UK2Node_Variable>(Node);
				}
			}
		}
		return nullptr;
	}

	// Spawn by key and check the node is the expected get/set, bound to Member, self or external.
	static void ExpectBound(FAutomationTestBase& Test, UBlueprint* Blueprint, const FString& Path, const FString& Key,
		UClass* ExpectedNodeClass, FName Member, UClass* ExternalOwner)
	{
		const FString NodeId = UBlueprintService::CreateNodeByKey(Path, TEXT("EventGraph"), Key, 0.0f, 0.0f);
		if (!Test.TestFalse(FString::Printf(TEXT("'%s' creates a node"), *Key), NodeId.IsEmpty()))
		{
			return;
		}
		UK2Node_Variable* Node = FindVariableNode(Blueprint, NodeId);
		if (!Test.TestNotNull(FString::Printf(TEXT("'%s' returns the id of a variable node"), *Key), Node))
		{
			return;
		}
		Test.TestTrue(FString::Printf(TEXT("'%s' makes a %s (got %s)"), *Key, *ExpectedNodeClass->GetName(), *Node->GetClass()->GetName()),
			Node->IsA(ExpectedNodeClass));
		Test.TestEqual(FString::Printf(TEXT("'%s' binds the member %s"), *Key, *Member.ToString()),
			Node->VariableReference.GetMemberName().ToString(), Member.ToString());
		if (ExternalOwner)
		{
			Test.TestFalse(FString::Printf(TEXT("'%s' is an external member (wired through a Target pin)"), *Key),
				Node->VariableReference.IsSelfContext());
			Test.TestTrue(FString::Printf(TEXT("'%s' names %s as the owner"), *Key, *ExternalOwner->GetName()),
				Node->VariableReference.GetMemberParentClass() == ExternalOwner);
		}
		else
		{
			Test.TestTrue(FString::Printf(TEXT("'%s' is a self member"), *Key), Node->VariableReference.IsSelfContext());
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeCreateNodeByKeyQualifiedKeyTest, "VibeUE.BlueprintService.CreateNodeByKeyQualifiedKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeCreateNodeByKeyQualifiedKeyTest::RunTest(const FString&)
{
	using namespace VibeVariableKeyTestUtil;

	// An Object Blueprint is not an Actor, so Actor/Character properties bind as EXTERNAL members.
	FString ObjectPath;
	FString ActorPath;
	UBlueprint* ObjectBP = MakeVariableKeyFixture(*this, TEXT("BP_QualifiedKeyObject"), UObject::StaticClass(), ObjectPath);
	UBlueprint* ActorBP = MakeVariableKeyFixture(*this, TEXT("BP_QualifiedKeyActor"), AActor::StaticClass(), ActorPath);
	ON_SCOPE_EXIT { ReleaseVariableKeyFixture(ObjectBP); ReleaseVariableKeyFixture(ActorBP); };
	if (!ObjectBP || !ActorBP)
	{
		return false;
	}

	// AActor::bCanBeDamaged is private, so GET_MEMBER_NAME_CHECKED cannot name it.
	const FName CanBeDamaged(TEXT("bCanBeDamaged"));

	// A bool by its menu (display) spelling: the exact call that returned "" live under #610.
	ExpectBound(*this, ObjectBP, ObjectPath, TEXT("SPAWN K2Node_VariableGet|Get Can Be Damaged|Actor"),
		UK2Node_VariableGet::StaticClass(), CanBeDamaged, AActor::StaticClass());
	ExpectBound(*this, ObjectBP, ObjectPath, TEXT("SPAWN K2Node_VariableSet|Set Can Be Damaged|Actor"),
		UK2Node_VariableSet::StaticClass(), CanBeDamaged, AActor::StaticClass());
	// ... and by its raw name.
	ExpectBound(*this, ObjectBP, ObjectPath, TEXT("SPAWN K2Node_VariableGet|Get bCanBeDamaged|Actor"),
		UK2Node_VariableGet::StaticClass(), CanBeDamaged, AActor::StaticClass());
	// A non-bool on a class further down, the form the skill documents.
	ExpectBound(*this, ObjectBP, ObjectPath, TEXT("SPAWN K2Node_VariableGet|Get Jump Max Count|Character"),
		UK2Node_VariableGet::StaticClass(), GET_MEMBER_NAME_CHECKED(ACharacter, JumpMaxCount), ACharacter::StaticClass());
	// When the target Blueprint already derives from the named owner, it is a self member (no Target pin).
	ExpectBound(*this, ActorBP, ActorPath, TEXT("SPAWN K2Node_VariableGet|Get Can Be Damaged|Actor"),
		UK2Node_VariableGet::StaticClass(), CanBeDamaged, nullptr);

	// Refusals: an owner that has no such property, and an owner that does not resolve.
	AddExpectedError(TEXT("qualified SPAWN key"), EAutomationExpectedErrorFlags::Contains, 2);
	TestTrue(TEXT("a qualified key naming a property the class lacks is refused"),
		UBlueprintService::CreateNodeByKey(ObjectPath, TEXT("EventGraph"), TEXT("SPAWN K2Node_VariableGet|Get Vibe No Such Prop 610|Actor"), 0.0f, 0.0f).IsEmpty());
	TestTrue(TEXT("a qualified key naming an unknown class is refused"),
		UBlueprintService::CreateNodeByKey(ObjectPath, TEXT("EventGraph"), TEXT("SPAWN K2Node_VariableGet|Get Can Be Damaged|VibeNoSuchClass610"), 0.0f, 0.0f).IsEmpty());

	// The same call through the Python API an agent uses.
	const FString Json = UPythonTools::ExecutePythonCode(FString::Printf(
		TEXT("import unreal\n")
		TEXT("print('VIBEUE_QKEY [%%s]' %% unreal.BlueprintService.create_node_by_key('%s', 'EventGraph', 'SPAWN K2Node_VariableGet|Get Can Be Damaged|Actor', 0.0, 0.0))\n"),
		*ObjectPath), /*bAutoSave=*/false);
	TSharedPtr<FJsonObject> Obj;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Obj);
	if (TestTrue(TEXT("Python run: reply parses"), Obj.IsValid()) && TestTrue(TEXT("Python run succeeds"), Obj->GetBoolField(TEXT("success"))))
	{
		const FString Output = Obj->GetStringField(TEXT("output"));
		TestTrue(FString::Printf(TEXT("create_node_by_key from Python returns a node id for the bool (output: %s)"), *Output),
			Output.Contains(TEXT("VIBEUE_QKEY [")) && !Output.Contains(TEXT("VIBEUE_QKEY []")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeCreateNodeByKeyFreshVariableTest, "VibeUE.BlueprintService.CreateNodeByKeyBindsFreshVariable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeCreateNodeByKeyFreshVariableTest::RunTest(const FString&)
{
	using namespace VibeVariableKeyTestUtil;

	FString Path;
	UBlueprint* Blueprint = MakeVariableKeyFixture(*this, TEXT("BP_FreshVariable"), AActor::StaticClass(), Path);
	ON_SCOPE_EXIT { ReleaseVariableKeyFixture(Blueprint); };
	if (!Blueprint)
	{
		return false;
	}

	// Added through the service and NOT compiled: the step-one / step-two flow #609 broke.
	if (!TestTrue(TEXT("add_member_variable int"), UBlueprintService::AddMemberVariable(Path, TEXT("VibeFreshCount609"), TEXT("int")))
		|| !TestTrue(TEXT("add_member_variable bool"), UBlueprintService::AddMemberVariable(Path, TEXT("bVibeFreshFlag609"), TEXT("bool"))))
	{
		return false;
	}

	// Before a compile the action database names a new variable's spawner by its RAW name ("Get
	// VibeFreshCount609"), so the raw spellings may be served by a spawner. The menu spellings below are
	// what an agent reads in the editor, and they match no spawner yet: only the fallback binds them.
	ExpectBound(*this, Blueprint, Path, TEXT("SPAWN K2Node_VariableGet|Get VibeFreshCount609"),
		UK2Node_VariableGet::StaticClass(), FName(TEXT("VibeFreshCount609")), nullptr);
	ExpectBound(*this, Blueprint, Path, TEXT("SPAWN K2Node_VariableSet|Set VibeFreshCount609"),
		UK2Node_VariableSet::StaticClass(), FName(TEXT("VibeFreshCount609")), nullptr);
	ExpectBound(*this, Blueprint, Path, TEXT("SPAWN K2Node_VariableGet|Get bVibeFreshFlag609"),
		UK2Node_VariableGet::StaticClass(), FName(TEXT("bVibeFreshFlag609")), nullptr);
	// The menu spellings, the bool without its "b" prefix (#609's fallback; observed 2026-11-25).
	ExpectBound(*this, Blueprint, Path, TEXT("SPAWN K2Node_VariableGet|Get Vibe Fresh Count 609"),
		UK2Node_VariableGet::StaticClass(), FName(TEXT("VibeFreshCount609")), nullptr);
	ExpectBound(*this, Blueprint, Path, TEXT("SPAWN K2Node_VariableGet|Get Vibe Fresh Flag 609"),
		UK2Node_VariableGet::StaticClass(), FName(TEXT("bVibeFreshFlag609")), nullptr);

	// The fallback does not invent members: a name the Blueprint does not have is still refused.
	AddExpectedError(TEXT("No action-database spawner matched"), EAutomationExpectedErrorFlags::Contains, 1);
	TestTrue(TEXT("a variable the Blueprint never declared is refused"),
		UBlueprintService::CreateNodeByKey(Path, TEXT("EventGraph"), TEXT("SPAWN K2Node_VariableGet|Get VibeNeverDeclared609"), 0.0f, 0.0f).IsEmpty());

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	TestTrue(TEXT("the Blueprint with the bound nodes compiles"), Blueprint->Status != BS_Error);
	return true;
}

#endif // WITH_AUTOMATION_TESTS
