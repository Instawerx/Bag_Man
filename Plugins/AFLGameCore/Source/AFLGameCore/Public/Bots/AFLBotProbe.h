// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FOutputDevice;
class UWorld;

/**
 * The movement probe, callable rather than only typeable.
 *
 * WHY THIS HEADER EXISTS. The verdict used to be reachable only by typing afl.Bot.MoveProbe at the right
 * moment -- host window, mid-firefight, late enough in the match to have rounds behind it. Timing cost two
 * reads in a row: once the command was never run at all, once it was run three seconds in and correctly
 * reported INCONCLUSIVE with 0 rounds of history. Both times the data existed and the verdict did not.
 *
 * So AAFLBotController::EndPlay calls this once per world at teardown. The console command remains, and both
 * go through this one function -- an auto-run that drifted from the typed version would be worse than none.
 */
namespace AFLBotProbe
{
	/** Evaluate every live AAFLBotController and write the verdict to Ar. Host-only; returns quietly on a client. */
	AFLGAMECORE_API void RunMove(UWorld* World, FOutputDevice& Ar);
}
