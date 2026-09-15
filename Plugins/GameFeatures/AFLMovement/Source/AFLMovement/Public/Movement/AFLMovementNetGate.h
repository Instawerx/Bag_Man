// Copyright C12 AI Gaming. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * HOTFIX GATE (2026-09-15) — the net-unsafe traversal verbs (wall-run, slide, climb) direct-drive the
 * CharacterMovementComponent's velocity/mode from a component that ticks on BOTH server and client, outside
 * the saved-move prediction path. In any networked session the two sides latch divergent states and fight
 * through corrections (the "stuck / pull / slide, jump to break free" bug; see architectural_parkour_family_
 * net_desync). Until they are re-implemented as predicted custom movement modes (movement-overhaul M1-M3),
 * they are DISABLED wherever there is a client/server split.
 *
 * True  => this verb must NOT run (any networked NetMode, unless the escape cvar is set).
 * False => standalone/solo play, where a single world cannot disagree with itself — the verbs run normally.
 *
 * Escape hatch for testing the predicted rewrite in a net session: `afl.Movement.AllowNetUnsafeTraversal 1`.
 */
AFLMOVEMENT_API bool AFLTraversalDisabledForNet(const UWorld* World);
