// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * AFLNetTypes module.
 *
 * Always-loaded (Default phase, non-GameFeature) home for AFL cross-cutting REPLICATED
 * types. See AFLNetTypes.uplugin for the load-order rationale (FNetSerializeScriptStructCache
 * must contain these structs on every endpoint before any GameFeature activates).
 */
