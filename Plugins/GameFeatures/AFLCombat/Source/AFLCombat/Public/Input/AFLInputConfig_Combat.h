// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "Input/LyraInputConfig.h"

#include "AFLInputConfig_Combat.generated.h"

class UAFLInputAction_Fire;


/**
 * UAFLInputConfig_Combat
 *
 * AFL-0107 native data asset that maps the AFLCombat-plugin input tags to
 * their InputAction assets. Currently binds InputTag.Weapon.Fire to the
 * owned UAFLInputAction_Fire subobject; further bindings (Aim, Reload,
 * weapon-swap) land alongside their respective abilities.
 *
 * AFL-0214 will author the canonical /AFLCombat/Input/DA_AFL_InputConfig_Combat.uasset
 * as a direct ULyraInputConfig data asset and wire it onto ULyraPawnData::InputConfig.
 * This native class exists so the binding (tag -> action) is checked-in code rather
 * than living only inside an editor-authored .uasset the autonomous pipeline cannot
 * create. The shape mirrors ULyraInputConfig::AbilityInputActions exactly, so
 * AFL-0214's HeroComponent code can copy entries from this asset's AbilityInputActions
 * into the ULyraInputConfig DA at game-feature activation, or in the interim, read
 * directly from this asset.
 *
 * NOTE: we don't subclass ULyraInputConfig because that class is not exported
 * from LyraGame (no LYRAGAME_API / MinimalAPI markup on the UCLASS — unlike
 * ULyraPawnData which IS exported). Modifying LyraGame source is out of scope
 * per the project's AFL-0215 hard rails; we use the FLyraInputAction struct
 * (header-only, no linkage required) to stay schema-compatible.
 */
UCLASS(BlueprintType, Const)
class AFLCOMBAT_API UAFLInputConfig_Combat : public UDataAsset
{
	GENERATED_BODY()

public:

	UAFLInputConfig_Combat(const FObjectInitializer& ObjectInitializer);

	/** Returns the FLyraInputAction entry mapped to InputTag, or nullptr. */
	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag) const;

	/**
	 * Ability input action -> tag bindings. Schema-compatible with
	 * ULyraInputConfig::AbilityInputActions so AFL-0214 can copy entries
	 * across without translation.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Input", meta=(TitleProperty="InputAction"))
	TArray<FLyraInputAction> AbilityInputActions;

protected:

	/**
	 * Default subobject for the Fire input action. Owned by this asset's CDO
	 * so the InputTag.Weapon.Fire entry resolves to a valid UInputAction* at
	 * module load without needing an editor pass to author IA_AFL_Fire.uasset.
	 * AFL-0214 may swap this for a BP child of UAFLInputAction_Fire authored
	 * in the editor; either route is schema-compatible.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AFL|Input")
	TObjectPtr<UAFLInputAction_Fire> FireInputAction;
};
