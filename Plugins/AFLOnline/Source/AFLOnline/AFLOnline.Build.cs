// Copyright C12 AI Gaming. All Rights Reserved.

using UnrealBuildTool;

public class AFLOnline : ModuleRules
{
	public AFLOnline(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				// The public header exposes FHttpRequestPtr/FHttpResponsePtr in a callback signature.
				"HTTP",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// FJsonObject / FJsonSerializer -- PlayFab REST request/response bodies.
				"Json",
			}
		);
	}
}
