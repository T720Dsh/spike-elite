using UnrealBuildTool;
using System.Collections.Generic;

public class SpikeEliteTarget : TargetRules
{
	public SpikeEliteTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		ExtraModuleNames.Add("SpikeElite");
	}
}
