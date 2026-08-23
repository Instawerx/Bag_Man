// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLCosmeticLoadoutComponent.h"
#include "GameFramework/Character.h"      // CC-X34 socket-existence guard
#include "Components/SkeletalMeshComponent.h"

#include "Cosmetics/AFLCosmeticServices.h"
#include "Cosmetics/AFLEconomyPersistenceSubsystem.h"  // Phase A0: local SaveGame persistence -- the GetPersistence() swap point
#include "AFLOnlineSubsystem.h"                         // A1.1: PlayFabId = the durable account key for MakePlayerId
#include "Cosmetics/AFLWalletComponent.h"             // S-ECON-WALLET: the real IAFLEntitlementSource (layer b)
#include "AFLCombat.h"
#include "Cosmetics/AFLCharacterPartActor.h"           // CC-5.1: slot-1 master lookup
#include "Components/MeshComponent.h"
#include "Components/ChildActorComponent.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/Pawn.h"
#include "Cosmetics/AFLEconomyPersistenceSubsystem.h"   // CC-3.5 build blob load/save
#include "Cosmetics/AFLPlayerIdentityComponent.h"       // GetResolvedPlayFabId (A1.4 verified id)
#include "JsonObjectConverter.h"                        // BuildSet <-> JSON blob
#include "Engine/GameInstance.h"                                   // LogAFLCombat (the AFL_TEST emit category)
#include "Cosmetics/AFLSkinColorComponent.h"
#include "AFLCosmeticCatalogSubsystem.h"          // AFLSkinDiag (shared cvar-gated diag: LogAFLSkinDiag / IsOn / Prefix)
#include "Cosmetics/AFLSkinColorControllerComponent.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Player/LyraPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCosmeticLoadoutComponent)

UAFLCosmeticLoadoutComponent::UAFLCosmeticLoadoutComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// UGameFrameworkComponent -> UActorComponent: no replicated base, so WE enable replication or the
	// Selection UPROPERTY never reaches clients (the "compiles but doesn't replicate" trap the skin
	// component documents). Selection changes are menu-rare -> no tick.
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UAFLCosmeticLoadoutComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAFLCosmeticLoadoutComponent, Selection);
	DOREPLIFETIME(UAFLCosmeticLoadoutComponent, bSelectionLocked);
	// OWNER-ONLY. A build set is the player's own creator authoring data -- no other client renders
	// from it (they render from the resolved Selection, which stays broadcast). Measured in the CC-3
	// proof: client B read builds=0 from its OWN component, correctly, which is exactly why sending
	// A's set to B is bandwidth for data nobody reads.
	DOREPLIFETIME_CONDITION(UAFLCosmeticLoadoutComponent, BuildSet, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UAFLCosmeticLoadoutComponent, bContinuumEditingLocked, COND_OwnerOnly);
}

void UAFLCosmeticLoadoutComponent::CopyProperties(UPlayerStateComponent* TargetPlayerStateComponent)
{
	Super::CopyProperties(TargetPlayerStateComponent);

	// RESPAWN DURABILITY (Phase 1): carry the cosmetic selection across Lyra's inactive-PlayerState swap.
	// AModularPlayerState::CopyProperties(newPS) finds the matching component on the new PS and calls this on
	// the OLD component with Target = the NEW one. Copy the ENTIRE FAFLCosmeticSelection so EVERY axis
	// (Character + Team + Edge/Body/Helmet/Weapon/Beam) survives the swap -- the body/color resolvers then
	// read the new PS and find the selection (no more <none> -> ARIA fallback). bSelectionLocked carries too
	// so the change-timing gate state survives. Authority-only path (CopyProperties runs server-side during
	// the handoff); the new value replicates from the new PS via the existing DOREPLIFETIME.
	if (UAFLCosmeticLoadoutComponent* Target = Cast<UAFLCosmeticLoadoutComponent>(TargetPlayerStateComponent))
	{
		Target->Selection = Selection;
		Target->bSelectionLocked = bSelectionLocked;
		// CC-3.2: builds must survive the PlayerState swap too. Without this a respawn silently
		// empties the saved set while the resolved Selection keeps rendering -- the set and what is
		// on screen would disagree, with no error anywhere to say so.
		Target->BuildSet = BuildSet;
		// AN ACTIVE BUILD IS THE SOURCE OF TRUTH ACROSS THE SWAP. MEASURED DEFECT: without this, the
		// build set survived a respawn (builds=2 active=1) while Selection was restored from
		// persistence -- so ActiveBuildIndex claimed build 1 and the pawn rendered the pre-build
		// persisted colour instead. The set and the screen disagreed and nothing reported it.
		// Re-resolve rather than trusting the copied Selection: ResolveInto() is pure and uses the
		// values frozen at save, so this re-derives what the build ALREADY meant -- it does not
		// recompute or re-clamp them, and CC-4.2's freeze-never-mutate promise is untouched.
		if (const FAFLCreatorBuild* ActiveBuild = Target->BuildSet.GetActive())
		{
			Target->Selection = ActiveBuild->ResolveInto();
		}

		if (AFLSkinDiag::IsOn())
		{
			UE_LOG(LogAFLSkinDiag, Log, TEXT("%s[Loadout] CopyProperties -> carried selection identity=%s/%s edge=%s across PS swap"),
				*AFLSkinDiag::Prefix(this),
				(Selection.IdentityType == EAFLIdentityType::Character) ? TEXT("Character") : TEXT("Team"),
				*Selection.GetActiveIdentityId().ToString(), *Selection.EdgeId.ToString());
		}
	}
}

void UAFLCosmeticLoadoutComponent::BeginPlay()
{
	Super::BeginPlay();

	// Diag (cvar afl.SkinDiag, OFF by default): prove the component attached + on which PlayerState + net role.
	// This is why we don't have to fall back to `GetAll` to confirm the attach -- enable the cvar and it logs.
	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s[Loadout] BeginPlay on %s (authority=%s)"),
			*AFLSkinDiag::Prefix(this),
			GetOwner() ? *GetOwner()->GetName() : TEXT("<no-owner>"),
			(GetOwner() && GetOwner()->HasAuthority()) ? TEXT("y") : TEXT("n"));
	}

	// RESPAWN-RACE FIX (Option 2 -- possession half): re-drive the proven controller push whenever this
	// PlayerState's pawn is (re)set. On respawn the pawn arrives via APlayerState::OnPawnSet (broadcast from
	// SetPawnPrivate) -- binding here means the push fires from the side that ALWAYS holds the selection (the
	// stable PlayerState), so a pawn arriving AFTER the selection value still gets the right edge. The push
	// itself is authority-guarded inside SetSkinColor, so this is effectively server-side; on a remote client
	// it's a harmless no-op (that client converges via the pawn component's own SkinColor OnRep). Together
	// with the OnRep_Selection re-drive (the value half), whichever lands last triggers the correct push --
	// the skin pillar's PATH1/PATH2 convergence applied to the selection tier.
	if (APlayerState* PS = GetPlayerState<APlayerState>())
	{
		PS->OnPawnSet.AddDynamic(this, &ThisClass::OnPlayerStatePawnSet);
	}

	// On the server, load any persisted selection for this player (async-shaped; the stub fires the
	// delegate synchronously). A found selection replicates to clients via the OnRep path; the controller
	// component reads it at the next possess to drive the proven push. No-op if persistence is unbound.
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (IAFLCosmeticPersistence* Persistence = GetPersistence())
		{
			TWeakObjectPtr<UAFLCosmeticLoadoutComponent> WeakThis(this);
			Persistence->LoadSelection(MakePlayerId(),
				FAFLOnSelectionLoaded::CreateLambda([WeakThis](bool bFound, const FAFLCosmeticSelection& Loaded)
				{
					if (UAFLCosmeticLoadoutComponent* Self = WeakThis.Get())
					{
						if (bFound && Self->GetOwner() && Self->GetOwner()->HasAuthority())
						{
							Self->Selection = Loaded;
							Self->NudgeControllerReapply();
						}
					}
				}));
		}
	}
}

// CC-5.2: the AFLCreatorGamut namespace MOVED to AFLCosmeticSelectionTypes.h. It was private to
// this file, so a creator UI could not preview with the same clamp the server commits with --
// two implementations of one rule, drifting silently. The definition is unchanged; only its home
// moved, so every ClampToNeon call site below resolves to the shared one.

bool UAFLCosmeticLoadoutComponent::IsSelectionEditable() const
{
	// D6 STUB-OPEN for #43: the match<->hub boundary that would raise bSelectionLocked at match-start is
	// not built yet, so the lock is never set and selection is always editable. The call site is LIVE
	// (ServerSetCosmeticSelection consults it) so the real policy -- `return !bSelectionLocked;` once
	// match-start sets the flag -- drops in with no structural change. Written long-hand to make the seam
	// explicit rather than implying the lock is already enforced.
	return !bSelectionLocked; // bSelectionLocked stays false in #43 -> always true, as ruled.
}

bool UAFLCosmeticLoadoutComponent::ServerSetCosmeticSelection_Validate(FAFLCosmeticSelection Requested)
{
	// Network-layer structural sanity: the identity discriminator must carry a non-None id for its type.
	// Malformed RPCs are rejected before any gameplay logic. (Per-axis ids may be None = "unset".)
	const bool bIdentityOk = (Requested.GetActiveIdentityId() != NAME_None);
	return bIdentityOk;
}

void UAFLCosmeticLoadoutComponent::ServerSetCosmeticSelection_Implementation(FAFLCosmeticSelection Requested)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const bool bDiag = AFLSkinDiag::IsOn();
	if (bDiag)
	{
		// Arrival on the server: the RPC reached authority. Show the requested identity + edge (the wired axis).
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s[Loadout] ServerSetCosmeticSelection RX on %s: reqIdentity=%s/%s reqEdge=%s reqFacemask=%s"),
			*AFLSkinDiag::Prefix(this), GetOwner() ? *GetOwner()->GetName() : TEXT("<no-owner>"),
			(Requested.IdentityType == EAFLIdentityType::Character) ? TEXT("Character") : TEXT("Team"),
			*Requested.GetActiveIdentityId().ToString(), *Requested.EdgeId.ToString(),
			*Requested.FacemaskId.ToString());
	}

	// Step 2 -- change-timing gate (D6). STUB-OPEN now; rejects mid-match once the lock lands.
	if (!IsSelectionEditable())
	{
		if (bDiag)
		{
			UE_LOG(LogAFLSkinDiag, Log, TEXT("%s[Loadout] REJECTED: selection locked (in-match)"), *AFLSkinDiag::Prefix(this));
		}
		return; // locked: keep the current selection unchanged.
	}

	const ALyraPlayerState* PS = GetLyraPlayerState();
	IAFLEntitlementSource* Entitlement = GetEntitlementSource();

	// Step 3 -- entitlement gate (per axis + identity). Start from the CURRENT selection and apply only
	// the entitled fields of the request, so an unentitled id leaves that axis at its prior (entitled)
	// value while the rest of the request still applies. Permissive impl returns true for basics now.
	FAFLCosmeticSelection NewSelection = Selection;

	auto IdentityOwned = [&](EAFLIdentityType Type, FName Id) -> bool
	{
		return (Id != NAME_None) && (!Entitlement || Entitlement->OwnsIdentity(PS, Type, Id));
	};
	auto AxisEntitled = [&](FName Id) -> bool
	{
		// None = "no change requested for this axis" -> treat as allowed (it just won't overwrite).
		return (Id == NAME_None) || (!Entitlement || Entitlement->IsEntitled(PS, Id));
	};
	// CC-2.1 CREATOR COLOUR entitlement -- shaped like AxisEntitled/IdentityOwned above.
	// TODO(Stage B): real gate (creator-colour subscription/tier via the entitlement source). STUB-OPEN now; the
	// CLAMP below is the LIVE, non-stubbed server protection -- ownership gating lands in Stage B, not the clamp.
	auto CreatorColorsEntitled = [&]() -> bool
	{
		return true; // STUB-OPEN (Stage B)
	};

	// Identity slot (either/or, resolved by type).
	if (IdentityOwned(Requested.IdentityType, Requested.GetActiveIdentityId()))
	{
		NewSelection.IdentityType = Requested.IdentityType;
		if (Requested.IdentityType == EAFLIdentityType::Character)
		{
			NewSelection.CharacterId = Requested.CharacterId;
		}
		else
		{
			NewSelection.TeamId = Requested.TeamId;
		}
	}

	// Per-axis cosmetics: overwrite only entitled, non-None requests.
	if (Requested.EdgeId   != NAME_None && AxisEntitled(Requested.EdgeId))   { NewSelection.EdgeId   = Requested.EdgeId;   }
	if (Requested.BodyId   != NAME_None && AxisEntitled(Requested.BodyId))   { NewSelection.BodyId   = Requested.BodyId;   }
	if (Requested.HelmetId != NAME_None && AxisEntitled(Requested.HelmetId)) { NewSelection.HelmetId = Requested.HelmetId; }
	if (Requested.WeaponId != NAME_None && AxisEntitled(Requested.WeaponId)) { NewSelection.WeaponId = Requested.WeaponId; }
	// LEFT-hand (dual-mount Hand-Cannon) axis: takes the FACEMASK shape, NOT the WeaponId shape -- NAME_None is a
	// MEANINGFUL un-equip (SetLeftWeapon none drops the left cannon back to the single-held path), so the request's
	// LeftWeaponId ALWAYS overwrites (entitlement gates a non-None equip; clearing needs none). Without this copy the
	// server dropped LeftWeaponId, RefreshWeaponForPawn never saw it, and the dual dispatch never fired.
	if (Requested.LeftWeaponId == NAME_None || AxisEntitled(Requested.LeftWeaponId)) { NewSelection.LeftWeaponId = Requested.LeftWeaponId; }
	if (Requested.WeaponSkinId != NAME_None && AxisEntitled(Requested.WeaponSkinId)) { NewSelection.WeaponSkinId = Requested.WeaponSkinId; }
	if (Requested.BeamId   != NAME_None && AxisEntitled(Requested.BeamId))   { NewSelection.BeamId   = Requested.BeamId;   }
	// FACEMASK axis: equip a NEW facemask (entitled, non-None), OR un-equip when the request is explicitly
	// NAME_None. UNLIKE the other axes, NAME_None is a MEANINGFUL un-equip here -- so the request's FacemaskId
	// ALWAYS overwrites (a None request clears the equipped mask). The entitlement check still gates a non-None
	// equip; None is always allowed (clearing needs no entitlement). This is the one-line the runtime equip
	// path was missing: without it, FacemaskId never left the current value -> the server committed <none>.
	if (Requested.FacemaskId == NAME_None || AxisEntitled(Requested.FacemaskId)) { NewSelection.FacemaskId = Requested.FacemaskId; }

	// Step 3b -- CREATOR COLOUR OVERLAY (CC-2.1). Clamp each supplied colour into the neon gamut SERVER-SIDE and
	// commit the CLAMPED values, NEVER the raw request. Entitlement-gated exactly as the axis ids are (Stage-B
	// stub). bUseCreatorColors==false clears the overlay; an unentitled request leaves the prior committed overlay
	// untouched (same shape as an unentitled axis id). NewSelection started from the current committed Selection.
	if (Requested.bUseCreatorColors)
	{
		if (CreatorColorsEntitled())
		{
			NewSelection.bUseCreatorColors = 1;
			NewSelection.CreatorBodyColor  = AFLCreatorGamut::ClampToNeon(Requested.CreatorBodyColor);
			NewSelection.CreatorEdgeColor  = AFLCreatorGamut::ClampToNeon(Requested.CreatorEdgeColor);
			NewSelection.CreatorGlowColor  = AFLCreatorGamut::ClampToNeon(Requested.CreatorGlowColor);
			// CC-6.4 visor: clamped like every other creator colour -- it is client-supplied and must not
			// escape the neon gamut. bVisorColorSet is carried verbatim: if the player never chose one we
			// keep it FALSE so BuildColorOverride mirrors the body and pre-split rendering is preserved.
			NewSelection.bVisorColorSet = Requested.bVisorColorSet;
			if (Requested.bVisorColorSet)
			{
				NewSelection.CreatorVisorColor = AFLCreatorGamut::ClampToNeon(Requested.CreatorVisorColor);
			}
			// No else: an unchosen visor is NOT written to body here. EffectiveVisorColor() decides the
			// fallback for every path at once -- writing it here as well would be a second mechanism.
		}
		// unentitled + requested -> leave the prior committed overlay as-is (do not clear, do not write raw).
	}
	else
	{
		NewSelection.bUseCreatorColors = 0; // explicit "creator colours off" -> drop the overlay (back to preset).
	}

	// Step 4 -- commit -> replicate (OnRep fires on clients; authority applies via the nudge below).
	// A DIRECT EDIT LEAVES THE ACTIVE BUILD. Otherwise ActiveBuildIndex keeps naming a build whose
	// colours are no longer what is rendering, and the creator UI would highlight a slot that does
	// not match the pawn. MEASURED: a direct colour set after activating build 1 left active=1 while
	// the pawn rendered the directly-set colour -- the index lied, and nothing reported it.
	// INVARIANT ESTABLISHED HERE: ActiveBuildIndex != INDEX_NONE IMPLIES Selection equals that
	// build's ResolveInto(). Saved builds are untouched -- only the pointer to the live one clears.
	if (BuildSet.ActiveBuildIndex != INDEX_NONE)
	{
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[BUILD] direct edit -> leaving active build %d"),
			BuildSet.ActiveBuildIndex);
		BuildSet.ActiveBuildIndex = INDEX_NONE;
	}
	Selection = NewSelection;

	if (bDiag)
	{
		// What landed after gating (edge is the wired axis -> what the controller push will resolve).
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s[Loadout] COMMITTED on %s: identity=%s/%s edge=%s facemask=%s (replicating)"),
			*AFLSkinDiag::Prefix(this), GetOwner() ? *GetOwner()->GetName() : TEXT("<no-owner>"),
			(Selection.IdentityType == EAFLIdentityType::Character) ? TEXT("Character") : TEXT("Team"),
			*Selection.GetActiveIdentityId().ToString(), *Selection.EdgeId.ToString(),
			*Selection.FacemaskId.ToString());
	}

	// Persist through the stub interface (D8). Fire-and-forget; no-op if unbound.
	if (IAFLCosmeticPersistence* Persistence = GetPersistence())
	{
		Persistence->SaveSelection(MakePlayerId(), Selection);
	}

	// Step 5 -- if already possessed (pre-match live change), re-run the proven controller push so the
	// change shows immediately without a respawn. In-match this line is unreachable (step 2 rejects).
	NudgeControllerReapply();
	RefreshStickers();   // CC-7: stickers are not a skin change; drive them explicitly
}

FAFLColorOverride UAFLCosmeticLoadoutComponent::BuildColorOverride(const FAFLCosmeticSelection& Sel)
{
	// CC-2.1: the ONE construction of the creator overlay -- shared by RefreshSkinForPawn (step 5, server push) and
	// OnRep_Selection (step 6, client populate). Invalid unless bUseCreatorColors -> non-creator = no overlay.
	return Sel.bUseCreatorColors
		? FAFLColorOverride(Sel.CreatorBodyColor, Sel.CreatorEdgeColor, Sel.CreatorGlowColor,
			// EffectiveVisorColor() applies the migration mirror regardless of HOW Sel was produced --
			// resolved from a build, replicated, clamped through the server RPC, or loaded from
			// persistence written before the field existed. That last path is the one that read White.
			Sel.EffectiveVisorColor())
		: FAFLColorOverride();
}

void UAFLCosmeticLoadoutComponent::OnRep_Selection()
{
	// Remote clients: the selection value replicated in. The controller component is the authoritative
	// driver of the ASSET push (which preset/mask is equipped -- server-side at possess); this OnRep lets
	// client-side UI (wallet preview, nameplates) react to a selection change.
	//
	// ⚠ UPDATED CC-2.1 (was: "No visual push from here ... never client-side"). That is no longer true, and
	// deliberately so: the CREATOR COLOUR overlay is a per-channel VALUE that rides this replicated Selection,
	// not an asset pointer. On a dedicated-server client the authority-only SetSkinColor never runs, so without
	// a client-side populate the pawn would re-apply PRESET colours (CC-2.0-R §3). The populate below is
	// therefore local-only CONVERGENCE onto already-replicated, already-server-CLAMPED values -- it never
	// originates or alters a selection, so the server stays the sole authority. Asset equips are unchanged:
	// they still flow exclusively through the proven server-authority SetSkinColor path.
	if (AFLSkinDiag::IsOn())
	{
		// Firing on a remote client proves the selection crossed the wire (the 2-client replication check).
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s[Loadout] OnRep_Selection on %s: identity=%s/%s edge=%s"),
			*AFLSkinDiag::Prefix(this), GetOwner() ? *GetOwner()->GetName() : TEXT("<no-owner>"),
			(Selection.IdentityType == EAFLIdentityType::Character) ? TEXT("Character") : TEXT("Team"),
			*Selection.GetActiveIdentityId().ToString(), *Selection.EdgeId.ToString());
	}

	// RESPAWN-RACE FIX (Option 2 -- value half): the selection VALUE just replicated in. If the pawn already
	// existed when the value arrived (pawn-then-value ordering on a remote client / late join), re-drive the
	// push now that we have the value. The push is authority-guarded, so this is a no-op on a pure remote
	// client (which converges via the pawn component's SkinColor OnRep) and meaningful on the listen-host.
	// Pairs with the OnPawnSet hook (the pawn half): whichever lands last fires the correct push.

	NudgeControllerReapply();
	RefreshStickers();   // CC-7: stickers are not a skin change; drive them explicitly
}

ALyraPlayerState* UAFLCosmeticLoadoutComponent::GetLyraPlayerState() const
{
	return GetPlayerState<ALyraPlayerState>();
}

IAFLEntitlementSource* UAFLCosmeticLoadoutComponent::GetEntitlementSource() const
{
	// S-ECON-WALLET (layer b): the real entitlement source is the player's UAFLWalletComponent (same
	// PlayerState), which implements IAFLEntitlementSource against its replicated owned-set + the catalog's
	// GrantedFree flag. Resolved here -- the swap point the #43 design reserved. If the wallet isn't attached
	// yet (early bring-up / an experience without it), this returns null and the call sites short-circuit to
	// permissive (basics owned), exactly as before -- so the gate degrades safely, never hard-fails.
	if (const ALyraPlayerState* PS = GetLyraPlayerState())
	{
		if (UAFLWalletComponent* Wallet = PS->FindComponentByClass<UAFLWalletComponent>())
		{
			return Wallet;
		}
	}
	return nullptr;
}

const FName UAFLCosmeticLoadoutComponent::SlotEntitlementKey(TEXT("AFL.CreatorSlot"));
const FName UAFLCosmeticLoadoutComponent::LeagueConditionId(TEXT("AFL.Condition.League"));

EAFLConditionState UAFLCosmeticLoadoutComponent::GetConditionState(FName ConditionId) const
{
	// ABSENT IS Unknown, NOT Lapsed. Collapsing those two is the dangerous direction: a server that has
	// not reached the entitlement source yet would treat every player as freshly lapsed.
	if (const EAFLConditionState* Found = ConditionStates.Find(ConditionId))
	{
		return *Found;
	}
	return EAFLConditionState::Unknown;
}

void UAFLCosmeticLoadoutComponent::SetConditionState(FName ConditionId, EAFLConditionState NewState)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }

	const EAFLConditionState Was = GetConditionState(ConditionId);
	ConditionStates.Add(ConditionId, NewState);
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[COND] %s: %d -> %d"),
		*ConditionId.ToString(), (int32)Was, (int32)NewState);

	if (ConditionId == LeagueConditionId)
	{
		RefreshLapseFromSubscription();
	}
}

void UAFLCosmeticLoadoutComponent::RefreshLapseFromSubscription()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }

	const EAFLConditionState State = GetConditionState(LeagueConditionId);

	if (State == EAFLConditionState::Unknown)
	{
		// PENALTIES FAIL OPEN ON Unknown -- the conditional-entitlement contract, obeyed literally.
		// Doing nothing is the correct action: ApplyLapseRule with ANY cap would be applying the
		// penalty to a player nobody has checked.
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[LAPSE] league condition UNKNOWN -- lapse rule NOT applied (penalties fail open)."));
		return;
	}

	// HELD: the player's full cap, continuum authoring held.
	// LAPSED: back to the baseline nobody has to buy, continuum authoring locked. Builds beyond the
	// baseline become READ-ONLY -- never deleted, and the active one keeps rendering.
	const bool bHeld = (State == EAFLConditionState::Held);
	const int32 Cap = bHeld ? GetEffectiveSlotCap() : SlotBaseline;
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[LAPSE] league=%s -> cap=%d editingHeld=%d"),
		bHeld ? TEXT("HELD") : TEXT("LAPSED"), Cap, bHeld ? 1 : 0);
	ApplyLapseRule(Cap, bHeld);
}

UAFLWalletComponent* UAFLCosmeticLoadoutComponent::GetWalletComponent() const
{
	if (const ALyraPlayerState* PS = GetLyraPlayerState())
	{
		return PS->FindComponentByClass<UAFLWalletComponent>();
	}
	return nullptr;
}

int32 UAFLCosmeticLoadoutComponent::CountUnlockedBuilds() const
{
	int32 N = 0;
	for (const FAFLCreatorBuild& B : BuildSet.Builds)
	{
		if (!B.bReadOnly) { ++N; }
	}
	return N;
}

int32 UAFLCosmeticLoadoutComponent::GetEffectiveSlotCap() const
{
	const UAFLWalletComponent* Wallet = GetWalletComponent();
	if (!Wallet)
	{
		// FAIL CLOSED. An unverifiable purchase count is not a licence to save without limit. The player
		// keeps the baseline they never had to buy; nothing purchased is honoured on a read that failed.
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[Creator] slot cap: NO WALLET -- failing closed to baseline %d."), SlotBaseline);
		return SlotBaseline;
	}
	const int32 Purchased = Wallet->GetCountedEntitlement(SlotEntitlementKey);
	return FMath::Clamp(SlotBaseline + Purchased, SlotBaseline, SlotHardCap);
}

IAFLCosmeticPersistence* UAFLCosmeticLoadoutComponent::GetPersistence() const
{
	// Phase A0: the local SaveGame persistence subsystem (first impl of the seam). The BeginPlay
	// LoadSelection + the RPC SaveSelection now round-trip to disk -> the loadout survives a session
	// boundary. A1 swaps this backing (Bag_Man_Backend Lambda, server-auth) behind the SAME interface.
	return UAFLEconomyPersistenceSubsystem::Get(this);
}

FAFLPlayerId UAFLCosmeticLoadoutComponent::MakePlayerId() const
{
	// A1.1: the account-system/PlayFab backing the #43 stub deferred -- now the PlayFabId (durable, cross-
	// session AND cross-device). Fall back to the net-id (A0 behavior) when not logged in.
	if (const UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this))
	{
		if (Online->IsLoggedIn() && !Online->GetPlayFabId().IsEmpty())
		{
			return FAFLPlayerId::MakeFromBacking(Online->GetPlayFabId());
		}
	}
	if (const APlayerState* PS = GetLyraPlayerState())
	{
		const FUniqueNetIdRepl& NetId = PS->GetUniqueId();
		if (NetId.IsValid())
		{
			return FAFLPlayerId::MakeFromBacking(NetId->ToString());
		}
	}
	return FAFLPlayerId();
}

void UAFLCosmeticLoadoutComponent::NudgeControllerReapply() const
{
	// Drive the PROVEN push: find the owning controller's UAFLSkinColorControllerComponent and ask it to
	// re-resolve+push for the current pawn (idempotent; same path the #38a part-arrival hook uses). The
	// controller component reads THIS selection during resolution (File 5 edit). No new propagation here.
	const ALyraPlayerState* PS = GetLyraPlayerState();
	AController* OwningController = PS ? PS->GetOwningController() : nullptr;
	if (!OwningController)
	{
		return;
	}

	if (UAFLSkinColorControllerComponent* SkinCtrl =
			OwningController->FindComponentByClass<UAFLSkinColorControllerComponent>())
	{
		if (APawn* Pawn = OwningController->GetPawn())
		{
			// Facemask FIRST (slot-1 material swap), THEN skin (param push) -- composition order, so a live
			// wallet-UI facemask change shows immediately without respawn (the FacemaskId is in this selection).
			SkinCtrl->RefreshFacemaskForPawn(Pawn);
			// CC-5: the emblem rides the same moments as the facemask -- same driver, same ordering.
			SkinCtrl->RefreshEmblemForPawn(Pawn);
			SkinCtrl->RefreshSkinForPawn(Pawn);
			// #43 WeaponId consumer: equip the selected weapon (D2 replace) on the SAME spine. Independent
			// subsystem (equipment, not material) -> order-free; server-only + Lyra fast-array-replicated.
			SkinCtrl->RefreshWeaponForPawn(Pawn);
				SkinCtrl->RefreshWeaponSkinForPawn(Pawn); // weapon COLOR (WeaponId suffix) -- AFTER equip so the mesh exists
				SkinCtrl->RefreshBeamColorForPawn(Pawn);  // INDEPENDENT BeamId axis -- beam applies to ANY equipped weapon
		}
	}
}

void UAFLCosmeticLoadoutComponent::OnPlayerStatePawnSet(APlayerState* /*Player*/, APawn* NewPawn, APawn* /*OldPawn*/)
{
	// RESPAWN-RACE FIX (Option 2 -- pawn half). The PlayerState's pawn was (re)set -- on respawn this is the
	// new pawn linking. Re-drive the proven push for THIS new pawn directly (not via GetPawn(), which can lag
	// the transition). The controller's RefreshSkinForPawn re-resolves the selection (Option 1 reads it off
	// the pawn's now-populated PlayerState) and SetSkinColor authority-gates internally -> meaningful on the
	// server (drives the authoritative SkinColor that then replicates), harmless no-op on a pure remote client.
	if (!NewPawn)
	{
		return; // pawn cleared (death teardown) -> nothing to push.
	}

	if (const ALyraPlayerState* PS = GetLyraPlayerState())
	{
		if (AController* OwningController = PS->GetOwningController())
		{
			if (UAFLSkinColorControllerComponent* SkinCtrl =
					OwningController->FindComponentByClass<UAFLSkinColorControllerComponent>())
			{
				// Facemask FIRST then skin (composition order) -- makes the equipped facemask respawn-durable
				// on the same spine as the skin/identity (the FacemaskId rides CopyProperties already).
				SkinCtrl->RefreshFacemaskForPawn(NewPawn);
				SkinCtrl->RefreshEmblemForPawn(NewPawn);
				SkinCtrl->RefreshSkinForPawn(NewPawn);
				// #43 WeaponId consumer: equip the selected weapon on possession/respawn (WeaponId rides
				// CopyProperties -> respawn-durable, same spine as facemask/skin).
				SkinCtrl->RefreshWeaponForPawn(NewPawn);
					SkinCtrl->RefreshWeaponSkinForPawn(NewPawn); // weapon COLOR (WeaponId suffix) -- AFTER equip so the mesh exists
					SkinCtrl->RefreshBeamColorForPawn(NewPawn);  // INDEPENDENT BeamId axis -- beam applies to ANY equipped weapon
			}
		}
	}
}

// --- CC-3.2 SAVED BUILDS -------------------------------------------------------------------------

void UAFLCosmeticLoadoutComponent::OnRep_BuildSet()
{
	// Creator-UI notify only. Gameplay reads Selection, which arrives via OnRep_Selection exactly as
	// before -- deliberately NOT re-resolved here: a client re-resolving would be a second source of
	// truth for what renders, and the two would drift with nothing to catch it.
	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s[Loadout] OnRep_BuildSet builds=%d active=%d"),
			*AFLSkinDiag::Prefix(this), BuildSet.Builds.Num(), BuildSet.ActiveBuildIndex);
	}
}

bool UAFLCosmeticLoadoutComponent::ServerSaveBuild_Validate(FAFLCreatorBuild Build, int32 Index)
{
	// _Validate DISCONNECTS A CHEATING CLIENT; it is not the business rule. It returned true
	// unconditionally, so a malformed call reached the implementation and was merely ignored there.
	//
	// Anything a legitimate client cannot produce is rejected here. The slot CAP is NOT checked in this
	// function: exceeding it is an ordinary refusal a truthful client can hit by trying to save one too
	// many, and disconnecting a player for that would be punishing them for a normal action.
	//
	// INDEX_NONE means "append". Any other negative, or an index past the hard cap, is a fabricated call.
	if (Index < INDEX_NONE || Index >= SlotHardCap)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("[Creator] ServerSaveBuild_Validate REJECT: index %d out of range."), Index);
		return false;
	}
	// A name longer than the validator could ever accept is not a name, it is a payload.
	if (Build.DisplayName.Len() > 256)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("[Creator] ServerSaveBuild_Validate REJECT: name length %d."),
			Build.DisplayName.Len());
		return false;
	}
	return true;
}

void UAFLCosmeticLoadoutComponent::ServerSaveBuild_Implementation(FAFLCreatorBuild Build, int32 Index)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }

	// CC-4.2: a read-only build refuses EDITS. It is never deleted and never mutated -- a shrinking
	// cap must not cost a player work they already did.
	if (BuildSet.Builds.IsValidIndex(Index) && BuildSet.Builds[Index].bReadOnly)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("[Creator] ServerSaveBuild REFUSED: build %d is read-only."), Index);
		return;
	}

	// CC-4.2: a lapse locks CONTINUUM authoring specifically. A build made only of catalog SKUs the
	// player owns outright is still editable -- the lapse took the creator, not their purchases.
	if (bContinuumEditingLocked && Build.UsesContinuum())
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("[Creator] ServerSaveBuild REFUSED: continuum editing locked (lapsed)."));
		return;
	}

	// VALIDATE ONCE, HERE. A continuum channel carries a client-chosen colour, so clamp it into the
	// neon gamut before storing -- the same server-authoritative clamp CC-2.1 proved. A catalog channel
	// carries an id instead; its ownership gate is Stage B policy and is NOT applied here, because
	// CreatorColorsEntitled() is still a stub returning true and un-stubbing it needs a ruling.
	auto ClampChannel = [](FAFLChannelValue& Ch)
	{
		if (Ch.Source == EAFLChannelSource::Continuum)
		{
			Ch.Resolved = AFLCreatorGamut::ClampToNeon(Ch.Resolved);
		}
	};
	ClampChannel(Build.BodyChannel);
	ClampChannel(Build.EdgeChannel);
	ClampChannel(Build.GlowChannel);
	// CC-5.4: validate the NAME before storing. A refused name refuses the SAVE rather than being
	// silently corrected -- a player who typed something must be told, not quietly overridden.
	FString Sanitised;
	const EAFLNameVerdict Verdict = ValidateBuildName(Build.DisplayName, BuildSet.Builds, Index, Sanitised);
	if (Verdict != EAFLNameVerdict::Ok)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[NAME] REFUSED verdict=%d name='%s'"),
			(int32)Verdict, *Build.DisplayName);
		return;
	}
	Build.DisplayName = Sanitised;
	// Any edit returns the name to Pending: an approved name must not be a token that lets later
	// text ride in behind it.
	Build.NameState = EAFLNameState::Pending;
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[NAME] accepted '%s' state=Pending"), *Sanitised);
	Build.bReadOnly = false;

	if (BuildSet.Builds.IsValidIndex(Index))
	{
		BuildSet.Builds[Index] = Build;
	}
	else
	{
		// SAVE AND LOCK -- the cap decides whether a new build is LOCKED, never whether it is KEPT.
		//
		// THE SAME EXPRESSION AS ApplyLapseRule, DELIBERATELY. That rule locks by `i >= Cap`, so a build
		// appended at index N is unlocked exactly when N < Cap -- which is Builds.Num() < Cap here. One
		// rule for what is locked, evaluated in two places that cannot disagree because they are the
		// same comparison. A second locking concept is what this avoids.
		//
		// THE BUILD IS ALWAYS KEPT. A refused save destroys a creation the player just made, and the
		// conversion model depends on them still having it to want back.
		const int32 Cap = GetEffectiveSlotCap();
		const bool bLockedOnArrival = (BuildSet.Builds.Num() >= Cap);
		Build.bReadOnly = bLockedOnArrival;
		Index = BuildSet.Builds.Add(Build);

		if (bLockedOnArrival)
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_TEST[SLOT] SAVED LOCKED: index=%d, %d unlocked slot(s) in use, cap %d. ")
				TEXT("Kept and rendering; buy a slot to unlock."),
				Index, CountUnlockedBuilds(), Cap);
		}
	}
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[BUILD] saved index=%d builds=%d continuum=%d"),
		Index, BuildSet.Builds.Num(), Build.UsesContinuum() ? 1 : 0);
	PushBuildsToPersistence();
}

bool UAFLCosmeticLoadoutComponent::ServerSetActiveBuild_Validate(int32) { return true; }

void UAFLCosmeticLoadoutComponent::ServerSetActiveBuild_Implementation(int32 Index)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	if (!BuildSet.Builds.IsValidIndex(Index))
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("[Creator] ServerSetActiveBuild REFUSED: index %d of %d."),
			Index, BuildSet.Builds.Num());
		return;
	}

	BuildSet.ActiveBuildIndex = Index;

	// RESOLVE INTO THE ONE SELECTION. No re-clamp, no catalog re-lookup: the values were validated at
	// save, and CC-4.2 requires the build to render identically afterwards whatever has happened to
	// the player's entitlements or to the catalog since. Re-validating here is exactly what would
	// break that promise. The index is server-side state, so there is no client payload to distrust.
	Selection = BuildSet.Builds[Index].ResolveInto();

	UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[BUILD] active=%d creatorOn=%d body=(%.4f,%.4f,%.4f)"),
		Index, Selection.bUseCreatorColors ? 1 : 0,
		Selection.CreatorBodyColor.R, Selection.CreatorBodyColor.G, Selection.CreatorBodyColor.B);

	// Drive the same post-commit path a direct selection change uses, so a build activation and a
	// direct edit are indistinguishable downstream.
	OnRep_Selection();
	NudgeControllerReapply();
	RefreshStickers();   // CC-7: stickers are not a skin change; drive them explicitly
}

// --- CC-4.2 LAPSE RULE ----------------------------------------------------------------------------

void UAFLCosmeticLoadoutComponent::ApplyLapseRule(int32 EffectiveSlotCap, bool bContinuumEditingHeld)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }

	// NEGATIVE CAP IS A CALLER BUG, NOT A LOCKOUT. Clamping to 0 rather than trusting it keeps a bad
	// entitlement read from silently read-only-ing every build a player owns.
	const int32 Cap = FMath::Max(0, EffectiveSlotCap);

	int32 Locked = 0, Unlocked = 0;
	for (int32 i = 0; i < BuildSet.Builds.Num(); ++i)
	{
		// INDEX ORDER, deliberately. "Beyond the cap" needs a rule that is STABLE and predictable: the
		// same cap must always produce the same locked set. Recency would reshuffle which build locks
		// every time a player touched one, so a lapse would feel arbitrary.
		const bool bShouldBeReadOnly = (i >= Cap);
		if (BuildSet.Builds[i].bReadOnly != bShouldBeReadOnly)
		{
			BuildSet.Builds[i].bReadOnly = bShouldBeReadOnly;
			if (bShouldBeReadOnly) { ++Locked; } else { ++Unlocked; }
		}
	}

	const bool bWasLocked = bContinuumEditingLocked;
	bContinuumEditingLocked = !bContinuumEditingHeld;

	// NOTHING IS DELETED AND NOTHING IS RE-RESOLVED. The active build keeps rendering exactly what it
	// rendered before -- including a build now read-only. That is the whole promise: a lapsed player
	// still LOOKS like the robot they built, they simply cannot change it. Re-resolving here, or
	// dropping builds past the cap, would be the two obvious ways to break it.
	UE_LOG(LogAFLCombat, Display,
		TEXT("AFL_TEST[LAPSE] cap=%d builds=%d newlyLocked=%d newlyUnlocked=%d editingLocked=%d->%d active=%d"),
		Cap, BuildSet.Builds.Num(), Locked, Unlocked, bWasLocked ? 1 : 0,
		bContinuumEditingLocked ? 1 : 0, BuildSet.ActiveBuildIndex);
}

// --- CC-3.5 BUILD PERSISTENCE ---------------------------------------------------------------------

void UAFLCosmeticLoadoutComponent::PushBuildsToPersistence()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UAFLEconomyPersistenceSubsystem* Persist = GI ? GI->GetSubsystem<UAFLEconomyPersistenceSubsystem>() : nullptr;
	if (!Persist) { return; }

	FString Json;
	if (!FJsonObjectConverter::UStructToJsonObjectString(BuildSet, Json, 0, 0))
	{
		// Refuse to push a blob we could not serialise. Pushing "{}" here would overwrite the player's
		// saved robots with an empty set on the remote -- a silent data loss triggered by a local bug.
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[BUILDSYNC] push ABORTED: serialise failed (nothing sent)."));
		return;
	}
	const FString PlayFabId = ResolvePlayFabIdForOwner();
	++BuildsRevision;
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[BUILDSYNC] push builds=%d bytes=%d rev=%d pid=%s"),
		BuildSet.Builds.Num(), Json.Len(), BuildsRevision, PlayFabId.IsEmpty() ? TEXT("<none>") : *PlayFabId);
	Persist->SaveCreatorBuilds(MakePlayerKey(), PlayFabId, Json, BuildsRevision);
}

void UAFLCosmeticLoadoutComponent::PullBuildsFromPersistence()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UAFLEconomyPersistenceSubsystem* Persist = GI ? GI->GetSubsystem<UAFLEconomyPersistenceSubsystem>() : nullptr;
	if (!Persist) { return; }

	TWeakObjectPtr<UAFLCosmeticLoadoutComponent> WeakThis(this);
	Persist->LoadCreatorBuilds(MakePlayerKey(), ResolvePlayFabIdForOwner(),
		FAFLOnCreatorBuildsLoaded::CreateLambda([WeakThis](bool bFound, const FString& BuildsJson)
	{
		UAFLCosmeticLoadoutComponent* Self = WeakThis.Get();
		if (!Self) { return; }
		if (!bFound || BuildsJson.IsEmpty())
		{
			// NEW PLAYER, not a failure. Leave BuildSet alone -- overwriting it with an empty set here
			// would discard anything authored in this session before the load landed.
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[BUILDSYNC] pull found=0 (new player, set untouched)"));
			return;
		}
		// The remote wraps the blob as {"found":..,"builds":{..}}. Take the inner object when present so
		// the same handler works against a bare cached blob and against the endpoint envelope.
		FString Inner = BuildsJson;
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BuildsJson);
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
		{
			const TSharedPtr<FJsonObject>* BuildsObj = nullptr;
			if (Root->TryGetObjectField(TEXT("builds"), BuildsObj) && BuildsObj)
			{
				Inner.Empty();
				const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Inner);
				FJsonSerializer::Serialize(BuildsObj->ToSharedRef(), W);
			}
		}
		FAFLCreatorBuildSet Loaded;
		if (!FJsonObjectConverter::JsonObjectStringToUStruct(Inner, &Loaded, 0, 0))
		{
			// Refuse rather than adopt a half-parsed set. A partially-populated BuildSet would look like a
			// player who lost builds, and nothing downstream could tell that apart from a real deletion.
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[BUILDSYNC] pull PARSE FAILED (set untouched)"));
			return;
		}
		Self->BuildSet = Loaded;
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[BUILDSYNC] pull found=1 builds=%d active=%d"),
			Self->BuildSet.Builds.Num(), Self->BuildSet.ActiveBuildIndex);
	}));
}

FAFLPlayerId UAFLCosmeticLoadoutComponent::MakePlayerKey() const
{
	// Mirrors how the wallet keys the same cache -- the wrapper is opaque, so build it the one way
	// the seam sanctions rather than inventing a second key format that would silently miss.
	return FAFLPlayerId::MakeFromBacking(ResolvePlayFabIdForOwner());
}

FString UAFLCosmeticLoadoutComponent::ResolvePlayFabIdForOwner() const
{
	// DISTINGUISH THE TWO EMPTIES. An empty return meant both "no identity component" and "component
	// present but the id is unresolved", which are different problems with different fixes -- the same
	// absent-versus-default ambiguity this programme keeps paying for. The caller logs pid=<none>
	// either way, so the reason has to be emitted HERE or it is unrecoverable from the log.
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[BUILDSYNC] pid unresolved: NO OWNER"));
		return FString();
	}
	const UAFLPlayerIdentityComponent* Identity = Owner->FindComponentByClass<UAFLPlayerIdentityComponent>();
	if (!Identity)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[BUILDSYNC] pid unresolved: NO IDENTITY COMPONENT on %s"),
			*Owner->GetName());
		return FString();
	}
	const FString Id = Identity->GetResolvedPlayFabId();
	if (Id.IsEmpty())
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[BUILDSYNC] pid unresolved: COMPONENT PRESENT, id empty "
			"(A1.4 resolve has not completed -- needs AFL_RESOLVE_URL + a PlayFab login)"));
	}
	return Id;
}

// --- CC-5.4 BUILD NAMING --------------------------------------------------------------------------

EAFLNameVerdict UAFLCosmeticLoadoutComponent::ValidateBuildName(const FString& Raw,
	const TArray<FAFLCreatorBuild>& Existing, int32 IgnoreIndex, FString& OutSanitised)
{
	// TRIM AND COLLAPSE FIRST. "  Ripper  " and "Ripper" are the same name to a reader, so treating
	// them as different would make the uniqueness check trivially bypassable with a leading space.
	FString S = Raw;
	S.TrimStartAndEndInline();
	while (S.ReplaceInline(TEXT("  "), TEXT(" ")) > 0) {}

	// Reject control and zero-width characters rather than stripping them. Stripping would silently
	// turn a name built to impersonate another into a near-duplicate that passes -- refusing says why.
	for (const TCHAR C : S)
	{
		const bool bControl = (C < 0x20) || (C == 0x7F);
		const bool bZeroWidth = (C == 0x200B || C == 0x200C || C == 0x200D || C == 0xFEFF);
		if (bControl || bZeroWidth)
		{
			return EAFLNameVerdict::IllegalCharacter;
		}
	}

	if (S.Len() < 1)  { return EAFLNameVerdict::TooShort; }
	if (S.Len() > 24) { return EAFLNameVerdict::TooLong; }

	// UNIQUENESS IS PER PLAYER, case-insensitively. Two builds a player cannot tell apart in their own
	// slot list is a usability bug, not a safety one -- global uniqueness would be a policy decision
	// about namespace ownership and is not made here.
	for (int32 i = 0; i < Existing.Num(); ++i)
	{
		if (i == IgnoreIndex) { continue; }
		if (Existing[i].DisplayName.Equals(S, ESearchCase::IgnoreCase))
		{
			return EAFLNameVerdict::Duplicate;
		}
	}

	OutSanitised = S;
	return EAFLNameVerdict::Ok;
}

bool UAFLCosmeticLoadoutComponent::ServerReportBuildName_Validate(int32) { return true; }

void UAFLCosmeticLoadoutComponent::ServerReportBuildName_Implementation(int32 Index)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	if (!BuildSet.Builds.IsValidIndex(Index)) { return; }

	// HIDE FIRST, REVIEW LATER. A reported name stops being shown immediately; the cost of hiding a
	// harmless name for a while is far below the cost of showing an abusive one while a queue drains.
	// The BUILD itself is untouched -- only the name is gated, so the player keeps their robot.
	BuildSet.Builds[Index].NameState = EAFLNameState::Rejected;
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[NAME] reported index=%d -> Rejected (build intact)"), Index);
	PushBuildsToPersistence();
}

// --- CC-5.1 CHANNEL SCHEMA ------------------------------------------------------------------------

FAFLCreatorChannelSchema UAFLCosmeticLoadoutComponent::GetChannelSchemaForPawn(APawn* Pawn) const
{
	FAFLCreatorChannelSchema Out;
	if (!Pawn) { return Out; }

	// Find the slot-1 material on the first part actor that has one. Slot 1 is the visor/facemask slot
	// whose master determines coverage; slot 0 is the body master and is the same for every chassis.
	TArray<UChildActorComponent*> CACs;
	Pawn->GetComponents<UChildActorComponent>(CACs);
	for (const UChildActorComponent* CAC : CACs)
	{
		AAFLCharacterPartActor* Part = Cast<AAFLCharacterPartActor>(CAC ? CAC->GetChildActor() : nullptr);
		if (!Part) { continue; }
		TArray<UMeshComponent*> Meshes;
		Part->GetComponents<UMeshComponent>(Meshes);
		for (UMeshComponent* Mesh : Meshes)
		{
			if (!Mesh || Mesh->GetNumMaterials() < 2) { continue; }
			if (UMaterialInterface* Slot1 = Mesh->GetMaterial(1))
			{
				Out = FAFLCreatorChannelSchema::DeriveFromMaterial(Slot1);
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[SCHEMA] master=%s body=%d edge=%d glow=%d available=%d"),
					*Out.ResolvedFromMaster.ToString(), Out.bBodyAvailable ? 1 : 0,
					Out.bEdgeAvailable ? 1 : 0, Out.bGlowAvailable ? 1 : 0, Out.AvailableCount());
				return Out;
			}
		}
	}
	// No slot-1 material found. Claim nothing rather than guessing a default schema -- a creator that
	// offers controls it cannot honour is worse than one that offers none until it knows.
	UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[SCHEMA] no slot-1 master found on %s -- no channels claimed"),
		*Pawn->GetName());
	return Out;
}

// --- CC-7.2 STICKERS ---------------------------------------------------------------------------
bool UAFLCosmeticLoadoutComponent::ServerSetStickerPlacement_Validate(EAFLStickerZone Zone, FAFLStickerPlacement)
{
	// VALIDATION REFUSES THE IMPOSSIBLE, the clamp CORRECTS the merely out-of-range. A zone outside
	// the enum is not a value to fix -- it is a malformed request, and dropping the connection is the
	// documented response to one.
	return static_cast<uint8>(Zone) < static_cast<uint8>(EAFLStickerZone::MAX);
}

void UAFLCosmeticLoadoutComponent::ServerSetStickerPlacement_Implementation(const EAFLStickerZone Zone, const FAFLStickerPlacement Placement)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }

	// RE-CLAMP ON THE SERVER, unconditionally. The UI clamps while dragging so the player sees the
	// bound; this is what makes the bound TRUE. Set() routes through AFLStickerBounds::Clamp -- there
	// is deliberately no unclamped setter on FAFLStickerSet for a caller to reach for.
	Selection.StickerSet.EnsureSized();
	Selection.StickerSet.Set(Zone, Placement);
	RefreshStickers();          // listen/standalone: the server's own view updates immediately

	// Same commit path the other axes use, so stickers cannot drift onto a private route.
	ServerSetCosmeticSelection(Selection);
}

bool UAFLCosmeticLoadoutComponent::ServerClearStickerZone_Validate(EAFLStickerZone Zone)
{
	return static_cast<uint8>(Zone) < static_cast<uint8>(EAFLStickerZone::MAX);
}

void UAFLCosmeticLoadoutComponent::ServerClearStickerZone_Implementation(const EAFLStickerZone Zone)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	Selection.StickerSet.EnsureSized();
	Selection.StickerSet.ClearZone(Zone);
	ServerSetCosmeticSelection(Selection);
}

// --- CC-8 ACCESSORIES --------------------------------------------------------------------------
bool UAFLCosmeticLoadoutComponent::ServerSetAccessory_Validate(EAFLAccessorySlot Slot, FName)
{
	return static_cast<uint8>(Slot) < static_cast<uint8>(EAFLAccessorySlot::MAX);
}

void UAFLCosmeticLoadoutComponent::ServerSetAccessory_Implementation(const EAFLAccessorySlot Slot, const FName AccessoryId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }

	// FAILS CLOSED on an unmapped slot. Attaching to NAME_None does NOT fail -- it parents to the
	// component root, which would hang the accessory at the pawn's feet and look like an art bug
	// rather than a wiring one. Refusing here is the difference between a visible refusal and a
	// mystery.
	const FName Socket = AFLAccessorySockets::ResolveSocket(Slot);
	if (Socket.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[AFLAccessory] REFUSED slot=%d -- no socket mapping. Nothing equipped."), static_cast<int32>(Slot));
		return;
	}

	// THE MAPPING IS NOT THE SOCKET. An earlier version stopped at the check above and was described as
	// "refuses until the sockets exist" -- it was not: ResolveSocket returns a NAME whether or not the
	// skeleton carries one, so the equip would have succeeded and the part would have attached to a
	// socket that does not exist. UE does not fail that attach; it silently parents to the component
	// ROOT, hanging the accessory at the pawn's feet. That reads as an art bug, and the wiring cause
	// would be invisible.
	//
	// So the guard asks the MESH, which is the thing that will actually resolve the name at attach time.
	if (const ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
	{
		if (const USkeletalMeshComponent* Mesh = OwnerChar->GetMesh())
		{
			if (!Mesh->DoesSocketExist(Socket))
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[AFLAccessory] REFUSED slot=%d -- socket '%s' is not on this mesh's skeleton. Nothing equipped."),
					static_cast<int32>(Slot), *Socket.ToString());
				return;
			}
		}
	}

	FAFLAccessoryPlacement P;
	P.AccessoryId = AccessoryId;
	Selection.AccessorySet.EnsureSized();
	Selection.AccessorySet.Set(Slot, P);

	// Same commit path every other axis uses, so accessories cannot drift onto a private route.
	ServerSetCosmeticSelection(Selection);
}

bool UAFLCosmeticLoadoutComponent::ServerClearAccessory_Validate(EAFLAccessorySlot Slot)
{
	return static_cast<uint8>(Slot) < static_cast<uint8>(EAFLAccessorySlot::MAX);
}

void UAFLCosmeticLoadoutComponent::ServerClearAccessory_Implementation(const EAFLAccessorySlot Slot)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	Selection.AccessorySet.EnsureSized();
	Selection.AccessorySet.ClearSlot(Slot);
	ServerSetCosmeticSelection(Selection);
}

void UAFLCosmeticLoadoutComponent::RefreshStickers() const
{
	// The loadout is on the PlayerState; the parts hang off the PAWN. Walking the wrong one finds
	// nothing and looks exactly like "this player owns no stickers".
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
	if (!Pawn) { return; }
	const UAFLCosmeticCatalogSubsystem* Cat = UAFLCosmeticCatalogSubsystem::Get(this);
	int32 Parts = 0;
	TArray<UChildActorComponent*> CACs;
	Pawn->GetComponents<UChildActorComponent>(CACs);
	for (UChildActorComponent* CAC : CACs)
	{
		if (AAFLCharacterPartActor* Part = Cast<AAFLCharacterPartActor>(CAC ? CAC->GetChildActor() : nullptr))
		{
			Part->ApplyStickerSet(Selection.StickerSet, Cat);
			++Parts;
		}
	}
	UE_LOG(LogAFLSkinDiag, Log, TEXT("%s[Sticker] RefreshStickers -> %d part(s), %d zone(s) set"),
		*AFLSkinDiag::Prefix(this), Parts, Selection.StickerSet.NumSet());
}
