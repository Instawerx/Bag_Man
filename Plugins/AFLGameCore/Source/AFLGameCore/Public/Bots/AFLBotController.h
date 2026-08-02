// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Bots/AFLBotAimTypes.h"
#include "Player/LyraPlayerBotController.h"

#include "AFLBotController.generated.h"

/**
 * AAFLBotController  (AI-1 -- the bot aim model)
 *
 * THE PROBLEM. Stock bot aim is a perfect, instantaneous ray. BTS_SetFocus calls SetFocus(target) and
 * AAIController::UpdateControlRotation then recomputes control rotation to point exactly at the focal
 * point EVERY FRAME -- no smoothing, no error, no delay. The trace reads that rotation straight off the
 * pawn (AFLAG_Hitscan_Base::ClientPredictAndSend takes GetViewRotation() for anything that is not a
 * PlayerController), so a bot is a hitscan turret that never misses and never looks like it is trying.
 *
 * THE SEAM. Humans are untouched BY CONSTRUCTION, not by a flag: this is an AAIController override, and
 * a human has no AAIController. There is no code path here a player can reach.
 *
 * THE STRUCTURAL IDEA -- WOBBLE GOES INTO THE TARGET, NOT THE OUTPUT. The bot chases a slightly-wrong,
 * slowly-drifting point while a spring-damper closes on it. Two things fall out for free:
 *   - motion is CONTINUOUS. Nothing here snaps, so nothing trips the AimAngularVelocity anti-cheat
 *     telemetry on Pulse and fills it with bot noise.
 *   - micro-correction is EMERGENT rather than authored. The bot is always slightly behind a moving
 *     point, which is what tracking actually looks like when a person does it.
 *
 * OVERSHOOT IS REQUIRED, not incidental. Damping ratio is held below 1 at every tier, so aim passes the
 * target and corrects. An asymptotic tracker -- error only ever shrinking -- is the most robotic-looking
 * option available even at a slow rate, and "never overshoots" is a test FAILURE (see afl.Bot.AimProbe).
 *
 * WHAT MUST NOT BE LOST. This override REPLACES AAIController::UpdateControlRotation rather than calling
 * Super (Super would immediately overwrite the model's output with a perfect snap). Three stock
 * behaviours are therefore reproduced by hand and each is a silent regression if dropped:
 *   1. the FAISystem::IsValidLocation(FocalPoint) check, with the bSetControlRotationFromPawnOrientation
 *      fallback and the leave-rotation-alone case beneath it
 *   2. pitch zeroed unless the focus is a Pawn
 *   3. the bUpdatePawn -> FaceRotation call that turns the BODY (distinct from aim, and the thing that
 *      makes the error visible to an observer)
 */
UCLASS(Blueprintable)
class AFLGAMECORE_API AAFLBotController : public ALyraPlayerBotController
{
	GENERATED_BODY()

public:
	AAFLBotController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The tier table. Null = the model is INERT and stock snap-aim is used, so a missing asset degrades
	 *  to today's behaviour rather than to a bot that cannot aim. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotAim")
	TObjectPtr<UAFLBotAimTiers> AimTiers;

	/** Resolved values in use right now (recomputed when the round advances). Probe + log read this. */
	const FAFLBotAimProfile& GetAimProfile() const { return Profile; }

	/** The bot's stable personality roll. */
	const FAFLBotAimRoll& GetAimRoll() const { return Roll; }

	/** 0..1 tier currently resolved from match state. */
	float GetAimTier() const { return CachedTier; }

	/** Metrics the probe asserts on. Reset at each acquisition. */
	float GetLastReactionDelay() const { return LastReactionDelay; }
	float GetPeakTrackRate() const { return PeakTrackRateThisAcquire; }
	int32 GetErrorSignCrossings() const { return SignCrossings; }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn) override;

private:
	/** Roll the personality ONCE, from a seed stable for this bot's life. */
	void RollProfile();

	/** Re-resolve Profile from the current round. Cheap; called only when the round number changes. */
	void RefreshTier();

	/** Continuous two-frequency drift, degrees. Incommensurate rates so it never visibly repeats. */
	float Wobble(double TimeSeconds, float PhaseA, float PhaseB) const;

	FAFLBotAimRoll    Roll;
	FAFLBotAimProfile Profile;

	// -- runtime aim state --
	float  AimVelYawDegPerSec   = 0.0f;
	float  AimVelPitchDegPerSec = 0.0f;
	double ReactionEndsAtSeconds = 0.0;
	TWeakObjectPtr<AActor> LastFocusActor;

	/** Per-bot wobble phases, so two bots never drift in sympathy. */
	float WobblePhaseA = 0.0f;
	float WobblePhaseB = 0.0f;

	int32 CachedRound = -1;
	float CachedTier  = 0.0f;

	// -- probe metrics --
	double AcquireTimeSeconds      = 0.0;
	float  LastReactionDelay       = 0.0f;
	float  PeakTrackRateThisAcquire = 0.0f;
	int32  SignCrossings           = 0;
	int32  LastYawErrorSign        = 0;
	bool   bTrackingStarted        = false;
};
