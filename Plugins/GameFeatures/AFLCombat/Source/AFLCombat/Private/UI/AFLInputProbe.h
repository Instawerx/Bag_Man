// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// ══ INPUT PIPELINE PROBE (development builds only) ═══════════════════════════════════════════════════
//
// P0 2026-09-13: the SECOND time the front end (L_IRONICS_Armory) loads in a session -- after a match,
// after Sign Out from the Outpost, any return -- the WHERE TO? cards are painted, focused, cursor shown,
// router config Menu/NoCapture ... and no click or key ever reaches them. Every static candidate was ruled
// out by source + the shipping log, so this probe makes the pipeline OBSERVABLE:
//
//   1. a front-of-chain Slate input pre-processor that LOGS (never consumes) every OS key/mouse-down it
//      sees -> proves whether input reaches Slate at all;
//   2. UAFLW_RouteChoice logs the mouse/key events that reach the WIDGET -> proves whether input survives
//      the pre-processor chain (loading-screen eater, CommonInput filter, analog cursor, AFL escape proc);
//   3. Dump(): every state that can eat input between those two points, in one block;
//   4. ClickFocused(): a synthetic OS-level click at the focused widget, through the same chain.
//
// Compiled out of shipping (`!UE_BUILD_SHIPPING`, the same guard as AFLDevMenuCommands).

#if !UE_BUILD_SHIPPING

class UWorld;

namespace AFLInputProbe
{
	/** Log every layer that can eat a click/key before a widget sees it (LogAFLCombat, one block). */
	void Dump(UWorld* World, const TCHAR* Why);

	/** Synthesize an OS-level left click at the centre of the Slate-focused widget (goes through every pre-processor). */
	void ClickFocused(UWorld* World);

	/** Register the front-of-chain logging pre-processor once (idempotent). It never consumes input. */
	void EnsureFrontLogger();
}

#endif // !UE_BUILD_SHIPPING
