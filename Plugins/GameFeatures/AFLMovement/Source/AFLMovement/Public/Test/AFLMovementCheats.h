// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/CheatManager.h"

#include "AFLMovementCheats.generated.h"

/**
 * UAFLMovementCheats
 *
 * CheatManagerExtension shell for the AFL movement/interaction lane. Self-registers with the cheat manager
 * on game start when UE_WITH_CHEAT_MANAGER is defined (mirrors UAFLCombatCheats / ULyraCosmeticCheats).
 *
 * Cycle 4c hand-IK isolation: the actual cheats live as DOTTED console commands in the .cpp --
 * `afl.HandIK.Set <X> <Y> <Z>` and `afl.HandIK.Clear` (FAutoConsoleCommandWithWorldArgsAndOutputDevice,
 * the same mechanism AFLCombatCheats uses for afl.GroundTruth / afl.Wallet.* / afl.Cosmetic.*). They drive
 * HandIKTarget / bHandIKEnabled / HandIKAlpha on the local hero's UAFLInteractionComponent, whose
 * TickComponent then pushes the values into CR_AFL_IRONICS.
 *
 * NOTE: an earlier draft used UFUNCTION(Exec) here and the operator typed `afl.SetHandIKTarget` -- Exec
 * functions need a BARE name and only route when the cheat manager is active, so it silently no-op'd. The
 * dotted console commands fix both. This class now holds no Exec functions; it is retained as the home for
 * any future movement cheat and for registration parity.
 *
 * NOT for production gameplay.
 */
UCLASS(NotBlueprintable)
class AFLMOVEMENT_API UAFLMovementCheats final : public UCheatManagerExtension
{
	GENERATED_BODY()

public:

	UAFLMovementCheats();
};
