// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "FunctionalTest.h"
#include "GameFramework/GameplayMessageSubsystem.h"

#include "AFLCombatPipelineTest_Base.generated.h"

class UAbilitySystemComponent;
struct FLyraVerbMessage;


/**
 * AAFLCombatPipelineTest_Base
 *
 * Sprint 1.5 Phase 2 — reusable base for AFL-0106 damage-pipeline functional
 * tests. Each placed instance encodes one row of the matrix via per-instance
 * UPROPERTY overrides; this base class provides the lifecycle, helpers, and
 * assertions shared across all rows.
 *
 * Test lifecycle (uses AFunctionalTest's standard async ladder):
 *   PrepareTest()             — cache local-player pawn + ASC
 *   IsReady_Implementation()  — poll until AFLAttributeSet_Combat is granted
 *                               (5s soft timeout, AddInfo on slow grant)
 *   StartTest()               — apply preconditions, register overkill
 *                               listener, fire damage GE
 *   Tick(DeltaSeconds)        — wait one tick for the GE to resolve, run
 *                               assertions, call FinishTest
 *
 * Row encoding lives in Phase 4 map/content (placed actor instance overrides).
 * The base class is intentionally row-agnostic.
 */
UCLASS(Abstract, Blueprintable)
class AFLCOMBATTESTS_API AAFLCombatPipelineTest_Base : public AFunctionalTest
{
	GENERATED_BODY()

public:

	AAFLCombatPipelineTest_Base(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin AFunctionalTest interface
	virtual void PrepareTest() override;
	virtual bool IsReady_Implementation() override;
	virtual void StartTest() override;
	virtual void Tick(float DeltaSeconds) override;
	//~ End AFunctionalTest interface

protected:

	// ----------------------------------------------------------------------
	// Row configuration — set per-instance on placed actors in the test map.
	// A sentinel of -1.0f on Precondition* means "leave the default from the
	// AbilitySet InitData GE". OverkillThreshold and MaxShield can be raised
	// or lowered; Health/Shield set after their Max companions for clamp safety.
	// ----------------------------------------------------------------------

	/** Human-readable row identifier for log output (e.g. "T1_BaseDamage25"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Row")
	FString RowLabel = TEXT("UnlabeledRow");

	/** Precondition override for Armor. -1 = leave default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Row|Preconditions")
	float PreconditionArmor = -1.0f;

	/** Precondition override for MaxShield. -1 = leave default. Applied before PreconditionShield. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Row|Preconditions")
	float PreconditionMaxShield = -1.0f;

	/** Precondition override for Shield. -1 = leave default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Row|Preconditions")
	float PreconditionShield = -1.0f;

	/** Precondition override for OverkillThreshold. -1 = leave default (50.0). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Row|Preconditions")
	float PreconditionOverkillThreshold = -1.0f;

	/** Source.Damage value written before the damage GE is applied. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Row|Damage", meta=(ClampMin="0.0"))
	float DamageBase = 25.0f;

	/** SetByCaller(Data.Damage.Headshot) multiplier. 1.0 = no effect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Row|Damage", meta=(ClampMin="0.0"))
	float DamageHeadshot = 1.0f;

	/** SetByCaller(Data.Damage.Weakpoint) multiplier. 1.0 = no effect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Row|Damage", meta=(ClampMin="0.0"))
	float DamageWeakpoint = 1.0f;

	/** SetByCaller(Data.Damage.Distance) multiplier. 1.0 = no falloff. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Row|Damage", meta=(ClampMin="0.0"))
	float DamageDistance = 1.0f;

	/** Expected Health after damage resolves. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Row|Expectations")
	float ExpectedHealth = 75.0f;

	/** Expected Shield after damage resolves. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Row|Expectations")
	float ExpectedShield = 0.0f;

	/** Expected count of Event.Damage.Overkill broadcasts during this test. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Row|Expectations", meta=(ClampMin="0"))
	int32 ExpectedOverkillCount = 0;

	/** Float-equality tolerance for ExpectedHealth/ExpectedShield assertions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Row|Expectations", meta=(ClampMin="0.0"))
	float Tolerance = 0.5f;

	/** Soft timeout for the IsReady-poll waiting on AFLAttributeSet_Combat grant. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Row|Timing", meta=(ClampMin="0.5"))
	float AttributeGrantTimeout = 5.0f;

	// ----------------------------------------------------------------------
	// Helpers (row-agnostic; the row content lives in placed-actor overrides).
	// ----------------------------------------------------------------------

	/** Find the local player pawn's UAbilitySystemComponent. Returns nullptr if not yet spawned. */
	UAbilitySystemComponent* ResolvePlayerASC() const;

	/** Override an attribute on the ASC via ApplyModToAttribute(Override). Short name lookup. */
	void ApplyAttributeOverride(UAbilitySystemComponent* ASC, const FString& ShortName, float Value);

	/** Apply all non-sentinel preconditions in clamp-safe order. */
	void ApplyPreconditions(UAbilitySystemComponent* ASC);

	/** Register a UGameplayMessageSubsystem listener for Event.Damage.Overkill. */
	void RegisterOverkillListener();

	/** Unregister the listener. Safe to call if never registered. */
	void UnregisterOverkillListener();

	/** Trigger the damage flow: write Source.Damage, build spec, inject SetByCallers, apply self. */
	void FireDamage(UAbilitySystemComponent* ASC);

	/** Read Health/Shield from the ASC, compare against expectations, AssertEqual_Float. */
	void EvaluateExpectations(UAbilitySystemComponent* ASC);

private:

	/** Listener message handler. */
	void OnOverkillReceived(FGameplayTag Channel, const FLyraVerbMessage& Message);

	/** Cached ASC for the local player; reset at PrepareTest. */
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	/** Listener handle returned by RegisterOverkillListener. */
	FGameplayMessageListenerHandle OverkillListenerHandle;

	/** Count of Event.Damage.Overkill broadcasts observed since StartTest. */
	int32 ObservedOverkillCount = 0;

	/** Accumulator for AttributeGrantTimeout in IsReady_Implementation. */
	float WaitedForAttributeSet = 0.0f;

	/** True once StartTest has fired and damage has been applied. */
	bool bDamageFired = false;

	/** Accumulator for the post-damage settle frame (we wait one Tick before asserting). */
	float SettleTime = 0.0f;
};
