// Copyright C12 AI Gaming. All Rights Reserved.

#include "Energy/AFLOverdriveComponent.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/AFLAttributeSet_Energy.h"
#include "Character/LyraHealthComponent.h"
#include "Effects/AFLGE_OverdriveBuff.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLOverdriveComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Energy_ThresholdReached_ODComp, "Event.Energy.ThresholdReached");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Energy_Overdrive_ODComp, "State.Energy.Overdrive");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_Energy_Drain_ODComp, "Data.Energy.Drain");

// Drain rate (energy/second) while overdriven. Cvar so the feel pass -- and the harness, which
// runs it fast to keep the drain timeline assertable in seconds -- can tune without a rebuild.
static TAutoConsoleVariable<float> CVarAFLEnergyDrainPerSecond(
	TEXT("afl.Energy.DrainPerSecond"),
	5.0f,
	TEXT("CarriedEnergy drained per second while State.Energy.Overdrive holds (UAFLGE_OverdriveBuff periodic SetByCaller)."));


UAFLOverdriveComponent::UAFLOverdriveComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // pure listener + delegate work.
}

void UAFLOverdriveComponent::BeginPlay()
{
	Super::BeginPlay();
	TryArm();
}

void UAFLOverdriveComponent::TryArm()
{
	AActor* Owner = GetOwner();
	APawn* Pawn = Cast<APawn>(Owner);
	APlayerState* PS = Pawn ? Pawn->GetPlayerState() : nullptr;
	UAbilitySystemComponent* ResolvedASC = PS ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS) : nullptr;

	if (!ResolvedASC)
	{
		// The PlayerState/ASC can land after pawn BeginPlay (possession ordering). Bounded retry --
		// the cheap deferred-arm shape; the death component solves the same race via GE-applied
		// retries, but a short poll reads simpler for a two-delegate arm.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(ArmRetryTimer,
				FTimerDelegate::CreateWeakLambda(this, [this] { TryArm(); }), 0.25f, false);
		}
		return;
	}
	ASC = ResolvedASC;

	// BOTH SIDES: the speed swap keys off the replicated tag (client prediction half).
	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		CMC = Character->GetCharacterMovement();
	}
	OverdriveTagHandle = ASC->RegisterGameplayTagEvent(TAG_State_Energy_Overdrive_ODComp, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UAFLOverdriveComponent::HandleOverdriveTagChanged);

	// SERVER ONLY: the trigger, the exit, and the death removal.
	if (Owner->HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			ThresholdListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(
				TAG_Event_Energy_ThresholdReached_ODComp, this, &UAFLOverdriveComponent::HandleThresholdReached);
		}
		EnergyChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
			UAFLAttributeSet_Energy::GetCarriedEnergyAttribute()).AddUObject(this, &UAFLOverdriveComponent::HandleEnergyChanged);

		if (ULyraHealthComponent* HealthComponent = ULyraHealthComponent::FindHealthComponent(Owner))
		{
			HealthComponent->OnDeathStarted.AddDynamic(this, &UAFLOverdriveComponent::HandleDeathStarted);
		}
	}

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_OVERDRIVE: %s armed (authority=%d)."),
		*GetNameSafe(Owner), Owner->HasAuthority() ? 1 : 0);
}

void UAFLOverdriveComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ArmRetryTimer);
	}
	if (ASC.IsValid())
	{
		if (OverdriveTagHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(TAG_State_Energy_Overdrive_ODComp, EGameplayTagEventType::NewOrRemoved).Remove(OverdriveTagHandle);
		}
		if (EnergyChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(
				UAFLAttributeSet_Energy::GetCarriedEnergyAttribute()).Remove(EnergyChangedHandle);
		}
	}
	if (ThresholdListener.IsValid())
	{
		ThresholdListener.Unregister();
	}
	Super::EndPlay(EndPlayReason);
}

void UAFLOverdriveComponent::HandleThresholdReached(FGameplayTag /*Channel*/, const FLyraVerbMessage& Msg)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !ASC.IsValid())
	{
		return;
	}
	// The broadcast is global; only OUR crossing applies OUR buff. Instigator = the PlayerState
	// whose set crossed (the attribute-set broadcast contract).
	const APawn* Pawn = Cast<APawn>(Owner);
	if (!Pawn || Msg.Instigator != Pawn->GetPlayerState())
	{
		return;
	}
	// Guard: never stack -- re-trigger requires the tag to have fallen (energy hit 0) and a fresh
	// upward crossing (the set broadcasts upward crossings only = hysteresis for free).
	if (ASC->HasMatchingGameplayTag(TAG_State_Energy_Overdrive_ODComp))
	{
		return;
	}

	const float DrainPerSecond = FMath::Max(0.0f, CVarAFLEnergyDrainPerSecond.GetValueOnGameThread());
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(Owner, Owner);
	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(UAFLGE_OverdriveBuff::StaticClass(), 1.0f, Context);
	if (SpecHandle.IsValid())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_Energy_Drain_ODComp, -DrainPerSecond);
		BuffHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_OVERDRIVE: %s ENTERED overdrive (energy %.1f, drain %.1f/s)."),
			*GetNameSafe(Owner), Msg.Magnitude, DrainPerSecond);
	}
}

void UAFLOverdriveComponent::HandleEnergyChanged(const FOnAttributeChangeData& Data)
{
	// Exit = full consumption: the drain (or a death burst) walked CarriedEnergy to 0.
	if (Data.NewValue <= 0.0f && BuffHandle.IsValid())
	{
		RemoveBuff(TEXT("energy depleted"));
	}
}

void UAFLOverdriveComponent::HandleDeathStarted(AActor* /*OwningActor*/)
{
	// Lyra does NOT clear Infinite GEs on the PlayerState ASC at death/respawn -- without this the
	// buff (and its drain) would ride into the next life. Explicit removal, the grounded decision.
	if (BuffHandle.IsValid())
	{
		RemoveBuff(TEXT("death"));
	}
}

void UAFLOverdriveComponent::RemoveBuff(const TCHAR* Reason)
{
	if (ASC.IsValid() && BuffHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(BuffHandle);
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_OVERDRIVE: %s EXITED overdrive (reason=%s)."),
			*GetNameSafe(GetOwner()), Reason);
	}
	BuffHandle.Invalidate();
}

void UAFLOverdriveComponent::HandleOverdriveTagChanged(const FGameplayTag /*Tag*/, int32 NewCount)
{
	// The dash component's CMC-property-swap precedent, including its re-entrancy guard: the cache
	// is written ONLY when not already swapped, so a duplicate rise event can never bake the
	// boosted value in as the restore target.
	UCharacterMovementComponent* Movement = CMC.Get();
	if (!Movement)
	{
		return;
	}
	if (NewCount > 0)
	{
		if (!bSpeedSwapped)
		{
			CachedMaxWalkSpeed = Movement->MaxWalkSpeed;
			Movement->MaxWalkSpeed = CachedMaxWalkSpeed * SpeedMultiplier;
			bSpeedSwapped = true;
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_OVERDRIVE: %s speed swap ON (%.0f -> %.0f)."),
				*GetNameSafe(GetOwner()), CachedMaxWalkSpeed, Movement->MaxWalkSpeed);
		}
	}
	else if (bSpeedSwapped)
	{
		Movement->MaxWalkSpeed = CachedMaxWalkSpeed;
		bSpeedSwapped = false;
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_OVERDRIVE: %s speed swap OFF (restored %.0f)."),
			*GetNameSafe(GetOwner()), CachedMaxWalkSpeed);
	}
}
