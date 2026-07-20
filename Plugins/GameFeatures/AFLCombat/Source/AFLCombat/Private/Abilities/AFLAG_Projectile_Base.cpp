// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLAG_Projectile_Base.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NativeGameplayTags.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAG_Projectile_Base)

// Native tags. Same CDO-safe native-static declaration Pulse uses (a RequestGameplayTag in a ctor
// ensure-fails before the per-plugin Tags/*.ini scan completes on a fresh boot -> editor crash). The
// FName value is the canonical string; the C++ symbol carries a file-specific _ProjBase suffix so the
// UBT Unity build's merged translation unit does not collide these with the laser abilities' symbols.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Ability_Projectile_ProjBase, "Ability.Projectile");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Firing_Projectile_ProjBase, "State.Firing.Projectile");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Carrying_ProjBase, "State.Carrying");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_ThrowRecovery_ProjBase, "State.Weapon.ThrowRecovery");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Match_Warmup_ProjBase, "State.Match.Warmup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Match_Ended_ProjBase, "State.Match.Ended");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputTag_Weapon_Fire_ProjBase, "InputTag.Weapon.Fire");

UAFLAG_Projectile_Base::UAFLAG_Projectile_Base()
{
	// Same net contract as the proven Pulse ability: the owner predicts activation (montage/cue feel);
	// the projectile spawns on authority only (below). InstancedPerActor for stable per-ASC state.
	ReplicationPolicy  = EGameplayAbilityReplicationPolicy::ReplicateNo;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// BOT-FIRE PARITY (mandatory): Lyra bots have no input -- ShooterCore BTS_Shoot sends a
	// GameplayEvent(InputTag.Weapon.Fire) to the pawn. Without a matching GameplayEvent trigger, bots
	// cannot fire (0 triggers vs stock's 1). Separate from the player input path (the AbilitySet InputTag
	// binding), so no double-activation; the cooldown GE rate-limits the bot's per-tick events to cadence.
	{
		FAbilityTriggerData FireEventTrigger;
		FireEventTrigger.TriggerTag    = TAG_InputTag_Weapon_Fire_ProjBase;
		FireEventTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		AbilityTriggers.Add(FireEventTrigger);
	}

	// Identity + the same activation gating the laser abilities carry.
	AbilityTags.AddTag(TAG_Ability_Projectile_ProjBase);
	ActivationOwnedTags.AddTag(TAG_State_Firing_Projectile_ProjBase);
	ActivationBlockedTags.AddTag(TAG_State_Carrying_ProjBase);      // holstered under a carry must not fire
	ActivationBlockedTags.AddTag(TAG_State_ThrowRecovery_ProjBase); // post-throw recovery window
	ActivationBlockedTags.AddTag(TAG_State_Match_Warmup_ProjBase);  // match-flow freeze
	ActivationBlockedTags.AddTag(TAG_State_Match_Ended_ProjBase);

	// Cooldown tag lives in AFLCombatTags.ini (Cooldown.Weapon.Rocket); the concrete Cooldown GE is set on
	// the BP child's CDO (CooldownGameplayEffectClass) -- same shape as the laser abilities, NOT the AbilitySet.

	// Default fire montage: the stock rifle trigger-pull additive on SK_Mannequin (the same asset
	// GA_Weapon_Fire plays). BP children may override per weapon. Missing/renamed -> null -> no montage
	// (the weapon still fires; only the hero's trigger-pull is absent).
	static ConstructorHelpers::FObjectFinder<UAnimMontage> FireMontageFinder(
		TEXT("/Game/Weapons/Rifle/Animations/AM_MM_Rifle_Fire.AM_MM_Rifle_Fire"));
	if (FireMontageFinder.Succeeded())
	{
		CharacterFireMontage = FireMontageFinder.Object;
	}
}

void UAFLAG_Projectile_Base::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	// Cooldown/cost gate. CommitAbility cancels the prediction key on failure; just bail (mirrors Pulse).
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());

	// 2-client watch instrumentation (same shape as AFL_PULSE: role distinguishes host vs client windows).
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ROCKET: Activate by %s (role=%d localctrl=%d)"),
		*GetNameSafe(AvatarPawn),
		AvatarPawn ? static_cast<int32>(AvatarPawn->GetLocalRole()) : 0,
		ActorInfo->IsLocallyControlled() ? 1 : 0);

	// CHARACTER FIRE MONTAGE -- trigger-pull + additive kick. Fire-and-forget via ASC->PlayMontage (the GAS
	// primitive), NOT PlayMontageAndWait: this ability EndAbility's immediately below (single-shot), and the
	// AndWait task's bStopWhenAbilityEnds would blend the kick out. Runs on the predicted client AND the
	// authority -> GAS dedups the owner's predicted play against the authority multicast (Pulse's proven
	// shape). COSMETIC only.
	if (CharacterFireMontage && ASC)
	{
		ASC->PlayMontage(this, CurrentActivationInfo, CharacterFireMontage, 1.0f);
	}

	// SERVER-AUTHORITATIVE PROJECTILE SPAWN. Only on authority -> exactly one replicated projectile; clients
	// receive it via replication (the owner's predicted feel is the montage above). The server aims off the
	// pawn's REPLICATED control rotation (GetBaseAimRotation) -- it never reads the client's raw viewpoint
	// (the same "server never trusts client view" doctrine as Pulse's lag-comp path; control rotation is
	// server-known + replicated). Spawned with Instigator = the firing pawn so the projectile's own BeginPlay
	// self-inits its damage-source ASC + team off GetInstigator (decoupled from this ability).
	if (ActorInfo->IsNetAuthority() && ProjectileClass && AvatarPawn)
	{
		if (UWorld* World = AvatarPawn->GetWorld())
		{
			const FVector  MuzzleLoc = ResolveMuzzleLocation(AvatarPawn);  // inherited; never world origin
			const FRotator AimRot    = AvatarPawn->GetBaseAimRotation();
			const FVector  AimDir    = AimRot.Vector();

			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner      = AvatarPawn;
			SpawnParams.Instigator = AvatarPawn;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AActor* Projectile = World->SpawnActor<AActor>(ProjectileClass, MuzzleLoc, AimRot, SpawnParams);
			if (Projectile)
			{
				// If LaunchSpeed is set, overwrite the projectile's velocity along the aim; else its own
				// ProjectileMovement InitialSpeed + this spawn rotation launch it (the harvest-default path).
				if (LaunchSpeed > 0.0f)
				{
					if (UProjectileMovementComponent* PMC =
						Projectile->FindComponentByClass<UProjectileMovementComponent>())
					{
						PMC->Velocity = AimDir * LaunchSpeed;
					}
				}
				UE_LOG(LogAFLCombat, Log,
					TEXT("AFL_ROCKET: spawned %s at muzzle=(%.0f,%.0f,%.0f) aimDir=(%.2f,%.2f,%.2f) speed=%.0f"),
					*GetNameSafe(Projectile), MuzzleLoc.X, MuzzleLoc.Y, MuzzleLoc.Z,
					AimDir.X, AimDir.Y, AimDir.Z, LaunchSpeed);
			}
			else
			{
				UE_LOG(LogAFLCombat, Warning, TEXT("AFL_ROCKET: SpawnActor(%s) returned null"),
					*GetNameSafe(ProjectileClass.Get()));
			}
		}
	}

	// Single-shot: end now. The montage is fire-and-forget (ClearAnimatingAbility nulls the ability<->montage
	// link WITHOUT stopping it, so the additive kick plays out cleanly). Mirrors Pulse's single-shot EndAbility.
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}
