// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Commands/AssetCommands.h"
#include "Services/Asset/AssetDiscoveryService.h"
#include "Services/Asset/AssetLifecycleService.h"
#include "Services/Asset/AssetImportService.h"
#include "Core/ServiceContext.h"
#include "Core/Result.h"
#include "Utils/HelpFileReader.h"
#include "Utils/ParamValidation.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

FAssetCommands::FAssetCommands()
{
    // Initialize service context and services
    ServiceContext = MakeShared<FServiceContext>();
    DiscoveryService = MakeShared<FAssetDiscoveryService>(ServiceContext);
    LifecycleService = MakeShared<FAssetLifecycleService>(ServiceContext);
    ImportService = MakeShared<FAssetImportService>(ServiceContext);
}

TSharedPtr<FJsonObject> FAssetCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    // Handle multi-action manage_asset routing
    if (CommandType == TEXT("manage_asset"))
    {
        if (!Params.IsValid())
        {
            return CreateErrorResponse(TEXT("Parameters are required"));
        }

        FString Action;
        if (!Params->TryGetStringField(TEXT("action"), Action))
        {
            return CreateErrorResponse(TEXT("action parameter is required"));
        }

        Action = Action.ToLower();
        UE_LOG(LogTemp, Display, TEXT("AssetCommands: Handling action '%s'"), *Action);

        // Help action
        if (Action == TEXT("help"))
        {
            return HandleHelp(Params);
        }
        // Route to specific handlers
        else if (Action == TEXT("search"))
        {
            return HandleSearchAssets(Params);
        }
        else if (Action == TEXT("import_texture"))
        {
            return HandleImportTextureAsset(Params);
        }
        else if (Action == TEXT("export_texture"))
        {
            return HandleExportTextureForAnalysis(Params);
        }
        else if (Action == TEXT("delete"))
        {
            return HandleDeleteAsset(Params);
        }
        else if (Action == TEXT("duplicate"))
        {
            return HandleDuplicateAsset(Params);
        }
        else if (Action == TEXT("save"))
        {
            return HandleSaveAsset(Params);
        }
        else if (Action == TEXT("save_all"))
        {
            return HandleSaveAllAssets(Params);
        }
        else if (Action == TEXT("list_references"))
        {
            return HandleListReferences(Params);
        }
        else if (Action == TEXT("open") || Action == TEXT("open_in_editor"))
        {
            return HandleOpenAssetInEditor(Params);
        }
        else
        {
            return CreateErrorResponse(FString::Printf(TEXT("Unknown action: %s. Use action='help' for available actions."), *Action));
        }
    }
    // Legacy direct command routing
    else if (CommandType == TEXT("import_texture_asset"))
    {
        return HandleImportTextureAsset(Params);
    }
    else if (CommandType == TEXT("export_texture_for_analysis"))
    {
        return HandleExportTextureForAnalysis(Params);
    }
    else if (CommandType == TEXT("delete_asset"))
    {
        return HandleDeleteAsset(Params);
    }
    else if (CommandType == TEXT("OpenAssetInEditor"))
    {
        return HandleOpenAssetInEditor(Params);
    }
    else if (CommandType == TEXT("duplicate_asset"))
    {
        return HandleDuplicateAsset(Params);
    }
    else if (CommandType == TEXT("save_asset"))
    {
        return HandleSaveAsset(Params);
    }
    else if (CommandType == TEXT("save_all_assets"))
    {
        return HandleSaveAllAssets(Params);
    }
    else if (CommandType == TEXT("list_references"))
    {
        return HandleListReferences(Params);
    }

    return CreateErrorResponse(FString::Printf(TEXT("Unknown asset command: %s"), *CommandType));
}


TSharedPtr<FJsonObject> FAssetCommands::HandleImportTextureAsset(const TSharedPtr<FJsonObject>& Params)
{
    // Validate required parameter
    static const TArray<FString> ValidParams = {
        TEXT("file_path"), TEXT("destination_path"), TEXT("texture_name"),
        TEXT("replace_existing"), TEXT("save")
    };
    
    if (!ParamValidation::HasStringParam(Params, TEXT("file_path")))
    {
        return ParamValidation::MissingParamsError(TEXT("file_path is required"), ValidParams);
    }
    
    // Extract parameters
    FString SourceFile;
    FString DestinationPath = TEXT("/Game/Textures/Imported");
    FString TextureName;
    bool bReplaceExisting = true;
    bool bSave = true;

    if (Params.IsValid())
    {
        Params->TryGetStringField(TEXT("file_path"), SourceFile);
        Params->TryGetStringField(TEXT("destination_path"), DestinationPath);
        Params->TryGetStringField(TEXT("texture_name"), TextureName);
        Params->TryGetBoolField(TEXT("replace_existing"), bReplaceExisting);
        Params->TryGetBoolField(TEXT("save"), bSave);
    }

    // Delegate to ImportService
    TResult<FTextureImportResult> Result = ImportService->ImportTexture(
        SourceFile,
        DestinationPath,
        TextureName,
        bReplaceExisting,
        bSave
    );

    // Convert result to JSON response
    if (Result.IsSuccess())
    {
        const FTextureImportResult& ImportResult = Result.GetValue();
        TSharedPtr<FJsonObject> Response = CreateSuccessResponse(TEXT("Texture imported"));
        Response->SetStringField(TEXT("asset_path"), ImportResult.AssetPath);
        Response->SetStringField(TEXT("destination_path"), ImportResult.DestinationPath);
        Response->SetStringField(TEXT("source_file"), ImportResult.SourceFile);
        Response->SetStringField(TEXT("asset_class"), ImportResult.AssetClass);
        Response->SetBoolField(TEXT("import_only"), true);
        return Response;
    }
    else
    {
        return CreateErrorResponse(Result.GetErrorMessage());
    }
}

TSharedPtr<FJsonObject> FAssetCommands::HandleExportTextureForAnalysis(const TSharedPtr<FJsonObject>& Params)
{
    // Validate required parameter
    static const TArray<FString> ValidParams = {
        TEXT("asset_path"), TEXT("export_format"), TEXT("temp_folder"), TEXT("max_size")
    };
    
    if (!ParamValidation::HasStringParam(Params, TEXT("asset_path")))
    {
        return ParamValidation::MissingParamsError(TEXT("asset_path is required"), ValidParams);
    }
    
    // Extract parameters
    FString AssetPath;
    FString ExportFormat = TEXT("PNG");
    FString TempFolder;
    int32 MaxWidth = 0;
    int32 MaxHeight = 0;
    
    if (Params.IsValid())
    {
        Params->TryGetStringField(TEXT("asset_path"), AssetPath);
        Params->TryGetStringField(TEXT("export_format"), ExportFormat);
        Params->TryGetStringField(TEXT("temp_folder"), TempFolder);
        
        // Parse max_size array if provided
        const TArray<TSharedPtr<FJsonValue>>* MaxSizeArray = nullptr;
        if (Params->TryGetArrayField(TEXT("max_size"), MaxSizeArray) && MaxSizeArray->Num() >= 2)
        {
            MaxWidth = (*MaxSizeArray)[0]->AsNumber();
            MaxHeight = (*MaxSizeArray)[1]->AsNumber();
        }
    }
    
    // Delegate to ImportService
    TResult<FTextureExportResult> Result = ImportService->ExportTextureForAnalysis(
        AssetPath,
        ExportFormat,
        TempFolder,
        MaxWidth,
        MaxHeight
    );
    
    // Convert result to JSON response
    if (Result.IsSuccess())
    {
        const FTextureExportResult& ExportResult = Result.GetValue();
        TSharedPtr<FJsonObject> Response = CreateSuccessResponse(TEXT("Texture exported successfully"));
        Response->SetStringField(TEXT("asset_path"), ExportResult.AssetPath);
        Response->SetStringField(TEXT("temp_file_path"), ExportResult.TempFilePath);
        Response->SetStringField(TEXT("export_format"), ExportResult.ExportFormat);
        
        TArray<TSharedPtr<FJsonValue>> ExportedSizeArray;
        ExportedSizeArray.Add(MakeShareable(new FJsonValueNumber(ExportResult.ExportedWidth)));
        ExportedSizeArray.Add(MakeShareable(new FJsonValueNumber(ExportResult.ExportedHeight)));
        Response->SetArrayField(TEXT("exported_size"), ExportedSizeArray);
        
        Response->SetNumberField(TEXT("file_size"), (double)ExportResult.FileSize);
        Response->SetBoolField(TEXT("cleanup_required"), true);
        
        return Response;
    }
    else
    {
        return CreateErrorResponse(Result.GetErrorMessage());
    }
}

TSharedPtr<FJsonObject> FAssetCommands::HandleOpenAssetInEditor(const TSharedPtr<FJsonObject>& Params)
{
    // Validate required parameter
    static const TArray<FString> ValidParams = {
        TEXT("asset_path"), TEXT("force_open")
    };
    
    if (!ParamValidation::HasStringParam(Params, TEXT("asset_path")))
    {
        return ParamValidation::MissingParamsError(TEXT("asset_path is required"), ValidParams);
    }
    
    // Extract parameters
    FString AssetPath;
    bool bForceOpen = false;
    
    if (Params.IsValid())
    {
        Params->TryGetStringField(TEXT("asset_path"), AssetPath);
        Params->TryGetBoolField(TEXT("force_open"), bForceOpen);
    }
    
    // Delegate to LifecycleService
    TResult<FString> Result = LifecycleService->OpenAssetInEditor(AssetPath, bForceOpen);
    
    // Convert result to JSON response
    if (Result.IsSuccess())
    {
        const FString& EditorType = Result.GetValue();
        TSharedPtr<FJsonObject> Response = CreateSuccessResponse(FString::Printf(TEXT("Successfully opened asset: %s"), *AssetPath));
        Response->SetStringField(TEXT("asset_path"), AssetPath);
        Response->SetStringField(TEXT("editor_type"), EditorType);
        
        // Check if it was already open by querying the service
        TResult<bool> WasOpenResult = LifecycleService->IsAssetOpen(AssetPath);
        bool bWasAlreadyOpen = WasOpenResult.IsSuccess() && WasOpenResult.GetValue();
        Response->SetBoolField(TEXT("was_already_open"), bWasAlreadyOpen);
        
        return Response;
    }
    else
    {
        return CreateErrorResponse(Result.GetErrorMessage());
    }
}

TSharedPtr<FJsonObject> FAssetCommands::HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params)
{
    // Validate required parameter
    static const TArray<FString> ValidParams = {
        TEXT("asset_path"), TEXT("force_delete"), TEXT("show_confirmation")
    };
    
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return ParamValidation::MissingParamsError(TEXT("asset_path is required"), ValidParams);
    }
    
    // Extract optional parameters
    // NOTE: bShowConfirmation defaults to false for MCP calls because modal dialogs
    // can cause deadlocks when MCP is blocking on the game thread
    bool bForceDelete = false;
    bool bShowConfirmation = false;
    
    if (Params.IsValid())
    {
        Params->TryGetBoolField(TEXT("force_delete"), bForceDelete);
        Params->TryGetBoolField(TEXT("show_confirmation"), bShowConfirmation);
    }
    
    // Validate LifecycleService is initialized
    if (!LifecycleService.IsValid())
    {
        return CreateErrorResponse(TEXT("LifecycleService not initialized"));
    }
    
    // Delegate to LifecycleService
    TResult<bool> Result = LifecycleService->DeleteAsset(AssetPath, bForceDelete, bShowConfirmation);
    
    // Convert result to JSON response
    if (Result.IsSuccess())
    {
        TSharedPtr<FJsonObject> Response = CreateSuccessResponse(
            FString::Printf(TEXT("Successfully deleted asset: %s"), *AssetPath)
        );
        Response->SetStringField(TEXT("asset_path"), AssetPath);
        Response->SetBoolField(TEXT("deleted"), Result.GetValue());
        return Response;
    }
    else
    {
        return CreateErrorResponse(Result.GetErrorMessage());
    }
}

TSharedPtr<FJsonObject> FAssetCommands::HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params)
{
    // Validate required parameters
    static const TArray<FString> ValidParams = {
        TEXT("asset_path"), TEXT("destination_path"), TEXT("new_name")
    };
    
    FString AssetPath;
    FString DestinationPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return ParamValidation::MissingParamsError(TEXT("asset_path is required"), ValidParams);
    }
    if (!Params->TryGetStringField(TEXT("destination_path"), DestinationPath))
    {
        return ParamValidation::MissingParamsError(TEXT("destination_path is required"), ValidParams);
    }
    
    // Extract optional parameter
    FString NewName;
    Params->TryGetStringField(TEXT("new_name"), NewName);
    
    // Delegate to LifecycleService
    TResult<FAssetDuplicateResult> Result = LifecycleService->DuplicateAsset(
        AssetPath,
        DestinationPath,
        NewName
    );
    
    // Convert result to JSON response
    if (Result.IsSuccess())
    {
        const FAssetDuplicateResult& DuplicateResult = Result.GetValue();
        TSharedPtr<FJsonObject> Response = CreateSuccessResponse(
            FString::Printf(TEXT("Successfully duplicated asset: %s"), *DuplicateResult.NewPath)
        );
        Response->SetStringField(TEXT("original_path"), DuplicateResult.OriginalPath);
        Response->SetStringField(TEXT("new_path"), DuplicateResult.NewPath);
        Response->SetStringField(TEXT("asset_type"), DuplicateResult.AssetType);
        return Response;
    }
    else
    {
        return CreateErrorResponse(Result.GetErrorMessage());
    }
}

TSharedPtr<FJsonObject> FAssetCommands::HandleSaveAsset(const TSharedPtr<FJsonObject>& Params)
{
    // Validate required parameter
    static const TArray<FString> ValidParams = {
        TEXT("asset_path")
    };
    
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return ParamValidation::MissingParamsError(TEXT("asset_path is required"), ValidParams);
    }
    
    // Delegate to LifecycleService
    TResult<void> Result = LifecycleService->SaveAsset(AssetPath);
    
    // Convert result to JSON response
    if (Result.IsSuccess())
    {
        return CreateSuccessResponse(
            FString::Printf(TEXT("Successfully saved asset: %s"), *AssetPath)
        );
    }
    else
    {
        return CreateErrorResponse(Result.GetErrorMessage());
    }
}

TSharedPtr<FJsonObject> FAssetCommands::HandleSaveAllAssets(const TSharedPtr<FJsonObject>& Params)
{
    // Extract optional parameter
    bool bPromptUserToSave = false;
    Params->TryGetBoolField(TEXT("prompt_user"), bPromptUserToSave);
    
    // Delegate to LifecycleService
    TResult<int32> Result = LifecycleService->SaveAllDirtyAssets(bPromptUserToSave);
    
    // Convert result to JSON response
    if (Result.IsSuccess())
    {
        int32 SavedCount = Result.GetValue();
        TSharedPtr<FJsonObject> Response = CreateSuccessResponse(
            FString::Printf(TEXT("Successfully saved %d dirty asset(s)"), SavedCount)
        );
        Response->SetNumberField(TEXT("saved_count"), SavedCount);
        return Response;
    }
    else
    {
        return CreateErrorResponse(Result.GetErrorMessage());
    }
}

TSharedPtr<FJsonObject> FAssetCommands::HandleListReferences(const TSharedPtr<FJsonObject>& Params)
{
    // Validate required parameter
    static const TArray<FString> ValidParams = {
        TEXT("asset_path"), TEXT("include_referencers"), TEXT("include_dependencies")
    };
    
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return ParamValidation::MissingParamsError(TEXT("asset_path is required"), ValidParams);
    }
    
    // Extract optional parameters
    bool bIncludeReferencers = true;
    bool bIncludeDependencies = true;
    
    Params->TryGetBoolField(TEXT("include_referencers"), bIncludeReferencers);
    Params->TryGetBoolField(TEXT("include_dependencies"), bIncludeDependencies);
    
    // Delegate to LifecycleService
    TResult<FAssetReferencesResult> Result = LifecycleService->GetAssetReferences(
        AssetPath,
        bIncludeReferencers,
        bIncludeDependencies
    );
    
    // Convert result to JSON response
    if (Result.IsSuccess())
    {
        const FAssetReferencesResult& RefResult = Result.GetValue();
        TSharedPtr<FJsonObject> Response = CreateSuccessResponse(
            FString::Printf(TEXT("Found %d referencer(s) and %d dependenc(ies) for: %s"),
                RefResult.ReferencerCount,
                RefResult.DependencyCount,
                *RefResult.AssetPath)
        );
        
        Response->SetStringField(TEXT("asset_path"), RefResult.AssetPath);
        Response->SetNumberField(TEXT("referencer_count"), RefResult.ReferencerCount);
        Response->SetNumberField(TEXT("dependency_count"), RefResult.DependencyCount);
        
        // Build referencers array
        TArray<TSharedPtr<FJsonValue>> ReferencersArray;
        for (const FAssetReferenceInfo& RefInfo : RefResult.Referencers)
        {
            TSharedPtr<FJsonObject> RefObj = MakeShareable(new FJsonObject);
            RefObj->SetStringField(TEXT("asset_path"), RefInfo.AssetPath);
            RefObj->SetStringField(TEXT("asset_class"), RefInfo.AssetClass);
            RefObj->SetStringField(TEXT("display_name"), RefInfo.DisplayName);
            ReferencersArray.Add(MakeShareable(new FJsonValueObject(RefObj)));
        }
        Response->SetArrayField(TEXT("referencers"), ReferencersArray);
        
        // Build dependencies array
        TArray<TSharedPtr<FJsonValue>> DependenciesArray;
        for (const FAssetReferenceInfo& RefInfo : RefResult.Dependencies)
        {
            TSharedPtr<FJsonObject> RefObj = MakeShareable(new FJsonObject);
            RefObj->SetStringField(TEXT("asset_path"), RefInfo.AssetPath);
            RefObj->SetStringField(TEXT("asset_class"), RefInfo.AssetClass);
            RefObj->SetStringField(TEXT("display_name"), RefInfo.DisplayName);
            DependenciesArray.Add(MakeShareable(new FJsonValueObject(RefObj)));
        }
        Response->SetArrayField(TEXT("dependencies"), DependenciesArray);
        
        return Response;
    }
    else
    {
        return CreateErrorResponse(Result.GetErrorMessage());
    }
}

TSharedPtr<FJsonObject> FAssetCommands::CreateSuccessResponse(const FString& Message)
{
    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), Message);
    return Response;
}

TSharedPtr<FJsonObject> FAssetCommands::CreateErrorResponse(const FString& ErrorMessage)
{
    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), false);
    Response->SetStringField(TEXT("error"), ErrorMessage);
    return Response;
}

//-----------------------------------------------------------------------------
// Help Action
//-----------------------------------------------------------------------------

TSharedPtr<FJsonObject> FAssetCommands::HandleHelp(const TSharedPtr<FJsonObject>& Params)
{
    return FHelpFileReader::HandleHelp(TEXT("manage_asset"), Params);
}

//-----------------------------------------------------------------------------
// Search Action
//-----------------------------------------------------------------------------

TSharedPtr<FJsonObject> FAssetCommands::HandleSearchAssets(const TSharedPtr<FJsonObject>& Params)
{
    // Validate required parameter
    static const TArray<FString> ValidParams = {
        TEXT("search_term"), TEXT("asset_type"), TEXT("path"),
        TEXT("case_sensitive"), TEXT("include_engine_content"), TEXT("max_results")
    };
    
    FString SearchTerm;
    if (!Params->TryGetStringField(TEXT("search_term"), SearchTerm))
    {
        return ParamValidation::MissingParamsError(TEXT("search_term is required"), ValidParams);
    }

    FString AssetType;
    Params->TryGetStringField(TEXT("asset_type"), AssetType);

    FString Path = TEXT("/Game");
    Params->TryGetStringField(TEXT("path"), Path);

    bool bCaseSensitive = false;
    Params->TryGetBoolField(TEXT("case_sensitive"), bCaseSensitive);

    bool bIncludeEngineContent = false;
    Params->TryGetBoolField(TEXT("include_engine_content"), bIncludeEngineContent);

    int32 MaxResults = 100;
    double MaxResultsValue = 0.0;
    if (Params->TryGetNumberField(TEXT("max_results"), MaxResultsValue))
    {
        MaxResults = FMath::Max(1, static_cast<int32>(MaxResultsValue));
    }

    // Build asset filter
    FARFilter Filter;
    Filter.bRecursivePaths = true;
    Filter.PackagePaths.Add(*Path);
    if (bIncludeEngineContent)
    {
        Filter.PackagePaths.Add(TEXT("/Engine"));
    }
    
    // Handle asset type filtering
    if (!AssetType.IsEmpty())
    {
        FTopLevelAssetPath AssetClassPath = UClass::TryConvertShortTypeNameToPathName<UClass>(AssetType, ELogVerbosity::NoLogging);
        if (AssetClassPath.IsNull())
        {
            if (AssetType.Contains(TEXT("/")))
            {
                AssetClassPath = FTopLevelAssetPath(*AssetType);
            }
            else if (UClass* AssetClass = FindFirstObjectSafe<UClass>(*AssetType))
            {
                AssetClassPath = AssetClass->GetClassPathName();
            }
        }

        if (!AssetClassPath.IsNull())
        {
            Filter.ClassPaths.Add(AssetClassPath);
        }
    }

    // Get asset registry
    IAssetRegistry* AssetRegistry = ServiceContext->GetAssetRegistry();
    if (!AssetRegistry)
    {
        return CreateErrorResponse(TEXT("Failed to get Asset Registry"));
    }

    // Query assets
    TArray<FAssetData> Assets;
    AssetRegistry->GetAssets(Filter, Assets);

    // Filter and format results
    TArray<TSharedPtr<FJsonValue>> ItemArray;
    const ESearchCase::Type SearchCase = bCaseSensitive ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase;
    int32 MatchCount = 0;

    for (const FAssetData& Asset : Assets)
    {
        if (MatchCount >= MaxResults)
        {
            break;
        }

        const FString AssetName = Asset.AssetName.ToString();
        const FString ObjectPath = Asset.GetObjectPathString();

        // Check if search term matches asset name or path
        if (AssetName.Contains(SearchTerm, SearchCase) || ObjectPath.Contains(SearchTerm, SearchCase))
        {
            TSharedPtr<FJsonObject> ItemObj = MakeShared<FJsonObject>();
            ItemObj->SetStringField(TEXT("asset_name"), AssetName);
            ItemObj->SetStringField(TEXT("object_path"), ObjectPath);
            ItemObj->SetStringField(TEXT("package_path"), Asset.PackageName.ToString());
            ItemObj->SetStringField(TEXT("class_name"), Asset.AssetClassPath.GetAssetName().ToString());
            
            ItemArray.Add(MakeShared<FJsonValueObject>(ItemObj));
            MatchCount++;
        }
    }

    // Build response
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetArrayField(TEXT("items"), ItemArray);
    Response->SetNumberField(TEXT("count"), ItemArray.Num());

    // Include search info for debugging
    TSharedPtr<FJsonObject> SearchInfo = MakeShared<FJsonObject>();
    SearchInfo->SetStringField(TEXT("search_term"), SearchTerm);
    SearchInfo->SetStringField(TEXT("asset_type"), AssetType.IsEmpty() ? TEXT("all") : AssetType);
    SearchInfo->SetStringField(TEXT("path"), Path);
    SearchInfo->SetBoolField(TEXT("case_sensitive"), bCaseSensitive);
    SearchInfo->SetBoolField(TEXT("include_engine_content"), bIncludeEngineContent);
    SearchInfo->SetNumberField(TEXT("max_results"), MaxResults);
    SearchInfo->SetNumberField(TEXT("total_scanned"), Assets.Num());
    Response->SetObjectField(TEXT("search_info"), SearchInfo);

    return Response;
}
