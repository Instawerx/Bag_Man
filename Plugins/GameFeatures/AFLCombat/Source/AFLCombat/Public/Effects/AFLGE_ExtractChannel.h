// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "AFLGE_ExtractChannel.generated.h"

/**
 * UAFLGE_ExtractChannel  (extraction cycle 1 -- the channel-window state)
 *
 * Infinite-duration GE held for exactly the channel lifetime by UAFLAG_Extract (handle-tracked:
 * applied in ActivateAbility on the authority, removed in EndAbility on EVERY exit path). One GE,
 * two tags (the O1 hard-lock pick):
 *  - State.Extracting       -- replicated channel truth (UI bar start/stop, dash/ability blocking).
 *  - Gameplay.MovementStopped -- Lyra CMC natively zeroes GetMaxSpeed + rotation while this tag is
 *    present (LyraCharacterMovementComponent.cpp:108-127): the stand-still costs ZERO new code.
 *
 * Infinite on purpose: the channel can end early (damage / zone exit / death) -- lifetime is owned
 * by the apply/remove pair, never by a duration race against the WaitDelay task.
 */
UCLASS()
class AFLCOMBAT_API UAFLGE_ExtractChannel : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAFLGE_ExtractChannel();
};
