// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLHeadLootBox.h"

#include "AFLDismember.h"               // LogAFLDismember
#include "AFLDismemberComponent.h"
#include "AFLBodyZone.h"
#include "Character/LyraHealthComponent.h"
#include "Cosmetics/AFLWalletComponent.h"   // COMBAT-LOOT: enemy-collect Watts grant (EarnWattsAuthority)
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"      // grabber pawn -> PlayerState -> wallet
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

void AAFLHeadLootBox::Initialize(APawn* InOwnerPawn, int32 InLootWatts)
{
	// Server-authority: the head-sever path (PHASE B-2) calls this right after SpawnActor on the server.
	if (!HasAuthority() || !InOwnerPawn)
	{
		return;
	}

	OwnerPawn = InOwnerPawn;
	LootWatts = InLootWatts;   // COMBAT-LOOT: the enemy-collect grant value (head zone row's LootWatts).

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

	// Grant the loot EXACTLY ONCE per head: once an enemy has collected it, this is spent (mirrors the
	// limb fix). Without this a re-grab / drop+regrab pays +LootWatts again. Owner self-retrieve below
	// Destroys instead of setting bCollected, so the reattach path is unaffected.
	if (bCollected)
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

	// ENEMY collect: grant the head's COMBAT-LOOT Watts to the grabber's wallet (IRONICS_ECONOMY_SPEC.md
	// section 3a -- this CLOSES the former P2 "scoring reserved" stub). The owner self-retrieve above is
	// granted nothing. Reach the wallet the proven way (AFLAG_Extract): grabber pawn -> PlayerState ->
	// UAFLWalletComponent -> EarnWattsAuthority (the server-auth CommitMutation funnel; reason names the
	// diag line). The enemy is now holding the head; release drops it as a physics prop (carry flow owns it).
	if (GrabberPawn)
	{
		APlayerState* GrabberPS = GrabberPawn->GetPlayerState();
		if (UAFLWalletComponent* Wallet = GrabberPS ? GrabberPS->FindComponentByClass<UAFLWalletComponent>() : nullptr)
		{
			Wallet->EarnWattsAuthority(LootWatts, TEXT("head-loot"));
			UE_LOG(LogAFLDismember, Display,
				TEXT("[AFLDismember] AFL_HEADLOOT: +%d W -> %s (enemy head-collect)"), LootWatts, *GetNameSafe(Grabber));
		}
		else
		{
			UE_LOG(LogAFLDismember, Warning,
				TEXT("[AFLDismember] head loot-box collected by enemy %s but no wallet -- no Watts granted"),
				*GetNameSafe(Grabber));
		}
	}
	bCollected = true;
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
