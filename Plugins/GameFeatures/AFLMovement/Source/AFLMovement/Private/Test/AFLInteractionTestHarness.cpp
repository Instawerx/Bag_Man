// Copyright C12 AI Gaming. All Rights Reserved.

#include "Test/AFLInteractionTestHarness.h"

#include "AFLMovement.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AttributeSet.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CheatManagerDefines.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/AFLGrabbableComponent.h"
#include "Interaction/AFLInteractionComponent.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLInteractionTestHarness)

TWeakObjectPtr<UAFLInteractionTestHarness> UAFLInteractionTestHarness::ActiveRun;
TWeakObjectPtr<UAFLInteractionTestHarness> UAFLInteractionTestHarness::ActiveObserver;

namespace
{
	// Runtime-requested (all registered: ini + native defines elsewhere). String-keyed on purpose --
	// the harness asserts against the SAME tag names the gameplay code gates on.
	const FName NAME_Tag_State_Carrying_Harness       = TEXT("State.Carrying");
	const FName NAME_Tag_State_ThrowRecovery_Harness  = TEXT("State.Weapon.ThrowRecovery");
	const FName NAME_Tag_Input_Grab_Harness           = TEXT("InputTag.Ability.Grab");
	const FName NAME_Tag_Input_Fire_Harness           = TEXT("InputTag.Weapon.Fire");

	// Expected ExecCalc outputs for AFL.Combat.Damage 10 (armor 0, shield 0 on the test pawn).
	constexpr float DamageCmdAmount   = 10.0f;
	constexpr float ExpectPlainDelta  = 10.0f;   // 10 * 1.0
	constexpr float ExpectVulnDelta   = 13.0f;   // 10 * 1.3 (State.Carrying.Vulnerable, ExecCalc 5b)
	constexpr float DeltaTolerance    = 0.6f;
}

void UAFLInteractionTestHarness::RunInWorld(UWorld* World)
{
	// HARD GUARD (2-client run 1 inverted exactly this way): the FSM needs AUTHORITY -- its teleports are
	// server ops and the damage cheat lands on the host pawn. Typed in a client window, every grab cancels
	// server-side (local-only teleports) and every health read misses (damage hit the host) -- a whole run
	// of structurally-invalid verdicts. Refuse loudly instead.
	if (World && World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogAFLMovement, Error,
			TEXT("AFL_TEST: afl.Interaction.Test.Run typed in a CLIENT window -- run it in the HOST window. ")
			TEXT("This window is where afl.Interaction.Test.Observe belongs."));
		return;
	}
	if (ActiveRun.IsValid())
	{
		UE_LOG(LogAFLMovement, Warning, TEXT("AFL_TEST: a run is already live -- wait for AFL_TEST COMPLETE."));
		return;
	}
	UAFLInteractionTestHarness* Harness = NewObject<UAFLInteractionTestHarness>(GetTransientPackage());
	if (!Harness->StartRun(World))
	{
		UE_LOG(LogAFLMovement, Error, TEXT("AFL_TEST ABORT -- preconditions unmet (see lines above). Nothing ran."));
		return;
	}
	Harness->AddToRoot();
	ActiveRun = Harness;
}

void UAFLInteractionTestHarness::RunObserverInWorld(UWorld* World)
{
	// HARD GUARD (mirror of RunInWorld's): the observer asserts what REPLICATION delivered, so it only
	// means anything in a CLIENT window. On the host everything is authority-local (vacuously true) and
	// the scripted carrier is the HOST pawn -- an observer there watches the wrong player.
	if (World && World->GetNetMode() != NM_Client)
	{
		UE_LOG(LogAFLMovement, Error,
			TEXT("AFL_TEST: afl.Interaction.Test.Observe typed in a non-CLIENT window -- observe from the ")
			TEXT("CLIENT window. This window is where afl.Interaction.Test.Run belongs."));
		return;
	}
	if (ActiveObserver.IsValid())
	{
		UE_LOG(LogAFLMovement, Warning, TEXT("AFL_TEST: an observer is already live -- wait for its COMPLETE."));
		return;
	}
	UAFLInteractionTestHarness* Observer = NewObject<UAFLInteractionTestHarness>(GetTransientPackage());
	if (!Observer->StartObserver(World))
	{
		UE_LOG(LogAFLMovement, Error, TEXT("AFL_TEST ABORT -- observer preconditions unmet (see lines above)."));
		return;
	}
	Observer->AddToRoot();
	ActiveObserver = Observer;
}

bool UAFLInteractionTestHarness::StartObserver(UWorld* World)
{
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogAFLMovement, Error, TEXT("AFL_TEST: observer needs a game world (run in the CLIENT PIE window)."));
		return false;
	}
	WorldPtr = World;
	PC = World->GetFirstPlayerController(); // the observer's own player; never driven, never written to

	// The REMOTE player (the host running the FSM) is the carrier under test. The ASC lives on the
	// PlayerState (survives pawn death/respawn); the pawn is re-resolved per sample off the ASC avatar.
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		if (It->GetPlayerState() && !It->IsLocallyControlled())
		{
			RemotePawn = *It;
			RemoteASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(It->GetPlayerState());
			break;
		}
	}
	if (!RemoteASC.IsValid())
	{
		UE_LOG(LogAFLMovement, Error, TEXT("AFL_TEST: observer found no REMOTE player (is the host in and visible?)."));
		return false;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->FindComponentByClass<UAFLGrabbableComponent>())
		{
			ObservedGrabbables.Add(*It);
		}
	}
	if (ObservedGrabbables.Num() == 0)
	{
		UE_LOG(LogAFLMovement, Error, TEXT("AFL_TEST: observer sees no grabbables in the client world."));
		return false;
	}

	bObserver = true;
	bRunning = true;
	Banner(FString::Printf(TEXT("OBSERVER up -- watching remote %s + %d grabbables for 75s. Read-only: no teleports, no writes."),
		*GetNameSafe(RemotePawn.Get()), ObservedGrabbables.Num()));
	return true;
}

void UAFLInteractionTestHarness::SampleObserver()
{
	// Re-resolve the remote pawn across respawns (the ASC outlives it).
	if (!RemotePawn.IsValid() && RemoteASC.IsValid())
	{
		RemotePawn = Cast<APawn>(RemoteASC->GetAvatarActor());
	}

	const FGameplayTag CarryTag = FGameplayTag::RequestGameplayTag(NAME_Tag_State_Carrying_Harness, false);
	const bool bCarrying = CarryTag.IsValid() && RemoteASC.IsValid() && RemoteASC->HasMatchingGameplayTag(CarryTag);

	if (bCarrying && !bPrevCarrying)
	{
		++CarryEpisodes;
	}
	if (!bCarrying && bPrevCarrying && LastAttached.IsValid())
	{
		// Falling edge: arm the settle watch (~2.2s out, then a stability re-check one sample later).
		SettleCountdown = 4;
	}

	if (bCarrying && RemotePawn.IsValid())
	{
		for (const TWeakObjectPtr<AActor>& GA : ObservedGrabbables)
		{
			AActor* Obj = GA.Get();
			if (!Obj || Obj->GetAttachParentActor() != RemotePawn.Get())
			{
				continue;
			}
			++AttachSamples;
			LastAttached = Obj;
			if (FVector::Dist(Obj->GetActorLocation(), RemotePawn->GetActorLocation()) < 250.0f)
			{
				++RideSamples;
			}
			if (const UAFLGrabbableComponent* G = Obj->FindComponentByClass<UAFLGrabbableComponent>())
			{
				(G->IsHeld() ? HeldCoherent : HeldViolations)++;
			}
		}
	}
	else if (!bCarrying && !bPrevCarrying && !bPrevPrevCarrying)
	{
		// Steady not-carrying (2-sample replication grace): NOTHING may still read held.
		for (const TWeakObjectPtr<AActor>& GA : ObservedGrabbables)
		{
			const AActor* Obj = GA.Get();
			const UAFLGrabbableComponent* G = Obj ? Obj->FindComponentByClass<UAFLGrabbableComponent>() : nullptr;
			if (G && G->IsHeld())
			{
				++StaleHeldViolations;
			}
		}
	}

	// Post-release settle/convergence: snapshot, then assert the object stopped moving on THIS client.
	if (SettleCountdown > 0 && LastAttached.IsValid())
	{
		--SettleCountdown;
		if (SettleCountdown == 1)
		{
			SettlePos = LastAttached->GetActorLocation();
		}
		else if (SettleCountdown == 0)
		{
			const float Delta = FVector::Dist(SettlePos, LastAttached->GetActorLocation());
			const bool bSettled = Delta < 5.0f;
			(bSettled ? ConvergePass : ConvergeFail)++;
			LogObjectPos(bSettled ? TEXT("settle") : TEXT("settle-UNSTABLE"), LastAttached.Get());
		}
	}

	bPrevPrevCarrying = bPrevCarrying;
	bPrevCarrying = bCarrying;
}

bool UAFLInteractionTestHarness::StartRun(UWorld* World)
{
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogAFLMovement, Error, TEXT("AFL_TEST: no game world (run inside PIE)."));
		return false;
	}
	WorldPtr = World;
	PC = World->GetFirstPlayerController();
	Pawn = PC.IsValid() ? PC->GetPawn() : nullptr;
	if (!Pawn.IsValid())
	{
		UE_LOG(LogAFLMovement, Error, TEXT("AFL_TEST: no possessed pawn."));
		return false;
	}

	// Lyra's ASC lives on the PlayerState (the BM-DEBT-008 lesson) -- resolve via the engine helper.
	APlayerState* PS = PC->PlayerState;
	UAbilitySystemComponent* ASC = PS ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS) : nullptr;
	LyraASC = Cast<ULyraAbilitySystemComponent>(ASC);
	if (!LyraASC.IsValid())
	{
		UE_LOG(LogAFLMovement, Error, TEXT("AFL_TEST: no ULyraAbilitySystemComponent on the PlayerState."));
		return false;
	}

	Interaction = Pawn->FindComponentByClass<UAFLInteractionComponent>();
	if (!Interaction.IsValid())
	{
		UE_LOG(LogAFLMovement, Error, TEXT("AFL_TEST: pawn has no UAFLInteractionComponent."));
		return false;
	}

	// DISCOVER + classify every placed grabbable by its live policy (instance deltas included --
	// box B's bDropOnDamage=false is a placed-instance override, so classification MUST read the
	// component on the placed actor, never the BP default).
	int32 Found = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		UAFLGrabbableComponent* G = It->FindComponentByClass<UAFLGrabbableComponent>();
		if (!G)
		{
			continue;
		}
		++Found;
		const FAFLGrabPolicy Policy = G->GetGrabPolicy();
		const TCHAR* Class = TEXT("PLAIN");
		if (!Policy.CarrierEffectClass.IsNull())
		{
			Class = TEXT("VULN");
			if (!VulnActor.IsValid()) { VulnActor = *It; }
		}
		else if (!Policy.bDropOnDamage)
		{
			Class = TEXT("NODROP");
			if (!NoDropActor.IsValid()) { NoDropActor = *It; }
		}
		else if (!PlainActor.IsValid())
		{
			PlainActor = *It;
		}
		UE_LOG(LogAFLMovement, Display, TEXT("AFL_TEST[%s] DISCOVER: %s -> %s (dropOnDamage=%d carrierGE=%s)"),
			*NetTag(), *GetNameSafe(*It), Class, Policy.bDropOnDamage ? 1 : 0,
			Policy.CarrierEffectClass.IsNull() ? TEXT("None") : *Policy.CarrierEffectClass.ToString());
	}
	UE_LOG(LogAFLMovement, Display, TEXT("AFL_TEST[%s] DISCOVER: %d grabbables -- VULN=%s NODROP=%s PLAIN=%s"),
		*NetTag(), Found, *GetNameSafe(VulnActor.Get()), *GetNameSafe(NoDropActor.Get()), *GetNameSafe(PlainActor.Get()));

	if (!VulnActor.IsValid() || !NoDropActor.IsValid() || !PlainActor.IsValid())
	{
		UE_LOG(LogAFLMovement, Error,
			TEXT("AFL_TEST: need one of each class placed (VULN=sphere, NODROP=box B override, PLAIN=box A)."));
		return false;
	}

	BuildSteps();
	StepIndex = 0;
	Accum = StepInterval; // fire the first step immediately
	bRunning = true;
	Banner(TEXT("AFL_TEST RUN -- P0-P4 scripted, ~1.5s pacing. Watch the pawn; stop PIE after COMPLETE."));
	return true;
}

void UAFLInteractionTestHarness::Tick(float DeltaTime)
{
	if (!bRunning)
	{
		return;
	}
	if (!WorldPtr.IsValid())
	{
		UE_LOG(LogAFLMovement, Error, TEXT("AFL_TEST ABORT -- world went away mid-run."));
		FinishRun();
		return;
	}

	// Observer mode: fixed-duration sampling loop, no step list. 0.75s cadence (2x the FSM pacing) for
	// clean edge detection; 75s window covers the ~45s host run with margin.
	if (bObserver)
	{
		if (!RemoteASC.IsValid())
		{
			Banner(TEXT("observer: remote ASC went away -- finishing with the samples collected."));
			FinishRun();
			return;
		}
		ObserverElapsed += DeltaTime;
		SampleAccum += DeltaTime;
		if (SampleAccum >= 0.75f)
		{
			SampleAccum = 0.0f;
			SampleObserver();
		}
		if (ObserverElapsed >= 75.0f)
		{
			FinishRun();
		}
		return;
	}

	if (!Pawn.IsValid() || !LyraASC.IsValid())
	{
		UE_LOG(LogAFLMovement, Error, TEXT("AFL_TEST ABORT -- pawn/ASC went away mid-run."));
		FinishRun();
		return;
	}
	Accum += DeltaTime;
	if (Accum < StepInterval)
	{
		return;
	}
	Accum = 0.0f;

	if (!Steps.IsValidIndex(StepIndex))
	{
		FinishRun();
		return;
	}
	const FStep& Step = Steps[StepIndex];
	Banner(FString::Printf(TEXT("step %d/%d -- %s"), StepIndex + 1, Steps.Num(), *Step.Label));
	Step.Action();
	++StepIndex;
}

void UAFLInteractionTestHarness::BuildSteps()
{
	// The script reads top-to-bottom as the PIE protocol. Each step runs 1.5s after the previous;
	// asserts always check live state from the PREVIOUS step's action.
	auto Add = [this](const FString& Label, TFunction<void()> Fn) { Steps.Add({Label, MoveTemp(Fn)}); };

	// ---- P0: forced-drop funnel + leak-dead ------------------------------------------------------
	Add(TEXT("[P0] place PLAIN, grab"), [this]
	{
		ParkAll(); PlaceInFront(PlainActor.Get());
		Console(FString::Printf(TEXT("SetCombatAttribute Health %.0f"), 100.0f));
		PressTag(NAME_Tag_Input_Grab_Harness);
	});
	Add(TEXT("[P0] assert carrying; damage"), [this]
	{
		Check(TEXT("P0"), IsCarryingNow(), TEXT("grab via input seam acquired the PLAIN box"));
		Console(FString::Printf(TEXT("AFL.Combat.Damage %.0f"), DamageCmdAmount));
	});
	Add(TEXT("[P0] assert leak-dead; re-grab"), [this]
	{
		const bool bTagGone = !HasTag(NAME_Tag_State_Carrying_Harness);
		Check(TEXT("P0"), !IsCarryingNow(), TEXT("damage force-dropped the box"));
		Check(TEXT("P0"), bTagGone, TEXT("State.Carrying GONE at the forced drop (the leak fix: no ghost G-press needed; fire-block gate down)"));
		PressTag(NAME_Tag_Input_Grab_Harness);
	});
	Add(TEXT("[P0] assert re-grab; toggle drop"), [this]
	{
		Check(TEXT("P0"), IsCarryingNow(), TEXT("grab works again after the forced drop (ability re-activatable)"));
		PressTag(NAME_Tag_Input_Grab_Harness);
	});
	Add(TEXT("[P0] assert toggle drop"), [this]
	{
		Check(TEXT("P0"), !IsCarryingNow(), TEXT("second press toggle-dropped"));
		LogObjectPos(TEXT("post-drop"), PlainActor.Get());
		Skip(TEXT("P0"), TEXT("climb-forced drop leg: MANUAL (needs a wall) -- carry a box, start a climb, expect drop + immediate fire"));
	});

	// ---- P1: vulnerability ON while carrying the flagged object ----------------------------------
	Add(TEXT("[P1] reset health, place VULN, grab"), [this]
	{
		ParkAll(); PlaceInFront(VulnActor.Get());
		Console(TEXT("SetCombatAttribute Health 100"));
		PressTag(NAME_Tag_Input_Grab_Harness);
	});
	Add(TEXT("[P1] assert carrying; read H; damage"), [this]
	{
		Check(TEXT("P1"), IsCarryingNow(), TEXT("carrying the VULN sphere"));
		bool bFound = false;
		HealthBefore = ReadHealth(bFound);
		Check(TEXT("P1"), bFound, TEXT("Health attribute readable (stringly)"));
		Console(FString::Printf(TEXT("AFL.Combat.Damage %.0f"), DamageCmdAmount));
	});
	Add(TEXT("[P1] assert x1.3 + drop; damage again"), [this]
	{
		bool bFound = false;
		const float Now = ReadHealth(bFound);
		const float Delta = HealthBefore - Now;
		Check(TEXT("P1"), bFound && FMath::Abs(Delta - ExpectVulnDelta) <= DeltaTolerance,
			FString::Printf(TEXT("vulnerable hit fell %.1f (expect %.1f) -- x1.3 through the real ExecCalc"), Delta, ExpectVulnDelta));
		Check(TEXT("P2"), !IsCarryingNow(), TEXT("damage force-dropped the sphere (first hit amplified + drops)"));
		HealthBefore = Now;
		Console(FString::Printf(TEXT("AFL.Combat.Damage %.0f"), DamageCmdAmount));
	});
	Add(TEXT("[P1] assert post-drop 10; setup PLAIN-carry control"), [this]
	{
		bool bFound = false;
		const float Now = ReadHealth(bFound);
		const float Delta = HealthBefore - Now;
		Check(TEXT("P2"), bFound && FMath::Abs(Delta - ExpectPlainDelta) <= DeltaTolerance,
			FString::Printf(TEXT("second hit after the forced drop fell %.1f (expect %.1f) -- vulnerability OFF"), Delta, ExpectPlainDelta));
		ParkAll(); PlaceInFront(PlainActor.Get());
		Console(TEXT("SetCombatAttribute Health 100"));
		PressTag(NAME_Tag_Input_Grab_Harness);
	});
	Add(TEXT("[P1] assert carrying PLAIN; damage"), [this]
	{
		Check(TEXT("P1"), IsCarryingNow(), TEXT("carrying the PLAIN box (control)"));
		bool bFound = false;
		HealthBefore = ReadHealth(bFound);
		Console(FString::Printf(TEXT("AFL.Combat.Damage %.0f"), DamageCmdAmount));
	});
	Add(TEXT("[P1] assert PLAIN-carry took 10"), [this]
	{
		bool bFound = false;
		const float Now = ReadHealth(bFound);
		const float Delta = HealthBefore - Now;
		Check(TEXT("P1"), bFound && FMath::Abs(Delta - ExpectPlainDelta) <= DeltaTolerance,
			FString::Printf(TEXT("carrying an UNFLAGGED box fell %.1f (expect %.1f) -- amplification is per-object"), Delta, ExpectPlainDelta));
	});

	// ---- P2: vulnerability OFF on every release verb ----------------------------------------------
	Add(TEXT("[P2] place VULN, grab"), [this]
	{
		ParkAll(); PlaceInFront(VulnActor.Get());
		Console(TEXT("SetCombatAttribute Health 100"));
		PressTag(NAME_Tag_Input_Grab_Harness);
	});
	Add(TEXT("[P2] assert carrying; G-drop"), [this]
	{
		Check(TEXT("P2"), IsCarryingNow(), TEXT("carrying the sphere (G-drop leg)"));
		PressTag(NAME_Tag_Input_Grab_Harness);
	});
	Add(TEXT("[P2] assert dropped; damage"), [this]
	{
		Check(TEXT("P2"), !IsCarryingNow(), TEXT("toggle-dropped the sphere"));
		LogObjectPos(TEXT("post-gdrop"), VulnActor.Get());
		bool bFound = false;
		HealthBefore = ReadHealth(bFound);
		Console(FString::Printf(TEXT("AFL.Combat.Damage %.0f"), DamageCmdAmount));
	});
	Add(TEXT("[P2] assert 10 after G-drop; re-place + grab"), [this]
	{
		bool bFound = false;
		const float Now = ReadHealth(bFound);
		const float Delta = HealthBefore - Now;
		Check(TEXT("P2"), bFound && FMath::Abs(Delta - ExpectPlainDelta) <= DeltaTolerance,
			FString::Printf(TEXT("after G-drop fell %.1f (expect %.1f) -- carrier GE removed by handle"), Delta, ExpectPlainDelta));
		ParkAll(); PlaceInFront(VulnActor.Get());
		PressTag(NAME_Tag_Input_Grab_Harness);
	});
	Add(TEXT("[P2] assert carrying; THROW (fire press)"), [this]
	{
		Check(TEXT("P2"), IsCarryingNow(), TEXT("carrying the sphere (throw leg)"));
		PressTag(NAME_Tag_Input_Fire_Harness); // arbitration live: this press must THROW, not fire
	});
	Add(TEXT("[P2] assert thrown; damage"), [this]
	{
		Check(TEXT("P2"), !IsCarryingNow(), TEXT("fire press while carrying THREW the sphere (arbitration intact)"));
		LogObjectPos(TEXT("post-throw"), VulnActor.Get());
		bool bFound = false;
		HealthBefore = ReadHealth(bFound);
		Console(FString::Printf(TEXT("AFL.Combat.Damage %.0f"), DamageCmdAmount));
	});
	Add(TEXT("[P2] assert 10 after throw"), [this]
	{
		bool bFound = false;
		const float Now = ReadHealth(bFound);
		const float Delta = HealthBefore - Now;
		Check(TEXT("P2"), bFound && FMath::Abs(Delta - ExpectPlainDelta) <= DeltaTolerance,
			FString::Printf(TEXT("after throw fell %.1f (expect %.1f) -- vulnerability dies with the carry on every verb"), Delta, ExpectPlainDelta));
	});

	// ---- P3: per-object opt-outs (null carrier GE + bDropOnDamage=false instance override) --------
	Add(TEXT("[P3] place NODROP, grab"), [this]
	{
		ParkAll(); PlaceInFront(NoDropActor.Get());
		Console(TEXT("SetCombatAttribute Health 100"));
		PressTag(NAME_Tag_Input_Grab_Harness);
	});
	Add(TEXT("[P3] assert carrying; damage"), [this]
	{
		Check(TEXT("P3"), IsCarryingNow(), TEXT("carrying the NODROP box (B's placed-instance override)"));
		bool bFound = false;
		HealthBefore = ReadHealth(bFound);
		Console(FString::Printf(TEXT("AFL.Combat.Damage %.0f"), DamageCmdAmount));
	});
	Add(TEXT("[P3] assert SURVIVED hit + 10; drop"), [this]
	{
		bool bFound = false;
		const float Now = ReadHealth(bFound);
		const float Delta = HealthBefore - Now;
		Check(TEXT("P3"), IsCarryingNow(), TEXT("bDropOnDamage=false box STILL carried through the hit"));
		Check(TEXT("P3"), bFound && FMath::Abs(Delta - ExpectPlainDelta) <= DeltaTolerance,
			FString::Printf(TEXT("null CarrierEffectClass box took %.1f (expect %.1f) -- no amplification"), Delta, ExpectPlainDelta));
		PressTag(NAME_Tag_Input_Grab_Harness);
	});
	Add(TEXT("[P3] assert dropped"), [this]
	{
		Check(TEXT("P3"), !IsCarryingNow(), TEXT("NODROP box released by the normal toggle"));
		LogObjectPos(TEXT("post-drop"), NoDropActor.Get());
	});

	// ---- P4: regression sweep (gate up while carrying, throw-recovery window) ---------------------
	Add(TEXT("[P4] place PLAIN, grab"), [this]
	{
		ParkAll(); PlaceInFront(PlainActor.Get());
		PressTag(NAME_Tag_Input_Grab_Harness);
	});
	Add(TEXT("[P4] assert carry gate; throw + 0.2s recovery snapshot"), [this]
	{
		Check(TEXT("P4"), IsCarryingNow() && HasTag(NAME_Tag_State_Carrying_Harness),
			TEXT("State.Carrying present while carrying (the fire-block gate is up)"));
		bRecoveryTagObserved = false;
		PressTag(NAME_Tag_Input_Fire_Harness);
		// The 0.4s ThrowRecovery window is shorter than the 1.5s step -- snapshot it mid-window.
		if (UWorld* W = WorldPtr.Get())
		{
			FTimerHandle Snap;
			W->GetTimerManager().SetTimer(Snap, FTimerDelegate::CreateWeakLambda(this, [this]
			{
				bRecoveryTagObserved = HasTag(NAME_Tag_State_ThrowRecovery_Harness);
			}), 0.2f, false);
		}
	});
	Add(TEXT("[P4] assert thrown + recovery window observed"), [this]
	{
		Check(TEXT("P4"), !IsCarryingNow(), TEXT("P4 throw released the box"));
		Check(TEXT("P4"), bRecoveryTagObserved, TEXT("State.Weapon.ThrowRecovery live 0.2s post-throw (the 0.4s gate window)"));
		LogObjectPos(TEXT("post-throw"), PlainActor.Get());
		Skip(TEXT("P5"), TEXT("sphere look = operator-visual only (orb material, not-a-test-box)"));
	});
}

void UAFLInteractionTestHarness::FinishRun()
{
	bRunning = false;

	// Observer aggregate verdicts (sampled invariants, not phase-scripted asserts).
	if (bObserver)
	{
		Check(TEXT("OBS"), CarryEpisodes >= 3,
			FString::Printf(TEXT("carry episodes seen on the wire (%d, expect >=3) -- State.Carrying tag replicates"), CarryEpisodes));
		Check(TEXT("OBS"), AttachSamples >= 5,
			FString::Printf(TEXT("attach-parent visible client-side while carried (%d samples) -- FRepAttachment flows"), AttachSamples));
		Check(TEXT("OBS"), AttachSamples > 0 && RideSamples * 10 >= AttachSamples * 8,
			FString::Printf(TEXT("carried object rides the remote carrier (%d/%d samples within 250cm)"), RideSamples, AttachSamples));
		Check(TEXT("OBS"), HeldViolations == 0 && HeldCoherent >= 5,
			FString::Printf(TEXT("bHeld replicated TRUE while attached (%d coherent / %d violations)"), HeldCoherent, HeldViolations));
		Check(TEXT("OBS"), StaleHeldViolations == 0,
			FString::Printf(TEXT("bHeld replicated FALSE after release (%d stale-held samples)"), StaleHeldViolations));
		Check(TEXT("OBS"), ConvergePass >= 2 && ConvergeFail == 0,
			FString::Printf(TEXT("released objects settle stably on this client (%d pass / %d unstable)"), ConvergePass, ConvergeFail));
	}

	int32 TotalChecks = 0, TotalFails = 0;
	for (const FString& Phase : PhaseOrder)
	{
		const int32 N = PhaseTotals.FindRef(Phase);
		const int32 F = PhaseFails.FindRef(Phase);
		TotalChecks += N; TotalFails += F;
		UE_LOG(LogAFLMovement, Display, TEXT("AFL_TEST[%s] SUMMARY [%s] %s -- %d/%d checks passed"),
			*NetTag(), *Phase, (F == 0) ? TEXT("PASS") : TEXT("FAIL"), N - F, N);
	}
	UE_LOG(LogAFLMovement, Display, TEXT("AFL_TEST[%s] SUMMARY total: %d checks, %d failed.%s"),
		*NetTag(), TotalChecks, TotalFails,
		bObserver ? TEXT("") : TEXT(" SKIPs: P0 climb-forced (manual), P5 visual (operator)."));
	UE_LOG(LogAFLMovement, Display, TEXT("AFL_TEST[%s] COMPLETE"), *NetTag());
	Banner(FString::Printf(TEXT("AFL_TEST COMPLETE -- %d checks, %d failed. Stop PIE for log verification."), TotalChecks, TotalFails));

	if (ActiveRun.Get() == this)
	{
		ActiveRun.Reset();
	}
	if (ActiveObserver.Get() == this)
	{
		ActiveObserver.Reset();
	}
	RemoveFromRoot();
}

// ---------------------------------------------------------------------------------------------------
// primitives
// ---------------------------------------------------------------------------------------------------

void UAFLInteractionTestHarness::PressTag(const FName TagName)
{
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TagName, /*ErrorIfNotFound*/ false);
	if (!Tag.IsValid() || !LyraASC.IsValid())
	{
		UE_LOG(LogAFLMovement, Error, TEXT("AFL_TEST: PressTag(%s) -- tag or ASC invalid."), *TagName.ToString());
		return;
	}
	// Press + release back-to-back: ProcessAbilityInput still activates OnInputTriggered specs from
	// the pressed set this frame, and the held set is clear before any WhileInputActive retry --
	// which is EXACTLY the shape the real IA trigger produces (release reported one frame after the
	// press; the throw-recovery saga's root). The realest synthetic press available.
	LyraASC->AbilityInputTagPressed(Tag);
	LyraASC->AbilityInputTagReleased(Tag);
}

void UAFLInteractionTestHarness::Console(const FString& Cmd)
{
	if (PC.IsValid())
	{
		// Reuse the established cheats (AFL.Combat.Damage -> real ExecCalc; SetCombatAttribute ->
		// the cheat-manager Exec). The harness never applies damage or writes attributes itself.
		PC->ConsoleCommand(Cmd, /*bWriteToLog*/ true);
	}
}

void UAFLInteractionTestHarness::PlaceInFront(AActor* Target)
{
	if (!Target || !Pawn.IsValid())
	{
		return;
	}
	const FVector Loc = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 150.0f + FVector(0, 0, 10.0f);
	if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Target->GetRootComponent()))
	{
		if (Prim->IsSimulatingPhysics())
		{
			Prim->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Prim->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}
	}
	Target->SetActorLocation(Loc, /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
}

void UAFLInteractionTestHarness::Park(AActor* Target)
{
	if (!Target || !Pawn.IsValid())
	{
		return;
	}
	// 6m behind the pawn -- outside GrabReachDistance (250) so a parked object can never win discovery.
	const FVector Loc = Pawn->GetActorLocation() - Pawn->GetActorForwardVector() * 600.0f + FVector(0, 0, 30.0f);
	if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Target->GetRootComponent()))
	{
		if (Prim->IsSimulatingPhysics())
		{
			Prim->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Prim->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}
	}
	Target->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
}

void UAFLInteractionTestHarness::ParkAll()
{
	Park(VulnActor.Get());
	Park(NoDropActor.Get());
	Park(PlainActor.Get());
}

bool UAFLInteractionTestHarness::IsCarryingNow() const
{
	return Interaction.IsValid() && Interaction->IsCarrying();
}

bool UAFLInteractionTestHarness::HasTag(const FName TagName) const
{
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TagName, false);
	return Tag.IsValid() && LyraASC.IsValid() && LyraASC->HasMatchingGameplayTag(Tag);
}

float UAFLInteractionTestHarness::ReadHealth(bool& bOutFound) const
{
	bOutFound = false;
	if (!LyraASC.IsValid())
	{
		return 0.0f;
	}
	// TEST-SCOPED stringly attribute READ. The harness lives in AFLMovement and deliberately does not
	// link AFLCombat just to name UAFLAttributeSet_Combat::GetHealthAttribute(); walking the spawned
	// sets for a FGameplayAttributeData property literally named "Health" keeps the dependency surface
	// zero. Read-only -- every WRITE in this file routes through the established cheats, so the
	// AFL-0215 rail (no direct SetHealth) holds.
	for (UAttributeSet* Set : LyraASC->GetSpawnedAttributes())
	{
		if (!Set)
		{
			continue;
		}
		// The PlayerState ASC carries BOTH Lyra's ULyraHealthSet and UAFLAttributeSet_Combat, and both
		// expose a "Health" FGameplayAttributeData. AFL damage writes the AFL set -- the first harness
		// run matched Lyra's set first and read a static 100 (every delta check fell 0.0). Pin the
		// read to the AFL combat set by class name (still stringly, still zero link deps).
		if (!Set->GetClass()->GetName().Contains(TEXT("AFLAttributeSet_Combat")))
		{
			continue;
		}
		if (const FStructProperty* Prop = FindFProperty<FStructProperty>(Set->GetClass(), TEXT("Health")))
		{
			if (Prop->Struct == FGameplayAttributeData::StaticStruct())
			{
				const FGameplayAttributeData* Data = Prop->ContainerPtrToValuePtr<FGameplayAttributeData>(Set);
				bOutFound = true;
				return Data->GetCurrentValue();
			}
		}
	}
	return 0.0f;
}

void UAFLInteractionTestHarness::Check(const TCHAR* Phase, bool bPass, const FString& What)
{
	const FString PhaseStr(Phase);
	if (!PhaseOrder.Contains(PhaseStr))
	{
		PhaseOrder.Add(PhaseStr);
	}
	PhaseTotals.FindOrAdd(PhaseStr)++;
	if (!bPass)
	{
		PhaseFails.FindOrAdd(PhaseStr)++;
	}
	UE_LOG(LogAFLMovement, Display, TEXT("AFL_TEST[%s][%s] %s -- %s"), *NetTag(), Phase, bPass ? TEXT("PASS") : TEXT("FAIL"), *What);
#if !(UE_BUILD_SHIPPING)
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, bPass ? FColor::Green : FColor::Red,
			FString::Printf(TEXT("AFL_TEST[%s][%s] %s  %s"), *NetTag(), Phase, bPass ? TEXT("PASS") : TEXT("FAIL"), *What));
	}
#endif
}

void UAFLInteractionTestHarness::Skip(const TCHAR* Phase, const FString& Why)
{
	UE_LOG(LogAFLMovement, Display, TEXT("AFL_TEST[%s][%s] SKIP -- %s"), *NetTag(), Phase, *Why);
#if !(UE_BUILD_SHIPPING)
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Yellow,
			FString::Printf(TEXT("AFL_TEST[%s][%s] SKIP  %s"), *NetTag(), Phase, *Why));
	}
#endif
}

void UAFLInteractionTestHarness::Banner(const FString& Msg) const
{
	UE_LOG(LogAFLMovement, Display, TEXT("AFL_TEST[%s]: %s"), *NetTag(), *Msg);
#if !(UE_BUILD_SHIPPING)
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Cyan, FString::Printf(TEXT("AFL_TEST[%s]  %s"), *NetTag(), *Msg));
	}
#endif
}

FString UAFLInteractionTestHarness::NetTag() const
{
	// Net-mode short tag + PIE instance id so a 2-window run's interleaved single log is orderable by eye
	// and greppable per side (the AFLSkinColorComponent two-client log precedent).
	const UWorld* W = WorldPtr.Get();
	const TCHAR* Mode = TEXT("SA");
	if (W)
	{
		switch (W->GetNetMode())
		{
		case NM_ListenServer:    Mode = TEXT("SV"); break;
		case NM_DedicatedServer: Mode = TEXT("DS"); break;
		case NM_Client:          Mode = TEXT("CL"); break;
		default:                 Mode = TEXT("SA"); break;
		}
	}
	int32 PieId = -1;
#if WITH_EDITOR
	if (W && W->GetOutermost())
	{
		PieId = W->GetOutermost()->GetPIEInstanceID();
	}
#endif
	return (PieId >= 0) ? FString::Printf(TEXT("%s%d"), Mode, PieId) : FString(Mode);
}

void UAFLInteractionTestHarness::LogObjectPos(const TCHAR* Context, AActor* Target) const
{
	// Cross-log convergence line: the [SV] FSM and the [CL] observer both emit these for released objects;
	// the post-PIE log read compares the pair within tolerance (the third leg beside observer settle +
	// operator visuals).
	if (Target)
	{
		const FVector L = Target->GetActorLocation();
		UE_LOG(LogAFLMovement, Display, TEXT("AFL_TEST[%s] POS %s %s=(%.0f, %.0f, %.0f)"),
			*NetTag(), Context, *GetNameSafe(Target), L.X, L.Y, L.Z);
	}
}

// ---------------------------------------------------------------------------------------------------
// console registration (the afl.HandIK.* pattern: dotted world-args command, fires regardless of
// cheat-manager state, resolves the typed-in window's world)
// ---------------------------------------------------------------------------------------------------

#if UE_WITH_CHEAT_MANAGER
namespace
{
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLInteractionTestRunCmd(
		TEXT("afl.Interaction.Test.Run"),
		TEXT("Stress-object cycle: scripted P0-P4 interaction test (grab/drop/throw/damage verbs through the real input + ExecCalc pipelines, ~1.5s pacing). 2-client: run in the HOST window. Watch the pawn; stop PIE after AFL_TEST COMPLETE."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				Ar.Log(TEXT("afl.Interaction.Test.Run -- starting (see AFL_TEST lines in LogAFLMovement)."));
				UAFLInteractionTestHarness::RunInWorld(World);
			}));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLInteractionTestObserveCmd(
		TEXT("afl.Interaction.Test.Observe"),
		TEXT("2-client cycle 1: READ-ONLY observer for the CLIENT window. Watches the remote (host) player for 75s and asserts what crossed the wire: State.Carrying tag, attach-parent + ride distance, replicated bHeld coherence, post-release settle. No teleports, no writes. Run alongside afl.Interaction.Test.Run on the host."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				Ar.Log(TEXT("afl.Interaction.Test.Observe -- observer starting (see AFL_TEST[CLn] lines in LogAFLMovement)."));
				UAFLInteractionTestHarness::RunObserverInWorld(World);
			}));
}
#endif // UE_WITH_CHEAT_MANAGER
