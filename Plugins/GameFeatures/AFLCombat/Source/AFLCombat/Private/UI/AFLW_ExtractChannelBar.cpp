// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_ExtractChannelBar.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Cosmetics/AFLWalletComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_ExtractChannelBar)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Extracting_Bar, "State.Extracting");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_Complete_Bar, "Event.Extraction.Complete");


void UAFLW_ExtractChannelBar::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
	TryArm();
}

void UAFLW_ExtractChannelBar::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ArmRetryTimer);
		World->GetTimerManager().ClearTimer(CollapseTimer);
	}
	if (ASC.IsValid() && ExtractingTagHandle.IsValid())
	{
		ASC->RegisterGameplayTagEvent(TAG_State_Extracting_Bar, EGameplayTagEventType::NewOrRemoved).Remove(ExtractingTagHandle);
	}
	if (Wallet.IsValid())
	{
		Wallet->OnWalletChanged.RemoveDynamic(this, &UAFLW_ExtractChannelBar::HandleWalletChanged);
	}
	if (CompleteListener.IsValid())
	{
		CompleteListener.Unregister();
	}
	Super::NativeDestruct();
}

void UAFLW_ExtractChannelBar::TryArm()
{
	const APlayerController* PC = GetOwningPlayer();
	APlayerState* PS = PC ? PC->PlayerState : nullptr;
	UAbilitySystemComponent* ResolvedASC = PS
		? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS) : nullptr;
	UAFLWalletComponent* ResolvedWallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
	if (!ResolvedASC || !ResolvedWallet)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(ArmRetryTimer,
				FTimerDelegate::CreateWeakLambda(this, [this] { TryArm(); }), 0.5f, false);
		}
		return;
	}
	ASC = ResolvedASC;
	Wallet = ResolvedWallet;
	LastWatts = Wallet->GetWatts();

	ExtractingTagHandle = ASC->RegisterGameplayTagEvent(TAG_State_Extracting_Bar, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UAFLW_ExtractChannelBar::HandleExtractingChanged);
	Wallet->OnWalletChanged.AddDynamic(this, &UAFLW_ExtractChannelBar::HandleWalletChanged);
	if (UWorld* World = GetWorld())
	{
		// Host-window refinement: the GA broadcasts Complete on the authority world only; on a
		// pure client this listener simply never fires and the wallet-gain heuristic decides.
		CompleteListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(
			TAG_Event_Extraction_Complete_Bar,
			[this](FGameplayTag /*Channel*/, const FLyraVerbMessage& /*Msg*/) { bSawComplete = true; });
	}
}

void UAFLW_ExtractChannelBar::HandleExtractingChanged(const FGameplayTag /*Tag*/, int32 NewCount)
{
	if (NewCount > 0)
	{
		bChanneling = true;
		bSawComplete = false;
		bSawWalletGain = false;
		Elapsed = 0.0f;
		LastWatts = Wallet.IsValid() ? Wallet->GetWatts() : LastWatts;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(CollapseTimer);
		}
		if (ChannelBar)
		{
			ChannelBar->SetPercent(0.0f);
			ChannelBar->SetFillColorAndOpacity(ChannelColor);
		}
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		bChanneling = false;
		// Outcome flash: server-confirmed green (wallet gain / Complete heard), else red.
		const bool bSucceeded = bSawComplete || bSawWalletGain;
		if (ChannelBar)
		{
			ChannelBar->SetPercent(1.0f);
			ChannelBar->SetFillColorAndOpacity(bSucceeded ? CompleteColor : FailColor);
		}
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(CollapseTimer,
				FTimerDelegate::CreateWeakLambda(this, [this] { Collapse(); }), 0.8f, false);
		}
	}
}

void UAFLW_ExtractChannelBar::HandleWalletChanged(int32 /*Volts*/, int32 Watts)
{
	if (Watts > LastWatts)
	{
		bSawWalletGain = true;
	}
	LastWatts = Watts;
}

void UAFLW_ExtractChannelBar::Collapse()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UAFLW_ExtractChannelBar::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bChanneling && ChannelBar)
	{
		Elapsed += InDeltaTime;
		ChannelBar->SetPercent(FMath::Min(Elapsed / FMath::Max(ChannelDuration, 0.1f), 1.0f));
	}
}
