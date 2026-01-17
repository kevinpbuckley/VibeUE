// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Tools/VisionTools.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Editor/EditorEngine.h"
#include "UnrealClient.h"
#include "Misc/Base64.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformTime.h"
#include "Engine/GameViewportClient.h"
#include "ImageUtils.h"

extern UNREALED_API UEditorEngine* GEditor;

FString UVisionTools::CaptureViewport(const FString& Prompt, float ResolutionScale)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	
	// Validate resolution scale
	if (ResolutionScale <= 0.0f || ResolutionScale > 4.0f)
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), TEXT("resolution_scale must be between 0.1 and 4.0"));
		
		FString OutputString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
		FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
		return OutputString;
	}

	// Get the active viewport
	FViewport* Viewport = nullptr;
	if (GEditor && GEditor->GetActiveViewport())
	{
		Viewport = GEditor->GetActiveViewport();
	}

	if (!Viewport)
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), TEXT("No active viewport found"));
		
		FString OutputString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
		FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
		return OutputString;
	}

	// Calculate screenshot dimensions
	int32 Width = FMath::RoundToInt(Viewport->GetSizeXY().X * ResolutionScale);
	int32 Height = FMath::RoundToInt(Viewport->GetSizeXY().Y * ResolutionScale);

	// Capture the viewport to a texture
	TArray<FColor> Bitmap;
	FIntVector Size(Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, 0);
	if (!Viewport->ReadPixels(Bitmap))
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), TEXT("Failed to read pixels from viewport"));
		
		FString OutputString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
		FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
		return OutputString;
	}
	
	// Get actual dimensions from the bitmap
	Width = Viewport->GetSizeXY().X;
	Height = Viewport->GetSizeXY().Y;
	
	// Convert to PNG using TArray64
	TArray64<uint8> CompressedBitmap;
	FImageUtils::PNGCompressImageArray(Width, Height, TArrayView64<const FColor>(Bitmap.GetData(), Bitmap.Num()), CompressedBitmap);
	
	// Convert to regular TArray for FBase64::Encode
	TArray<uint8> CompressedData(CompressedBitmap);
	
	// Encode as base64
	FString Base64String = FBase64::Encode(CompressedData);
	FString DataUrl = FString::Printf(TEXT("data:image/png;base64,%s"), *Base64String);

	// Return success with the image data
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("image"), DataUrl);
	Result->SetNumberField(TEXT("width"), Width);
	Result->SetNumberField(TEXT("height"), Height);
	
	if (!Prompt.IsEmpty())
	{
		Result->SetStringField(TEXT("prompt"), Prompt);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
	return OutputString;
}
