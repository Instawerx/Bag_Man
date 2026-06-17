// Copyright C12 AI Gaming. All Rights Reserved.

#include "Loot/AFLOverlapCollectComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLOverlapCollectComponent)

// Mirrors the energy pickup's death-tag check (the canonical tag string).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Status_Death_OverlapCollect, "Status.Death");

UAFLOverlapCollectComponent::UAFLOverlapCollectComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Collect volume -- EXTRACTED from AAFLEnergyPickup's CollectSphere config (QueryOnly, pawn-overlap).
	InitSphereRadius(60.0f);
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SetGenerateOverlapEvents(true);
}

void UAFLOverlapCollectComponent::BeginPlay()
{
	Super::BeginPlay();

	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		OnComponentBeginOverlap.AddDynamic(this, &UAFLOverlapCollectComponent::OnBeginOverlap);
	}
	else
	{
		// Clients never run the magnet; replicated movement of the owner drives them.
		SetComponentTickEnabled(false);
	}
}

bool UAFLOverlapCollectComponent::IsViableCollector(const AActor* Candidate) const
{
	if (!Candidate || !Candidate->IsA<APawn>())
	{
		return false;
	}
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(const_cast<AActor*>(Candidate));
	if (!ASC)
	{
		return false;
	}
	// Dead pawns are invisible to BOTH magnet + collect (a death burst must never re-collect into its corpse).
	return !ASC->HasMatchingGameplayTag(TAG_Status_Death_OverlapCollect);
}

void UAFLOverlapCollectComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bCollected || MagnetRadius <= 0.0f)
	{
		return;   // magnet disabled (MagnetRadius<=0) -> pure overlap; collect still works via OnBeginOverlap
	}

	// Server-authoritative magnetic pull -- EXTRACTED VERBATIM from AAFLEnergyPickup::Tick. Nearest viable
	// pawn within MagnetRadius; VInterpTo-move the OWNER actor toward it; dormancy wake/sleep on the owner.
	const FVector MyLoc = Owner->GetActorLocation();
	AActor* Target = nullptr;
	float BestDistSq = MagnetRadius * MagnetRadius;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const float DistSq = static_cast<float>(FVector::DistSquared(It->GetActorLocation(), MyLoc));
		if (DistSq < BestDistSq && IsViableCollector(*It))
		{
			BestDistSq = DistSq;
			Target = *It;
		}
	}

	if (Target)
	{
		if (!bMagnetAwake)
		{
			bMagnetAwake = true;
			Owner->SetNetDormancy(DORM_Awake);
			Owner->FlushNetDormancy();
		}
		Owner->SetActorLocation(FMath::VInterpTo(MyLoc, Target->GetActorLocation(), DeltaTime, PullInterpSpeed));
	}
	else if (bMagnetAwake)
	{
		bMagnetAwake = false;
		Owner->SetNetDormancy(DORM_DormantAll);
	}
}

void UAFLOverlapCollectComponent::OnBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bCollected || !IsViableCollector(OtherActor))
	{
		return;
	}
	// One-shot: the consumer (bound to OnCollected) does the grant + consume (Destroy). Mirrors the energy
	// pickup's bCollected guard, now generic -- the substrate decides WHO collected; the consumer decides WHAT.
	bCollected = true;
	OnCollected.Broadcast(OtherActor);
}
