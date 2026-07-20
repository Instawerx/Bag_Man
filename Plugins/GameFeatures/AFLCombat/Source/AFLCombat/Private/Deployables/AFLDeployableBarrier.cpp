// Copyright C12 AI Gaming. All Rights Reserved.

#include "Deployables/AFLDeployableBarrier.h"

#include "AbilitySystemComponent.h"                        // FOnAttributeChangeData, EGameplayEffectReplicationMode
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDeployableBarrier)

AAFLDeployableBarrier::AAFLDeployableBarrier()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);   // it's static once planted; only the spawn transform matters

	// Root = the wall. Placeholder cube mesh + material are set on the BP child (or swapped in at MEET);
	// this C++ only owns the collision contract. QueryAndPhysics + BlockAll = "passes nothing".
	BarrierMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrierMesh"));
	SetRootComponent(BarrierMesh);
	BarrierMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BarrierMesh->SetCollisionProfileName(FName(TEXT("BlockAll")));
	BarrierMesh->SetIsReplicated(true);

	// Self-owned ASC so AFL weapon-damage GEs have a valid target. Minimal replication: no player owns it,
	// clients only need the death/GC effects, not the full GE list (mirrors an AI/minion ASC).
	AbilitySystemComponent = CreateDefaultSubobject<ULyraAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// Attribute sets as default subobjects of the ASC's owner -> auto-registered on InitAbilityActorInfo
	// (the proven ALyraCharacterWithAbilities pattern). HealthSet = the ExecCalc's damage output target;
	// CombatSet = the Armor/Shield/zone captures (all default 0 -> every hit lands on Health).
	HealthSet = CreateDefaultSubobject<ULyraHealthSet>(TEXT("HealthSet"));
	CombatSet = CreateDefaultSubobject<UAFLAttributeSet_Combat>(TEXT("CombatSet"));
}

UAbilitySystemComponent* AAFLDeployableBarrier::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AAFLDeployableBarrier::BeginPlay()
{
	Super::BeginPlay();

	// Apply the (possibly designer-overridden) block profile.
	if (BarrierMesh)
	{
		BarrierMesh->SetCollisionProfileName(BlockProfileName);
	}

	if (!AbilitySystemComponent)
	{
		return;
	}

	// Owner + Avatar are both this self-contained actor.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	// The HP economy + the despawn timer are AUTHORITY-ONLY; Health replicates to clients, and Destroy()
	// replicates the removal, so clients follow without running this.
	if (HasAuthority())
	{
		// Seed HP from the knob (no init GE needed: CombatSet's Armor/Shield/zones default to 0 in its ctor,
		// so damage passes straight to Health). Base value = current value at spawn.
		AbilitySystemComponent->SetNumericAttributeBase(ULyraHealthSet::GetMaxHealthAttribute(), MaxHP);
		AbilitySystemComponent->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), MaxHP);

		// Break when Health crosses to <= 0 (the AAActor equivalent of ULyraHealthComponent's OnDeathStarted;
		// we bind the raw attribute-change since a bare actor has no health component).
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(ULyraHealthSet::GetHealthAttribute())
			.AddUObject(this, &AAFLDeployableBarrier::HandleHealthChanged);

		// Independent lifetime despawn (0 = never; destructible-only).
		if (Duration > 0.0f)
		{
			GetWorldTimerManager().SetTimer(
				LifetimeTimerHandle, this, &AAFLDeployableBarrier::HandleLifetimeExpired, Duration, /*bLoop=*/false);
		}
	}
}

void AAFLDeployableBarrier::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue <= 0.0f)
	{
		BreakBarrier(/*bDestroyed=*/true);
	}
}

void AAFLDeployableBarrier::HandleLifetimeExpired()
{
	BreakBarrier(/*bDestroyed=*/false);
}

void AAFLDeployableBarrier::BreakBarrier(bool bDestroyed)
{
	if (bBroken)
	{
		return;   // shot down at ~timeout, or the health delegate fired twice -> first wins
	}
	bBroken = true;

	GetWorldTimerManager().ClearTimer(LifetimeTimerHandle);

	// Stop blocking immediately so a broken wall no longer stops shots/bodies while the FX plays out.
	if (BarrierMesh)
	{
		BarrierMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Content-authored break FX (cue/Niagara/audio) in the BP child; then despawn. A short lifespan lets
	// the FX spawn/replicate before the actor is removed (Destroy replicates to clients).
	OnBarrierBroken(bDestroyed);
	SetLifeSpan(0.15f);
}
