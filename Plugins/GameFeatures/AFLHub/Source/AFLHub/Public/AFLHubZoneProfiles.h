// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "AFLHubZoneProfiles.generated.h"

/**
 * One row per hub zone (AFL-3012: "data, not code"). The component consumes NetCullDistance today;
 * MirrorCaptureRes/Rate are carried now so the M7 mirror pass reads the SAME rows instead of growing
 * a second registry (the MANIFEST=registry lesson).
 */
USTRUCT(BlueprintType)
struct FAFLHubZoneProfile
{
	GENERATED_BODY()

	/** Which Hub.Zone.* this row tunes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Hub", meta = (Categories = "Hub.Zone"))
	FGameplayTag ZoneTag;

	/** Pawn relevancy radius while inside this zone, cm. Smaller = cull harder (dense social zones). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Hub", meta = (ClampMin = "1000.0", ForceUnits = "cm"))
	float NetCullDistance = 15000.0f;

	/** Mirror SceneCapture resolution while inside this zone (M7 consumes; data parked here per AC). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Hub|Mirror")
	int32 MirrorCaptureRes = 512;

	/** Mirror SceneCapture rate, Hz (M7 consumes). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Hub|Mirror")
	float MirrorCaptureRate = 15.0f;
};

/**
 * UAFLHubZoneProfiles -- the per-zone tuning table (DA_AFL_HubZoneProfiles).
 * Adding a zone's net posture is a ROW, never a code change.
 */
UCLASS(BlueprintType)
class AFLHUB_API UAFLHubZoneProfiles : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Cull radius when the pawn stands in NO profiled zone, cm. 30000 = the stock Lyra pawn's
	 *  sqrt(900000000) -- "no zone" means "behave like the match pawn". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Hub", meta = (ClampMin = "1000.0", ForceUnits = "cm"))
	float DefaultNetCullDistance = 30000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Hub", meta = (TitleProperty = "ZoneTag"))
	TArray<FAFLHubZoneProfile> Zones;

	/** Row lookup by exact zone tag; nullptr when unprofiled. */
	const FAFLHubZoneProfile* FindProfile(const FGameplayTag& InZoneTag) const
	{
		return Zones.FindByPredicate([&InZoneTag](const FAFLHubZoneProfile& Row)
		{
			return Row.ZoneTag == InZoneTag;
		});
	}
};
