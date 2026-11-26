// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Guid.h"
#include "PythonAPI/UBlueprintService.h"
#include "Tools/PythonTools.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "UObject/Interface.h"
#include "UObject/Package.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_FunctionEntry.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_AUTOMATION_TESTS

// Issue #614: create_function_graph (#607) shipped with no automated test at all - its four refusal
// paths were verified only by a live run recorded in the PR, and a mutation run showed the suite
// stayed green with the duplicate-name and the shadows-a-parent checks deleted. These tests pin the
// behaviour an agent depends on: what is created, and that every refused call leaves the Blueprint
// exactly as it was.

namespace VibeCreateFunctionGraphTestUtil
{
	// An in-memory Blueprint under /Game, registered with the asset registry so the service's
	// path-based API resolves it. Never saved. A unique name keeps runs independent.
	static UBlueprint* MakeFunctionGraphFixture(FAutomationTestBase& Test, const TCHAR* Prefix, UClass* ParentClass,
		EBlueprintType Type, FString& OutPath)
	{
		const FString Leaf = FString::Printf(TEXT("%s_%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		OutPath = FString(TEXT("/Game/__VibeUETest/")) + Leaf;
		UPackage* Package = CreatePackage(*OutPath);
		if (!Test.TestNotNull(TEXT("created a package for the transient test Blueprint"), Package))
		{
			return nullptr;
		}
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass, Package, FName(*Leaf), Type,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		if (!Test.TestNotNull(TEXT("CreateBlueprint returned a Blueprint"), Blueprint))
		{
			return nullptr;
		}
		FAssetRegistryModule::AssetCreated(Blueprint);
		return Blueprint;
	}

	static void ReleaseFunctionGraphFixture(UBlueprint* Blueprint)
	{
		if (Blueprint)
		{
			FAssetRegistryModule::AssetDeleted(Blueprint);
			Blueprint->ClearFlags(RF_Standalone | RF_Public);
		}
	}

	// Every graph name the Blueprint holds (all kinds), so a refusal can be proven to have changed nothing.
	static TArray<FString> AllGraphNames(UBlueprint* Blueprint)
	{
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		TArray<FString> Names;
		for (const UEdGraph* Graph : Graphs)
		{
			if (Graph)
			{
				Names.Add(Graph->GetName());
			}
		}
		Names.Sort();
		return Names;
	}

	static UK2Node_FunctionEntry* FindEntryNode(UBlueprint* Blueprint, const FString& GraphName)
	{
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph && Graph->GetName() == GraphName)
			{
				TArray<UK2Node_FunctionEntry*> Entries;
				Graph->GetNodesOfClass<UK2Node_FunctionEntry>(Entries);
				return Entries.Num() > 0 ? Entries[0] : nullptr;
			}
		}
		return nullptr;
	}

	static TSharedPtr<FJsonObject> RunFunctionGraphPython(const FString& Code)
	{
		// auto_save=false: the fixtures are unsaved scratch packages and must never reach disk.
		const FString Json = UPythonTools::ExecutePythonCode(Code, /*bAutoSave=*/false);
		TSharedPtr<FJsonObject> Obj;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Obj);
		return Obj;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeCreateFunctionGraphRefusalsTest, "VibeUE.BlueprintService.CreateFunctionGraphRefusals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeCreateFunctionGraphRefusalsTest::RunTest(const FString&)
{
	using namespace VibeCreateFunctionGraphTestUtil;

	FString Path;
	UBlueprint* Blueprint = MakeFunctionGraphFixture(*this, TEXT("BP_CreateFunctionGraph"), AActor::StaticClass(), BPTYPE_Normal, Path);
	if (!Blueprint)
	{
		return false;
	}
	ON_SCOPE_EXIT { ReleaseFunctionGraphFixture(Blueprint); };

	// Created: the returned name is the graph's, the graph is a user function with an entry node,
	// and it is impure unless asked otherwise.
	TestEqual(TEXT("create_function_graph returns the new graph's name"),
		UBlueprintService::CreateFunctionGraph(Path, TEXT("VibeOpenDoor"), /*bIsPure*/ false), FString(TEXT("VibeOpenDoor")));
	UK2Node_FunctionEntry* Impure = FindEntryNode(Blueprint, TEXT("VibeOpenDoor"));
	if (TestNotNull(TEXT("VibeOpenDoor is a function graph with an entry node"), Impure))
	{
		TestTrue(TEXT("an impure function carries no BlueprintPure flag"), (Impure->GetExtraFlags() & FUNC_BlueprintPure) == 0);
	}

	TestEqual(TEXT("a pure function is created"),
		UBlueprintService::CreateFunctionGraph(Path, TEXT("VibeGetAngle"), /*bIsPure*/ true), FString(TEXT("VibeGetAngle")));
	UK2Node_FunctionEntry* Pure = FindEntryNode(Blueprint, TEXT("VibeGetAngle"));
	if (TestNotNull(TEXT("VibeGetAngle is a function graph with an entry node"), Pure))
	{
		TestTrue(TEXT("bIsPure marks the entry node BlueprintPure"), (Pure->GetExtraFlags() & FUNC_BlueprintPure) != 0);
	}

	// Surrounding whitespace is trimmed, not refused.
	TestEqual(TEXT("a padded name is trimmed"),
		UBlueprintService::CreateFunctionGraph(Path, TEXT("  VibePadded  "), false), FString(TEXT("VibePadded")));

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	TestTrue(TEXT("the Blueprint with the three new functions compiles"), Blueprint->Status != BS_Error);

	// Every refusal returns "" and logs why, and the Blueprint's graphs are exactly as before.
	AddExpectedError(TEXT("CreateFunctionGraph:"), EAutomationExpectedErrorFlags::Contains, 0);
	const TArray<FString> Before = AllGraphNames(Blueprint);

	struct FRefusal { const TCHAR* Why; const TCHAR* Name; };
	const FRefusal Refusals[] =
	{
		{ TEXT("empty name"),                               TEXT("") },
		{ TEXT("whitespace-only name"),                     TEXT("   ") },
		{ TEXT("name with a space"),                        TEXT("Vibe Space") },
		{ TEXT("name starting with a digit"),               TEXT("9Lives") },
		{ TEXT("name with punctuation"),                    TEXT("Vibe-Dash") },
		{ TEXT("duplicate of a function graph"),            TEXT("VibeOpenDoor") },
		{ TEXT("duplicate of a trimmed function graph"),    TEXT(" VibePadded") },
		{ TEXT("duplicate of the EventGraph"),              TEXT("EventGraph") },
		{ TEXT("shadows a parent function (SetLifeSpan)"),  TEXT("SetLifeSpan") },
		{ TEXT("shadows a parent event (ReceiveBeginPlay)"), TEXT("ReceiveBeginPlay") },
	};
	for (const FRefusal& Refusal : Refusals)
	{
		const FString Result = UBlueprintService::CreateFunctionGraph(Path, Refusal.Name, false);
		TestTrue(FString::Printf(TEXT("%s ('%s') is refused with \"\" (got '%s')"), Refusal.Why, Refusal.Name, *Result), Result.IsEmpty());
		const TArray<FString> After = AllGraphNames(Blueprint);
		TestTrue(FString::Printf(TEXT("%s ('%s') leaves the Blueprint's graphs unchanged (before: %s; after: %s)"), Refusal.Why, Refusal.Name,
			*FString::Join(Before, TEXT(", ")), *FString::Join(After, TEXT(", "))), After == Before);
	}

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	TestTrue(TEXT("the Blueprint still compiles after every refusal"), Blueprint->Status != BS_Error);
	return true;
}

// The point of #607 was that a Blueprint Interface - nothing but function graphs - could not be
// authored from Python at all, and #602's interface features (override_function on an interface,
// list_graphs' "Interface (<Name>)" kind) were reachable only from C++ fixtures. This drives the
// whole flow through the Python API an agent uses; only the two empty assets come from C++.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeInterfaceAuthoredFromPythonTest, "VibeUE.BlueprintService.InterfaceAuthoredFromPython",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeInterfaceAuthoredFromPythonTest::RunTest(const FString&)
{
	using namespace VibeCreateFunctionGraphTestUtil;

	FString InterfacePath;
	FString ActorPath;
	UBlueprint* Interface = MakeFunctionGraphFixture(*this, TEXT("BPI_PyAuthored"), UInterface::StaticClass(), BPTYPE_Interface, InterfacePath);
	UBlueprint* Actor = MakeFunctionGraphFixture(*this, TEXT("BP_PyImplementer"), AActor::StaticClass(), BPTYPE_Normal, ActorPath);
	ON_SCOPE_EXIT { ReleaseFunctionGraphFixture(Actor); ReleaseFunctionGraphFixture(Interface); };
	if (!Interface || !Actor)
	{
		return false;
	}

	// The duplicate below is refused by design and logs why.
	AddExpectedError(TEXT("CreateFunctionGraph:"), EAutomationExpectedErrorFlags::Contains, 1);

	const TSharedPtr<FJsonObject> Obj = RunFunctionGraphPython(FString::Printf(
		TEXT("import unreal\n")
		TEXT("S = unreal.BlueprintService\n")
		TEXT("ITF, ACT = '%s', '%s'\n")
		TEXT("r = []\n")
		TEXT("r.append(('create', S.create_function_graph(ITF, 'VibeGetAngle')))\n")
		TEXT("r.append(('dup', S.create_function_graph(ITF, 'VibeGetAngle')))\n")
		TEXT("r.append(('param', S.add_function_parameter(ITF, 'VibeGetAngle', 'Angle', 'float', True)))\n")
		TEXT("r.append(('void', S.create_function_graph(ITF, 'VibeOpen', False)))\n")
		TEXT("r.append(('compile_itf', S.compile_blueprint(ITF).success))\n")
		TEXT("r.append(('add_interface', S.add_interface(ACT, ITF)))\n")
		TEXT("r.append(('override_void', S.override_function(ACT, 'VibeOpen')))\n")
		TEXT("kinds = {g.graph_name: g.graph_kind for g in S.list_graphs(ACT)}\n")
		TEXT("r.append(('kind', kinds.get('VibeGetAngle', '<missing>')))\n")
		TEXT("r.append(('compile_act', S.compile_blueprint(ACT).success))\n")
		TEXT("for k, v in r:\n")
		TEXT("    print('VIBEUE_ITF %%s=[%%s]' %% (k, v))\n"),
		*InterfacePath, *ActorPath));
	if (!TestTrue(TEXT("Python run: reply parses"), Obj.IsValid()))
	{
		return false;
	}
	FString Error;
	Obj->TryGetStringField(TEXT("error_message"), Error);
	if (!TestTrue(FString::Printf(TEXT("Python run succeeds (error: '%s')"), *Error), Obj->GetBoolField(TEXT("success"))))
	{
		return false;
	}
	const FString Output = Obj->GetStringField(TEXT("output"));
	auto Expect = [this, &Output](const TCHAR* What, const FString& Line)
	{
		TestTrue(FString::Printf(TEXT("%s: output has '%s' (output: %s)"), What, *Line, *Output), Output.Contains(Line));
	};
	Expect(TEXT("create_function_graph defines a function on an interface"), TEXT("VIBEUE_ITF create=[VibeGetAngle]"));
	Expect(TEXT("create_function_graph refuses the duplicate"), TEXT("VIBEUE_ITF dup=[]"));
	Expect(TEXT("add_function_parameter gives it a return value"), TEXT("VIBEUE_ITF param=[True]"));
	Expect(TEXT("a second, void interface function"), TEXT("VIBEUE_ITF void=[VibeOpen]"));
	Expect(TEXT("the interface compiles"), TEXT("VIBEUE_ITF compile_itf=[True]"));
	Expect(TEXT("add_interface implements it on the actor"), TEXT("VIBEUE_ITF add_interface=[True]"));
	Expect(TEXT("override_function implements the void interface function"), TEXT("VIBEUE_ITF override_void=[True]"));
	Expect(TEXT("list_graphs tags the return-valued interface graph with its interface"), TEXT("VIBEUE_ITF kind=[Interface ("));
	Expect(TEXT("the implementer compiles"), TEXT("VIBEUE_ITF compile_act=[True]"));
	return true;
}

#endif // WITH_AUTOMATION_TESTS
