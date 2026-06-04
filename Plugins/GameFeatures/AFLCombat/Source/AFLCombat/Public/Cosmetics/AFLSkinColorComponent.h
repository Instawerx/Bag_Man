// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "AFLSkinColorComponent.generated.h"

class UAFLSkinColorAsset;

// Skin-color race diagnostics (cvar-gated `afl.SkinDiag`, OFF by default). Shared by the part actor,
// the pawn component, and the controller component so all log lines share one category + format. Defined
// in AFLSkinColorComponent.cpp. This is a PERMANENT diagnostic, not temp scaffolding -- it is inert (zero
// log output) unless `afl.SkinDiag 1` is set, so it does not affect shipping or normal PIE.
AFLCOMBAT_API DECLARE_LOG_CATEGORY_EXTERN(LogAFLSkinDiag, Log, All);

namespace AFLSkinDiag
{
	/** True when `afl.SkinDiag` > 0 (game thread). Gate every diagnostic block on this. */
	AFLCOMBAT_API bool IsOn();

	/** Shared line prefix: "[SkinDiag][SRV|CLI][f=<GFrameCounter>] " resolved from WorldContext's net mode. */
	AFLCOMBAT_API FString Prefix(const UObject* WorldContext);
}

/**
 * Pawn-side replicated COLOR selection for the robot skin (L5, Option F composition).
 *
 * Holds the replicated SkinColor and applies it to the pawn's AAFLCharacterPartActor body parts via
 * direct engine material calls (no unexported Lyra symbol, no reflection). Standalone UActorComponent --
 * we do NOT subclass the module-private ULyraPawnComponent_CharacterParts; we observe its result (the
 * spawned part actors) by class-filtered iteration.
 *
 * Race-safety (two channels: the parts FastArray on the stock component vs this SkinColor UPROPERTY):
 *  - PATH 1 (part-arrives-second): each AAFLCharacterPartActor self-colors on its BeginPlay (reads GetSkinColor()).
 *  - PATH 2 (color-arrives-second): OnRep_SkinColor -> ReapplyColorToAllParts() pushes to already-spawned parts
 *    that read null at their BeginPlay. LOAD-BEARING -- without it, color-after-part desyncs on the wire.
 *  Both idempotent (the part's owned-MID create-once) -> safe to fire redundantly when both land close together.
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLSkinColorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLSkinColorComponent();

	/** AUTHORITY-ONLY: set the active color on the server. Replicates to all clients. */
	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "AFL|Cosmetics")
	void SetSkinColor(UAFLSkinColorAsset* NewColor);

	/** Read by the part actor on its BeginPlay (PATH 1). */
	UAFLSkinColorAsset* GetSkinColor() const { return SkinColor; }

protected:
	//~UActorComponent interface
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End of UActorComponent interface

	UPROPERTY(ReplicatedUsing = OnRep_SkinColor)
	TObjectPtr<UAFLSkinColorAsset> SkinColor = nullptr;

	UFUNCTION()
	void OnRep_SkinColor();

	/** PATH 2: push the current color to all already-spawned AAFLCharacterPartActor body parts. Idempotent. */
	void ReapplyColorToAllParts();
};
