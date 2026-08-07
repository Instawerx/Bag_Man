// Copyright C12 AI Gaming. All Rights Reserved.

#include "Spawn/AFLPlayerSpawningManagerComponent.h"

#include "AFLGameCore.h"                       // LogAFLGameCore
#include "AFLRoundRestartPolicy.h"             // IAFLRoundRestartPolicy (the core round-side seam)
#include "GameFeatures/AFLGFA_ActivateDataLayers.h"    // GetActiveDistrictForWorld -- the district seam
#include "WorldPartition/DataLayer/DataLayerAsset.h"   // UDataLayerAsset (ContainsDataLayer arg)
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

namespace
{
	/**
	 * District data layer -> the spawn tag its PlayerStarts carry.
	 *
	 * An EXPLICIT map, not a string transform. Deriving "AFL.Spawn.District.Duel" from "District_Duel" by
	 * chopping a prefix would silently produce a plausible-but-wrong tag the day a district is renamed, and
	 * a wrong tag here means spawning outside the play space -- the exact failure this whole change exists
	 * to remove. An unmapped district is loud (see the caller) rather than approximately handled.
	 *
	 * RequestGameplayTag rather than UE_DEFINE_GAMEPLAY_TAG_STATIC because these tags are CONFIG-defined in
	 * AFLCoreTags.ini (they are authored on actors by designers, alongside AFL.Spawner.Weapon.* and
	 * AFL.GamePhase.*). Defining them natively as well would double-register them.
	 */
	FGameplayTag DistrictSpawnTagFor(const UDataLayerAsset* District)
	{
		if (District == nullptr)
		{
			return FGameplayTag();
		}

		static const TMap<FName, FName> DistrictToTag = {
			{ FName(TEXT("District_Duel")),  FName(TEXT("AFL.Spawn.District.Duel"))  },
			{ FName(TEXT("District_Arena")), FName(TEXT("AFL.Spawn.District.Arena")) },
			{ FName(TEXT("District_Team")),  FName(TEXT("AFL.Spawn.District.Team"))  },
		};

		if (const FName* TagName = DistrictToTag.Find(District->GetFName()))
		{
			return FGameplayTag::RequestGameplayTag(*TagName, /*ErrorIfNotFound=*/false);
		}
		return FGameplayTag();
	}
}

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

	// (-1) DISTRICT filter -- restrict to starts INSIDE the active district, before anything else runs.
	//
	// THE BUG THIS CLOSES (measured 2026-08-07, L_ShantyTown 2v2 via the front-end tile): District_Duel
	// activated correctly and its four starts streamed in, and players STILL spawned on the island. The
	// reason is that step (2) below picks the start FURTHEST FROM AN ENEMY over every start in the world --
	// and L_ShantyTown also carries 24 always-loaded island-wide BR starts (ff28ad98) which are, by
	// construction, the furthest ones there are. So the selector did exactly what it was told and reliably
	// chose the wrong half of the map: a 2v2 spread ~150 m apart with nobody in sight.
	//
	// A district is a PLAY SPACE, so it must bound the candidate set before any heuristic sees it -- no
	// distance or LOS rule can be tuned around a candidate that should never have been offered.
	//
	// Matched by TAG, never by data layer membership. The starts are authored outside the district's runtime
	// data layer precisely so they are ALWAYS loaded: a fence is streamed geometry, a spawn point is match
	// configuration. That separation is what makes this filter total rather than best-effort -- there is no
	// window in which the district is active but its starts do not exist, so no timeout, retry or fallback
	// is needed anywhere. See AFLCoreTags.ini for the full rationale.
	//
	// The active district comes from the ACTION, so a district supplied by the fallback list (hand-opened
	// map, direct PIE) filters identically to one supplied by the matchmaker URL. No district (every other
	// map) -> one null check and the behaviour below is unchanged.
	TArray<ALyraPlayerStart*> DistrictStarts;
	if (const UDataLayerAsset* District = UAFLGFA_ActivateDataLayers::GetActiveDistrictForWorld(World))
	{
		const FGameplayTag DistrictTag = DistrictSpawnTagFor(District);
		if (DistrictTag.IsValid())
		{
			for (ALyraPlayerStart* Start : PlayerStarts)
			{
				if (Start && Start->GetGameplayTags().HasTag(DistrictTag))
				{
					DistrictStarts.Add(Start);
				}
			}
		}

		// Both branches below are AUTHORING errors, not race conditions, and they are Errors rather than
		// Warnings for that reason: with the starts always loaded, the only way to get here is a district
		// with no mapped tag, or a map whose starts were never tagged.
		if (!DistrictTag.IsValid())
		{
			UE_LOG(LogAFLGameCore, Error,
				TEXT("AFLSpawn: district '%s' has no AFL.Spawn.District.* tag mapped -- add it to "
				     "DistrictSpawnTagFor and AFLCoreTags.ini. Falling back to every start on the map."),
				*District->GetName());
		}
		else if (DistrictStarts.Num() == 0)
		{
			UE_LOG(LogAFLGameCore, Error,
				TEXT("AFLSpawn: district '%s' is active but NOT ONE of the map's %d PlayerStart(s) carries "
				     "%s -- the map's starts are untagged. Falling back to every start on the map, which "
				     "will spawn players outside the district."),
				*District->GetName(), PlayerStarts.Num(), *DistrictTag.ToString());
		}
	}
	const TArray<ALyraPlayerStart*>& DistrictScoped = (DistrictStarts.Num() > 0) ? DistrictStarts : PlayerStarts;

	// (0) FIXED-MIRROR SIDE filter (T1.4b) -- restrict to the team's CURRENT side, folding in the round's
	//     half-time swap (read layering-safe via the core IAFLRoundRestartPolicy seam). INDEX_NONE -> no side
	//     constraint (4a behavior); a side with no tagged starts -> keep all (graceful, e.g. an untagged map).
	const int32 SideIndex = QueryTeamSideIndex(PlayerTeamId);
	TArray<ALyraPlayerStart*> SideStarts;
	if (SideIndex == 0 || SideIndex == 1)
	{
		const FGameplayTag SideTag = (SideIndex == 0) ? TAG_AFL_Spawn_Side_0 : TAG_AFL_Spawn_Side_1;
		for (ALyraPlayerStart* Start : DistrictScoped)
		{
			if (Start && Start->GetGameplayTags().HasTag(SideTag))
			{
				SideStarts.Add(Start);
			}
		}
	}
	const TArray<ALyraPlayerStart*>& SideScoped = (SideStarts.Num() > 0) ? SideStarts : DistrictScoped;

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

	ALyraPlayerStart* Chosen = BestPlayerStart ? BestPlayerStart : FallbackPlayerStart;

	// (3) NO ENEMY PAWN EXISTED TO MEASURE AGAINST -- take the filtered set instead of giving up.
	//
	// The pass above is driven ENTIRELY by enemy pawns (`PS->GetPawn()`), so at the first spawn of a match --
	// before any opponent has a body -- its loop body never runs and Chosen stays null. Returning null there
	// hands the decision back to ULyraPlayerSpawningManagerComponent::ChoosePlayerStart, which draws from
	// EVERY cached start and silently discards the district / side / LOS / anti-camp filtering computed above.
	//
	// MEASURED CONSEQUENCE (2026-08-07, L_ShantyTown 2v2): the human is normally the first to spawn, so the
	// human -- and only the human -- was placed outside the district on every single run, while every bot
	// (spawning later, with an enemy pawn present) was placed correctly inside it. The district filter looked
	// broken from the player's seat while being provably correct in the log.
	//
	// "Nothing to be far from" is not "no valid spawn". Every other stage in this function already degrades
	// this way -- bAllExposed, bAllHot, and the side fallback all choose a worse candidate over none. This is
	// the one stage that did not, and the only one whose failure silently un-does all the others.
	bool bNoEnemyFallback = false;
	if (Chosen == nullptr)
	{
		for (ALyraPlayerStart* Start : FinalCandidates)
		{
			// Occupancy still applies, so sequential spawns at match start spread across the set rather than
			// stacking on one point.
			if (Start && Start->GetLocationOccupancy(Player) < ELyraPlayerStartLocationOccupancy::Full)
			{
				Chosen = Start;
				bNoEnemyFallback = true;
				break;
			}
		}
	}

	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFLSpawn: team=%d side=%d -> '%s' (dist=%.0f) | side-scoped %d, LOS-safe %d/%d rejected=[%s]%s | anti-camp: %d hot-rejected, %d cool, %d hotpts%s%s"),
		PlayerTeamId, SideIndex,
		Chosen ? *Chosen->GetName() : TEXT("<none>"),
		BestPlayerStart ? MaxDistance : FallbackMaxDistance,
		SideStarts.Num(), SafeStarts.Num(), SideScoped.Num(),
		*FString::Join(RejectedNames, TEXT(",")),
		bAllExposed ? TEXT(" [FALLBACK all-exposed]") : TEXT(""),
		HotRejected, CoolStarts.Num(), RecentHotPoints.Num(),
		bAllHot ? TEXT(" [FALLBACK all-hot]") : TEXT(""),
		bNoEnemyFallback ? TEXT(" [no-enemy: took filtered set]") : TEXT(""));

	return Chosen;
}
