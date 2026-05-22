// Copyright C12 AI Gaming. All Rights Reserved.

#include "AbilitySystem/AFLDamageExecCalc.h"

#include "AFLCombat.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "HUD/AFLHitConfirmMessage.h"
#include "Messages/LyraVerbMessage.h"
#include "Telemetry/AFLCombatTelemetry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDamageExecCalc)


// File-specific suffix on the C++ symbol (the FName *value* stays as the
// canonical tag string). Required because UBT Unity builds merge multiple
// .cpp files into one translation unit, and anonymous namespaces collapse
// into a single TU-level namespace under that merge. Per-file rename is
// the minimal Unity-safe pattern.
namespace
{
	// SetByCaller magnitude tags. Authored in AFLCoreTags.ini (Stage 1).
	const FName NAME_Data_Damage_Headshot_ExecCalc   = TEXT("Data.Damage.Headshot");
	const FName NAME_Data_Damage_Distance_ExecCalc   = TEXT("Data.Damage.Distance");
	const FName NAME_Data_Damage_Weakpoint_ExecCalc  = TEXT("Data.Damage.Weakpoint");

	// Verb tag broadcast when damage exceeds OverkillThreshold. Listeners (e.g.
	// AFLDismember in a later stage) subscribe via UGameplayMessageSubsystem.
	const FName NAME_Event_Damage_Overkill_ExecCalc  = TEXT("Event.Damage.Overkill");

	// AFL-0204: broadcast when EffectiveDamage > 0 so the firing client's
	// UAFLHitConfirmComponent can play crosshair pulse + camera shake.
	const FName NAME_Event_Damage_Confirmed_ExecCalc = TEXT("Event.Damage.Confirmed");
}


// Capture definitions for the damage execution. We construct
// FGameplayEffectAttributeCaptureDefinition directly rather than using the
// DECLARE/DEFINE_ATTRIBUTE_CAPTUREDEF macros, because those macros expand to
// raw member-property references (e.g. UAFLAttributeSet_Combat::Damage) which
// require the property be accessible to the calling code. Our properties are
// private (which is the correct UE convention — only ATTRIBUTE_ACCESSORS' static
// getters expose them). Lyra's own LyraDamageExecution uses this same pattern;
// see LyraDamageExecution.cpp:20.
struct FAFLDamageCaptureDefs
{
	FGameplayEffectAttributeCaptureDefinition DamageDef;
	FGameplayEffectAttributeCaptureDefinition ArmorDef;
	FGameplayEffectAttributeCaptureDefinition ShieldDef;
	FGameplayEffectAttributeCaptureDefinition HealthDef;
	FGameplayEffectAttributeCaptureDefinition OverkillThresholdDef;

	FAFLDamageCaptureDefs()
	{
		// Damage is captured from the Source side, snapshotted at spec
		// creation so SetByCaller multipliers apply against the value the
		// instigating ability authored.
		DamageDef = FGameplayEffectAttributeCaptureDefinition(
			UAFLAttributeSet_Combat::GetDamageAttribute(),
			EGameplayEffectAttributeCaptureSource::Source,
			/*bSnapshot=*/true);

		// Target-side captures, not snapshotted so they reflect the target's
		// current mitigation/shield/health at execution time.
		ArmorDef = FGameplayEffectAttributeCaptureDefinition(
			UAFLAttributeSet_Combat::GetArmorAttribute(),
			EGameplayEffectAttributeCaptureSource::Target, false);
		ShieldDef = FGameplayEffectAttributeCaptureDefinition(
			UAFLAttributeSet_Combat::GetShieldAttribute(),
			EGameplayEffectAttributeCaptureSource::Target, false);
		HealthDef = FGameplayEffectAttributeCaptureDefinition(
			UAFLAttributeSet_Combat::GetHealthAttribute(),
			EGameplayEffectAttributeCaptureSource::Target, false);
		OverkillThresholdDef = FGameplayEffectAttributeCaptureDefinition(
			UAFLAttributeSet_Combat::GetOverkillThresholdAttribute(),
			EGameplayEffectAttributeCaptureSource::Target, false);
	}
};

static const FAFLDamageCaptureDefs& AFLDamageCaptureDefs()
{
	static FAFLDamageCaptureDefs DamageStatics;
	return DamageStatics;
}


UAFLDamageExecCalc::UAFLDamageExecCalc()
{
	RelevantAttributesToCapture.Add(AFLDamageCaptureDefs().DamageDef);
	RelevantAttributesToCapture.Add(AFLDamageCaptureDefs().ArmorDef);
	RelevantAttributesToCapture.Add(AFLDamageCaptureDefs().ShieldDef);
	RelevantAttributesToCapture.Add(AFLDamageCaptureDefs().HealthDef);
	RelevantAttributesToCapture.Add(AFLDamageCaptureDefs().OverkillThresholdDef);
}

void UAFLDamageExecCalc::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
#if WITH_SERVER_CODE
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	FAggregatorEvaluateParameters EvalParams;
	EvalParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvalParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	// 1. Capture attribute values.
	float SourceDamage = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		AFLDamageCaptureDefs().DamageDef, EvalParams, SourceDamage);

	float TargetArmor = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		AFLDamageCaptureDefs().ArmorDef, EvalParams, TargetArmor);

	float TargetShield = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		AFLDamageCaptureDefs().ShieldDef, EvalParams, TargetShield);

	float TargetHealth = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		AFLDamageCaptureDefs().HealthDef, EvalParams, TargetHealth);

	float TargetOverkillThreshold = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		AFLDamageCaptureDefs().OverkillThresholdDef, EvalParams, TargetOverkillThreshold);

	// 2. SetByCaller multipliers (default 1.0 when the ability didn't provide them).
	const float HeadshotMult    = Spec.GetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Headshot_ExecCalc,  false), false, 1.0f);
	const float DistanceFalloff = Spec.GetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Distance_ExecCalc,  false), false, 1.0f);
	const float WeakpointMult   = Spec.GetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Weakpoint_ExecCalc, false), false, 1.0f);

	// 3. Raw damage before mitigation.
	const float RawDamage = SourceDamage * HeadshotMult * WeakpointMult * DistanceFalloff;

	// 4. Reciprocal mitigation curve. Armor=0 -> 0% mitigation, Armor=100 -> 50%,
	//    Armor=900 -> 90%. The 100.0 constant is the half-mitigation pivot and
	//    will move to DT_AFL_DamageCurves in a later stage.
	const float ClampedArmor = FMath::Max(TargetArmor, 0.0f);
	const float Mitigation   = ClampedArmor / (ClampedArmor + 100.0f);

	// 5. Effective damage after mitigation. Never negative.
	const float EffectiveDamage = FMath::Max(RawDamage * (1.0f - Mitigation), 0.0f);

	if (EffectiveDamage <= 0.0f)
	{
		// AFL-0213: armor (or zero source damage) fully absorbed the hit. This is
		// a documented reject path — no attribute changes will be emitted, so
		// downstream listeners (UI hit markers, damage numbers) need to know
		// the shot landed but was nullified. EmitRejection logs only today;
		// AFL-1307 swaps the sink for PlayFab Player Streams.
		FAFLCombatTelemetry::EmitRejection(
			TEXT("mitigated"),
			Spec.GetEffectContext().GetEffectCauser(),
			FString::Printf(TEXT("raw=%.1f armor=%.1f"), RawDamage, ClampedArmor));
		return;
	}

	// 6. Shield absorbs first.
	const float ShieldAbsorbed = FMath::Min(FMath::Max(TargetShield, 0.0f), EffectiveDamage);
	const float ShieldDelta    = -ShieldAbsorbed;

	// 7. Remainder hits health.
	const float HealthDelta = -(EffectiveDamage - ShieldAbsorbed);

	// 8. Emit output modifiers. AddOutputModifier takes an FGameplayAttribute,
	// which is exactly what GetXxxAttribute() returns.
	if (FMath::Abs(ShieldDelta) > KINDA_SMALL_NUMBER)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			UAFLAttributeSet_Combat::GetShieldAttribute(),
			EGameplayModOp::Additive,
			ShieldDelta));
	}
	if (FMath::Abs(HealthDelta) > KINDA_SMALL_NUMBER)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			UAFLAttributeSet_Combat::GetHealthAttribute(),
			EGameplayModOp::Additive,
			HealthDelta));
	}

	// 9. AFL-0204 hit-confirm. EffectiveDamage > 0 has already been guaranteed
	//    by the early-return at (5); broadcast Event.Damage.Confirmed so the
	//    firing client's UAFLHitConfirmComponent can play the crosshair pulse
	//    + camera shake. We pull bone name from the EffectContext's hit result
	//    (populated by FAFLAbilityTargetData_Hitscan::AddTargetDataToContext)
	//    and distance from the claimed view origin Context.GetOrigin() — both
	//    are server-authoritative reads, no GetPlayerViewPoint involved.
	{
		UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
		AActor* ConfirmTargetActor = TargetASC ? TargetASC->GetAvatarActor_Direct() : nullptr;
		UWorld* ConfirmWorld = ConfirmTargetActor ? ConfirmTargetActor->GetWorld() : nullptr;
		if (ConfirmWorld)
		{
			const FGameplayEffectContextHandle& ContextHandle = Spec.GetEffectContext();
			const FHitResult* HitResult = ContextHandle.GetHitResult();

			FAFLHitConfirmMessage HitConfirm;
			HitConfirm.Instigator = ContextHandle.GetEffectCauser();
			HitConfirm.Target     = ConfirmTargetActor;
			HitConfirm.Damage     = EffectiveDamage;
			HitConfirm.BoneName   = HitResult ? HitResult->BoneName : NAME_None;

			if (HitResult)
			{
				HitConfirm.DistanceCm = static_cast<float>(
					FVector::Dist(ContextHandle.GetOrigin(), HitResult->ImpactPoint));
			}

			UGameplayMessageSubsystem::Get(ConfirmWorld).BroadcastMessage(
				FGameplayTag::RequestGameplayTag(NAME_Event_Damage_Confirmed_ExecCalc, false),
				HitConfirm);
		}
	}

	// 10. Overkill detection. If the health-damage component exceeded
	//    OverkillThreshold, broadcast Event.Damage.Overkill via the gameplay
	//    message subsystem. AFLDismember (Stage 5) will register a listener
	//    for this verb tag.
	//
	//    Mechanism choice: Lyra ships no precedent for emitting a spec-side
	//    tag from an ExecCalc — LyraDamageExecution only writes output
	//    modifiers. LyraHealthSet broadcasts FLyraVerbMessage on damage; we
	//    mirror that pattern here (server-side, observable, no const_cast on
	//    the immutable Spec). See ULyraHealthSet::PostGameplayEffectExecute.
	const float HealthComponent = -HealthDelta;  // positive damage actually dealt to health
	if (HealthComponent > TargetOverkillThreshold)
	{
		UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
		AActor* TargetActor = TargetASC ? TargetASC->GetAvatarActor_Direct() : nullptr;
		UWorld* World = TargetActor ? TargetActor->GetWorld() : nullptr;
		if (World)
		{
			FLyraVerbMessage Message;
			Message.Verb           = FGameplayTag::RequestGameplayTag(NAME_Event_Damage_Overkill_ExecCalc, false);
			Message.Instigator     = Spec.GetEffectContext().GetEffectCauser();
			Message.InstigatorTags = *Spec.CapturedSourceTags.GetAggregatedTags();
			Message.Target         = TargetActor;
			Message.TargetTags     = *Spec.CapturedTargetTags.GetAggregatedTags();
			Message.Magnitude      = HealthComponent;

			UGameplayMessageSubsystem::Get(World).BroadcastMessage(Message.Verb, Message);

			UE_LOG(LogAFLCombat, Verbose, TEXT("Overkill broadcast: HealthDmg=%.1f Threshold=%.1f Target=%s"),
				HealthComponent, TargetOverkillThreshold, *GetNameSafe(TargetActor));
		}
	}
#endif // WITH_SERVER_CODE
}
