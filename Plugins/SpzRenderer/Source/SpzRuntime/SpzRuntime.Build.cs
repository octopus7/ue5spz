using UnrealBuildTool;

public class SpzRuntime : ModuleRules
{
	public SpzRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			});

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"RenderCore",
				"RHI"
			});
	}
}
