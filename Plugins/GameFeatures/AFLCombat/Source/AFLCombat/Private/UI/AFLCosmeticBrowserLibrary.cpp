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

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCosmeticBrowserLibrary)

namespace
{
	// Mirror of UAFLW_LoadoutBase's private axis helpers (kept in sync BY HAND -- in-match is untouched, so it
	// keeps its own copies). Only the two feeders GetOwnedEntriesForAxis needs live here.
	EAFLCosmeticType QueryTypeForAxis(EAFLLoadoutAxis Axis)
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

	FString GetAxisIdPrefix(EAFLLoadoutAxis Axis)
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
	const FString AxisPrefix = GetAxisIdPrefix(Axis);

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
		Catalog->GetEntriesByType(QueryTypeForAxis(Axis), All);
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
