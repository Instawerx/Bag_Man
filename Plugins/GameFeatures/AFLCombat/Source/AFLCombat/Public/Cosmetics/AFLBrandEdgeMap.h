// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

// #43: included (not just forward-declared) so ResolveEdgeById can call UAFLSkinColorAsset::GetCosmeticId()
// inline below. The reverse-by-CosmeticId lookup is a stopgap until the catalog (S-ECON-CAT) is the registry.
#include "Cosmetics/AFLSkinColorAsset.h"

#include "AFLBrandEdgeMap.generated.h"

/**
 * Brand -> default-edge mapping for the BagMan robots (#38a).
 *
 * A pure, typed lookup: each robot carries a Cosmetic.Brand.* tag (on its AAFLCharacterPartActor's
 * StaticGameplayTags, read via IGameplayTagAssetInterface); this asset maps that brand tag to the edge
 * COLOR preset (UAFLSkinColorAsset) that is the brand's factory default. The controller component resolves
 * the per-robot default at possess (server-side authority) and feeds the result through the SAME
 * PawnComp->SetSkinColor(...) propagation path the proven Race A/B/C wire uses -- this asset changes ONLY
 * which preset is chosen, never how it propagates.
 *
 * ZERO reflection (mirrors UAFLSkinColorAsset): a C++-typed TMap + a direct getter, so resolution reads it
 * with no FindFProperty / runtime reflection lookup that could silently miss.
 */
UCLASS(BlueprintType)
class AFLCOMBAT_API UAFLBrandEdgeMap : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Brand tag (Cosmetic.Brand.*) -> default edge preset for that brand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Cosmetic")
	TMap<FGameplayTag, TObjectPtr<UAFLSkinColorAsset>> BrandToEdge;

	/** Resolve the edge preset for a brand tag; nullptr if unmapped. */
	UAFLSkinColorAsset* ResolveEdge(const FGameplayTag& BrandTag) const
	{
		const TObjectPtr<UAFLSkinColorAsset>* Found = BrandToEdge.Find(BrandTag);
		return Found ? Found->Get() : nullptr;
	}

	/** #43 stopgap: resolve an edge preset by its immutable CosmeticId (UAFLSkinColorAsset::CosmeticId),
	 *  scanning the same presets this map already references. Replaced by the catalog (S-ECON-CAT) when it
	 *  lands; until then it lets the selection's EdgeId resolve without a separate registry. nullptr on miss. */
	UAFLSkinColorAsset* ResolveEdgeById(FName CosmeticId) const
	{
		if (CosmeticId == NAME_None)
		{
			return nullptr;
		}
		for (const TPair<FGameplayTag, TObjectPtr<UAFLSkinColorAsset>>& Pair : BrandToEdge)
		{
			if (Pair.Value && Pair.Value->GetCosmeticId() == CosmeticId)
			{
				return Pair.Value.Get();
			}
		}
		return nullptr;
	}
};
