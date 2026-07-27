// Copyright 2025, BlueprintsLab, All rights reserved

using UnrealBuildTool;

public class UltimateDifficultyScalingEditor : ModuleRules
{
    public UltimateDifficultyScalingEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "UltimateDifficultyScaling" 
            }
        );
			
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Projects",
                "InputCore",
                "EditorFramework",
                "UnrealEd",
                "ToolMenus",
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "AssetRegistry",
                "Kismet",
                "BlueprintGraph",
                "EditorStyle",
                // --- Web UI (HTML/CSS/JS) host + C++/JS bridge ---
                "WebBrowser",
                "WebBrowserWidget",
                "HTTP",
                "Json",
                "JsonUtilities",
                "UMG",
                "UMGEditor",
                "ContentBrowser",
                "AssetTools",
                "DesktopPlatform",
                "ApplicationCore",
            }
        );
    }
}