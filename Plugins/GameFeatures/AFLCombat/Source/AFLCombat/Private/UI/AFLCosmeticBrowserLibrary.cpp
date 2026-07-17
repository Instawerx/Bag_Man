// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLCosmeticBrowserLibrary.h"

#include "AFLCosmeticCatalogSubsystem.h"
#include "AFLCosmeticCoreTypes.h"
#include "Cosmetics/AFLCosmeticLoadoutComponent.h"
#include "Cosmetics/AFLCosmeticSelectionTypes.h"
#include "Cosmetics/AFLSkinColorControllerComponent.h"
#include "Cosmetics/AFLWalletComponent.h"
#include "Player/LyraPlayerState.h"      // ALyraPlayerState -- IsEntitled keys entitlement on the exact Lyra PS type
#include "GameFramework/PlayerState.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "AFLCombat.h"                       // LogAFLCombat (preview-chain instrumentation)
#include "Cosmetics/AFLCharacterPartMap.h"  // STORE PREVIEW: identity id -> robot body class
#include "UI/AFLLoadoutDisplayPawn.h"        // STORE PREVIEW: AAFLLoadoutDisplayPawn::SetRobotBody

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCosmeticBrowserLibrary)

namespace
{
	// Mirror of UAFLW_LoadoutBase's private axis helpers (kept in sync BY HAND -- in-match is untouched, so it
	// keeps its own copies). Only the two feeders GetOwnedEntriesForAxis needs live here. NOTE: prefixed "Browser"
	// (distinct from the loadout-base names) so these anonymous-namespace copies do NOT redefine UAFLW_LoadoutBase's
	// same-named anonymous helpers when UBT adaptive-unity merges both .cpp into one TU (the documented AFL trap --
	// see AFLAG_Laser_Pulse; a changed sibling .cpp gets pulled standalone + the rest repack together).
	EAFLCosmeticType BrowserQueryTypeForAxis(EAFLLoadoutAxis Axis)
	{
		switch (Axis)
		{
		case EAFLLoadoutAxis::Beam:       return EAFLCosmeticType::Beam;
		case EAFLLoadoutAxis::BodyColor:  return EAFLCosmeticType::Finish;
		case EAFLLoadoutAxis::EdgeColor:  return EAFLCosmeticType::SkinColor_Edge;
		case EAFLLoadoutAxis::Facemask:   return EAFLCosmeticType::Facemask;
		default:                          return EAFLCosmeticType::Weapon; // Weapon + WeaponSkin; Identity special-cased
		}
	}

	FString BrowserGetAxisIdPrefix(EAFLLoadoutAxis Axis)
	{
		switch (Axis)
		{
		case EAFLLoadoutAxis::Weapon:      return TEXT("AFL.Weapon.");
		case EAFLLoadoutAxis::WeaponSkin:  return TEXT("AFL.WeaponSkin.");
		case EAFLLoadoutAxis::Beam:        return TEXT("AFL.Beam.");
		case EAFLLoadoutAxis::BodyColor:   return TEXT("AFL.Finish.");
		case EAFLLoadoutAxis::EdgeColor:   return TEXT("AFL.Edge.");
		case EAFLLoadoutAxis::Facemask:    return TEXT("AFL.Facemask.");
		default:                           return FString(); // Identity -> dual-type query, no single prefix
		}
	}
}

void UAFLCosmeticBrowserLibrary::GetPurchasableEntries(const UObject* WorldContext, TArray<FAFLCatalogEntry>& OutEntries)
{
	OutEntries.Reset();
	if (UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(WorldContext))
	{
		Catalog->GetPurchasableEntries(OutEntries);
	}
}

void UAFLCosmeticBrowserLibrary::GetOwnedEntriesForAxis(const UObject* WorldContext, const APlayerState* PS, EAFLLoadoutAxis Axis, TArray<FAFLCatalogEntry>& OutOwned)
{
	OutOwned.Reset();

	const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(WorldContext);
	if (!Catalog)
	{
		return;
	}

	// GrantedFree is owned by everyone; a paid item requires the wallet's owned-set -> a MISSING wallet shows
	// ONLY the free base (mirrors the loadout's fix for the over-permissive leak).
	const UAFLWalletComponent* Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
	const FString AxisPrefix = BrowserGetAxisIdPrefix(Axis);

	TArray<const FAFLCatalogEntry*> All;
	if (Axis == EAFLLoadoutAxis::Identity)
	{
		Catalog->GetEntriesByType(EAFLCosmeticType::Team, All);
		TArray<const FAFLCatalogEntry*> Characters;
		Catalog->GetEntriesByType(EAFLCosmeticType::Character, Characters);
		All.Append(Characters);
	}
	else
	{
		Catalog->GetEntriesByType(BrowserQueryTypeForAxis(Axis), All);
	}

	for (const FAFLCatalogEntry* Entry : All)
	{
		if (!Entry)
		{
			continue;
		}
		// EAFLCosmeticType::Weapon is overloaded (weapons AND weapon-skins) -> filter to the axis's namespace.
		if (!AxisPrefix.IsEmpty() && !Entry->CosmeticId.ToString().StartsWith(AxisPrefix, ESearchCase::IgnoreCase))
		{
			continue;
		}
		const bool bGrantedFree = (Entry->Acquisition == EAFLAcquisition::GrantedFree);
		const bool bOwned = bGrantedFree || (Wallet != nullptr && Wallet->IsEntitled(Cast<ALyraPlayerState>(PS), Entry->CosmeticId));
		if (bOwned)
		{
			OutOwned.Add(*Entry);
		}
	}
}

FName UAFLCosmeticBrowserLibrary::GetEquippedIdForAxis(const UAFLCosmeticLoadoutComponent* Loadout, EAFLLoadoutAxis Axis)
{
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
	case EAFLLoadoutAxis::Identity:    return Sel.GetActiveIdentityId();
	case EAFLLoadoutAxis::BodyColor:   return Sel.BodyId;
	case EAFLLoadoutAxis::EdgeColor:   return Sel.EdgeId;
	case EAFLLoadoutAxis::Facemask:    return Sel.FacemaskId;
	default:                           return NAME_None;
	}
}

void UAFLCosmeticBrowserLibrary::EquipForAxis(UAFLCosmeticLoadoutComponent* Loadout, EAFLLoadoutAxis Axis, FName CosmeticId)
{
	if (!Loadout)
	{
		return;
	}

	FAFLCosmeticSelection Sel = Loadout->GetSelection(); // copy current; set the ONE axis field

	// ServerSetCosmeticSelection's _Validate rejects an identity-less selection, so seed the free IRONICS team
	// if the player never picked an identity (mirrors the loadout + the proven cheat).
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
	case EAFLLoadoutAxis::Identity:
		if (CosmeticId.ToString().StartsWith(TEXT("AFL.Character."), ESearchCase::IgnoreCase))
		{
			Sel.IdentityType = EAFLIdentityType::Character;
			Sel.CharacterId = CosmeticId;
		}
		else
		{
			Sel.IdentityType = EAFLIdentityType::Team;
			Sel.TeamId = CosmeticId;
		}
		break;
	case EAFLLoadoutAxis::BodyColor:   Sel.BodyId = CosmeticId; break;
	case EAFLLoadoutAxis::EdgeColor:   Sel.EdgeId = CosmeticId; break;
	case EAFLLoadoutAxis::Facemask:    Sel.FacemaskId = CosmeticId; break;
	default:                           return;
	}

	// BlueprintAuthorityOnly -> dispatching from C++ sends the client->server RPC (a BP node would self-gate).
	Loadout->ServerSetCosmeticSelection(Sel);
}

void UAFLCosmeticBrowserLibrary::ApplySelectionToPawn(AController* Controller, APawn* Pawn)
{
	if (!Controller || !Pawn)
	{
		return;
	}
	UAFLSkinColorControllerComponent* SkinCtrl = Controller->FindComponentByClass<UAFLSkinColorControllerComponent>();
	if (!SkinCtrl)
	{
		return;
	}
	// Proven composition order (mirrors UAFLCosmeticLoadoutComponent::NudgeControllerReapply): the material
	// SWAP (facemask) before the param push (skin), then the weapon axes.
	SkinCtrl->RefreshFacemaskForPawn(Pawn);
	SkinCtrl->RefreshSkinForPawn(Pawn);
	SkinCtrl->RefreshWeaponForPawn(Pawn);
	SkinCtrl->RefreshWeaponSkinForPawn(Pawn);
	SkinCtrl->RefreshBeamColorForPawn(Pawn);
}

// ===================================================================================================
//  STORE PREVIEW (front-end try-before-buy). Preview = a TEMPORARY visual apply with NO commit; the
//  entitlement gate lives only in the commit (ServerSetCosmeticSelection), so unowned ids preview for
//  free. The 5 Refresh*ForPawn read the controller's PreviewSelection override; Identity swaps the body.
// ===================================================================================================

namespace
{
	// Resolve an identity CosmeticId -> the robot body class (mirrors the market's equip identity path, IRONICS
	// fallback). SetRobotBody is idempotent (guarded), so preview + revert can call this freely.
	UClass* ResolveIdentityRobotClass(FName IdentityId)
	{
		static UAFLCharacterPartMap* PartMap = LoadObject<UAFLCharacterPartMap>(nullptr,
			TEXT("/Game/BagMan/Characters/Cosmetics/SkinColors/DA_AFL_CharacterPartMap.DA_AFL_CharacterPartMap"));
		UClass* RobotCls = nullptr;
		if (PartMap && !IdentityId.IsNone())
		{
			const TSoftClassPtr<AActor> Soft = PartMap->ResolveCharacterPart(IdentityId);
			if (!Soft.IsNull())
			{
				RobotCls = Soft.LoadSynchronous();
			}
		}
		if (!RobotCls)
		{
			RobotCls = LoadClass<AActor>(nullptr, TEXT("/Game/BagMan/Characters/Cosmetics/B_AFL_Robot_IRONICS.B_AFL_Robot_IRONICS_C"));
		}
		return RobotCls;
	}
}

void UAFLCosmeticBrowserLibrary::ApplyPreview(AController* Controller, APawn* DisplayPawn, EAFLLoadoutAxis Axis, FName CosmeticId)
{
	if (!Controller || !DisplayPawn)
	{
		return;
	}
	UAFLSkinColorControllerComponent* SkinCtrl = Controller->FindComponentByClass<UAFLSkinColorControllerComponent>();
	// [PREVIEW-DIAG] step e -- reached the library; is the SkinCtrl present on this controller?
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_BROWSER: [PREVIEW e] ApplyPreview reached ctrl=%d disp=%d skinCtrl=%d axis=%d id=%s."),
		Controller != nullptr, DisplayPawn != nullptr, SkinCtrl != nullptr, (int32)Axis, *CosmeticId.ToString());
	if (!SkinCtrl)
	{
		return;
	}

	// Seed the preview from the player's COMMITTED selection so the un-previewed axes stay as the real loadout --
	// only the selected item differs. Re-seeding each call means only the CURRENTLY selected item is previewed.
	FAFLCosmeticSelection Preview;
	const APlayerState* PS = Controller->PlayerState;
	if (const UAFLCosmeticLoadoutComponent* Loadout = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr)
	{
		Preview = Loadout->GetSelection();
	}
	// Seed the free IRONICS identity if none, so an identity-less committed selection still resolves a body.
	if (Preview.GetActiveIdentityId() == NAME_None)
	{
		Preview.IdentityType = EAFLIdentityType::Team;
		Preview.TeamId = FName(TEXT("AFL.Team.IRONICS"));
	}

	// Set the ONE previewed axis (same axis-switch as EquipForAxis, but NO ServerSetCosmeticSelection -> no gate).
	switch (Axis)
	{
	case EAFLLoadoutAxis::Weapon:      Preview.WeaponId = CosmeticId; break;
	case EAFLLoadoutAxis::WeaponSkin:  Preview.WeaponSkinId = CosmeticId; break;
	case EAFLLoadoutAxis::Beam:        Preview.BeamId = CosmeticId; break;
	case EAFLLoadoutAxis::Identity:
		if (CosmeticId.ToString().StartsWith(TEXT("AFL.Character."), ESearchCase::IgnoreCase))
		{
			Preview.IdentityType = EAFLIdentityType::Character;
			Preview.CharacterId = CosmeticId;
		}
		else
		{
			Preview.IdentityType = EAFLIdentityType::Team;
			Preview.TeamId = CosmeticId;
		}
		break;
	case EAFLLoadoutAxis::BodyColor:   Preview.BodyId = CosmeticId; break;
	case EAFLLoadoutAxis::EdgeColor:   Preview.EdgeId = CosmeticId; break;
	case EAFLLoadoutAxis::Facemask:    Preview.FacemaskId = CosmeticId; break;
	default:                           return;
	}

	// WEAPON FIX (b) -- a weapon SKIN or BEAM needs a weapon to land on. If the effective selection has NO weapon
	// (bare hand), inject a showcase default gun FOR THE PREVIEW so the skin/beam has a surface; RefreshWeaponForPawn
	// (the proven equip path) spawns it. Body/edge/visor/identity previews don't need it. On revert the committed
	// WeaponId (None here) unequips this preview gun again -> the resting state stays the clean, bare-handed robot.
	if ((Axis == EAFLLoadoutAxis::WeaponSkin || Axis == EAFLLoadoutAxis::Beam) && Preview.WeaponId.IsNone())
	{
		Preview.WeaponId = FName(TEXT("AFL.Weapon.Arclight")); // proven pilot weapon; reads as a showcase rifle
	}

	SkinCtrl->SetPreviewSelection(Preview);
	// [PREVIEW-DIAG] step f -- override set; fanning out. Enable `afl.SkinDiag 1` to see RefreshWeapon read this id.
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_BROWSER: [PREVIEW f] preview override set weaponId=%s -> fan-out now."),
		*Preview.WeaponId.ToString());

	// Identity previews the robot BODY (a character-part swap, not part of the Refresh spine) -- BEFORE the fan-out
	// so the skin controller re-paints the NEW body. SetRobotBody is idempotent, so a non-identity axis leaves the
	// body untouched here.
	if (Axis == EAFLLoadoutAxis::Identity)
	{
		if (AAFLLoadoutDisplayPawn* Disp = Cast<AAFLLoadoutDisplayPawn>(DisplayPawn))
		{
			if (UClass* RobotCls = ResolveIdentityRobotClass(Preview.GetActiveIdentityId()))
			{
				Disp->SetRobotBody(RobotCls);
			}
		}
	}

	// Repaint: the 5 Refresh*ForPawn now read the preview override.
	ApplySelectionToPawn(Controller, DisplayPawn);
}

void UAFLCosmeticBrowserLibrary::RevertToSaved(AController* Controller, APawn* DisplayPawn)
{
	if (!Controller || !DisplayPawn)
	{
		return;
	}
	UAFLSkinColorControllerComponent* SkinCtrl = Controller->FindComponentByClass<UAFLSkinColorControllerComponent>();
	if (!SkinCtrl)
	{
		return;
	}

	// Drop the override -> the Refresh*ForPawn read the committed selection again.
	SkinCtrl->ClearPreviewSelection();

	// Restore the committed robot body (a prior Identity preview may have swapped it). SetRobotBody's idempotency
	// guard makes this a no-op when the body is already the committed identity (the common IRONICS case).
	const APlayerState* PS = Controller->PlayerState;
	if (const UAFLCosmeticLoadoutComponent* Loadout = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr)
	{
		if (AAFLLoadoutDisplayPawn* Disp = Cast<AAFLLoadoutDisplayPawn>(DisplayPawn))
		{
			if (UClass* RobotCls = ResolveIdentityRobotClass(Loadout->GetSelection().GetActiveIdentityId()))
			{
				Disp->SetRobotBody(RobotCls);
			}
		}
	}

	ApplySelectionToPawn(Controller, DisplayPawn);
}
