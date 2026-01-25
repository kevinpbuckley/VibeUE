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

DECLARE_LOG_CATEGORY_EXTERN(LogInternalChatTools, Log, All);
DEFINE_LOG_CATEGORY(LogInternalChatTools);

namespace InternalChatToolsHelpers
{
    /**
     * Load an image from disk and convert to base64 data URL.
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

        // Determine MIME type from extension
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
