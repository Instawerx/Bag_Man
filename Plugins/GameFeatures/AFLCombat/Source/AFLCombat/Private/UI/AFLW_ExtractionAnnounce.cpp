// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_ExtractionAnnounce.h"

#include "AFLCombat.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_ExtractionAnnounce)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_WindowOpen_Announce, "Event.Extraction.WindowOpen");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_WindowClosed_Announce, "Event.Extraction.WindowClosed");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Match_Ended_Announce, "Event.Match.Ended");


void UAFLW_ExtractionAnnounce::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);

	if (UWorld* World = GetWorld())
	{
		OpenListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(
			TAG_Event_Extraction_WindowOpen_Announce,
			[this](FGameplayTag Channel, const FLyraVerbMessage& Msg) { HandleWindowOpen(Channel, Msg); });
		ClosedListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(
			TAG_Event_Extraction_WindowClosed_Announce,
			[this](FGameplayTag Channel, const FLyraVerbMessage& Msg) { HandleWindowClosed(Channel, Msg); });
		MatchEndedListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(
			TAG_Event_Match_Ended_Announce,
			[this](FGameplayTag Channel, const FLyraVerbMessage& Msg) { HandleMatchEnded(Channel, Msg); });
	}
}

void UAFLW_ExtractionAnnounce::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CollapseTimer);
	}
	if (OpenListener.IsValid()) { OpenListener.Unregister(); }
	if (ClosedListener.IsValid()) { ClosedListener.Unregister(); }
	if (MatchEndedListener.IsValid()) { MatchEndedListener.Unregister(); }
	Super::NativeDestruct();
}

void UAFLW_ExtractionAnnounce::HandleWindowOpen(FGameplayTag /*Channel*/, const FLyraVerbMessage& /*Msg*/)
{
	Show(NSLOCTEXT("AFL", "ExtractWindowOpen", "EXTRACTION WINDOW OPEN"), OpenColor);
}

void UAFLW_ExtractionAnnounce::HandleWindowClosed(FGameplayTag /*Channel*/, const FLyraVerbMessage& /*Msg*/)
{
	Show(NSLOCTEXT("AFL", "ExtractWindowClosed", "WINDOW CLOSED"), ClosedColor);
}

void UAFLW_ExtractionAnnounce::HandleMatchEnded(FGameplayTag /*Channel*/, const FLyraVerbMessage& Msg)
{
	// Per-player payload: only show MY result (Target == my PlayerState). Magnitude = this-match Watts.
	const APlayerController* PC = GetOwningPlayer();
	if (!PC || Msg.Target != PC->PlayerState)
	{
		return;
	}
	const int32 Watts = FMath::RoundToInt(Msg.Magnitude);
	const FText Banner = FText::Format(
		NSLOCTEXT("AFL", "MatchComplete", "MATCH COMPLETE -- {0} WATTS EARNED"), FText::AsNumber(Watts));
	ShowHeld(Banner, MatchEndColor); // terminal -- no collapse.
}

void UAFLW_ExtractionAnnounce::Show(const FText& Message, const FLinearColor& Color)
{
	if (AnnounceText)
	{
		AnnounceText->SetText(Message);
		AnnounceText->SetColorAndOpacity(FSlateColor(Color));
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CollapseTimer);
		World->GetTimerManager().SetTimer(CollapseTimer,
			FTimerDelegate::CreateWeakLambda(this, [this] { Collapse(); }), HoldSeconds, false);
	}
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ANNOUNCE: %s"), *Message.ToString());
}

void UAFLW_ExtractionAnnounce::ShowHeld(const FText& Message, const FLinearColor& Color)
{
	// Terminal banner: paint + show, but clear (do NOT arm) the collapse timer so it holds.
	if (AnnounceText)
	{
		AnnounceText->SetText(Message);
		AnnounceText->SetColorAndOpacity(FSlateColor(Color));
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CollapseTimer);
	}
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ANNOUNCE (held): %s"), *Message.ToString());
}

void UAFLW_ExtractionAnnounce::Collapse()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
