using UnrealBuildTool;

public class WeaponAlignmentStudio : ModuleRules
{
	public WeaponAlignmentStudio(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// Plugin manager (IPluginManager::Get)
			"Projects",

			// Editor framework
			"UnrealEd",
			"EditorFramework",
			"EditorStyle",
			"LevelEditor",
			"ToolMenus",

			// Slate UI
			"Slate",
			"SlateCore",
			"ApplicationCore",   // FPlatformApplicationMisc::ClipboardCopy

			// Viewport / Preview Scene
			"AdvancedPreviewScene",
			"RenderCore",

			// Interactive Tools Framework (gizmos)
			"InteractiveToolsFramework",
			"EditorInteractiveToolsFramework",

			// Asset / package serialization
			"AssetTools",
			"AssetRegistry",

			// JSON (scriptable AlignmentAssetToJson for AIK/Claude)
			"Json",

			// Property editor (asset picker widget)
			"PropertyEditor",
		});
	}
}
