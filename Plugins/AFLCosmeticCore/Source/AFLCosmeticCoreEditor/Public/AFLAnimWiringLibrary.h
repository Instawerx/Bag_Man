// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "AFLAnimWiringLibrary.generated.h"

class UAnimBlueprint;

/**
 * CC-8: author an AnimDynamics post-process AnimGraph from C++, because the pose-link connection is not
 * reachable from any scripting surface here.
 *
 * WHY THIS EXISTS. UE Python exposes AnimGraphNode classes but not pins, node GUIDs, or a schema connect;
 * the AIK Lua bridge's find_nodes does not traverse an AnimGraph and its connect needs handles it cannot
 * obtain. Placement and per-node CONFIG are scriptable; the single dyn.Pose -> Output.Result WIRE is not.
 * TryCreateConnection is C++-only, so the wire is done here -- and while we are in C++, the node is
 * created and configured here too, so the whole graph is authored by one reproducible call rather than a
 * mix of AIK placement + manual wiring.
 */
UCLASS()
class UAFLAnimWiringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Author a chain post-process ABP: one AnimDynamics node running a bone chain from BoundBone to
	 * ChainEnd, in component space, wired to the output pose, at the given LOD threshold. Idempotent --
	 * an existing AnimDynamics node is reconfigured rather than duplicated. Compiles the Blueprint.
	 * Returns true only if the wire was verified present after connection.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Editor")
	static bool WireChainDynamics(UAnimBlueprint* AnimBP, FName BoundBone, FName ChainEnd, int32 LODThreshold);
};
