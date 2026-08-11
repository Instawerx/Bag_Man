// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/Lobby/AFLW_Lobby_DetailPanel.h"

#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetSwitcher.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_Lobby_DetailPanel)

#define LOCTEXT_NAMESPACE "AFLLobbyDetail"

// ══════════════════════════════════════════════════════════════════════════════════════════════════════
//  ONE RUNG
// ══════════════════════════════════════════════════════════════════════════════════════════════════════

void UAFLW_Lobby_PayoutRow::SetRung(const FAFLPayoutRung& Rung, const FString& CurrencySuffix)
{
	static const TCHAR* Ordinals[] = { TEXT("1st"), TEXT("2nd"), TEXT("3rd"), TEXT("4th"), TEXT("5th"),
	                                   TEXT("6th"), TEXT("7th"), TEXT("8th"), TEXT("9th"), TEXT("10th") };

	if (PlaceText)
	{
		PlaceText->SetText(Rung.Place >= 1 && Rung.Place <= 10
			? FText::FromString(Ordinals[Rung.Place - 1])
			: FText::AsNumber(Rung.Place));
	}
	// ⚠ EVERY FIGURE CARRIES THE `~`. It is not decoration: the ladder is solved for a field size that is
	// not final until the match starts, so an unqualified number here is a promise the game has not made.
	if (PayoutText)
	{
		PayoutText->SetText(FText::Format(LOCTEXT("RungPayout", "~{0} {1}"),
			FText::AsNumber(Rung.Amount), FText::FromString(CurrencySuffix)));
	}
	if (MultipleText)
	{
		MultipleText->SetText(FText::Format(LOCTEXT("RungMultiple", "~{0}x"),
			FText::AsNumber(FMath::RoundToFloat(Rung.Multiple * 100.f) / 100.f)));
	}
	if (ShareText)
	{
		ShareText->SetText(FText::Format(LOCTEXT("RungShare", "~{0}% of pool"),
			FText::AsNumber(FMath::RoundToFloat(Rung.SharePercent * 10.f) / 10.f)));
	}

	BP_OnRungSet(Rung.bIsMinCash);
}

// ══════════════════════════════════════════════════════════════════════════════════════════════════════
//  THE PANEL
// ══════════════════════════════════════════════════════════════════════════════════════════════════════

void UAFLW_Lobby_DetailPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Same no-param FCommonButtonEvent idiom as the rest of the lobby -- never AddDynamic.
	if (OverviewTab) { OverviewTab->OnClicked().AddWeakLambda(this, [this] { SelectTab(EAFLQueueDetailTab::Overview); }); }
	if (PayoutsTab)  { PayoutsTab->OnClicked().AddWeakLambda(this,  [this] { SelectTab(EAFLQueueDetailTab::Payouts); }); }
	if (RulesTab)    { RulesTab->OnClicked().AddWeakLambda(this,    [this] { SelectTab(EAFLQueueDetailTab::Rules); }); }

	if (VenueNote)
	{
		VenueNote->SetText(LOCTEXT("VenueAtStart", "Venue assigned at match start"));
	}
	SelectTab(EAFLQueueDetailTab::Overview);
}

void UAFLW_Lobby_DetailPanel::ResolveField(const FAFLLobbyQueue& InQueue, int32 InStakePerPlayer,
	int32& OutPositions, int32& OutEntryPerPosition)
{
	OutPositions = InQueue.Positions;

	// ⚠ A TEAM IS ONE POSITION. Match Play resolves over exactly two positions however many players are in
	// it, so a 5v5's position is a team of five and its entry is five stakes. Dividing slots by positions
	// gets the team size without the panel needing to know the ruleset -- and it stays correct for BR,
	// where positions == slots for solo and slots/squad-size otherwise.
	const int32 PlayersPerPosition = (InQueue.Positions > 0)
		? FMath::Max(1, InQueue.Slots / InQueue.Positions)
		: 1;

	OutEntryPerPosition = InStakePerPlayer * PlayersPerPosition;
}

void UAFLW_Lobby_DetailPanel::SetQueue(const FAFLLobbyQueue& InQueue, int32 InStakePerPlayer, const FText& BandLabel)
{
	Queue = InQueue;
	StakePerPlayer = InStakePerPlayer;
	Band = BandLabel;

	int32 Positions = 0, EntryPerPosition = 0;
	ResolveField(Queue, StakePerPlayer, Positions, EntryPerPosition);
	Ladder = FAFLPayoutSolver::Solve(Positions, EntryPerPosition);

	if (TitleText)
	{
		TitleText->SetText(FText::Format(LOCTEXT("DetailTitle", "{0} · {1}"),
			Queue.Ruleset == EAFLRuleset::BattleRoyale
				? LOCTEXT("RulesetBR", "BATTLE ROYALE")
				: LOCTEXT("RulesetMP", "MATCH PLAY"),
			FText::FromString(Queue.Bracket.ToUpper())));
	}
	if (BandText)
	{
		// Collapsed rather than blank on the league route: there is no buy-in, so there is no band, and an
		// empty band line under the title is the greyed-out stake field R98 removed wearing a smaller hat.
		BandText->SetText(BandLabel);
		BandText->SetVisibility(BandLabel.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	RebuildLadder();
	RefreshBodies();
	BP_OnQueueShown(Queue, Ladder.bWinnerTakesAll);
}

void UAFLW_Lobby_DetailPanel::ShowNoSelection(const FText& Reason)
{
	// Every field is written, including the ones that would otherwise keep a previous queue's numbers.
	// A dash is the honest reading for a metric that has no value yet -- not "0", which is a claim.
	const FText Dash = LOCTEXT("DetailNoValue", "—");

	if (TitleText)    { TitleText->SetText(Reason); }
	if (BandText)     { BandText->SetText(FText::GetEmpty()); }
	if (StakeValue)   { StakeValue->SetText(Dash); }
	if (StakeSub)     { StakeSub->SetText(FText::GetEmpty()); }
	if (PoolValue)    { PoolValue->SetText(Dash); }
	if (PoolSub)      { PoolSub->SetText(FText::GetEmpty()); }
	if (PlayersValue) { PlayersValue->SetText(Dash); }
	if (PlayersSub)   { PlayersSub->SetText(FText::GetEmpty()); }
	if (OverviewText) { OverviewText->SetText(FText::GetEmpty()); }
	if (RulesText)    { RulesText->SetText(FText::GetEmpty()); }
	if (VenueNote)    { VenueNote->SetText(FText::GetEmpty()); }
	if (PayoutFootnote) { PayoutFootnote->SetText(FText::GetEmpty()); }

	// The ladder is rows spawned from a previous queue; leaving them is the same staleness in list form.
	if (PayoutLadderBox)
	{
		PayoutLadderBox->ClearChildren();
	}

	// Nothing to queue for. Enabling this would offer entry to a queue that does not exist.
	if (QueueButton)
	{
		QueueButton->SetIsEnabled(false);
	}
}

void UAFLW_Lobby_DetailPanel::SelectTab(EAFLQueueDetailTab Tab)
{
	CurrentTab = Tab;
	if (BodySwitcher)
	{
		BodySwitcher->SetActiveWidgetIndex(static_cast<int32>(Tab));
	}
	BP_OnTabChanged(Tab);
}

FString UAFLW_Lobby_DetailPanel::CurrencySuffix() const
{
	// Copy law: Volts and Watts, integers only. NEVER USD, anywhere, ever.
	switch (Queue.Tier)
	{
	case EAFLPlayTier::VoltsPlay: return TEXT("V");
	case EAFLPlayTier::WattsPlay: return TEXT("W");
	default:                      return FString();
	}
}

void UAFLW_Lobby_DetailPanel::RebuildLadder()
{
	if (!PayoutLadderBox)
	{
		return;
	}
	PayoutLadderBox->ClearChildren();
	SpawnedRungs.Reset();

	if (!Ladder.IsValid() || !PayoutRowClass)
	{
		return;
	}

	const FString Suffix = CurrencySuffix();
	for (const FAFLPayoutRung& Rung : Ladder.Rungs)
	{
		UAFLW_Lobby_PayoutRow* Row = CreateWidget<UAFLW_Lobby_PayoutRow>(this, PayoutRowClass);
		if (!Row)
		{
			continue;
		}
		Row->SetRung(Rung, Suffix);
		PayoutLadderBox->AddChild(Row);
		SpawnedRungs.Add(Row);
	}

	if (PayoutFootnote)
	{
		// The estimate disclaimer AND the rake caveat, together, because they are the same admission: these
		// figures depend on a field that is not final and a rate that is not ruled.
		PayoutFootnote->SetText(Ladder.bWinnerTakesAll
			? FText::Format(LOCTEXT("FootnoteWTA",
				"Winner takes all — {0} finishing positions resolves to one paid place. "
				"Estimated; rake is a working assumption."),
				FText::AsNumber(Ladder.Positions))
			: FText::Format(LOCTEXT("FootnotePaid",
				"{0} of {1} positions paid · min cash 1.40x, fixed as an input. "
				"Estimated for the exact field size; the field is not final until the match starts."),
				FText::AsNumber(Ladder.PaidPlaces), FText::AsNumber(Ladder.Positions)));
	}
}

void UAFLW_Lobby_DetailPanel::RefreshBodies()
{
	if (StakeValue)
	{
		StakeValue->SetText(StakePerPlayer > 0
			? FText::Format(LOCTEXT("StakeVal", "{0} {1}"), FText::AsNumber(StakePerPlayer), FText::FromString(CurrencySuffix()))
			: LOCTEXT("StakeNone", "No buy-in"));
	}
	if (StakeSub)
	{
		StakeSub->SetText(Band.IsEmpty() ? LOCTEXT("StakeSubFree", "Played for loot and Watts") : Band);
	}
	if (PoolValue)
	{
		PoolValue->SetText(Ladder.IsValid() && Ladder.Pool > 0
			? FText::Format(LOCTEXT("PoolVal", "~{0} {1}"), FText::AsNumber(Ladder.Pool), FText::FromString(CurrencySuffix()))
			: LOCTEXT("PoolNone", "—"));
	}
	if (PoolSub)
	{
		PoolSub->SetText(FText::Format(LOCTEXT("PoolSub", "est. {0} position{1}"),
			FText::AsNumber(Ladder.Positions), Ladder.Positions == 1 ? FText::GetEmpty() : FText::FromString(TEXT("s"))));
	}
	if (PlayersValue)
	{
		// Same honesty rule as the row: an unknown count NEVER renders as 0.
		PlayersValue->SetText(Queue.HasCount()
			? FText::Format(LOCTEXT("PlayersVal", "{0} waiting"), FText::AsNumber(Queue.PlayersMatching))
			: LOCTEXT("PlayersUnknown", "Count unavailable"));
	}
	if (PlayersSub)
	{
		PlayersSub->SetText(FText::Format(LOCTEXT("PlayersSub", "{0} paid"), FText::AsNumber(Ladder.PaidPlaces)));
	}

	if (OverviewText)
	{
		OverviewText->SetText(FText::Format(LOCTEXT("Overview",
			"Field {0} · {1} finishing position{2}\nWarmup 30s"),
			FText::AsNumber(Queue.Slots),
			FText::AsNumber(Ladder.Positions),
			Ladder.Positions == 1 ? FText::GetEmpty() : FText::FromString(TEXT("s"))));
	}
	if (RulesText)
	{
		// Ruleset-scoped, and EXTRACTION IS INSIDE MATCH PLAY rather than layered over it: the round manager
		// resolves a round on wiping the enemy team OR completing the central extract bank, so it belongs in
		// the ruleset's own description and not as a third tab.
		RulesText->SetText(Queue.Ruleset == EAFLRuleset::BattleRoyale
			? LOCTEXT("RulesBR",
				"Last standing · no timer · no respawn.\n"
				"Respawn is suppressed for the match.\n"
				"The playable area shrinks — it exists to force last-standing to resolve.")
			: LOCTEXT("RulesMP",
				"Two teams · first to 7 rounds · best of 13 · sides swap at half.\n"
				"A round is won by wiping the enemy team OR completing the central extract bank.\n"
				"Respawn between rounds, never within one."));
	}
}

#undef LOCTEXT_NAMESPACE
