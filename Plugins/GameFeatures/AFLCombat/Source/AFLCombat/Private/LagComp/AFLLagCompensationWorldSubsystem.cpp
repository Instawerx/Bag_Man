// Copyright C12 AI Gaming. All Rights Reserved.

#include "LagComp/AFLLagCompensationWorldSubsystem.h"

#include "AFLCombat.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "LagComp/AFLPawnHitboxHistoryComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLLagCompensationWorldSubsystem)


bool FAFLLagRewindToken::QueryBoneTransform(const AActor* Target, FName BoneName, FTransform& OutWorldXForm) const
{
	if (!bValid || !Target)
	{
		return false;
	}
	for (const FAFLRewindPawnEntry& Entry : Entries)
	{
		if (Entry.Pawn.Get() != Target)
		{
			continue;
		}
		for (const FAFLHitboxBoneSample& Sample : Entry.RewoundBones)
		{
			if (Sample.Bone == BoneName)
			{
				OutWorldXForm = Sample.WorldXForm;
				return true;
			}
		}
		return false;
	}
	return false;
}

bool FAFLLagRewindToken::BuildBoundingBox(const AActor* Target, FBox& OutBox) const
{
	if (!bValid || !Target)
	{
		return false;
	}
	for (const FAFLRewindPawnEntry& Entry : Entries)
	{
		if (Entry.Pawn.Get() != Target)
		{
			continue;
		}
		if (Entry.RewoundBones.Num() == 0)
		{
			return false;
		}
		// Build from bone world locations. The bounding-box check in master
		// doc Sec. 7.4 expands this by a fixed 30cm pad for rig accuracy; we
		// leave the pad to the caller so the box stays a faithful snapshot
		// of the rewound rig.
		OutBox = FBox(ForceInit);
		for (const FAFLHitboxBoneSample& Sample : Entry.RewoundBones)
		{
			OutBox += Sample.WorldXForm.GetLocation();
		}
		return OutBox.IsValid != 0;
	}
	return false;
}


UAFLLagCompensationWorldSubsystem::UAFLLagCompensationWorldSubsystem() = default;

bool UAFLLagCompensationWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	if (!World)
	{
		return false;
	}
	// Editor / preview / inactive worlds never simulate pawns, so the
	// registry is wasted memory there.
	switch (World->WorldType)
	{
		case EWorldType::Game:
		case EWorldType::PIE:
			return true;
		default:
			return false;
	}
}

void UAFLLagCompensationWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogAFLCombat, Verbose, TEXT("AFL_LAGCOMP: subsystem initialized for world %s"),
		*GetNameSafe(GetWorld()));
}

void UAFLLagCompensationWorldSubsystem::Deinitialize()
{
	RegisteredComponents.Reset();
	Super::Deinitialize();
}

void UAFLLagCompensationWorldSubsystem::RegisterComponent(UAFLPawnHitboxHistoryComponent* Component)
{
	if (!Component)
	{
		return;
	}
	for (const TWeakObjectPtr<UAFLPawnHitboxHistoryComponent>& Existing : RegisteredComponents)
	{
		if (Existing.Get() == Component)
		{
			return;
		}
	}
	RegisteredComponents.Add(Component);
}

void UAFLLagCompensationWorldSubsystem::UnregisterComponent(UAFLPawnHitboxHistoryComponent* Component)
{
	if (!Component)
	{
		return;
	}
	RegisteredComponents.RemoveAllSwap(
		[Component](const TWeakObjectPtr<UAFLPawnHitboxHistoryComponent>& Existing)
		{
			return !Existing.IsValid() || Existing.Get() == Component;
		});
}

FAFLLagRewindToken UAFLLagCompensationWorldSubsystem::RewindWorldFor(APlayerController* RequestingPC, float ServerTime)
{
	FAFLLagRewindToken Token;
	Token.bValid = true;
	Token.SampledServerTime = static_cast<double>(ServerTime);

	// Hard server gate. Lag comp is server-only — returning a valid-but-empty
	// token lets shared code call this without an IsServer guard and still
	// scope-guard the matching RestoreWorld.
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return Token;
	}

	const APawn* ExcludedPawn = RequestingPC ? RequestingPC->GetPawn() : nullptr;

	Token.Entries.Reserve(RegisteredComponents.Num());

	// Sweep dangling weak ptrs as we go to keep the registry tight.
	for (int32 Idx = RegisteredComponents.Num() - 1; Idx >= 0; --Idx)
	{
		UAFLPawnHitboxHistoryComponent* Component = RegisteredComponents[Idx].Get();
		if (!Component)
		{
			RegisteredComponents.RemoveAtSwap(Idx);
			continue;
		}

		const AActor* Owner = Component->GetOwner();
		if (!Owner || Owner == ExcludedPawn)
		{
			continue;
		}

		TArray<FAFLHitboxBoneSample> Samples;
		if (!Component->SampleAtTime(static_cast<double>(ServerTime), Samples))
		{
			// No history yet (just spawned) or component is on a client.
			continue;
		}

		FAFLRewindPawnEntry Entry;
		Entry.Pawn          = Owner;
		Entry.Component     = Component;
		Entry.RewoundBones  = MoveTemp(Samples);
		Token.Entries.Add(MoveTemp(Entry));
	}

	return Token;
}

void UAFLLagCompensationWorldSubsystem::RestoreWorld(FAFLLagRewindToken& Token)
{
	// AFL-0211 rewind is non-mutating, so this does not touch the live world.
	// Invalidating the token catches double-restores and keeps the scope-
	// guard pattern meaningful. A future mutating-rewind impl can drop into
	// this entry point without changing call sites.
	if (!Token.bValid)
	{
		return;
	}
	Token.Entries.Reset();
	Token.bValid = false;
}
