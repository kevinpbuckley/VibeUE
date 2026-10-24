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
#include "Layout/WidgetPath.h"
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

	// A minimized window has no valid Slate paint state: TakeScreenshot against it usually
	// fails gracefully but can null-deref inside Slate (two EXCEPTION_ACCESS_VIOLATION
	// editor crashes with all-Slate callstacks while an automation client drove a
	// minimized editor, 2026-09-01). Refuse with an actionable error instead — callers
	// can restore the window (ShowWindow/AppActivate) and retry.
	//
	// The same hazard covers every window Slate will not draw. TakeScreenshot hands the renderer a pointer to the
	// caller's pixel array and draws the window; the request is cleared only when that window is really drawn.
	// DrawWindowAndChildren skips hidden and minimized windows, and while a modal window is open PrivateDrawWindows
	// draws only that one. In those cases the request stays armed with a pointer into a freed array and is written at
	// the next real draw.
	bool EnsureWindowCapturable(const TSharedPtr<SWindow>& Window, FString& OutError)
	{
		if (!Window.IsValid())
		{
			OutError = TEXT("No window found for the capture target.");
			return false;
		}
		if (Window->IsWindowMinimized())
		{
			OutError = TEXT("The capture target's window is minimized — Slate cannot capture a minimized window (and may crash trying). If it is the main editor window, restore it with execute_python_code: unreal.ViewportService.set_editor_window_state(\"restore\"). If it is a separate PIE window (a 'Preview [NetMode: Client N]' window), that helper does not reach it: un-minimize that window from the OS (taskbar, or ShowWindow/AppActivate from the client). Then retry.");
			return false;
		}
		if (!Window->IsVisible())
		{
			OutError = TEXT("The target window is hidden — Slate would not draw it and would leave the screenshot request armed. Show the window and retry.");
			return false;
		}
		const FVector2D WindowSize = Window->GetSizeInScreen();
		if (WindowSize.X < 1.0f || WindowSize.Y < 1.0f)
		{
			OutError = TEXT("The editor window has zero size — nothing to capture.");
			return false;
		}
		const FVector2D ViewportSize = Window->GetViewportSize();
		if (ViewportSize.X < 1.0f || ViewportSize.Y < 1.0f)
		{
			OutError = TEXT("The target window's viewport has zero size — nothing would be drawn to capture.");
			return false;
		}
		const TSharedPtr<SWindow> ModalWindow = FSlateApplication::Get().GetActiveModalWindow();
		if (ModalWindow.IsValid() && ModalWindow != Window)
		{
			OutError = FString::Printf(TEXT("A modal window ('%s') is open — Slate draws only that window until it closes. Close it and retry."),
				*ModalWindow->GetTitle().ToString());
			return false;
		}
		return true;
	}

	// TakeScreenshot of a widget calls GeneratePathToWidgetChecked, which asserts when the widget has no visible
	// path. FindPathToWidget only reports, so a widget that is not on screen is refused first.
	bool EnsureWidgetOnScreen(const TSharedRef<SWidget>& Widget, FString& OutError)
	{
		FWidgetPath Path;
		if (!FSlateApplication::Get().FindPathToWidget(Widget, Path))
		{
			OutError = TEXT("The capture target is not visible on screen (no widget path to it) — nothing to capture.");
			return false;
		}
		return true;
	}

	// One game viewport client through the shared refusal checks (EnsureWindowCapturable / EnsureWidgetOnScreen).
	bool CaptureGameViewportClient(UGameViewportClient* GameViewport, TArray<FColor>& OutPixels, FIntPoint& OutSize, FString& OutError)
	{
		if (!GameViewport)
		{
			OutError = TEXT("No game viewport.");
			return false;
		}
		TSharedPtr<SViewport> ViewportWidget = GameViewport->GetGameViewportWidget();
		if (!ViewportWidget.IsValid() || !FSlateApplication::IsInitialized())
		{
			OutError = TEXT("Game viewport widget unavailable.");
			return false;
		}
		if (!EnsureWindowCapturable(FSlateApplication::Get().FindWidgetWindow(ViewportWidget.ToSharedRef()), OutError)
			|| !EnsureWidgetOnScreen(ViewportWidget.ToSharedRef(), OutError))
		{
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

	bool CaptureGameViewport(TArray<FColor>& OutPixels, FIntPoint& OutSize, FString& OutError)
	{
		if (!GEngine || !GEngine->GameViewport)
		{
			OutError = TEXT("No game viewport — start PIE first (EditorAppToolset.StartPIE or PerformanceService.start_pie), or use source='editor'/'window'.");
			return false;
		}
		return CaptureGameViewportClient(GEngine->GameViewport, OutPixels, OutSize, OutError);
	}

	// The game viewport of one PIE instance (FWorldContext::PIEInstance), for capturing a chosen client in a
	// multi-player PIE session. GEngine->GameViewport is only one of them. Returns false with OutErrorCode
	// PIE_INSTANCE_NOT_FOUND (OutAvailable lists the PIE instances that exist, empty when PIE is not running) or
	// PIE_INSTANCE_NO_VIEWPORT (a dedicated server instance has no game viewport). Reads world contexts only.
	bool FindPieInstanceGameViewport(int32 PieInstance, UGameViewportClient*& OutViewport, TArray<int32>& OutAvailable,
		FString& OutErrorCode, FString& OutError)
	{
		OutViewport = nullptr;
		OutAvailable.Reset();
		bool bFound = false;
		if (GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType != EWorldType::PIE)
				{
					continue;
				}
				OutAvailable.Add(Context.PIEInstance);
				if (Context.PIEInstance == PieInstance)
				{
					bFound = true;
					OutViewport = Context.GameViewport;
				}
			}
		}
		OutAvailable.Sort();
		if (!bFound)
		{
			TArray<FString> Names;
			for (const int32 Instance : OutAvailable)
			{
				Names.Add(FString::FromInt(Instance));
			}
			OutErrorCode = TEXT("PIE_INSTANCE_NOT_FOUND");
			OutError = OutAvailable.Num() == 0
				? FString::Printf(TEXT("No PIE instance %d: PIE is not running. Start PIE first, or omit pie_instance."), PieInstance)
				: FString::Printf(TEXT("No PIE instance %d. Available PIE instances: %s."), PieInstance, *FString::Join(Names, TEXT(", ")));
			return false;
		}
		if (!OutViewport)
		{
			OutErrorCode = TEXT("PIE_INSTANCE_NO_VIEWPORT");
			OutError = FString::Printf(TEXT("PIE instance %d has no game viewport (a dedicated server has none) — pick a client's instance."), PieInstance);
			return false;
		}
		return true;
	}

	// pie_instance as sent by the bridge (JSON numbers arrive as "%.10g" text): an optional '-' and digits only, so
	// "-", "+", ".", "1.5" and "0x1" are refused rather than read as 0 or truncated. Values below -1 are refused too.
	bool ParsePieInstanceParam(const FString& Text, int32& OutInstance)
	{
		const int32 Start = Text.StartsWith(TEXT("-")) ? 1 : 0;
		const int32 Digits = Text.Len() - Start;
		if (Digits < 1 || Digits > 9)
		{
			return false;
		}
		for (int32 Index = Start; Index < Text.Len(); ++Index)
		{
			if (!FChar::IsDigit(Text[Index]))
			{
				return false;
			}
		}
		const int32 Value = FCString::Atoi(*Text);
		if (Value < -1)
		{
			return false;
		}
		OutInstance = Value;
		return true;
	}

	// The PIE instance whose game viewport GEngine->GameViewport is, or -1 when none matches.
	int32 PieInstanceOfDefaultGameViewport()
	{
		if (!GEngine || !GEngine->GameViewport)
		{
			return -1;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.GameViewport == GEngine->GameViewport)
			{
				return Context.PIEInstance;
			}
		}
		return -1;
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
		if (!EnsureWindowCapturable(Window, OutError))
		{
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
	"Capture a screenshot delivered as a REAL image (the client renders it — no base64 text blob, no token blowout). source='game' captures the PIE game viewport INCLUDING the Slate/UMG HUD (the thing CaptureViewport cannot see); add pie_instance to pick one PIE instance in a multi-player session. 'window' captures the whole active editor window (any editor UI); 'editor' captures the active editor viewport scene (no UI). Also writes a PNG under Saved/VibeUE/Captures and returns its path.",
	"Capture",
	TOOL_PARAMS(
		TOOL_PARAM("source", "'game' (PIE viewport incl. UMG HUD — requires a running PIE session), 'window' (active editor window incl. all editor UI), or 'editor' (active editor viewport scene, no UI). Default: 'game' when PIE is running or pie_instance is given, else 'editor'.", "string", false),
		TOOL_PARAM("max_width", "Downscale to at most this width in pixels, preserving aspect ratio (default 1600; 0 = keep full size).", "number", false),
		TOOL_PARAM("pie_instance", "source='game' only: capture the game viewport of this PIE instance (FWorldContext PIEInstance: 0 = the first window, e.g. the listen server; 1, 2 = clients). Default -1 = the editor's current game viewport (one instance only). An unknown instance returns PIE_INSTANCE_NOT_FOUND with the available instances.", "integer", false)
	),
	{
		int32 PieInstance = -1;
		if (const FString* PieInstanceStr = Params.Find(TEXT("pie_instance")))
		{
			const FString Trimmed = PieInstanceStr->TrimStartAndEnd();
			if (!Trimmed.IsEmpty())
			{
				if (!ParsePieInstanceParam(Trimmed, PieInstance))
				{
					return CaptureErrorJson(TEXT("BAD_PIE_INSTANCE"), FString::Printf(TEXT("pie_instance must be an integer >= -1 (-1 = default), got '%s'."), *Trimmed));
				}
			}
		}

		FString Source = Params.FindRef(TEXT("source"));
		if (Source.IsEmpty())
		{
			Source = (PieInstance >= 0 || (GEngine && GEngine->GameViewport)) ? TEXT("game") : TEXT("editor");
		}
		const bool bGameSource = Source.Equals(TEXT("game"), ESearchCase::IgnoreCase);
		if (PieInstance >= 0 && !bGameSource)
		{
			return CaptureErrorJson(TEXT("BAD_SOURCE"), FString::Printf(TEXT("pie_instance applies only to source='game' (got source '%s')."), *Source));
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
		int32 UsedPieInstance = -1;
		if (bGameSource && PieInstance >= 0)
		{
			UGameViewportClient* InstanceViewport = nullptr;
			TArray<int32> Available;
			FString ErrorCode;
			if (!FindPieInstanceGameViewport(PieInstance, InstanceViewport, Available, ErrorCode, Error))
			{
				TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
				Err->SetBoolField(TEXT("success"), false);
				Err->SetStringField(TEXT("error_code"), ErrorCode);
				Err->SetStringField(TEXT("error_message"), Error);
				Err->SetNumberField(TEXT("pie_instance"), PieInstance);
				TArray<TSharedPtr<FJsonValue>> AvailableJson;
				for (const int32 Instance : Available)
				{
					AvailableJson.Add(MakeShared<FJsonValueNumber>(Instance));
				}
				Err->SetArrayField(TEXT("available_pie_instances"), AvailableJson);
				FString ErrOut;
				const TSharedRef<TJsonWriter<>> ErrWriter = TJsonWriterFactory<>::Create(&ErrOut);
				FJsonSerializer::Serialize(Err.ToSharedRef(), ErrWriter);
				return ErrOut;
			}
			bOk = CaptureGameViewportClient(InstanceViewport, Pixels, Size, Error);
			UsedPieInstance = PieInstance;
		}
		else if (bGameSource)
		{
			bOk = CaptureGameViewport(Pixels, Size, Error);
			UsedPieInstance = PieInstanceOfDefaultGameViewport();
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
			FString::Printf(TEXT("capture-%s-%s.png"), *Source.ToLower(), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S-%s"))));
		const bool bSaved = FFileHelper::SaveArrayToFile(TArrayView<const uint8>(Png.GetData(), (int32)Png.Num()), *FilePath);

		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("source"), Source.ToLower());
		if (bGameSource)
		{
			// The PIE instance actually captured (-1 if the default game viewport matched no PIE world context).
			Result->SetNumberField(TEXT("pie_instance"), UsedPieInstance);
		}
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

#if WITH_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Widgets/Layout/SBox.h"

// TakeScreenshot arms a request that only a real draw of the target window clears. A window Slate will not draw must
// be refused before anything is armed. Test path prefix VibeUE.Capture.*

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeUECaptureRefusesUndrawnWindowsTest, "VibeUE.Capture.RefusesUndrawnWindows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeUECaptureRefusesUndrawnWindowsTest::RunTest(const FString& Parameters)
{
	// A window that was never shown has a size but no native window, so Slate never draws it.
	const TSharedRef<SWindow> NeverShown = SNew(SWindow)
		.Title(FText::FromString(TEXT("VibeUE capture test")))
		.ClientSize(FVector2D(320.0, 240.0));
	TestTrue(TEXT("precondition: the window has a size"), NeverShown->GetSizeInScreen().X >= 1.0f && NeverShown->GetSizeInScreen().Y >= 1.0f);
	TestFalse(TEXT("precondition: the window is not minimized"), NeverShown->IsWindowMinimized());
	TestFalse(TEXT("precondition: Slate does not see it as visible"), NeverShown->IsVisible());

	FString Error;
	TestFalse(TEXT("a window Slate will not draw is refused"), EnsureWindowCapturable(NeverShown, Error));
	TestTrue(FString::Printf(TEXT("the refusal says why: '%s'"), *Error), Error.Contains(TEXT("hidden")));

	// A widget in no window has no path on screen; TakeScreenshot would assert on it.
	const TSharedRef<SWidget> Orphan = SNew(SBox);
	Error.Reset();
	TestFalse(TEXT("a widget with no path on screen is refused"), EnsureWidgetOnScreen(Orphan, Error));
	TestTrue(FString::Printf(TEXT("the refusal says why: '%s'"), *Error), Error.Contains(TEXT("not visible on screen")));
	return true;
}

// capture_image pie_instance picks one PIE world's game viewport. With no PIE running, asking for instance 0 must
// come back as PIE_INSTANCE_NOT_FOUND with an empty instance list, before any window or widget is touched. Also checks
// that the parameter is in the tool's schema and that bad combinations are refused up front.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeUECapturePieInstanceNotFoundTest, "VibeUE.Capture.PieInstanceNotFound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeUECapturePieInstanceNotFoundTest::RunTest(const FString& Parameters)
{
	const FToolMetadata* Tool = FToolRegistry::Get().FindTool(TEXT("capture_image"));
	if (!TestNotNull(TEXT("capture_image is registered"), Tool))
	{
		return false;
	}
	const FToolParameter* PieParam = Tool->Parameters.FindByPredicate([](const FToolParameter& P) { return P.Name == TEXT("pie_instance"); });
	if (TestNotNull(TEXT("capture_image declares pie_instance"), PieParam))
	{
		TestEqual(TEXT("pie_instance is an integer"), PieParam->Type, FString(TEXT("integer")));
		TestFalse(TEXT("pie_instance is optional"), PieParam->bRequired);
	}

	bool bAnyPie = false;
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			bAnyPie |= Context.WorldType == EWorldType::PIE;
		}
	}
	if (bAnyPie)
	{
		AddWarning(TEXT("SKIPPED: a PIE session is running, so the no-PIE PIE_INSTANCE_NOT_FOUND path cannot be exercised."));
		return true;
	}

	// The lookup on its own.
	UGameViewportClient* Found = nullptr;
	TArray<int32> Available;
	FString Code;
	FString Error;
	TestFalse(TEXT("no PIE instance 0 without PIE"), FindPieInstanceGameViewport(0, Found, Available, Code, Error));
	TestNull(TEXT("no viewport is returned"), Found);
	TestEqual(TEXT("error code"), Code, FString(TEXT("PIE_INSTANCE_NOT_FOUND")));
	TestEqual(TEXT("no instances are available"), Available.Num(), 0);

	auto RunTool = [](const TMap<FString, FString>& Args) -> TSharedPtr<FJsonObject>
	{
		const FString Reply = FToolRegistry::Get().ExecuteTool(TEXT("capture_image"), Args);
		TSharedPtr<FJsonObject> Obj;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Reply), Obj);
		return Obj;
	};
	// TryGet, so a reply without the field fails the comparison instead of logging a LogJson warning.
	auto ErrorCodeOf = [](const TSharedPtr<FJsonObject>& Obj)
	{
		FString ErrorCode;
		if (Obj.IsValid())
		{
			Obj->TryGetStringField(TEXT("error_code"), ErrorCode);
		}
		return ErrorCode;
	};

	// Through the tool, as an MCP client would call it.
	const TSharedPtr<FJsonObject> Reply = RunTool({ { TEXT("source"), TEXT("game") }, { TEXT("pie_instance"), TEXT("0") } });
	if (!TestTrue(TEXT("the reply is JSON"), Reply.IsValid()))
	{
		return false;
	}
	if (ErrorCodeOf(Reply) == TEXT("TOOL_DISABLED"))
	{
		AddWarning(TEXT("SKIPPED tool-call arm: capture_image is disabled in this project."));
		return true;
	}
	TestFalse(TEXT("pie_instance=0 without PIE fails"), Reply->HasTypedField<EJson::Boolean>(TEXT("success")) && Reply->GetBoolField(TEXT("success")));
	TestEqual(TEXT("tool error code"), ErrorCodeOf(Reply), FString(TEXT("PIE_INSTANCE_NOT_FOUND")));
	const TArray<TSharedPtr<FJsonValue>>* AvailableJson = nullptr;
	if (TestTrue(TEXT("the reply lists available_pie_instances"), Reply->TryGetArrayField(TEXT("available_pie_instances"), AvailableJson)))
	{
		TestEqual(TEXT("the list is empty"), AvailableJson->Num(), 0);
	}
	TestFalse(TEXT("no image is returned"), Reply->HasField(TEXT("vibeue_image")));

	// Source omitted: pie_instance implies 'game'.
	const TSharedPtr<FJsonObject> Implied = RunTool({ { TEXT("pie_instance"), TEXT("0") } });
	if (TestTrue(TEXT("implied-source reply is JSON"), Implied.IsValid()))
	{
		TestEqual(TEXT("pie_instance alone means source='game'"), ErrorCodeOf(Implied), FString(TEXT("PIE_INSTANCE_NOT_FOUND")));
	}

	// pie_instance with a non-game source, and a non-integer pie_instance, are refused before any capture.
	const TSharedPtr<FJsonObject> WrongSource = RunTool({ { TEXT("source"), TEXT("editor") }, { TEXT("pie_instance"), TEXT("0") } });
	if (TestTrue(TEXT("wrong-source reply is JSON"), WrongSource.IsValid()))
	{
		TestEqual(TEXT("pie_instance with source='editor' is refused"), ErrorCodeOf(WrongSource), FString(TEXT("BAD_SOURCE")));
	}
	for (const TCHAR* BadValue : { TEXT("client"), TEXT("-"), TEXT("+1"), TEXT("."), TEXT("1.5"), TEXT("0x1"), TEXT("-2") })
	{
		const TSharedPtr<FJsonObject> Bad = RunTool({ { TEXT("source"), TEXT("game") }, { TEXT("pie_instance"), BadValue } });
		TestEqual(FString::Printf(TEXT("pie_instance '%s' is refused"), BadValue), ErrorCodeOf(Bad), FString(TEXT("BAD_PIE_INSTANCE")));
	}

	// The parser on its own: the accepted forms.
	int32 Parsed = 99;
	TestTrue(TEXT("'-1' parses"), ParsePieInstanceParam(TEXT("-1"), Parsed) && Parsed == -1);
	TestTrue(TEXT("'0' parses"), ParsePieInstanceParam(TEXT("0"), Parsed) && Parsed == 0);
	TestTrue(TEXT("'12' parses"), ParsePieInstanceParam(TEXT("12"), Parsed) && Parsed == 12);
	TestFalse(TEXT("'' is refused"), ParsePieInstanceParam(TEXT(""), Parsed));

	// pie_instance=-1 explicitly is the default path: with no PIE it is the old "no game viewport" refusal, not BAD_PIE_INSTANCE.
	const TSharedPtr<FJsonObject> Default = RunTool({ { TEXT("source"), TEXT("game") }, { TEXT("pie_instance"), TEXT("-1") } });
	TestEqual(TEXT("pie_instance -1 without PIE takes the default path"), ErrorCodeOf(Default), FString(TEXT("CAPTURE_FAILED")));
	return true;
}

#endif // WITH_AUTOMATION_TESTS
