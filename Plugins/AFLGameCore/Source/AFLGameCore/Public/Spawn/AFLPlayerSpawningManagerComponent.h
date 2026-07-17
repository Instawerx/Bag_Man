// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Player/LyraPlayerSpawningManagerComponent.h"

#include "AFLPlayerSpawningManagerComponent.generated.h"

class AActor;
class AController;
class ALyraPlayerStart;

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

	//~ULyraPlayerSpawningManagerComponent interface
	virtual AActor* OnChoosePlayerStart(AController* Player, TArray<ALyraPlayerStart*>& PlayerStarts) override;
	//~End of ULyraPlayerSpawningManagerComponent interface

private:
	/** True if ANY enemy-team pawn has a clear Visibility sightline to this start (start + enemy ignored). */
	bool AnyEnemyHasLineOfSight(int32 PlayerTeamId, const ALyraPlayerStart* Start) const;

	/** The side index (0/1) the team is currently on, via the core IAFLRoundRestartPolicy seam on the GameState
	 *  (mirrors AAFLGameMode's query). INDEX_NONE if no policy provider -> selector ignores sides (4a behavior). */
	int32 QueryTeamSideIndex(int32 TeamId) const;
};
