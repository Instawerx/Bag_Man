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

	/** Fires when the authoritative build set lands (OnRep) or changes server-side. OnRep_BuildSet has
	 *  always described itself as a "creator-UI notify" -- it simply had nothing to notify. */
	DECLARE_MULTICAST_DELEGATE(FOnAFLBuildSetChanged);
	FOnAFLBuildSetChanged OnBuildSetChanged;

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

	/**
	 * EQUIP BY CATALOG ID, LETTING THE SERVER CHOOSE THE SLOT. The client cannot name a slot here, and
	 * that is the point: an either-side wrist piece has no slot until the server looks at what is
	 * already worn, so a client-chosen slot would either be a guess or an authority the client does
	 * not have. ServerSetAccessory stays for the slot-known case; this is the wearable path.
	 *
	 * REFUSES, NEVER SUBSTITUTES. Unowned, unknown, slotless, a pendant with no chain, or a wrist piece
	 * with no open side are all refusals with a logged reason -- never a silent swap of something the
	 * player chose deliberately, which they could not undo because nothing would say what was dropped.
	 */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "AFL|Accessory")
	void ServerEquipWearable(FName CosmeticId);

	/**
	 * IS THIS SLOT'S CONTENT DRAWABLE RIGHT NOW? Distinct from "is it equipped": a pendant whose chain
	 * has been un-equipped is STILL HELD in its slot and simply stops rendering. Removing it instead
	 * would destroy a choice the player made in response to an unrelated one, and re-equipping the
	 * chain could not put it back.
	 */
	UFUNCTION(BlueprintPure, Category = "AFL|Accessory")
	bool IsAccessorySlotRenderable(EAFLAccessorySlot Slot) const;

	/**
	 * WHERE WOULD THIS ROW GO? Pure, authority-side resolution, factored out so the equip and any test
	 * or UI preview ask the SAME function rather than two implementations of one rule. Returns MAX and
	 * fills OutReason when the answer is "nowhere".
	 */
	EAFLAccessorySlot ResolveWearableSlot(const FAFLCatalogEntry& Row, FString& OutReason) const;

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

	// ===== THE SLOT LADDER -- server-authoritative, and the ONLY definition =================
	//
	// It previously lived on UAFLW_Creator. A widget computing the cap means the only answer to "how
	// many slots" was client-side, which is not a gate at all. The widget now delegates here.
	//
	// SlotTierCeiling (5) was declared alongside these and referenced NOWHERE -- League grants counted
	// slots like any other purchase, so a tier constant implied a mechanism that does not exist. Gone.

	/** Slots nobody has to buy. */
	static constexpr int32 SlotBaseline = 2;

	/** Ceiling regardless of how many were bought. */
	static constexpr int32 SlotHardCap = 10;

	/** The counted, account-durable entitlement a robot pack / slot SKU increments. */
	static const FName SlotEntitlementKey;

	/**
	 * How many builds this player may hold. SlotBaseline + counted AFL.CreatorSlot, clamped to
	 * SlotHardCap.
	 *
	 * FAILS CLOSED. No wallet means the purchased count cannot be verified, so it resolves to the
	 * BASELINE -- not to unlimited, and not to zero. This deliberately does NOT inherit the permissive
	 * degrade documented on GetEntitlementSource(): being generous with what a player can SEE is not the
	 * same as being generous with what they may SAVE.
	 */
	UFUNCTION(BlueprintPure, Category = "AFL|Creator|Slots")
	int32 GetEffectiveSlotCap() const;

	/** Builds that are NOT read-only -- the number the cap actually governs. A locked build is kept and
	 *  rendered but sits outside the cap, so counting it would compare against the wrong total. */
	UFUNCTION(BlueprintPure, Category = "AFL|Creator|Slots")
	int32 CountUnlockedBuilds() const;

	/** Builds held in total, locked included. Distinct from the slot count on purpose: this is the
	 *  STORAGE figure, and nothing bounds it today -- see the note on the save path. */
	UFUNCTION(BlueprintPure, Category = "AFL|Creator|Slots")
	int32 CountAllBuilds() const { return BuildSet.Builds.Num(); }

	// ===== SUBSCRIPTION STATE -> THE LAPSE RULE ===================================================
	//
	// ApplyLapseRule was complete and correct with a cheat command as its only caller. What was missing
	// was a runtime holder for the conditional set: it is persisted, but nothing could be ASKED whether
	// a condition is held right now.

	/** The condition the creator's continuum authoring hangs off. */
	static const FName LeagueConditionId;

	/**
	 * Record the authoritative state of a condition and re-apply the lapse rule.
	 *
	 * SERVER ONLY. Called by whatever establishes subscription truth (login, entitlement refresh, a
	 * store purchase completing) -- this component does not poll and does not guess.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Creator|Lapse")
	void SetConditionState(FName ConditionId, EAFLConditionState NewState);

	/**
	 * Pull the authoritative condition set from /conditional-entitlement and record it.
	 *
	 * ON FAILURE IT CHANGES NOTHING. Writing Unknown on a failed read would fail grants CLOSED and
	 * strip a subscriber's perks over a network blip; writing Lapsed would lock their builds, which is
	 * the exact defect AwaitingActivation exists to prevent, reintroduced through the back door. So a
	 * failure logs and returns, and whatever was last established stands.
	 *
	 * AUTHORITY ONLY. The set replicates to the owning client COND_OwnerOnly -- a client fetching its
	 * own conditions would be a second source of truth for whether it is a subscriber.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Cosmetics")
	void RefreshConditionalEntitlement();

	/** The state we last recorded. Unknown until something establishes it. */
	UFUNCTION(BlueprintPure, Category = "AFL|Creator|Lapse")
	EAFLConditionState GetConditionState(FName ConditionId) const;

	/**
	 * Re-apply the lapse rule from the recorded subscription state.
	 *
	 * ON Unknown THIS DOES NOTHING. A lapse is a PENALTY, and the conditional-entitlement contract makes
	 * penalties fail OPEN on Unknown. Calling ApplyLapseRule with a guessed cap would BE the penalty.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Creator|Lapse")
	void RefreshLapseFromSubscription();

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

	/** Recorded condition states. Keyed by ConditionId -- one condition confers many grants, so the
	 *  state belongs to the condition, not to each grant. Absent = Unknown, never = Lapsed.
	 *
	 *  REPLICATED COND_OwnerOnly. It was UPROPERTY() with no replication at all, so a client could
	 *  never see its own subscription state -- the server knew, and the player's own UI could not.
	 *  Owner-only because whether someone subscribes is theirs: every other client has no use for it
	 *  and no business with it. */
	UPROPERTY(Replicated)
	TArray<FAFLConditionStateEntry> ConditionStates;

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

	/** The same wallet, CONCRETE -- counted entitlements are not on the IAFLEntitlementSource interface. */
	class UAFLWalletComponent* GetWalletComponent() const;

	/** Resolve the persistence backend (stub now). May be null in early bring-up -> persistence no-ops. */
	IAFLCosmeticPersistence* GetPersistence() const;

	/** Derive the opaque player key for persistence from the PlayerState's net-id (stub backing). */
	FAFLPlayerId MakePlayerId() const;

	/** After an accepted authority-side change while already possessed: re-run the proven controller
	 *  push so a pre-match live edit shows immediately (same idempotent path the #38a part-arrival hook
	 *  uses; no respawn). In-match this is unreachable because step 2 rejects the mutation. */
	void NudgeControllerReapply() const;

	/**
	 * CC-8: re-read the pendant selection on every chain actor currently worn by this player's pawn.
	 *
	 * WHY THIS IS NOT PART OF NudgeControllerReapply'S CONTROLLER BLOCK. That block needs
	 * GetOwningController(), and an OBSERVED player's PlayerState has no owning controller in a remote
	 * client's world -- it returns null and the whole function early-returns. The chain actors are still
	 * there (they arrive with Lyra's replicated character-part list), so on a client every other player's
	 * pendant depended on nothing at all. This walks PlayerState -> Pawn -> attached actors instead, so it
	 * works for any pawn in any world, authority or not.
	 *
	 * WHY IT IS NEEDED AT ALL, given the chain reads its pendant in BeginPlay: BeginPlay only covers the
	 * ordering where the selection is already known when the chain spawns. The other ordering -- chain
	 * first, pendant value replicates after -- had no path, and left the pendant permanently absent. The
	 * two together cover both orders, which is why this is not a retry: each call site answers one
	 * ordering exactly once.
	 *
	 * It is also what stops the SERVER path being incidental. Today the pendant updates on authority only
	 * because the accessory consumer removes and re-adds all three pawn parts on every refresh, which
	 * destroys and respawns the chain. Anything that turned that into a diff would silently stop updating
	 * pendants; after this, the re-drive is explicit and does not care.
	 */
	void RedriveAccessoryChains() const;

	/**
	 * IRONICS IS ALWAYS ASSIGNED FIRST -- the standing ruling, applied to a player who has no identity.
	 *
	 * Measured before writing: nothing assigned one. The selection struct defaults both TeamId and
	 * CharacterId to NAME_None and the load path only copies a stored selection in when one is FOUND,
	 * so a new account held no identity at all -- and ServerSetCosmeticSelection refuses a selection
	 * whose identity is NAME_None, making a failed guard the first thing a new account could do.
	 *
	 * Sets BOTH axes. IdentityType chooses which of TeamId/CharacterId is live; setting only the
	 * currently-selected one would leave the other empty and put the player back in the same state one
	 * axis-switch later.
	 *
	 * IDEMPOTENT: writes only when the identity is unset, so it can never overwrite a real choice.
	 * Returns true if it changed anything, so the caller can decide whether to persist.
	 */
	bool ApplyDefaultIdentityIfUnset();

public:
	/**
	 * Tell the OWNING CLIENT why an equip was refused. Every refusal in ServerEquipWearable was a
	 * server-side UE_LOG; on a dedicated server the player's machine never saw one, so a refused item
	 * simply did not appear and nothing anywhere said why.
	 *
	 * The reason is STORED as well as logged. A value that only passes through a log cannot be read by
	 * UI, cannot be asserted by a proof, and cannot be acted on by anything -- which is what made
	 * "REFUSED, readable reason" unprovable as an arm.
	 */
	UFUNCTION(Client, Reliable)
	void ClientWearableRefused(FName CosmeticId, const FString& Reason);

	/** The last refusal this client was told about. Read by UI and by the render proof. */
	UFUNCTION(BlueprintPure, Category = "AFL|Cosmetics")
	FString GetLastRefusalReason() const { return LastRefusalReason; }

	/** Which id was refused, so a stale reason cannot be misread as a fresh one. */
	UFUNCTION(BlueprintPure, Category = "AFL|Cosmetics")
	FName GetLastRefusalId() const { return LastRefusalId; }

private:
	UPROPERTY(Transient)
	FString LastRefusalReason;

	UPROPERTY(Transient)
	FName LastRefusalId;
};
