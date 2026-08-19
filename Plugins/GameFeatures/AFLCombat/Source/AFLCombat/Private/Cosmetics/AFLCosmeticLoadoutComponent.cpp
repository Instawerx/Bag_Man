// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLCosmeticLoadoutComponent.h"

#include "Cosmetics/AFLCosmeticServices.h"
#include "Cosmetics/AFLEconomyPersistenceSubsystem.h"  // Phase A0: local SaveGame persistence -- the GetPersistence() swap point
#include "AFLOnlineSubsystem.h"                         // A1.1: PlayFabId = the durable account key for MakePlayerId
#include "Cosmetics/AFLWalletComponent.h"             // S-ECON-WALLET: the real IAFLEntitlementSource (layer b)
#include "AFLCombat.h"                                   // LogAFLCombat (the AFL_TEST emit category)
#include "Cosmetics/AFLSkinColorComponent.h"          // AFLSkinDiag (shared cvar-gated diag: LogAFLSkinDiag / IsOn / Prefix)
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
	DOREPLIFETIME(UAFLCosmeticLoadoutComponent, BuildSet);
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

namespace AFLCreatorGamut
{
	// CC-2.1 neon gamut bounds -- the SINGLE source (tune here, never scatter magic numbers at call sites). The
	// X-body master (M_AFL_Character) is emissive-heavy, so a low-saturation / low-value pick reads as muddy
	// "no colour"; these floors keep a creator choice legibly neon. Value is also ceiling-clamped to full neon.
	static constexpr float MinSaturation = 0.55f;  // saturation floor: no washed-out greys
	static constexpr float MinValue      = 0.45f;  // value floor: no near-black
	static constexpr float MaxValue      = 1.00f;  // value ceiling: full neon brightness

	// Server-authoritative clamp of a requested colour into the neon gamut. HSV via FLinearColor helpers
	// (R=Hue[deg], G=Saturation[0-1], B=Value[0-1]); hue is preserved, S/V are floored/ceilinged.
	static FLinearColor ClampToNeon(const FLinearColor& In)
	{
		FLinearColor HSV = In.LinearRGBToHSV();
		HSV.G = FMath::Max(HSV.G, MinSaturation);
		HSV.B = FMath::Clamp(HSV.B, MinValue, MaxValue);
		FLinearColor Out = HSV.HSVToLinearRGB();
		Out.A = 1.0f;
		return Out;
	}
}

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
		}
		// unentitled + requested -> leave the prior committed overlay as-is (do not clear, do not write raw).
	}
	else
	{
		NewSelection.bUseCreatorColors = 0; // explicit "creator colours off" -> drop the overlay (back to preset).
	}

	// Step 4 -- commit -> replicate (OnRep fires on clients; authority applies via the nudge below).
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
}

FAFLColorOverride UAFLCosmeticLoadoutComponent::BuildColorOverride(const FAFLCosmeticSelection& Sel)
{
	// CC-2.1: the ONE construction of the creator overlay -- shared by RefreshSkinForPawn (step 5, server push) and
	// OnRep_Selection (step 6, client populate). Invalid unless bUseCreatorColors -> non-creator = no overlay.
	return Sel.bUseCreatorColors
		? FAFLColorOverride(Sel.CreatorBodyColor, Sel.CreatorEdgeColor, Sel.CreatorGlowColor)
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

bool UAFLCosmeticLoadoutComponent::ServerSaveBuild_Validate(FAFLCreatorBuild, int32) { return true; }

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
	Build.bReadOnly = false;

	if (BuildSet.Builds.IsValidIndex(Index))
	{
		BuildSet.Builds[Index] = Build;
	}
	else
	{
		// SLOT CAP DELIBERATELY NOT ENFORCED. How many slots a player gets is product intent -- the
		// pricing SSOT flags the $3 robot/slot collision as unresolved -- and the counted entitlement
		// that will carry it landed in CC-3.3. Enforcement arrives with that ruling; inventing a number
		// here would ship a cap nobody chose.
		Index = BuildSet.Builds.Add(Build);
	}
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[BUILD] saved index=%d builds=%d continuum=%d"),
		Index, BuildSet.Builds.Num(), Build.UsesContinuum() ? 1 : 0);
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
}

