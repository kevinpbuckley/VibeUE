// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Tools/VisionTools.h"
#include "Core/ToolRegistry.h"
#include "Json.h"

// Helper function to extract a field from parameters
static FString ExtractParam(const TMap<FString, FString>& Params, const FString& FieldName, const FString& DefaultValue = FString())
{
	const FString* DirectParam = Params.Find(FieldName);
	if (DirectParam)
	{
		return *DirectParam;
	}
	
	// Try capitalized version (MCP server may capitalize first letter)
	FString CapitalizedField = FieldName;
	if (CapitalizedField.Len() > 0)
	{
		CapitalizedField[0] = FChar::ToUpper(CapitalizedField[0]);
	}
	DirectParam = Params.Find(CapitalizedField);
	if (DirectParam)
	{
		return *DirectParam;
	}

	return DefaultValue;
}

// Helper to extract float
static float ExtractFloatParam(const TMap<FString, FString>& Params, const FString& FieldName, float DefaultValue)
{
	FString Value = ExtractParam(Params, FieldName);
	if (Value.IsEmpty())
	{
		return DefaultValue;
	}
	return FCString::Atof(*Value);
}

// Register capture_viewport tool
REGISTER_VIBEUE_TOOL(capture_viewport,
	"Captures the current viewport as a screenshot and returns it as a base64-encoded image for AI vision analysis. Use this when you need to see what the user is seeing in the editor.",
	"Vision",
	TOOL_PARAMS(
		TOOL_PARAM("prompt", "Optional prompt describing what to look for in the image", "string", false),
		TOOL_PARAM("resolution_scale", "Scale factor for screenshot resolution (1.0 = current viewport size, 2.0 = 2x resolution)", "number", false)
	),
	{
		FString Prompt = ExtractParam(Params, TEXT("prompt"), TEXT(""));
		float ResolutionScale = ExtractFloatParam(Params, TEXT("resolution_scale"), 1.0f);
		return UVisionTools::CaptureViewport(Prompt, ResolutionScale);
	}
);
