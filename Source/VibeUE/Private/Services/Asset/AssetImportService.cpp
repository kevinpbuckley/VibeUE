// Copyright VibeUE 2025

#include "Services/Asset/AssetImportService.h"
#include "Core/ErrorCodes.h"
#include "EditorAssetLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "HAL/PlatformFilemanager.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"

// Reentrancy guard for texture import
static FThreadSafeBool GVibeUE_ImportInProgress(false);

FAssetImportService::FAssetImportService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

TResult<void> FAssetImportService::ValidateSourceFile(const FString& SourceFile) const
{
    if (SourceFile.IsEmpty())
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::PARAM_EMPTY,
            TEXT("Source file path is required")
        );
    }
    
    if (!FPaths::FileExists(SourceFile))
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::ASSET_NOT_FOUND,
            FString::Printf(TEXT("Source file does not exist: %s"), *SourceFile)
        );
    }
    
    return TResult<void>::Success();
}

TResult<void> FAssetImportService::EnsureDestinationPath(const FString& DestinationPath) const
{
    if (!UEditorAssetLibrary::DoesDirectoryExist(DestinationPath))
    {
        if (!UEditorAssetLibrary::MakeDirectory(DestinationPath))
        {
            return TResult<void>::Error(
                VibeUE::ErrorCodes::OPERATION_FAILED,
                FString::Printf(TEXT("Failed to create destination path: %s"), *DestinationPath)
            );
        }
    }
    
    return TResult<void>::Success();
}

TResult<void> FAssetImportService::LoadImageFile(
    const FString& SourceFile,
    int32& OutWidth,
    int32& OutHeight,
    TArray<FColor>& OutPixels
) const
{
    // Validate file format
    FString Ext = FPaths::GetExtension(SourceFile, false).ToLower();
    const TSet<FString> RasterExt = {
        TEXT("png"), TEXT("jpg"), TEXT("jpeg"), TEXT("tga"), TEXT("bmp"),
        TEXT("exr"), TEXT("hdr"), TEXT("tif"), TEXT("tiff"), TEXT("dds"), TEXT("psd")
    };
    
    if (!RasterExt.Contains(Ext))
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::TEXTURE_FORMAT_UNSUPPORTED,
            TEXT("Unsupported image format")
        );
    }
    
    // Load file data
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *SourceFile))
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::ASSET_LOAD_FAILED,
            TEXT("Failed to read file")
        );
    }
    
    if (FileData.Num() == 0)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::TEXTURE_DATA_INVALID,
            TEXT("File is empty")
        );
    }
    
    // Detect and decode image
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    EImageFormat Detected = ImageWrapperModule.DetectImageFormat(FileData.GetData(), FileData.Num());
    
    if (Detected == EImageFormat::Invalid)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::TEXTURE_FORMAT_UNSUPPORTED,
            TEXT("Unable to detect image format")
        );
    }
    
    TSharedPtr<IImageWrapper> Wrapper = ImageWrapperModule.CreateImageWrapper(Detected);
    if (!Wrapper.IsValid() || !Wrapper->SetCompressed(FileData.GetData(), FileData.Num()))
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::TEXTURE_DATA_INVALID,
            TEXT("Failed to parse image data")
        );
    }
    
    TArray<uint8> Raw;
    if (!Wrapper->GetRaw(ERGBFormat::RGBA, 8, Raw))
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::TEXTURE_DATA_INVALID,
            TEXT("Failed to decode raw RGBA")
        );
    }
    
    OutWidth = Wrapper->GetWidth();
    OutHeight = Wrapper->GetHeight();
    
    if (OutWidth <= 0 || OutHeight <= 0)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::TEXTURE_DATA_INVALID,
            TEXT("Invalid image dimensions")
        );
    }
    
    if (Raw.Num() != OutWidth * OutHeight * 4)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::TEXTURE_SIZE_MISMATCH,
            TEXT("Decoded size mismatch")
        );
    }
    
    // Convert to FColor array
    OutPixels.SetNumUninitialized(OutWidth * OutHeight);
    const uint8* Src = Raw.GetData();
    for (int32 i = 0; i < OutWidth * OutHeight; ++i)
    {
        int32 bi = i * 4;
        OutPixels[i] = FColor(Src[bi + 0], Src[bi + 1], Src[bi + 2], Src[bi + 3]);
    }
    
    return TResult<void>::Success();
}

UTexture2D* FAssetImportService::CreateTextureAsset(
    int32 Width,
    int32 Height,
    const TArray<FColor>& Pixels,
    const FString& PackagePath,
    const FString& TextureName
) const
{
    UPackage* Pkg = CreatePackage(*PackagePath);
    EObjectFlags Flags = RF_Public | RF_Standalone;
    
    FCreateTexture2DParameters TexParams;
    TexParams.bDeferCompression = true;
    TexParams.bSRGB = true;
    
    UTexture2D* NewTex = FImageUtils::CreateTexture2D(Width, Height, Pixels, Pkg, TextureName, Flags, TexParams);
    
    if (NewTex)
    {
        NewTex->CompressionSettings = TC_Default;
        NewTex->SRGB = true;
        NewTex->MarkPackageDirty();
    }
    
    return NewTex;
}

TResult<FTextureImportResult> FAssetImportService::ImportTexture(
    const FString& SourceFile,
    const FString& DestinationPath,
    const FString& TextureName,
    bool bReplaceExisting,
    bool bSave
)
{
    // Validate source file
    TResult<void> ValidationResult = ValidateSourceFile(SourceFile);
    if (ValidationResult.IsError())
    {
        return TResult<FTextureImportResult>::Error(
            ValidationResult.GetErrorCode(),
            ValidationResult.GetErrorMessage()
        );
    }
    
    // Normalize destination path
    FString NormalizedDestPath = DestinationPath;
    if (!NormalizedDestPath.StartsWith(TEXT("/")))
    {
        NormalizedDestPath = TEXT("/Game/") + NormalizedDestPath;
    }
    
    // Ensure destination exists
    TResult<void> PathResult = EnsureDestinationPath(NormalizedDestPath);
    if (PathResult.IsError())
    {
        return TResult<FTextureImportResult>::Error(
            PathResult.GetErrorCode(),
            PathResult.GetErrorMessage()
        );
    }
    
    // Reentrancy guard
    if (GVibeUE_ImportInProgress)
    {
        return TResult<FTextureImportResult>::Error(
            VibeUE::ErrorCodes::TEXTURE_IMPORT_IN_PROGRESS,
            TEXT("Another texture import is already in progress")
        );
    }
    
    struct FScopedImportFlag
    {
        FScopedImportFlag() { GVibeUE_ImportInProgress = true; }
        ~FScopedImportFlag() { GVibeUE_ImportInProgress = false; }
    } ScopedImportFlag;
    
    // Load image
    int32 Width, Height;
    TArray<FColor> Pixels;
    TResult<void> LoadResult = LoadImageFile(SourceFile, Width, Height, Pixels);
    if (LoadResult.IsError())
    {
        return TResult<FTextureImportResult>::Error(
            LoadResult.GetErrorCode(),
            LoadResult.GetErrorMessage()
        );
    }
    
    // Generate texture name
    FString FinalName = TextureName;
    if (FinalName.IsEmpty())
    {
        FinalName = TEXT("T_") + FPaths::GetBaseFilename(SourceFile);
    }
    
    const FString PackagePath = NormalizedDestPath + TEXT("/") + FinalName;
    const FString AssetObjectPath = PackagePath + TEXT(".") + FinalName;
    
    // Replace existing if requested
    if (bReplaceExisting && UEditorAssetLibrary::DoesAssetExist(AssetObjectPath))
    {
        UEditorAssetLibrary::DeleteAsset(AssetObjectPath);
    }
    
    // Create texture
    UTexture2D* NewTex = CreateTextureAsset(Width, Height, Pixels, PackagePath, FinalName);
    if (!NewTex)
    {
        return TResult<FTextureImportResult>::Error(
            VibeUE::ErrorCodes::TEXTURE_IMPORT_FAILED,
            TEXT("Failed to create texture asset")
        );
    }
    
    // Save if requested
    if (bSave && !UEditorAssetLibrary::SaveAsset(AssetObjectPath, bSave))
    {
        return TResult<FTextureImportResult>::Error(
            VibeUE::ErrorCodes::OPERATION_FAILED,
            TEXT("Failed to save asset")
        );
    }
    
    // Build result
    FTextureImportResult Result;
    Result.AssetPath = AssetObjectPath;
    Result.DestinationPath = NormalizedDestPath;
    Result.SourceFile = SourceFile;
    Result.AssetClass = TEXT("Texture2D");
    
    return TResult<FTextureImportResult>::Success(Result);
}

TResult<void> FAssetImportService::ReadTextureData(UTexture2D* Texture, TArray<FColor>& OutPixels) const
{
    if (!Texture)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::PARAM_INVALID,
            TEXT("Texture cannot be null")
        );
    }
    
    if (!Texture->GetPlatformData() || !Texture->GetPlatformData()->Mips.Num())
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::TEXTURE_DATA_INVALID,
            TEXT("Texture has no valid platform data")
        );
    }
    
    const FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
    const void* TextureData = Mip.BulkData.LockReadOnly();
    
    if (!TextureData)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::TEXTURE_DATA_INVALID,
            TEXT("Failed to lock texture data")
        );
    }
    
    int32 TextureWidth = Texture->GetSizeX();
    int32 TextureHeight = Texture->GetSizeY();
    EPixelFormat PixelFormat = Texture->GetPixelFormat();
    
    if (PixelFormat == PF_B8G8R8A8)
    {
        const FColor* ColorData = static_cast<const FColor*>(TextureData);
        OutPixels.Append(ColorData, TextureWidth * TextureHeight);
    }
    else if (PixelFormat == PF_R8G8B8A8)
    {
        const uint8* ByteData = static_cast<const uint8*>(TextureData);
        OutPixels.Reserve(TextureWidth * TextureHeight);
        
        for (int32 i = 0; i < TextureWidth * TextureHeight; i++)
        {
            uint8 R = ByteData[i * 4 + 0];
            uint8 G = ByteData[i * 4 + 1];
            uint8 B = ByteData[i * 4 + 2];
            uint8 A = ByteData[i * 4 + 3];
            OutPixels.Add(FColor(R, G, B, A));
        }
    }
    else
    {
        Mip.BulkData.Unlock();
        return TResult<void>::Error(
            VibeUE::ErrorCodes::TEXTURE_FORMAT_UNSUPPORTED,
            FString::Printf(TEXT("Unsupported pixel format: %d"), (int32)PixelFormat)
        );
    }
    
    Mip.BulkData.Unlock();
    return TResult<void>::Success();
}

void FAssetImportService::ResizeImage(
    const TArray<FColor>& InPixels,
    int32 InWidth,
    int32 InHeight,
    int32 OutWidth,
    int32 OutHeight,
    TArray<FColor>& OutPixels
) const
{
    OutPixels.Reserve(OutWidth * OutHeight);
    
    for (int32 Y = 0; Y < OutHeight; Y++)
    {
        for (int32 X = 0; X < OutWidth; X++)
        {
            float SrcX = (float)X * (float)InWidth / (float)OutWidth;
            float SrcY = (float)Y * (float)InHeight / (float)OutHeight;
            
            int32 X1 = FMath::FloorToInt(SrcX);
            int32 Y1 = FMath::FloorToInt(SrcY);
            
            X1 = FMath::Clamp(X1, 0, InWidth - 1);
            Y1 = FMath::Clamp(Y1, 0, InHeight - 1);
            
            FColor Color = InPixels[Y1 * InWidth + X1];
            OutPixels.Add(Color);
        }
    }
}

TResult<void> FAssetImportService::SaveImageToFile(
    const TArray<FColor>& Pixels,
    int32 Width,
    int32 Height,
    const FString& FilePath,
    const FString& Format
) const
{
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    
    if (Format.ToUpper() == TEXT("PNG"))
    {
        TArray<uint8> PNGData;
        PNGData.Reserve(Width * Height * 4);
        
        for (const FColor& Color : Pixels)
        {
            PNGData.Add(Color.R);
            PNGData.Add(Color.G);
            PNGData.Add(Color.B);
            PNGData.Add(Color.A);
        }
        
        TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
        if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(PNGData.GetData(), PNGData.Num(), Width, Height, ERGBFormat::RGBA, 8))
        {
            const TArray64<uint8>& CompressedData = ImageWrapper->GetCompressed();
            if (FFileHelper::SaveArrayToFile(TArray<uint8>(CompressedData), *FilePath))
            {
                return TResult<void>::Success();
            }
        }
    }
    else if (Format.ToUpper() == TEXT("TGA"))
    {
        TArray<uint8> TGAData;
        TGAData.Reserve(Width * Height * 4);
        
        for (const FColor& Color : Pixels)
        {
            TGAData.Add(Color.B);
            TGAData.Add(Color.G);
            TGAData.Add(Color.R);
            TGAData.Add(Color.A);
        }
        
        TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::TGA);
        if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(TGAData.GetData(), TGAData.Num(), Width, Height, ERGBFormat::BGRA, 8))
        {
            const TArray64<uint8>& CompressedData = ImageWrapper->GetCompressed();
            if (FFileHelper::SaveArrayToFile(TArray<uint8>(CompressedData), *FilePath))
            {
                return TResult<void>::Success();
            }
        }
    }
    else
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::TEXTURE_FORMAT_UNSUPPORTED,
            FString::Printf(TEXT("Unsupported export format: %s"), *Format)
        );
    }
    
    return TResult<void>::Error(
        VibeUE::ErrorCodes::ASSET_EXPORT_FAILED,
        TEXT("Failed to save exported texture file")
    );
}

TResult<FTextureExportResult> FAssetImportService::ExportTextureForAnalysis(
    const FString& AssetPath,
    const FString& ExportFormat,
    const FString& TempFolder,
    int32 MaxWidth,
    int32 MaxHeight
)
{
    if (AssetPath.IsEmpty())
    {
        return TResult<FTextureExportResult>::Error(
            VibeUE::ErrorCodes::PARAM_EMPTY,
            TEXT("Asset path is required")
        );
    }
    
    // Load texture
    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset)
    {
        return TResult<FTextureExportResult>::Error(
            VibeUE::ErrorCodes::ASSET_NOT_FOUND,
            FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath)
        );
    }
    
    UTexture2D* Texture = Cast<UTexture2D>(Asset);
    if (!Texture)
    {
        return TResult<FTextureExportResult>::Error(
            VibeUE::ErrorCodes::ASSET_TYPE_INCORRECT,
            FString::Printf(TEXT("Asset is not a Texture2D: %s"), *AssetPath)
        );
    }
    
    // Read texture data
    TArray<FColor> RawData;
    TResult<void> ReadResult = ReadTextureData(Texture, RawData);
    if (ReadResult.IsError())
    {
        return TResult<FTextureExportResult>::Error(
            ReadResult.GetErrorCode(),
            ReadResult.GetErrorMessage()
        );
    }
    
    int32 TextureWidth = Texture->GetSizeX();
    int32 TextureHeight = Texture->GetSizeY();
    
    // Apply size constraints
    int32 ExportWidth = TextureWidth;
    int32 ExportHeight = TextureHeight;
    
    if (MaxWidth > 0 && MaxHeight > 0)
    {
        float ScaleX = (float)MaxWidth / (float)TextureWidth;
        float ScaleY = (float)MaxHeight / (float)TextureHeight;
        float Scale = FMath::Min(ScaleX, ScaleY);
        
        if (Scale < 1.0f)
        {
            ExportWidth = FMath::RoundToInt(TextureWidth * Scale);
            ExportHeight = FMath::RoundToInt(TextureHeight * Scale);
        }
    }
    
    // Resize if needed
    TArray<FColor> FinalData;
    if (ExportWidth != TextureWidth || ExportHeight != TextureHeight)
    {
        ResizeImage(RawData, TextureWidth, TextureHeight, ExportWidth, ExportHeight, FinalData);
    }
    else
    {
        FinalData = MoveTemp(RawData);
    }
    
    // Generate temp file path
    FString ActualTempFolder = TempFolder;
    if (ActualTempFolder.IsEmpty())
    {
        ActualTempFolder = FPaths::ProjectSavedDir() / TEXT("Temp") / TEXT("TextureExports");
    }
    
    // Ensure temp directory exists
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*ActualTempFolder))
    {
        if (!PlatformFile.CreateDirectoryTree(*ActualTempFolder))
        {
            return TResult<FTextureExportResult>::Error(
                VibeUE::ErrorCodes::OPERATION_FAILED,
                FString::Printf(TEXT("Failed to create temp directory: %s"), *ActualTempFolder)
            );
        }
    }
    
    // Generate unique filename
    FString AssetName = FPaths::GetBaseFilename(AssetPath);
    FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    FString UniqueId = FGuid::NewGuid().ToString(EGuidFormats::Short);
    FString FileName = FString::Printf(TEXT("%s_%s_%s.%s"), *AssetName, *Timestamp, *UniqueId, *ExportFormat.ToLower());
    FString TempFilePath = ActualTempFolder / FileName;
    
    // Save file
    TResult<void> SaveResult = SaveImageToFile(FinalData, ExportWidth, ExportHeight, TempFilePath, ExportFormat);
    if (SaveResult.IsError())
    {
        return TResult<FTextureExportResult>::Error(
            SaveResult.GetErrorCode(),
            SaveResult.GetErrorMessage()
        );
    }
    
    // Build result
    FTextureExportResult Result;
    Result.AssetPath = AssetPath;
    Result.TempFilePath = TempFilePath;
    Result.ExportFormat = ExportFormat;
    Result.ExportedWidth = ExportWidth;
    Result.ExportedHeight = ExportHeight;
    Result.FileSize = PlatformFile.FileSize(*TempFilePath);
    
    return TResult<FTextureExportResult>::Success(Result);
}
