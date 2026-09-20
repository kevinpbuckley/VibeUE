// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "PythonAPI/UMaterialNodeService.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#if WITH_AUTOMATION_TESTS

// Regression coverage for the material connect-to-output brief (branch feat/material-output-connect):
//   1. FindExpressionById must resolve ONLY a valid session id, an owned object path, or a strict
//      in-range numeric index. Empty / malformed / stale / out-of-range / foreign ids resolve to
//      nothing (the old code fell through to Atoi and silently returned expression index 0).
//   2. ConnectExpressionToOutput / DisconnectOutput are the new material-output writers, with strict
//      property + output-name validation and a read-back before reporting success.
//
// EditorContext: the service resolves materials through UEditorAssetLibrary::LoadAsset, so the test
// registers in-memory materials under /Game paths with FAssetRegistryModule::AssetCreated (never
// saved to disk) and drops them on exit. Requires a full editor; not a commandlet/headless test.

namespace
{
	// Create an in-memory UMaterial discoverable by the service's path-based API. Caller owns cleanup.
	UMaterial* CreateScratchMaterial(const FString& PackageName, const FName AssetName)
	{
		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			return nullptr;
		}
		UMaterial* Material = NewObject<UMaterial>(
			Package, AssetName, RF_Public | RF_Standalone | RF_Transactional);
		if (Material)
		{
			FAssetRegistryModule::AssetCreated(Material);
		}
		return Material;
	}

	// Connection state of one material output property, read through the public getter.
	bool IsOutputConnected(const FString& MaterialPath, const FString& PropertyName, FString& OutExpressionId)
	{
		OutExpressionId.Empty();
		const TArray<FMaterialOutputConnectionInfo> Conns = UMaterialNodeService::GetOutputConnections(MaterialPath);
		for (const FMaterialOutputConnectionInfo& Conn : Conns)
		{
			if (Conn.PropertyName.Equals(PropertyName, ESearchCase::IgnoreCase))
			{
				OutExpressionId = Conn.ConnectedExpressionId;
				return Conn.bIsConnected;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeMaterialNodeServiceOutputTest,
	"VibeUE.MaterialNodeService.ConnectExpressionToOutput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeMaterialNodeServiceOutputTest::RunTest(const FString&)
{
	// --- API shape: the new writers are reflected and AICallable. ---
	{
		UFunction* ConnectFn = UMaterialNodeService::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(UMaterialNodeService, ConnectExpressionToOutput));
		TestNotNull(TEXT("ConnectExpressionToOutput is reflected"), ConnectFn);
		if (ConnectFn)
		{
			TestTrue(TEXT("ConnectExpressionToOutput is AICallable"), ConnectFn->HasMetaData(TEXT("AICallable")));
		}
		UFunction* DisconnectFn = UMaterialNodeService::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(UMaterialNodeService, DisconnectOutput));
		TestNotNull(TEXT("DisconnectOutput is reflected"), DisconnectFn);
		if (DisconnectFn)
		{
			TestTrue(TEXT("DisconnectOutput is AICallable"), DisconnectFn->HasMetaData(TEXT("AICallable")));
		}
	}

	const FString PathA = TEXT("/Game/__VibeUETest/M_MatNodeOutputRegression");
	const FString PathB = TEXT("/Game/__VibeUETest/M_MatNodeForeignRegression");

	UMaterial* MaterialA = CreateScratchMaterial(PathA, FName(TEXT("M_MatNodeOutputRegression")));
	UMaterial* MaterialB = CreateScratchMaterial(PathB, FName(TEXT("M_MatNodeForeignRegression")));
	if (!TestNotNull(TEXT("scratch material A created"), MaterialA) ||
		!TestNotNull(TEXT("scratch (foreign) material B created"), MaterialB))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		if (MaterialA)
		{
			FAssetRegistryModule::AssetDeleted(MaterialA);
			MaterialA->ClearFlags(RF_Standalone | RF_Public);
		}
		if (MaterialB)
		{
			FAssetRegistryModule::AssetDeleted(MaterialB);
			MaterialB->ClearFlags(RF_Standalone | RF_Public);
		}
	};

	// --- Populate A with two expressions through the service, B with one. ---
	const TArray<FMaterialExpressionInfo> CreatedA = UMaterialNodeService::BatchCreateExpressions(
		PathA, {TEXT("Constant3Vector"), TEXT("Constant")}, {-400, -400}, {0, 200});
	const TArray<FMaterialExpressionInfo> CreatedB = UMaterialNodeService::BatchCreateExpressions(
		PathB, {TEXT("Constant3Vector")}, {-400}, {0});

	if (!TestTrue(TEXT("service resolved material A and created two expressions (needs a full editor)"),
			CreatedA.Num() == 2 && !CreatedA[0].Id.IsEmpty() && !CreatedA[1].Id.IsEmpty()))
	{
		return false;
	}
	if (!TestTrue(TEXT("service resolved foreign material B and created one expression"),
			CreatedB.Num() == 1 && !CreatedB[0].Id.IsEmpty()))
	{
		return false;
	}

	// ListExpressions defines the numeric-index order; element [0] is what id "0" must resolve to.
	const TArray<FMaterialExpressionInfo> ListA = UMaterialNodeService::ListExpressions(PathA);
	if (!TestTrue(TEXT("ListExpressions returns A's two expressions"), ListA.Num() == 2))
	{
		return false;
	}
	const FMaterialExpressionInfo ExprAt0 = ListA[0];
	TestFalse(TEXT("expression [0] has a non-empty id"), ExprAt0.Id.IsEmpty());
	TestFalse(TEXT("expression [0] has a non-empty object_path"), ExprAt0.ObjectPath.IsEmpty());

	// =====================================================================================
	// (A) Identifier resolution: valid id / index / object path resolve to the SAME expression.
	// =====================================================================================
	{
		FMaterialExpressionInfo ByIndex, ById, ByPath;
		TestTrue(TEXT("numeric index \"0\" resolves"), UMaterialNodeService::GetExpressionDetails(PathA, TEXT("0"), ByIndex));
		TestTrue(TEXT("session id resolves"), UMaterialNodeService::GetExpressionDetails(PathA, ExprAt0.Id, ById));
		TestTrue(TEXT("object path resolves"), UMaterialNodeService::GetExpressionDetails(PathA, ExprAt0.ObjectPath, ByPath));

		TestEqual(TEXT("index \"0\" resolves to expression [0]"), ByIndex.Id, ExprAt0.Id);
		TestEqual(TEXT("session id resolves to expression [0]"), ById.Id, ExprAt0.Id);
		TestEqual(TEXT("object path resolves to expression [0]"), ByPath.Id, ExprAt0.Id);
	}

	// Foreign identifiers (from material B) must NOT resolve against material A.
	const FString ForeignId = CreatedB[0].Id;
	const FString ForeignPath = CreatedB[0].ObjectPath;

	// Bad identifiers that must all resolve to nothing (parallel label/id arrays).
	const TArray<FString> BadLabels = {
		TEXT("empty"), TEXT("malformed"), TEXT("stale-looking"), TEXT("out-of-range index"),
		TEXT("negative-looking index"), TEXT("foreign id"), TEXT("foreign object path")};
	const TArray<FString> BadIds = {
		TEXT(""), TEXT("R2_INVALID_ID_20260919"),
		TEXT("MaterialExpressionConstant3Vector_0x0000000000000001"), TEXT("9999"),
		TEXT("-1"), ForeignId, ForeignPath};

	for (int32 i = 0; i < BadIds.Num(); ++i)
	{
		FMaterialExpressionInfo Ignored;
		TestFalse(FString::Printf(TEXT("get_details rejects %s id"), *BadLabels[i]),
			UMaterialNodeService::GetExpressionDetails(PathA, BadIds[i], Ignored));
	}

	// =====================================================================================
	// (B) A writer given a bad id must return false AND leave the graph unchanged (this is the
	//     concrete regression: the old resolver would have mutated expression [0]).
	// =====================================================================================
	{
		const int32 CountBefore = UMaterialNodeService::ListExpressions(PathA).Num();
		FString Unused;
		const bool bBaseColorBefore = IsOutputConnected(PathA, TEXT("BaseColor"), Unused);
		TestFalse(TEXT("BaseColor starts disconnected"), bBaseColorBefore);

		for (int32 i = 0; i < BadIds.Num(); ++i)
		{
			TestFalse(FString::Printf(TEXT("connect_expression_to_output rejects %s id"), *BadLabels[i]),
				UMaterialNodeService::ConnectExpressionToOutput(PathA, BadIds[i], TEXT(""), TEXT("BaseColor")));
		}

		const int32 CountAfter = UMaterialNodeService::ListExpressions(PathA).Num();
		TestEqual(TEXT("expression count unchanged after rejected connects"), CountAfter, CountBefore);
		FString AfterId;
		TestFalse(TEXT("BaseColor still disconnected after rejected connects"),
			IsOutputConnected(PathA, TEXT("BaseColor"), AfterId));
	}

	// =====================================================================================
	// (C) Valid connect to BaseColor, verified through GetOutputConnections.
	// =====================================================================================
	const FString Const3Id = CreatedA[0].Id; // Constant3Vector
	{
		TestTrue(TEXT("connect Constant3Vector -> BaseColor (empty output name = output 0)"),
			UMaterialNodeService::ConnectExpressionToOutput(PathA, Const3Id, TEXT(""), TEXT("BaseColor")));

		FString ConnectedId;
		TestTrue(TEXT("get_output_connections reports BaseColor connected"),
			IsOutputConnected(PathA, TEXT("BaseColor"), ConnectedId));
		TestEqual(TEXT("BaseColor is wired to the Constant3Vector we connected"), ConnectedId, Const3Id);

		// Enum spelling "MP_BaseColor" must be accepted too.
		TestTrue(TEXT("connect accepts the MP_ enum spelling"),
			UMaterialNodeService::ConnectExpressionToOutput(PathA, Const3Id, TEXT(""), TEXT("mp_basecolor")));
	}

	// =====================================================================================
	// (D) Unknown property name and unknown output name return false AND preserve the existing
	//     BaseColor connection.
	// =====================================================================================
	{
		TestFalse(TEXT("unknown property name is rejected"),
			UMaterialNodeService::ConnectExpressionToOutput(PathA, Const3Id, TEXT(""), TEXT("NotARealProperty")));
		TestFalse(TEXT("unknown output name is rejected"),
			UMaterialNodeService::ConnectExpressionToOutput(PathA, Const3Id, TEXT("NoSuchOutput"), TEXT("Metallic")));

		FString StillId;
		TestTrue(TEXT("BaseColor connection preserved after rejected connects"),
			IsOutputConnected(PathA, TEXT("BaseColor"), StillId));
		TestEqual(TEXT("BaseColor still wired to the Constant3Vector"), StillId, Const3Id);
		FString MetallicId;
		TestFalse(TEXT("Metallic was NOT connected by the rejected bad-output call"),
			IsOutputConnected(PathA, TEXT("Metallic"), MetallicId));
	}

	// =====================================================================================
	// (E) Disconnect clears ONLY the requested property.
	// =====================================================================================
	{
		// Wire a second output so we can prove disconnect is surgical.
		const FString ConstId = CreatedA[1].Id; // Constant (scalar)
		TestTrue(TEXT("connect Constant -> Metallic"),
			UMaterialNodeService::ConnectExpressionToOutput(PathA, ConstId, TEXT(""), TEXT("Metallic")));

		FString Tmp;
		TestTrue(TEXT("Metallic connected before disconnect"), IsOutputConnected(PathA, TEXT("Metallic"), Tmp));
		TestTrue(TEXT("BaseColor connected before disconnect"), IsOutputConnected(PathA, TEXT("BaseColor"), Tmp));

		TestTrue(TEXT("disconnect_output clears BaseColor"),
			UMaterialNodeService::DisconnectOutput(PathA, TEXT("BaseColor")));

		FString AfterBase, AfterMetal;
		TestFalse(TEXT("BaseColor cleared after disconnect"), IsOutputConnected(PathA, TEXT("BaseColor"), AfterBase));
		TestTrue(TEXT("Metallic preserved after disconnecting BaseColor"),
			IsOutputConnected(PathA, TEXT("Metallic"), AfterMetal));

		// Idempotent: disconnecting an already-empty output is success and does not disturb Metallic.
		TestTrue(TEXT("disconnect_output on an already-empty property is idempotent success"),
			UMaterialNodeService::DisconnectOutput(PathA, TEXT("BaseColor")));
		TestTrue(TEXT("Metallic still connected after idempotent BaseColor disconnect"),
			IsOutputConnected(PathA, TEXT("Metallic"), AfterMetal));

		// Unknown property name is rejected.
		TestFalse(TEXT("disconnect_output rejects an unknown property name"),
			UMaterialNodeService::DisconnectOutput(PathA, TEXT("NotARealProperty")));
	}

	return true;
}

// Issue #611: every material output the WRITERS accept must also be visible to the READER.
// Before the fix, StringToMaterialProperty accepted 19 properties while GetOutputConnections
// reported a hand-maintained list of 16 — so ClearCoat, ClearCoatRoughness and Displacement could
// be connected and internally verified, yet never showed up when reading the graph back. Both are
// now driven from GetMaterialOutputProperties(), and this test pins that they cannot drift apart.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeMaterialOutputPropertyParityTest, "VibeUE.MaterialNodeService.OutputPropertyParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeMaterialOutputPropertyParityTest::RunTest(const FString&)
{
	const TArray<TPair<FString, EMaterialProperty>>& Properties = UMaterialNodeService::GetMaterialOutputProperties();
	TestTrue(TEXT("the shared property table is non-empty"), Properties.Num() > 0);

	// 1. Every name in the table resolves through the writer-side mapper, to the same enum value.
	for (const TPair<FString, EMaterialProperty>& Pair : Properties)
	{
		EMaterialProperty Resolved = MP_MAX;
		if (TestTrue(FString::Printf(TEXT("writers accept '%s'"), *Pair.Key),
			UMaterialNodeService::StringToMaterialProperty(Pair.Key, Resolved)))
		{
			TestEqual(FString::Printf(TEXT("'%s' maps to the table's enum value"), *Pair.Key),
				static_cast<int32>(Resolved), static_cast<int32>(Pair.Value));
		}
	}

	// 2. The reader reports one entry per table property, in the same order — so nothing the writers
	//    accept is invisible. A scratch material is enough; connectedness is irrelevant here.
	// Same in-memory fixture the other tests in this file use: the service resolves materials via
	// UEditorAssetLibrary::LoadAsset, which will not see an RF_Transient object.
	const FString Path = TEXT("/Game/__VibeUETest/M_OutputParity");
	UMaterial* Material = CreateScratchMaterial(Path, TEXT("M_OutputParity"));
	if (!TestNotNull(TEXT("created scratch material"), Material))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		FAssetRegistryModule::AssetDeleted(Material);
		Material->ClearFlags(RF_Standalone | RF_Public);
	};

	const TArray<FMaterialOutputConnectionInfo> Reported = UMaterialNodeService::GetOutputConnections(Path);
	TestEqual(TEXT("reader reports exactly one entry per writable property"), Reported.Num(), Properties.Num());

	TSet<FString> ReportedNames;
	for (const FMaterialOutputConnectionInfo& Info : Reported)
	{
		ReportedNames.Add(Info.PropertyName);
	}
	for (const TPair<FString, EMaterialProperty>& Pair : Properties)
	{
		TestTrue(FString::Printf(TEXT("get_output_connections reports '%s'"), *Pair.Key),
			ReportedNames.Contains(Pair.Key));
	}

	// 3. Name the three that regressed, explicitly, so the original bug can never come back quietly.
	for (const TCHAR* Name : { TEXT("ClearCoat"), TEXT("ClearCoatRoughness"), TEXT("Displacement") })
	{
		TestTrue(FString::Printf(TEXT("previously-invisible property '%s' is reported"), Name),
			ReportedNames.Contains(FString(Name)));
	}

	return true;
}

#endif // WITH_AUTOMATION_TESTS
