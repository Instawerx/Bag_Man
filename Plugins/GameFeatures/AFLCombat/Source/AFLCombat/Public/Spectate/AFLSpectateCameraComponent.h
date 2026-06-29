// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Templates/SubclassOf.h"

#include "AFLSpectateCameraComponent.generated.h"

class APawn;
class APlayerController;
class ULyraCameraMode;
class ULyraHealthComponent;

/**
 * UAFLSpectateCameraComponent  (Surface 3 LAYER 2a -- auto-follow-on-death spectate camera, THIRD-PERSON)
 *
 * Added to the PlayerController via the experience GameFeatureAction_AddComponents (client + server). On a
 * mid-round death (the tactical out-until-round-reset model -- respawn suppressed, the ragdoll stays possessed)
 * it retargets the dead player's camera to a LIVING TEAMMATE in THIRD-PERSON (the teammate's normal gameplay POV).
 * COSMETIC retarget ONLY -- no Possess/UnPossess/Reset; the proven respawn architecture is untouched.
 *
 * SERVER: binds the possessed pawn's OnDeathStarted (re-bound across respawns); on death in an active round it
 * enumerates living teammates (LyraTeamSubsystem + PlayerArray + IsDeadOrDying, teammates-only) and Client-RPCs
 * the owning client. The view target is transient/not-replicated, so a Client RPC is the canonical cross-the-wire
 * path (mirrors APlayerController::ServerViewSelf -> ClientSetViewTarget).
 *
 * CLIENT -- the two camera problems, both resolved from Lyra source:
 *  (1) bAutoManageActiveCameraTarget re-asserts our own (ragdoll) pawn over SetViewTarget ("stuck on own pawn")
 *      -> disable it on follow, restore on respawn (Client_StopFollow).
 *  (2) a spectated PROXY teammate's ULyraCameraComponent has NO bound DetermineCameraModeDelegate (the hero
 *      component binds it only on the locally-controlled pawn) -> empty mode stack -> origin, NOT third-person.
 *      FIX: bind that proxy camera's DetermineCameraModeDelegate ourselves to the teammate's
 *      PawnData->DefaultCameraMode (the SAME mode ULyraHeroComponent::DetermineCameraMode returns in live play)
 *      and SetActive(true) so base APawn::CalcCamera evaluates it -> renders the teammate's normal third-person POV.
 *
 * 2-client-PROVEN (natural death-follow + respawn-return): FOLLOW + settled-on-teammate + STOP-back-to-own-pawn.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLSpectateCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLSpectateCameraComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Blend time (s) for the retarget to a teammate (SetViewTargetWithBlend, cubic). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Spectate")
	float FollowBlendTime = 0.4f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	APlayerController* GetPC() const;

	// -- server: death detection, re-bound across respawns (the round-reset force-restart replaces the pawn) --
	void ServerBindToPawnDeath();
	UFUNCTION() void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);
	UFUNCTION() void HandleOwnerDeath(AActor* OwningActor);

	// -- server: pick a LIVING TEAMMATE (LyraTeamSubsystem + PlayerArray + IsDeadOrDying enumeration) --
	APawn* FindFirstLivingTeammatePawn() const;

	// -- the cross-the-wire retarget: server picks, the OWNING CLIENT runs the camera change --
	UFUNCTION(Client, Reliable) void Client_FollowTarget(AActor* Target);
	UFUNCTION(Client, Reliable) void Client_StopFollow();

	// -- client: drive the proxy teammate's camera to its normal third-person mode --
	TSubclassOf<ULyraCameraMode> GetSpectatedCameraMode() const;   // the proxy camera's DetermineCameraModeDelegate target
	void ClearSpectateCameraMode();                                // unbind the followed proxy's camera-mode delegate

	TWeakObjectPtr<ULyraHealthComponent> BoundHealth;   // server: the currently-bound pawn's health comp
	TWeakObjectPtr<APawn> FollowedPawn;                 // client: the teammate we are viewing (drives GetSpectatedCameraMode)
	bool bFollowing = false;                            // server: retargeted this death; cleared on re-possess
};
