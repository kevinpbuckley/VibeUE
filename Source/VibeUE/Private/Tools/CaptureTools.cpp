// Copyright Buckley Builds LLC 2026 All Rights Reserved.
//
// capture_image: screenshots delivered as real MCP image content blocks (issues #544, #546).
//
// Epic's EditorToolset capture tools return base64 inside a text block, which clients can't render
// and which can blow their token limits; and none of them can see the PIE game viewport's Slate/UMG
// layer at all. This tool captures synchronously via Slate (game viewport / editor window) or the
// active editor viewport, and returns the PNG through the bridge's "vibeue_image" channel so the
// MCP layer emits a genuine image content block.

#include "Core/ToolRegistry.h"

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/SWindow.h"
#include "Widgets/SViewport.h"
#include "UnrealClient.h"
#include "ImageUtils.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	FString CaptureErrorJson(const FString& Code, const FString& Message)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetBoolField(TEXT("success"), false);
		Obj->SetStringField(TEXT("error_code"), Code);
		Obj->SetStringField(TEXT("error_message"), Message);
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
		return Out;
	}

	bool CaptureGameViewport(TArray<FColor>& OutPixels, FIntPoint& OutSize, FString& OutError)
	{
		if (!GEngine || !GEngine->GameViewport)
		{
			OutError = TEXT("No game viewport — start PIE first (EditorAppToolset.StartPIE or PerformanceService.start_pie), or use source='editor'/'window'.");
			return false;
		}
		TSharedPtr<SViewport> ViewportWidget = GEngine->GameViewport->GetGameViewportWidget();
		if (!ViewportWidget.IsValid() || !FSlateApplication::IsInitialized())
		{
			OutError = TEXT("Game viewport widget unavailable.");
			return false;
		}
		FIntVector Size;
		if (!FSlateApplication::Get().TakeScreenshot(ViewportWidget.ToSharedRef(), OutPixels, Size))
		{
			OutError = TEXT("Slate screenshot of the game viewport failed.");
			return false;
		}
		OutSize = FIntPoint(Size.X, Size.Y);
		return true;
	}

	bool CaptureActiveWindow(TArray<FColor>& OutPixels, FIntPoint& OutSize, FString& OutError)
	{
		if (!FSlateApplication::IsInitialized())
		{
			OutError = TEXT("Slate not initialized.");
			return false;
		}
		TSharedPtr<SWindow> Window = FSlateApplication::Get().GetActiveTopLevelWindow();
		if (!Window.IsValid())
		{
			// Unfocused editors have no "active" window — fall back to the main editor window.
			Window = FGlobalTabmanager::Get()->GetRootWindow();
		}
		if (!Window.IsValid())
		{
			OutError = TEXT("No editor window available to capture.");
			return false;
		}
		FIntVector Size;
		if (!FSlateApplication::Get().TakeScreenshot(StaticCastSharedRef<SWidget>(Window.ToSharedRef()), OutPixels, Size))
		{
			OutError = TEXT("Slate screenshot of the editor window failed.");
			return false;
		}
		OutSize = FIntPoint(Size.X, Size.Y);
		return true;
	}

	bool CaptureEditorViewport(TArray<FColor>& OutPixels, FIntPoint& OutSize, FString& OutError)
	{
		FViewport* Viewport = GEditor ? GEditor->GetActiveViewport() : nullptr;
		if (!Viewport)
		{
			OutError = TEXT("No active editor viewport.");
			return false;
		}
		Viewport->Draw(false);
		if (!Viewport->ReadPixels(OutPixels))
		{
			OutError = TEXT("ReadPixels on the active editor viewport failed.");
			return false;
		}
		OutSize = FIntPoint(Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y);
		return true;
	}
}

REGISTER_VIBEUE_TOOL(capture_image,
	"Capture a screenshot delivered as a REAL image (the client renders it — no base64 text blob, no token blowout). source='game' captures the PIE game viewport INCLUDING the Slate/UMG HUD (the thing CaptureViewport cannot see); 'window' captures the whole active editor window (any editor UI); 'editor' captures the active editor viewport scene (no UI). Also writes a PNG under Saved/VibeUE/Captures and returns its path.",
	"Capture",
	TOOL_PARAMS(
		TOOL_PARAM("source", "'game' (PIE viewport incl. UMG HUD — requires a running PIE session), 'window' (active editor window incl. all editor UI), or 'editor' (active editor viewport scene, no UI). Default: 'game' when PIE is running, else 'editor'.", "string", false),
		TOOL_PARAM("max_width", "Downscale to at most this width in pixels, preserving aspect ratio (default 1600; 0 = keep full size).", "number", false)
	),
	{
		FString Source = Params.FindRef(TEXT("source"));
		if (Source.IsEmpty())
		{
			Source = (GEngine && GEngine->GameViewport) ? TEXT("game") : TEXT("editor");
		}
		int32 MaxWidth = 1600;
		if (const FString* MaxWidthStr = Params.Find(TEXT("max_width")))
		{
			LexFromString(MaxWidth, **MaxWidthStr);
		}

		TArray<FColor> Pixels;
		FIntPoint Size(0, 0);
		FString Error;
		bool bOk = false;
		if (Source.Equals(TEXT("game"), ESearchCase::IgnoreCase))
		{
			bOk = CaptureGameViewport(Pixels, Size, Error);
		}
		else if (Source.Equals(TEXT("window"), ESearchCase::IgnoreCase))
		{
			bOk = CaptureActiveWindow(Pixels, Size, Error);
		}
		else if (Source.Equals(TEXT("editor"), ESearchCase::IgnoreCase))
		{
			bOk = CaptureEditorViewport(Pixels, Size, Error);
		}
		else
		{
			return CaptureErrorJson(TEXT("BAD_SOURCE"), FString::Printf(TEXT("Unknown source '%s' — use 'game', 'window', or 'editor'."), *Source));
		}

		if (!bOk || Pixels.Num() == 0 || Size.X <= 0 || Size.Y <= 0)
		{
			return CaptureErrorJson(TEXT("CAPTURE_FAILED"), Error.IsEmpty() ? TEXT("Capture produced no pixels.") : Error);
		}

		// Slate hands back whatever alpha the compositor had — force opaque or the PNG can render
		// as a checkerboard in viewers.
		for (FColor& Pixel : Pixels)
		{
			Pixel.A = 255;
		}

		if (MaxWidth > 0 && Size.X > MaxWidth)
		{
			const int32 DstWidth = MaxWidth;
			const int32 DstHeight = FMath::Max(1, (int32)((int64)Size.Y * MaxWidth / Size.X));
			TArray<FColor> Resized;
			FImageUtils::ImageResize(Size.X, Size.Y, Pixels, DstWidth, DstHeight, Resized, /*bLinearSpace=*/false);
			Pixels = MoveTemp(Resized);
			Size = FIntPoint(DstWidth, DstHeight);
		}

		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		if (Png.Num() == 0)
		{
			return CaptureErrorJson(TEXT("ENCODE_FAILED"), TEXT("PNG encoding produced no data."));
		}

		const FString CapturesDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VibeUE"), TEXT("Captures"));
		const FString FilePath = FPaths::Combine(CapturesDir,
			FString::Printf(TEXT("capture-%s-%s.png"), *Source.ToLower(), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))));
		const bool bSaved = FFileHelper::SaveArrayToFile(TArrayView<const uint8>(Png.GetData(), (int32)Png.Num()), *FilePath);

		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("source"), Source.ToLower());
		Result->SetNumberField(TEXT("width"), Size.X);
		Result->SetNumberField(TEXT("height"), Size.Y);
		Result->SetStringField(TEXT("file_path"), bSaved ? FilePath : FString());

		// The bridge converts this reserved field into an MCP image content block (issue #544).
		TSharedPtr<FJsonObject> Image = MakeShared<FJsonObject>();
		Image->SetStringField(TEXT("mime_type"), TEXT("image/png"));
		Image->SetStringField(TEXT("base64"), FBase64::Encode(Png.GetData(), (uint32)Png.Num()));
		Result->SetObjectField(TEXT("vibeue_image"), Image);

		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
		return Out;
	}
);
