// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_PassTierViewer.h"

#include "AFLPassProgressComponent.h"
#include "AFLPassSeasonAsset.h"
#include "AFLCombat.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "CommonTextBlock.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

void UAFLW_PassTierRowBase::SetRowData(const FAFLPassTierRowData& In)
{
	TierIndex        = In.TierIndex;
	TierLabelText    = In.TierLabelText;
	FreeRewardId     = In.FreeRewardId;
	PremiumRewardId  = In.PremiumRewardId;
	bEarned          = In.bEarned;
	bFreeClaimed     = In.bFreeClaimed;
	bPremiumClaimed  = In.bPremiumClaimed;
	bFreeClaimable   = In.bFreeClaimable;
	bPremiumClaimable= In.bPremiumClaimable;

	OnRowDataSet();
}

UAFLPassProgressComponent* UAFLW_PassTierViewer::GetProgress() const
{
	const APlayerController* PC = GetOwningPlayer();
	APlayerState* PS = PC ? PC->PlayerState : nullptr;
	return PS ? PS->FindComponentByClass<UAFLPassProgressComponent>() : nullptr;
}

void UAFLW_PassTierViewer::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (ClaimAllButton)
	{
		ClaimAllButton->OnClicked.RemoveDynamic(this, &UAFLW_PassTierViewer::HandleClaimAllClicked);
		ClaimAllButton->OnClicked.AddDynamic(this, &UAFLW_PassTierViewer::HandleClaimAllClicked);
	}
	if (UpsellButton)
	{
		UpsellButton->OnClicked.RemoveDynamic(this, &UAFLW_PassTierViewer::HandleUpsellClicked);
		UpsellButton->OnClicked.AddDynamic(this, &UAFLW_PassTierViewer::HandleUpsellClicked);
	}

	// REBUILD ON REPLICATION, not just on open. A claim lands server-side and comes back as replicated
	// progress; without this the ladder would keep showing the pre-claim state until reopened, which
	// reads as the claim having failed.
	if (UAFLPassProgressComponent* P = GetProgress())
	{
		P->OnProgressChanged.RemoveDynamic(this, &UAFLW_PassTierViewer::HandleProgressChanged);
		P->OnProgressChanged.AddDynamic(this, &UAFLW_PassTierViewer::HandleProgressChanged);
	}

	RefreshLadder();
}

UWidget* UAFLW_PassTierViewer::NativeGetDesiredFocusTarget() const
{
	// EXISTENCE IS NOT FOCUSABILITY -- the same correction the creator needed. A panel always
	// resolves and can never take focus, so returning one hands CommonUI a dead target and the
	// genuinely focusable candidates below are never reached.
	for (UWidget* W : { static_cast<UWidget*>(ClaimAllButton), static_cast<UWidget*>(UpsellButton) })
	{
		const TSharedPtr<SWidget> Slate = W ? W->GetCachedWidget() : nullptr;
		if (Slate.IsValid() && Slate->SupportsKeyboardFocus())
		{
			return W;
		}
	}
	return const_cast<UAFLW_PassTierViewer*>(this);
}

void UAFLW_PassTierViewer::RefreshLadder()
{
	UAFLPassProgressComponent* P = GetProgress();
	const UAFLPassSeasonAsset* S = P ? P->GetSeason() : nullptr;

	if (SeasonNameText)
	{
		SeasonNameText->SetText(S ? S->DisplayName : FText::GetEmpty());
	}
	if (TierCounterText)
	{
		TierCounterText->SetText(S
			? FText::FromString(FString::Printf(TEXT("%d / %d"), P->GetCurrentTier(), S->GetTierCount()))
			: FText::GetEmpty());
	}

	// UPSELL IS A COUNT, and it hides at zero. "Subscribe" shown to someone owed nothing is noise;
	// the honest prompt is how much is waiting.
	const int32 Owed = GetUpsellCount();
	if (UpsellText)
	{
		UpsellText->SetText(Owed > 0
			? FText::FromString(FString::Printf(TEXT("%d reward%s waiting"), Owed, Owed == 1 ? TEXT("") : TEXT("s")))
			: FText::GetEmpty());
		UpsellText->SetVisibility(Owed > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (UpsellButton)
	{
		UpsellButton->SetVisibility(Owed > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (!TierListContainer)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[PassViewer] no TierListContainer bound -- the ladder cannot render. (BindWidgetOptional)"));
		return;
	}
	TierListContainer->ClearChildren();

	if (!S || !RowClass)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[PassViewer] season=%s rowClass=%s -- %d tier(s) resolved but none can be spawned."),
			S ? TEXT("ok") : TEXT("NULL"), RowClass ? TEXT("ok") : TEXT("UNSET"),
			S ? S->GetTierCount() : 0);
		return;
	}

	for (int32 i = 0; i < S->GetTierCount(); ++i)
	{
		UAFLW_PassTierRowBase* Row = CreateWidget<UAFLW_PassTierRowBase>(GetOwningPlayer(), RowClass);
		if (!Row)
		{
			continue;
		}

		FAFLPassTierRowData D;
		D.TierIndex         = i;
		D.TierLabelText     = FText::AsNumber(i + 1);   // players count from 1
		D.FreeRewardId      = S->Tiers[i].FreeReward.CosmeticId;
		D.PremiumRewardId   = S->Tiers[i].PremiumReward.CosmeticId;
		D.bEarned           = P->IsTierEarned(i);
		D.bFreeClaimed      = P->IsTrackClaimed(i, EAFLPassTrack::Free);
		D.bPremiumClaimed   = P->IsTrackClaimed(i, EAFLPassTrack::Premium);

		// STRAIGHT FROM THE SERVER'S OWN RULE. Not recomputed here -- a second implementation would
		// drift, and the drift shows up as a claim button that refuses.
		D.bFreeClaimable    = P->IsTrackClaimable(i, EAFLPassTrack::Free);
		D.bPremiumClaimable = P->IsTrackClaimable(i, EAFLPassTrack::Premium);

		Row->SetRowData(D);
		TierListContainer->AddChild(Row);
	}

	UE_LOG(LogAFLCombat, Log, TEXT("[PassViewer] ladder rebuilt: %d row(s), tier=%d, upsell=%d"),
		S->GetTierCount(), P->GetCurrentTier(), Owed);
}

void UAFLW_PassTierViewer::RequestClaimTier(const int32 TierIndex)
{
	if (UAFLPassProgressComponent* P = GetProgress())
	{
		// The server decides what tier N owes and whether it was earned. This only asks -- see
		// ServerClaimTier's note on why a claim names a TIER and never a reward.
		P->ServerClaimTier(TierIndex);
		RefreshLadder();
	}
}

void UAFLW_PassTierViewer::RequestClaimAll()
{
	if (UAFLPassProgressComponent* P = GetProgress())
	{
		P->ServerClaimAllEarned();
		RefreshLadder();
	}
}

int32 UAFLW_PassTierViewer::GetUpsellCount() const
{
	const UAFLPassProgressComponent* P = GetProgress();
	return P ? P->GetUnclaimablePremiumCount() : 0;
}

void UAFLW_PassTierViewer::HandleClaimAllClicked()  { RequestClaimAll(); }
void UAFLW_PassTierViewer::HandleUpsellClicked()    { OnUpsellRequested(); }
void UAFLW_PassTierViewer::HandleProgressChanged()  { RefreshLadder(); }
