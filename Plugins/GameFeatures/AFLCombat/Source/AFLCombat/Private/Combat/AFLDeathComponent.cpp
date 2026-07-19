// Copyright C12 AI Gaming. All Rights Reserved.

#include "Combat/AFLDeathComponent.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"   // CONVERGENCE: bind THIS set's OnOutOfHealth; read Lyra Health/MaxHealth
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Attributes/AFLAttributeSet_Energy.h"
#include "Character/LyraHealthComponent.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Effects/AFLGE_OverloadRestore.h"
#include "Effects/AFLGE_OverloadStun.h"
#include "Energy/AFLEnergyDropComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Messages/LyraVerbMessage.h"
#include "Messages/LyraVerbMessageHelpers.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDeathComponent)

// P2 close-out OVERLOAD (S7 AFL-0706): a player who would die WHILE CARRYING ENERGY instead
// OVERLOADS -- bursts their energy, restores Health to a floor, and is briefly stunned/vulnerable,
// surviving. The State.Overloaded tag (on the stun GE) is the re-overload lockout. Death still fires
// normally when there is no energy to lose.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Overloaded_Death, "State.Overloaded");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_Health_Restore_Death, "Data.Health.Restore");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Combat_Overload_Death, "Event.Combat.Overload");

// AFL's custom death (CombatSet->StartDeath) bypasses ULyraHealthComponent::HandleOutOfHealth, where Lyra
// normally fires this. Firing it from the AFL death path drives EVERY Lyra elimination-consumer: the
// ShooterCore scoring component (K/D StatTags -> the match scoreboard), the assist/elim-streak processors,
// and the kill feed -- all silent in AFL without it.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Lyra_Elimination_Message, "Lyra.Elimination.Message");

static TAutoConsoleVariable<float> CVarAFLOverloadMinEnergy(
	TEXT("afl.Overload.MinEnergy"),
	1.0f,
	TEXT("Minimum CarriedEnergy to OVERLOAD instead of dying (below this = real death). S7 AFL-0706."));

static TAutoConsoleVariable<float> CVarAFLOverloadDropPercent(
	TEXT("afl.Overload.DropPercent"),
	70.0f,
	TEXT("Percent of CarriedEnergy scattered as pickups on overload (the survive-burst). S7 AFL-0706."));

static TAutoConsoleVariable<float> CVarAFLOverloadRestoreFraction(
	TEXT("afl.Overload.RestoreFraction"),
	0.5f,
	TEXT("Health restored to this fraction of MaxHealth on overload (0..1). S7 AFL-0706."));

UAFLDeathComponent::UAFLDeathComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);   // the death drive is authority-side; Lyra's DeathState replicates
}

UAFLDeathComponent* UAFLDeathComponent::FindDeathComponent(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UAFLDeathComponent>() : nullptr;
}

void UAFLDeathComponent::BeginPlay()
{
	Super::BeginPlay();

	// Resolve the ASC. DIRECT resolve is the PRIMARY path; the PawnExtension hook is the FALLBACK.
	// WHY (verified against LyraPawnExtensionComponent.cpp:292-303 + LyraHeroComponent.cpp:164):
	// OnAbilitySystemInitialized_RegisterAndCall only immediate-executes `if (PawnExt->AbilitySystemComponent)`,
	// and that member is set ONLY by PawnExt->InitializeAbilitySystem, whose ONLY caller is
	// LyraHeroComponent (the possessed-hero / PlayerState flow). A self-ASC'd, unpossessed
	// ALyraCharacterWithAbilities (our AAFLTargetDummy) has NO LyraHeroComponent and self-inits its
	// ASC in PostInitializeComponents -- so the PawnExt member stays null forever, the hook NEVER
	// fires, and the original PawnExt-first ordering left death unbound (listeners=0, no bind log).
	// For a self-ASC'd pawn the ASC IS ready here at BeginPlay -> resolve it directly. For a
	// possessed PLAYER (ASC on PlayerState, lands after pawn BeginPlay) the direct resolve returns
	// null, so we fall through to the PawnExt hook, which DOES fire via LyraHeroComponent. This one
	// component thus serves both: the dummy/enemies (direct) AND the player (deferred via PawnExt).
	if (AActor* Owner = GetOwner())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
		{
			InitializeWithAbilitySystem(ASC);
		}
		else if (ULyraPawnExtensionComponent* PawnExt = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Owner))
		{
			PawnExt->OnAbilitySystemInitialized_RegisterAndCall(
				FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAbilitySystemReady));
		}
	}
}

void UAFLDeathComponent::OnAbilitySystemReady()
{
	// Bring-up trace (Log level so it shows without verbose flags): confirms the PawnExt
	// OnAbilitySystemInitialized hook actually fired for this owner.
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_DEATH: %s OnAbilitySystemReady fired -> resolving ASC + set."),
		*GetNameSafe(GetOwner()));
	if (AActor* Owner = GetOwner())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
		{
			InitializeWithAbilitySystem(ASC);
		}
	}
}

void UAFLDeathComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void UAFLDeathComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (AbilitySystemComponent == InASC && CombatSet != nullptr)
	{
		return;   // already wired to this ASC
	}
	UninitializeFromAbilitySystem();

	AbilitySystemComponent = InASC;
	if (!AbilitySystemComponent)
	{
		return;
	}

	CombatSet = AbilitySystemComponent->GetSet<UAFLAttributeSet_Combat>();
	if (!CombatSet)
	{
		// The AFL combat set is granted via the experience's AddAbilities action
		// (DA_AFL_Combat_AbilitySet) -- DEFERRED, it can land AFTER ASC init. So instead of
		// failing, wait for it via the GE-applied delegate; GE_AFL_Combat_InitData (applied by the
		// AbilitySet to seed the just-added set) fires it, at which point we re-resolve and bind.
		// This is the fix for the "ASC has no UAFLAttributeSet_Combat" race (death never triggered
		// because the set arrived after BeginPlay).
		// Use OnGameplayEffectAppliedDelegateToSelf, NOT OnActiveGameplayEffectAddedDelegateToSelf.
		// The latter fires ONLY for DURATION/Infinite GEs; GE_AFL_Combat_InitData (which the AbilitySet
		// applies to seed the just-added set) is INSTANT, so the Active-added delegate never fired and
		// the retry never bound (the previous listeners=0 bug). OnGameplayEffectAppliedDelegateToSelf
		// fires from OnGameplayEffectAppliedToSelf for BOTH Instant and duration paths (verified:
		// AbilitySystemComponent.cpp:962 ApplyGameplayEffectSpecToSelf -> OnGameplayEffectAppliedToSelf),
		// so the InitData application catches us here and we re-resolve the now-present set.
		if (!GESpawnedHandle.IsValid())
		{
			GESpawnedHandle = AbilitySystemComponent->OnGameplayEffectAppliedDelegateToSelf.AddUObject(
				this, &ThisClass::OnGameplayEffectAddedToSelf);
		}
		UE_LOG(LogAFLCombat, Log,
			TEXT("AFL_DEATH: %s AFL combat set not yet granted at init; registered GE-applied retry (waiting for deferred grant)."),
			*GetNameSafe(GetOwner()));
		return;
	}

	// CONVERGENCE TRIGGER: bind ULyraHealthSet::OnOutOfHealth (Health is the SSOT on the Lyra set now). The AFL set's
	// presence (resolved above) is the arming gate -- when it's granted the ASC is fully seeded and ULyraHealthSet
	// (stock, on the PlayerState) is guaranteed present. We drive StartDeath; ULyraHealthComponent (also bound to
	// this signal) fires the elimination message natively, so we deliberately do NOT (no double kill-feed).
	if (const ULyraHealthSet* HealthSet = AbilitySystemComponent->GetSet<ULyraHealthSet>())
	{
		LyraDeathSet = HealthSet;
		OutOfHealthHandle = HealthSet->OnOutOfHealth.AddUObject(this, &ThisClass::HandleLyraOutOfHealth);
	}
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_DEATH: %s bound to ULyraHealthSet OnOutOfHealth (death armed)."), *GetNameSafe(GetOwner()));

	// OVERLOAD: subscribe to Event.Combat.Overload (UAFLDamageExecCalc broadcasts it on a survive-clamped killing
	// blow); the handler defers the burst/restore/stun/announce off the ExecCalc's broadcast (re-entrant GAS).
	if (UWorld* World = GetWorld())
	{
		OverloadListenerHandle = UGameplayMessageSubsystem::Get(World).RegisterListener(
			FGameplayTag::RequestGameplayTag(FName("Event.Combat.Overload")), this, &ThisClass::HandleOverloadMessage);
	}

	// SEQUENCE: cache the owner's Lyra health component -- we drive its replicated death sequence.
	if (AActor* Owner = GetOwner())
	{
		LyraHealthComponent = ULyraHealthComponent::FindHealthComponent(Owner);
	}
}

void UAFLDeathComponent::OnGameplayEffectAddedToSelf(
	UAbilitySystemComponent* /*Source*/, const FGameplayEffectSpec& /*Spec*/, FActiveGameplayEffectHandle /*Handle*/)
{
	// A GE just applied. If it was the InitData grant, the AFL set now exists -- re-resolve and
	// bind. Once bound, stop listening for further GE-adds (one-shot arm).
	if (!CombatSet && AbilitySystemComponent)
	{
		if (const UAFLAttributeSet_Combat* Set = AbilitySystemComponent->GetSet<UAFLAttributeSet_Combat>())
		{
			CombatSet = Set;
			// CONVERGENCE: bind Lyra's OnOutOfHealth (present with the ASC) + subscribe to Event.Combat.Overload.
			if (const ULyraHealthSet* HealthSet = AbilitySystemComponent->GetSet<ULyraHealthSet>())
			{
				LyraDeathSet = HealthSet;
				OutOfHealthHandle = HealthSet->OnOutOfHealth.AddUObject(this, &ThisClass::HandleLyraOutOfHealth);
			}
			if (UWorld* World = GetWorld())
			{
				OverloadListenerHandle = UGameplayMessageSubsystem::Get(World).RegisterListener(
					FGameplayTag::RequestGameplayTag(FName("Event.Combat.Overload")), this, &ThisClass::HandleOverloadMessage);
			}
			if (AActor* Owner = GetOwner())
			{
				LyraHealthComponent = ULyraHealthComponent::FindHealthComponent(Owner);
			}
			UE_LOG(LogAFLCombat, Log,
				TEXT("AFL_DEATH: %s bound to ULyraHealthSet OnOutOfHealth via deferred grant (death armed)."),
				*GetNameSafe(GetOwner()));

			if (GESpawnedHandle.IsValid())
			{
				AbilitySystemComponent->OnGameplayEffectAppliedDelegateToSelf.Remove(GESpawnedHandle);
				GESpawnedHandle.Reset();
			}
		}
	}
}

void UAFLDeathComponent::UninitializeFromAbilitySystem()
{
	if (LyraDeathSet && OutOfHealthHandle.IsValid())
	{
		LyraDeathSet->OnOutOfHealth.Remove(OutOfHealthHandle);
	}
	if (AbilitySystemComponent && GESpawnedHandle.IsValid())
	{
		AbilitySystemComponent->OnGameplayEffectAppliedDelegateToSelf.Remove(GESpawnedHandle);
	}
	if (OverloadListenerHandle.IsValid())
	{
		OverloadListenerHandle.Unregister();
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(OverloadApplyTimer);
	}
	OutOfHealthHandle.Reset();
	GESpawnedHandle.Reset();
	CombatSet = nullptr;
	LyraDeathSet = nullptr;
	AbilitySystemComponent = nullptr;
	LyraHealthComponent = nullptr;
}

void UAFLDeathComponent::HandleLyraOutOfHealth(AActor* Instigator, AActor* Causer, const FGameplayEffectSpec* /*Spec*/, float /*Magnitude*/, float /*OldValue*/, float /*NewValue*/)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Authority-only: the death sequence drives the REPLICATED ELyraDeathState, which OnReps to
	// every client. Running it on a client would desync. (Listen-server host is authority -> runs.)
	if (Owner->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	if (bDeathStarted)
	{
		return;   // idempotent -- a second sub-zero tick must not restart death
	}

	// CONVERGENCE: the OVERLOAD intercept MOVED to UAFLDamageExecCalc (the Option-A pre-death clamp). The ExecCalc
	// survive-clamps a would-be-killing-blow BEFORE the 0-crossing (Health -> 1) and broadcasts Event.Combat.Overload,
	// which HandleOverloadMessage below turns into the deferred burst/restore/stun. So if THIS handler runs at all,
	// the 0-crossing was a REAL death (no energy / locked out) -> proceed.

	bDeathStarted = true;

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_DEATH: %s out of Health -> StartDeath (killer=%s)"),
		*GetNameSafe(Owner), *GetNameSafe(Causer ? Causer : Instigator));

	// CONVERGENCE: the elimination message is NOT fired here anymore -- ULyraHealthComponent::HandleOutOfHealth
	// (also bound to ULyraHealthSet::OnOutOfHealth) fires Lyra.Elimination.Message NATIVELY now, feeding the
	// ShooterCore scoring/assist/kill-feed processors. Firing it here too would DOUBLE the kill-feed + K/D.

	// REUSE Lyra's shipped, networked death sequence. StartDeath/FinishDeath are HealthSet-
	// independent (drive DeathState + Status.Death.* tags + OnDeathStarted/Finished + ForceNetUpdate);
	// ALyraCharacter ragdolls off OnDeathStarted/DeathState. We enter it from the AFL trigger.
	if (LyraHealthComponent)
	{
		LyraHealthComponent->StartDeath();   // dying: ragdoll begins, replicates to all clients

		if (DeathFinishDelay > 0.0f)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					FinishDeathTimer, this, &ThisClass::HandleFinishDeath, DeathFinishDelay, /*bLoop=*/false);
			}
		}
		else
		{
			HandleFinishDeath();
		}
	}
	else
	{
		// No Lyra health component (a non-ALyraCharacter combatant). Rather than hand-roll a
		// host-only ragdoll, log and destroy on the authority (replicated removal). A future
		// non-character combatant that needs visible death should carry a ULyraHealthComponent
		// or a dedicated replicated death-state; flagged rather than silently faked.
		UE_LOG(LogAFLCombat, Warning,
			TEXT("AFL_DEATH: %s has no ULyraHealthComponent -- no Lyra death sequence to drive; destroying on authority."),
			*GetNameSafe(Owner));
		if (bDestroyOnDeathFinish)
		{
			Owner->Destroy();
		}
	}
}

void UAFLDeathComponent::HandleFinishDeath()
{
	AActor* Owner = GetOwner();
	if (!Owner || Owner->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	if (LyraHealthComponent)
	{
		LyraHealthComponent->FinishDeath();   // dead: cleanup, replicates
	}

	if (bDestroyOnDeathFinish)
	{
		Owner->Destroy();   // authority destroy replicates the removal to all clients
	}
}

void UAFLDeathComponent::HandleOverloadMessage(FGameplayTag /*Channel*/, const FLyraVerbMessage& Message)
{
	AActor* Owner = GetOwner();
	// The ExecCalc broadcasts Event.Combat.Overload GLOBALLY; act only when WE are the survive-clamped target, on
	// authority (the burst/restore/stun are server-driven; the results replicate down).
	if (!Owner || Message.Target != Owner || Owner->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	// DEFER off the ExecCalc's in-execution broadcast -- applying GEs / BurstNow synchronously inside a running
	// ExecCalc is re-entrant GAS (same reason the sever/overkill broadcasts are consumed asynchronously). Next tick.
	PendingOverloadEnergy = Message.Magnitude;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(OverloadApplyTimer, this, &ThisClass::ApplyOverloadConsequences, 0.001f, /*bLoop=*/false);
	}
}

void UAFLDeathComponent::ApplyOverloadConsequences()
{
	AActor* Owner = GetOwner();
	if (!Owner || !AbilitySystemComponent || Owner->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	// (1) Scatter the energy through the PROVEN burst rail (same component as the death burst).
	if (UAFLEnergyDropComponent* Drop = Owner->FindComponentByClass<UAFLEnergyDropComponent>())
	{
		Drop->BurstNow(CVarAFLOverloadDropPercent.GetValueOnGameThread(), TEXT("overload"));
	}

	// (2) Restore Health from 1 (the ExecCalc's survive-clamp) UP TO the floor, through the GE rail -- now
	//     UAFLGE_OverloadRestore modifies ULyraHealthSet's Healing meta (+Health). RestoreAmt = max(0, floor*Max - cur).
	{
		const float MaxHealth = AbilitySystemComponent->GetNumericAttribute(ULyraHealthSet::GetMaxHealthAttribute());
		const float CurHealth = AbilitySystemComponent->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute());
		const float Floor = FMath::FloorToFloat(FMath::Clamp(CVarAFLOverloadRestoreFraction.GetValueOnGameThread(), 0.0f, 1.0f) * MaxHealth);
		const float RestoreAmt = FMath::Max(0.0f, Floor - CurHealth);
		FGameplayEffectContextHandle Ctx = AbilitySystemComponent->MakeEffectContext();
		Ctx.AddInstigator(Owner, Owner);
		FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(UAFLGE_OverloadRestore::StaticClass(), 1.0f, Ctx);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(TAG_Data_Health_Restore_Death, RestoreAmt);
			AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	// (3) The brief stun/vulnerability window -- also the re-overload lockout (State.Overloaded), which the
	//     ExecCalc's next-hit check reads to refuse a repeat survive.
	{
		FGameplayEffectContextHandle Ctx = AbilitySystemComponent->MakeEffectContext();
		Ctx.AddInstigator(Owner, Owner);
		FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(UAFLGE_OverloadStun::StaticClass(), 1.0f, Ctx);
		if (Spec.IsValid())
		{
			AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	// (4) The announce IS the ExecCalc's original Event.Combat.Overload broadcast (a future HUD flash subscribes to
	//     the same channel). We do NOT re-broadcast it -- that would re-enter this handler.
	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_OVERLOAD: %s consequences applied (carried %.1f, dropped %.0f%%, restored to floor, stunned/locked)."),
		*GetNameSafe(Owner), PendingOverloadEnergy, CVarAFLOverloadDropPercent.GetValueOnGameThread());
}
