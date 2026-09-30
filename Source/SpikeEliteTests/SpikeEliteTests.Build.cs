using UnrealBuildTool;

public class SpikeEliteTests : ModuleRules
{
	public SpikeEliteTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Tests link the game module so they can exercise the pure-logic rules
		// (SEVolleyballRules) directly, without loading a map.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"SpikeElite"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore"
		});
	}
}
