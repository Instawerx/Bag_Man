// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/Lobby/AFLW_TicketReview.h"

#include "AFLCombat.h"              // LogAFLCombat
#include "AFLMatchmakingSubsystem.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/ProgressBar.h"
#include "Engine/GameInstance.h"
#include "Internationalization/Text.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_TicketReview)

#define LOCTEXT_NAMESPACE "AFLTicketReview"

namespace
{
	/** Group digits. A four-figure stake read as `12480` is a different number at a glance than `12,480`. */
	FText Grouped(int64 Value)
	{
		FNumberFormattingOptions Opts;
		Opts.SetUseGrouping(true);
		return FText::AsNumber(Value, &Opts);
	}

	FText CurrencyMark(bool bVolts)
	{
		return bVolts ? LOCTEXT("MarkVolts", "V") : LOCTEXT("MarkWatts", "W");
	}
}

void UAFLW_TicketReview::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// UCommonButtonBase::OnClicked() is a plain no-param FCommonButtonEvent, not a dynamic delegate --
	// AddUObject, never AddDynamic. Same note as the home screen and the lobby root.
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().AddUObject(this, &UAFLW_TicketReview::Confirm);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().AddUObject(this, &UAFLW_TicketReview::Cancel);
	}

	// Shut until the guardrails are on screen. See the header: a commit taken without them is the skip R22
	// forbids, and the player could not tell the difference.
	SetBusy(true);
}

void UAFLW_TicketReview::NativeOnActivated()
{
	Super::NativeOnActivated();

	// Re-read on every activation rather than trusting what was fetched when the widget was built. The
	// window moves, the balance moves, and a player returning to S4 after a match must not be shown the
	// exposure they had before it.
	RequestLimits();
}

UWidget* UAFLW_TicketReview::NativeGetDesiredFocusTarget() const
{
	// ⚠ CANCEL TAKES INITIAL FOCUS, and this is the one place in the front end where the default is the
	// negative action. Everywhere else the default is what most players want (the home screen focuses
	// LEAGUE for exactly that reason). Here the default is what a mis-press should cost: this is the last
	// screen before currency moves, and a controller player who mashes A on arrival must not thereby stake
	// money. R22 makes the screen unskippable; defaulting focus to CONFIRM would let a reflex skip it.
	if (CancelButton)
	{
		return CancelButton;
	}
	return Super::NativeGetDesiredFocusTarget();
}

void UAFLW_TicketReview::SetTicket(const FString& InQueueId, int64 InTypedStake, bool bInVolts,
                                   const FText& InQueueLabel, const FText& InBandLabel)
{
	QueueId    = InQueueId;
	TypedStake = InTypedStake;
	bVolts     = bInVolts;
	QueueLabel = InQueueLabel;
	BandLabel  = InBandLabel;

	if (QueueLine) { QueueLine->SetText(QueueLabel); }
	if (BandLine)
	{
		// §5.2 requires the band be DISCLOSED. A player must never believe they will be matched at exactly
		// the figure they typed -- a surprise about money costs trust permanently.
		BandLine->SetText(BandLabel);
		BandLine->SetVisibility(BandLabel.IsEmpty() ? ESlateVisibility::Collapsed
		                                            : ESlateVisibility::SelfHitTestInvisible);
	}
	if (StakeValue)
	{
		StakeValue->SetText(FText::Format(LOCTEXT("StakeFmt", "{0} {1}"), Grouped(TypedStake), CurrencyMark(bVolts)));
	}

	ApplyLimits();
}

void UAFLW_TicketReview::RequestLimits()
{
	bLimitsKnown = false;
	SetBusy(true);
	ApplyLimits();

	UAFLPlayLimitsSubsystem* Subsystem = UAFLPlayLimitsSubsystem::Get(this);
	if (!Subsystem)
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_S4: no play-limits subsystem -- CONFIRM stays shut."));
		ApplyLimits();
		return;
	}

	TWeakObjectPtr<UAFLW_TicketReview> WeakThis(this);
	Subsystem->FetchLimits(FAFLOnPlayLimits::CreateLambda(
		[WeakThis](bool bOk, const FAFLPlayLimits& InLimits)
		{
			UAFLW_TicketReview* Self = WeakThis.Get();
			if (!Self)
			{
				return;   // the player left while the request was in flight
			}
			Self->Limits = InLimits;
			Self->bLimitsKnown = bOk;
			if (!bOk)
			{
				// NOT a silent degradation to "no limits". The screen stays shut and says why.
				UE_LOG(LogAFLCombat, Warning,
					TEXT("AFL_S4: play limits unavailable -- refusing to present a commit without them."));
			}
			Self->SetBusy(false);
			Self->ApplyLimits();
		}));
}

bool UAFLW_TicketReview::EvaluateEntry(const FAFLPlayLimit& Limit, int64 Stake, FText& OutRefusal)
{
	OutRefusal = FText::GetEmpty();
	if (!Limit.bKnown)
	{
		OutRefusal = LOCTEXT("LimitsUnknown", "Checking your limits...");
		return false;
	}
	if (Stake > Limit.Balance)
	{
		OutRefusal = LOCTEXT("Insufficient", "Not enough in your balance for this entry.");
		return false;
	}
	// Entry cap FIRST -- it is the one the player can act on immediately by staking less. The ceiling means
	// come back later, which is a worse thing to say when a smaller number would have worked.
	if (Stake > Limit.EntryCap)
	{
		OutRefusal = FText::Format(
			LOCTEXT("OverEntryCap", "Over your cap for a single entry ({0} max)."), Grouped(Limit.EntryCap));
		return false;
	}
	if (Limit.WindowStaked + Stake > Limit.WindowCeiling)
	{
		OutRefusal = FText::Format(
			LOCTEXT("OverWindow", "This would pass your limit for the period. {0} left."),
			Grouped(Limit.WindowRemaining));
		return false;
	}
	return true;
}

bool UAFLW_TicketReview::IsEntryPermitted() const
{
	FText Unused;
	return bLimitsKnown && EvaluateEntry(Limits.ForCurrency(bVolts), TypedStake, Unused);
}

void UAFLW_TicketReview::ApplyLimits()
{
	const FAFLPlayLimit& Limit = Limits.ForCurrency(bVolts);
	FText Refusal;
	const bool bPermitted = bLimitsKnown && EvaluateEntry(Limit, TypedStake, Refusal);
	if (!bLimitsKnown && Refusal.IsEmpty())
	{
		Refusal = LOCTEXT("LimitsUnavailable", "Could not check your limits. Try again in a moment.");
	}

	if (BalanceValue)
	{
		// §6 shows the balance AFTER as well as before, because "what will I have left" is the question a
		// player is actually asking and arithmetic under pressure is where people misread a stake.
		BalanceValue->SetText(Limit.bKnown
			? FText::Format(LOCTEXT("BalanceFmt", "{0} {2}  →  {1} {2}"),
				Grouped(Limit.Balance), Grouped(Limit.Balance - TypedStake), CurrencyMark(bVolts))
			: LOCTEXT("BalanceUnknown", "—"));
	}

	if (SessionMeterLabel)
	{
		// The meter reports LOSS, and the label says so in words rather than leaving the player to guess
		// which quantity a bar is filling with. The ceiling it is measured against binds on amount STAKED
		// -- two quantities on one control, so the wording has to carry the distinction.
		SessionMeterLabel->SetText(Limit.bKnown
			? FText::Format(LOCTEXT("MeterFmt", "lost {0} of {1} {2} limit · last {3}h"),
				Grouped(Limit.WindowLoss), Grouped(Limit.WindowCeiling), CurrencyMark(bVolts),
				FText::AsNumber(Limits.WindowHours))
			: LOCTEXT("MeterUnknown", "—"));
	}
	if (SessionMeter)
	{
		// Filled by STAKED-plus-this-entry, which is what the ceiling actually binds on, so the bar
		// predicts the refusal rather than trailing it. VISIBLE BEFORE IT BINDS is the whole §7 point.
		const float Fill = (Limit.bKnown && Limit.WindowCeiling > 0)
			? FMath::Clamp(static_cast<float>(Limit.WindowStaked + TypedStake) / static_cast<float>(Limit.WindowCeiling), 0.f, 1.f)
			: 0.f;
		SessionMeter->SetPercent(Fill);
	}

	if (EntryCapLabel)
	{
		EntryCapLabel->SetText(Limit.bKnown
			? FText::Format(LOCTEXT("CapFmt", "cap this entry: {0} {1} max"),
				Grouped(Limit.EntryCap), CurrencyMark(bVolts))
			: LOCTEXT("CapUnknown", "—"));
	}

	if (RefusalLine)
	{
		RefusalLine->SetText(Refusal);
		RefusalLine->SetVisibility(Refusal.IsEmpty() ? ESlateVisibility::Collapsed
		                                             : ESlateVisibility::SelfHitTestInvisible);
	}
	if (ConfirmButton)
	{
		ConfirmButton->SetIsInteractionEnabled(bPermitted && !bCommitting);
	}

	BP_OnLimitsChanged(Limit, bPermitted);
}

void UAFLW_TicketReview::SetBusy(bool bInBusy)
{
	if (ConfirmButton)
	{
		ConfirmButton->SetIsInteractionEnabled(!bInBusy && IsEntryPermitted());
	}
	// CANCEL IS NEVER DISABLED, including mid-commit. A player must always be able to leave a money screen;
	// a surface that can trap someone while it talks to a server is worse than one that occasionally
	// cancels a request nobody wanted.
}

void UAFLW_TicketReview::Confirm()
{
	if (bCommitting)
	{
		return;   // a second press is a double-click, not a second entry
	}

	// ⚠ RE-CHECKED HERE, not trusted from the button's appearance. SetIsInteractionEnabled is presentation,
	// and a gamepad or accessibility path can still deliver the press -- the same argument ChooseDoor and
	// OpenNavTarget each make. On a money surface it is the difference between a disabled control and an
	// enforced rule.
	FText Refusal;
	if (!bLimitsKnown || !EvaluateEntry(Limits.ForCurrency(bVolts), TypedStake, Refusal))
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_S4: confirm refused -- %s"),
			Refusal.IsEmpty() ? TEXT("limits unknown") : *Refusal.ToString());
		ApplyLimits();
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UAFLMatchmakingSubsystem* Matchmaking =
		GameInstance ? GameInstance->GetSubsystem<UAFLMatchmakingSubsystem>() : nullptr;
	if (!Matchmaking)
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_S4: no matchmaking subsystem -- cannot enter %s."), *QueueId);
		return;
	}

	bCommitting = true;
	if (ConfirmButton)
	{
		ConfirmButton->SetIsInteractionEnabled(false);
	}

	// The TYPED figure travels, unrounded (R59). The server resolves the band, re-checks the balance AND
	// re-applies both guardrails against its own numbers -- everything this screen showed is a preview of
	// that decision, never a substitute for it.
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_S4: confirmed %s at %lld -- entering matchmaking."), *QueueId, TypedStake);
	Matchmaking->StartMatchmaking(QueueId, static_cast<int32>(TypedStake));

	OnTicketConfirmed.Broadcast(QueueId, TypedStake);
}

void UAFLW_TicketReview::Cancel()
{
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_S4: cancelled %s -- nothing staked."), *QueueId);
	OnTicketCancelled.Broadcast();
	DeactivateWidget();   // pops this screen off UI.Layer.Menu; the lobby is still underneath
}

#undef LOCTEXT_NAMESPACE
