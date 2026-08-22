// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/PlayerStateComponent.h"

#include "Cosmetics/AFLCosmeticSelectionTypes.h"
#include "Cosmetics/AFLCosmeticTypes.h"   // FAFLColorOverride (CC-2.1: BuildColorOverride return type)
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

	/** CC-7: push the equipped stickers onto the owner's character parts.
	 *  DIRECT, not via the skin-colour reapply: OnRep_Selection nudges the CONTROLLER component, and a
	 *  sticker change is not a skin change, so relying on that path is relying on a side effect. */
	void RefreshStickers() const;

	/** CC-2.1: the ONE construction of the creator colour overlay (invalid unless bUseCreatorColors). Shared by the
	 *  server push (RefreshSkinForPawn, step 5) and the client OnRep_Selection populate (step 6) -- two copies drift. */
	static FAFLColorOverride BuildColorOverride(const FAFLCosmeticSelection& Sel);

	/**
	 * The ONE server-authoritative write path. The wallet UI AND the dev cheat both call THIS -- the
	 * cheat thus exercises the genuine route, not a fake. Flow (authority):
	 *   1. _Validate: cheap structural sanity (identity discriminator + matching id non-None).
	 *   2. Change-timing gate (D6): IsSelectionEditable() -- STUB-OPEN for #43 (always true), the real
	 *      bSelectionLocked match-start signal wires in when the hub<->match boundary lands. The gate is
	 *      CALLED now so the call site is proven; only the policy fills in later.
	 *   3. Entitlement gate (per axis + identity): unentitled ids are dropped, the rest still apply.
	 *      Real owned-set impl now (UAFLWalletComponent IS the IAFLEntitlementSource -- S-ECON-WALLET): owned +
	 *      GrantedFree apply. Was a permissive stub during #43 bring-up -- same interface, no re-architecture.
	 *   4. Commit -> replicated UPROPERTY -> OnRep on clients; persist through the stub interface.
	 *   5. If already possessed (pre-match live change), re-run the proven controller push (no respawn).
	 */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, BlueprintAuthorityOnly, Category = "AFL|Cosmetics")
	void ServerSetCosmeticSelection(FAFLCosmeticSelection Requested);

	// --- CC-3.2 SAVED BUILDS -------------------------------------------------------------------
	// A build is an AUTHORING layer ABOVE the proven seam, never a second thing gameplay reads.
	// The server resolves the active build INTO Selection and the existing commit path then runs
	// unchanged, so replication, OnRep, persistence and every CC-1/CC-2 proof keep their meaning.
	// GetSelection() is untouched DELIBERATELY: routing gameplay through a build-aware accessor
	// would invalidate the read-site shape this programme has spent its whole length protecting.

	UFUNCTION(BlueprintPure, Category = "AFL|Creator")
	const FAFLCreatorBuildSet& GetBuildSet() const { return BuildSet; }

	/** CC-5.1: which creator channels this pawn's chassis actually renders. Derived by asking the
	 *  bound slot-1 master, so a rewired or new master is handled without editing code. */
	UFUNCTION(BlueprintPure, Category = "AFL|Creator")
	FAFLCreatorChannelSchema GetChannelSchemaForPawn(APawn* Pawn) const;

	/** VALIDATE ONCE, AT SAVE. The payload is client-supplied, so continuum channels are gamut-
	 *  clamped HERE and the clamped value is what is stored. Activation never re-clamps, which is
	 *  what lets CC-4.2 promise a saved build renders identically forever. Invalid Index appends. */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, BlueprintAuthorityOnly, Category = "AFL|Creator")
	void ServerSaveBuild(FAFLCreatorBuild Build, int32 Index);

	// --- CC-5.4 BUILD NAMING -------------------------------------------------------------------
	/** STRUCTURAL validation only -- length, control characters, whitespace, and uniqueness within
	 *  this player's own set. Policy (which words are allowed) is deliberately NOT decided here.
	 *  OutSanitised receives the trimmed/collapsed form actually worth storing. */
	static EAFLNameVerdict ValidateBuildName(const FString& Raw, const TArray<FAFLCreatorBuild>& Existing,
		int32 IgnoreIndex, FString& OutSanitised);

	/** Report a build name for review. Sets Rejected immediately -- an unreviewed report should hide
	 *  the name rather than leave it visible while a queue drains. Reversible by policy, not here. */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "AFL|Creator")
	void ServerReportBuildName(int32 Index);

	/** Activate a saved build BY INDEX into the server's own BuildSet -- never from a client
	 *  payload, so there is nothing to re-validate and nothing a client can smuggle in. */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, BlueprintAuthorityOnly, Category = "AFL|Creator")
	void ServerSetActiveBuild(int32 Index);

	// --- CC-7.2 STICKERS -----------------------------------------------------------------------
	/**
	 * Place one sticker in one zone. THE CLIENT NEVER DECIDES A FINAL POSITION.
	 *
	 * The server re-clamps through AFLStickerBounds on arrival, exactly as the colour path re-clamps
	 * through AFLCreatorGamut: a UI that clamps on drag is a courtesy to the player, not a guarantee,
	 * and a packet editor is not obliged to be courteous. Position is normalised ZONE space, so a
	 * clamped placement cannot cross a seam by construction.
	 */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "AFL|Sticker")
	void ServerSetStickerPlacement(EAFLStickerZone Zone, FAFLStickerPlacement Placement);

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "AFL|Sticker")
	void ServerClearStickerZone(EAFLStickerZone Zone);

	// --- CC-8 ACCESSORIES ----------------------------------------------------------------------
	/** Equip an accessory in a slot. Server-authoritative and FAILS CLOSED on an unmapped slot. */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "AFL|Accessory")
	void ServerSetAccessory(EAFLAccessorySlot Slot, FName AccessoryId);

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "AFL|Accessory")
	void ServerClearAccessory(EAFLAccessorySlot Slot);

	// --- CC-4.2 LAPSE RULE ---------------------------------------------------------------------
	// FREEZE, NEVER MUTATE. What a lapse takes away is the ABILITY TO CHANGE, never the work already
	// done. Applied colours stay applied (ResolveInto reads values frozen at save and never
	// recomputes), builds beyond the effective cap go READ-ONLY rather than being deleted, and
	// anything PURCHASED outright is untouched -- a counted entitlement has no condition to lapse.
	//
	// AUTHORITY-ONLY and IDEMPOTENT: re-running with the same inputs must not accumulate state, so a
	// repeated entitlement refresh cannot progressively lock a player out.
	//
	// THE CAP IS A PARAMETER, NOT A CONSTANT. How many slots a subscription or a purchase confers is
	// product intent (the pricing SSOT still flags the $3 robot-vs-slot collision as unresolved).
	// This applies whatever cap it is handed; it does not choose one.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "AFL|Creator")
	void ApplyLapseRule(int32 EffectiveSlotCap, bool bContinuumEditingHeld);

	/** True when creator EDITING is locked by a lapse. Applied colours are unaffected -- this gates
	 *  authoring only. Replicated so the UI can show the lock rather than silently rejecting input. */
	UFUNCTION(BlueprintPure, Category = "AFL|Creator")
	bool IsContinuumEditingLocked() const { return bContinuumEditingLocked; }

	// --- CC-3.5 BUILD PERSISTENCE --------------------------------------------------------------
	// The set is serialised WHOLE, as one JSON blob, matching how the backend stores it and how this
	// component replicates it. Serialising per-build would let a partial push leave the remote set
	// disagreeing with the local one, with nothing to detect the divergence.

	/** Push the current BuildSet to the persistence seam. Authority-only; called after any mutation. */
	void PushBuildsToPersistence();

	/** Pull the saved BuildSet from the persistence seam and adopt it. Authority-only. */
	void PullBuildsFromPersistence();

private:
	FAFLPlayerId MakePlayerKey() const;
	FString ResolvePlayFabIdForOwner() const;
	/** Strictly increasing per push. Guards against two in-flight saves landing out of order. */
	int32 BuildsRevision = 0;
public:

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

	//~UPlayerStateComponent
	/** RESPAWN DURABILITY: copy the cosmetic selection across Lyra's inactive-PlayerState swap. On death-
	 *  respawn AModularPlayerState::CopyProperties iterates its UPlayerStateComponents and calls this hook
	 *  (old comp -> new comp, matched by class+name) so each component carries its must-survive state to the
	 *  new PlayerState. Without it the selection (which lives only on the PS) is dropped on the swap and the
	 *  body/color resolvers read the new PS as empty -> ARIA fallback (the Phase-1 bug). Copying the WHOLE
	 *  FAFLCosmeticSelection makes BOTH the Character and Team axes (and every cosmetic axis) respawn-durable
	 *  in one stroke. Respawn-durability is a DESIGNED property of the identity system, not incidental. */
	virtual void CopyProperties(UPlayerStateComponent* TargetPlayerStateComponent) override;
	//~End

	/** The replicated selection. Single OnRep -- selection changes are menu-rare, not per-tick. */
	/** CC-3.2: the player's saved builds. Replicated for the creator UI; gameplay never reads it --
	 *  it reads Selection, which the server resolves from the active build. */
	UPROPERTY(ReplicatedUsing = OnRep_BuildSet)
	FAFLCreatorBuildSet BuildSet;

	/** CC-4.2: creator authoring locked by a lapse. Owner-only -- no other client needs it. */
	UPROPERTY(Replicated)
	bool bContinuumEditingLocked = false;

	UFUNCTION()
	void OnRep_BuildSet();

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

	/** Resolve the entitlement source (the real UAFLWalletComponent owned-set impl -- S-ECON-WALLET). Null-tolerant:
	 *  a missing source means basics are owned (the call sites short-circuit to allowed). */
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
