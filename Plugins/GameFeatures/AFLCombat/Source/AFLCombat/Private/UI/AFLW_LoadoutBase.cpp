// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_LoadoutBase.h"

#include "Cosmetics/AFLCosmeticLoadoutComponent.h"
#include "Cosmetics/AFLWalletComponent.h"       // UAFLWalletComponent::IsEntitled (the public entitlement check)
#include "AFLCosmeticCatalogSubsystem.h"
#include "Player/LyraPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Input/CommonUIInputTypes.h"
#include "UI/AFLW_LoadoutTileBase.h"
#include "Blueprint/UserWidget.h"
#include "Components/PanelWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/Pawn.h"

#if !UE_BUILD_SHIPPING
#include "Engine/LocalPlayer.h"
#include "PrimaryGameLayout.h"
#include "GameplayTagContainer.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_LoadoutBase)

namespace
{
	/** The catalog Type to query for a loadout axis. Weapon AND WeaponSkin BOTH live under Type==Weapon (the
	 *  on-disk overload -- there is no WeaponSkin EAFLCosmeticType); they are split by namespace below. Beam
	 *  is its own Type. */
	EAFLCosmeticType QueryTypeForAxis(EAFLLoadoutAxis Axis)
	{
		switch (Axis)
		{
		case EAFLLoadoutAxis::Beam: return EAFLCosmeticType::Beam;
		default:                    return EAFLCosmeticType::Weapon; // Weapon + WeaponSkin
		}
	}

	/** The CosmeticId namespace prefix that disambiguates an axis within its (possibly overloaded) Type.
	 *  "AFL.Weapon." excludes "AFL.WeaponSkin." (the char after "Weapon" is '.' vs 'S'), and vice-versa. */
	FString GetAxisIdPrefix(EAFLLoadoutAxis Axis)
	{
		switch (Axis)
		{
		case EAFLLoadoutAxis::Weapon:      return TEXT("AFL.Weapon.");
		case EAFLLoadoutAxis::WeaponSkin:  return TEXT("AFL.WeaponSkin.");
		case EAFLLoadoutAxis::Beam:        return TEXT("AFL.Beam.");
		default:                           return FString();
		}
	}
}

UAFLCosmeticLoadoutComponent* UAFLW_LoadoutBase::GetLoadoutComponent() const
{
	const APlayerController* PC = GetOwningPlayer();
	const APlayerState* PS = PC ? PC->PlayerState : nullptr;
	return PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
}

const ALyraPlayerState* UAFLW_LoadoutBase::GetLyraPlayerState() const
{
	const APlayerController* PC = GetOwningPlayer();
	return PC ? Cast<ALyraPlayerState>(PC->PlayerState) : nullptr;
}

UAFLCosmeticCatalogSubsystem* UAFLW_LoadoutBase::GetCatalog() const
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<UAFLCosmeticCatalogSubsystem>();
		}
	}
	return nullptr;
}

void UAFLW_LoadoutBase::GetOwnedEntriesForAxis(EAFLLoadoutAxis Axis, TArray<FAFLCatalogEntry>& OutOwned) const
{
	OutOwned.Reset();

	const UAFLCosmeticCatalogSubsystem* Catalog = GetCatalog();
	if (!Catalog)
	{
		return;
	}

	// The entitlement source IS the wallet (UAFLCosmeticLoadoutComponent::GetEntitlementSource resolves it,
	// but that resolver is private) -- so resolve the same UAFLWalletComponent off the PlayerState directly.
	const ALyraPlayerState* PS = GetLyraPlayerState();
	const UAFLWalletComponent* Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
	const FString AxisPrefix = GetAxisIdPrefix(Axis);

	int32 Scanned = 0;
	TArray<const FAFLCatalogEntry*> All;
	Catalog->GetEntriesByType(QueryTypeForAxis(Axis), All);
	for (const FAFLCatalogEntry* Entry : All)
	{
		if (!Entry)
		{
			continue;
		}
		++Scanned;

		// EAFLCosmeticType::Weapon is OVERLOADED on disk -- weapons AND weapon-skins both carry Type==Weapon.
		// Filter to the axis's OWN id-namespace so the weapon picker shows only AFL.Weapon.* (not the skins).
		if (!AxisPrefix.IsEmpty() && !Entry->CosmeticId.ToString().StartsWith(AxisPrefix, ESearchCase::IgnoreCase))
		{
			continue;
		}

		// OWNED-ONLY: GrantedFree is owned by EVERYONE (no wallet needed); a paid item requires the wallet's
		// owned-set. So a MISSING wallet shows ONLY the free base -- NOT everything (the earlier over-permissive
		// bug that leaked unowned paid items, which then rejected on the equip server-validation).
		const bool bGrantedFree = (Entry->Acquisition == EAFLAcquisition::GrantedFree);
		const bool bOwned = bGrantedFree || (Wallet != nullptr && Wallet->IsEntitled(PS, Entry->CosmeticId));
		if (bOwned)
		{
			OutOwned.Add(*Entry);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[AFLLoadout] GetOwnedEntriesForAxis(type=%d): scanned=%d owned=%d wallet=%s prefix=%s"),
		(int32)Axis, Scanned, OutOwned.Num(), (Wallet ? TEXT("yes") : TEXT("no")), *AxisPrefix);
}

FName UAFLW_LoadoutBase::GetEquippedIdForAxis(EAFLLoadoutAxis Axis) const
{
	const UAFLCosmeticLoadoutComponent* Loadout = GetLoadoutComponent();
	if (!Loadout)
	{
		return NAME_None;
	}

	const FAFLCosmeticSelection& Sel = Loadout->GetSelection();
	switch (Axis)
	{
	case EAFLLoadoutAxis::Weapon:      return Sel.WeaponId;
	case EAFLLoadoutAxis::WeaponSkin:  return Sel.WeaponSkinId;
	case EAFLLoadoutAxis::Beam:        return Sel.BeamId;
	default:                           return NAME_None;
	}
}

void UAFLW_LoadoutBase::EquipForAxis(EAFLLoadoutAxis Axis, FName CosmeticId)
{
	UAFLCosmeticLoadoutComponent* Loadout = GetLoadoutComponent();
	if (!Loadout)
	{
		return;
	}

	FAFLCosmeticSelection Sel = Loadout->GetSelection(); // copy current selection; set the ONE axis field

	// ServerSetCosmeticSelection's _Validate REJECTS an identity-less selection (GetActiveIdentityId()==None),
	// so a fresh player who never picked an identity would have this RPC silently DROPPED (the click logs, but
	// nothing equips). Seed the free default identity (IRONICS, GrantedFree) if none is set -- mirrors the proven
	// afl.Cosmetic.SetWeapon cheat, which seeds a default team for exactly this reason.
	if (Sel.GetActiveIdentityId() == NAME_None)
	{
		Sel.IdentityType = EAFLIdentityType::Team;
		Sel.TeamId = FName(TEXT("AFL.Team.IRONICS"));
	}

	switch (Axis)
	{
	case EAFLLoadoutAxis::Weapon:      Sel.WeaponId = CosmeticId; break;
	case EAFLLoadoutAxis::WeaponSkin:  Sel.WeaponSkinId = CosmeticId; break;
	case EAFLLoadoutAxis::Beam:        Sel.BeamId = CosmeticId; break;
	default:                           return;
	}

	// ServerSetCosmeticSelection is BlueprintAuthorityOnly; dispatching from C++ sends the client->server RPC.
	Loadout->ServerSetCosmeticSelection(Sel);
}

TOptional<FUIInputConfig> UAFLW_LoadoutBase::GetDesiredInputConfig() const
{
	// Menu input while the locker owns the screen: cursor visible + clickable tiles (mirrors the match-end takeover).
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UAFLW_LoadoutBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UAFLW_LoadoutBase::HandleCloseClicked);
	}
}

void UAFLW_LoadoutBase::NativeOnActivated()
{
	Super::NativeOnActivated();
	RebuildTiles();        // populate the owned grid when the locker opens
	SetupPreviewCapture(); // start the live 3D preview of the REAL pawn
}

void UAFLW_LoadoutBase::NativeOnDeactivated()
{
	TeardownPreviewCapture();
	Super::NativeOnDeactivated();
}

APawn* UAFLW_LoadoutBase::GetLocalPawn() const
{
	const APlayerController* PC = GetOwningPlayer();
	return PC ? PC->GetPawn() : nullptr;
}

void UAFLW_LoadoutBase::SetupPreviewCapture()
{
	APawn* Pawn = GetLocalPawn();
	UWorld* World = GetWorld();
	if (!Pawn || !World)
	{
		return; // no pawn yet (e.g. pre-spawn) -> no preview; the locker still works.
	}

	// Runtime render target (transient; sized from PreviewResolution). Created once, reused across opens.
	if (!PreviewRT)
	{
		PreviewRT = NewObject<UTextureRenderTarget2D>(this);
		PreviewRT->ClearColor = FLinearColor(0.01f, 0.02f, 0.05f, 1.f); // dark glass backdrop
		PreviewRT->InitCustomFormat(PreviewResolution.X, PreviewResolution.Y, PF_B8G8R8A8, false);
		PreviewRT->UpdateResourceImmediate(true);
	}

	// Spawn a scene-capture actor + ATTACH it to the pawn so it follows if the pawn drifts. Front-3/4 framing.
	if (!PreviewCapture.IsValid())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;
		SpawnParams.Owner = Pawn;
		PreviewCapture = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), SpawnParams);
	}
	ASceneCapture2D* Cap = PreviewCapture.Get();
	if (!Cap)
	{
		return;
	}

	Cap->AttachToActor(Pawn, FAttachmentTransformRules::KeepRelativeTransform);
	Cap->SetActorRelativeLocation(PreviewCamOffset);
	Cap->SetActorRelativeRotation((PreviewFocusOffset - PreviewCamOffset).Rotation()); // look back at the chest

	if (USceneCaptureComponent2D* CapComp = Cap->GetCaptureComponent2D())
	{
		CapComp->TextureTarget = PreviewRT;
		CapComp->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR; // lit + post (neon bloom reads true)
		CapComp->FOVAngle = PreviewFOV;
		CapComp->bCaptureEveryFrame = true;  // LIVE: an equip updates the REAL pawn -> the next capture shows it
		CapComp->bCaptureOnMovement = false;
		// Full-scene capture (no ShowOnlyList) so the equipped weapon -- a SEPARATE actor attached to the pawn's
		// hand -- is captured too. Isolating the pawn+weapon onto a clean backdrop is a later polish.
	}

	// Route the render target into the center-stage image.
	if (PreviewImage && PreviewRT)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(PreviewRT);
		Brush.ImageSize = FVector2D(PreviewResolution.X, PreviewResolution.Y);
		PreviewImage->SetBrush(Brush);
	}
}

void UAFLW_LoadoutBase::TeardownPreviewCapture()
{
	if (ASceneCapture2D* Cap = PreviewCapture.Get())
	{
		Cap->Destroy();
	}
	PreviewCapture = nullptr;
}

void UAFLW_LoadoutBase::RebuildTiles()
{
	// Rebuild every axis grid from the current owned-set + selection. Skin/Beam containers are optional (the
	// Inc-1 WBP had only the weapon TileContainer) -> a null container is skipped inside RebuildAxisTiles.
	RebuildAxisTiles(EAFLLoadoutAxis::Weapon,     TileContainer);
	RebuildAxisTiles(EAFLLoadoutAxis::WeaponSkin, SkinTileContainer);
	RebuildAxisTiles(EAFLLoadoutAxis::Beam,       BeamTileContainer);
}

void UAFLW_LoadoutBase::RebuildAxisTiles(EAFLLoadoutAxis Axis, UPanelWidget* Container)
{
	if (!Container)
	{
		return; // this axis's grid isn't present in the WBP (optional container) -> skip.
	}
	Container->ClearChildren();
	if (!TileClass)
	{
		return;
	}

	TArray<FAFLCatalogEntry> Owned;
	GetOwnedEntriesForAxis(Axis, Owned);
	const FName EquippedId = GetEquippedIdForAxis(Axis);

	for (const FAFLCatalogEntry& Entry : Owned)
	{
		UAFLW_LoadoutTileBase* Tile = CreateWidget<UAFLW_LoadoutTileBase>(this, TileClass);
		if (!Tile)
		{
			continue;
		}

		// Prefer the marketing DisplayName; fall back to the CosmeticId's last token ("AFL.Weapon.Voltaic" -> "Voltaic").
		FText Label = Entry.DisplayName;
		if (Label.IsEmpty())
		{
			const FString IdStr = Entry.CosmeticId.ToString();
			FString Left, Right;
			Label = FText::FromString(IdStr.Split(TEXT("."), &Left, &Right, ESearchCase::IgnoreCase, ESearchDir::FromEnd) ? Right : IdStr);
		}

		Tile->SetTileData(Axis, Entry.CosmeticId, Label, Entry.CosmeticId == EquippedId);
		Tile->OnTileClicked.AddDynamic(this, &UAFLW_LoadoutBase::HandleTileClicked);
		Container->AddChild(Tile);
	}
}

void UAFLW_LoadoutBase::HandleTileClicked(EAFLLoadoutAxis Axis, FName CosmeticId)
{
	UE_LOG(LogTemp, Log, TEXT("[AFLLoadout] tile clicked -> equip %s (axis=%d)"), *CosmeticId.ToString(), (int32)Axis);
	EquipForAxis(Axis, CosmeticId);
	RebuildTiles(); // refresh EQUIPPED badges across all axes (optimistic; the replicated selection catches up)
}

void UAFLW_LoadoutBase::HandleCloseClicked()
{
	DeactivateWidget(); // pop the locker off UI.Layer.Menu
}

#if !UE_BUILD_SHIPPING
// Dev-only PIE-open for the Increment-1 prove. The PLAYER entry is the hub / pre-match-lobby button
// (decision 6), wired in the WBP layer; this command just guarantees a reliable open path for the prove.
static FAutoConsoleCommandWithWorld GAFLLoadoutOpenCmd(
	TEXT("afl.Loadout.Open"),
	TEXT("Dev: push the IRONICS Loadout locker (WBP_AFL_Loadout) onto UI.Layer.Menu."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (!PC)
		{
			return;
		}
		UPrimaryGameLayout* Layout = UPrimaryGameLayout::GetPrimaryGameLayout(PC);
		if (!Layout)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AFLLoadout] afl.Loadout.Open: no PrimaryGameLayout."));
			return;
		}
		const FSoftClassPath WbpPath(TEXT("/Game/BagMan/UI/Loadout/WBP_AFL_Loadout.WBP_AFL_Loadout_C"));
		UClass* WbpClass = WbpPath.TryLoadClass<UAFLW_LoadoutBase>();
		if (!WbpClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AFLLoadout] afl.Loadout.Open: WBP_AFL_Loadout not found at %s (author it first)."), *WbpPath.ToString());
			return;
		}
		static const FGameplayTag MenuLayer = FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Menu"));
		Layout->PushWidgetToLayerStack<UAFLW_LoadoutBase>(MenuLayer, WbpClass);
		UE_LOG(LogTemp, Log, TEXT("[AFLLoadout] afl.Loadout.Open: pushed WBP_AFL_Loadout to UI.Layer.Menu."));
	}));
#endif
