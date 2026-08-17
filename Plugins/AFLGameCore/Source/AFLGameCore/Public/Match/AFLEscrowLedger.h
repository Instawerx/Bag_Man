// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"

/**
 * THE ESCROW LEDGER -- what this server actually took, and from whom.
 *
 * ⚠ THE REASON THIS TYPE EXISTS, and it is the whole of it:
 *
 *     AT ABANDONMENT TIME THE PLAYERS ARE GONE.
 *
 * `/settle-match` needs a playFabId and an entryAmount for every entry in the pot. Every other settlement path
 * in this codebase reads those off live state -- `GS->PlayerArray`, the reconcile-id component on each
 * PlayerState. That works at a normal match end because the players are still connected. It does NOT work for
 * the case this type was added for: a match that ends BECAUSE everyone left. By the time we notice, PlayerArray
 * is empty, the PlayerStates are destroyed, and there is nothing left to enumerate. A refund built from live
 * state at that moment is a refund to nobody.
 *
 * So the roster is snapshotted at the ONE moment it is both complete and authoritative -- when escrow is taken
 * at match start -- and the snapshot outlives the players it describes.
 *
 * THIS IS NOT A SECOND SOURCE OF TRUTH FOR THE POT. The backend's escrow rows remain the only trustworthy
 * record of money taken (`settle-match/index.ts` -> `verifyAgainstEscrow`), and settlement still reconciles
 * against them and refuses on any disagreement. This is a record of what this server ASKED FOR, kept so that
 * the ask can still be made after the askers have disconnected.
 *
 * PLAIN C++, deliberately -- not a USTRUCT, not BlueprintReadOnly. It holds per-player currency amounts, and
 * the shortest path to a client reading a stake off a replicated property is for the type to be capable of it.
 * It is server-side plumbing that travels from the escrow call to the settlement call and nowhere else.
 *
 * Held by `TSharedPtr` because the escrow POSTs complete asynchronously and write their outcome back into it.
 */
struct FAFLEscrowedEntry
{
	/** PlayFabId-shaped reconcile key -- the same identity `/escrow-entry` was posted with. */
	FString ReconcileId;

	/** The team this player staked as. The cancel body groups by this to synthesise finishing positions. */
	int32 TeamId = INDEX_NONE;

	/** This player's share of their team's one stake unit. Integer currency (E1). */
	int32 Amount = 0;

	/**
	 * Did the `/escrow-entry` POST come back OK.
	 *
	 * Written asynchronously by the escrow completion callback, so it is false until the answer lands.
	 * ADVISORY, NOT AUTHORITATIVE: an unconfirmed entry may still have been debited (a POST that timed out
	 * may have landed anyway), which is exactly why settlement is never built from this flag -- see
	 * `FAFLMatchReporter::BuildCancelBody`, which sends the full plan regardless and lets the ledger arbitrate.
	 * Its job is to let the cancel path say, in the log, WHICH entry is going to make the backend refuse.
	 */
	bool bConfirmed = false;
};

/** The per-match escrow snapshot. One per staked match; absent (null) when nothing was staked. */
struct FAFLEscrowLedger
{
	/** The SAME id escrow, settlement and rating all join on (`FAFLMatchReporter::MatchIdToWire`). */
	FGuid MatchId;

	/** VO or WA. A match is SINGLE-CURRENCY -- the pools are sealed (R78/R81). */
	FString CurrencyCode;

	/** What ONE finishing position staked. Every team's entries sum to this, which is what makes the
	 *  backend's uniform-stake-per-position check pass by construction rather than by luck. */
	int32 StakePerPosition = 0;

	/** One per player debited. Bots never appear here -- bots are barred from staked play (R85). */
	TArray<FAFLEscrowedEntry> Entries;

	/** Was anything actually staked. An unstaked (LEAGUE PLAY) match has no ledger to settle or refund. */
	bool IsStaked() const { return StakePerPosition > 0 && Entries.Num() > 0; }

	/** How many entries the backend has told us it holds. Diagnostic -- see `bConfirmed`. */
	int32 ConfirmedCount() const
	{
		int32 Count = 0;
		for (const FAFLEscrowedEntry& E : Entries)
		{
			if (E.bConfirmed) { ++Count; }
		}
		return Count;
	}
};
