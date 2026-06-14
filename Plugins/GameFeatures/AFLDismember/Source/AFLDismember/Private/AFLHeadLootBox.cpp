// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLHeadLootBox.h"

#include "AFLDismember.h"               // LogAFLDismember
#include "AFLDismemberComponent.h"
#include "AFLBodyZone.h"
#include "Character/LyraHealthComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Interaction/AFLGrabbableComponent.h"

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

void AAFLHeadLootBox::Initialize(APawn* InOwnerPawn)
{
	// Server-authority: the head-sever path (PHASE B-2) calls this right after SpawnActor on the server.
	if (!HasAuthority() || !InOwnerPawn)
	{
		return;
	}

	OwnerPawn = InOwnerPawn;

	// "Tied to owner life": if the owner dies before this head is collected, the head vanishes.
	if (ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(InOwnerPawn))
	{
		Health->OnDeathStarted.AddDynamic(this, &AAFLHeadLootBox::OnOwnerDeathStarted);
	}
}

void AAFLHeadLootBox::OnGrabbedBy(AActor* Grabber)
{
	// Authoritative consequence (RestoreZone / collect). The grab broadcast is server-side, but guard
	// regardless so a replicated/late bind can never double-fire the reattach on a client.
	if (!HasAuthority())
	{
		return;
	}

	const APawn* GrabberPawn = Cast<APawn>(Grabber);
	const AController* GrabberController = GrabberPawn ? GrabberPawn->GetController() : nullptr;
	const AController* OwnerController = OwnerPawn.IsValid() ? OwnerPawn->GetController() : nullptr;

	// SELF-retrieve: the decapitated owner picked up their own head -> reattach. Match by CONTROLLER
	// (the stable player identity) rather than the pawn pointer, so a respawned/replaced pawn still
	// counts as self; decap is survivable so normally GrabberPawn == OwnerPawn anyway.
	const bool bIsSelf = (OwnerController != nullptr) && (GrabberController == OwnerController);

	if (bIsSelf && OwnerPawn.IsValid())
	{
		if (UAFLDismemberComponent* Dismember = OwnerPawn->FindComponentByClass<UAFLDismemberComponent>())
		{
			Dismember->RestoreZone(EAFLBodyZone::Head);
		}
		UE_LOG(LogAFLDismember, Display,
			TEXT("[AFLDismember] head loot-box self-retrieved by %s -> RestoreZone(Head) + destroy"),
			*GetNameSafe(Grabber));
		Destroy();
		return;
	}

	// ENEMY collect: consume the head. Scoring/economy is reserved for P2 -- mark it and leave the
	// rest to the carry/throw flow (the enemy is now holding it; release drops it as a physics prop).
	bCollected = true;
	UE_LOG(LogAFLDismember, Display,
		TEXT("[AFLDismember] head loot-box collected by enemy %s (scoring reserved for P2)"),
		*GetNameSafe(Grabber));
}

void AAFLHeadLootBox::OnOwnerDeathStarted(AActor* OwningActor)
{
	if (!HasAuthority())
	{
		return;
	}

	// The owner died before retrieval -> the head is no longer claimable loot. If an enemy already
	// collected it (bCollected) it would have a different lifetime path; uncollected -> vanish.
	if (!bCollected)
	{
		UE_LOG(LogAFLDismember, Display,
			TEXT("[AFLDismember] head loot-box owner %s died uncollected -> destroy"), *GetNameSafe(OwningActor));
		Destroy();
	}
}
