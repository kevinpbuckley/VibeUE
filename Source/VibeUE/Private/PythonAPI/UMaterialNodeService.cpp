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
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialEditingLibrary.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectIterator.h"

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
	
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	TArrayView<FExpressionInput*> Inputs = Expression->GetInputsView();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	// Try exact name match
	for (int32 i = 0; i < Inputs.Num(); i++)
	{
		FName Name = Expression->GetInputName(i);
		if (Name.ToString().Equals(InputName, ESearchCase::IgnoreCase))
		{
			return Inputs[i];
		}
	}
	
	// Try index-based match (Input_0, Input_1, etc.)
	if (InputName.StartsWith(TEXT("Input_")))
	{
		int32 Index = FCString::Atoi(*InputName.RightChop(6));
		if (Index >= 0 && Index < Inputs.Num())
		{
			return Inputs[Index];
		}
	}
	
	// Try numeric index directly
	int32 Index = FCString::Atoi(*InputName);
	if (Index >= 0 && Index < Inputs.Num() && InputName.IsNumeric())
	{
		return Inputs[Index];
	}
	
	// Common aliases
	if (Inputs.Num() > 0 && (InputName.Equals(TEXT("A"), ESearchCase::IgnoreCase) || InputName.Equals(TEXT("Input"), ESearchCase::IgnoreCase)))
	{
		return Inputs[0];
	}
	if (Inputs.Num() > 1 && InputName.Equals(TEXT("B"), ESearchCase::IgnoreCase))
	{
		return Inputs[1];
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
	
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	TArrayView<FExpressionInput*> Inputs = Expression->GetInputsView();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	for (int32 i = 0; i < Inputs.Num(); i++)
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
	
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	TArrayView<FExpressionInput*> Inputs = Expression->GetInputsView();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	for (int32 i = 0; i < Inputs.Num(); i++)
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
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	TArrayView<FExpressionInput*> Inputs = Expression->GetInputsView();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	for (int32 i = 0; i < Inputs.Num(); i++)
	{
		FExpressionInput* Input = Inputs[i];
		if (Input)
		{
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
		
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		TArrayView<FExpressionInput*> Inputs = Expression->GetInputsView();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		for (int32 i = 0; i < Inputs.Num(); i++)
		{
			FExpressionInput* Input = Inputs[i];
			if (Input && Input->Expression)
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
				if (Color.InitFromString(DefaultValue))
				{
					VecParam->DefaultValue = Color;
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
			if (!GroupName.IsEmpty())
			{
				TexParam->Group = FName(*GroupName);
			}
			NewExpression = TexParam;
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
		
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		TArrayView<FExpressionInput*> Inputs = Expr->GetInputsView();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		for (FExpressionInput* Input : Inputs)
		{
			if (Input && Input->Expression == OldExpression)
			{
				Input->Expression = NewExpression;
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
