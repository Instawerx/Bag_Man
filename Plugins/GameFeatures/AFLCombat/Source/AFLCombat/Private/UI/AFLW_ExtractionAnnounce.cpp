// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_ExtractionAnnounce.h"

#include "AFLCombat.h"
#include "Components/TextBlock.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_ExtractionAnnounce)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_WindowOpen_Announce, "Event.Extraction.WindowOpen");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_WindowClosed_Announce, "Event.Extraction.WindowClosed");


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

void UAFLW_ExtractionAnnounce::Collapse()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
