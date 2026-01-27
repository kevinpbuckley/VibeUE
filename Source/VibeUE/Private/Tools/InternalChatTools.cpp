// Copyright Buckley Builds LLC 2026 All Rights Reserved.

/**
 * Internal Chat Tools - Tools only available to VibeUE internal chat.
 * These are NOT exposed via MCP to external clients (e.g., VS Code Copilot).
 * 
 * Use REGISTER_VIBEUE_INTERNAL_TOOL macro to register tools in this file.
 */

#include "Core/ToolRegistry.h"
#include "Chat/ChatSession.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Base64.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Modules/ModuleManager.h"
#include "Engine/Texture2D.h"

DECLARE_LOG_CATEGORY_EXTERN(LogInternalChatTools, Log, All);
DEFINE_LOG_CATEGORY(LogInternalChatTools);

// Maximum image dimensions for AI vision APIs (prevents payload too large errors)
// Claude and other vision models work well with images up to ~1.5MB
static constexpr int32 MAX_AI_IMAGE_WIDTH = 1920;
static constexpr int32 MAX_AI_IMAGE_HEIGHT = 1080;
static constexpr int32 MAX_AI_IMAGE_BYTES = 1500000; // 1.5MB max for base64 data URL
static constexpr int32 JPEG_QUALITY = 85; // Good balance of quality and size

namespace InternalChatToolsHelpers
{
    /**
     * Resize and compress an image for AI vision APIs.
     * Scales down large images and converts to JPEG for smaller payload.
     * @param InImageData Raw image file data
     * @param OutCompressedData Output compressed JPEG data
     * @param OutWidth Output image width after resize
     * @param OutHeight Output image height after resize
     * @param OutError Error message if failed
     * @return true if successful
     */
    bool ResizeAndCompressForAI(const TArray<uint8>& InImageData, TArray<uint8>& OutCompressedData, 
                                 int32& OutWidth, int32& OutHeight, FString& OutError)
    {
        IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
        
        // Detect format from raw data
        EImageFormat DetectedFormat = ImageWrapperModule.DetectImageFormat(InImageData.GetData(), InImageData.Num());
        if (DetectedFormat == EImageFormat::Invalid)
        {
            OutError = TEXT("Could not detect image format");
            return false;
        }
        
        // Create wrapper to decode the image
        TSharedPtr<IImageWrapper> SourceWrapper = ImageWrapperModule.CreateImageWrapper(DetectedFormat);
        if (!SourceWrapper.IsValid())
        {
            OutError = TEXT("Failed to create image wrapper for decoding");
            return false;
        }
        
        if (!SourceWrapper->SetCompressed(InImageData.GetData(), InImageData.Num()))
        {
            OutError = TEXT("Failed to decode image data");
            return false;
        }
        
        int32 OrigWidth = SourceWrapper->GetWidth();
        int32 OrigHeight = SourceWrapper->GetHeight();
        
        // Calculate target dimensions (maintain aspect ratio)
        float ScaleX = (OrigWidth > MAX_AI_IMAGE_WIDTH) ? (float)MAX_AI_IMAGE_WIDTH / OrigWidth : 1.0f;
        float ScaleY = (OrigHeight > MAX_AI_IMAGE_HEIGHT) ? (float)MAX_AI_IMAGE_HEIGHT / OrigHeight : 1.0f;
        float Scale = FMath::Min(ScaleX, ScaleY);
        
        OutWidth = FMath::RoundToInt(OrigWidth * Scale);
        OutHeight = FMath::RoundToInt(OrigHeight * Scale);
        
        // Get raw RGBA data
        TArray<uint8> RawData;
        if (!SourceWrapper->GetRaw(ERGBFormat::RGBA, 8, RawData))
        {
            OutError = TEXT("Failed to get raw image data");
            return false;
        }
        
        TArray<uint8> ResizedData;
        
        if (Scale < 1.0f)
        {
            // Need to resize - use simple bilinear sampling
            ResizedData.SetNum(OutWidth * OutHeight * 4);
            
            for (int32 Y = 0; Y < OutHeight; Y++)
            {
                for (int32 X = 0; X < OutWidth; X++)
                {
                    // Map to source coordinates
                    float SrcX = X / Scale;
                    float SrcY = Y / Scale;
                    
                    int32 SrcX0 = FMath::Clamp(FMath::FloorToInt(SrcX), 0, OrigWidth - 1);
                    int32 SrcY0 = FMath::Clamp(FMath::FloorToInt(SrcY), 0, OrigHeight - 1);
                    
                    int32 SrcIdx = (SrcY0 * OrigWidth + SrcX0) * 4;
                    int32 DstIdx = (Y * OutWidth + X) * 4;
                    
                    if (SrcIdx + 3 < RawData.Num() && DstIdx + 3 < ResizedData.Num())
                    {
                        ResizedData[DstIdx + 0] = RawData[SrcIdx + 0];
                        ResizedData[DstIdx + 1] = RawData[SrcIdx + 1];
                        ResizedData[DstIdx + 2] = RawData[SrcIdx + 2];
                        ResizedData[DstIdx + 3] = RawData[SrcIdx + 3];
                    }
                }
            }
            
            UE_LOG(LogInternalChatTools, Log, TEXT("Resized image from %dx%d to %dx%d (scale %.2f)"), 
                OrigWidth, OrigHeight, OutWidth, OutHeight, Scale);
        }
        else
        {
            // No resize needed, use original
            ResizedData = MoveTemp(RawData);
            OutWidth = OrigWidth;
            OutHeight = OrigHeight;
        }
        
        // Compress as JPEG for smaller size
        TSharedPtr<IImageWrapper> JpegWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
        if (!JpegWrapper.IsValid())
        {
            OutError = TEXT("Failed to create JPEG wrapper");
            return false;
        }
        
        if (!JpegWrapper->SetRaw(ResizedData.GetData(), ResizedData.Num(), OutWidth, OutHeight, ERGBFormat::RGBA, 8))
        {
            OutError = TEXT("Failed to set raw data for JPEG compression");
            return false;
        }
        
        TArray64<uint8> CompressedData = JpegWrapper->GetCompressed(JPEG_QUALITY);
        if (CompressedData.Num() == 0)
        {
            OutError = TEXT("Failed to compress to JPEG");
            return false;
        }
        
        OutCompressedData.SetNum(CompressedData.Num());
        FMemory::Memcpy(OutCompressedData.GetData(), CompressedData.GetData(), CompressedData.Num());
        
        UE_LOG(LogInternalChatTools, Log, TEXT("Compressed image: %d bytes (JPEG quality %d)"), 
            OutCompressedData.Num(), JPEG_QUALITY);
        
        return true;
    }

    /**
     * Load an image from disk and convert to base64 data URL.
     * Automatically resizes and compresses large images for AI vision APIs.
     * @param FilePath Path to the image file
     * @param OutDataUrl Output base64 data URL
     * @param OutError Output error message if failed
     * @return true if successful
     */
    bool LoadImageAsDataUrl(const FString& FilePath, FString& OutDataUrl, FString& OutError)
    {
        // Check file exists
        if (!FPaths::FileExists(FilePath))
        {
            OutError = FString::Printf(TEXT("File not found: %s"), *FilePath);
            return false;
        }

        // Load file data
        TArray<uint8> ImageData;
        if (!FFileHelper::LoadFileToArray(ImageData, *FilePath))
        {
            OutError = FString::Printf(TEXT("Failed to read file: %s"), *FilePath);
            return false;
        }

        if (ImageData.Num() == 0)
        {
            OutError = FString::Printf(TEXT("File is empty: %s"), *FilePath);
            return false;
        }

        // Check if image needs resizing/compression for AI
        // Estimate base64 size: raw * 1.37 for base64 overhead
        int32 EstimatedBase64Size = static_cast<int32>(ImageData.Num() * 1.37);
        
        if (EstimatedBase64Size > MAX_AI_IMAGE_BYTES)
        {
            UE_LOG(LogInternalChatTools, Log, TEXT("Image too large for AI (%d bytes, estimated %d base64). Resizing and compressing..."), 
                ImageData.Num(), EstimatedBase64Size);
            
            TArray<uint8> CompressedData;
            int32 NewWidth, NewHeight;
            
            if (ResizeAndCompressForAI(ImageData, CompressedData, NewWidth, NewHeight, OutError))
            {
                // Use compressed JPEG data
                FString Base64Data = FBase64::Encode(CompressedData);
                OutDataUrl = FString::Printf(TEXT("data:image/jpeg;base64,%s"), *Base64Data);
                
                UE_LOG(LogInternalChatTools, Log, TEXT("Optimized image %s: %dx%d, %d bytes -> data URL (%d chars)"), 
                    *FPaths::GetCleanFilename(FilePath), NewWidth, NewHeight, CompressedData.Num(), OutDataUrl.Len());
                
                return true;
            }
            else
            {
                UE_LOG(LogInternalChatTools, Warning, TEXT("Failed to resize/compress: %s. Trying original..."), *OutError);
                // Fall through to try original
            }
        }

        // Use original image (small enough or compression failed)
        FString Extension = FPaths::GetExtension(FilePath).ToLower();
        FString MimeType;
        
        if (Extension == TEXT("png"))
        {
            MimeType = TEXT("image/png");
        }
        else if (Extension == TEXT("jpg") || Extension == TEXT("jpeg"))
        {
            MimeType = TEXT("image/jpeg");
        }
        else if (Extension == TEXT("bmp"))
        {
            MimeType = TEXT("image/bmp");
        }
        else if (Extension == TEXT("gif"))
        {
            MimeType = TEXT("image/gif");
        }
        else if (Extension == TEXT("webp"))
        {
            MimeType = TEXT("image/webp");
        }
        else
        {
            OutError = FString::Printf(TEXT("Unsupported image format: %s"), *Extension);
            return false;
        }

        // Encode to base64
        FString Base64Data = FBase64::Encode(ImageData);
        
        // Build data URL
        OutDataUrl = FString::Printf(TEXT("data:%s;base64,%s"), *MimeType, *Base64Data);
        
        UE_LOG(LogInternalChatTools, Log, TEXT("Loaded image %s (%d bytes) -> data URL (%d chars)"), 
            *FilePath, ImageData.Num(), OutDataUrl.Len());
        
        return true;
    }

    /**
     * Build a JSON success response
     */
    FString BuildSuccessResponse(const FString& Message)
    {
        return FString::Printf(TEXT("{\"success\": true, \"message\": \"%s\"}"), *Message);
    }

    /**
     * Build a JSON error response
     */
    FString BuildErrorResponse(const FString& Error)
    {
        return FString::Printf(TEXT("{\"success\": false, \"error\": \"%s\"}"), *Error);
    }
}

// ============================================================================
// attach_image - Attach an image to be analyzed by the AI
// ============================================================================

// Manual registration for internal-only tools (can't use macro due to comma in struct init)
static FToolAutoRegistrar AutoRegister_attach_image(
    []() {
        FToolRegistration Reg;
        Reg.Name = TEXT("attach_image");
        Reg.Description = TEXT("Attach an image file to be included in the next AI request for visual analysis. Use this after taking a screenshot to have the AI analyze it. Supported formats: PNG JPG JPEG BMP GIF WEBP.");
        Reg.Category = TEXT("Chat");
        Reg.Parameters = TArray<FToolParameter>({
            FToolParameter(TEXT("file_path"), TEXT("Absolute path to the image file to attach"), TEXT("string"), true)
        });
        Reg.ExecuteFunc = [](const TMap<FString, FString>& Params) -> FString {
            FString FilePath = Params.FindRef(TEXT("file_path"));
            
            // Also check 'path' as an alias
            if (FilePath.IsEmpty())
            {
                FilePath = Params.FindRef(TEXT("path"));
            }
            
            if (FilePath.IsEmpty())
            {
                return InternalChatToolsHelpers::BuildErrorResponse(TEXT("file_path parameter is required"));
            }

            // Normalize path separators
            FilePath = FilePath.Replace(TEXT("/"), TEXT("\\"));
            FilePath = FilePath.Replace(TEXT("\\\\"), TEXT("\\"));

            FString DataUrl, Error;
            if (!InternalChatToolsHelpers::LoadImageAsDataUrl(FilePath, DataUrl, Error))
            {
                UE_LOG(LogInternalChatTools, Warning, TEXT("attach_image failed: %s"), *Error);
                return InternalChatToolsHelpers::BuildErrorResponse(Error);
            }

            // Queue the image for the next LLM request
            FChatSession::SetPendingImageForNextRequest(DataUrl);

            FString SuccessMsg = FString::Printf(
                TEXT("Image attached successfully. The image from '%s' will be included in the next AI request for analysis."),
                *FPaths::GetCleanFilename(FilePath));
            
            UE_LOG(LogInternalChatTools, Log, TEXT("attach_image: %s"), *SuccessMsg);
            
            return InternalChatToolsHelpers::BuildSuccessResponse(SuccessMsg);
        };
        Reg.bInternalOnly = true;
        return Reg;
    }()
);
