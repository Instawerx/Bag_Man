// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLHeadLootBox.h"

#include "AFLDismember.h"               // LogAFLDismember
#include "AFLDismemberComponent.h"
#include "AFLBodyZone.h"
#include "Character/LyraHealthComponent.h"
#include "GameFramework/Pawn.h"
#include "Interaction/AFLGrabbableComponent.h"
#include "Loot/AFLLootGrantComponent.h"     // COMBAT-LOOT now flows through the shared grant component (Phase 1)

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
		LootGrant->Configure(EAFLLootValueModel::Watts, InLootWatts, EAFLLootEligibility::EnemyOnly,
			InOwnerPawn, TEXT("head-loot"));
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

bool AAFLHeadLootBox::IsCollected() const
{
	return LootGrant && LootGrant->IsSpent();
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
