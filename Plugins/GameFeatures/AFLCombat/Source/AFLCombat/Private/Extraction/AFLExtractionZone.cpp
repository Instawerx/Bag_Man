// Copyright C12 AI Gaming. All Rights Reserved.

#include "Extraction/AFLExtractionZone.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SphereComponent.h"
#include "Effects/AFLGE_InExtractionZone.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLExtractionZone)

AAFLExtractionZone::AAFLExtractionZone()
{
	PrimaryActorTick.bCanEverTick = false; // pure overlap dispenser.

	// Pickup net posture minus motion: replicated from birth (BP visuals on every client),
	// static, no dormancy (always relevant, mutation-free), low update rate.
	bReplicates = true;
	SetNetUpdateFrequency(2.0f);

	ZoneSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ZoneSphere"));
	SetRootComponent(ZoneSphere);
	ZoneSphere->InitSphereRadius(300.0f);
	ZoneSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ZoneSphere->SetCollisionObjectType(ECC_WorldStatic);
	ZoneSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	ZoneSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	ZoneEffectClass = UAFLGE_InExtractionZone::StaticClass();
}

void AAFLExtractionZone::BeginPlay()
{
	Super::BeginPlay();

	// Server-only logic (the pickup's overlap guard shape) -- tags replicate down via the GE.
	if (HasAuthority())
	{
		ZoneSphere->OnComponentBeginOverlap.AddDynamic(this, &AAFLExtractionZone::OnZoneBeginOverlap);
		ZoneSphere->OnComponentEndOverlap.AddDynamic(this, &AAFLExtractionZone::OnZoneEndOverlap);

		// Catch pawns the zone spawned ON TOP OF (harness spawn-at-pawn; map-placed spawn points):
		// BeginOverlap only fires on transitions, so seed the already-inside set explicitly.
		TArray<AActor*> AlreadyInside;
		ZoneSphere->GetOverlappingActors(AlreadyInside, APawn::StaticClass());
		for (AActor* Inside : AlreadyInside)
		{
			OnZoneBeginOverlap(ZoneSphere, Inside, nullptr, 0, false, FHitResult());
		}
	}
}

void AAFLExtractionZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Remove every live handle -- a despawning zone must not strand State.InExtractionZone
	// (the grab EndPlay-funnel lesson).
	for (TPair<TWeakObjectPtr<AActor>, FActiveGameplayEffectHandle>& Pair : ZoneEffectHandles)
	{
		if (Pair.Key.IsValid() && Pair.Value.IsValid())
		{
			if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pair.Key.Get()))
			{
				ASC->RemoveActiveGameplayEffect(Pair.Value);
			}
		}
	}
	ZoneEffectHandles.Empty();
	Super::EndPlay(EndPlayReason);
}

void AAFLExtractionZone::OnZoneBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (!HasAuthority() || !OtherActor || !OtherActor->IsA<APawn>() || ZoneEffectHandles.Contains(OtherActor))
	{
		return;
	}
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OtherActor);
	if (!ASC || !ZoneEffectClass)
	{
		return;
	}

	// Apply-to-self through the target's ASC (the death-burst rail shape).
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(this, this);
	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(ZoneEffectClass, 1.0f, Context);
	if (SpecHandle.IsValid())
	{
		const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		if (Handle.IsValid())
		{
			ZoneEffectHandles.Add(OtherActor, Handle);
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_EXTRACT: %s entered zone %s -> State.InExtractionZone applied."),
				*GetNameSafe(OtherActor), *GetName());
		}
	}
}

void AAFLExtractionZone::OnZoneEndOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}
	FActiveGameplayEffectHandle Handle;
	if (ZoneEffectHandles.RemoveAndCopyValue(OtherActor, Handle) && Handle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OtherActor))
		{
			ASC->RemoveActiveGameplayEffect(Handle);
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_EXTRACT: %s left zone %s -> State.InExtractionZone removed."),
				*GetNameSafe(OtherActor), *GetName());
		}
	}
}
