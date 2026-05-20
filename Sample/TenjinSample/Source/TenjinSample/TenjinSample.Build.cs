// Copyright (c) Tenjin. All Rights Reserved.

using UnrealBuildTool;

public class TenjinSample : ModuleRules
{
	public TenjinSample(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Lets files in subfolders (e.g. Widgets/) include module-root
		// headers like "TenjinSampleListener.h" without relative paths.
		PrivateIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"UMG",
			"Slate",
			"SlateCore",
			"TenjinSDK",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"HTTP",
		});
	}
}
