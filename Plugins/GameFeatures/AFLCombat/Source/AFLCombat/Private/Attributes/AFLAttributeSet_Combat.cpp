// Copyright C12 AI Gaming. All Rights Reserved.

#include "Attributes/AFLAttributeSet_Combat.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"   // HUD-mirror target (the shipped bar + spectate read this set)
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffectExtension.h"
#include "Messages/LyraVerbMessage.h"
#include "Messages/LyraVerbMessageHelpers.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAttributeSet_Combat)

// Heat-system native tag consumed inside PostGameplayEffectExecute. Same
// CDO-vs-ini-scan rationale as the rest of AFLCombat — UE_DEFINE_GAMEPLAY_TAG_STATIC
// at file scope registers the tag at module init, strictly before any CDO of
// a class in this module is constructed (per the 2026-05-20 crash post-mortem).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Overheated_AttrSet, "State.Overheated");

// AFL's ExecCalc writes Health directly and never travels ULyraHealthSet, where Lyra fires the standardized
// damage message. Firing it here feeds the ShooterCore assist processor (which sums per-victim damage history
// to credit assists at elimination) -- the same AFL-bypass we fixed for the elimination message.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Lyra_Damage_Message_AttrSet, "Lyra.Damage.Message");

// HUD/spectate decoupling fix (2026-07-18): the shipped Lyra health bar (W_Healthbar) and the AFL spectate
// surfaces (AFLW_SpectateOverlay, AFLSpectateCameraComponent) read ULyraHealthSet via ULyraHealthComponent, but
// AFL damage/heal/death run on UAFLAttributeSet_Combat and NOTHING drives ULyraHealthSet -- so those surfaces sit
// pinned at full. Mirror AFL Health/MaxHealth onto the Lyra set on every Health/MaxHealth change. BOTH are mirrored
// together so the pair stays consistent regardless of the order LyraHealthComponent's init (sets Lyra Health=Max)
// vs the AFL InitData GE run in -- the spawn state ends up Health==Max = full either way. DEATH GUARD: floor the
// mirrored Health at 1 so we NEVER write 0 -> ULyraHealthSet.OnOutOfHealth (Lyra's death path, which
// AFLDeathComponent deliberately bypasses) never trips; the <=1 sliver at the instant of AFL death is covered by
// the death sequence taking over. Runs server-side (PostGameplayEffectExecute is authority); the base value then
// replicates to clients' bars.
static void MirrorAFLHealthToLyra(UAbilitySystemComponent* ASC, float AFLHealth, float AFLMaxHealth)
{
	if (!ASC)
	{
		return;
	}
	const ULyraHealthSet* LyraHealth = ASC->GetSet<ULyraHealthSet>();
	if (!LyraHealth)
	{
		return;
	}

	const float NewHealth = FMath::Max(1.0f, AFLHealth);   // death guard: never write 0 -> never trip Lyra's OnOutOfHealth
	const float OldHealth = LyraHealth->GetHealth();
	const float OldMax    = LyraHealth->GetMaxHealth();

	ASC->SetNumericAttributeBase(ULyraHealthSet::GetMaxHealthAttribute(), AFLMaxHealth);
	ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), NewHealth);

	// CRITICAL: SetNumericAttributeBase sets the value SILENTLY -- it does NOT run ULyraHealthSet::PostGameplayEffectExecute,
	// the ONLY place ULyraHealthSet::On*Changed fire. ULyraHealthComponent (which drives the HUD bar + the AFL spectate
	// surfaces) binds THOSE set-delegates (LyraHealthComponent.cpp:78-79), NOT the generic value-change delegate -- so
	// without these broadcasts the value updates but the bar never refreshes (the bug seen in the first PIE). Broadcast
	// them ourselves, exactly as Lyra does after its own base-set (LyraHealthComponent.cpp:83-88). FLyraAttributeEvent =
	// (Instigator, Causer, Spec, Magnitude, OldValue, NewValue); nullptr instigator/causer/spec is fine -- the bar only
	// reads Old/NewValue.
	if (AFLMaxHealth != OldMax)
	{
		LyraHealth->OnMaxHealthChanged.Broadcast(nullptr, nullptr, nullptr, AFLMaxHealth - OldMax, OldMax, AFLMaxHealth);
	}
	if (NewHealth != OldHealth)
	{
		LyraHealth->OnHealthChanged.Broadcast(nullptr, nullptr, nullptr, NewHealth - OldHealth, OldHealth, NewHealth);
	}
}


UAFLAttributeSet_Combat::UAFLAttributeSet_Combat()
	: Health(100.0f)
	, MaxHealth(100.0f)
	, Shield(0.0f)
	, MaxShield(0.0f)
	, Armor(0.0f)
	, OverkillThreshold(50.0f)
	, Heat(0.0f)
	, MaxHeat(100.0f)
	, HeatDecayRate(20.0f)
	, RecoilMultiplier(1.0f)   // S4-INC2: baseline; consequence GEs Multiply it up
	, HeadHealth(0.0f)         // S4-INC3: zone-HP; InitData GE seeds real values (PHASE B)
	, LeftArmHealth(0.0f)
	, RightArmHealth(0.0f)
	, LeftLegHealth(0.0f)
	, RightLegHealth(0.0f)
{
}

void UAFLAttributeSet_Combat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, Health,            COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, MaxHealth,         COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, Shield,            COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, MaxShield,         COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, Armor,             COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, OverkillThreshold, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, Heat,              COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, MaxHeat,           COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, HeatDecayRate,     COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, RecoilMultiplier,  COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, HeadHealth,        COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, LeftArmHealth,     COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, RightArmHealth,    COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, LeftLegHealth,     COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, RightLegHealth,    COND_OwnerOnly, REPNOTIFY_Always);
}

void UAFLAttributeSet_Combat::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetShieldAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxShield());
	}
	else if (Attribute == GetHeatAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHeat());
	}
	else if (Attribute == GetMaxHealthAttribute()
		|| Attribute == GetMaxShieldAttribute()
		|| Attribute == GetArmorAttribute()
		|| Attribute == GetOverkillThresholdAttribute()
		|| Attribute == GetMaxHeatAttribute()
		|| Attribute == GetHeatDecayRateAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetRecoilMultiplierAttribute())
	{
		// S4-INC2: floor at baseline. A dismember consequence only ever INCREASES
		// recoil/spread; nothing should drive it below 1.0 (no "steadier-than-normal").
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetHeadHealthAttribute()
		|| Attribute == GetLeftArmHealthAttribute()
		|| Attribute == GetRightArmHealthAttribute()
		|| Attribute == GetLeftLegHealthAttribute()
		|| Attribute == GetRightLegHealthAttribute())
	{
		// S4-INC3: zone-HP never stores negative (mirrors Health). DEPLETION is NOT detected
		// from this post-clamp value -- UAFLDamageExecCalc computes the sever + overflow from
		// the CAPTURED (pre-change) zone-HP vs EffectiveDamage, exactly as it derives health
		// spill from EffectiveDamage - ShieldAbsorbed. So the floor here is purely storage
		// hygiene; the absorber's math is unaffected. 0.0 = severed/inert.
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	// Damage / HeatPerBeamTick: no clamping; consumed in PostGameplayEffectExecute.
}

void UAFLAttributeSet_Combat::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// AFL-native death/damage signal. The ExecCalc applies the Health output modifier; here we
	// fire the AFL Health events (the trigger half of the death system). Instigator/causer are
	// extracted from the effect context the same way ULyraHealthSet does, so UAFLDeathComponent
	// gets the killer. PreAttributeChange already clamped Health to [0, MaxHealth]. We fire
	// OnHealthChanged on every Health change (hit-react/HUD) and OnOutOfHealth exactly once on
	// the >0 -> <=0 crossing (the death trigger). Authority-only realistically, but the events
	// are side-effect-free for listeners to gate as needed.
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		// Capture the post-ExecCalc value BEFORE clamping, so the diagnostic logs the true
		// incoming Health (incl. the negative the ExecCalc produced) and we can see the crossing.
		const float PreClampHealth = GetHealth();

		// Clamp here, NOT just in PreAttributeChange: the ExecCalc applies its Health output
		// modifier (Additive) on a path that bypasses PreAttributeChange clamping, so without
		// this Health goes negative (the log showed -8/-26/...). Clamp to [0, MaxHealth] so the
		// stored value is correct and GetHealth() reads exactly 0 at death.
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));

		// HUD/spectate mirror: push the clamped AFL Health onto ULyraHealthSet so the shipped Lyra bar + spectate
		// surfaces reflect real health. Fires on BOTH damage and heal (the overload/consumable restore included).
		MirrorAFLHealthToLyra(GetOwningAbilitySystemComponent(), GetHealth(), GetMaxHealth());

		const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetEffectContext();
		AActor* Instigator = Context.GetOriginalInstigator();
		AActor* Causer     = Context.GetEffectCauser();
		const float Magnitude = Data.EvaluatedData.Magnitude;  // signed delta actually applied

		OnHealthChanged.Broadcast(Instigator, Causer, Magnitude);

		// Fire the canonical Lyra damage message on actual damage (Magnitude < 0 = Health decreased; healing
		// and the overload restore are positive, excluded). Mirrors ULyraHealthSet -- the source the assist
		// processor sums into per-victim damage history. Instigator resolves the SAME way as the elimination
		// message (GetOriginalInstigator -> PlayerState) so the processor's killer-exclusion lines up; Magnitude
		// is the positive damage dealt. Self-damage / null instigator are filtered by the processor.
		if (Magnitude < 0.0f)
		{
			if (UWorld* DmgWorld = GetWorld())
			{
				FLyraVerbMessage DamageMessage;
				DamageMessage.Verb = TAG_Lyra_Damage_Message_AttrSet;
				DamageMessage.Instigator = ULyraVerbMessageHelpers::GetPlayerStateFromObject(Instigator);
				DamageMessage.Target = ULyraVerbMessageHelpers::GetPlayerStateFromObject(GetOwningActor());
				DamageMessage.Magnitude = -Magnitude;
				UGameplayMessageSubsystem::Get(DmgWorld).BroadcastMessage(DamageMessage.Verb, DamageMessage);
			}
		}

		// Death trigger: Health reached 0. StartDeath() is idempotent (guards DeathState), and
		// UAFLDeathComponent::HandleAFLOutOfHealth has its own bDeathStarted guard, so repeat
		// broadcasts (further shots into a 0-Health corpse before cleanup) are safe no-ops.
		const bool bWillBroadcastDeath = (GetHealth() <= 0.0f);

		// Bring-up diagnostic (Log level, unconditional on a Health change): proves whether the
		// crossing test evaluated true AND whether OnOutOfHealth has any listeners. This is the
		// signal that disambiguates "delegate never broadcast" from "broadcast but listener
		// missed it" -- the fourth outcome the bind/StartDeath logs alone can't distinguish.
		// 2-client watch instrumentation (watch b/d): stamp WHOSE health this is. In a 2-client
		// log the dummy (C_1) and a dying player both drain Health through this same line, so a
		// bare "Health 82 -> 64" can't say which actor is taking damage / dying. The owning actor
		// name (from the set's owning ASC) disambiguates dummy-death from player-death.
		const AActor* HealthOwner = GetOwningActor();
		UE_LOG(LogAFLCombat, Log,
			TEXT("AFL_DEATH: %s Health %.1f -> %.1f (clamped %.1f), OnOutOfHealth %s (listeners=%d)"),
			*GetNameSafe(HealthOwner),
			PreClampHealth, GetHealth(), GetHealth(),
			bWillBroadcastDeath ? TEXT("FIRING") : TEXT("not-yet"),
			OnOutOfHealth.IsBound() ? 1 : 0);

		if (bWillBroadcastDeath)
		{
			OnOutOfHealth.Broadcast(Instigator, Causer, Magnitude);
		}
		return;
	}

	// MaxHealth change (the InitData GE seeds it; a max-health buff would too): re-mirror so the Lyra bar's
	// Health/MaxHealth ratio stays correct. Mirror BOTH (Health + Max) so the pair is always consistent.
	if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		MirrorAFLHealthToLyra(GetOwningAbilitySystemComponent(), GetHealth(), GetMaxHealth());
		return;
	}

	// The ExecCalc writes Shield/Health output modifiers directly. Damage is a
	// transit meta-attribute used only inside the ExecCalc; if any GE somehow
	// routes through Damage as the evaluated attribute, zero it here so it
	// can't accumulate across calls.
	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		SetDamage(0.0f);
	}
	else if (Data.EvaluatedData.Attribute == GetHeatPerBeamTickAttribute())
	{
		// Same transit-meta pattern as Damage. GE_AFL_Heat_BeamTick writes
		// HeatPerBeamTick (Override +4); here we fold it into the persistent
		// Heat attribute, clamp, grant State.Overheated when Heat hits MaxHeat,
		// then zero the meta so it can't accumulate across ticks.
		const float Delta = GetHeatPerBeamTick();
		SetHeatPerBeamTick(0.0f);

		if (Delta != 0.0f)
		{
			const float NewHeat = FMath::Clamp(GetHeat() + Delta, 0.0f, GetMaxHeat());
			SetHeat(NewHeat);

			if (NewHeat >= GetMaxHeat() && GetMaxHeat() > 0.0f)
			{
				UAbilitySystemComponent* TargetASC = &Data.Target;
				if (!TargetASC->HasMatchingGameplayTag(TAG_State_Overheated_AttrSet))
				{
					// Loose-tag grant: the Overheated tag has to persist past
					// this GE instance (BeamTick is Instant) but is owned by
					// the heat state machine in this AttributeSet, not by an
					// active GE. Replicate so client-side ActivationBlockedTags
					// checks on the beam ability also see it.
					TargetASC->AddLooseGameplayTag(TAG_State_Overheated_AttrSet);
					TargetASC->SetReplicatedLooseGameplayTagCount(TAG_State_Overheated_AttrSet, 1);

					// Acceptance log line — orchestrator log-stream scrape looks
					// for `AFL_LOG: heat_overheat` at the overheat boundary.
					UE_LOG(LogAFLCombat, Log, TEXT("AFL_LOG: heat_overheat"));
				}
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHeatAttribute())
	{
		// Decay GE writes negative deltas into Heat. When the cooled value
		// drops below MaxHeat * 0.3 and the target still carries
		// State.Overheated, clear the tag. (The old venting-complete marker GE
		// apply was removed -- it only granted a dead Event tag on an
		// Instant GE: never registered, zero consumers. The vent boundary is
		// the AFL_LOG line below.)
		UAbilitySystemComponent* TargetASC = &Data.Target;
		if (TargetASC->HasMatchingGameplayTag(TAG_State_Overheated_AttrSet)
			&& GetHeat() <= GetMaxHeat() * 0.3f)
		{
			TargetASC->SetReplicatedLooseGameplayTagCount(TAG_State_Overheated_AttrSet, 0);
			TargetASC->RemoveLooseGameplayTag(TAG_State_Overheated_AttrSet);

			// Acceptance log line — orchestrator log-stream scrape looks for
			// `AFL_LOG: heat_vented` at the vent-complete boundary.
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_LOG: heat_vented"));
		}
	}
}

void UAFLAttributeSet_Combat::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, Health, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, MaxHealth, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_Shield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, Shield, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_MaxShield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, MaxShield, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_Armor(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, Armor, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_OverkillThreshold(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, OverkillThreshold, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_Heat(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, Heat, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_MaxHeat(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, MaxHeat, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_HeatDecayRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, HeatDecayRate, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_RecoilMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, RecoilMultiplier, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_HeadHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, HeadHealth, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_LeftArmHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, LeftArmHealth, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_RightArmHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, RightArmHealth, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_LeftLegHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, LeftLegHealth, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_RightLegHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, RightLegHealth, OldValue);
}
