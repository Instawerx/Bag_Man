// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FGameplayAbilityActorInfo;

/**
 * The path-conflict pattern for body-owning movement abilities (AI-3).
 *
 * THE PROBLEM. BTTask_MoveTo hands a move request to UPathFollowingComponent, which writes movement input
 * every frame toward the current path segment. Dash overrides XY velocity outright (LaunchCharacter with
 * bXYOverride) and Slide/Roll drive root-motion montages. Left alone, path following fights them: the dash
 * gets damped, the montage gets dragged, and the bot ends up somewhere neither system intended.
 *
 * WHY PAUSE AND NOT ABORT. AbortMove makes BTTask_MoveTo return Failed, which fails the Shoot And Move
 * Sequence, which drops the Selector out of the branch -- and that stops Shoot, SetFocus, Find Best Position
 * and the sprint service ticking. A 0.2s dash would blink the bot's entire combat brain. PauseMove keeps the
 * task alive and only silences the movement input.
 *
 * WHY THE STALE PATH IS NOT A PROBLEM. After a dash the pawn is metres from where its path expected it, so
 * resuming looks wrong on paper. But Find Best Position re-runs EQS every 1.5s +/- 0.5s and MoveTo tracks the
 * blackboard goal, so a fresh request re-paths from wherever the bot actually is within about a second and a
 * half. Resume covers the ability window; the cadence that already exists corrects what follows.
 *
 * SYMMETRY IS GUARANTEED TWICE, because a path paused and never resumed is a bot frozen for the rest of the
 * round -- the worst thing this pattern could produce:
 *   1. Resume is called ONLY from EndAbility, and GAS funnels every exit through EndAbility -- normal
 *      completion, CancelAbility, death, match end, and a failed activation after commit. Both calls are
 *      idempotent and state-guarded (Suspend only acts on Moving, Resume only on Paused), so an unmatched
 *      call in either direction is a no-op rather than a corruption.
 *   2. Even if Resume were somehow missed, UPathFollowingComponent::RequestMove calls SetStatus(Moving) for
 *      any new request, so the next MoveTo -- at most ~2s away on the EQS cadence -- clears a stuck pause on
 *      its own. A permanently frozen bot is not reachable through this path.
 *
 * HUMANS ARE UNTOUCHED BY CONSTRUCTION, not by a flag: both functions resolve the path following component
 * through AAIController. A player-controlled pawn has none, so both are no-ops on the human path.
 */
namespace AFLMovementPath
{
	/** Pause path following for the duration of a body-owning ability. No-op for humans and for a pawn that
	 *  is not currently following a path. Velocity is KEPT, not reset -- resetting would kill a dash launch
	 *  in the same frame it was applied. */
	AFLMOVEMENT_API void Suspend(const FGameplayAbilityActorInfo* ActorInfo);

	/** Resume path following. Safe to call unmatched; only acts on a genuinely paused component. */
	AFLMOVEMENT_API void Resume(const FGameplayAbilityActorInfo* ActorInfo);
}
