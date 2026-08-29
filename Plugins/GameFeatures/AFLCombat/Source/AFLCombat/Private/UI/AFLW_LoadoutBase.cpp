// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_LoadoutBase.h"

#include "Cosmetics/AFLPreviewRigSubsystem.h" // C1: the ONE owner of the display pawn + pod
#include "UI/AFLW_Creator.h"                 // CC-5: the creator this loadout opens
#include "PrimaryGameLayout.h"                // CC-5: PushWidgetToLayerStack -- the loadout's own push pattern
#include "NativeGameplayTags.h"              // CC-5: the menu layer tag
#include "AFLCombat.h"                    // CC-5: LogAFLCombat -- this file never logged before
#include "Components/TextBlock.h"          // CC-5 step 3: detail panel + commit label

// Same string every other push in this module uses; each file declares its own static, which is the
// established convention here rather than a shared header.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_Layer_Menu_Creator, "UI.Layer.Menu");

#include "Cosmetics/AFLCosmeticLoadoutComponent.h"
#include "Cosmetics/AFLWalletComponent.h"       // UAFLWalletComponent::IsEntitled (the public entitlement check)
#include "Cosmetics/AFLSkinColorAsset.h"        // swatch color-resolve (ColorParameters)
#include "AFLCosmeticCatalogSubsystem.h"
#include "AFLColorIdentityRegistry.h"    // FAFLColorIdentity / FAFLSkinFinish -- registry-aware swatch resolve (same source as the pawn)
#include "Cosmetics/AFLSkinColorControllerComponent.h" // the proven Refresh*ForPawn fan-out (driven at the display pawn)
#include "Cosmetics/AFLCharacterPartMap.h"             // identity -> robot body class (display-pawn IDENTITY axis)
#include "Player/LyraPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
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
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "UI/AFLLoadoutPod.h"
#include "UI/AFLLoadoutDisplayPawn.h"
#include "Cosmetics/AFLCharacterPartActor.h" // CollectPartsOn -- measured-feet pod grounding

#if !UE_BUILD_SHIPPING
#include "Engine/LocalPlayer.h"
#include "PrimaryGameLayout.h"
#include "GameplayTagContainer.h"
#endif

#include "Cook/AFLCookedAssetRegistry.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_LoadoutBase)

// Declared AND enrolled for cook validation in one statement. MEASURED 2026-08-09: absent from
// the cooked build -- the whole /Game/BagMan/UI/Loadout folder was uncooked, so afl.Loadout.Open
// took its "author it first" early-out on every cooked client. Declared at FILE scope on purpose:
// the load below lives in a console-command lambda, and a function-scope static would not register
// until that command ran, which would defeat the startup sweep entirely.
AFL_COOKED_ASSET(GAFLLoadoutRootWidget,
	TEXT("/Game/BagMan/UI/Loadout/WBP_AFL_Loadout.WBP_AFL_Loadout_C"));

// Live-tunable preview framing (afl.Loadout.Preview* -> tune at the prove without a rebuild). Defaults match
// the UPROPERTY seeds; RepositionPreviewCamera reads them per-tick.
// Defaults tuned for a FULL-BODY, capsule-filling frame (lower look-at includes the feet, less downward
// angle). Framing is operator-eyeball-tuned live from here via these cvars -- no rebuild.
// Defaults computed to FILL the panel with the 270cm capsule (camera ~180cm out, look-at the pod's vertical
// mid). All live-tunable in PIE via these cvars -- no rebuild -- so the operator dials the final composition.
// AUTO-FRAMED (2026-08-28): the camera derives distance + focus from the measured worn-body bounds;
// these are TRIMS on top of that (all default 0 except the 3/4 lateral). Old hand-tuned absolutes
// (fwd 180 / up 47 / focus 21 / FOV 82) belonged to the floating composition.
static TAutoConsoleVariable<float> CVarLoadoutPreviewFwd(TEXT("afl.Loadout.PreviewFwd"), 0.f, TEXT("EXTRA camera distance (cm) on top of the auto-framed distance. Negative = closer."));
static TAutoConsoleVariable<float> CVarLoadoutPreviewRight(TEXT("afl.Loadout.PreviewRight"), 40.f, TEXT("Preview cam right offset (3/4 angle)."));
static TAutoConsoleVariable<float> CVarLoadoutPreviewUp(TEXT("afl.Loadout.PreviewUp"), 0.f, TEXT("EXTRA camera height (cm) above the auto height (focus + 12)."));
static TAutoConsoleVariable<float> CVarLoadoutPreviewFocusUp(TEXT("afl.Loadout.PreviewFocusUp"), 0.f, TEXT("EXTRA look-at height (cm) on top of the auto focus (48% of body height)."));
static TAutoConsoleVariable<float> CVarLoadoutPreviewFOV(TEXT("afl.Loadout.PreviewFOV"), 45.f, TEXT("Preview cam horizontal FOV. Auto-distance compensates -- this sets perspective character."));
// Grounding: raises the HERO relative to the capsule (drops the pod under the pawn) so the feet clear the
// capsule's base geometry, with the glowing floor disc glued under the feet. THIS is the raise-the-robot
// knob (the old PlatformZ moved only the disc). Tunable live; a bigger value lifts the hero higher.
static TAutoConsoleVariable<float> CVarLoadoutPodGroundZ(TEXT("afl.Loadout.PodGroundZ"), 0.f, TEXT("Sink the pod (cm) below the MEASURED feet (toe-clip trim). Grounding is bounds-measured now; default 0."));
// Enlarge the CAPSULE (not the hero) about the grounded feet -> headroom above the head + side clearance,
// while the feet stay exactly on the floor disc. Live-tunable; re-check framing after (a bigger pod fills more).
static TAutoConsoleVariable<float> CVarLoadoutPodScale(TEXT("afl.Loadout.PodScale"), 1.2f, TEXT("Uniform capsule scale about the hero's feet -- bigger = more headroom + side buffer. Re-tune framing after."));

namespace
{
	/** The catalog Type to query for a loadout axis. Weapon AND WeaponSkin BOTH live under Type==Weapon (the
	 *  on-disk overload -- there is no WeaponSkin EAFLCosmeticType); they are split by namespace below. Beam
	 *  is its own Type. */
	EAFLCosmeticType QueryTypeForAxis(EAFLLoadoutAxis Axis)
	{
		switch (Axis)
		{
		case EAFLLoadoutAxis::Beam:       return EAFLCosmeticType::Beam;
		case EAFLLoadoutAxis::BodyColor:  return EAFLCosmeticType::Finish;         // BodyId resolves to a Finish (free base = 7 AFL.Finish.*)
		case EAFLLoadoutAxis::EdgeColor:  return EAFLCosmeticType::SkinColor_Edge;
		case EAFLLoadoutAxis::Facemask:   return EAFLCosmeticType::Facemask;
		case EAFLLoadoutAxis::Emblem:     return EAFLCosmeticType::Emblem;
		default:                          return EAFLCosmeticType::Weapon; // Weapon + WeaponSkin (Identity is dual-type, special-cased)
		}
	}

	/** EVERY CosmeticId namespace an axis owns.
	 *
	 *  A LIST, NOT A STRING, and that is the whole fix. An axis can legitimately span more than one
	 *  namespace: BodyColor owns BOTH "AFL.Finish." and "AFL.Body.", which the STORE has always known
	 *  (AFLW_FrontEndMarket's Tab_SKINS matches Finish + Body + Edge) and this filter did not. Ten
	 *  FINISH-typed rows in AFL.Body.* were therefore purchasable in the store and invisible in the
	 *  locker that equips them.
	 *
	 *  Third instance of this shape after the 27 facemasks and the AFL.Body duplicates. Returning a list
	 *  means the next two-namespace axis is expressible rather than silently truncated to its first.
	 *
	 *  "AFL.Weapon." still excludes "AFL.WeaponSkin." -- the char after "Weapon" is '.' vs 'S'. */
	TArray<FString> GetAxisIdPrefixes(EAFLLoadoutAxis Axis)
	{
		switch (Axis)
		{
		case EAFLLoadoutAxis::Weapon:      return { TEXT("AFL.Weapon.") };
		case EAFLLoadoutAxis::WeaponSkin:  return { TEXT("AFL.WeaponSkin.") };
		case EAFLLoadoutAxis::Beam:        return { TEXT("AFL.Beam.") };
		case EAFLLoadoutAxis::BodyColor:   return { TEXT("AFL.Finish."), TEXT("AFL.Body.") };
		case EAFLLoadoutAxis::EdgeColor:   return { TEXT("AFL.Edge.") };
		case EAFLLoadoutAxis::Facemask:    return { TEXT("AFL.Facemask.") };
		case EAFLLoadoutAxis::Emblem:      return { TEXT("AFL.Emblem.") };
		default:                           return {}; // Identity -> dual-type query, no namespace filter
		}
	}

	/** Does this id belong to the axis? Empty list = the axis does not filter by namespace (Identity). */
	bool AxisOwnsId(EAFLLoadoutAxis Axis, FName CosmeticId)
	{
		const TArray<FString> Prefixes = GetAxisIdPrefixes(Axis);
		if (Prefixes.Num() == 0)
		{
			return true;
		}
		const FString Id = CosmeticId.ToString();
		for (const FString& Prefix : Prefixes)
		{
			if (Id.StartsWith(Prefix, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	/** Joined for logs and for the coverage control's "does this axis filter at all" test. */
	FString GetAxisIdPrefix(EAFLLoadoutAxis Axis)
	{
		return FString::Join(GetAxisIdPrefixes(Axis), TEXT("|"));
	}

	/** CC-5: does this axis resolve to ONE id in FAFLCosmeticSelection, or to an ARRANGEMENT?
	 *
	 *  This is the property that decides whether an axis can be a tile rail at all, and it is exactly the
	 *  property GetEquippedIdForAxis's switch already encodes: every axis with a case there names a single
	 *  FName field; these two name StickerPlacements (nine zones) and AccessorySet (N hardpoints) instead.
	 *  So the two functions must agree, and AFLReportAxisCoverage below checks that they do rather than
	 *  trusting this list to be maintained by hand.
	 *
	 *  NOT an id test. The affordance tile carries NAME_None -- and so does an unset BodyColor, so an id
	 *  test would send a player who has equipped nothing into the creator. */
	bool IsArrangementAxis(EAFLLoadoutAxis Axis)
	{
		return Axis == EAFLLoadoutAxis::Sticker || Axis == EAFLLoadoutAxis::Accessory;
	}

	/** CC-5 CONTROL. Every axis must be EXACTLY ONE of: a namespace rail (GetAxisIdPrefix non-empty), an
	 *  arrangement (IsArrangementAxis), or Identity (dual-type, special-cased in GetOwnedEntriesForAxis).
	 *  An axis that is none of those falls through QueryTypeForAxis's default and fills its own rail with
	 *  WEAPONS -- which looks like a content mistake, not a code one, so nothing would report it. */
	void AFLReportAxisCoverage()
	{
		const UEnum* E = StaticEnum<EAFLLoadoutAxis>();
		if (!E) { UE_LOG(LogAFLCombat, Warning, TEXT("[AFLLoadout] AXIS COVERAGE: no UEnum -- NOT CHECKED.")); return; }
		const int32 Num = E->NumEnums() - 1; // trailing _MAX
		FString Bad;
		int32 Rails = 0, Arrangements = 0, Special = 0;
		for (int32 i = 0; i < Num; ++i)
		{
			const EAFLLoadoutAxis A = static_cast<EAFLLoadoutAxis>(E->GetValueByIndex(i));
			const int32 Kinds = (GetAxisIdPrefix(A).IsEmpty() ? 0 : 1)
				+ (IsArrangementAxis(A) ? 1 : 0)
				+ ((A == EAFLLoadoutAxis::Identity) ? 1 : 0);
			if (Kinds == 1)
			{
				if (IsArrangementAxis(A))                { ++Arrangements; }
				else if (A == EAFLLoadoutAxis::Identity) { ++Special; }
				else                                     { ++Rails; }
			}
			else
			{
				Bad += FString::Printf(TEXT(" %s(kinds=%d)"), *E->GetNameStringByIndex(i), Kinds);
			}
		}
		UE_LOG(LogAFLCombat, Display,
			TEXT("[AFLLoadout] AXIS COVERAGE: axes=%d rails=%d arrangements=%d special=%d unclassified=%s %s"),
			Num, Rails, Arrangements, Special, Bad.IsEmpty() ? TEXT("none") : *Bad,
			Bad.IsEmpty() ? TEXT("PASS") : TEXT("FAIL <- that axis will render WEAPONS in its own rail"));
	}

	/** Color axes render as tinted swatch chips (the cosmetic IS a color); the rest are name/thumbnail tiles. */
	bool IsColorAxis(EAFLLoadoutAxis Axis)
	{
		return Axis == EAFLLoadoutAxis::BodyColor || Axis == EAFLLoadoutAxis::EdgeColor || Axis == EAFLLoadoutAxis::Beam;
	}

	/** A representative FLinearColor for a color cosmetic. REGISTRY-AWARE: resolves the SAME way the PAWN does
	 *  (AFLCharacterPartActor -- RegistryTone ? *RegistryTone : baked), so the tile swatch CANNOT diverge from
	 *  what the equipped robot renders -- for every SKU, present and future. The asset's ColorParameters supply
	 *  the SHAPE (which key is axis-appropriate); the VALUE is the identity-registry tone when the asset's
	 *  ColorIdentityTag resolves, else the baked ColorParameters (the same fallback the pawn uses). Mid-gray on none. */
	FLinearColor ResolveAxisColor(const UObject* WorldContext, EAFLLoadoutAxis Axis, const UAFLSkinColorAsset* Asset)
	{
		const FLinearColor Fallback(0.3f, 0.3f, 0.3f, 1.f);
		if (!Asset)
		{
			return Fallback;
		}
		const TMap<FName, FLinearColor>& Colors = Asset->GetColors();
		TArray<FName> Keys;
		if (Axis == EAFLLoadoutAxis::Beam)
		{
			Keys = { FName(TEXT("BeamColor")), FName(TEXT("EmissiveColor")), FName(TEXT("TeamColor")) };
		}
		else if (Axis == EAFLLoadoutAxis::EdgeColor)
		{
			Keys = { FName(TEXT("EdgeGlowColor")), FName(TEXT("EmissiveColor")), FName(TEXT("TeamColor")) };
		}
		else // BodyColor (finish)
		{
			Keys = { FName(TEXT("TeamColor")), FName(TEXT("EmissiveColor")), FName(TEXT("EdgeGlowColor")) };
		}

		// REGISTRY-AWARE resolve (mirrors AFLCharacterPartActor's pawn apply): resolve the asset's
		// ColorIdentityTag ONCE, then prefer the registry tone per key (RegistryTone ? *RegistryTone : baked).
		// Beam is not a SkinFinish axis -- FindToneForParam has no "BeamColor" tone, so beams fall through to
		// the baked BeamColor unchanged; un-tagged / unresolved -> baked (byte-identical to the old behavior).
		FAFLColorIdentity Identity;
		const bool bIdentityResolved =
			Asset->GetColorIdentityTag().IsValid() &&
			UAFLCosmeticCatalogSubsystem::ResolveColorIdentity(WorldContext, Asset->GetColorIdentityTag(), Identity);

		for (const FName& K : Keys)
		{
			if (bIdentityResolved)
			{
				if (const FLinearColor* Tone = Identity.SkinFinish.FindToneForParam(K))
				{
					return *Tone;
				}
			}
			if (const FLinearColor* Found = Colors.Find(K))
			{
				return *Found;
			}
		}
		for (const TPair<FName, FLinearColor>& Pair : Colors)
		{
			return Pair.Value;
		}
		return Fallback;
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
	if (Axis == EAFLLoadoutAxis::Identity)
	{
		// Identity is Team OR Character (either/or) -> query BOTH types; the owned filter keeps only owned ones.
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
		++Scanned;

		// EAFLCosmeticType::Weapon is OVERLOADED on disk -- weapons AND weapon-skins both carry Type==Weapon.
		// Filter to the axis's OWN id-namespaces so the weapon picker shows only AFL.Weapon.* (not the
		// skins) -- and so BodyColor sees AFL.Body.* as well as AFL.Finish.*, which it never did.
		if (!AxisOwnsId(Axis, Entry->CosmeticId))
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
	case EAFLLoadoutAxis::Identity:    return Sel.GetActiveIdentityId();
	case EAFLLoadoutAxis::BodyColor:   return Sel.BodyId;
	case EAFLLoadoutAxis::EdgeColor:   return Sel.EdgeId;
	case EAFLLoadoutAxis::Facemask:    return Sel.FacemaskId;
	case EAFLLoadoutAxis::Emblem:      return Sel.EmblemId;
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
	case EAFLLoadoutAxis::Identity:
	{
		// Identity is either/or, resolved by the id's namespace (AFL.Character.* vs AFL.Team.*).
		//
		// ⚠ THE ELSE USED TO BE A CATCH-ALL, and it silently coerced every unrecognised namespace into
		// a Team. AFL.Chassis.Creator -- the shared Pro Mod blank base -- was stored as TeamId and the
		// equip reported success while the identity meant something nobody intended. An id whose
		// namespace is not understood must be REFUSED, not filed under whichever branch is last.
		const FString Id = CosmeticId.ToString();
		if (Id.StartsWith(TEXT("AFL.Character."), ESearchCase::IgnoreCase))
		{
			Sel.IdentityType = EAFLIdentityType::Character;
			Sel.CharacterId = CosmeticId;
		}
		else if (Id.StartsWith(TEXT("AFL.Team."), ESearchCase::IgnoreCase)
			  || Id.StartsWith(TEXT("AFL.Chassis."), ESearchCase::IgnoreCase))
		{
			// AFL.Chassis.* rides the Team slot deliberately: GetActiveIdentityId() reads TeamId for
			// this type and the part map keys off that id, so the blank base resolves through the
			// SAME path as every other identity rather than needing a fourth mechanism. Named here so
			// it is a decision on the record, not a coincidence of the fallthrough.
			Sel.IdentityType = EAFLIdentityType::Team;
			Sel.TeamId = CosmeticId;
		}
		else
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("[AFLLoadout] EquipForAxis(Identity) REFUSED '%s' -- unrecognised namespace. "
				     "Expected AFL.Character.*, AFL.Team.* or AFL.Chassis.*. Nothing equipped."),
				*Id);
			return;
		}
		break;
	}
	case EAFLLoadoutAxis::BodyColor:   Sel.BodyId = CosmeticId; break;
	case EAFLLoadoutAxis::EdgeColor:   Sel.EdgeId = CosmeticId; break;
	case EAFLLoadoutAxis::Facemask:    Sel.FacemaskId = CosmeticId; break;
	case EAFLLoadoutAxis::Emblem:      Sel.EmblemId = CosmeticId; break;
	default:                           return;
	}

	// ServerSetCosmeticSelection is BlueprintAuthorityOnly; dispatching from C++ sends the client->server RPC.
	Loadout->ServerSetCosmeticSelection(Sel);
}

void UAFLW_LoadoutBase::HandleSwapClicked()
{
	// I-27 SWAP SLOT: exchange the main and left weapon mounts, one click, through the same commit
	// seam as everything else. Only the Weapon axis has two slots; elsewhere the verb says why.
	if (ActiveAxis != EAFLLoadoutAxis::Weapon)
	{
		UE_LOG(LogAFLCombat, Display, TEXT("[AFLLoadout] SWAP applies to the weapon mounts -- axis %d has one slot."), (int32)ActiveAxis);
		return;
	}
	UAFLCosmeticLoadoutComponent* Loadout = GetLoadoutComponent();
	if (!Loadout)
	{
		return;
	}
	FAFLCosmeticSelection Sel = Loadout->GetSelection();
	if (Sel.WeaponId == NAME_None && Sel.LeftWeaponId == NAME_None)
	{
		UE_LOG(LogAFLCombat, Display, TEXT("[AFLLoadout] SWAP: no weapons mounted -- nothing to exchange."));
		return;
	}
	Swap(Sel.WeaponId, Sel.LeftWeaponId);
	if (Sel.GetActiveIdentityId() == NAME_None)
	{
		Sel.IdentityType = EAFLIdentityType::Team;
		Sel.TeamId = FName(TEXT("AFL.Team.IRONICS"));
	}
	Loadout->ServerSetCosmeticSelection(Sel);
	UE_LOG(LogAFLCombat, Display, TEXT("[AFLLoadout] SWAP: main <-> left (%s <-> %s)."),
		*Sel.WeaponId.ToString(), *Sel.LeftWeaponId.ToString());
}

void UAFLW_LoadoutBase::HandleDiscardClicked()
{
	// I-27 DISCARD: clear the ACTIVE axis with one click. Ownership is untouched -- the item goes
	// back to the owned grid. EquipForAxis refuses the Identity axis by namespace (a robot cannot
	// wear no identity), which is exactly the right refusal here too.
	UE_LOG(LogAFLCombat, Display, TEXT("[AFLLoadout] DISCARD: clearing axis %d."), (int32)ActiveAxis);
	EquipForAxis(ActiveAxis, NAME_None);
}

TOptional<FUIInputConfig> UAFLW_LoadoutBase::GetDesiredInputConfig() const
{
	// I-27: THE OVERLAY NEVER TAKES THE PAWN. ECommonInputMode::All keeps game input flowing (the
	// player stays free-moving in the world beside the overlay) while the cursor stays visible and
	// the panel stays clickable. Menu-mode was the full-screen locker's takeover semantics.
	return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::NoCapture);
}

void UAFLW_LoadoutBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (EquipButton)
	{
		EquipButton->OnClicked.AddDynamic(this, &UAFLW_LoadoutBase::HandleEquipButtonClicked);
	}
	if (SwapButton)
	{
		SwapButton->OnClicked.AddDynamic(this, &UAFLW_LoadoutBase::HandleSwapClicked);
	}
	if (DiscardButton)
	{
		DiscardButton->OnClicked.AddDynamic(this, &UAFLW_LoadoutBase::HandleDiscardClicked);
	}
	if (NewBuildButton)
	{
		NewBuildButton->OnClicked.AddDynamic(this, &UAFLW_LoadoutBase::HandleNewBuildClicked);
	}
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
	UpdatePreviewCaptureActivity(); // pooled re-activation resumes a paused capture
}

void UAFLW_LoadoutBase::NativeOnDeactivated()
{
	// C1: DO NOT tear the capture down here. Pushing the creator on the same layer deactivates this
	// loadout -- and the creator DISPLAYS this widget's PreviewRT (GetPreviewRenderTarget). Destroying
	// the capture on deactivate froze that RT into a still photograph: every drag recolored the pawn's
	// MIDs while the panel showed the dead lens's last frame (measured: CAPS total=0 with the creator
	// open, black panel, live SetColorOverride stream). The lens dies with the WIDGET, not the focus.
	// It does PAUSE here, though, unless a creator holds a borrow -- an unwatched every-frame scene
	// capture per stacked instance is pure GPU burn (operator-reported input lag).
	UpdatePreviewCaptureActivity();
	Super::NativeOnDeactivated();
}

void UAFLW_LoadoutBase::NativeDestruct()
{
	TeardownPreviewCapture();
	Super::NativeDestruct();
}

void UAFLW_LoadoutBase::UpdatePreviewCaptureActivity()
{
	const bool bRun = IsActivated() || PreviewBorrowCount > 0;
	if (ASceneCapture2D* Cap = PreviewCapture.Get())
	{
		if (USceneCaptureComponent2D* CapComp = Cap->GetCaptureComponent2D())
		{
			CapComp->bCaptureEveryFrame = bRun;
		}
	}
}

void UAFLW_LoadoutBase::AddPreviewBorrow()
{
	++PreviewBorrowCount;
	UpdatePreviewCaptureActivity();
}

void UAFLW_LoadoutBase::ReleasePreviewBorrow()
{
	PreviewBorrowCount = FMath::Max(0, PreviewBorrowCount - 1);
	UpdatePreviewCaptureActivity();
}

APawn* UAFLW_LoadoutBase::GetLocalPawn() const
{
	const APlayerController* PC = GetOwningPlayer();
	return PC ? PC->GetPawn() : nullptr;
}

APawn* UAFLW_LoadoutBase::GetPreviewPawn()
{
	if (DisplayPawn.IsValid())
	{
		return DisplayPawn.Get();
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// C1 PREVIEW SPINE (AFL-3210/3211/3214): the RIG owns the pawn; this widget only APPLIES to it.
	// Per-widget spawn/adopt/destroy is what produced four coexisting display pawns in one world --
	// the capture showed one while the drags painted another. Location preference (at the local
	// gameplay pawn in-match, origin in the front end) is passed through; the first acquirer decides.
	UAFLPreviewRigSubsystem* Rig = World->GetSubsystem<UAFLPreviewRigSubsystem>();
	if (!Rig)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AFLDisplayPawn] no preview rig subsystem -- no display pawn."));
		return nullptr;
	}
	FVector SpawnLoc = FVector::ZeroVector;
	if (const APawn* Local = GetLocalPawn())
	{
		SpawnLoc = Local->GetActorLocation();
	}
	AAFLLoadoutDisplayPawn* Spawned = Rig->AcquireDisplayPawn(
		DisplayPawnClass ? TSubclassOf<AAFLLoadoutDisplayPawn>(DisplayPawnClass.Get()) : TSubclassOf<AAFLLoadoutDisplayPawn>(),
		SpawnLoc);
	if (!Spawned)
	{
		return nullptr;
	}
	DisplayPawn = Spawned;

	// Apply the player's full selection now (identity body + colors/facemask/weapon/beam) via the proven fan-out.
	ApplySelectionToDisplayPawn();
	if (const UAFLCosmeticLoadoutComponent* Loadout = GetLoadoutComponent())
	{
		LastAppliedDisplaySelection = Loadout->GetSelection();
	}
	return Spawned;
}

// Field-wise selection equality (FAFLCosmeticSelection has no operator==) -> the NativeTick change-poll.
static bool AFLSelectionEquals(const FAFLCosmeticSelection& A, const FAFLCosmeticSelection& B)
{
	return A.IdentityType == B.IdentityType && A.TeamId == B.TeamId && A.CharacterId == B.CharacterId
		&& A.EdgeId == B.EdgeId && A.BodyId == B.BodyId && A.HelmetId == B.HelmetId
		&& A.WeaponId == B.WeaponId && A.WeaponSkinId == B.WeaponSkinId && A.BeamId == B.BeamId
		&& A.FacemaskId == B.FacemaskId;
}

void UAFLW_LoadoutBase::ApplySelectionToDisplayPawn()
{
	AAFLLoadoutDisplayPawn* Pawn = DisplayPawn.Get();
	if (!Pawn)
	{
		return;
	}

	// IDENTITY body: resolve the player's identity -> robot class (IRONICS fallback). Re-spawn ONLY on identity
	// change (SetRobotBody removes+adds -> don't thrash on color/weapon picks). The selector's ResolveBodyForPawn
	// targets the CONTROLLER's possessed pawn, so the display pawn resolves+adds here instead.
	FName IdentityId = NAME_None;
	if (const UAFLCosmeticLoadoutComponent* Loadout = GetLoadoutComponent())
	{
		IdentityId = Loadout->GetSelection().GetActiveIdentityId();
	}
	if (!bDisplayBodyApplied || IdentityId != LastAppliedBodyIdentity)
	{
		UClass* RobotCls = nullptr;
		const TCHAR* ResolveVia = TEXT("none");
		if (DisplayPartMap && IdentityId != NAME_None)
		{
			const TSoftClassPtr<AActor> Soft = DisplayPartMap->ResolveCharacterPart(IdentityId);
			if (!Soft.IsNull()) { RobotCls = Soft.LoadSynchronous(); ResolveVia = TEXT("partMap"); }
		}
		if (!RobotCls)
		{
			RobotCls = DisplayFallbackRobotClass.IsNull() ? nullptr : DisplayFallbackRobotClass.LoadSynchronous();
			if (RobotCls) { ResolveVia = TEXT("fallbackProp"); }
			if (!RobotCls)
			{
				RobotCls = LoadClass<AActor>(nullptr, TEXT("/Game/BagMan/Characters/Cosmetics/B_AFL_Robot_IRONICS.B_AFL_Robot_IRONICS_C"));
				if (RobotCls) { ResolveVia = TEXT("hardFallback"); }
			}
		}
		// The resolve outcome SAYS SO either way (AFL-3214): a null class here previously vanished
		// into a naked pawn with no trace.
		UE_LOG(LogTemp, Warning, TEXT("[AFLDisplayPawn] body resolve: identity=%s partMap=%s via=%s cls=%s"),
			IdentityId.IsNone() ? TEXT("<none>") : *IdentityId.ToString(),
			DisplayPartMap ? TEXT("set") : TEXT("NULL"), ResolveVia,
			RobotCls ? *RobotCls->GetName() : TEXT("NULL"));
		if (RobotCls)
		{
			Pawn->SetRobotBody(RobotCls);
			LastAppliedBodyIdentity = IdentityId;
			bDisplayBodyApplied = true;
		}
	}

	// COLOR / FACEMASK / WEAPON / BEAM: the proven fan-out at the display pawn. The controller's SkinCtrl resolves
	// the player's selection (PS-less pawn -> ctrl-PS fallback, verified) + pushes to the display pawn's comps; the
	// display pawn HasAuthority (non-replicated) so the BlueprintAuthorityOnly setters apply.
	const ALyraPlayerState* PS = GetLyraPlayerState();
	AController* Ctrl = PS ? PS->GetOwningController() : nullptr;
	UAFLSkinColorControllerComponent* SkinCtrl = Ctrl ? Ctrl->FindComponentByClass<UAFLSkinColorControllerComponent>() : nullptr;
	if (SkinCtrl)
	{
		SkinCtrl->RefreshFacemaskForPawn(Pawn); // slot-1 material swap (proven; before skin -- composition order)
		SkinCtrl->RefreshEmblemForPawn(Pawn);   // emblem MIC swap (EmblemId axis) -- was the one omitted axis
		SkinCtrl->RefreshSkinForPawn(Pawn);     // body finish (TeamColor) + edge emissive (proven; also re-tints the decal)
		// WEAPON / WEAPONSKIN / BEAM: ASC-SAFE on this pawn -- Lyra's FLyraEquipmentList::AddEntry guards the
		// ability grant with `if (ASC)` (LyraEquipmentManagerComponent.cpp:89) and GetAbilitySystemComponent
		// returns null for an ASC-less pawn, so the equip spawns the weapon MESH + SKIPS the grant (no fault).
		// Equip FIRST (the mesh must exist), THEN weapon-skin + beam recolor the equipped weapon.
		SkinCtrl->RefreshWeaponForPawn(Pawn);
		SkinCtrl->RefreshWeaponSkinForPawn(Pawn);
		SkinCtrl->RefreshBeamColorForPawn(Pawn);
	}

	// Instrumentation (always-on, temporary): confirms the poll fired + the fan-out ran on the DISPLAY pawn.
	// Fires on loadout-open + each selection change. Pair with `afl.SkinDiag 1` to see the resolved ids.
	UE_LOG(LogTemp, Warning, TEXT("[AFLDisplayPawn] apply -> pawn=%s identity=%s skinCtrl=%s"),
		*GetNameSafe(Pawn), IdentityId.IsNone() ? TEXT("<none>") : *IdentityId.ToString(),
		SkinCtrl ? TEXT("FOUND") : TEXT("NULL"));
}


// ─── CC-5.3 · CREATOR PREVIEW ────────────────────────────────────────────────────────────────────

void UAFLW_LoadoutBase::CreatorSetChannel(const EAFLCreatorChannel Channel, const FLinearColor Colour)
{
	// Seed from the COMMITTED selection the first time, so the creator opens showing what the player
	// already has rather than a blank. Seeded-ness is a flag, not an inference from the contents.
	if (!bCreatorWorkingSeeded)
	{
		if (const UAFLCosmeticLoadoutComponent* Loadout = GetLoadoutComponent())
		{
			CreatorWorking = Loadout->GetSelection();
		}
		bCreatorWorkingSeeded = true;
	}

	// Clamp HERE, with the shared gamut. The server clamps on commit; if the preview did not, the
	// player would pick a colour, see it, and be handed a different one on save.
	const FLinearColor Clamped = AFLCreatorGamut::ClampToNeon(Colour);
	CreatorWorking.bUseCreatorColors = 1;

	auto Assign = [&](const EAFLCreatorChannel Ch)
	{
		switch (Ch)
		{
			case EAFLCreatorChannel::Body:  CreatorWorking.CreatorBodyColor  = Clamped; break;
			case EAFLCreatorChannel::Edge:  CreatorWorking.CreatorEdgeColor  = Clamped; break;
			case EAFLCreatorChannel::Glow:  CreatorWorking.CreatorGlowColor  = Clamped; break;
			case EAFLCreatorChannel::Visor:
				CreatorWorking.CreatorVisorColor = Clamped;
				CreatorWorking.bVisorColorSet    = 1; // an EXPLICIT choice; stops the body mirror
				break;
			case EAFLCreatorChannel::Emblem:
				CreatorWorking.CreatorEmblemColor = Clamped;
				CreatorWorking.bEmblemColorSet    = 1; // explicit; unset = registry tone keeps the decal
				break;
		}
	};

	Assign(Channel);
	// Linked channels follow. Default is UNLINKED (CC-X24), so this is a no-op until a pairing is ruled.
	if (CreatorLinks.IsLinked(Channel))
	{
		for (const EAFLCreatorChannel Other : { EAFLCreatorChannel::Body, EAFLCreatorChannel::Edge,
		                                        EAFLCreatorChannel::Glow, EAFLCreatorChannel::Visor,
		                                        EAFLCreatorChannel::Emblem })
		{
			if (Other != Channel && CreatorLinks.IsLinked(Other)) { Assign(Other); }
		}
	}
}

void UAFLW_LoadoutBase::CreatorApplyPreview()
{
	APawn* Pawn = GetPreviewPawn();
	APlayerController* PC = GetOwningPlayer();
	UAFLSkinColorControllerComponent* SkinCtrl =
		PC ? PC->FindComponentByClass<UAFLSkinColorControllerComponent>() : nullptr;
	if (!Pawn || !SkinCtrl)
	{
		// SAYS SO (SILENT-ZERO family): a preview that silently applies to nothing reads as
		// "the arc is broken" when the missing piece is the pawn or the controller component.
		UE_LOG(LogTemp, Warning, TEXT("[Creator] ApplyPreview bail: pawn=%s skinCtrl=%s"),
			Pawn ? TEXT("ok") : TEXT("NULL"), SkinCtrl ? TEXT("ok") : TEXT("NULL"));
		return;
	}
	if (!bCreatorWorkingSeeded)
	{
		if (const UAFLCosmeticLoadoutComponent* Loadout = GetLoadoutComponent())
		{
			CreatorWorking = Loadout->GetSelection();
		}
		bCreatorWorkingSeeded = true;
	}
	// The shipping path, deliberately: preview selection -> GetEffectiveSelection -> RefreshSkinForPawn
	// -> BuildColorOverride -> SetColorOverride. Identical to what the gameplay pawn receives.
	SkinCtrl->SetPreviewSelection(CreatorWorking);
	ApplySelectionToDisplayPawn();
}

void UAFLW_LoadoutBase::CreatorSetPart(const EAFLLoadoutAxis Axis, const FName CosmeticId)
{
	// Same seed rule as CreatorSetChannel: the working selection opens showing what the player has.
	if (!bCreatorWorkingSeeded)
	{
		if (const UAFLCosmeticLoadoutComponent* Loadout = GetLoadoutComponent())
		{
			CreatorWorking = Loadout->GetSelection();
		}
		bCreatorWorkingSeeded = true;
	}
	switch (Axis)
	{
		case EAFLLoadoutAxis::Facemask:  CreatorWorking.FacemaskId = CosmeticId; break;
		case EAFLLoadoutAxis::Emblem:    CreatorWorking.EmblemId   = CosmeticId; break;
		case EAFLLoadoutAxis::BodyColor: CreatorWorking.BodyId     = CosmeticId; break;
		default:
			UE_LOG(LogAFLCombat, Warning,
				TEXT("[Creator] CreatorSetPart: axis %d is not a part axis -- refused."), (int32)Axis);
			return;
	}
	CreatorApplyPreview();
}

bool UAFLW_LoadoutBase::CreatorToggleCombatRange()
{
	bPreviewCombatRange = !bPreviewCombatRange;
	RepositionPreviewCamera(); // apply NOW -- the 0.25s timer would land it anyway, but a toggle
	                           // that answers on the next tick reads as lag.
	return bPreviewCombatRange;
}

void UAFLW_LoadoutBase::CreatorSyncIdentityFromCommitted()
{
	if (!bCreatorWorkingSeeded)
	{
		return; // next CreatorApplyPreview seeds from the committed selection anyway -- already in sync
	}
	if (const UAFLCosmeticLoadoutComponent* Loadout = GetLoadoutComponent())
	{
		const FAFLCosmeticSelection& Committed = Loadout->GetSelection();
		CreatorWorking.IdentityType = Committed.IdentityType;
		CreatorWorking.TeamId       = Committed.TeamId;
		CreatorWorking.CharacterId  = Committed.CharacterId;
	}
}

void UAFLW_LoadoutBase::CreatorRotatePreview(const float DeltaYawDegrees)
{
	AAFLLoadoutDisplayPawn* Pawn = DisplayPawn.Get();
	if (!Pawn || !Pawn->GetMesh())
	{
		return;
	}
	// MESH, not actor -- see the header note: the capture is attached to the actor.
	Pawn->GetMesh()->AddRelativeRotation(FRotator(0.f, DeltaYawDegrees, 0.f));
}

float UAFLW_LoadoutBase::CreatorGetPreviewYaw() const
{
	const AAFLLoadoutDisplayPawn* Pawn = DisplayPawn.Get();
	return (Pawn && Pawn->GetMesh()) ? Pawn->GetMesh()->GetRelativeRotation().Yaw : 0.f;
}

FAFLCreatorChannelSchema UAFLW_LoadoutBase::CreatorGetSchema() const
{
	if (const UAFLCosmeticLoadoutComponent* Loadout = GetLoadoutComponent())
	{
		return Loadout->GetChannelSchemaForPawn(DisplayPawn.Get());
	}
	return FAFLCreatorChannelSchema();
}

void UAFLW_LoadoutBase::SetupPreviewCapture()
{
	APawn* Pawn = GetPreviewPawn(); // the ASC-less display pawn (NOT the gameplay pawn) -> works with no live pawn
	UWorld* World = GetWorld();
	if (!Pawn || !World)
	{
		return; // display pawn couldn't spawn (no world) -> no preview; the locker still works.
	}

	// Runtime render target (transient; sized from PreviewResolution). Created once, reused across opens.
	if (!PreviewRT)
	{
		PreviewRT = NewObject<UTextureRenderTarget2D>(this);
		PreviewRT->ClearColor = FLinearColor(0.006f, 0.009f, 0.016f, 1.f); // #05080F dark-theater backdrop
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
		// ISOLATE the robot onto the clean ClearColor backdrop (not the arena) via the ShowOnlyList -- refreshed
		// per-tick (RefreshPreviewShowList) so the equipped weapon (a separate attached actor that changes on
		// pick) stays in the shot.
		CapComp->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		// DARK THEATER: kill the bright sky/atmosphere/fog bleed so the backdrop is the near-black ClearColor,
		// not the washed-out arena -- robot + neon read against dark (Image-2 concept).
		CapComp->ShowFlags.SetAtmosphere(false);
		CapComp->ShowFlags.SetFog(false);
		CapComp->ShowFlags.SetVolumetricFog(false);
		CapComp->ShowFlags.SetCloud(false);
	}

	// Stage the reusable kiosk-pod diorama AROUND the previewed hero: spawn it client-side + ATTACH it to the
	// pawn so RefreshPreviewShowList's GetAttachedActors auto-includes it in the isolated capture (the hero
	// renders INSIDE the pod). Align the pod's base (PawnAnchor = pod-local origin) to the pawn's feet.
	if (!PreviewPod.IsValid())
	{
		// C1: the rig owns THE pod too -- two widgets each spawning one around the shared pawn put
		// two overlapping dioramas into every capture.
		if (UAFLPreviewRigSubsystem* Rig = World->GetSubsystem<UAFLPreviewRigSubsystem>())
		{
			PreviewPod = Rig->AcquirePod(
				PodClass ? TSubclassOf<AAFLLoadoutPod>(PodClass.Get()) : TSubclassOf<AAFLLoadoutPod>(),
				Cast<AAFLLoadoutDisplayPawn>(Pawn));
		}
	}
	if (AAFLLoadoutPod* Pod = PreviewPod.Get())
	{
		Pod->AttachToActor(Pawn, FAttachmentTransformRules::KeepRelativeTransform);
		PreviewFeetDrop = 90.f; // fallback pawn half-height
		if (const ACharacter* Char = Cast<ACharacter>(Pawn))
		{
			if (const UCapsuleComponent* Capsule = Char->GetCapsuleComponent())
			{
				PreviewFeetDrop = Capsule->GetScaledCapsuleHalfHeight();
			}
		}
		Pod->SetActorRelativeRotation(FRotator::ZeroRotator);
		RepositionPreviewPod();
	}

	RefreshPreviewShowList();

	// C1: keep the LENS honest while this widget is DEACTIVATED (collapsed widgets stop ticking,
	// but the capture now lives on under the pushed creator). Show-list: a chassis swap replaces the
	// part ACTOR. Camera + pod: both derive from the MEASURED body bounds, and the last tick before
	// the creator covered this widget caught the robot MID-DRESS (bounds max still low) -- the
	// camera froze at knee height forever (measured: settled bounds relMax=178 while the frame
	// showed a ~65cm-body framing). World timers ignore widget visibility; 4 Hz re-derives all three.
	if (!ShowListRefreshTimer.IsValid())
	{
		World->GetTimerManager().SetTimer(ShowListRefreshTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				RefreshPreviewShowList();
				RepositionPreviewCamera();
				RepositionPreviewPod();
			}),
			0.25f, /*bLoop*/ true);
	}

	// Route the render target into the center-stage image.
	if (PreviewImage && PreviewRT)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(PreviewRT);
		Brush.ImageSize = FVector2D(PreviewResolution.X, PreviewResolution.Y);
		PreviewImage->SetBrush(Brush);
	}

	// A Setup on a widget that is not on screen (cheat-built, never pushed) must not arm an
	// every-frame capture nobody watches.
	UpdatePreviewCaptureActivity();
}

void UAFLW_LoadoutBase::TeardownPreviewCapture()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShowListRefreshTimer);
	}
	if (ASceneCapture2D* Cap = PreviewCapture.Get())
	{
		Cap->Destroy();
	}
	PreviewCapture = nullptr;

	// C1: the pawn and the pod belong to the RIG, not this widget -- destroying them here is how one
	// widget's teardown orphaned every other widget's preview (measured: the creator repainted a
	// respawned pawn while the capture still showed the dead one's replacement). Drop the caches only.
	PreviewPod = nullptr;
	DisplayPawn = nullptr;
}

void UAFLW_LoadoutBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (PreviewCapture.IsValid())
	{
		RefreshPreviewShowList();  // keep the equipped-weapon actor in the isolated show-list as picks change
		RepositionPreviewCamera(); // live-tunable framing via the afl.Loadout.Preview* cvars

		// Live-sync the display pawn to the player's CURRENT selection. An equip lands via OnRep (async), so a
		// per-frame delta-poll is more robust than a post-equip call; the fan-out re-runs only on a real change.
		if (DisplayPawn.IsValid())
		{
			if (const UAFLCosmeticLoadoutComponent* Loadout = GetLoadoutComponent())
			{
				const FAFLCosmeticSelection& Cur = Loadout->GetSelection();
				if (!AFLSelectionEquals(Cur, LastAppliedDisplaySelection))
				{
					ApplySelectionToDisplayPawn();
					LastAppliedDisplaySelection = Cur;
				}
			}
		}
	}
	RepositionPreviewPod(); // live grounding: raise the hero relative to the capsule + glue the disc under the feet
}

bool UAFLW_LoadoutBase::MeasureWornBodyZ(float& OutMinZ, float& OutMaxZ) const
{
	// The worn robot's rendered vertical extent, in WORLD Z -- the ground truth both the pod
	// grounding and the camera auto-framing key off (chassis- and pose-agnostic by construction).
	const AAFLLoadoutDisplayPawn* Pawn = DisplayPawn.Get();
	if (!Pawn)
	{
		return false;
	}
	TArray<AAFLCharacterPartActor*> WornParts;
	AAFLCharacterPartActor::CollectPartsOn(Pawn, WornParts);
	bool bMeasured = false;
	OutMinZ = TNumericLimits<float>::Max();
	OutMaxZ = TNumericLimits<float>::Lowest();
	for (const AAFLCharacterPartActor* Part : WornParts)
	{
		if (!Part) { continue; }
		TArray<USkeletalMeshComponent*> Meshes;
		Part->GetComponents<USkeletalMeshComponent>(Meshes);
		for (const USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh && Mesh->GetSkeletalMeshAsset())
			{
				const FBox Box = Mesh->Bounds.GetBox();
				OutMinZ = FMath::Min(OutMinZ, static_cast<float>(Box.Min.Z));
				OutMaxZ = FMath::Max(OutMaxZ, static_cast<float>(Box.Max.Z));
				bMeasured = true;
			}
		}
	}
	return bMeasured;
}

void UAFLW_LoadoutBase::RepositionPreviewPod()
{
	AAFLLoadoutPod* Pod = PreviewPod.Get();
	AAFLLoadoutDisplayPawn* Pawn = DisplayPawn.Get();
	if (!Pod || !Pawn)
	{
		return;
	}
	const float GroundZ = CVarLoadoutPodGroundZ.GetValueOnGameThread();
	const float Scale = FMath::Max(0.1f, CVarLoadoutPodScale.GetValueOnGameThread());

	// MEASURED FEET, not the capsule (operator-reported float, RT-confirmed): the robot renders as an
	// ATTACHED PART ACTOR whose mesh puts the feet at the PAWN ORIGIN -- capsule centre -- while the
	// old formula placed the pod floor a capsule-half-height lower, so the hero hovered ~90cm above
	// the disc. Glue the pod's floor (pod-local Z 0 = the branded base-top, by design) to the worn
	// body's actual lowest rendered point.
	float MinZ = 0.f, MaxZ = 0.f;
	const float FeetWorldZ = MeasureWornBodyZ(MinZ, MaxZ) ? MinZ : Pawn->GetActorLocation().Z;

	// Scale the whole pod ABOUT the grounded feet; positive GroundZ sinks the pod slightly below the
	// measured feet (toe-clip trim), default 0.
	Pod->SetActorScale3D(FVector(Scale));
	Pod->SetActorRelativeLocation(FVector(0.f, 0.f,
		(FeetWorldZ - Pawn->GetActorLocation().Z) - GroundZ * Scale));
}

void UAFLW_LoadoutBase::RefreshPreviewShowList()
{
	ASceneCapture2D* Cap = PreviewCapture.Get();
	APawn* Pawn = DisplayPawn.Get(); // isolate the DISPLAY pawn (+ its robot part + weapon + pod) in the capture
	if (!Cap || !Pawn)
	{
		return;
	}
	USceneCaptureComponent2D* CapComp = Cap->GetCaptureComponent2D();
	if (!CapComp)
	{
		return;
	}
	// Show ONLY the pawn + everything attached to it (the equipped weapon + any accessories, resolved
	// recursively) -> the robot renders isolated on the clean backdrop.
	TArray<AActor*> Attached;
	Pawn->GetAttachedActors(Attached, /*bResetArray*/ true, /*bRecursivelyIncludeAttachedActors*/ true);
	CapComp->ShowOnlyActors.Reset();
	CapComp->ShowOnlyActors.Add(Pawn);
	for (AActor* Actor : Attached)
	{
		if (Actor)
		{
			CapComp->ShowOnlyActors.Add(Actor);
		}
	}
}

void UAFLW_LoadoutBase::RepositionPreviewCamera()
{
	ASceneCapture2D* Cap = PreviewCapture.Get();
	AAFLLoadoutDisplayPawn* Pawn = DisplayPawn.Get();
	if (!Cap)
	{
		return;
	}
	// AUTO-FRAME from the measured worn-body bounds (2026-08-28, with the measured-feet grounding):
	// the old hand-tuned offsets (focus at shin height, FOV 82) were compensating for the floating
	// composition -- once the feet grounded, the head cropped and the pedestal filled the frame.
	// Distance is derived from the body height and the VERTICAL field of view (portrait RT), so any
	// chassis, any pose, frames itself; the cvars are TRIMS on top of the computed frame.
	float MinZ = 0.f, MaxZ = 0.f;
	const bool bMeasured = MeasureWornBodyZ(MinZ, MaxZ);
	const float PawnZ = Pawn ? Pawn->GetActorLocation().Z : 0.f;
	const float BottomRel = bMeasured ? (MinZ - PawnZ) : 0.f;
	const float BodyH = bMeasured ? FMath::Max(60.f, MaxZ - MinZ) : 180.f;
	// Slight low bias (48%) keeps a little air above the head and the base rim in shot.
	const float FocusZ = BottomRel + BodyH * 0.48f + CVarLoadoutPreviewFocusUp.GetValueOnGameThread();

	const float FOV = CVarLoadoutPreviewFOV.GetValueOnGameThread();
	const float Aspect = (PreviewResolution.X > 0)
		? static_cast<float>(PreviewResolution.Y) / static_cast<float>(PreviewResolution.X) : 1.5f;
	// FOVAngle is HORIZONTAL; the portrait RT's vertical half-tangent is larger by the aspect.
	const float VTan = FMath::Max(0.1f, FMath::Tan(FMath::DegreesToRadians(FOV * 0.5f)) * Aspect);
	// Fill ~82% of the frame height (portrait). COMBAT RANGE (I-6) multiplies the distance so the
	// build is judged the way opponents read it. The old 250cm ceiling was the pod CHAMBER wall --
	// the stage is open now (decapsulated); 600 stays inside the ~9m backdrop dome.
	const float RangeMul = bPreviewCombatRange ? 2.2f : 1.f;
	const float Dist = FMath::Clamp(
		(BodyH * 0.5f * 1.22f) / VTan * RangeMul + CVarLoadoutPreviewFwd.GetValueOnGameThread(), 110.f, 600.f);

	// The camera stays on +X BY DESIGN (the pod's backdrop/dome/FX all live at X<0, "strictly
	// behind the hero"); the ROBOT'S facing is the mesh yaw (ApplyDrivingMesh initial + the
	// CreatorRotatePreview spin), never the camera's azimuth.
	const FVector Off(Dist,
	                  CVarLoadoutPreviewRight.GetValueOnGameThread(),
	                  FocusZ + 12.f + CVarLoadoutPreviewUp.GetValueOnGameThread()); // slight high look-down
	const FVector Focus(0.f, 0.f, FocusZ);
	Cap->SetActorRelativeLocation(Off);
	Cap->SetActorRelativeRotation((Focus - Off).Rotation());
	if (USceneCaptureComponent2D* CapComp = Cap->GetCaptureComponent2D())
	{
		CapComp->FOVAngle = FOV;
	}
}

#if !UE_BUILD_SHIPPING
// CC-5: out of the header so the Cast has a COMPLETE type -- see the declaration's note.
APawn* UAFLW_LoadoutBase::GetPreviewPawnForTest() const
{
	return Cast<APawn>(DisplayPawn.Get());
}
#endif

void UAFLW_LoadoutBase::RebuildTiles()
{
	// CC-5: once per run, and it always prints -- silence must not be readable as a pass.
	static bool bReportedCoverage = false;
	if (!bReportedCoverage) { bReportedCoverage = true; AFLReportAxisCoverage(); }

	// Rebuild every axis grid from the current owned-set + selection. All non-weapon containers are optional
	// (BindWidgetOptional) -> a null container is skipped inside RebuildAxisTiles.
	RebuildAxisTiles(EAFLLoadoutAxis::Weapon,     TileContainer);
	RebuildAxisTiles(EAFLLoadoutAxis::WeaponSkin, SkinTileContainer);
	RebuildAxisTiles(EAFLLoadoutAxis::Beam,       BeamTileContainer);
	RebuildAxisTiles(EAFLLoadoutAxis::Identity,   IdentityTileContainer);
	RebuildAxisTiles(EAFLLoadoutAxis::BodyColor,  BodyColorTileContainer);
	RebuildAxisTiles(EAFLLoadoutAxis::EdgeColor,  EdgeColorTileContainer);
	RebuildAxisTiles(EAFLLoadoutAxis::Facemask,   FacemaskTileContainer);
	RebuildAxisTiles(EAFLLoadoutAxis::Emblem,     EmblemTileContainer);
	// CC-5: same call, same function -- the arrangement branch is INSIDE RebuildAxisTiles so there is one
	// rail-building path, not two that can drift.
	RebuildAxisTiles(EAFLLoadoutAxis::Sticker,    StickerTileContainer);
	RebuildAxisTiles(EAFLLoadoutAxis::Accessory,  AccessoryTileContainer);

	// CC-5 step 3: the design-system regions. Every one is optional, so a WBP binding none of them
	// behaves exactly as before -- which is what keeps the in-match locker out of this pass.
	RebuildAxisTabs();
	RebuildRail();
	RefreshDetail();
	// ITEM 6: builds are rebuilt HERE and not in SetActiveAxis -- switching axis does not change what a
	// player has saved, and rebuilding them on every tab click would discard scroll and selection state.
	RebuildBuilds();
}

// ===== CC-5 step 3: ACTIVE-AXIS MODEL ==========================================================

void UAFLW_LoadoutBase::SetActiveAxis(EAFLLoadoutAxis Axis)
{
	if (IsArrangementAxis(Axis))
	{
		// A nine-zone arrangement is not something a one-id rail can show. The tab is a DOOR here, and
		// ActiveAxis deliberately does not move -- backing out of the creator must reveal the axis the
		// player was actually on, not the door they walked through.
		UE_LOG(LogAFLCombat, Log, TEXT("[AFLLoadout] axis tab %d is an arrangement -> creator SHORTCUT"), (int32)Axis);
		OpenCreatorOnAxis(Axis);
		return;
	}

	ActiveAxis = Axis;
	// Seed the highlight from what is EQUIPPED on the newly-shown axis, so the detail panel opens on the
	// player's current choice rather than blank. Browsing then moves the highlight, never the equip.
	SelectedId = GetEquippedIdForAxis(Axis);
	UE_LOG(LogAFLCombat, Log, TEXT("[AFLLoadout] active axis -> %d (seeded selection %s)"),
		(int32)Axis, *SelectedId.ToString());

	RebuildAxisTabs();
	RebuildRail();
	RefreshDetail();
}

void UAFLW_LoadoutBase::SelectItem(FName CosmeticId)
{
	SelectedId = CosmeticId;
	// The rail is rebuilt so the highlight moves. Nothing is equipped and no RPC is sent.
	RebuildRail();
	RefreshDetail();
}

void UAFLW_LoadoutBase::CommitEquip()
{
	// FAIL LOUDLY, NOT SILENTLY. A commit button that does nothing without saying why is
	// indistinguishable from one that is not wired -- which is exactly how the creator's missing close
	// control survived long enough to trap a player.
	if (SelectedId.IsNone())
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("[AFLLoadout] CommitEquip REFUSED -- nothing selected on axis %d."),
			(int32)ActiveAxis);
		return;
	}
	if (SelectedId == GetEquippedIdForAxis(ActiveAxis))
	{
		UE_LOG(LogAFLCombat, Log, TEXT("[AFLLoadout] CommitEquip skipped -- %s is already equipped."),
			*SelectedId.ToString());
		return;
	}
	UE_LOG(LogAFLCombat, Log, TEXT("[AFLLoadout] CommitEquip axis=%d id=%s"),
		(int32)ActiveAxis, *SelectedId.ToString());
	EquipForAxis(ActiveAxis, SelectedId);
	RebuildTiles();
}

void UAFLW_LoadoutBase::RebuildAxisTabs()
{
	if (!AxisTabContainer) { return; }
	AxisTabContainer->ClearChildren();

	UClass* SpawnClass = AxisTabClass ? AxisTabClass.Get() : TileClass.Get();
	if (!SpawnClass) { return; }

	// EVERY axis the enum declares, in declaration order -- NOT a hand-written list a later appended
	// axis would silently miss. The enum IS the inventory of axes.
	const UEnum* E = StaticEnum<EAFLLoadoutAxis>();
	if (!E) { return; }
	const int32 Num = E->NumEnums() - 1; // trailing _MAX
	for (int32 i = 0; i < Num; ++i)
	{
		const EAFLLoadoutAxis Axis = static_cast<EAFLLoadoutAxis>(E->GetValueByIndex(i));

		// OWNED-ONLY: an axis with nothing to show does not get a tab. An empty tab promises a category
		// the game cannot fill -- the defect the store's phantom web categories have, and the one
		// WeaponSkin left behind when its content retired and its tab did not.
		if (!ShouldAxisAppear(Axis)) { continue; }

		UAFLW_LoadoutTileBase* Tab = CreateWidget<UAFLW_LoadoutTileBase>(this, SpawnClass);
		if (!Tab) { continue; }
		// The axis names itself. Typing the labels here would be a second source that drifts.
		Tab->SetTileData(Axis, NAME_None, E->GetDisplayNameTextByValue(static_cast<int64>(Axis)),
			/*bEquipped=*/(Axis == ActiveAxis), /*bIsSwatch=*/false, FLinearColor::White, nullptr);
		// THE DISCRIMINATION: bound to the TAB handler because the TAB region spawned it.
		Tab->OnTileClicked.AddDynamic(this, &UAFLW_LoadoutBase::HandleAxisTabClicked);
		// A card is not a tab. Ten cards overflowed 1280px; ten labels do not.
		Tab->ApplyTabStyle(Axis == ActiveAxis);
		AxisTabContainer->AddChild(Tab);
	}
}

void UAFLW_LoadoutBase::RebuildRail()
{
	if (!RailContainer) { return; }
	RailContainer->ClearChildren();

	UClass* SpawnClass = TileClass.Get();
	if (!SpawnClass) { return; }

	TArray<FAFLCatalogEntry> Owned;
	GetOwnedEntriesForAxis(ActiveAxis, Owned);
	const FName EquippedId = GetEquippedIdForAxis(ActiveAxis);
	const bool bColorAxis = IsColorAxis(ActiveAxis);
	const UAFLCosmeticCatalogSubsystem* Catalog = bColorAxis ? GetCatalog() : nullptr;

	for (const FAFLCatalogEntry& Entry : Owned)
	{
		UAFLW_LoadoutTileBase* Row = CreateWidget<UAFLW_LoadoutTileBase>(this, SpawnClass);
		if (!Row) { continue; }

		FText Label = Entry.DisplayName;
		if (Label.IsEmpty())
		{
			const FString IdStr = Entry.CosmeticId.ToString();
			FString Left, Right;
			Label = FText::FromString(IdStr.Split(TEXT("."), &Left, &Right, ESearchCase::IgnoreCase,
				ESearchDir::FromEnd) ? Right : IdStr);
		}

		bool bIsSwatch = false;
		FLinearColor SwatchColor = FLinearColor::White;
		if (bColorAxis && Catalog)
		{
			bIsSwatch = true;
			SwatchColor = ResolveAxisColor(this, ActiveAxis,
				Cast<UAFLSkinColorAsset>(Catalog->ResolveAsset(Entry.CosmeticId)));
		}
		// HIGHLIGHT TRACKS SELECTION, NOT EQUIPMENT. The equipped item is named in the detail panel and
		// on the commit button; using one visual for both makes "what am I about to equip" unreadable at
		// exactly the moment it matters.
		Row->SetTileData(ActiveAxis, Entry.CosmeticId, Label, Entry.CosmeticId == SelectedId,
			bIsSwatch, SwatchColor, Entry.ShopThumbnail);
		Row->OnTileClicked.AddDynamic(this, &UAFLW_LoadoutBase::HandleRailItemClicked);
		RailContainer->AddChild(Row);
	}

	UE_LOG(LogAFLCombat, Log, TEXT("[AFLLoadout] rail axis=%d rows=%d equipped=%s selected=%s"),
		(int32)ActiveAxis, Owned.Num(), *EquippedId.ToString(), *SelectedId.ToString());
}

void UAFLW_LoadoutBase::RefreshDetail()
{
	const FName EquippedId = GetEquippedIdForAxis(ActiveAxis);
	const bool bHasSelection = !SelectedId.IsNone();
	const bool bAlready = bHasSelection && (SelectedId == EquippedId);

	if (DetailNameText)
	{
		FText Name = NSLOCTEXT("AFLLoadout", "NothingSelected", "Nothing selected");
		if (bHasSelection)
		{
			// Prefer the marketing name; the id is the fallback, never the other way round.
			Name = FText::FromName(SelectedId);
			TArray<FAFLCatalogEntry> Owned;
			GetOwnedEntriesForAxis(ActiveAxis, Owned);
			for (const FAFLCatalogEntry& Row : Owned)
			{
				if (Row.CosmeticId == SelectedId && !Row.DisplayName.IsEmpty())
				{
					Name = Row.DisplayName;
					break;
				}
			}
		}
		DetailNameText->SetText(Name);
	}

	if (DetailMetaText)
	{
		const UEnum* E = StaticEnum<EAFLLoadoutAxis>();
		const FText AxisName = E ? E->GetDisplayNameTextByValue(static_cast<int64>(ActiveAxis)) : FText::GetEmpty();
		const FText State = bAlready
			? NSLOCTEXT("AFLLoadout", "MetaEquipped", "equipped")
			: (bHasSelection ? NSLOCTEXT("AFLLoadout", "MetaOwned", "owned")
							 : NSLOCTEXT("AFLLoadout", "MetaNone", "--"));
		DetailMetaText->SetText(FText::Format(
			NSLOCTEXT("AFLLoadout", "DetailMeta", "{0}  |  {1}"), AxisName, State));
	}

	if (EquipLabelText)
	{
		EquipLabelText->SetText(bAlready ? NSLOCTEXT("AFLLoadout", "BtnEquipped", "EQUIPPED")
										 : NSLOCTEXT("AFLLoadout", "BtnEquip", "EQUIP"));
	}
	if (EquipButton)
	{
		// Disabled is a STATE, not a hidden control -- the player must still see where the commit lives.
		EquipButton->SetIsEnabled(bHasSelection && !bAlready);
	}
}


bool UAFLW_LoadoutBase::ShouldAxisAppear(EAFLLoadoutAxis Axis) const
{
	// AN ARRANGEMENT AXIS IS A DOOR, NOT A LIST. It appears when the CATALOG has rows for it -- the
	// player does not need to own a sticker to open the placement surface. Accessory has zero catalog
	// rows and so has no door to offer.
	if (IsArrangementAxis(Axis))
	{
		const UAFLCosmeticCatalogSubsystem* Catalog = GetCatalog();
		if (!Catalog) { return false; }
		TArray<const FAFLCatalogEntry*> All;
		Catalog->GetEntriesByType(QueryTypeForAxis(Axis), All);
		for (const FAFLCatalogEntry* E : All)
		{
			if (E && AxisOwnsId(Axis, E->CosmeticId)) { return true; }
		}
		return false;
	}

	// A SELECTION AXIS NEEDS OWNED ROWS. Nothing unowned appears in the loadout at all, so an axis with
	// nothing owned has nothing to show -- and that is honest rather than broken: the player owns
	// nothing on it. The store is where unowned things live.
	TArray<FAFLCatalogEntry> Owned;
	GetOwnedEntriesForAxis(Axis, Owned);
	return Owned.Num() > 0;
}

void UAFLW_LoadoutBase::RebuildBuilds()
{
	const UAFLCosmeticLoadoutComponent* LC = GetLoadoutComponent();
	const int32 BuildCount = LC ? LC->GetBuildSet().Builds.Num() : 0;

	// THE EMPTY STATE IS THE COMMON FIRST EXPERIENCE, not an edge case: a new player owns the six free
	// identities and nothing else. It says what is true and where to go rather than rendering an empty
	// grid that reads as a failure.
	if (EmptyStateText)
	{
		const bool bNothingYet = (BuildCount == 0);
		EmptyStateText->SetVisibility(bNothingYet ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bNothingYet)
		{
			EmptyStateText->SetText(NSLOCTEXT("AFLLoadout", "NoBuildsYet",
				"No builds yet. Start one in the Creator, or equip an identity below."));
		}
	}

	if (!BuildsContainer) { return; }
	BuildsContainer->ClearChildren();
	UClass* SpawnClass = TileClass.Get();
	if (!SpawnClass || !LC) { return; }

	const FAFLCreatorBuildSet& Set = LC->GetBuildSet();
	for (int32 i = 0; i < Set.Builds.Num(); ++i)
	{
		const FAFLCreatorBuild& Build = Set.Builds[i];
		UAFLW_LoadoutTileBase* Tile = CreateWidget<UAFLW_LoadoutTileBase>(this, SpawnClass);
		if (!Tile) { continue; }

		// THE NAME MAY NOT BE SHOWABLE. A pending or rejected name is not shown to anyone -- the build
		// is intact, only the name is gated -- so the public accessor is the one to ask.
		Tile->SetBuildData(i, FText::FromString(Build.GetPublicDisplayName()),
			Build.bReadOnly, i == Set.ActiveBuildIndex);
		Tile->OnBuildTileClicked.AddDynamic(this, &UAFLW_LoadoutBase::HandleBuildTileClicked);
		BuildsContainer->AddChild(Tile);
	}

	UE_LOG(LogAFLCombat, Log, TEXT("[AFLLoadout] builds rebuilt: %d held, active=%d"),
		Set.Builds.Num(), Set.ActiveBuildIndex);
}

void UAFLW_LoadoutBase::HandleNewBuildClicked()
{
	// -1 is the append case, and the only one the slot cap gates.
	OpenCreator(INDEX_NONE);
}

void UAFLW_LoadoutBase::HandleBuildTileClicked(int32 InBuildIndex)
{
	// EDIT: the creator opens ON this build, with it loaded. Editing an existing build is always
	// permitted -- the cap gates creating another, never changing one already held.
	UE_LOG(LogAFLCombat, Log, TEXT("[AFLLoadout] build tile clicked -> edit index=%d"), InBuildIndex);
	OpenCreator(InBuildIndex);
}

void UAFLW_LoadoutBase::HandleAxisTabClicked(EAFLLoadoutAxis Axis, FName /*CosmeticId*/)
{
	SetActiveAxis(Axis);
}

void UAFLW_LoadoutBase::HandleRailItemClicked(EAFLLoadoutAxis /*Axis*/, FName CosmeticId)
{
	SelectItem(CosmeticId);
}

void UAFLW_LoadoutBase::HandleEquipButtonClicked()
{
	CommitEquip();
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

	// CC-5 ARRANGEMENT AXES: one affordance tile, not a catalog rail. Falling through would query the
	// catalog with QueryTypeForAxis's DEFAULT (Weapon) and fill the sticker rail with weapons.
	if (IsArrangementAxis(Axis))
	{
		UAFLW_LoadoutTileBase* Entry = CreateWidget<UAFLW_LoadoutTileBase>(this, TileClass);
		if (Entry)
		{
			// The axis's own name for itself, via reflection. Typing it here would be a second source.
			const FText AxisName = StaticEnum<EAFLLoadoutAxis>()
				? StaticEnum<EAFLLoadoutAxis>()->GetDisplayNameTextByValue(static_cast<int64>(Axis))
				: FText::GetEmpty();
			// NAME_None, and bEquipped=false: there is no single id to be equipped ON this axis, which is
			// the whole reason it is an affordance. HandleTileClicked branches on the AXIS, never on this.
			Entry->SetTileData(Axis, NAME_None, AxisName, /*bEquipped=*/false,
				/*bIsSwatch=*/false, FLinearColor::White, nullptr);
			Entry->OnTileClicked.AddDynamic(this, &UAFLW_LoadoutBase::HandleTileClicked);
			if (bStoreCardStyle)
			{
				Entry->ApplyLoadoutCardStyle(false);
			}
			Container->AddChild(Entry);
		}
		return;
	}

	TArray<FAFLCatalogEntry> Owned;
	GetOwnedEntriesForAxis(Axis, Owned);
	const FName EquippedId = GetEquippedIdForAxis(Axis);

	// Color axes (body/edge/beam) render as tinted swatch chips -> resolve each cosmetic's color from its asset.
	const bool bColorAxis = IsColorAxis(Axis);
	const UAFLCosmeticCatalogSubsystem* Catalog = bColorAxis ? GetCatalog() : nullptr;

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

		bool bIsSwatch = false;
		FLinearColor SwatchColor = FLinearColor::White;
		if (bColorAxis && Catalog)
		{
			bIsSwatch = true;
			const UAFLSkinColorAsset* ColorAsset = Cast<UAFLSkinColorAsset>(Catalog->ResolveAsset(Entry.CosmeticId));
			SwatchColor = ResolveAxisColor(this, Axis, ColorAsset);
		}
		Tile->SetTileData(Axis, Entry.CosmeticId, Label, Entry.CosmeticId == EquippedId, bIsSwatch, SwatchColor, Entry.ShopThumbnail);
		Tile->OnTileClicked.AddDynamic(this, &UAFLW_LoadoutBase::HandleTileClicked);
		if (bStoreCardStyle)
		{
			// STORE PARITY (front-end locker only; gated by the WBP flag): reveal the rarity frame + neon-pipe EQUIP
			// button. The EQUIP button routes back through OnTileClicked -> HandleTileClicked, so no new handler.
			Tile->ApplyLoadoutCardStyle(Entry.CosmeticId == EquippedId);
		}
		Container->AddChild(Tile);
	}
}

void UAFLW_LoadoutBase::HandleTileClicked(EAFLLoadoutAxis Axis, FName CosmeticId)
{
	// CC-5: an arrangement axis has nothing to equip -- its tile OPENS THE CREATOR, focused on that axis.
	// Branching on the axis and not on CosmeticId is deliberate: see IsArrangementAxis.
	if (IsArrangementAxis(Axis))
	{
		UE_LOG(LogAFLCombat, Log, TEXT("[AFLLoadout] tile clicked -> creator SHORTCUT (axis=%d)"), (int32)Axis);
		OpenCreatorOnAxis(Axis);
		return; // no equip, no rebuild: the loadout is unchanged and stays alive under the creator.
	}

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
		const FSoftClassPath WbpPath(GAFLLoadoutRootWidget.Path);
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

UAFLW_Creator* UAFLW_LoadoutBase::OpenCreatorOnAxis(EAFLLoadoutAxis FocusAxis)
{
	// The shortcut. Opens a NEW build focused on one axis -- the sticker/accessory tiles' door.
	UAFLW_Creator* W = OpenCreator(INDEX_NONE);
	if (W)
	{
		W->FocusAxis = FocusAxis;
		W->bHasFocusAxis = true;
	}
	return W;
}

UAFLW_Creator* UAFLW_LoadoutBase::OpenCreator(int32 BuildIndex)
{
	// CONFORMS TO HOW THE LOADOUT ITSELF IS PUSHED -- PushWidgetToLayerStack on UI.Layer.Menu, the same
	// call the front-end market uses, NOT a second push pattern. Its init hook runs BEFORE activation,
	// which is what lets the focus axis and the loadout handle be set on a widget that has not drawn a
	// frame yet; pushing first and poking after is a visible jump.
	//
	// BACK/CANCEL NEEDS NO CODE: both widgets are UCommonActivatableWidgets on the SAME layer stack, so
	// the creator sits ON TOP of this loadout and popping it reveals this instance -- state intact,
	// because it was never destroyed. That is the framework's own semantics, not something re-invented.
	if (CreatorWidgetClass.IsNull())
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("[Loadout] OpenCreator REFUSED -- CreatorWidgetClass unset on %s."),
			*GetClass()->GetName());
		return nullptr;
	}
	UClass* Resolved = CreatorWidgetClass.LoadSynchronous();
	if (!Resolved)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("[Loadout] OpenCreator REFUSED -- %s did not load."),
			*CreatorWidgetClass.ToString());
		return nullptr;
	}
	APlayerController* PC = GetOwningPlayer();
	UPrimaryGameLayout* Layout = PC ? UPrimaryGameLayout::GetPrimaryGameLayout(PC) : nullptr;
	if (!Layout)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("[Loadout] OpenCreator REFUSED -- no PrimaryGameLayout for this player."));
		return nullptr;
	}

	UAFLW_LoadoutBase* Self = this;
	UAFLW_Creator* Pushed = Layout->PushWidgetToLayerStack<UAFLW_Creator>(
		TAG_UI_Layer_Menu_Creator, Resolved,
		[Self, BuildIndex](UAFLW_Creator& W)
		{
			// Both BEFORE activation: the creator reads the schema, preview pawn and slot counter through
			// the loadout, and a pushed-but-uninitialised creator renders an empty rail that reads as a
			// data bug rather than a wiring one.
			//
			// NO AXIS BY DEFAULT. The creator lands on the build as a whole; OpenCreatorOnAxis sets one
			// afterwards for the two shortcut tiles, which are secondary doors.
			W.bHasFocusAxis = false;
			W.InitializeCreator(Self);
			if (BuildIndex != INDEX_NONE) { W.LoadBuild(BuildIndex); }
			else                          { W.BeginNewBuild(); }
		});

	if (!Pushed)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("[Loadout] OpenCreator -- push returned null for %s."), *Resolved->GetName());
		return nullptr;
	}
	UE_LOG(LogAFLCombat, Display, TEXT("[Loadout] OpenCreator -> %s buildIndex=%d"),
		*Resolved->GetName(), BuildIndex);
	return Pushed;
}
