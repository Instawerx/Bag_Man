// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class LyraEditorTarget : TargetRules
{
	public LyraEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;

		ExtraModuleNames.AddRange(new string[] { "LyraGame", "LyraEditor" });

		if (!bBuildAllModules)
		{
			// AFL: was PointerMemberBehavior.Disallow. UBT applies this SINGLE value to all three UHT
			// categories at once (UEBuildTarget.cs ~2569: Engine, EnginePlugin AND NonEngine all get the
			// same override), so there is no way to keep our modules strict while tolerating third-party
			// engine plugins. The installed marketplace plugin CloudsLighting declares raw UObject*
			// UPROPERTY members, which failed the entire LyraEditor header parse ("Total of 0 written")
			// before any AFL code was even compiled -- and its headers live under the installed engine,
			// so we cannot fix them. Relaxed to unblock. Note UHT's own ini default is AllowSilently;
			// this override is the only thing that made it strict, and a project-side
			// Config/DefaultEngine.ini setting CANNOT undo it (the -ini: command-line override wins).
			// AFL modules should still use TObjectPtr by convention -- that is now a review rule rather
			// than a compile-time one. Restore Disallow if CloudsLighting is ever removed/disabled.
			NativePointerMemberBehaviorOverride = PointerMemberBehavior.AllowSilently;
		}

		LyraGameTarget.ApplySharedLyraTargetSettings(this);

		// This is used for touch screen development along with the "Unreal Remote 2" app
		EnablePlugins.Add("RemoteSession");
	}
}
