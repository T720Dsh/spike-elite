using UnrealBuildTool;

public class SpikeEliteTarget : TargetRules
{
	public SpikeEliteTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		// M11a: keep the Game target on the same include-order version as the
		// Editor target to eliminate the Unreal5_6 include-order build warning.
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("SpikeElite");
	}
}
