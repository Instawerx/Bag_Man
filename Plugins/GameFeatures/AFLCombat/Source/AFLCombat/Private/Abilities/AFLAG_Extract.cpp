// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLAG_Extract.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Attributes/AFLAttributeSet_Energy.h"
#include "Cosmetics/AFLWalletComponent.h"
#include "Effects/AFLGE_ExtractChannel.h"
#include "Effects/GE_AFL_EnergyGain_Small.h"
#include "Energy/AFLEnergyDropComponent.h"
#include "GameFramework/PlayerState.h"
#include "Messages/AFLHitConfirmMessage.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAG_Extract)

// Native tags (module-init-safe; the ini rows remain the spec source of truth).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_InExtractionZone_Extract, "State.InExtractionZone");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Extracting_Extract, "State.Extracting");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_Start_Extract, "Event.Extraction.Start");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_Complete_Extract, "Event.Extraction.Complete");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_Failed_Extract, "Event.Extraction.Failed");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Damage_Confirmed_Extract, "Event.Damage.Confirmed");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_Energy_Gain_Extract, "Data.Energy.Gain");
// P2 close-out stress-object chaos (S10): extracting WHILE carrying the stress object pays a Watts
// bonus. The tag is granted by AFLGE_StressObjectCarrier (the multi-effect carrier buff).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Carrying_StressObject_Extract, "State.Carrying.StressObject");

static TAutoConsoleVariable<float> CVarAFLChaosExtractMult(
	TEXT("afl.Chaos.ExtractMult"),
	1.5f,
	TEXT("Watts multiplier when extracting while carrying the stress object (S10 1.5x extraction reward)."));

static TAutoConsoleVariable<float> CVarAFLExtractChannelOverride(
	TEXT("afl.Extract.ChannelDuration"),
	-1.0f,
	TEXT("Override the extraction channel seconds (-1 = use the GA's ChannelDuration default of 6s). Test-compression knob."));


UAFLAG_Extract::UAFLAG_Extract()
{
	// Instance state (handles + listeners) -> per-actor. LocalPredicted: activation feels instant
	// (the local State.Extracting via ActivationOwnedTags starts the UI bar); every COMMIT below is
	// authority-gated, so prediction can never mint Watts or zero energy.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

	ActivationRequiredTags.AddTag(TAG_State_InExtractionZone_Extract);
	ActivationOwnedTags.AddTag(TAG_State_Extracting_Extract);
	// No double-channel: the owned tag also blocks re-activation while live.
	ActivationBlockedTags.AddTag(TAG_State_Extracting_Extract);

	ChannelEffectClass = UAFLGE_ExtractChannel::StaticClass();
}

void UAFLAG_Extract::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	bChannelCompleted = false;
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = GetWorld();

	// Zone-exit watch (both sides -- the local cancel mirrors the server's; GAS reconciles).
	if (ASC)
	{
		ZoneTagHandle = ASC->RegisterGameplayTagEvent(TAG_State_InExtractionZone_Extract, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UAFLAG_Extract::HandleZoneTagChanged);
	}

	if (HasAuthority(&ActivationInfo) && ASC && World)
	{
		// Channel-window GE by handle (State.Extracting replicated + Gameplay.MovementStopped O1 lock).
		const FGameplayEffectSpecHandle SpecHandle =
			MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, ChannelEffectClass, 1.0f);
		if (SpecHandle.IsValid())
		{
			ChannelEffectHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
		}

		// Damage interrupt: the ExecCalc broadcasts Event.Damage.Confirmed on the authority world
		// (EffectiveDamage > 0 only) -- the exact message the drop-on-damage carry release consumes.
		DamageListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FAFLHitConfirmMessage>(
			TAG_Event_Damage_Confirmed_Extract,
			[this](FGameplayTag Channel, const FAFLHitConfirmMessage& Msg) { HandleDamageConfirmed(Channel, Msg); });

		BroadcastExtractionEvent(TAG_Event_Extraction_Start_Extract,
			ASC->GetNumericAttribute(UAFLAttributeSet_Energy::GetCarriedEnergyAttribute()));
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_EXTRACT: %s channel START (%.1fs, energy %.1f)."),
			*GetNameSafe(GetAvatarActorFromActorInfo()), ChannelDuration,
			ASC->GetNumericAttribute(UAFLAttributeSet_Energy::GetCarriedEnergyAttribute()));
	}

	// The channel clock (default 6s; afl.Extract.ChannelDuration > 0 compresses it for tests). Task
	// auto-ends with the ability on every exit path.
	const float Override = CVarAFLExtractChannelOverride.GetValueOnGameThread();
	const float Duration = (Override > 0.0f) ? Override : ChannelDuration;
	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, Duration);
	WaitTask->OnFinish.AddDynamic(this, &UAFLAG_Extract::HandleChannelComplete);
	WaitTask->ReadyForActivation();
}

void UAFLAG_Extract::HandleChannelComplete()
{
	// (1) SUCCESS COMMIT -- authority only (the client instance just ends; Watts/energy replicate).
	if (HasAuthority(&CurrentActivationInfo))
	{
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		APlayerState* PS = Cast<APlayerState>(GetOwningActorFromActorInfo());
		UAFLWalletComponent* Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
		if (ASC && Wallet)
		{
			const float Energy = ASC->GetNumericAttribute(UAFLAttributeSet_Energy::GetCarriedEnergyAttribute());
			// Stress-object chaos (S10): carrying THE stress object at extract pays afl.Chaos.ExtractMult
			// x Watts (default 1.5x). The tag rides AFLGE_StressObjectCarrier; no carry code here.
			const bool bStressCarrier = ASC->HasMatchingGameplayTag(TAG_State_Carrying_StressObject_Extract);
			const float ExtractMult = bStressCarrier ? FMath::Max(1.0f, CVarAFLChaosExtractMult.GetValueOnGameThread()) : 1.0f;
			const int32 Reward = FMath::RoundToInt(Energy * WattsPerEnergy * ExtractMult);

			// Cash out THROUGH the wallet funnel (CommitMutation diag line carries the reason)...
			Wallet->EarnWattsAuthority(Reward, TEXT("extraction"));

			// ...and zero CarriedEnergy THROUGH the GE rail (the death-burst negative-SetByCaller
			// shape verbatim) -- never a direct attribute write.
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddInstigator(GetOwningActorFromActorInfo(), GetAvatarActorFromActorInfo());
			FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(UGE_AFL_EnergyGain_Small::StaticClass(), 1.0f, Context);
			if (SpecHandle.IsValid())
			{
				SpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_Energy_Gain_Extract, -Energy);
				ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			}

			bChannelCompleted = true;
			BroadcastExtractionEvent(TAG_Event_Extraction_Complete_Extract, Energy);
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_EXTRACT: %s channel COMPLETE -- %.1f energy -> %d Watts (rate %.0f, mult %.1fx%s)."),
				*GetNameSafe(GetAvatarActorFromActorInfo()), Energy, Reward, WattsPerEnergy, ExtractMult,
				bStressCarrier ? TEXT(" STRESS") : TEXT(""));
		}
		else
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_EXTRACT: commit aborted -- ASC %s / wallet %s."),
				ASC ? TEXT("ok") : TEXT("MISSING"), Wallet ? TEXT("ok") : TEXT("MISSING"));
		}
	}
	K2_EndAbility();
}

void UAFLAG_Extract::HandleDamageConfirmed(FGameplayTag /*Channel*/, const FAFLHitConfirmMessage& Msg)
{
	// (2) DAMAGE INTERRUPT (authority -- the listener only exists there). Any confirmed hit ON the
	// channeler (incl. the self-damage cheat) breaks the channel.
	if (Msg.Target != GetAvatarActorFromActorInfo())
	{
		return;
	}
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_EXTRACT: %s channel INTERRUPTED by damage (%.1f from %s)."),
		*GetNameSafe(GetAvatarActorFromActorInfo()), Msg.Damage, *GetNameSafe(Cast<AActor>(Msg.Instigator)));

	// AFL-0808: the interrupt drops ALL carried energy as a burst through the death rail.
	// (Re-collect wrinkle + feel debt documented on BurstNow.)
	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		if (UAFLEnergyDropComponent* Drop = Avatar->FindComponentByClass<UAFLEnergyDropComponent>())
		{
			Drop->BurstNow(100.0f, TEXT("extraction-interrupt"));
		}
		else
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_EXTRACT: no UAFLEnergyDropComponent on %s -- AFL-0808 burst skipped."),
				*GetNameSafe(Avatar));
		}
	}
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateCancelAbility=*/true);
}

void UAFLAG_Extract::HandleZoneTagChanged(const FGameplayTag /*Tag*/, int32 NewCount)
{
	// (3) ZONE-EXIT INTERRUPT: the dispenser GE vanished. Energy retained -- leaving is not
	// getting shot (the AFL-0808 burst belongs to damage only).
	if (NewCount <= 0)
	{
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_EXTRACT: %s channel CANCELLED -- left the zone."),
			*GetNameSafe(GetAvatarActorFromActorInfo()));
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateCancelAbility=*/true);
	}
}

void UAFLAG_Extract::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// THE funnel -- paths (1)-(5) in the header all land here exactly once.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (ZoneTagHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(TAG_State_InExtractionZone_Extract, EGameplayTagEventType::NewOrRemoved)
				.Remove(ZoneTagHandle);
			ZoneTagHandle.Reset();
		}
		if (HasAuthority(&ActivationInfo) && ChannelEffectHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(ChannelEffectHandle);
			ChannelEffectHandle.Invalidate();
		}
	}
	if (DamageListener.IsValid())
	{
		DamageListener.Unregister();
	}
	// Every non-complete end is a FAILED extraction (damage / zone exit / death / teardown) --
	// the single broadcast home, so interrupt handlers can never double-fire it.
	if (HasAuthority(&ActivationInfo) && !bChannelCompleted)
	{
		BroadcastExtractionEvent(TAG_Event_Extraction_Failed_Extract, 0.0f);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAFLAG_Extract::BroadcastExtractionEvent(const FGameplayTag& EventTag, float Magnitude) const
{
	// The ThresholdReached broadcast shape (FLyraVerbMessage; Instigator = the channeling pawn).
	if (UWorld* World = GetWorld())
	{
		FLyraVerbMessage Message;
		Message.Verb = EventTag;
		Message.Instigator = GetAvatarActorFromActorInfo();
		Message.Magnitude = Magnitude;
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(Message.Verb, Message);
	}
}
