// Copyright C12 AI Gaming. All Rights Reserved.

#include "Test/AFLLootCarryTestRunner.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbilityTypes.h"   // FGameplayEventData
#include "Engine/World.h"
// UE_WITH_CHEAT_MANAGER lives here -- without it the #if at the bottom reads an UNDEFINED macro as 0 and
// the afl.LootCarry.Test.Run registration compiles out SILENTLY (the extract-runner run-1 lesson).
#include "GameFramework/CheatManagerDefines.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Loot/AFLLootCacheCarry.h"
#include "Loot/AFLLootCarryComponent.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLLootCarryTestRunner)

TWeakObjectPtr<UAFLLootCarryTestRunner> UAFLLootCarryTestRunner::ActiveRun;

namespace
{
	constexpr float TestChannelSeconds = 2.0f;   // afl.Channel.DurationOverride pinned for the run
	constexpr int32 CarryCacheWatts = 200;        // AAFLLootCacheCarry::LootWatts default

	FGameplayTag CollectChannelTag()  { return FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Loot.CollectChannel")), false); }
	FGameplayTag ChannelCompleteTag() { return FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Channel.Complete")), false); }
	FGameplayTag ChannelInterruptTag(){ return FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Channel.Interrupted")), false); }
}

void UAFLLootCarryTestRunner::RunInWorld(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogAFLCombat, Error,
			TEXT("AFL_LOOTCARRYRUN: afl.LootCarry.Test.Run belongs in the HOST window (spawns/cheats are authority ops)."));
		return;
	}
	if (ActiveRun.IsValid())
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_LOOTCARRYRUN: a run is already live -- wait for COMPLETE."));
		return;
	}
	UAFLLootCarryTestRunner* Runner = NewObject<UAFLLootCarryTestRunner>(GetTransientPackage());
	if (!Runner->StartRun(World))
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_LOOTCARRYRUN ABORT -- preconditions unmet (see lines above)."));
		return;
	}
	Runner->AddToRoot();
	ActiveRun = Runner;
}

bool UAFLLootCarryTestRunner::StartRun(UWorld* World)
{
	WorldPtr = World;
	PC = World->GetFirstPlayerController();
	Pawn = PC.IsValid() ? PC->GetPawn() : nullptr;
	APlayerState* PS = PC.IsValid() ? PC->PlayerState : nullptr;
	ASC = PS ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS) : nullptr;
	Carry = Pawn.IsValid() ? Pawn->FindComponentByClass<UAFLLootCarryComponent>() : nullptr;
	if (!Pawn.IsValid() || !ASC.IsValid())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_LOOTCARRYRUN: no possessed pawn / PlayerState ASC."));
		return false;
	}
	if (!Carry.IsValid())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_LOOTCARRYRUN: no UAFLLootCarryComponent on the pawn (experience grant missing)."));
		return false;
	}
	StartLocation = Pawn->GetActorLocation();

	// Pin the channel to a known 2s so the mid-channel interrupts (fired at t=1s) land deterministically.
	if (IConsoleVariable* DurVar = IConsoleManager::Get().FindConsoleVariable(TEXT("afl.Channel.DurationOverride")))
	{
		DurationCvarRestore = DurVar->GetFloat();
		DurVar->Set(TestChannelSeconds, ECVF_SetByConsole);
	}

	// Registered Event.Channel.* listeners (the conservation shape -- never log greps).
	CompleteCount = 0;
	InterruptedCount = 0;
	CompleteListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(
		ChannelCompleteTag(), [this](FGameplayTag, const FLyraVerbMessage&) { ++CompleteCount; });
	InterruptedListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(
		ChannelInterruptTag(), [this](FGameplayTag, const FLyraVerbMessage&) { ++InterruptedCount; });

	Step = EStep::L1_Start;
	StepTimer = 0.0f;
	bRunning = true;
	Marker(TEXT("RUN -- 3 legs: collect+despawn / move-cancel / damage-cancel. Channel pinned 2s. WATCH ONLY -- do NOT move the pawn (~12s)."));
	return true;
}

void UAFLLootCarryTestRunner::Tick(float DeltaTime)
{
	if (!bRunning)
	{
		return;
	}
	StepTimer += DeltaTime;

	switch (Step)
	{
	case EStep::L1_Start:
	{
		PoolAtLegStart = ReadPool();
		Cache = SpawnCache();
		Check(Cache.IsValid(), TEXT("LEG 1: CARRY cache spawned"));
		SendCollectChannel(Cache.Get());
		Marker(TEXT("LEG 1 (collect): channel sent -- expect complete + pool +200 + cache despawn"));
		Step = EStep::L1_Wait; StepTimer = 0.0f;
		break;
	}
	case EStep::L1_Wait:
		if (StepTimer >= TestChannelSeconds + 0.6f) { Step = EStep::L1_Assert; StepTimer = 0.0f; }
		break;
	case EStep::L1_Assert:
	{
		Check(CompleteCount == 1, FString::Printf(TEXT("LEG 1: Event.Channel.Complete received (count %d)"), CompleteCount));
		Check(ReadPool() - PoolAtLegStart == CarryCacheWatts,
			FString::Printf(TEXT("LEG 1: pool +%d (delta %d)"), CarryCacheWatts, ReadPool() - PoolAtLegStart));
		Check(!Cache.IsValid(), TEXT("LEG 1: cache DESPAWNED on collect (Decision B collect->despawn)"));
		Marker(TEXT("LEG 2 (move-cancel): seeding"));
		Step = EStep::L2_Start; StepTimer = 0.0f;
		break;
	}
	case EStep::L2_Start:
	{
		PoolAtLegStart = ReadPool();
		Cache = SpawnCache();
		SendCollectChannel(Cache.Get());   // channel starts at the pawn's StartLocation
		Marker(TEXT("LEG 2: channel sent -- will teleport >MaxMoveRadius mid-channel"));
		Step = EStep::L2_Mid; StepTimer = 0.0f;
		break;
	}
	case EStep::L2_Mid:
		if (StepTimer >= 1.0f)   // mid-channel (channel is 2s)
		{
			// Teleport 300cm (> the 120cm MaxMoveRadius) from the channel-start, onto proven floor.
			Pawn->SetActorLocation(StartLocation + FVector(300.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
			Marker(TEXT("LEG 2: teleported 300cm (> 120 radius) -- expect move-cancel"));
			Step = EStep::L2_Assert; StepTimer = 0.0f;
		}
		break;
	case EStep::L2_Assert:
		if (StepTimer >= 0.6f)
		{
			Check(InterruptedCount == 1, FString::Printf(TEXT("LEG 2: Event.Channel.Interrupted received (count %d)"), InterruptedCount));
			Check(ReadPool() - PoolAtLegStart == 0, FString::Printf(TEXT("LEG 2: pool UNCHANGED (delta %d, no grant)"), ReadPool() - PoolAtLegStart));
			Check(Cache.IsValid(), TEXT("LEG 2: cache STILL alive (move-cancel -> no grant, no despawn)"));
			if (Cache.IsValid()) { Cache->Destroy(); }   // clean up the un-collected cache
			Marker(TEXT("LEG 3 (damage-cancel): seeding"));
			Step = EStep::L3_Start; StepTimer = 0.0f;
		}
		break;
	case EStep::L3_Start:
	{
		PoolAtLegStart = ReadPool();
		// Return to the proven start spot (within radius) so the channel does not move-cancel itself.
		Pawn->SetActorLocation(StartLocation, false, nullptr, ETeleportType::TeleportPhysics);
		Cache = SpawnCache();
		SendCollectChannel(Cache.Get());
		Marker(TEXT("LEG 3: channel sent -- will self-damage mid-channel"));
		Step = EStep::L3_Mid; StepTimer = 0.0f;
		break;
	}
	case EStep::L3_Mid:
		if (StepTimer >= 1.0f)
		{
			Console(TEXT("AFL.Combat.Damage 10"));   // the extract-runner self-damage -> Event.Damage.Confirmed
			Marker(TEXT("LEG 3: self-damage at t=1 -- expect damage-cancel"));
			Step = EStep::L3_Assert; StepTimer = 0.0f;
		}
		break;
	case EStep::L3_Assert:
		if (StepTimer >= 0.6f)
		{
			Check(InterruptedCount == 2, FString::Printf(TEXT("LEG 3: Event.Channel.Interrupted received (count %d, expect 2)"), InterruptedCount));
			// The self-damage that cancels the channel is the SAME Event.Damage.Confirmed that drop-scatters
			// the carried pool (correct -- damage while carrying scatters ~33%, net of any re-collect). So the
			// proof of NO CHANNEL-grant is delta <= 0 (it never gains the cache's +200); a decrease is the
			// legitimate drop-on-damage, NOT a channel grant. (cache-alive below is the independent no-grant proof.)
			const int32 L3Delta = ReadPool() - PoolAtLegStart;
			Check(L3Delta <= 0,
				FString::Printf(TEXT("LEG 3: NO channel-grant (pool delta %d <= 0; the self-damage ALSO drop-scatters -- expected interaction)"), L3Delta));
			Check(Cache.IsValid(), TEXT("LEG 3: cache STILL alive (damage-cancel -> no grant, no despawn)"));
			if (Cache.IsValid()) { Cache->Destroy(); }
			Step = EStep::Finish; StepTimer = 0.0f;
		}
		break;
	case EStep::Finish:
		FinishRun();
		break;
	}
}

void UAFLLootCarryTestRunner::FinishRun()
{
	bRunning = false;
	if (CompleteListener.IsValid()) { CompleteListener.Unregister(); }
	if (InterruptedListener.IsValid()) { InterruptedListener.Unregister(); }
	if (Cache.IsValid()) { Cache->Destroy(); }
	if (IConsoleVariable* DurVar = IConsoleManager::Get().FindConsoleVariable(TEXT("afl.Channel.DurationOverride")))
	{
		DurVar->Set(DurationCvarRestore, ECVF_SetByConsole);
	}
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRYRUN[%s] SUMMARY: %d checks, %d failed."), *NetTag(), ChecksTotal, ChecksFailed);
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRYRUN[%s] COMPLETE"), *NetTag());
	Marker(FString::Printf(TEXT("COMPLETE -- %d checks, %d failed. Stop PIE for log verification."), ChecksTotal, ChecksFailed));
	if (ActiveRun.Get() == this) { ActiveRun.Reset(); }
	RemoveFromRoot();
}

AAFLLootCacheCarry* UAFLLootCarryTestRunner::SpawnCache()
{
	UWorld* World = WorldPtr.Get();
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// In front of the pawn, on proven floor. The channel is event-triggered (no proximity needed), so the
	// exact spot only needs to be a valid spawn -- the cache is just the grant SOURCE the channel targets.
	const FVector Loc = StartLocation + FVector(200.0f, 0.0f, 50.0f);
	return World->SpawnActor<AAFLLootCacheCarry>(AAFLLootCacheCarry::StaticClass(), Loc, FRotator::ZeroRotator, Params);
}

void UAFLLootCarryTestRunner::SendCollectChannel(AActor* CacheActor)
{
	if (!ASC.IsValid() || !Pawn.IsValid() || !CacheActor)
	{
		return;
	}
	// The SAME event the grab fork sends (Decision D): Target = the cache; the channel grants from it.
	FGameplayEventData Payload;
	Payload.EventTag = CollectChannelTag();
	Payload.Instigator = Pawn.Get();
	Payload.Target = CacheActor;
	ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
}

int32 UAFLLootCarryTestRunner::ReadPool() const
{
	return Carry.IsValid() ? Carry->GetCarriedValue() : -1;
}

void UAFLLootCarryTestRunner::Console(const FString& Cmd)
{
	if (PC.IsValid())
	{
		PC->ConsoleCommand(Cmd, true);
	}
}

FString UAFLLootCarryTestRunner::NetTag() const
{
	const UWorld* W = WorldPtr.Get();
	int32 PieId = -1;
#if WITH_EDITOR
	if (W && W->GetOutermost())
	{
		PieId = W->GetOutermost()->GetPIEInstanceID();
	}
#endif
	const TCHAR* Prefix = (W && W->GetNetMode() == NM_Client) ? TEXT("CL") : TEXT("SV");
	return (PieId >= 0) ? FString::Printf(TEXT("%s%d"), Prefix, PieId) : FString(Prefix);
}

void UAFLLootCarryTestRunner::Marker(const FString& Msg) const
{
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRYRUN[%s]: %s"), *NetTag(), *Msg);
}

void UAFLLootCarryTestRunner::Check(bool bPass, const FString& What)
{
	++ChecksTotal;
	if (!bPass)
	{
		++ChecksFailed;
	}
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRYRUN[%s] %s -- %s"), *NetTag(), bPass ? TEXT("PASS") : TEXT("FAIL"), *What);
}

#if UE_WITH_CHEAT_MANAGER
namespace
{
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLLootCarryTestRunCmd(
		TEXT("afl.LootCarry.Test.Run"),
		TEXT("Loot-Carry Phase B: 3-leg scripted proof of the CARRY collect-channel (collect+despawn / move-cancel / damage-cancel) -- HOST window, observe-only, ~12s."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				Ar.Log(TEXT("afl.LootCarry.Test.Run -- starting (see AFL_LOOTCARRYRUN lines in LogAFLCombat)."));
				UAFLLootCarryTestRunner::RunInWorld(World);
			}));
}
#endif // UE_WITH_CHEAT_MANAGER
