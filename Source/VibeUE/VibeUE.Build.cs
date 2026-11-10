// Copyright Buckley Builds LLC 2026 All Rights Reserved.

using System.IO;
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
		// Do not add blanket /WX to this module. Engine patch releases can introduce
		// deprecation warnings in engine headers included by VibeUE, and /WX turns
		// those upstream C4996 diagnostics into plugin build failures. UnrealBuildTool
		// still applies its curated warning-as-error policy; deprecations remain visible
		// as warnings so supported-engine CI can identify VibeUE-owned API migrations.
		// See issues #491 and #569.
		bWarningsAsErrors = false;

		// EnvironmentQueryEditor is a PLUGIN (Engine/Plugins/AI/EnvironmentQueryEditor), not an
		// engine module like BehaviorTreeEditor. It is EnabledByDefault, but a project may disable
		// it, so the EQS service compiles to stubs rather than breaking the whole plugin's build.
		bool bEqsEditorPresent = File.Exists(
			Path.Combine(EngineDirectory, "Plugins", "AI", "EnvironmentQueryEditor",
				"EnvironmentQueryEditor.uplugin"));
		PrivateDefinitions.Add("WITH_VIBEUE_EQS=" + (bEqsEditorPresent ? "1" : "0"));

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
				"HTTP",                 // For DeepResearch / web tools
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
				"MovieScene",
				"MovieSceneTracks",
				"RenderCore",
				"RHI",                    // For GMaxRHIShaderPlatform / GShaderPlatformForFeatureLevel (material diagnostics)
				"EditorStyle",
				"AssetTools",
				"PropertyEditor",         // For property reflection
				"EnhancedInput",          // For Enhanced Input System support
				"InputCore",              // For input types
				"ImageWrapper",           // For image encoding/decoding
				"DesktopPlatform",        // For file dialogs
				"Niagara",                // For Niagara VFX runtime classes
				"NiagaraEditor",          // For Niagara editor utilities and factories
				"AnimGraph",              // For AnimGraphNode types (state machines, states)
				"AnimGraphRuntime",       // For animation runtime types
				"Persona",                // For IAnimationBlueprintEditor interface
				"SkeletalMeshModifiers",  // For SkeletonModifier (bone manipulation) - MeshModelingToolset plugin
				"SkeletalMeshEditor",     // For SkeletalMeshEditorSubsystem
				"InputBlueprintNodes",    // For UK2Node_EnhancedInputAction (Enhanced Input event nodes)
				"Landscape",              // For ALandscape, ULandscapeInfo, ULandscapeComponent
				"PhysicsCore",            // UPhysicalMaterial / EPhysicalSurface in LandscapeService + FoliageService
				"LandscapeEditor",        // For landscape editor factories and utilities
				"Foliage",                // Required by LandscapeEdit.h (InstancedFoliageActor)
				"ModelViewViewModel",     // For MVVM ViewModel base classes (UMVVMViewModelBase)
				"ModelViewViewModelBlueprint", // For MVVM Blueprint View and bindings
				"StateTreeModule",        // For UStateTree, UStateTreeEditorData, StateTree core types
				"StateTreeEditorModule",  // For FStateTreeCompiler, UStateTreeState, StateTree editor types
				"PropertyBindingUtils",    // For FPropertyBindingBindableStructDescriptor (base of FStateTreeBindableStructDesc)
				"GameplayTags",           // For FGameplayTag (required by StateTree)
				"AudioEditor",            // For SoundCue graph classes (USoundCueGraphNode, USoundCueGraph, USoundCueFactoryNew)
				"MeshDescription",        // For FMeshDescription / TVertexInstanceAttributesRef (UV editing)
				"StaticMeshDescription",  // For FStaticMeshAttributes / FStaticMeshOperations / FUVMapParameters
				"ToolsetRegistry",        // UE 5.8 native AI toolset registry — exposes services as AICallable tools on Epic's MCP endpoint
				"AsyncFunctionResult",    // UE 5.9: UAsyncFunctionResultString (was UToolCallAsyncResultString, removed in 5.9) returned by UToolsetRegistry::ExecuteTool
				"ModelContextProtocol",   // UE 5.8 native MCP server — VibeUE's dynamic tools are bridged onto Epic's endpoint
				"ModelContextProtocolEngine", // UE::ModelContextProtocol::GetServerPortNumber() — configured MCP port + -ModelContextProtocolPort= override (B6)
				"Sockets",                // FSocket / ISocketSubsystem bind probe for the MCP port-fight cross-check (B6)
				"AIModule",               // UBehaviorTree, UBlackboardData, UBTNode, blackboard key types
				"GameplayTasks",          // AIModule dependency
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
					"ContentBrowser",      // For content browser selection queries
					"AIGraph",             // UAIGraphNode, FGraphNodeClassHelper (BT node class discovery)
					"BehaviorTreeEditor",  // UBehaviorTreeGraph / UBehaviorTreeGraphNode_* — the BT EdGraph write path
				"GameplayTagsEditor",  // For IGameplayTagsEditorModule (add/remove/rename tags at editor time)
				"TraceServices",       // For ITraceServicesModule / IAnalysisService (editor_control analyse action)
				}
			);

			if (bEqsEditorPresent)
			{
				PrivateDependencyModuleNames.Add("EnvironmentQueryEditor");  // EQS service: UEnvironmentQueryGraph / GraphNode_{Root,Option,Test}
			}
		}
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
		);
	}
}
