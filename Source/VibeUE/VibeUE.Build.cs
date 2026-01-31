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
				"ApplicationCore",      // For FPlatformApplicationMisc (clipboard, etc.)
				"WebSockets"            // For ElevenLabs WebSocket connection
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"EditorScriptingUtilities",
				"EditorSubsystem",
				"PythonScriptPlugin",   // For Python code execution
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
				"InputCore",              // For input types
				"AudioCapture",           // For microphone input
				"AudioCaptureCore",       // For FAudioCaptureSynth
				"ImageWrapper",           // For image encoding/decoding
				"DesktopPlatform",        // For file dialogs
				"Niagara",                // For Niagara VFX runtime classes
				"NiagaraEditor",          // For Niagara editor utilities and factories
				"AnimGraph",              // For AnimGraphNode types (state machines, states)
				"AnimGraphRuntime",       // For animation runtime types
				"Persona",                // For IAnimationBlueprintEditor interface
				"SkeletalMeshModifiers",  // For SkeletonModifier (bone manipulation) - MeshModelingToolset plugin
				"SkeletalMeshEditor"      // For SkeletalMeshEditorSubsystem
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
					"LevelEditor",         // For global keyboard shortcuts
					"StatusBar",           // For panel drawer integration
					"ContentBrowser"       // For content browser selection queries
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