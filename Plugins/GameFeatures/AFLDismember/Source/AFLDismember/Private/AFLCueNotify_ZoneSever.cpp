// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLCueNotify_ZoneSever.h"

#include "AFLDismember.h"
#include "AFLDismemberComponent.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCueNotify_ZoneSever)

namespace
{
	// The cue tag is GameplayCue.Combat.Dismember.State.<Zone>; map the LEAF name to the zone.
	// Driven off the cue tag (not a separate enum on the notify) so one notify serves all zones.
	EAFLBodyZone ZoneFromCueTag(const FGameplayTag& Tag)
	{
		if (!Tag.IsValid())
		{
			return EAFLBodyZone::None;
		}
		const FString Full = Tag.ToString();   // e.g. "GameplayCue.Combat.Dismember.State.Head"
		FString Leaf = Full;
		int32 DotIdx;
		if (Full.FindLastChar(TEXT('.'), DotIdx))
		{
			Leaf = Full.RightChop(DotIdx + 1);
		}
		if (Leaf == TEXT("Head"))     { return EAFLBodyZone::Head; }
		if (Leaf == TEXT("LeftArm"))  { return EAFLBodyZone::LeftArm; }
		if (Leaf == TEXT("RightArm")) { return EAFLBodyZone::RightArm; }
		if (Leaf == TEXT("LeftLeg"))  { return EAFLBodyZone::LeftLeg; }
		if (Leaf == TEXT("RightLeg")) { return EAFLBodyZone::RightLeg; }
		return EAFLBodyZone::None;
	}

	// The per-event cue tag GAS matched. Prefer OriginalTag (the exact added tag, .State.<Zone>);
	// fall back to MatchedTagName. BOTH are FGameplayTag (engine FGameplayCueParameters, UE5.6).
	FGameplayTag ResolveCueTag(const FGameplayCueParameters& Parameters)
	{
		if (Parameters.OriginalTag.IsValid())
		{
			return Parameters.OriginalTag;
		}
		if (Parameters.MatchedTagName.IsValid())
		{
			return Parameters.MatchedTagName;
		}
		return FGameplayTag();
	}

	void DriveHide(AActor* MyTarget, const FGameplayCueParameters& Parameters, bool bHide)
	{
		if (!MyTarget)
		{
			return;
		}
		const EAFLBodyZone Zone = ZoneFromCueTag(ResolveCueTag(Parameters));
		if (Zone == EAFLBodyZone::None)
		{
			UE_LOG(LogAFLDismember, Warning,
				TEXT("[AFLDismember] ZoneSever cue on %s but tag->zone unresolved -- no cosmetic"),
				*GetNameSafe(MyTarget));
			return;
		}
		if (UAFLDismemberComponent* Dismember = MyTarget->FindComponentByClass<UAFLDismemberComponent>())
		{
			if (bHide) { Dismember->ApplyZoneHideCosmetic(Zone); }
			else       { Dismember->ApplyZoneRestoreCosmetic(Zone); }
		}
		else
		{
			UE_LOG(LogAFLDismember, Warning,
				TEXT("[AFLDismember] ZoneSever cue on %s but no UAFLDismemberComponent -- no cosmetic"),
				*GetNameSafe(MyTarget));
		}
	}
}

UAFLCueNotify_ZoneSever::UAFLCueNotify_ZoneSever()
{
	// DO NOT set GameplayCueTag here. UAbilitySystemGlobals::DeriveGameplayCueTagFromClass RESETS a BP
	// child's tag whenever it EQUALS the parent CDO's tag, then re-derives from the asset NAME -- so a
	// tag set on this C++ parent CDO actively POISONS the BP child's tag (it gets cleared + mis-derived
	// to GameplayCue.AFL.Dismember.State, and GameplayCueName ends up empty -> the cue is UNDISCOVERABLE).
	// Instead the BP child is NAMED per the derive convention (GC_Combat_Dismember_State -> strip GC_ ->
	// Combat.Dismember.State -> prepend GameplayCue. -> GameplayCue.Combat.Dismember.State), which auto-
	// populates BOTH GameplayCueTag and the asset-registry GameplayCueName on load. Parent CDO tag stays
	// empty; routing still works because every .State.<Zone> child cue finds this notify by the BP's tag.
}

bool UAFLCueNotify_ZoneSever::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	// Cue ADDED -> hide on this client (server + every remote). WhileActive covers late-join.
	DriveHide(MyTarget, Parameters, /*bHide=*/true);
	return Super::OnActive_Implementation(MyTarget, Parameters);
}

bool UAFLCueNotify_ZoneSever::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	// Late-join / relevancy-rejoin catch-up: a client that receives the already-present cue container
	// gets WhileActive (not OnActive) -- hide here too so a player who joins AFTER the sever still sees
	// the headless body. Idempotent (HideBoneByName on an already-hidden bone is a no-op).
	DriveHide(MyTarget, Parameters, /*bHide=*/true);
	return Super::WhileActive_Implementation(MyTarget, Parameters);
}

bool UAFLCueNotify_ZoneSever::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	// Cue REMOVED (server RemoveGameplayCue at reattach) -> un-hide on every client. Symmetric.
	DriveHide(MyTarget, Parameters, /*bHide=*/false);
	return Super::OnRemove_Implementation(MyTarget, Parameters);
}
