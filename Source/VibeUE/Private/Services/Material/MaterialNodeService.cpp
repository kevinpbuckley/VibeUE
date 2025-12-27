// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "Services/Material/MaterialNodeService.h"
#include "Core/ErrorCodes.h"
#include "Core/JsonValueHelper.h"
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
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionCrossProduct.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionRotator.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionPixelNormalWS.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionComment.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialEditingLibrary.h"
#include "MaterialEditorUtilities.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/SavePackage.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectIterator.h"
#include "AssetRegistry/AssetRegistryModule.h"

FMaterialNodeService::FMaterialNodeService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

// ============================================================================
// Expression Discovery
// ============================================================================

TResult<TArray<FMaterialExpressionTypeInfo>> FMaterialNodeService::DiscoverExpressionTypes(
    const FString& Category,
    const FString& SearchTerm,
    int32 MaxResults)
{
    TArray<FMaterialExpressionTypeInfo> Results;
    
    // Get all classes derived from UMaterialExpression
    for (TObjectIterator<UClass> It; It; ++It)
    {
        UClass* Class = *It;
        if (!Class->IsChildOf(UMaterialExpression::StaticClass()))
        {
            continue;
        }
        
        // Skip abstract classes
        if (Class->HasAnyClassFlags(CLASS_Abstract))
        {
            continue;
        }
        
        // Skip the base class itself
        if (Class == UMaterialExpression::StaticClass())
        {
            continue;
        }
        
        // Get the CDO for inspection
        UMaterialExpression* CDO = Class->GetDefaultObject<UMaterialExpression>();
        if (!CDO)
        {
            continue;
        }
        
        FMaterialExpressionTypeInfo TypeInfo;
        TypeInfo.ClassName = Class->GetName();
        TypeInfo.DisplayName = Class->GetName().Replace(TEXT("MaterialExpression"), TEXT(""));
        
        // Get category from metadata (note: most UE5 expression classes don't have this)
        const FString* CategoryMeta = Class->FindMetaData(TEXT("Category"));
        TypeInfo.Category = CategoryMeta ? *CategoryMeta : TEXT("Misc");
        
        // Get tooltip/description
        const FString* TooltipMeta = Class->FindMetaData(TEXT("ToolTip"));
        TypeInfo.Description = TooltipMeta ? *TooltipMeta : TEXT("");
        
        // Check if it's a parameter type
        TypeInfo.bIsParameter = Class->IsChildOf(UMaterialExpressionParameter::StaticClass());
        
        // NOTE: We intentionally do NOT enumerate inputs/outputs from CDOs here.
        // Some material expression types (particularly those with MCT_Unknown or exotic 
        // material value types like 131072) cause engine assertions when their CDO
        // inputs are accessed. The inputs/outputs can be safely enumerated once an
        // actual expression instance is created within a material.
        // TypeInfo.InputNames and TypeInfo.OutputNames remain empty for discovery.
        
        // Apply category filter
        if (!Category.IsEmpty() && !TypeInfo.Category.Contains(Category, ESearchCase::IgnoreCase))
        {
            continue;
        }
        
        // Apply search filter
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
    
    // If category was specified but no results found, provide helpful error
    if (Results.Num() == 0 && !Category.IsEmpty())
    {
        return TResult<TArray<FMaterialExpressionTypeInfo>>::Error(
            VibeUE::ErrorCodes::PARAM_INVALID,
            FString::Printf(TEXT("No expression types found for category '%s'. "
                "RECOMMENDATION: Don't use category filter - use search_term instead. "
                "Examples: search_term='Constant' for scalar constants, search_term='Vector' for vectors, "
                "search_term='Parameter' for parameters, search_term='Texture' for texture samplers."),
                *Category));
    }
    
    // Sort by category then name
    Results.Sort([](const FMaterialExpressionTypeInfo& A, const FMaterialExpressionTypeInfo& B) {
        if (A.Category != B.Category)
        {
            return A.Category < B.Category;
        }
        return A.DisplayName < B.DisplayName;
    });
    
    return TResult<TArray<FMaterialExpressionTypeInfo>>::Success(Results);
}

TResult<TArray<FString>> FMaterialNodeService::GetExpressionCategories()
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
    return TResult<TArray<FString>>::Success(Result);
}

// ============================================================================
// Expression Lifecycle
// ============================================================================

TResult<FMaterialExpressionInfo> FMaterialNodeService::CreateExpression(
    UMaterial* Material,
    const FString& ExpressionClassName,
    int32 PosX,
    int32 PosY)
{
    if (!Material)
    {
        return TResult<FMaterialExpressionInfo>::Error(
            VibeUE::ErrorCodes::INVALID_PARAMETER,
            TEXT("Material is null")
        );
    }
    
    // Resolve expression class
    UClass* ExpressionClass = ResolveExpressionClass(ExpressionClassName);
    if (!ExpressionClass)
    {
        return TResult<FMaterialExpressionInfo>::Error(
            VibeUE::ErrorCodes::INVALID_PARAMETER,
            FString::Printf(TEXT("Unknown expression class: %s"), *ExpressionClassName)
        );
    }
    
    // Use scoped transaction for undo support
    FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Create Material Expression", "Create Material Expression"));
    Material->Modify();
    
    // Create the expression using UMaterialEditingLibrary
    UMaterialExpression* NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
        Material,
        ExpressionClass,
        PosX,
        PosY
    );
    
    if (!NewExpression)
    {
        return TResult<FMaterialExpressionInfo>::Error(
            VibeUE::ErrorCodes::OPERATION_FAILED,
            FString::Printf(TEXT("Failed to create expression of type: %s"), *ExpressionClassName)
        );
    }
    
    // Refresh the material graph
    RefreshMaterialGraph(Material);
    
    LogInfo(FString::Printf(TEXT("Created material expression '%s' at (%d, %d)"), 
        *ExpressionClassName, PosX, PosY));
    
    return TResult<FMaterialExpressionInfo>::Success(BuildExpressionInfo(NewExpression));
}

TResult<void> FMaterialNodeService::DeleteExpression(UMaterial* Material, const FString& ExpressionId)
{
    if (!Material)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::INVALID_PARAMETER, TEXT("Material is null"));
    }
    
    UMaterialExpression* Expression = FindExpressionById(Material, ExpressionId);
    if (!Expression)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::EXPRESSION_NOT_FOUND,
            FString::Printf(TEXT("Expression not found: %s"), *ExpressionId)
        );
    }
    
    FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Delete Material Expression", "Delete Material Expression"));
    Material->Modify();
    
    // Use UMaterialEditingLibrary to delete
    UMaterialEditingLibrary::DeleteMaterialExpression(Material, Expression);
    
    RefreshMaterialGraph(Material);
    
    LogInfo(FString::Printf(TEXT("Deleted material expression: %s"), *ExpressionId));
    
    return TResult<void>::Success();
}

TResult<void> FMaterialNodeService::MoveExpression(
    UMaterial* Material,
    const FString& ExpressionId,
    int32 PosX,
    int32 PosY)
{
    if (!Material)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::INVALID_PARAMETER, TEXT("Material is null"));
    }
    
    UMaterialExpression* Expression = FindExpressionById(Material, ExpressionId);
    if (!Expression)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::EXPRESSION_NOT_FOUND,
            FString::Printf(TEXT("Expression not found: %s"), *ExpressionId)
        );
    }
    
    FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Move Material Expression", "Move Material Expression"));
    Expression->Modify();
    
    Expression->MaterialExpressionEditorX = PosX;
    Expression->MaterialExpressionEditorY = PosY;
    
    RefreshMaterialGraph(Material);
    
    return TResult<void>::Success();
}

// ============================================================================
// Expression Information
// ============================================================================

TResult<TArray<FMaterialExpressionInfo>> FMaterialNodeService::ListExpressions(UMaterial* Material)
{
    if (!Material)
    {
        return TResult<TArray<FMaterialExpressionInfo>>::Error(
            VibeUE::ErrorCodes::INVALID_PARAMETER,
            TEXT("Material is null")
        );
    }
    
    TArray<FMaterialExpressionInfo> Results;
    
    // Get all expressions from the material
    TArray<UMaterialExpression*> Expressions;
    Material->GetAllExpressionsInMaterialAndFunctionsOfType<UMaterialExpression>(Expressions);
    
    for (UMaterialExpression* Expression : Expressions)
    {
        if (Expression)
        {
            Results.Add(BuildExpressionInfo(Expression));
        }
    }
    
    return TResult<TArray<FMaterialExpressionInfo>>::Success(Results);
}

TResult<FMaterialExpressionInfo> FMaterialNodeService::GetExpressionDetails(
    UMaterial* Material,
    const FString& ExpressionId)
{
    if (!Material)
    {
        return TResult<FMaterialExpressionInfo>::Error(
            VibeUE::ErrorCodes::INVALID_PARAMETER,
            TEXT("Material is null")
        );
    }
    
    UMaterialExpression* Expression = FindExpressionById(Material, ExpressionId);
    if (!Expression)
    {
        return TResult<FMaterialExpressionInfo>::Error(
            VibeUE::ErrorCodes::EXPRESSION_NOT_FOUND,
            FString::Printf(TEXT("Expression not found: %s"), *ExpressionId)
        );
    }
    
    return TResult<FMaterialExpressionInfo>::Success(BuildExpressionInfo(Expression));
}

TResult<TArray<FMaterialPinInfo>> FMaterialNodeService::GetExpressionPins(
    UMaterial* Material,
    const FString& ExpressionId)
{
    if (!Material)
    {
        return TResult<TArray<FMaterialPinInfo>>::Error(
            VibeUE::ErrorCodes::INVALID_PARAMETER,
            TEXT("Material is null")
        );
    }
    
    UMaterialExpression* Expression = FindExpressionById(Material, ExpressionId);
    if (!Expression)
    {
        return TResult<TArray<FMaterialPinInfo>>::Error(
            VibeUE::ErrorCodes::EXPRESSION_NOT_FOUND,
            FString::Printf(TEXT("Expression not found: %s"), *ExpressionId)
        );
    }
    
    TArray<FMaterialPinInfo> Pins;
    
    // Get inputs (using GetInputsView - suppress deprecation warning until FExpressionInputIterator is stable)
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    TArrayView<FExpressionInput*> Inputs = Expression->GetInputsView();
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    for (int32 i = 0; i < Inputs.Num(); i++)
    {
        FExpressionInput* Input = Inputs[i];
        if (Input)
        {
            FMaterialPinInfo PinInfo;
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
        FMaterialPinInfo PinInfo;
        PinInfo.Name = Outputs[i].OutputName.IsNone() ? FString::Printf(TEXT("Output_%d"), i) : Outputs[i].OutputName.ToString();
        PinInfo.Index = i;
        PinInfo.Direction = TEXT("Output");
        PinInfo.bIsConnected = false; // Would need to scan all inputs to determine this
        Pins.Add(PinInfo);
    }
    
    return TResult<TArray<FMaterialPinInfo>>::Success(Pins);
}

// ============================================================================
// Connections
// ============================================================================

TResult<void> FMaterialNodeService::ConnectExpressions(
    UMaterial* Material,
    const FString& SourceExpressionId,
    const FString& SourceOutputName,
    const FString& TargetExpressionId,
    const FString& TargetInputName)
{
    if (!Material)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::INVALID_PARAMETER, TEXT("Material is null"));
    }
    
    UMaterialExpression* SourceExpr = FindExpressionById(Material, SourceExpressionId);
    if (!SourceExpr)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::EXPRESSION_NOT_FOUND,
            FString::Printf(TEXT("Source expression not found: %s"), *SourceExpressionId)
        );
    }
    
    UMaterialExpression* TargetExpr = FindExpressionById(Material, TargetExpressionId);
    if (!TargetExpr)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::EXPRESSION_NOT_FOUND,
            FString::Printf(TEXT("Target expression not found: %s"), *TargetExpressionId)
        );
    }
    
    // Find output index
    int32 OutputIndex = FindOutputIndexByName(SourceExpr, SourceOutputName);
    if (OutputIndex < 0)
    {
        OutputIndex = 0; // Default to first output
    }
    
    // Find input
    FExpressionInput* TargetInput = FindInputByName(TargetExpr, TargetInputName);
    if (!TargetInput)
    {
        TArray<FString> ValidInputs = GetExpressionInputNames(TargetExpr);
        FString ValidInputsStr = ValidInputs.Num() > 0 
            ? FString::Join(ValidInputs, TEXT(", "))
            : TEXT("none - this expression has no inputs");
            
        return TResult<void>::Error(
            VibeUE::ErrorCodes::EXPRESSION_INPUT_NOT_FOUND,
            FString::Printf(TEXT("Input '%s' not found on target expression. Valid inputs: %s"), 
                *TargetInputName, *ValidInputsStr)
        );
    }
    
    FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Connect Material Expressions", "Connect Material Expressions"));
    Material->Modify();
    
    // Make the connection
    TargetInput->Connect(OutputIndex, SourceExpr);
    
    RefreshMaterialGraph(Material);
    
    LogInfo(FString::Printf(TEXT("Connected %s.%s -> %s.%s"), 
        *SourceExpressionId, *SourceOutputName, *TargetExpressionId, *TargetInputName));
    
    return TResult<void>::Success();
}

TResult<void> FMaterialNodeService::DisconnectInput(
    UMaterial* Material,
    const FString& ExpressionId,
    const FString& InputName)
{
    if (!Material)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::INVALID_PARAMETER, TEXT("Material is null"));
    }
    
    UMaterialExpression* Expression = FindExpressionById(Material, ExpressionId);
    if (!Expression)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::EXPRESSION_NOT_FOUND,
            FString::Printf(TEXT("Expression not found: %s"), *ExpressionId)
        );
    }
    
    FExpressionInput* Input = FindInputByName(Expression, InputName);
    if (!Input)
    {
        TArray<FString> ValidInputs = GetExpressionInputNames(Expression);
        FString ValidInputsStr = ValidInputs.Num() > 0 
            ? FString::Join(ValidInputs, TEXT(", "))
            : TEXT("none - this expression has no inputs");
            
        return TResult<void>::Error(
            VibeUE::ErrorCodes::EXPRESSION_INPUT_NOT_FOUND,
            FString::Printf(TEXT("Input '%s' not found. Valid inputs: %s"), 
                *InputName, *ValidInputsStr)
        );
    }
    
    FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Disconnect Material Input", "Disconnect Material Input"));
    Material->Modify();
    
    Input->Expression = nullptr;
    Input->OutputIndex = 0;
    
    RefreshMaterialGraph(Material);
    
    return TResult<void>::Success();
}

TResult<void> FMaterialNodeService::ConnectToMaterialProperty(
    UMaterial* Material,
    const FString& ExpressionId,
    const FString& OutputName,
    const FString& MaterialProperty)
{
    if (!Material)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::INVALID_PARAMETER, TEXT("Material is null"));
    }
    
    UMaterialExpression* Expression = FindExpressionById(Material, ExpressionId);
    if (!Expression)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::EXPRESSION_NOT_FOUND,
            FString::Printf(TEXT("Expression not found: %s"), *ExpressionId)
        );
    }
    
    FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Connect to Material Property", "Connect to Material Property"));
    Material->Modify();
    
    // Normalize OutputName: "Output_0", "Output_1" etc. should become empty string
    // since UMaterialEditingLibrary expects empty for default output or actual output name
    FString NormalizedOutputName = OutputName;
    if (NormalizedOutputName.StartsWith(TEXT("Output_")))
    {
        // For synthesized output names like "Output_0", pass empty to use default
        NormalizedOutputName = TEXT("");
    }
    
    // Use UMaterialEditingLibrary for this
    bool bSuccess = UMaterialEditingLibrary::ConnectMaterialProperty(
        Expression,
        NormalizedOutputName,
        StringToMaterialProperty(MaterialProperty)
    );
    
    if (!bSuccess)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::OPERATION_FAILED,
            FString::Printf(TEXT("Failed to connect to material property: %s"), *MaterialProperty)
        );
    }
    
    RefreshMaterialGraph(Material);
    
    LogInfo(FString::Printf(TEXT("Connected %s to material property %s"), *ExpressionId, *MaterialProperty));
    
    return TResult<void>::Success();
}

TResult<void> FMaterialNodeService::DisconnectMaterialProperty(UMaterial* Material, const FString& MaterialProperty)
{
    if (!Material)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::INVALID_PARAMETER, TEXT("Material is null"));
    }
    
    FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Disconnect Material Property", "Disconnect Material Property"));
    Material->Modify();
    
    EMaterialProperty PropEnum = StringToMaterialProperty(MaterialProperty);
    
    // Get the expression input for this property and clear it
    FExpressionInput* PropertyInput = Material->GetExpressionInputForProperty(PropEnum);
    if (PropertyInput)
    {
        PropertyInput->Expression = nullptr;
        PropertyInput->OutputIndex = 0;
    }
    
    RefreshMaterialGraph(Material);
    
    return TResult<void>::Success();
}

TResult<TArray<FMaterialConnectionInfo>> FMaterialNodeService::ListConnections(UMaterial* Material)
{
    if (!Material)
    {
        return TResult<TArray<FMaterialConnectionInfo>>::Error(
            VibeUE::ErrorCodes::INVALID_PARAMETER,
            TEXT("Material is null")
        );
    }
    
    TArray<FMaterialConnectionInfo> Connections;
    
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
                FMaterialConnectionInfo ConnInfo;
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
    
    return TResult<TArray<FMaterialConnectionInfo>>::Success(Connections);
}

// ============================================================================
// Expression Properties
// ============================================================================

TResult<FString> FMaterialNodeService::GetExpressionProperty(
    UMaterial* Material,
    const FString& ExpressionId,
    const FString& PropertyName)
{
    if (!Material)
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::INVALID_PARAMETER, TEXT("Material is null"));
    }
    
    UMaterialExpression* Expression = FindExpressionById(Material, ExpressionId);
    if (!Expression)
    {
        return TResult<FString>::Error(
            VibeUE::ErrorCodes::EXPRESSION_NOT_FOUND,
            FString::Printf(TEXT("Expression not found: %s"), *ExpressionId)
        );
    }
    
    FProperty* Property = Expression->GetClass()->FindPropertyByName(FName(*PropertyName));
    if (!Property)
    {
        return TResult<FString>::Error(
            VibeUE::ErrorCodes::EXPRESSION_NOT_FOUND,
            FString::Printf(TEXT("Property not found: %s"), *PropertyName)
        );
    }
    
    FString Value;
    Property->ExportTextItem_Direct(Value, Property->ContainerPtrToValuePtr<void>(Expression), nullptr, Expression, PPF_None);
    
    return TResult<FString>::Success(Value);
}

TResult<void> FMaterialNodeService::SetExpressionProperty(
    UMaterial* Material,
    const FString& ExpressionId,
    const FString& PropertyName,
    const FString& Value)
{
    if (!Material)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::INVALID_PARAMETER, TEXT("Material is null"));
    }
    
    UMaterialExpression* Expression = FindExpressionById(Material, ExpressionId);
    if (!Expression)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::EXPRESSION_NOT_FOUND,
            FString::Printf(TEXT("Expression not found: %s"), *ExpressionId)
        );
    }
    
    FProperty* Property = Expression->GetClass()->FindPropertyByName(FName(*PropertyName));
    if (!Property)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::EXPRESSION_NOT_FOUND,
            FString::Printf(TEXT("Property not found: %s"), *PropertyName)
        );
    }
    
    FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Set Material Expression Property", "Set Material Expression Property"));
    Expression->Modify();
    
    void* PropertyValue = Property->ContainerPtrToValuePtr<void>(Expression);
    bool bValueSet = false;
    
    // Handle FLinearColor properties with JSON helper for robust parsing
    if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        if (StructProp->Struct->GetName() == TEXT("LinearColor"))
        {
            FLinearColor Color;
            if (FJsonValueHelper::TryParseLinearColor(Value, Color))
            {
                FLinearColor* ColorPtr = static_cast<FLinearColor*>(PropertyValue);
                *ColorPtr = Color;
                bValueSet = true;
            }
        }
        else if (StructProp->Struct->GetName() == TEXT("Color"))
        {
            FLinearColor LinearColor;
            if (FJsonValueHelper::TryParseLinearColor(Value, LinearColor))
            {
                FColor* ColorPtr = static_cast<FColor*>(PropertyValue);
                *ColorPtr = LinearColor.ToFColor(true);
                bValueSet = true;
            }
        }
    }
    
    // Fallback to Unreal's standard text import
    if (!bValueSet)
    {
        Property->ImportText_Direct(*Value, PropertyValue, Expression, PPF_None);
    }
    
    RefreshMaterialGraph(Material);
    
    return TResult<void>::Success();
}

TResult<TArray<TPair<FString, FString>>> FMaterialNodeService::ListExpressionProperties(
    UMaterial* Material,
    const FString& ExpressionId)
{
    if (!Material)
    {
        return TResult<TArray<TPair<FString, FString>>>::Error(
            VibeUE::ErrorCodes::INVALID_PARAMETER,
            TEXT("Material is null")
        );
    }
    
    UMaterialExpression* Expression = FindExpressionById(Material, ExpressionId);
    if (!Expression)
    {
        return TResult<TArray<TPair<FString, FString>>>::Error(
            VibeUE::ErrorCodes::EXPRESSION_NOT_FOUND,
            FString::Printf(TEXT("Expression not found: %s"), *ExpressionId)
        );
    }
    
    TArray<TPair<FString, FString>> Properties;
    
    for (TFieldIterator<FProperty> PropIt(Expression->GetClass()); PropIt; ++PropIt)
    {
        FProperty* Property = *PropIt;
        
        // Skip internal/hidden properties
        if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient))
        {
            continue;
        }
        
        // Check if editable
        if (!Property->HasAnyPropertyFlags(CPF_Edit))
        {
            continue;
        }
        
        FString Value;
        Property->ExportTextItem_Direct(Value, Property->ContainerPtrToValuePtr<void>(Expression), nullptr, Expression, PPF_None);
        
        Properties.Add(TPair<FString, FString>(Property->GetName(), Value));
    }
    
    return TResult<TArray<TPair<FString, FString>>>::Success(Properties);
}

// ============================================================================
// Parameter Operations
// ============================================================================

TResult<FMaterialExpressionInfo> FMaterialNodeService::PromoteToParameter(
    UMaterial* Material,
    const FString& ExpressionId,
    const FString& ParameterName,
    const FString& GroupName)
{
    if (!Material)
    {
        return TResult<FMaterialExpressionInfo>::Error(
            VibeUE::ErrorCodes::INVALID_PARAMETER,
            TEXT("Material is null")
        );
    }
    
    UMaterialExpression* OldExpression = FindExpressionById(Material, ExpressionId);
    if (!OldExpression)
    {
        return TResult<FMaterialExpressionInfo>::Error(
            VibeUE::ErrorCodes::EXPRESSION_NOT_FOUND,
            FString::Printf(TEXT("Expression not found: %s"), *ExpressionId)
        );
    }
    
    FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Promote to Parameter", "Promote to Parameter"));
    Material->Modify();
    
    UMaterialExpression* NewExpression = nullptr;
    int32 PosX = OldExpression->MaterialExpressionEditorX;
    int32 PosY = OldExpression->MaterialExpressionEditorY;
    
    // Determine parameter type based on expression type
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
        return TResult<FMaterialExpressionInfo>::Error(
            VibeUE::ErrorCodes::INVALID_PARAMETER,
            FString::Printf(TEXT("Cannot promote expression of type %s to parameter"), *OldExpression->GetClass()->GetName())
        );
    }
    
    // Transfer connections - find anything connected to the old expression's output
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
                // Keep OutputIndex the same
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
    
    LogInfo(FString::Printf(TEXT("Promoted expression to parameter '%s'"), *ParameterName));
    
    return TResult<FMaterialExpressionInfo>::Success(BuildExpressionInfo(NewExpression));
}

TResult<FMaterialExpressionInfo> FMaterialNodeService::CreateParameter(
    UMaterial* Material,
    const FString& ParameterType,
    const FString& ParameterName,
    const FString& GroupName,
    const FString& DefaultValue,
    int32 PosX,
    int32 PosY)
{
    if (!Material)
    {
        return TResult<FMaterialExpressionInfo>::Error(
            VibeUE::ErrorCodes::INVALID_PARAMETER,
            TEXT("Material is null")
        );
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
            // Use FJsonValueHelper for robust color parsing (hex, named, Unreal format, etc.)
            if (!DefaultValue.IsEmpty())
            {
                FLinearColor Color;
                if (FJsonValueHelper::TryParseLinearColor(DefaultValue, Color))
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
        return TResult<FMaterialExpressionInfo>::Error(
            VibeUE::ErrorCodes::INVALID_PARAMETER,
            FString::Printf(TEXT("Unknown parameter type: %s (valid types: Scalar, Vector, Texture, StaticBool)"), *ParameterType)
        );
    }
    
    RefreshMaterialGraph(Material);
    
    LogInfo(FString::Printf(TEXT("Created %s parameter '%s'"), *ParameterType, *ParameterName));
    
    return TResult<FMaterialExpressionInfo>::Success(BuildExpressionInfo(NewExpression));
}

TResult<void> FMaterialNodeService::SetParameterMetadata(
    UMaterial* Material,
    const FString& ExpressionId,
    const FString& GroupName,
    int32 SortPriority)
{
    if (!Material)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::INVALID_PARAMETER, TEXT("Material is null"));
    }
    
    UMaterialExpression* Expression = FindExpressionById(Material, ExpressionId);
    if (!Expression)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::EXPRESSION_NOT_FOUND,
            FString::Printf(TEXT("Expression not found: %s"), *ExpressionId)
        );
    }
    
    UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(Expression);
    if (!ParamExpr)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::INVALID_PARAMETER,
            TEXT("Expression is not a parameter")
        );
    }
    
    FScopedTransaction Transaction(NSLOCTEXT("MaterialNodeService", "Set Parameter Metadata", "Set Parameter Metadata"));
    ParamExpr->Modify();
    
    if (!GroupName.IsEmpty())
    {
        ParamExpr->Group = FName(*GroupName);
    }
    ParamExpr->SortPriority = SortPriority;
    
    RefreshMaterialGraph(Material);
    
    return TResult<void>::Success();
}

// ============================================================================
// Material Output Properties
// ============================================================================

TResult<TArray<FString>> FMaterialNodeService::GetMaterialOutputProperties(UMaterial* Material)
{
    TArray<FString> Properties;
    
    // Common material properties
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
    
    return TResult<TArray<FString>>::Success(Properties);
}

TResult<TMap<FString, FString>> FMaterialNodeService::GetMaterialOutputConnections(UMaterial* Material)
{
    if (!Material)
    {
        return TResult<TMap<FString, FString>>::Error(
            VibeUE::ErrorCodes::INVALID_PARAMETER,
            TEXT("Material is null")
        );
    }
    
    TMap<FString, FString> Connections;
    
    // Check each material property
    auto CheckProperty = [&](EMaterialProperty Prop, const FString& Name) {
        FExpressionInput* Input = Material->GetExpressionInputForProperty(Prop);
        if (Input && Input->Expression)
        {
            Connections.Add(Name, GetExpressionId(Input->Expression));
        }
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
    
    return TResult<TMap<FString, FString>>::Success(Connections);
}

// ============================================================================
// Helper Methods
// ============================================================================

UMaterialExpression* FMaterialNodeService::FindExpressionById(UMaterial* Material, const FString& ExpressionId)
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

FString FMaterialNodeService::GetExpressionId(UMaterialExpression* Expression)
{
    if (!Expression) return TEXT("");
    
    // Use a combination of class name and object pointer for unique ID
    return FString::Printf(TEXT("%s_%p"), *Expression->GetClass()->GetName(), Expression);
}

FExpressionInput* FMaterialNodeService::FindInputByName(UMaterialExpression* Expression, const FString& InputName)
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
    
    // Default to first input if A or B (common names)
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

int32 FMaterialNodeService::FindOutputIndexByName(UMaterialExpression* Expression, const FString& OutputName)
{
    if (!Expression) return -1;
    
    TArray<FExpressionOutput>& Outputs = Expression->GetOutputs();
    
    // Empty name = first output
    if (OutputName.IsEmpty())
    {
        return Outputs.Num() > 0 ? 0 : -1;
    }
    
    // Try exact name match
    for (int32 i = 0; i < Outputs.Num(); i++)
    {
        if (Outputs[i].OutputName.ToString().Equals(OutputName, ESearchCase::IgnoreCase))
        {
            return i;
        }
    }
    
    // Handle synthetic names like "Output_0", "Output_1", etc.
    if (OutputName.StartsWith(TEXT("Output_")))
    {
        FString IndexStr = OutputName.RightChop(7); // Remove "Output_"
        int32 Index = FCString::Atoi(*IndexStr);
        if (Index >= 0 && Index < Outputs.Num())
        {
            return Index;
        }
    }
    
    // Try numeric index (for backwards compat with raw numbers like "0")
    int32 Index = FCString::Atoi(*OutputName);
    if (Index >= 0 && Index < Outputs.Num())
    {
        return Index;
    }
    
    return 0; // Default to first output
}

TArray<FString> FMaterialNodeService::GetExpressionInputNames(UMaterialExpression* Expression)
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

TArray<FString> FMaterialNodeService::GetExpressionOutputNames(UMaterialExpression* Expression)
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

UClass* FMaterialNodeService::ResolveExpressionClass(const FString& ClassName)
{
    // Try with MaterialExpression prefix
    FString FullName = ClassName;
    if (!FullName.StartsWith(TEXT("MaterialExpression")))
    {
        FullName = TEXT("MaterialExpression") + ClassName;
    }
    
    // Search for the class
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
    
    // Try finding by short name (e.g., "Add" -> "MaterialExpressionAdd")
    FString ShortName = TEXT("UMaterialExpression") + ClassName;
    UClass* FoundClass = FindObject<UClass>(nullptr, *ShortName);
    if (FoundClass)
    {
        return FoundClass;
    }
    
    return nullptr;
}

FMaterialExpressionInfo FMaterialNodeService::BuildExpressionInfo(UMaterialExpression* Expression)
{
    FMaterialExpressionInfo Info;
    if (!Expression) return Info;
    
    Info.Id = GetExpressionId(Expression);
    Info.ClassName = Expression->GetClass()->GetName();
    Info.DisplayName = Info.ClassName.Replace(TEXT("MaterialExpression"), TEXT(""));
    Info.PosX = Expression->MaterialExpressionEditorX;
    Info.PosY = Expression->MaterialExpressionEditorY;
    Info.Description = Expression->GetDescription();
    
    // Check if parameter
    if (UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(Expression))
    {
        Info.bIsParameter = true;
        Info.ParameterName = ParamExpr->ParameterName.ToString();
        Info.Category = ParamExpr->Group.ToString();
    }
    
    // Get inputs (suppress deprecation warning)
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    TArrayView<FExpressionInput*> Inputs = Expression->GetInputsView();
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    for (int32 i = 0; i < Inputs.Num(); i++)
    {
        FName InputName = Expression->GetInputName(i);
        Info.InputNames.Add(InputName.IsNone() ? FString::Printf(TEXT("Input_%d"), i) : InputName.ToString());
    }
    
    // Get outputs
    TArray<FExpressionOutput>& Outputs = Expression->GetOutputs();
    for (int32 i = 0; i < Outputs.Num(); i++)
    {
        Info.OutputNames.Add(Outputs[i].OutputName.IsNone() ? FString::Printf(TEXT("Output_%d"), i) : Outputs[i].OutputName.ToString());
    }
    
    return Info;
}

EMaterialProperty FMaterialNodeService::StringToMaterialProperty(const FString& PropertyName)
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

void FMaterialNodeService::RefreshMaterialGraph(UMaterial* Material)
{
    if (!Material) return;
    
    // Don't refresh if we're not on the game thread
    if (!IsInGameThread())
    {
        UE_LOG(LogTemp, Warning, TEXT("RefreshMaterialGraph called from non-game thread, skipping"));
        return;
    }
    
    // Mark material as modified
    Material->MarkPackageDirty();
    
    // Update preview material - wrap in validity check
    if (IsValid(Material))
    {
        Material->PreEditChange(nullptr);
        Material->PostEditChange();
    }
    
    // Rebuild the material graph if it exists
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
    
    // Skip the close/reopen cycle which can cause crashes during rapid operations
    // The graph rebuild above is sufficient for most operations
}

