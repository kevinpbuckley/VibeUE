// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "Services/Material/MaterialService.h"
#include "Core/ErrorCodes.h"
#include "Core/JsonValueHelper.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialOverrideNanite.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Factories/MaterialFactoryNew.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/PackageName.h"
#include "Engine/Texture.h"

#if WITH_EDITOR
#include "MaterialEditorUtilities.h"
#include "IMaterialEditor.h"
#include "EditorSupportDelegates.h"
#include "Subsystems/AssetEditorSubsystem.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMaterialService, Log, All);

// Known material input names (graph connections, not simple properties)
static const TSet<FString> MaterialInputNames = {
    TEXT("BaseColor"), TEXT("Metallic"), TEXT("Specular"), TEXT("Roughness"),
    TEXT("Anisotropy"), TEXT("EmissiveColor"), TEXT("Opacity"), TEXT("OpacityMask"),
    TEXT("Normal"), TEXT("Tangent"), TEXT("WorldPositionOffset"), TEXT("SubsurfaceColor"),
    TEXT("ClearCoat"), TEXT("ClearCoatRoughness"), TEXT("AmbientOcclusion"),
    TEXT("Refraction"), TEXT("PixelDepthOffset"), TEXT("ShadingModelFromMaterialExpression"),
    TEXT("FrontMaterial"), TEXT("Displacement")
};

// Check if property name is a material input (graph connection)
static bool IsMaterialInputProperty(const FString& PropertyName)
{
    return MaterialInputNames.Contains(PropertyName);
}

// Get all editable property names from UMaterial
static TArray<FString> GetAllEditablePropertyNames()
{
    TArray<FString> PropertyNames;
    
    for (TFieldIterator<FProperty> It(UMaterial::StaticClass()); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property->HasAnyPropertyFlags(CPF_Edit))
        {
            continue;
        }
        
        // Skip material input types (they're not settable via set_property)
        FString TypeName = Property->GetCPPType();
        if (TypeName.Contains(TEXT("MaterialInput")))
        {
            continue;
        }
        
        PropertyNames.Add(Property->GetName());
    }
    
    return PropertyNames;
}

// Get the most commonly used editable properties (key properties)
static TArray<FString> GetKeyPropertyNames()
{
    return {
        TEXT("TwoSided"),
        TEXT("BlendMode"),
        TEXT("ShadingModel"),
        TEXT("MaterialDomain"),
        TEXT("OpacityMaskClipValue"),
        TEXT("bCastDynamicShadowAsMasked"),
        TEXT("DitheredLODTransition"),
        TEXT("bTangentSpaceNormal")
    };
}

// Local helper function for property type names
static FString GetPropertyTypeName(FProperty* Property)
{
    if (Property->IsA<FBoolProperty>())
    {
        return TEXT("bool");
    }
    else if (Property->IsA<FFloatProperty>() || Property->IsA<FDoubleProperty>())
    {
        return TEXT("float");
    }
    else if (Property->IsA<FIntProperty>())
    {
        return TEXT("int");
    }
    else if (Property->IsA<FByteProperty>())
    {
        return TEXT("byte");
    }
    else if (CastField<FEnumProperty>(Property))
    {
        return TEXT("enum");
    }
    else if (CastField<FStructProperty>(Property))
    {
        return TEXT("struct");
    }
    else if (CastField<FObjectPropertyBase>(Property))
    {
        return TEXT("object");
    }
    else if (Property->IsA<FStrProperty>())
    {
        return TEXT("string");
    }
    else
    {
        return Property->GetCPPType();
    }
}

// Helper function to find enum value by name, trying multiple variations
// Supports: "BLEND_Masked", "Masked", "masked", "EBlendMode::BLEND_Masked"
static int64 FindEnumValueFlexible(UEnum* Enum, const FString& ValueName)
{
    if (!Enum)
    {
        return INDEX_NONE;
    }
    
    // Try exact match first
    int64 Value = Enum->GetValueByNameString(ValueName);
    if (Value != INDEX_NONE)
    {
        return Value;
    }
    
    // Get the enum prefix (e.g., "BLEND_" from "BLEND_Opaque")
    FString EnumPrefix;
    if (Enum->NumEnums() > 0)
    {
        FString FirstName = Enum->GetNameStringByIndex(0);
        int32 UnderscoreIndex;
        if (FirstName.FindChar('_', UnderscoreIndex))
        {
            EnumPrefix = FirstName.Left(UnderscoreIndex + 1);  // Include the underscore
        }
    }
    
    // Try with prefix prepended (e.g., "Masked" -> "BLEND_Masked")
    if (!EnumPrefix.IsEmpty())
    {
        Value = Enum->GetValueByNameString(EnumPrefix + ValueName);
        if (Value != INDEX_NONE)
        {
            return Value;
        }
    }
    
    // Try case-insensitive search through all enum values
    for (int32 i = 0; i < Enum->NumEnums(); ++i)
    {
        FString EnumValueName = Enum->GetNameStringByIndex(i);
        
        // Exact case-insensitive match
        if (EnumValueName.Equals(ValueName, ESearchCase::IgnoreCase))
        {
            return Enum->GetValueByIndex(i);
        }
        
        // Check if the value after the prefix matches (e.g., "Masked" matches "BLEND_Masked")
        if (!EnumPrefix.IsEmpty() && EnumValueName.StartsWith(EnumPrefix))
        {
            FString ShortName = EnumValueName.Mid(EnumPrefix.Len());
            if (ShortName.Equals(ValueName, ESearchCase::IgnoreCase))
            {
                return Enum->GetValueByIndex(i);
            }
        }
    }
    
    return INDEX_NONE;
}

FMaterialService::FMaterialService(TSharedPtr<FServiceContext> InContext)
    : FServiceBase(InContext)
{
}

//-----------------------------------------------------------------------------
// Material Lifecycle
//-----------------------------------------------------------------------------

TResult<FString> FMaterialService::CreateMaterial(const FMaterialCreateParams& Params)
{
    // Validate parameters
    if (Params.DestinationPath.IsEmpty())
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::PARAM_EMPTY, TEXT("DestinationPath cannot be empty"));
    }
    if (Params.MaterialName.IsEmpty())
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::PARAM_EMPTY, TEXT("MaterialName cannot be empty"));
    }

    // Construct full asset path
    FString PackagePath = Params.DestinationPath / Params.MaterialName;
    FString PackageName = PackagePath;
    
    // Ensure path starts with /Game/
    if (!PackageName.StartsWith(TEXT("/Game/")))
    {
        if (PackageName.StartsWith(TEXT("/")))
        {
            PackageName = TEXT("/Game") + PackageName;
        }
        else
        {
            PackageName = TEXT("/Game/") + PackageName;
        }
    }

    // Create the package
    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::ASSET_CREATE_FAILED, 
            FString::Printf(TEXT("Failed to create package: %s"), *PackageName));
    }

    // Create material using factory
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* NewMaterial = Cast<UMaterial>(Factory->FactoryCreateNew(
        UMaterial::StaticClass(),
        Package,
        *Params.MaterialName,
        RF_Public | RF_Standalone,
        nullptr,
        GWarn
    ));

    if (!NewMaterial)
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::ASSET_CREATE_FAILED, 
            FString::Printf(TEXT("Failed to create material: %s"), *Params.MaterialName));
    }

    // Apply initial properties if provided
    for (const auto& PropPair : Params.InitialProperties)
    {
        auto SetResult = SetProperty(PackageName, PropPair.Key, PropPair.Value);
        if (!SetResult.IsSuccess())
        {
            UE_LOG(LogMaterialService, Warning, TEXT("Failed to set initial property %s: %s"), 
                *PropPair.Key, *SetResult.GetErrorMessage());
        }
    }

    // Mark package dirty
    Package->MarkPackageDirty();

    // Notify asset registry
    FAssetRegistryModule::AssetCreated(NewMaterial);

    UE_LOG(LogMaterialService, Log, TEXT("Created material: %s"), *PackageName);
    
    return TResult<FString>::Success(PackageName);
}

TResult<FString> FMaterialService::CreateMaterialInstance(const FMaterialInstanceCreateParams& Params)
{
    // Validate parameters
    if (Params.ParentMaterialPath.IsEmpty())
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::PARAM_EMPTY, TEXT("ParentMaterialPath cannot be empty"));
    }
    if (Params.DestinationPath.IsEmpty())
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::PARAM_EMPTY, TEXT("DestinationPath cannot be empty"));
    }
    if (Params.InstanceName.IsEmpty())
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::PARAM_EMPTY, TEXT("InstanceName cannot be empty"));
    }

    // Load the parent material
    UMaterialInterface* ParentMaterial = LoadObject<UMaterialInterface>(nullptr, *Params.ParentMaterialPath);
    if (!ParentMaterial)
    {
        // Try with suffix
        FString PathWithSuffix = Params.ParentMaterialPath + TEXT(".") + FPackageName::GetShortName(Params.ParentMaterialPath);
        ParentMaterial = LoadObject<UMaterialInterface>(nullptr, *PathWithSuffix);
    }
    
    if (!ParentMaterial)
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND, 
            FString::Printf(TEXT("Parent material not found: %s"), *Params.ParentMaterialPath));
    }

    // Construct full asset path
    FString PackagePath = Params.DestinationPath / Params.InstanceName;
    FString PackageName = PackagePath;
    
    // Ensure path starts with /Game/
    if (!PackageName.StartsWith(TEXT("/Game/")))
    {
        if (PackageName.StartsWith(TEXT("/")))
        {
            PackageName = TEXT("/Game") + PackageName;
        }
        else
        {
            PackageName = TEXT("/Game/") + PackageName;
        }
    }

    // Create the package
    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::ASSET_CREATE_FAILED, 
            FString::Printf(TEXT("Failed to create package: %s"), *PackageName));
    }

    // Create the Material Instance Constant
    UMaterialInstanceConstant* NewInstance = NewObject<UMaterialInstanceConstant>(
        Package,
        *Params.InstanceName,
        RF_Public | RF_Standalone
    );

    if (!NewInstance)
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::ASSET_CREATE_FAILED, 
            FString::Printf(TEXT("Failed to create material instance: %s"), *Params.InstanceName));
    }

    // Set the parent material
    NewInstance->SetParentEditorOnly(ParentMaterial);

    // Apply scalar parameter overrides
    for (const auto& Param : Params.ScalarParameters)
    {
        NewInstance->SetScalarParameterValueEditorOnly(FName(*Param.Key), Param.Value);
        UE_LOG(LogMaterialService, Log, TEXT("Set scalar parameter %s = %f"), *Param.Key, Param.Value);
    }

    // Apply vector parameter overrides
    for (const auto& Param : Params.VectorParameters)
    {
        NewInstance->SetVectorParameterValueEditorOnly(FName(*Param.Key), Param.Value);
        UE_LOG(LogMaterialService, Log, TEXT("Set vector parameter %s"), *Param.Key);
    }

    // Apply texture parameter overrides
    for (const auto& Param : Params.TextureParameters)
    {
        UTexture* Texture = LoadObject<UTexture>(nullptr, *Param.Value);
        if (Texture)
        {
            NewInstance->SetTextureParameterValueEditorOnly(FName(*Param.Key), Texture);
            UE_LOG(LogMaterialService, Log, TEXT("Set texture parameter %s = %s"), *Param.Key, *Param.Value);
        }
        else
        {
            UE_LOG(LogMaterialService, Warning, TEXT("Failed to load texture for parameter %s: %s"), *Param.Key, *Param.Value);
        }
    }

    // Mark package dirty
    Package->MarkPackageDirty();

    // Notify asset registry
    FAssetRegistryModule::AssetCreated(NewInstance);

    UE_LOG(LogMaterialService, Log, TEXT("Created material instance: %s (parent: %s)"), *PackageName, *Params.ParentMaterialPath);
    
    return TResult<FString>::Success(PackageName);
}

TResult<UMaterial*> FMaterialService::LoadMaterial(const FString& MaterialPath)
{
    if (MaterialPath.IsEmpty())
    {
        return TResult<UMaterial*>::Error(VibeUE::ErrorCodes::PARAM_EMPTY, TEXT("MaterialPath cannot be empty"));
    }

    // Try to load the material
    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
    {
        // Try with .MaterialName suffix
        FString PathWithSuffix = MaterialPath + TEXT(".") + FPackageName::GetShortName(MaterialPath);
        Material = LoadObject<UMaterial>(nullptr, *PathWithSuffix);
    }

    if (!Material)
    {
        return TResult<UMaterial*>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND, 
            FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
    }

    return TResult<UMaterial*>::Success(Material);
}

TResult<UMaterialInterface*> FMaterialService::LoadMaterialInterface(const FString& MaterialPath)
{
    if (MaterialPath.IsEmpty())
    {
        return TResult<UMaterialInterface*>::Error(VibeUE::ErrorCodes::PARAM_EMPTY, TEXT("MaterialPath cannot be empty"));
    }

    // Try to load as UMaterialInterface (base class for both UMaterial and UMaterialInstance)
    UMaterialInterface* MaterialInterface = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
    if (!MaterialInterface)
    {
        // Try with .AssetName suffix
        FString PathWithSuffix = MaterialPath + TEXT(".") + FPackageName::GetShortName(MaterialPath);
        MaterialInterface = LoadObject<UMaterialInterface>(nullptr, *PathWithSuffix);
    }

    if (!MaterialInterface)
    {
        return TResult<UMaterialInterface*>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND, 
            FString::Printf(TEXT("Material or Material Instance not found: %s"), *MaterialPath));
    }

    return TResult<UMaterialInterface*>::Success(MaterialInterface);
}

TResult<UMaterialInstanceConstant*> FMaterialService::LoadMaterialInstance(const FString& InstancePath)
{
    if (InstancePath.IsEmpty())
    {
        return TResult<UMaterialInstanceConstant*>::Error(VibeUE::ErrorCodes::PARAM_EMPTY, TEXT("InstancePath cannot be empty"));
    }

    // Try to load as UMaterialInstanceConstant
    UMaterialInstanceConstant* MaterialInstance = LoadObject<UMaterialInstanceConstant>(nullptr, *InstancePath);
    if (!MaterialInstance)
    {
        // Try with .AssetName suffix
        FString PathWithSuffix = InstancePath + TEXT(".") + FPackageName::GetShortName(InstancePath);
        MaterialInstance = LoadObject<UMaterialInstanceConstant>(nullptr, *PathWithSuffix);
    }

    if (!MaterialInstance)
    {
        return TResult<UMaterialInstanceConstant*>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND, 
            FString::Printf(TEXT("Material Instance not found: %s"), *InstancePath));
    }

    return TResult<UMaterialInstanceConstant*>::Success(MaterialInstance);
}

TResult<void> FMaterialService::SaveMaterial(const FString& MaterialPath)
{
    auto LoadResult = LoadMaterial(MaterialPath);
    if (!LoadResult.IsSuccess())
    {
        return TResult<void>::Error(LoadResult.GetErrorCode(), LoadResult.GetErrorMessage());
    }

    UMaterial* Material = LoadResult.GetValue();
    UPackage* Package = Material->GetOutermost();

    // Build filename for save
    FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());

    // Save the package
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.Error = GWarn;
    
    FSavePackageResultStruct Result = UPackage::Save(Package, Material, *PackageFileName, SaveArgs);
    
    if (Result.Result != ESavePackageResult::Success)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::ASSET_SAVE_FAILED, 
            FString::Printf(TEXT("Failed to save material: %s"), *MaterialPath));
    }

    UE_LOG(LogMaterialService, Log, TEXT("Saved material: %s"), *MaterialPath);
    return TResult<void>::Success();
}

TResult<void> FMaterialService::CompileMaterial(const FString& MaterialPath)
{
    auto LoadResult = LoadMaterial(MaterialPath);
    if (!LoadResult.IsSuccess())
    {
        return TResult<void>::Error(LoadResult.GetErrorCode(), LoadResult.GetErrorMessage());
    }

    UMaterial* Material = LoadResult.GetValue();
    
    // Force recompilation
    Material->PreEditChange(nullptr);
    Material->PostEditChange();

    UE_LOG(LogMaterialService, Log, TEXT("Compiled material: %s"), *MaterialPath);
    return TResult<void>::Success();
}

TResult<void> FMaterialService::RefreshMaterialEditor(const FString& MaterialPath)
{
    auto LoadResult = LoadMaterial(MaterialPath);
    if (!LoadResult.IsSuccess())
    {
        return TResult<void>::Error(LoadResult.GetErrorCode(), LoadResult.GetErrorMessage());
    }

    UMaterial* Material = LoadResult.GetValue();
#if WITH_EDITOR
    if (!GEditor)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::EDITOR_NOT_AVAILABLE, TEXT("GEditor is not available"));
    }

    // Force save the package to disk first to ensure changes are persisted
    UPackage* Package = Material->GetOutermost();
    FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.Error = GWarn;
    UPackage::Save(Package, Material, *PackageFileName, SaveArgs);

    // Close and reopen the material editor to force complete UI refresh
    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (AssetEditorSubsystem)
    {
        // Close the editor if it's open
        AssetEditorSubsystem->CloseAllEditorsForAsset(Material);
        
        // Wait for close to complete and save to flush
        FPlatformProcess::Sleep(0.25f);
        
        // Reopen the editor - this will reload from disk with fresh data
        AssetEditorSubsystem->OpenEditorForAsset(Material);
        
        UE_LOG(LogMaterialService, Log, TEXT("Reopened Material Editor for: %s"), *MaterialPath);
    }
    
    // Refresh all viewports to show updated preview
    FEditorSupportDelegates::RedrawAllViewports.Broadcast();
    
    UE_LOG(LogMaterialService, Log, TEXT("Refreshed material editor for: %s"), *MaterialPath);
#else
    UE_LOG(LogMaterialService, Verbose, TEXT("RefreshMaterialEditor called without editor support."));
#endif

    return TResult<void>::Success();
}

//-----------------------------------------------------------------------------
// Material Information
//-----------------------------------------------------------------------------

TResult<FMaterialInfo> FMaterialService::GetMaterialInfo(const FString& MaterialPath)
{
    auto LoadResult = LoadMaterial(MaterialPath);
    if (!LoadResult.IsSuccess())
    {
        return TResult<FMaterialInfo>::Error(LoadResult.GetErrorCode(), LoadResult.GetErrorMessage());
    }

    UMaterial* Material = LoadResult.GetValue();
    FMaterialInfo Info;

    Info.AssetPath = MaterialPath;
    Info.Name = Material->GetName();
    
    // Get domain
    switch (Material->MaterialDomain)
    {
        case MD_Surface: Info.MaterialDomain = TEXT("Surface"); break;
        case MD_DeferredDecal: Info.MaterialDomain = TEXT("DeferredDecal"); break;
        case MD_LightFunction: Info.MaterialDomain = TEXT("LightFunction"); break;
        case MD_Volume: Info.MaterialDomain = TEXT("Volume"); break;
        case MD_PostProcess: Info.MaterialDomain = TEXT("PostProcess"); break;
        case MD_UI: Info.MaterialDomain = TEXT("UI"); break;
        default: Info.MaterialDomain = TEXT("Unknown"); break;
    }

    // Get blend mode
    switch (Material->BlendMode)
    {
        case BLEND_Opaque: Info.BlendMode = TEXT("Opaque"); break;
        case BLEND_Masked: Info.BlendMode = TEXT("Masked"); break;
        case BLEND_Translucent: Info.BlendMode = TEXT("Translucent"); break;
        case BLEND_Additive: Info.BlendMode = TEXT("Additive"); break;
        case BLEND_Modulate: Info.BlendMode = TEXT("Modulate"); break;
        case BLEND_AlphaComposite: Info.BlendMode = TEXT("AlphaComposite"); break;
        case BLEND_AlphaHoldout: Info.BlendMode = TEXT("AlphaHoldout"); break;
        default: Info.BlendMode = TEXT("Unknown"); break;
    }

    // Get shading model (primary)
    Info.ShadingModel = TEXT("DefaultLit"); // Default
    FMaterialShadingModelField ShadingModels = Material->GetShadingModels();
    if (ShadingModels.HasShadingModel(MSM_Unlit)) Info.ShadingModel = TEXT("Unlit");
    else if (ShadingModels.HasShadingModel(MSM_Subsurface)) Info.ShadingModel = TEXT("Subsurface");
    else if (ShadingModels.HasShadingModel(MSM_SubsurfaceProfile)) Info.ShadingModel = TEXT("SubsurfaceProfile");
    else if (ShadingModels.HasShadingModel(MSM_ClearCoat)) Info.ShadingModel = TEXT("ClearCoat");
    else if (ShadingModels.HasShadingModel(MSM_TwoSidedFoliage)) Info.ShadingModel = TEXT("TwoSidedFoliage");
    else if (ShadingModels.HasShadingModel(MSM_Hair)) Info.ShadingModel = TEXT("Hair");
    else if (ShadingModels.HasShadingModel(MSM_Cloth)) Info.ShadingModel = TEXT("Cloth");
    else if (ShadingModels.HasShadingModel(MSM_Eye)) Info.ShadingModel = TEXT("Eye");

    Info.bTwoSided = Material->TwoSided;

    // Count expressions
#if WITH_EDITORONLY_DATA
    if (UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData())
    {
        Info.ExpressionCount = EditorData->ExpressionCollection.Expressions.Num();
        
        // Count texture samples and parameters
        for (UMaterialExpression* Expr : EditorData->ExpressionCollection.Expressions)
        {
            if (Expr)
            {
                if (Expr->IsA<UMaterialExpressionTextureBase>())
                {
                    Info.TextureSampleCount++;
                }
                if (Expr->bIsParameterExpression)
                {
                    Info.ParameterCount++;
                    // Get parameter name
                    if (UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(Expr))
                    {
                        Info.ParameterNames.Add(ParamExpr->ParameterName.ToString());
                    }
                }
            }
        }
    }
#endif

    // Get all properties
    auto PropsResult = ListProperties(MaterialPath, true);
    if (PropsResult.IsSuccess())
    {
        Info.Properties = PropsResult.GetValue();
    }

    return TResult<FMaterialInfo>::Success(Info);
}

//-----------------------------------------------------------------------------
// Material Instance Information
//-----------------------------------------------------------------------------

TResult<FMaterialInfo> FMaterialService::GetInstanceInfo(const FString& InstancePath)
{
    auto LoadResult = LoadMaterialInstance(InstancePath);
    if (!LoadResult.IsSuccess())
    {
        return TResult<FMaterialInfo>::Error(LoadResult.GetErrorCode(), LoadResult.GetErrorMessage());
    }

    UMaterialInstanceConstant* MaterialInstance = LoadResult.GetValue();
    FMaterialInfo Info;

    Info.AssetPath = InstancePath;
    Info.Name = MaterialInstance->GetName();
    
    // Get parent material info
    if (UMaterialInterface* Parent = MaterialInstance->Parent)
    {
        Info.MaterialDomain = TEXT("Instance");  // Mark as instance
        
        // Get info from parent material
        if (UMaterial* ParentMaterial = Parent->GetMaterial())
        {
            switch (ParentMaterial->MaterialDomain)
            {
                case MD_Surface: Info.MaterialDomain = TEXT("Surface (Instance)"); break;
                case MD_DeferredDecal: Info.MaterialDomain = TEXT("DeferredDecal (Instance)"); break;
                case MD_LightFunction: Info.MaterialDomain = TEXT("LightFunction (Instance)"); break;
                case MD_Volume: Info.MaterialDomain = TEXT("Volume (Instance)"); break;
                case MD_PostProcess: Info.MaterialDomain = TEXT("PostProcess (Instance)"); break;
                case MD_UI: Info.MaterialDomain = TEXT("UI (Instance)"); break;
                default: Info.MaterialDomain = TEXT("Unknown (Instance)"); break;
            }
            
            switch (ParentMaterial->BlendMode)
            {
                case BLEND_Opaque: Info.BlendMode = TEXT("Opaque"); break;
                case BLEND_Masked: Info.BlendMode = TEXT("Masked"); break;
                case BLEND_Translucent: Info.BlendMode = TEXT("Translucent"); break;
                case BLEND_Additive: Info.BlendMode = TEXT("Additive"); break;
                case BLEND_Modulate: Info.BlendMode = TEXT("Modulate"); break;
                case BLEND_AlphaComposite: Info.BlendMode = TEXT("AlphaComposite"); break;
                case BLEND_AlphaHoldout: Info.BlendMode = TEXT("AlphaHoldout"); break;
                default: Info.BlendMode = TEXT("Unknown"); break;
            }
            
            Info.bTwoSided = ParentMaterial->TwoSided;
        }
    }
    
    // Store parent path
    if (MaterialInstance->Parent)
    {
        // Store parent info in a parameter name (reusing the field)
        Info.ParameterNames.Add(FString::Printf(TEXT("Parent: %s"), *MaterialInstance->Parent->GetPathName()));
    }

    // Count overridden parameters
    Info.ParameterCount = 0;
    
    // Scalar parameters (iterate over current overrides)
    for (const FScalarParameterValue& Param : MaterialInstance->ScalarParameterValues)
    {
        Info.ParameterCount++;
        Info.ParameterNames.Add(FString::Printf(TEXT("Scalar: %s = %f"), *Param.ParameterInfo.Name.ToString(), Param.ParameterValue));
    }
    
    // Vector parameters
    for (const FVectorParameterValue& Param : MaterialInstance->VectorParameterValues)
    {
        Info.ParameterCount++;
        Info.ParameterNames.Add(FString::Printf(TEXT("Vector: %s = (%f, %f, %f, %f)"), 
            *Param.ParameterInfo.Name.ToString(),
            Param.ParameterValue.R, Param.ParameterValue.G, Param.ParameterValue.B, Param.ParameterValue.A));
    }
    
    // Texture parameters
    for (const FTextureParameterValue& Param : MaterialInstance->TextureParameterValues)
    {
        Info.ParameterCount++;
        FString TexturePath = Param.ParameterValue ? Param.ParameterValue->GetPathName() : TEXT("None");
        Info.ParameterNames.Add(FString::Printf(TEXT("Texture: %s = %s"), *Param.ParameterInfo.Name.ToString(), *TexturePath));
    }

    // Get all properties
    auto PropsResult = ListInstanceProperties(InstancePath, true);
    if (PropsResult.IsSuccess())
    {
        Info.Properties = PropsResult.GetValue();
    }

    return TResult<FMaterialInfo>::Success(Info);
}

TResult<TArray<FMaterialPropertyInfo>> FMaterialService::ListInstanceProperties(const FString& InstancePath, bool bIncludeAdvanced)
{
    auto LoadResult = LoadMaterialInstance(InstancePath);
    if (!LoadResult.IsSuccess())
    {
        return TResult<TArray<FMaterialPropertyInfo>>::Error(LoadResult.GetErrorCode(), LoadResult.GetErrorMessage());
    }

    UMaterialInstanceConstant* MaterialInstance = LoadResult.GetValue();
    TArray<FMaterialPropertyInfo> Properties;

    // Iterate over all properties using reflection
    for (TFieldIterator<FProperty> PropIt(UMaterialInstanceConstant::StaticClass()); PropIt; ++PropIt)
    {
        FProperty* Property = *PropIt;
        
        // Skip if not editable
        if (!Property->HasAnyPropertyFlags(CPF_Edit))
        {
            continue;
        }

        // Skip advanced properties if not requested
        bool bIsAdvanced = Property->HasMetaData(TEXT("AdvancedDisplay"));
        if (bIsAdvanced && !bIncludeAdvanced)
        {
            continue;
        }

        FMaterialPropertyInfo PropInfo;
        PropInfo.Name = Property->GetName();
        PropInfo.DisplayName = Property->GetDisplayNameText().ToString();
        PropInfo.Type = GetPropertyTypeName(Property);
        PropInfo.Category = Property->GetMetaData(TEXT("Category"));
        PropInfo.bIsEditable = true;
        PropInfo.bIsAdvanced = bIsAdvanced;
        
        // Get tooltip
        PropInfo.Tooltip = Property->GetMetaData(TEXT("Tooltip"));

        // Get current value as string
        void* ValuePtr = Property->ContainerPtrToValuePtr<void>(MaterialInstance);
        Property->ExportTextItem_Direct(PropInfo.CurrentValue, ValuePtr, nullptr, nullptr, PPF_None);

        // Get allowed values for enums
        if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
        {
            if (UEnum* Enum = EnumProp->GetEnum())
            {
                for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
                {
                    PropInfo.AllowedValues.Add(Enum->GetNameStringByIndex(i));
                }
            }
        }
        else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
        {
            if (UEnum* Enum = ByteProp->GetIntPropertyEnum())
            {
                for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
                {
                    PropInfo.AllowedValues.Add(Enum->GetNameStringByIndex(i));
                }
            }
        }

        // Handle struct properties
        if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
        {
            PropInfo.ObjectClass = StructProp->Struct->GetName();
            
            // Get struct members
            for (TFieldIterator<FProperty> MemberIt(StructProp->Struct); MemberIt; ++MemberIt)
            {
                FProperty* MemberProp = *MemberIt;
                FStructMemberInfo MemberInfo;
                MemberInfo.Name = MemberProp->GetName();
                MemberInfo.Type = GetPropertyTypeName(MemberProp);
                
                void* MemberValuePtr = MemberProp->ContainerPtrToValuePtr<void>(ValuePtr);
                MemberProp->ExportTextItem_Direct(MemberInfo.CurrentValue, MemberValuePtr, nullptr, nullptr, PPF_None);
                
                PropInfo.StructMembers.Add(MemberInfo);
            }
        }

        // Handle object properties
        if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
        {
            PropInfo.ObjectClass = ObjProp->PropertyClass->GetName();
        }

        Properties.Add(PropInfo);
    }

    return TResult<TArray<FMaterialPropertyInfo>>::Success(Properties);
}

TResult<FString> FMaterialService::GetInstanceProperty(const FString& InstancePath, const FString& PropertyName)
{
    auto LoadResult = LoadMaterialInstance(InstancePath);
    if (!LoadResult.IsSuccess())
    {
        return TResult<FString>::Error(LoadResult.GetErrorCode(), LoadResult.GetErrorMessage());
    }

    UMaterialInstanceConstant* MaterialInstance = LoadResult.GetValue();
    
    // Find the property
    FProperty* Property = UMaterialInstanceConstant::StaticClass()->FindPropertyByName(FName(*PropertyName));
    if (!Property)
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::PARAM_INVALID, 
            FString::Printf(TEXT("Property not found: %s"), *PropertyName));
    }

    // Get value as string
    FString Value;
    void* ValuePtr = Property->ContainerPtrToValuePtr<void>(MaterialInstance);
    Property->ExportTextItem_Direct(Value, ValuePtr, nullptr, nullptr, PPF_None);

    return TResult<FString>::Success(Value);
}

TResult<void> FMaterialService::SetInstanceProperty(const FString& InstancePath, const FString& PropertyName, const FString& Value)
{
    auto LoadResult = LoadMaterialInstance(InstancePath);
    if (!LoadResult.IsSuccess())
    {
        return TResult<void>::Error(LoadResult.GetErrorCode(), LoadResult.GetErrorMessage());
    }

    UMaterialInstanceConstant* MaterialInstance = LoadResult.GetValue();
    
    // Find the property
    FProperty* Property = UMaterialInstanceConstant::StaticClass()->FindPropertyByName(FName(*PropertyName));
    if (!Property)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::PARAM_INVALID, 
            FString::Printf(TEXT("Property not found: %s"), *PropertyName));
    }

    // Check if editable
    if (!Property->HasAnyPropertyFlags(CPF_Edit))
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::PARAM_INVALID, 
            FString::Printf(TEXT("Property is not editable: %s"), *PropertyName));
    }

    // Notify pre-edit
    MaterialInstance->PreEditChange(Property);

    // Set value from string
    void* ValuePtr = Property->ContainerPtrToValuePtr<void>(MaterialInstance);
    Property->ImportText_Direct(*Value, ValuePtr, MaterialInstance, PPF_None);

    // Notify post-edit
    FPropertyChangedEvent ChangedEvent(Property);
    MaterialInstance->PostEditChangeProperty(ChangedEvent);
    
    // Mark package dirty
    MaterialInstance->MarkPackageDirty();

    UE_LOG(LogMaterialService, Log, TEXT("Set instance property %s = %s on %s"), *PropertyName, *Value, *InstancePath);
    return TResult<void>::Success();
}

//-----------------------------------------------------------------------------
// Material Instance Parameters
//-----------------------------------------------------------------------------

TResult<TArray<FVibeMaterialParamInfo>> FMaterialService::ListInstanceParameters(const FString& InstancePath)
{
    auto LoadResult = LoadMaterialInstance(InstancePath);
    if (!LoadResult.IsSuccess())
    {
        return TResult<TArray<FVibeMaterialParamInfo>>::Error(LoadResult.GetErrorCode(), LoadResult.GetErrorMessage());
    }

    UMaterialInstanceConstant* MaterialInstance = LoadResult.GetValue();
    TArray<FVibeMaterialParamInfo> Parameters;

    // Get all available parameters from the parent material
    TArray<FMaterialParameterInfo> AllScalarParams;
    TArray<FGuid> ScalarGuids;
    MaterialInstance->GetAllScalarParameterInfo(AllScalarParams, ScalarGuids);
    
    for (const FMaterialParameterInfo& UEParamInfo : AllScalarParams)
    {
        FVibeMaterialParamInfo Info;
        Info.Name = UEParamInfo.Name.ToString();
        Info.Type = TEXT("Scalar");
        
        // Get current value
        float ScalarValue = 0.0f;
        if (MaterialInstance->GetScalarParameterValue(UEParamInfo.Name, ScalarValue))
        {
            Info.CurrentValue = FString::SanitizeFloat(ScalarValue);
        }
        
        // Check if overridden in this instance
        for (const FScalarParameterValue& Override : MaterialInstance->ScalarParameterValues)
        {
            if (Override.ParameterInfo.Name == UEParamInfo.Name)
            {
                Info.CurrentValue = FString::SanitizeFloat(Override.ParameterValue);
                break;
            }
        }
        
        Parameters.Add(Info);
    }

    // Vector parameters
    TArray<FMaterialParameterInfo> AllVectorParams;
    TArray<FGuid> VectorGuids;
    MaterialInstance->GetAllVectorParameterInfo(AllVectorParams, VectorGuids);
    
    for (const FMaterialParameterInfo& UEParamInfo : AllVectorParams)
    {
        FVibeMaterialParamInfo Info;
        Info.Name = UEParamInfo.Name.ToString();
        Info.Type = TEXT("Vector");
        
        // Get current value
        FLinearColor VectorValue = FLinearColor::Black;
        if (MaterialInstance->GetVectorParameterValue(UEParamInfo.Name, VectorValue))
        {
            Info.CurrentValue = FString::Printf(TEXT("(%f,%f,%f,%f)"), VectorValue.R, VectorValue.G, VectorValue.B, VectorValue.A);
        }
        
        // Check if overridden in this instance
        for (const FVectorParameterValue& Override : MaterialInstance->VectorParameterValues)
        {
            if (Override.ParameterInfo.Name == UEParamInfo.Name)
            {
                Info.CurrentValue = FString::Printf(TEXT("(%f,%f,%f,%f)"), 
                    Override.ParameterValue.R, Override.ParameterValue.G, Override.ParameterValue.B, Override.ParameterValue.A);
                break;
            }
        }
        
        Parameters.Add(Info);
    }

    // Texture parameters
    TArray<FMaterialParameterInfo> AllTextureParams;
    TArray<FGuid> TextureGuids;
    MaterialInstance->GetAllTextureParameterInfo(AllTextureParams, TextureGuids);
    
    for (const FMaterialParameterInfo& UEParamInfo : AllTextureParams)
    {
        FVibeMaterialParamInfo Info;
        Info.Name = UEParamInfo.Name.ToString();
        Info.Type = TEXT("Texture");
        
        // Get current value
        UTexture* TextureValue = nullptr;
        if (MaterialInstance->GetTextureParameterValue(UEParamInfo.Name, TextureValue))
        {
            Info.CurrentValue = TextureValue ? TextureValue->GetPathName() : TEXT("None");
        }
        
        // Check if overridden in this instance
        for (const FTextureParameterValue& Override : MaterialInstance->TextureParameterValues)
        {
            if (Override.ParameterInfo.Name == UEParamInfo.Name)
            {
                Info.CurrentValue = Override.ParameterValue ? Override.ParameterValue->GetPathName() : TEXT("None");
                break;
            }
        }
        
        Parameters.Add(Info);
    }

    return TResult<TArray<FVibeMaterialParamInfo>>::Success(Parameters);
}

TResult<void> FMaterialService::SetInstanceScalarParameter(const FString& InstancePath, const FString& ParameterName, float Value)
{
    auto LoadResult = LoadMaterialInstance(InstancePath);
    if (!LoadResult.IsSuccess())
    {
        return TResult<void>::Error(LoadResult.GetErrorCode(), LoadResult.GetErrorMessage());
    }

    UMaterialInstanceConstant* MaterialInstance = LoadResult.GetValue();
    
    // Verify the parameter exists in the parent material
    TArray<FMaterialParameterInfo> AllScalarParams;
    TArray<FGuid> ScalarGuids;
    MaterialInstance->GetAllScalarParameterInfo(AllScalarParams, ScalarGuids);
    
    bool bParameterExists = false;
    for (const FMaterialParameterInfo& ParamInfo : AllScalarParams)
    {
        if (ParamInfo.Name.ToString() == ParameterName)
        {
            bParameterExists = true;
            break;
        }
    }
    
    if (!bParameterExists)
    {
        // List available parameters to help the user
        TArray<FString> AvailableParams;
        for (const FMaterialParameterInfo& ParamInfo : AllScalarParams)
        {
            AvailableParams.Add(ParamInfo.Name.ToString());
        }
        
        FString AvailableStr = AvailableParams.Num() > 0 
            ? FString::Printf(TEXT("Available: %s"), *FString::Join(AvailableParams, TEXT(", ")))
            : TEXT("The parent material has NO scalar parameters exposed");
            
        return TResult<void>::Error(VibeUE::ErrorCodes::PARAM_INVALID,
            FString::Printf(TEXT("CANNOT SET: Scalar parameter '%s' does not exist in parent material. %s. "
                "This operation will ALWAYS FAIL until the parent material is modified to add this parameter. "
                "To add parameters: use manage_material_node to create a ScalarParameter node and connect it to a material output."),
                *ParameterName, *AvailableStr));
    }
    
    // Set the parameter value
    MaterialInstance->SetScalarParameterValueEditorOnly(FName(*ParameterName), Value);
    
    // Mark package dirty
    MaterialInstance->MarkPackageDirty();

    UE_LOG(LogMaterialService, Log, TEXT("Set scalar parameter %s = %f on %s"), *ParameterName, Value, *InstancePath);
    return TResult<void>::Success();
}

TResult<void> FMaterialService::SetInstanceVectorParameter(const FString& InstancePath, const FString& ParameterName, const FLinearColor& Value)
{
    auto LoadResult = LoadMaterialInstance(InstancePath);
    if (!LoadResult.IsSuccess())
    {
        return TResult<void>::Error(LoadResult.GetErrorCode(), LoadResult.GetErrorMessage());
    }

    UMaterialInstanceConstant* MaterialInstance = LoadResult.GetValue();
    
    // Verify the parameter exists in the parent material
    TArray<FMaterialParameterInfo> AllVectorParams;
    TArray<FGuid> VectorGuids;
    MaterialInstance->GetAllVectorParameterInfo(AllVectorParams, VectorGuids);
    
    bool bParameterExists = false;
    for (const FMaterialParameterInfo& ParamInfo : AllVectorParams)
    {
        if (ParamInfo.Name.ToString() == ParameterName)
        {
            bParameterExists = true;
            break;
        }
    }
    
    if (!bParameterExists)
    {
        // List available parameters to help the user
        TArray<FString> AvailableParams;
        for (const FMaterialParameterInfo& ParamInfo : AllVectorParams)
        {
            AvailableParams.Add(ParamInfo.Name.ToString());
        }
        
        FString AvailableStr = AvailableParams.Num() > 0 
            ? FString::Printf(TEXT("Available: %s"), *FString::Join(AvailableParams, TEXT(", ")))
            : TEXT("The parent material has NO vector/color parameters exposed");
            
        return TResult<void>::Error(VibeUE::ErrorCodes::PARAM_INVALID,
            FString::Printf(TEXT("CANNOT SET: Vector parameter '%s' does not exist in parent material. %s. "
                "This operation will ALWAYS FAIL until the parent material is modified to add this parameter. "
                "To add parameters: use manage_material_node to create a VectorParameter node and connect it to a material output."),
                *ParameterName, *AvailableStr));
    }
    
    // Set the parameter value
    MaterialInstance->SetVectorParameterValueEditorOnly(FName(*ParameterName), Value);
    
    // Mark package dirty
    MaterialInstance->MarkPackageDirty();

    UE_LOG(LogMaterialService, Log, TEXT("Set vector parameter %s = (%f,%f,%f,%f) on %s"), 
        *ParameterName, Value.R, Value.G, Value.B, Value.A, *InstancePath);
    return TResult<void>::Success();
}

TResult<void> FMaterialService::SetInstanceTextureParameter(const FString& InstancePath, const FString& ParameterName, const FString& TexturePath)
{
    auto LoadResult = LoadMaterialInstance(InstancePath);
    if (!LoadResult.IsSuccess())
    {
        return TResult<void>::Error(LoadResult.GetErrorCode(), LoadResult.GetErrorMessage());
    }

    UMaterialInstanceConstant* MaterialInstance = LoadResult.GetValue();
    
    // Verify the parameter exists in the parent material
    TArray<FMaterialParameterInfo> AllTextureParams;
    TArray<FGuid> TextureGuids;
    MaterialInstance->GetAllTextureParameterInfo(AllTextureParams, TextureGuids);
    
    bool bParameterExists = false;
    for (const FMaterialParameterInfo& ParamInfo : AllTextureParams)
    {
        if (ParamInfo.Name.ToString() == ParameterName)
        {
            bParameterExists = true;
            break;
        }
    }
    
    if (!bParameterExists)
    {
        // List available parameters to help the user
        TArray<FString> AvailableParams;
        for (const FMaterialParameterInfo& ParamInfo : AllTextureParams)
        {
            AvailableParams.Add(ParamInfo.Name.ToString());
        }
        
        FString AvailableStr = AvailableParams.Num() > 0 
            ? FString::Printf(TEXT("Available: %s"), *FString::Join(AvailableParams, TEXT(", ")))
            : TEXT("The parent material has NO texture parameters exposed");
            
        return TResult<void>::Error(VibeUE::ErrorCodes::PARAM_INVALID,
            FString::Printf(TEXT("CANNOT SET: Texture parameter '%s' does not exist in parent material. %s. "
                "This operation will ALWAYS FAIL until the parent material is modified to add this parameter. "
                "To add parameters: use manage_material_node to create a TextureSampleParameter node and connect it to a material output."),
                *ParameterName, *AvailableStr));
    }
    
    // Load the texture
    UTexture* Texture = nullptr;
    if (!TexturePath.IsEmpty() && TexturePath != TEXT("None"))
    {
        Texture = LoadObject<UTexture>(nullptr, *TexturePath);
        if (!Texture)
        {
            // Try with suffix
            FString PathWithSuffix = TexturePath + TEXT(".") + FPackageName::GetShortName(TexturePath);
            Texture = LoadObject<UTexture>(nullptr, *PathWithSuffix);
        }
        
        if (!Texture)
        {
            return TResult<void>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND, 
                FString::Printf(TEXT("Texture not found: %s"), *TexturePath));
        }
    }
    
    // Set the parameter value
    MaterialInstance->SetTextureParameterValueEditorOnly(FName(*ParameterName), Texture);
    
    // Mark package dirty
    MaterialInstance->MarkPackageDirty();

    UE_LOG(LogMaterialService, Log, TEXT("Set texture parameter %s = %s on %s"), *ParameterName, *TexturePath, *InstancePath);
    return TResult<void>::Success();
}

TResult<void> FMaterialService::ClearInstanceParameterOverride(const FString& InstancePath, const FString& ParameterName)
{
    auto LoadResult = LoadMaterialInstance(InstancePath);
    if (!LoadResult.IsSuccess())
    {
        return TResult<void>::Error(LoadResult.GetErrorCode(), LoadResult.GetErrorMessage());
    }

    UMaterialInstanceConstant* MaterialInstance = LoadResult.GetValue();
    bool bFound = false;
    
    // Remove from scalar parameters
    for (int32 i = MaterialInstance->ScalarParameterValues.Num() - 1; i >= 0; --i)
    {
        if (MaterialInstance->ScalarParameterValues[i].ParameterInfo.Name.ToString() == ParameterName)
        {
            MaterialInstance->ScalarParameterValues.RemoveAt(i);
            bFound = true;
        }
    }
    
    // Remove from vector parameters
    for (int32 i = MaterialInstance->VectorParameterValues.Num() - 1; i >= 0; --i)
    {
        if (MaterialInstance->VectorParameterValues[i].ParameterInfo.Name.ToString() == ParameterName)
        {
            MaterialInstance->VectorParameterValues.RemoveAt(i);
            bFound = true;
        }
    }
    
    // Remove from texture parameters
    for (int32 i = MaterialInstance->TextureParameterValues.Num() - 1; i >= 0; --i)
    {
        if (MaterialInstance->TextureParameterValues[i].ParameterInfo.Name.ToString() == ParameterName)
        {
            MaterialInstance->TextureParameterValues.RemoveAt(i);
            bFound = true;
        }
    }
    
    if (!bFound)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::PARAM_INVALID, 
            FString::Printf(TEXT("Parameter override not found: %s"), *ParameterName));
    }
    
    // Notify changes
    MaterialInstance->PreEditChange(nullptr);
    MaterialInstance->PostEditChange();
    
    // Mark package dirty
    MaterialInstance->MarkPackageDirty();

    UE_LOG(LogMaterialService, Log, TEXT("Cleared parameter override %s on %s"), *ParameterName, *InstancePath);
    return TResult<void>::Success();
}

TResult<TArray<FMaterialPropertyInfo>> FMaterialService::ListProperties(const FString& MaterialPath, bool bIncludeAdvanced)
{
    auto LoadResult = LoadMaterial(MaterialPath);
    if (!LoadResult.IsSuccess())
    {
        return TResult<TArray<FMaterialPropertyInfo>>::Error(LoadResult.GetErrorCode(), LoadResult.GetErrorMessage());
    }

    UMaterial* Material = LoadResult.GetValue();
    TArray<FMaterialPropertyInfo> Properties;

    // Iterate over all properties using reflection
    for (TFieldIterator<FProperty> PropIt(UMaterial::StaticClass()); PropIt; ++PropIt)
    {
        FProperty* Property = *PropIt;
        
        // Skip if not editable
        if (!Property->HasAnyPropertyFlags(CPF_Edit))
        {
            continue;
        }

        // Skip advanced properties if not requested
        bool bIsAdvanced = Property->HasMetaData(TEXT("AdvancedDisplay"));
        if (bIsAdvanced && !bIncludeAdvanced)
        {
            continue;
        }

        // Skip deprecated properties
        if (Property->HasMetaData(TEXT("DeprecatedProperty")))
        {
            continue;
        }

        FMaterialPropertyInfo PropInfo;
        PropInfo.Name = Property->GetName();
        PropInfo.DisplayName = Property->GetDisplayNameText().ToString();
        PropInfo.Category = GetPropertyCategory(Property);
        PropInfo.Tooltip = Property->GetToolTipText().ToString();
        PropInfo.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);
        PropInfo.bIsAdvanced = bIsAdvanced;

        // Get type
        if (Property->IsA<FBoolProperty>())
        {
            PropInfo.Type = TEXT("bool");
        }
        else if (Property->IsA<FFloatProperty>() || Property->IsA<FDoubleProperty>())
        {
            PropInfo.Type = TEXT("float");
        }
        else if (Property->IsA<FIntProperty>())
        {
            PropInfo.Type = TEXT("int");
        }
        else if (Property->IsA<FByteProperty>())
        {
            PropInfo.Type = TEXT("byte");
        }
        else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
        {
            PropInfo.Type = TEXT("enum");
            PropInfo.AllowedValues = GetEnumValues(EnumProp);
        }
        else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
        {
            PropInfo.Type = TEXT("struct");
            
            // Add struct type name
            if (StructProp->Struct)
            {
                PropInfo.ObjectClass = StructProp->Struct->GetName();
                
                // Enumerate struct members for better AI discoverability
                // This follows UE's PropertyEditor pattern - expose child properties
                for (TFieldIterator<FProperty> MemberIt(StructProp->Struct); MemberIt; ++MemberIt)
                {
                    FProperty* MemberProp = *MemberIt;
                    
                    FStructMemberInfo MemberInfo;
                    MemberInfo.Name = MemberProp->GetName();
                    
                    // Determine member type
                    if (MemberProp->IsA<FBoolProperty>())
                    {
                        MemberInfo.Type = TEXT("bool");
                    }
                    else if (MemberProp->IsA<FFloatProperty>() || MemberProp->IsA<FDoubleProperty>())
                    {
                        MemberInfo.Type = TEXT("float");
                    }
                    else if (MemberProp->IsA<FIntProperty>())
                    {
                        MemberInfo.Type = TEXT("int");
                    }
                    else if (FEnumProperty* MemberEnum = CastField<FEnumProperty>(MemberProp))
                    {
                        MemberInfo.Type = TEXT("enum");
                        MemberInfo.AllowedValues = GetEnumValues(MemberEnum);
                    }
                    else if (FObjectProperty* MemberObj = CastField<FObjectProperty>(MemberProp))
                    {
                        MemberInfo.Type = TEXT("object");
                        if (MemberObj->PropertyClass)
                        {
                            MemberInfo.ObjectClass = MemberObj->PropertyClass->GetName();
                        }
                    }
                    else
                    {
                        MemberInfo.Type = MemberProp->GetCPPType();
                    }
                    
                    // Get current member value from the struct instance
                    const void* StructPtr = Property->ContainerPtrToValuePtr<void>(Material);
                    if (StructPtr)
                    {
                        MemberInfo.CurrentValue = PropertyToString(MemberProp, const_cast<void*>(StructPtr));
                    }
                    
                    PropInfo.StructMembers.Add(MemberInfo);
                }
            }
        }
        else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
        {
            PropInfo.Type = TEXT("object");
            if (ObjProp->PropertyClass)
            {
                PropInfo.ObjectClass = ObjProp->PropertyClass->GetName();
            }
        }
        else if (Property->IsA<FStrProperty>())
        {
            PropInfo.Type = TEXT("string");
        }
        else
        {
            PropInfo.Type = Property->GetCPPType();
        }

        // Get current value
        PropInfo.CurrentValue = PropertyToString(Property, Material);

        Properties.Add(PropInfo);
    }

    return TResult<TArray<FMaterialPropertyInfo>>::Success(Properties);
}

//-----------------------------------------------------------------------------
// Property Management
//-----------------------------------------------------------------------------

TResult<FString> FMaterialService::GetProperty(const FString& MaterialPath, const FString& PropertyName)
{
    auto LoadResult = LoadMaterial(MaterialPath);
    if (!LoadResult.IsSuccess())
    {
        return TResult<FString>::Error(LoadResult.GetErrorCode(), LoadResult.GetErrorMessage());
    }

    UMaterial* Material = LoadResult.GetValue();

    // Find property
    FProperty* Property = FindFProperty<FProperty>(UMaterial::StaticClass(), *PropertyName);
    if (!Property)
    {
        // Check if it's a known material input name
        if (IsMaterialInputProperty(PropertyName))
        {
            return TResult<FString>::Error(VibeUE::ErrorCodes::PROPERTY_NOT_FOUND, 
                FString::Printf(TEXT("%s is a MATERIAL INPUT (graph connection), not a simple property. "
                    "Use manage_material_node to create and connect expression nodes to this input. "
                    "Example: Create a MaterialExpressionConstant node and connect it to the %s input."), 
                    *PropertyName, *PropertyName));
        }
        
        // Return all valid editable properties
        TArray<FString> KeyProps = GetKeyPropertyNames();
        TArray<FString> AllProps = GetAllEditablePropertyNames();
        
        FString KeyPropsStr = FString::Join(KeyProps, TEXT(", "));
        
        return TResult<FString>::Error(VibeUE::ErrorCodes::PROPERTY_NOT_FOUND, 
            FString::Printf(TEXT("Property not found: '%s'. Key properties: %s. Total editable properties: %d. "
                "Use action='summarize' or action='list_properties' to see all."), 
                *PropertyName, *KeyPropsStr, AllProps.Num()));
    }

    FString Value = PropertyToString(Property, Material);
    
    // Check if this is a material input type (FScalarMaterialInput, FColorMaterialInput, etc.)
    FString TypeName = Property->GetCPPType();
    if (TypeName.Contains(TEXT("MaterialInput")))
    {
        // This is a material input - provide guidance
        return TResult<FString>::Error(VibeUE::ErrorCodes::PROPERTY_NOT_FOUND, 
            FString::Printf(TEXT("%s is a MATERIAL INPUT (type: %s). Current connection: %s. "
                "To modify this value, use manage_material_node to create expression nodes (like MaterialExpressionConstant) "
                "and connect them to this input. You cannot set material inputs directly with set_property."), 
                *PropertyName, *TypeName, *Value));
    }
    
    return TResult<FString>::Success(Value);
}

TResult<FMaterialPropertyInfo> FMaterialService::GetPropertyInfo(const FString& MaterialPath, const FString& PropertyName)
{
    auto LoadResult = LoadMaterial(MaterialPath);
    if (!LoadResult.IsSuccess())
    {
        return TResult<FMaterialPropertyInfo>::Error(LoadResult.GetErrorCode(), LoadResult.GetErrorMessage());
    }

    UMaterial* Material = LoadResult.GetValue();

    // Find property
    FProperty* Property = FindFProperty<FProperty>(UMaterial::StaticClass(), *PropertyName);
    if (!Property)
    {
        // Check if it's a known material input name
        if (IsMaterialInputProperty(PropertyName))
        {
            return TResult<FMaterialPropertyInfo>::Error(VibeUE::ErrorCodes::PROPERTY_NOT_FOUND, 
                FString::Printf(TEXT("%s is a MATERIAL INPUT (graph connection), not a simple property. "
                    "Use manage_material_node to create and connect expression nodes. "
                    "Use action='summarize' to see all material inputs and their connection status."), 
                    *PropertyName));
        }
        
        // Return all valid editable properties
        TArray<FString> KeyProps = GetKeyPropertyNames();
        FString KeyPropsStr = FString::Join(KeyProps, TEXT(", "));
        
        return TResult<FMaterialPropertyInfo>::Error(VibeUE::ErrorCodes::PROPERTY_NOT_FOUND, 
            FString::Printf(TEXT("Property not found: '%s'. Key properties: %s. "
                "Use action='summarize' or action='list_properties' to see all."), 
                *PropertyName, *KeyPropsStr));
    }

    FMaterialPropertyInfo PropInfo;
    PropInfo.Name = Property->GetName();
    PropInfo.DisplayName = Property->GetDisplayNameText().ToString();
    PropInfo.Category = GetPropertyCategory(Property);
    PropInfo.Tooltip = Property->GetToolTipText().ToString();
    PropInfo.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);
    PropInfo.bIsAdvanced = Property->HasMetaData(TEXT("AdvancedDisplay"));
    PropInfo.CurrentValue = PropertyToString(Property, Material);

    // Get type and allowed values
    if (Property->IsA<FBoolProperty>())
    {
        PropInfo.Type = TEXT("bool");
        PropInfo.AllowedValues = {TEXT("true"), TEXT("false")};
    }
    else if (Property->IsA<FFloatProperty>() || Property->IsA<FDoubleProperty>())
    {
        PropInfo.Type = TEXT("float");
    }
    else if (Property->IsA<FIntProperty>())
    {
        PropInfo.Type = TEXT("int");
    }
    else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
    {
        PropInfo.Type = TEXT("enum");
        PropInfo.AllowedValues = GetEnumValues(EnumProp);
    }
    else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
    {
        if (ByteProp->Enum)
        {
            PropInfo.Type = TEXT("enum");
            for (int32 i = 0; i < ByteProp->Enum->NumEnums() - 1; i++)
            {
                PropInfo.AllowedValues.Add(ByteProp->Enum->GetNameStringByIndex(i));
            }
        }
        else
        {
            PropInfo.Type = TEXT("byte");
        }
    }
    else
    {
        PropInfo.Type = Property->GetCPPType();
    }

    return TResult<FMaterialPropertyInfo>::Success(PropInfo);
}

TResult<FString> FMaterialService::SetProperty(const FString& MaterialPath, const FString& PropertyName, const FString& Value)
{
    auto LoadResult = LoadMaterial(MaterialPath);
    if (!LoadResult.IsSuccess())
    {
        return TResult<FString>::Error(LoadResult.GetErrorCode(), LoadResult.GetErrorMessage());
    }

    UMaterial* Material = LoadResult.GetValue();

    // Find property
    FProperty* Property = FindFProperty<FProperty>(UMaterial::StaticClass(), *PropertyName);
    if (!Property)
    {
        // Check if it's a known material input name
        if (IsMaterialInputProperty(PropertyName))
        {
            return TResult<FString>::Error(VibeUE::ErrorCodes::PROPERTY_NOT_FOUND, 
                FString::Printf(TEXT("CANNOT SET %s with set_property - it is a MATERIAL INPUT (graph connection). "
                    "Use manage_material_node instead: "
                    "(1) Create a node like MaterialExpressionConstant or MaterialExpressionVectorParameter, "
                    "(2) Set its value, "
                    "(3) Connect its output to the %s input using connect_to_output action."), 
                    *PropertyName, *PropertyName));
        }
        
        // Return all valid editable properties
        TArray<FString> KeyProps = GetKeyPropertyNames();
        FString KeyPropsStr = FString::Join(KeyProps, TEXT(", "));
        
        return TResult<FString>::Error(VibeUE::ErrorCodes::PROPERTY_NOT_FOUND, 
            FString::Printf(TEXT("Property not found: '%s'. Key properties: %s. "
                "Use action='summarize' or action='list_properties' to see all."), 
                *PropertyName, *KeyPropsStr));
    }

    // Check if editable
    if (!Property->HasAnyPropertyFlags(CPF_Edit))
    {
        // Check if it's a material input type
        FString TypeName = Property->GetCPPType();
        if (TypeName.Contains(TEXT("MaterialInput")))
        {
            return TResult<FString>::Error(VibeUE::ErrorCodes::PROPERTY_NOT_FOUND, 
                FString::Printf(TEXT("CANNOT SET %s - it is a MATERIAL INPUT (type: %s). "
                    "Material inputs require graph nodes. Use manage_material_node to: "
                    "(1) Create expression nodes (MaterialExpressionConstant for scalars, MaterialExpressionConstant3Vector for colors), "
                    "(2) Connect them to this input. "
                    "DO NOT call get_property or set_property for material inputs again."), 
                    *PropertyName, *TypeName));
        }
        
        return TResult<FString>::Error(VibeUE::ErrorCodes::PROPERTY_NOT_FOUND, 
            FString::Printf(TEXT("Property is not editable: %s"), *PropertyName));
    }

    // Use a scoped transaction for undo/redo support
    FScopedTransaction Transaction(NSLOCTEXT("MaterialService", "SetMaterialProperty", "Set Material Property"));
    
    // Modify the material for undo/redo tracking
    Material->Modify();

    // Set the value
    TResult<void> SetResult = StringToProperty(Property, Material, Value);
    if (!SetResult.IsSuccess())
    {
        return TResult<FString>::Error(SetResult.GetErrorCode(), SetResult.GetErrorMessage());
    }

    // Notify post-edit to allow engine validation (some properties have clamps in PostEditChange)
    // This may modify the value if it's outside valid range
    FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
    Material->PostEditChangeProperty(PropertyChangedEvent);

    // Get actual value after validation
    FString ActualValue = PropertyToString(Property, Material);

    // Mark dirty for save
    MarkMaterialDirty(Material);

    UE_LOG(LogMaterialService, Log, TEXT("Set property %s = %s (actual: %s) on %s"), 
        *PropertyName, *Value, *ActualValue, *MaterialPath);
    return TResult<FString>::Success(ActualValue);
}

TResult<void> FMaterialService::SetProperties(const FString& MaterialPath, const TMap<FString, FString>& Properties)
{
    auto LoadResult = LoadMaterial(MaterialPath);
    if (!LoadResult.IsSuccess())
    {
        return TResult<void>::Error(LoadResult.GetErrorCode(), LoadResult.GetErrorMessage());
    }

    UMaterial* Material = LoadResult.GetValue();
    TArray<FString> FailedProperties;

    // Use transaction for undo/redo
    FScopedTransaction Transaction(NSLOCTEXT("MaterialService", "SetMaterialProperties", "Set Material Properties"));
    Material->Modify();

    for (const auto& PropPair : Properties)
    {
        FProperty* Property = FindFProperty<FProperty>(UMaterial::StaticClass(), *PropPair.Key);
        if (!Property)
        {
            FailedProperties.Add(FString::Printf(TEXT("%s (not found)"), *PropPair.Key));
            continue;
        }

        TResult<void> SetResult = StringToProperty(Property, Material, PropPair.Value);
        if (!SetResult.IsSuccess())
        {
            FailedProperties.Add(FString::Printf(TEXT("%s (%s)"), *PropPair.Key, *SetResult.GetErrorMessage()));
        }
    }

    // Post-edit to apply engine validation
    Material->PostEditChange();
    MarkMaterialDirty(Material);

    if (FailedProperties.Num() > 0)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::PROPERTY_SET_FAILED, 
            FString::Printf(TEXT("Some properties failed: %s"), *FString::Join(FailedProperties, TEXT(", "))));
    }

    return TResult<void>::Success();
}

//-----------------------------------------------------------------------------
// Parameter Management
//-----------------------------------------------------------------------------

TResult<TArray<FVibeMaterialParamInfo>> FMaterialService::ListParameters(const FString& MaterialPath)
{
    auto LoadResult = LoadMaterial(MaterialPath);
    if (!LoadResult.IsSuccess())
    {
        return TResult<TArray<FVibeMaterialParamInfo>>::Error(LoadResult.GetErrorCode(), LoadResult.GetErrorMessage());
    }

    UMaterial* Material = LoadResult.GetValue();
    TArray<FVibeMaterialParamInfo> Parameters;

#if WITH_EDITORONLY_DATA
    if (UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData())
    {
        for (UMaterialExpression* Expr : EditorData->ExpressionCollection.Expressions)
        {
            if (!Expr || !Expr->bIsParameterExpression)
            {
                continue;
            }

            FVibeMaterialParamInfo ParamInfo;
            
            if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expr))
            {
                ParamInfo.Name = ScalarParam->ParameterName.ToString();
                ParamInfo.Type = TEXT("Scalar");
                ParamInfo.DefaultValue = FString::SanitizeFloat(ScalarParam->DefaultValue);
                ParamInfo.CurrentValue = ParamInfo.DefaultValue;
                ParamInfo.Group = ScalarParam->Group.ToString();
                ParamInfo.SortPriority = ScalarParam->SortPriority;
            }
            else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expr))
            {
                ParamInfo.Name = VectorParam->ParameterName.ToString();
                ParamInfo.Type = TEXT("Vector");
                ParamInfo.DefaultValue = FString::Printf(TEXT("(%f,%f,%f,%f)"), 
                    VectorParam->DefaultValue.R, VectorParam->DefaultValue.G, 
                    VectorParam->DefaultValue.B, VectorParam->DefaultValue.A);
                ParamInfo.CurrentValue = ParamInfo.DefaultValue;
                ParamInfo.Group = VectorParam->Group.ToString();
                ParamInfo.SortPriority = VectorParam->SortPriority;
            }
            else if (UMaterialExpressionParameter* GenericParam = Cast<UMaterialExpressionParameter>(Expr))
            {
                ParamInfo.Name = GenericParam->ParameterName.ToString();
                ParamInfo.Type = Expr->GetClass()->GetName();
                ParamInfo.Group = GenericParam->Group.ToString();
                ParamInfo.SortPriority = GenericParam->SortPriority;
            }

            if (!ParamInfo.Name.IsEmpty())
            {
                Parameters.Add(ParamInfo);
            }
        }
    }
#endif

    return TResult<TArray<FVibeMaterialParamInfo>>::Success(Parameters);
}

TResult<FVibeMaterialParamInfo> FMaterialService::GetParameter(const FString& MaterialPath, const FString& ParameterName)
{
    auto ParamsResult = ListParameters(MaterialPath);
    if (!ParamsResult.IsSuccess())
    {
        return TResult<FVibeMaterialParamInfo>::Error(ParamsResult.GetErrorCode(), ParamsResult.GetErrorMessage());
    }

    for (const FVibeMaterialParamInfo& Param : ParamsResult.GetValue())
    {
        if (Param.Name.Equals(ParameterName, ESearchCase::IgnoreCase))
        {
            return TResult<FVibeMaterialParamInfo>::Success(Param);
        }
    }

    return TResult<FVibeMaterialParamInfo>::Error(VibeUE::ErrorCodes::PROPERTY_NOT_FOUND, 
        FString::Printf(TEXT("Parameter not found: %s"), *ParameterName));
}

TResult<void> FMaterialService::SetParameterDefault(const FString& MaterialPath, const FString& ParameterName, const FString& Value)
{
    auto LoadResult = LoadMaterial(MaterialPath);
    if (!LoadResult.IsSuccess())
    {
        return TResult<void>::Error(LoadResult.GetErrorCode(), LoadResult.GetErrorMessage());
    }

    UMaterial* Material = LoadResult.GetValue();

#if WITH_EDITORONLY_DATA
    if (UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData())
    {
        for (UMaterialExpression* Expr : EditorData->ExpressionCollection.Expressions)
        {
            if (!Expr || !Expr->bIsParameterExpression)
            {
                continue;
            }

            if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expr))
            {
                if (ScalarParam->ParameterName.ToString().Equals(ParameterName, ESearchCase::IgnoreCase))
                {
                    ScalarParam->PreEditChange(nullptr);
                    ScalarParam->DefaultValue = FCString::Atof(*Value);
                    ScalarParam->PostEditChange();
                    MarkMaterialDirty(Material);
                    return TResult<void>::Success();
                }
            }
            else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expr))
            {
                if (VectorParam->ParameterName.ToString().Equals(ParameterName, ESearchCase::IgnoreCase))
                {
                    // Use transaction for undo support
                    FScopedTransaction Transaction(NSLOCTEXT("MaterialService", "SetVectorParameter", "Set Vector Parameter"));
                    VectorParam->Modify();
                    
                    // Use FJsonValueHelper for robust color parsing (hex, named, Unreal format, comma-separated)
                    FLinearColor Color;
                    if (FJsonValueHelper::TryParseLinearColor(Value, Color))
                    {
                        VectorParam->DefaultValue = Color;
                        VectorParam->PostEditChange();
                        
                        // Mark material dirty and trigger recompile
                        Material->PreEditChange(nullptr);
                        Material->PostEditChange();
                        MarkMaterialDirty(Material);
                        
                        // Save the material to persist the change
                        SaveMaterial(MaterialPath);
                        
                        UE_LOG(LogMaterialService, Log, TEXT("Set vector parameter %s = (%f,%f,%f,%f)"), 
                            *ParameterName, Color.R, Color.G, Color.B, Color.A);
                    }
                    else
                    {
                        UE_LOG(LogMaterialService, Warning, TEXT("Failed to parse color value: %s"), *Value);
                    }
                    
                    return TResult<void>::Success();
                }
            }
        }
    }
#endif

    return TResult<void>::Error(VibeUE::ErrorCodes::PROPERTY_NOT_FOUND, 
        FString::Printf(TEXT("Parameter not found or not settable: %s"), *ParameterName));
}

//-----------------------------------------------------------------------------
// Private Helpers
//-----------------------------------------------------------------------------

FString FMaterialService::PropertyToString(const FProperty* Property, const void* Container) const
{
    if (!Property || !Container)
    {
        return TEXT("");
    }

    const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);

    if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
    {
        return BoolProp->GetPropertyValue(ValuePtr) ? TEXT("true") : TEXT("false");
    }
    else if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
    {
        return FString::SanitizeFloat(FloatProp->GetPropertyValue(ValuePtr));
    }
    else if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
    {
        return FString::SanitizeFloat(DoubleProp->GetPropertyValue(ValuePtr));
    }
    else if (const FIntProperty* IntProp = CastField<FIntProperty>(Property))
    {
        return FString::FromInt(IntProp->GetPropertyValue(ValuePtr));
    }
    else if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
    {
        uint8 ByteValue = ByteProp->GetPropertyValue(ValuePtr);
        if (ByteProp->Enum)
        {
            return ByteProp->Enum->GetNameStringByValue(ByteValue);
        }
        return FString::FromInt(ByteValue);
    }
    else if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
    {
        int64 EnumValue = EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
        return EnumProp->GetEnum()->GetNameStringByValue(EnumValue);
    }
    else if (const FStrProperty* StrProp = CastField<FStrProperty>(Property))
    {
        return StrProp->GetPropertyValue(ValuePtr);
    }
    else if (const FNameProperty* NameProp = CastField<FNameProperty>(Property))
    {
        return NameProp->GetPropertyValue(ValuePtr).ToString();
    }
    else if (const FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
    {
        UObject* Obj = ObjProp->GetObjectPropertyValue(ValuePtr);
        return Obj ? Obj->GetPathName() : TEXT("None");
    }

    // For complex types, try ExportText
    FString ExportedValue;
    Property->ExportTextItem_Direct(ExportedValue, ValuePtr, nullptr, nullptr, PPF_None);
    return ExportedValue;
}

TResult<void> FMaterialService::StringToProperty(FProperty* Property, void* Container, const FString& Value) const
{
    if (!Property || !Container)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::PARAM_INVALID, TEXT("Invalid property or container"));
    }

    void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);

    if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
    {
        bool bValue = Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || 
                      Value.Equals(TEXT("1")) || 
                      Value.Equals(TEXT("yes"), ESearchCase::IgnoreCase);
        BoolProp->SetPropertyValue(ValuePtr, bValue);
        return TResult<void>::Success();
    }
    else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
    {
        FloatProp->SetPropertyValue(ValuePtr, FCString::Atof(*Value));
        return TResult<void>::Success();
    }
    else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
    {
        DoubleProp->SetPropertyValue(ValuePtr, FCString::Atod(*Value));
        return TResult<void>::Success();
    }
    else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
    {
        IntProp->SetPropertyValue(ValuePtr, FCString::Atoi(*Value));
        return TResult<void>::Success();
    }
    else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
    {
        if (ByteProp->Enum)
        {
            int64 EnumValue = FindEnumValueFlexible(ByteProp->Enum, Value);
            if (EnumValue == INDEX_NONE)
            {
                // Build list of valid values for the error message
                TArray<FString> ValidValues;
                for (int32 i = 0; i < ByteProp->Enum->NumEnums() - 1; ++i)  // -1 to skip _MAX
                {
                    ValidValues.Add(ByteProp->Enum->GetNameStringByIndex(i));
                }
                return TResult<void>::Error(VibeUE::ErrorCodes::PARAM_INVALID, 
                    FString::Printf(TEXT("Invalid enum value: %s. Valid values: %s"), *Value, *FString::Join(ValidValues, TEXT(", "))));
            }
            ByteProp->SetPropertyValue(ValuePtr, static_cast<uint8>(EnumValue));
        }
        else
        {
            ByteProp->SetPropertyValue(ValuePtr, static_cast<uint8>(FCString::Atoi(*Value)));
        }
        return TResult<void>::Success();
    }
    else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
    {
        int64 EnumValue = FindEnumValueFlexible(EnumProp->GetEnum(), Value);
        if (EnumValue == INDEX_NONE)
        {
            // Build list of valid values for the error message
            TArray<FString> ValidValues;
            UEnum* Enum = EnumProp->GetEnum();
            for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)  // -1 to skip _MAX
            {
                ValidValues.Add(Enum->GetNameStringByIndex(i));
            }
            return TResult<void>::Error(VibeUE::ErrorCodes::PARAM_INVALID, 
                FString::Printf(TEXT("Invalid enum value: %s. Valid values: %s"), *Value, *FString::Join(ValidValues, TEXT(", "))));
        }
        EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, EnumValue);
        return TResult<void>::Success();
    }
    else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
    {
        StrProp->SetPropertyValue(ValuePtr, Value);
        return TResult<void>::Success();
    }
    else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
    {
        NameProp->SetPropertyValue(ValuePtr, FName(*Value));
        return TResult<void>::Success();
    }
    else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
    {
        if (Value.IsEmpty() || Value.Equals(TEXT("None"), ESearchCase::IgnoreCase))
        {
            ObjProp->SetObjectPropertyValue(ValuePtr, nullptr);
        }
        else
        {
            UObject* LoadedObj = LoadObject<UObject>(nullptr, *Value);
            if (!LoadedObj)
            {
                return TResult<void>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND, 
                    FString::Printf(TEXT("Could not load object: %s"), *Value));
            }
            ObjProp->SetObjectPropertyValue(ValuePtr, LoadedObj);
        }
        return TResult<void>::Success();
    }
    else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        // Generic reflection-based struct handling following UE's PropertyHandle pattern:
        // For structs, we iterate through child properties and set them individually
        // This matches how PropertyHandleImpl.cpp handles Vector, Rotator, etc.
        
        FString CleanValue = Value;
        CleanValue.TrimStartAndEndInline();
        
        // If it's in struct format "(Member1=Value1,Member2=Value2)", parse and set individual members
        if (CleanValue.StartsWith(TEXT("(")))
        {
            UScriptStruct* Struct = StructProp->Struct;
            if (Struct)
            {
                // Parse member=value pairs from the struct string
                FString Inner = CleanValue.Mid(1);
                Inner.RemoveFromEnd(TEXT(")"));
                
                bool bAnyMemberSet = false;
                
                // Iterate through struct properties and try to set them
                for (TFieldIterator<FProperty> It(Struct); It; ++It)
                {
                    FProperty* MemberProp = *It;
                    FString MemberName = MemberProp->GetName();
                    
                    // Try to find this member in the input string
                    FString SearchKey = MemberName + TEXT("=");
                    int32 MemberIdx = Inner.Find(SearchKey, ESearchCase::IgnoreCase);
                    if (MemberIdx != INDEX_NONE)
                    {
                        // Extract the value for this member
                        FString MemberValueStr = Inner.Mid(MemberIdx + SearchKey.Len());
                        
                        // Handle quoted strings and object references
                        if (MemberValueStr.StartsWith(TEXT("\"")))
                        {
                            // Find closing quote (handle escaped quotes)
                            int32 EndQuote = 1;
                            while (EndQuote < MemberValueStr.Len())
                            {
                                int32 NextQuote = MemberValueStr.Find(TEXT("\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, EndQuote);
                                if (NextQuote == INDEX_NONE) break;
                                // Check if escaped
                                if (NextQuote > 0 && MemberValueStr[NextQuote - 1] == '\\')
                                {
                                    EndQuote = NextQuote + 1;
                                    continue;
                                }
                                MemberValueStr = MemberValueStr.Mid(1, NextQuote - 1);
                                break;
                            }
                        }
                        else
                        {
                            // Find comma or end of string (but be careful of nested parens)
                            int32 ParenDepth = 0;
                            int32 CommaIdx = INDEX_NONE;
                            for (int32 i = 0; i < MemberValueStr.Len(); ++i)
                            {
                                TCHAR C = MemberValueStr[i];
                                if (C == '(') ParenDepth++;
                                else if (C == ')') ParenDepth--;
                                else if (C == ',' && ParenDepth == 0)
                                {
                                    CommaIdx = i;
                                    break;
                                }
                            }
                            if (CommaIdx != INDEX_NONE)
                            {
                                MemberValueStr = MemberValueStr.Left(CommaIdx);
                            }
                        }
                        
                        // Get pointer to this member within the struct value
                        void* MemberPtr = MemberProp->ContainerPtrToValuePtr<void>(ValuePtr);
                        
                        // Recursively set the member value using its proper container (ValuePtr is the struct base)
                        TResult<void> MemberResult = StringToProperty(MemberProp, ValuePtr, MemberValueStr);
                        if (!MemberResult.IsSuccess())
                        {
                            UE_LOG(LogMaterialService, Warning, TEXT("Failed to set struct member %s: %s"), 
                                *MemberName, *MemberResult.GetErrorMessage());
                        }
                        else
                        {
                            UE_LOG(LogMaterialService, Log, TEXT("Set struct member %s = %s"), *MemberName, *MemberValueStr);
                            bAnyMemberSet = true;
                        }
                    }
                }
                
                if (bAnyMemberSet)
                {
                    return TResult<void>::Success();
                }
                // If no members were set from parsing, fall through to ImportText
            }
        }
        
        // For non-parenthesized values or if parsing failed, try ImportText_Direct
        // This handles simple struct values and UE's native text format
    }

    // Try ImportText for complex types
    const TCHAR* ImportResult = Property->ImportText_Direct(*Value, ValuePtr, nullptr, PPF_None);
    if (!ImportResult)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::PROPERTY_SET_FAILED, 
            FString::Printf(TEXT("Failed to parse value for property %s: %s"), *Property->GetName(), *Value));
    }

    return TResult<void>::Success();
}

FString FMaterialService::GetPropertyCategory(const FProperty* Property) const
{
    if (!Property)
    {
        return TEXT("");
    }

    // Try to get category from metadata
    if (Property->HasMetaData(TEXT("Category")))
    {
        return Property->GetMetaData(TEXT("Category"));
    }

    return TEXT("Default");
}

TArray<FString> FMaterialService::GetEnumValues(const FEnumProperty* EnumProperty) const
{
    TArray<FString> Values;
    if (EnumProperty && EnumProperty->GetEnum())
    {
        UEnum* Enum = EnumProperty->GetEnum();
        for (int32 i = 0; i < Enum->NumEnums() - 1; i++) // -1 to skip _MAX
        {
            // Skip hidden values
            if (!Enum->HasMetaData(TEXT("Hidden"), i))
            {
                Values.Add(Enum->GetNameStringByIndex(i));
            }
        }
    }
    return Values;
}

void FMaterialService::MarkMaterialDirty(UMaterial* Material) const
{
    if (Material)
    {
        Material->MarkPackageDirty();
        
        // Notify the material editor if open
        Material->PostEditChange();
        
        // Force refresh any open editor's Details panel
        // The Details panel caches property values and doesn't automatically update
        // when we change properties externally via reflection
        // Force all Details panels showing this object to refresh
        if (GEditor)
        {
            // Request that property editors refresh their view of this object
            // This broadcasts to all registered DetailsViews
            FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
            
            // Create an array with just this material
            TArray<UObject*> ObjectsToRefresh;
            ObjectsToRefresh.Add(Material);
            
            // Notify that object properties have changed - this tells any Details panels
            // viewing this object to refresh their display
            PropertyEditorModule.NotifyCustomizationModuleChanged();
            
            // Also try to force Slate to repaint the widgets
            if (FSlateApplication::IsInitialized())
            {
                FSlateApplication::Get().InvalidateAllWidgets(false);
            }
        }
    }
}
