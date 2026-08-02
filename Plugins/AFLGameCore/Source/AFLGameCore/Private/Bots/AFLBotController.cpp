// Copyright C12 AI Gaming. All Rights Reserved.

#include "Bots/AFLBotController.h"

#include "AFLGameCore.h"
#include "AFLMatchTierSource.h"
#include "AITypes.h"                 // FAISystem::IsValidLocation -- the stock focal-point validity check
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Math/RandomStream.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLBotController)

AAFLBotController::AAFLBotController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AAFLBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	RollProfile();
	RefreshTier();

	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFL_BOTAIM: ROLL    %s tier=%.2f react=%.3fs rate=%.0fd/s stiff=%.0f damp=%.2f err=%.2fdeg freq=%.2fHz"),
		*GetName(), CachedTier, Profile.ReactionSeconds, Profile.MaxTrackRateDegPerSec,
		Profile.TrackStiffness, Profile.TrackDampingRatio, Profile.SteadyErrorDeg, Profile.ErrorFrequencyHz);
}

void AAFLBotController::RollProfile()
{
	if (Roll.bRolled)
	{
		return;   // personality is for life. Re-rolling turns character into noise.
	}

	// SEED STABILITY is the whole point. The bot's own name is stable for its lifetime, unique between
	// bots, and survives possession changes -- so the same bot always rolls the same personality while
	// two bots at the same tier reliably differ. Prefer the PlayerState name (the RandomBotNames entry,
	// which is also what shows in the scoreboard) so a logged roll is traceable to a bot you can see.
	FString SeedSource = GetName();
	if (const APlayerState* PS = GetPlayerState<APlayerState>())
	{
		const FString PlayerName = PS->GetPlayerName();
		if (!PlayerName.IsEmpty())
		{
			SeedSource = PlayerName;
		}
	}

	FRandomStream Stream(static_cast<int32>(FCrc::StrCrc32(*SeedSource)));
	auto Signed = [&Stream]() { return Stream.FRandRange(-1.0f, 1.0f); };

	Roll.Reaction    = Signed();
	Roll.TrackRate   = Signed();
	Roll.Stiffness   = Signed();
	Roll.Damping     = Signed();
	Roll.SteadyError = Signed();
	Roll.ErrorFreq   = Signed();
	Roll.bRolled     = true;

	// Distinct wobble phases so a squad never drifts in sympathy -- synchronised wobble across five bots
	// reads as a single puppeteer.
	WobblePhaseA = Stream.FRandRange(0.0f, 2.0f * PI);
	WobblePhaseB = Stream.FRandRange(0.0f, 2.0f * PI);
}

void AAFLBotController::RefreshTier()
{
	if (!AimTiers)
	{
		return;
	}

	int32 Round = 0;
	int32 RoundsToWin = 1;
	int32 ScoreDelta = 0;

	// The round FSM lives in a GameFeature; reach it only through the always-loaded interface so the
	// dependency stays GameFeature -> always-loaded. No implementer (warmup, a non-round mode, a probe
	// map) simply leaves tier at 0 -- the weakest, safest end of the curve.
	if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr)
	{
		for (UActorComponent* Comp : GS->GetComponents())
		{
			if (const IAFLMatchTierSource* Src = Cast<IAFLMatchTierSource>(Comp))
			{
				Round       = Src->GetCurrentRoundNumber();
				RoundsToWin = FMath::Max(1, Src->GetRoundsToWin());
				ScoreDelta  = Src->GetScoreDelta();
				break;
			}
		}
	}

	CachedRound = Round;

	// ROUND NUMBER RAISES. SCORE DELTA ONLY EVER HOLDS BACK, and symmetrically -- it cannot manufacture
	// a comeback because it never raises and it ignores WHICH side is ahead. BlowoutDamping is 0 in v1,
	// so this reduces to a pure function of round number until the base model is proven.
	const float RoundProgress = FMath::Clamp(static_cast<float>(Round) / static_cast<float>(RoundsToWin), 0.0f, 1.0f);
	const float Blowout = FMath::Clamp(
		(static_cast<float>(FMath::Abs(ScoreDelta)) - AimTiers->BlowoutThreshold) / FMath::Max(1.0f, AimTiers->BlowoutRange),
		0.0f, 1.0f);
	CachedTier = RoundProgress * (1.0f - AimTiers->BlowoutDamping * Blowout);

	Profile = AimTiers->Resolve(CachedTier, Roll);

	UE_LOG(LogAFLGameCore, Log, TEXT("AFL_BOTAIM: TIER    round=%d/%d delta=%d -> tier=%.2f (blowout damp %.2f)"),
		Round, RoundsToWin, ScoreDelta, CachedTier, AimTiers->BlowoutDamping * Blowout);
}

float AAFLBotController::Wobble(double TimeSeconds, float PhaseA, float PhaseB) const
{
	// Two incommensurate frequencies (1 : 1.7) so the drift never visibly repeats, and CONTINUOUS so the
	// slew stays smooth. A per-tick random offset would be jitter, not wobble -- it would look wrong and
	// it would spike angular velocity into the anti-cheat telemetry.
	const float W = Profile.SteadyErrorDeg;
	const float F = Profile.ErrorFrequencyHz;
	const float T = static_cast<float>(TimeSeconds);
	return W * (0.6f * FMath::Sin(2.0f * PI * F * T + PhaseA)
	          + 0.4f * FMath::Sin(2.0f * PI * F * 1.7f * T + PhaseB));
}

void AAFLBotController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	APawn* const MyPawn = GetPawn();
	UWorld* const World = GetWorld();

	// INERT WITHOUT A TIER TABLE. Falls back to stock snap-aim rather than to a bot that cannot aim.
	if (!MyPawn || !World || !AimTiers)
	{
		Super::UpdateControlRotation(DeltaTime, bUpdatePawn);
		return;
	}

	if (CachedRound < 0 || !Roll.bRolled)
	{
		RollProfile();
		RefreshTier();
	}

	FRotator NewControlRotation = GetControlRotation();

	// STOCK BEHAVIOUR 1 of 3 -- the focal-point validity check, with both fallbacks beneath it.
	const FVector FocalPoint = GetFocalPoint();
	if (FAISystem::IsValidLocation(FocalPoint))
	{
		AActor* const FocusActor = GetFocusActor();
		const double Now = World->GetTimeSeconds();

		// ACQUISITION. A new focus is a new perception event, so the reaction cost is paid again --
		// this is what stops a bot punishing every peek on the frame it becomes visible.
		if (FocusActor != LastFocusActor.Get())
		{
			// Fold the closing acquisition's excursion into the lifetime maxima BEFORE resetting, or a
			// bot that overshoots then re-acquires loses the evidence -- which is exactly how the probe
			// came to read 0 crossings on bots that had been crossing all round.
			LifetimeMaxOvershootDeg = FMath::Max(LifetimeMaxOvershootDeg, OvershootPeakThisExcursion);
			LifetimeMaxCrossings    = FMath::Max(LifetimeMaxCrossings, SignCrossings);

			LastFocusActor = FocusActor;
			const float Jitter = FMath::FRandRange(-AimTiers->ReactionJitterSeconds, AimTiers->ReactionJitterSeconds);
			LastReactionDelay = FMath::Max(AimTiers->MinReactionSeconds, Profile.ReactionSeconds + Jitter);
			ReactionEndsAtSeconds = Now + LastReactionDelay;
			LifetimeMinReactionDelay = FMath::Min(LifetimeMinReactionDelay, LastReactionDelay);
			++AcquisitionCount;

			AimVelYawDegPerSec = 0.0f;          // start from rest: accelerates in, never steps
			AimVelPitchDegPerSec = 0.0f;
			AcquireTimeSeconds = Now;
			PeakTrackRateThisAcquire = 0.0f;
			SignCrossings = 0;
			LastTrueYawErrorSign = 0;
			OvershootPeakThisExcursion = 0.0f;
			bInOvershootWindow = false;
			bTrackingStarted = false;

			UE_LOG(LogAFLGameCore, Log, TEXT("AFL_BOTAIM: ACQUIRE %s target=%s reactionDelay=%.3fs"),
				*GetName(), *GetNameSafe(FocusActor), LastReactionDelay);
		}

		if (Now >= ReactionEndsAtSeconds)
		{
			// TrueDesired is the un-wobbled solution: where a perfect aimbot would point. The MODEL does
			// not use it -- the spring still chases the wobbled point exactly as before -- but every
			// overshoot measurement is taken against it, because that is the only frame in which
			// "the aim passed the target" means anything.
			const FRotator TrueDesired = (FocalPoint - MyPawn->GetPawnViewLocation()).Rotation();

			// Perfect aim, then displaced by the drifting error. The spring chases the WOBBLED point.
			FRotator Desired = TrueDesired;
			Desired.Yaw   += Wobble(Now, WobblePhaseA, WobblePhaseB);
			Desired.Pitch += Wobble(Now, WobblePhaseB, WobblePhaseA) * 0.5f;   // less vertical wander

			const float ErrYaw   = FRotator::NormalizeAxis(Desired.Yaw   - NewControlRotation.Yaw);
			const float ErrPitch = FRotator::NormalizeAxis(Desired.Pitch - NewControlRotation.Pitch);

			// Spring-damper. c = 2*zeta*sqrt(k); zeta < 1 is what produces the overshoot-and-correct that
			// makes tracking read as a person rather than a servo.
			const float K = Profile.TrackStiffness;
			const float C = 2.0f * Profile.TrackDampingRatio * FMath::Sqrt(K);
			AimVelYawDegPerSec   += (K * ErrYaw   - C * AimVelYawDegPerSec)   * DeltaTime;
			AimVelPitchDegPerSec += (K * ErrPitch - C * AimVelPitchDegPerSec) * DeltaTime;

			// MOTOR CAP -- the hard ceiling on how fast aim may move, whatever the spring wants.
			const float Speed = FMath::Sqrt(AimVelYawDegPerSec * AimVelYawDegPerSec
			                              + AimVelPitchDegPerSec * AimVelPitchDegPerSec);
			if (Speed > Profile.MaxTrackRateDegPerSec && Speed > KINDA_SMALL_NUMBER)
			{
				const float Scale = Profile.MaxTrackRateDegPerSec / Speed;
				AimVelYawDegPerSec   *= Scale;
				AimVelPitchDegPerSec *= Scale;
			}
			const float AchievedRate = FMath::Min(Speed, Profile.MaxTrackRateDegPerSec);
			PeakTrackRateThisAcquire = FMath::Max(PeakTrackRateThisAcquire, AchievedRate);
			LifetimePeakRate         = FMath::Max(LifetimePeakRate, AchievedRate);

			NewControlRotation.Yaw   += AimVelYawDegPerSec   * DeltaTime;
			NewControlRotation.Pitch += AimVelPitchDegPerSec * DeltaTime;

			// -- OVERSHOOT, measured against the TRUE target --
			// A sign change on the TRUE error means the aim genuinely passed the target and is coming
			// back. The old test used the WOBBLED error, which reverses whenever the wobble does -- about
			// twice a second -- so it counted crossings on a perfectly still aim and a damping regression
			// would have sailed through it. This version can actually fail.
			const float TrueErrYaw = FRotator::NormalizeAxis(TrueDesired.Yaw - NewControlRotation.Yaw);
			const int32 TrueSign = (TrueErrYaw > 0.0f) ? 1 : ((TrueErrYaw < 0.0f) ? -1 : 0);

			if (bInOvershootWindow)
			{
				// How far PAST the target this excursion reached. Measured across the window, not at the
				// crossing -- the error at a zero-crossing is ~0 by definition, which is why the previous
				// figure was an arithmetic tautology rather than a measurement.
				OvershootPeakThisExcursion = FMath::Max(OvershootPeakThisExcursion, FMath::Abs(TrueErrYaw));
			}

			if (LastTrueYawErrorSign != 0 && TrueSign != 0 && TrueSign != LastTrueYawErrorSign)
			{
				++SignCrossings;
				LifetimeMaxCrossings = FMath::Max(LifetimeMaxCrossings, SignCrossings);
				LifetimeMaxOvershootDeg = FMath::Max(LifetimeMaxOvershootDeg, OvershootPeakThisExcursion);
				OvershootPeakThisExcursion = 0.0f;
				bInOvershootWindow = true;

				if (SignCrossings == 1)
				{
					UE_LOG(LogAFLGameCore, Log,
						TEXT("AFL_BOTAIM: SETTLE  %s timeToTrack=%.3fs peakRate=%.0fd/s trueErr=%.2fdeg crossings=%d"),
						*GetName(), static_cast<float>(Now - AcquireTimeSeconds),
						PeakTrackRateThisAcquire, FMath::Abs(TrueErrYaw), SignCrossings);
				}
			}
			if (TrueSign != 0)
			{
				LastTrueYawErrorSign = TrueSign;
			}

			bTrackingStarted = true;
			if (AchievedRate > KINDA_SMALL_NUMBER)
			{
				bHasTrackedEver = true;   // vacuous-sample guard: a bot that never moved proves nothing
			}
		}
		// else: still inside the reaction window -- control rotation is deliberately left where it was.
	}
	else if (bSetControlRotationFromPawnOrientation)
	{
		NewControlRotation = MyPawn->GetActorRotation();
	}

	// STOCK BEHAVIOUR 2 of 3 -- don't pitch the view unless looking at another pawn.
	if (NewControlRotation.Pitch != 0 && Cast<APawn>(GetFocusActor()) == nullptr)
	{
		NewControlRotation.Pitch = 0.f;
	}

	SetControlRotation(NewControlRotation);

	// STOCK BEHAVIOUR 3 of 3 -- turn the BODY. Distinct from aim, and the thing that makes the aim error
	// visible to an observer, which is where "believable" actually lives.
	if (bUpdatePawn)
	{
		const FRotator CurrentPawnRotation = MyPawn->GetActorRotation();
		if (CurrentPawnRotation.Equals(NewControlRotation, 1e-3f) == false)
		{
			MyPawn->FaceRotation(NewControlRotation, DeltaTime);
		}
	}
}
