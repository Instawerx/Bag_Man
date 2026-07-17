// Copyright C12 AI Gaming. All Rights Reserved.

#include "Spawn/AFLPlayerSpawningManagerComponent.h"

#include "AFLGameCore.h"                       // LogAFLGameCore
#include "AFLRoundRestartPolicy.h"             // IAFLRoundRestartPolicy (the core round-side seam)
#include "GameModes/LyraGameState.h"           // ALyraGameState (GetGameStateChecked)
#include "Player/LyraPlayerStart.h"            // ALyraPlayerStart + ELyraPlayerStartLocationOccupancy
#include "Teams/LyraTeamSubsystem.h"           // team lookup (FindTeamFromObject)
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"                // ECC_Visibility
#include "Engine/World.h"
#include "GameFramework/Controller.h"          // AController -> UObject upcast for FindTeamFromObject
#include "GameFramework/GameStateBase.h"       // GetComponents (policy query)
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "GameFramework/GameplayMessageSubsystem.h"    // UGameplayMessageSubsystem (anti-camp feed)
#include "Messages/LyraVerbMessage.h"                  // FLyraVerbMessage (elimination payload)

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLPlayerSpawningManagerComponent)

// Side tags on the arena starts (T1.4b): the selector maps GetTeamSideIndex(0/1) -> the matching tag and
// restricts candidates to that side's cluster. The 8 L_Arena_01 starts carry these in StartPointTags
// (S cluster @ Y<0 -> Side.0, N cluster @ Y>0 -> Side.1). Native-defined here so the tags exist for the map to set.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_Spawn_Side_0, "AFL.Spawn.Side.0");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_Spawn_Side_1, "AFL.Spawn.Side.1");

int32 UAFLPlayerSpawningManagerComponent::QueryTeamSideIndex(int32 TeamId) const
{
	// Layering-safe read of the round manager's side-state: iterate the GameState components for the core
	// IAFLRoundRestartPolicy seam (the round driver, in AFLCombat, implements it) -- exactly AAFLGameMode's
	// pattern. No concrete GameFeature type referenced; INDEX_NONE if no provider (selector ignores sides).
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState<AGameStateBase>() : nullptr;
	if (!GS)
	{
		return INDEX_NONE;
	}
	TArray<UActorComponent*> Comps;
	GS->GetComponents(Comps);
	for (const UActorComponent* Comp : Comps)
	{
		if (const IAFLRoundRestartPolicy* Policy = Cast<IAFLRoundRestartPolicy>(Comp))
		{
			return Policy->GetTeamSideIndex(TeamId);   // first policy provider decides (mirrors AAFLGameMode)
		}
	}
	return INDEX_NONE;
}

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

void UAFLPlayerSpawningManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	// Anti-camp feed (T1.4b-ii): listen to the CORE Lyra.Elimination.Message bus. Server-only -- the elimination
	// broadcast is server-authoritative and spawn selection runs on the server. The channel tag is a file-static
	// in ULyraHealthComponent, so resolve it by name (the tag is registered natively at startup).
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (Owner && Owner->HasAuthority() && World)
	{
		const FGameplayTag ElimChannel = FGameplayTag::RequestGameplayTag(FName(TEXT("Lyra.Elimination.Message")), /*ErrorIfNotFound=*/false);
		if (ElimChannel.IsValid())
		{
			ElimListenerHandle = UGameplayMessageSubsystem::Get(World).RegisterListener(
				ElimChannel, this, &UAFLPlayerSpawningManagerComponent::HandleEliminationMessage);
		}
	}
}

void UAFLPlayerSpawningManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ElimListenerHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(ElimListenerHandle);
		}
		ElimListenerHandle = FGameplayMessageListenerHandle();
	}
	Super::EndPlay(EndPlayReason);
}

void UAFLPlayerSpawningManagerComponent::HandleEliminationMessage(FGameplayTag /*Channel*/, const FLyraVerbMessage& Payload)
{
	// TEARDOWN-SAFE location resolution: Target is the victim PlayerState; its pawn may already be unpossessed/
	// destroyed at elimination time. Skip (don't record) rather than record a garbage/origin point.
	const APlayerState* VictimPS = Cast<APlayerState>(Payload.Target);
	if (!VictimPS)
	{
		return;   // Target absent or not a PlayerState
	}
	const APawn* VictimPawn = VictimPS->GetPawn();
	if (!VictimPawn)
	{
		return;   // pawn torn down before we resolved it -- a missed hot point is harmless
	}
	const FVector Loc = VictimPawn->GetActorLocation();
	if (Loc.IsNearlyZero())
	{
		return;   // NEVER record (0,0,0) -- the origin-penalty trap
	}

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	RecentHotPoints.Add(FAFLHotPoint{ Loc, Now });
	PruneHotPoints(Now);

	UE_LOG(LogAFLGameCore, Verbose,
		TEXT("AFLSpawn: recorded hot point at %s (t=%.1f, %d live)"), *Loc.ToCompactString(), Now, RecentHotPoints.Num());
}

void UAFLPlayerSpawningManagerComponent::PruneHotPoints(double Now)
{
	const double Window = static_cast<double>(RecentHotPointWindowSeconds);
	RecentHotPoints.RemoveAll([Now, Window](const FAFLHotPoint& HP)
	{
		return (Now - HP.Time) > Window;
	});
}

bool UAFLPlayerSpawningManagerComponent::IsNearRecentHotPoint(const FVector& Loc, double Now) const
{
	const double R2 = static_cast<double>(HotPointRadius) * static_cast<double>(HotPointRadius);
	const double Window = static_cast<double>(RecentHotPointWindowSeconds);
	for (const FAFLHotPoint& HP : RecentHotPoints)
	{
		if ((Now - HP.Time) > Window)
		{
			continue;   // expired
		}
		if (FVector::DistSquared(Loc, HP.Location) <= R2)
		{
			return true;
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

	// (0) FIXED-MIRROR SIDE filter (T1.4b) -- restrict to the team's CURRENT side, folding in the round's
	//     half-time swap (read layering-safe via the core IAFLRoundRestartPolicy seam). INDEX_NONE -> no side
	//     constraint (4a behavior); a side with no tagged starts -> keep all (graceful, e.g. an untagged map).
	const int32 SideIndex = QueryTeamSideIndex(PlayerTeamId);
	TArray<ALyraPlayerStart*> SideStarts;
	if (SideIndex == 0 || SideIndex == 1)
	{
		const FGameplayTag SideTag = (SideIndex == 0) ? TAG_AFL_Spawn_Side_0 : TAG_AFL_Spawn_Side_1;
		for (ALyraPlayerStart* Start : PlayerStarts)
		{
			if (Start && Start->GetGameplayTags().HasTag(SideTag))
			{
				SideStarts.Add(Start);
			}
		}
	}
	const TArray<ALyraPlayerStart*>& SideScoped = (SideStarts.Num() > 0) ? SideStarts : PlayerStarts;

	// (1) No-enemy-LOS pre-filter over the side-scoped set -- drop starts an enemy can currently see.
	TArray<ALyraPlayerStart*> SafeStarts;
	TArray<FString> RejectedNames;
	if (bRejectOnEnemyLOS)
	{
		SafeStarts.Reserve(SideScoped.Num());
		for (ALyraPlayerStart* Start : SideScoped)
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
	const bool bAllExposed = bRejectOnEnemyLOS && SafeStarts.Num() == 0 && SideScoped.Num() > 0;
	// Prefer the LOS-safe subset; if every start is exposed, fall back to the side-scoped set (spawning beats not spawning).
	const TArray<ALyraPlayerStart*>& Candidates = (bRejectOnEnemyLOS && SafeStarts.Num() > 0) ? SafeStarts : SideScoped;

	// (1.5) ANTI-CAMP (T1.4b-ii) -- steer away from recent death/contested points, over the side+LOS-safe set.
	//       Ordering per SSOT §4: side -> LOS -> anti-camp -> furthest-from-enemy tiebreak. Deprioritize-with-
	//       fallback: if EVERY candidate is hot, keep them all (spawning beats not spawning) -- never reject the
	//       last option because the whole map is hot.
	const double Now = World->GetTimeSeconds();
	PruneHotPoints(Now);
	TArray<ALyraPlayerStart*> CoolStarts;
	int32 HotRejected = 0;
	if (bRejectOnHotPoint && RecentHotPoints.Num() > 0)
	{
		CoolStarts.Reserve(Candidates.Num());
		for (ALyraPlayerStart* Start : Candidates)
		{
			if (Start && IsNearRecentHotPoint(Start->GetActorLocation(), Now))
			{
				++HotRejected;
			}
			else if (Start)
			{
				CoolStarts.Add(Start);
			}
		}
	}
	const bool bAllHot = HotRejected > 0 && CoolStarts.Num() == 0;
	const TArray<ALyraPlayerStart*>& FinalCandidates = (CoolStarts.Num() > 0) ? CoolStarts : Candidates;

	// (2) Team-aware furthest-from-enemy over the final candidates -- faithful mirror of ShooterCore UTDM (see class note).
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
		for (ALyraPlayerStart* PlayerStart : FinalCandidates)
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
		TEXT("AFLSpawn: team=%d side=%d -> '%s' (dist=%.0f) | side-scoped %d, LOS-safe %d/%d rejected=[%s]%s | anti-camp: %d hot-rejected, %d cool, %d hotpts%s"),
		PlayerTeamId, SideIndex,
		Chosen ? *Chosen->GetName() : TEXT("<none>"),
		BestPlayerStart ? MaxDistance : FallbackMaxDistance,
		SideStarts.Num(), SafeStarts.Num(), SideScoped.Num(),
		*FString::Join(RejectedNames, TEXT(",")),
		bAllExposed ? TEXT(" [FALLBACK all-exposed]") : TEXT(""),
		HotRejected, CoolStarts.Num(), RecentHotPoints.Num(),
		bAllHot ? TEXT(" [FALLBACK all-hot]") : TEXT(""));

	return Chosen;
}
