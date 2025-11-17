// Copyright Kevin Buckley 2025 All Rights Reserved.

/**
 * @file WidgetLifecycleService.cpp
 * @brief Implementation of widget blueprint lifecycle management
 * 
 * This service provides widget blueprint creation and deletion,
 * extracted from UMGCommands.cpp as part of Phase 4 refactoring.
 */

#include "Services/UMG/WidgetLifecycleService.h"
#include "Core/ErrorCodes.h"
#include "WidgetBlueprint.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "WidgetBlueprintFactory.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogWidgetLifecycle, Log, All);

FWidgetLifecycleService::FWidgetLifecycleService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

TResult<UWidgetBlueprint*> FWidgetLifecycleService::CreateWidgetBlueprint(
    const FString& WidgetName,
    const FString& PackagePath,
    const FString& ParentClass)
{
    auto ValidationResult = ValidateNotEmpty(WidgetName, TEXT("WidgetName"));
    if (ValidationResult.IsError())
    {
        return TResult<UWidgetBlueprint*>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    if (!IsValidWidgetName(WidgetName))
    {
        return TResult<UWidgetBlueprint*>::Error(
            VibeUE::ErrorCodes::PARAM_INVALID,
            FString::Printf(TEXT("Widget name '%s' is not valid"), *WidgetName)
        );
    }

    // Get parent class
    UClass* ParentUClass = GetParentClass(ParentClass);
    if (!ParentUClass)
    {
        return TResult<UWidgetBlueprint*>::Error(
            VibeUE::ErrorCodes::BLUEPRINT_INVALID_PARENT,
            FString::Printf(TEXT("Parent class '%s' not found"), *ParentClass)
        );
    }

    // Construct package path
    FString NormalizedPackagePath = PackagePath;
    if (!NormalizedPackagePath.EndsWith(TEXT("/")))
    {
        NormalizedPackagePath += TEXT("/");
    }
    FString FullPackageName = NormalizedPackagePath + WidgetName;

    // Check if already exists
    FString CheckPath = FullPackageName;
    if (UEditorAssetLibrary::DoesAssetExist(CheckPath))
    {
        return TResult<UWidgetBlueprint*>::Error(
            VibeUE::ErrorCodes::WIDGET_ALREADY_EXISTS,
            FString::Printf(TEXT("Widget blueprint already exists at path"))
        );
    }

    // Create the widget blueprint
    UWidgetBlueprint* NewWidgetBP = Cast<UWidgetBlueprint>(
        FKismetEditorUtilities::CreateBlueprint(
            ParentUClass,
            CreatePackage(*FullPackageName),
            FName(*WidgetName),
            BPTYPE_Normal,
            UWidgetBlueprint::StaticClass(),
            UWidgetBlueprintGeneratedClass::StaticClass(),
            NAME_None
        )
    );

    if (!NewWidgetBP)
    {
        return TResult<UWidgetBlueprint*>::Error(
            VibeUE::ErrorCodes::WIDGET_CREATE_FAILED,
            FString::Printf(TEXT("Failed to create widget blueprint '%s'"), *WidgetName)
        );
    }

    // Initialize widget tree
    if (!NewWidgetBP->WidgetTree)
    {
        NewWidgetBP->WidgetTree = NewObject<UWidgetTree>(NewWidgetBP);
    }

    // Save the asset
    FAssetRegistryModule::AssetCreated(NewWidgetBP);
    NewWidgetBP->MarkPackageDirty();

    return TResult<UWidgetBlueprint*>::Success(NewWidgetBP);
}

TResult<void> FWidgetLifecycleService::DeleteWidgetBlueprint(const FString& WidgetBlueprintPath)
{
    auto ValidationResult = ValidateNotEmpty(WidgetBlueprintPath, TEXT("WidgetBlueprintPath"));
    if (ValidationResult.IsError())
    {
        return ValidationResult;
    }

    if (!UEditorAssetLibrary::DoesAssetExist(WidgetBlueprintPath))
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::WIDGET_BLUEPRINT_NOT_FOUND,
            FString::Printf(TEXT("Widget blueprint not found at '%s'"), *WidgetBlueprintPath)
        );
    }

    bool bSuccess = UEditorAssetLibrary::DeleteAsset(WidgetBlueprintPath);
    if (!bSuccess)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::WIDGET_DELETE_FAILED,
            FString::Printf(TEXT("Failed to delete widget blueprint at '%s'"), *WidgetBlueprintPath)
        );
    }

    return TResult<void>::Success();
}

TResult<bool> FWidgetLifecycleService::CanDeleteWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return TResult<bool>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    // Check if widget blueprint is in engine content
    FString PackageName = WidgetBlueprint->GetPackage()->GetName();
    if (PackageName.StartsWith(TEXT("/Engine/")))
    {
        return TResult<bool>::Success(false);
    }

    // Check for references (simplified - full implementation would check asset registry)
    // For now, allow deletion
    return TResult<bool>::Success(true);
}

TResult<void> FWidgetLifecycleService::CompileWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return ValidationResult;
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
    FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

    return TResult<void>::Success();
}

TResult<void> FWidgetLifecycleService::SaveWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return ValidationResult;
    }

    FString PackageName = WidgetBlueprint->GetPackage()->GetName();
    bool bSuccess = UEditorAssetLibrary::SaveAsset(PackageName);
    
    if (!bSuccess)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::OPERATION_FAILED,
            FString::Printf(TEXT("Failed to save widget blueprint '%s'"), *PackageName)
        );
    }

    return TResult<void>::Success();
}

bool FWidgetLifecycleService::IsValidWidgetName(const FString& WidgetName)
{
    if (WidgetName.IsEmpty())
    {
        return false;
    }

    // Check for invalid characters
    for (TCHAR Ch : WidgetName)
    {
        if (!FChar::IsAlnum(Ch) && Ch != TEXT('_'))
        {
            return false;
        }
    }

    return true;
}

UClass* FWidgetLifecycleService::GetParentClass(const FString& ParentClassName)
{
    if (ParentClassName.IsEmpty() || ParentClassName.Equals(TEXT("UserWidget"), ESearchCase::IgnoreCase))
    {
        return UUserWidget::StaticClass();
    }

    UClass* ParentClass = FindFirstObjectSafe<UClass>(*ParentClassName);
    if (!ParentClass || !ParentClass->IsChildOf(UUserWidget::StaticClass()))
    {
        return nullptr;
    }

    return ParentClass;
}
