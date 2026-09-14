// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UViewportService.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "SEditorViewport.h"
// LevelEditorViewport.h declares an override of the deprecated
// FEditorViewportClient::DropObjectsAtCoordinates, which trips C4996
// (promoted to C2220 by this module's bWarningsAsErrors) purely from
// including the header — VibeUE never calls the deprecated overload
// itself. See GitHub #491.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
#include "LevelEditorViewport.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#include "LevelViewportActions.h"
#include "EditorViewportClient.h"
// capture_scene (issue B4): synchronous SceneCapture2D → PNG that works while backgrounded
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "TextureResource.h"
#include "RenderingThread.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogViewportService, Log, All);

// =================================================================
// Internal helpers
// =================================================================

FLevelEditorViewportClient* UViewportService::GetActiveViewportClient()
{
	TSharedPtr<SLevelViewport> LevelViewport = GetActiveLevelViewport();
	if (LevelViewport.IsValid())
	{
		return &LevelViewport->GetLevelViewportClient();
	}

	if (!GEditor) { return nullptr; }

	const TArray<FLevelEditorViewportClient*>& Clients = GEditor->GetLevelViewportClients();
	if (Clients.Num() == 0) { return nullptr; }

	for (FLevelEditorViewportClient* Client : Clients)
	{
		if (Client)
		{
			return Client;
		}
	}

	return nullptr;
}

TSharedPtr<SLevelViewport> UViewportService::GetActiveLevelViewport()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	return LevelEditorModule.GetFirstActiveLevelViewport();
}

/**
 * Force the viewport to visually redraw.
 * In non-realtime mode, SEditorViewport has no active tick timer, so
 * Invalidate() on the FViewport only sets a dirty flag with nobody to process it.
 * We must call SEditorViewport::Invalidate() which registers a one-shot active timer
 * to wake the widget up for rendering.
 */
void UViewportService::ForceViewportRedraw()
{
	// Invalidate the FViewport data (marks display dirty)
	FLevelEditorViewportClient* Client = GetActiveViewportClient();
	if (Client)
	{
		Client->Invalidate();
	}

	// Wake the Slate viewport widget so it actually processes the redraw
	TSharedPtr<SLevelViewport> LevelViewport = GetActiveLevelViewport();
	if (LevelViewport.IsValid())
	{
		LevelViewport->Invalidate();
	}
}

FString UViewportService::ViewportTypeToString(int32 ViewportType)
{
	switch (ViewportType)
	{
	case LVT_Perspective:      return TEXT("perspective");
	case LVT_OrthoXY:          return TEXT("top");
	case LVT_OrthoNegativeXY:  return TEXT("bottom");
	case LVT_OrthoNegativeXZ:  return TEXT("left");
	case LVT_OrthoXZ:          return TEXT("right");
	case LVT_OrthoNegativeYZ:  return TEXT("front");
	case LVT_OrthoYZ:          return TEXT("back");
	case LVT_OrthoFreelook:    return TEXT("ortho_freelook");
	default:                   return TEXT("unknown");
	}
}

int32 UViewportService::StringToViewportType(const FString& TypeStr)
{
	FString Lower = TypeStr.ToLower().TrimStartAndEnd();
	if (Lower == TEXT("perspective"))    { return LVT_Perspective; }
	if (Lower == TEXT("top"))            { return LVT_OrthoXY; }
	if (Lower == TEXT("bottom"))         { return LVT_OrthoNegativeXY; }
	if (Lower == TEXT("left"))           { return LVT_OrthoNegativeXZ; }
	if (Lower == TEXT("right"))          { return LVT_OrthoXZ; }
	if (Lower == TEXT("front"))          { return LVT_OrthoNegativeYZ; }
	if (Lower == TEXT("back"))           { return LVT_OrthoYZ; }
	if (Lower == TEXT("ortho_freelook")) { return LVT_OrthoFreelook; }
	return -1;
}

// =================================================================
// Viewport Information
// =================================================================

FViewportInfo UViewportService::GetViewportInfo()
{
	FViewportInfo Info;

	FLevelEditorViewportClient* Client = GetActiveViewportClient();
	if (!Client)
	{
		UE_LOG(LogViewportService, Warning, TEXT("GetViewportInfo: No active viewport client"));
		return Info;
	}

	Info.ViewportType = ViewportTypeToString(static_cast<int32>(Client->GetViewportType()));
	Info.Location = Client->GetViewLocation();
	Info.Rotation = Client->GetViewRotation();
	Info.FOV = Client->ViewFOV;
	Info.NearClipPlane = Client->GetNearClipPlane();
	Info.FarClipPlane = Client->GetFarClipPlaneOverride();
	Info.bIsRealtime = Client->IsRealtime();
	Info.bIsGameView = Client->IsInGameView();
	Info.bAllowCinematicControl = Client->AllowsCinematicControl();
	Info.bExposureFixed = Client->ExposureSettings.bFixed;
	Info.ExposureEV100 = Client->ExposureSettings.FixedEV100;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Info.CameraSpeedSetting = Client->GetCameraSpeedSetting();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	Info.ViewportIndex = Client->ViewIndex;

	// Get layout from SLevelViewport
	TSharedPtr<SLevelViewport> Viewport = GetActiveLevelViewport();
	if (Viewport.IsValid())
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		// Check known layouts
		for (const FName& LayoutName : {
			LevelViewportConfigurationNames::OnePane,
			LevelViewportConfigurationNames::TwoPanesHoriz,
			LevelViewportConfigurationNames::TwoPanesVert,
			LevelViewportConfigurationNames::ThreePanesLeft,
			LevelViewportConfigurationNames::ThreePanesRight,
			LevelViewportConfigurationNames::ThreePanesTop,
			LevelViewportConfigurationNames::ThreePanesBottom,
			LevelViewportConfigurationNames::FourPanesLeft,
			LevelViewportConfigurationNames::FourPanesRight,
			LevelViewportConfigurationNames::FourPanesTop,
			LevelViewportConfigurationNames::FourPanesBottom,
			LevelViewportConfigurationNames::FourPanes2x2 })
		{
			if (Viewport->IsViewportConfigurationSet(LayoutName))
			{
				Info.Layout = LayoutName.ToString();
				break;
			}
		}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	return Info;
}

// =================================================================
// View Type
// =================================================================

bool UViewportService::SetViewportType(const FString& ViewType)
{
	FLevelEditorViewportClient* Client = GetActiveViewportClient();
	if (!Client)
	{
		UE_LOG(LogViewportService, Warning, TEXT("SetViewportType: No active viewport client"));
		return false;
	}

	int32 Type = StringToViewportType(ViewType);
	if (Type < 0)
	{
		UE_LOG(LogViewportService, Warning, TEXT("SetViewportType: Invalid type '%s'. Use: perspective, top, bottom, left, right, front, back"), *ViewType);
		return false;
	}

	Client->SetViewportType(static_cast<ELevelViewportType>(Type));
	ForceViewportRedraw();
	UE_LOG(LogViewportService, Log, TEXT("SetViewportType: Changed to '%s'"), *ViewType);
	return true;
}

FString UViewportService::GetViewportType()
{
	FLevelEditorViewportClient* Client = GetActiveViewportClient();
	if (!Client) { return TEXT("unknown"); }
	return ViewportTypeToString(static_cast<int32>(Client->GetViewportType()));
}

// =================================================================
// View Mode
// =================================================================

bool UViewportService::SetViewMode(const FString& ViewMode)
{
	FLevelEditorViewportClient* Client = GetActiveViewportClient();
	if (!Client)
	{
		UE_LOG(LogViewportService, Warning, TEXT("SetViewMode: No active viewport client"));
		return false;
	}

	FString Lower = ViewMode.ToLower().TrimStartAndEnd();
	EViewModeIndex NewMode;

	if (Lower == TEXT("lit"))                       NewMode = VMI_Lit;
	else if (Lower == TEXT("unlit"))                NewMode = VMI_Unlit;
	else if (Lower == TEXT("wireframe"))            NewMode = VMI_Wireframe;
	else if (Lower == TEXT("detaillighting"))       NewMode = VMI_Lit_DetailLighting;
	else if (Lower == TEXT("lightingonly"))          NewMode = VMI_LightingOnly;
	else if (Lower == TEXT("lightcomplexity"))      NewMode = VMI_LightComplexity;
	else if (Lower == TEXT("shadercomplexity"))     NewMode = VMI_ShaderComplexity;
	else if (Lower == TEXT("pathtracing"))          NewMode = VMI_PathTracing;
	else if (Lower == TEXT("clay"))                 NewMode = VMI_Clay;
	else
	{
		UE_LOG(LogViewportService, Warning, TEXT("SetViewMode: Unknown mode '%s'. Use: lit, unlit, wireframe, detaillighting, lightingonly, lightcomplexity, shadercomplexity, pathtracing, clay"), *ViewMode);
		return false;
	}

	Client->SetViewMode(NewMode);
	ForceViewportRedraw();
	UE_LOG(LogViewportService, Log, TEXT("SetViewMode: Changed to '%s'"), *ViewMode);
	return true;
}

FString UViewportService::GetViewMode()
{
	FLevelEditorViewportClient* Client = GetActiveViewportClient();
	if (!Client) { return TEXT("unknown"); }

	switch (Client->GetViewMode())
	{
	case VMI_Lit:                  return TEXT("lit");
	case VMI_Unlit:                return TEXT("unlit");
	case VMI_Wireframe:            return TEXT("wireframe");
	case VMI_Lit_DetailLighting:   return TEXT("detaillighting");
	case VMI_LightingOnly:         return TEXT("lightingonly");
	case VMI_LightComplexity:      return TEXT("lightcomplexity");
	case VMI_ShaderComplexity:     return TEXT("shadercomplexity");
	case VMI_PathTracing:          return TEXT("pathtracing");
	case VMI_Clay:                 return TEXT("clay");
	default:                       return TEXT("unknown");
	}
}

// =================================================================
// Field of View
// =================================================================

bool UViewportService::SetFOV(float FOVDegrees)
{
	FLevelEditorViewportClient* Client = GetActiveViewportClient();
	if (!Client)
	{
		UE_LOG(LogViewportService, Warning, TEXT("SetFOV: No active viewport client"));
		return false;
	}

	FOVDegrees = FMath::Clamp(FOVDegrees, 5.0f, 170.0f);
	Client->ViewFOV = FOVDegrees;
	Client->FOVAngle = FOVDegrees;
	ForceViewportRedraw();
	UE_LOG(LogViewportService, Log, TEXT("SetFOV: Set to %.1f degrees"), FOVDegrees);
	return true;
}

float UViewportService::GetFOV()
{
	FLevelEditorViewportClient* Client = GetActiveViewportClient();
	if (!Client) { return 90.0f; }
	return Client->ViewFOV;
}

// =================================================================
// Clipping Planes
// =================================================================

bool UViewportService::SetNearClipPlane(float Distance)
{
	FLevelEditorViewportClient* Client = GetActiveViewportClient();
	if (!Client)
	{
		UE_LOG(LogViewportService, Warning, TEXT("SetNearClipPlane: No active viewport client"));
		return false;
	}

	Client->OverrideNearClipPlane(Distance);
	ForceViewportRedraw();
	UE_LOG(LogViewportService, Log, TEXT("SetNearClipPlane: Set to %.1f"), Distance);
	return true;
}

bool UViewportService::SetFarClipPlane(float Distance)
{
	FLevelEditorViewportClient* Client = GetActiveViewportClient();
	if (!Client)
	{
		UE_LOG(LogViewportService, Warning, TEXT("SetFarClipPlane: No active viewport client"));
		return false;
	}

	Client->OverrideFarClipPlane(Distance);
	ForceViewportRedraw();
	UE_LOG(LogViewportService, Log, TEXT("SetFarClipPlane: Set to %.1f"), Distance);
	return true;
}

// =================================================================
// Exposure Settings
// =================================================================

bool UViewportService::SetExposure(bool bFixed, float EV100)
{
	FLevelEditorViewportClient* Client = GetActiveViewportClient();
	if (!Client)
	{
		UE_LOG(LogViewportService, Warning, TEXT("SetExposure: No active viewport client"));
		return false;
	}

	Client->ExposureSettings.bFixed = bFixed;
	Client->ExposureSettings.FixedEV100 = EV100;
	ForceViewportRedraw();
	UE_LOG(LogViewportService, Log, TEXT("SetExposure: Fixed=%s, EV100=%.2f"),
		bFixed ? TEXT("true") : TEXT("false"), EV100);
	return true;
}

bool UViewportService::SetExposureGameSettings()
{
	return SetExposure(false, 1.0f);
}

// =================================================================
// Game View & Cinematic
// =================================================================

bool UViewportService::SetGameView(bool bEnable)
{
	FLevelEditorViewportClient* Client = GetActiveViewportClient();
	if (!Client)
	{
		UE_LOG(LogViewportService, Warning, TEXT("SetGameView: No active viewport client"));
		return false;
	}

	if (Client->IsInGameView() != bEnable)
	{
		Client->SetGameView(bEnable);
		ForceViewportRedraw();
	}
	UE_LOG(LogViewportService, Log, TEXT("SetGameView: %s"), bEnable ? TEXT("enabled") : TEXT("disabled"));
	return true;
}

bool UViewportService::SetAllowCinematicControl(bool bAllow)
{
	FLevelEditorViewportClient* Client = GetActiveViewportClient();
	if (!Client)
	{
		UE_LOG(LogViewportService, Warning, TEXT("SetAllowCinematicControl: No active viewport client"));
		return false;
	}

	Client->SetAllowCinematicControl(bAllow);
	ForceViewportRedraw();
	UE_LOG(LogViewportService, Log, TEXT("SetAllowCinematicControl: %s"), bAllow ? TEXT("true") : TEXT("false"));
	return true;
}

// =================================================================
// Realtime Rendering
// =================================================================

bool UViewportService::SetRealtime(bool bRealtime)
{
	FLevelEditorViewportClient* Client = GetActiveViewportClient();
	if (!Client)
	{
		UE_LOG(LogViewportService, Warning, TEXT("SetRealtime: No active viewport client"));
		return false;
	}

	Client->SetRealtime(bRealtime);
	ForceViewportRedraw();
	UE_LOG(LogViewportService, Log, TEXT("SetRealtime: %s"), bRealtime ? TEXT("true") : TEXT("false"));
	return true;
}

// =================================================================
// Camera Speed
// =================================================================

bool UViewportService::SetCameraSpeed(int32 SpeedSetting)
{
	FLevelEditorViewportClient* Client = GetActiveViewportClient();
	if (!Client)
	{
		UE_LOG(LogViewportService, Warning, TEXT("SetCameraSpeed: No active viewport client"));
		return false;
	}

	SpeedSetting = FMath::Clamp(SpeedSetting, 1, 8);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Client->SetCameraSpeedSetting(SpeedSetting);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	UE_LOG(LogViewportService, Log, TEXT("SetCameraSpeed: Set to %d"), SpeedSetting);
	return true;
}

// =================================================================
// Viewport Layout
// =================================================================

bool UViewportService::SetViewportLayout(const FString& LayoutName)
{
	TSharedPtr<SLevelViewport> Viewport = GetActiveLevelViewport();
	if (!Viewport.IsValid())
	{
		UE_LOG(LogViewportService, Warning, TEXT("SetViewportLayout: No active level viewport"));
		return false;
	}

	// Validate layout name against known configurations
	static const TMap<FString, FName> ValidLayouts = {
		{ TEXT("onepane"),           LevelViewportConfigurationNames::OnePane },
		{ TEXT("single"),            LevelViewportConfigurationNames::OnePane },
		{ TEXT("twopanesh"),         LevelViewportConfigurationNames::TwoPanesHoriz },
		{ TEXT("twopanes"),          LevelViewportConfigurationNames::TwoPanesHoriz },
		{ TEXT("twopaneshoriz"),     LevelViewportConfigurationNames::TwoPanesHoriz },
		{ TEXT("twopanesvert"),      LevelViewportConfigurationNames::TwoPanesVert },
		{ TEXT("threepanesleft"),    LevelViewportConfigurationNames::ThreePanesLeft },
		{ TEXT("threepanesright"),  LevelViewportConfigurationNames::ThreePanesRight },
		{ TEXT("threepanestop"),     LevelViewportConfigurationNames::ThreePanesTop },
		{ TEXT("threepanesbottom"),  LevelViewportConfigurationNames::ThreePanesBottom },
		{ TEXT("fourpanesleft"),     LevelViewportConfigurationNames::FourPanesLeft },
		{ TEXT("fourpanesright"),    LevelViewportConfigurationNames::FourPanesRight },
		{ TEXT("fourpanestop"),      LevelViewportConfigurationNames::FourPanesTop },
		{ TEXT("fourpanesbottom"),   LevelViewportConfigurationNames::FourPanesBottom },
		{ TEXT("fourpanes2x2"),      LevelViewportConfigurationNames::FourPanes2x2 },
		{ TEXT("quad"),              LevelViewportConfigurationNames::FourPanes2x2 },
	};

	FString LookupKey = LayoutName.ToLower().TrimStartAndEnd();
	const FName* ConfigName = ValidLayouts.Find(LookupKey);

	if (!ConfigName)
	{
		UE_LOG(LogViewportService, Warning,
			TEXT("SetViewportLayout: Invalid layout '%s'. Valid: OnePane, TwoPanesHoriz, TwoPanesVert, "
				"ThreePanesLeft, ThreePanesRight, ThreePanesTop, ThreePanesBottom, "
				"FourPanesLeft, FourPanesRight, FourPanesTop, FourPanesBottom, FourPanes2x2, Quad"),
			*LayoutName);
		return false;
	}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Viewport->OnSetViewportConfiguration(*ConfigName);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	UE_LOG(LogViewportService, Log, TEXT("SetViewportLayout: Changed to '%s'"), *ConfigName->ToString());
	return true;
}

FString UViewportService::GetViewportLayout()
{
	TSharedPtr<SLevelViewport> Viewport = GetActiveLevelViewport();
	if (!Viewport.IsValid()) { return TEXT("unknown"); }

	FString Result = TEXT("unknown");

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	for (const FName& LayoutName : {
		LevelViewportConfigurationNames::OnePane,
		LevelViewportConfigurationNames::TwoPanesHoriz,
		LevelViewportConfigurationNames::TwoPanesVert,
		LevelViewportConfigurationNames::ThreePanesLeft,
		LevelViewportConfigurationNames::ThreePanesRight,
		LevelViewportConfigurationNames::ThreePanesTop,
		LevelViewportConfigurationNames::ThreePanesBottom,
		LevelViewportConfigurationNames::FourPanesLeft,
		LevelViewportConfigurationNames::FourPanesRight,
		LevelViewportConfigurationNames::FourPanesTop,
		LevelViewportConfigurationNames::FourPanesBottom,
		LevelViewportConfigurationNames::FourPanes2x2 })
	{
		if (Viewport->IsViewportConfigurationSet(LayoutName))
		{
			Result = LayoutName.ToString();
			break;
		}
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	return Result;
}

// =================================================================
// Scene Capture (works while the editor is backgrounded)
// =================================================================

FSceneCaptureResult UViewportService::CaptureScene(
	FVector Location,
	FRotator Rotation,
	int32 Width,
	int32 Height,
	const FString& OutputPngPath,
	float OrthoWidth,
	float FOV,
	float ManualEV100)
{
	FSceneCaptureResult Result;

	// --- Argument / environment validation (refusals: Warning) ---
	if (!GEditor)
	{
		Result.ErrorMessage = TEXT("CaptureScene: no GEditor (running headless?).");
		UE_LOG(LogViewportService, Warning, TEXT("%s"), *Result.ErrorMessage);
		return Result;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Result.ErrorMessage = TEXT("CaptureScene: no editor world available.");
		UE_LOG(LogViewportService, Warning, TEXT("%s"), *Result.ErrorMessage);
		return Result;
	}

	if (Width <= 0 || Height <= 0 || Width > 8192 || Height > 8192)
	{
		Result.ErrorMessage = FString::Printf(
			TEXT("CaptureScene: invalid dimensions %dx%d (each must be 1..8192)."), Width, Height);
		UE_LOG(LogViewportService, Warning, TEXT("%s"), *Result.ErrorMessage);
		return Result;
	}

	if (OutputPngPath.TrimStartAndEnd().IsEmpty())
	{
		Result.ErrorMessage = TEXT("CaptureScene: OutputPngPath is empty.");
		UE_LOG(LogViewportService, Warning, TEXT("%s"), *Result.ErrorMessage);
		return Result;
	}

	// Resolve output path: relative → <Project>/Saved/VibeUE/Captures; ensure .png extension.
	FString OutPath = OutputPngPath.TrimStartAndEnd();
	FPaths::NormalizeFilename(OutPath);
	if (FPaths::IsRelative(OutPath))
	{
		OutPath = FPaths::Combine(
			FPaths::ProjectSavedDir(), TEXT("VibeUE"), TEXT("Captures"), OutPath);
	}
	if (!OutPath.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase))
	{
		OutPath += TEXT(".png");
	}

	// --- Render target: RTF_RGBA8 (the RTF_RGBA16f default writes non-PNG bytes) ---
	UTextureRenderTarget2D* RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		World, Width, Height, RTF_RGBA8, FLinearColor::Black, /*bAutoGenerateMipMaps=*/false, /*bSupportUAVs=*/false);
	if (!RenderTarget)
	{
		Result.ErrorMessage = TEXT("CaptureScene: CreateRenderTarget2D returned null.");
		UE_LOG(LogViewportService, Error, TEXT("%s"), *Result.ErrorMessage);
		return Result;
	}
	// Root the render target so it cannot be GC'd between creation and readback.
	RenderTarget->AddToRoot();

	// --- Spawn the transient capture actor ---
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASceneCapture2D* CaptureActor = World->SpawnActor<ASceneCapture2D>(Location, Rotation, SpawnParams);
	if (!CaptureActor)
	{
		RenderTarget->RemoveFromRoot();
		Result.ErrorMessage = TEXT("CaptureScene: failed to spawn ASceneCapture2D in the editor world.");
		UE_LOG(LogViewportService, Error, TEXT("%s"), *Result.ErrorMessage);
		return Result;
	}

	USceneCaptureComponent2D* CaptureComp = CaptureActor->GetCaptureComponent2D();
	if (!CaptureComp)
	{
		World->DestroyActor(CaptureActor);
		RenderTarget->RemoveFromRoot();
		Result.ErrorMessage = TEXT("CaptureScene: ASceneCapture2D has no capture component.");
		UE_LOG(LogViewportService, Error, TEXT("%s"), *Result.ErrorMessage);
		return Result;
	}

	CaptureComp->bCaptureEveryFrame = false;
	CaptureComp->bCaptureOnMovement = false;
	CaptureComp->TextureTarget = RenderTarget;
	// SCS_FinalColorLDR gives alpha 255; SCS_BaseColor writes alpha 0 (blank-looking PNGs).
	CaptureComp->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

	if (OrthoWidth > 0.0f)
	{
		CaptureComp->ProjectionType = ECameraProjectionMode::Orthographic;
		CaptureComp->OrthoWidth = OrthoWidth;
	}
	else
	{
		CaptureComp->ProjectionType = ECameraProjectionMode::Perspective;
		CaptureComp->FOVAngle = FOV;
	}

	// Exposure. ManualEV100 == 0 keeps the engine's DEFAULT (automatic) exposure — correct for a
	// foreground / PIE window. A backgrounded editor has no converged eye adaptation and captures
	// black on auto, so a non-zero value switches to a FIXED manual exposure DECOUPLED from the
	// physical camera (AutoExposureApplyPhysicalCameraExposure=false, so the exposure does not depend
	// on the default f/4, 1/60, ISO100 physical settings).
	//
	// In this decoupled-Manual path the engine's AutoExposureBias acts as the manual exposure TARGET
	// in EV100 — exactly like a camera's metered EV: a HIGHER EV100 assumes a brighter scene and stops
	// down, so the captured image gets DARKER; a lower/negative value brightens. This is the MEASURED
	// behaviour (rebuilt UE 5.8, daylit scene from a backgrounded editor, mean RGB luminance):
	//   EV100  0(auto)=16.4  +1=12.2  +2=8.8  +3=6.2  +4=4.3  -1=21.9  -2=28.1  -4=43.8  -6=63.4  -8=86.0
	// (Note: the engine source reads AutoExposureBias as a pow(2,bias) exposure *compensation* —
	// PostProcessEyeAdaptation.usf:179/193 — which would predict the opposite sign; the inversion above
	// is empirical for this SceneCapture Manual path, so we document the measurement, not the formula.)
	// A daylit backgrounded scene reads well around -6..-8; -4 is a good first try for bright scenes.
	if (!FMath::IsNearlyZero(ManualEV100))
	{
		CaptureComp->PostProcessSettings.bOverride_AutoExposureMethod = true;
		CaptureComp->PostProcessSettings.AutoExposureMethod = AEM_Manual;
		CaptureComp->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		CaptureComp->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
		CaptureComp->PostProcessSettings.bOverride_AutoExposureBias = true;
		CaptureComp->PostProcessSettings.AutoExposureBias = ManualEV100;
	}

	// --- Synchronous render + readback ---
	CaptureComp->CaptureScene();
	FlushRenderingCommands();

	TArray<FColor> Pixels;
	bool bReadOk = false;
	if (FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource())
	{
		// Default flags: SCS_FinalColorLDR into an RTF_RGBA8 target is already display-encoded
		// 8-bit BGRA, so a straight read (the pattern UAnimSequenceService uses) is correct.
		bReadOk = RTResource->ReadPixels(Pixels);
	}

	// The transient actor has done its job; destroy it before any early return below.
	World->DestroyActor(CaptureActor);

	if (!bReadOk || Pixels.Num() == 0)
	{
		RenderTarget->RemoveFromRoot();
		Result.ErrorMessage = TEXT("CaptureScene: ReadPixels from the render target returned no data.");
		UE_LOG(LogViewportService, Error, TEXT("%s"), *Result.ErrorMessage);
		return Result;
	}

	// Force opaque: RTF_RGBA8 / final-color captures can leave A=0, which renders as a
	// blank page in image viewers even though the RGB is a full capture.
	for (FColor& Px : Pixels)
	{
		Px.A = 255;
	}

	TArray64<uint8> Png;
	FImageUtils::PNGCompressImageArray(Width, Height, Pixels, Png);

	// The render target is no longer needed; allow it to be GC'd.
	RenderTarget->RemoveFromRoot();

	if (Png.Num() == 0)
	{
		Result.ErrorMessage = TEXT("CaptureScene: PNG encoding produced no data.");
		UE_LOG(LogViewportService, Error, TEXT("%s"), *Result.ErrorMessage);
		return Result;
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutPath), /*Tree=*/true);
	if (!FFileHelper::SaveArrayToFile(TArrayView<const uint8>(Png.GetData(), (int32)Png.Num()), *OutPath))
	{
		Result.ErrorMessage = FString::Printf(TEXT("CaptureScene: failed to write PNG to '%s'."), *OutPath);
		UE_LOG(LogViewportService, Error, TEXT("%s"), *Result.ErrorMessage);
		return Result;
	}

	Result.bSuccess = true;
	Result.OutputPath = OutPath;
	Result.Width = Width;
	Result.Height = Height;
	Result.FileSizeBytes = IFileManager::Get().FileSize(*OutPath);
	UE_LOG(LogViewportService, Log,
		TEXT("CaptureScene: wrote %dx%d (%lld bytes, %s) to %s"),
		Width, Height, Result.FileSizeBytes,
		OrthoWidth > 0.0f ? TEXT("orthographic") : TEXT("perspective"), *OutPath);
	return Result;
}
