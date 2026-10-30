// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "EditorAssetLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "PythonAPI/UMaterialService.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeMaterialInfoResultTest, "VibeUE.Materials.GetMaterialInfoResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeMaterialInfoResultTest::RunTest(const FString&)
{
	UFunction* Function = UMaterialService::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UMaterialService, GetMaterialInfoResult));
	TestNotNull(TEXT("Result operation is reflected"), Function);
	if (Function)
	{
		TestTrue(TEXT("Result operation is AICallable"), Function->HasMetaData(TEXT("AICallable")));
		const FStructProperty* ReturnProperty = CastField<FStructProperty>(Function->GetReturnProperty());
		TestTrue(TEXT("MCP receives the result struct as the direct return"),
			ReturnProperty && ReturnProperty->Struct == FMaterialInfoResult::StaticStruct());
		TestEqual(TEXT("Only the path and return value are parameters"), Function->NumParms, uint8(2));
	}

	const FString MaterialPath = TEXT("/Engine/EngineMaterials/DefaultMaterial");
	FMaterialDetailedInfo LegacyInfo;
	TestTrue(TEXT("Legacy C++ signature still works"), UMaterialService::GetMaterialInfo(MaterialPath, LegacyInfo));
	const FMaterialInfoResult Result = UMaterialService::GetMaterialInfoResult(MaterialPath);
	TestTrue(TEXT("Valid material succeeds"), Result.bSuccess);
	TestTrue(TEXT("Success has no error"), Result.ErrorMessage.IsEmpty());
	TestEqual(TEXT("Material identity is populated"), Result.Info.MaterialName, FString(TEXT("DefaultMaterial")));
	TestEqual(TEXT("Requested path is retained"), Result.Info.MaterialPath, MaterialPath);
	TestTrue(TEXT("Every detail matches the legacy read"),
		FMaterialDetailedInfo::StaticStruct()->CompareScriptStruct(&Result.Info, &LegacyInfo, 0));

	const FMaterialInfoResult OpaqueInstance = UMaterialService::GetMaterialInfoResult(
		TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque"));
	TestTrue(TEXT("Opaque instance is readable"), OpaqueInstance.bSuccess);
	TestTrue(TEXT("Opaque instance is identified as an instance"), OpaqueInstance.Info.bIsMaterialInstance);
	TestEqual(TEXT("Instance blend mode includes overrides"), OpaqueInstance.Info.BlendMode, FString(TEXT("BLEND_Opaque")));
	TestEqual(TEXT("Instance shading model"), OpaqueInstance.Info.ShadingModel, FString(TEXT("MSM_Unlit")));
	TestTrue(TEXT("Instance two-sided override is effective"), OpaqueInstance.Info.bTwoSided);
	const FMaterialParameterInfo_Custom* InheritedOpacity = OpaqueInstance.Info.Parameters.FindByPredicate(
		[](const FMaterialParameterInfo_Custom& Parameter) { return Parameter.ParameterName == TEXT("OpacityFromTexture"); });
	if (TestNotNull(TEXT("Inherited opacity parameter is present"), InheritedOpacity))
	{
		TestFalse(TEXT("Inherited parameter is not a local override"), InheritedOpacity->bIsOverridden);
		TestEqual(TEXT("Ungrouped parameter has no group"), InheritedOpacity->Group, FString());
	}
	const FMaterialParameterInfo_Custom* InheritedTexture = OpaqueInstance.Info.Parameters.FindByPredicate(
		[](const FMaterialParameterInfo_Custom& Parameter) { return Parameter.ParameterName == TEXT("SlateUI"); });
	if (TestNotNull(TEXT("Inherited texture parameter is present"), InheritedTexture))
	{
		TestEqual(TEXT("Inherited texture has matching value and default"), InheritedTexture->CurrentValue, InheritedTexture->DefaultValue);
		TestFalse(TEXT("Inherited texture is not a local override"), InheritedTexture->bIsOverridden);
	}

	const FMaterialInfoResult PostProcessInstance = UMaterialService::GetMaterialInfoResult(
		TEXT("/Engine/EngineMaterials/PPVolumeMaterialInstance"));
	TestTrue(TEXT("Post-process instance is readable"), PostProcessInstance.bSuccess);
	const FMaterialParameterInfo_Custom* Desaturation = PostProcessInstance.Info.Parameters.FindByPredicate(
		[](const FMaterialParameterInfo_Custom& Parameter) { return Parameter.ParameterName == TEXT("Desaturation"); });
	if (TestNotNull(TEXT("Desaturation parameter is present"), Desaturation))
	{
		TestEqual(TEXT("Equal-valued override has matching value and default"), Desaturation->CurrentValue, Desaturation->DefaultValue);
		TestTrue(TEXT("Equal-valued local override is still reported"), Desaturation->bIsOverridden);
	}
	const FMaterialParameterInfo_Custom* Highlights = PostProcessInstance.Info.Parameters.FindByPredicate(
		[](const FMaterialParameterInfo_Custom& Parameter) { return Parameter.ParameterName == TEXT("HighLights"); });
	if (TestNotNull(TEXT("HighLights parameter is present"), Highlights))
	{
		TestEqual(TEXT("Instance parameter value"), Highlights->CurrentValue, FString(TEXT("(R=1.000,G=1.000,B=1.000,A=0.000)")));
		TestEqual(TEXT("Parent parameter default"), Highlights->DefaultValue, FString(TEXT("(R=1.000,G=1.000,B=1.000,A=1.000)")));
		TestTrue(TEXT("Instance override is reported"), Highlights->bIsOverridden);
	}

	const FMaterialInfoResult NestedInstance = UMaterialService::GetMaterialInfoResult(
		TEXT("/Engine/VREditor/LaserPointer/TranslucentLaserPointerMaterialInst"));
	TestTrue(TEXT("Nested instance is readable"), NestedInstance.bSuccess);
	const FMaterialParameterInfo_Custom* Amplify = NestedInstance.Info.Parameters.FindByPredicate(
		[](const FMaterialParameterInfo_Custom& Parameter) { return Parameter.ParameterName == TEXT("Amplify"); });
	if (TestNotNull(TEXT("Nested scalar parameter is present"), Amplify))
	{
		TestEqual(TEXT("Nested local value"), Amplify->CurrentValue, FString(TEXT("16.000")));
		TestEqual(TEXT("Nested default is the parent instance override"), Amplify->DefaultValue, FString(TEXT("10.000")));
		TestTrue(TEXT("Nested local override is reported"), Amplify->bIsOverridden);
	}
	const FMaterialParameterInfo_Custom* LaserColor = NestedInstance.Info.Parameters.FindByPredicate(
		[](const FMaterialParameterInfo_Custom& Parameter) { return Parameter.ParameterName == TEXT("LaserColor"); });
	if (TestNotNull(TEXT("Inherited parent override is present"), LaserColor))
	{
		TestEqual(TEXT("Inherited parent override is the default"), LaserColor->DefaultValue, FString(TEXT("(R=1.000,G=0.000,B=0.009,A=0.000)")));
		TestEqual(TEXT("Inherited parent override is the current value"), LaserColor->CurrentValue, LaserColor->DefaultValue);
		TestFalse(TEXT("Parent override is not a local override"), LaserColor->bIsOverridden);
	}

	const FMaterialInfoResult GroupedInstance = UMaterialService::GetMaterialInfoResult(
		TEXT("/Engine/VREditor/Devices/Vive/VivePreControllerMaterial_Inst"));
	TestTrue(TEXT("Grouped instance is readable"), GroupedInstance.bSuccess);
	const FMaterialParameterInfo_Custom* Button = GroupedInstance.Info.Parameters.FindByPredicate(
		[](const FMaterialParameterInfo_Custom& Parameter) { return Parameter.ParameterName == TEXT("B1"); });
	if (TestNotNull(TEXT("Grouped parameter is present"), Button))
	{
		TestEqual(TEXT("Parameter group is reported"), Button->Group, FString(TEXT("Buttons")));
	}

	const FMaterialInfoResult TextureInstance = UMaterialService::GetMaterialInfoResult(
		TEXT("/Engine/EditorLandscapeResources/SelectBrushMaterial_SelectedRegion"));
	TestTrue(TEXT("Texture instance is readable"), TextureInstance.bSuccess);
	const FMaterialParameterInfo_Custom* SelectedData = TextureInstance.Info.Parameters.FindByPredicate(
		[](const FMaterialParameterInfo_Custom& Parameter) { return Parameter.ParameterName == TEXT("SelectedData"); });
	if (TestNotNull(TEXT("Texture parameter is present"), SelectedData))
	{
		TestEqual(TEXT("Texture override value"), SelectedData->CurrentValue, FString(TEXT("/Engine/EditorLandscapeResources/DefaultAlphaTexture.DefaultAlphaTexture")));
		TestEqual(TEXT("Texture inherited default"), SelectedData->DefaultValue, FString(TEXT("/Engine/EditorLandscapeResources/WhiteSquareTexture.WhiteSquareTexture")));
		TestTrue(TEXT("Texture override is reported"), SelectedData->bIsOverridden);
	}

	// LoadAsset intentionally logs errors for the empty and missing paths, not the wrong asset type.
	AddExpectedError(TEXT("LoadAsset failed:"), EAutomationExpectedErrorFlags::Contains, 2);
	const FString InvalidPaths[] = {
		TEXT(""),
		TEXT("/Engine/EngineMaterials/VibeUE_MissingMaterialInfo"),
		TEXT("/Engine/EngineResources/DefaultTexture")
	};
	const FMaterialDetailedInfo EmptyInfo;
	for (const FString& InvalidPath : InvalidPaths)
	{
		const FMaterialInfoResult Failure = UMaterialService::GetMaterialInfoResult(InvalidPath);
		TestFalse(FString::Printf(TEXT("Rejects '%s'"), *InvalidPath), Failure.bSuccess);
		TestFalse(TEXT("Failure has an explicit error"), Failure.ErrorMessage.IsEmpty());
		TestTrue(TEXT("Error includes the requested path"), Failure.ErrorMessage.Contains(InvalidPath));
		TestTrue(TEXT("Failure cannot expose stale successful details"),
			FMaterialDetailedInfo::StaticStruct()->CompareScriptStruct(&Failure.Info, &EmptyInfo, 0));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMaterialInfoShadingOverrideTest, "VibeUE.Materials.GetMaterialInfoShadingOverride",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMaterialInfoShadingOverrideTest::RunTest(const FString&)
{
	UMaterial* Parent = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(TEXT("/Engine/EngineMaterials/DefaultMaterial")));
	if (!TestNotNull(TEXT("Fixture parent loads"), Parent) ||
		!TestTrue(TEXT("Fixture parent is Default Lit"), Parent->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit)) ||
		!TestEqual(TEXT("Fixture parent is opaque"), Parent->GetBlendMode(), BLEND_Opaque) ||
		!TestFalse(TEXT("Fixture parent is one-sided"), Parent->IsTwoSided()))
	{
		return false;
	}

	const FString Name = FString::Printf(TEXT("VibeUE_ShadingOverride_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	TStrongObjectPtr<UPackage> Package(CreatePackage(*(TEXT("/Game/") + Name)));
	TStrongObjectPtr<UMaterialInstanceConstant> Instance(NewObject<UMaterialInstanceConstant>(Package.Get(), *Name, RF_Public));
	Instance->SetParentEditorOnly(Parent, false);
	FMaterialInstanceBasePropertyOverrides Overrides = Instance->BasePropertyOverrides;
	Overrides.bOverride_ShadingModel = true;
	Overrides.ShadingModel = MSM_Unlit;
	Overrides.bOverride_BlendMode = true;
	Overrides.BlendMode = BLEND_Translucent;
	Overrides.bOverride_TwoSided = true;
	Overrides.TwoSided = true;
	{
		FMaterialInstanceParameterUpdateContext Update(Instance.Get());
		Update.SetBasePropertyOverrides(Overrides);
	}

	TestTrue(TEXT("Fixture is a loadable asset"), Instance->IsAsset());
	TestTrue(TEXT("Fixture override is effective"), Instance->GetShadingModels().HasOnlyShadingModel(MSM_Unlit));
	TestEqual(TEXT("Fixture blend override is effective"), Instance->GetBlendMode(), BLEND_Translucent);
	TestTrue(TEXT("Fixture two-sided override is effective"), Instance->IsTwoSided());
	const FString ObjectPath = Instance->GetPathName();
	if (TestTrue(TEXT("Asset loader resolves the unsaved fixture"), UEditorAssetLibrary::LoadAsset(ObjectPath) == Instance.Get()))
	{
		const FMaterialInfoResult Result = UMaterialService::GetMaterialInfoResult(ObjectPath);
		TestTrue(TEXT("Override instance succeeds"), Result.bSuccess);
		TestTrue(TEXT("Result identifies an instance"), Result.Info.bIsMaterialInstance);
		TestEqual(TEXT("Instance shading override is reported"), Result.Info.ShadingModel, FString(TEXT("MSM_Unlit")));
		TestEqual(TEXT("Instance blend override is reported"), Result.Info.BlendMode, FString(TEXT("BLEND_Translucent")));
		TestTrue(TEXT("Instance two-sided override is reported"), Result.Info.bTwoSided);
	}
	TestFalse(TEXT("Unsaved fixture does not dirty its package"), Package->IsDirty());
	if (Package->IsDirty())
	{
		Package->SetDirtyFlag(false);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMaterialInfoRegistryTest, "VibeUE.Materials.GetMaterialInfoResultRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMaterialInfoRegistryTest::RunTest(const FString&)
{
	UToolCallAsyncResultString* Call = UToolsetRegistry::ExecuteTool(TEXT("VibeUE.MaterialService"),
		TEXT("GetMaterialInfoResult"), TEXT("{\"materialPath\":\"/Engine/EngineMaterials/PPVolumeMaterialInstance\"}"));
	if (!TestNotNull(TEXT("Registry returned a result"), Call) ||
		!TestTrue(TEXT("Registry call completed synchronously"), Call->bIsComplete) ||
		!TestTrue(TEXT("Registry call has no error"), Call->Error.IsEmpty()))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Json;
	if (!TestTrue(TEXT("Registry returned JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Call->Value), Json)) ||
		!TestTrue(TEXT("JSON object is valid"), Json.IsValid()))
	{
		return false;
	}
	const TSharedPtr<FJsonObject>* Result = nullptr;
	const TSharedPtr<FJsonObject>* Info = nullptr;
	if (!TestTrue(TEXT("JSON contains returnValue"), Json->TryGetObjectField(TEXT("returnValue"), Result)) ||
		!TestTrue(TEXT("Tool succeeded"), (*Result)->GetBoolField(TEXT("bSuccess"))) ||
		!TestTrue(TEXT("JSON contains info"), (*Result)->TryGetObjectField(TEXT("info"), Info)))
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* JsonParameters = nullptr;
	if (!TestTrue(TEXT("JSON contains parameters"), (*Info)->TryGetArrayField(TEXT("parameters"), JsonParameters)))
	{
		return false;
	}
	const TSharedPtr<FJsonValue>* Highlight = JsonParameters->FindByPredicate([](const TSharedPtr<FJsonValue>& Value)
	{
		return Value.IsValid() && Value->Type == EJson::Object && Value->AsObject()->GetStringField(TEXT("parameterName")) == TEXT("HighLights");
	});
	if (TestNotNull(TEXT("JSON contains HighLights"), Highlight))
	{
		const TSharedPtr<FJsonObject> Parameter = (*Highlight)->AsObject();
		TestEqual(TEXT("JSON contains inherited default"), Parameter->GetStringField(TEXT("defaultValue")), FString(TEXT("(R=1.000,G=1.000,B=1.000,A=1.000)")));
		TestTrue(TEXT("JSON contains local override"), Parameter->GetBoolField(TEXT("bIsOverridden")));
	}
	return true;
}

#endif // WITH_AUTOMATION_TESTS
