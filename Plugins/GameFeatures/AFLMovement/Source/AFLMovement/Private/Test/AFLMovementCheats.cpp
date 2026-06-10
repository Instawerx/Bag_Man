// Copyright C12 AI Gaming. All Rights Reserved.

#include "Test/AFLMovementCheats.h"

#include "AFLMovement.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CheatManagerDefines.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/AFLInteractionComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLMovementCheats)

// ─────────────────────────────────────────────────────────────────────────────
// afl.HandIK.* console commands.
//
// THESE ARE DOTTED FAutoConsoleCommandWithWorldArgsAndOutputDevice, **not**
// UFUNCTION(Exec) -- the exact lesson AFLCombatCheats already paid for (see the
// big WHY comment by afl.Cosmetic.SetEdge): a UFUNCTION(Exec) named "Foo" must be
// typed BARE ("Foo 1 2 3", no prefix) AND only routes when Lyra's cheat manager
// is active. Typing "afl.SetHandIKTarget" against an Exec routes NOWHERE and
// silently no-ops -- which is precisely why the hand never moved across three PIE
// sessions (the function was never called; zero AFL_HANDIK log lines).
//
// The world-args delegate fires regardless of cheat-manager state and hands us the
// UWorld of the PIE window the command was typed in, so we resolve THAT window's
// local player and its pawn-side UAFLInteractionComponent. Consistent with the
// operator's afl.* muscle memory (afl.GroundTruth / afl.Wallet.* / afl.Cosmetic.*).
// ─────────────────────────────────────────────────────────────────────────────

#if UE_WITH_CHEAT_MANAGER

namespace
{
	// Resolve the local player's hero UAFLInteractionComponent from the typed-in window's world. The
	// component is added to the hero PAWN via the experience's GameFeatureAction_AddComponents (the proven
	// dash/climb delivery path), so we reach pawn-first.
	UAFLInteractionComponent* FindHeroInteractionComponent(UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.HandIK -- no game world (run inside PIE)."));
			return nullptr;
		}
		APlayerController* PC = World->GetFirstPlayerController();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		UAFLInteractionComponent* Interaction = Pawn ? Pawn->FindComponentByClass<UAFLInteractionComponent>() : nullptr;
		if (!Interaction)
		{
			Ar.Log(TEXT("afl.HandIK -- no UAFLInteractionComponent on the local pawn (possessed hero with the AFL interaction component required)."));
		}
		return Interaction;
	}

	void HandleAFLHandIKSet(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 3)
		{
			Ar.Log(TEXT("afl.HandIK.Set -- usage: afl.HandIK.Set <X> <Y> <Z>  (world-space cm; e.g. afl.HandIK.Set 200 0 120)"));
			return;
		}
		UAFLInteractionComponent* Interaction = FindHeroInteractionComponent(World, Ar);
		if (!Interaction)
		{
			return;
		}

		const FVector Target(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), FCString::Atof(*Args[2]));
		Interaction->HandIKTarget = Target;
		Interaction->bHandIKEnabled = true;
		Interaction->HandIKAlpha = 1.0f;

		// Logged via LogAFLMovement (proven category) AND echoed to the console device so the operator sees
		// confirmation in-window even if the log is filtered.
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_HANDIK: target set %s, enabled (alpha=1) on %s."),
			*Target.ToCompactString(), *GetNameSafe(Interaction->GetOwner()));
		Ar.Logf(TEXT("afl.HandIK.Set -- target=%s enabled on %s. The component's TickComponent now drives CR_AFL_IRONICS; watch the right hand."),
			*Target.ToCompactString(), *GetNameSafe(Interaction->GetOwner()));
	}

	void HandleAFLHandIKClear(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		UAFLInteractionComponent* Interaction = FindHeroInteractionComponent(World, Ar);
		if (!Interaction)
		{
			return;
		}
		Interaction->bHandIKEnabled = false;
		Interaction->HandIKAlpha = 0.0f;

		UE_LOG(LogAFLMovement, Log, TEXT("AFL_HANDIK: cleared (disabled, alpha=0) on %s."),
			*GetNameSafe(Interaction->GetOwner()));
		Ar.Logf(TEXT("afl.HandIK.Clear -- disabled on %s. Hand returns to the clip pose next tick."),
			*GetNameSafe(Interaction->GetOwner()));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLHandIKSetCmd(
		TEXT("afl.HandIK.Set"),
		TEXT("Cycle 4c hand-IK isolation: set the right-hand world-space IK target + enable it (alpha=1) on the local hero's UAFLInteractionComponent. Usage: afl.HandIK.Set <X> <Y> <Z> (cm). Run in PIE."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLHandIKSet));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLHandIKClearCmd(
		TEXT("afl.HandIK.Clear"),
		TEXT("Cycle 4c hand-IK isolation: disable the right-hand IK (alpha=0); the hand returns to the clip/slot pose. Run in PIE."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLHandIKClear));
}

#endif // UE_WITH_CHEAT_MANAGER

// The UAFLMovementCheats CheatManagerExtension shell is retained (no Exec functions now -- the IK cheats
// moved to the afl.HandIK.* console commands above). Kept as the home for any future movement cheat and to
// preserve the registration parity with UAFLCombatCheats / ULyraCosmeticCheats.
UAFLMovementCheats::UAFLMovementCheats()
{
#if UE_WITH_CHEAT_MANAGER
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		UCheatManager::RegisterForOnCheatManagerCreated(FOnCheatManagerCreated::FDelegate::CreateLambda(
			[](UCheatManager* CheatManager)
			{
				CheatManager->AddCheatManagerExtension(NewObject<ThisClass>(CheatManager));
			}));
	}
#endif
}
