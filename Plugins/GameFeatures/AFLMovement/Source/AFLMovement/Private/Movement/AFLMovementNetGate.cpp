// Copyright C12 AI Gaming. All Rights Reserved.
#include "Movement/AFLMovementNetGate.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarAllowNetUnsafeTraversal(
	TEXT("afl.Movement.AllowNetUnsafeTraversal"),
	0,
	TEXT("0 (default): wall-run/slide/climb are disabled in any networked session (they direct-drive the CMC ")
	TEXT("outside prediction and desync). 1: allow them in net play (only for testing the predicted rewrite)."),
	ECVF_Default);

bool AFLTraversalDisabledForNet(const UWorld* World)
{
	if (!World)
	{
		return false; // no world context -> do not block (nothing to desync)
	}
	if (CVarAllowNetUnsafeTraversal.GetValueOnAnyThread() != 0)
	{
		return false; // explicit test override
	}
	// NM_Standalone is the only single-world mode; every other mode has a client/server split to desync.
	return World->GetNetMode() != NM_Standalone;
}
