// Copyright C12 AI Gaming. All Rights Reserved.

#include "Water/AFLWaterVolume.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLWaterVolume)


AAFLWaterVolume::AAFLWaterVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// THE ONE SETTING THAT MAKES THIS A WATER VOLUME. Without it the volume is an ordinary physics volume:
	// the pawn walks through it, MOVE_Swimming never engages, and the only symptom is that swimming does not
	// happen -- no error, no warning. Set on the type so it cannot be missed per placement.
	bWaterVolume = true;

	// ALWAYS LOADED. Written directly rather than through SetIsSpatiallyLoaded, because that setter
	// check()s CanChangeIsSpatiallyLoadedFlag() -- which this class deliberately returns false from (below).
	// Going through the setter would assert on our own lock.
	//
	// WITH_EDITORONLY_DATA, not WITH_EDITOR: AActor declares bIsSpatiallyLoaded under that macro, and the two
	// are NOT interchangeable (a cooked-editor-data target has the member without being an editor build), so
	// the guard has to match the engine's exactly or it breaks somewhere less obvious than Shipping.
	//
	// Nothing is lost by guarding it. This value exists to be read AT COOK TIME by World Partition, which
	// bakes the streaming decision into the built level; a cooked runtime has no flag to set and no streaming
	// choice left to make. The always-loaded guarantee is therefore carried by the cooked data, not by this
	// line -- this line is what tells the cooker.
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif

	// A water volume has no per-frame work of its own. Phase 4's component ticks against the pawn, not here.
	PrimaryActorTick.bCanEverTick = false;

	// Replicated so clients agree the volume exists. The swim state itself is CMC-driven and needs no custom
	// replication -- the movement mode is already part of the replicated movement state.
	bReplicates = true;
	SetReplicateMovement(false);   // it never moves; do not spend bandwidth saying so
}

#if WITH_EDITOR
bool AAFLWaterVolume::CanChangeIsSpatiallyLoadedFlag() const
{
	// FALSE, PERMANENTLY.
	//
	// This is the enforcement half of the always-loaded requirement. The constructor sets the default; this
	// stops anyone changing it afterwards -- the World Partition editor consults this before offering the
	// toggle, and SetIsSpatiallyLoaded check()s it.
	//
	// The failure it prevents is silent and mid-match: a volume that streams out while a player is inside it
	// drops them from MOVE_Swimming into falling, submerged, with no volume to re-enter. Nothing logs, and the
	// player cannot tell what happened. Same class as DOCTRINE C6, which this project already paid for once on
	// the map this volume is built for.
	//
	// If a future design genuinely needs a streaming water body, that is a NEW class with its own reasoning,
	// not a flag flipped on this one.
	return false;
}
#endif // WITH_EDITOR
