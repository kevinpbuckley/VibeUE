// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Tools/ToolMacros.h"
#include "VisionTools.generated.h"

/**
 * Vision tools for capturing and analyzing viewport screenshots
 * Enables AI to "see" the level and provide visual feedback
 */
UCLASS(meta = (
	ToolCategory = "Vision",
	ToolDescription = "Tools for capturing and analyzing viewport screenshots"
))
class VIBEUE_API UVisionTools : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Captures the current viewport as a base64-encoded image
	 * Uses HighResShot console command to render and save screenshot
	 * 
	 * @param Prompt Optional prompt describing what to look for in the image
	 * @param ResolutionScale Scale factor for screenshot resolution (1.0 = current viewport size, 2.0 = 2x resolution)
	 * @return JSON with success status and base64-encoded image data URL
	 */
	UFUNCTION(meta = (
		ToolName = "capture_viewport",
		ToolDescription = "Captures the current viewport as a screenshot and returns it as a base64-encoded image for AI vision analysis. Use this when you need to see what the user is seeing in the editor.",
		ToolCategory = "Vision",
		ToolExamples = "capture_viewport(prompt=\"Analyze the level lighting\", resolution_scale=1.0)"
	))
	static FString CaptureViewport(const FString& Prompt = TEXT(""), float ResolutionScale = 1.0f);
};
