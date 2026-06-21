// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameModes/LyraGameMode.h"

#include "AFLGameMode.generated.h"

class AController;

/**
 * AAFLGameMode  (Arena round respawn gate)
 *
 * Minimal ALyraGameMode subclass whose ONLY job is the round-based respawn gate. It lives in the
 * ALWAYS-LOADED AFLGameCore plugin (NOT a GameFeature) because a map-default GameMode is instantiated at
 * WORLD INIT, before the experience's GameFeature loads -- a GameMode inside the GameFeature it bootstraps
 * is absent/unregistered at map load.
 *
 * The gate is on the virtual ControllerCanRestart (LYRAGAME_API-exported; ULyraPlayerSpawningManager-
 * Component::ControllerCanRestart is PRIVATE/non-virtual, so the game mode is the extension point). It
 * queries the IAFLRoundRestartPolicy seam on the GameState's components -- NO concrete GameFeature type
 * referenced (the round driver implements the interface; dependency direction stays GameFeature ->
 * always-loaded). SAFE as a global default: without a policy provider it falls through to Super (stock
 * Lyra respawn). Wiring this as the project/experience game mode is Task 2 (config), not C++.
 */
UCLASS()
class AFLGAMECORE_API AAFLGameMode : public ALyraGameMode
{
	GENERATED_BODY()

public:
	//~ALyraGameMode interface
	virtual bool ControllerCanRestart(AController* Controller) override;
	//~End of ALyraGameMode interface
};
