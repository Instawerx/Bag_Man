// Copyright C12 AI Gaming. All Rights Reserved.

#include "Test/AFLChaosTestRunner.h"

#include "AFLCombat.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/AFLAG_Extract.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Attributes/AFLAttributeSet_Energy.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"   // CONVERGENCE: Health/MaxHealth live on the Lyra set now
#include "Cosmetics/AFLWalletComponent.h"
#include "Energy/AFLEnergyPickup.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Extraction/AFLExtractionZone.h"
#include "GameFramework/CheatManagerDefines.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "NativeGameplayTags.h"
#include "Phases/AFLMatchPhaseComponent.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLChaosTestRunner)

TWeakObjectPtr<UAFLChaosTestRunner> UAFLChaosTestRunner::ActiveRun;

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Overloaded_ChaosRun, "State.Overloaded");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Carrying_Vulnerable_ChaosRun, "State.Carrying.Vulnerable");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Carrying_StressObject_ChaosRun, "State.Carrying.StressObject");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_InExtractionZone_ChaosRun, "State.InExtractionZone");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Status_Death_ChaosRun, "Status.Death");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Input_Grab_ChaosRun, "InputTag.Ability.Grab");

namespace
{
	constexpr float WattsPerEnergyExpected_ChaosRun = 10.0f;
	constexpr float ChaosExtractMultExpected = 1.5f;
}

void UAFLChaosTestRunner::RunInWorld(UWorld* World, ERunMode Mode)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_CHAOSRUN/OVERLOADRUN: belongs in the HOST window (cheats are authority ops)."));
		return;
	}
	if (ActiveRun.IsValid())
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_CHAOSRUN/OVERLOADRUN: a run is already live -- wait for COMPLETE."));
		return;
	}
	UAFLChaosTestRunner* Runner = NewObject<UAFLChaosTestRunner>(GetTransientPackage());
	if (!Runner->StartRun(World, Mode))
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_CHAOSRUN/OVERLOADRUN ABORT -- preconditions unmet (see lines above)."));
		return;
	}
	Runner->AddToRoot();
	ActiveRun = Runner;
}

bool UAFLChaosTestRunner::StartRun(UWorld* World, ERunMode InMode)
{
	Mode = InMode;
	WorldPtr = World;
	PC = World->GetFirstPlayerController();
	Pawn = PC.IsValid() ? PC->GetPawn() : nullptr;
	APlayerState* PS = PC.IsValid() ? PC->PlayerState : nullptr;
	ASC = PS ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS) : nullptr;
	Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
	AGameStateBase* GS = World->GetGameState();
	Driver = GS ? GS->FindComponentByClass<UAFLMatchPhaseComponent>() : nullptr;

	if (!Pawn.IsValid() || !ASC.IsValid())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_CHAOSRUN: no possessed pawn / PlayerState ASC."));
		return false;
	}
	if (!ASC->GetSet<UAFLAttributeSet_Energy>() || !ASC->GetSet<UAFLAttributeSet_Combat>())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_CHAOSRUN: energy/combat set not granted."));
		return false;
	}

	// Pin overdrive drain off (seeding crosses the threshold). Restored at finish.
	if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("afl.Energy.DrainPerSecond")))
	{
		DrainRestore = V->GetFloat(); V->Set(0.0f, ECVF_SetByConsole);
	}

	// Overload burst conservation: count re-collected energy (the magnet refunds the burst to the
	// living owner before any ground-count can see it).
	ReCollectedSum = 0.0f;
	CollectListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FAFLEnergyCollectedMessage>(
		FGameplayTag::RequestGameplayTag(TEXT("Event.Energy.Collected"), false),
		[this](FGameplayTag, const FAFLEnergyCollectedMessage& Msg) { ReCollectedSum += Msg.EnergyValue; });

	if (Mode == ERunMode::Chaos)
	{
		if (!Wallet.IsValid() || !Driver.IsValid())
		{
			UE_LOG(LogAFLCombat, Error, TEXT("AFL_CHAOSRUN: no wallet / match-phase driver (experience rows; relaunch if just added)."));
			return false;
		}
		StressObject = FindStressObject();
		if (!StressObject.IsValid())
		{
			UE_LOG(LogAFLCombat, Error, TEXT("AFL_CHAOSRUN: no stress object placed in the map (a grabbable with State.Carrying.StressObject carrier). Place BP_AFL_StressObject."));
			return false;
		}
		StressStartLoc = StressObject->GetActorLocation();
		// Place the pawn next to the object AND FACE IT -- the grab trace + the reach montage need the
		// pawn looking at the target (run-1: the reach interrupted before contact). Then hold still
		// (no further teleports) so the reach-then-attach montage can complete uninterrupted.
		const FVector PawnLoc = StressStartLoc + FVector(150.0f, 0.0f, 0.0f);
		const FRotator FaceObject = (StressStartLoc - PawnLoc).Rotation();
		Pawn->SetActorLocationAndRotation(PawnLoc, FaceObject, false, nullptr, ETeleportType::TeleportPhysics);
		if (PC.IsValid()) { PC->SetControlRotation(FaceObject); }
		Step = EStep::CH_Grab;
	}
	else
	{
		Step = EStep::OV_Seed;
	}

	StepTimer = 0.0f;
	bRunning = true;
	bExtractStarted = false;
	Marker(Mode == ERunMode::Chaos
		? TEXT("RUN (chaos) -- grab the stress object, x1.3 dmg, x1.5 extract, instability. ~25s.")
		: TEXT("RUN (overload) -- survive-at-zero-while-carrying, then real death with no energy. ~20s."));
	return true;
}

void UAFLChaosTestRunner::Tick(float DeltaTime)
{
	if (!bRunning) { return; }
	StepTimer += DeltaTime;

	switch (Step)
	{
	// ───────── OVERLOAD ─────────
	case EStep::OV_Seed:
	{
		Console(TEXT("AFL.Combat.EnergyGain 50"));
		Marker(TEXT("OV leg: seeded 50 energy; killing while carrying"));
		Step = EStep::OV_Kill; StepTimer = 0.0f;
		break;
	}
	case EStep::OV_Kill:
	{
		if (StepTimer >= 0.6f)
		{
			EnergyAtKill = ReadCarriedEnergy();
			HealthFloorExpected = FMath::FloorToFloat(0.5f * ReadMaxHealth());
			OverloadSpawnLoc = Pawn.IsValid() ? Pawn->GetActorLocation() : FVector::ZeroVector;
			Console(TEXT("AFL.Combat.Damage 500")); // lethal -> should overload (survive)
			Step = EStep::OV_AssertSurvive; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::OV_AssertSurvive:
	{
		// Sample EARLY (0.25s) -- the living player's magnet vacuums the dropped burst within ~1s
		// (the documented re-collect wrinkle: run showed 50 dropped then re-collected -> net 50->50).
		// Assert the burst is OBSERVABLE on the ground here, then flee so it's not re-collected.
		if (StepTimer >= 0.25f)
		{
			Check(!IsDead(), TEXT("SURVIVED a lethal blow while carrying (no Status.Death -- overload, not death)"));
			// Burst proof = on-ground OR already re-collected (the living owner's magnet vacuums it in
			// <0.25s -- the documented re-collect wrinkle; run-2 saw 0 on-ground because all 35 was
			// refunded). Either way the burst happened.
			Check(CountPickups() > 0 || ReCollectedSum > 0.0f,
				FString::Printf(TEXT("energy burst on overload (on-ground %d, re-collected %.1f)"), CountPickups(), ReCollectedSum));
			Check(FMath::Abs(ReadHealth() - HealthFloorExpected) <= 2.0f,
				FString::Printf(TEXT("Health restored to floor (%.0f, expect ~%.0f)"), ReadHealth(), HealthFloorExpected));
			Check(HasTag(TAG_State_Overloaded_ChaosRun), TEXT("State.Overloaded up (stun + lockout active)"));
			// Flee the burst so leg-2's "drained to zero" is clean (else the magnet refunds it).
			if (Pawn.IsValid())
			{
				Pawn->SetActorLocation(OverloadSpawnLoc + FVector(2500.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
			}
			Marker(TEXT("OV leg: burst observed + fled; waiting for the overload stun to clear"));
			Step = EStep::OV_AwaitClear; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::OV_AwaitClear:
	{
		if (!HasTag(TAG_State_Overloaded_ChaosRun))
		{
			Check(true, TEXT("State.Overloaded cleared (lockout lifted, ~1s stun done)"));
			Step = EStep::OV_DrainToZero; StepTimer = 0.0f;
		}
		else if (StepTimer >= 3.0f)
		{
			Check(false, TEXT("State.Overloaded never cleared within 3s"));
			Step = EStep::OV_DrainToZero; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::OV_DrainToZero:
	{
		// Remove all remaining energy so the NEXT lethal blow is a real death.
		const float Remaining = ReadCarriedEnergy();
		if (Remaining > 0.5f)
		{
			Console(FString::Printf(TEXT("AFL.Combat.EnergyGain %d"), -FMath::CeilToInt(Remaining + 1.0f)));
		}
		Marker(TEXT("OV leg: drained energy to zero; killing again (expect REAL death)"));
		Step = EStep::OV_KillForReal; StepTimer = 0.0f;
		break;
	}
	case EStep::OV_KillForReal:
	{
		if (StepTimer >= 0.6f)
		{
			Check(ReadCarriedEnergy() <= 0.5f, FString::Printf(TEXT("energy at zero before the real-death kill (%.1f)"), ReadCarriedEnergy()));
			Console(TEXT("AFL.Combat.Damage 500"));
			Step = EStep::OV_AssertDeath; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::OV_AssertDeath:
	{
		if (StepTimer >= 0.8f)
		{
			Check(IsDead(), TEXT("REAL death fires with no energy (Status.Death -- the proven path, untouched)"));
			Step = EStep::Finish; StepTimer = 0.0f;
		}
		break;
	}

	// ───────── CHAOS ─────────
	case EStep::CH_Grab:
	{
		if (StepTimer >= 0.4f)
		{
			PressGrab();
			Marker(TEXT("CH leg: grab pressed on the stress object"));
			Step = EStep::CH_AssertCarrier; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::CH_AssertCarrier:
	{
		// The grab is REACH-then-attach (4f): the carrier buff applies at the GrabAttach notify (contact
		// frame of AM_AFL_GrabReach), ~1.5-2s in. Poll until the tag lands (or 4s timeout) so we assert
		// the COMPLETED grab, not a mid-reach frame (run-1: the reach interrupted before contact).
		if (HasTag(TAG_State_Carrying_StressObject_ChaosRun) || StepTimer >= 4.0f)
		{
			Check(HasTag(TAG_State_Carrying_StressObject_ChaosRun), TEXT("State.Carrying.StressObject applied (the multi-effect carrier buff)"));
			// State.Carrying.Vulnerable is the PROVEN x1.3 (carrier-vulnerability cycle, 22/22). Asserting
			// the tag is applied IS the +30%-damage proof -- we deliberately do NOT damage to measure it,
			// because the object is bDropOnDamage: a test hit would DROP it the same instant (the drop is
			// proven LAST). Go straight to the x1.5 extract while the ORIGINAL grab still holds (no
			// re-grab -- every extra PressGrab toggles the carry off: the run-3/4 cascade).
			Check(HasTag(TAG_State_Carrying_Vulnerable_ChaosRun), TEXT("State.Carrying.Vulnerable applied = the proven +30% damage (S10); carrier buff grants it"));
			Step = EStep::CH_SeedExtract; StepTimer = 0.0f;
		}
		break;
	}
	// EXTRACT FIRST (while still carrying), THEN damage -- because the stress object is bDropOnDamage,
	// so the x1.3 damage test DROPS it (intended chaos!); doing it before extract would leave nothing
	// carried for the x1.5 bonus. Order: grab -> extract(x1.5) -> damage(x1.3 + drop) -> instability.
	case EStep::CH_SeedExtract:
	{
		// Spawn a zone AT the pawn (no teleport -- a teleport would drop the carry). Seed + extract.
		if (!Zone.IsValid() && WorldPtr.IsValid() && Pawn.IsValid())
		{
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Zone = WorldPtr->SpawnActor<AAFLExtractionZone>(AAFLExtractionZone::StaticClass(), Pawn->GetActorLocation(), FRotator::ZeroRotator, P);
		}
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("afl.Extract.WindowDuration"))) { V->Set(30.0f, ECVF_SetByConsole); }
		// COMPRESS the channel to 1s -- run-2 showed the carry got thrown ~6s in (carry-pose montage
		// blend-out at the default channel length), reading mult 1.0x. A 1s channel completes WHILE the
		// object is still carried, so the x1.5 carrier read is valid.
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("afl.Extract.ChannelDuration"))) { V->Set(1.0f, ECVF_SetByConsole); }
		if (Zone.IsValid()) { Zone->SetZoneActiveForTest(true); }
		Console(TEXT("AFL.Combat.EnergyGain 50"));
		WattsBeforeExtract = ReadWatts();
		bExtractStarted = false;
		Marker(TEXT("CH leg: zone active + 50 energy; extracting WHILE carrying (1s channel; expect x1.5 Watts)"));
		Step = EStep::CH_AssertExtractMult; StepTimer = 0.0f;
		break;
	}
	case EStep::CH_AssertExtractMult:
	{
		if (StepTimer >= 0.6f && !bExtractStarted)
		{
			bExtractStarted = true;
			Check(HasTag(TAG_State_InExtractionZone_ChaosRun), TEXT("zone dispensed State.InExtractionZone"));
			if (ULyraAbilitySystemComponent* LyraASC = Cast<ULyraAbilitySystemComponent>(ASC.Get()))
			{
				LyraASC->TryActivateAbilityByClass(UAFLAG_Extract::StaticClass());
			}
		}
		if (StepTimer >= 2.5f)   // 1s channel + 0.6s pre-activate + margin
		{
			const int32 Delta = ReadWatts() - WattsBeforeExtract;
			const int32 Expected = FMath::RoundToInt(50.0f * WattsPerEnergyExpected_ChaosRun * ChaosExtractMultExpected); // 750
			Check(Delta == Expected, FString::Printf(TEXT("Watts x1.5 carrier bonus: delta %d == %d (50 x 10 x 1.5)"), Delta, Expected));
			Step = EStep::CH_AssertVuln; StepTimer = 0.0f; // now prove drop-on-damage
		}
		break;
	}
	case EStep::CH_AssertVuln:
	{
		// Drop-on-damage proof. After extract the object is still carried (extract drops ENERGY, not the
		// object). Damage the carrier once -> it drops (bDropOnDamage); assert it is no longer carried.
		if (StepTimer < 0.1f && HasTag(TAG_State_Carrying_StressObject_ChaosRun))
		{
			Console(TEXT("AFL.Combat.Damage 10"));
		}
		if (StepTimer >= 0.8f)
		{
			Check(!HasTag(TAG_State_Carrying_StressObject_ChaosRun), TEXT("damage DROPPED the stress object (drop-on-damage, intended chaos)"));
			Marker(TEXT("CH leg: instability -- compress to ~3s + re-grab once to test reposition"));
			if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("afl.Chaos.InstabilitySeconds"))) { InstabilityRestore = V->GetFloat(); V->Set(3.0f, ECVF_SetByConsole); }
			// Single re-grab for the instability hold (face the dropped object first).
			if (Pawn.IsValid() && StressObject.IsValid())
			{
				const FVector ObjLoc = StressObject->GetActorLocation();
				const FVector PawnLoc = ObjLoc + FVector(150.0f, 0.0f, 0.0f);
				const FRotator Face = (ObjLoc - PawnLoc).Rotation();
				Pawn->SetActorLocationAndRotation(PawnLoc, Face, false, nullptr, ETeleportType::TeleportPhysics);
				if (PC.IsValid()) { PC->SetControlRotation(Face); }
			}
			PressGrab();
			Step = EStep::CH_Instability; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::CH_Instability:
	{
		// Hold > 3s (compressed). The stress component repositions the object.
		if (StepTimer >= 4.5f)
		{
			Step = EStep::CH_AssertReposition; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::CH_AssertReposition:
	{
		const float Moved = StressObject.IsValid() ? FVector::Dist(StressObject->GetActorLocation(), StressStartLoc) : 0.0f;
		Check(StressObject.IsValid() && Moved > 100.0f,
			FString::Printf(TEXT("instability repositioned the object (moved %.0fuu from start after compressed hold)"), Moved));
		Step = EStep::Finish; StepTimer = 0.0f;
		break;
	}

	case EStep::Finish:
	{
		FinishRun();
		break;
	}
	}
}

void UAFLChaosTestRunner::FinishRun()
{
	bRunning = false;
	if (CollectListener.IsValid()) { CollectListener.Unregister(); }
	if (Zone.IsValid()) { Zone->Destroy(); }
	auto Restore = [](const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name)) { V->Set(Value, ECVF_SetByConsole); }
	};
	Restore(TEXT("afl.Energy.DrainPerSecond"), DrainRestore);
	Restore(TEXT("afl.Chaos.InstabilitySeconds"), InstabilityRestore);
	Restore(TEXT("afl.Extract.ChannelDuration"), -1.0f); // back to the GA default (6s)
	Restore(TEXT("afl.Extract.WindowDuration"), 60.0f);

	const TCHAR* Tag = (Mode == ERunMode::Chaos) ? TEXT("AFL_CHAOSRUN") : TEXT("AFL_OVERLOADRUN");
	UE_LOG(LogAFLCombat, Display, TEXT("%s[%s] SUMMARY total: %d checks, %d failed."), Tag, *NetTag(), ChecksTotal, ChecksFailed);
	UE_LOG(LogAFLCombat, Display, TEXT("%s[%s] COMPLETE"), Tag, *NetTag());
	Marker(FString::Printf(TEXT("COMPLETE -- %d checks, %d failed. Stop PIE for log verification."), ChecksTotal, ChecksFailed));
	if (ActiveRun.Get() == this) { ActiveRun.Reset(); }
	RemoveFromRoot();
}

void UAFLChaosTestRunner::PressGrab()
{
	if (ULyraAbilitySystemComponent* LyraASC = Cast<ULyraAbilitySystemComponent>(ASC.Get()))
	{
		LyraASC->AbilityInputTagPressed(TAG_Input_Grab_ChaosRun);
		LyraASC->AbilityInputTagReleased(TAG_Input_Grab_ChaosRun);
	}
}

int32 UAFLChaosTestRunner::CountPickups() const
{
	int32 Count = 0;
	for (TActorIterator<AAFLEnergyPickup> It(WorldPtr.Get()); It; ++It)
	{
		++Count;
	}
	return Count;
}

float UAFLChaosTestRunner::ReadCarriedEnergy() const
{
	return ASC.IsValid() ? ASC->GetNumericAttribute(UAFLAttributeSet_Energy::GetCarriedEnergyAttribute()) : -1.0f;
}

float UAFLChaosTestRunner::ReadHealth() const
{
	// CONVERGENCE: Health migrated to ULyraHealthSet (the ExecCalc drives it; the AFL Health is retired/un-driven).
	return ASC.IsValid() ? ASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()) : -1.0f;
}

float UAFLChaosTestRunner::ReadMaxHealth() const
{
	return ASC.IsValid() ? ASC->GetNumericAttribute(ULyraHealthSet::GetMaxHealthAttribute()) : 100.0f;
}

int32 UAFLChaosTestRunner::ReadWatts() const
{
	return Wallet.IsValid() ? Wallet->GetWatts() : -1;
}

bool UAFLChaosTestRunner::HasTag(const FGameplayTag& Tag) const
{
	return ASC.IsValid() && ASC->HasMatchingGameplayTag(Tag);
}

bool UAFLChaosTestRunner::IsDead() const
{
	// Status.Death.* is granted by Lyra's StartDeath. PartialMatch via the parent tag.
	return ASC.IsValid() && ASC->HasMatchingGameplayTag(TAG_Status_Death_ChaosRun);
}

AActor* UAFLChaosTestRunner::FindStressObject() const
{
	// The stress object is any actor carrying a component named AFLStressObjectComponent.
	for (TActorIterator<AActor> It(WorldPtr.Get()); It; ++It)
	{
		for (UActorComponent* Comp : It->GetComponents())
		{
			if (Comp && Comp->GetClass()->GetName().Contains(TEXT("AFLStressObjectComponent")))
			{
				return *It;
			}
		}
	}
	return nullptr;
}

void UAFLChaosTestRunner::Console(const FString& Cmd)
{
	if (PC.IsValid()) { PC->ConsoleCommand(Cmd, true); }
}

FString UAFLChaosTestRunner::NetTag() const
{
	const UWorld* W = WorldPtr.Get();
	int32 PieId = -1;
#if WITH_EDITOR
	if (W && W->GetOutermost()) { PieId = W->GetOutermost()->GetPIEInstanceID(); }
#endif
	const TCHAR* Prefix = (W && W->GetNetMode() == NM_Client) ? TEXT("CL") : TEXT("SV");
	return (PieId >= 0) ? FString::Printf(TEXT("%s%d"), Prefix, PieId) : FString(Prefix);
}

void UAFLChaosTestRunner::Marker(const FString& Msg) const
{
	const TCHAR* Tag = (Mode == ERunMode::Chaos) ? TEXT("AFL_CHAOSRUN") : TEXT("AFL_OVERLOADRUN");
	UE_LOG(LogAFLCombat, Display, TEXT("%s[%s]: %s"), Tag, *NetTag(), *Msg);
}

void UAFLChaosTestRunner::Check(bool bPass, const FString& What)
{
	++ChecksTotal;
	if (!bPass) { ++ChecksFailed; }
	const TCHAR* Tag = (Mode == ERunMode::Chaos) ? TEXT("AFL_CHAOSRUN") : TEXT("AFL_OVERLOADRUN");
	UE_LOG(LogAFLCombat, Display, TEXT("%s[%s] %s -- %s"), Tag, *NetTag(), bPass ? TEXT("PASS") : TEXT("FAIL"), *What);
}

#if UE_WITH_CHEAT_MANAGER
namespace
{
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLOverloadTestRunCmd(
		TEXT("afl.Overload.Test.Run"),
		TEXT("P2 close-out: survive-at-zero-while-carrying then real-death-with-no-energy -- HOST window, observe-only, ~20s."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				Ar.Log(TEXT("afl.Overload.Test.Run -- starting (see AFL_OVERLOADRUN lines in LogAFLCombat)."));
				UAFLChaosTestRunner::RunInWorld(World, UAFLChaosTestRunner::ERunMode::Overload);
			}));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLChaosTestRunCmd(
		TEXT("afl.Chaos.Test.Run"),
		TEXT("P2 close-out: stress-object carrier multipliers (x1.3 dmg, x1.5 extract) + instability -- HOST window, observe-only, requires a placed stress object, ~25s."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				Ar.Log(TEXT("afl.Chaos.Test.Run -- starting (see AFL_CHAOSRUN lines in LogAFLCombat)."));
				UAFLChaosTestRunner::RunInWorld(World, UAFLChaosTestRunner::ERunMode::Chaos);
			}));
}
#endif // UE_WITH_CHEAT_MANAGER
