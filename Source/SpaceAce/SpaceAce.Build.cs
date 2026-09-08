// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class SpaceAce : ModuleRules
{
	public SpaceAce(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"EnhancedInput", 
			"Niagara", 
			"GameplayTags",
			"UMG"
		 });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Add subdirectories to include paths
		PublicIncludePaths.AddRange(new string[] {
			"SpaceAce/Ships",
			"SpaceAce/Projectiles",
			"SpaceAce/Controllers",
			"SpaceAce/Data",
			"SpaceAce/Components",
			"SpaceAce/Subsystems",
			"SpaceAce/AIStates",
			"SpaceAce/Managers",
			"SpaceAce/UI",
			"SpaceAce/Audio",
			"SpaceAce/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
