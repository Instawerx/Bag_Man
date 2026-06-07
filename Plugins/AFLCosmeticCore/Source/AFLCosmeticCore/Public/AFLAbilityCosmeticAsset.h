// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"

#include "AFLAbilityCosmeticAsset.generated.h"

class UGameplayAbility;

/**
 * UAFLAbilityCosmeticAsset — the catalog target for an AbilityCosmetic (S-ECON-CAT 4b, ability-grant path).
 *
 * An AbilityCosmetic catalog entry's Asset (TSoftObjectPtr<UPrimaryDataAsset>) resolves to one of these.
 * It names the GameplayAbility CLASS the cosmetic grants (e.g. GA_BagMan_EMP, a GA_Grenade reskin). Why a
 * tiny asset rather than the catalog pointing at the ability directly: a GameplayAbility Blueprint is a
 * UClass, NOT a UPrimaryDataAsset, so the uniform catalog Asset field (TSoftObjectPtr<UPrimaryDataAsset>)
 * can't reference it -- and pointing at the AbilitySet would conflate "the cosmetic" with "the grant
 * bundle." This keeps a catalog row meaning "a cosmetic," consistent with the skin-color cosmetic assets.
 *
 * Lives in AFLCosmeticCore (the always-loaded catalog home); AbilityClass is a SOFT class so the ability
 * loads on demand. The grant happens via an AbilitySet (the path the Lyra grenade actually uses -- the
 * grenade is an ability, not equipment); this asset is what the catalog RESOLVES, naming which ability.
 */
UCLASS(BlueprintType)
class AFLCOSMETICCORE_API UAFLAbilityCosmeticAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The GameplayAbility this cosmetic grants/activates (SOFT -> loads on demand). For the EMP this is
	 *  GA_BagMan_EMP (a GA_Grenade reskin). The AbilitySet grants it; the catalog resolves THIS to name it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|AbilityCosmetic")
	TSoftClassPtr<UGameplayAbility> AbilityClass;
};
