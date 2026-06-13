// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "AFLDismemberTypes.generated.h"

class AAFLDismemberedPart;
class UGameplayEffect;
class UNiagaraSystem;

/**
 * S4-INC1: the six dismemberable body zones (+ None). The killing-blow bone is
 * mapped to one of these via FAFLDismemberZone::BoneMatches; the head slice
 * (proven) is Head, the leg-consequence vehicle is Left/RightLeg.
 */
UENUM(BlueprintType)
enum class EAFLBodyZone : uint8
{
	None      UMETA(DisplayName = "None"),
	Head      UMETA(DisplayName = "Head"),
	LeftArm   UMETA(DisplayName = "Left Arm"),
	RightArm  UMETA(DisplayName = "Right Arm"),
	LeftLeg   UMETA(DisplayName = "Left Leg"),
	RightLeg  UMETA(DisplayName = "Right Leg"),
	Torso     UMETA(DisplayName = "Torso"),
};

/**
 * S4-INC1: one row of the data-driven dismember table. Generalizes the proven
 * hard-coded head path (UAFLDismemberComponent::OnOverkill) into data:
 *   - SeveredBone is what HideBoneByName(PBO_Term) hides + GetSocketTransform reads
 *     (the proven head used "head"; limbs use upperarm_l/r, thigh_l/r, spine_01).
 *   - BoneMatches maps the killing-blow bone to this zone (LeftArm matches
 *     upperarm_l/lowerarm_l/hand_l, so any of those resolves to the LeftArm row).
 *   - PropClass is the decoupled physics prop, SOFT (async-load before spawn) so
 *     no per-class hard ref is baked into the component.
 *   - ImpulseXYRange/ImpulseZ reproduce the proven head pop (+-100 XY, +500 Z).
 *   - CueTag is the cosmetic GameplayCue bundle (GameplayCue.Combat.Dismember.<Zone>).
 *   - ConsequenceGE grants State.Dismembered.<Zone>; EMPTY for lethal-cosmetic zones
 *     (head/torso = the pawn dies, no surviving consequence). SOFT.
 *   - SparkFX is the AFL-0406 Niagara slot (declared now, unused this increment). SOFT.
 *
 * AAA: every asset reference is a soft pointer (TSoftClassPtr/TSoftObjectPtr) ->
 * the table holds no hard refs; the component async-loads what it needs at sever time.
 */
USTRUCT(BlueprintType)
struct AFLDISMEMBER_API FAFLDismemberZone
{
	GENERATED_BODY()

	/** Which zone this row describes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dismember")
	EAFLBodyZone Zone = EAFLBodyZone::None;

	/** The joint to hide (HideBoneByName) + read for the prop spawn transform. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dismember")
	FName SeveredBone = NAME_None;

	/** Hit bones that map to this zone (linear-scanned by FindZoneForBone). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dismember")
	TArray<FName> BoneMatches;

	/** The decoupled physics prop spawned at the severed-bone transform (soft, async-load). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dismember")
	TSoftClassPtr<AAFLDismemberedPart> PropClass;

	/** Randomized planar pop applied to the prop -- matches the proven head +-100. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dismember")
	FVector2D ImpulseXYRange = FVector2D(-100.0f, 100.0f);

	/** Vertical pop applied to the prop -- matches the proven head +500. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dismember")
	float ImpulseZ = 500.0f;

	/** Cosmetic cue bundle fired at detach (GameplayCue.Combat.Dismember.<Zone>). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dismember")
	FGameplayTag CueTag;

	/** GE granting State.Dismembered.<Zone> (soft). EMPTY for lethal-cosmetic zones. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dismember")
	TSoftClassPtr<UGameplayEffect> ConsequenceGE;

	/** AFL-0406 spark/wire/emissive Niagara at detach (soft). Declared now, unused this increment. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dismember")
	TSoftObjectPtr<UNiagaraSystem> SparkFX;
};
