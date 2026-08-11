// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_CareerRank.h"

#include "AFLCombat.h"              // LogAFLCombat
#include "CommonTextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_CareerRank)

#define LOCTEXT_NAMESPACE "AFLCareerRank"

FText UAFLW_CareerRank::FormatRating(const FAFLCareerLadder& Ladder)
{
	// ⚠ THE WHOLE POINT OF THIS FUNCTION. An unplaced ladder reads as UNRANKED, never as 0 -- the server
	// sent null and the subsystem kept it as INDEX_NONE precisely so this line could be written.
	if (!Ladder.bPlaced || Ladder.Rating == INDEX_NONE)
	{
		return LOCTEXT("Unranked", "UNRANKED");
	}
	return FText::AsNumber(Ladder.Rating);
}

FText UAFLW_CareerRank::FormatContext(const FAFLCareerLadder& Ladder)
{
	if (Ladder.MatchCount <= 0)
	{
		// An invitation, not an absence. This ladder is the one that tells a MATCH PLAY regular that
		// BATTLE ROYALE exists, so an empty one should read as somewhere to go.
		return LOCTEXT("NeverPlayed", "not played yet");
	}
	// Matches are CONTEXT for a rating, never a score of their own -- R10 keeps the two axes apart.
	return FText::Format(LOCTEXT("MatchesFmt", "{0} matches"), FText::AsNumber(Ladder.MatchCount));
}

void UAFLW_CareerRank::NativeOnActivated()
{
	Super::NativeOnActivated();
	RequestCareer();   // re-read every time: a rating moves whenever a match settles
}

void UAFLW_CareerRank::RequestCareer()
{
	CachedCareer = FAFLCareer();
	Apply();

	UAFLCareerSubsystem* Subsystem = UAFLCareerSubsystem::Get(this);
	if (!Subsystem)
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_CAREER: no career subsystem."));
		Apply();
		return;
	}

	TWeakObjectPtr<UAFLW_CareerRank> WeakThis(this);
	Subsystem->FetchCareer(FAFLOnCareer::CreateLambda(
		[WeakThis](bool bOk, const FAFLCareer& In)
		{
			UAFLW_CareerRank* Self = WeakThis.Get();
			if (!Self)
			{
				return;   // the player left while the request was in flight
			}
			Self->CachedCareer = In;
			if (!bOk)
			{
				UE_LOG(LogAFLCombat, Warning,
					TEXT("AFL_CAREER: career unavailable -- showing that rather than an unranked player."));
			}
			Self->Apply();
			Self->BP_OnCareerLoaded(Self->CachedCareer);
		}));
}

void UAFLW_CareerRank::Apply()
{
	// Unknown is its own state. A failed fetch must not render as a player with no rank -- that is a claim
	// about them, and it is the one they would remember.
	if (!CachedCareer.bKnown)
	{
		const FText Dash = LOCTEXT("Pending", "--");
		for (UCommonTextBlock* T : { BR_Rating.Get(), BR_Context.Get(), MP_Rating.Get(), MP_Context.Get() })
		{
			if (T) { T->SetText(Dash); }
		}
		if (StatusLine)
		{
			StatusLine->SetText(LOCTEXT("CareerUnavailable", "Could not read your rank right now."));
			StatusLine->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		return;
	}

	if (StatusLine)
	{
		StatusLine->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (const FAFLCareerLadder* BR = CachedCareer.Find(EAFLRuleset::BattleRoyale))
	{
		if (BR_Rating)  { BR_Rating->SetText(FormatRating(*BR)); }
		if (BR_Context) { BR_Context->SetText(FormatContext(*BR)); }
	}
	if (const FAFLCareerLadder* MP = CachedCareer.Find(EAFLRuleset::MatchPlay))
	{
		if (MP_Rating)  { MP_Rating->SetText(FormatRating(*MP)); }
		if (MP_Context) { MP_Context->SetText(FormatContext(*MP)); }
	}
}

#undef LOCTEXT_NAMESPACE
