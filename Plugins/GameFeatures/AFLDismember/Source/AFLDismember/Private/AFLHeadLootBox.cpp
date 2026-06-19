// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLHeadLootBox.h"

#include "AFLDismember.h"               // LogAFLDismember
#include "AFLDismemberComponent.h"
#include "AFLBodyZone.h"
#include "Character/LyraHealthComponent.h"
#include "GameFramework/Pawn.h"
#include "Components/StaticMeshComponent.h" // E2: the gib body's pawn-collision override (no shove)
#include "Interaction/AFLGrabbableComponent.h"
#include "Loot/AFLLootGrantComponent.h"     // COMBAT-LOOT now flows through the shared grant component (Phase 1)
#include "Loot/AFLOverlapCollectComponent.h" // E2: the enemy walk-over auto-collect substrate (the INSTANT-cache pattern)

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLHeadLootBox)

AAFLHeadLootBox::AAFLHeadLootBox()
{
	// Inherit the proven head prop (SKM_Manny head-only skeletal mesh + victim skin color, roll audio,
	// PhysicsActor, snapshot replication). PERSIST: override the base 5s lifespan -- a loot-box waits.
	InitialLifeSpan = 0.0f;

	// The grab substrate marker + policy. This is the entire "make it carriable" cost -- the existing
	// discovery (line-trace + proximity) finds it and offers the grab ability; the carrier attaches it
	// to hand_r. The BP child sets GrabAbility = GA_AFL_Grab_C (and may tune the policy: a head is light).
	Grabbable = CreateDefaultSubobject<UAFLGrabbableComponent>(TEXT("Grabbable"));

	// The generalized loot grant (Loot Phase 1) -- eligibility + grant-once + the Watts grant, shared with
	// limbs/caches. Configured at Initialize; OnGrabbedBy routes retrieval to it.
	LootGrant = CreateDefaultSubobject<UAFLLootGrantComponent>(TEXT("LootGrant"));

	// E2 overlap pivot: the ENEMY walk-over auto-collect trigger (the proven INSTANT-cache substrate -- a
	// QueryOnly sphere, no shove). Enemy-gated in IsViableCollector via the grant's ResolveRetrievalMode; magnet
	// off (a head shouldn't fly at you). Attached to the head prop so it tracks the rolling gib.
	Overlap = CreateDefaultSubobject<UAFLOverlapCollectComponent>(TEXT("OverlapCollect"));
	Overlap->SetupAttachment(GetPartMesh());
	// E2 cause-B + presentation pass: the head presents + tumbles + lands before it's collectible. Primary signal
	// is LANDING (bArmOnSettle -> the overlap arms on the gib's physics sleep); ActivationDelay is the MAX-CAP
	// fallback (a gib that never sleeps). Layered on the grant's bConfigured race fix (cause A).
	Overlap->bArmOnSettle = true;
	Overlap->ActivationDelay = 3.0f;

	// E2 (align #3): the head physics body must OVERLAP the pawn (no shove -- the "pushing it away" bug) while
	// still Block-ing the world for the death-tumble. Override only the Pawn response on the inherited
	// PhysicsActor profile; physics-sim + world collision are untouched.
	if (UStaticMeshComponent* Body = GetPartMesh())
	{
		Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}
}

void AAFLHeadLootBox::BeginPlay()
{
	Super::BeginPlay();

	// React to our own pickup (server broadcasts OnGrabbedBy from GrabActor). Bind on every machine;
	// the dispatch body guards on authority so only the server reattaches/collects.
	if (Grabbable)
	{
		Grabbable->OnGrabbedBy.AddDynamic(this, &AAFLHeadLootBox::OnGrabbedBy);
	}

	// Owner-branch seam: the grant component fires OnOwnerRetrieved when the OWNER retrieves -> reattach +
	// destroy (the component never references RestoreZone -- the dependency-inversion seam).
	if (LootGrant)
	{
		LootGrant->OnOwnerRetrieved.AddDynamic(this, &AAFLHeadLootBox::HandleOwnerRetrieved);
		// C3: enemy-collect despawn (Decision B -- the gib disappears into the carried pool). The owner-collect
		// fires OnOwnerRetrieved instead (TryGrant returns before OnLootGranted), so this never fires for the owner.
		LootGrant->OnLootGranted.AddDynamic(this, &AAFLHeadLootBox::HandleLootGranted);
	}

	// E2 overlap pivot: enemy walk-over auto-collect. The substrate fires OnCollected on authority only, for an
	// enemy-VIABLE collector (IsViableCollector consults the grant -> EnemyCollect; the owner is excluded).
	if (Overlap)
	{
		Overlap->OnCollected.AddDynamic(this, &AAFLHeadLootBox::HandleOverlapCollected);
	}
}

void AAFLHeadLootBox::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Grabbable)
	{
		Grabbable->OnGrabbedBy.RemoveDynamic(this, &AAFLHeadLootBox::OnGrabbedBy);
	}

	// Drop the owner-death binding so a destroyed loot-box never fires into a stale health component.
	if (OwnerPawn.IsValid())
	{
		if (ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(OwnerPawn.Get()))
		{
			Health->OnDeathStarted.RemoveDynamic(this, &AAFLHeadLootBox::OnOwnerDeathStarted);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AAFLHeadLootBox::Initialize(APawn* InOwnerPawn, int32 InLootWatts)
{
	// Server-authority: the head-sever path (PHASE B-2) calls this right after SpawnActor on the server.
	if (!HasAuthority() || !InOwnerPawn)
	{
		return;
	}

	OwnerPawn = InOwnerPawn;

	// COMBAT-LOOT via the shared grant component (Loot Phase 1): the head grants InLootWatts to an ENEMY
	// collector; the OWNER (controller match) reattaches instead (HandleOwnerRetrieved), granted nothing.
	if (LootGrant)
	{
		// Loot-Carry Phase C3: the head's value now enters the ENEMY retriever's CARRIED-at-risk pool (banked at
		// extract), NOT instant-bank Watts -- mirroring Phase B's cache flip. Pass the head's OWN gib mesh
		// (HeadGibMesh, inherited from AAFLDismemberedHead) so the scattered loot reads as the real head gib.
		LootGrant->Configure(EAFLLootValueModel::CarryToExtractEnergy, InLootWatts, EAFLLootEligibility::EnemyOnly,
			InOwnerPawn, TEXT("head-loot"), GetHeadGibMesh());
	}

	// "Tied to owner life": if the owner dies before this head is collected, the head vanishes.
	if (ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(InOwnerPawn))
	{
		Health->OnDeathStarted.AddDynamic(this, &AAFLHeadLootBox::OnOwnerDeathStarted);
	}
}

void AAFLHeadLootBox::OnGrabbedBy(AActor* Grabber)
{
	// Route retrieval to the shared grant component (Loot Phase 1): it runs the SAME decision flow this method
	// used inline (committed 9ac3e0ae) -- server-auth + grant-once + owner-seam (-> HandleOwnerRetrieved) +
	// eligibility (EnemyOnly) + the Watts grant. No behavior change; the logic is now generalized.
	if (LootGrant)
	{
		LootGrant->TryGrant(Grabber);
	}
}

void AAFLHeadLootBox::HandleOwnerRetrieved(AActor* Retriever)
{
	// The decapitated OWNER picked up their own head -> reattach (un-hide + clear State.Decapitated -> camera
	// auto-pops + restore head-HP) and destroy this loot-box. Fired by the grant component's owner-seam on
	// authority; the component itself never references RestoreZone (the dependency-inversion that keeps it generic).
	if (UAFLDismemberComponent* Dismember = OwnerPawn.IsValid() ? OwnerPawn->FindComponentByClass<UAFLDismemberComponent>() : nullptr)
	{
		Dismember->RestoreZone(EAFLBodyZone::Head);
	}
	UE_LOG(LogAFLDismember, Display,
		TEXT("[AFLDismember] head loot-box self-retrieved by %s -> RestoreZone(Head) + destroy"),
		*GetNameSafe(Retriever));
	Destroy();
}

void AAFLHeadLootBox::HandleLootGranted(AActor* Retriever, int32 Value)
{
	// C3 carry-model: an ENEMY collected the head -> its value entered their carried-at-risk pool (via the
	// collect-channel + the grant's CarryToExtractEnergy case). The physical head now DESPAWNS -- invisible while
	// carried (Decision B); it reappears as the real head gib only if that pool scatters. The owner branch
	// (OnOwnerRetrieved -> RestoreZone + Destroy) is separate + untouched -- OnLootGranted never fires for the owner.
	UE_LOG(LogAFLDismember, Display,
		TEXT("[AFLDismember] head loot-box collected by ENEMY %s (+%d to carried pool) -> despawn"),
		*GetNameSafe(Retriever), Value);
	Destroy();
}

void AAFLHeadLootBox::HandleOverlapCollected(AActor* Collector)
{
	// E2: an ENEMY walked over the head (the overlap is enemy-gated -> the owner never reaches here). Route to the
	// SAME grant flow the grab uses -> GrantValue (+carried pool) + OnLootGranted -> HandleLootGranted (despawn).
	// Grant-once guards a double-pay if a grab also fires. MIRRORS the INSTANT cache's HandleCollected -> TryGrant.
	if (LootGrant)
	{
		LootGrant->TryGrant(Collector);
	}
}

bool AAFLHeadLootBox::IsCollected() const
{
	return LootGrant && LootGrant->IsSpent();
}

EAFLRetrievalMode AAFLHeadLootBox::ResolveRetrievalMode(const AActor* Grabber) const
{
	// C3: forward to the grant's owner-vs-enemy SSOT -- the grab ability queries this before the mechanism fork
	// (owner -> instant CarryObject reattach, untouched; enemy -> the collect-channel).
	return LootGrant ? LootGrant->ResolveRetrievalMode(Grabber) : EAFLRetrievalMode::Ineligible;
}

void AAFLHeadLootBox::OnOwnerDeathStarted(AActor* OwningActor)
{
	if (!HasAuthority())
	{
		return;
	}

	// The owner died before retrieval -> the head is no longer claimable loot. Uncollected = the grant
	// component has not spent (no enemy collected); -> vanish.
	if (!LootGrant || !LootGrant->IsSpent())
	{
		UE_LOG(LogAFLDismember, Display,
			TEXT("[AFLDismember] head loot-box owner %s died uncollected -> destroy"), *GetNameSafe(OwningActor));
		Destroy();
	}
}
