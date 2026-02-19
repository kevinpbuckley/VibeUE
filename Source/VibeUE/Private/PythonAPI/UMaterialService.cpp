// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UMaterialService.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Engine/Texture.h"

// =================================================================
// Helper Methods
// =================================================================

UMaterial* UMaterialService::LoadMaterialAsset(const FString& MaterialPath)
{
	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(MaterialPath);
	if (!LoadedObject)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialService: Failed to load material: %s"), *MaterialPath);
		return nullptr;
	}
	
	UMaterial* Material = Cast<UMaterial>(LoadedObject);
	if (!Material)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialService: Object is not a material: %s"), *MaterialPath);
		return nullptr;
	}
	
	return Material;
}

UMaterialInstance* UMaterialService::LoadMaterialInstanceAsset(const FString& InstancePath)
{
	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(InstancePath);
	if (!LoadedObject)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialService: Failed to load instance: %s"), *InstancePath);
		return nullptr;
	}
	
	UMaterialInstance* Instance = Cast<UMaterialInstance>(LoadedObject);
	if (!Instance)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialService: Object is not a material instance: %s"), *InstancePath);
		return nullptr;
	}
	
	return Instance;
}

UMaterialInstanceConstant* UMaterialService::LoadMaterialInstanceConstant(const FString& InstancePath)
{
	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(InstancePath);
	if (!LoadedObject)
	{
		return nullptr;
	}
	
	return Cast<UMaterialInstanceConstant>(LoadedObject);
}

FString UMaterialService::PropertyValueToString(const FProperty* Property, const void* Container)
{
	if (!Property || !Container)
	{
		return FString();
	}

	const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);

	// Bool
	if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		return BoolProp->GetPropertyValue(ValuePtr) ? TEXT("true") : TEXT("false");
	}
	// Float/Double
	if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		return FString::Printf(TEXT("%f"), FloatProp->GetPropertyValue(ValuePtr));
	}
	if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
	{
		return FString::Printf(TEXT("%f"), DoubleProp->GetPropertyValue(ValuePtr));
	}
	// Int
	if (const FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		return FString::Printf(TEXT("%d"), IntProp->GetPropertyValue(ValuePtr));
	}
	// Byte/Enum
	if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		if (UEnum* Enum = ByteProp->Enum)
		{
			return Enum->GetNameStringByValue(ByteProp->GetPropertyValue(ValuePtr));
		}
		return FString::Printf(TEXT("%d"), ByteProp->GetPropertyValue(ValuePtr));
	}
	if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		if (UEnum* Enum = EnumProp->GetEnum())
		{
			int64 Value = EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
			return Enum->GetNameStringByValue(Value);
		}
	}
	// String
	if (const FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		return StrProp->GetPropertyValue(ValuePtr);
	}
	// Name
	if (const FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		return NameProp->GetPropertyValue(ValuePtr).ToString();
	}

	// Fallback - use export text
	FString ExportedValue;
	Property->ExportTextItem_Direct(ExportedValue, ValuePtr, nullptr, nullptr, PPF_None);
	return ExportedValue;
}

bool UMaterialService::StringToPropertyValue(FProperty* Property, void* Container, const FString& Value)
{
	if (!Property || !Container)
	{
		return false;
	}

	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);

	// Bool
	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bool bValue = Value.ToBool() || Value.Equals(TEXT("1")) || Value.Equals(TEXT("true"), ESearchCase::IgnoreCase);
		BoolProp->SetPropertyValue(ValuePtr, bValue);
		return true;
	}
	// Float
	if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		FloatProp->SetPropertyValue(ValuePtr, FCString::Atof(*Value));
		return true;
	}
	// Double
	if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
	{
		DoubleProp->SetPropertyValue(ValuePtr, FCString::Atod(*Value));
		return true;
	}
	// Int
	if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		IntProp->SetPropertyValue(ValuePtr, FCString::Atoi(*Value));
		return true;
	}
	// Byte/Enum
	if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		if (UEnum* Enum = ByteProp->Enum)
		{
			int64 EnumValue = Enum->GetValueByNameString(Value);
			if (EnumValue != INDEX_NONE)
			{
				ByteProp->SetPropertyValue(ValuePtr, static_cast<uint8>(EnumValue));
				return true;
			}
		}
		ByteProp->SetPropertyValue(ValuePtr, static_cast<uint8>(FCString::Atoi(*Value)));
		return true;
	}
	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		if (UEnum* Enum = EnumProp->GetEnum())
		{
			int64 EnumValue = Enum->GetValueByNameString(Value);
			if (EnumValue != INDEX_NONE)
			{
				EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, EnumValue);
				return true;
			}
		}
	}
	// String
	if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		StrProp->SetPropertyValue(ValuePtr, Value);
		return true;
	}
	// Name
	if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		NameProp->SetPropertyValue(ValuePtr, FName(*Value));
		return true;
	}

	// Fallback - use import text
	return Property->ImportText_Direct(*Value, ValuePtr, nullptr, PPF_None) != nullptr;
}

TArray<FString> UMaterialService::GetEnumPropertyValues(const FEnumProperty* EnumProp)
{
	TArray<FString> Values;
	if (EnumProp && EnumProp->GetEnum())
	{
		UEnum* Enum = EnumProp->GetEnum();
		for (int32 i = 0; i < Enum->NumEnums() - 1; ++i) // -1 to skip _MAX
		{
			Values.Add(Enum->GetNameStringByIndex(i));
		}
	}
	return Values;
}

// =================================================================
// Lifecycle Actions
// =================================================================

FMaterialCreateResult UMaterialService::CreateMaterial(
	const FString& MaterialName,
	const FString& DestinationPath)
{
	FMaterialCreateResult Result;

	FString PackagePath = DestinationPath;
	if (!PackagePath.EndsWith(TEXT("/")))
	{
		PackagePath += TEXT("/");
	}

	// Check if asset already exists to avoid blocking overwrite dialog
	FString FullAssetPath = PackagePath + MaterialName;
	if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Material '%s' already exists at '%s'. Delete it first or use a different name."), *MaterialName, *FullAssetPath);
		UE_LOG(LogTemp, Error, TEXT("UMaterialService::CreateMaterial: %s"), *Result.ErrorMessage);
		return Result;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	
	UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
	
	UObject* NewAsset = AssetTools.CreateAsset(MaterialName, PackagePath, UMaterial::StaticClass(), Factory);
	
	if (NewAsset)
	{
		Result.bSuccess = true;
		Result.AssetPath = NewAsset->GetPathName();
		
		// Save immediately
		UEditorAssetLibrary::SaveAsset(Result.AssetPath, false);
	}
	else
	{
		Result.ErrorMessage = TEXT("Failed to create material asset");
	}
	
	return Result;
}

FMaterialCreateResult UMaterialService::CreateInstance(
	const FString& ParentMaterialPath,
	const FString& InstanceName,
	const FString& DestinationPath)
{
	FMaterialCreateResult Result;

	// Load parent material
	UObject* ParentObj = UEditorAssetLibrary::LoadAsset(ParentMaterialPath);
	UMaterialInterface* ParentMaterial = Cast<UMaterialInterface>(ParentObj);
	
	if (!ParentMaterial)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Parent material not found: %s"), *ParentMaterialPath);
		return Result;
	}

	FString PackagePath = DestinationPath;
	if (!PackagePath.EndsWith(TEXT("/")))
	{
		PackagePath += TEXT("/");
	}

	// Check if asset already exists to avoid blocking overwrite dialog
	FString FullAssetPath = PackagePath + InstanceName;
	if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Material instance '%s' already exists at '%s'. Delete it first or use a different name."), *InstanceName, *FullAssetPath);
		UE_LOG(LogTemp, Error, TEXT("UMaterialService::CreateInstance: %s"), *Result.ErrorMessage);
		return Result;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	
	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
	Factory->InitialParent = ParentMaterial;
	
	UObject* NewAsset = AssetTools.CreateAsset(InstanceName, PackagePath, UMaterialInstanceConstant::StaticClass(), Factory);
	
	if (NewAsset)
	{
		Result.bSuccess = true;
		Result.AssetPath = NewAsset->GetPathName();
		
		// Save immediately
		UEditorAssetLibrary::SaveAsset(Result.AssetPath, false);
	}
	else
	{
		Result.ErrorMessage = TEXT("Failed to create material instance");
	}
	
	return Result;
}

bool UMaterialService::SaveMaterial(const FString& MaterialPath)
{
	return UEditorAssetLibrary::SaveAsset(MaterialPath, false);
}

bool UMaterialService::CompileMaterial(const FString& MaterialPath)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return false;
	}

	Material->ForceRecompileForRendering();
	return true;
}

bool UMaterialService::RefreshEditor(const FString& MaterialPath)
{
	if (!GEditor)
	{
		return false;
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(MaterialPath);
	if (!Asset)
	{
		return false;
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (AssetEditorSubsystem)
	{
		// Close and reopen to refresh
		AssetEditorSubsystem->CloseAllEditorsForAsset(Asset);
		AssetEditorSubsystem->OpenEditorForAsset(Asset);
		return true;
	}

	return false;
}

bool UMaterialService::OpenInEditor(const FString& MaterialPath)
{
	if (!GEditor)
	{
		return false;
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(MaterialPath);
	if (!Asset)
	{
		return false;
	}

	return GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Asset);
}

// =================================================================
// Information Actions
// =================================================================

bool UMaterialService::GetMaterialInfo(const FString& MaterialPath, FMaterialDetailedInfo& OutInfo)
{
	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(MaterialPath);
	if (!LoadedObject)
	{
		return false;
	}

	UMaterial* Material = Cast<UMaterial>(LoadedObject);
	UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(LoadedObject);

	if (Material)
	{
		OutInfo.MaterialName = Material->GetName();
		OutInfo.MaterialPath = MaterialPath;
		OutInfo.bIsMaterialInstance = false;
		OutInfo.ParentMaterial = TEXT("");
		
		// Get domain, blend mode, etc.
		OutInfo.MaterialDomain = UEnum::GetValueAsString(Material->MaterialDomain);
		OutInfo.BlendMode = UEnum::GetValueAsString(Material->BlendMode);
		OutInfo.ShadingModel = UEnum::GetValueAsString(Material->GetShadingModels().GetFirstShadingModel());
		OutInfo.bTwoSided = Material->IsTwoSided();
		OutInfo.ExpressionCount = Material->GetExpressions().Num();
		
		// Count texture samples
		int32 TextureCount = 0;
		for (UMaterialExpression* Expr : Material->GetExpressions())
		{
			if (Expr && Expr->IsA<UMaterialExpressionTextureSampleParameter>())
			{
				TextureCount++;
			}
		}
		OutInfo.TextureSampleCount = TextureCount;
	}
	else if (MaterialInstance)
	{
		OutInfo.MaterialName = MaterialInstance->GetName();
		OutInfo.MaterialPath = MaterialPath;
		OutInfo.bIsMaterialInstance = true;

		if (UMaterialInterface* Parent = MaterialInstance->Parent)
		{
			OutInfo.ParentMaterial = Parent->GetPathName();
		}

		// Get info from parent
		if (UMaterial* BaseMat = MaterialInstance->GetMaterial())
		{
			OutInfo.MaterialDomain = UEnum::GetValueAsString(BaseMat->MaterialDomain);
			OutInfo.BlendMode = UEnum::GetValueAsString(BaseMat->BlendMode);
			OutInfo.ShadingModel = UEnum::GetValueAsString(BaseMat->GetShadingModels().GetFirstShadingModel());
			OutInfo.bTwoSided = BaseMat->IsTwoSided();
		}
	}
	else
	{
		return false;
	}

	// Get parameters
	OutInfo.Parameters = ListParameters(MaterialPath);

	return true;
}

bool UMaterialService::Summarize(const FString& MaterialPath, FMaterialSummary& OutSummary)
{
	FMaterialDetailedInfo Info;
	if (!GetMaterialInfo(MaterialPath, Info))
	{
		return false;
	}

	OutSummary.MaterialPath = Info.MaterialPath;
	OutSummary.MaterialName = Info.MaterialName;
	OutSummary.MaterialDomain = Info.MaterialDomain;
	OutSummary.BlendMode = Info.BlendMode;
	OutSummary.ShadingModel = Info.ShadingModel;
	OutSummary.bTwoSided = Info.bTwoSided;
	OutSummary.ExpressionCount = Info.ExpressionCount;
	OutSummary.ParameterCount = Info.Parameters.Num();

	for (const FMaterialParameterInfo_Custom& Param : Info.Parameters)
	{
		OutSummary.ParameterNames.Add(Param.ParameterName);
	}

	// Get key properties
	OutSummary.KeyProperties = ListProperties(MaterialPath, false);
	OutSummary.EditableProperties = ListProperties(MaterialPath, true);

	return true;
}

TArray<FMaterialPropertyInfo_Custom> UMaterialService::ListProperties(
	const FString& MaterialPath,
	bool bIncludeAdvanced)
{
	TArray<FMaterialPropertyInfo_Custom> Properties;

	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(MaterialPath);
	if (!LoadedObject)
	{
		return Properties;
	}

	UClass* Class = LoadedObject->GetClass();
	
	for (TFieldIterator<FProperty> PropIt(Class, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		
		// Skip non-edit properties
		if (!Property->HasAnyPropertyFlags(CPF_Edit))
		{
			continue;
		}
		
		bool bIsAdvanced = Property->HasAnyPropertyFlags(CPF_AdvancedDisplay);
		if (bIsAdvanced && !bIncludeAdvanced)
		{
			continue;
		}

		FMaterialPropertyInfo_Custom Info;
		Info.PropertyName = Property->GetName();
		Info.DisplayName = Property->GetDisplayNameText().ToString();
		Info.PropertyType = Property->GetCPPType();
		Info.Category = Property->GetMetaData(TEXT("Category"));
		Info.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);
		Info.bIsAdvanced = bIsAdvanced;
		Info.CurrentValue = PropertyValueToString(Property, LoadedObject);

		// Get enum values if applicable
		if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
		{
			Info.AllowedValues = GetEnumPropertyValues(EnumProp);
		}

		Properties.Add(Info);
	}

	return Properties;
}

FString UMaterialService::GetProperty(const FString& MaterialPath, const FString& PropertyName)
{
	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(MaterialPath);
	if (!LoadedObject)
	{
		return FString();
	}

	FProperty* Property = LoadedObject->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		return FString();
	}

	return PropertyValueToString(Property, LoadedObject);
}

bool UMaterialService::GetPropertyInfo(
	const FString& MaterialPath,
	const FString& PropertyName,
	FMaterialPropertyInfo_Custom& OutInfo)
{
	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(MaterialPath);
	if (!LoadedObject)
	{
		return false;
	}

	FProperty* Property = LoadedObject->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		return false;
	}

	OutInfo.PropertyName = Property->GetName();
	OutInfo.DisplayName = Property->GetDisplayNameText().ToString();
	OutInfo.PropertyType = Property->GetCPPType();
	OutInfo.Category = Property->GetMetaData(TEXT("Category"));
	OutInfo.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);
	OutInfo.bIsAdvanced = Property->HasAnyPropertyFlags(CPF_AdvancedDisplay);
	OutInfo.CurrentValue = PropertyValueToString(Property, LoadedObject);

	if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		OutInfo.AllowedValues = GetEnumPropertyValues(EnumProp);
	}

	return true;
}

// =================================================================
// Property Management
// =================================================================

bool UMaterialService::SetProperty(
	const FString& MaterialPath,
	const FString& PropertyName,
	const FString& PropertyValue)
{
	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(MaterialPath);
	if (!LoadedObject)
	{
		return false;
	}

	FProperty* Property = LoadedObject->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialService::SetProperty: Property not found: %s"), *PropertyName);
		return false;
	}

	LoadedObject->Modify();
	
	if (!StringToPropertyValue(Property, LoadedObject, PropertyValue))
	{
		return false;
	}

	LoadedObject->PostEditChange();
	
	// Mark package dirty
	UPackage* Package = LoadedObject->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
	}

	return true;
}

int32 UMaterialService::SetProperties(
	const FString& MaterialPath,
	const TMap<FString, FString>& Properties)
{
	int32 SetCount = 0;
	
	for (const auto& Pair : Properties)
	{
		if (SetProperty(MaterialPath, Pair.Key, Pair.Value))
		{
			SetCount++;
		}
	}
	
	return SetCount;
}

// =================================================================
// Parameter Management
// =================================================================

TArray<FMaterialParameterInfo_Custom> UMaterialService::ListParameters(const FString& MaterialPath)
{
	TArray<FMaterialParameterInfo_Custom> Parameters;

	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(MaterialPath);
	if (!LoadedObject)
	{
		return Parameters;
	}

	UMaterialInterface* MatInterface = Cast<UMaterialInterface>(LoadedObject);
	if (!MatInterface)
	{
		return Parameters;
	}

	// Get scalar parameters
	TArray<FMaterialParameterInfo> ScalarParams;
	TArray<FGuid> ScalarGuids;
	MatInterface->GetAllScalarParameterInfo(ScalarParams, ScalarGuids);

	for (const FMaterialParameterInfo& Param : ScalarParams)
	{
		FMaterialParameterInfo_Custom CustomInfo;
		CustomInfo.ParameterName = Param.Name.ToString();
		CustomInfo.ParameterType = TEXT("Scalar");
		CustomInfo.Group = TEXT("");

		float Value;
		FHashedMaterialParameterInfo HashedParam(Param);
		if (MatInterface->GetScalarParameterValue(HashedParam, Value))
		{
			CustomInfo.CurrentValue = FString::Printf(TEXT("%.3f"), Value);
			CustomInfo.DefaultValue = CustomInfo.CurrentValue;
		}

		Parameters.Add(CustomInfo);
	}

	// Get vector parameters
	TArray<FMaterialParameterInfo> VectorParams;
	TArray<FGuid> VectorGuids;
	MatInterface->GetAllVectorParameterInfo(VectorParams, VectorGuids);

	for (const FMaterialParameterInfo& Param : VectorParams)
	{
		FMaterialParameterInfo_Custom CustomInfo;
		CustomInfo.ParameterName = Param.Name.ToString();
		CustomInfo.ParameterType = TEXT("Vector");
		CustomInfo.Group = TEXT("");

		FLinearColor Value;
		FHashedMaterialParameterInfo HashedParam(Param);
		if (MatInterface->GetVectorParameterValue(HashedParam, Value))
		{
			CustomInfo.CurrentValue = FString::Printf(TEXT("(R=%.3f,G=%.3f,B=%.3f,A=%.3f)"), Value.R, Value.G, Value.B, Value.A);
			CustomInfo.DefaultValue = CustomInfo.CurrentValue;
		}

		Parameters.Add(CustomInfo);
	}

	// Get texture parameters
	TArray<FMaterialParameterInfo> TextureParams;
	TArray<FGuid> TextureGuids;
	MatInterface->GetAllTextureParameterInfo(TextureParams, TextureGuids);

	for (const FMaterialParameterInfo& Param : TextureParams)
	{
		FMaterialParameterInfo_Custom CustomInfo;
		CustomInfo.ParameterName = Param.Name.ToString();
		CustomInfo.ParameterType = TEXT("Texture");
		CustomInfo.Group = TEXT("");

		UTexture* Texture = nullptr;
		FHashedMaterialParameterInfo HashedParam(Param);
		if (MatInterface->GetTextureParameterValue(HashedParam, Texture) && Texture)
		{
			CustomInfo.CurrentValue = Texture->GetPathName();
			CustomInfo.DefaultValue = CustomInfo.CurrentValue;
		}

		Parameters.Add(CustomInfo);
	}

	return Parameters;
}

bool UMaterialService::GetParameter(
	const FString& MaterialPath,
	const FString& ParameterName,
	FMaterialParameterInfo_Custom& OutInfo)
{
	TArray<FMaterialParameterInfo_Custom> AllParams = ListParameters(MaterialPath);
	
	for (const FMaterialParameterInfo_Custom& Param : AllParams)
	{
		if (Param.ParameterName.Equals(ParameterName, ESearchCase::IgnoreCase))
		{
			OutInfo = Param;
			return true;
		}
	}
	
	return false;
}

bool UMaterialService::SetParameterDefault(
	const FString& MaterialPath,
	const FString& ParameterName,
	const FString& DefaultValue)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return false;
	}

	// Find the parameter expression and set its default
	for (UMaterialExpression* Expr : Material->GetExpressions())
	{
		if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expr))
		{
			if (ScalarParam->ParameterName.ToString().Equals(ParameterName, ESearchCase::IgnoreCase))
			{
				ScalarParam->Modify();
				ScalarParam->DefaultValue = FCString::Atof(*DefaultValue);
				Material->PostEditChange();
				return true;
			}
		}
		else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expr))
		{
			if (VectorParam->ParameterName.ToString().Equals(ParameterName, ESearchCase::IgnoreCase))
			{
				VectorParam->Modify();
				// Parse color from string (R=x,G=x,B=x,A=x) or (x,x,x,x)
				FLinearColor Color;
				if (Color.InitFromString(DefaultValue))
				{
					VectorParam->DefaultValue = Color;
					Material->PostEditChange();
					return true;
				}
			}
		}
	}

	return false;
}

// =================================================================
// Instance Information Actions
// =================================================================

bool UMaterialService::GetInstanceInfo(const FString& InstancePath, FVibeUEMaterialInstanceInfo& OutInfo)
{
	UMaterialInstanceConstant* Instance = LoadMaterialInstanceConstant(InstancePath);
	if (!Instance)
	{
		return false;
	}

	OutInfo.InstanceName = Instance->GetName();
	OutInfo.InstancePath = InstancePath;
	
	if (Instance->Parent)
	{
		OutInfo.ParentMaterialPath = Instance->Parent->GetPathName();
		OutInfo.ParentMaterialName = Instance->Parent->GetName();
	}

	// Get scalar parameters
	TArray<FMaterialParameterInfo> ScalarParams;
	TArray<FGuid> ScalarGuids;
	Instance->GetAllScalarParameterInfo(ScalarParams, ScalarGuids);

	for (const FMaterialParameterInfo& Param : ScalarParams)
	{
		FMaterialParameterInfo_Custom CustomInfo;
		CustomInfo.ParameterName = Param.Name.ToString();
		CustomInfo.ParameterType = TEXT("Scalar");

		float Value;
		FHashedMaterialParameterInfo HashedParam(Param);
		if (Instance->GetScalarParameterValue(HashedParam, Value))
		{
			CustomInfo.CurrentValue = FString::Printf(TEXT("%.3f"), Value);
		}

		OutInfo.ScalarParameters.Add(CustomInfo);
	}

	// Get vector parameters
	TArray<FMaterialParameterInfo> VectorParams;
	TArray<FGuid> VectorGuids;
	Instance->GetAllVectorParameterInfo(VectorParams, VectorGuids);

	for (const FMaterialParameterInfo& Param : VectorParams)
	{
		FMaterialParameterInfo_Custom CustomInfo;
		CustomInfo.ParameterName = Param.Name.ToString();
		CustomInfo.ParameterType = TEXT("Vector");

		FLinearColor Value;
		FHashedMaterialParameterInfo HashedParam(Param);
		if (Instance->GetVectorParameterValue(HashedParam, Value))
		{
			CustomInfo.CurrentValue = FString::Printf(TEXT("(R=%.3f,G=%.3f,B=%.3f,A=%.3f)"), Value.R, Value.G, Value.B, Value.A);
		}

		OutInfo.VectorParameters.Add(CustomInfo);
	}

	// Get texture parameters
	TArray<FMaterialParameterInfo> TextureParams;
	TArray<FGuid> TextureGuids;
	Instance->GetAllTextureParameterInfo(TextureParams, TextureGuids);

	for (const FMaterialParameterInfo& Param : TextureParams)
	{
		FMaterialParameterInfo_Custom CustomInfo;
		CustomInfo.ParameterName = Param.Name.ToString();
		CustomInfo.ParameterType = TEXT("Texture");

		UTexture* Texture = nullptr;
		FHashedMaterialParameterInfo HashedParam(Param);
		if (Instance->GetTextureParameterValue(HashedParam, Texture) && Texture)
		{
			CustomInfo.CurrentValue = Texture->GetPathName();
		}

		OutInfo.TextureParameters.Add(CustomInfo);
	}

	return true;
}

TArray<FMaterialPropertyInfo_Custom> UMaterialService::ListInstanceProperties(
	const FString& InstancePath,
	bool bIncludeAdvanced)
{
	return ListProperties(InstancePath, bIncludeAdvanced);
}

FString UMaterialService::GetInstanceProperty(const FString& InstancePath, const FString& PropertyName)
{
	return GetProperty(InstancePath, PropertyName);
}

bool UMaterialService::SetInstanceProperty(
	const FString& InstancePath,
	const FString& PropertyName,
	const FString& PropertyValue)
{
	return SetProperty(InstancePath, PropertyName, PropertyValue);
}

// =================================================================
// Instance Parameter Actions
// =================================================================

TArray<FMaterialParameterInfo_Custom> UMaterialService::ListInstanceParameters(const FString& InstancePath)
{
	TArray<FMaterialParameterInfo_Custom> Parameters;

	UMaterialInstanceConstant* Instance = LoadMaterialInstanceConstant(InstancePath);
	if (!Instance)
	{
		return Parameters;
	}

	// Get scalar parameters with override status
	for (const FScalarParameterValue& ScalarParam : Instance->ScalarParameterValues)
	{
		FMaterialParameterInfo_Custom CustomInfo;
		CustomInfo.ParameterName = ScalarParam.ParameterInfo.Name.ToString();
		CustomInfo.ParameterType = TEXT("Scalar");
		CustomInfo.CurrentValue = FString::Printf(TEXT("%.3f"), ScalarParam.ParameterValue);
		CustomInfo.bIsOverridden = true;
		Parameters.Add(CustomInfo);
	}

	// Get vector parameters with override status
	for (const FVectorParameterValue& VectorParam : Instance->VectorParameterValues)
	{
		FMaterialParameterInfo_Custom CustomInfo;
		CustomInfo.ParameterName = VectorParam.ParameterInfo.Name.ToString();
		CustomInfo.ParameterType = TEXT("Vector");
		CustomInfo.CurrentValue = FString::Printf(TEXT("(R=%.3f,G=%.3f,B=%.3f,A=%.3f)"), 
			VectorParam.ParameterValue.R, VectorParam.ParameterValue.G, 
			VectorParam.ParameterValue.B, VectorParam.ParameterValue.A);
		CustomInfo.bIsOverridden = true;
		Parameters.Add(CustomInfo);
	}

	// Get texture parameters with override status
	for (const FTextureParameterValue& TextureParam : Instance->TextureParameterValues)
	{
		FMaterialParameterInfo_Custom CustomInfo;
		CustomInfo.ParameterName = TextureParam.ParameterInfo.Name.ToString();
		CustomInfo.ParameterType = TEXT("Texture");
		if (TextureParam.ParameterValue)
		{
			CustomInfo.CurrentValue = TextureParam.ParameterValue->GetPathName();
		}
		CustomInfo.bIsOverridden = true;
		Parameters.Add(CustomInfo);
	}

	return Parameters;
}

bool UMaterialService::SetInstanceScalarParameter(
	const FString& InstancePath,
	const FString& ParameterName,
	float Value)
{
	UMaterialInstanceConstant* Instance = LoadMaterialInstanceConstant(InstancePath);
	if (!Instance)
	{
		return false;
	}

	Instance->Modify();
	Instance->SetScalarParameterValueEditorOnly(FName(*ParameterName), Value);
	
	UPackage* Package = Instance->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
	}

	return true;
}

bool UMaterialService::SetInstanceVectorParameter(
	const FString& InstancePath,
	const FString& ParameterName,
	float R, float G, float B, float A)
{
	UMaterialInstanceConstant* Instance = LoadMaterialInstanceConstant(InstancePath);
	if (!Instance)
	{
		return false;
	}

	Instance->Modify();
	Instance->SetVectorParameterValueEditorOnly(FName(*ParameterName), FLinearColor(R, G, B, A));
	
	UPackage* Package = Instance->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
	}

	return true;
}

bool UMaterialService::SetInstanceTextureParameter(
	const FString& InstancePath,
	const FString& ParameterName,
	const FString& TexturePath)
{
	UMaterialInstanceConstant* Instance = LoadMaterialInstanceConstant(InstancePath);
	if (!Instance)
	{
		return false;
	}

	UTexture* Texture = nullptr;
	if (!TexturePath.IsEmpty())
	{
		Texture = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(TexturePath));
		if (!Texture)
		{
			UE_LOG(LogTemp, Warning, TEXT("UMaterialService::SetInstanceTextureParameter: Texture not found: %s"), *TexturePath);
			return false;
		}
	}

	Instance->Modify();
	Instance->SetTextureParameterValueEditorOnly(FName(*ParameterName), Texture);
	
	UPackage* Package = Instance->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
	}

	return true;
}

bool UMaterialService::ClearInstanceParameterOverride(
	const FString& InstancePath,
	const FString& ParameterName)
{
	UMaterialInstanceConstant* Instance = LoadMaterialInstanceConstant(InstancePath);
	if (!Instance)
	{
		return false;
	}

	FName ParamName(*ParameterName);
	Instance->Modify();

	// Try to clear each parameter type
	bool bCleared = false;

	// Clear scalar
	for (int32 i = Instance->ScalarParameterValues.Num() - 1; i >= 0; --i)
	{
		if (Instance->ScalarParameterValues[i].ParameterInfo.Name == ParamName)
		{
			Instance->ScalarParameterValues.RemoveAt(i);
			bCleared = true;
		}
	}

	// Clear vector
	for (int32 i = Instance->VectorParameterValues.Num() - 1; i >= 0; --i)
	{
		if (Instance->VectorParameterValues[i].ParameterInfo.Name == ParamName)
		{
			Instance->VectorParameterValues.RemoveAt(i);
			bCleared = true;
		}
	}

	// Clear texture
	for (int32 i = Instance->TextureParameterValues.Num() - 1; i >= 0; --i)
	{
		if (Instance->TextureParameterValues[i].ParameterInfo.Name == ParamName)
		{
			Instance->TextureParameterValues.RemoveAt(i);
			bCleared = true;
		}
	}

	if (bCleared)
	{
		Instance->PostEditChange();
		UPackage* Package = Instance->GetOutermost();
		if (Package)
		{
			Package->MarkPackageDirty();
		}
	}

	return bCleared;
}

bool UMaterialService::SaveInstance(const FString& InstancePath)
{
	return UEditorAssetLibrary::SaveAsset(InstancePath, false);
}

// =================================================================
// Existence Checks
// =================================================================

bool UMaterialService::MaterialExists(const FString& MaterialPath)
{
	if (MaterialPath.IsEmpty())
	{
		return false;
	}
	return UEditorAssetLibrary::DoesAssetExist(MaterialPath);
}

bool UMaterialService::MaterialInstanceExists(const FString& InstancePath)
{
	if (InstancePath.IsEmpty())
	{
		return false;
	}
	return UEditorAssetLibrary::DoesAssetExist(InstancePath);
}

bool UMaterialService::ParameterExists(const FString& MaterialPath, const FString& ParameterName)
{
	if (MaterialPath.IsEmpty() || ParameterName.IsEmpty())
	{
		return false;
	}

	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return false;
	}

	// Get all parameters and check if any match
	TArray<FMaterialParameterInfo> ParameterInfos;
	TArray<FGuid> ParameterGuids;

	// Check scalar parameters
	Material->GetAllScalarParameterInfo(ParameterInfos, ParameterGuids);
	for (const FMaterialParameterInfo& Info : ParameterInfos)
	{
		if (Info.Name.ToString().Equals(ParameterName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	// Check vector parameters
	ParameterInfos.Empty();
	ParameterGuids.Empty();
	Material->GetAllVectorParameterInfo(ParameterInfos, ParameterGuids);
	for (const FMaterialParameterInfo& Info : ParameterInfos)
	{
		if (Info.Name.ToString().Equals(ParameterName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	// Check texture parameters
	ParameterInfos.Empty();
	ParameterGuids.Empty();
	Material->GetAllTextureParameterInfo(ParameterInfos, ParameterGuids);
	for (const FMaterialParameterInfo& Info : ParameterInfos)
	{
		if (Info.Name.ToString().Equals(ParameterName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}
