// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class LyraGameEOSTarget : LyraGameTarget
{
	public LyraGameEOSTarget(TargetInfo Target) : base(Target)
	{
		CustomConfig = "EOS";
		bOverrideBuildEnvironment = true;   // allow CustomConfig override on the installed Launcher engine (UBT-canonical; Lyra base target already written for the Unique env)
	}
}
