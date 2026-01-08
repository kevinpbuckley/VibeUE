// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "PythonAPI/UMaterialService.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "EditorAssetLibrary.h"

bool UMaterialService::GetMaterialInfo(const FString& MaterialPath, FMaterialDetailedInfo& OutInfo)
{
	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(MaterialPath);
	if (!LoadedObject)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialService::GetMaterialInfo: Failed to load material: %s"), *MaterialPath);
		return false;
	}

	UMaterial* Material = Cast<UMaterial>(LoadedObject);
	UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(LoadedObject);

	if (!Material && !MaterialInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialService::GetMaterialInfo: Object is not a material: %s"), *MaterialPath);
		return false;
	}

	if (Material)
	{
		OutInfo.MaterialName = Material->GetName();
		OutInfo.MaterialPath = MaterialPath;
		OutInfo.bIsMaterialInstance = false;
		OutInfo.ParentMaterial = TEXT("BaseMaterial");
	}
	else if (MaterialInstance)
	{
		OutInfo.MaterialName = MaterialInstance->GetName();
		OutInfo.MaterialPath = MaterialPath;
		OutInfo.bIsMaterialInstance = true;

		if (UMaterialInterface* Parent = MaterialInstance->Parent)
		{
			OutInfo.ParentMaterial = Parent->GetName();
		}
	}

	// Get parameters
	OutInfo.Parameters = ListParameters(MaterialPath);

	return true;
}

TArray<FMaterialParameterInfo_Custom> UMaterialService::ListParameters(const FString& MaterialPath)
{
	TArray<FMaterialParameterInfo_Custom> Parameters;

	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(MaterialPath);
	if (!LoadedObject)
	{
		return Parameters;
	}

	UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(LoadedObject);
	if (!MaterialInstance)
	{
		return Parameters;
	}

	// Get scalar parameters
	TArray<FMaterialParameterInfo> ScalarParams;
	TArray<FGuid> ScalarGuids;
	MaterialInstance->GetAllScalarParameterInfo(ScalarParams, ScalarGuids);

	for (const FMaterialParameterInfo& Param : ScalarParams)
	{
		FMaterialParameterInfo_Custom CustomInfo;
		CustomInfo.ParameterName = Param.Name.ToString();
		CustomInfo.ParameterType = TEXT("Scalar");
		CustomInfo.Group = TEXT(""); // Association is enum, no direct ToString in UE 5.7

		float Value;
		FHashedMaterialParameterInfo HashedParam(Param);
		if (MaterialInstance->GetScalarParameterValue(HashedParam, Value))
		{
			CustomInfo.DefaultValue = FString::Printf(TEXT("%.3f"), Value);
		}

		Parameters.Add(CustomInfo);
	}

	// Get vector parameters
	TArray<FMaterialParameterInfo> VectorParams;
	TArray<FGuid> VectorGuids;
	MaterialInstance->GetAllVectorParameterInfo(VectorParams, VectorGuids);

	for (const FMaterialParameterInfo& Param : VectorParams)
	{
		FMaterialParameterInfo_Custom CustomInfo;
		CustomInfo.ParameterName = Param.Name.ToString();
		CustomInfo.ParameterType = TEXT("Vector");
		CustomInfo.Group = TEXT(""); // Association is enum, no direct ToString in UE 5.7

		FLinearColor Value;
		FHashedMaterialParameterInfo HashedParam(Param);
		if (MaterialInstance->GetVectorParameterValue(HashedParam, Value))
		{
			CustomInfo.DefaultValue = Value.ToString();
		}

		Parameters.Add(CustomInfo);
	}

	// Get texture parameters
	TArray<FMaterialParameterInfo> TextureParams;
	TArray<FGuid> TextureGuids;
	MaterialInstance->GetAllTextureParameterInfo(TextureParams, TextureGuids);

	for (const FMaterialParameterInfo& Param : TextureParams)
	{
		FMaterialParameterInfo_Custom CustomInfo;
		CustomInfo.ParameterName = Param.Name.ToString();
		CustomInfo.ParameterType = TEXT("Texture");
		CustomInfo.Group = TEXT(""); // Association is enum, no direct ToString in UE 5.7

		UTexture* Texture;
		FHashedMaterialParameterInfo HashedParam(Param);
		if (MaterialInstance->GetTextureParameterValue(HashedParam, Texture) && Texture)
		{
			CustomInfo.DefaultValue = Texture->GetName();
		}

		Parameters.Add(CustomInfo);
	}

	return Parameters;
}

FString UMaterialService::GetParameterValue(const FString& MaterialPath, const FString& ParameterName)
{
	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(MaterialPath);
	if (!LoadedObject)
	{
		return FString();
	}

	UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(LoadedObject);
	if (!MaterialInstance)
	{
		return FString();
	}

	// Create FName first, then construct FHashedMaterialParameterInfo from it
	FName ParamName(*ParameterName);
	FHashedMaterialParameterInfo HashedParam(ParamName);

	// Try scalar
	float ScalarValue;
	if (MaterialInstance->GetScalarParameterValue(HashedParam, ScalarValue))
	{
		return FString::Printf(TEXT("%.3f"), ScalarValue);
	}

	// Try vector
	FLinearColor VectorValue;
	if (MaterialInstance->GetVectorParameterValue(HashedParam, VectorValue))
	{
		return VectorValue.ToString();
	}

	// Try texture
	UTexture* TextureValue;
	if (MaterialInstance->GetTextureParameterValue(HashedParam, TextureValue) && TextureValue)
	{
		return TextureValue->GetPathName();
	}

	return FString();
}
