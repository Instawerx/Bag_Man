// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "AFLFullBodyIKComponent.generated.h"

class UAnimInstance;

/**
 * UAFLFullBodyIKComponent -- Pro Mod / Melee full-body IK delivery (IRONICS_GAME_MODES_SSOT, Phase 1B).
 *
 * The true-FBIK layer that the stock rig lacks. It applies CR_ProMod_FBIK (a Full-Body-IK / PBIK Control
 * Rig over the shared SK_Mannequin) as a PER-COMPONENT POST-PROCESS on this pawn's mesh -- so it is scoped
 * to Pro pawns only and never touches the shared ABP_Mannequin_Base or CR_AFL_IRONICS (Risk F.6 isolation).
 *
 * The override post-process AnimBP property (USkeletalMeshComponent::OverridePostProcessAnimBP) is transient
 * (runtime-only, never serialized), so it MUST be set at runtime -- hence this component rather than a BP
 * property. Added to Pro pawns via the ProMod/Melee experience GameFeatureAction_AddComponents (the same
 * seam as UAFLWeaponIKComponent), so stock/Haywire pawns are untouched by construction.
 *
 * v1: applies the post-process ABP at BeginPlay (identity solve until effectors are driven).
 * v2 (planned): ground-trace foot targets -> drive CR_ProMod_FBIK's foot effectors for terrain planting.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLMOVEMENT_API UAFLFullBodyIKComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLFullBodyIKComponent();

	virtual void BeginPlay() override;

	/** The post-process AnimBP that runs CR_ProMod_FBIK. Applied to the owner's mesh at BeginPlay. Defaults
	 *  to /Game/BagMan/ProMod/ABP_ProMod_FBIK_PP; overridable per Pro variant in the details panel. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|FBIK")
	TSoftClassPtr<UAnimInstance> PostProcessABP;
};
