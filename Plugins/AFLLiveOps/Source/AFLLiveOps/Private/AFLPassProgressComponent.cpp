// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLPassProgressComponent.h"

#include "AFLLiveOps.h"
#include "AFLPassRewardSink.h"
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


// ===== CLAIM (S21 slice 2) ==========================================================================

bool UAFLPassProgressComponent::ClaimOneTrack(const int32 TierIndex, const EAFLPassTrack Track)
{
	const FAFLPassTier& Tier = Season->Tiers[TierIndex];
	const FAFLPassReward& Reward =
		(Track == EAFLPassTrack::Free) ? Tier.FreeReward : Tier.PremiumReward;

	if (Reward.IsEmpty())
	{
		return false;   // nothing owed on this track -- not an error, and not a claim
	}
	if (Progress.HasClaimed(Track, TierIndex))
	{
		return false;   // idempotent: already handed over
	}

	// PREMIUM REQUIRES THE ENTITLEMENT, and a refusal must NOT mark the tier claimed.
	//
	// bPremiumHeld mirrors the conditional entitlement, which is fail-closed on the proven path --
	// AwaitingActivation means paid-but-not-live and grants nothing. Setting the bit here on a refusal
	// would consume the tier, and a player who subscribed afterwards would find the reward they paid
	// for already "claimed".
	if (Track == EAFLPassTrack::Premium && !Progress.bPremiumHeld)
	{
		return false;
	}

	if (!RewardSink)
	{
		// LOUD. A missing sink means the server can score a pass and hand nothing over -- the whole
		// system would look like it worked. Never silently skipped.
		UE_LOG(LogAFLLiveOps, Error,
			TEXT("AFL_PASS: claim tier %d (%s) has no reward sink -- NOTHING GRANTED. "
			     "SetRewardSink was never called on %s."),
			TierIndex, Track == EAFLPassTrack::Free ? TEXT("free") : TEXT("premium"),
			*GetNameSafe(GetOwner()));
		return false;
	}

	// GRANT FIRST, MARK SECOND. Marking first would make a failed grant permanent.
	const bool bGranted = RewardSink->GrantPassReward(
		Reward.CosmeticId, Reward.Quantity, TEXT("PassClaim"));

	if (!bGranted)
	{
		UE_LOG(LogAFLLiveOps, Warning,
			TEXT("AFL_PASS: grant REFUSED for tier %d (%s) reward=%s x%d -- tier stays UNCLAIMED."),
			TierIndex, Track == EAFLPassTrack::Free ? TEXT("free") : TEXT("premium"),
			*Reward.CosmeticId.ToString(), Reward.Quantity);
		return false;
	}

	FAFLPassProgress::MaskSet(
		Track == EAFLPassTrack::Free ? Progress.ClaimedFree : Progress.ClaimedPremium, TierIndex);

	UE_LOG(LogAFLLiveOps, Display, TEXT("AFL_PASS: claimed tier %d (%s) -> %s x%d"),
		TierIndex, Track == EAFLPassTrack::Free ? TEXT("free") : TEXT("premium"),
		*Reward.CosmeticId.ToString(), Reward.Quantity);
	return true;
}

int32 UAFLPassProgressComponent::ServerClaimTier(const int32 TierIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogAFLLiveOps, Warning, TEXT("ServerClaimTier called without authority -- ignored."));
		return 0;
	}
	if (!Season)
	{
		UE_LOG(LogAFLLiveOps, Warning, TEXT("ServerClaimTier with no season -- nothing to claim."));
		return 0;
	}
	if (!Season->Tiers.IsValidIndex(TierIndex))
	{
		UE_LOG(LogAFLLiveOps, Warning,
			TEXT("AFL_PASS: claim REFUSED -- tier %d out of range (0..%d)."),
			TierIndex, Season->GetTierCount() - 1);
		return 0;
	}

	// EARNED IS CHECKED SERVER-SIDE, against the server's own XP. This is the arm that stops a client
	// claiming tier 99 on its first match: the request names a tier, and the server decides whether
	// that tier has been reached.
	const int32 Earned = Season->TierForXp(GetProgressForCurrentSeason().Xp);
	if (TierIndex > Earned)
	{
		UE_LOG(LogAFLLiveOps, Warning,
			TEXT("AFL_PASS: claim REFUSED -- tier %d not earned (at tier %d)."), TierIndex, Earned);
		return 0;
	}

	int32 Granted = 0;
	if (ClaimOneTrack(TierIndex, EAFLPassTrack::Free))    { ++Granted; }
	if (ClaimOneTrack(TierIndex, EAFLPassTrack::Premium)) { ++Granted; }

	if (Granted > 0)
	{
		OnProgressChanged.Broadcast();
	}
	return Granted;
}

int32 UAFLPassProgressComponent::ServerClaimAllEarned()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Season)
	{
		return 0;
	}

	// LOWEST FIRST. Order is visible to the player through the grant log, and settling a ladder
	// top-down reads as arbitrary.
	const int32 Earned = Season->TierForXp(GetProgressForCurrentSeason().Xp);
	int32 Total = 0;
	for (int32 i = 0; i <= Earned && Season->Tiers.IsValidIndex(i); ++i)
	{
		Total += ServerClaimTier(i);
	}
	return Total;
}


// ===== TIER VIEWER / UPSELL (S21 slice 3) ===========================================================

bool UAFLPassProgressComponent::IsTierEarned(const int32 TierIndex) const
{
	if (!Season || !Season->Tiers.IsValidIndex(TierIndex))
	{
		return false;
	}
	return TierIndex <= Season->TierForXp(GetProgressForCurrentSeason().Xp);
}

bool UAFLPassProgressComponent::IsTrackClaimed(const int32 TierIndex, const EAFLPassTrack Track) const
{
	return GetProgressForCurrentSeason().HasClaimed(Track, TierIndex);
}

bool UAFLPassProgressComponent::IsTrackClaimable(const int32 TierIndex, const EAFLPassTrack Track) const
{
	if (!Season || !Season->Tiers.IsValidIndex(TierIndex) || !IsTierEarned(TierIndex))
	{
		return false;
	}

	const FAFLPassTier& Tier = Season->Tiers[TierIndex];
	const FAFLPassReward& Reward =
		(Track == EAFLPassTrack::Free) ? Tier.FreeReward : Tier.PremiumReward;

	if (Reward.IsEmpty() || IsTrackClaimed(TierIndex, Track))
	{
		return false;
	}

	// MIRRORS ServerClaimTier's OWN CONDITIONS. If this answered yes where the claim would refuse,
	// the viewer would light a button that does nothing -- and the two would drift the first time
	// either changed. Kept adjacent deliberately.
	if (Track == EAFLPassTrack::Premium && !GetProgressForCurrentSeason().bPremiumHeld)
	{
		return false;
	}
	return true;
}

int32 UAFLPassProgressComponent::GetUnclaimablePremiumCount() const
{
	const FAFLPassProgress P = GetProgressForCurrentSeason();
	if (!Season || P.bPremiumHeld)
	{
		return 0;   // nothing to sell to someone who already holds it
	}

	const int32 Earned = Season->TierForXp(P.Xp);
	int32 Count = 0;
	for (int32 i = 0; i <= Earned && Season->Tiers.IsValidIndex(i); ++i)
	{
		const FAFLPassReward& R = Season->Tiers[i].PremiumReward;
		if (!R.IsEmpty() && !P.HasClaimed(EAFLPassTrack::Premium, i))
		{
			++Count;
		}
	}
	return Count;
}
