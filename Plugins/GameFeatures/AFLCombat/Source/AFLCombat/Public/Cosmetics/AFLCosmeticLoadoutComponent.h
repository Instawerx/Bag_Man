// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/PlayerStateComponent.h"

#include "Cosmetics/AFLCosmeticSelectionTypes.h"
// FAFLPlayerId (returned BY VALUE from MakePlayerId below) + the two seam interfaces are defined here,
// so the header needs the full include, not a forward declaration.
#include "Cosmetics/AFLCosmeticServices.h"

#include "AFLCosmeticLoadoutComponent.generated.h"

class ALyraPlayerState;
class APlayerState;
class APawn;

/**
 * UAFLCosmeticLoadoutComponent -- the server-authoritative cosmetic SELECTION home (#43).
 *
 * Lives on the PlayerState (attached via GameFeatureAction_AddComponents; we do NOT subclass the
 * shared LyraGame ALyraPlayerState). The PlayerState is the right home: it already holds player
 * identity (team / ASC / PawnData) and -- crucially -- replicates to EVERY client, which an
 * owner-only ControllerComponent cannot. Store preview, nameplates, and lobby visibility all need the
 * selection readable on remote clients, so the WHAT (the chosen selection) lives here. The proven
 * UAFLSkinColorControllerComponent stays unchanged and reads this at spawn to drive the HOW (the
 * SetSkinColor / equip push).
 *
 * The selection is plain replicated state (a single ReplicatedUsing UPROPERTY), NOT a GAS struct --
 * see FAFLCosmeticSelection. Both the wallet UI and the dev cheat mutate it through the ONE server
 * RPC below; there is no second write path.
 *
 * Replication idiom mirrors UAFLSkinColorComponent (the proven Race A/B/C component): replication is
 * explicitly enabled in the ctor (no replicated base), DOREPLIFETIME on the value, authority-guarded
 * setter that also applies locally on the listen-host (OnRep does not fire on authority), OnRep for
 * remote clients.
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLCosmeticLoadoutComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	// UPlayerStateComponent has no default ctor (its only ctor takes FObjectInitializer), so we must
	// declare the FObjectInitializer overload and forward to Super.
	UAFLCosmeticLoadoutComponent(const FObjectInitializer& ObjectInitializer);

	/** Read the current replicated selection (any client; remote-visible). */
	UFUNCTION(BlueprintPure, Category = "AFL|Cosmetics")
	const FAFLCosmeticSelection& GetSelection() const { return Selection; }

	/**
	 * The ONE server-authoritative write path. The wallet UI AND the dev cheat both call THIS -- the
	 * cheat thus exercises the genuine route, not a fake. Flow (authority):
	 *   1. _Validate: cheap structural sanity (identity discriminator + matching id non-None).
	 *   2. Change-timing gate (D6): IsSelectionEditable() -- STUB-OPEN for #43 (always true), the real
	 *      bSelectionLocked match-start signal wires in when the hub<->match boundary lands. The gate is
	 *      CALLED now so the call site is proven; only the policy fills in later.
	 *   3. Entitlement gate (per axis + identity): unentitled ids are dropped, the rest still apply.
	 *      Permissive impl now; owned-set impl later -- same interface, no re-architecture.
	 *   4. Commit -> replicated UPROPERTY -> OnRep on clients; persist through the stub interface.
	 *   5. If already possessed (pre-match live change), re-run the proven controller push (no respawn).
	 */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, BlueprintAuthorityOnly, Category = "AFL|Cosmetics")
	void ServerSetCosmeticSelection(FAFLCosmeticSelection Requested);

	/**
	 * Change-timing gate (D6). STUB-OPEN for #43: returns true (always editable) because the match<->hub
	 * boundary that would set the lock isn't built yet -- wiring a lock now would build ahead of its
	 * dependency. The real implementation reads bSelectionLocked (set authoritatively at match-start)
	 * when P-MATCH/P-HUB lands. Authority-side; the call site is live from day one.
	 */
	bool IsSelectionEditable() const;

protected:
	//~UActorComponent
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End

	/** The replicated selection. Single OnRep -- selection changes are menu-rare, not per-tick. */
	UPROPERTY(ReplicatedUsing = OnRep_Selection)
	FAFLCosmeticSelection Selection;

	UFUNCTION()
	void OnRep_Selection();

	/** Bound to the PlayerState's OnPawnSet (AddDynamic -> must be UFUNCTION). Re-drives the proven
	 *  controller push for the new pawn on (re)possession -- the respawn-race fix's pawn half (#43). */
	UFUNCTION()
	void OnPlayerStatePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn);

	/**
	 * The real lock flag the change-timing gate WILL read once match-start exists (D6). Replicated so
	 * clients can grey out the wallet UI in-match. Stays false for #43 (nothing sets it yet) -> the gate
	 * is effectively open, exactly as ruled. Documented seam, not dead code: its consumer is
	 * IsSelectionEditable(), already wired.
	 */
	UPROPERTY(Replicated)
	bool bSelectionLocked = false;

private:
	/** Owning PlayerState (typed convenience; null-safe). */
	ALyraPlayerState* GetLyraPlayerState() const;

	/** Resolve the entitlement source (permissive impl now). Null-tolerant: a missing source means
	 *  basics are owned (the call sites short-circuit to allowed). */
	IAFLEntitlementSource* GetEntitlementSource() const;

	/** Resolve the persistence backend (stub now). May be null in early bring-up -> persistence no-ops. */
	IAFLCosmeticPersistence* GetPersistence() const;

	/** Derive the opaque player key for persistence from the PlayerState's net-id (stub backing). */
	FAFLPlayerId MakePlayerId() const;

	/** After an accepted authority-side change while already possessed: re-run the proven controller
	 *  push so a pre-match live edit shows immediately (same idempotent path the #38a part-arrival hook
	 *  uses; no respawn). In-match this is unreachable because step 2 rejects the mutation. */
	void NudgeControllerReapply() const;
};
