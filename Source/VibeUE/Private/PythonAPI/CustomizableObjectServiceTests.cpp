// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "EQSServiceTestFixture.h"
#include "PythonAPI/UCustomizableObjectService.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Every test here needs the Mutable plugin loaded in the host project (the UE 5.8 default, through
// MutableAssetUserData). A project that disables it gets an info line instead: VibeUE deliberately
// does not enable Mutable.

static const EAutomationTestFlags kMutableTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

namespace VibeCOTest
{
	const TCHAR* const Dir = TEXT("/Game/Developers/VibeUEMutableTests");
	const TCHAR* const Mesh = TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube");
	const TCHAR* const Material = TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial");

	using S = UCustomizableObjectService;

	FString AssetPath(const TCHAR* Name)
	{
		return FString(Dir) / Name;
	}

	FString Props(std::initializer_list<TPair<FString, FString>> Entries)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		for (const TPair<FString, FString>& Entry : Entries)
		{
			Object->SetStringField(Entry.Key, Entry.Value);
		}
		FString Out;
		FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Out));
		return Out;
	}

	int32 CountPins(const TArray<FVibeCOPinInfo>& Pins, const TCHAR* Direction, const TCHAR* Category)
	{
		int32 Count = 0;
		for (const FVibeCOPinInfo& Pin : Pins)
		{
			Count += (Pin.Direction == Direction && Pin.Category == Category) ? 1 : 0;
		}
		return Count;
	}

	bool HasPin(const TArray<FVibeCOPinInfo>& Pins, const TCHAR* Name)
	{
		return Pins.ContainsByPredicate([Name](const FVibeCOPinInfo& Pin) { return Pin.PinName == Name; });
	}

	/** The minimal compilable chain into the Base Object. Returns false (with test errors) on any failure. */
	bool BuildMeshChain(FAutomationTestBase& Test, const FString& Path, const FString& BaseNodeId,
		bool bWithReferenceMesh, FString* OutSectionId = nullptr)
	{
		const FVibeCOEditResult SkeletalMesh = S::AddNode(Path, TEXT("CustomizableObjectNodeSkeletalMesh"), -1200, 0,
			Props({ { TEXT("SkeletalMesh"), Mesh } }));
		const FVibeCOEditResult Section = S::AddNode(Path, TEXT("CONodeSkeletalMeshSection"), -900, 0,
			Props({ { TEXT("Material"), Material } }));
		const FVibeCOEditResult Make = S::AddNode(Path, TEXT("CONodeSkeletalMeshMake_V2"), -600, 0);
		const FVibeCOEditResult ObjectMake = S::AddNode(Path, TEXT("CONodeSkeletalMeshObjectMake"), -400, 0);
		const FVibeCOEditResult Component = S::AddNode(Path, TEXT("CONodeComponentSkeletalMesh"), -200, 0,
			bWithReferenceMesh
				? Props({ { TEXT("ReferenceSkeletalMesh"), Mesh }, { TEXT("ComponentName"), TEXT("Body") } })
				: Props({ { TEXT("ComponentName"), TEXT("Body") } }));
		for (const FVibeCOEditResult* Added : { &SkeletalMesh, &Section, &Make, &ObjectMake, &Component })
		{
			if (!Test.TestTrue(FString::Printf(TEXT("chain node added: %s"), *Added->Error), Added->bSuccess))
			{
				return false;
			}
		}
		// A Skeletal Mesh node given its mesh up front is created with one output per LOD/section.
		Test.TestTrue(TEXT("skeletal mesh node has its LOD/section pin"),
			HasPin(SkeletalMesh.Pins, TEXT("LOD 0 - Section 0 - Mesh")));

		// Mixed pin addressing on purpose: display names ("Mesh", "Mesh Section") and internal names.
		const FVibeCOEditResult Links[] = {
			S::ConnectPins(Path, SkeletalMesh.NodeId, TEXT("LOD 0 - Section 0"), Section.NodeId, TEXT("Mesh")),
			S::ConnectPins(Path, Section.NodeId, TEXT("Mesh Section"), Make.NodeId, TEXT("LOD 0")),
			S::ConnectPins(Path, Make.NodeId, TEXT("Skeletal Mesh"), ObjectMake.NodeId, TEXT("Skeletal Mesh")),
			S::ConnectPins(Path, ObjectMake.NodeId, TEXT("Passthrough Skeletal Mesh"), Component.NodeId, TEXT("Passthrough Skeletal Mesh")),
			S::ConnectPins(Path, Component.NodeId, TEXT("Component"), BaseNodeId, TEXT("Components")),
		};
		bool bAllLinked = true;
		for (const FVibeCOEditResult& Link : Links)
		{
			bAllLinked &= Test.TestTrue(FString::Printf(TEXT("chain link: %s"), *Link.Error), Link.bSuccess);
		}
		if (OutSectionId)
		{
			*OutSectionId = Section.NodeId;
		}
		return bAllLinked;
	}
}

#define VIBE_CO_REQUIRE_MUTABLE() \
	if (!UCustomizableObjectService::IsMutableAvailable()) \
	{ \
		AddInfo(TEXT("skipped: the Mutable plugin is not enabled in this project")); \
		return true; \
	}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeCODiscoveryTest, "VibeUE.Mutable.Discovery", kMutableTestFlags)
bool FVibeCODiscoveryTest::RunTest(const FString&)
{
	VIBE_CO_REQUIRE_MUTABLE();
	using namespace VibeCOTest;

	const TArray<FVibeCONodeTypeInfo> Placeable = S::DiscoverNodeTypes();
	const TArray<FVibeCONodeTypeInfo> All = S::DiscoverNodeTypes(TEXT(""), true);
	TestTrue(TEXT("finds placeable node types"), Placeable.Num() > 50);
	TestTrue(TEXT("hidden types widen the list"), All.Num() > Placeable.Num());

	auto Contains = [](const TArray<FVibeCONodeTypeInfo>& Types, const TCHAR* Name)
	{
		return Types.ContainsByPredicate([Name](const FVibeCONodeTypeInfo& Type) { return Type.ClassName == Name; });
	};
	for (const TCHAR* Required : { TEXT("CustomizableObjectNodeObject"), TEXT("CustomizableObjectNodeObjectGroup"),
			TEXT("CustomizableObjectNodeSkeletalMesh"), TEXT("CONodeSkeletalMeshSection"), TEXT("CONodeSkeletalMeshMake_V2"),
			TEXT("CONodeSkeletalMeshObjectMake"), TEXT("CONodeComponentSkeletalMesh"), TEXT("CONodeSwitch"),
			TEXT("CustomizableObjectNodeEnumParameter"), TEXT("CustomizableObjectNodeFloatParameter") })
	{
		TestTrue(FString::Printf(TEXT("placeable: %s"), Required), Contains(Placeable, Required));
	}
	TestFalse(TEXT("abstract bases are never listed"), Contains(All, TEXT("CustomizableObjectNodeParameter")));
	TestFalse(TEXT("typed switch subclasses are hidden by default"), Contains(Placeable, TEXT("CustomizableObjectNodeMaterialSwitch")));

	const TArray<FVibeCONodeTypeInfo> Filtered = S::DiscoverNodeTypes(TEXT("Parameter"));
	TestTrue(TEXT("filter narrows"), Filtered.Num() > 0 && Filtered.Num() < Placeable.Num());

	const TArray<FVibeCOPropertyInfo> SwitchProps = S::GetNodeTypeProperties(TEXT("CONodeSwitch"));
	TestTrue(TEXT("CONodeSwitch exposes PinType"),
		SwitchProps.ContainsByPredicate([](const FVibeCOPropertyInfo& P) { return P.Name == TEXT("PinType"); }));
	TestTrue(TEXT("U-prefixed names resolve"), S::GetNodeTypeProperties(TEXT("UCONodeSwitch")).Num() > 0);
	TestTrue(TEXT("legacy redirected names resolve"), S::GetNodeTypeProperties(TEXT("CustomizableObjectNodeMaterial")).Num() > 0);
	TestEqual(TEXT("unknown class resolves to nothing"), S::GetNodeTypeProperties(TEXT("NotANodeClass")).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeCOCreateTest, "VibeUE.Mutable.Asset.Create", kMutableTestFlags)
bool FVibeCOCreateTest::RunTest(const FString&)
{
	VIBE_CO_REQUIRE_MUTABLE();
	using namespace VibeCOTest;
	const FString Path = AssetPath(TEXT("CO_Create"));
	FEQSScopedFixtureReset Reset(Path);

	const FVibeCOEditResult Created = S::CreateCustomizableObject(Path);
	if (!TestTrue(FString::Printf(TEXT("create: %s"), *Created.Error), Created.bSuccess))
	{
		return false;
	}
	TestTrue(TEXT("saved to disk"), FEQSScopedFixtureReset::FixtureFileExists(Path));
	TestTrue(TEXT("base node has Components input"), HasPin(Created.Pins, TEXT("Components")));
	TestTrue(TEXT("listed"), S::ListCustomizableObjects(Dir).Contains(Path));

	const FVibeCOGraphInfo Graph = S::GetGraph(Path);
	TestTrue(TEXT("graph reads"), Graph.bSuccess);
	TestEqual(TEXT("one node"), Graph.Nodes.Num(), 1);
	if (Graph.Nodes.Num() == 1)
	{
		TestEqual(TEXT("it is the base node"), Graph.Nodes[0].NodeId, Created.NodeId);
		TestFalse(TEXT("base node is not deletable"), Graph.Nodes[0].bCanDelete);
	}
	TestEqual(TEXT("object info names the base node"), S::GetObjectInfo(Path).BaseNodeId, Created.NodeId);

	TestFalse(TEXT("duplicate refused"), S::CreateCustomizableObject(Path).bSuccess);
	TestTrue(TEXT("engine content refused"), S::CreateCustomizableObject(TEXT("/Engine/VibeUE/CO_X")).Error.Contains(TEXT("/Engine/")));
	const FVibeCOEditResult RemoveBase = S::RemoveNode(Path, Created.NodeId);
	TestFalse(TEXT("base node removal refused"), RemoveBase.bSuccess);
	TestTrue(TEXT("wrong asset type is named"), S::GetGraph(TEXT("/Engine/EngineMeshes/SkeletalCube")).Error.Contains(TEXT("not a Customizable Object")));
	TestTrue(TEXT("missing asset is reported"), S::GetGraph(AssetPath(TEXT("CO_DoesNotExist"))).Error.Contains(TEXT("not found")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeCOPlaceEveryTypeTest, "VibeUE.Mutable.Nodes.PlaceEveryType", kMutableTestFlags)
bool FVibeCOPlaceEveryTypeTest::RunTest(const FString&)
{
	VIBE_CO_REQUIRE_MUTABLE();
	using namespace VibeCOTest;
	const FString Path = AssetPath(TEXT("CO_EveryType"));
	FEQSScopedFixtureReset Reset(Path);
	if (!TestTrue(TEXT("create"), S::CreateCustomizableObject(Path).bSuccess))
	{
		return false;
	}

	// Every class AddNode accepts — placeable, hidden and legacy alike — must construct through the
	// generic path without asserting, and come out removable. A crash here is exactly what
	// reflection-only construction risks (Tunnel nodes did, and are now refused up front).
	for (const FVibeCONodeTypeInfo& Type : S::DiscoverNodeTypes(TEXT(""), /*bIncludeHidden*/ true))
	{
		if (Type.ClassName == TEXT("CustomizableObjectNodeTunnel"))
		{
			TestFalse(TEXT("macro-only Tunnel is refused, not constructed"), S::AddNode(Path, Type.ClassName).bSuccess);
			TestFalse(TEXT("Tunnel is not placeable"), Type.bPlaceable);
			continue;
		}
		const FVibeCOEditResult Added = S::AddNode(Path, Type.ClassName, 0, 0);
		if (!TestTrue(FString::Printf(TEXT("add %s: %s"), *Type.ClassName, *Added.Error), Added.bSuccess))
		{
			continue;
		}
		const FVibeCONodeInfo Node = S::GetNode(Path, Added.NodeId);
		TestEqual(FString::Printf(TEXT("%s reads back"), *Type.ClassName), Node.NodeClass, Type.ClassName);
		S::GetNodeProperties(Path, Added.NodeId);
		if (Node.bCanDelete)
		{
			const FVibeCOEditResult Removed = S::RemoveNode(Path, Added.NodeId);
			TestTrue(FString::Printf(TEXT("remove %s: %s"), *Type.ClassName, *Removed.Error), Removed.bSuccess);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeCOCompileTest, "VibeUE.Mutable.Compile.MinimalChain", kMutableTestFlags)
bool FVibeCOCompileTest::RunTest(const FString&)
{
	VIBE_CO_REQUIRE_MUTABLE();
	using namespace VibeCOTest;
	const FString Path = AssetPath(TEXT("CO_Compile"));
	FEQSScopedFixtureReset Reset(Path);
	const FVibeCOEditResult Created = S::CreateCustomizableObject(Path);
	if (!TestTrue(TEXT("create"), Created.bSuccess) || !BuildMeshChain(*this, Path, Created.NodeId, true))
	{
		return false;
	}
	TestEqual(TEXT("five connections"), S::GetGraph(Path).Connections.Num(), 5);
	TestFalse(TEXT("not compiled before compile"), S::GetObjectInfo(Path).bIsCompiled);

	const FVibeCOCompileResult Compiled = S::CompileObject(Path);
	TestTrue(FString::Printf(TEXT("compiles (%s)"), *FString::Join(Compiled.Errors, TEXT(" | "))), Compiled.bSuccess);
	TestEqual(TEXT("state"), Compiled.State, FString(TEXT("Completed")));
	TestEqual(TEXT("no warnings with a material assigned"), Compiled.Warnings.Num(), 0);
	TestTrue(TEXT("compile log captured"), Compiled.Log.Num() > 0);

	const FVibeCOObjectInfo Info = S::GetObjectInfo(Path);
	TestTrue(TEXT("compiled"), Info.bIsCompiled);
	TestEqual(TEXT("component named"), Info.Components, TArray<FString>{ TEXT("Body") });
	TestTrue(TEXT("has a state"), Info.States.Num() > 0);

	TestTrue(TEXT("unknown optimization level refused"), S::CompileObject(Path, TEXT("Turbo")).Error.Contains(TEXT("OptimizationLevel")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeCOCompileFailureTest, "VibeUE.Mutable.Compile.ReportsErrors", kMutableTestFlags)
bool FVibeCOCompileFailureTest::RunTest(const FString&)
{
	VIBE_CO_REQUIRE_MUTABLE();
	using namespace VibeCOTest;
	const FString Path = AssetPath(TEXT("CO_CompileFail"));
	FEQSScopedFixtureReset Reset(Path);
	const FVibeCOEditResult Created = S::CreateCustomizableObject(Path);
	if (!TestTrue(TEXT("create"), Created.bSuccess) || !BuildMeshChain(*this, Path, Created.NodeId, false))
	{
		return false;
	}

	// A component with no ReferenceSkeletalMesh is a hard compiler error, reported only through the
	// "Mutable" message log. The service must surface it rather than report a quiet failure.
	AddExpectedError(TEXT("Missing reference Skeletal Mesh"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedError(TEXT("finished compiling with"), EAutomationExpectedErrorFlags::Contains, 0);
	const FVibeCOCompileResult Compiled = S::CompileObject(Path);
	TestFalse(TEXT("compile fails"), Compiled.bSuccess);
	TestEqual(TEXT("state"), Compiled.State, FString(TEXT("Failed")));
	TestFalse(TEXT("no model"), Compiled.bIsCompiled);
	TestTrue(TEXT("the compiler's error is returned"), Compiled.Errors.ContainsByPredicate(
		[](const FString& Error) { return Error.Contains(TEXT("Missing reference Skeletal Mesh")); }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeCOSwitchTest, "VibeUE.Mutable.Nodes.SwitchFollowsEnum", kMutableTestFlags)
bool FVibeCOSwitchTest::RunTest(const FString&)
{
	VIBE_CO_REQUIRE_MUTABLE();
	using namespace VibeCOTest;
	const FString Path = AssetPath(TEXT("CO_Switch"));
	FEQSScopedFixtureReset Reset(Path);
	if (!TestTrue(TEXT("create"), S::CreateCustomizableObject(Path).bSuccess))
	{
		return false;
	}

	const FVibeCOEditResult Enum = S::AddNode(Path, TEXT("CustomizableObjectNodeEnumParameter"), 0, 0,
		Props({ { TEXT("ParameterName"), TEXT("Style") }, { TEXT("Values"), TEXT("((Name=\"Red\"),(Name=\"Blue\"))") } }));
	const FVibeCOEditResult Switch = S::AddNode(Path, TEXT("CONodeSwitch"), 300, 0, Props({ { TEXT("PinType"), TEXT("material") } }));
	TestTrue(FString::Printf(TEXT("enum added: %s"), *Enum.Error), Enum.bSuccess);
	TestTrue(FString::Printf(TEXT("switch added: %s"), *Switch.Error), Switch.bSuccess);
	TestEqual(TEXT("typed switch output"), CountPins(Switch.Pins, TEXT("output"), TEXT("material")), 1);

	const FVibeCOEditResult Linked = S::ConnectPins(Path, Enum.NodeId, TEXT("Enum"), Switch.NodeId, TEXT("Switch Parameter"));
	TestTrue(FString::Printf(TEXT("enum -> switch: %s"), *Linked.Error), Linked.bSuccess);
	TestEqual(TEXT("one option pin per enum value"), CountPins(Linked.Pins, TEXT("input"), TEXT("material")), 2);

	// Editing the enum alone does not rebuild the switch in the engine; the service must.
	const FVibeCOEditResult Edited = S::SetNodeProperty(Path, Enum.NodeId, TEXT("Values"),
		TEXT("((Name=\"Red\"),(Name=\"Blue\"),(Name=\"Green\"))"));
	TestTrue(FString::Printf(TEXT("values edited: %s"), *Edited.Error), Edited.bSuccess);
	const FVibeCONodeInfo After = S::GetNode(Path, Switch.NodeId);
	TestEqual(TEXT("switch follows the enum"), CountPins(After.Pins, TEXT("input"), TEXT("material")), 3);
	TestTrue(TEXT("new option pin named"), HasPin(After.Pins, TEXT("Green")));

	const FVibeCOObjectInfo Info = S::GetObjectInfo(Path);
	const FVibeCOParameterInfo* Style = Info.GraphParameters.FindByPredicate(
		[](const FVibeCOParameterInfo& P) { return P.Name == TEXT("Style"); });
	if (TestNotNull(TEXT("graph parameter listed"), Style))
	{
		TestEqual(TEXT("options"), Style->Options, TArray<FString>{ TEXT("Red"), TEXT("Blue"), TEXT("Green") });
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeCOPropertyTest, "VibeUE.Mutable.Nodes.PropertyValidation", kMutableTestFlags)
bool FVibeCOPropertyTest::RunTest(const FString&)
{
	VIBE_CO_REQUIRE_MUTABLE();
	using namespace VibeCOTest;
	const FString Path = AssetPath(TEXT("CO_Properties"));
	FEQSScopedFixtureReset Reset(Path);
	const FVibeCOEditResult Created = S::CreateCustomizableObject(Path);
	if (!TestTrue(TEXT("create"), Created.bSuccess))
	{
		return false;
	}
	const FVibeCOEditResult Enum = S::AddNode(Path, TEXT("CustomizableObjectNodeEnumParameter"));
	const FVibeCOEditResult Group = S::AddNode(Path, TEXT("CustomizableObjectNodeObjectGroup"));

	// A second (or no) Base Object would break the compiler; node identity is not an authoring knob.
	TestFalse(TEXT("bIsBase refused"), S::SetNodeProperty(Path, Created.NodeId, TEXT("bIsBase"), TEXT("False")).bSuccess);
	TestFalse(TEXT("Identifier refused"), S::SetNodeProperty(Path, Created.NodeId, TEXT("Identifier"),
		TEXT("00000000000000000000000000000001")).bSuccess);
	TestTrue(TEXT("ObjectName settable"), S::SetNodeProperty(Path, Created.NodeId, TEXT("ObjectName"), TEXT("Hero")).bSuccess);

	const FVibeCOEditResult Good = S::SetNodeProperty(Path, Enum.NodeId, TEXT("DefaultIndex"), TEXT("2"));
	TestTrue(TEXT("int accepted"), Good.bSuccess);
	TestEqual(TEXT("read back"), Good.ValueAfterWrite, FString(TEXT("2")));

	// ImportText would take "abc" as 0 and report success; the service must not.
	const FVibeCOEditResult BadInt = S::SetNodeProperty(Path, Enum.NodeId, TEXT("DefaultIndex"), TEXT("abc"));
	TestFalse(TEXT("non-number refused"), BadInt.bSuccess);
	TestEqual(TEXT("value unchanged"), BadInt.ValueAfterWrite, FString(TEXT("2")));
	TestFalse(TEXT("unknown enum value refused"), S::SetNodeProperty(Path, Group.NodeId, TEXT("GroupType"), TEXT("Nope")).bSuccess);
	TestTrue(TEXT("enum value accepted"), S::SetNodeProperty(Path, Group.NodeId, TEXT("GroupType"), TEXT("COGT_ONE")).bSuccess);
	TestFalse(TEXT("unknown property refused"), S::SetNodeProperty(Path, Enum.NodeId, TEXT("NoSuchProperty"), TEXT("1")).bSuccess);

	// Pin handles are node bookkeeping: hidden from listings and refused on write.
	const TArray<FVibeCOPropertyInfo> Listed = S::GetNodeProperties(Path, Enum.NodeId);
	TestFalse(TEXT("pin references not listed"),
		Listed.ContainsByPredicate([](const FVibeCOPropertyInfo& P) { return P.Type.Contains(TEXT("EdGraphPinReference")); }));
	TestFalse(TEXT("pin reference write refused"), S::SetNodeProperty(Path, Enum.NodeId, TEXT("NamePin"), TEXT("()")).bSuccess);
	TestTrue(TEXT("custom-UI name property listed"),
		Listed.ContainsByPredicate([](const FVibeCOPropertyInfo& P) { return P.Name == TEXT("ParameterName"); }));

	TestFalse(TEXT("bad PropertiesJson refused"), S::AddNode(Path, TEXT("CONodeSwitch"), 0, 0, TEXT("not json")).bSuccess);
	TestFalse(TEXT("bad property in PropertiesJson refused"),
		S::AddNode(Path, TEXT("CustomizableObjectNodeEnumParameter"), 0, 0, Props({ { TEXT("DefaultIndex"), TEXT("x") } })).bSuccess);
	TestEqual(TEXT("refused adds left no node behind"), S::GetGraph(Path).Nodes.Num(), 3);
	TestFalse(TEXT("abstract class refused"), S::AddNode(Path, TEXT("CustomizableObjectNodeParameter")).bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeCOConnectionTest, "VibeUE.Mutable.Nodes.Connections", kMutableTestFlags)
bool FVibeCOConnectionTest::RunTest(const FString&)
{
	VIBE_CO_REQUIRE_MUTABLE();
	using namespace VibeCOTest;
	const FString Path = AssetPath(TEXT("CO_Connections"));
	FEQSScopedFixtureReset Reset(Path);
	const FVibeCOEditResult Created = S::CreateCustomizableObject(Path);
	if (!TestTrue(TEXT("create"), Created.bSuccess))
	{
		return false;
	}
	const FVibeCOEditResult Enum = S::AddNode(Path, TEXT("CustomizableObjectNodeEnumParameter"));
	const FVibeCOEditResult GroupA = S::AddNode(Path, TEXT("CustomizableObjectNodeObjectGroup"));
	const FVibeCOEditResult GroupB = S::AddNode(Path, TEXT("CustomizableObjectNodeObjectGroup"));

	const FVibeCOEditResult Mismatch = S::ConnectPins(Path, Enum.NodeId, TEXT("Enum"), Created.NodeId, TEXT("Components"));
	TestFalse(TEXT("category mismatch refused"), Mismatch.bSuccess);
	TestTrue(TEXT("schema reason passed through"), Mismatch.Error.Contains(TEXT("enum")) && Mismatch.Error.Contains(TEXT("component")));

	// Named input-first: the service reorders.
	const FVibeCOEditResult Reversed = S::ConnectPins(Path, Created.NodeId, TEXT("Children"), GroupA.NodeId, TEXT("Group"));
	TestTrue(FString::Printf(TEXT("either order connects: %s"), *Reversed.Error), Reversed.bSuccess);
	TestTrue(TEXT("array input takes a second link"),
		S::ConnectPins(Path, GroupB.NodeId, TEXT("Group"), Created.NodeId, TEXT("Children")).bSuccess);
	TestEqual(TEXT("two links"), S::GetGraph(Path).Connections.Num(), 2);

	TestFalse(TEXT("unknown pin refused"), S::ConnectPins(Path, GroupA.NodeId, TEXT("Nope"), Created.NodeId, TEXT("Children")).bSuccess);

	const FVibeCOEditResult One = S::DisconnectPins(Path, Created.NodeId, TEXT("Children"), GroupA.NodeId, TEXT("Group"));
	TestTrue(TEXT("single link broken"), One.bSuccess);
	TestEqual(TEXT("one link left"), S::GetGraph(Path).Connections.Num(), 1);
	TestFalse(TEXT("breaking a missing link refused"),
		S::DisconnectPins(Path, Created.NodeId, TEXT("Children"), GroupA.NodeId, TEXT("Group")).bSuccess);
	TestTrue(TEXT("all links broken"), S::DisconnectPins(Path, Created.NodeId, TEXT("Children")).bSuccess);
	TestEqual(TEXT("none left"), S::GetGraph(Path).Connections.Num(), 0);
	const FVibeCOEditResult NoOp = S::DisconnectPins(Path, Created.NodeId, TEXT("Children"));
	TestTrue(TEXT("disconnecting a bare pin is a no-op"), NoOp.bSuccess && NoOp.Notes.Num() == 1);

	TestTrue(TEXT("move"), S::SetNodePosition(Path, GroupA.NodeId, 123, -45).bSuccess);
	const FVibeCONodeInfo Moved = S::GetNode(Path, GroupA.NodeId);
	TestEqual(TEXT("x"), Moved.PosX, 123);
	TestEqual(TEXT("y"), Moved.PosY, -45);
	TestTrue(TEXT("refresh"), S::RefreshNode(Path, GroupA.NodeId).bSuccess);
	TestTrue(TEXT("remove"), S::RemoveNode(Path, GroupB.NodeId).bSuccess);
	TestTrue(TEXT("removed node is gone"), S::GetNode(Path, GroupB.NodeId).NodeId.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeCOChildObjectTest, "VibeUE.Mutable.Asset.ChildObjects", kMutableTestFlags)
bool FVibeCOChildObjectTest::RunTest(const FString&)
{
	VIBE_CO_REQUIRE_MUTABLE();
	using namespace VibeCOTest;
	const FString Root = AssetPath(TEXT("CO_ChildRoot"));
	const FString Child = AssetPath(TEXT("CO_ChildCap"));
	FEQSScopedFixtureReset ResetChild(Child);
	FEQSScopedFixtureReset ResetRoot(Root);

	const FVibeCOEditResult RootCreated = S::CreateCustomizableObject(Root);
	const FVibeCOEditResult ChildCreated = S::CreateCustomizableObject(Child);
	if (!TestTrue(TEXT("create both"), RootCreated.bSuccess && ChildCreated.bSuccess)
		|| !BuildMeshChain(*this, Root, RootCreated.NodeId, true))
	{
		return false;
	}
	const FVibeCOEditResult Group = S::AddNode(Root, TEXT("CustomizableObjectNodeObjectGroup"), 0, 300,
		Props({ { TEXT("GroupName"), TEXT("Hat") }, { TEXT("GroupType"), TEXT("COGT_ONE_OR_NONE") } }));
	TestTrue(TEXT("group linked"), S::ConnectPins(Root, Group.NodeId, TEXT("Group"), RootCreated.NodeId, TEXT("Children")).bSuccess);
	TestTrue(TEXT("child named"), S::SetNodeProperty(Child, ChildCreated.NodeId, TEXT("ObjectName"), TEXT("Cap")).bSuccess);

	TestFalse(TEXT("parenting to a non-group node refused"), S::SetParentObject(Child, Root, RootCreated.NodeId).bSuccess);
	const FVibeCOEditResult Parented = S::SetParentObject(Child, Root, Group.NodeId);
	TestTrue(FString::Printf(TEXT("parented: %s"), *Parented.Error), Parented.bSuccess);

	const FVibeCOObjectInfo ChildInfo = S::GetObjectInfo(Child);
	TestTrue(TEXT("child knows it is a child"), ChildInfo.bIsChildObject);
	TestEqual(TEXT("child names its parent"), ChildInfo.ParentObject, Root);
	TestTrue(TEXT("parent finds the child"), S::GetObjectInfo(Root).ChildObjects.Contains(Child));
	TestFalse(TEXT("cycle refused"), S::SetParentObject(Root, Child, Group.NodeId).bSuccess);

	// The compiler finds children through the same registry lookup; the group's options prove it did.
	const FVibeCOCompileResult Compiled = S::CompileObject(Root);
	TestTrue(FString::Printf(TEXT("root compiles: %s"), *FString::Join(Compiled.Errors, TEXT(" | "))), Compiled.bSuccess);
	const FVibeCOObjectInfo RootInfo = S::GetObjectInfo(Root);
	const FVibeCOParameterInfo* Hat = RootInfo.CompiledParameters.FindByPredicate(
		[](const FVibeCOParameterInfo& P) { return P.Name == TEXT("Hat"); });
	if (TestNotNull(TEXT("group compiled to a parameter"), Hat))
	{
		TestTrue(TEXT("child object is an option"), Hat->Options.Contains(TEXT("Cap")));
	}

	TestTrue(TEXT("detach"), S::SetParentObject(Child, TEXT(""), TEXT("")).bSuccess);
	TestFalse(TEXT("detached"), S::GetObjectInfo(Child).bIsChildObject);
	TestFalse(TEXT("parent no longer lists it"), S::GetObjectInfo(Root).ChildObjects.Contains(Child));
	return true;
}

#undef VIBE_CO_REQUIRE_MUTABLE

#endif // WITH_AUTOMATION_TESTS
