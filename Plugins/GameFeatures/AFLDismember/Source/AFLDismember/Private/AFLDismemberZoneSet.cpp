// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDismemberZoneSet.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDismemberZoneSet)

const FAFLDismemberZone* UAFLDismemberZoneSet::FindZoneForBone(FName Bone) const
{
	if (Bone.IsNone())
	{
		return nullptr;
	}

	// Linear scan: small fixed table (<= 6 zones), authored once. Each row claims a
	// set of hit bones; the first row whose BoneMatches contains Bone wins.
	for (const FAFLDismemberZone& Row : Zones)
	{
		if (Row.BoneMatches.Contains(Bone))
		{
			return &Row;
		}
	}
	return nullptr;
}
