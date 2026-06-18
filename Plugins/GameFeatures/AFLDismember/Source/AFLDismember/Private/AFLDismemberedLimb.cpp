// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDismemberedLimb.h"

#include "AFLDismember.h"
#include "AFLDismemberComponent.h"             // COMBAT-LOOT: owner self-retrieve -> RestoreZone(<zone>)
#include "Character/LyraHealthComponent.h"     // owner-death-vanish bind (mirrors the head loot-box)
#include "Components/StaticMeshComponent.h"
#include "Cosmetics/AFLSkinColorAsset.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"                // APawn (OwnerPawn) -- the wallet/controller reach now lives in the grant component
#include "Interaction/AFLGrabbableComponent.h" // the grab substrate the limb wears (AFLMovement)
#include "Loot/AFLLootGrantComponent.h"        // COMBAT-LOOT now flows through the shared grant component (Phase 1)
#include "Loot/AFLOverlapCollectComponent.h"   // E2: the enemy walk-over auto-collect substrate (the INSTANT-cache pattern)
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDismemberedLimb)

AAFLDismemberedLimb::AAFLDismemberedLimb()
{
	// The limb prop IS the base PartMesh (UStaticMeshComponent) -- the base ctor already set
	// SetSimulatePhysics(true) + PhysicsActor + bReplicates + SetReplicateMovement on it. We only set
	// the gib mesh when one is assigned (the per-limb BP child sets LimbGibMesh; the BP also sets the
	// PartMesh component-template static_mesh directly, since this ctor runs before the BP default is
	// applied). ApplyPopImpulse is overridden (velocity, not force) below. No roll-audio default (head-specific).
	if (UStaticMeshComponent* Body = GetPartMesh())
	{
		if (LimbGibMesh)
		{
			Body->SetStaticMesh(LimbGibMesh);
		}
		Body->SetCollisionProfileName(TEXT("PhysicsActor"));
		// E2 (align #3): OVERLAP the pawn (no shove -- the "pushing it away" bug) while still Block-ing the world
		// for the death-tumble. Override only the Pawn response; physics-sim + world collision are untouched.
		Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}

	// COMBAT-LOOT (clone of AAFLHeadLootBox): the severed limb PERSISTS to be retrieved -- override the
	// base 5s lifespan (limbs-only; the shared base AAFLDismemberedPart and the head keep their own).
	InitialLifeSpan = 0.0f;

	// The grab substrate marker + policy -- the limb IS retrievable loot (the existing discovery finds it
	// and offers the grab ability; the carrier attaches it to hand_r). BP child sets GrabAbility = GA_AFL_Grab_C.
	Grabbable = CreateDefaultSubobject<UAFLGrabbableComponent>(TEXT("Grabbable"));

	// The generalized loot grant (Loot Phase 1) -- shared with the head/caches. Configured at Initialize.
	LootGrant = CreateDefaultSubobject<UAFLLootGrantComponent>(TEXT("LootGrant"));

	// E2 overlap pivot: the ENEMY walk-over auto-collect trigger (the proven INSTANT-cache substrate -- a
	// QueryOnly sphere, no shove). Enemy-gated in IsViableCollector via the grant's ResolveRetrievalMode; magnet
	// off. Attached to the limb prop so it tracks the rolling gib.
	Overlap = CreateDefaultSubobject<UAFLOverlapCollectComponent>(TEXT("OverlapCollect"));
	Overlap->SetupAttachment(GetPartMesh());
	// E2 cause-B: the limb spawns ON the victim + pops/tumbles -- present + land for ~1.5s before it's collectible
	// (vs the watched instant point-blank absorb). Layered on the grant's bConfigured race fix (cause A).
	Overlap->ActivationDelay = 1.5f;
}

void AAFLDismemberedLimb::ApplyPopImpulse(const FVector& Impulse)
{
	// TUMBLE FIX (velocity, not force) -- IDENTICAL to AAFLDismemberedHead::ApplyPopImpulse. bVelChange=TRUE
	// makes the vector a TARGET VELOCITY (mass/scale-independent) rather than a force the base divides by the
	// gib's (small) mass. Watched in PIE: the base force-pop launched the light limb gib off-screen with no
	// visible tumble; this gives the same gentle pop+tumble the head gib gets. Pops the gib PartMesh (the gib
	// IS the physics body), not a cosmetic child.
	if (UStaticMeshComponent* Body = GetPartMesh())
	{
		Body->AddImpulse(Impulse, NAME_None, /*bVelChange=*/true);
	}
}

void AAFLDismemberedLimb::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Both identity layers replicate as content-asset pointers -> OnRep applies on every client + late-joiner.
	// MIRRORS AAFLDismemberedHead.
	DOREPLIFETIME(AAFLDismemberedLimb, PartSkinColor);
	DOREPLIFETIME(AAFLDismemberedLimb, PartMaterial);
}

void AAFLDismemberedLimb::BeginPlay()
{
	Super::BeginPlay();

	// Apply whatever identity already replicated (server set-then-spawn, or a late-join client that received
	// the replicated values before BeginPlay). Each OnRep re-runs the apply so the look completes whenever both
	// land. MIRRORS AAFLDismemberedHead::BeginPlay (no roll audio).
	ApplyLimbAppearance();

	// COMBAT-LOOT: react to our own pickup (server broadcasts OnGrabbedBy from GrabActor). Bind on every
	// machine; OnGrabbedBy guards on authority so only the server reattaches/grants. MIRRORS the head loot-box.
	if (Grabbable)
	{
		Grabbable->OnGrabbedBy.AddDynamic(this, &AAFLDismemberedLimb::OnGrabbedBy);
	}

	// Owner-branch seam: the grant component fires OnOwnerRetrieved when the OWNER retrieves -> reattach +
	// destroy (the component never references RestoreZone -- the dependency-inversion seam).
	if (LootGrant)
	{
		LootGrant->OnOwnerRetrieved.AddDynamic(this, &AAFLDismemberedLimb::HandleOwnerRetrieved);
		// C3: enemy-collect despawn (Decision B -- the gib disappears into the carried pool). The owner-collect
		// fires OnOwnerRetrieved instead (TryGrant returns before OnLootGranted), so this never fires for the owner.
		LootGrant->OnLootGranted.AddDynamic(this, &AAFLDismemberedLimb::HandleLootGranted);
	}

	// E2 overlap pivot: enemy walk-over auto-collect. The substrate fires OnCollected on authority only, for an
	// enemy-VIABLE collector (IsViableCollector consults the grant -> EnemyCollect; the owner is excluded).
	if (Overlap)
	{
		Overlap->OnCollected.AddDynamic(this, &AAFLDismemberedLimb::HandleOverlapCollected);
	}
}

void AAFLDismemberedLimb::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Grabbable)
	{
		Grabbable->OnGrabbedBy.RemoveDynamic(this, &AAFLDismemberedLimb::OnGrabbedBy);
	}

	// Drop the owner-death binding so a destroyed limb never fires into a stale health component.
	if (OwnerPawn.IsValid())
	{
		if (ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(OwnerPawn.Get()))
		{
			Health->OnDeathStarted.RemoveDynamic(this, &AAFLDismemberedLimb::OnOwnerDeathStarted);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AAFLDismemberedLimb::Initialize(APawn* InOwnerPawn, EAFLBodyZone InZone, int32 InLootWatts)
{
	// Server-authority: the SeverZone spawn path calls this right after SpawnActor on the server.
	// MIRRORS AAFLHeadLootBox::Initialize (with the limb's zone added for the owner-retrieve RestoreZone).
	if (!HasAuthority() || !InOwnerPawn)
	{
		return;
	}

	OwnerPawn = InOwnerPawn;
	LootZone  = InZone;

	// COMBAT-LOOT via the shared grant component (Loot Phase 1): grant InLootWatts (= head/8) to an ENEMY
	// collector; the OWNER (controller match) reattaches instead (HandleOwnerRetrieved -> RestoreZone(LootZone)).
	if (LootGrant)
	{
		// Loot-Carry Phase C3: the limb's value now enters the ENEMY retriever's CARRIED-at-risk pool (banked at
		// extract), NOT instant-bank Watts -- mirroring Phase B's cache flip + the head. Pass the limb's OWN gib
		// mesh (LimbGibMesh, the per-limb BP value) so the scattered loot reads as the real arm/leg gib.
		LootGrant->Configure(EAFLLootValueModel::CarryToExtractEnergy, InLootWatts, EAFLLootEligibility::EnemyOnly,
			InOwnerPawn, TEXT("limb-loot"), LimbGibMesh);
	}

	// "Tied to owner life": if the owner dies before this limb is collected, the limb vanishes.
	if (ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(InOwnerPawn))
	{
		Health->OnDeathStarted.AddDynamic(this, &AAFLDismemberedLimb::OnOwnerDeathStarted);
	}
}

void AAFLDismemberedLimb::OnGrabbedBy(AActor* Grabber)
{
	// Route retrieval to the shared grant component (Loot Phase 1): it runs the SAME decision flow this method
	// used inline (committed 9ac3e0ae) -- server-auth + grant-once + owner-seam (-> HandleOwnerRetrieved) +
	// eligibility (EnemyOnly) + the Watts grant. No behavior change; the logic is now generalized.
	if (LootGrant)
	{
		LootGrant->TryGrant(Grabber);
	}
}

void AAFLDismemberedLimb::HandleOwnerRetrieved(AActor* Retriever)
{
	// The OWNER picked up their own limb -> reattach (RestoreZone is zone-generic: un-hide bone + revert the
	// consequence GE + refill zone HP) and destroy this prop. Fired by the grant component's owner-seam on
	// authority; the component itself never references RestoreZone (the dependency-inversion that keeps it generic).
	if (UAFLDismemberComponent* Dismember = OwnerPawn.IsValid() ? OwnerPawn->FindComponentByClass<UAFLDismemberComponent>() : nullptr)
	{
		Dismember->RestoreZone(LootZone);
	}
	UE_LOG(LogAFLDismember, Display,
		TEXT("[AFLDismember] limb loot self-retrieved by %s -> RestoreZone(zone=%d) + destroy (no Watts)"),
		*GetNameSafe(Retriever), static_cast<int32>(LootZone));
	Destroy();
}

void AAFLDismemberedLimb::HandleLootGranted(AActor* Retriever, int32 Value)
{
	// C3 carry-model: an ENEMY collected the limb -> its value entered their carried-at-risk pool (via the
	// collect-channel + the grant's CarryToExtractEnergy case). The physical limb now DESPAWNS -- invisible while
	// carried (Decision B); it reappears as the real arm/leg gib only if that pool scatters. The owner branch
	// (OnOwnerRetrieved -> RestoreZone + Destroy) is separate + untouched -- OnLootGranted never fires for the owner.
	UE_LOG(LogAFLDismember, Display,
		TEXT("[AFLDismember] limb loot collected by ENEMY %s (+%d to carried pool) -> despawn"),
		*GetNameSafe(Retriever), Value);
	Destroy();
}

EAFLRetrievalMode AAFLDismemberedLimb::ResolveRetrievalMode(const AActor* Grabber) const
{
	// C3: forward to the grant's owner-vs-enemy SSOT -- the grab ability queries this before the mechanism fork
	// (owner -> instant CarryObject reattach, untouched; enemy -> the collect-channel).
	return LootGrant ? LootGrant->ResolveRetrievalMode(Grabber) : EAFLRetrievalMode::Ineligible;
}

void AAFLDismemberedLimb::HandleOverlapCollected(AActor* Collector)
{
	// E2: an ENEMY walked over the limb (the overlap is enemy-gated -> the owner never reaches here). Route to the
	// SAME grant flow the grab uses -> GrantValue (+carried pool) + OnLootGranted -> HandleLootGranted (despawn).
	// Grant-once guards a double-pay if a grab also fires. MIRRORS the head loot-box's HandleOverlapCollected.
	if (LootGrant)
	{
		LootGrant->TryGrant(Collector);
	}
}

void AAFLDismemberedLimb::OnOwnerDeathStarted(AActor* OwningActor)
{
	if (!HasAuthority())
	{
		return;
	}

	// The owner died before retrieval -> the limb is no longer claimable loot / reattachable. Uncollected =
	// the grant component has not spent (no enemy collected); -> vanish.
	if (!LootGrant || !LootGrant->IsSpent())
	{
		UE_LOG(LogAFLDismember, Display,
			TEXT("[AFLDismember] limb loot owner %s died uncollected -> destroy"), *GetNameSafe(OwningActor));
		Destroy();
	}
}

void AAFLDismemberedLimb::SetPartSkinColor(UAFLSkinColorAsset* InColor)
{
	// Server-authority: set the replicated value; replication -> OnRep on clients -> apply. Apply locally too
	// (the server is a client on a listen-server and gets no OnRep for its own write). MIRRORS the head.
	if (!HasAuthority())
	{
		return;
	}
	PartSkinColor = InColor;
	ApplyLimbAppearance();
}

void AAFLDismemberedLimb::SetPartMaterial(UMaterialInstanceConstant* InMaterial)
{
	// Server-authority: mirror SetPartSkinColor. InMaterial is the victim's per-skin slot-1 base MIC (a content
	// asset -> replicates safely); UAFLDismemberComponent resolved it from GetMaterial(1)->Parent at spawn.
	if (!HasAuthority())
	{
		return;
	}
	PartMaterial = InMaterial;
	ApplyLimbAppearance();
}

void AAFLDismemberedLimb::OnRep_PartSkinColor()
{
	ApplyLimbAppearance();
}

void AAFLDismemberedLimb::OnRep_PartMaterial()
{
	ApplyLimbAppearance();
}

void AAFLDismemberedLimb::ApplyLimbAppearance()
{
	// Runs on every client. TWO layers, mirroring the live robot exactly (and AAFLDismemberedHead::ApplyHeadAppearance):
	//   1) assign the victim's base MIC (PartMaterial) to EVERY slot; then
	//   2) drive the color params on TOP via the proven AAFLCharacterPartActor::ApplySkinColor pattern --
	//      per-slot CreateAndSetMaterialInstanceDynamic, then SetScalar/Vector/Texture from the color asset.
	// OnRep ORDERING: PartMaterial + PartSkinColor replicate independently; this re-runs on each OnRep so whichever
	// lands second completes the look. Each layer is null-guarded so a partial arrival shows the best available
	// state and the second OnRep fills the rest. Idempotent.
	UStaticMeshComponent* Body = GetPartMesh();
	if (!Body)
	{
		return;
	}

	const int32 NumSlots = Body->GetNumMaterials();

	// (1) Base material: assign the victim MIC to every slot so the whole gib reads as that robot's limb.
	if (PartMaterial)
	{
		for (int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex)
		{
			Body->SetMaterial(SlotIndex, PartMaterial);
		}
	}

	// (2) Color params on top. Require BOTH: the color params are M_Mannequin params that only mean anything over
	// the MIC base -- tinting them over the gib's default material is a no-op, so wait for the MIC. The two-OnRep
	// ordering resolves here, identical end state either order. MIRRORS the head.
	if (PartMaterial && PartSkinColor)
	{
		for (int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex)
		{
			UMaterialInstanceDynamic* MID = Body->CreateAndSetMaterialInstanceDynamic(SlotIndex);
			if (!MID)
			{
				continue;
			}
			for (const TPair<FName, float>& KV : PartSkinColor->GetScalars())
			{
				MID->SetScalarParameterValue(KV.Key, KV.Value);
			}
			for (const TPair<FName, FLinearColor>& KV : PartSkinColor->GetColors())
			{
				MID->SetVectorParameterValue(KV.Key, FVector(KV.Value));
			}
			for (const TPair<FName, TObjectPtr<UTexture>>& KV : PartSkinColor->GetTextures())
			{
				MID->SetTextureParameterValue(KV.Key, KV.Value);
			}
		}
	}

	UE_LOG(LogAFLDismember, Display,
		TEXT("[AFLDismember] limb gib %s: appearance applied (material=%s color=%s slots=%d)"),
		*GetName(), *GetNameSafe(PartMaterial), *GetNameSafe(PartSkinColor), NumSlots);
}
