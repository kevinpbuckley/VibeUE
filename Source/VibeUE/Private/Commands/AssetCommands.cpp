#include "Commands/AssetCommands.h"
#include "Services/Asset/AssetDiscoveryService.h"
#include "Services/Asset/AssetLifecycleService.h"
#include "Services/Asset/AssetImportService.h"
#include "Core/ServiceContext.h"
#include "Core/Result.h"

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
    if (CommandType == TEXT("import_texture_asset"))
    {
        return HandleImportTextureAsset(Params);
    }
    else if (CommandType == TEXT("export_texture_for_analysis"))
    {
        return HandleExportTextureForAnalysis(Params);
    }
    else if (CommandType == TEXT("OpenAssetInEditor"))
    {
        return HandleOpenAssetInEditor(Params);
    }

    return CreateErrorResponse(FString::Printf(TEXT("Unknown asset command: %s"), *CommandType));
}


TSharedPtr<FJsonObject> FAssetCommands::HandleImportTextureAsset(const TSharedPtr<FJsonObject>& Params)
{
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
