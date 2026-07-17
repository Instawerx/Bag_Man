// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "AFLRoundRestartPolicy.generated.h"

UINTERFACE(MinimalAPI)
class UAFLRoundRestartPolicy : public UInterface
{
	GENERATED_BODY()
};

/**
 * IAFLRoundRestartPolicy  (the always-loaded restart-policy seam)
 *
 * The decoupling boundary between AAFLGameMode (always-loaded AFLGameCore, instantiated at world init)
 * and the round driver (UAFLRoundManagerComponent, a GameFeature GameStateComponent). The game mode
 * exists before the GameFeature loads, so it CANNOT reference the component's concrete type -- it queries
 * this interface on the GameState's components instead. A component implementing it answers "block a
 * controller restart right now?". The interface lives in the always-loaded module, so the dependency
 * direction stays GameFeature(implementer) -> always-loaded(interface).
 */
class IAFLRoundRestartPolicy
{
	GENERATED_BODY()

public:
	/** True = deny a controller restart right now (e.g. a round is active and mid-round respawn is off). */
	virtual bool ShouldBlockRestart() const = 0;

	/**
	 * The side index (0 or 1) the team is CURRENTLY on -- folds in the round's half-time side swap so the spawn
	 * selector can pick the team's fixed mirror side. INDEX_NONE = no side policy (the selector then ignores
	 * sides and uses furthest-from-enemy only). Defaulted -> a non-breaking extension for existing implementers.
	 */
	virtual int32 GetTeamSideIndex(int32 TeamId) const { return INDEX_NONE; }
};
