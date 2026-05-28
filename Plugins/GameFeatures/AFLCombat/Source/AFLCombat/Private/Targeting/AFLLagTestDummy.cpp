// Copyright C12 AI Gaming. All Rights Reserved.

#include "Targeting/AFLLagTestDummy.h"

#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "LagComp/AFLPawnHitboxHistoryComponent.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLLagTestDummy)


AAFLLagTestDummy::AAFLLagTestDummy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Moves every frame — the whole point of this actor vs the static AAFLDamageTarget.
	PrimaryActorTick.bCanEverTick = true;

	// Skeletal mesh root so UAFLPawnHitboxHistoryComponent has bones to sample.
	// SKM_Manny via FObjectFinder (skill trap #2: static + Succeeded() + full
	// /package.object path). The 8 default TrackedBones on the history component
	// match this skeleton, so all 8 resolve and the ring fills with real poses.
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyFinder(
		TEXT("/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny"));
	if (MannyFinder.Succeeded())
	{
		Mesh->SetSkeletalMesh(MannyFinder.Object);
	}

	// ASC owned by THIS actor (self-contained), not a PlayerState. Mirrors
	// AAFLDamageTarget exactly (which mirrors ALyraCharacterWithAbilities). Mixed
	// replication mode is the canonical Lyra choice for non-PlayerState-owned ASCs.
	AbilitySystemComponent = ObjectInitializer.CreateDefaultSubobject<ULyraAbilitySystemComponent>(
		this, TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// AttributeSet auto-detected by UAbilitySystemComponent::InitializeComponent's
	// GetObjectsWithOuter scan. Ctor seeds Health=100/MaxHealth=100/etc.
	CombatSet = CreateDefaultSubobject<UAFLAttributeSet_Combat>(TEXT("CombatSet"));

	// Hitbox history publisher. Self-registers with UAFLLagCompensationWorldSubsystem
	// in its own BeginPlay (server-only) and ticks TG_PostPhysics at 60Hz. Carrying
	// it here makes the dummy a registered rewind target with no GameFeature grant.
	HitboxHistory = CreateDefaultSubobject<UAFLPawnHitboxHistoryComponent>(TEXT("HitboxHistory"));

	// Lyra convention for ASC-bearing actors — matches AAFLDamageTarget.
	SetNetUpdateFrequency(100.0f);
}

void AAFLLagTestDummy::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Mirror AAFLDamageTarget: explicit re-bind of AbilityActorInfo with owner and
	// avatar both = this actor. InitializeComponent already did this, but the
	// belt-and-braces re-call matches the proven sibling.
	check(AbilitySystemComponent);
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

void AAFLLagTestDummy::BeginPlay()
{
	Super::BeginPlay();

	// Oscillate around wherever we were placed. Captured here (not in ctor) so the
	// designer's map placement is the sweep center.
	SpawnOrigin = GetActorLocation();
}

void AAFLLagTestDummy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Sinusoidal lateral (Y) sweep around the spawn origin. Lateral so a
	// forward-facing shooter sees the dummy cross left-to-right; that lateral
	// displacement over the rewind window is exactly what BM-0105c's latency
	// cohorts exercise (at 200ms RTT the dummy has moved meaningfully, so a hit
	// MUST be due to rewinding to the past pose).
	const UWorld* World = GetWorld();
	const float Time = World ? static_cast<float>(World->GetTimeSeconds()) : 0.0f;
	const float LateralOffset = FMath::Sin(Time * SweepFrequency) * SweepAmplitude;
	SetActorLocation(SpawnOrigin + FVector(0.0f, LateralOffset, 0.0f));
}

UAbilitySystemComponent* AAFLLagTestDummy::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
