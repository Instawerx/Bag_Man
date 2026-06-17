// Copyright C12 AI Gaming. All Rights Reserved.

#include "Test/AFLLootTestRunner.h"

#include "AFLCombat.h"                       // LogAFLCombat
#include "Cosmetics/AFLWalletComponent.h"
#include "Loot/AFLLootGrantComponent.h"
#include "Loot/AFLLootTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLLootTestRunner)

void UAFLLootTestRunner::OnOwnerRetrievedProbe(AActor* Retriever)
{
	++OwnerRetrievedCount;
}

void UAFLLootTestRunner::OnLootGrantedProbe(AActor* Retriever, int32 Value)
{
	++LootGrantedCount;
}

void UAFLLootTestRunner::Check(bool bPass, const FString& What)
{
	++ChecksTotal;
	if (!bPass)
	{
		++ChecksFailed;
	}
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTRUN %s -- %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), *What);
}

void UAFLLootTestRunner::RunInWorld(UWorld* World)
{
	if (!World)
	{
		return;
	}

	// HOST/authority: the grant + EarnWattsAuthority are authority ops (mirrors the Extract runner's gate).
	APlayerController* PC = World->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || !Pawn->HasAuthority())
	{
		UE_LOG(LogAFLCombat, Error,
			TEXT("AFL_LOOTRUN: run on the HOST with a possessed pawn (grant/Watts are authority ops)."));
		return;
	}
	APlayerState* PS = Pawn->GetPlayerState();
	UAFLWalletComponent* Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
	if (!Wallet)
	{
		UE_LOG(LogAFLCombat, Error,
			TEXT("AFL_LOOTRUN: no UAFLWalletComponent on the local PlayerState (experience row)."));
		return;
	}

	// Transient host actor (provides GetOwner()+authority for the grant component's TryGrant) + a stand-in
	// "other owner" (no controller) so the local pawn reads as a NON-owner in the enemy test.
	AActor* HostActor = World->SpawnActor<AActor>();
	AActor* OtherOwner = World->SpawnActor<AActor>();
	if (!HostActor || !OtherOwner)
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_LOOTRUN: failed to spawn the transient test actors."));
		if (HostActor) { HostActor->Destroy(); }
		if (OtherOwner) { OtherOwner->Destroy(); }
		return;
	}

	UAFLLootTestRunner* Probe = NewObject<UAFLLootTestRunner>(GetTransientPackage());
	Probe->AddToRoot();   // keep alive across the synchronous run (it owns the delegate-fire counters)

	// NON-DESTRUCTIVE: snapshot the wallet, restore it at the end so the run leaves the balance untouched.
	const int32 Volts0 = Wallet->GetVolts();
	const int32 Watts0 = Wallet->GetWatts();

	auto MakeComp = [HostActor]() -> UAFLLootGrantComponent*
	{
		UAFLLootGrantComponent* C = NewObject<UAFLLootGrantComponent>(HostActor);
		C->RegisterComponent();
		return C;
	};

	// -- T1 Anyone: grant + the EXACT wallet delta (the local pawn IS the retriever, has a real wallet). --
	// -- T2 grant-once: a second TryGrant on the now-spent component is a no-op (the double-pay guard). --
	{
		UAFLLootGrantComponent* C = MakeComp();
		C->OnLootGranted.AddDynamic(Probe, &UAFLLootTestRunner::OnLootGrantedProbe);
		C->OnOwnerRetrieved.AddDynamic(Probe, &UAFLLootTestRunner::OnOwnerRetrievedProbe);
		C->Configure(EAFLLootValueModel::Watts, 100, EAFLLootEligibility::Anyone, nullptr, TEXT("test-anyone"));

		const int32 WBefore = Wallet->GetWatts();
		const int32 GrantsBefore = Probe->LootGrantedCount;
		const bool bGranted = C->TryGrant(Pawn);
		Probe->Check(bGranted, TEXT("T1 Anyone: TryGrant granted"));
		Probe->Check(Wallet->GetWatts() == WBefore + 100,
			FString::Printf(TEXT("T1 Anyone: wallet +100 (was %d, now %d)"), WBefore, Wallet->GetWatts()));
		Probe->Check(C->IsSpent(), TEXT("T1 Anyone: IsSpent after grant"));
		Probe->Check(Probe->LootGrantedCount == GrantsBefore + 1, TEXT("T1 Anyone: OnLootGranted fired once"));

		const int32 WAfterFirst = Wallet->GetWatts();
		const bool bSecond = C->TryGrant(Pawn);
		Probe->Check(!bSecond, TEXT("T2 grant-once: second TryGrant did NOT grant"));
		Probe->Check(Wallet->GetWatts() == WAfterFirst, TEXT("T2 grant-once: wallet unchanged on re-grab (no double-pay)"));
		C->DestroyComponent();
	}

	// -- T3 owner-seam: EnemyOnly + the OWNER (same controller) retrieving -> OnOwnerRetrieved fires, NO grant,
	//    NOT spent, wallet flat. This is the real owner case (the local pawn is its own owner). --
	{
		UAFLLootGrantComponent* C = MakeComp();
		C->OnLootGranted.AddDynamic(Probe, &UAFLLootTestRunner::OnLootGrantedProbe);
		C->OnOwnerRetrieved.AddDynamic(Probe, &UAFLLootTestRunner::OnOwnerRetrievedProbe);
		C->Configure(EAFLLootValueModel::Watts, 100, EAFLLootEligibility::EnemyOnly, Pawn /*owner = local pawn*/, TEXT("test-owner"));

		const int32 WBefore = Wallet->GetWatts();
		const int32 OwnerBefore = Probe->OwnerRetrievedCount;
		const int32 GrantsBefore = Probe->LootGrantedCount;
		const bool bGranted = C->TryGrant(Pawn /*= owner, same controller*/);
		Probe->Check(!bGranted, TEXT("T3 owner-seam: TryGrant did NOT grant (owner reattaches, no loot)"));
		Probe->Check(Probe->OwnerRetrievedCount == OwnerBefore + 1, TEXT("T3 owner-seam: OnOwnerRetrieved fired once (the reattach seam)"));
		Probe->Check(Probe->LootGrantedCount == GrantsBefore, TEXT("T3 owner-seam: OnLootGranted did NOT fire"));
		Probe->Check(!C->IsSpent(), TEXT("T3 owner-seam: NOT spent (the owner did not consume it)"));
		Probe->Check(Wallet->GetWatts() == WBefore, TEXT("T3 owner-seam: wallet unchanged (no Watts to the owner)"));
		C->DestroyComponent();
	}

	// -- T4 enemy: EnemyOnly + a NON-owner retriever -> grant + wallet delta. (OtherOwner has no controller,
	//    so the local pawn is not its owner -> eligible.) --
	{
		UAFLLootGrantComponent* C = MakeComp();
		C->OnLootGranted.AddDynamic(Probe, &UAFLLootTestRunner::OnLootGrantedProbe);
		C->Configure(EAFLLootValueModel::Watts, 100, EAFLLootEligibility::EnemyOnly, OtherOwner /*owner = someone else*/, TEXT("test-enemy"));

		const int32 WBefore = Wallet->GetWatts();
		const bool bGranted = C->TryGrant(Pawn /*not the owner -> enemy*/);
		Probe->Check(bGranted, TEXT("T4 enemy: TryGrant granted (a non-owner loots)"));
		Probe->Check(Wallet->GetWatts() == WBefore + 100, TEXT("T4 enemy: wallet +100 to the non-owner"));
		C->DestroyComponent();
	}

	// Restore the wallet (non-destructive) + summarize.
	Wallet->DebugSetBalance(Volts0, Watts0);
	const int32 Passed = Probe->ChecksTotal - Probe->ChecksFailed;
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTRUN: %s %d/%d -- wallet restored to %d W."),
		Probe->ChecksFailed == 0 ? TEXT("PASS") : TEXT("FAIL"), Passed, Probe->ChecksTotal, Watts0);

	Probe->RemoveFromRoot();
	HostActor->Destroy();
	OtherOwner->Destroy();
}

namespace
{
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLLootTestRunCmd(
		TEXT("afl.Loot.Test.Run"),
		TEXT("Loot Phase 1: deterministically assert the UAFLLootGrantComponent decision flow (grant-once, "
			 "owner-seam, eligibility) + the real wallet delta. HOST/authority. -> AFL_LOOTRUN: PASS N/N (wallet restored)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				Ar.Log(TEXT("afl.Loot.Test.Run -- starting (see AFL_LOOTRUN lines in LogAFLCombat)."));
				UAFLLootTestRunner::RunInWorld(World);
			}));
}
