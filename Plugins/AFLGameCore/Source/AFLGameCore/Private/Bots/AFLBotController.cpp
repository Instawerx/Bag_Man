// Copyright C12 AI Gaming. All Rights Reserved.

#include "Bots/AFLBotController.h"

#include "AFLGameCore.h"
#include "Bots/AFLBotProbe.h"   // auto-verdict at teardown
#include "AFLMatchTierSource.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AITypes.h"                 // FAISystem::IsValidLocation -- the stock focal-point validity check
#include "BehaviorTree/BlackboardComponent.h"   // AI-2: per-bot movement params -> query params
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"   // RVO avoidance opt-in, per instance
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Math/RandomStream.h"
#include "NativeGameplayTags.h"
#include "Navigation/PathFollowingComponent.h"   // wedge discriminator: did it try to move, and get anywhere

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLBotController)

// The C++ <-> blackboard contract for AI-2. EQS_AFL_CombatReposition binds its query params to these
// exact names; a mismatch is silent (the query falls back to authored defaults) so it is logged below
// and surfaced by afl.Bot.MoveProbe rather than left to be noticed as "bots feel samey".
const FName AAFLBotController::BBKey_DonutInner          = TEXT("BotDonutInner");
const FName AAFLBotController::BBKey_DonutOuter          = TEXT("BotDonutOuter");
const FName AAFLBotController::BBKey_RepositionInterval  = TEXT("BotRepositionInterval");
const FName AAFLBotController::BBKey_LateralBias         = TEXT("BotLateralBias");
const FName AAFLBotController::BBKey_MoveGoal            = TEXT("MoveGoal");   // observed, never written

// AI-3 probe input. Granted by GE_AFL_Sprint_Active and watched by UAFLSprintMovementComponent -- reading the
// tag keeps AFLGameCore free of any AFLMovement dependency.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Movement_Sprinting, "State.Movement.Sprinting");

namespace
{
	/** Below this speed a bot counts as stationary for the probe. Chosen well under a walk so a bot
	 *  turning on the spot or micro-adjusting is not scored as "moving". */
	constexpr float StationarySpeedThreshold = 40.0f;
	/** Cell edge for the distinct-positions metric, cm. */
	constexpr float PositionCellSizeCm = 200.0f;

	/** Donut floors applied when converting preferred-range/band into radii. Inner must stay clear of zero
	 *  or the generator degenerates to a disc; thickness must stay positive or the ring has no area. */
	constexpr float MinDonutInnerCm     = 100.0f;
	constexpr float MinDonutThicknessCm = 100.0f;

	/** Goal-watch poll period and its two distance epsilons. Measurement resolution, not behaviour: at 0.5s
	 *  a bot walking at ~400 uu/s covers ~200cm, so 60cm of travel is comfortably "it moved" while still
	 *  ignoring collision jitter, and a goal that shifted less than 100cm is the same goal re-issued. */
	constexpr float GoalWatchPeriodSec  = 0.5f;
	constexpr float NewGoalEpsilonCm    = 100.0f;
	constexpr float NoProgressEpsilonCm = 60.0f;

	/** Every avoidance group. The hero CDOs ship with GroupsToAvoid EMPTY so that a pawn registers with the
	 *  UAvoidanceManager -- and is therefore avoidable -- without ever being deflected itself. Bots opt IN
	 *  to dodging by taking this mask at possession; the player never does, so player input stays untouched. */
	constexpr int32 AvoidAllGroups = static_cast<int32>(0xFFFFFFFFu);
}

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

	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFL_BOTMOVE: ROLL    %s tier=%.2f range=%.0fcm band=%.0fcm interval=%.2fs lateral=%.2f"),
		*GetName(), CachedTier, MoveProfile.PreferredRangeCm, MoveProfile.RangeBandCm,
		MoveProfile.RepositionIntervalSec, MoveProfile.LateralBias);

	// AVOIDANCE OPT-IN, PER INSTANCE. This one line is the entire difference between a bot and the player:
	// both share B_Hero_BagMan_C and both register with the UAvoidanceManager (CDO has bUseRVOAvoidance on),
	// but the CDO's GroupsToAvoid is EMPTY, so by default a pawn is avoidable and never deflected. Bots take
	// the full mask here and start dodging; the player never runs this and keeps clean input.
	//
	// Logged with a READ-BACK rather than assumed: a silent miss leaves bots registered but avoiding nothing,
	// which is indistinguishable from a partial fix on screen -- piles would still form and the cause would
	// look like "RVO does not work" instead of "the mask never landed".
	if (UCharacterMovementComponent* CMC = InPawn ? InPawn->FindComponentByClass<UCharacterMovementComponent>() : nullptr)
	{
		CMC->SetGroupsToAvoidMask(AvoidAllGroups);

		const int32 ReadBack = CMC->GetGroupsToAvoidMask();
		const bool  bOk = CMC->bUseRVOAvoidance && (ReadBack == AvoidAllGroups);

		// UE_LOG pastes the verbosity as a token, so it cannot be a runtime ternary. Build the line once and
		// pick the call, rather than duplicating the format string across two branches.
		const FString Msg = FString::Printf(
			TEXT("AFL_BOTMOVE: AVOID   %s rvo=%s weight=%.2f radius=%.0f groupsToAvoid=0x%08X -- %s"),
			*GetName(),
			CMC->bUseRVOAvoidance ? TEXT("on") : TEXT("OFF"),
			CMC->AvoidanceWeight,
			CMC->AvoidanceConsiderationRadius,
			ReadBack,
			bOk ? TEXT("ok") : TEXT("MISSED -- this bot is registered but will avoid NOTHING"));

		if (bOk) { UE_LOG(LogAFLGameCore, Log,     TEXT("%s"), *Msg); }
		else     { UE_LOG(LogAFLGameCore, Warning, TEXT("%s"), *Msg); }
	}
	else
	{
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AFL_BOTMOVE: AVOID   %s -- no CharacterMovementComponent on the possessed pawn; no avoidance."),
			*GetName());
	}

	// The blackboard does not exist yet -- the BT starts after possession -- so this self-retries.
	MovePushAttempts = 0;
	bMoveParamsPushed = false;
	PushMoveParamsToBlackboard();

	// Round watch for AFL_MOVESNAP. A poll rather than a possession hook precisely BECAUSE possession is
	// what the tier already rides: hanging the snapshot off OnPossess would miss the final round, where the
	// match ends and nothing re-possesses. 1 Hz x 5 bots is free next to a round that lasts a minute.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RoundWatchTimer,
			FTimerDelegate::CreateWeakLambda(this, [this] { TickRoundWatch(); }),
			1.0f, /*loop=*/true);

		World->GetTimerManager().SetTimer(GoalWatchTimer,
			FTimerDelegate::CreateWeakLambda(this, [this] { TickGoalWatch(); }),
			GoalWatchPeriodSec, /*loop=*/true);
	}
	bHasPolledOnce = false;   // a new pawn is a new position history
}

void AAFLBotController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// THE LAST ROUND ONLY EXISTS HERE. The round watch emits a snapshot when it sees the number CHANGE, and
	// after the final round it never does -- the match ends and the world tears down. Without this the curve
	// would be missing its most interesting point, and missing it silently.
	if (SnapRound != INDEX_NONE)
	{
		EmitMoveSnapshot(SnapRound, TEXT("end-of-play"));
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MovePushRetryTimer);
		World->GetTimerManager().ClearTimer(RoundWatchTimer);
		World->GetTimerManager().ClearTimer(GoalWatchTimer);

		// AUTO-VERDICT. The probe used to be reachable only by typing it at the right moment, and timing cost
		// two reads running -- once never typed, once typed 3s in and correctly reporting INCONCLUSIVE against
		// 0 rounds of history. The data existed both times; the verdict did not.
		//
		// Runs ONCE per world: whichever bot tears down first does it for everyone. Ordering does not matter
		// because RunMove reads RoundHistory PLUS each bot's in-progress round, so a bot that has not emitted
		// its final snapshot yet is still counted.
		static TWeakObjectPtr<UWorld> LastAutoProbedWorld;
		if (World->GetNetMode() != NM_Client && LastAutoProbedWorld.Get() != World)
		{
			LastAutoProbedWorld = World;
			UE_LOG(LogAFLGameCore, Log, TEXT("AFL_MOVEPROBE: auto-run at end of play -- no console timing required."));
			AFLBotProbe::RunMove(World, *GLog);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AAFLBotController::PushMoveParamsToBlackboard()
{
	static constexpr int32 MaxAttempts = 40;      // x 0.25s = 10s
	static constexpr float RetryPeriod = 0.25f;
	++MovePushAttempts;

	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		// The query wants radii around the enemy, so convert here -- see BBKey_DonutInner. The inner floor
		// is not cosmetic: EnvQueryGenerator_Donut with InnerRadius <= 0 degenerates into a disc that
		// generates points ON the target, which is how this query got broken once already.
		const float Inner = FMath::Max(MinDonutInnerCm, MoveProfile.PreferredRangeCm - MoveProfile.RangeBandCm);
		const float Outer = FMath::Max(Inner + MinDonutThicknessCm, MoveProfile.PreferredRangeCm + MoveProfile.RangeBandCm);

		BB->SetValueAsFloat(BBKey_DonutInner,         Inner);
		BB->SetValueAsFloat(BBKey_DonutOuter,         Outer);
		BB->SetValueAsFloat(BBKey_RepositionInterval, MoveProfile.RepositionIntervalSec);
		BB->SetValueAsFloat(BBKey_LateralBias,        MoveProfile.LateralBias);
		PushedDonutInnerCm = Inner;
		PushedDonutOuterCm = Outer;
		bMoveParamsPushed = true;
		UE_LOG(LogAFLGameCore, Log,
			TEXT("AFL_BOTMOVE: PUSH    %s -> donut=[%.0f..%.0f]cm lateral=%.2f (from range=%.0f band=%.0f, attempt %d)."),
			*GetName(), Inner, Outer, MoveProfile.LateralBias,
			MoveProfile.PreferredRangeCm, MoveProfile.RangeBandCm, MovePushAttempts);
		return;
	}

	if (MovePushAttempts < MaxAttempts)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(MovePushRetryTimer,
				FTimerDelegate::CreateWeakLambda(this, [this] { PushMoveParamsToBlackboard(); }),
				RetryPeriod, /*loop=*/false);
		}
		return;
	}

	// Loud, because the failure is otherwise invisible: the query keeps working on its authored
	// defaults and every bot moves identically, which reads as "the personality axes did nothing".
	UE_LOG(LogAFLGameCore, Warning,
		TEXT("AFL_BOTMOVE: PUSH FAILED %s -- no BlackboardComponent after %d attempts. Bot will run the query's DEFAULTS, not its own profile."),
		*GetName(), MovePushAttempts);
}

void AAFLBotController::RollProfile()
{
	if (Roll.bRolled)
	{
		return;   // personality is for life. Re-rolling turns character into noise.
	}

	// SEED STABILITY is the whole point: the same bot must always roll the same personality, and two bots
	// must reliably differ.
	//
	// THE PLAYER NAME ALONE IS NOT UNIQUE. It comes from the RandomBotNames pool, and at 16 bots that pool
	// is exhausted -- a 3-round 8v8 log showed SIX distinct names across the roster, with "Tinplate" on 50
	// snapshot rows against 5 for the others, plus at least one bot whose name was empty. Duplicate name ->
	// identical CRC -> byte-identical personality, which is what failed ROLLED. It gets worse at BR 36.
	//
	// So the seed is NAME + the controller's own object name. GetName() is unique per actor instance and
	// stable for its whole life (possession changes do not rename it), which is exactly the guarantee this
	// needs -- cross-session stability was never required, since bot identities are built fresh each match.
	// The player name stays in the seed purely so a logged roll is still traceable to a bot you can see.
	FString SeedSource = GetName();
	if (const APlayerState* PS = GetPlayerState<APlayerState>())
	{
		const FString PlayerName = PS->GetPlayerName();
		if (!PlayerName.IsEmpty())
		{
			SeedSource = PlayerName + TEXT(":") + GetName();
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
	// AI-2 draws from the SAME stream, so a bot's footwork and its aim come from one identity rather
	// than two unrelated dice. A jumpy aimer is also a restless mover.
	Roll.PreferredRange = Signed();
	Roll.RangeBand      = Signed();
	Roll.RepositionRate = Signed();
	Roll.LateralBias    = Signed();
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

	Profile     = AimTiers->Resolve(CachedTier, Roll);
	MoveProfile = AimTiers->ResolveMove(CachedTier, Roll);

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

void AAFLBotController::SampleMovement(float DeltaTime, const APawn* MyPawn, const AActor* FocusActor)
{
	// COMBAT TIME ONLY. Idle wandering between engagements is not the behaviour under test, and letting
	// it into the denominator would let a bot that stands still in every firefight pass STATIONARY by
	// having strolled around a lot beforehand.
	if (!MyPawn || !FocusActor || DeltaTime <= 0.0f)
	{
		return;
	}

	// ROUND-LIVE GATE. A pawn frozen at a round edge still holds its focus target, so without this every
	// frozen second lands in StationarySeconds as though the bot chose to stand there -- pure inflation, and
	// the likeliest reason warmup read 66.9%. The gate REPORTS ON ITSELF: FrozenSeconds is emitted on the
	// snapshot, so if it stays 0.00 the freeze is not implemented via move-input suppression and this gate
	// caught nothing. That is a fact worth seeing rather than assuming away.
	if (MyPawn->IsMoveInputIgnored())
	{
		Lifetime.FrozenSeconds  += DeltaTime;
		ThisRound.FrozenSeconds += DeltaTime;
		return;
	}

	Lifetime.CombatSeconds  += DeltaTime;
	ThisRound.CombatSeconds += DeltaTime;

	// AI-3. Read the STATE TAG rather than UAFLSprintMovementComponent::IsSprinting(): AFLGameCore is always
	// loaded and must not take a dependency on the AFLMovement GameFeature. The tag is the same contract the
	// movement component itself listens to, so this observes exactly what drives the speed swap.
	if (const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<APawn*>(MyPawn)))
	{
		if (ASC->HasMatchingGameplayTag(TAG_State_Movement_Sprinting))
		{
			Lifetime.SprintSeconds  += DeltaTime;
			ThisRound.SprintSeconds += DeltaTime;
		}
	}

	const FVector Vel = MyPawn->GetVelocity();
	const FVector Flat(Vel.X, Vel.Y, 0.0f);
	const float Speed = Flat.Size();
	if (Speed < StationarySpeedThreshold)
	{
		Lifetime.StationarySeconds  += DeltaTime;
		ThisRound.StationarySeconds += DeltaTime;
		LastLateralSign = 0;   // a stop breaks the reversal chain; resuming is not a direction change
	}
	else
	{
		// Decompose velocity against where the bot is AIMING, not where it is heading -- lateral means
		// "sideways relative to the target", which is what strafe looks like to the person being shot at.
		const FVector Fwd = GetControlRotation().Vector().GetSafeNormal2D();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd);
		const float FwdSpeed = FMath::Abs(FVector::DotProduct(Flat, Fwd));
		const float LatSpeed = FVector::DotProduct(Flat, Right);

		Lifetime.ForwardSpeedSum  += FwdSpeed * DeltaTime;
		ThisRound.ForwardSpeedSum += FwdSpeed * DeltaTime;
		Lifetime.LateralSpeedSum  += FMath::Abs(LatSpeed) * DeltaTime;
		ThisRound.LateralSpeedSum += FMath::Abs(LatSpeed) * DeltaTime;

		const int32 Sign = (LatSpeed > StationarySpeedThreshold) ? 1 : ((LatSpeed < -StationarySpeedThreshold) ? -1 : 0);
		if (Sign != 0)
		{
			if (LastLateralSign != 0 && Sign != LastLateralSign)
			{
				++Lifetime.LateralReversals;
				++ThisRound.LateralReversals;
			}
			LastLateralSign = Sign;
		}
	}

	const FVector P = MyPawn->GetActorLocation();
	const FIntVector Cell(
		FMath::FloorToInt(P.X / PositionCellSizeCm),
		FMath::FloorToInt(P.Y / PositionCellSizeCm),
		0);
	Lifetime.Cells.Add(Cell);
	ThisRound.Cells.Add(Cell);

	const float R = FVector::Dist2D(P, FocusActor->GetActorLocation());
	Lifetime.RangeSumCm  += R;
	ThisRound.RangeSumCm += R;
	++Lifetime.RangeSamples;
	++ThisRound.RangeSamples;
}

void AAFLBotController::TickRoundWatch()
{
	// Resolve the tier source once and cache it. Same lookup RefreshTier does; done here so the 1 Hz poll
	// does not walk the component list of every actor forever.
	IAFLMatchTierSource* Src = Cast<IAFLMatchTierSource>(CachedTierSource.Get());
	if (!Src)
	{
		if (const UWorld* World = GetWorld())
		{
			if (const AGameStateBase* GS = World->GetGameState())
			{
				for (UActorComponent* Comp : GS->GetComponents())
				{
					if (IAFLMatchTierSource* Found = Cast<IAFLMatchTierSource>(Comp))
					{
						CachedTierSource = Comp;
						Src = Found;
						break;
					}
				}
			}
		}
	}
	if (!Src)
	{
		return;   // no round authority in this mode -- nothing to snapshot against
	}

	const int32 Round = Src->GetCurrentRoundNumber();
	if (SnapRound == INDEX_NONE)
	{
		SnapRound = Round;      // first read: adopt, do not emit an empty snapshot for a round we missed
		return;
	}
	if (Round != SnapRound)
	{
		EmitMoveSnapshot(SnapRound, TEXT("round-change"));
		SnapRound = Round;
	}
}

void AAFLBotController::TickGoalWatch()
{
	const APawn* MyPawn = GetPawn();
	const AActor* Focus = GetFocusActor();
	const UBlackboardComponent* BB = GetBlackboardComponent();

	// Same gates as the sampler, so the counters share a denominator with stationary and the two can be
	// read against each other on one line.
	if (!MyPawn || !Focus || !BB || MyPawn->IsMoveInputIgnored())
	{
		bHasPolledOnce = false;   // do not measure "progress" across a gap in observation
		return;
	}

	const FVector Pos  = MyPawn->GetActorLocation();
	const bool    bSet = BB->IsVectorValueSet(BBKey_MoveGoal);
	const FVector Goal = bSet ? BB->GetValueAsVector(BBKey_MoveGoal) : FVector::ZeroVector;

	++Lifetime.GoalPolls;
	++ThisRound.GoalPolls;

	if (bSet)
	{
		++Lifetime.GoalValid;
		++ThisRound.GoalValid;

		if (!bHasPolledOnce || FVector::DistSquared2D(Goal, LastPolledGoal) > FMath::Square(NewGoalEpsilonCm))
		{
			++Lifetime.GoalChanges;
			++ThisRound.GoalChanges;
		}

		// Did it TRY, and did trying achieve anything? Path status separates "never asked to move" from
		// "asked, and physically could not".
		const UPathFollowingComponent* PFC = GetPathFollowingComponent();
		const bool bPathActive = PFC && (PFC->GetStatus() == EPathFollowingStatus::Moving);
		if (!bPathActive)
		{
			++Lifetime.PollsPathIdle;
			++ThisRound.PollsPathIdle;
		}
		else if (bHasPolledOnce && FVector::DistSquared2D(Pos, LastPolledPos) < FMath::Square(NoProgressEpsilonCm))
		{
			++Lifetime.PollsNoProgress;
			++ThisRound.PollsNoProgress;
		}
	}

	LastPolledGoal = Goal;
	LastPolledPos  = Pos;
	bHasPolledOnce = true;
}

void AAFLBotController::EmitMoveSnapshot(int32 ForRound, const TCHAR* Trigger)
{
	const APlayerState* PS = GetPlayerState<APlayerState>();
	const FString Name = PS ? PS->GetPlayerName() : GetName();

	// NO DATA, NEVER A ZERO. A bot that never engaged this round has 0 combat seconds, and every ratio below
	// would divide to 0.0 -- which reads as "perfectly mobile" and would drag the curve down with a sample
	// that measures nothing. It is reported as absent, in the same shape the probes use.
	if (ThisRound.CombatSeconds <= KINDA_SMALL_NUMBER)
	{
		// frozen= is reported even here: a round that is ALL freeze and no combat looks identical to a round
		// the bot sat out, and those are very different facts.
		UE_LOG(LogAFLGameCore, Log,
			TEXT("AFL_MOVESNAP: round=%d tier=%.2f bot=%s NO DATA -- no combat time this round (frozen=%.1fs) (%s)"),
			ForRound, CachedTier, *Name, ThisRound.FrozenSeconds, Trigger);
		ThisRound.Reset();
		return;
	}

	// goalvalid% IS the EQS item count, reduced to the bit that decides: see TickGoalWatch.
	const int32 P = FMath::Max(1, ThisRound.GoalPolls);
	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFL_MOVESNAP: round=%d tier=%.2f bot=%s combat=%.1fs stationary=%.0f%% cells=%d lateral=%.2f ")
		TEXT("reversals=%.2f/s range=%.0fcm donut=[%.0f..%.0f] pref=%.0f band=%.0f latbias=%.2f ")
		TEXT("| polls=%d goalvalid=%.0f%% goalchanges=%d pathidle=%.0f%% noprogress=%.0f%% frozen=%.1fs sprint=%.0f%% (%s)"),
		ForRound, CachedTier, *Name,
		ThisRound.CombatSeconds,
		ThisRound.StationaryFraction() * 100.0f,
		ThisRound.Cells.Num(),
		ThisRound.LateralRatio(),
		ThisRound.ReversalsPerSecond(),
		ThisRound.MeanRangeCm(),
		PushedDonutInnerCm, PushedDonutOuterCm,
		MoveProfile.PreferredRangeCm, MoveProfile.RangeBandCm, MoveProfile.LateralBias,
		ThisRound.GoalPolls,
		100.0f * ThisRound.GoalValid       / P,
		ThisRound.GoalChanges,
		100.0f * ThisRound.PollsPathIdle   / P,
		100.0f * ThisRound.PollsNoProgress / P,
		ThisRound.FrozenSeconds,
		ThisRound.SprintFraction() * 100.0f,
		Trigger);

	// Freeze the round before clearing it. This is what the probe asserts on -- see FAFLBotRoundSummary.
	FAFLBotRoundSummary S;
	S.Round            = ForRound;
	S.Tier             = CachedTier;
	S.CombatSeconds    = ThisRound.CombatSeconds;
	S.Stationary       = ThisRound.StationaryFraction();
	S.Cells            = ThisRound.Cells.Num();
	S.Lateral          = ThisRound.LateralRatio();
	S.Reversals        = ThisRound.ReversalsPerSecond();
	S.Sprint           = ThisRound.SprintFraction();
	S.MeanRangeCm      = ThisRound.MeanRangeCm();
	S.PreferredRangeCm = MoveProfile.PreferredRangeCm;
	S.RangeBandCm      = MoveProfile.RangeBandCm;
	RoundHistory.Add(S);

	ThisRound.Reset();
}

FAFLBotRoundSummary AAFLBotController::GetCurrentRoundSummary() const
{
	FAFLBotRoundSummary S;
	S.Round            = SnapRound;
	S.Tier             = CachedTier;
	S.CombatSeconds    = ThisRound.CombatSeconds;
	S.Stationary       = ThisRound.StationaryFraction();
	S.Cells            = ThisRound.Cells.Num();
	S.Lateral          = ThisRound.LateralRatio();
	S.Reversals        = ThisRound.ReversalsPerSecond();
	S.Sprint           = ThisRound.SprintFraction();
	S.MeanRangeCm      = ThisRound.MeanRangeCm();
	S.PreferredRangeCm = MoveProfile.PreferredRangeCm;
	S.RangeBandCm      = MoveProfile.RangeBandCm;
	return S;
}

float AAFLBotController::GetStationaryFraction() const { return Lifetime.StationaryFraction(); }
float AAFLBotController::GetLateralRatio()       const { return Lifetime.LateralRatio(); }
float AAFLBotController::GetReversalsPerSecond() const { return Lifetime.ReversalsPerSecond(); }
float AAFLBotController::GetMeanRangeCm()        const { return Lifetime.MeanRangeCm(); }

void AAFLBotController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	APawn* const MyPawn = GetPawn();
	UWorld* const World = GetWorld();

	// AI-2 sampling rides the same per-frame call as the aim model. Deliberately BEFORE the inert-return
	// below, so movement is still measured on a build where the aim model is switched off.
	SampleMovement(DeltaTime, MyPawn, GetFocusActor());

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
