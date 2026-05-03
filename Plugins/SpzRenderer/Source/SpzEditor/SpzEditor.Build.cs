using System.IO;
using UnrealBuildTool;

public class SpzEditor : ModuleRules
{
	public SpzEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"SpzRuntime"
			});

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"UnrealEd",
				"AssetTools",
				"AssetDefinition",
				"Projects",
				"zlib"
			});

		string ThirdPartyPath = Path.Combine(ModuleDirectory, "Private", "ThirdParty");
		string SpzPath = Path.Combine(ThirdPartyPath, "spz");
		string ZstdPath = Path.Combine(ThirdPartyPath, "zstd", "lib");

		PrivateIncludePaths.AddRange(
			new[]
			{
				SpzPath,
				ZstdPath,
				Path.Combine(ZstdPath, "common"),
				Path.Combine(ZstdPath, "compress"),
				Path.Combine(ZstdPath, "decompress")
			});

		PrivateDefinitions.AddRange(
			new[]
			{
				"ZSTD_DISABLE_ASM=1",
				"ZSTD_MULTITHREAD=0",
				"_CRT_SECURE_NO_WARNINGS"
			});

		CppCompileWarningSettings.ShadowVariableWarningLevel = WarningLevel.Off;
	}
}
