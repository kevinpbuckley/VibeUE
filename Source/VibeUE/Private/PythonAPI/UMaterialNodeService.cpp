// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UMaterialNodeService.h"
#include "Core/JsonValueHelper.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Materials/MaterialExpressionLandscapeGrassOutput.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialParameterCollection.h"
#include "LandscapeGrassType.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialEditingLibrary.h"
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/MaterialFunctionFactoryNew.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectIterator.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Texture2D.h"
#include "MaterialShared.h"
#include "RHIDefinitions.h"

// =================================================================
// Diagnostics
// =================================================================


// =================================================================
// Helper Methods
// =================================================================

UMaterial* UMaterialNodeService::LoadMaterialAsset(const FString& MaterialPath)
{
	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(MaterialPath);
	if (!LoadedObject)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService: Failed to load material: %s"), *MaterialPath);
		return nullptr;
	}
	
	UMaterial* Material = Cast<UMaterial>(LoadedObject);
	if (!Material)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService: Object is not a material: %s"), *MaterialPath);
		return nullptr;
	}
	
	return Material;
}

UMaterialExpression* UMaterialNodeService::FindExpressionById(UMaterial* Material, const FString& ExpressionId)
{
	// Strict identifier validation. An empty/garbage id must resolve to NOTHING — the old code
	// fell through to FCString::Atoi (which returns 0 for non-numeric input), so a bogus or empty
	// id silently resolved to expression index 0, and any writer using this helper mutated the
	// wrong node. Empty, malformed, stale, out-of-range and foreign ids now all return nullptr.
	if (!Material || ExpressionId.IsEmpty())
	{
		return nullptr;
	}

	TArray<UMaterialExpression*> Expressions;
	Material->GetAllExpressionsInMaterialAndFunctionsOfType<UMaterialExpression>(Expressions);

	// (1) Exact VibeUE session id ("<Class>_<pointer>") — matches this material's graph,
	//     including expressions inside referenced material functions.
	for (UMaterialExpression* Expression : Expressions)
	{
		if (Expression && GetExpressionId(Expression) == ExpressionId)
		{
			return Expression;
		}
	}

	// (2) Object path ("/Game/M.M:MaterialExpressionConstant_0") or bare subobject name
	//     ("MaterialExpressionConstant_0"), accepted ONLY for expressions this material owns
	//     directly (Outer == Material). This refuses a foreign material's expression and a nested
	//     material-function expression addressed through the wrong graph — both would otherwise
	//     be reachable objects, so ownership is what makes the path legal.
	if (ExpressionId.Contains(TEXT(":")) || ExpressionId.Contains(TEXT(".")) ||
		ExpressionId.StartsWith(TEXT("MaterialExpression")))
	{
		for (UMaterialExpression* Expression : Expressions)
		{
			if (!Expression || Expression->GetOuter() != Material)
			{
				continue;
			}
			if (Expression->GetPathName() == ExpressionId || Expression->GetName() == ExpressionId)
			{
				return Expression;
			}
		}
	}

	// (3) Strict whole-string, range-checked numeric index (back-compat). Every character must be a
	//     digit — a leading sign, trailing text or an out-of-range value is rejected, not truncated.
	bool bAllDigits = true;
	for (const TCHAR Ch : ExpressionId)
	{
		if (!FChar::IsDigit(Ch))
		{
			bAllDigits = false;
			break;
		}
	}
	if (bAllDigits)
	{
		const int32 Index = FCString::Atoi(*ExpressionId);
		if (Index >= 0 && Index < Expressions.Num())
		{
			return Expressions[Index];
		}
	}

	return nullptr;
}

FString UMaterialNodeService::GetExpressionId(UMaterialExpression* Expression)
{
	if (!Expression) return TEXT("");
	return FString::Printf(TEXT("%s_%p"), *Expression->GetClass()->GetName(), Expression);
}

FExpressionInput* UMaterialNodeService::FindInputByName(UMaterialExpression* Expression, const FString& InputName)
{
	if (!Expression) return nullptr;
	
	// Try exact name match
	for (int32 i = 0; ; i++)
	{
		FExpressionInput* Input = Expression->GetInput(i);
		if (!Input) break;
		FName Name = Expression->GetInputName(i);
		if (Name.ToString().Equals(InputName, ESearchCase::IgnoreCase))
		{
			return Input;
		}
	}
	
	// Try index-based match (Input_0, Input_1, etc.)
	if (InputName.StartsWith(TEXT("Input_")))
	{
		int32 Index = FCString::Atoi(*InputName.RightChop(6));
		FExpressionInput* Input = (Index >= 0) ? Expression->GetInput(Index) : nullptr;
		if (Input)
		{
			return Input;
		}
	}
	
	// Try numeric index directly
	if (InputName.IsNumeric())
	{
		int32 Index = FCString::Atoi(*InputName);
		FExpressionInput* Input = (Index >= 0) ? Expression->GetInput(Index) : nullptr;
		if (Input)
		{
			return Input;
		}
	}
	
	// Common aliases
	FExpressionInput* FirstInput = Expression->GetInput(0);
	if (FirstInput && (InputName.Equals(TEXT("A"), ESearchCase::IgnoreCase) || InputName.Equals(TEXT("Input"), ESearchCase::IgnoreCase)))
	{
		return FirstInput;
	}
	FExpressionInput* SecondInput = Expression->GetInput(1);
	if (SecondInput && InputName.Equals(TEXT("B"), ESearchCase::IgnoreCase))
	{
		return SecondInput;
	}
	
	return nullptr;
}

int32 UMaterialNodeService::FindOutputIndexByName(UMaterialExpression* Expression, const FString& OutputName)
{
	if (!Expression) return -1;
	
	TArray<FExpressionOutput>& Outputs = Expression->GetOutputs();
	
	if (OutputName.IsEmpty())
	{
		return Outputs.Num() > 0 ? 0 : -1;
	}
	
	for (int32 i = 0; i < Outputs.Num(); i++)
	{
		if (Outputs[i].OutputName.ToString().Equals(OutputName, ESearchCase::IgnoreCase))
		{
			return i;
		}
	}
	
	if (OutputName.StartsWith(TEXT("Output_")))
	{
		FString IndexStr = OutputName.RightChop(7);
		int32 Index = FCString::Atoi(*IndexStr);
		if (Index >= 0 && Index < Outputs.Num())
		{
			return Index;
		}
	}
	
	if (OutputName.IsNumeric())
	{
		int32 Index = FCString::Atoi(*OutputName);
		if (Index >= 0 && Index < Outputs.Num())
		{
			return Index;
		}
	}

	// A non-empty output name that matches nothing is unresolved. Return INDEX_NONE so callers can
	// reject it; ConnectFunctionExpressions clamps a negative index back to 0 to preserve its old
	// lenient behaviour, while ConnectExpressionToOutput treats it as a hard failure.
	return INDEX_NONE;
}

TArray<FString> UMaterialNodeService::GetExpressionInputNames(UMaterialExpression* Expression)
{
	TArray<FString> Names;
	if (!Expression) return Names;
	
	for (int32 i = 0; Expression->GetInput(i) != nullptr; i++)
	{
		FName Name = Expression->GetInputName(i);
		if (Name != NAME_None)
		{
			Names.Add(Name.ToString());
		}
		else
		{
			Names.Add(FString::Printf(TEXT("Input_%d"), i));
		}
	}
	
	return Names;
}

TArray<FString> UMaterialNodeService::GetExpressionOutputNames(UMaterialExpression* Expression)
{
	TArray<FString> Names;
	if (!Expression) return Names;
	
	TArray<FExpressionOutput>& Outputs = Expression->GetOutputs();
	
	for (int32 i = 0; i < Outputs.Num(); i++)
	{
		if (Outputs[i].OutputName != NAME_None)
		{
			Names.Add(Outputs[i].OutputName.ToString());
		}
		else
		{
			Names.Add(FString::Printf(TEXT("Output_%d"), i));
		}
	}
	
	return Names;
}

UClass* UMaterialNodeService::ResolveExpressionClass(const FString& ClassName)
{
	FString FullName = ClassName;
	if (!FullName.StartsWith(TEXT("MaterialExpression")))
	{
		FullName = TEXT("MaterialExpression") + ClassName;
	}
	
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->IsChildOf(UMaterialExpression::StaticClass()) && 
			(Class->GetName().Equals(FullName, ESearchCase::IgnoreCase) ||
			 Class->GetName().Equals(ClassName, ESearchCase::IgnoreCase)))
		{
			return Class;
		}
	}
	
	return nullptr;
}

FMaterialExpressionInfo UMaterialNodeService::BuildExpressionInfo(UMaterialExpression* Expression)
{
	FMaterialExpressionInfo Info;
	if (!Expression) return Info;
	
	Info.Id = GetExpressionId(Expression);
	Info.ObjectPath = Expression->GetPathName();
	Info.ClassName = Expression->GetClass()->GetName();
	Info.DisplayName = Info.ClassName.Replace(TEXT("MaterialExpression"), TEXT(""));
	Info.PosX = Expression->MaterialExpressionEditorX;
	Info.PosY = Expression->MaterialExpressionEditorY;
	Info.Description = Expression->GetDescription();
	
	if (UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(Expression))
	{
		Info.bIsParameter = true;
		Info.ParameterName = ParamExpr->ParameterName.ToString();
		Info.Category = ParamExpr->Group.ToString();
	}
	
	for (int32 i = 0; Expression->GetInput(i) != nullptr; i++)
	{
		FName InputName = Expression->GetInputName(i);
		Info.InputNames.Add(InputName.IsNone() ? FString::Printf(TEXT("Input_%d"), i) : InputName.ToString());
	}
	
	TArray<FExpressionOutput>& Outputs = Expression->GetOutputs();
	for (int32 i = 0; i < Outputs.Num(); i++)
	{
		Info.OutputNames.Add(Outputs[i].OutputName.IsNone() ? FString::Printf(TEXT("Output_%d"), i) : Outputs[i].OutputName.ToString());
	}
	
	return Info;
}

const TArray<TPair<FString, EMaterialProperty>>& UMaterialNodeService::GetMaterialOutputProperties()
{
	// THE single source of truth for "which material outputs this service understands". Both the
	// writers (StringToMaterialProperty, used by connect_expression_to_output / disconnect_output)
	// and the reader (GetOutputConnections) are driven from this list, so the two can never drift
	// apart again — previously the writers accepted 19 properties while the getter reported only 16,
	// leaving ClearCoat / ClearCoatRoughness / Displacement writable but invisible (issue #611).
	static const TArray<TPair<FString, EMaterialProperty>> Properties = {
		{TEXT("BaseColor"), MP_BaseColor},
		{TEXT("Metallic"), MP_Metallic},
		{TEXT("Specular"), MP_Specular},
		{TEXT("Roughness"), MP_Roughness},
		{TEXT("Anisotropy"), MP_Anisotropy},
		{TEXT("EmissiveColor"), MP_EmissiveColor},
		{TEXT("Opacity"), MP_Opacity},
		{TEXT("OpacityMask"), MP_OpacityMask},
		{TEXT("Normal"), MP_Normal},
		{TEXT("Tangent"), MP_Tangent},
		{TEXT("WorldPositionOffset"), MP_WorldPositionOffset},
		{TEXT("SubsurfaceColor"), MP_SubsurfaceColor},
		{TEXT("ClearCoat"), MP_CustomData0},
		{TEXT("ClearCoatRoughness"), MP_CustomData1},
		{TEXT("AmbientOcclusion"), MP_AmbientOcclusion},
		{TEXT("Refraction"), MP_Refraction},
		{TEXT("PixelDepthOffset"), MP_PixelDepthOffset},
		{TEXT("ShadingModel"), MP_ShadingModel},
		{TEXT("Displacement"), MP_Displacement},
	};
	return Properties;
}

bool UMaterialNodeService::StringToMaterialProperty(const FString& PropertyName, EMaterialProperty& OutProperty)
{
	// Normalise: trim, and accept the "MP_BaseColor" enum spelling by dropping the leading "MP_".
	FString Key = PropertyName.TrimStartAndEnd();
	if (Key.StartsWith(TEXT("MP_"), ESearchCase::IgnoreCase))
	{
		Key = Key.RightChop(3);
	}

	for (const TPair<FString, EMaterialProperty>& Pair : GetMaterialOutputProperties())
	{
		if (Pair.Key.Equals(Key, ESearchCase::IgnoreCase))
		{
			OutProperty = Pair.Value;
			return true;
		}
	}

	// Unknown property name: refuse (do NOT default to MP_BaseColor — a bad name must not silently
	// rewire the wrong material output).
	return false;
}

void UMaterialNodeService::RefreshMaterialGraph(UMaterial* Material)
{
	if (!Material) return;
	
	if (!IsInGameThread())
	{
		return;
	}
	
	Material->MarkPackageDirty();
	
	if (IsValid(Material))
	{
		Material->PreEditChange(nullptr);
		Material->PostEditChange();
	}
	
	if (Material->MaterialGraph)
	{
		if (UMaterialGraph* MaterialGraph = Cast<UMaterialGraph>(Material->MaterialGraph))
		{
			if (IsValid(MaterialGraph))
			{
				MaterialGraph->LinkMaterialExpressionsFromGraph();
				MaterialGraph->RebuildGraph();
			}
		}
	}
}

// =================================================================
// Discovery Actions
// =================================================================



// =================================================================
// Lifecycle Actions
// =================================================================

// =================================================================
// Specialized Creation Actions
// =================================================================

FMaterialExpressionInfo UMaterialNodeService::CreateFunctionCall(
	const FString& MaterialPath,
	const FString& FunctionPath,
	int32 PosX,
	int32 PosY)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return FMaterialExpressionInfo();
	}

	// Load the material function asset
	UMaterialFunction* Function = LoadObject<UMaterialFunction>(nullptr, *FunctionPath);
	if (!Function)
	{
		// Try with explicit class
		UObject* LoadedObj = UEditorAssetLibrary::LoadAsset(FunctionPath);
		Function = Cast<UMaterialFunction>(LoadedObj);
	}
	if (!Function)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::CreateFunctionCall: Failed to load function: %s"), *FunctionPath);
		return FMaterialExpressionInfo();
	}

	FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Create Function Call", "Create Function Call"));
	Material->Modify();

	UMaterialExpressionMaterialFunctionCall* FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(
		UMaterialEditingLibrary::CreateMaterialExpression(
			Material, UMaterialExpressionMaterialFunctionCall::StaticClass(), PosX, PosY));

	if (!FuncCall)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::CreateFunctionCall: Failed to create expression"));
		return FMaterialExpressionInfo();
	}

	// SetMaterialFunction automatically creates the correct inputs/outputs
	FuncCall->SetMaterialFunction(Function);

	RefreshMaterialGraph(Material);

	return BuildExpressionInfo(FuncCall);
}


FMaterialExpressionInfo UMaterialNodeService::CreateCollectionParameter(
	const FString& MaterialPath,
	const FString& CollectionPath,
	const FString& ParameterName,
	int32 PosX,
	int32 PosY)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return FMaterialExpressionInfo();
	}

	// Load the parameter collection asset
	UMaterialParameterCollection* Collection = LoadObject<UMaterialParameterCollection>(nullptr, *CollectionPath);
	if (!Collection)
	{
		UObject* LoadedObj = UEditorAssetLibrary::LoadAsset(CollectionPath);
		Collection = Cast<UMaterialParameterCollection>(LoadedObj);
	}
	if (!Collection)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::CreateCollectionParameter: Failed to load collection: %s"), *CollectionPath);
		return FMaterialExpressionInfo();
	}

	FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Create Collection Parameter", "Create Collection Parameter"));
	Material->Modify();

	UMaterialExpressionCollectionParameter* CollParam = Cast<UMaterialExpressionCollectionParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(
			Material, UMaterialExpressionCollectionParameter::StaticClass(), PosX, PosY));

	if (!CollParam)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::CreateCollectionParameter: Failed to create expression"));
		return FMaterialExpressionInfo();
	}

	CollParam->Collection = Collection;

	// Find and set the parameter by name
	FName ParamFName(*ParameterName);
	FGuid ParamGuid;
	bool bFound = false;

	// Check scalar parameters
	for (const FCollectionScalarParameter& Param : Collection->ScalarParameters)
	{
		if (Param.ParameterName == ParamFName)
		{
			ParamGuid = Param.Id;
			bFound = true;
			break;
		}
	}

	// Check vector parameters if not found
	if (!bFound)
	{
		for (const FCollectionVectorParameter& Param : Collection->VectorParameters)
		{
			if (Param.ParameterName == ParamFName)
			{
				ParamGuid = Param.Id;
				bFound = true;
				break;
			}
		}
	}

	if (bFound)
	{
		CollParam->ParameterId = ParamGuid;
		CollParam->ParameterName = ParamFName;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::CreateCollectionParameter: Parameter '%s' not found in collection '%s'"),
			*ParameterName, *CollectionPath);
	}

	RefreshMaterialGraph(Material);

	return BuildExpressionInfo(CollParam);
}

// =================================================================
// Information Actions
// =================================================================




// =================================================================
// Connection Actions
// =================================================================


// =================================================================
// Property Actions
// =================================================================




// =================================================================
// Parameter Actions
// =================================================================




// =================================================================
// Batch Operations
// =================================================================





// =================================================================
// Export Actions
// =================================================================




// =================================================================
// Material Output Actions
// =================================================================





// =================================================================
// Material Function Helpers
// =================================================================

UMaterialFunction* UMaterialNodeService::LoadMaterialFunctionAsset(const FString& FunctionPath)
{
	UObject* LoadedObj = UEditorAssetLibrary::LoadAsset(FunctionPath);
	if (!LoadedObj)
	{
		// Try direct load
		LoadedObj = LoadObject<UMaterialFunction>(nullptr, *FunctionPath);
	}
	if (!LoadedObj)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService: Failed to load material function: %s"), *FunctionPath);
		return nullptr;
	}

	UMaterialFunction* Function = Cast<UMaterialFunction>(LoadedObj);
	if (!Function)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService: Object is not a material function: %s (is %s)"),
			*FunctionPath, *LoadedObj->GetClass()->GetName());
		return nullptr;
	}

	return Function;
}

UMaterialExpression* UMaterialNodeService::FindExpressionInFunctionById(UMaterialFunction* Function, const FString& ExpressionId)
{
	if (!Function) return nullptr;

	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Function->GetExpressions();
	for (UMaterialExpression* Expr : Expressions)
	{
		if (!Expr) continue;
		if (GetExpressionId(Expr) == ExpressionId)
		{
			return Expr;
		}
	}

	// Try matching by index — but ONLY when the id is genuinely numeric. FCString::Atoi
	// returns 0 for any non-numeric string, so without this guard a stale or unknown id
	// (e.g. "MaterialExpressionConstant_0x...") silently resolved to expression index 0 and
	// the caller mutated/deleted the wrong node. Require an all-digit id for the fallback.
	if (ExpressionId.IsNumeric())
	{
		int32 Index = FCString::Atoi(*ExpressionId);
		if (Index >= 0 && Index < Expressions.Num())
		{
			return Expressions[Index];
		}
	}

	return nullptr;
}

FString UMaterialNodeService::FunctionInputTypeToString(int32 InputType)
{
	switch (InputType)
	{
	case 0: return TEXT("Scalar");
	case 1: return TEXT("Vector2");
	case 2: return TEXT("Vector3");
	case 3: return TEXT("Vector4");
	case 4: return TEXT("Texture2D");
	case 5: return TEXT("TextureCube");
	case 6: return TEXT("Texture2DArray");
	case 7: return TEXT("VolumeTexture");
	case 8: return TEXT("StaticBool");
	case 9: return TEXT("MaterialAttributes");
	case 10: return TEXT("TextureExternal");
	default: return FString::Printf(TEXT("Unknown_%d"), InputType);
	}
}

int32 UMaterialNodeService::StringToFunctionInputType(const FString& TypeName)
{
	FString Upper = TypeName.ToUpper();
	if (Upper == TEXT("SCALAR") || Upper == TEXT("FLOAT") || Upper == TEXT("FLOAT1")) return 0;
	if (Upper == TEXT("VECTOR2") || Upper == TEXT("FLOAT2")) return 1;
	if (Upper == TEXT("VECTOR3") || Upper == TEXT("FLOAT3")) return 2;
	if (Upper == TEXT("VECTOR4") || Upper == TEXT("FLOAT4")) return 3;
	if (Upper == TEXT("TEXTURE2D") || Upper == TEXT("TEXTURE")) return 4;
	if (Upper == TEXT("TEXTURECUBE") || Upper == TEXT("CUBEMAP")) return 5;
	if (Upper == TEXT("TEXTURE2DARRAY")) return 6;
	if (Upper == TEXT("VOLUMETEXTURE")) return 7;
	if (Upper == TEXT("STATICBOOL") || Upper == TEXT("BOOL")) return 8;
	if (Upper == TEXT("MATERIALATTRIBUTES")) return 9;
	if (Upper == TEXT("TEXTUREEXTERNAL") || Upper == TEXT("EXTERNAL")) return 10;

	UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService: Unknown function input type: %s, defaulting to Vector3"), *TypeName);
	return 2; // Default to Vector3
}

// =================================================================
// Material Function Actions
// =================================================================









// =================================================================
// Cleanup (graph reachability — "Clean Graph -> Clean Up")
// =================================================================
//
// Note: single-node deletion (for both Material and MaterialFunction graphs) is provided by
// Epic's native MaterialTools.delete_expression, so VibeUE does not duplicate it. This covers
// only the gap Epic leaves: cleaning *unused* nodes out of a MaterialFunction (Epic's
// delete_unused_expressions is material-only).

