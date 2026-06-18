// Copyright C12 AI Gaming. All Rights Reserved.

#include "Loot/AFLLootCarryComponent.h"

#include "AFLCombat.h"
#include "Character/LyraHealthComponent.h"
#include "Cosmetics/AFLWalletComponent.h"     // the BANKED twin -- extract banks the at-risk pool into it
#include "Engine/World.h"
#include "GameFramework/CheatManagerDefines.h" // UE_WITH_CHEAT_MANAGER -- without it the #if reads 0 + the cmd compiles out SILENTLY
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"     // GetFirstPlayerController (the gate-4 dev cheat)
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"                // FAutoConsoleCommandWithWorldArgsAndOutputDevice
#include "Loot/AFLLootCarryPickup.h"
#include "Messages/AFLHitConfirmMessage.h"     // FAFLHitConfirmMessage (AFLCore damage verb)
#include "Messages/LyraVerbMessage.h"          // FLyraVerbMessage (the extraction broadcast payload)
#include "Net/UnrealNetwork.h"                 // DOREPLIFETIME
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLLootCarryComponent)

// The two hooks Phase A subscribes to -- both already broadcast by shipped code (UAFLDamageExecCalc /
// UAFLAG_Extract). Native-define per the file convention so module init never races the tag-ini scan.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Damage_Confirmed_Carry, "Event.Damage.Confirmed");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_Complete_Carry, "Event.Extraction.Complete");

static TAutoConsoleVariable<float> CVarAFLLootDropPercent(
	TEXT("afl.Loot.DropPercent"),
	33.0f,
	TEXT("Percent of the carried-loot pool scattered on each confirmed hit (death scatters the remainder)."));

UAFLLootCarryComponent::UAFLLootCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Mirror the wallet rail: a plain UActorComponent has no replicated base -> WE enable replication or
	// CarriedValue never reaches clients (the wallet's "compiles but doesn't replicate" trap). Pool changes
	// are collect/scatter/extract-rare -> no tick.
	SetIsReplicatedByDefault(true);

	// Default the scatter pickup to the Phase-A pickup (same module; a BP child can override).
	ScatterPickupClass = AAFLLootCarryPickup::StaticClass();
}

void UAFLLootCarryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAFLLootCarryComponent, CarriedValue);
}

void UAFLLootCarryComponent::Collect(int32 Value)
{
	AActor* Owner = GetOwner();
	if (Value <= 0 || !Owner || !Owner->HasAuthority())
	{
		return;
	}
	CommitCarriedDelta(+Value);
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRY: %s collected +%d -> pool %d"),
		*GetNameSafe(Owner), Value, CarriedValue);
}

void UAFLLootCarryComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;   // pool mutation + scatter are authority ops; the int replicates down to clients via OnRep
	}

	// Death -> scatter the remainder (the same OnDeathStarted the head loot-box's owner-death-vanish uses).
	if (ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Owner))
	{
		Health->OnDeathStarted.AddDynamic(this, &UAFLLootCarryComponent::HandleDeathStarted);
	}

	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem& Messages = UGameplayMessageSubsystem::Get(World);
		// Confirmed hit on me -> scatter a portion (the Event.Damage.Confirmed the carry-drop consumes).
		DamageListenerHandle = Messages.RegisterListener<FAFLHitConfirmMessage>(TAG_Event_Damage_Confirmed_Carry,
			[this](FGameplayTag C, const FAFLHitConfirmMessage& M) { HandleDamageConfirmed(C, M); });
		// I completed an extraction -> bank the pool to Watts (UAFLAG_Extract already broadcasts this).
		ExtractListenerHandle = Messages.RegisterListener<FLyraVerbMessage>(TAG_Event_Extraction_Complete_Carry,
			[this](FGameplayTag C, const FLyraVerbMessage& M) { HandleExtractionComplete(C, M); });
	}
}

void UAFLLootCarryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* Owner = GetOwner())
	{
		if (ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Owner))
		{
			Health->OnDeathStarted.RemoveDynamic(this, &UAFLLootCarryComponent::HandleDeathStarted);
		}
	}
	if (DamageListenerHandle.IsValid())
	{
		DamageListenerHandle.Unregister();
	}
	if (ExtractListenerHandle.IsValid())
	{
		ExtractListenerHandle.Unregister();
	}
	Super::EndPlay(EndPlayReason);
}

void UAFLLootCarryComponent::HandleDamageConfirmed(FGameplayTag /*Channel*/, const FAFLHitConfirmMessage& Message)
{
	if (Message.Target != GetOwner())
	{
		return;   // someone else's hit
	}
	const int32 Pool = CarriedValue;
	if (Pool <= 0)
	{
		return;
	}
	const float Pct = FMath::Clamp(CVarAFLLootDropPercent.GetValueOnGameThread(), 0.0f, 100.0f);
	const int32 Drop = FMath::Clamp(FMath::RoundToInt(Pool * Pct / 100.0f), 0, Pool);
	if (Drop > 0)
	{
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRY: %s hit -> scatter %d/%d (%.0f%%)"),
			*GetNameSafe(GetOwner()), Drop, Pool, Pct);
		ScatterValue(Drop);
	}
}

void UAFLLootCarryComponent::HandleDeathStarted(AActor* /*OwningActor*/)
{
	const int32 Pool = CarriedValue;
	if (Pool > 0)
	{
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRY: %s died -> scatter remainder %d"),
			*GetNameSafe(GetOwner()), Pool);
		ScatterValue(Pool);
	}
}

void UAFLLootCarryComponent::HandleExtractionComplete(FGameplayTag /*Channel*/, const FLyraVerbMessage& Message)
{
	if (Message.Instigator != GetOwner())
	{
		return;   // someone else's extraction
	}
	const int32 Pool = CarriedValue;
	if (Pool <= 0)
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerState* PS = Pawn ? Pawn->GetPlayerState() : nullptr;
	UAFLWalletComponent* Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
	if (Wallet)
	{
		// Bank the at-risk pool into the banked wallet (the at-risk -> banked bridge), then zero the pool.
		Wallet->EarnWattsAuthority(Pool, TEXT("loot-extract"));
		CommitCarriedDelta(-Pool);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRY: %s extracted -> +%d Watts, pool cleared"),
			*GetNameSafe(GetOwner()), Pool);
	}
	else
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_LOOTCARRY: %s extracted but no wallet found -- pool %d not banked"),
			*GetNameSafe(GetOwner()), Pool);
	}
}

void UAFLLootCarryComponent::ScatterValue(int32 Amount)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}
	Amount = FMath::Min(Amount, CarriedValue);
	if (Amount <= 0)
	{
		return;
	}

	// Remove from the pool FIRST (authoritative), then spawn recoverable pickups summing to Amount.
	CommitCarriedDelta(-Amount);

	if (!ScatterPickupClass)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_LOOTCARRY: no ScatterPickupClass -- %d removed but not scattered"), Amount);
		return;
	}
	const int32 ChunkSize = 50;
	const int32 K = FMath::Clamp(FMath::CeilToInt(Amount / static_cast<float>(ChunkSize)), 1, 8);
	const FVector Base = Owner->GetActorLocation();
	int32 Remaining = Amount;
	for (int32 i = 0; i < K && Remaining > 0; ++i)
	{
		const int32 Per = (i == K - 1) ? Remaining : FMath::Max(1, Amount / K);
		Remaining -= Per;
		const FVector Loc = Base + FVector(FMath::RandRange(-150.0f, 150.0f), FMath::RandRange(-150.0f, 150.0f), 40.0f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AAFLLootCarryPickup* Pickup = World->SpawnActor<AAFLLootCarryPickup>(*ScatterPickupClass, Loc, FRotator::ZeroRotator, Params))
		{
			Pickup->SetValue(Per);
		}
	}
}

void UAFLLootCarryComponent::CommitCarriedDelta(int32 Delta)
{
	// The single authority commit point (mirrors UAFLWalletComponent::CommitMutation, minus persistence -- the
	// at-risk pool is ephemeral by design). Listen-host applies locally here (OnRep does not fire on authority)
	// -> broadcast for the host HUD too; remote clients fire the same broadcast from OnRep_CarriedValue.
	CarriedValue = FMath::Max(0, CarriedValue + Delta);
	OnCarriedValueChanged.Broadcast(CarriedValue);
}

void UAFLLootCarryComponent::OnRep_CarriedValue()
{
	// Remote clients: the pool replicated in -- the owner's HUD updates from here (event-driven, mirrors the
	// wallet's OnRep_Balance).
	OnCarriedValueChanged.Broadcast(CarriedValue);
}

#if UE_WITH_CHEAT_MANAGER
namespace
{
	// Phase A gate-4 proof in ISOLATION. The real extraction zone is match-phase-gated (it dispenses only while
	// AFL.GamePhase.Playing.ExtractionWindow is open), so this dev cheat broadcasts the SAME message
	// UAFLAG_Extract sends on a real channel-complete (AFLAG_Extract.cpp:244 -- FLyraVerbMessage with
	// Verb + Instigator = the host hero) -> UAFLLootCarryComponent::HandleExtractionComplete banks the carried
	// pool to Watts + zeroes it. Proves the listener hook without driving the match spine. Remove post-Phase-A.
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLLootTestExtractCmd(
		TEXT("afl.Loot.TestExtract"),
		TEXT("Phase A: broadcast Event.Extraction.Complete for the host hero -> banks the carried loot pool to Watts (proves the extract hook; no match window needed). HOST only."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				if (!World || World->GetNetMode() == NM_Client)
				{
					Ar.Log(TEXT("afl.Loot.TestExtract -- HOST window only (the extraction broadcast is an authority op)."));
					return;
				}
				APlayerController* PC = World->GetFirstPlayerController();
				APawn* Pawn = PC ? PC->GetPawn() : nullptr;
				if (!Pawn)
				{
					Ar.Log(TEXT("afl.Loot.TestExtract -- no possessed pawn."));
					return;
				}
				// Mirror UAFLAG_Extract::BroadcastExtractionEvent exactly (Verb + Instigator = the channeling pawn).
				FLyraVerbMessage Message;
				Message.Verb = TAG_Event_Extraction_Complete_Carry;
				Message.Instigator = Pawn;
				UGameplayMessageSubsystem::Get(World).BroadcastMessage(Message.Verb, Message);
				Ar.Logf(TEXT("afl.Loot.TestExtract -- broadcast Event.Extraction.Complete for %s (watch AFL_LOOTCARRY)."), *Pawn->GetName());
			}));
}
#endif // UE_WITH_CHEAT_MANAGER
