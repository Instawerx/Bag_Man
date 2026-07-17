// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Player/LyraPlayerSpawningManagerComponent.h"
#include "GameplayTagContainer.h"                    // FGameplayTag (message channel)
#include "GameFramework/GameplayMessageSubsystem.h"  // FGameplayMessageListenerHandle

#include "AFLPlayerSpawningManagerComponent.generated.h"

class AActor;
class AController;
class ALyraPlayerStart;
struct FLyraVerbMessage;

/**
 * UAFLPlayerSpawningManagerComponent  (Team SSOT §4 T1.4a -- team-aware spawn selection + no-enemy-LOS)
 *
 * Subclass of the EXPORTED Lyra base ULyraPlayerSpawningManagerComponent (UCLASS(MinimalAPI)). It deliberately
 * does NOT subclass ShooterCore's UTDM_PlayerSpawningManagmentComponent: that class is plain UCLASS() with NO
 * SHOOTERCORERUNTIME_API export macro, so it cannot be subclassed cross-module in a modular editor build (link
 * error on the base ctor/StaticClass), and exporting it would be a stock ShooterCore edit (against doctrine).
 * So OnChoosePlayerStart re-creates TDM's small team-aware furthest-from-enemy loop (a faithful mirror of the
 * ShooterCore sample) and ADDS a no-enemy-line-of-sight filter on top.
 *
 * SELECTION (OnChoosePlayerStart):
 *   1. No-enemy-LOS pre-filter -- drop any candidate start an ENEMY pawn currently has a clear Visibility
 *      sightline to. Distance-far can still be sight-exposed; this closes that spawn-camp gap. If EVERY start is
 *      exposed, fall back to the full set (spawning beats not spawning).
 *   2. Team-aware furthest-from-enemy over the LOS-safe subset (mirrors UTDM).
 * Each pick is logged (chosen start + distance + LOS-safe count + rejected names) so the selector is VISIBLE in
 * PIE -- essential because ULyraPlayerSpawningManagerComponent::FindPlayFromHereStart masks player 0 under
 * WITH_EDITOR; verify via the 2nd client + bots + this log, not player 0's position.
 *
 * FIXED-MIRROR SIDE-SWAP (T1.4b) restricts candidates to the team's CURRENT side first (IAFLRoundRestartPolicy::
 * GetTeamSideIndex -> AFL.Spawn.Side.{0,1} start tags), folding in the round manager's half-time bSidesSwapped
 * swap, read layering-safe via the core seam. ANTI-CAMP (4b-ii) and spawn INVULN (4c) are NOT here yet.
 */
UCLASS()
class AFLGAMECORE_API UAFLPlayerSpawningManagerComponent : public ULyraPlayerSpawningManagerComponent
{
	GENERATED_BODY()

protected:
	/** Reject a candidate start if an enemy pawn currently has line-of-sight to it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Spawn")
	bool bRejectOnEnemyLOS = true;

	/** Eye-height offset (uu) added to both trace ends so the LOS check is a realistic sightline, not ground-to-ground. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Spawn")
	float SpawnLosEyeHeight = 80.0f;

	/** T1.4b-ii anti-camp: exclude starts within HotPointRadius of a recent hot point (deprioritize-with-fallback). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Spawn")
	bool bRejectOnHotPoint = true;

	/** Radius (uu) around a recent death/contested point within which a start is treated as camped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Spawn")
	float HotPointRadius = 800.0f;

	/** How long (game-time seconds) an elimination location stays a "hot point". Tier-scalable in 4c. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Spawn")
	float RecentHotPointWindowSeconds = 12.0f;

	//~UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

	//~ULyraPlayerSpawningManagerComponent interface
	virtual AActor* OnChoosePlayerStart(AController* Player, TArray<ALyraPlayerStart*>& PlayerStarts) override;
	//~End of ULyraPlayerSpawningManagerComponent interface

private:
	/** True if ANY enemy-team pawn has a clear Visibility sightline to this start (start + enemy ignored). */
	bool AnyEnemyHasLineOfSight(int32 PlayerTeamId, const ALyraPlayerStart* Start) const;

	/** The side index (0/1) the team is currently on, via the core IAFLRoundRestartPolicy seam on the GameState
	 *  (mirrors AAFLGameMode's query). INDEX_NONE if no policy provider -> selector ignores sides (4a behavior). */
	int32 QueryTeamSideIndex(int32 TeamId) const;

	// --- T1.4b-ii anti-camp: recent death/contested points from the core Lyra.Elimination.Message bus ---

	/** One recent elimination location, stamped with server game-time (World->GetTimeSeconds()). */
	struct FAFLHotPoint
	{
		FVector Location = FVector::ZeroVector;
		double  Time = 0.0;
	};

	/** Listener for TAG "Lyra.Elimination.Message" (core ULyraHealthComponent broadcast). Server-only. */
	void HandleEliminationMessage(FGameplayTag Channel, const FLyraVerbMessage& Payload);

	/** Drop hot points older than RecentHotPointWindowSeconds (bounds the ring). */
	void PruneHotPoints(double Now);

	/** True if Loc is within HotPointRadius of any non-expired hot point. */
	bool IsNearRecentHotPoint(const FVector& Loc, double Now) const;

	/** Recent death/contested locations (value-typed; no GC refs -> not a UPROPERTY). */
	TArray<FAFLHotPoint> RecentHotPoints;

	/** Handle for the elimination-message listener, released in EndPlay. */
	FGameplayMessageListenerHandle ElimListenerHandle;
};
