// Copyright C12 AI Gaming. All Rights Reserved.

#include "Deployables/AFLEMPDevice.h"

#include "AbilitySystemBlueprintLibrary.h"                  // GetAbilitySystemComponent(AActor*)
#include "AbilitySystemComponent.h"                         // FOnAttributeChangeData, EGameplayEffectReplicationMode, spec/context
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"                           // FOverlapResult (UE5.6 split out of EngineTypes)
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLEMPDevice)

// SetByCaller magnitude the device passes to DisableGE so the device's DisableDuration knob is the
// single source of truth for how long enemies stay disabled. Native-registered (no ini needed -- the
// same FNativeGameplayTag mechanism State.Weapon.Overheated uses). Author GE_AFL_EMP_Disable's
// Duration Magnitude as SetByCaller of THIS tag.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_Combat_EMPDuration, "Data.Combat.EMPDuration");

AAFLEMPDevice::AAFLEMPDevice()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);   // planted once on land; only the spawn transform matters

	// Root = a primitive collision box (NOT the skeletal mesh -- the harvested Tripmine has no
	// guaranteed simple collision). BlockAll = the barrier's proven AFL-weapon hittability so the
	// "shoot it during arm" counterplay lands. Box extent generously covers the ~20x4x21cm device;
	// the operator tunes it in the BP child once the reskin mesh is in.
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);
	CollisionBox->InitBoxExtent(FVector(12.0f, 12.0f, 14.0f));
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBox->SetCollisionProfileName(FName(TEXT("BlockAll")));
	CollisionBox->SetIsReplicated(true);

	// Visual mesh (SK_Tripmine reskin) under the box. Mesh + reactor MI are set on the BP child;
	// this C++ owns none of the look. NoCollision -> the box is the single hit target.
	DeviceMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("DeviceMesh"));
	DeviceMesh->SetupAttachment(CollisionBox);
	DeviceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Self-owned ASC so AFL weapon-damage GEs have a valid target. Minimal replication (no player owns
	// it; clients only need the death/GC effects, not the full GE list) -- mirrors an AI/minion ASC.
	AbilitySystemComponent = CreateDefaultSubobject<ULyraAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// Attribute sets as default subobjects of the ASC's owner -> auto-registered on InitAbilityActorInfo
	// (the proven ALyraCharacterWithAbilities pattern the barrier reuses). HealthSet = the ExecCalc's
	// damage output target; CombatSet = the Armor/Shield/zone captures (all default 0 -> hits land on Health).
	HealthSet = CreateDefaultSubobject<ULyraHealthSet>(TEXT("HealthSet"));
	CombatSet = CreateDefaultSubobject<UAFLAttributeSet_Combat>(TEXT("CombatSet"));
}

UAbilitySystemComponent* AAFLEMPDevice::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AAFLEMPDevice::BeginPlay()
{
	Super::BeginPlay();

	// Apply the (possibly designer-overridden) block profile.
	if (CollisionBox)
	{
		CollisionBox->SetCollisionProfileName(BlockProfileName);
	}

	// The TELEGRAPH runs on EVERY machine: BeginPlay fires on server + each client (the actor
	// replicates), so the BP emissive ramp + charge audio play everywhere with no authority gate. All
	// the GAMEPLAY below is authority-only (Health replicates; Destroy replicates the removal).
	OnArmStart();

	if (!AbilitySystemComponent)
	{
		return;
	}

	// Owner + Avatar are both this self-contained actor.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (HasAuthority())
	{
		// Seed HP from the knob (no init GE needed: CombatSet's Armor/Shield/zones default to 0, so
		// damage passes straight to Health). Base value = current value at spawn.
		AbilitySystemComponent->SetNumericAttributeBase(ULyraHealthSet::GetMaxHealthAttribute(), MaxHP);
		AbilitySystemComponent->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), MaxHP);

		// DESTROYABLE during arm = the counterplay. Health crossing <= 0 -> fizzle (no pulse).
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(ULyraHealthSet::GetHealthAttribute())
			.AddUObject(this, &AAFLEMPDevice::HandleHealthChanged);

		// Arm -> pulse after ArmTime (single-shot; the device is consumed by the pulse).
		GetWorldTimerManager().SetTimer(
			ArmTimerHandle, this, &AAFLEMPDevice::HandleArmComplete, ArmTime, /*bLoop=*/false);
	}
}

void AAFLEMPDevice::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue <= 0.0f)
	{
		Consume(/*bPulsed=*/false);   // shot down before the pulse -> fizzle, NO disable applied
	}
}

void AAFLEMPDevice::HandleArmComplete()
{
	if (bConsumed)
	{
		return;   // shot down at ~arm-completion, or a double-fire -> first wins
	}
	Pulse();
	Consume(/*bPulsed=*/true);
}

void AAFLEMPDevice::Pulse()
{
	UWorld* World = GetWorld();
	if (!World || !AbilitySystemComponent || !DisableGE)
	{
		return;
	}

	// Team source = the THROWER. The puck spawns this device with Instigator = the throwing pawn, so
	// GetInstigator() gives the team to filter against; friendlies (and neutral/unknown) are skipped.
	AActor* TeamSource = GetInstigator();
	ULyraTeamSubsystem* Teams = World->GetSubsystem<ULyraTeamSubsystem>();

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(FName(TEXT("AFLEMPPulse")), /*bTraceComplex=*/false, this);
	QueryParams.AddIgnoredActor(this);
	World->OverlapMultiByChannel(
		Overlaps, GetActorLocation(), FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(PulseRadius), QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!Target || Target == TeamSource)
		{
			continue;
		}

		// ENEMIES ONLY -- a valid team check that is NOT DifferentTeams (same team, or no team data)
		// is skipped, so friendlies are never disabled.
		if (Teams && TeamSource
			&& Teams->CompareTeams(TeamSource, Target) != ELyraTeamComparison::DifferentTeams)
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (!TargetASC)
		{
			continue;
		}

		FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
		Context.AddInstigator(TeamSource ? TeamSource : this, this);   // credit the thrower
		const FGameplayEffectSpecHandle Spec =
			AbilitySystemComponent->MakeOutgoingSpec(DisableGE, 1.0f, Context);
		if (Spec.IsValid())
		{
			// Device knob -> the disable length (GE authors Duration = SetByCaller of this tag). The GE
			// applies to the TARGET's own ASC, so its granted State.Weapon.Disabled replicates to the
			// victim's client for free -> client-side ActivationBlockedTags blocks their fire too.
			Spec.Data->SetSetByCallerMagnitude(TAG_Data_Combat_EMPDuration, DisableDuration);
			AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
		}
	}

	// Replicated burst FX (BP cue); the disable is already applied above.
	OnPulse();
}

void AAFLEMPDevice::Consume(bool bPulsed)
{
	if (bConsumed)
	{
		return;   // one pulse or one death -> exactly one consume
	}
	bConsumed = true;

	GetWorldTimerManager().ClearTimer(ArmTimerHandle);

	// Stop being a hittable/blocking obstacle immediately.
	if (CollisionBox)
	{
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (!bPulsed)
	{
		OnDeviceFizzled();   // shot down -> the fizzle cue (OnPulse already fired on the pulse path)
	}

	// Short lifespan lets the pulse/fizzle cue spawn + replicate before the actor (and its arm audio)
	// are removed (Destroy replicates the removal to clients).
	SetLifeSpan(0.15f);
}
