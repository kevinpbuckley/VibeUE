// Copyright VibeUE 2025

#include "Services/UMG/WidgetAssetService.h"
#include "Core/ErrorCodes.h"
#include "WidgetBlueprint.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"

FWidgetAssetService::FWidgetAssetService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

TResult<FWidgetDeleteResult> FWidgetAssetService::DeleteWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint, bool bCheckReferences)
{
    auto Validation = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (Validation.IsError())
    {
        return TResult<FWidgetDeleteResult>::Error(Validation.GetErrorCode(), Validation.GetErrorMessage());
    }

    FWidgetDeleteResult Result;
    Result.WidgetName = WidgetBlueprint->GetName();
    Result.AssetPath = WidgetBlueprint->GetPathName();
    Result.bReferencesChecked = bCheckReferences;

    if (bCheckReferences)
    {
        IAssetRegistry* AssetRegistry = GetContext()->GetAssetRegistry();
        if (!AssetRegistry)
        {
            return TResult<FWidgetDeleteResult>::Error(
                VibeUE::ErrorCodes::INTERNAL_ERROR,
                TEXT("Failed to get Asset Registry")
            );
        }

        TArray<FName> PackageNamesReferencingAsset;
        AssetRegistry->GetReferencers(WidgetBlueprint->GetPackage()->GetFName(), PackageNamesReferencingAsset);

        for (const FName& PackageName : PackageNamesReferencingAsset)
        {
            if (PackageName == WidgetBlueprint->GetPackage()->GetFName())
            {
                continue; // Skip self references
            }

            FWidgetReferenceInfo ReferenceInfo;
            ReferenceInfo.PackageName = PackageName.ToString();
            ReferenceInfo.ReferenceType = TEXT("AssetRegistry");
            Result.References.Add(ReferenceInfo);
        }
    }

    Result.ReferenceCount = Result.References.Num();

    if (!UEditorAssetLibrary::DoesAssetExist(Result.AssetPath))
    {
        Result.ErrorMessage = FString::Printf(TEXT("Asset '%s' does not exist"), *Result.AssetPath);
        Result.bDeletionSucceeded = false;
        return TResult<FWidgetDeleteResult>::Success(Result);
    }

    const bool bDeleted = UEditorAssetLibrary::DeleteAsset(Result.AssetPath);
    Result.bDeletionSucceeded = bDeleted;
    if (!bDeleted)
    {
        Result.ErrorMessage = TEXT("UEditorAssetLibrary::DeleteAsset returned false");
    }

    return TResult<FWidgetDeleteResult>::Success(Result);
}
