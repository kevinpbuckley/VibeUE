#include "Commands/AssetCommands.h"
#include "Commands/CommonUtils.h"
#include "Engine/Engine.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor/EditorEngine.h"
#include "EditorAssetLibrary.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "AssetToolsModule.h"
#include "AssetImportTask.h"
#include "ContentBrowserModule.h"
#include "ObjectTools.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Factories/TextureFactory.h"
#include "Interfaces/IPluginManager.h"
#include "ImageUtils.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "TextureResource.h"
#include "RenderUtils.h"

// Reentrancy guard for texture import to avoid TaskGraph recursion/assertions
static FThreadSafeBool GVibeUE_ImportInProgress(false);

FAssetCommands::FAssetCommands()
{
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
    // IMPORT-ONLY: All rasterization / SVG / procedural generation moved to Python side.
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

    if (SourceFile.IsEmpty())
    {
        return CreateErrorResponse(TEXT("file_path is required"));
    }
    if (!FPaths::FileExists(SourceFile))
    {
        return CreateErrorResponse(FString::Printf(TEXT("Source file does not exist: %s"), *SourceFile));
    }

    if (!DestinationPath.StartsWith(TEXT("/")))
    {
        DestinationPath = TEXT("/Game/") + DestinationPath;
    }
    if (!UEditorAssetLibrary::DoesDirectoryExist(DestinationPath) && !UEditorAssetLibrary::MakeDirectory(DestinationPath))
    {
        return CreateErrorResponse(FString::Printf(TEXT("Failed to create destination path: %s"), *DestinationPath));
    }

    // Reentrancy guard retained (avoids prior TaskGraph recursion crash when multiple imports fired).
    if (GVibeUE_ImportInProgress)
    {
        return CreateErrorResponse(TEXT("Another texture import is already in progress"));
    }
    struct FScopedImportFlag { FScopedImportFlag(){ GVibeUE_ImportInProgress = true; } ~FScopedImportFlag(){ GVibeUE_ImportInProgress = false; } } ScopedImportFlag;

    FString Ext = FPaths::GetExtension(SourceFile, false).ToLower();
    const TSet<FString> RasterExt = { TEXT("png"), TEXT("jpg"), TEXT("jpeg"), TEXT("tga"), TEXT("bmp"), TEXT("exr"), TEXT("hdr"), TEXT("tif"), TEXT("tiff"), TEXT("dds"), TEXT("psd") };
    if (!RasterExt.Contains(Ext))
    {
        return CreateErrorResponse(TEXT("Unsupported format for import-only path (convert externally first)"));
    }

    // Load & decode raster via ImageWrapper only – no gradients, no dithering, no SVG logic.
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *SourceFile))
    {
        return CreateErrorResponse(TEXT("Failed to read file"));
    }
    if (FileData.Num() == 0)
    {
        return CreateErrorResponse(TEXT("File is empty"));
    }

    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    EImageFormat Detected = ImageWrapperModule.DetectImageFormat(FileData.GetData(), FileData.Num());
    if (Detected == EImageFormat::Invalid)
    {
        return CreateErrorResponse(TEXT("Unable to detect image format"));
    }
    TSharedPtr<IImageWrapper> Wrapper = ImageWrapperModule.CreateImageWrapper(Detected);
    if (!Wrapper.IsValid() || !Wrapper->SetCompressed(FileData.GetData(), FileData.Num()))
    {
        return CreateErrorResponse(TEXT("Failed to parse image data"));
    }
    TArray<uint8> Raw;
    if (!Wrapper->GetRaw(ERGBFormat::RGBA, 8, Raw))
    {
        return CreateErrorResponse(TEXT("Failed to decode raw RGBA"));
    }
    const int32 Width = Wrapper->GetWidth();
    const int32 Height = Wrapper->GetHeight();
    if (Width <= 0 || Height <= 0)
    {
        return CreateErrorResponse(TEXT("Invalid image dimensions"));
    }
    if (Raw.Num() != Width * Height * 4)
    {
        return CreateErrorResponse(TEXT("Decoded size mismatch"));
    }

    TArray<FColor> Pixels; Pixels.SetNumUninitialized(Width * Height);
    const uint8* Src = Raw.GetData();
    for (int32 i=0; i<Width*Height; ++i)
    {
        int32 bi = i*4; Pixels[i] = FColor(Src[bi+0], Src[bi+1], Src[bi+2], Src[bi+3]);
    }

    FString FinalName = TextureName;
    if (FinalName.IsEmpty())
    {
        FinalName = TEXT("T_") + FPaths::GetBaseFilename(SourceFile);
    }
    const FString PackagePath = DestinationPath + TEXT("/") + FinalName;
    const FString AssetObjectPath = PackagePath + TEXT(".") + FinalName;
    if (bReplaceExisting && UEditorAssetLibrary::DoesAssetExist(AssetObjectPath))
    {
        UEditorAssetLibrary::DeleteAsset(AssetObjectPath);
    }

    UPackage* Pkg = CreatePackage(*PackagePath);
    EObjectFlags Flags = RF_Public | RF_Standalone;
    FCreateTexture2DParameters TexParams; TexParams.bDeferCompression = true; TexParams.bSRGB = true;
    UTexture2D* NewTex = FImageUtils::CreateTexture2D(Width, Height, Pixels, Pkg, FinalName, Flags, TexParams);
    if (!NewTex)
    {
        return CreateErrorResponse(TEXT("Failed to create texture asset"));
    }
    NewTex->CompressionSettings = TC_Default;
    NewTex->SRGB = true;
    NewTex->MarkPackageDirty();
    if (bSave && !UEditorAssetLibrary::SaveAsset(AssetObjectPath, bSave))
    {
        return CreateErrorResponse(TEXT("Failed to save asset"));
    }

    TSharedPtr<FJsonObject> Resp = CreateSuccessResponse(TEXT("Texture imported"));
    Resp->SetStringField(TEXT("asset_path"), AssetObjectPath);
    Resp->SetStringField(TEXT("destination_path"), DestinationPath);
    Resp->SetStringField(TEXT("source_file"), SourceFile);
    Resp->SetStringField(TEXT("asset_class"), TEXT("Texture2D"));
    Resp->SetBoolField(TEXT("import_only"), true);
    return Resp;
}

TSharedPtr<FJsonObject> FAssetCommands::HandleExportTextureForAnalysis(const TSharedPtr<FJsonObject>& Params)
{
    // Extract parameters
    FString AssetPath = TEXT("");
    FString ExportFormat = TEXT("PNG");
    FString TempFolder = TEXT("");
    TArray<int32> MaxSize;
    
    if (Params.IsValid())
    {
        Params->TryGetStringField(TEXT("asset_path"), AssetPath);
        Params->TryGetStringField(TEXT("export_format"), ExportFormat);
        Params->TryGetStringField(TEXT("temp_folder"), TempFolder);
        
        // Parse max_size array if provided
        const TArray<TSharedPtr<FJsonValue>>* MaxSizeArray = nullptr;
        if (Params->TryGetArrayField(TEXT("max_size"), MaxSizeArray) && MaxSizeArray->Num() >= 2)
        {
            MaxSize.Add((*MaxSizeArray)[0]->AsNumber());
            MaxSize.Add((*MaxSizeArray)[1]->AsNumber());
        }
    }
    
    if (AssetPath.IsEmpty())
    {
        return CreateErrorResponse(TEXT("Asset path is required"));
    }
    
    // Load the texture asset
    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath));
    }
    
    UTexture2D* Texture = Cast<UTexture2D>(Asset);
    if (!Texture)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Asset is not a Texture2D: %s"), *AssetPath));
    }
    
    // Get texture dimensions
    int32 TextureWidth = Texture->GetSizeX();
    int32 TextureHeight = Texture->GetSizeY();
    
    // Apply max size constraint if specified
    int32 ExportWidth = TextureWidth;
    int32 ExportHeight = TextureHeight;
    
    if (MaxSize.Num() >= 2 && MaxSize[0] > 0 && MaxSize[1] > 0)
    {
        float ScaleX = (float)MaxSize[0] / (float)TextureWidth;
        float ScaleY = (float)MaxSize[1] / (float)TextureHeight;
        float Scale = FMath::Min(ScaleX, ScaleY);
        
        if (Scale < 1.0f)
        {
            ExportWidth = FMath::RoundToInt(TextureWidth * Scale);
            ExportHeight = FMath::RoundToInt(TextureHeight * Scale);
        }
    }
    
    // Generate temp file path
    if (TempFolder.IsEmpty())
    {
        TempFolder = FPaths::ProjectSavedDir() / TEXT("Temp") / TEXT("TextureExports");
    }
    
    // Ensure temp directory exists
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*TempFolder))
    {
        if (!PlatformFile.CreateDirectoryTree(*TempFolder))
        {
            return CreateErrorResponse(FString::Printf(TEXT("Failed to create temp directory: %s"), *TempFolder));
        }
    }
    
    // Generate unique filename
    FString AssetName = FPaths::GetBaseFilename(AssetPath);
    FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    FString UniqueId = FGuid::NewGuid().ToString(EGuidFormats::Short);
    FString FileName = FString::Printf(TEXT("%s_%s_%s.%s"), *AssetName, *Timestamp, *UniqueId, *ExportFormat.ToLower());
    FString TempFilePath = TempFolder / FileName;
    
    // Read texture data
    TArray<FColor> RawData;
    
    // Ensure texture is loaded and has valid platform data
    if (!Texture->GetPlatformData() || !Texture->GetPlatformData()->Mips.Num())
    {
        return CreateErrorResponse(TEXT("Texture has no valid platform data"));
    }
    
    // Get the first mip level
    const FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
    
    // Lock the texture data
    const void* TextureData = Mip.BulkData.LockReadOnly();
    if (!TextureData)
    {
        return CreateErrorResponse(TEXT("Failed to lock texture data"));
    }
    
    // Convert texture data to FColor array
    EPixelFormat PixelFormat = Texture->GetPixelFormat();
    
    if (PixelFormat == PF_B8G8R8A8)
    {
        // BGRA format - most common
        const FColor* ColorData = static_cast<const FColor*>(TextureData);
        RawData.Append(ColorData, TextureWidth * TextureHeight);
    }
    else if (PixelFormat == PF_R8G8B8A8)
    {
        // RGBA format
        const uint8* ByteData = static_cast<const uint8*>(TextureData);
        RawData.Reserve(TextureWidth * TextureHeight);
        
        for (int32 i = 0; i < TextureWidth * TextureHeight; i++)
        {
            uint8 R = ByteData[i * 4 + 0];
            uint8 G = ByteData[i * 4 + 1];
            uint8 B = ByteData[i * 4 + 2];
            uint8 A = ByteData[i * 4 + 3];
            RawData.Add(FColor(R, G, B, A));
        }
    }
    else
    {
        // Unlock and return error for unsupported formats
        Mip.BulkData.Unlock();
        return CreateErrorResponse(FString::Printf(TEXT("Unsupported pixel format: %d"), (int32)PixelFormat));
    }
    
    // Unlock the texture data
    Mip.BulkData.Unlock();
    
    // Resize if needed
    TArray<FColor> FinalData;
    if (ExportWidth != TextureWidth || ExportHeight != TextureHeight)
    {
        // Simple bilinear resize
        FinalData.Reserve(ExportWidth * ExportHeight);
        
        for (int32 Y = 0; Y < ExportHeight; Y++)
        {
            for (int32 X = 0; X < ExportWidth; X++)
            {
                float SrcX = (float)X * (float)TextureWidth / (float)ExportWidth;
                float SrcY = (float)Y * (float)TextureHeight / (float)ExportHeight;
                
                int32 X1 = FMath::FloorToInt(SrcX);
                int32 Y1 = FMath::FloorToInt(SrcY);
                int32 X2 = FMath::Min(X1 + 1, TextureWidth - 1);
                int32 Y2 = FMath::Min(Y1 + 1, TextureHeight - 1);
                
                FColor Color = RawData[Y1 * TextureWidth + X1];
                FinalData.Add(Color);
            }
        }
    }
    else
    {
        FinalData = MoveTemp(RawData);
    }
    
    // Convert to requested format and save
    bool bSaveSuccess = false;
    
    if (ExportFormat.ToUpper() == TEXT("PNG"))
    {
        // Convert FColor to uint8 array for PNG
        TArray<uint8> PNGData;
        PNGData.Reserve(ExportWidth * ExportHeight * 4);
        
        for (const FColor& Color : FinalData)
        {
            PNGData.Add(Color.R);
            PNGData.Add(Color.G);
            PNGData.Add(Color.B);
            PNGData.Add(Color.A);
        }
        
        // Use ImageWrapper to save as PNG
        IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
        TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
        
        if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(PNGData.GetData(), PNGData.Num(), ExportWidth, ExportHeight, ERGBFormat::RGBA, 8))
        {
            const TArray64<uint8>& CompressedData = ImageWrapper->GetCompressed();
            bSaveSuccess = FFileHelper::SaveArrayToFile(TArray<uint8>(CompressedData), *TempFilePath);
        }
    }
    else if (ExportFormat.ToUpper() == TEXT("TGA"))
    {
        // Convert FColor to uint8 array for TGA
        TArray<uint8> TGAData;
        TGAData.Reserve(ExportWidth * ExportHeight * 4);
        
        for (const FColor& Color : FinalData)
        {
            TGAData.Add(Color.B);  // TGA is BGR
            TGAData.Add(Color.G);
            TGAData.Add(Color.R);
            TGAData.Add(Color.A);
        }
        
        IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
        TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::TGA);
        
        if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(TGAData.GetData(), TGAData.Num(), ExportWidth, ExportHeight, ERGBFormat::BGRA, 8))
        {
            const TArray64<uint8>& CompressedData = ImageWrapper->GetCompressed();
            bSaveSuccess = FFileHelper::SaveArrayToFile(TArray<uint8>(CompressedData), *TempFilePath);
        }
    }
    else
    {
        return CreateErrorResponse(FString::Printf(TEXT("Unsupported export format: %s"), *ExportFormat));
    }
    
    if (!bSaveSuccess)
    {
        return CreateErrorResponse(TEXT("Failed to save exported texture file"));
    }
    
    // Get file size
    int64 FileSize = PlatformFile.FileSize(*TempFilePath);
    
    // Create success response
    TSharedPtr<FJsonObject> Response = CreateSuccessResponse(TEXT("Texture exported successfully"));
    Response->SetStringField(TEXT("asset_path"), AssetPath);
    Response->SetStringField(TEXT("temp_file_path"), TempFilePath);
    Response->SetStringField(TEXT("export_format"), ExportFormat);
    
    TArray<TSharedPtr<FJsonValue>> ExportedSizeArray;
    ExportedSizeArray.Add(MakeShareable(new FJsonValueNumber(ExportWidth)));
    ExportedSizeArray.Add(MakeShareable(new FJsonValueNumber(ExportHeight)));
    Response->SetArrayField(TEXT("exported_size"), ExportedSizeArray);
    
    Response->SetNumberField(TEXT("file_size"), (double)FileSize);
    Response->SetBoolField(TEXT("cleanup_required"), true);
    
    return Response;
}

TSharedPtr<FJsonObject> FAssetCommands::HandleOpenAssetInEditor(const TSharedPtr<FJsonObject>& Params)
{
    // Extract parameters
    FString AssetPath = TEXT("");
    bool bForceOpen = false;
    
    if (Params.IsValid())
    {
        Params->TryGetStringField(TEXT("asset_path"), AssetPath);
        Params->TryGetBoolField(TEXT("force_open"), bForceOpen);
    }
    
    if (AssetPath.IsEmpty())
    {
        return CreateErrorResponse(TEXT("asset_path parameter is required"));
    }
    
    // Ensure asset path starts with /Game or other valid mount point
    if (!AssetPath.StartsWith(TEXT("/Game")) && !AssetPath.StartsWith(TEXT("/Engine")) && !AssetPath.StartsWith(TEXT("/Script")))
    {
        // Try adding /Game prefix if it's a relative path
        if (!AssetPath.StartsWith(TEXT("/")))
        {
            AssetPath = TEXT("/Game/") + AssetPath;
        }
        else
        {
            AssetPath = TEXT("/Game") + AssetPath;
        }
    }
    
    // Try to find the asset
    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset)
    {
        // Try with different extensions if not found
        TArray<FString> Extensions = {TEXT(".uasset"), TEXT("")};
        bool bFoundAsset = false;
        
        for (const FString& Extension : Extensions)
        {
            FString TestPath = AssetPath + Extension;
            Asset = UEditorAssetLibrary::LoadAsset(TestPath);
            if (Asset)
            {
                AssetPath = TestPath;
                bFoundAsset = true;
                break;
            }
        }
        
        if (!bFoundAsset)
        {
            // Check if the asset exists in the Content Browser but failed to load
            FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
            IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
            
            FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
            if (AssetData.IsValid())
            {
                return CreateErrorResponse(FString::Printf(TEXT("Asset exists but failed to load: %s. Asset may be corrupted or have dependencies issues."), *AssetPath));
            }
            else
            {
                return CreateErrorResponse(FString::Printf(TEXT("Asset not found in registry: %s. Check if the asset path is correct."), *AssetPath));
            }
        }
    }
    
    // Get the Asset Editor Subsystem
    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!AssetEditorSubsystem)
    {
        return CreateErrorResponse(TEXT("Failed to get Asset Editor Subsystem"));
    }
    
    // Check if asset is already open
    bool bWasAlreadyOpen = AssetEditorSubsystem->FindEditorForAsset(Asset, false) != nullptr;
    
    // Open the asset in its appropriate editor
    bool bSuccess = false;
    FString EditorType = TEXT("Unknown");
    
    try
    {
        if (bForceOpen && bWasAlreadyOpen)
        {
            // Close existing editor first if force open is requested
            AssetEditorSubsystem->CloseAllEditorsForAsset(Asset);
        }
        
        bSuccess = AssetEditorSubsystem->OpenEditorForAsset(Asset);
        
        // Determine editor type based on asset class (do this regardless of bSuccess value)
        if (Asset->IsA<UTexture>())
        {
            EditorType = TEXT("Texture Editor");
        }
        else if (Asset->IsA<UMaterial>() || Asset->IsA<UMaterialInstance>())
        {
            EditorType = TEXT("Material Editor");
        }
        else if (Asset->IsA<UBlueprint>())
        {
            EditorType = TEXT("Blueprint Editor");
        }
        else if (Asset->IsA<UStaticMesh>())
        {
            EditorType = TEXT("Static Mesh Editor");
        }
        else if (Asset->IsA<USoundBase>())
        {
            EditorType = TEXT("Audio Editor");
        }
        else if (Asset->IsA<UDataTable>())
        {
            EditorType = TEXT("Data Table Editor");
        }
        else
        {
            EditorType = Asset->GetClass()->GetName() + TEXT(" Editor");
        }
        
        // Check if editor is now open (sometimes OpenEditorForAsset returns false but still opens the editor)
        bool bIsNowOpen = AssetEditorSubsystem->FindEditorForAsset(Asset, false) != nullptr;
        
        if (!bSuccess && !bIsNowOpen)
        {
            // Only return error if both the function failed AND the editor is not actually open
            FString AssetClassName = Asset ? Asset->GetClass()->GetName() : TEXT("Unknown");
            return CreateErrorResponse(FString::Printf(TEXT("OpenEditorForAsset failed for %s (Class: %s). Asset may not have an appropriate editor or the editor subsystem failed."), *AssetPath, *AssetClassName));
        }
        
        // If we get here, either bSuccess was true OR the editor is actually open despite bSuccess being false
        bSuccess = true; // Force success since the editor is open
    }
    catch (const std::exception& e)
    {
        return CreateErrorResponse(FString::Printf(TEXT("C++ exception occurred while opening asset %s: %s"), *AssetPath, UTF8_TO_TCHAR(e.what())));
    }
    catch (...)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Unknown exception occurred while opening asset: %s"), *AssetPath));
    }
    
    // Success case - this should only be reached if bSuccess is true
    TSharedPtr<FJsonObject> Response = CreateSuccessResponse(FString::Printf(TEXT("Successfully opened asset: %s"), *AssetPath));
    Response->SetStringField(TEXT("asset_path"), AssetPath);
    Response->SetStringField(TEXT("editor_type"), EditorType);
    Response->SetBoolField(TEXT("was_already_open"), bWasAlreadyOpen);
    return Response;
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
