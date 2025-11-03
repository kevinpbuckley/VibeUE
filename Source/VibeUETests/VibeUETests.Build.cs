// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VibeUETests : ModuleRules
{
	public VibeUETests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		// Disable IWYU for better cross-environment compatibility
		IWYUSupport = IWYUSupport.None;
		
		// Disable unity builds to ensure each file compiles independently
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"VibeUE",
				"UnrealEd"
			}
		);
	}
}
