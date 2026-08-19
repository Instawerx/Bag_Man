// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLSkinColorComponent.h"

#include "Components/ChildActorComponent.h"
#include "Cosmetics/AFLCharacterPartActor.h"
#include "Cosmetics/AFLSkinColorAsset.h"
#include "Materials/MaterialInstanceConstant.h"   // the equipped facemask MIC (replication-safe content asset)
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"   // SkinDiag prefix: resolve the LOCAL player
#include "GameFramework/PlayerState.h"        //  -> its replicated PlayerId, the two-client discriminator
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "Equipment/LyraEquipmentManagerComponent.h"   // weapon-skin: find the equipped weapon instance
#include "Equipment/LyraEquipmentInstance.h"            //  -> its spawned actor
#include "Weapons/LyraRangedWeaponInstance.h"           // the ranged-weapon instance type (AFL weapons derive from it)
#include "Components/SkeletalMeshComponent.h"           //  -> SetMaterial on the weapon mesh's slots
#include "UObject/UnrealType.h"                          // beam-color: reflection read/write of LaserTintColor + bLockedSignatureBeam on the weapon instance

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLSkinColorComponent)

// ---- Skin race diagnostics (cvar-gated, OFF by default) -----------------------------------------
DEFINE_LOG_CATEGORY(LogAFLSkinDiag);

static TAutoConsoleVariable<int32> CVarAFLSkinDiag(
	TEXT("afl.SkinDiag"),
	0,
	TEXT("1=log skin color apply/replication for race diagnosis (PATH 1/2 + controller). 0=silent (default)."),
	ECVF_Default);

namespace AFLSkinDiag
{
	bool IsOn()
	{
		return CVarAFLSkinDiag.GetValueOnGameThread() > 0;
	}

	FString Prefix(const UObject* WorldContext)
	{
		// Resolve the world's net mode to a SHORT tag so two-client logs are orderable by eye.
		// SRV = listen-host / authority world; CLI = remote client; STD = standalone; SVR-D = dedicated.
		const TCHAR* Role = TEXT("???");
		if (const UObject* Ctx = WorldContext)
		{
			if (const UWorld* World = Ctx->GetWorld())
			{
				switch (World->GetNetMode())
				{
				case NM_ListenServer:    Role = TEXT("SRV");   break;
				case NM_Client:          Role = TEXT("CLI");   break;
				case NM_Standalone:      Role = TEXT("STD");   break;
				case NM_DedicatedServer: Role = TEXT("SVR-D"); break;
				default:                 Role = TEXT("???");   break;
				}
			}
		}
		// PIE NAMESPACE COLLISION -- the reason this tag carries a player id at all. Two PIE clients share
		// EVERY identity field: same world name (L_Arena_04), same PlayerState name (LyraPlayerState_0), same
		// pawn instance name. Net mode does not separate them either -- BOTH are NM_Client, so both tag [CLI].
		// Without a per-client discriminator, two-client logs interleave INDISTINGUISHABLY, and a reader
		// attributing an emit to the wrong client gets a confident wrong answer rather than an obvious failure.
		// PlayerId is server-assigned and replicated -> stable and distinct per client. Resolved off the
		// world's FIRST PlayerController, which on a CLIENT is the LOCAL player (a client holds no PCs for
		// remote players). So this id answers "WHOSE LOG IS THIS", not "whose pawn is this": in B's world an
		// emit for A's part still tags B. That is the intended pairing -- id identifies the OBSERVER, the
		// owner name in each emit identifies the OBSERVED actor, and within one client's log the two pawns
		// have distinct names even though those names collide ACROSS worlds.
		int32 LocalId = -1;
		if (const UObject* Ctx = WorldContext)
		{
			if (const UWorld* World = Ctx->GetWorld())
			{
				if (const APlayerController* PC = World->GetFirstPlayerController())
				{
					if (const APlayerState* PS = PC->PlayerState)
					{
						LocalId = PS->GetPlayerId();
					}
				}
			}
		}
		return FString::Printf(TEXT("[SkinDiag][%s][pid=%d][f=%llu] "), Role, LocalId, (uint64)GFrameCounter);
	}
}
// -------------------------------------------------------------------------------------------------

UAFLSkinColorComponent::UAFLSkinColorComponent()
{
	// No replicated base to inherit from (standalone UActorComponent) -> WE must enable replication so
	// SkinColor replicates. Do NOT omit -- without it, SkinColor never reaches clients and convergence
	// is silently zero (the "compiles but doesn't replicate" trap).
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UAFLSkinColorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAFLSkinColorComponent, SkinColor);
	DOREPLIFETIME(UAFLSkinColorComponent, BodyColor);
	DOREPLIFETIME(UAFLSkinColorComponent, Facemask);
	DOREPLIFETIME(UAFLSkinColorComponent, WeaponSkinMaterial);
	DOREPLIFETIME(UAFLSkinColorComponent, BeamColorAsset);
	// Creator overlay rides the SAME pawn-side replication spine as the five above -- that is what lets the
	// part self-apply it at BeginPlay (PATH 1) instead of depending on PlayerState ordering.
	DOREPLIFETIME(UAFLSkinColorComponent, ActiveColorOverride);
}

void UAFLSkinColorComponent::BeginPlay()
{
	Super::BeginPlay();

	// Reconcile: if a color + parts are BOTH already present when we begin play (late join, or color set
	// before our BeginPlay), apply now. Idempotent + null-guarded -> safe no-op otherwise.
	ReapplyColorToAllParts();

	// Same reconcile for the body finish (the TeamColor axis) -- re-applies body THEN edge (composition order).
	ReapplyBodyColorToAllParts();

	// Same reconcile for the facemask (a part arriving after the replicated facemask value picks it up here).
	ReapplyFacemaskToAllParts();

	// Same reconcile for the weapon-skin (a weapon equipped/replicated after our BeginPlay picks it up here).
	ApplyWeaponSkinToEquipped();

	// Same reconcile for the beam color (the INDEPENDENT BeamId axis) -- a weapon equipped/replicated after our
	// BeginPlay picks up the selected beam tint here.
	ApplyBeamColorToEquipped();
}

void UAFLSkinColorComponent::SetSkinColor(UAFLSkinColorAsset* NewColor)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		SkinColor = NewColor;

		// Listen-host: the authority is also a client, but OnRep does NOT fire on the authority --
		// apply locally now so the host's own view updates immediately.
		ReapplyColorToAllParts();
	}
}

void UAFLSkinColorComponent::OnRep_SkinColor()
{
	// PATH 2 of 2 (covers COLOR-ARRIVES-SECOND): the color value replicated in. Push to already-spawned
	// parts that read null/stale at their BeginPlay. LOAD-BEARING -- without this, color-after-part desyncs.
	if (AFLSkinDiag::IsOn())
	{
		// On a LATE client this firing proves Race C: SkinColor arrived in the join-in-progress bunch.
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : OnRep_SkinColor fired: color=%s"),
			*AFLSkinDiag::Prefix(this),
			GetOwner() ? *GetOwner()->GetName() : TEXT("<no-owner>"),
			SkinColor ? *SkinColor->GetName() : TEXT("null"));
	}
	ReapplyColorToAllParts();
}

void UAFLSkinColorComponent::ReapplyColorToAllParts()
{
	if (SkinColor == nullptr)
	{
		return; // guard: nothing to apply yet
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<UChildActorComponent*> ChildActorComps;
	Owner->GetComponents<UChildActorComponent>(ChildActorComps);

	const bool bDiag = AFLSkinDiag::IsOn();
	int32 NumPartsFound = 0;
	for (UChildActorComponent* CAC : ChildActorComps)
	{
		// EDIT 1 FILTER (by-construction): only OUR body parts. Non-body child-actors (e.g. a weapon)
		// are not AAFLCharacterPartActor -> Cast returns null -> skipped. Skin never bleeds onto them.
		if (AAFLCharacterPartActor* Part = Cast<AAFLCharacterPartActor>(CAC ? CAC->GetChildActor() : nullptr))
		{
			++NumPartsFound;
			if (bDiag)
			{
				UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s/%s : Reapply -> part"),
					*AFLSkinDiag::Prefix(this), *Owner->GetName(), *Part->GetName());
			}
			Part->ApplySkinColor(SkinColor, ActiveColorOverride); // idempotent (part's owned-MID create-once); CC-2.1 overlay passed in
		}
	}

	if (bDiag)
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : Reapply: found %d parts (color=%s)"),
			*AFLSkinDiag::Prefix(this), *Owner->GetName(), NumPartsFound,
			SkinColor ? *SkinColor->GetName() : TEXT("null"));
	}
}

// ---- BODY FINISH (replicated PARALLEL to SkinColor; the TeamColor axis of the unified identity) ---

void UAFLSkinColorComponent::SetBodyColor(UAFLSkinColorAsset* NewColor)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		BodyColor = NewColor;
		// Listen-host: OnRep does NOT fire on authority -> apply locally now (mirrors SetSkinColor).
		ReapplyBodyColorToAllParts();
	}
}

void UAFLSkinColorComponent::SetColorOverride(const FAFLColorOverride& NewOverride)
{
	// AUTHORITY-ONLY, mirroring SetSkinColor/SetBodyColor. The SERVER owns this write because it is the only
	// side that reliably holds BOTH the PlayerState (carrying the selection) and the pawn at once --
	// RefreshSkinForPawn runs off OnPossessedPawnChanged, exactly that moment. Clients receive it via
	// OnRep_ColorOverride; they never write it.
	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : SetColorOverride auth=%d inBValid=%d Body=(%.4f,%.4f,%.4f)"),
			*AFLSkinDiag::Prefix(this), GetOwner() ? *GetOwner()->GetName() : TEXT("<no-owner>"),
			(GetOwner() && GetOwner()->HasAuthority()) ? 1 : 0, NewOverride.bValid ? 1 : 0,
			NewOverride.BodyColor.R, NewOverride.BodyColor.G, NewOverride.BodyColor.B);
	}
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		// UNCHANGED-EARLY-OUT (restored; mirrors the one deleted with ApplyCreatorOverride in d68bccbe). If the
		// override is identical to what is already stored, do NOTHING. This is what keeps the NEVER-creator path --
		// invalid == invalid, written on every RefreshSkinForPawn -- doing ZERO WORK, not merely zero VISIBLE work.
		// The bUseCreatorColors == false regression guarantee has to hold structurally, not just observationally:
		// "nothing renders differently" is a testable claim, "nothing happens differently" is the one that lets
		// CC-2.1 ship without re-proving the shipped colour path, and that Stage B inherits when it touches this seam.
		// An unconditional re-apply for players who have never opened the creator broadens blast radius for no benefit.
		// A REAL change still falls through: creator ON (invalid->valid), OFF (valid->invalid -- the clear arm the P2
		// round-trip exercises), or a colour edit. The assignment is skipped with it: when equal it is a no-op, and an
		// equal assignment never marks the property dirty anyway (replication diffs against shadow state), so the
		// replicated value is unaffected either way. Late-arriving PARTS are NOT this function's job -- they self-apply
		// at BeginPlay via GetColorOverride() (PATH 1), which is precisely why skipping the re-apply here is safe.
		const bool bUnchanged =
			(!ActiveColorOverride.bValid && !NewOverride.bValid) ||
			(ActiveColorOverride.bValid == NewOverride.bValid &&
			 ActiveColorOverride.BodyColor == NewOverride.BodyColor &&
			 ActiveColorOverride.EdgeColor == NewOverride.EdgeColor &&
			 ActiveColorOverride.GlowColor == NewOverride.GlowColor);
		if (bUnchanged)
		{
			return;
		}

		ActiveColorOverride = NewOverride;
		// Listen-host: OnRep does NOT fire on the authority -> apply locally now (mirrors SetSkinColor).
		ReapplyBodyColorToAllParts();
	}
}

void UAFLSkinColorComponent::OnRep_ColorOverride()
{
	// PATH 2 for the overlay (covers OVERRIDE-ARRIVES-SECOND): re-apply to parts that read an invalid override
	// at their BeginPlay. Pairs with the part's PATH 1 self-apply; BOTH required, as for SkinColor/BodyColor.
	if (AFLSkinDiag::IsOn())
	{
		// parts= is load-bearing: a correct dispatch that finds ZERO parts lands nowhere, and without this a clean
		// OnRep followed by unchanged MIDs is ambiguous (it was the server's structural condition).
		int32 DiagParts = 0;
		if (const AActor* O = GetOwner())
		{
			TArray<UChildActorComponent*> CACs; O->GetComponents<UChildActorComponent>(CACs);
			for (const UChildActorComponent* C : CACs) { if (Cast<AAFLCharacterPartActor>(C ? C->GetChildActor() : nullptr)) { ++DiagParts; } }
		}
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : OnRep_ColorOverride netmode=%d bValid=%d parts=%d Body=(%.4f,%.4f,%.4f)"),
			*AFLSkinDiag::Prefix(this), GetOwner() ? *GetOwner()->GetName() : TEXT("<no-owner>"),
			GetWorld() ? (int32)GetWorld()->GetNetMode() : -1, ActiveColorOverride.bValid ? 1 : 0, DiagParts,
			ActiveColorOverride.BodyColor.R, ActiveColorOverride.BodyColor.G, ActiveColorOverride.BodyColor.B);
	}
	ReapplyBodyColorToAllParts();
}

void UAFLSkinColorComponent::OnRep_BodyColor()
{
	// PATH 2 (body-arrives-second): the body finish replicated in -> re-apply to already-spawned parts.
	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : OnRep_BodyColor fired: body=%s"),
			*AFLSkinDiag::Prefix(this),
			GetOwner() ? *GetOwner()->GetName() : TEXT("<no-owner>"),
			BodyColor ? *BodyColor->GetName() : TEXT("null"));
	}
	ReapplyBodyColorToAllParts();
}

void UAFLSkinColorComponent::ReapplyBodyColorToAllParts()
{
	// COMPOSITION LAYER: apply the body Finish FIRST (TeamColor + its emissive), THEN re-apply the edge SkinColor
	// (emissive overlays -> edge WINS the shared emissive keys; the Finish supplies the TeamColor). That is the
	// mix-and-match: body = Finish TeamColor, edge = Edge emissive. BodyColor may be null (no body) -> ApplySkinColor
	// early-returns (guarded no-op) and the edge still re-applies. Idempotent (the part's owned-MID create-once).
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<UChildActorComponent*> ChildActorComps;
	Owner->GetComponents<UChildActorComponent>(ChildActorComps);

	const bool bDiag = AFLSkinDiag::IsOn();
	int32 NumPartsFound = 0;
	for (UChildActorComponent* CAC : ChildActorComps)
	{
		// Same by-construction filter as ReapplyColorToAllParts: only OUR body parts (weapons cast to null).
		if (AAFLCharacterPartActor* Part = Cast<AAFLCharacterPartActor>(CAC ? CAC->GetChildActor() : nullptr))
		{
			++NumPartsFound;
			Part->ApplySkinColor(BodyColor, ActiveColorOverride); // body finish (TeamColor + emissive); null -> guarded no-op; CC-2.1 overlay -> TeamColor
			Part->ApplySkinColor(SkinColor, ActiveColorOverride); // edge overlays (restores the edge emissive on top); CC-2.1 overlay -> EdgeGlowColor/EmissiveColor
		}
	}

	if (bDiag)
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : ReapplyBody: found %d parts (body=%s edge=%s)"),
			*AFLSkinDiag::Prefix(this), *Owner->GetName(), NumPartsFound,
			BodyColor ? *BodyColor->GetName() : TEXT("null"),
			SkinColor ? *SkinColor->GetName() : TEXT("null"));
	}
}

// ---- FACEMASK (replicated PARALLEL to SkinColor; same two-path race-safe spine) ------------------

void UAFLSkinColorComponent::SetFacemask(UMaterialInstanceConstant* NewMaterial)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Facemask = NewMaterial;
		// Listen-host: OnRep does NOT fire on authority -> apply locally now (mirrors SetSkinColor).
		ReapplyFacemaskToAllParts();
	}
}

void UAFLSkinColorComponent::OnRep_Facemask()
{
	// PATH 2 (facemask-arrives-second): the facemask value replicated in -> swap slot-1 on already-spawned parts.
	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : OnRep_Facemask fired: mask=%s"),
			*AFLSkinDiag::Prefix(this),
			GetOwner() ? *GetOwner()->GetName() : TEXT("<no-owner>"),
			Facemask ? *Facemask->GetName() : TEXT("null"));
	}
	ReapplyFacemaskToAllParts();
}

void UAFLSkinColorComponent::ReapplyFacemaskToAllParts()
{
	// NOTE: NO early-return on null Facemask -- unlike SkinColor, a NULL facemask is a MEANINGFUL state (un-equip
	// -> restore the part's authored slot-1). ApplyFacemask(nullptr, ...) does the restore.
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<UChildActorComponent*> ChildActorComps;
	Owner->GetComponents<UChildActorComponent>(ChildActorComps);

	const bool bDiag = AFLSkinDiag::IsOn();
	int32 NumPartsFound = 0;
	for (UChildActorComponent* CAC : ChildActorComps)
	{
		// Same by-construction filter as ReapplyColorToAllParts: only OUR body parts (weapons cast to null).
		if (AAFLCharacterPartActor* Part = Cast<AAFLCharacterPartActor>(CAC ? CAC->GetChildActor() : nullptr))
		{
			++NumPartsFound;
			// Re-establish the FULL finish on the swapped material so the BODY is not STRANDED (Option B). The swap
			// DROPS our slot-1 MID, and slot 1 = M_HeadLegs is SHARED -- the body finish params live here too. The
			// old code re-layered ONLY the edge (SkinColor) -> the body was lost whenever THIS facemask reapply ran
			// AFTER the body reapply on a machine. That order differs server (BeginPlay) vs client (OnRep arrival),
			// which was the host/client HEAD SPLIT. Compose facemask -> BODY (TeamColor + emissive) -> EDGE (emissive
			// wins the shared keys). Each null -> ApplySkinColor early-returns (guard), so an unset axis is a no-op.
			Part->ApplyFacemask(Facemask, BodyColor, ActiveColorOverride); // swap slot 1 + re-layer finish [+ overlay]
			Part->ApplySkinColor(SkinColor, ActiveColorOverride); // edge overlays on top (emissive wins) [+ overlay]
		}
	}

	if (bDiag)
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : ReapplyFacemask: found %d parts (mask=%s color=%s)"),
			*AFLSkinDiag::Prefix(this), *Owner->GetName(), NumPartsFound,
			Facemask ? *Facemask->GetName() : TEXT("null"),
			SkinColor ? *SkinColor->GetName() : TEXT("null"));
	}
}

// ---- WEAPON SKIN (replicated PARALLEL to Facemask; same two-path race-safe spine, weapon-actor target) ----
// Completes the #43 WeaponId generalization: RefreshWeaponForPawn EQUIPS the weapon (fast-array replicated);
// this applies the selected COLOR MI to the equipped weapon mesh. MIRRORS the proven SetFacemask/OnRep spine
// exactly -- the only difference is the target (the equipped weapon actor's mesh vs the pawn's body parts).

void UAFLSkinColorComponent::SetWeaponSkin(UMaterialInstanceConstant* NewMaterial)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		WeaponSkinMaterial = NewMaterial;
		// Listen-host: OnRep does NOT fire on authority -> apply locally now (mirrors SetFacemask).
		ApplyWeaponSkinToEquipped();
	}
}

void UAFLSkinColorComponent::OnRep_WeaponSkin()
{
	// PATH 2 (weapon-skin-arrives-second): the MI replicated in -> apply to the already-spawned weapon mesh.
	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : OnRep_WeaponSkin fired: mi=%s"),
			*AFLSkinDiag::Prefix(this),
			GetOwner() ? *GetOwner()->GetName() : TEXT("<no-owner>"),
			WeaponSkinMaterial ? *WeaponSkinMaterial->GetName() : TEXT("null"));
	}
	ApplyWeaponSkinToEquipped();
}

void UAFLSkinColorComponent::ApplyWeaponSkinToEquipped()
{
	// GUARD: a null MI = NO override -> keep the weapon's baked default (UNLIKE facemask, where null = un-equip).
	if (WeaponSkinMaterial == nullptr)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Reach the equipped weapon the SAME way RefreshWeaponForPawn does: the pawn's equipment manager -> the
	// ranged-weapon instance (all AFL weapons derive from it) -> its spawned actor -> the SkeletalMesh.
	ULyraEquipmentManagerComponent* EquipMgr = Owner->FindComponentByClass<ULyraEquipmentManagerComponent>();
	if (!EquipMgr)
	{
		return; // equipment not ready this early -> the spine re-drives (idempotent), like RefreshWeaponForPawn
	}

	const bool bDiag = AFLSkinDiag::IsOn();
	int32 NumMeshesFound = 0;
	for (ULyraEquipmentInstance* Inst : EquipMgr->GetEquipmentInstancesOfType(ULyraRangedWeaponInstance::StaticClass()))
	{
		if (!Inst)
		{
			continue;
		}
		for (AActor* WeaponActor : Inst->GetSpawnedActors())
		{
			if (!WeaponActor)
			{
				continue;
			}
			if (USkeletalMeshComponent* Mesh = WeaponActor->FindComponentByClass<USkeletalMeshComponent>())
			{
				++NumMeshesFound;
				// The weapon SKIN colors the WHOLE weapon mesh (Body + Emitter -- BOTH are weapon-mesh slots). The
				// BEAM is the INDEPENDENT thing (the Niagara, driven by the BeamId axis / LaserTintColor) and is
				// already decoupled -- so coloring every weapon-mesh slot here never touches the beam.
				const int32 NumMats = Mesh->GetNumMaterials();
				for (int32 Slot = 0; Slot < NumMats; ++Slot)
				{
					Mesh->SetMaterial(Slot, WeaponSkinMaterial);
				}
			}
		}
	}

	if (bDiag)
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : ApplyWeaponSkin: found %d weapon-mesh(es) (mi=%s)"),
			*AFLSkinDiag::Prefix(this), *Owner->GetName(), NumMeshesFound,
			WeaponSkinMaterial ? *WeaponSkinMaterial->GetName() : TEXT("null"));
	}
}

// ---- INDEPENDENT BeamId axis (the 3rd axis: weapon + weapon-skin + beam) ------------------------
// Mirrors the WeaponSkin spine EXACTLY (replicated asset + OnRep + reconcile), but the apply writes the
// weapon INSTANCE's LaserTintColor (the beam seam) instead of the mesh material. The beam is DECOUPLED
// from the weapon skin: it is its own owned item and applies to ANY equipped weapon.

void UAFLSkinColorComponent::SetBeamColor(UAFLSkinColorAsset* NewBeamColor)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		BeamColorAsset = NewBeamColor;

		// Listen-host: OnRep does NOT fire on the authority -> apply locally now so the host's own beam updates.
		ApplyBeamColorToEquipped();
	}
}

void UAFLSkinColorComponent::OnRep_BeamColor()
{
	// PATH 2 (beam-color-arrives-second): the beam asset replicated in -> apply to the already-equipped weapon.
	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : OnRep_BeamColor fired: beam=%s"),
			*AFLSkinDiag::Prefix(this),
			GetOwner() ? *GetOwner()->GetName() : TEXT("<no-owner>"),
			BeamColorAsset ? *BeamColorAsset->GetName() : TEXT("null"));
	}
	ApplyBeamColorToEquipped();
}

void UAFLSkinColorComponent::ApplyBeamColorToEquipped()
{
	// GUARD: a null asset = NO beam override -> keep the weapon's DEFAULT beam (its authored LaserTintColor, or
	// the Niagara's authored colour when that is unset). This axis simply isn't driving; a real selection
	// overwrites LaserTintColor on the next apply.
	if (BeamColorAsset == nullptr)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// The beam tint lives in the asset's ColorParameters under "BeamColor" (the beam-axis contract; the skin
	// param maps are unused for a beam SKU, exactly as FacemaskMaterial is the facemask-only field). Force A=1
	// so ReadLaserTint treats it as a REAL tint (A<=0 is its "unset -> keep default" sentinel).
	FLinearColor Tint = BeamColorAsset->GetColors().FindRef(FName("BeamColor"));
	Tint.A = 1.0f;

	// Reach the equipped weapon the SAME way ApplyWeaponSkinToEquipped does: the pawn's equipment manager -> the
	// ranged-weapon instance(s). The INSTANCE (not the actor) carries LaserTintColor -- it is the ability's
	// SourceObject that AFLLaserVisualStatics::ReadLaserTint reflects, so we write that seam directly on it.
	ULyraEquipmentManagerComponent* EquipMgr = Owner->FindComponentByClass<ULyraEquipmentManagerComponent>();
	if (!EquipMgr)
	{
		return; // equipment not ready this early -> the spine re-drives (idempotent), like the skin path
	}

	const bool bDiag = AFLSkinDiag::IsOn();
	int32 NumApplied = 0;
	for (ULyraEquipmentInstance* Inst : EquipMgr->GetEquipmentInstancesOfType(ULyraRangedWeaponInstance::StaticClass()))
	{
		if (!Inst)
		{
			continue;
		}

		// SPECIAL-GUN LOCK: a weapon whose bLockedSignatureBeam bool is TRUE keeps its authored signature beam --
		// the BeamId override does NOT apply to it (reflection-read; absent property = false = not locked = apply).
		if (const FBoolProperty* LockProp =
				CastField<FBoolProperty>(Inst->GetClass()->FindPropertyByName(FName("bLockedSignatureBeam"))))
		{
			if (LockProp->GetPropertyValue_InContainer(Inst))
			{
				continue;
			}
		}

		// Reflection-WRITE LaserTintColor (mirrors ReadLaserTint's reflection-read -- the proven HOP3 write). The
		// beam re-reads it on the NEXT fire; there is no live-beam component to touch.
		if (const FStructProperty* TintProp =
				CastField<FStructProperty>(Inst->GetClass()->FindPropertyByName(FName("LaserTintColor"))))
		{
			if (TintProp->Struct == TBaseStructure<FLinearColor>::Get())
			{
				*TintProp->ContainerPtrToValuePtr<FLinearColor>(Inst) = Tint;
				++NumApplied;
			}
		}
	}

	if (bDiag)
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : ApplyBeamColor: wrote LaserTintColor to %d instance(s) (beam=%s rgb=%.2f,%.2f,%.2f)"),
			*AFLSkinDiag::Prefix(this), *Owner->GetName(), NumApplied,
			BeamColorAsset ? *BeamColorAsset->GetName() : TEXT("null"), Tint.R, Tint.G, Tint.B);
	}
}
