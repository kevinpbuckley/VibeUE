// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UMaterialService.h"
#include "Core/JsonValueHelper.h"
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
#include "StaticParameterSet.h"

// =================================================================
// Helper Methods
// =================================================================

// Resolve a material property by internal name first, then fall back to a forgiving
// match against internal + display names (case-insensitive, ignoring spaces). This lets
// callers pass either the internal name ("TwoSided", "BlendMode", "OpacityMaskClipValue")
// or the editor display name ("Two Sided", "Blend Mode", "Opacity Mask Clip Value").
static FProperty* ResolveMaterialProperty(UObject* Object, const FString& PropertyName)
{
	if (!Object)
	{
		return nullptr;
	}

	UClass* Class = Object->GetClass();

	// 1. Exact internal-name match (fast path).
	if (FProperty* Exact = Class->FindPropertyByName(FName(*PropertyName)))
	{
		return Exact;
	}

	// 2. Forgiving match against internal + display names, ignoring case and spaces
	//    ("Two Sided" -> "TwoSided").
	const FString Wanted = PropertyName.Replace(TEXT(" "), TEXT("")).ToLower();
	for (TFieldIterator<FProperty> PropIt(Class, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (Property->GetName().Replace(TEXT(" "), TEXT("")).ToLower() == Wanted)
		{
			return Property;
		}
		if (Property->GetDisplayNameText().ToString().Replace(TEXT(" "), TEXT("")).ToLower() == Wanted)
		{
			return Property;
		}
	}

	return nullptr;
}

// Return the allowed enum value names for a property, handling both FEnumProperty and the
// classic TEnumAsByte<EFoo> (FByteProperty backed by a UEnum). Material enums like BlendMode,
// ShadingModel, and MaterialDomain are TEnumAsByte, which an FEnumProperty-only path misses —
// leaving allowed_values empty and forcing callers to guess the legal values.
static TArray<FString> GetPropertyAllowedValues(const FProperty* Property)
{
	TArray<FString> Values;

	const UEnum* Enum = nullptr;
	if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		Enum = EnumProp->GetEnum();
	}
	else if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		Enum = ByteProp->Enum;
	}

	if (Enum)
	{
		for (int32 i = 0; i < Enum->NumEnums() - 1; ++i) // -1 to skip _MAX
		{
			Values.Add(Enum->GetNameStringByIndex(i));
		}
	}

	return Values;
}

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

int64 UMaterialService::ResolveEnumValue(UEnum* Enum, const FString& Value)
{
	if (!Enum)
	{
		return INDEX_NONE;
	}

	// 1. Try exact match first (e.g., "BLEND_Masked", "MSM_DefaultLit")
	int64 Result = Enum->GetValueByNameString(Value);
	if (Result != INDEX_NONE)
	{
		return Result;
	}

	// 2. Try case-insensitive partial suffix match against all enum values.
	//    This handles AI sending "Masked" instead of "BLEND_Masked",
	//    or "DefaultLit" instead of "MSM_DefaultLit".
	//    We match if the enum name string ends with "_<Value>" (case-insensitive).
	FString SuffixPattern = FString::Printf(TEXT("_%s"), *Value);
	for (int32 i = 0; i < Enum->NumEnums() - 1; ++i) // -1 to skip _MAX
	{
		FString EnumName = Enum->GetNameStringByIndex(i);
		if (EnumName.Equals(Value, ESearchCase::IgnoreCase))
		{
			return Enum->GetValueByIndex(i);
		}
		if (EnumName.EndsWith(SuffixPattern, ESearchCase::IgnoreCase))
		{
			return Enum->GetValueByIndex(i);
		}
	}

	// 3. Try substring match — if the value is contained anywhere in the enum name
	for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
	{
		FString EnumName = Enum->GetNameStringByIndex(i);
		if (EnumName.Contains(Value, ESearchCase::IgnoreCase))
		{
			return Enum->GetValueByIndex(i);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("UMaterialService::ResolveEnumValue: Could not resolve '%s' in enum %s. Valid values:"), *Value, *Enum->GetName());
	for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
	{
		UE_LOG(LogTemp, Warning, TEXT("  - %s"), *Enum->GetNameStringByIndex(i));
	}

	return INDEX_NONE;
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
			int64 EnumValue = ResolveEnumValue(Enum, Value);
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
			int64 EnumValue = ResolveEnumValue(Enum, Value);
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

	// Auto-refresh the Material Editor UI if it's open, so the user
	// sees updated preview/graph without manually closing and reopening
	if (GEditor)
	{
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (AssetEditorSubsystem)
		{
			IAssetEditorInstance* Editor = AssetEditorSubsystem->FindEditorForAsset(Material, false);
			if (Editor)
			{
				AssetEditorSubsystem->CloseAllEditorsForAsset(Material);
				AssetEditorSubsystem->OpenEditorForAsset(Material);
			}
		}
	}

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

		// Get enum values if applicable (handles both FEnumProperty and TEnumAsByte)
		Info.AllowedValues = GetPropertyAllowedValues(Property);

		Properties.Add(Info);
	}

	return Properties;
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

	FProperty* Property = ResolveMaterialProperty(LoadedObject, PropertyName);
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

	OutInfo.AllowedValues = GetPropertyAllowedValues(Property);

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

	FProperty* Property = ResolveMaterialProperty(LoadedObject, PropertyName);
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

// =================================================================
// Bulk Parameter Setting
// =================================================================

int32 UMaterialService::SetInstanceParametersBulk(
	const FString& InstancePath,
	const TArray<FString>& Names,
	const TArray<FString>& Types,
	const TArray<FString>& Values)
{
	if (Names.Num() != Types.Num() || Names.Num() != Values.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("UMaterialService::SetInstanceParametersBulk: Mismatched array lengths (Names=%d, Types=%d, Values=%d)"),
			Names.Num(), Types.Num(), Values.Num());
		return 0;
	}

	UMaterialInstanceConstant* Instance = LoadMaterialInstanceConstant(InstancePath);
	if (!Instance)
	{
		return 0;
	}

	Instance->Modify();

	int32 SuccessCount = 0;

	for (int32 i = 0; i < Names.Num(); i++)
	{
		const FString& ParamName = Names[i];
		const FString& ParamType = Types[i];
		const FString& ParamValue = Values[i];
		FString TypeUpper = ParamType.ToUpper();

		if (TypeUpper == TEXT("SCALAR") || TypeUpper == TEXT("FLOAT"))
		{
			float Val = FCString::Atof(*ParamValue);
			Instance->SetScalarParameterValueEditorOnly(FName(*ParamName), Val);
			SuccessCount++;
		}
		else if (TypeUpper == TEXT("VECTOR") || TypeUpper == TEXT("COLOR"))
		{
			// Accept UE tuple "(R=..,G=..,B=..,A=..)", "R,G,B[,A]" arrays, #RRGGBB[AA]
			// hex, and named colors via the shared parser (issue #459 — hex parsed to
			// white and short arrays dropped channels).
			FLinearColor Color = FLinearColor::White;
			if (!FJsonValueHelper::TryParseLinearColor(ParamValue, Color))
			{
				UE_LOG(LogTemp, Warning,
					TEXT("UMaterialService::SetInstanceParametersBulk: could not parse color '%s' for param '%s'"),
					*ParamValue, *ParamName);
				continue;
			}
			Instance->SetVectorParameterValueEditorOnly(FName(*ParamName), Color);
			SuccessCount++;
		}
		else if (TypeUpper == TEXT("TEXTURE") || TypeUpper == TEXT("TEXTURE2D"))
		{
			if (ParamValue.IsEmpty())
			{
				Instance->SetTextureParameterValueEditorOnly(FName(*ParamName), nullptr);
				SuccessCount++;
			}
			else
			{
				UTexture* Texture = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(ParamValue));
				if (Texture)
				{
					Instance->SetTextureParameterValueEditorOnly(FName(*ParamName), Texture);
					SuccessCount++;
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("UMaterialService::SetInstanceParametersBulk: Texture not found: %s (param=%s)"),
						*ParamValue, *ParamName);
				}
			}
		}
		else if (TypeUpper == TEXT("STATICSWITCH") || TypeUpper == TEXT("BOOL"))
		{
			bool bVal = ParamValue.ToBool() || ParamValue == TEXT("1");
			// StaticSwitchParameters are on the base FStaticParameterSetRuntimeData in UE 5.7
			FStaticParameterSet StaticParams;
			Instance->GetStaticParameterValues(StaticParams);

			bool bFound = false;
			for (auto& Param : StaticParams.StaticSwitchParameters)
			{
				if (Param.ParameterInfo.Name.ToString().Equals(ParamName, ESearchCase::IgnoreCase))
				{
					Param.Value = bVal;
					Param.bOverride = true;
					bFound = true;
					break;
				}
			}

			if (!bFound)
			{
				// Add a new static switch parameter
				FStaticSwitchParameter NewParam;
				NewParam.ParameterInfo.Name = FName(*ParamName);
				NewParam.Value = bVal;
				NewParam.bOverride = true;
				StaticParams.StaticSwitchParameters.Add(NewParam);
			}

			Instance->UpdateStaticPermutation(StaticParams);
			SuccessCount++;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UMaterialService::SetInstanceParametersBulk: Unknown type '%s' for param '%s'"),
				*ParamType, *ParamName);
		}
	}

	// Mark dirty and trigger update
	UPackage* Package = Instance->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
	}

	UE_LOG(LogTemp, Log, TEXT("UMaterialService::SetInstanceParametersBulk: Set %d/%d parameters on '%s'"),
		SuccessCount, Names.Num(), *InstancePath);

	return SuccessCount;
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
