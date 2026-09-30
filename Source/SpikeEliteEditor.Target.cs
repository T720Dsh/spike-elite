using UnrealBuildTool;

public class SpikeEliteEditorTarget : TargetRules
{
	public SpikeEliteEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		// Android File Server is an editor-side tool for mobile work; this is a
		// Win64-only project and the plugin must never run (it writes a fresh
		// random SecurityToken back into DefaultEngine.ini on every launch).
		DisablePlugins.Add("AndroidFileServer");
		ExtraModuleNames.Add("SpikeElite");
		// Developer-only automation test module (never shipped).
		ExtraModuleNames.Add("SpikeEliteTests");
	}
}
