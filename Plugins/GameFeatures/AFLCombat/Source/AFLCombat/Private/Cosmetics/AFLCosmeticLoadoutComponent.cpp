// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLCosmeticLoadoutComponent.h"

#include "Cosmetics/AFLCosmeticServices.h"
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
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s[Loadout] ServerSetCosmeticSelection RX on %s: reqIdentity=%s/%s reqEdge=%s"),
			*AFLSkinDiag::Prefix(this), GetOwner() ? *GetOwner()->GetName() : TEXT("<no-owner>"),
			(Requested.IdentityType == EAFLIdentityType::Character) ? TEXT("Character") : TEXT("Team"),
			*Requested.GetActiveIdentityId().ToString(), *Requested.EdgeId.ToString());
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
	if (Requested.BeamId   != NAME_None && AxisEntitled(Requested.BeamId))   { NewSelection.BeamId   = Requested.BeamId;   }

	// Step 4 -- commit -> replicate (OnRep fires on clients; authority applies via the nudge below).
	Selection = NewSelection;

	if (bDiag)
	{
		// What landed after gating (edge is the wired axis -> what the controller push will resolve).
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s[Loadout] COMMITTED on %s: identity=%s/%s edge=%s (replicating)"),
			*AFLSkinDiag::Prefix(this), GetOwner() ? *GetOwner()->GetName() : TEXT("<no-owner>"),
			(Selection.IdentityType == EAFLIdentityType::Character) ? TEXT("Character") : TEXT("Team"),
			*Selection.GetActiveIdentityId().ToString(), *Selection.EdgeId.ToString());
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

void UAFLCosmeticLoadoutComponent::OnRep_Selection()
{
	// Remote clients: the selection value replicated in. The controller component is the authoritative
	// driver of the visual push (server-side at possess); this OnRep exists so client-side UI (wallet
	// preview, nameplates) can react to a selection change. No visual push from here -- cosmetics apply
	// through the proven server-authority SetSkinColor path, never client-side.
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
	// #43 permissive impl: no external source wired yet -> return null, and the call sites treat a null
	// source as "basics are owned" (the lambdas above short-circuit to allowed). When S-ECON-WALLET lands
	// it provides a real IAFLEntitlementSource (e.g. a GameState/subsystem impl) resolved here; the call
	// sites are unchanged. Kept as an explicit resolve point so the swap is one function, not a hunt.
	return nullptr;
}

IAFLCosmeticPersistence* UAFLCosmeticLoadoutComponent::GetPersistence() const
{
	// #43 stub: no persistence backend wired yet -> null (load/save no-op). The real stub impl
	// (in-memory / SaveGame) or PlayFab (Phase 3) is resolved here behind the SAME interface. One
	// function to change; the BeginPlay load + the RPC save call through the interface regardless.
	return nullptr;
}

FAFLPlayerId UAFLCosmeticLoadoutComponent::MakePlayerId() const
{
	// Opaque key from the PlayerState's net-id string (stub backing). #43 does not own cross-session
	// identity -- the account system / PlayFab fills the real backing in later, inside FAFLPlayerId,
	// with no call-site change (the wrapper is opaque).
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
			SkinCtrl->RefreshSkinForPawn(Pawn);
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
				SkinCtrl->RefreshSkinForPawn(NewPawn);
			}
		}
	}
}
