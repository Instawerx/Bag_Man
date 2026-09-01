// Copyright C12 AI Gaming. All Rights Reserved.

#include "Retail/AFLRetailSubsystem.h"

#include "AFLCombat.h"
#include "AFLCosmeticCatalogSubsystem.h"
#include "AFLCosmeticCoreTypes.h"
#include "Blueprint/UserWidget.h"
#include "CommonUIExtensions.h"
#include "Components/InputComponent.h"
#include "Cosmetics/AFLCosmeticLoadoutComponent.h"
#include "Cosmetics/AFLWalletComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "InputCoreTypes.h"
#include "Player/LyraPlayerState.h"
#include "TimerManager.h"
#include "UI/AFLW_FrontEndMarket.h"
#include "UI/AFLW_ProductPage.h"
#include "UI/AFLW_RetailCard.h"
#include "UI/AFLW_RetailCartChip.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLRetailSubsystem)

namespace AFLRetail
{
	static const FLinearColor Good(0.16f, 0.85f, 0.35f, 1.0f);
	static const FLinearColor Bad(1.0f, 0.16f, 0.22f, 1.0f);
	static const FLinearColor Dim(1.0f, 1.0f, 1.0f, 0.55f);

	/** Which counted credit pays for a credit-pool row (mirrors the wallet's server-side resolve). */
	static FName CreditKeyForType(EAFLCosmeticType Type)
	{
		switch (Type)
		{
		case EAFLCosmeticType::Weapon:  return FName(TEXT("AFL.WeaponCredit"));
		case EAFLCosmeticType::Sticker: return FName(TEXT("AFL.StickerCredit"));
		default:                        return NAME_None;
		}
	}
}

UAFLRetailSubsystem* UAFLRetailSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UAFLRetailSubsystem>() : nullptr;
}

// --- Pad seam --------------------------------------------------------------------------------------

void UAFLRetailSubsystem::PadEntered(FName CosmeticId, EAFLGrabArmMode ArmMode, float DwellSeconds, APawn* InLocalPawn)
{
	if (CosmeticId.IsNone() || !InLocalPawn || !InLocalPawn->IsLocallyControlled())
	{
		return;
	}
	AtPadId = CosmeticId;
	AtPadArmMode = ArmMode;
	LocalPawn = InLocalPawn;
	EnsureKeyBinds(InLocalPawn);

	// Speak wearable refusals on the card (polish: a jewellery slot refusal was a silent nothing).
	if (UAFLCosmeticLoadoutComponent* Loadout = GetLoadout(); Loadout && !bRefusalBound)
	{
		Loadout->OnWearableRefused.AddUObject(this, &UAFLRetailSubsystem::HandleWearableRefused);
		bRefusalBound = true;
	}

	// INTENTIONAL pickups (operator law): a short dwell -- or the explicit E-grab -- arms the apply.
	// Brushing past does nothing; both behaviors ship behind the pad's one knob.
	if (ArmMode == EAFLGrabArmMode::Dwell)
	{
		if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
		{
			World->GetTimerManager().SetTimer(DwellTimer, this, &UAFLRetailSubsystem::ArmTryOn,
				FMath::Max(0.05f, DwellSeconds), false);
		}
	}
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_RETAIL: at pad '%s' (%s)."), *CosmeticId.ToString(),
		ArmMode == EAFLGrabArmMode::Dwell ? TEXT("dwell") : TEXT("press-E"));
}

void UAFLRetailSubsystem::PadLeft(FName CosmeticId)
{
	if (CosmeticId == AtPadId)
	{
		AtPadId = NAME_None;
		if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
		{
			World->GetTimerManager().ClearTimer(DwellTimer);
		}
	}
	// "Nothing leaves the store unbought" -- stepping off the pad restores the look unless it was bought.
	if (CosmeticId == CurrentId && State != ERetailState::Idle && State != ERetailState::Purchasing)
	{
		CloseCurrent(!bPurchasedCurrent);
	}
}

void UAFLRetailSubsystem::OpenTill()
{
	bAtTill = true;
	bChipExpanded = true;
	RefreshChip();
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_RETAIL: at the till (cart=%d)."), CartIds.Num());
}

void UAFLRetailSubsystem::CloseTill()
{
	bAtTill = false;
	RefreshChip();
}

// --- Arming ----------------------------------------------------------------------------------------

void UAFLRetailSubsystem::ArmTryOn()
{
	if (AtPadId.IsNone() || AtPadId == CurrentId)
	{
		return;
	}
	if (State != ERetailState::Idle)
	{
		// Pad-to-pad chain: the server keeps the ORIGINAL baseline; locally just drop the old card state.
		CloseCurrent(false);
	}
	CurrentId = AtPadId;
	bPurchasedCurrent = false;
	State = ERetailState::Browsing;

	// Non-wearable rows (credit packs) skip the try-on RPC -- the server would only refuse them
	// with a warning; the card is their whole surface.
	EAFLLoadoutAxis TryAxis;
	if (UAFLCosmeticLoadoutComponent* Loadout = GetLoadout();
		Loadout && UAFLW_FrontEndMarket::ClassifyStoreAxis(CurrentId, TryAxis))
	{
		Loadout->ServerRequestTryOn(CurrentId);
		Loadout->LocalActiveTryOnId = CurrentId;
	}
	ShowCard();
	RefreshCard();
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_RETAIL: GRAB '%s' -> wearing/holding (map-exception try-on)."), *CurrentId.ToString());
}

void UAFLRetailSubsystem::CloseCurrent(bool bReleaseServer)
{
	if (UAFLCosmeticLoadoutComponent* Loadout = GetLoadout())
	{
		if (bReleaseServer && !CurrentId.IsNone())
		{
			Loadout->ServerReleaseTryOn(false); // discard -> server restores the baseline look
		}
		Loadout->LocalActiveTryOnId = NAME_None;
	}
	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		World->GetTimerManager().ClearTimer(GrantPollTimer);
	}
	DestroyCard();
	CurrentId = NAME_None;
	bPurchasedCurrent = false;
	State = ERetailState::Idle;
}

// --- Keys ------------------------------------------------------------------------------------------

void UAFLRetailSubsystem::EnsureKeyBinds(APawn* InLocalPawn)
{
	APlayerController* PC = InLocalPawn ? Cast<APlayerController>(InLocalPawn->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController() || !PC->InputComponent || BoundPC.Get() == PC)
	{
		return;
	}
	// PERSISTENT, non-consuming (the door lazy-bind pattern): only keys with no gameplay collision.
	// The card's action keys (F/C/Q) are NOT here -- lap-3 found Q double-fired the GRENADE ability
	// and the operator blew themselves up mid-discard. Those bind CONSUMING, card-scoped, below.
	auto Bind = [&](const FKey& Key, void (UAFLRetailSubsystem::*Fn)())
	{
		FInputKeyBinding& KB = PC->InputComponent->BindKey(Key, IE_Pressed, this, Fn);
		KB.bConsumeInput = false;
	};
	Bind(EKeys::E, &UAFLRetailSubsystem::OnKeyGrabDetails);
	Bind(EKeys::Gamepad_FaceButton_Left, &UAFLRetailSubsystem::OnKeyGrabDetails);
	Bind(EKeys::V, &UAFLRetailSubsystem::OnKeyChipToggle);
	Bind(EKeys::X, &UAFLRetailSubsystem::OnKeyCheckout);
	BoundPC = PC;
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_RETAIL: persistent keys armed (E/V/X, non-consuming)."));
}

void UAFLRetailSubsystem::BindCardKeys()
{
	APlayerController* PC = GetLocalPC();
	if (bCardKeysBound || !PC || !PC->InputComponent)
	{
		return;
	}
	// CARD-SCOPED, CONSUMING: while the card is open, F/C/Q belong to retail and gameplay abilities
	// on those keys (grenade on Q!) never see the press. Removed the moment the card closes, so the
	// grenade works everywhere else in the hub.
	auto Bind = [&](const FKey& Key, void (UAFLRetailSubsystem::*Fn)())
	{
		FInputKeyBinding& KB = PC->InputComponent->BindKey(Key, IE_Pressed, this, Fn);
		KB.bConsumeInput = true;
	};
	Bind(EKeys::F, &UAFLRetailSubsystem::OnKeyBuy);
	Bind(EKeys::C, &UAFLRetailSubsystem::OnKeyCart);
	Bind(EKeys::Q, &UAFLRetailSubsystem::OnKeyDiscard);
	// Gamepad pass (polish): same card-scoped consuming discipline -- Y buy, D-pad-up cart, B discard.
	Bind(EKeys::Gamepad_FaceButton_Top, &UAFLRetailSubsystem::OnKeyBuy);
	Bind(EKeys::Gamepad_DPad_Up, &UAFLRetailSubsystem::OnKeyCart);
	Bind(EKeys::Gamepad_FaceButton_Right, &UAFLRetailSubsystem::OnKeyDiscard);
	bCardKeysBound = true;
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_RETAIL: card keys armed (F/C/Q + pad Y/DUp/B, CONSUMING -- gameplay shielded)."));
}

void UAFLRetailSubsystem::UnbindCardKeys()
{
	if (!bCardKeysBound)
	{
		return;
	}
	bCardKeysBound = false;
	APlayerController* PC = BoundPC.Get();
	if (!PC || !PC->InputComponent)
	{
		return;
	}
	PC->InputComponent->KeyBindings.RemoveAll([this](const FInputKeyBinding& B)
	{
		return B.KeyDelegate.IsBoundToObject(this)
			&& (B.Chord.Key == EKeys::F || B.Chord.Key == EKeys::C || B.Chord.Key == EKeys::Q
				|| B.Chord.Key == EKeys::Gamepad_FaceButton_Top || B.Chord.Key == EKeys::Gamepad_DPad_Up
				|| B.Chord.Key == EKeys::Gamepad_FaceButton_Right);
	});
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_RETAIL: card keys released (F/C/Q back to gameplay)."));
}

void UAFLRetailSubsystem::OnKeyGrabDetails()
{
	// Press-mode arm first; with a live card, E is the DETAILED tier (the full product page, opt-in).
	if (State == ERetailState::Idle)
	{
		if (!AtPadId.IsNone() && AtPadArmMode == EAFLGrabArmMode::Press)
		{
			ArmTryOn();
		}
		return;
	}
	if ((State == ERetailState::Browsing || State == ERetailState::ConfirmBuy) && !CurrentId.IsNone())
	{
		APlayerController* PC = GetLocalPC();
		ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
		if (!LP)
		{
			return;
		}
		UCommonActivatableWidget* Page = UCommonUIExtensions::PushContentToLayer_ForPlayer(LP,
			FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Menu")),
			TSubclassOf<UCommonActivatableWidget>(UAFLW_ProductPage::StaticClass()));
		if (UAFLW_ProductPage* Product = Cast<UAFLW_ProductPage>(Page))
		{
			Product->FocusCosmeticId(CurrentId);
		}
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_RETAIL: DETAILS '%s' -> product page."), *CurrentId.ToString());
	}
}

void UAFLRetailSubsystem::OnKeyBuy()
{
	if (State == ERetailState::Browsing && !CurrentId.IsNone())
	{
		const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
		const FAFLCatalogEntry* Entry = Catalog ? Catalog->FindEntry(CurrentId) : nullptr;
		if (!Entry)
		{
			return;
		}
		const bool bCounted = !Entry->CountedKey.IsNone();
		if (IsOwned(CurrentId) && !bCounted)
		{
			return; // D-7: owned non-counted rows never re-charge
		}
		const UAFLWalletComponent* Wallet = GetWallet();
		if (Entry->bTransactable)
		{
			bConfirmRedeem = false;
			State = ERetailState::ConfirmBuy;
			if (Card)
			{
				const int32 Bal = Wallet ? Wallet->GetVolts() : 0;
				Card->SetBuyRow(FText::Format(NSLOCTEXT("AFLRetail", "ConfirmFmt", "[F]  SURE?  −{0} V  →  {1} V"),
					FText::AsNumber(Entry->PriceVolts), FText::AsNumber(Bal - Entry->PriceVolts)), true);
				Card->SetStatus(NSLOCTEXT("AFLRetail", "ConfirmHint", "F again buys it · Q backs out"), AFLRetail::Dim);
			}
			return;
		}
		// Credit-pool row (the ruled weapon acquisition): confirm spends ONE credit.
		const FName CreditKey = Entry->bCreditRedeemable ? AFLRetail::CreditKeyForType(Entry->Type) : NAME_None;
		const int32 Credits = (!CreditKey.IsNone() && Wallet) ? Wallet->GetCountedEntitlement(CreditKey) : 0;
		if (Credits > 0)
		{
			bConfirmRedeem = true;
			State = ERetailState::ConfirmBuy;
			if (Card)
			{
				Card->SetBuyRow(FText::Format(NSLOCTEXT("AFLRetail", "ConfirmRedeemFmt", "[F]  SURE?  −1 CREDIT  →  {0} left"),
					FText::AsNumber(Credits - 1)), true);
				Card->SetStatus(NSLOCTEXT("AFLRetail", "ConfirmHint2b", "F again redeems it · Q backs out"), AFLRetail::Dim);
			}
		}
		return;
	}
	if (State == ERetailState::ConfirmBuy && !CurrentId.IsNone())
	{
		BeginPurchase(CurrentId);
	}
}

void UAFLRetailSubsystem::OnKeyCart()
{
	if ((State != ERetailState::Browsing && State != ERetailState::ConfirmBuy) || CurrentId.IsNone())
	{
		return;
	}
	// Credit-pool rows redeem on the spot -- the cart's checkout runs the Volts purchase path,
	// which would refuse them one by one. Keep the cart honest.
	{
		const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
		const FAFLCatalogEntry* Entry = Catalog ? Catalog->FindEntry(CurrentId) : nullptr;
		if (Entry && !Entry->bTransactable)
		{
			if (Card)
			{
				Card->SetStatus(NSLOCTEXT("AFLRetail", "CreditNoCart", "Credit item — redeem it right here with [F]"), AFLRetail::Dim);
			}
			return;
		}
	}
	AddToCart(CurrentId);
	State = ERetailState::Browsing;
	RefreshCard();
	if (Card)
	{
		Card->SetStatus(NSLOCTEXT("AFLRetail", "InCart", "IN CART ✓ — checkout at the till, or [V] opens the chip"), AFLRetail::Good);
	}
}

void UAFLRetailSubsystem::OnKeyDiscard()
{
	if (State == ERetailState::ConfirmBuy)
	{
		State = ERetailState::Browsing;
		RefreshCard();
		return;
	}
	if (State == ERetailState::Browsing)
	{
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_RETAIL: DISCARD '%s'."), *CurrentId.ToString());
		CloseCurrent(!bPurchasedCurrent);
	}
}

void UAFLRetailSubsystem::OnKeyChipToggle()
{
	if (CartIds.Num() == 0 && !Chip)
	{
		return;
	}
	bChipExpanded = !bChipExpanded;
	RefreshChip();
}

void UAFLRetailSubsystem::OnKeyCheckout()
{
	if (State == ERetailState::CheckingOut || CartIds.Num() == 0)
	{
		return;
	}
	if (!bChipExpanded)
	{
		bChipExpanded = true; // X with a minimized chip opens it armed -- one less tap at the till
	}
	if (CheckoutIndex == INDEX_NONE)
	{
		// Second X inside the armed window -> run it.
		StepCheckout();
		return;
	}
	// First X: arm the one confirm ("checkout confirms once, not per item" -- plan cart discipline).
	CheckoutIndex = INDEX_NONE;
	RefreshChip();
}

void UAFLRetailSubsystem::HandleWearableRefused(FName CosmeticId, const FString& Reason)
{
	if (Card && CosmeticId == CurrentId)
	{
		// The server's authored reason, verbatim ("both wrists are full -- remove one first").
		Card->SetStatus(FText::FromString(Reason), AFLRetail::Bad);
	}
}

// --- Cart ------------------------------------------------------------------------------------------

void UAFLRetailSubsystem::AddToCart(FName CosmeticId)
{
	if (CosmeticId.IsNone() || CartIds.Contains(CosmeticId))
	{
		return;
	}
	CartIds.Add(CosmeticId);
	RefreshChip();
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_RETAIL: cart +'%s' (%d items)."), *CosmeticId.ToString(), CartIds.Num());
}

void UAFLRetailSubsystem::RemoveFromCart(FName CosmeticId)
{
	if (CartIds.Remove(CosmeticId) > 0)
	{
		RefreshChip();
	}
}

int64 UAFLRetailSubsystem::CartTotalVolts() const
{
	const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	int64 Total = 0;
	for (const FName& Id : CartIds)
	{
		if (const FAFLCatalogEntry* Entry = Catalog ? Catalog->FindEntry(Id) : nullptr)
		{
			Total += Entry->PriceVolts;
		}
	}
	return Total;
}

// --- Purchase --------------------------------------------------------------------------------------

void UAFLRetailSubsystem::BeginPurchase(FName CosmeticId)
{
	UAFLWalletComponent* Wallet = GetWallet();
	if (!Wallet)
	{
		if (Card) { Card->SetStatus(NSLOCTEXT("AFLRetail", "NoWallet", "Wallet unavailable — nothing charged."), AFLRetail::Bad); }
		return;
	}
	State = ERetailState::Purchasing;
	if (bConfirmRedeem)
	{
		// The ruled credit path (first-3 signup weapons + pool rows): the server validates the pool
		// membership + count; production re-validates at the Lambda past zero.
		Wallet->ServerRequestCreditRedemption(CosmeticId);
	}
	else
	{
		// The proven split (product page / market chassis): PlayFab in shipping, the dev grant path in PIE.
#if UE_BUILD_SHIPPING
		Wallet->ClientRequestPurchase(CosmeticId);
#else
		Wallet->ServerPurchaseCosmetic(CosmeticId);
#endif
	}
	if (Card)
	{
		Card->SetBuyRow(bConfirmRedeem
			? NSLOCTEXT("AFLRetail", "Redeeming", "REDEEMING…")
			: NSLOCTEXT("AFLRetail", "Purchasing", "PURCHASING…"), false);
		Card->SetStatus(FText::GetEmpty(), AFLRetail::Dim);
	}
	bConfirmRedeem = false;
	GrantPolls = 0;
	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		World->GetTimerManager().SetTimer(GrantPollTimer, this, &UAFLRetailSubsystem::PollPurchaseGrant, 0.5f, true);
	}
}

void UAFLRetailSubsystem::PollPurchaseGrant()
{
	++GrantPolls;
	const bool bOwned = IsOwned(CurrentId);
	if (!bOwned && GrantPolls <= 12)
	{
		return;
	}
	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		World->GetTimerManager().ClearTimer(GrantPollTimer);
	}
	State = ERetailState::Browsing;
	if (bOwned)
	{
		bPurchasedCurrent = true;
		RemoveFromCart(CurrentId);
		if (UAFLCosmeticLoadoutComponent* Loadout = GetLoadout())
		{
			Loadout->ServerReleaseTryOn(true); // real entitlement holds the equip now -- no restore
			Loadout->LocalActiveTryOnId = NAME_None;
		}
		if (Card)
		{
			Card->SetWearState(NSLOCTEXT("AFLRetail", "OwnedTag", "OWNED ✓"));
			Card->SetBuyRow(NSLOCTEXT("AFLRetail", "OwnedBuy", "OWNED ✓"), false);
			Card->SetStatus(NSLOCTEXT("AFLRetail", "GrantedWalkOut", "Granted — it's yours, walking out wearing it."), AFLRetail::Good);
		}
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_RETAIL: BOUGHT '%s' on the spot."), *CurrentId.ToString());
	}
	else
	{
		// Refused while already off the pad -> don't leave a stale trial standing; restore and close.
		if (AtPadId != CurrentId)
		{
			CloseCurrent(true);
			return;
		}
		RefreshCard();
		if (Card)
		{
			Card->SetStatus(NSLOCTEXT("AFLRetail", "Refused", "Refused — wallet untouched (funds or availability)."), AFLRetail::Bad);
		}
	}
}

// --- Checkout (sequential validated purchases; one confirm) ----------------------------------------

void UAFLRetailSubsystem::StepCheckout()
{
	State = ERetailState::CheckingOut;
	CheckoutQueue = CartIds;
	CheckoutIndex = 0;
	RefreshChip();
	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		World->GetTimerManager().SetTimer(CheckoutTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (CheckoutIndex < CheckoutQueue.Num())
			{
				const FName Id = CheckoutQueue[CheckoutIndex++];
				if (UAFLWalletComponent* Wallet = GetWallet())
				{
#if UE_BUILD_SHIPPING
					Wallet->ClientRequestPurchase(Id);
#else
					Wallet->ServerPurchaseCosmetic(Id);
#endif
				}
				return;
			}
			if (UWorld* W2 = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
			{
				W2->GetTimerManager().ClearTimer(CheckoutTimer);
				// Grants replicate back -- sweep after a settle window and report honestly.
				W2->GetTimerManager().SetTimer(CheckoutTimer, this, &UAFLRetailSubsystem::FinishCheckout, 2.0f, false);
			}
		}), 0.6f, true);
	}
}

void UAFLRetailSubsystem::FinishCheckout()
{
	int32 NumGranted = 0;
	for (const FName& Id : CheckoutQueue)
	{
		if (IsOwned(Id))
		{
			++NumGranted;
			CartIds.Remove(Id);
			if (Id == CurrentId)
			{
				bPurchasedCurrent = true;
				if (UAFLCosmeticLoadoutComponent* Loadout = GetLoadout())
				{
					Loadout->ServerReleaseTryOn(true);
					Loadout->LocalActiveTryOnId = NAME_None;
				}
				RefreshCard();
			}
		}
	}
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_RETAIL: CHECKOUT %d of %d granted (%d stay in the cart)."),
		NumGranted, CheckoutQueue.Num(), CartIds.Num());
	State = CurrentId.IsNone() ? ERetailState::Idle : ERetailState::Browsing;
	CheckoutQueue.Reset();
	CheckoutIndex = 0;
	RefreshChip();
}

// --- Widgets ---------------------------------------------------------------------------------------

void UAFLRetailSubsystem::ShowCard()
{
	if (Card)
	{
		return;
	}
	APlayerController* PC = GetLocalPC();
	if (!PC)
	{
		return;
	}
	Card = CreateWidget<UAFLW_RetailCard>(PC, UAFLW_RetailCard::StaticClass());
	if (Card)
	{
		Card->SetOwnerSubsystem(this);
		Card->AddToViewport(60);
		BindCardKeys(); // F/C/Q consume ONLY while the card is up (grenade shield)
	}
	RefreshChip(); // restack: the chip lifts above the card
}

void UAFLRetailSubsystem::RefreshCard()
{
	if (!Card || CurrentId.IsNone())
	{
		return;
	}
	const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	const FAFLCatalogEntry* Entry = Catalog ? Catalog->FindEntry(CurrentId) : nullptr;
	if (!Entry)
	{
		Card->SetHeader(FText::FromName(CurrentId), FText::GetEmpty(), FText::GetEmpty());
		Card->SetBuyRow(NSLOCTEXT("AFLRetail", "NoRow", "NOT IN THE CATALOG"), false);
		return;
	}
	Card->SetHeader(
		Entry->DisplayName.IsEmpty() ? FText::FromName(CurrentId) : Entry->DisplayName,
		FText::Format(NSLOCTEXT("AFLRetail", "MetaFmt", "{0} · {1}"),
			FText::FromString(StaticEnum<EAFLCosmeticTier>()->GetNameStringByValue((int64)Entry->Tier)),
			FText::FromString(StaticEnum<EAFLCosmeticType>()->GetNameStringByValue((int64)Entry->Type))),
		Catalog->GetEntryPriceText(*Entry));

	EAFLLoadoutAxis Axis;
	const bool bClassified = UAFLW_FrontEndMarket::ClassifyStoreAxis(CurrentId, Axis);
	const bool bHolding = bClassified && (Axis == EAFLLoadoutAxis::Weapon || Axis == EAFLLoadoutAxis::WeaponSkin);
	Card->SetWearState(bPurchasedCurrent
		? NSLOCTEXT("AFLRetail", "OwnedTag2", "OWNED ✓")
		: (bHolding ? NSLOCTEXT("AFLRetail", "Holding", "HOLDING") : NSLOCTEXT("AFLRetail", "Wearing", "WEARING")));

	const bool bCounted = !Entry->CountedKey.IsNone();
	const FName CreditKey = Entry->bCreditRedeemable ? AFLRetail::CreditKeyForType(Entry->Type) : NAME_None;
	if ((IsOwned(CurrentId) && !bCounted) || bPurchasedCurrent)
	{
		Card->SetBuyRow(NSLOCTEXT("AFLRetail", "OwnedBuy2", "OWNED ✓"), false);
	}
	else if (Entry->bTransactable)
	{
		Card->SetBuyRow(FText::Format(NSLOCTEXT("AFLRetail", "BuyFmt", "[F]  BUY · {0}"),
			Catalog->GetEntryPriceText(*Entry)), true);
	}
	else if (!CreditKey.IsNone())
	{
		// The RULED acquisition for pool weapons: 1 credit per row (the first-3 signup grant seeds 3).
		const UAFLWalletComponent* Wallet = GetWallet();
		const int32 Credits = Wallet ? Wallet->GetCountedEntitlement(CreditKey) : 0;
		if (Credits > 0)
		{
			Card->SetBuyRow(FText::Format(NSLOCTEXT("AFLRetail", "RedeemFmt", "[F]  REDEEM · 1 CREDIT  ({0} left)"),
				FText::AsNumber(Credits)), true);
		}
		else
		{
			Card->SetBuyRow(NSLOCTEXT("AFLRetail", "NoCredits", "NO CREDITS — grab a credit pack"), false);
		}
	}
	else
	{
		Card->SetBuyRow(NSLOCTEXT("AFLRetail", "NotForSale", "NOT FOR SALE"), false);
	}
	Card->SetStatus(FText::GetEmpty(), AFLRetail::Dim);
}

void UAFLRetailSubsystem::DestroyCard()
{
	if (Card)
	{
		Card->RemoveFromParent();
		Card = nullptr;
		UnbindCardKeys(); // F/C/Q return to gameplay the moment the card closes
		RefreshChip(); // restack: the chip drops back to the corner
	}
}

void UAFLRetailSubsystem::RefreshChip()
{
	// Chip discipline: it exists while the cart is non-empty, a checkout is settling, or the player
	// stands at a till (the counter always answers -- lap-2 "TILL Button does not work" was an empty
	// cart making OpenTill a silent no-op).
	if (CartIds.Num() == 0 && State != ERetailState::CheckingOut && !bAtTill)
	{
		if (Chip)
		{
			Chip->RemoveFromParent();
			Chip = nullptr;
		}
		bChipExpanded = false;
		CheckoutIndex = 0;
		return;
	}
	if (!Chip)
	{
		APlayerController* PC = GetLocalPC();
		if (!PC)
		{
			return;
		}
		Chip = CreateWidget<UAFLW_RetailCartChip>(PC, UAFLW_RetailCartChip::StaticClass());
		if (!Chip)
		{
			return;
		}
		Chip->SetOwnerSubsystem(this);
		Chip->AddToViewport(59);
	}
	// Pinned lower-right (operator ruling); stacks ABOVE the small card when both are on screen.
	Chip->SetCornerOffset(Card ? FVector2D(-24.f, -420.f) : FVector2D(-24.f, -24.f));

	// Empty-cart courtesy view at the till.
	if (CartIds.Num() == 0 && State != ERetailState::CheckingOut)
	{
		Chip->SetCartView(NSLOCTEXT("AFLRetail", "ChipEmpty", "🛒 CART EMPTY"), {},
			FText::GetEmpty(),
			NSLOCTEXT("AFLRetail", "ChipEmptyHint", "grab items on the pads — buy on the spot or add here"),
			true);
		return;
	}

	const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	TArray<FText> Lines;
	for (const FName& Id : CartIds)
	{
		const FAFLCatalogEntry* Entry = Catalog ? Catalog->FindEntry(Id) : nullptr;
		Lines.Add(Entry
			? FText::Format(NSLOCTEXT("AFLRetail", "LineFmt", "{0}  ·  {1}"),
				Entry->DisplayName.IsEmpty() ? FText::FromName(Id) : Entry->DisplayName,
				UAFLCosmeticCatalogSubsystem::GetEntryPriceText(*Entry))
			: FText::FromName(Id));
	}
	const FText Summary = FText::Format(NSLOCTEXT("AFLRetail", "ChipFmt", "🛒 {0} · {1} V · [V]"),
		FText::AsNumber(CartIds.Num()), FText::AsNumber(CartTotalVolts()));

	FText CheckoutLabel;
	FText Status;
	if (State == ERetailState::CheckingOut)
	{
		CheckoutLabel = NSLOCTEXT("AFLRetail", "CheckingOut", "CHECKING OUT…");
		Status = NSLOCTEXT("AFLRetail", "SettleHint", "validating each item — refusals stay in the cart");
	}
	else if (CheckoutIndex == INDEX_NONE)
	{
		CheckoutLabel = FText::Format(NSLOCTEXT("AFLRetail", "ConfirmCheckout", "[X]  SURE? — charge {0} V"),
			FText::AsNumber(CartTotalVolts()));
		Status = NSLOCTEXT("AFLRetail", "ConfirmHint2", "X again runs it · V minimizes");
	}
	else
	{
		CheckoutLabel = NSLOCTEXT("AFLRetail", "Checkout", "[X]  CHECKOUT");
		Status = FText::GetEmpty();
	}
	Chip->SetCartView(Summary, Lines, CheckoutLabel, Status, bChipExpanded);
}

// --- Resolution ------------------------------------------------------------------------------------

APlayerController* UAFLRetailSubsystem::GetLocalPC() const
{
	if (APawn* Pawn = LocalPawn.Get())
	{
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			return PC;
		}
	}
	return GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
}

UAFLCosmeticLoadoutComponent* UAFLRetailSubsystem::GetLoadout() const
{
	APlayerController* PC = GetLocalPC();
	APlayerState* PS = PC ? PC->PlayerState : nullptr;
	return PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
}

UAFLWalletComponent* UAFLRetailSubsystem::GetWallet() const
{
	APlayerController* PC = GetLocalPC();
	APlayerState* PS = PC ? PC->PlayerState : nullptr;
	return PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
}

bool UAFLRetailSubsystem::IsOwned(FName CosmeticId) const
{
	const UAFLWalletComponent* Wallet = GetWallet();
	APlayerController* PC = GetLocalPC();
	const ALyraPlayerState* LyraPS = PC ? Cast<ALyraPlayerState>(PC->PlayerState) : nullptr;
	return Wallet && LyraPS && Wallet->IsEntitled(LyraPS, CosmeticId);
}
