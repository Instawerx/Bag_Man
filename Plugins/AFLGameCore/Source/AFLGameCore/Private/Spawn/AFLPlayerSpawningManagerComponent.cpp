// Copyright C12 AI Gaming. All Rights Reserved.

#include "Spawn/AFLPlayerSpawningManagerComponent.h"

#include "AFLGameCore.h"                       // LogAFLGameCore
#include "GameModes/LyraGameState.h"           // ALyraGameState (GetGameStateChecked)
#include "Player/LyraPlayerStart.h"            // ALyraPlayerStart + ELyraPlayerStartLocationOccupancy
#include "Teams/LyraTeamSubsystem.h"           // team lookup (FindTeamFromObject)
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"                // ECC_Visibility
#include "Engine/World.h"
#include "GameFramework/Controller.h"          // AController -> UObject upcast for FindTeamFromObject
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLPlayerSpawningManagerComponent)

bool UAFLPlayerSpawningManagerComponent::AnyEnemyHasLineOfSight(int32 PlayerTeamId, const ALyraPlayerStart* Start) const
{
	UWorld* World = GetWorld();
	ULyraTeamSubsystem* Teams = World ? World->GetSubsystem<ULyraTeamSubsystem>() : nullptr;
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!World || !Teams || !GameState || !Start)
	{
		return false;
	}

	const FVector Eye(0.0, 0.0, SpawnLosEyeHeight);
	const FVector StartLoc = Start->GetActorLocation() + Eye;

	for (const APlayerState* PS : GameState->PlayerArray)
	{
		if (!PS || PS->IsOnlyASpectator() || Teams->FindTeamFromObject(PS) == PlayerTeamId)
		{
			continue;   // spectators + same-team are not a spawn-camp threat
		}
		const APawn* EnemyPawn = PS->GetPawn();
		if (!EnemyPawn)
		{
			continue;
		}
		const FVector EnemyLoc = EnemyPawn->GetActorLocation() + Eye;

		FCollisionQueryParams Params(FName(TEXT("AFLSpawnLOS")), /*bTraceComplex=*/ false);
		Params.AddIgnoredActor(Start);
		Params.AddIgnoredActor(EnemyPawn);
		FHitResult Hit;
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, StartLoc, EnemyLoc, ECC_Visibility, Params);
		if (!bBlocked)
		{
			return true;   // clear sightline enemy<->start -> exposed
		}
	}
	return false;
}

AActor* UAFLPlayerSpawningManagerComponent::OnChoosePlayerStart(AController* Player, TArray<ALyraPlayerStart*>& PlayerStarts)
{
	UWorld* World = GetWorld();
	ULyraTeamSubsystem* TeamSubsystem = World ? World->GetSubsystem<ULyraTeamSubsystem>() : nullptr;
	if (!ensure(TeamSubsystem))
	{
		return nullptr;
	}

	const int32 PlayerTeamId = TeamSubsystem->FindTeamFromObject(Player);
	// Early-login (pre post-login) can call this before a team exists -- let the base fall back to random.
	if (!ensure(PlayerTeamId != INDEX_NONE))
	{
		return nullptr;
	}

	// (1) No-enemy-LOS pre-filter -- drop starts an enemy can currently see.
	TArray<ALyraPlayerStart*> SafeStarts;
	TArray<FString> RejectedNames;
	if (bRejectOnEnemyLOS)
	{
		SafeStarts.Reserve(PlayerStarts.Num());
		for (ALyraPlayerStart* Start : PlayerStarts)
		{
			if (!Start)
			{
				continue;
			}
			if (AnyEnemyHasLineOfSight(PlayerTeamId, Start))
			{
				RejectedNames.Add(Start->GetName());
			}
			else
			{
				SafeStarts.Add(Start);
			}
		}
	}
	const bool bAllExposed = bRejectOnEnemyLOS && SafeStarts.Num() == 0 && PlayerStarts.Num() > 0;
	// Prefer the LOS-safe subset; if every start is exposed, fall back to all (spawning beats not spawning).
	const TArray<ALyraPlayerStart*>& Candidates = (bRejectOnEnemyLOS && SafeStarts.Num() > 0) ? SafeStarts : PlayerStarts;

	// (2) Team-aware furthest-from-enemy over the safe subset -- faithful mirror of ShooterCore UTDM (see class note).
	ALyraGameState* GameState = GetGameStateChecked<ALyraGameState>();
	ALyraPlayerStart* BestPlayerStart = nullptr;
	double MaxDistance = 0;
	ALyraPlayerStart* FallbackPlayerStart = nullptr;
	double FallbackMaxDistance = 0;

	for (APlayerState* PS : GameState->PlayerArray)
	{
		const int32 TeamId = TeamSubsystem->FindTeamFromObject(PS);
		if (PS->IsOnlyASpectator() || TeamId == INDEX_NONE || TeamId == PlayerTeamId)
		{
			continue;   // only measure distance from ENEMIES
		}
		APawn* EnemyPawn = PS->GetPawn();
		if (!EnemyPawn)
		{
			continue;
		}
		for (ALyraPlayerStart* PlayerStart : Candidates)
		{
			if (!PlayerStart)
			{
				continue;
			}
			const double Distance = PlayerStart->GetDistanceTo(EnemyPawn);
			if (PlayerStart->IsClaimed())
			{
				if (FallbackPlayerStart == nullptr || Distance > FallbackMaxDistance)
				{
					FallbackPlayerStart = PlayerStart;
					FallbackMaxDistance = Distance;
				}
			}
			else if (PlayerStart->GetLocationOccupancy(Player) < ELyraPlayerStartLocationOccupancy::Full)
			{
				if (BestPlayerStart == nullptr || Distance > MaxDistance)
				{
					BestPlayerStart = PlayerStart;
					MaxDistance = Distance;
				}
			}
		}
	}

	ALyraPlayerStart* const Chosen = BestPlayerStart ? BestPlayerStart : FallbackPlayerStart;

	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFLSpawn: team=%d -> '%s' (dist=%.0f) | LOS-safe %d/%d rejected=[%s]%s"),
		PlayerTeamId,
		Chosen ? *Chosen->GetName() : TEXT("<none>"),
		BestPlayerStart ? MaxDistance : FallbackMaxDistance,
		SafeStarts.Num(), PlayerStarts.Num(),
		*FString::Join(RejectedNames, TEXT(",")),
		bAllExposed ? TEXT(" [FALLBACK all-exposed]") : TEXT(""));

	return Chosen;
}
