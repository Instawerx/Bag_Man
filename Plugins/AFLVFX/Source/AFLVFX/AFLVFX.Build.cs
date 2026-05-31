// Copyright C12 AI Gaming. AFL / BAG MAN.
using UnrealBuildTool;

public class AFLVFX : ModuleRules
{
	public AFLVFX(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",   // AGameplayCueNotify_Actor, FGameplayCueParameters
			"GameplayTags",
			"GameplayTasks",
			"Niagara"              // UNiagaraComponent, UNiagaraFunctionLibrary, UNiagaraSystem
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
