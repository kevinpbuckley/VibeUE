// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/PackageName.h"
#include "PythonAPI/UWidgetService.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "WidgetBlueprint.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_VariableGet.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#if WITH_AUTOMATION_TESTS

// remove_component took a widget out of the tree but left its WidgetVariableNameToGuidMap entry, which only the create
// path writes; the next compile hit WidgetBlueprintCompiler's "was deleted but still has a GUID" ensure.
// These tests pin what a removal must leave: no GUID entry for the widget or any child, no object of its name left in
// the widget tree's outer (a later add of that name as another class is fatal in NewObject), a clean compile, and a
// refusal, with nothing removed, when a graph node uses the widget or a child.
//
// The Blueprints are in-memory /Game assets registered with the asset registry, so the service resolves them by path;
// they are never saved. Needs a full editor (EditorContext), like the other service tests.

namespace VibeWidgetRemoveTest
{
	static UWidgetBlueprint* MakeRegisteredWidgetBlueprint(const FString& PackageName)
	{
		const FString AssetName = FPackageName::GetShortName(PackageName);
		UPackage* Package = CreatePackage(*PackageName);
		// CreateBlueprint check()s that the name is free; refuse here instead of asserting.
		if (!Package || FindObject<UObject>(Package, *AssetName))
		{
			return nullptr;
		}
		UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
			UUserWidget::StaticClass(), Package, FName(*AssetName), BPTYPE_Normal,
			UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
		if (WidgetBP)
		{
			FAssetRegistryModule::AssetCreated(WidgetBP);
		}
		return WidgetBP;
	}

	static void Unregister(UWidgetBlueprint* WidgetBP)
	{
		if (WidgetBP)
		{
			FAssetRegistryModule::AssetDeleted(WidgetBP);
			WidgetBP->ClearFlags(RF_Standalone | RF_Public);
		}
	}

	// An object of this name still in the widget tree's outer is where ConstructWidget would put a new widget of the
	// same name; NewObject is fatal there when the classes differ ("Cannot replace existing object of a different class").
	static bool NameHeldInTree(const UWidgetBlueprint* WidgetBP, const TCHAR* Name)
	{
		return FindObjectFast<UObject>(WidgetBP->WidgetTree, FName(Name)) != nullptr;
	}

	struct FAdd
	{
		const TCHAR* Type;
		const TCHAR* Name;
		const TCHAR* Parent;
	};

	static bool AddAll(FAutomationTestBase& Test, const FString& Path, TConstArrayView<FAdd> Adds)
	{
		for (const FAdd& Add : Adds)
		{
			const FWidgetAddComponentResult R = UWidgetService::AddComponent(Path, Add.Type, Add.Name, Add.Parent);
			if (!Test.TestTrue(FString::Printf(TEXT("added %s as %s (%s)"), Add.Name, Add.Type, *R.ErrorMessage), R.bSuccess))
			{
				return false;
			}
		}
		return true;
	}

	static bool CompiledClean(const UWidgetBlueprint* WidgetBP)
	{
		return WidgetBP->Status == BS_UpToDate || WidgetBP->Status == BS_UpToDateWithWarnings;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeWidgetRemoveDropsGuidsTest, "VibeUE.WidgetService.RemoveComponentDropsGuids",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeWidgetRemoveDropsGuidsTest::RunTest(const FString&)
{
	using namespace VibeWidgetRemoveTest;
	const FString Path = TEXT("/Game/__VibeUETest/WBP_RemoveDropsGuids");
	UWidgetBlueprint* WidgetBP = MakeRegisteredWidgetBlueprint(Path);
	ON_SCOPE_EXIT { Unregister(WidgetBP); };
	if (!TestNotNull(TEXT("made the in-memory widget Blueprint"), WidgetBP))
	{
		return false;
	}

	const FAdd Adds[] = {
		{ TEXT("CanvasPanel"), TEXT("Root"), TEXT("") },
		{ TEXT("Button"), TEXT("Btn_Solo"), TEXT("Root") },
		{ TEXT("VerticalBox"), TEXT("Box_A"), TEXT("Root") },
		{ TEXT("Button"), TEXT("Btn_A"), TEXT("Box_A") },
		{ TEXT("VerticalBox"), TEXT("Box_B"), TEXT("Root") },
		{ TEXT("TextBlock"), TEXT("Txt_B"), TEXT("Box_B") },
	};
	if (!AddAll(*this, Path, Adds))
	{
		return false;
	}
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	if (!TestTrue(TEXT("the starting Blueprint compiles"), CompiledClean(WidgetBP)))
	{
		return false;
	}

	const TMap<FName, FGuid>& Guids = WidgetBP->WidgetVariableNameToGuidMap;

	// A lone widget (crash C12's case).
	FWidgetRemoveComponentResult R = UWidgetService::RemoveComponent(Path, TEXT("Btn_Solo"));
	TestTrue(FString::Printf(TEXT("removed Btn_Solo (%s)"), *R.ErrorMessage), R.bSuccess);
	TestFalse(TEXT("Btn_Solo's GUID entry is gone"), Guids.Contains(TEXT("Btn_Solo")));
	TestFalse(TEXT("no object named Btn_Solo is left in the widget tree's outer"), NameHeldInTree(WidgetBP, TEXT("Btn_Solo")));

	// A panel removed with its children.
	R = UWidgetService::RemoveComponent(Path, TEXT("Box_A"), /*bRemoveChildren=*/ true);
	TestTrue(FString::Printf(TEXT("removed Box_A (%s)"), *R.ErrorMessage), R.bSuccess);
	TestFalse(TEXT("Box_A's GUID entry is gone"), Guids.Contains(TEXT("Box_A")));
	TestFalse(TEXT("its child Btn_A's GUID entry is gone"), Guids.Contains(TEXT("Btn_A")));
	TestFalse(TEXT("no object named Box_A is left in the widget tree's outer"), NameHeldInTree(WidgetBP, TEXT("Box_A")));
	TestFalse(TEXT("no object named Btn_A is left in the widget tree's outer"), NameHeldInTree(WidgetBP, TEXT("Btn_A")));

	// A panel removed without remove_children: its children leave the tree with it, so their entries go too.
	R = UWidgetService::RemoveComponent(Path, TEXT("Box_B"), /*bRemoveChildren=*/ false);
	TestTrue(FString::Printf(TEXT("removed Box_B (%s)"), *R.ErrorMessage), R.bSuccess);
	TestFalse(TEXT("Box_B's GUID entry is gone"), Guids.Contains(TEXT("Box_B")));
	TestFalse(TEXT("its child Txt_B's GUID entry is gone"), Guids.Contains(TEXT("Txt_B")));
	TestTrue(TEXT("Txt_B is no longer in the tree"), WidgetBP->WidgetTree->FindWidget(TEXT("Txt_B")) == nullptr);

	// The same name again as another class, attempted only once nothing holds the name: with the old object still
	// in the outer, NewObject would end the editor instead of failing this test.
	if (!NameHeldInTree(WidgetBP, TEXT("Btn_Solo")))
	{
		const FWidgetAddComponentResult Again = UWidgetService::AddComponent(Path, TEXT("Image"), TEXT("Btn_Solo"), TEXT("Root"));
		TestTrue(FString::Printf(TEXT("re-added Btn_Solo as an Image (%s)"), *Again.ErrorMessage), Again.bSuccess);
	}

	// The compile that raised that ensure, run only once no stale entry is left (with one, it would raise
	// that ensure again, which this test has already reported as failed checks above).
	bool bStale = false;
	for (const TPair<FName, FGuid>& Entry : Guids)
	{
		if (!WidgetBP->WidgetTree->FindWidget(Entry.Key))
		{
			bStale = true;
			AddError(FString::Printf(TEXT("GUID entry '%s' names no widget in the tree"), *Entry.Key.ToString()));
		}
	}
	if (!bStale)
	{
		FKismetEditorUtilities::CompileBlueprint(WidgetBP);
		TestTrue(TEXT("the Blueprint compiles after the removals"), CompiledClean(WidgetBP));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeWidgetRemoveRefusesGraphUseTest, "VibeUE.WidgetService.RemoveComponentRefusesGraphUse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeWidgetRemoveRefusesGraphUseTest::RunTest(const FString&)
{
	using namespace VibeWidgetRemoveTest;
	const FString Path = TEXT("/Game/__VibeUETest/WBP_RemoveRefusesGraphUse");
	UWidgetBlueprint* WidgetBP = MakeRegisteredWidgetBlueprint(Path);
	ON_SCOPE_EXIT { Unregister(WidgetBP); };
	if (!TestNotNull(TEXT("made the in-memory widget Blueprint"), WidgetBP))
	{
		return false;
	}

	const FAdd Adds[] = {
		{ TEXT("CanvasPanel"), TEXT("Root"), TEXT("") },
		{ TEXT("Button"), TEXT("Btn_Used"), TEXT("Root") },
		{ TEXT("VerticalBox"), TEXT("Box_Holds"), TEXT("Root") },
		{ TEXT("Button"), TEXT("Btn_UsedChild"), TEXT("Box_Holds") },
	};
	if (!AddAll(*this, Path, Adds))
	{
		return false;
	}
	// The skeleton class must hold the widget variables before a graph node can reference them.
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);

	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(WidgetBP);
	if (!TestNotNull(TEXT("the widget Blueprint has an event graph"), EventGraph))
	{
		return false;
	}
	auto AddGet = [&](const TCHAR* Name)
	{
		FGraphNodeCreator<UK2Node_VariableGet> Creator(*EventGraph);
		UK2Node_VariableGet* Node = Creator.CreateNode(false);
		Node->VariableReference.SetSelfMember(FName(Name), WidgetBP->WidgetVariableNameToGuidMap.FindRef(FName(Name)));
		Creator.Finalize();
		return Node;
	};
	UEdGraphNode* GetUsed = AddGet(TEXT("Btn_Used"));
	UEdGraphNode* GetChild = AddGet(TEXT("Btn_UsedChild"));
	if (!TestTrue(TEXT("the engine sees Btn_Used as used in the graph (test setup)"), FBlueprintEditorUtils::IsVariableUsed(WidgetBP, TEXT("Btn_Used")))
		|| !TestTrue(TEXT("the engine sees Btn_UsedChild as used in the graph (test setup)"), FBlueprintEditorUtils::IsVariableUsed(WidgetBP, TEXT("Btn_UsedChild"))))
	{
		return false;
	}

	// The widget a node reads.
	FWidgetRemoveComponentResult R = UWidgetService::RemoveComponent(Path, TEXT("Btn_Used"));
	TestFalse(TEXT("remove_component refuses a widget a graph node uses"), R.bSuccess);
	TestTrue(FString::Printf(TEXT("the refusal names the widget (%s)"), *R.ErrorMessage), R.ErrorMessage.Contains(TEXT("Btn_Used")));
	TestTrue(FString::Printf(TEXT("the refusal names the graph (%s)"), *R.ErrorMessage), R.ErrorMessage.Contains(EventGraph->GetName()));
	TestTrue(TEXT("Btn_Used is still in the tree"), WidgetBP->WidgetTree->FindWidget(TEXT("Btn_Used")) != nullptr);
	TestTrue(TEXT("Btn_Used keeps its GUID entry"), WidgetBP->WidgetVariableNameToGuidMap.Contains(TEXT("Btn_Used")));
	TestTrue(TEXT("the node that reads it is still in the graph"), EventGraph->Nodes.Contains(GetUsed));

	// A panel whose child a node reads.
	R = UWidgetService::RemoveComponent(Path, TEXT("Box_Holds"), /*bRemoveChildren=*/ true);
	TestFalse(TEXT("remove_component refuses a panel whose child a graph node uses"), R.bSuccess);
	TestTrue(FString::Printf(TEXT("the refusal names the child (%s)"), *R.ErrorMessage), R.ErrorMessage.Contains(TEXT("Btn_UsedChild")));
	TestTrue(TEXT("Box_Holds is still in the tree"), WidgetBP->WidgetTree->FindWidget(TEXT("Box_Holds")) != nullptr);
	TestTrue(TEXT("Btn_UsedChild is still in the tree"), WidgetBP->WidgetTree->FindWidget(TEXT("Btn_UsedChild")) != nullptr);
	TestTrue(TEXT("the node that reads the child is still in the graph"), EventGraph->Nodes.Contains(GetChild));
	return true;
}

#endif // WITH_AUTOMATION_TESTS
