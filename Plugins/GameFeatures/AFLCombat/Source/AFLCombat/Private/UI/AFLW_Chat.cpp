// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_Chat.h"

#include "AFLCombat.h"
#include "Chat/AFLChatSubsystem.h"        // UAFLChatSubsystem (AFLOnline)
#include "Blueprint/WidgetTree.h"
#include "CommonUIExtensions.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"               // EKeys::Tab
#include "Input/CommonUIInputTypes.h"     // FUIInputConfig / ECommonInputMode

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_Chat)

namespace AFLChatUI
{
	// Ratified IRONICS tokens (afl-design.md / AFLTokenCompiler.cpp). FLinearColor(FColor) does sRGB->linear,
	// so a hex maps to the value it displays. NO CYAN -- accents are electric blue + purple.
	static FLinearColor Hex(const TCHAR* H) { return FLinearColor(FColor::FromHex(H)); }

	static const FLinearColor CardBg   = Hex(TEXT("0E122BF2"));   // surface-card, ~0.95 alpha
	static const FLinearColor Scrim    = FLinearColor(0.f, 0.f, 0.f, 0.28f);
	static const FLinearColor Accent   = Hex(TEXT("1E5AFF"));     // electric neon blue
	static const FLinearColor Violet   = Hex(TEXT("A855F7"));     // purple (whisper)
	static const FLinearColor Amber    = Hex(TEXT("E0A93A"));     // system / near-limit
	static const FLinearColor SayWhite = Hex(TEXT("EAF0FF"));
	static const FLinearColor BodyCol  = Hex(TEXT("C9D2E8"));
	static const FLinearColor Muted    = Hex(TEXT("5A6480"));

	static UFont* Orbitron() { return LoadObject<UFont>(nullptr, TEXT("/Game/UI/Foundation/Fonts/Orbitron.Orbitron")); }
	static UFont* NotoSans() { return LoadObject<UFont>(nullptr, TEXT("/Game/UI/Foundation/Fonts/NotoSans.NotoSans")); }

	static void Style(UTextBlock* T, UFont* F, float Size, const FLinearColor& C)
	{
		if (!T) { return; }
		T->SetFont(FSlateFontInfo(F, static_cast<int32>(Size)));
		T->SetColorAndOpacity(FSlateColor(C));
	}

	// Sender-name / row tint per channel (Say = white).
	static FLinearColor ChannelColor(EAFLChatChannel C)
	{
		switch (C)
		{
		case EAFLChatChannel::Team:    return Accent;
		case EAFLChatChannel::Whisper: return Violet;
		case EAFLChatChannel::System:  return Amber;
		case EAFLChatChannel::Say:
		default:                       return SayWhite;
		}
	}

	// Active send-channel chip colour (Say uses the accent so the chip reads as "live").
	static FLinearColor ChipColor(EAFLChatChannel C)
	{
		return (C == EAFLChatChannel::Whisper) ? Violet : Accent;
	}

	static FString ChannelTag(EAFLChatChannel C)
	{
		switch (C)
		{
		case EAFLChatChannel::Team:    return TEXT("TEAM");
		case EAFLChatChannel::Whisper: return TEXT("WHISPER");
		case EAFLChatChannel::System:  return TEXT("SYSTEM");
		default:                       return FString();
		}
	}

	// Resolve a display name to its FUniqueNetIdRepl from the connected (non-bot) players.
	static bool ResolveTarget(UWorld* World, const FString& Name, FUniqueNetIdRepl& OutId)
	{
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		if (!GS) { return false; }
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (PS && !PS->IsABot() && PS->GetPlayerName() == Name)
			{
				OutId = PS->GetUniqueId();
				return true;
			}
		}
		return false;
	}
}

UAFLW_Chat::UAFLW_Chat()
{
	// Esc / gamepad-B -> NativeOnHandleBackAction (close). With Menu-mode focus this also makes the global
	// System-Menu Esc preprocessor yield, so Esc closes chat FIRST.
	bIsBackHandler = true;
}

UAFLW_Chat* UAFLW_Chat::Open(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	ULocalPlayer* LP = World ? World->GetFirstLocalPlayerFromController() : nullptr;
	if (!LP)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_CHATUI: Open failed -- no local player."));
		return nullptr;
	}
	UCommonActivatableWidget* Pushed = UCommonUIExtensions::PushContentToLayer_ForPlayer(LP,
		FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Menu")),
		TSubclassOf<UCommonActivatableWidget>(UAFLW_Chat::StaticClass()));
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_CHATUI: chat panel opened on UI.Layer.Menu."));
	return Cast<UAFLW_Chat>(Pushed);
}

TSharedRef<SWidget> UAFLW_Chat::RebuildWidget()
{
	using namespace AFLChatUI;
	UFont* Display = Orbitron();
	UFont* Body = NotoSans();

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ChatCanvas"));
	WidgetTree->RootWidget = Canvas;

	// Light scrim (readability; not a full modal blackout so it stays usable in-match).
	UBorder* ScrimB = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ChatScrim"));
	ScrimB->SetBrushColor(Scrim);
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(ScrimB))
	{
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	}

	// Chat card, bottom-left.
	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ChatCard"));
	Card->SetBrushColor(CardBg);
	Card->SetPadding(FMargin(18.f, 14.f, 18.f, 14.f));
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Card))
	{
		S->SetAnchors(FAnchors(0.f, 1.f, 0.f, 1.f));   // bottom-left
		S->SetAlignment(FVector2D(0.f, 1.f));
		S->SetPosition(FVector2D(40.f, -40.f));
		S->SetSize(FVector2D(620.f, 440.f));
	}

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->AddChild(Col);

	// Header.
	UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Style(Header, Display, 16.f, SayWhite);
	Header->SetText(NSLOCTEXT("AFLChat", "Header", "COMMS"));
	if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(Header))
	{
		VS->SetPadding(FMargin(2.f, 0.f, 0.f, 8.f));
	}

	// Message list (fills).
	MessageScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ChatScroll"));
	MessageBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ChatMsgBox"));
	MessageScroll->AddChild(MessageBox);
	if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(MessageScroll))
	{
		VS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		VS->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	// Compose row: [channel chip] [input] [counter].
	UHorizontalBox* ComposeRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(ComposeRow))
	{
		VS->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}

	ChannelChip = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ChatChip"));
	Style(ChannelChip, Display, 11.f, Accent);
	ChannelChip->SetText(NSLOCTEXT("AFLChat", "ChipSay", "SAY"));
	if (UHorizontalBoxSlot* HS = ComposeRow->AddChildToHorizontalBox(ChannelChip))
	{
		HS->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		HS->SetVerticalAlignment(VAlign_Center);
	}

	ComposeInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("ChatInput"));
	ComposeInput->SetHintText(NSLOCTEXT("AFLChat", "Hint", "Message  (Tab: channel  Enter: send  /w name msg: whisper)"));
	ComposeInput->OnTextCommitted.AddDynamic(this, &UAFLW_Chat::OnComposeCommitted);
	ComposeInput->OnTextChanged.AddDynamic(this, &UAFLW_Chat::OnComposeChanged);
	if (UHorizontalBoxSlot* HS = ComposeRow->AddChildToHorizontalBox(ComposeInput))
	{
		HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		HS->SetVerticalAlignment(VAlign_Center);
	}

	CharCounter = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ChatCounter"));
	Style(CharCounter, Body, 11.f, Muted);
	CharCounter->SetText(NSLOCTEXT("AFLChat", "Counter0", "0 / 256"));
	if (UHorizontalBoxSlot* HS = ComposeRow->AddChildToHorizontalBox(CharCounter))
	{
		HS->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
		HS->SetVerticalAlignment(VAlign_Center);
	}

	// Footer hint.
	UTextBlock* Foot = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Style(Foot, Body, 10.f, Muted);
	Foot->SetText(NSLOCTEXT("AFLChat", "Foot", "Esc to close"));
	if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(Foot))
	{
		VS->SetPadding(FMargin(2.f, 2.f, 0.f, 0.f));
	}

	return Super::RebuildWidget();
}

void UAFLW_Chat::NativeOnActivated()
{
	Super::NativeOnActivated();
	RefreshChannelChip();

	if (UAFLChatSubsystem* Subsys = UAFLChatSubsystem::Get(this))
	{
		// Backfill first (a UI opening mid-session shows the 200-ring), then subscribe for live messages.
		for (const FAFLChatMessage& M : Subsys->GetHistory())
		{
			AppendRow(M);
		}
		ChatHandle = Subsys->OnChatMessage.AddUObject(this, &UAFLW_Chat::HandleChatMessage);
	}

	if (ComposeInput)
	{
		ComposeInput->SetKeyboardFocus();
	}
}

void UAFLW_Chat::NativeOnDeactivated()
{
	// The subsystem outlives the widget -> a handle left bound fires into a dead panel. Release it.
	if (ChatHandle.IsValid())
	{
		if (UAFLChatSubsystem* Subsys = UAFLChatSubsystem::Get(this))
		{
			Subsys->OnChatMessage.Remove(ChatHandle);
		}
		ChatHandle.Reset();
	}
	Super::NativeOnDeactivated();
}

UWidget* UAFLW_Chat::NativeGetDesiredFocusTarget() const
{
	if (ComposeInput) { return ComposeInput; }
	return Super::NativeGetDesiredFocusTarget();
}

bool UAFLW_Chat::NativeOnHandleBackAction()
{
	DeactivateWidget();
	return true;
}

FReply UAFLW_Chat::NativeOnKeyDown(const FGeometry& Geo, const FKeyEvent& Key)
{
	if (Key.GetKey() == EKeys::Tab)
	{
		CycleChannel();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geo, Key);
}

TOptional<FUIInputConfig> UAFLW_Chat::GetDesiredInputConfig() const
{
	// Menu mode + visible cursor: the compose box owns focus (so movement/fire are dead while typing) and the
	// System-Menu Esc preprocessor yields to CommonUI's back handler.
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UAFLW_Chat::HandleChatMessage(const FAFLChatMessage& Message)
{
	AppendRow(Message);
}

void UAFLW_Chat::AppendRow(const FAFLChatMessage& Msg)
{
	using namespace AFLChatUI;
	if (!MessageBox) { return; }

	UFont* Display = Orbitron();
	UFont* Body = NotoSans();

	const bool bEphemeral = Msg.bLocalEphemeral;
	const FLinearColor ChanCol = ChannelColor(Msg.Channel);

	FString SenderStr = Msg.SenderDisplayName;
	if (SenderStr.IsEmpty() && Msg.Channel == EAFLChatChannel::System) { SenderStr = TEXT("SYSTEM"); }
	const FString Tag = ChannelTag(Msg.Channel);
	const FString Prefix = Tag.IsEmpty() ? (SenderStr + TEXT("  "))
	                                     : (FString::Printf(TEXT("[%s] %s  "), *Tag, *SenderStr));

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	UTextBlock* Sender = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Style(Sender, Display, 12.f, bEphemeral ? Muted : ChanCol);
	Sender->SetText(FText::FromString(Prefix));
	if (UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(Sender))
	{
		HS->SetVerticalAlignment(VAlign_Top);
	}

	UTextBlock* BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Style(BodyText, Body, 13.f, bEphemeral ? Muted : BodyCol);
	BodyText->SetAutoWrapText(true);
	BodyText->SetText(FText::FromString(Msg.Body));
	if (UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(BodyText))
	{
		HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	if (UVerticalBoxSlot* VS = MessageBox->AddChildToVerticalBox(Row))
	{
		VS->SetPadding(FMargin(2.f, 2.f, 2.f, 2.f));
	}

	// Trim oldest beyond the cap.
	while (MessageBox->GetChildrenCount() > MaxRows)
	{
		MessageBox->RemoveChildAt(0);
	}
	if (MessageScroll)
	{
		MessageScroll->ScrollToEnd();
	}
}

void UAFLW_Chat::OnComposeCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		SendCurrent();
	}
}

void UAFLW_Chat::OnComposeChanged(const FText& Text)
{
	using namespace AFLChatUI;
	if (!CharCounter) { return; }
	const int32 Len = Text.ToString().Len();
	CharCounter->SetText(FText::FromString(FString::Printf(TEXT("%d / 256"), Len)));
	CharCounter->SetColorAndOpacity(FSlateColor(Len >= 240 ? Amber : Muted));
}

void UAFLW_Chat::CycleChannel()
{
	switch (CurrentChannel)
	{
	case EAFLChatChannel::Say:     CurrentChannel = EAFLChatChannel::Team; break;
	case EAFLChatChannel::Team:    CurrentChannel = EAFLChatChannel::Whisper; break;
	case EAFLChatChannel::Whisper:
	default:                       CurrentChannel = EAFLChatChannel::Say; break;
	}
	RefreshChannelChip();
}

void UAFLW_Chat::RefreshChannelChip()
{
	using namespace AFLChatUI;
	if (!ChannelChip) { return; }
	FText Label;
	switch (CurrentChannel)
	{
	case EAFLChatChannel::Team:    Label = NSLOCTEXT("AFLChat", "ChipTeam", "TEAM"); break;
	case EAFLChatChannel::Whisper: Label = NSLOCTEXT("AFLChat", "ChipWhisper", "WHISPER"); break;
	default:                       Label = NSLOCTEXT("AFLChat", "ChipSay2", "SAY"); break;
	}
	ChannelChip->SetText(Label);
	ChannelChip->SetColorAndOpacity(FSlateColor(ChipColor(CurrentChannel)));
}

void UAFLW_Chat::SendCurrent()
{
	using namespace AFLChatUI;
	if (!ComposeInput) { return; }

	FString Text = ComposeInput->GetText().ToString();
	Text.TrimStartAndEndInline();
	if (Text.IsEmpty()) { return; }

	UAFLChatSubsystem* Subsys = UAFLChatSubsystem::Get(this);
	if (!Subsys) { return; }

	EAFLChatChannel Channel = CurrentChannel;
	FUniqueNetIdRepl Target;

	// `/w Name message` -> whisper (resolve the target from the player list). Falls back to Say if unmatched.
	if (Text.StartsWith(TEXT("/w ")))
	{
		FString Rest = Text.RightChop(3);
		Rest.TrimStartInline();
		FString Name, Rem;
		if (Rest.Split(TEXT(" "), &Name, &Rem) && ResolveTarget(GetWorld(), Name, Target))
		{
			Channel = EAFLChatChannel::Whisper;
			Rem.TrimStartAndEndInline();
			Text = Rem;
		}
		else
		{
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_CHATUI: /w target not found -- send ignored."));
			return;
		}
	}
	else if (Channel == EAFLChatChannel::Whisper)
	{
		// Whisper chip selected but no target -> Say (a target picker is a later increment).
		Channel = EAFLChatChannel::Say;
	}

	if (!Text.IsEmpty())
	{
		Subsys->Send(Channel, Text, Target);
	}

	ComposeInput->SetText(FText::GetEmpty());
	if (CharCounter)
	{
		CharCounter->SetText(NSLOCTEXT("AFLChat", "Counter0b", "0 / 256"));
		CharCounter->SetColorAndOpacity(FSlateColor(Muted));
	}
	ComposeInput->SetKeyboardFocus();
}

// ------------------------------------------------------------------------------------------------
// Console mount (autonomous -- no editor-authored input asset yet). `afl.Chat.Open` pushes the panel.
// ------------------------------------------------------------------------------------------------
static FAutoConsoleCommandWithWorld GAFLChatOpenCmd(
	TEXT("afl.Chat.Open"),
	TEXT("Open the IRONICS chat panel (COMMS-2)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		UAFLW_Chat::Open(World);
	}));
