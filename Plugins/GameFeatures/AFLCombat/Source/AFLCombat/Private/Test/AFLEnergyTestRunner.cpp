// Copyright C12 AI Gaming. All Rights Reserved.

#include "Test/AFLEnergyTestRunner.h"

#include "AFLCombat.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"   // CONVERGENCE: the dummy's Health lives on the Lyra set now
#include "Attributes/AFLAttributeSet_Energy.h"
#include "Energy/AFLEnergyPickup.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Targeting/AFLLagTestDummy.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CheatManagerDefines.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLEnergyTestRunner)

TWeakObjectPtr<UAFLEnergyTestRunner> UAFLEnergyTestRunner::ActiveRun;

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Energy_ThresholdReached_EnergyRun, "Event.Energy.ThresholdReached");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Energy_Overdrive_EnergyRun, "State.Energy.Overdrive");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Input_Weapon_Fire_EnergyRun, "InputTag.Weapon.Fire");


void UAFLEnergyTestRunner::RunInWorld(UWorld* World)
{
	// HOST window only: spawns + the damage cheat are authority ops (the mirror of the lag-comp
	// runner's client-side guard -- each runner names its window and refuses the other).
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogAFLCombat, Error,
			TEXT("AFL_ENERGYRUN: afl.Energy.Test.Run belongs in the HOST window (spawns/damage are authority ops)."));
		return;
	}
	if (ActiveRun.IsValid())
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_ENERGYRUN: a run is already live -- wait for COMPLETE."));
		return;
	}
	UAFLEnergyTestRunner* Runner = NewObject<UAFLEnergyTestRunner>(GetTransientPackage());
	if (!Runner->StartRun(World))
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_ENERGYRUN ABORT -- preconditions unmet (see lines above)."));
		return;
	}
	Runner->AddToRoot();
	ActiveRun = Runner;
}

bool UAFLEnergyTestRunner::StartRun(UWorld* World)
{
	WorldPtr = World;
	PC = World->GetFirstPlayerController();
	Pawn = PC.IsValid() ? PC->GetPawn() : nullptr;
	APlayerState* PS = PC.IsValid() ? PC->PlayerState : nullptr;
	ASC = PS ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS) : nullptr;
	if (!Pawn.IsValid() || !ASC.IsValid())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_ENERGYRUN: no possessed pawn / PlayerState ASC."));
		return false;
	}
	if (!ASC->GetSet<UAFLAttributeSet_Energy>())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_ENERGYRUN: UAFLAttributeSet_Energy not granted (check the AbilitySet GrantedAttributes row)."));
		return false;
	}

	// Threshold + collection proofs = REGISTERED listeners (not log greps). The collect listener is
	// the v2 conservation instrument: counts + sums every collection REGARDLESS of which player
	// vacuumed it (run-1 lesson: the client stole 2/5 and every per-player expectation broke while
	// total conservation held exactly).
	bThresholdSeen = false;
	ThresholdListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(
		TAG_Event_Energy_ThresholdReached_EnergyRun,
		[this](FGameplayTag /*Channel*/, const FLyraVerbMessage& /*Msg*/) { bThresholdSeen = true; });
	CollectedCount = 0;
	CollectedSum = 0.0f;
	CollectListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FAFLEnergyCollectedMessage>(
		FGameplayTag::RequestGameplayTag(TEXT("Event.Energy.Collected"), false),
		[this](FGameplayTag /*Channel*/, const FAFLEnergyCollectedMessage& Msg)
		{
			++CollectedCount;
			CollectedSum += Msg.EnergyValue;
		});

	Energy0 = ReadCarriedEnergy();
	BaselineWalkSpeed = ReadPawnMaxWalkSpeed();

	// Run the drain FAST so the full-consumption exit is assertable in seconds -- but not so fast the
	// overdriven shot loses its window: the SetByCaller magnitude is CAPTURED AT APPLY, so the rate
	// cannot be re-paced mid-buff. 15/s: crossing at ~85-100 energy -> tag holds ~6s (the shot fires
	// ~2.5s in), exit well inside the 8s wait. Restored at FinishRun; cvars are process-global.
	if (IConsoleVariable* DrainVar = IConsoleManager::Get().FindConsoleVariable(TEXT("afl.Energy.DrainPerSecond")))
	{
		DrainCvarRestore = DrainVar->GetFloat();
		DrainVar->Set(15.0f, ECVF_SetByConsole);
	}

	Step = EStep::Burst1;
	StepTimer = 0.0f;
	bRunning = true;
	Marker(FString::Printf(TEXT("RUN -- burst/pull/collect/threshold/death-burst. E0=%.1f. Operator: just watch (~20s). Keep the CLIENT pawn away from the corpse during the death leg."), Energy0));
	return true;
}

void UAFLEnergyTestRunner::Tick(float DeltaTime)
{
	if (!bRunning)
	{
		return;
	}
	if (!WorldPtr.IsValid() || !PC.IsValid() || !ASC.IsValid())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_ENERGYRUN ABORT -- world/PC/ASC went away."));
		FinishRun();
		return;
	}
	StepTimer += DeltaTime;

	switch (Step)
	{
	case EStep::Burst1:
	{
		Console(TEXT("afl.Energy.SpawnBurst S 5"));
		Marker(TEXT("burst 1: 5 small pickups, 700uu ahead (outside magnet range -- count-stable)"));
		Step = EStep::AssertCountStable; StepTimer = 0.0f;
		break;
	}
	case EStep::AssertCountStable:
	{
		if (StepTimer >= 0.7f)
		{
			const int32 Count = CountPickups(MeanDistAtBurst);
			Check(Count == 5, FString::Printf(TEXT("burst spawned 5 count-stable pickups (counted %d at mean %.0fuu)"), Count, MeanDistAtBurst));
			// Remember where the ring is so the teleport lands beside it.
			RingCenter = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 700.0f;
			Step = EStep::TeleportIn; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::TeleportIn:
	{
		// Authority op (the interaction-harness PlaceInFront precedent): walk the pawn into magnet
		// range so the pull phase is OBSERVABLE rather than instant.
		Pawn->SetActorLocation(RingCenter + FVector(-350.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		Marker(TEXT("teleported into magnet range -- watch the orbs stream in"));
		Step = EStep::AssertPull; StepTimer = 0.0f;
		break;
	}
	case EStep::AssertPull:
	{
		if (StepTimer >= 1.0f)
		{
			float MeanNow = 0.0f;
			const int32 Count = CountPickups(MeanNow);
			// Either signal proves the pull: distances shrinking, or pickups already collected.
			Check(Count < 5 || MeanNow < MeanDistAtBurst,
				FString::Printf(TEXT("magnetic pull converging (mean %.0f -> %.0f, remaining %d, collected %d)"),
					MeanDistAtBurst, MeanNow, Count, CollectedCount));
			Step = EStep::AssertCollected_Burst2; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::AssertCollected_Burst2:
	{
		if (StepTimer >= 3.0f)
		{
			float MeanUnused = 0.0f;
			const int32 Count = CountPickups(MeanUnused);
			// CONSERVATION asserts (collector-agnostic): every spawned orb collected, energy sum exact.
			Check(Count == 0 && CollectedCount == 5,
				FString::Printf(TEXT("all 5 collected via listener (remaining %d, collected %d)"), Count, CollectedCount));
			Check(FMath::Abs(CollectedSum - 50.0f) <= 0.5f,
				FString::Printf(TEXT("collected energy conserves (sum %.1f, expect 50.0)"), CollectedSum));
			Marker(FString::Printf(TEXT("host CarriedEnergy now %.1f (INFO -- split depends on who collected)"), ReadCarriedEnergy()));
			Console(TEXT("afl.Energy.SpawnBurst M 4"));
			// Run-2 lesson: ring 2 ALSO spawns out of magnet range -- teleport in immediately (the
			// console spawn is synchronous, so the ring exists before this line).
			RingCenter = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 700.0f;
			Pawn->SetActorLocation(RingCenter + FVector(-350.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
			Marker(TEXT("burst 2: 4 mediums + teleport-in (100 energy in play -- a threshold crossing is guaranteed for SOMEONE)"));
			Step = EStep::AssertCollected2; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::AssertCollected2:
	{
		if (StepTimer >= 4.0f)
		{
			Check(CollectedCount == 9 && FMath::Abs(CollectedSum - 150.0f) <= 0.5f,
				FString::Printf(TEXT("burst 2 fully collected (count %d expect 9, sum %.1f expect 150)"), CollectedCount, CollectedSum));
			Step = EStep::OD_AssertEntered; StepTimer = 0.0f;
		}
		break;
	}
	// ---- Energy cycle 2: the overdrive leg ---------------------------------------------------------
	case EStep::OD_AssertEntered:
	{
		// Crossing 80 during burst-2 collects should have entered overdrive (listener-applied buff).
		const float SpeedNow = ReadPawnMaxWalkSpeed();
		Check(HasOverdriveTag(), TEXT("State.Energy.Overdrive UP after crossing 80 (listener-applied buff, no GA)"));
		Check(SpeedNow > BaselineWalkSpeed * 1.10f,
			FString::Printf(TEXT("CMC speed swapped (baseline %.0f -> %.0f, expect x1.15)"), BaselineWalkSpeed, SpeedNow));
		Step = EStep::OD_OverdrivenShot; StepTimer = 0.0f;
		break;
	}
	case EStep::OD_OverdrivenShot:
	{
		// Damage pair, shot A (overdriven): pin the aim at the lag dummy (process-global cvar) and
		// fire the REAL pulse through the input seam -- the ExecCalc multiplies x1.25 off the
		// captured SOURCE tags.
		DummyHealthBefore = ReadDummyHealth();
		// ReadDummyHealth() returns -1 when no dummy exists, and (-1) - (-1) = 0 reads exactly like a
		// blocked shot (run-4 ambiguity). Pin the precondition explicitly so an absent dummy fails LOUD.
		Check(DummyHealthBefore > 0.0f,
			FString::Printf(TEXT("lag dummy present for the damage pair (health %.1f)"), DummyHealthBefore));
		if (IConsoleVariable* PinVar = IConsoleManager::Get().FindConsoleVariable(TEXT("afl.LagComp.AimAtDummy")))
		{
			PinVar->Set(1, ECVF_SetByConsole);
		}
		PairRetries = 0;
		PressFire();
		Marker(TEXT("overdriven shot fired at the dummy (pinned aim)"));
		Step = EStep::OD_ReadHit1; StepTimer = 0.0f;
		break;
	}
	case EStep::OD_ReadHit1:
	{
		if (StepTimer >= 1.0f)
		{
			OverdrivenHitDelta = DummyHealthBefore - ReadDummyHealth();
			// Transient-blocker retry (header comment at PairRetries). The overdriven leg additionally
			// requires the tag still UP at re-fire -- a drained-out "hit" would not be an overdriven shot.
			if (OverdrivenHitDelta <= 0.0f && PairRetries < 3 && HasOverdriveTag())
			{
				++PairRetries;
				Marker(FString::Printf(TEXT("overdriven shot blocked/missed -- retry %d/3 (pin AFL_LAGCOMP_PIN names the blocker)"), PairRetries));
				DummyHealthBefore = ReadDummyHealth();
				PressFire();
				StepTimer = 0.0f;
				break;
			}
			Check(OverdrivenHitDelta > 0.0f,
				FString::Printf(TEXT("overdriven hit landed (dummy -%.1f, retries %d)"), OverdrivenHitDelta, PairRetries));
			Marker(TEXT("waiting out the fast drain to the energy-0 exit..."));
			Step = EStep::OD_AssertDrainExit; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::OD_AssertDrainExit:
	{
		// ~100 energy at 15/s = ~6.7s to zero from the crossing; 8s from here covers it with margin.
		if (StepTimer >= 8.0f)
		{
			const float Energy = ReadCarriedEnergy();
			const float SpeedNow = ReadPawnMaxWalkSpeed();
			Check(Energy <= 0.5f, FString::Printf(TEXT("drain consumed the meter (energy %.1f)"), Energy));
			Check(!HasOverdriveTag(), TEXT("State.Energy.Overdrive DOWN at energy 0 (handle-removed exit)"));
			Check(FMath::Abs(SpeedNow - BaselineWalkSpeed) <= 1.0f,
				FString::Printf(TEXT("CMC speed restored (%.0f, baseline %.0f)"), SpeedNow, BaselineWalkSpeed));
			Step = EStep::OD_ControlShot; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::OD_ControlShot:
	{
		// Damage pair, shot B (NOT overdriven): same pinned aim, same pipeline.
		DummyHealthBefore = ReadDummyHealth();
		PairRetries = 0;
		PressFire();
		Marker(TEXT("control shot fired (overdrive off)"));
		Step = EStep::OD_AssertRatio_Retrigger; StepTimer = 0.0f;
		break;
	}
	case EStep::OD_AssertRatio_Retrigger:
	{
		if (StepTimer >= 1.0f)
		{
			const float ControlDelta = DummyHealthBefore - ReadDummyHealth();
			// Same transient-blocker retry as the overdriven leg (run 4: this exact shot hit a body that
			// had wandered into the swept ray). Pin stays armed until the pair is settled.
			if (ControlDelta <= 0.0f && PairRetries < 3)
			{
				++PairRetries;
				Marker(FString::Printf(TEXT("control shot blocked/missed -- retry %d/3 (pin AFL_LAGCOMP_PIN names the blocker)"), PairRetries));
				DummyHealthBefore = ReadDummyHealth();
				PressFire();
				StepTimer = 0.0f;
				break;
			}
			if (IConsoleVariable* PinVar = IConsoleManager::Get().FindConsoleVariable(TEXT("afl.LagComp.AimAtDummy")))
			{
				PinVar->Set(0, ECVF_SetByConsole);
			}
			const float Ratio = (ControlDelta > 0.0f) ? OverdrivenHitDelta / ControlDelta : -1.0f;
			Check(ControlDelta > 0.0f && FMath::Abs(Ratio - 1.25f) <= 0.08f,
				FString::Printf(TEXT("damage pair ratio %.3f (overdriven %.1f / control %.1f, retries %d, expect 1.25)"),
					Ratio, OverdrivenHitDelta, ControlDelta, PairRetries));
			// Re-trigger: fresh ring + teleport-in -- crossing 80 again must re-enter (hysteresis works).
			Console(TEXT("afl.Energy.SpawnBurst M 4"));
			RingCenter = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 700.0f;
			Pawn->SetActorLocation(RingCenter + FVector(-350.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
			Marker(TEXT("re-trigger ring: collecting past 80 again"));
			Step = EStep::OD_AssertRetriggered; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::OD_AssertRetriggered:
	{
		if (StepTimer >= 4.0f)
		{
			Check(HasOverdriveTag(), TEXT("overdrive RE-ENTERED on the second upward crossing (hysteresis intact)"));
			Step = EStep::DeathLeg; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::DeathLeg:
	{
		CollectedSumAtDeath = CollectedSum; // baseline for the re-collected-drop evidence below
		EnergyPreDeath = ReadCarriedEnergy(); // read in the SAME frame as the kill -- the drain is live
		Console(TEXT("AFL.Combat.Damage 200"));
		Marker(TEXT("death leg: lethal self-damage -- watch the burst scatter at the corpse on BOTH screens"));
		Step = EStep::AssertDeathBurst; StepTimer = 0.0f;
		break;
	}
	case EStep::AssertDeathBurst:
	{
		if (StepTimer >= 2.0f)
		{
			const float Energy = ReadCarriedEnergy();
			float MeanUnused = 0.0f;
			const int32 CountNow = CountPickups(MeanUnused);
			const float OnGroundSum = SumOnGroundEnergy();
			const float DeathDropSum = CollectedSum - CollectedSumAtDeath; // re-collected drop portion
			// The drop is proven by EITHER pickups still on the ground OR their collections having
			// landed in the conservation sum (run-1: the other player vacuumed the burst in <300ms).
			Check(CountNow > 0 || DeathDropSum > 0.0f,
				FString::Printf(TEXT("death burst observable (on-ground %d, re-collected sum %.1f)"), CountNow, DeathDropSum));
			// Run-4 lesson: the victim is reduced by what was actually SPAWNED, and ceil-S tiering
			// legitimately overshoots the 70% floor (55 -> 1M+2S = 45 dropped, not 38.5). Asserting
			// "remain == pre x 30%" was the HARNESS lying about the component's contract. The contract
			// is CONSERVATION: reduction == dropped (on-ground + already-re-collected), dropped >= floor.
			const float Dropped = OnGroundSum + DeathDropSum;
			Check(EnergyPreDeath < 1.0f || FMath::Abs((EnergyPreDeath - Energy) - Dropped) <= 0.5f,
				FString::Printf(TEXT("victim reduction conserves into the drop (%.1f -> %.1f, dropped %.1f = ground %.1f + re-collected %.1f)"),
					EnergyPreDeath, Energy, Dropped, OnGroundSum, DeathDropSum));
			Check(EnergyPreDeath < 1.0f || Dropped >= EnergyPreDeath * 0.7f - 0.25f,
				FString::Printf(TEXT("dropped %.1f meets the 70%% floor of %.1f (ceil-S may overshoot)"),
					Dropped, EnergyPreDeath * 0.7f));
			// Cycle 2: death must also strip the buff (explicit handle removal -- Lyra leaves
			// Infinite GEs on the PlayerState ASC across respawn).
			Check(!HasOverdriveTag(), TEXT("overdrive removed at death (no buff rides into the next life)"));
			Step = EStep::Finish; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::Finish:
	{
		// Threshold assert lives at the END deliberately: 150 energy entered play across two players,
		// so SOMEONE crossed 80 -- any upward crossing anywhere proves the broadcast mechanism.
		Check(bThresholdSeen, TEXT("Event.Energy.ThresholdReached received via registered listener (any player, any time this run)"));
		FinishRun();
		break;
	}
	}
}

void UAFLEnergyTestRunner::FinishRun()
{
	bRunning = false;
	if (ThresholdListener.IsValid())
	{
		ThresholdListener.Unregister();
	}
	if (CollectListener.IsValid())
	{
		CollectListener.Unregister();
	}
	// Restore the drain rate + disarm the pin (the runner armed both).
	if (IConsoleVariable* DrainVar = IConsoleManager::Get().FindConsoleVariable(TEXT("afl.Energy.DrainPerSecond")))
	{
		DrainVar->Set(DrainCvarRestore, ECVF_SetByConsole);
	}
	if (IConsoleVariable* PinVar = IConsoleManager::Get().FindConsoleVariable(TEXT("afl.LagComp.AimAtDummy")))
	{
		PinVar->Set(0, ECVF_SetByConsole);
	}
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_ENERGYRUN[%s] SUMMARY total: %d checks, %d failed."), *NetTag(), ChecksTotal, ChecksFailed);
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_ENERGYRUN[%s] COMPLETE"), *NetTag());
	Marker(FString::Printf(TEXT("COMPLETE -- %d checks, %d failed. Stop PIE for log verification."), ChecksTotal, ChecksFailed));
	if (ActiveRun.Get() == this)
	{
		ActiveRun.Reset();
	}
	RemoveFromRoot();
}

int32 UAFLEnergyTestRunner::CountPickups(float& OutMeanDistToPawn) const
{
	OutMeanDistToPawn = 0.0f;
	int32 Count = 0;
	float DistSum = 0.0f;
	const FVector PawnLoc = Pawn.IsValid() ? Pawn->GetActorLocation() : FVector::ZeroVector;
	for (TActorIterator<AAFLEnergyPickup> It(WorldPtr.Get()); It; ++It)
	{
		++Count;
		DistSum += static_cast<float>(FVector::Dist(It->GetActorLocation(), PawnLoc));
	}
	OutMeanDistToPawn = (Count > 0) ? DistSum / Count : 0.0f;
	return Count;
}

float UAFLEnergyTestRunner::SumOnGroundEnergy() const
{
	float Sum = 0.0f;
	for (TActorIterator<AAFLEnergyPickup> It(WorldPtr.Get()); It; ++It)
	{
		Sum += It->GetEnergyValue();
	}
	return Sum;
}

float UAFLEnergyTestRunner::ReadCarriedEnergy() const
{
	// Same-module direct read -- alias-proof by construction (the stringly reader is the
	// cross-module fallback only; the Lyra-HealthSet alias lesson does not apply here).
	return ASC.IsValid() ? ASC->GetNumericAttribute(UAFLAttributeSet_Energy::GetCarriedEnergyAttribute()) : -1.0f;
}

float UAFLEnergyTestRunner::ReadDummyHealth() const
{
	for (TActorIterator<AAFLLagTestDummy> It(WorldPtr.Get()); It; ++It)
	{
		if (UAbilitySystemComponent* DummyASC = It->GetAbilitySystemComponent())
		{
			// CONVERGENCE: the dummy drains ULyraHealthSet Health now (ExecCalc output), not the AFL set.
			return DummyASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute());
		}
	}
	return -1.0f;
}

float UAFLEnergyTestRunner::ReadPawnMaxWalkSpeed() const
{
	const ACharacter* Character = Cast<ACharacter>(Pawn.Get());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	return Movement ? Movement->MaxWalkSpeed : -1.0f;
}

bool UAFLEnergyTestRunner::HasOverdriveTag() const
{
	return ASC.IsValid() && ASC->HasMatchingGameplayTag(TAG_State_Energy_Overdrive_EnergyRun);
}

void UAFLEnergyTestRunner::PressFire()
{
	// The real input seam (the lag-comp runner primitive): press+release back-to-back.
	if (ULyraAbilitySystemComponent* LyraASC = Cast<ULyraAbilitySystemComponent>(ASC.Get()))
	{
		LyraASC->AbilityInputTagPressed(TAG_Input_Weapon_Fire_EnergyRun);
		LyraASC->AbilityInputTagReleased(TAG_Input_Weapon_Fire_EnergyRun);
	}
}

void UAFLEnergyTestRunner::Console(const FString& Cmd)
{
	if (PC.IsValid())
	{
		PC->ConsoleCommand(Cmd, true);
	}
}

FString UAFLEnergyTestRunner::NetTag() const
{
	const UWorld* W = WorldPtr.Get();
	int32 PieId = -1;
#if WITH_EDITOR
	if (W && W->GetOutermost())
	{
		PieId = W->GetOutermost()->GetPIEInstanceID();
	}
#endif
	return (PieId >= 0) ? FString::Printf(TEXT("SV%d"), PieId) : FString(TEXT("SV"));
}

void UAFLEnergyTestRunner::Marker(const FString& Msg) const
{
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_ENERGYRUN[%s]: %s"), *NetTag(), *Msg);
#if !(UE_BUILD_SHIPPING)
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Yellow, FString::Printf(TEXT("AFL_ENERGYRUN  %s"), *Msg));
	}
#endif
}

void UAFLEnergyTestRunner::Check(bool bPass, const FString& What)
{
	++ChecksTotal;
	if (!bPass)
	{
		++ChecksFailed;
	}
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_ENERGYRUN[%s] %s -- %s"), *NetTag(), bPass ? TEXT("PASS") : TEXT("FAIL"), *What);
#if !(UE_BUILD_SHIPPING)
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, bPass ? FColor::Green : FColor::Red,
			FString::Printf(TEXT("AFL_ENERGYRUN %s  %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), *What));
	}
#endif
}

// ---------------------------------------------------------------------------------------------------
// afl.Energy.SpawnBurst <S|M|L> <count> -- deterministic ring spawn ahead of the host pawn.
// Server-side (spawning is an authority op); the runner drives it via PC->ConsoleCommand.
// ---------------------------------------------------------------------------------------------------

#if UE_WITH_CHEAT_MANAGER
namespace
{
	void HandleAFLEnergySpawnBurst(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
		{
			Ar.Log(TEXT("afl.Energy.SpawnBurst -- authority worlds only (HOST window)."));
			return;
		}
		const FString Tier = Args.Num() > 0 ? Args[0].ToUpper() : TEXT("S");
		const int32 Count = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 5;
		const float Value = (Tier == TEXT("L")) ? 50.0f : (Tier == TEXT("M")) ? 25.0f : 10.0f;

		APlayerController* PC = World->GetFirstPlayerController();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			Ar.Log(TEXT("afl.Energy.SpawnBurst -- no host pawn."));
			return;
		}
		// 700uu ahead = OUTSIDE the 500uu magnet radius, so the spawned ring is count-stable until the
		// runner teleports the pawn in (run-1 lesson: at 300uu the burst vacuumed in ~350ms and every
		// count assert lost the race).
		const FVector Center = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 700.0f;
		int32 Spawned = 0;
		for (int32 i = 0; i < Count; ++i)
		{
			const float Angle = (2.0f * PI) * (static_cast<float>(i) / FMath::Max(1, Count));
			const FVector Loc = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * 120.0f + FVector(0, 0, 50.0f);
			FTransform SpawnXForm(FRotator::ZeroRotator, Loc);
			if (AAFLEnergyPickup* Pickup = World->SpawnActorDeferred<AAFLEnergyPickup>(
					AAFLEnergyPickup::StaticClass(), SpawnXForm, nullptr, nullptr,
					ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
			{
				Pickup->InitEnergyValue(Value);
				Pickup->FinishSpawning(SpawnXForm);
				++Spawned;
			}
		}
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_ENERGY: SpawnBurst %s x%d (value %.0f each) -> %d spawned at ring."),
			*Tier, Count, Value, Spawned);
		Ar.Logf(TEXT("afl.Energy.SpawnBurst -- %d %s pickups spawned."), Spawned, *Tier);
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLEnergySpawnBurstCmd(
		TEXT("afl.Energy.SpawnBurst"),
		TEXT("Spawn a deterministic ring of energy pickups ahead of the host pawn. Usage: afl.Energy.SpawnBurst <S|M|L> <count>. Authority worlds only."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLEnergySpawnBurst));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLEnergyTestRunCmd(
		TEXT("afl.Energy.Test.Run"),
		TEXT("Energy cycle-1 proof, HOST window, observe-only: burst -> magnetic pull -> collection -> attribute delta -> overdrive threshold -> death burst + victim reduction. ~20s; stop PIE after COMPLETE."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				Ar.Log(TEXT("afl.Energy.Test.Run -- starting (see AFL_ENERGYRUN lines in LogAFLCombat)."));
				UAFLEnergyTestRunner::RunInWorld(World);
			}));
}
#endif // UE_WITH_CHEAT_MANAGER
