// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"

#include "AFLHelmetAsset.generated.h"

class AActor;

/**
 * UAFLHelmetAsset — the catalog target for a Helmet cosmetic (S-ECON-CAT 4b, part path).
 *
 * A Helmet catalog entry's Asset (TSoftObjectPtr<UPrimaryDataAsset>) resolves to one of these. It holds
 * exactly what the part-add seam needs to build an FLyraCharacterPart: the part actor CLASS to spawn and
 * the head SOCKET to attach it to. Lives in AFLCosmeticCore (the always-loaded catalog home) so the
 * catalog can reference it; PartClass is a SOFT class so the actual helmet part actor loads on demand.
 *
 * Why a tiny asset rather than pointing the catalog directly at the part class: the catalog Asset field is
 * a uniform TSoftObjectPtr<UPrimaryDataAsset> (one shape for every cosmetic kind); the helmet-SPECIFIC
 * shape (a class + a socket) lives here. The BP add-event on B_BagMan_AssignCharacterPart resolves this
 * via the catalog subsystem, reads PartClass + SocketName, builds FLyraCharacterPart, calls AddCharacterPart
 * (AddCharacterPart is not LYRAGAME_API-exported, so the add is BP-side -- the canonical part seam, the same
 * one the robot body already uses).
 */
UCLASS(BlueprintType)
class AFLCOSMETICCORE_API UAFLHelmetAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The helmet part actor to spawn + attach (SOFT -> loads on demand). Becomes FLyraCharacterPart.PartClass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Helmet")
	TSoftClassPtr<AActor> PartClass;

	/** The head-bone socket on the Lyra skeleton to attach the helmet to. Becomes FLyraCharacterPart.SocketName.
	 *  This is the socket-fit watch-point: the Tripo->bridge visor mesh's pivot/scale must seat cleanly here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Helmet")
	FName SocketName;
};
