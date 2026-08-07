// Copyright C12 AI Gaming. All Rights Reserved.

#include "Test/AFLBoundaryHeightTestHarness.h"

#include "AFLGameCore.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/CheatManagerDefines.h"   // UE_WITH_CHEAT_MANAGER -- WITHOUT THIS the macro is
                                                 // undefined, #if evaluates as 0, and the console command
                                                 // is silently compiled out. Build stays green. (B231)
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"

namespace
{
	TWeakObjectPtr<UAFLBoundaryHeightTestHarness> GLiveRun;

	const TCHAR* kRigLayer = TEXT("/Game/Maps/DataLayers/L_ShantyTown/Scaffold_HeightRig.Scaffold_HeightRig");

	// Stage budgets. Apex of a single jump is vz/g = 900/980 = 0.92s, so Execute must outlast that.
	constexpr float kRigSettle    = 1.5f;
	constexpr float kPlaceSettle  = 0.5f;
	constexpr float kExecute      = 2.6f;
	constexpr float kObserve      = 1.6f;
	constexpr float kSecondJumpAt = 0.85f;   // just under apex -- maximises double-jump gain

	// Standoff from the wall face. Vertical verbs start adjacent; run-up verbs need runway.
	constexpr float kStandoffVertical = 300.0f;
	constexpr float kStandoffRunup    = 1400.0f;

	constexpr float kClearTolerance = 20.0f;  // uu of slack on "feet at or above the top"
	constexpr int32 kMaxConsecFails = 2;      // abilities are flaky; one miss must not end a verb
}

const TCHAR* UAFLBoundaryHeightTestHarness::VerbName(EVerb V)
{
	switch (V)
	{
	case EVerb::Jump:        return TEXT("Jump");
	case EVerb::DoubleJump:  return TEXT("DoubleJump");
	case EVerb::Vault:       return TEXT("Vault");
	case EVerb::Climb:       return TEXT("Climb");
	case EVerb::WallRunJump: return TEXT("WallRunJump");
	default:                 return TEXT("?");
	}
}

const TCHAR* UAFLBoundaryHeightTestHarness::ResultName(EResult R)
{
	switch (R)
	{
	case EResult::Cleared:   return TEXT("CLEARED");
	case EResult::Failed:    return TEXT("FAILED");
	default:                 return TEXT("AMBIGUOUS");
	}
}

void UAFLBoundaryHeightTestHarness::RunInWorld(UWorld* World)
{
	if (GLiveRun.IsValid())
	{
		UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_TEST ABORT -- a HeightTest run is already live. Stop PIE and retry."));
		return;
	}
	UAFLBoundaryHeightTestHarness* H = NewObject<UAFLBoundaryHeightTestHarness>();
	if (H->StartRun(World))
	{
		GLiveRun = H;
	}
}

bool UAFLBoundaryHeightTestHarness::StartRun(UWorld* World)
{
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_TEST ABORT -- no game world. Run from inside PIE."));
		return false;
	}
	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_TEST ABORT -- CLIENT window. Data layer activation is server-authoritative. Run in the HOST window."));
		return false;
	}

	ACharacter* C = UGameplayStatics::GetPlayerCharacter(World, 0);
	if (!C)
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_TEST ABORT -- no player character."));
		return false;
	}
	ULyraAbilitySystemComponent* A = Cast<ULyraAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(C));
	if (!A)
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_TEST ABORT -- pawn has no LyraAbilitySystemComponent."));
		return false;
	}

	WorldPtr = World;
	Pawn     = C;
	ASC      = A;
	if (const UCapsuleComponent* Cap = C->GetCapsuleComponent())
	{
		CapsuleHalf = Cap->GetScaledCapsuleHalfHeight();
	}
	bRunning     = true;
	Stage        = EStage::ActivateRig;
	StageElapsed = 0.0f;
	AddToRoot();

	UE_LOG(LogAFLGameCore, Display,
		TEXT("AFL_TEST BEGIN -- boundary height measurement. capsuleHalf=%.0f jumpZ=%.0f jumpMaxCount=%d"),
		CapsuleHalf,
		C->GetCharacterMovement() ? C->GetCharacterMovement()->JumpZVelocity : 0.0f,
		C->JumpMaxCount);
	return true;
}

void UAFLBoundaryHeightTestHarness::FinishRun()
{
	SetRigState(false);
	bRunning = false;
	Stage    = EStage::Done;
	UE_LOG(LogAFLGameCore, Display, TEXT("AFL_TEST COMPLETE"));
	GLiveRun = nullptr;
	RemoveFromRoot();
}

void UAFLBoundaryHeightTestHarness::SetRigState(bool bActivated)
{
	UWorld* W = WorldPtr.Get();
	UDataLayerManager* M = W ? UDataLayerManager::GetDataLayerManager(W) : nullptr;
	const UDataLayerAsset* Asset = LoadObject<UDataLayerAsset>(nullptr, kRigLayer);
	if (!M || !Asset)
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_TEST -- rig data layer unavailable ('%s')."), kRigLayer);
		return;
	}
	const bool bOk = M->SetDataLayerRuntimeState(Asset,
		bActivated ? EDataLayerRuntimeState::Activated : EDataLayerRuntimeState::Unloaded);
	UE_LOG(LogAFLGameCore, Display, TEXT("AFL_TEST -- rig layer -> %s (accepted=%s)"),
		bActivated ? TEXT("Activated") : TEXT("Unloaded"), bOk ? TEXT("true") : TEXT("false"));
}

bool UAFLBoundaryHeightTestHarness::CollectRig()
{
	Walls.Reset();
	UWorld* W = WorldPtr.Get();
	if (!W) { return false; }

	// Membership, NOT name. AActor::SetActorLabel writes an EDITOR-ONLY property; at runtime a World
	// Partition actor's GetName() is 'StaticMeshActor_UAID_<hash>' and a label match can never hit.
	// ContainsDataLayer is ENGINE_API and not editor-only -- the same seam StreamTest uses.
	const UDataLayerAsset* RigAsset = LoadObject<UDataLayerAsset>(nullptr, kRigLayer);
	if (!RigAsset)
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_TEST -- rig data layer asset missing ('%s')."), kRigLayer);
		return false;
	}

	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* A = *It;
		if (!A || !A->ContainsDataLayer(RigAsset)) { continue; }

		FVector Origin, Extent;
		A->GetActorBounds(false, Origin, Extent);
		FWall Wall;
		Wall.Actor  = A;
		Wall.TopZ   = Origin.Z + Extent.Z;
		Wall.BaseZ  = Origin.Z - Extent.Z;
		Wall.Metres = FMath::RoundToInt((Wall.TopZ - Wall.BaseZ) / 100.0f);
		Walls.Add(Wall);
	}
	Walls.Sort([](const FWall& A, const FWall& B) { return A.Metres < B.Metres; });

	UE_LOG(LogAFLGameCore, Display, TEXT("AFL_TEST -- rig collected: %d walls (%s)"),
		Walls.Num(),
		Walls.Num() ? *FString::Printf(TEXT("%dm..%dm"), Walls[0].Metres, Walls.Last().Metres) : TEXT("none"));
	return Walls.Num() > 0;
}

void UAFLBoundaryHeightTestHarness::PlaceForTrial()
{
	ACharacter* C = Pawn.Get();
	if (!C || !Walls.IsValidIndex(WallIndex)) { return; }
	AActor* WallActor = Walls[WallIndex].Actor.Get();
	if (!WallActor) { return; }

	const EVerb V = (EVerb)VerbIndex;
	const float Standoff = (V == EVerb::Jump || V == EVerb::DoubleJump) ? kStandoffVertical : kStandoffRunup;

	const FVector WallLoc = WallActor->GetActorLocation();
	const FVector Start(WallLoc.X, WallLoc.Y - Standoff, Walls[WallIndex].BaseZ + CapsuleHalf + 10.0f);

	// Clear any input still held from the previous trial (sprint is held for a whole trial) so state
	// cannot bleed across trials and make one verb's result depend on the last.
	static const FGameplayTag TagSprintR = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Movement.Sprint"));
	ReleaseAllPending();
	Release(TagSprintR);

	C->TeleportTo(Start, FRotator(0.0f, 90.0f, 0.0f), false, true);   // face +Y, toward the wall
	if (UCharacterMovementComponent* CMC = C->GetCharacterMovement())
	{
		CMC->StopMovementImmediately();
	}
	PeakFeetZ       = -BIG_NUMBER;
	PeakTags.Reset();
	StartY          = Start.Y;
	ClosestGapY     = BIG_NUMBER;
	bFiredThisTrial = false;
}

void UAFLBoundaryHeightTestHarness::Press(const FGameplayTag& Tag)
{
	if (ULyraAbilitySystemComponent* A = ASC.Get()) { A->AbilityInputTagPressed(Tag); }
}
void UAFLBoundaryHeightTestHarness::Release(const FGameplayTag& Tag)
{
	if (ULyraAbilitySystemComponent* A = ASC.Get()) { A->AbilityInputTagReleased(Tag); }
}

void UAFLBoundaryHeightTestHarness::PressHeld(const FGameplayTag& Tag, float HoldSeconds)
{
	Press(Tag);
	Pending.Add({ Tag, StageElapsed + HoldSeconds });
}

void UAFLBoundaryHeightTestHarness::ReleaseAllPending()
{
	for (const FPendingRelease& P : Pending) { Release(P.Tag); }
	Pending.Reset();
}

void UAFLBoundaryHeightTestHarness::DriveVerb(float DeltaTime)
{
	ACharacter* C = Pawn.Get();
	if (!C) { return; }

	static const FGameplayTag TagJump   = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Jump"));
	static const FGameplayTag TagVault  = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Movement.Vault"));
	static const FGameplayTag TagClimb  = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Ability.Climb"));
	static const FGameplayTag TagSprint = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Movement.Sprint"));

	const EVerb V = (EVerb)VerbIndex;
	const FVector Toward(0.0f, 1.0f, 0.0f);   // +Y, into the wall
	const float T = StageElapsed;

	// Movement input is always driven -- air control (0.4) is what carries the pawn ONTO the top.
	C->AddMovementInput(Toward, 1.0f);

	// Deferred releases. Every ability input is HELD across at least one movement tick; see PressHeld.
	for (int32 i = Pending.Num() - 1; i >= 0; --i)
	{
		if (T >= Pending[i].At) { Release(Pending[i].Tag); Pending.RemoveAt(i); }
	}

	const bool bFirstFrame = (T < DeltaTime * 1.5f);
	if (V == EVerb::Vault || V == EVerb::Climb || V == EVerb::WallRunJump)
	{
		if (bFirstFrame) { Press(TagSprint); }   // held for the whole trial, released in PlaceForTrial
	}

	const float WallY   = Walls[WallIndex].Actor.IsValid() ? Walls[WallIndex].Actor->GetActorLocation().Y : 0.0f;
	const bool  bNearWall = (WallY - C->GetActorLocation().Y) < 250.0f;

	switch (V)
	{
	case EVerb::Jump:
		if (bFirstFrame) { PressHeld(TagJump, 0.20f); }
		break;

	case EVerb::DoubleJump:
		if (bFirstFrame)                                              { PressHeld(TagJump, 0.20f); }
		else if (T >= kSecondJumpAt && T < kSecondJumpAt + DeltaTime)  { PressHeld(TagJump, 0.20f); }
		break;

	case EVerb::Vault:
		// distance is the real trigger condition, not a timer
		if (bNearWall && T > 0.2f && !bFiredThisTrial) { PressHeld(TagVault, 0.20f); bFiredThisTrial = true; }
		break;

	case EVerb::Climb:
		if (bNearWall && T > 0.2f && !bFiredThisTrial) { PressHeld(TagClimb, 0.20f); bFiredThisTrial = true; }
		break;

	case EVerb::WallRunJump:
		// GA_AFL_WallRun_C has NO input tag -- it is induced by carrying speed into the wall. We only
		// send Jump once wall-run state has actually appeared, so a plain double-jump cannot masquerade
		// as a wall-jump result.
		if (PeakTags.Contains(TEXT("WallRun")) && T > 0.3f && !bFiredThisTrial)
		{
			PressHeld(TagJump, 0.20f); bFiredThisTrial = true;
		}
		break;

	default:
		break;
	}
}

UAFLBoundaryHeightTestHarness::EResult
UAFLBoundaryHeightTestHarness::ScoreTrial(float& OutPeakFeetZ, FString& OutTagsAtPeak) const
{
	OutPeakFeetZ  = PeakFeetZ;
	OutTagsAtPeak = PeakTags;

	const ACharacter* C = Pawn.Get();
	if (!C || !Walls.IsValidIndex(WallIndex)) { return EResult::Ambiguous; }

	const float TopZ     = Walls[WallIndex].TopZ;
	const float FinalFeet = C->GetActorLocation().Z - CapsuleHalf;
	const UCharacterMovementComponent* CMC = C->GetCharacterMovement();
	const bool bGrounded = CMC && CMC->IsMovingOnGround();

	// CLEARED means it ENDED on top -- grounded at or above the wall top. Peak alone is not a clear:
	// an ability that lofts the pawn and drops it back is a FAIL, by design.
	if (bGrounded && FinalFeet >= TopZ - kClearTolerance) { return EResult::Cleared; }
	if (bGrounded && FinalFeet <= Walls[WallIndex].BaseZ + kClearTolerance) { return EResult::Failed; }
	return EResult::Ambiguous;   // snagged, mid-air at timeout, or standing on something unexpected
}

void UAFLBoundaryHeightTestHarness::AdvanceTrial()
{
	if (ConsecFails >= kMaxConsecFails || WallIndex + 1 >= Walls.Num())
	{
		++VerbIndex;
		WallIndex   = 0;
		ConsecFails = 0;
		if (VerbIndex >= (int32)EVerb::MAX) { Stage = EStage::Summarise; return; }
	}
	else
	{
		++WallIndex;
	}
	Stage = EStage::Place;
}

void UAFLBoundaryHeightTestHarness::Tick(float DeltaTime)
{
	if (!bRunning) { return; }
	if (!WorldPtr.IsValid() || !Pawn.IsValid())
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_TEST ABORT -- world or pawn went away mid-run."));
		FinishRun();
		return;
	}
	StageElapsed += DeltaTime;

	// Continuous peak sampling -- must run in every stage where the pawn is moving.
	if (Stage == EStage::Execute || Stage == EStage::Observe)
	{
		// How close did we actually GET to the wall? Without this, "never jumped" and "never reached
		// the wall" are indistinguishable -- both show only a low peak Z.
		if (Walls.IsValidIndex(WallIndex) && Walls[WallIndex].Actor.IsValid())
		{
			const float Gap = Walls[WallIndex].Actor->GetActorLocation().Y - Pawn->GetActorLocation().Y;
			ClosestGapY = FMath::Min(ClosestGapY, Gap);
		}

		const float Feet = Pawn->GetActorLocation().Z - CapsuleHalf;
		if (Feet > PeakFeetZ)
		{
			PeakFeetZ = Feet;
			if (ULyraAbilitySystemComponent* A = ASC.Get())
			{
				FGameplayTagContainer Owned;
				A->GetOwnedGameplayTags(Owned);
				PeakTags = Owned.ToStringSimple();
			}
		}
	}

	switch (Stage)
	{
	case EStage::ActivateRig:
		if (StageElapsed < DeltaTime * 1.5f) { SetRigState(true); }
		else if (StageElapsed >= kRigSettle)
		{
			if (!CollectRig())
			{
				UE_LOG(LogAFLGameCore, Error, TEXT("AFL_TEST ABORT -- no AFL_HR_Wall actors streamed in. Rig layer empty or not activated."));
				FinishRun();
				return;
			}
			Stage = EStage::Place; StageElapsed = 0.0f;
		}
		break;

	case EStage::Place:
		if (StageElapsed < DeltaTime * 1.5f) { PlaceForTrial(); }
		else if (StageElapsed >= kPlaceSettle) { Stage = EStage::Execute; StageElapsed = 0.0f; }
		break;

	case EStage::Execute:
		DriveVerb(DeltaTime);
		if (StageElapsed >= kExecute) { Stage = EStage::Observe; StageElapsed = 0.0f; }
		break;

	case EStage::Observe:
		if (StageElapsed >= kObserve) { Stage = EStage::Score; StageElapsed = 0.0f; }
		break;

	case EStage::Score:
	{
		float Peak = 0.0f; FString Tags;
		const EResult R = ScoreTrial(Peak, Tags);
		const FWall& W  = Walls[WallIndex];
		const float PeakAboveBase = Peak - W.BaseZ;

		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST[H] verb=%-12s wall=%2dm topZ=%.0f baseZ=%.0f | peakFeetZ=%.0f (%.2f m above base) | closestGap=%.0f uu | fired=%s | %s | tagsAtPeak=%s"),
			VerbName((EVerb)VerbIndex), W.Metres, W.TopZ, W.BaseZ, Peak, PeakAboveBase / 100.0f,
			(ClosestGapY < BIG_NUMBER * 0.5f) ? ClosestGapY : -1.0f,
			bFiredThisTrial ? TEXT("yes") : TEXT("n/a"),
			ResultName(R), Tags.IsEmpty() ? TEXT("<none>") : *Tags);

		if (R == EResult::Cleared)
		{
			BestMetres[VerbIndex] = FMath::Max(BestMetres[VerbIndex], W.Metres);
			ConsecFails = 0;
		}
		else
		{
			++ConsecFails;
			if (R == EResult::Ambiguous) { ++AmbiguousCount; }
		}
		AdvanceTrial();
		StageElapsed = 0.0f;
		break;
	}

	case EStage::Summarise:
	{
		int32 OverallMax = 0;
		UE_LOG(LogAFLGameCore, Display, TEXT("AFL_TEST HEIGHT SUMMARY --------------------------------"));
		for (int32 i = 0; i < (int32)EVerb::MAX; ++i)
		{
			UE_LOG(LogAFLGameCore, Display, TEXT("AFL_TEST HEIGHT   %-12s max cleared = %d m"),
				VerbName((EVerb)i), BestMetres[i]);
			OverallMax = FMath::Max(OverallMax, BestMetres[i]);
		}
		// Margin: one full rig step above the measured max, because the rig resolution IS the
		// uncertainty -- the true ceiling lies between OverallMax and OverallMax+1.
		const int32 Recommended = OverallMax + 2;
		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST VERDICT overallMax=%d m recommendedBoundary=%d m (max + 1m rig resolution + 1m margin) ambiguous=%d%s"),
			OverallMax, Recommended, AmbiguousCount,
			(OverallMax >= Walls.Last().Metres)
				? TEXT(" | WARNING: cleared the TALLEST wall -- the rig did not bracket the answer, extend it")
				: TEXT(""));
		FinishRun();
		break;
	}

	default:
		FinishRun();
		break;
	}
}

#if UE_WITH_CHEAT_MANAGER
namespace
{
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLBoundaryHeightTestCmd(
		TEXT("afl.DL.HeightTest"),
		TEXT("Boundary height measurement: scripts Jump/DoubleJump/Vault/Climb/WallRunJump against the ")
		TEXT("AFL_HR_Wall 3-14 m rig and logs the max height each verb clears. Activates the rig data layer ")
		TEXT("itself. Run in the HOST window; stop PIE after AFL_TEST COMPLETE."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				Ar.Log(TEXT("afl.DL.HeightTest -- starting (see AFL_TEST lines in LogAFLGameCore)."));
				UAFLBoundaryHeightTestHarness::RunInWorld(World);
			}));
}
#endif // UE_WITH_CHEAT_MANAGER
