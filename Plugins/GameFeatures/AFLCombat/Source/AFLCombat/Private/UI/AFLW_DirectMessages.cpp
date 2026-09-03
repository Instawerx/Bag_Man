// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_DirectMessages.h"

#include "AFLCombat.h"
#include "Cosmetics/AFLPlayerIdentityComponent.h"   // GetResolvedPlayFabId (replicated to all clients)
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
#include "Input/CommonUIInputTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_DirectMessages)

namespace AFLDMUI
{
	static FLinearColor Hex(const TCHAR* H) { return FLinearColor(FColor::FromHex(H)); }
	static const FLinearColor CardBg   = Hex(TEXT("0E122BF7"));
	static const FLinearColor Scrim    = FLinearColor(0.f, 0.f, 0.f, 0.42f);
	static const FLinearColor Accent   = Hex(TEXT("1E5AFF"));
	static const FLinearColor SentBg   = Hex(TEXT("15398F"));   // dim electric for the sender's bubbles
	static const FLinearColor RecvBg   = Hex(TEXT("161C2E"));
	static const FLinearColor SayWhite = Hex(TEXT("EAF0FF"));
	static const FLinearColor BodyCol  = Hex(TEXT("D6DCEC"));
	static const FLinearColor Muted    = Hex(TEXT("5A6480"));

	static UFont* Orbitron() { return LoadObject<UFont>(nullptr, TEXT("/Game/UI/Foundation/Fonts/Orbitron.Orbitron")); }
	static UFont* NotoSans() { return LoadObject<UFont>(nullptr, TEXT("/Game/UI/Foundation/Fonts/NotoSans.NotoSans")); }
	static void Style(UTextBlock* T, UFont* F, float Size, const FLinearColor& C)
	{
		if (!T) { return; }
		T->SetFont(FSlateFontInfo(F, static_cast<int32>(Size)));
		T->SetColorAndOpacity(FSlateColor(C));
	}
}

UAFLW_DirectMessages::UAFLW_DirectMessages()
{
	bIsBackHandler = true;
}

UAFLW_DirectMessages* UAFLW_DirectMessages::Open(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	ULocalPlayer* LP = World ? World->GetFirstLocalPlayerFromController() : nullptr;
	if (!LP)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_DMUI: Open failed -- no local player."));
		return nullptr;
	}
	UCommonActivatableWidget* Pushed = UCommonUIExtensions::PushContentToLayer_ForPlayer(LP,
		FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Menu")),
		TSubclassOf<UCommonActivatableWidget>(UAFLW_DirectMessages::StaticClass()));
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_DMUI: DM panel opened on UI.Layer.Menu."));
	return Cast<UAFLW_DirectMessages>(Pushed);
}

TSharedRef<SWidget> UAFLW_DirectMessages::RebuildWidget()
{
	using namespace AFLDMUI;
	UFont* Display = Orbitron();
	UFont* Body = NotoSans();

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DMCanvas"));
	WidgetTree->RootWidget = Canvas;

	UBorder* ScrimB = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DMScrim"));
	ScrimB->SetBrushColor(Scrim);
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(ScrimB))
	{
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	}

	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DMCard"));
	Card->SetBrushColor(CardBg);
	Card->SetPadding(FMargin(20.f, 18.f, 20.f, 16.f));
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Card))
	{
		S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetSize(FVector2D(680.f, 520.f));
	}

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->AddChild(Col);

	// Header row: title + current recipient.
	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(HeaderRow)) { VS->SetPadding(FMargin(2.f, 0.f, 0.f, 8.f)); }
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Style(Title, Display, 16.f, SayWhite);
	Title->SetText(NSLOCTEXT("AFLDM", "Title", "DIRECT MESSAGES"));
	if (UHorizontalBoxSlot* HS = HeaderRow->AddChildToHorizontalBox(Title)) { HS->SetVerticalAlignment(VAlign_Center); }
	RecipientLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DMRecipient"));
	Style(RecipientLabel, Body, 12.f, Muted);
	RecipientLabel->SetText(NSLOCTEXT("AFLDM", "PickHint", "select someone to message"));
	if (UHorizontalBoxSlot* HS = HeaderRow->AddChildToHorizontalBox(RecipientLabel))
	{
		HS->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
		HS->SetVerticalAlignment(VAlign_Center);
	}

	// Recipient picker.
	RecipientCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("DMCombo"));
	RecipientCombo->OnSelectionChanged.AddDynamic(this, &UAFLW_DirectMessages::OnRecipientSelected);
	if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(RecipientCombo)) { VS->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f)); }

	// Thread (fills).
	ThreadScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("DMThread"));
	ThreadBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DMThreadBox"));
	ThreadScroll->AddChild(ThreadBox);
	if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(ThreadScroll))
	{
		VS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		VS->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	// Reply.
	ReplyInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("DMReply"));
	ReplyInput->SetHintText(NSLOCTEXT("AFLDM", "ReplyHint", "Message  (Enter to send)"));
	ReplyInput->OnTextCommitted.AddDynamic(this, &UAFLW_DirectMessages::OnReplyCommitted);
	if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(ReplyInput)) { VS->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f)); }

	UTextBlock* Foot = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Style(Foot, Body, 10.f, Muted);
	Foot->SetText(NSLOCTEXT("AFLDM", "Foot", "Esc to close"));
	Col->AddChildToVerticalBox(Foot);

	return Super::RebuildWidget();
}

void UAFLW_DirectMessages::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (UAFLSocialSubsystem* Social = UAFLSocialSubsystem::Get(this))
	{
		DmHandle = Social->OnDirectMessage.AddUObject(this, &UAFLW_DirectMessages::HandleInbound);
		// Backfill the offline inbox, then build the recipient list (in-session players + inbox partners).
		Social->FetchInbox(0, [WeakThis = TWeakObjectPtr<UAFLW_DirectMessages>(this)](const TArray<FAFLDirectMessage>& Msgs, int64 /*Cursor*/)
		{
			if (UAFLW_DirectMessages* Self = WeakThis.Get())
			{
				Self->Cache.Append(Msgs);
				Self->RebuildRecipientList();
				Self->RenderThread();
			}
		});
	}
	RebuildRecipientList();
}

void UAFLW_DirectMessages::NativeOnDeactivated()
{
	if (DmHandle.IsValid())
	{
		if (UAFLSocialSubsystem* Social = UAFLSocialSubsystem::Get(this))
		{
			Social->OnDirectMessage.Remove(DmHandle);
		}
		DmHandle.Reset();
	}
	Super::NativeOnDeactivated();
}

UWidget* UAFLW_DirectMessages::NativeGetDesiredFocusTarget() const
{
	if (RecipientCombo) { return RecipientCombo; }
	return Super::NativeGetDesiredFocusTarget();
}

bool UAFLW_DirectMessages::NativeOnHandleBackAction()
{
	DeactivateWidget();
	return true;
}

TOptional<FUIInputConfig> UAFLW_DirectMessages::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

FString UAFLW_DirectMessages::MyPlayFabId() const
{
	const APlayerController* PC = GetOwningPlayer();
	const APlayerState* PS = PC ? PC->PlayerState : nullptr;
	const UAFLPlayerIdentityComponent* Id = PS ? PS->FindComponentByClass<UAFLPlayerIdentityComponent>() : nullptr;
	return Id ? Id->GetResolvedPlayFabId() : FString();
}

FString UAFLW_DirectMessages::ResolveName(const FString& PlayFabId) const
{
	if (PlayFabId.IsEmpty()) { return TEXT("Unknown"); }
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (GS)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (!PS) { continue; }
			const UAFLPlayerIdentityComponent* Id = PS->FindComponentByClass<UAFLPlayerIdentityComponent>();
			if (Id && Id->GetResolvedPlayFabId() == PlayFabId)
			{
				return PS->GetPlayerName();
			}
		}
	}
	// Offline / not-in-session: a short, stable tag rather than the full opaque id.
	return FString::Printf(TEXT("Player-%s"), *PlayFabId.Right(6));
}

FString UAFLW_DirectMessages::ConversationOf(const FString& A, const FString& B)
{
	return (A <= B) ? (A + TEXT("#") + B) : (B + TEXT("#") + A);
}

void UAFLW_DirectMessages::RebuildRecipientList()
{
	if (!RecipientCombo) { return; }
	const FString Me = MyPlayFabId();

	DisplayToId.Reset();
	RecipientCombo->ClearOptions();

	// In-session players (name + replicated id), excluding self + bots.
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (GS)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (!PS || PS->IsABot()) { continue; }
			const UAFLPlayerIdentityComponent* Id = PS->FindComponentByClass<UAFLPlayerIdentityComponent>();
			const FString Pid = Id ? Id->GetResolvedPlayFabId() : FString();
			if (Pid.IsEmpty() || Pid == Me) { continue; }
			const FString Display = PS->GetPlayerName();
			if (!DisplayToId.Contains(Display))
			{
				DisplayToId.Add(Display, Pid);
				RecipientCombo->AddOption(Display);
			}
		}
	}

	// Inbox conversation partners not already listed (people who DM'd you while offline / cross-session).
	for (const FAFLDirectMessage& M : Cache)
	{
		const FString Other = (M.SenderId == Me) ? M.RecipientId : M.SenderId;
		if (Other.IsEmpty() || Other == Me) { continue; }
		const FString Display = ResolveName(Other);
		if (!DisplayToId.Contains(Display))
		{
			DisplayToId.Add(Display, Other);
			RecipientCombo->AddOption(Display);
		}
	}
}

void UAFLW_DirectMessages::OnRecipientSelected(FString SelectedItem, ESelectInfo::Type /*SelectType*/)
{
	if (const FString* Id = DisplayToId.Find(SelectedItem))
	{
		SelectConversation(*Id, SelectedItem);
	}
}

void UAFLW_DirectMessages::SelectConversation(const FString& OtherId, const FString& OtherName)
{
	CurrentOtherId = OtherId;
	CurrentOtherName = OtherName;
	CurrentConversationId = ConversationOf(MyPlayFabId(), OtherId);
	if (RecipientLabel)
	{
		RecipientLabel->SetText(FText::Format(NSLOCTEXT("AFLDM", "With", "with {0}"), FText::FromString(OtherName)));
	}
	RenderThread();
	if (UAFLSocialSubsystem* Social = UAFLSocialSubsystem::Get(this))
	{
		Social->MarkConversationRead(CurrentConversationId);
	}
	if (ReplyInput) { ReplyInput->SetKeyboardFocus(); }
}

void UAFLW_DirectMessages::RenderThread()
{
	using namespace AFLDMUI;
	if (!ThreadBox) { return; }
	ThreadBox->ClearChildren();
	if (CurrentConversationId.IsEmpty()) { return; }

	UFont* Body = NotoSans();
	const FString Me = MyPlayFabId();

	for (const FAFLDirectMessage& M : Cache)
	{
		if (M.ConversationId != CurrentConversationId) { continue; }
		const bool bMine = (M.SenderId == Me);

		UBorder* Bubble = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Bubble->SetBrushColor(bMine ? SentBg : RecvBg);
		Bubble->SetPadding(FMargin(10.f, 6.f, 10.f, 6.f));
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Style(Text, Body, 13.f, bMine ? SayWhite : BodyCol);
		Text->SetAutoWrapText(true);
		Text->SetText(FText::FromString(M.Body));
		Bubble->AddChild(Text);

		if (UVerticalBoxSlot* VS = ThreadBox->AddChildToVerticalBox(Bubble))
		{
			VS->SetHorizontalAlignment(bMine ? HAlign_Right : HAlign_Left);
			VS->SetPadding(FMargin(2.f, 3.f, 2.f, 3.f));
		}
	}
	if (ThreadScroll) { ThreadScroll->ScrollToEnd(); }
}

void UAFLW_DirectMessages::HandleInbound(const FAFLDirectMessage& Msg)
{
	Cache.Add(Msg);
	if (!Msg.ConversationId.IsEmpty() && Msg.ConversationId == CurrentConversationId)
	{
		RenderThread();
	}
	else
	{
		// A DM from someone not open -> make sure they are pickable.
		RebuildRecipientList();
	}
}

void UAFLW_DirectMessages::OnReplyCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType != ETextCommit::OnEnter) { return; }
	FString Body = Text.ToString();
	Body.TrimStartAndEndInline();
	if (Body.IsEmpty() || CurrentOtherId.IsEmpty()) { return; }

	if (UAFLSocialSubsystem* Social = UAFLSocialSubsystem::Get(this))
	{
		Social->SendDirectMessage(CurrentOtherId, Body); // the WS echo returns via OnDirectMessage -> rendered
	}
	if (ReplyInput)
	{
		ReplyInput->SetText(FText::GetEmpty());
		ReplyInput->SetKeyboardFocus();
	}
}

// afl.DM.Open -- autonomous mount (no editor-authored input asset yet).
static FAutoConsoleCommandWithWorld GAFLDMOpenCmd(
	TEXT("afl.DM.Open"),
	TEXT("Open the IRONICS direct-messages panel (COMMS-5)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		UAFLW_DirectMessages::Open(World);
	}));
