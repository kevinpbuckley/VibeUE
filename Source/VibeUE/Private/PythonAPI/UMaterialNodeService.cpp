// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UMaterialNodeService.h"
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
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialParameterCollection.h"
#include "LandscapeGrassType.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialEditingLibrary.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectIterator.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Texture2D.h"

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
	if (!Material) return nullptr;
	
	TArray<UMaterialExpression*> Expressions;
	Material->GetAllExpressionsInMaterialAndFunctionsOfType<UMaterialExpression>(Expressions);
	
	for (UMaterialExpression* Expression : Expressions)
	{
		if (GetExpressionId(Expression) == ExpressionId)
		{
			return Expression;
		}
	}
	
	// Try matching by index
	int32 Index = FCString::Atoi(*ExpressionId);
	if (Index >= 0 && Index < Expressions.Num())
	{
		return Expressions[Index];
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
	
	int32 Index = FCString::Atoi(*OutputName);
	if (Index >= 0 && Index < Outputs.Num())
	{
		return Index;
	}
	
	return 0;
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

EMaterialProperty UMaterialNodeService::StringToMaterialProperty(const FString& PropertyName)
{
	static TMap<FString, EMaterialProperty> PropertyMap = {
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
	
	EMaterialProperty* Found = PropertyMap.Find(PropertyName);
	return Found ? *Found : MP_BaseColor;
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

TArray<FMaterialExpressionTypeInfo> UMaterialNodeService::DiscoverTypes(
	const FString& Category,
	const FString& SearchTerm,
	int32 MaxResults)
{
	TArray<FMaterialExpressionTypeInfo> Results;
	
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class->IsChildOf(UMaterialExpression::StaticClass()))
		{
			continue;
		}
		
		if (Class->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}
		
		if (Class == UMaterialExpression::StaticClass())
		{
			continue;
		}
		
		UMaterialExpression* CDO = Class->GetDefaultObject<UMaterialExpression>();
		if (!CDO)
		{
			continue;
		}
		
		FMaterialExpressionTypeInfo TypeInfo;
		TypeInfo.ClassName = Class->GetName();
		TypeInfo.DisplayName = Class->GetName().Replace(TEXT("MaterialExpression"), TEXT(""));
		
		const FString* CategoryMeta = Class->FindMetaData(TEXT("Category"));
		TypeInfo.Category = CategoryMeta ? *CategoryMeta : TEXT("Misc");
		
		const FString* TooltipMeta = Class->FindMetaData(TEXT("ToolTip"));
		TypeInfo.Description = TooltipMeta ? *TooltipMeta : TEXT("");
		
		TypeInfo.bIsParameter = Class->IsChildOf(UMaterialExpressionParameter::StaticClass());
		
		// Apply filters
		if (!Category.IsEmpty() && !TypeInfo.Category.Contains(Category, ESearchCase::IgnoreCase))
		{
			continue;
		}
		
		if (!SearchTerm.IsEmpty())
		{
			bool bMatch = TypeInfo.ClassName.Contains(SearchTerm, ESearchCase::IgnoreCase) ||
						 TypeInfo.DisplayName.Contains(SearchTerm, ESearchCase::IgnoreCase) ||
						 TypeInfo.Category.Contains(SearchTerm, ESearchCase::IgnoreCase) ||
						 TypeInfo.Description.Contains(SearchTerm, ESearchCase::IgnoreCase);
			if (!bMatch)
			{
				continue;
			}
		}
		
		Results.Add(TypeInfo);
		
		if (Results.Num() >= MaxResults)
		{
			break;
		}
	}
	
	Results.Sort([](const FMaterialExpressionTypeInfo& A, const FMaterialExpressionTypeInfo& B) {
		if (A.Category != B.Category)
		{
			return A.Category < B.Category;
		}
		return A.DisplayName < B.DisplayName;
	});
	
	return Results;
}

TArray<FString> UMaterialNodeService::GetCategories()
{
	TSet<FString> Categories;
	
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class->IsChildOf(UMaterialExpression::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}
		
		const FString* CategoryMeta = Class->FindMetaData(TEXT("Category"));
		if (CategoryMeta && !CategoryMeta->IsEmpty())
		{
			Categories.Add(*CategoryMeta);
		}
	}
	
	TArray<FString> Result = Categories.Array();
	Result.Sort();
	return Result;
}

// =================================================================
// Lifecycle Actions
// =================================================================

FMaterialExpressionInfo UMaterialNodeService::CreateExpression(
	const FString& MaterialPath,
	const FString& ExpressionClass,
	int32 PosX,
	int32 PosY)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return FMaterialExpressionInfo();
	}
	
	UClass* ExpClass = ResolveExpressionClass(ExpressionClass);
	if (!ExpClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::CreateExpression: Unknown class: %s"), *ExpressionClass);
		return FMaterialExpressionInfo();
	}
	
	FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Create Material Expression", "Create Material Expression"));
	Material->Modify();
	
	UMaterialExpression* NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
		Material, ExpClass, PosX, PosY);
	
	if (!NewExpression)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::CreateExpression: Failed to create expression"));
		return FMaterialExpressionInfo();
	}
	
	RefreshMaterialGraph(Material);
	
	return BuildExpressionInfo(NewExpression);
}

bool UMaterialNodeService::DeleteExpression(const FString& MaterialPath, const FString& ExpressionId)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return false;
	}
	
	UMaterialExpression* Expression = FindExpressionById(Material, ExpressionId);
	if (!Expression)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::DeleteExpression: Expression not found: %s"), *ExpressionId);
		return false;
	}
	
	FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Delete Material Expression", "Delete Material Expression"));
	Material->Modify();
	
	UMaterialEditingLibrary::DeleteMaterialExpression(Material, Expression);
	
	RefreshMaterialGraph(Material);
	
	return true;
}

bool UMaterialNodeService::MoveExpression(
	const FString& MaterialPath,
	const FString& ExpressionId,
	int32 PosX,
	int32 PosY)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return false;
	}
	
	UMaterialExpression* Expression = FindExpressionById(Material, ExpressionId);
	if (!Expression)
	{
		return false;
	}
	
	FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Move Material Expression", "Move Material Expression"));
	Expression->Modify();
	
	Expression->MaterialExpressionEditorX = PosX;
	Expression->MaterialExpressionEditorY = PosY;
	
	RefreshMaterialGraph(Material);
	
	return true;
}

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

FMaterialExpressionInfo UMaterialNodeService::CreateCustomExpression(
	const FString& MaterialPath,
	const FString& Code,
	const FString& OutputType,
	const FString& Description,
	const FString& InputNames,
	int32 PosX,
	int32 PosY)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return FMaterialExpressionInfo();
	}

	FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Create Custom Expression", "Create Custom Expression"));
	Material->Modify();

	UMaterialExpressionCustom* CustomExpr = Cast<UMaterialExpressionCustom>(
		UMaterialEditingLibrary::CreateMaterialExpression(
			Material, UMaterialExpressionCustom::StaticClass(), PosX, PosY));

	if (!CustomExpr)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::CreateCustomExpression: Failed to create expression"));
		return FMaterialExpressionInfo();
	}

	// Set HLSL code
	CustomExpr->Code = Code;
	CustomExpr->Description = Description;

	// Parse output type enum
	FString TypeUpper = OutputType.ToUpper();
	if (TypeUpper == TEXT("CMOT_FLOAT1") || TypeUpper == TEXT("FLOAT") || TypeUpper == TEXT("FLOAT1"))
	{
		CustomExpr->OutputType = CMOT_Float1;
	}
	else if (TypeUpper == TEXT("CMOT_FLOAT2") || TypeUpper == TEXT("FLOAT2"))
	{
		CustomExpr->OutputType = CMOT_Float2;
	}
	else if (TypeUpper == TEXT("CMOT_FLOAT3") || TypeUpper == TEXT("FLOAT3"))
	{
		CustomExpr->OutputType = CMOT_Float3;
	}
	else if (TypeUpper == TEXT("CMOT_FLOAT4") || TypeUpper == TEXT("FLOAT4"))
	{
		CustomExpr->OutputType = CMOT_Float4;
	}
	else if (TypeUpper == TEXT("CMOT_MATERIALATTRIBUTES") || TypeUpper == TEXT("MATERIALATTRIBUTES"))
	{
		CustomExpr->OutputType = CMOT_MaterialAttributes;
	}

	// Set up custom inputs
	if (!InputNames.IsEmpty())
	{
		TArray<FString> Names;
		InputNames.ParseIntoArray(Names, TEXT(","), true);

		CustomExpr->Inputs.Empty();
		for (const FString& Name : Names)
		{
			FCustomInput NewInput;
			NewInput.InputName = FName(*Name.TrimStartAndEnd());
			CustomExpr->Inputs.Add(NewInput);
		}
	}

	RefreshMaterialGraph(Material);

	return BuildExpressionInfo(CustomExpr);
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

TArray<FMaterialExpressionInfo> UMaterialNodeService::ListExpressions(const FString& MaterialPath)
{
	TArray<FMaterialExpressionInfo> Results;
	
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return Results;
	}
	
	TArray<UMaterialExpression*> Expressions;
	Material->GetAllExpressionsInMaterialAndFunctionsOfType<UMaterialExpression>(Expressions);
	
	for (UMaterialExpression* Expression : Expressions)
	{
		if (Expression)
		{
			Results.Add(BuildExpressionInfo(Expression));
		}
	}
	
	return Results;
}

bool UMaterialNodeService::GetExpressionDetails(
	const FString& MaterialPath,
	const FString& ExpressionId,
	FMaterialExpressionInfo& OutInfo)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return false;
	}
	
	UMaterialExpression* Expression = FindExpressionById(Material, ExpressionId);
	if (!Expression)
	{
		return false;
	}
	
	OutInfo = BuildExpressionInfo(Expression);
	return true;
}

TArray<FMaterialNodePinInfo> UMaterialNodeService::GetExpressionPins(
	const FString& MaterialPath,
	const FString& ExpressionId)
{
	TArray<FMaterialNodePinInfo> Pins;
	
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return Pins;
	}
	
	UMaterialExpression* Expression = FindExpressionById(Material, ExpressionId);
	if (!Expression)
	{
		return Pins;
	}
	
	// Get inputs
	for (int32 i = 0; ; i++)
	{
		FExpressionInput* Input = Expression->GetInput(i);
		if (!Input) break;
		FMaterialNodePinInfo PinInfo;
		PinInfo.Name = Expression->GetInputName(i).ToString();
		if (PinInfo.Name.IsEmpty())
		{
			PinInfo.Name = FString::Printf(TEXT("Input_%d"), i);
		}
		PinInfo.Index = i;
		PinInfo.Direction = TEXT("Input");
		PinInfo.bIsConnected = Input->Expression != nullptr;
		if (PinInfo.bIsConnected)
		{
			PinInfo.ConnectedExpressionId = GetExpressionId(Input->Expression);
			PinInfo.ConnectedOutputIndex = Input->OutputIndex;
		}
		Pins.Add(PinInfo);
	}
	
	// Get outputs
	TArray<FExpressionOutput>& Outputs = Expression->GetOutputs();
	for (int32 i = 0; i < Outputs.Num(); i++)
	{
		FMaterialNodePinInfo PinInfo;
		PinInfo.Name = Outputs[i].OutputName.IsNone() ? FString::Printf(TEXT("Output_%d"), i) : Outputs[i].OutputName.ToString();
		PinInfo.Index = i;
		PinInfo.Direction = TEXT("Output");
		PinInfo.bIsConnected = false;
		Pins.Add(PinInfo);
	}
	
	return Pins;
}

// =================================================================
// Connection Actions
// =================================================================

bool UMaterialNodeService::ConnectExpressions(
	const FString& MaterialPath,
	const FString& SourceExpressionId,
	const FString& SourceOutput,
	const FString& TargetExpressionId,
	const FString& TargetInput)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return false;
	}
	
	UMaterialExpression* SourceExpr = FindExpressionById(Material, SourceExpressionId);
	if (!SourceExpr)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::ConnectExpressions: Source expression not found: %s"), *SourceExpressionId);
		return false;
	}
	
	UMaterialExpression* TargetExpr = FindExpressionById(Material, TargetExpressionId);
	if (!TargetExpr)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::ConnectExpressions: Target expression not found: %s"), *TargetExpressionId);
		return false;
	}
	
	int32 OutputIndex = FindOutputIndexByName(SourceExpr, SourceOutput);
	if (OutputIndex < 0)
	{
		OutputIndex = 0;
	}
	
	FExpressionInput* InputPtr = FindInputByName(TargetExpr, TargetInput);
	if (!InputPtr)
	{
		TArray<FString> ValidInputs = GetExpressionInputNames(TargetExpr);
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::ConnectExpressions: Input '%s' not found. Valid inputs: %s"),
			*TargetInput, *FString::Join(ValidInputs, TEXT(", ")));
		return false;
	}
	
	FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Connect Material Expressions", "Connect Material Expressions"));
	Material->Modify();
	
	InputPtr->Connect(OutputIndex, SourceExpr);
	
	RefreshMaterialGraph(Material);
	
	return true;
}

bool UMaterialNodeService::DisconnectInput(
	const FString& MaterialPath,
	const FString& ExpressionId,
	const FString& InputName)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return false;
	}
	
	UMaterialExpression* Expression = FindExpressionById(Material, ExpressionId);
	if (!Expression)
	{
		return false;
	}
	
	FExpressionInput* Input = FindInputByName(Expression, InputName);
	if (!Input)
	{
		return false;
	}
	
	FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Disconnect Material Input", "Disconnect Material Input"));
	Material->Modify();
	
	Input->Expression = nullptr;
	Input->OutputIndex = 0;
	
	RefreshMaterialGraph(Material);
	
	return true;
}

TArray<FMaterialNodeConnectionInfo> UMaterialNodeService::ListConnections(const FString& MaterialPath)
{
	TArray<FMaterialNodeConnectionInfo> Connections;
	
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return Connections;
	}
	
	TArray<UMaterialExpression*> Expressions;
	Material->GetAllExpressionsInMaterialAndFunctionsOfType<UMaterialExpression>(Expressions);
	
	for (UMaterialExpression* Expression : Expressions)
	{
		if (!Expression) continue;
		
		for (int32 i = 0; ; i++)
		{
			FExpressionInput* Input = Expression->GetInput(i);
			if (!Input) break;
			if (Input->Expression)
			{
				FMaterialNodeConnectionInfo ConnInfo;
				ConnInfo.SourceExpressionId = GetExpressionId(Input->Expression);
				ConnInfo.SourceOutput = FString::Printf(TEXT("%d"), Input->OutputIndex);
				ConnInfo.TargetExpressionId = GetExpressionId(Expression);
				ConnInfo.TargetInput = Expression->GetInputName(i).ToString();
				if (ConnInfo.TargetInput.IsEmpty())
				{
					ConnInfo.TargetInput = FString::Printf(TEXT("Input_%d"), i);
				}
				Connections.Add(ConnInfo);
			}
		}
	}
	
	return Connections;
}

bool UMaterialNodeService::ConnectToOutput(
	const FString& MaterialPath,
	const FString& ExpressionId,
	const FString& OutputName,
	const FString& MaterialProperty)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return false;
	}
	
	UMaterialExpression* Expression = FindExpressionById(Material, ExpressionId);
	if (!Expression)
	{
		return false;
	}
	
	FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Connect to Material Property", "Connect to Material Property"));
	Material->Modify();
	
	FString NormalizedOutputName = OutputName;
	if (NormalizedOutputName.StartsWith(TEXT("Output_")))
	{
		NormalizedOutputName = TEXT("");
	}
	
	bool bSuccess = UMaterialEditingLibrary::ConnectMaterialProperty(
		Expression,
		NormalizedOutputName,
		StringToMaterialProperty(MaterialProperty)
	);
	
	if (bSuccess)
	{
		RefreshMaterialGraph(Material);
	}
	
	return bSuccess;
}

bool UMaterialNodeService::DisconnectOutput(
	const FString& MaterialPath,
	const FString& MaterialProperty)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return false;
	}
	
	FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Disconnect Material Property", "Disconnect Material Property"));
	Material->Modify();
	
	EMaterialProperty PropEnum = StringToMaterialProperty(MaterialProperty);
	
	FExpressionInput* PropertyInput = Material->GetExpressionInputForProperty(PropEnum);
	if (PropertyInput)
	{
		PropertyInput->Expression = nullptr;
		PropertyInput->OutputIndex = 0;
	}
	
	RefreshMaterialGraph(Material);
	
	return true;
}

// =================================================================
// Property Actions
// =================================================================

FString UMaterialNodeService::GetExpressionProperty(
	const FString& MaterialPath,
	const FString& ExpressionId,
	const FString& PropertyName)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return FString();
	}
	
	UMaterialExpression* Expression = FindExpressionById(Material, ExpressionId);
	if (!Expression)
	{
		return FString();
	}
	
	FProperty* Property = Expression->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		return FString();
	}
	
	FString Value;
	Property->ExportTextItem_Direct(Value, Property->ContainerPtrToValuePtr<void>(Expression), nullptr, Expression, PPF_None);
	
	return Value;
}

bool UMaterialNodeService::SetExpressionProperty(
	const FString& MaterialPath,
	const FString& ExpressionId,
	const FString& PropertyName,
	const FString& PropertyValue)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return false;
	}
	
	UMaterialExpression* Expression = FindExpressionById(Material, ExpressionId);
	if (!Expression)
	{
		return false;
	}
	
	FProperty* Property = Expression->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		return false;
	}
	
	FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Set Material Expression Property", "Set Material Expression Property"));
	Expression->Modify();
	
	void* PropertyPtr = Property->ContainerPtrToValuePtr<void>(Expression);
	
	// Handle FLinearColor
	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (StructProp->Struct->GetName() == TEXT("LinearColor"))
		{
			FLinearColor Color;
			if (Color.InitFromString(PropertyValue))
			{
				FLinearColor* ColorPtr = static_cast<FLinearColor*>(PropertyPtr);
				*ColorPtr = Color;
				RefreshMaterialGraph(Material);
				return true;
			}
		}
	}
	
	// Standard import
	Property->ImportText_Direct(*PropertyValue, PropertyPtr, Expression, PPF_None);
	
	RefreshMaterialGraph(Material);
	
	return true;
}

TArray<FMaterialNodePropertyInfo> UMaterialNodeService::ListExpressionProperties(
	const FString& MaterialPath,
	const FString& ExpressionId)
{
	TArray<FMaterialNodePropertyInfo> Properties;
	
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return Properties;
	}
	
	UMaterialExpression* Expression = FindExpressionById(Material, ExpressionId);
	if (!Expression)
	{
		return Properties;
	}
	
	for (TFieldIterator<FProperty> PropIt(Expression->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		
		if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient))
		{
			continue;
		}
		
		if (!Property->HasAnyPropertyFlags(CPF_Edit))
		{
			continue;
		}
		
		FMaterialNodePropertyInfo PropInfo;
		PropInfo.Name = Property->GetName();
		PropInfo.PropertyType = Property->GetCPPType();
		PropInfo.bIsEditable = true;
		
		Property->ExportTextItem_Direct(PropInfo.Value, Property->ContainerPtrToValuePtr<void>(Expression), nullptr, Expression, PPF_None);
		
		Properties.Add(PropInfo);
	}
	
	return Properties;
}

// =================================================================
// Parameter Actions
// =================================================================

FMaterialExpressionInfo UMaterialNodeService::CreateParameter(
	const FString& MaterialPath,
	const FString& ParameterType,
	const FString& ParameterName,
	const FString& GroupName,
	const FString& DefaultValue,
	int32 PosX,
	int32 PosY)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return FMaterialExpressionInfo();
	}
	
	FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Create Material Parameter", "Create Material Parameter"));
	Material->Modify();
	
	UMaterialExpression* NewExpression = nullptr;
	FString TypeLower = ParameterType.ToLower();
	
	if (TypeLower == TEXT("scalar") || TypeLower == TEXT("float"))
	{
		UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionScalarParameter::StaticClass(), PosX, PosY)
		);
		if (ScalarParam)
		{
			ScalarParam->ParameterName = FName(*ParameterName);
			if (!DefaultValue.IsEmpty())
			{
				ScalarParam->DefaultValue = FCString::Atof(*DefaultValue);
			}
			if (!GroupName.IsEmpty())
			{
				ScalarParam->Group = FName(*GroupName);
			}
			NewExpression = ScalarParam;
		}
	}
	else if (TypeLower == TEXT("vector") || TypeLower == TEXT("color"))
	{
		UMaterialExpressionVectorParameter* VecParam = Cast<UMaterialExpressionVectorParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionVectorParameter::StaticClass(), PosX, PosY)
		);
		if (VecParam)
		{
			VecParam->ParameterName = FName(*ParameterName);
			if (!DefaultValue.IsEmpty())
			{
				FLinearColor Color;
				bool bParsed = Color.InitFromString(DefaultValue);

				// Support comma-separated formats: "R,G,B" or "R,G,B,A"
				if (!bParsed)
				{
					TArray<FString> Parts;
					DefaultValue.ParseIntoArray(Parts, TEXT(","), true);
					if (Parts.Num() >= 3)
					{
						Color.R = FCString::Atof(*Parts[0].TrimStartAndEnd());
						Color.G = FCString::Atof(*Parts[1].TrimStartAndEnd());
						Color.B = FCString::Atof(*Parts[2].TrimStartAndEnd());
						Color.A = Parts.Num() >= 4 ? FCString::Atof(*Parts[3].TrimStartAndEnd()) : 1.0f;
						bParsed = true;
					}
				}

				// Support slash-separated: "R/G/B"
				if (!bParsed)
				{
					TArray<FString> Parts;
					DefaultValue.ParseIntoArray(Parts, TEXT("/"), true);
					if (Parts.Num() >= 3)
					{
						Color.R = FCString::Atof(*Parts[0].TrimStartAndEnd());
						Color.G = FCString::Atof(*Parts[1].TrimStartAndEnd());
						Color.B = FCString::Atof(*Parts[2].TrimStartAndEnd());
						Color.A = Parts.Num() >= 4 ? FCString::Atof(*Parts[3].TrimStartAndEnd()) : 1.0f;
						bParsed = true;
					}
				}

				if (bParsed)
				{
					VecParam->DefaultValue = Color;
					UE_LOG(LogTemp, Log, TEXT("UMaterialNodeService::CreateParameter: Set '%s' default to (R=%.3f,G=%.3f,B=%.3f,A=%.3f)"),
						*ParameterName, Color.R, Color.G, Color.B, Color.A);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::CreateParameter: Could not parse default_value '%s' for Vector parameter '%s'. "
						"Accepted formats: '(R=1.0,G=0.0,B=0.0,A=1.0)' or '1.0,0.0,0.0' or '1.0,0.0,0.0,1.0' or '1.0/0.0/0.0'. "
						"Parameter will use black (0,0,0,0)."),
						*DefaultValue, *ParameterName);
				}
			}
			if (!GroupName.IsEmpty())
			{
				VecParam->Group = FName(*GroupName);
			}
			NewExpression = VecParam;
		}
	}
	else if (TypeLower == TEXT("texture") || TypeLower == TEXT("texture2d"))
	{
		UMaterialExpressionTextureSampleParameter2D* TexParam = Cast<UMaterialExpressionTextureSampleParameter2D>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), PosX, PosY)
		);
		if (TexParam)
		{
			TexParam->ParameterName = FName(*ParameterName);
			if (!DefaultValue.IsEmpty())
			{
				UTexture* Texture = LoadObject<UTexture>(nullptr, *DefaultValue);
				if (!Texture)
				{
					UObject* TexObj = UEditorAssetLibrary::LoadAsset(DefaultValue);
					Texture = Cast<UTexture>(TexObj);
				}
				if (Texture)
				{
					TexParam->Texture = Texture;
				}
			}
			if (!GroupName.IsEmpty())
			{
				TexParam->Group = FName(*GroupName);
			}
			NewExpression = TexParam;
		}
	}
	else if (TypeLower == TEXT("textureobject") || TypeLower == TEXT("texture_object"))
	{
		UMaterialExpressionTextureObjectParameter* TexObjParam = Cast<UMaterialExpressionTextureObjectParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionTextureObjectParameter::StaticClass(), PosX, PosY)
		);
		if (TexObjParam)
		{
			TexObjParam->ParameterName = FName(*ParameterName);
			if (!DefaultValue.IsEmpty())
			{
				UTexture* Texture = LoadObject<UTexture>(nullptr, *DefaultValue);
				if (!Texture)
				{
					UObject* TexObj = UEditorAssetLibrary::LoadAsset(DefaultValue);
					Texture = Cast<UTexture>(TexObj);
				}
				if (Texture)
				{
					TexObjParam->Texture = Texture;
				}
			}
			if (!GroupName.IsEmpty())
			{
				TexObjParam->Group = FName(*GroupName);
			}
			NewExpression = TexObjParam;
		}
	}
	else if (TypeLower == TEXT("staticbool") || TypeLower == TEXT("bool"))
	{
		UMaterialExpressionStaticBoolParameter* BoolParam = Cast<UMaterialExpressionStaticBoolParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionStaticBoolParameter::StaticClass(), PosX, PosY)
		);
		if (BoolParam)
		{
			BoolParam->ParameterName = FName(*ParameterName);
			if (!DefaultValue.IsEmpty())
			{
				BoolParam->DefaultValue = DefaultValue.ToBool();
			}
			if (!GroupName.IsEmpty())
			{
				BoolParam->Group = FName(*GroupName);
			}
			NewExpression = BoolParam;
		}
	}
	else if (TypeLower == TEXT("staticswitch") || TypeLower == TEXT("switch"))
	{
		UMaterialExpressionStaticSwitchParameter* SwitchParam = Cast<UMaterialExpressionStaticSwitchParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionStaticSwitchParameter::StaticClass(), PosX, PosY)
		);
		if (SwitchParam)
		{
			SwitchParam->ParameterName = FName(*ParameterName);
			if (!DefaultValue.IsEmpty())
			{
				SwitchParam->DefaultValue = DefaultValue.ToBool();
			}
			if (!GroupName.IsEmpty())
			{
				SwitchParam->Group = FName(*GroupName);
			}
			NewExpression = SwitchParam;
		}
	}
	
	if (!NewExpression)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::CreateParameter: Unknown type: %s"), *ParameterType);
		return FMaterialExpressionInfo();
	}
	
	RefreshMaterialGraph(Material);
	
	return BuildExpressionInfo(NewExpression);
}

FMaterialExpressionInfo UMaterialNodeService::PromoteToParameter(
	const FString& MaterialPath,
	const FString& ExpressionId,
	const FString& ParameterName,
	const FString& GroupName)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return FMaterialExpressionInfo();
	}
	
	UMaterialExpression* OldExpression = FindExpressionById(Material, ExpressionId);
	if (!OldExpression)
	{
		return FMaterialExpressionInfo();
	}
	
	FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Promote to Parameter", "Promote to Parameter"));
	Material->Modify();
	
	UMaterialExpression* NewExpression = nullptr;
	int32 PosX = OldExpression->MaterialExpressionEditorX;
	int32 PosY = OldExpression->MaterialExpressionEditorY;
	
	if (UMaterialExpressionConstant* Const = Cast<UMaterialExpressionConstant>(OldExpression))
	{
		UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionScalarParameter::StaticClass(), PosX, PosY)
		);
		if (ScalarParam)
		{
			ScalarParam->ParameterName = FName(*ParameterName);
			ScalarParam->DefaultValue = Const->R;
			if (!GroupName.IsEmpty())
			{
				ScalarParam->Group = FName(*GroupName);
			}
			NewExpression = ScalarParam;
		}
	}
	else if (UMaterialExpressionConstant3Vector* Const3 = Cast<UMaterialExpressionConstant3Vector>(OldExpression))
	{
		UMaterialExpressionVectorParameter* VecParam = Cast<UMaterialExpressionVectorParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionVectorParameter::StaticClass(), PosX, PosY)
		);
		if (VecParam)
		{
			VecParam->ParameterName = FName(*ParameterName);
			VecParam->DefaultValue = FLinearColor(Const3->Constant.R, Const3->Constant.G, Const3->Constant.B, 1.0f);
			if (!GroupName.IsEmpty())
			{
				VecParam->Group = FName(*GroupName);
			}
			NewExpression = VecParam;
		}
	}
	else if (UMaterialExpressionConstant4Vector* Const4 = Cast<UMaterialExpressionConstant4Vector>(OldExpression))
	{
		UMaterialExpressionVectorParameter* VecParam = Cast<UMaterialExpressionVectorParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionVectorParameter::StaticClass(), PosX, PosY)
		);
		if (VecParam)
		{
			VecParam->ParameterName = FName(*ParameterName);
			VecParam->DefaultValue = Const4->Constant;
			if (!GroupName.IsEmpty())
			{
				VecParam->Group = FName(*GroupName);
			}
			NewExpression = VecParam;
		}
	}
	else if (UMaterialExpressionTextureSample* TexSample = Cast<UMaterialExpressionTextureSample>(OldExpression))
	{
		UMaterialExpressionTextureSampleParameter2D* TexParam = Cast<UMaterialExpressionTextureSampleParameter2D>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), PosX, PosY)
		);
		if (TexParam)
		{
			TexParam->ParameterName = FName(*ParameterName);
			TexParam->Texture = TexSample->Texture;
			if (!GroupName.IsEmpty())
			{
				TexParam->Group = FName(*GroupName);
			}
			NewExpression = TexParam;
		}
	}
	else if (UMaterialExpressionTextureObject* TexObj = Cast<UMaterialExpressionTextureObject>(OldExpression))
	{
		UMaterialExpressionTextureObjectParameter* TexParam = Cast<UMaterialExpressionTextureObjectParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionTextureObjectParameter::StaticClass(), PosX, PosY)
		);
		if (TexParam)
		{
			TexParam->ParameterName = FName(*ParameterName);
			TexParam->Texture = TexObj->Texture;
			if (!GroupName.IsEmpty())
			{
				TexParam->Group = FName(*GroupName);
			}
			NewExpression = TexParam;
		}
	}
	
	if (!NewExpression)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::PromoteToParameter: Cannot promote type %s"), *OldExpression->GetClass()->GetName());
		return FMaterialExpressionInfo();
	}
	
	// Transfer connections
	TArray<UMaterialExpression*> AllExpressions;
	Material->GetAllExpressionsInMaterialAndFunctionsOfType<UMaterialExpression>(AllExpressions);
	
	for (UMaterialExpression* Expr : AllExpressions)
	{
		if (!Expr || Expr == OldExpression || Expr == NewExpression) continue;
		
		for (FExpressionInputIterator InputIt(Expr); InputIt; ++InputIt)
		{
			if (InputIt->Expression == OldExpression)
			{
				InputIt->Expression = NewExpression;
			}
		}
	}
	
	// Check material outputs
	for (int32 i = 0; i < MP_MAX; i++)
	{
		FExpressionInput* PropInput = Material->GetExpressionInputForProperty((EMaterialProperty)i);
		if (PropInput && PropInput->Expression == OldExpression)
		{
			PropInput->Expression = NewExpression;
		}
	}
	
	// Delete old expression
	UMaterialEditingLibrary::DeleteMaterialExpression(Material, OldExpression);
	
	RefreshMaterialGraph(Material);
	
	return BuildExpressionInfo(NewExpression);
}

bool UMaterialNodeService::SetParameterMetadata(
	const FString& MaterialPath,
	const FString& ExpressionId,
	const FString& GroupName,
	int32 SortPriority)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return false;
	}
	
	UMaterialExpression* Expression = FindExpressionById(Material, ExpressionId);
	if (!Expression)
	{
		return false;
	}
	
	UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(Expression);
	if (!ParamExpr)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::SetParameterMetadata: Expression is not a parameter"));
		return false;
	}
	
	FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Set Parameter Metadata", "Set Parameter Metadata"));
	ParamExpr->Modify();
	
	if (!GroupName.IsEmpty())
	{
		ParamExpr->Group = FName(*GroupName);
	}
	ParamExpr->SortPriority = SortPriority;
	
	RefreshMaterialGraph(Material);
	
	return true;
}

// =================================================================
// Batch Operations
// =================================================================

TArray<FMaterialExpressionInfo> UMaterialNodeService::BatchCreateExpressions(
	const FString& MaterialPath,
	const TArray<FString>& ExpressionClasses,
	const TArray<int32>& PosXArray,
	const TArray<int32>& PosYArray)
{
	TArray<FMaterialExpressionInfo> Results;

	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return Results;
	}

	int32 Count = ExpressionClasses.Num();
	if (PosXArray.Num() != Count || PosYArray.Num() != Count)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::BatchCreateExpressions: Array length mismatch: classes=%d, posX=%d, posY=%d"),
			Count, PosXArray.Num(), PosYArray.Num());
		return Results;
	}

	FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Batch Create Expressions", "Batch Create Expressions"));
	Material->Modify();

	for (int32 i = 0; i < Count; i++)
	{
		UClass* ExpClass = ResolveExpressionClass(ExpressionClasses[i]);
		if (!ExpClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::BatchCreateExpressions: Unknown class at index %d: %s"), i, *ExpressionClasses[i]);
			Results.Add(FMaterialExpressionInfo());
			continue;
		}

		UMaterialExpression* NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
			Material, ExpClass, PosXArray[i], PosYArray[i]);

		if (NewExpression)
		{
			Results.Add(BuildExpressionInfo(NewExpression));
		}
		else
		{
			Results.Add(FMaterialExpressionInfo());
		}
	}

	// Single refresh at end
	RefreshMaterialGraph(Material);

	UE_LOG(LogTemp, Log, TEXT("UMaterialNodeService::BatchCreateExpressions: Created %d/%d expressions"), Results.Num(), Count);
	return Results;
}

int32 UMaterialNodeService::BatchConnectExpressions(
	const FString& MaterialPath,
	const TArray<FString>& SourceIds,
	const TArray<FString>& SourceOutputs,
	const TArray<FString>& TargetIds,
	const TArray<FString>& TargetInputs)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return 0;
	}

	int32 Count = SourceIds.Num();
	if (SourceOutputs.Num() != Count || TargetIds.Num() != Count || TargetInputs.Num() != Count)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::BatchConnectExpressions: Array length mismatch"));
		return 0;
	}

	FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Batch Connect Expressions", "Batch Connect Expressions"));
	Material->Modify();

	int32 SuccessCount = 0;
	for (int32 i = 0; i < Count; i++)
	{
		UMaterialExpression* SourceExpr = FindExpressionById(Material, SourceIds[i]);
		UMaterialExpression* TargetExpr = FindExpressionById(Material, TargetIds[i]);

		if (!SourceExpr || !TargetExpr)
		{
			UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::BatchConnectExpressions: Expression not found at index %d"), i);
			continue;
		}

		int32 OutputIndex = FindOutputIndexByName(SourceExpr, SourceOutputs[i]);
		if (OutputIndex < 0) OutputIndex = 0;

		FExpressionInput* InputPtr = FindInputByName(TargetExpr, TargetInputs[i]);
		if (!InputPtr)
		{
			UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::BatchConnectExpressions: Input '%s' not found at index %d"), *TargetInputs[i], i);
			continue;
		}

		InputPtr->Connect(OutputIndex, SourceExpr);
		SuccessCount++;
	}

	// Single refresh at end
	RefreshMaterialGraph(Material);

	UE_LOG(LogTemp, Log, TEXT("UMaterialNodeService::BatchConnectExpressions: Connected %d/%d"), SuccessCount, Count);
	return SuccessCount;
}

int32 UMaterialNodeService::BatchSetProperties(
	const FString& MaterialPath,
	const TArray<FString>& ExpressionIds,
	const TArray<FString>& PropertyNames,
	const TArray<FString>& PropertyValues)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return 0;
	}

	int32 Count = ExpressionIds.Num();
	if (PropertyNames.Num() != Count || PropertyValues.Num() != Count)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::BatchSetProperties: Array length mismatch"));
		return 0;
	}

	FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Batch Set Properties", "Batch Set Properties"));
	Material->Modify();

	int32 SuccessCount = 0;
	for (int32 i = 0; i < Count; i++)
	{
		UMaterialExpression* Expression = FindExpressionById(Material, ExpressionIds[i]);
		if (!Expression)
		{
			continue;
		}

		FProperty* Property = Expression->GetClass()->FindPropertyByName(FName(*PropertyNames[i]));
		if (!Property)
		{
			continue;
		}

		Expression->Modify();
		void* PropertyPtr = Property->ContainerPtrToValuePtr<void>(Expression);

		// Handle FLinearColor
		if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			if (StructProp->Struct->GetName() == TEXT("LinearColor"))
			{
				FLinearColor Color;
				if (Color.InitFromString(PropertyValues[i]))
				{
					FLinearColor* ColorPtr = static_cast<FLinearColor*>(PropertyPtr);
					*ColorPtr = Color;
					SuccessCount++;
					continue;
				}
			}
		}

		Property->ImportText_Direct(*PropertyValues[i], PropertyPtr, Expression, PPF_None);
		SuccessCount++;
	}

	// Single refresh at end
	RefreshMaterialGraph(Material);

	UE_LOG(LogTemp, Log, TEXT("UMaterialNodeService::BatchSetProperties: Set %d/%d properties"), SuccessCount, Count);
	return SuccessCount;
}

TArray<FMaterialExpressionInfo> UMaterialNodeService::BatchCreateSpecialized(
	const FString& MaterialPath,
	const TArray<FBatchCreateDescriptor>& Descriptors)
{
	TArray<FMaterialExpressionInfo> Results;

	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return Results;
	}

	int32 Count = Descriptors.Num();
	if (Count == 0)
	{
		return Results;
	}

	FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Batch Create Specialized", "Batch Create Specialized"));
	Material->Modify();

	int32 SuccessCount = 0;
	for (int32 i = 0; i < Count; i++)
	{
		const FBatchCreateDescriptor& Desc = Descriptors[i];
		FString ClassUpper = Desc.ClassName.ToUpper();

		// MaterialFunctionCall — requires FunctionPath
		if (ClassUpper == TEXT("MATERIALFUNCTIONCALL") || ClassUpper == TEXT("FUNCTIONCALL"))
		{
			if (Desc.FunctionPath.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("BatchCreateSpecialized [%d]: MaterialFunctionCall requires FunctionPath"), i);
				Results.Add(FMaterialExpressionInfo());
				continue;
			}

			UMaterialFunction* Function = LoadObject<UMaterialFunction>(nullptr, *Desc.FunctionPath);
			if (!Function)
			{
				UObject* LoadedObj = UEditorAssetLibrary::LoadAsset(Desc.FunctionPath);
				Function = Cast<UMaterialFunction>(LoadedObj);
			}
			if (!Function)
			{
				UE_LOG(LogTemp, Warning, TEXT("BatchCreateSpecialized [%d]: Failed to load function: %s"), i, *Desc.FunctionPath);
				Results.Add(FMaterialExpressionInfo());
				continue;
			}

			UMaterialExpressionMaterialFunctionCall* FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(
				UMaterialEditingLibrary::CreateMaterialExpression(
					Material, UMaterialExpressionMaterialFunctionCall::StaticClass(), Desc.PosX, Desc.PosY));

			if (!FuncCall)
			{
				Results.Add(FMaterialExpressionInfo());
				continue;
			}

			FuncCall->SetMaterialFunction(Function);
			Results.Add(BuildExpressionInfo(FuncCall));
			SuccessCount++;
		}
		// Custom HLSL — requires HLSLCode
		else if (ClassUpper == TEXT("CUSTOM") || ClassUpper == TEXT("CUSTOMEXPRESSION"))
		{
			UMaterialExpressionCustom* CustomExpr = Cast<UMaterialExpressionCustom>(
				UMaterialEditingLibrary::CreateMaterialExpression(
					Material, UMaterialExpressionCustom::StaticClass(), Desc.PosX, Desc.PosY));

			if (!CustomExpr)
			{
				Results.Add(FMaterialExpressionInfo());
				continue;
			}

			CustomExpr->Code = Desc.HLSLCode;
			CustomExpr->Description = Desc.HLSLDescription;

			// Parse output type
			FString TypeUpper = Desc.HLSLOutputType.ToUpper();
			if (TypeUpper == TEXT("CMOT_FLOAT1") || TypeUpper == TEXT("FLOAT") || TypeUpper == TEXT("FLOAT1"))
			{
				CustomExpr->OutputType = CMOT_Float1;
			}
			else if (TypeUpper == TEXT("CMOT_FLOAT2") || TypeUpper == TEXT("FLOAT2"))
			{
				CustomExpr->OutputType = CMOT_Float2;
			}
			else if (TypeUpper == TEXT("CMOT_FLOAT3") || TypeUpper == TEXT("FLOAT3"))
			{
				CustomExpr->OutputType = CMOT_Float3;
			}
			else if (TypeUpper == TEXT("CMOT_FLOAT4") || TypeUpper == TEXT("FLOAT4"))
			{
				CustomExpr->OutputType = CMOT_Float4;
			}
			else if (TypeUpper == TEXT("CMOT_MATERIALATTRIBUTES") || TypeUpper == TEXT("MATERIALATTRIBUTES"))
			{
				CustomExpr->OutputType = CMOT_MaterialAttributes;
			}

			// Parse input names
			if (!Desc.HLSLInputNames.IsEmpty())
			{
				TArray<FString> Names;
				Desc.HLSLInputNames.ParseIntoArray(Names, TEXT(","), true);
				CustomExpr->Inputs.Empty();
				for (const FString& Name : Names)
				{
					FCustomInput NewInput;
					NewInput.InputName = FName(*Name.TrimStartAndEnd());
					CustomExpr->Inputs.Add(NewInput);
				}
			}

			// Parse additional outputs
			if (!Desc.HLSLAdditionalOutputs.IsEmpty())
			{
				TArray<FString> OutputDefs;
				Desc.HLSLAdditionalOutputs.ParseIntoArray(OutputDefs, TEXT(";"), true);
				for (const FString& OutputDef : OutputDefs)
				{
					FString OutputName, OutputTypeStr;
					if (OutputDef.Split(TEXT(":"), &OutputName, &OutputTypeStr))
					{
						FCustomOutput NewOutput;
						NewOutput.OutputName = FName(*OutputName.TrimStartAndEnd());
						FString OT = OutputTypeStr.TrimStartAndEnd().ToUpper();
						if (OT == TEXT("FLOAT1") || OT == TEXT("FLOAT"))
						{
							NewOutput.OutputType = ECustomMaterialOutputType::CMOT_Float1;
						}
						else if (OT == TEXT("FLOAT2"))
						{
							NewOutput.OutputType = ECustomMaterialOutputType::CMOT_Float2;
						}
						else if (OT == TEXT("FLOAT3"))
						{
							NewOutput.OutputType = ECustomMaterialOutputType::CMOT_Float3;
						}
						else if (OT == TEXT("FLOAT4"))
						{
							NewOutput.OutputType = ECustomMaterialOutputType::CMOT_Float4;
						}
						else
						{
							NewOutput.OutputType = ECustomMaterialOutputType::CMOT_Float3;
						}
						CustomExpr->AdditionalOutputs.Add(NewOutput);
					}
				}
			}

			Results.Add(BuildExpressionInfo(CustomExpr));
			SuccessCount++;
		}
		// CollectionParameter — requires CollectionPath
		else if (ClassUpper == TEXT("COLLECTIONPARAMETER"))
		{
			if (Desc.CollectionPath.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("BatchCreateSpecialized [%d]: CollectionParameter requires CollectionPath"), i);
				Results.Add(FMaterialExpressionInfo());
				continue;
			}

			UMaterialParameterCollection* Collection = LoadObject<UMaterialParameterCollection>(nullptr, *Desc.CollectionPath);
			if (!Collection)
			{
				UObject* LoadedObj = UEditorAssetLibrary::LoadAsset(Desc.CollectionPath);
				Collection = Cast<UMaterialParameterCollection>(LoadedObj);
			}
			if (!Collection)
			{
				UE_LOG(LogTemp, Warning, TEXT("BatchCreateSpecialized [%d]: Failed to load collection: %s"), i, *Desc.CollectionPath);
				Results.Add(FMaterialExpressionInfo());
				continue;
			}

			UMaterialExpressionCollectionParameter* CollParam = Cast<UMaterialExpressionCollectionParameter>(
				UMaterialEditingLibrary::CreateMaterialExpression(
					Material, UMaterialExpressionCollectionParameter::StaticClass(), Desc.PosX, Desc.PosY));

			if (!CollParam)
			{
				Results.Add(FMaterialExpressionInfo());
				continue;
			}

			CollParam->Collection = Collection;

			// Find and set the parameter by name
			if (!Desc.CollectionParamName.IsEmpty())
			{
				FName ParamFName(*Desc.CollectionParamName);
				FGuid ParamGuid;
				bool bFound = false;

				for (const FCollectionScalarParameter& Param : Collection->ScalarParameters)
				{
					if (Param.ParameterName == ParamFName)
					{
						ParamGuid = Param.Id;
						bFound = true;
						break;
					}
				}
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
					UE_LOG(LogTemp, Warning, TEXT("BatchCreateSpecialized [%d]: Parameter '%s' not found in collection"), i, *Desc.CollectionParamName);
				}
			}

			Results.Add(BuildExpressionInfo(CollParam));
			SuccessCount++;
		}
		// Generic expression — same as BatchCreateExpressions
		else
		{
			UClass* ExpClass = ResolveExpressionClass(Desc.ClassName);
			if (!ExpClass)
			{
				UE_LOG(LogTemp, Warning, TEXT("BatchCreateSpecialized [%d]: Unknown class: %s"), i, *Desc.ClassName);
				Results.Add(FMaterialExpressionInfo());
				continue;
			}

			UMaterialExpression* NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
				Material, ExpClass, Desc.PosX, Desc.PosY);

			if (NewExpression)
			{
				Results.Add(BuildExpressionInfo(NewExpression));
				SuccessCount++;
			}
			else
			{
				Results.Add(FMaterialExpressionInfo());
			}
		}
	}

	// Single refresh at end for all expressions
	RefreshMaterialGraph(Material);

	UE_LOG(LogTemp, Log, TEXT("UMaterialNodeService::BatchCreateSpecialized: Created %d/%d expressions"), SuccessCount, Count);
	return Results;
}

// =================================================================
// Export Actions
// =================================================================

FString UMaterialNodeService::ExportMaterialGraph(const FString& MaterialPath)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return FString();
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();

	// Material settings
	TSharedRef<FJsonObject> MatSettings = MakeShared<FJsonObject>();
	MatSettings->SetStringField(TEXT("blend_mode"), *UEnum::GetValueAsString(Material->BlendMode));
	MatSettings->SetStringField(TEXT("shading_model"), *UEnum::GetValueAsString(Material->GetShadingModels().GetFirstShadingModel()));
	MatSettings->SetBoolField(TEXT("two_sided"), Material->IsTwoSided());
	Root->SetObjectField(TEXT("material"), MatSettings);

	// All expressions
	TArray<TSharedPtr<FJsonValue>> ExpressionsArray;
	TArray<UMaterialExpression*> AllExpressions;
	Material->GetAllExpressionsInMaterialAndFunctionsOfType<UMaterialExpression>(AllExpressions);

	// Only include top-level expressions (not inside material functions)
	TArray<UMaterialExpression*> TopLevelExpressions;
	for (UMaterialExpression* Expr : AllExpressions)
	{
		if (Expr && Expr->GetOuter() == Material)
		{
			TopLevelExpressions.Add(Expr);
		}
	}

	for (UMaterialExpression* Expr : TopLevelExpressions)
	{
		if (!Expr) continue;

		TSharedRef<FJsonObject> ExprObj = MakeShared<FJsonObject>();
		ExprObj->SetStringField(TEXT("id"), GetExpressionId(Expr));
		ExprObj->SetStringField(TEXT("class"), Expr->GetClass()->GetName().Replace(TEXT("MaterialExpression"), TEXT("")));
		ExprObj->SetStringField(TEXT("class_full"), Expr->GetClass()->GetName());
		ExprObj->SetNumberField(TEXT("pos_x"), Expr->MaterialExpressionEditorX);
		ExprObj->SetNumberField(TEXT("pos_y"), Expr->MaterialExpressionEditorY);

		// Properties
		TSharedRef<FJsonObject> PropsObj = MakeShared<FJsonObject>();
		for (TFieldIterator<FProperty> PropIt(Expr->GetClass()); PropIt; ++PropIt)
		{
			FProperty* Prop = *PropIt;
			if (Prop->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient)) continue;
			if (!Prop->HasAnyPropertyFlags(CPF_Edit)) continue;

			FString Value;
			Prop->ExportTextItem_Direct(Value, Prop->ContainerPtrToValuePtr<void>(Expr), nullptr, Expr, PPF_None);
			if (!Value.IsEmpty())
			{
				PropsObj->SetStringField(Prop->GetName(), Value);
			}
		}
		ExprObj->SetObjectField(TEXT("properties"), PropsObj);

		// Parameter info
		if (UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(Expr))
		{
			ExprObj->SetBoolField(TEXT("is_parameter"), true);
			ExprObj->SetStringField(TEXT("parameter_name"), ParamExpr->ParameterName.ToString());
			ExprObj->SetStringField(TEXT("group"), ParamExpr->Group.ToString());
		}

		// Function call info
		if (UMaterialExpressionMaterialFunctionCall* FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expr))
		{
			if (FuncCall->MaterialFunction)
			{
				ExprObj->SetStringField(TEXT("function_path"), FuncCall->MaterialFunction->GetPathName());
			}
		}

		// Custom expression info (ISSUE-4: include custom_input_names)
		if (UMaterialExpressionCustom* CustomExpr = Cast<UMaterialExpressionCustom>(Expr))
		{
			ExprObj->SetStringField(TEXT("hlsl_code"), CustomExpr->Code);
			ExprObj->SetStringField(TEXT("output_type"), *UEnum::GetValueAsString(CustomExpr->OutputType));

			// Export custom input definitions for faithful recreation
			TArray<FString> CustomInputNames;
			for (const FCustomInput& CI : CustomExpr->Inputs)
			{
				CustomInputNames.Add(CI.InputName.ToString());
			}
			if (CustomInputNames.Num() > 0)
			{
				ExprObj->SetStringField(TEXT("custom_input_names"), FString::Join(CustomInputNames, TEXT(",")));
			}

			// Export additional outputs if defined
			TArray<TSharedPtr<FJsonValue>> AdditionalOutputsArr;
			for (const FCustomOutput& CO : CustomExpr->AdditionalOutputs)
			{
				TSharedRef<FJsonObject> OutObj = MakeShared<FJsonObject>();
				OutObj->SetStringField(TEXT("name"), CO.OutputName.ToString());
				OutObj->SetStringField(TEXT("type"), *UEnum::GetValueAsString(CO.OutputType));
				AdditionalOutputsArr.Add(MakeShared<FJsonValueObject>(OutObj));
			}
			if (AdditionalOutputsArr.Num() > 0)
			{
				ExprObj->SetArrayField(TEXT("custom_additional_outputs"), AdditionalOutputsArr);
			}
		}

		// Collection parameter info
		if (UMaterialExpressionCollectionParameter* CollParam = Cast<UMaterialExpressionCollectionParameter>(Expr))
		{
			if (CollParam->Collection)
			{
				ExprObj->SetStringField(TEXT("collection_path"), CollParam->Collection->GetPathName());
				ExprObj->SetStringField(TEXT("collection_parameter_name"), CollParam->ParameterName.ToString());
			}
		}

		// ISSUE-2: LandscapeLayerBlend layer configuration
		if (UMaterialExpressionLandscapeLayerBlend* LayerBlend = Cast<UMaterialExpressionLandscapeLayerBlend>(Expr))
		{
			TArray<TSharedPtr<FJsonValue>> LayersArr;
			for (const FLayerBlendInput& Layer : LayerBlend->Layers)
			{
				TSharedRef<FJsonObject> LayerObj = MakeShared<FJsonObject>();
				LayerObj->SetStringField(TEXT("layer_name"), Layer.LayerName.ToString());
				LayerObj->SetStringField(TEXT("blend_type"), *UEnum::GetValueAsString(Layer.BlendType));
				LayerObj->SetNumberField(TEXT("preview_weight"), Layer.PreviewWeight);
				LayersArr.Add(MakeShared<FJsonValueObject>(LayerObj));
			}
			ExprObj->SetArrayField(TEXT("landscape_layers"), LayersArr);
		}

		// ISSUE-9: LandscapeGrassOutput grass type mappings
		if (UMaterialExpressionLandscapeGrassOutput* GrassOut = Cast<UMaterialExpressionLandscapeGrassOutput>(Expr))
		{
			TArray<TSharedPtr<FJsonValue>> GrassArr;
			for (const FGrassInput& GI : GrassOut->GrassTypes)
			{
				TSharedRef<FJsonObject> GrassObj = MakeShared<FJsonObject>();
				GrassObj->SetStringField(TEXT("name"), GI.Name.ToString());
				if (GI.GrassType)
				{
					GrassObj->SetStringField(TEXT("grass_type_path"), GI.GrassType->GetPathName());
				}
				GrassArr.Add(MakeShared<FJsonValueObject>(GrassObj));
			}
			ExprObj->SetArrayField(TEXT("grass_types"), GrassArr);
		}

		// Inputs
		TArray<TSharedPtr<FJsonValue>> InputsArr;
		for (int32 i = 0; Expr->GetInput(i) != nullptr; i++)
		{
			FName InputName = Expr->GetInputName(i);
			FString InputStr = InputName.IsNone() ? FString::Printf(TEXT("Input_%d"), i) : InputName.ToString();
			InputsArr.Add(MakeShared<FJsonValueString>(InputStr));
		}
		ExprObj->SetArrayField(TEXT("inputs"), InputsArr);

		// Outputs
		TArray<TSharedPtr<FJsonValue>> OutputsArr;
		TArray<FExpressionOutput>& Outputs = Expr->GetOutputs();
		for (int32 i = 0; i < Outputs.Num(); i++)
		{
			FString OutputStr = Outputs[i].OutputName.IsNone() ? FString::Printf(TEXT("Output_%d"), i) : Outputs[i].OutputName.ToString();
			OutputsArr.Add(MakeShared<FJsonValueString>(OutputStr));
		}
		ExprObj->SetArrayField(TEXT("outputs"), OutputsArr);

		ExpressionsArray.Add(MakeShared<FJsonValueObject>(ExprObj));
	}
	Root->SetArrayField(TEXT("expressions"), ExpressionsArray);

	// Connections
	TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
	for (UMaterialExpression* Expr : TopLevelExpressions)
	{
		if (!Expr) continue;

		for (int32 i = 0; ; i++)
		{
			FExpressionInput* Input = Expr->GetInput(i);
			if (!Input) break;
			if (Input->Expression)
			{
				TSharedRef<FJsonObject> ConnObj = MakeShared<FJsonObject>();
				ConnObj->SetStringField(TEXT("source_id"), GetExpressionId(Input->Expression));
				ConnObj->SetNumberField(TEXT("source_output_index"), Input->OutputIndex);

				// ISSUE-1: Also emit source_output_name for use with connect APIs
				TArray<FExpressionOutput>& SourceOutputs = Input->Expression->GetOutputs();
				FString SourceOutputName;
				if (Input->OutputIndex >= 0 && Input->OutputIndex < SourceOutputs.Num())
				{
					SourceOutputName = SourceOutputs[Input->OutputIndex].OutputName.IsNone()
						? TEXT("")
						: SourceOutputs[Input->OutputIndex].OutputName.ToString();
				}
				ConnObj->SetStringField(TEXT("source_output_name"), SourceOutputName);

				ConnObj->SetStringField(TEXT("target_id"), GetExpressionId(Expr));
				FName InputName = Expr->GetInputName(i);
				ConnObj->SetStringField(TEXT("target_input"), InputName.IsNone() ? FString::Printf(TEXT("Input_%d"), i) : InputName.ToString());

				// ISSUE-8: Flag layer blend connections with structured data
				if (UMaterialExpressionLandscapeLayerBlend* BlendTarget = Cast<UMaterialExpressionLandscapeLayerBlend>(Expr))
				{
					ConnObj->SetBoolField(TEXT("is_layer_blend_input"), true);
					// Determine which layer this input belongs to
					// Layer inputs are: Layer0, Height0, Layer1, Height1, ...
					FString InputStr = InputName.IsNone() ? FString::Printf(TEXT("Input_%d"), i) : InputName.ToString();
					FString InputType;
					FString LayerNameStr;
					// Parse e.g. "Layer Grass" or "Height Grass" format
					if (InputStr.StartsWith(TEXT("Layer ")))
					{
						InputType = TEXT("Layer");
						LayerNameStr = InputStr.RightChop(6);
					}
					else if (InputStr.StartsWith(TEXT("Height ")))
					{
						InputType = TEXT("Height");
						LayerNameStr = InputStr.RightChop(7);
					}
					if (!InputType.IsEmpty())
					{
						ConnObj->SetStringField(TEXT("layer_input_type"), InputType);
						ConnObj->SetStringField(TEXT("layer_name"), LayerNameStr);
					}
				}

				ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
			}
		}
	}
	Root->SetArrayField(TEXT("connections"), ConnectionsArray);

	// Material output connections
	TSharedRef<FJsonObject> OutputConns = MakeShared<FJsonObject>();
	auto ExportOutput = [&](EMaterialProperty Prop, const FString& Name) {
		FExpressionInput* Input = Material->GetExpressionInputForProperty(Prop);
		if (Input && Input->Expression)
		{
			OutputConns->SetStringField(Name, GetExpressionId(Input->Expression));
		}
	};
	ExportOutput(MP_BaseColor, TEXT("BaseColor"));
	ExportOutput(MP_Metallic, TEXT("Metallic"));
	ExportOutput(MP_Specular, TEXT("Specular"));
	ExportOutput(MP_Roughness, TEXT("Roughness"));
	ExportOutput(MP_EmissiveColor, TEXT("EmissiveColor"));
	ExportOutput(MP_Opacity, TEXT("Opacity"));
	ExportOutput(MP_OpacityMask, TEXT("OpacityMask"));
	ExportOutput(MP_Normal, TEXT("Normal"));
	ExportOutput(MP_Tangent, TEXT("Tangent"));
	ExportOutput(MP_WorldPositionOffset, TEXT("WorldPositionOffset"));
	ExportOutput(MP_SubsurfaceColor, TEXT("SubsurfaceColor"));
	ExportOutput(MP_AmbientOcclusion, TEXT("AmbientOcclusion"));
	ExportOutput(MP_Refraction, TEXT("Refraction"));
	ExportOutput(MP_PixelDepthOffset, TEXT("PixelDepthOffset"));
	ExportOutput(MP_ShadingModel, TEXT("ShadingModel"));
	Root->SetObjectField(TEXT("output_connections"), OutputConns);

	// Serialize to string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Root, Writer);

	UE_LOG(LogTemp, Log, TEXT("UMaterialNodeService::ExportMaterialGraph: Exported %d expressions, %d connections from %s"),
		TopLevelExpressions.Num(), ConnectionsArray.Num(), *MaterialPath);

	return OutputString;
}

FString UMaterialNodeService::ExportMaterialGraphSummary(const FString& MaterialPath)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return FString();
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("material_path"), MaterialPath);

	// Gather top-level expressions
	TArray<UMaterialExpression*> AllExpressions;
	Material->GetAllExpressionsInMaterialAndFunctionsOfType<UMaterialExpression>(AllExpressions);

	TArray<UMaterialExpression*> TopLevel;
	for (UMaterialExpression* Expr : AllExpressions)
	{
		if (Expr && Expr->GetOuter() == Material)
		{
			TopLevel.Add(Expr);
		}
	}

	Root->SetNumberField(TEXT("expression_count"), TopLevel.Num());

	// Class frequency map
	TMap<FString, int32> ClassCounts;
	for (UMaterialExpression* Expr : TopLevel)
	{
		FString ClassName = Expr->GetClass()->GetName().Replace(TEXT("MaterialExpression"), TEXT(""));
		ClassCounts.FindOrAdd(ClassName)++;
	}

	TSharedRef<FJsonObject> ClassCountsObj = MakeShared<FJsonObject>();
	for (const auto& Pair : ClassCounts)
	{
		ClassCountsObj->SetNumberField(Pair.Key, Pair.Value);
	}
	Root->SetObjectField(TEXT("class_counts"), ClassCountsObj);

	// Count connections
	int32 ConnectionCount = 0;
	for (UMaterialExpression* Expr : TopLevel)
	{
		for (FExpressionInputIterator It(Expr); It; ++It)
		{
			if (It->Expression)
			{
				ConnectionCount++;
			}
		}
	}
	Root->SetNumberField(TEXT("connection_count"), ConnectionCount);

	// Count material output connections via GetExpressionInputForProperty
	int32 OutputCount = 0;
	static const EMaterialProperty OutputProps[] = {
		MP_BaseColor, MP_Normal, MP_Roughness, MP_Metallic,
		MP_Specular, MP_EmissiveColor, MP_Opacity, MP_AmbientOcclusion
	};
	for (EMaterialProperty Prop : OutputProps)
	{
		FExpressionInput* PropInput = Material->GetExpressionInputForProperty(Prop);
		if (PropInput && PropInput->Expression)
		{
			OutputCount++;
		}
	}
	Root->SetNumberField(TEXT("material_output_count"), OutputCount);

	// Parameters
	TArray<TSharedPtr<FJsonValue>> ParameterArray;
	int32 ParamCount = 0;
	for (UMaterialExpression* Expr : TopLevel)
	{
		if (Expr->IsA<UMaterialExpressionParameter>() ||
			Expr->IsA<UMaterialExpressionTextureSampleParameter>())
		{
			TSharedRef<FJsonObject> ParamObj = MakeShared<FJsonObject>();
			FString ClassName = Expr->GetClass()->GetName().Replace(TEXT("MaterialExpression"), TEXT(""));
			ParamObj->SetStringField(TEXT("type"), ClassName);

			// Get parameter name
			if (UMaterialExpressionParameter* Param = Cast<UMaterialExpressionParameter>(Expr))
			{
				ParamObj->SetStringField(TEXT("name"), Param->ParameterName.ToString());
			}
			else if (UMaterialExpressionTextureSampleParameter* TexParam = Cast<UMaterialExpressionTextureSampleParameter>(Expr))
			{
				ParamObj->SetStringField(TEXT("name"), TexParam->ParameterName.ToString());
			}
			ParameterArray.Add(MakeShared<FJsonValueObject>(ParamObj));
			ParamCount++;
		}
	}
	Root->SetNumberField(TEXT("parameter_count"), ParamCount);
	Root->SetArrayField(TEXT("parameters"), ParameterArray);

	// Serialize
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Root, Writer);

	return OutputString;
}

FString UMaterialNodeService::CompareMaterialGraphs(
	const FString& MaterialPathA,
	const FString& MaterialPathB)
{
	UMaterial* MaterialA = LoadMaterialAsset(MaterialPathA);
	UMaterial* MaterialB = LoadMaterialAsset(MaterialPathB);
	if (!MaterialA || !MaterialB)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMaterialNodeService::CompareMaterialGraphs: Failed to load one or both materials"));
		return FString();
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("material_a"), MaterialPathA);
	Root->SetStringField(TEXT("material_b"), MaterialPathB);

	// Helper lambda to get top-level expressions from a material
	auto GetTopLevel = [](UMaterial* Mat) -> TArray<UMaterialExpression*>
	{
		TArray<UMaterialExpression*> All;
		Mat->GetAllExpressionsInMaterialAndFunctionsOfType<UMaterialExpression>(All);
		TArray<UMaterialExpression*> TopLevel;
		for (UMaterialExpression* Expr : All)
		{
			if (Expr && Expr->GetOuter() == Mat)
			{
				TopLevel.Add(Expr);
			}
		}
		return TopLevel;
	};

	TArray<UMaterialExpression*> TopA = GetTopLevel(MaterialA);
	TArray<UMaterialExpression*> TopB = GetTopLevel(MaterialB);

	Root->SetNumberField(TEXT("expression_count_a"), TopA.Num());
	Root->SetNumberField(TEXT("expression_count_b"), TopB.Num());
	bool bExprCountMatch = (TopA.Num() == TopB.Num());
	Root->SetBoolField(TEXT("expression_count_match"), bExprCountMatch);

	// Class frequency comparison
	auto GetClassCounts = [](const TArray<UMaterialExpression*>& Exprs) -> TMap<FString, int32>
	{
		TMap<FString, int32> Counts;
		for (UMaterialExpression* Expr : Exprs)
		{
			FString ClassName = Expr->GetClass()->GetName().Replace(TEXT("MaterialExpression"), TEXT(""));
			Counts.FindOrAdd(ClassName)++;
		}
		return Counts;
	};

	TMap<FString, int32> ClassA = GetClassCounts(TopA);
	TMap<FString, int32> ClassB = GetClassCounts(TopB);

	TArray<TSharedPtr<FJsonValue>> ClassDiffArray;
	TSet<FString> AllClasses;
	for (const auto& Pair : ClassA) AllClasses.Add(Pair.Key);
	for (const auto& Pair : ClassB) AllClasses.Add(Pair.Key);

	bool bAllClassesMatch = true;
	for (const FString& ClassName : AllClasses)
	{
		int32 CountA = ClassA.Contains(ClassName) ? ClassA[ClassName] : 0;
		int32 CountB = ClassB.Contains(ClassName) ? ClassB[ClassName] : 0;
		if (CountA != CountB)
		{
			bAllClassesMatch = false;
			TSharedRef<FJsonObject> DiffObj = MakeShared<FJsonObject>();
			DiffObj->SetStringField(TEXT("class_name"), ClassName);
			DiffObj->SetNumberField(TEXT("count_a"), CountA);
			DiffObj->SetNumberField(TEXT("count_b"), CountB);
			ClassDiffArray.Add(MakeShared<FJsonValueObject>(DiffObj));
		}
	}
	Root->SetArrayField(TEXT("class_diff"), ClassDiffArray);

	// Connection count comparison
	auto CountConnections = [](const TArray<UMaterialExpression*>& Exprs) -> int32
	{
		int32 Count = 0;
		for (UMaterialExpression* Expr : Exprs)
		{
			for (FExpressionInputIterator It(Expr); It; ++It)
			{
				if (It->Expression)
				{
					Count++;
				}
			}
		}
		return Count;
	};

	int32 ConnA = CountConnections(TopA);
	int32 ConnB = CountConnections(TopB);
	Root->SetNumberField(TEXT("connection_count_a"), ConnA);
	Root->SetNumberField(TEXT("connection_count_b"), ConnB);
	bool bConnMatch = (ConnA == ConnB);
	Root->SetBoolField(TEXT("connection_count_match"), bConnMatch);

	// Parameter comparison
	auto GetParameters = [](const TArray<UMaterialExpression*>& Exprs) -> TArray<TPair<FString, FString>>
	{
		TArray<TPair<FString, FString>> Params;
		for (UMaterialExpression* Expr : Exprs)
		{
			FString ClassName = Expr->GetClass()->GetName().Replace(TEXT("MaterialExpression"), TEXT(""));
			if (UMaterialExpressionParameter* Param = Cast<UMaterialExpressionParameter>(Expr))
			{
				Params.Add(TPair<FString, FString>(Param->ParameterName.ToString(), ClassName));
			}
			else if (UMaterialExpressionTextureSampleParameter* TexParam = Cast<UMaterialExpressionTextureSampleParameter>(Expr))
			{
				Params.Add(TPair<FString, FString>(TexParam->ParameterName.ToString(), ClassName));
			}
		}
		return Params;
	};

	TArray<TPair<FString, FString>> ParamsA = GetParameters(TopA);
	TArray<TPair<FString, FString>> ParamsB = GetParameters(TopB);

	// Find missing in B and extra in B
	TArray<TSharedPtr<FJsonValue>> MissingInB;
	TArray<TSharedPtr<FJsonValue>> ExtraInB;

	TSet<FString> ParamNamesA;
	for (const auto& P : ParamsA) ParamNamesA.Add(P.Key);
	TSet<FString> ParamNamesB;
	for (const auto& P : ParamsB) ParamNamesB.Add(P.Key);

	for (const auto& P : ParamsA)
	{
		if (!ParamNamesB.Contains(P.Key))
		{
			TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("name"), P.Key);
			Obj->SetStringField(TEXT("type"), P.Value);
			MissingInB.Add(MakeShared<FJsonValueObject>(Obj));
		}
	}
	for (const auto& P : ParamsB)
	{
		if (!ParamNamesA.Contains(P.Key))
		{
			TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("name"), P.Key);
			Obj->SetStringField(TEXT("type"), P.Value);
			ExtraInB.Add(MakeShared<FJsonValueObject>(Obj));
		}
	}

	Root->SetArrayField(TEXT("missing_parameters_in_b"), MissingInB);
	Root->SetArrayField(TEXT("extra_parameters_in_b"), ExtraInB);
	bool bParamMatch = (MissingInB.Num() == 0 && ExtraInB.Num() == 0 && ParamsA.Num() == ParamsB.Num());
	Root->SetBoolField(TEXT("parameter_match"), bParamMatch);

	// Overall match
	bool bOverallMatch = bExprCountMatch && bAllClassesMatch && bConnMatch && bParamMatch;
	Root->SetBoolField(TEXT("match"), bOverallMatch);

	// Serialize
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Root, Writer);

	UE_LOG(LogTemp, Log, TEXT("UMaterialNodeService::CompareMaterialGraphs: match=%s (exprs %d/%d, conns %d/%d, params %d/%d)"),
		bOverallMatch ? TEXT("true") : TEXT("false"),
		TopA.Num(), TopB.Num(), ConnA, ConnB, ParamsA.Num(), ParamsB.Num());

	return OutputString;
}

// =================================================================
// Layout Actions
// =================================================================

bool UMaterialNodeService::LayoutExpressions(
	const FString& MaterialPath,
	int32 ColumnSpacing,
	int32 RowSpacing)
{
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return false;
	}

	TArray<UMaterialExpression*> AllExpressions;
	Material->GetAllExpressionsInMaterialAndFunctionsOfType<UMaterialExpression>(AllExpressions);

	if (AllExpressions.Num() == 0)
	{
		return true;
	}

	// Build adjacency: for each expression, find what it feeds INTO
	// TargetId -> [SourceIds] (who feeds into target)
	TMap<FString, TArray<FString>> InputsOf; // target -> sources
	TMap<FString, TArray<FString>> OutputsOf; // source -> targets

	for (UMaterialExpression* Expr : AllExpressions)
	{
		if (!Expr) continue;
		FString ExprId = GetExpressionId(Expr);
		if (!InputsOf.Contains(ExprId)) InputsOf.Add(ExprId, TArray<FString>());
		if (!OutputsOf.Contains(ExprId)) OutputsOf.Add(ExprId, TArray<FString>());

		for (FExpressionInputIterator InputIt(Expr); InputIt; ++InputIt)
		{
			if (InputIt->Expression)
			{
				FString SourceId = GetExpressionId(InputIt->Expression);
				InputsOf.FindOrAdd(ExprId).AddUnique(SourceId);
				OutputsOf.FindOrAdd(SourceId).AddUnique(ExprId);
			}
		}
	}

	// Find root nodes: expressions connected to material outputs
	TSet<FString> RootIds;
	auto CheckRoot = [&](EMaterialProperty Prop) {
		FExpressionInput* Input = Material->GetExpressionInputForProperty(Prop);
		if (Input && Input->Expression)
		{
			RootIds.Add(GetExpressionId(Input->Expression));
		}
	};
	CheckRoot(MP_BaseColor);
	CheckRoot(MP_Metallic);
	CheckRoot(MP_Specular);
	CheckRoot(MP_Roughness);
	CheckRoot(MP_Anisotropy);
	CheckRoot(MP_EmissiveColor);
	CheckRoot(MP_Opacity);
	CheckRoot(MP_OpacityMask);
	CheckRoot(MP_Normal);
	CheckRoot(MP_Tangent);
	CheckRoot(MP_WorldPositionOffset);
	CheckRoot(MP_SubsurfaceColor);
	CheckRoot(MP_AmbientOcclusion);
	CheckRoot(MP_Refraction);
	CheckRoot(MP_PixelDepthOffset);
	CheckRoot(MP_ShadingModel);

	// BFS from roots to assign depth (0 = closest to output, higher = farther)
	TMap<FString, int32> DepthMap;
	TArray<FString> Queue;

	for (const FString& RootId : RootIds)
	{
		DepthMap.Add(RootId, 0);
		Queue.Add(RootId);
	}

	int32 QueueIdx = 0;
	while (QueueIdx < Queue.Num())
	{
		FString CurrentId = Queue[QueueIdx++];
		int32 CurrentDepth = DepthMap[CurrentId];

		if (InputsOf.Contains(CurrentId))
		{
			for (const FString& SourceId : InputsOf[CurrentId])
			{
				int32 NewDepth = CurrentDepth + 1;
				if (!DepthMap.Contains(SourceId) || DepthMap[SourceId] < NewDepth)
				{
					DepthMap.Add(SourceId, NewDepth);
					Queue.Add(SourceId);
				}
			}
		}
	}

	// Assign unconnected nodes to max depth + 1
	int32 MaxDepth = 0;
	for (const auto& Pair : DepthMap)
	{
		MaxDepth = FMath::Max(MaxDepth, Pair.Value);
	}
	for (UMaterialExpression* Expr : AllExpressions)
	{
		if (!Expr) continue;
		FString ExprId = GetExpressionId(Expr);
		if (!DepthMap.Contains(ExprId))
		{
			DepthMap.Add(ExprId, MaxDepth + 1);
		}
	}

	// Group by depth column
	TMap<int32, TArray<UMaterialExpression*>> Columns;
	for (UMaterialExpression* Expr : AllExpressions)
	{
		if (!Expr) continue;
		FString ExprId = GetExpressionId(Expr);
		int32 Depth = DepthMap[ExprId];
		Columns.FindOrAdd(Depth).Add(Expr);
	}

	// Position nodes: depth 0 is rightmost, higher depths go left
	FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Layout Material Graph", "Layout Material Graph"));
	Material->Modify();

	for (auto& Pair : Columns)
	{
		int32 Depth = Pair.Key;
		TArray<UMaterialExpression*>& ColumnExprs = Pair.Value;

		int32 ColX = -Depth * ColumnSpacing;

		// Center the column vertically
		int32 TotalHeight = (ColumnExprs.Num() - 1) * RowSpacing;
		int32 StartY = -TotalHeight / 2;

		for (int32 i = 0; i < ColumnExprs.Num(); i++)
		{
			ColumnExprs[i]->Modify();
			ColumnExprs[i]->MaterialExpressionEditorX = ColX;
			ColumnExprs[i]->MaterialExpressionEditorY = StartY + i * RowSpacing;
		}
	}

	RefreshMaterialGraph(Material);

	UE_LOG(LogTemp, Log, TEXT("UMaterialNodeService::LayoutExpressions: Arranged %d expressions across %d columns"),
		AllExpressions.Num(), Columns.Num());
	return true;
}

// =================================================================
// Material Output Actions
// =================================================================

TArray<FString> UMaterialNodeService::GetOutputProperties(const FString& MaterialPath)
{
	TArray<FString> Properties;
	
	Properties.Add(TEXT("BaseColor"));
	Properties.Add(TEXT("Metallic"));
	Properties.Add(TEXT("Specular"));
	Properties.Add(TEXT("Roughness"));
	Properties.Add(TEXT("Anisotropy"));
	Properties.Add(TEXT("EmissiveColor"));
	Properties.Add(TEXT("Opacity"));
	Properties.Add(TEXT("OpacityMask"));
	Properties.Add(TEXT("Normal"));
	Properties.Add(TEXT("Tangent"));
	Properties.Add(TEXT("WorldPositionOffset"));
	Properties.Add(TEXT("SubsurfaceColor"));
	Properties.Add(TEXT("ClearCoat"));
	Properties.Add(TEXT("ClearCoatRoughness"));
	Properties.Add(TEXT("AmbientOcclusion"));
	Properties.Add(TEXT("Refraction"));
	Properties.Add(TEXT("PixelDepthOffset"));
	Properties.Add(TEXT("ShadingModel"));
	Properties.Add(TEXT("Displacement"));
	
	return Properties;
}

TArray<FMaterialOutputConnectionInfo> UMaterialNodeService::GetOutputConnections(const FString& MaterialPath)
{
	TArray<FMaterialOutputConnectionInfo> Results;
	
	UMaterial* Material = LoadMaterialAsset(MaterialPath);
	if (!Material)
	{
		return Results;
	}
	
	auto CheckProperty = [&](EMaterialProperty Prop, const FString& Name) {
		FMaterialOutputConnectionInfo Info;
		Info.PropertyName = Name;
		
		FExpressionInput* Input = Material->GetExpressionInputForProperty(Prop);
		if (Input && Input->Expression)
		{
			Info.bIsConnected = true;
			Info.ConnectedExpressionId = GetExpressionId(Input->Expression);
		}
		
		Results.Add(Info);
	};
	
	CheckProperty(MP_BaseColor, TEXT("BaseColor"));
	CheckProperty(MP_Metallic, TEXT("Metallic"));
	CheckProperty(MP_Specular, TEXT("Specular"));
	CheckProperty(MP_Roughness, TEXT("Roughness"));
	CheckProperty(MP_Anisotropy, TEXT("Anisotropy"));
	CheckProperty(MP_EmissiveColor, TEXT("EmissiveColor"));
	CheckProperty(MP_Opacity, TEXT("Opacity"));
	CheckProperty(MP_OpacityMask, TEXT("OpacityMask"));
	CheckProperty(MP_Normal, TEXT("Normal"));
	CheckProperty(MP_Tangent, TEXT("Tangent"));
	CheckProperty(MP_WorldPositionOffset, TEXT("WorldPositionOffset"));
	CheckProperty(MP_SubsurfaceColor, TEXT("SubsurfaceColor"));
	CheckProperty(MP_AmbientOcclusion, TEXT("AmbientOcclusion"));
	CheckProperty(MP_Refraction, TEXT("Refraction"));
	CheckProperty(MP_PixelDepthOffset, TEXT("PixelDepthOffset"));
	CheckProperty(MP_ShadingModel, TEXT("ShadingModel"));
	
	return Results;
}
