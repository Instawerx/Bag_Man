// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "Zone/AFLZonePlan.h"          // FAFLZoneRules

#include "AFLZoneConfig.generated.h"

class UGameplayEffect;

/**
 * UAFLZoneConfig — the designer-tunable half of the shrinking Zone (IRONICS_BR_ZONE_SYSTEM.md §3.5).
 *
 * Every number the Zone behaves by lives here, and NOT in C++, because §7 lists phase count, timing,
 * damage curve, centre selection and the final footprint as decisions the operator has explicitly not
 * made yet. A decision nobody has taken must not be welded into code where changing it costs a rebuild —
 * the whole point of this asset is that tuning the mode is a data edit and a PIE session.
 *
 * ONE ASSET PER FIELD SIZE. BR_9, BR_20 and BR_36 are the same rules with different arcs: nine players on
 * the same map want a faster, tighter sequence than thirty-six. Whether they diverge at all is §7 decision
 * 1 and is answered by authoring one asset or three, with no code change either way.
 *
 * The component runs perfectly well with NO config assigned — `FAFLZoneRules`' defaults are a playable
 * arc — but it will log loudly, because a zone centred on world origin is almost certainly not where the
 * map is.
 */
UCLASS(BlueprintType)
class AFLCOMBAT_API UAFLZoneConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Geometry and pacing. See FAFLZoneRules for what each field costs the player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Zone") FAFLZoneRules Rules;

	/**
	 * The damage effect applied to a participant standing outside the zone, once per damage period.
	 *
	 * ⚠ INSTANT, NOT PERIODIC-DURATION — a deliberate divergence from the scope doc's "periodic damage GE,
	 * inside -> removed". An instant effect re-applied each period has NO removal problem at all: there is
	 * no live handle to leak when the player re-enters, dies, disconnects, or is possessed by a new pawn.
	 * The duration version needs correct teardown on all four of those paths and is wrong on any one of
	 * them; this version cannot be, because nothing persists between ticks. Z3 ("DoT clears on re-entry")
	 * is then true by construction rather than by cleanup.
	 *
	 * Magnitude arrives via SetByCaller `Data.Damage` = DamagePerSecond * period, so the same asset serves
	 * every phase and every field size.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Zone") TSubclassOf<UGameplayEffect> OutsideDamageEffect;

	/**
	 * Seconds between damage applications. NOT a frame-rate concern — a cadence the player can feel and
	 * react to. One second is the genre's readable beat: fast enough that leaving is urgent, slow enough
	 * that a player crossing the boundary is not deleted by a burst they never saw.
	 *
	 * Also the outside-check cadence, so cost is (participants / period) traces per second: 36 per second
	 * at the largest field, which is nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Zone", meta = (ClampMin = "0.1")) float DamagePeriodSeconds = 1.f;
};
