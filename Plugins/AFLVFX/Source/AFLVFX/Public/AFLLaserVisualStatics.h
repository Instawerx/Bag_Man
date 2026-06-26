// Copyright C12 AI Gaming. AFL / BAG MAN.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "AFLLaserVisualStatics.generated.h"

class UNiagaraComponent;

/**
 * Cue-side laser tint helper.
 *
 * Reads a weapon's IAFLLaserVisualProvider::GetBeamColor and, when A>0, drives a spawned Niagara
 * component's User.Color param. This is the EXACT tint block AAFLCueNotify_LaserBeamFlash runs inline
 * -- exposed BlueprintCallable so the CONTENT pulse cues (GCN_AFL_Pulse_Fire / GCN_AFL_Pulse_Tracer,
 * which spawn NS_AFL_Pulse_* in their notify graphs) call it right after spawning their Niagara, with
 * the cue SourceObject as the Provider. One helper => pulse and beam tint by the identical contract,
 * with no per-cue colour logic duplicated.
 */
UCLASS()
class AFLVFX_API UAFLLaserVisualStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * If Provider implements IAFLLaserVisualProvider and its GetBeamColor has A>0, set NiagaraComp's
	 * ColorParam (default "User.Color") to that colour. A==0 (the sentinel default) or a non-provider
	 * leaves the NS at its authored colour -- so existing weapons are unchanged.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Laser")
	static void DriveLaserTint(UNiagaraComponent* NiagaraComp, const UObject* Provider, FName ColorParam = FName("User.Color"));

	/**
	 * Reflection-read the provider's LaserTintColor (the unified FX tint). Returns the value if the
	 * provider has an FLinearColor "LaserTintColor" property; otherwise (0,0,0,0) -- a SENTINEL whose
	 * A<=0 means "no override, keep the default" (the beam's green BeamColorOverride / the cue's
	 * as-authored NS). Replaces IAFLLaserVisualProvider::Execute_GetBeamColor, whose bridge-wired BP
	 * override does NOT dispatch at runtime (proven: HOP4 returned the C++ default (0,0,0,0)). The
	 * cosmetic resolver/cheat SETS LaserTintColor by this same reflection; this READS it symmetrically.
	 */
	static FLinearColor ReadLaserTint(const UObject* Provider);
};
