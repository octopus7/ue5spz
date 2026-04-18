using UnrealBuildTool;

public class SpzGaussianActorRebuild : ModuleRules
{
	public SpzGaussianActorRebuild(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"RHI",
				"RenderCore"
			});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new[]
				{
					"EditorFramework",
					"UnrealEd"
				});
		}
	}
}
