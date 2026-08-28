#include "Cosmetics/AFLPreviewRigSubsystem.h"

#include "EngineUtils.h"
#include "UI/AFLLoadoutDisplayPawn.h"
#include "UI/AFLLoadoutPod.h"

DEFINE_LOG_CATEGORY_STATIC(LogAFLPreviewRig, Log, All);

bool UAFLPreviewRigSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

AAFLLoadoutDisplayPawn* UAFLPreviewRigSubsystem::AcquireDisplayPawn(
	TSubclassOf<AAFLLoadoutDisplayPawn> PreferredClass, const FVector& SpawnLocation)
{
	if (SharedPawn.IsValid())
	{
		return SharedPawn.Get();
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Adopt-first: exactly ONE transient display pawn may live; every surplus is a stray from the
	// pre-rig era (or a logic error) and is destroyed LOUDLY. Level-placed set-piece pawns (startup
	// actors) are not the rig's -- left alone.
	for (TActorIterator<AAFLLoadoutDisplayPawn> It(World); It; ++It)
	{
		AAFLLoadoutDisplayPawn* Candidate = *It;
		if (!IsValid(Candidate) || Candidate->IsNetStartupActor())
		{
			continue;
		}
		if (!SharedPawn.IsValid())
		{
			SharedPawn = Candidate;
			UE_LOG(LogAFLPreviewRig, Log, TEXT("AFL_C1[Rig] adopted %s as THE display pawn."), *Candidate->GetName());
		}
		else
		{
			UE_LOG(LogAFLPreviewRig, Warning, TEXT("AFL_C1[Rig] destroying STRAY display pawn %s -- one pawn per world."), *Candidate->GetName());
			Candidate->Destroy();
		}
	}
	if (SharedPawn.IsValid())
	{
		return SharedPawn.Get();
	}

	UClass* PawnCls = PreferredClass ? PreferredClass.Get() : AAFLLoadoutDisplayPawn::StaticClass();
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAFLLoadoutDisplayPawn* Spawned = World->SpawnActor<AAFLLoadoutDisplayPawn>(PawnCls, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
	if (Spawned)
	{
		SharedPawn = Spawned;
		UE_LOG(LogAFLPreviewRig, Log, TEXT("AFL_C1[Rig] spawned THE display pawn %s (%s)."),
			*Spawned->GetName(), *PawnCls->GetName());
	}
	else
	{
		UE_LOG(LogAFLPreviewRig, Warning, TEXT("AFL_C1[Rig] display pawn spawn FAILED (%s)."), *PawnCls->GetName());
	}
	return Spawned;
}

AAFLLoadoutPod* UAFLPreviewRigSubsystem::AcquirePod(TSubclassOf<AAFLLoadoutPod> PreferredClass, AAFLLoadoutDisplayPawn* ForPawn)
{
	if (SharedPod.IsValid())
	{
		return SharedPod.Get();
	}
	UWorld* World = GetWorld();
	if (!World || !ForPawn)
	{
		return nullptr;
	}
	// Same stray rule as the pawn: one pod; surplus transient pods are destroyed loudly.
	for (TActorIterator<AAFLLoadoutPod> It(World); It; ++It)
	{
		AAFLLoadoutPod* Candidate = *It;
		if (!IsValid(Candidate) || Candidate->IsNetStartupActor())
		{
			continue;
		}
		if (!SharedPod.IsValid())
		{
			SharedPod = Candidate;
			UE_LOG(LogAFLPreviewRig, Log, TEXT("AFL_C1[Rig] adopted %s as THE pod."), *Candidate->GetName());
		}
		else
		{
			UE_LOG(LogAFLPreviewRig, Warning, TEXT("AFL_C1[Rig] destroying STRAY pod %s."), *Candidate->GetName());
			Candidate->Destroy();
		}
	}
	if (SharedPod.IsValid())
	{
		return SharedPod.Get();
	}

	UClass* PodCls = PreferredClass ? PreferredClass.Get() : AAFLLoadoutPod::StaticClass();
	FActorSpawnParameters PodParams;
	PodParams.ObjectFlags |= RF_Transient;
	PodParams.Owner = ForPawn;
	AAFLLoadoutPod* Spawned = World->SpawnActor<AAFLLoadoutPod>(PodCls, PodParams);
	if (Spawned)
	{
		SharedPod = Spawned;
		UE_LOG(LogAFLPreviewRig, Log, TEXT("AFL_C1[Rig] spawned THE pod %s."), *Spawned->GetName());
	}
	return Spawned;
}

AAFLLoadoutDisplayPawn* UAFLPreviewRigSubsystem::PeekDisplayPawn() const
{
	return SharedPawn.IsValid() ? SharedPawn.Get() : nullptr;
}
