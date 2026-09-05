// Copyright C12 AI Gaming. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

// IRONICS -- the player-facing Shipping client target. Identical to LyraGameEOSTarget (Game + EOS
// custom config), it exists only to brand the staged executable IRONICS.exe instead of LyraGameEOS.exe.
// CustomConfig="EOS" pulls Config/Custom/EOS/ by config-NAME, so this target gets the same EOS overlay;
// nothing at runtime keys off the target name. Cook/package with -target=IRONICS.
public class IRONICSTarget : LyraGameTarget
{
	public IRONICSTarget(TargetInfo Target) : base(Target)
	{
		CustomConfig = "EOS";
		bOverrideBuildEnvironment = true;   // allow CustomConfig override (UBT-canonical; matches LyraGameEOS)
	}
}
