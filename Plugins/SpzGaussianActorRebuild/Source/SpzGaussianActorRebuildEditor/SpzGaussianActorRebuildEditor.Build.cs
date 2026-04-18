using System.IO;
using UnrealBuildTool;

public class SpzGaussianActorRebuildEditor : ModuleRules
{
	public SpzGaussianActorRebuildEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"SpzGaussianActorRebuild"
			});

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"AssetRegistry",
				"AssetTools",
				"EditorFramework",
				"Kismet",
				"MaterialEditor",
				"UnrealEd"
			});

		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "ThirdParty", "SPZ"));
		AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");
	}
}
