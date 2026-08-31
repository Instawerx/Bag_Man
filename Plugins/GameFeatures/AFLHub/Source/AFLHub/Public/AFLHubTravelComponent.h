// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "AFLHubTravelComponent.generated.h"

/**
 * UAFLHubTravelComponent  (club-door Travel backend)
 *
 * The ONE server seam for hub door travel. Door volumes are cosmetic + client-side (they push
 * screens and draw signs); a Travel action must move the SESSION, and a placed level actor has no
 * owning connection to RPC through -- so the request rides the player-owned hero pawn instead,
 * the same GameFeatureAction_AddComponents delivery as the proven net-profile sibling.
 *
 * Server-authoritative by doctrine (N1): the client sends only the DestinationId; the server
 * re-resolves the row from a placed door volume's table and refuses anything that is not an
 * enabled Travel row with a payload. The payload IS the map package path (the door header's
 * documented Travel shape), fed to ServerTravel -- non-seamless, whole session moves; the EOS
 * club sub-lobby layer replaces these semantics when its gate clears.
 */
UCLASS(ClassGroup = (AFL), Blueprintable, meta = (BlueprintSpawnableComponent))
class AFLHUB_API UAFLHubTravelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Ask the server to run the Travel action of the named destination row. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "AFL|Hub")
	void ServerRequestHubTravel(FName DestinationId);
};
