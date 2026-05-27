// Copyright (c) Tenjin. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class TenjinSampleTarget : TargetRules
{
	public TenjinSampleTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		ExtraModuleNames.AddRange(new string[] { "TenjinSample" });
	}
}
