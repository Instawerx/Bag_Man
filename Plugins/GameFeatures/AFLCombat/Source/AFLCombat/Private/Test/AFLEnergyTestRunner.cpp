// Copyright C12 AI Gaming. All Rights Reserved.

#include "Test/AFLEnergyTestRunner.h"

#include "AFLCombat.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/AFLAttributeSet_Energy.h"
#include "Energy/AFLEnergyPickup.h"
#include "Engine/Engine.h"
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
			EnergyPreDeath = ReadCarriedEnergy();
			Step = EStep::DeathLeg; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::DeathLeg:
	{
		CollectedSumAtDeath = CollectedSum; // baseline for the re-collected-drop evidence below
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
			const float ExpectedRemain = EnergyPreDeath * 0.3f;
			float MeanUnused = 0.0f;
			const int32 CountNow = CountPickups(MeanUnused);
			const float DeathDropSum = CollectedSum - CollectedSumAtDeath; // re-collected drop portion
			// The drop is proven by EITHER pickups still on the ground OR their collections having
			// landed in the conservation sum (run-1: the other player vacuumed the burst in <300ms).
			Check(CountNow > 0 || DeathDropSum > 0.0f,
				FString::Printf(TEXT("death burst observable (on-ground %d, re-collected sum %.1f)"), CountNow, DeathDropSum));
			Check(EnergyPreDeath < 1.0f || FMath::Abs(Energy - ExpectedRemain) <= 6.0f,
				FString::Printf(TEXT("victim CarriedEnergy reduced through the rail (%.1f -> %.1f, expect ~%.1f at 70%%)"),
					EnergyPreDeath, Energy, ExpectedRemain));
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

float UAFLEnergyTestRunner::ReadCarriedEnergy() const
{
	// Same-module direct read -- alias-proof by construction (the stringly reader is the
	// cross-module fallback only; the Lyra-HealthSet alias lesson does not apply here).
	return ASC.IsValid() ? ASC->GetNumericAttribute(UAFLAttributeSet_Energy::GetCarriedEnergyAttribute()) : -1.0f;
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
