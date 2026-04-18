// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class spzdemoEditor : ModuleRules
{
	public spzdemoEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"spzdemo"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"AssetTools",
				"EditorFramework",
				"Kismet",
				"Niagara",
				"NiagaraEditor",
				"UnrealEd"
			});

		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty", "SPZ"));

		AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");
	}
}
