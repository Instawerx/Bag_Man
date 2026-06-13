// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NameTypes.h"

#include "AFLBodyZone.generated.h"

/**
 * The six dismemberable body zones (+ None). RELOCATED to AFLCore (S4-INC3) from
 * AFLDismember so the always-loaded shared module owns it: BOTH AFLCombat (the
 * ExecCalc, which classifies the hit bone -> zone to route limb-HP absorption) and
 * AFLDismember (the data-row table) depend on AFLCore, so this avoids the circular
 * dep that putting the classifier in AFLDismember would force (AFLDismember -> AFLCombat
 * is the existing direction; AFLCombat must NOT depend on AFLDismember).
 *
 * AFLDismember's FAFLDismemberZone/UAFLDismemberZoneSet (the rich, soft-ref data rows)
 * STAY in AFLDismember and consume this enum from here.
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

namespace AFLCore
{
	/**
	 * Lightweight static classifier: hit bone -> body zone, for code that needs the
	 * zone WITHOUT the AFLDismember ZoneSet (the ExecCalc, in AFLCombat). Mirrors the
	 * BoneMatches the data table uses (SK_Mannequin bones): head; upperarm/lowerarm/hand
	 * _l/_r -> Left/RightArm; thigh/calf _l/_r -> Left/RightLeg; spine_* / pelvis -> Torso.
	 * Returns None for an unmapped bone (-> no limb routing; damage flows to the body).
	 *
	 * Inline + header-only so AFLCombat consumes it with zero link dependency beyond the
	 * AFLCore it already has. The data table's FindZoneForBone stays authoritative for the
	 * rich rows; this is the minimal classifier for the damage hot path.
	 */
	inline EAFLBodyZone BoneToZone(FName Bone)
	{
		if (Bone.IsNone())
		{
			return EAFLBodyZone::None;
		}

		// FName comparison against the canonical SK_Mannequin bone names.
		static const FName NAME_Head(TEXT("head"));
		static const FName NAME_Neck01(TEXT("neck_01"));
		static const FName NAME_Neck02(TEXT("neck_02"));

		static const FName NAME_UpperArmL(TEXT("upperarm_l"));
		static const FName NAME_LowerArmL(TEXT("lowerarm_l"));
		static const FName NAME_HandL(TEXT("hand_l"));
		static const FName NAME_UpperArmR(TEXT("upperarm_r"));
		static const FName NAME_LowerArmR(TEXT("lowerarm_r"));
		static const FName NAME_HandR(TEXT("hand_r"));

		static const FName NAME_ThighL(TEXT("thigh_l"));
		static const FName NAME_CalfL(TEXT("calf_l"));
		static const FName NAME_FootL(TEXT("foot_l"));
		static const FName NAME_ThighR(TEXT("thigh_r"));
		static const FName NAME_CalfR(TEXT("calf_r"));
		static const FName NAME_FootR(TEXT("foot_r"));

		static const FName NAME_Spine01(TEXT("spine_01"));
		static const FName NAME_Spine02(TEXT("spine_02"));
		static const FName NAME_Spine03(TEXT("spine_03"));
		static const FName NAME_Pelvis(TEXT("pelvis"));

		if (Bone == NAME_Head || Bone == NAME_Neck01 || Bone == NAME_Neck02)
		{
			return EAFLBodyZone::Head;
		}
		if (Bone == NAME_UpperArmL || Bone == NAME_LowerArmL || Bone == NAME_HandL)
		{
			return EAFLBodyZone::LeftArm;
		}
		if (Bone == NAME_UpperArmR || Bone == NAME_LowerArmR || Bone == NAME_HandR)
		{
			return EAFLBodyZone::RightArm;
		}
		if (Bone == NAME_ThighL || Bone == NAME_CalfL || Bone == NAME_FootL)
		{
			return EAFLBodyZone::LeftLeg;
		}
		if (Bone == NAME_ThighR || Bone == NAME_CalfR || Bone == NAME_FootR)
		{
			return EAFLBodyZone::RightLeg;
		}
		if (Bone == NAME_Spine01 || Bone == NAME_Spine02 || Bone == NAME_Spine03 || Bone == NAME_Pelvis)
		{
			return EAFLBodyZone::Torso;
		}
		return EAFLBodyZone::None;
	}
}
