// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/TimerHandle.h"

#include "AFLRetailSubsystem.generated.h"

class APawn;
class APlayerController;
class UAFLW_RetailCard;
class UAFLW_RetailCartChip;
class UAFLCosmeticLoadoutComponent;
class UAFLWalletComponent;

/** How a grab pad arms the try-on (operator playtest knob -- plan "INTENTIONAL pickups"). */
UENUM(BlueprintType)
enum class EAFLGrabArmMode : uint8
{
	Dwell  UMETA(DisplayName = "Dwell (short stand on the pad arms it)"),
	Press  UMETA(DisplayName = "Press (explicit E-grab arms it)")
};

/**
 * UAFLRetailSubsystem -- the CLIENT-LOCAL orchestrator of distributed retail
 * (PX_DISTRIBUTED_RETAIL_PLAN, operator greenlight 2026-08-31).
 *
 * One brain, five surfaces: grab pads (AFLHub) report enter/leave; this subsystem owns the dwell/press
 * ARM gate, the try-on (server map-exception grant through the REAL selection seam -- the loadout
 * component's ServerRequestTryOn), the SMALL CARD (corner, world never dims), the session CART + chip,
 * and CHECKOUT (till pad or from-chip -- operator ruled BOTH). Everything visual is client-local; the
 * server sees only the try-on RPCs and the final purchase RPCs.
 *
 * Keys (lazy-bound, never consuming -- the world stays live): F buy/confirm, C add-to-cart,
 * Q discard/back, E details (full product page, the DETAILED tier), V cart chip open/minimize
 * (player discretion, ruled), X checkout/confirm.
 */
UCLASS()
class AFLCOMBAT_API UAFLRetailSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UAFLRetailSubsystem* Get(const UObject* WorldContext);

	//~ Pad seam (AFLHub grab pads call these; local-pawn events only) ----------------------------------

	/** The local pawn stepped onto a pad. Dwell mode starts the arm timer; Press mode waits for E. */
	void PadEntered(FName CosmeticId, EAFLGrabArmMode ArmMode, float DwellSeconds, APawn* LocalPawn);

	/** The local pawn left the pad: cancel an un-armed dwell; if this id is on trial and unbought,
	 *  release (server restores the baseline) and close the card. "Nothing leaves the store unbought." */
	void PadLeft(FName CosmeticId);

	/** Till pad (PX counter): open the cart chip expanded with checkout focused. */
	void OpenTill();

	//~ Cart (client-local, session-only by ruling) -----------------------------------------------------

	void AddToCart(FName CosmeticId);
	void RemoveFromCart(FName CosmeticId);
	int32 CartCount() const { return CartIds.Num(); }
	int64 CartTotalVolts() const;
	const TArray<FName>& GetCartIds() const { return CartIds; }

private:
	enum class ERetailState : uint8 { Idle, Browsing, ConfirmBuy, Purchasing, CheckingOut };

	//~ Arming ------------------------------------------------------------------------------------------
	void ArmTryOn();
	void CloseCurrent(bool bReleaseServer);

	//~ Key handlers (bConsumeInput=false; guarded by state so idle presses cost nothing) ---------------
	void EnsureKeyBinds(APawn* LocalPawn);
	void OnKeyGrabDetails();  // E
	void OnKeyBuy();          // F
	void OnKeyCart();         // C
	void OnKeyDiscard();      // Q
	void OnKeyChipToggle();   // V
	void OnKeyCheckout();     // X

	//~ Purchase / checkout -----------------------------------------------------------------------------
	void BeginPurchase(FName CosmeticId);
	void PollPurchaseGrant();
	void StepCheckout();
	void FinishCheckout();

	//~ Widgets -----------------------------------------------------------------------------------------
	void ShowCard();
	void RefreshCard();
	void DestroyCard();
	void RefreshChip();

	//~ Resolution helpers (all local-player scoped) ----------------------------------------------------
	APlayerController* GetLocalPC() const;
	UAFLCosmeticLoadoutComponent* GetLoadout() const;
	UAFLWalletComponent* GetWallet() const;
	bool IsOwned(FName CosmeticId) const;

	ERetailState State = ERetailState::Idle;

	/** The pad the pawn is standing on (armed or not). */
	FName AtPadId;
	EAFLGrabArmMode AtPadArmMode = EAFLGrabArmMode::Dwell;

	/** The id on trial / in the card. */
	FName CurrentId;
	bool bPurchasedCurrent = false;

	TWeakObjectPtr<APawn> LocalPawn;
	TWeakObjectPtr<APlayerController> BoundPC;

	UPROPERTY(Transient)
	TObjectPtr<UAFLW_RetailCard> Card;

	UPROPERTY(Transient)
	TObjectPtr<UAFLW_RetailCartChip> Chip;

	TArray<FName> CartIds;
	bool bChipExpanded = false;

	FTimerHandle DwellTimer;
	FTimerHandle GrantPollTimer;
	int32 GrantPolls = 0;

	/** Sequential checkout drive (0.6s stagger -> sweep). */
	FTimerHandle CheckoutTimer;
	TArray<FName> CheckoutQueue;
	int32 CheckoutIndex = 0;

	friend class UAFLW_RetailCard;
	friend class UAFLW_RetailCartChip;
};
