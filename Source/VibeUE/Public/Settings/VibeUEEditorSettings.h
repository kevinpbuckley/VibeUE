// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VibeUEEditorSettings.generated.h"

// VibeUE editor settings.
//
// Surfaced under Editor Preferences > Plugins > VibeUE. Stored as a per-machine user setting
// (globaluserconfig) so the key is entered once and shared across all projects, and never
// committed to source control. globaluserconfig also suppresses the panel's Set-as-Default /
// Reset-to-Defaults buttons. Read it from C++ with: GetDefault<UVibeUEEditorSettings>()->ApiKey
UCLASS(config = EditorSettings, globaluserconfig, meta = (DisplayName = "VibeUE"))
class VIBEUE_API UVibeUEEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Free VibeUE API key (vibeue.com/login). Used by the terrain_data tool. */
	UPROPERTY(config, EditAnywhere, Category = "API", meta = (DisplayName = "API Key", PasswordField = true))
	FString ApiKey;

	//~ Place the settings panel under Editor Preferences > Plugins > VibeUE
	virtual FName GetContainerName() const override { return TEXT("Editor"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("VibeUE"); }
};
