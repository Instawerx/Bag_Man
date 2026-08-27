// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLPassProgressComponent.h"

#include "AFLLiveOps.h"
#include "AFLPassSeasonAsset.h"
#include "Net/UnrealNetwork.h"

UAFLPassProgressComponent::UAFLPassProgressComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAFLPassProgressComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// OWNER ONLY. A player's pass tier is their own business, and replicating every player's
	// progression to every client is bandwidth spent on something no one can see. Mirrors the
	// cosmetic loadout component, which scopes subscription state the same way.
	DOREPLIFETIME_CONDITION(UAFLPassProgressComponent, Progress, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UAFLPassProgressComponent, Season, COND_OwnerOnly);
}

FAFLPassProgress UAFLPassProgressComponent::GetProgressForCurrentSeason() const
{
	if (!Season || !Progress.IsForSeason(Season->SeasonId))
	{
		// STALE PROGRESS IS RETURNED AS ZERO, not as-is. Tier 47 of a finished season renders
		// identically to tier 47 of this one, so handing the raw struct back would show a player
		// progress they do not have.
		return FAFLPassProgress();
	}
	return Progress;
}

int32 UAFLPassProgressComponent::GetCurrentTier() const
{
	if (!Season)
	{
		return INDEX_NONE;
	}
	const FAFLPassProgress P = GetProgressForCurrentSeason();
	return Season->TierForXp(P.Xp);
}

void UAFLPassProgressComponent::GetTierProgress(int32& OutXpIntoTier, int32& OutXpForNextTier) const
{
	OutXpIntoTier = 0;
	OutXpForNextTier = 0;

	if (!Season || Season->GetTierCount() == 0)
	{
		return;
	}

	const int32 Xp = GetProgressForCurrentSeason().Xp;
	const int32 Tier = Season->TierForXp(Xp);
	if (Tier == INDEX_NONE)
	{
		return;
	}

	const int32 ThisThreshold = Season->XpForTier(Tier);
	OutXpIntoTier = Xp - ThisThreshold;

	// At the top of the ladder BOTH stay 0 rather than reporting a next tier that does not exist --
	// a UI dividing by a phantom "next" would render a bar that can never fill.
	if (Tier + 1 < Season->GetTierCount())
	{
		OutXpForNextTier = Season->XpForTier(Tier + 1) - ThisThreshold;
	}
}

void UAFLPassProgressComponent::ServerSetSeason(UAFLPassSeasonAsset* InSeason)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogAFLLiveOps, Warning, TEXT("ServerSetSeason called without authority -- ignored."));
		return;
	}

	Season = InSeason;

	// A NEW SEASON RESETS PROGRESS; the SAME season does not. Resetting unconditionally would wipe a
	// player's tier every time the component was pointed at the season it already had -- on a
	// respawn, a reconnect, or any re-init.
	if (InSeason && !Progress.IsForSeason(InSeason->SeasonId))
	{
		Progress = FAFLPassProgress();
		Progress.SeasonId = InSeason->SeasonId;
	}

	OnProgressChanged.Broadcast();
}

int32 UAFLPassProgressComponent::ServerGrantXp(const int32 Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogAFLLiveOps, Warning, TEXT("ServerGrantXp called without authority -- ignored."));
		return GetCurrentTier();
	}
	if (!Season)
	{
		UE_LOG(LogAFLLiveOps, Warning, TEXT("ServerGrantXp with no season set -- nothing to score against."));
		return INDEX_NONE;
	}
	if (Amount <= 0)
	{
		// REFUSED, not clamped. A caller passing 0 or negative has a bug; treating it as a no-op
		// hides it, and a negative would otherwise walk a player BACKWARDS down a paid ladder.
		UE_LOG(LogAFLLiveOps, Warning,
			TEXT("ServerGrantXp REFUSED amount=%d -- must be > 0."), Amount);
		return GetCurrentTier();
	}

	if (!Progress.IsForSeason(Season->SeasonId))
	{
		Progress = FAFLPassProgress();
		Progress.SeasonId = Season->SeasonId;
	}

	const int32 Before = Season->TierForXp(Progress.Xp);

	// Saturating add. An overflow here would wrap to a negative and read as a tier reset.
	Progress.Xp = (Progress.Xp > MAX_int32 - Amount) ? MAX_int32 : Progress.Xp + Amount;

	const int32 After = Season->TierForXp(Progress.Xp);

	if (After != Before)
	{
		UE_LOG(LogAFLLiveOps, Display, TEXT("AFL_PASS: %s tier %d -> %d (xp=%d, season=%s)"),
			*GetNameSafe(GetOwner()), Before, After, Progress.Xp, *Season->SeasonId.ToString());
	}

	OnProgressChanged.Broadcast();
	return After;
}

void UAFLPassProgressComponent::ServerSetPremiumHeld(const bool bHeld)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogAFLLiveOps, Warning, TEXT("ServerSetPremiumHeld called without authority -- ignored."));
		return;
	}
	if (Progress.bPremiumHeld == bHeld)
	{
		return;
	}
	Progress.bPremiumHeld = bHeld;
	OnProgressChanged.Broadcast();
}

void UAFLPassProgressComponent::OnRep_Progress()
{
	OnProgressChanged.Broadcast();
}
