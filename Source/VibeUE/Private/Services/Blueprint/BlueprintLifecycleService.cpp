// Copyright VibeUE 2025

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Services/Blueprint/BlueprintLifecycleService.h"
#include "Engine/Blueprint.h"
#include "Factories/BlueprintFactory.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "UObject/Package.h"
#include "Misc/App.h"
#include "UObject/UObjectGlobals.h"

FBlueprintLifecycleService::FBlueprintLifecycleService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

TResult<UBlueprint*> FBlueprintLifecycleService::CreateBlueprint(const FString& Name, const FString& ParentClass)
{
    // Parse name to extract path and asset name
    FString CleanName = Name;
    CleanName.ReplaceInline(TEXT("\\"), TEXT("/"));
    CleanName.TrimStartAndEndInline();

    FString PackagePath;
    FString AssetName;

    if (CleanName.Contains(TEXT("/")))
    {
        FString PackagePart = CleanName;
        FString ObjectName;

        if (CleanName.Contains(TEXT(".")))
        {
            CleanName.Split(TEXT("."), &PackagePart, &ObjectName);
        }

        PackagePart.TrimEndInline();
        while (PackagePart.EndsWith(TEXT("/")))
        {
            PackagePart.LeftChopInline(1);
        }

        int32 LastSlashIndex = INDEX_NONE;
        if (PackagePart.FindLastChar(TEXT('/'), LastSlashIndex))
        {
            AssetName = ObjectName.IsEmpty() ? PackagePart.Mid(LastSlashIndex + 1) : ObjectName;
            PackagePath = PackagePart.Left(LastSlashIndex);
        }
    }

    if (PackagePath.IsEmpty() || AssetName.IsEmpty())
    {
        AssetName = CleanName;
        PackagePath = TEXT("/Game/Blueprints");
    }

    // Normalize package path
    PackagePath.ReplaceInline(TEXT("\\"), TEXT("/"));
    PackagePath.TrimStartAndEndInline();
    while (PackagePath.EndsWith(TEXT("/")))
    {
        PackagePath.LeftChopInline(1);
    }
    if (!PackagePath.StartsWith(TEXT("/")) && !PackagePath.IsEmpty())
    {
        PackagePath = TEXT("/") + PackagePath;
    }
    if (PackagePath.IsEmpty())
    {
        PackagePath = TEXT("/Game/Blueprints");
    }

    const FString FullAssetPath = PackagePath + TEXT("/") + AssetName;

    // Check if blueprint already exists
    if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath))
    {
        return TResult<UBlueprint*>::Error(VibeUE::ErrorCodes::OPERATION_FAILED, FString::Printf(TEXT("Blueprint already exists: %s"), *FullAssetPath));
    }

    // Find parent class
    UClass* SelectedParentClass = FindParentClass(ParentClass);
    if (!SelectedParentClass)
    {
        // Default to Actor if parent class not found or not specified
        SelectedParentClass = AActor::StaticClass();
    }

    // Create the blueprint factory
    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = SelectedParentClass;

    // Create the blueprint
    UPackage* Package = CreatePackage(*FullAssetPath);
    UBlueprint* NewBlueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(
        UBlueprint::StaticClass(), Package, *AssetName, RF_Standalone | RF_Public, nullptr, GWarn));

    if (!NewBlueprint)
    {
        return TResult<UBlueprint*>::Error(VibeUE::ErrorCodes::BLUEPRINT_CREATE_FAILED, TEXT("Failed to create blueprint"));
    }

    // Notify the asset registry
    FAssetRegistryModule::AssetCreated(NewBlueprint);

    // Mark the package dirty
    Package->MarkPackageDirty();

    return TResult<UBlueprint*>::Success(NewBlueprint);
}

TResult<void> FBlueprintLifecycleService::CompileBlueprint(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }

    // Compile the blueprint
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    // Check for compilation errors
    if (Blueprint->Status == BS_Error)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint compilation failed with errors"));
    }

    return TResult<void>::Success();
}

TResult<void> FBlueprintLifecycleService::ReparentBlueprint(UBlueprint* Blueprint, UClass* NewParentClass)
{
    if (!Blueprint)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }

    if (!NewParentClass)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::OPERATION_FAILED, TEXT("New parent class is null"));
    }

    // Set the new parent class
    Blueprint->ParentClass = NewParentClass;

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    // Refresh the blueprint to update inheritance
    FBlueprintEditorUtils::RefreshAllNodes(Blueprint);

    // Recompile the blueprint
    FBlueprintEditorUtils::RefreshVariables(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None);

    return TResult<void>::Success();
}

TResult<void> FBlueprintLifecycleService::ReparentBlueprint(UBlueprint* Blueprint, const FString& NewParentClassName)
{
    if (!Blueprint)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }

    // Find the new parent class using the internal helper
    UClass* NewParentClass = FindParentClass(NewParentClassName);
    if (!NewParentClass)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::BLUEPRINT_INVALID_PARENT, 
            FString::Printf(TEXT("Parent class not found: %s"), *NewParentClassName));
    }

    // Delegate to the UClass* overload
    return ReparentBlueprint(Blueprint, NewParentClass);
}

TResult<void> FBlueprintLifecycleService::DeleteBlueprint(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }

    FString AssetPath = Blueprint->GetPathName();

    // Use EditorAssetLibrary to delete the asset
    bool bDeleted = UEditorAssetLibrary::DeleteAsset(AssetPath);

    if (!bDeleted)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::OPERATION_FAILED, FString::Printf(TEXT("Failed to delete blueprint: %s"), *AssetPath));
    }

    return TResult<void>::Success();
}

TResult<TArray<FString>> FBlueprintLifecycleService::GetCompilationErrors(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return TResult<TArray<FString>>::Error(VibeUE::ErrorCodes::OPERATION_FAILED, TEXT("Blueprint is null"));
    }

    TArray<FString> Errors;

    // Check blueprint status
    if (Blueprint->Status == BS_Error)
    {
        Errors.Add(TEXT("Blueprint has compilation errors"));
    }

    // Could be extended to extract specific error messages from the message log
    // For now, just return basic status

    return TResult<TArray<FString>>::Success(Errors);
}

TResult<bool> FBlueprintLifecycleService::IsCompiled(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return TResult<bool>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }

    bool bIsCompiled = (Blueprint->Status != BS_Unknown && Blueprint->Status != BS_Dirty);
    return TResult<bool>::Success(bIsCompiled);
}

UClass* FBlueprintLifecycleService::FindParentClass(const FString& ClassDescriptor)
{
    if (ClassDescriptor.IsEmpty())
    {
        return nullptr;
    }

    FString Descriptor = ClassDescriptor;
    Descriptor.TrimStartAndEndInline();
    Descriptor.ReplaceInline(TEXT("\\"), TEXT("/"));

    // Full path descriptors can be loaded directly
    if (Descriptor.Contains(TEXT("/")))
    {
        if (UClass* Loaded = LoadObject<UClass>(nullptr, *Descriptor))
        {
            return Loaded;
        }
    }

    // Try existing objects in memory
    if (UClass* Existing = FindFirstObjectSafe<UClass>(*Descriptor))
    {
        return Existing;
    }

    // Try loading from common script modules
    static const TArray<FString> ModuleHints = {
        TEXT("Engine"),
        TEXT("Game"),
        FApp::GetProjectName()
    };

    FString CandidateBase = Descriptor;

    // Generate permutations (with/without leading 'A' or 'U')
    TArray<FString> NamePermutations;
    NamePermutations.Add(CandidateBase);
    if (!CandidateBase.StartsWith(TEXT("A")) && !CandidateBase.StartsWith(TEXT("U")))
    {
        NamePermutations.Add(TEXT("A") + CandidateBase);
        NamePermutations.Add(TEXT("U") + CandidateBase);
    }

    for (const FString& NameVariant : NamePermutations)
    {
    if (UClass* ExistingVariant = FindFirstObjectSafe<UClass>(*NameVariant))
        {
            return ExistingVariant;
        }

        for (const FString& ModuleName : ModuleHints)
        {
            const FString ModulePath = FString::Printf(TEXT("/Script/%s.%s"), *ModuleName, *NameVariant);
            if (UClass* LoadedVariant = LoadObject<UClass>(nullptr, *ModulePath))
            {
                return LoadedVariant;
            }
        }
    }

    return nullptr;
}
