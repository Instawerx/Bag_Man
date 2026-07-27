// Copyright 2025, BlueprintsLab, All rights reserved

using UnrealBuildTool;

public class UltimateDifficultyScaling : ModuleRules
{
    public UltimateDifficultyScaling(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        
        // This is not strictly needed for this setup but is good practice
        PublicIncludePaths.AddRange(new string[] { });
        PrivateIncludePaths.AddRange(new string[] { });

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
            }
        );
    }
}