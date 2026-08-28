// AFL C1 preview spine (AFL-3210/3211/3214): the ONE owner of the front-end preview rig.
//
// WHY THIS EXISTS (measured, 2026-08-28): every loadout-base widget instance owned its own display
// pawn -- spawned, adopted, and DESTROYED it (TeardownPreviewCapture killed pawns other widgets
// shared). With the home screen surviving under the pushed creator, up to four display pawns
// coexisted in ONE world; the capture showed one, the drags painted another, and each individual
// subsystem (dress, schema, override, fan-out) was provably correct while the whole read as broken.
//
// THE RULE: widgets APPLY; the rig OWNS. One display pawn per world, one pod per world; strays are
// destroyed on acquire, and no widget may destroy either. The pawn dies with the world.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "AFLPreviewRigSubsystem.generated.h"

class AAFLLoadoutDisplayPawn;
class AAFLLoadoutPod;

UCLASS()
class AFLCOMBAT_API UAFLPreviewRigSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * THE display pawn. Returns the shared pawn, adopting a live transient one (destroying any
	 * surplus -- says so) or spawning fresh at the given transform when none exists. PreferredClass
	 * is honoured only at spawn time; the first acquirer decides.
	 */
	AAFLLoadoutDisplayPawn* AcquireDisplayPawn(TSubclassOf<AAFLLoadoutDisplayPawn> PreferredClass, const FVector& SpawnLocation);

	/**
	 * THE kiosk pod, attached to the shared pawn. Same single-owner contract as the pawn -- two
	 * widgets previously each spawned a pod around the same robot and both rode into every capture.
	 */
	AAFLLoadoutPod* AcquirePod(TSubclassOf<AAFLLoadoutPod> PreferredClass, AAFLLoadoutDisplayPawn* ForPawn);

	/** The shared pawn if alive; never spawns. */
	AAFLLoadoutDisplayPawn* PeekDisplayPawn() const;

	//~UWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TWeakObjectPtr<AAFLLoadoutDisplayPawn> SharedPawn;
	TWeakObjectPtr<AAFLLoadoutPod> SharedPod;
};
