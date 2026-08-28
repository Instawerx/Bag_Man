// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLHubZoneVolume.h"

#include "AFLHub.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLHubZoneVolume)

AAFLHubZoneVolume::AAFLHubZoneVolume()
{
	PrimaryActorTick.bCanEverTick = false; // pure overlap dispenser (extraction-zone posture).

	// Server-authoritative and invisible: the tag lands on the pawn's REPLICATED ASC, so the volume
	// itself has nothing to say to clients -- no bReplicates, no net traffic of its own.
	bReplicates = false;

	ZoneBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneBox"));
	SetRootComponent(ZoneBox);
	ZoneBox->InitBoxExtent(FVector(800.0f, 800.0f, 400.0f));
	ZoneBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ZoneBox->SetCollisionObjectType(ECC_WorldStatic);
	ZoneBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	ZoneBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AAFLHubZoneVolume::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	ZoneBox->OnComponentBeginOverlap.AddDynamic(this, &AAFLHubZoneVolume::OnZoneBeginOverlap);
	ZoneBox->OnComponentEndOverlap.AddDynamic(this, &AAFLHubZoneVolume::OnZoneEndOverlap);

	// Seed pawns already standing inside: overlap-begin only fires on transitions, and the plaza
	// spawn cluster sits INSIDE Hub.Zone.Main -- a freshly spawned pawn must not miss its tag.
	TArray<AActor*> Inside;
	ZoneBox->GetOverlappingActors(Inside, APawn::StaticClass());
	for (AActor* PawnActor : Inside)
	{
		TryApplyTo(PawnActor);
	}
}

void AAFLHubZoneVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveAllZoneEffects();
	Super::EndPlay(EndPlayReason);
}

void AAFLHubZoneVolume::OnZoneBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	TryApplyTo(OtherActor);
}

void AAFLHubZoneVolume::OnZoneEndOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	if (FActiveGameplayEffectHandle* Handle = ZoneEffectHandles.Find(OtherActor))
	{
		if (Handle->IsValid())
		{
			if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OtherActor))
			{
				ASC->RemoveActiveGameplayEffect(*Handle);
			}
		}
		ZoneEffectHandles.Remove(OtherActor);
		UE_LOG(LogAFLHub, Log, TEXT("AFL_HUB: %s exit zone %s -> %s removed."),
			*GetNameSafe(OtherActor), *GetName(), *ZoneTag.ToString());
	}
}

void AAFLHubZoneVolume::TryApplyTo(AActor* PawnActor)
{
	if (!HasAuthority() || !PawnActor || !PawnActor->IsA<APawn>() || ZoneEffectHandles.Contains(PawnActor))
	{
		return;
	}
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PawnActor);
	if (!ASC || !ZoneEffectClass || !ZoneTag.IsValid())
	{
		return;
	}

	// Apply-to-self through the target's ASC (the sibling's TryDispenseTo shape). The per-zone tag
	// rides the SPEC, not the GE asset: DynamicGrantedTags grant like asset GrantedTags but are
	// per-application, which is what lets ONE GE_AFL_Hub_Zone serve all eleven zones.
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(this, this);
	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(ZoneEffectClass, 1.0f, Context);
	if (SpecHandle.IsValid())
	{
		SpecHandle.Data->DynamicGrantedTags.AddTag(ZoneTag);
		const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		if (Handle.IsValid())
		{
			ZoneEffectHandles.Add(PawnActor, Handle);
			UE_LOG(LogAFLHub, Log, TEXT("AFL_HUB: %s enter zone %s -> %s applied."),
				*GetNameSafe(PawnActor), *GetName(), *ZoneTag.ToString());
		}
	}
}

void AAFLHubZoneVolume::RemoveAllZoneEffects()
{
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
}
