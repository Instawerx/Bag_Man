// Copyright C12 AI Gaming. All Rights Reserved.

#include "Targeting/AFLTargetDummy.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Character/LyraHealthComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"   // CONVERGENCE: react binds THIS set's OnHealthChanged now
#include "Combat/AFLDeathComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "LagComp/AFLPawnHitboxHistoryComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLTargetDummy)

AAFLTargetDummy::AAFLTargetDummy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The reusable AFL death driver -- subobject so every placed dummy carries it. The AFL
	// combat set itself is granted by the experience AddAbilities entry keyed on this class
	// (DA_AFL_Combat_AbilitySet), the proven 1a path; the death component binds it at BeginPlay.
	DeathComponent = CreateDefaultSubobject<UAFLDeathComponent>(TEXT("AFLDeathComponent"));
	// TEST-RIG: hold the corpse longer than the gameplay default so the ragdoll visibly falls
	// before cleanup (operator's visible-death standard). Not a balance value -- test watchability.
	DeathComponent->SetDeathFinishDelay(3.0f);
	// The dummy is a transient target with no respawn flow to own its lifetime, so it must self-destruct
	// on death. The component CDO default is now FALSE (player-safe: respawn/Lyra teardown owns the player
	// pawn); the dummy opts back into TRUE here so its proven ragdoll-then-self-destruct death is unchanged.
	DeathComponent->SetDestroyOnDeathFinish(true);

	// TEST-RIG: hitbox-history publisher so the lag-comp rewind can read this dummy (BM-0105).
	// Self-registers with UAFLLagCompensationWorldSubsystem (server-only) in its own BeginPlay.
	HitboxHistory = CreateDefaultSubobject<UAFLPawnHitboxHistoryComponent>(TEXT("HitboxHistory"));

	// TEST-RIG: tick for the lateral sweep (server drives it; see Tick).
	PrimaryActorTick.bCanEverTick = true;

	// No AI; static target. AutoPossess stays disabled (set on the placed instance / BP).
	AutoPossessAI = EAutoPossessAI::Disabled;
}

void AAFLTargetDummy::BeginPlay()
{
	Super::BeginPlay();

	// Bind the per-hit react to the LYRA health set's OnHealthChanged (CONVERGENCE: Health lives on ULyraHealthSet
	// now -- the AFL set no longer drives it). Death is handled by UAFLDeathComponent off ULyraHealthSet::
	// OnOutOfHealth; this is only the cosmetic react. ASC is the self-owned ULyraASC from ALyraCharacterWithAbilities,
	// which creates ULyraHealthSet as a ctor subobject -- present now.
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(this))
	{
		ReactHealthSet = ASC->GetSet<ULyraHealthSet>();
		if (ReactHealthSet)
		{
			HealthChangedHandle = ReactHealthSet->OnHealthChanged.AddUObject(this, &ThisClass::HandleHealthChanged);
		}
	}

	// TEST-RIG visible death: bind our own ULyraHealthComponent's OnDeathStarted (driven by
	// UAFLDeathComponent -> StartDeath) ADDITIVELY -- Lyra's own handler still runs
	// (DisableMovementAndCollision); we add the ragdoll on top. The health component is a ctor
	// subobject (present now), and its delegate exists independent of the deferred AFL set.
	if (ULyraHealthComponent* HC = ULyraHealthComponent::FindHealthComponent(this))
	{
		HC->OnDeathStarted.AddDynamic(this, &ThisClass::HandleDeathStarted);
	}

	// TEST-RIG: capture the placed location as the lateral-sweep center (BM-0105).
	SpawnOrigin = GetActorLocation();
}

void AAFLTargetDummy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// TEST-RIG lateral sweep, AUTHORITY-ONLY (BM-0105 lag-comp watch). Driving SetActorLocation
	// on the server makes the SERVER position (what ConfirmHit rewinds against) sweep laterally;
	// it replicates to the client with latency, so under net emulation the client sees the dummy
	// at a different position than the server -- the divergence lag-comp exists to compensate.
	// Stops once dead (death disables collision/movement; a ragdolling corpse must not be teleported).
	if (!bEnableLateralSweep || !HasAuthority())
	{
		return;
	}
	if (const ULyraHealthComponent* HC = ULyraHealthComponent::FindHealthComponent(this))
	{
		if (HC->IsDeadOrDying())
		{
			return;
		}
	}

	const UWorld* World = GetWorld();
	const float Time = World ? static_cast<float>(World->GetTimeSeconds()) : 0.0f;
	const float LateralOffset = FMath::Sin(Time * SweepFrequency) * SweepAmplitude;
	SetActorLocation(SpawnOrigin + FVector(0.0f, LateralOffset, 0.0f));
}

void AAFLTargetDummy::HandleDeathStarted(AActor* /*OwningActor*/)
{
	// Ragdoll the mannequin so the kill is unmistakable on screen (SKM_Manny ships a physics
	// asset, so SetSimulatePhysics gives a real limp-fall). TEST-RIG, host-visible: this runs
	// wherever StartDeath fired (authority/listen-host); a networked dedicated-server build needs
	// this driven from OnRep_DeathState in a replicated death cue -- flagged, not built here.
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComp->SetAllBodiesSimulatePhysics(true);
		MeshComp->SetSimulatePhysics(true);
		MeshComp->WakeAllRigidBodies();
	}
}

void AAFLTargetDummy::HandleHealthChanged(AActor* /*EffectInstigator*/, AActor* /*EffectCauser*/, const FGameplayEffectSpec* /*EffectSpec*/, float /*EffectMagnitude*/, float OldValue, float NewValue)
{
	// CONVERGENCE: ULyraHealthSet fires OnHealthChanged with EffectMagnitude = the positive damage (the Damage meta),
	// NOT a signed Health delta -- so compute the real delta from Old/New and react only to a decrease (damage).
	const float Delta = NewValue - OldValue;
	if (Delta < 0.0f)
	{
		OnDamageReact(-Delta);   // hand the BP a positive damage value for the flash/montage
	}
}
