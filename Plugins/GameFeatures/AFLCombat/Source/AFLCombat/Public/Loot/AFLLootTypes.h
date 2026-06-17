// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AFLLootTypes.generated.h"

/**
 * Generalized loot system -- shared enums (Loot Phase 1). The system generalizes the proven loot-box
 * (AAFLHeadLootBox / AAFLDismemberedLimb) + energy pickup into one parameter space; see
 * Docs/IRONICS_LOOT_SYSTEM_DESIGN.md. These enums are the axes the config (DA_AFL_LootConfig) and the
 * grant component (UAFLLootGrantComponent) read. NOTHING here is randomized -- every value is known
 * (IRONICS_ECONOMY_SPEC.md NO-RANDOMIZED-ACQUISITION invariant).
 */

/** What a loot object grants on retrieval. */
UENUM(BlueprintType)
enum class EAFLLootValueModel : uint8
{
	/** ECONOMY: grant Watts directly to the retriever's wallet (combat-loot head/limb -- LIVE this phase). */
	Watts                  UMETA(DisplayName = "Watts (instant economy)"),
	/** ECONOMY: grant carried energy (realized as Watts only at the extraction zone). Substrate refactor = Phase 3. */
	CarryToExtractEnergy   UMETA(DisplayName = "Carry-to-extract energy"),
	/** Owner-branch: NO economy grant -- the owner reattaches (dismember). Handled via OnOwnerRetrieved, not GrantValue. */
	Reattach               UMETA(DisplayName = "Reattach (owner, no grant)"),
	/** GAMEPLAY-RESOURCE: grant ammo/health via Lyra IPickupable (NOT currency). The IPickupable bridge = Phase 5. */
	GameplayResource       UMETA(DisplayName = "Gameplay-resource (ammo/health)")
};

/** Who may LOOT a loot object for value (Q1 -- mixed team-gating). */
UENUM(BlueprintType)
enum class EAFLLootEligibility : uint8
{
	/** Anyone who retrieves it gets the grant (caches, energy pickups). */
	Anyone                 UMETA(DisplayName = "Anyone"),
	/** Only a NON-owner (the owner reattaches instead) -- the dismember head/limb model. */
	EnemyOnly              UMETA(DisplayName = "Enemy only (owner reattaches)"),
	/** Only the owner's TEAM (a team's own dropped cache). Team compare wired in Phase 3 with its first consumer. */
	TeamOnly               UMETA(DisplayName = "Team only")
};

/** How a loot object is retrieved (per-cache flag, Q2/#2). The substrate behind each lives in its own
 *  component: Carry = UAFLGrabbableComponent (LIVE); Instant = UAFLOverlapCollectComponent (Phase 3);
 *  Harvest = UAFLHarvestComponent (Phase 4). This enum lets the config pick per placement. */
UENUM(BlueprintType)
enum class EAFLLootRetrievalMode : uint8
{
	/** Pick up + carry (grab substrate) -- deliberate, contestable, throwable. LIVE (dismember). */
	Carry                  UMETA(DisplayName = "Carry (grab)"),
	/** Walk-over instant collect (overlap+magnet). Substrate = Phase 3. */
	Instant                UMETA(DisplayName = "Instant (overlap)"),
	/** Channel-over-time yield (resource nodes). Substrate = Phase 4 (HARVEST -- new pattern). */
	Harvest                UMETA(DisplayName = "Harvest (over time)")
};
