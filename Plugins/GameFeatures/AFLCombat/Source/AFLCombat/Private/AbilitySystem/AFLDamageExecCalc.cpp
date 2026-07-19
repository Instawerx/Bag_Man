// Copyright C12 AI Gaming. All Rights Reserved.

#include "AbilitySystem/AFLDamageExecCalc.h"

#include "AFLBodyZone.h"   // S4-INC3: EAFLBodyZone + AFLCore::BoneToZone (AFLCore)
#include "AFLCombat.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"   // CONVERGENCE: Health lives here now -> output to its Damage meta
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Attributes/AFLAttributeSet_Energy.h"         // overload eligibility: CarriedEnergy capture
#include "Engine/HitResult.h"
#include "HAL/IConsoleManager.h"                        // overload: read afl.Overload.MinEnergy (owned by AFLDeathComponent)
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Messages/AFLHitConfirmMessage.h"   // AFLCore (relocated -- drop-on-damage cycle)
#include "HUD/AFLDismemberSeverMessage.h"     // S4-INC3: the live zone-HP sever broadcast
#include "HUD/AFLOverkillMessage.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"
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

	// AFL overkill channel — broadcast alongside Event.Damage.Overkill, carrying
	// BoneName for the dismember system (S4-02). Canonical tag in AFLCoreTags.ini.
	const FName NAME_Event_Damage_Overkill_AFL_ExecCalc = TEXT("Event.Damage.Overkill.AFL");

	// AFL-0204: broadcast when EffectiveDamage > 0 so the firing client's
	// UAFLHitConfirmComponent can play crosshair pulse + camera shake.
	const FName NAME_Event_Damage_Confirmed_ExecCalc = TEXT("Event.Damage.Confirmed");

	// S4-INC3: the live zone-HP sever broadcast. A limb/head falls off when ITS zone-HP
	// depletes on a hit -- the dismember component (PHASE B) listens to this. Canonical tag
	// in AFLCombatTags.ini (added PHASE B); RequestGameplayTag(ErrorIfNotFound=false) until then.
	const FName NAME_Event_Dismember_Sever_AFL_ExecCalc = TEXT("Event.Dismember.Sever.AFL");

	// CONVERGENCE (S7 AFL-0706 overload port): broadcast on a would-be-killing-blow that the ExecCalc
	// clamped to survive. UAFLDeathComponent's Event.Combat.Overload handler does the burst/restore/stun/announce.
	const FName NAME_Event_Combat_Overload_ExecCalc = TEXT("Event.Combat.Overload");
}

// Victim-state tag for the carrier-vulnerability check (stress-object cycle). Native define (with the
// per-file Unity-safe suffix, matching the fire abilities' convention) rather than a runtime Request:
// self-registering at module load, so the check can never silently read an empty tag.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Carrying_Vulnerable_ExecCalc, "State.Carrying.Vulnerable");

// Attacker-state tag for the Overdrive damage buff (energy cycle 2) -- the SOURCE-side mirror of the
// victim-side vulnerability check below.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Energy_Overdrive_ExecCalc, "State.Energy.Overdrive");

// CONVERGENCE overload port: the re-overload lockout tag (granted by UAFLGE_OverloadStun in the handler).
// The ExecCalc checks it on the target's aggregated tags to gate the survive-clamp -- mirrors the old
// AFLDeathComponent HasMatchingGameplayTag(State.Overloaded) check exactly.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Overloaded_ExecCalc, "State.Overloaded");


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
	// CONVERGENCE: Health lives on ULyraHealthSet now. Capture its CURRENT value (for the overload survive-clamp);
	// the damage OUTPUT goes to ULyraHealthSet.Damage (meta). CarriedEnergy gates the overload survive.
	FGameplayEffectAttributeCaptureDefinition LyraHealthDef;
	FGameplayEffectAttributeCaptureDefinition CarriedEnergyDef;
	FGameplayEffectAttributeCaptureDefinition OverkillThresholdDef;
	// S4-INC3: per-zone HP (Target/live), the outermost absorber.
	FGameplayEffectAttributeCaptureDefinition HeadHealthDef;
	FGameplayEffectAttributeCaptureDefinition LeftArmHealthDef;
	FGameplayEffectAttributeCaptureDefinition RightArmHealthDef;
	FGameplayEffectAttributeCaptureDefinition LeftLegHealthDef;
	FGameplayEffectAttributeCaptureDefinition RightLegHealthDef;

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
		LyraHealthDef = FGameplayEffectAttributeCaptureDefinition(
			ULyraHealthSet::GetHealthAttribute(),
			EGameplayEffectAttributeCaptureSource::Target, false);
		CarriedEnergyDef = FGameplayEffectAttributeCaptureDefinition(
			UAFLAttributeSet_Energy::GetCarriedEnergyAttribute(),
			EGameplayEffectAttributeCaptureSource::Target, false);
		OverkillThresholdDef = FGameplayEffectAttributeCaptureDefinition(
			UAFLAttributeSet_Combat::GetOverkillThresholdAttribute(),
			EGameplayEffectAttributeCaptureSource::Target, false);

		// S4-INC3: zone-HP, target-side live (current value at execution, like Health).
		HeadHealthDef = FGameplayEffectAttributeCaptureDefinition(
			UAFLAttributeSet_Combat::GetHeadHealthAttribute(),
			EGameplayEffectAttributeCaptureSource::Target, false);
		LeftArmHealthDef = FGameplayEffectAttributeCaptureDefinition(
			UAFLAttributeSet_Combat::GetLeftArmHealthAttribute(),
			EGameplayEffectAttributeCaptureSource::Target, false);
		RightArmHealthDef = FGameplayEffectAttributeCaptureDefinition(
			UAFLAttributeSet_Combat::GetRightArmHealthAttribute(),
			EGameplayEffectAttributeCaptureSource::Target, false);
		LeftLegHealthDef = FGameplayEffectAttributeCaptureDefinition(
			UAFLAttributeSet_Combat::GetLeftLegHealthAttribute(),
			EGameplayEffectAttributeCaptureSource::Target, false);
		RightLegHealthDef = FGameplayEffectAttributeCaptureDefinition(
			UAFLAttributeSet_Combat::GetRightLegHealthAttribute(),
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
	RelevantAttributesToCapture.Add(AFLDamageCaptureDefs().LyraHealthDef);
	RelevantAttributesToCapture.Add(AFLDamageCaptureDefs().CarriedEnergyDef);
	RelevantAttributesToCapture.Add(AFLDamageCaptureDefs().OverkillThresholdDef);
	// S4-INC3: zone-HP captures.
	RelevantAttributesToCapture.Add(AFLDamageCaptureDefs().HeadHealthDef);
	RelevantAttributesToCapture.Add(AFLDamageCaptureDefs().LeftArmHealthDef);
	RelevantAttributesToCapture.Add(AFLDamageCaptureDefs().RightArmHealthDef);
	RelevantAttributesToCapture.Add(AFLDamageCaptureDefs().LeftLegHealthDef);
	RelevantAttributesToCapture.Add(AFLDamageCaptureDefs().RightLegHealthDef);
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

	// CONVERGENCE: current Lyra Health (for the overload survive-clamp) + CarriedEnergy (overload eligibility).
	float CurLyraHealth = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		AFLDamageCaptureDefs().LyraHealthDef, EvalParams, CurLyraHealth);

	float CarriedEnergy = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		AFLDamageCaptureDefs().CarriedEnergyDef, EvalParams, CarriedEnergy);

	float TargetOverkillThreshold = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		AFLDamageCaptureDefs().OverkillThresholdDef, EvalParams, TargetOverkillThreshold);

	// S4-INC3: capture the zone-HP (live target values) for the limb-absorber (step 5c).
	float TargetHeadHealth = 0.0f, TargetLeftArmHealth = 0.0f, TargetRightArmHealth = 0.0f,
	      TargetLeftLegHealth = 0.0f, TargetRightLegHealth = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(AFLDamageCaptureDefs().HeadHealthDef,     EvalParams, TargetHeadHealth);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(AFLDamageCaptureDefs().LeftArmHealthDef,  EvalParams, TargetLeftArmHealth);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(AFLDamageCaptureDefs().RightArmHealthDef, EvalParams, TargetRightArmHealth);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(AFLDamageCaptureDefs().LeftLegHealthDef,  EvalParams, TargetLeftLegHealth);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(AFLDamageCaptureDefs().RightLegHealthDef, EvalParams, TargetRightLegHealth);

	// 2. SetByCaller multipliers (default 1.0 when the ability didn't provide them).
	const float HeadshotMult    = Spec.GetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Headshot_ExecCalc,  false), false, 1.0f);
	const float DistanceFalloff = Spec.GetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Distance_ExecCalc,  false), false, 1.0f);
	const float WeakpointMult   = Spec.GetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Weakpoint_ExecCalc, false), false, 1.0f);

	// 3. Raw damage before mitigation.
	float RawDamage = SourceDamage * HeadshotMult * WeakpointMult * DistanceFalloff;

	// 3b. Overdrive damage buff (energy cycle 2): an ATTACKER carrying State.Energy.Overdrive deals
	//     amplified damage. Source tags were captured with the spec (EvalParams above -- zero new
	//     captures, the same free seam as the victim side). PRE-mitigation by design: a damage buff
	//     scales the shot itself; armor then mitigates the bigger shot. Symmetric with the victim-side
	//     vulnerability at step 5b (which is POST-mitigation, pre-shield). The 1.25 constant moves to
	//     DT_AFL_DamageCurves alongside the armor pivot (step 4).
	if (EvalParams.SourceTags && EvalParams.SourceTags->HasTag(TAG_State_Energy_Overdrive_ExecCalc))
	{
		RawDamage *= 1.25f;
	}

	// 4. Reciprocal mitigation curve. Armor=0 -> 0% mitigation, Armor=100 -> 50%,
	//    Armor=900 -> 90%. The 100.0 constant is the half-mitigation pivot and
	//    will move to DT_AFL_DamageCurves in a later stage.
	const float ClampedArmor = FMath::Max(TargetArmor, 0.0f);
	const float Mitigation   = ClampedArmor / (ClampedArmor + 100.0f);

	// 5. Effective damage after mitigation. Never negative. (Non-const: the carrier-vulnerability
	//    step below may amplify it.)
	float EffectiveDamage = FMath::Max(RawDamage * (1.0f - Mitigation), 0.0f);

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

	// 5b. Carrier vulnerability (stress-object cycle): a victim carrying a vulnerability-flagged object
	//     takes amplified damage. The tag rides the spec's captured TARGET tags (aggregated into
	//     EvalParams at the top); it is granted by the per-object carrier GE (UGE_AFL_CarrierVulnerability
	//     via FAFLGrabPolicy.CarrierEffectClass). Applied POST-mitigation, PRE-shield-split, so the whole
	//     amplified hit drains shield first as normal. A fully-mitigated hit stays rejected (the early
	//     return above) -- vulnerability cannot resurrect a zero. Symmetric with the SOURCE-side Overdrive
	//     buff at step 3b (which multiplies PRE-mitigation). The 1.3 constant will move to
	//     DT_AFL_DamageCurves alongside the armor half-mitigation pivot (step 4).
	if (EvalParams.TargetTags && EvalParams.TargetTags->HasTag(TAG_State_Carrying_Vulnerable_ExecCalc))
	{
		EffectiveDamage *= 1.3f;
	}

	// 5c. S4-INC3 ZONE-HP LIMB ABSORBER (the outermost absorber, AFTER carrier-vuln, BEFORE shield).
	//     Classify the hit bone -> zone (AFLCore::BoneToZone -- AFLCombat-reachable, no AFLDismember
	//     dep). For a LIMB or HEAD zone, the matching zone-HP drains FIRST; only the OVERFLOW past
	//     zero continues to shield/health. Zone-HP <= 0 already = severed/inert:
	//       - LIMB: a DEAD ZONE -- consume the hit entirely (no limb dmg, no body dmg, no re-sever).
	//       - HEAD: inert too (no double-decapitate; head already off).
	//     Crossing <= 0 THIS hit broadcasts Event.Dismember.Sever.AFL (bLethal: Head=true, limbs=false).
	//     Head overflow STILL flows to health (head is the lethal zone). Torso/None: SKIP (body damage).
	//     The sever decision is computed from the CAPTURED (pre-change) zone-HP vs EffectiveDamage --
	//     never from a post-clamp read (PreAttributeChange only floors the STORED value).
	{
		const FHitResult* ZoneHitResult = Spec.GetEffectContext().GetHitResult();
		const FName ZoneBone = ZoneHitResult ? ZoneHitResult->BoneName : NAME_None;
		const EAFLBodyZone Zone = AFLCore::BoneToZone(ZoneBone);

		// Map the zone -> its captured HP + the FGameplayAttribute to drain. Head/limbs only.
		float ZoneHP = -1.0f;            // -1 = "not a zone we route" (Torso/None handled by the if below)
		FGameplayAttribute ZoneAttr;
		bool bIsZoneRouted = true;
		switch (Zone)
		{
		case EAFLBodyZone::Head:     ZoneHP = TargetHeadHealth;     ZoneAttr = UAFLAttributeSet_Combat::GetHeadHealthAttribute();     break;
		case EAFLBodyZone::LeftArm:  ZoneHP = TargetLeftArmHealth;  ZoneAttr = UAFLAttributeSet_Combat::GetLeftArmHealthAttribute();  break;
		case EAFLBodyZone::RightArm: ZoneHP = TargetRightArmHealth; ZoneAttr = UAFLAttributeSet_Combat::GetRightArmHealthAttribute(); break;
		case EAFLBodyZone::LeftLeg:  ZoneHP = TargetLeftLegHealth;  ZoneAttr = UAFLAttributeSet_Combat::GetLeftLegHealthAttribute();  break;
		case EAFLBodyZone::RightLeg: ZoneHP = TargetRightLegHealth; ZoneAttr = UAFLAttributeSet_Combat::GetRightLegHealthAttribute(); break;
		default:                     bIsZoneRouted = false;         break;   // Torso / None -> body chain unchanged
		}

		if (bIsZoneRouted)
		{
			const bool bIsHead = (Zone == EAFLBodyZone::Head);

			if (ZoneHP > KINDA_SMALL_NUMBER)
			{
				// Living zone: drain it first. The absorbed portion is consumed by the zone.
				const float Absorbed = FMath::Min(ZoneHP, EffectiveDamage);
				OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
					ZoneAttr, EGameplayModOp::Additive, -Absorbed));

				const bool bDepletes = (EffectiveDamage >= ZoneHP - KINDA_SMALL_NUMBER);

				// S4-INC3 PHASE B-1 -- LOCKED HEAD MODEL: decapitation is a SURVIVABLE state change
				// (head loot-box tied to owner life), NOT a kill. A head hit deals ZERO body damage:
				// the head zone CONSUMES the whole hit (like an inert zone -- no overflow to shield/
				// health). Limbs keep the overflow -> body chain. So:
				//   - HEAD: EffectiveDamage = 0 (consumed). The early-return below stops the body chain.
				//   - LIMB: EffectiveDamage -= Absorbed (overflow continues to shield/health).
				if (bIsHead)
				{
					EffectiveDamage = 0.0f;   // decap = zero body damage; head consumes the hit
				}
				else
				{
					EffectiveDamage -= Absorbed;   // limb overflow continues to the body
				}

				if (bDepletes)
				{
					// Limb: EffectiveDamage == overflow. Head: 0 (decap spills nothing).
					const double SeverOverflow = static_cast<double>(FMath::Max(0.0f, EffectiveDamage));

					UAbilitySystemComponent* SeverASC = ExecutionParams.GetTargetAbilitySystemComponent();
					AActor* SeverActor = SeverASC ? SeverASC->GetAvatarActor_Direct() : nullptr;
					UWorld* SeverWorld = SeverActor ? SeverActor->GetWorld() : nullptr;
					if (SeverWorld)
					{
						FAFLDismemberSeverMessage Sever;
						Sever.Instigator = Spec.GetEffectContext().GetEffectCauser();
						Sever.Target     = SeverActor;
						Sever.BoneName   = ZoneBone;
						Sever.Zone       = Zone;
						// LOCKED MODEL: decapitation is SURVIVABLE (head loot-box), NOT a kill -- bLethal
						// is false for ALL zones now. The consumer branches on Zone==Head for the head
						// path (grant State.Decapitated + spawn the head loot-box). Field kept for a
						// future genuinely-lethal zone; head no longer sets it.
						Sever.bLethal    = false;
						Sever.Overflow   = SeverOverflow;

						// PRESENTATION (dismember pass): the shot vector so the gib pops AWAY from the shooter (the
						// gib pop biases its linear impulse along this; ZeroVector -> the pop's forward-cone fallback).
						// Derive from the hit's trace ray (TraceStart->TraceEnd), else the negated surface normal.
						FVector SeverShotDir = FVector::ZeroVector;
						if (const FHitResult* SeverHit = Spec.GetEffectContext().GetHitResult())
						{
							SeverShotDir = (SeverHit->TraceEnd - SeverHit->TraceStart).GetSafeNormal();
							if (SeverShotDir.IsNearlyZero())
							{
								SeverShotDir = (-SeverHit->ImpactNormal).GetSafeNormal();
							}
						}
						Sever.HitDirection = SeverShotDir;

						UGameplayMessageSubsystem::Get(SeverWorld).BroadcastMessage(
							FGameplayTag::RequestGameplayTag(NAME_Event_Dismember_Sever_AFL_ExecCalc, false),
							Sever);

						// Instrument the TRUTH: head= is the zone discriminator (was mislabeled "lethal");
						// lethal= is the actual broadcast Sever.bLethal (false for all zones under the
						// locked survivable-decap model). Logging both so a log-reader is never misled.
						UE_LOG(LogAFLCombat, Log,
							TEXT("AFL_SEVER: zone=%d bone=%s head=%d lethal=%d zoneHP=%.2f overflow=%.2f target=%s"),
							static_cast<int32>(Zone), *ZoneBone.ToString(), bIsHead ? 1 : 0,
							Sever.bLethal ? 1 : 0, ZoneHP, SeverOverflow, *GetNameSafe(SeverActor));
					}
				}
			}
			else
			{
				// Zone already severed / inert (HP <= 0): a DEAD-ZONE hit.
				//   Limb: changes NOTHING -- consume the whole hit (no body damage, no re-sever).
				//   Head: head is gone; treat the same (no double-decapitate). Body takes nothing from
				//   a bone-hit on a missing head this hit (the kill already happened on decapitation).
				EffectiveDamage = 0.0f;
			}
		}
	}

	if (EffectiveDamage <= 0.0f)
	{
		// S4-INC3: a dead-zone hit (severed limb / off head) fully consumed the hit. No shield/health
		// modifiers, no overkill. Mirrors the step-5 mitigated early-return (no body damage lands).
		return;
	}

	// 6. Shield absorbs first.
	const float ShieldAbsorbed = FMath::Min(FMath::Max(TargetShield, 0.0f), EffectiveDamage);
	const float ShieldDelta    = -ShieldAbsorbed;

	// 7. Remainder hits health. CONVERGENCE: Health lives on ULyraHealthSet -- output a POSITIVE value to its
	//    Damage META (not a negative delta on AFL Health). ULyraHealthSet::PostGameplayEffectExecute converts
	//    Damage -> -Health (clamped [0,Max]), fires the standardized Lyra.Damage.Message, and fires OnOutOfHealth
	//    natively -- UAFLDeathComponent now binds THAT.
	float HealthDamage = FMath::Max(0.0f, EffectiveDamage - ShieldAbsorbed);

	// 7b. OVERLOAD (S7 AFL-0706, Option-A port). A hit that WOULD drop Health to <=0 while carrying enough energy
	//     and not already overloaded -> SURVIVE: clamp the damage so Health lands at 1 (NO 0-crossing -> Lyra's
	//     native death/elimination never fires -> no false kill), and broadcast Event.Combat.Overload so
	//     UAFLDeathComponent's handler does the burst/restore-to-floor/stun/announce (deferred, re-entrancy-safe).
	//     Eligibility mirrors the OLD AFLDeathComponent intercept EXACTLY: CarriedEnergy >= afl.Overload.MinEnergy
	//     AND NOT State.Overloaded (the re-overload lockout, read off the target's aggregated tags).
	if (CurLyraHealth - HealthDamage <= KINDA_SMALL_NUMBER)
	{
		static IConsoleVariable* CVarOverloadMinEnergy =
			IConsoleManager::Get().FindConsoleVariable(TEXT("afl.Overload.MinEnergy"));
		const float MinEnergy = CVarOverloadMinEnergy ? CVarOverloadMinEnergy->GetFloat() : 1.0f;
		const bool bLockedOut = EvalParams.TargetTags && EvalParams.TargetTags->HasTag(TAG_State_Overloaded_ExecCalc);
		if (CarriedEnergy >= MinEnergy && !bLockedOut)
		{
			HealthDamage = FMath::Max(0.0f, CurLyraHealth - 1.0f);   // survive at 1; the handler restores to the floor

			UAbilitySystemComponent* OverloadASC = ExecutionParams.GetTargetAbilitySystemComponent();
			AActor* OverloadActor = OverloadASC ? OverloadASC->GetAvatarActor_Direct() : nullptr;
			if (UWorld* OverloadWorld = OverloadActor ? OverloadActor->GetWorld() : nullptr)
			{
				FLyraVerbMessage OverloadMsg;
				OverloadMsg.Verb       = FGameplayTag::RequestGameplayTag(NAME_Event_Combat_Overload_ExecCalc, false);
				OverloadMsg.Instigator = OverloadActor;
				OverloadMsg.Target     = OverloadActor;
				OverloadMsg.Magnitude  = CarriedEnergy;
				UGameplayMessageSubsystem::Get(OverloadWorld).BroadcastMessage(OverloadMsg.Verb, OverloadMsg);
			}
			UE_LOG(LogAFLCombat, Log,
				TEXT("AFL_OVERLOAD: %s would die (energy %.1f) -> clamped to survive; Event.Combat.Overload broadcast."),
				*GetNameSafe(OverloadActor), CarriedEnergy);
		}
	}

	// 8. Emit output modifiers. Shield stays on the AFL set; Health damage -> ULyraHealthSet.Damage (meta).
	if (FMath::Abs(ShieldDelta) > KINDA_SMALL_NUMBER)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			UAFLAttributeSet_Combat::GetShieldAttribute(),
			EGameplayModOp::Additive,
			ShieldDelta));
	}
	if (HealthDamage > KINDA_SMALL_NUMBER)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			ULyraHealthSet::GetDamageAttribute(),
			EGameplayModOp::Additive,
			HealthDamage));
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
	const float HealthComponent = HealthDamage;  // positive damage actually dealt to health (post-overload-clamp; overload -> small -> no overkill/gib)
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

			// AFL overkill broadcast (S4-02): alongside the Lyra verb message above,
			// emit a dedicated AFL message carrying BoneName so the dismember system
			// (UAFLDismemberComponent, S4-04) knows which zone the killing blow hit.
			// HitResult is re-fetched here: the Confirmed block's local fell out of
			// scope at its closing brace.
			const FHitResult* OverkillHitResult = Spec.GetEffectContext().GetHitResult();

			FAFLOverkillMessage AFLOverkill;
			AFLOverkill.Instigator = Spec.GetEffectContext().GetEffectCauser();
			AFLOverkill.Target     = TargetActor;
			AFLOverkill.BoneName   = OverkillHitResult ? OverkillHitResult->BoneName : NAME_None;
			AFLOverkill.Magnitude  = HealthComponent;

			UGameplayMessageSubsystem::Get(World).BroadcastMessage(
				FGameplayTag::RequestGameplayTag(NAME_Event_Damage_Overkill_AFL_ExecCalc, false),
				AFLOverkill);

			UE_LOG(LogAFLCombat, Verbose, TEXT("Overkill broadcast: HealthDmg=%.1f Threshold=%.1f Target=%s Bone=%s"),
				HealthComponent, TargetOverkillThreshold, *GetNameSafe(TargetActor), *AFLOverkill.BoneName.ToString());
		}
	}
#endif // WITH_SERVER_CODE
}
