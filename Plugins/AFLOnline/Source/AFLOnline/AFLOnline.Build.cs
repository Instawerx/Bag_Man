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

		// A1.3b earn signer: OpenSSL HMAC-SHA256 for the server-authoritative /earn request signature. Mirror
		// PlatformCryptoContext's mechanism -- a module-private define gates the <openssl/*> include so the signer
		// compiles clean on every platform and only RUNS under IsRunningDedicatedServer() (the KEY is env-only,
		// never cooked). The angle-bracket openssl headers come from the OpenSSL ThirdParty dependency.
		PrivateDefinitions.Add("AFLONLINE_USE_OPENSSL=1");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
	}
}
