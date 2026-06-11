// Copyright C12 AI Gaming. All Rights Reserved.

#include "Energy/AFLEnergyDropComponent.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/AFLAttributeSet_Energy.h"
#include "Character/LyraHealthComponent.h"
#include "Effects/GE_AFL_EnergyGain_Small.h"
#include "Energy/AFLEnergyPickup.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLEnergyDropComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_Energy_Gain_Drop, "Data.Energy.Gain");

// Tunable drop fraction (the S7 spec's 70%). Cvar so the death-economy feel pass iterates
// without a rebuild; the overload-mode re-skin will reuse it.
static TAutoConsoleVariable<float> CVarAFLEnergyDropPercent(
	TEXT("afl.Energy.DropPercent"),
	70.0f,
	TEXT("Percent of CarriedEnergy dropped as pickups on death (0-100)."));


UAFLEnergyDropComponent::UAFLEnergyDropComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // pure listener.
}

void UAFLEnergyDropComponent::BeginPlay()
{
	Super::BeginPlay();

	// Server-only logic; the AddComponents row is server-flagged too (belt and braces).
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}
	// The AFLTargetDummy precedent: bind Lyra's pawn-local death delegate -- fires when
	// UAFLDeathComponent drives StartDeath, no ASC-resolution race, no death-component coupling.
	if (ULyraHealthComponent* HealthComponent = ULyraHealthComponent::FindHealthComponent(Owner))
	{
		HealthComponent->OnDeathStarted.AddDynamic(this, &UAFLEnergyDropComponent::HandleDeathStarted);
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_ENERGY: %s drop component armed (OnDeathStarted bound)."), *GetNameSafe(Owner));
	}
	else
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_ENERGY: %s has no ULyraHealthComponent -- death burst unbound."), *GetNameSafe(Owner));
	}
}

void UAFLEnergyDropComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* Owner = GetOwner())
	{
		if (ULyraHealthComponent* HealthComponent = ULyraHealthComponent::FindHealthComponent(Owner))
		{
			HealthComponent->OnDeathStarted.RemoveDynamic(this, &UAFLEnergyDropComponent::HandleDeathStarted);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UAFLEnergyDropComponent::HandleDeathStarted(AActor* OwningActor)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bBurstDone)
	{
		return;
	}
	bBurstDone = true;

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	if (!ASC)
	{
		return;
	}

	const float Carried = ASC->GetNumericAttribute(UAFLAttributeSet_Energy::GetCarriedEnergyAttribute());
	const float Percent = FMath::Clamp(CVarAFLEnergyDropPercent.GetValueOnGameThread(), 0.0f, 100.0f);
	const float DropTarget = Carried * Percent * 0.01f;
	if (DropTarget < 1.0f)
	{
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_ENERGY: %s died carrying %.1f -- nothing to drop."), *GetNameSafe(Owner), Carried);
		return;
	}

	// Tier distribution, smalls-favored: at most coarse tiers for the bulk, ceil the remainder in
	// smalls so the dropped TOTAL never undershoots the target (ceil-distributed).
	int32 NumLarge = 0, NumMedium = 0, NumSmall = 0;
	float Remaining = DropTarget;
	while (Remaining >= 50.0f) { ++NumLarge;  Remaining -= 50.0f; }
	while (Remaining >= 25.0f) { ++NumMedium; Remaining -= 25.0f; }
	NumSmall = FMath::CeilToInt(Remaining / 10.0f);
	const float DroppedTotal = NumLarge * 50.0f + NumMedium * 25.0f + NumSmall * 10.0f;

	// Ring scatter at the corpse (spawn-position scatter; a physics scatter impulse is a later
	// feel item -- the magnet dominates motion within a second anyway).
	UWorld* World = Owner->GetWorld();
	const FVector Center = Owner->GetActorLocation();
	const int32 Total = NumLarge + NumMedium + NumSmall;
	int32 SpawnedCount = 0;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	auto SpawnTier = [&](TSubclassOf<AAFLEnergyPickup> TierClass, int32 Count)
	{
		for (int32 i = 0; i < Count && World; ++i)
		{
			const float Angle = (2.0f * PI) * (static_cast<float>(SpawnedCount) / FMath::Max(1, Total));
			const FVector Loc = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * ScatterRadius + FVector(0, 0, 30.0f);
			// Tier classes come from the BP child component; the raw C++ pickup is the visible fallback
			// so an unconfigured grant still functions (and the harness can assert on it).
			UClass* SpawnClass = TierClass ? *TierClass : AAFLEnergyPickup::StaticClass();
			if (World->SpawnActor<AAFLEnergyPickup>(SpawnClass, Loc, FRotator::ZeroRotator, Params))
			{
				++SpawnedCount;
			}
		}
	};
	SpawnTier(LargePickupClass, NumLarge);
	SpawnTier(MediumPickupClass, NumMedium);
	SpawnTier(SmallPickupClass, NumSmall);

	// Victim reduction THROUGH the rail: the same gain GE with a NEGATIVE SetByCaller magnitude.
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddInstigator(Owner, Owner);
		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(UGE_AFL_EnergyGain_Small::StaticClass(), 1.0f, Context);
		if (SpecHandle.IsValid())
		{
			SpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_Energy_Gain_Drop, -DroppedTotal);
			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}

	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_ENERGY: death burst by %s -- carried %.1f, dropped %.1f as %dL/%dM/%dS (%d pickups, %.0f%%)."),
		*GetNameSafe(Owner), Carried, DroppedTotal, NumLarge, NumMedium, NumSmall, SpawnedCount, Percent);
}
