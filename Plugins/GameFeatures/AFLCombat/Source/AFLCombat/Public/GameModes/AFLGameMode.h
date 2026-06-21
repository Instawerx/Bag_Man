// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameModes/LyraGameMode.h"

#include "AFLGameMode.generated.h"

class AController;

/**
 * AAFLGameMode  (Arena round respawn gate)
 *
 * Minimal ALyraGameMode subclass whose ONLY job is the round-based respawn gate. ULyraPlayerSpawning-
 * ManagerComponent::ControllerCanRestart is PRIVATE + non-virtual (friended to ALyraGameMode), so it
 * cannot be subclass-overridden -- the gate lives here, on the game mode's virtual ControllerCanRestart
 * (LYRAGAME_API-exported, the intended extension point).
 *
 * SAFE as a global default game mode: it only blocks when a UAFLRoundManagerComponent on the GameState
 * reports a live round (ShouldBlockRestart); without a round manager it falls straight through to Super
 * (stock Lyra respawn). Wiring this as the project/experience game mode is Task 2 (config), not C++.
 */
UCLASS()
class AFLCOMBAT_API AAFLGameMode : public ALyraGameMode
{
	GENERATED_BODY()

public:
	//~ALyraGameMode interface
	virtual bool ControllerCanRestart(AController* Controller) override;
	//~End of ALyraGameMode interface
};
