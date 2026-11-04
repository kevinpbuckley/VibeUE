// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "LandscapeEditLayer.h"
#include "LandscapeBlueprintBrushBase.h"
#include "LandscapeServiceTestEditLayer.generated.h"

// Abstract keeps this fixture out of the editor's layer picker.
UCLASS(Abstract, Transient, NotBlueprintable, NotBlueprintType, HideDropdown)
class UVibeUELandscapeServiceTestEditLayer : public ULandscapeEditLayer
{
	GENERATED_BODY()

public:
	// These inherited implementations are not exported by the Landscape module.
	virtual bool SupportsTargetType(ELandscapeToolTargetType InType) const override
	{
		return InType == ELandscapeToolTargetType::Heightmap || InType == ELandscapeToolTargetType::Weightmap ||
			InType == ELandscapeToolTargetType::Visibility;
	}

#if WITH_EDITOR
	virtual UE::Landscape::EditLayers::ERenderFlags GetRenderFlags(const UE::Landscape::EditLayers::FMergeContext* InMergeContext) const override
	{
		using namespace UE::Landscape::EditLayers;
		return ERenderFlags::RenderMode_Recorded | ERenderFlags::BlendMode_SeparateBlend;
	}
#endif
};

UCLASS(Transient, NotBlueprintable, NotBlueprintType, NotPlaceable, HideDropdown)
class AVibeUELandscapeServiceTestBrush : public ALandscapeBlueprintBrushBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TObjectPtr<ULandscapeLayerInfoObject> LayerInfo;
};
