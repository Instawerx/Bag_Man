// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "AFLGE_Channel.generated.h"

/**
 * UAFLGE_Channel  (Loot-Carry Phase B -- the generic channel-window state)
 *
 * The reusable cousin of UAFLGE_ExtractChannel, generalized for any UAFLGameplayAbility_Channel (the CARRY
 * collect-channel now; HARVEST in Phase 4). Infinite-duration, held for exactly the channel lifetime by the
 * channel ability (applied in ActivateAbility on the authority, removed in EndAbility on EVERY exit path --
 * the handle-tracked apply/remove pair, never a duration race against the WaitDelay task). One tag:
 *  - State.Channeling -- replicated channel truth (progress UI, blocking a second channel).
 *
 * DELIBERATELY NOT Gameplay.MovementStopped -- the REQUIRED divergence from extract. Decision 3 is
 * "stand-and-channel, MOVING AWAY cancels it (exposing you)"; moving must be POSSIBLE, so this GE does NOT
 * lock movement. The channel ability's MaxMoveRadius poll is the live "stand still or lose it" interrupt
 * (the lock was what made that move-interrupt dead code). HARVEST may add its own lock GE if it wants a
 * hard stand-still; the collect-channel does not.
 */
UCLASS()
class AFLCOMBAT_API UAFLGE_Channel : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAFLGE_Channel();
};
