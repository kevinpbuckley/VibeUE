// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VibeUE : ModuleRules
{
	public VibeUE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		// Disable IWYU for better cross-environment compatibility (fix for GitHub issue #7)
		IWYUSupport = IWYUSupport.None;
		// Disable unity builds to ensure each file compiles independently
		bUseUnity = false;
		
		// Ensure proper debug symbol generation for PDB files
		if (Target.Configuration == UnrealTargetConfiguration.Debug || 
		    Target.Configuration == UnrealTargetConfiguration.DebugGame ||
		    Target.Configuration == UnrealTargetConfiguration.Development)
		{
			OptimizeCode = CodeOptimization.Never; // Ensure no optimization interferes with debugging
		}

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
		);
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
		);
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Networking",
				"Sockets",
				"HTTP",
				"Json",
				"JsonUtilities",
				"DeveloperSettings",
				"ApplicationCore"       // For FPlatformApplicationMisc (clipboard, etc.)
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"EditorScriptingUtilities",
				"EditorSubsystem",
				"Slate",
				"SlateCore",
				"UMG",
				"Kismet",
				"KismetCompiler",
				"BlueprintGraph",
				"Projects",
				"AssetRegistry",
				"MessageLog",
				"EditorStyle",
				"AssetTools",
				"PropertyEditor",         // For property reflection
				"EnhancedInput",          // For Enhanced Input System support
				"InputCore"               // For input types
			}
		);
		
		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"PropertyEditor",      // For widget property editing
					"ToolMenus",           // For editor UI
					"BlueprintEditorLibrary", // For Blueprint utilities
					"UMGEditor",           // For WidgetBlueprint.h and other UMG editor functionality
					"MaterialEditor",      // For material editor integration
					"LevelEditor"          // For global keyboard shortcuts
				}
			);
		}
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
		);
	}
} 