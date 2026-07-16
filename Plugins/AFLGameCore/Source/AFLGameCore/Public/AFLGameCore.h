// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Module-wide log channel for the always-loaded AFL game-framework classes. */
AFLGAMECORE_API DECLARE_LOG_CATEGORY_EXTERN(LogAFLGameCore, Log, All);

/**
 * AFLGameCore module.
 *
 * Always-loaded (Default phase, non-GameFeature) home for AFL game-framework classes that must exist
 * at WORLD INIT -- before any GameFeature activates. See AFLGameCore.uplugin for the load-order
 * rationale (a map-default GameMode is instantiated at world init and bootstraps experience loading,
 * so it cannot live in the GameFeature it loads).
 */
