// Copyright (c) Tenjin. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class TenjinSDK : ModuleRules
{
	public TenjinSDK(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[] {
			Path.Combine(ModuleDirectory, "Public"),
		});

		PrivateIncludePaths.AddRange(new string[] {
			Path.Combine(ModuleDirectory, "Private"),
		});

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Projects",
		});

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PrivateDependencyModuleNames.Add("Launch");

			// TenjinSDK.xcframework lives in ThirdParty/iOS/. Run
			// Scripts/install-ios-spm.sh from the repo root to populate it
			// via Swift Package Manager.
			string XCFrameworkPath = Path.Combine(PluginDirectory, "ThirdParty", "iOS", "TenjinSDK.xcframework");
			if (Directory.Exists(XCFrameworkPath))
			{
				PublicAdditionalFrameworks.Add(new Framework(
					"TenjinSDK",
					XCFrameworkPath,
					null,
					true
				));
			}
			else
			{
				System.Console.WriteLine(
					"WARNING: TenjinSDK.xcframework not found at " + XCFrameworkPath + ".\n" +
					"         Run Scripts/install-ios-spm.sh to fetch it via SPM, or drop the\n" +
					"         framework in manually before packaging an iOS build."
				);
			}

			// Apple frameworks Tenjin depends on at runtime.
			PublicFrameworks.AddRange(new string[] {
				"AdSupport",
				"StoreKit",
				"SystemConfiguration",
				"WebKit",
			});

			// iOS 14+ frameworks — link weakly so the app still loads on older OS.
			PublicWeakFrameworks.AddRange(new string[] {
				"AdServices",
				"AppTrackingTransparency",
			});

			PublicSystemLibraries.AddRange(new string[] {
				"sqlite3",
				"z",
			});

			// Ship a plist fragment so we register the URL scheme & ATT key.
			AdditionalPropertiesForReceipt.Add("IOSPlugin", Path.Combine(ModuleDirectory, "TenjinSDK_UPL_iOS.xml"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PrivateDependencyModuleNames.Add("Launch");
			AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModuleDirectory, "TenjinSDK_UPL_Android.xml"));
		}
	}
}
