// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UViewportService.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "SEditorViewport.h"
#include "LevelEditorViewport.h"
#include "LevelViewportActions.h"
#include "EditorViewportClient.h"

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
// Camera Position & Speed
// =================================================================

bool UViewportService::SetCameraLocation(FVector NewLocation)
{
	FLevelEditorViewportClient* Client = GetActiveViewportClient();
	if (!Client)
	{
		UE_LOG(LogViewportService, Warning, TEXT("SetCameraLocation: No active viewport client"));
		return false;
	}

	Client->SetViewLocation(NewLocation);
	ForceViewportRedraw();
	UE_LOG(LogViewportService, Log, TEXT("SetCameraLocation: (%.1f, %.1f, %.1f)"),
		NewLocation.X, NewLocation.Y, NewLocation.Z);
	return true;
}

bool UViewportService::SetCameraRotation(FRotator NewRotation)
{
	FLevelEditorViewportClient* Client = GetActiveViewportClient();
	if (!Client)
	{
		UE_LOG(LogViewportService, Warning, TEXT("SetCameraRotation: No active viewport client"));
		return false;
	}

	Client->SetViewRotation(NewRotation);
	ForceViewportRedraw();
	UE_LOG(LogViewportService, Log, TEXT("SetCameraRotation: (P=%.1f, Y=%.1f, R=%.1f)"),
		NewRotation.Pitch, NewRotation.Yaw, NewRotation.Roll);
	return true;
}

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
