// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_LoadoutBase.h"

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

#if !UE_BUILD_SHIPPING
#include "Engine/LocalPlayer.h"
#include "PrimaryGameLayout.h"
#include "GameplayTagContainer.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_LoadoutBase)

// Live-tunable preview framing (afl.Loadout.Preview* -> tune at the prove without a rebuild). Defaults match
// the UPROPERTY seeds; RepositionPreviewCamera reads them per-tick.
// Defaults tuned for a FULL-BODY, capsule-filling frame (lower look-at includes the feet, less downward
// angle). Framing is operator-eyeball-tuned live from here via these cvars -- no rebuild.
// Defaults computed to FILL the panel with the 270cm capsule (camera ~180cm out, look-at the pod's vertical
// mid). All live-tunable in PIE via these cvars -- no rebuild -- so the operator dials the final composition.
static TAutoConsoleVariable<float> CVarLoadoutPreviewFwd(TEXT("afl.Loadout.PreviewFwd"), 180.f, TEXT("Preview cam forward offset -- LOWER = closer = pod FILLS more."));
static TAutoConsoleVariable<float> CVarLoadoutPreviewRight(TEXT("afl.Loadout.PreviewRight"), 40.f, TEXT("Preview cam right offset (3/4 angle)."));
static TAutoConsoleVariable<float> CVarLoadoutPreviewUp(TEXT("afl.Loadout.PreviewUp"), 47.f, TEXT("Preview cam up offset (camera height)."));
static TAutoConsoleVariable<float> CVarLoadoutPreviewFocusUp(TEXT("afl.Loadout.PreviewFocusUp"), 21.f, TEXT("Preview cam look-at height -- ~pod vertical mid; lower to frame the robot lower. (operator-tuned)"));
static TAutoConsoleVariable<float> CVarLoadoutPreviewFOV(TEXT("afl.Loadout.PreviewFOV"), 82.f, TEXT("Preview cam FOV -- higher = wider. (operator-tuned to fill the panel)"));
// Grounding: raises the HERO relative to the capsule (drops the pod under the pawn) so the feet clear the
// capsule's base geometry, with the glowing floor disc glued under the feet. THIS is the raise-the-robot
// knob (the old PlatformZ moved only the disc). Tunable live; a bigger value lifts the hero higher.
static TAutoConsoleVariable<float> CVarLoadoutPodGroundZ(TEXT("afl.Loadout.PodGroundZ"), 10.f, TEXT("Raise the hero relative to the capsule (cm) so feet clear the base; disc follows. Re-check framing after."));
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
		default:                          return EAFLCosmeticType::Weapon; // Weapon + WeaponSkin (Identity is dual-type, special-cased)
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
		case EAFLLoadoutAxis::BodyColor:   return TEXT("AFL.Finish.");
		case EAFLLoadoutAxis::EdgeColor:   return TEXT("AFL.Edge.");
		case EAFLLoadoutAxis::Facemask:    return TEXT("AFL.Facemask.");
		default:                           return FString(); // Identity -> dual-type query, no single namespace filter
		}
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
	case EAFLLoadoutAxis::Identity:    return Sel.GetActiveIdentityId();
	case EAFLLoadoutAxis::BodyColor:   return Sel.BodyId;
	case EAFLLoadoutAxis::EdgeColor:   return Sel.EdgeId;
	case EAFLLoadoutAxis::Facemask:    return Sel.FacemaskId;
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
		// Identity is either/or, resolved by the id's namespace (AFL.Character.* vs AFL.Team.*).
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
	// Spawn the ASC-less display pawn. NEVER possessed -> no ASC -> the ASC-gated AFLCombat ability grant has no
	// target (the flagged combat-leak risk, dodged BY CONSTRUCTION). Location: at the local gameplay pawn if one
	// exists (in-match de-risk), else origin (front-end, Inc 3). The capture isolates it via the ShowOnlyList.
	FVector SpawnLoc = FVector::ZeroVector;
	if (const APawn* Local = GetLocalPawn())
	{
		SpawnLoc = Local->GetActorLocation();
	}
	UClass* PawnCls = DisplayPawnClass ? DisplayPawnClass.Get() : AAFLLoadoutDisplayPawn::StaticClass();
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAFLLoadoutDisplayPawn* Spawned = World->SpawnActor<AAFLLoadoutDisplayPawn>(PawnCls, SpawnLoc, FRotator::ZeroRotator, SpawnParams);
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
		if (DisplayPartMap && IdentityId != NAME_None)
		{
			const TSoftClassPtr<AActor> Soft = DisplayPartMap->ResolveCharacterPart(IdentityId);
			if (!Soft.IsNull()) { RobotCls = Soft.LoadSynchronous(); }
		}
		if (!RobotCls)
		{
			RobotCls = DisplayFallbackRobotClass.IsNull() ? nullptr : DisplayFallbackRobotClass.LoadSynchronous();
			if (!RobotCls)
			{
				RobotCls = LoadClass<AActor>(nullptr, TEXT("/Game/BagMan/Characters/Cosmetics/B_AFL_Robot_IRONICS.B_AFL_Robot_IRONICS_C"));
			}
		}
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
		SkinCtrl->RefreshSkinForPawn(Pawn);     // body finish (TeamColor) + edge emissive (proven)
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
		UClass* PodCls = PodClass ? PodClass.Get() : AAFLLoadoutPod::StaticClass();
		FActorSpawnParameters PodSpawnParams;
		PodSpawnParams.ObjectFlags |= RF_Transient;
		PodSpawnParams.Owner = Pawn;
		PreviewPod = World->SpawnActor<AAFLLoadoutPod>(PodCls, PodSpawnParams);
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

	if (AAFLLoadoutPod* Pod = PreviewPod.Get())
	{
		Pod->Destroy();
	}
	PreviewPod = nullptr;

	if (AAFLLoadoutDisplayPawn* Pawn = DisplayPawn.Get())
	{
		Pawn->Destroy();
	}
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

void UAFLW_LoadoutBase::RepositionPreviewPod()
{
	AAFLLoadoutPod* Pod = PreviewPod.Get();
	if (!Pod)
	{
		return;
	}
	const float GroundZ = CVarLoadoutPodGroundZ.GetValueOnGameThread();
	const float Scale = FMath::Max(0.1f, CVarLoadoutPodScale.GetValueOnGameThread());
	// Scale the whole pod ABOUT the grounded feet: SetActorScale + a matching drop keep the floor disc's TOP
	// exactly at the feet for ANY scale, while the capsule top rises (headroom) and the base drops below.
	Pod->SetActorScale3D(FVector(Scale));
	Pod->SetActorRelativeLocation(FVector(0.f, 0.f, -(PreviewFeetDrop + GroundZ * Scale)));
	Pod->SetPlatformZ(GroundZ - 2.f);
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
	if (!Cap)
	{
		return;
	}
	const FVector Off(CVarLoadoutPreviewFwd.GetValueOnGameThread(),
	                  CVarLoadoutPreviewRight.GetValueOnGameThread(),
	                  CVarLoadoutPreviewUp.GetValueOnGameThread());
	const FVector Focus(0.f, 0.f, CVarLoadoutPreviewFocusUp.GetValueOnGameThread());
	Cap->SetActorRelativeLocation(Off);
	Cap->SetActorRelativeRotation((Focus - Off).Rotation());
	if (USceneCaptureComponent2D* CapComp = Cap->GetCaptureComponent2D())
	{
		CapComp->FOVAngle = CVarLoadoutPreviewFOV.GetValueOnGameThread();
	}
}

void UAFLW_LoadoutBase::RebuildTiles()
{
	// Rebuild every axis grid from the current owned-set + selection. All non-weapon containers are optional
	// (BindWidgetOptional) -> a null container is skipped inside RebuildAxisTiles.
	RebuildAxisTiles(EAFLLoadoutAxis::Weapon,     TileContainer);
	RebuildAxisTiles(EAFLLoadoutAxis::WeaponSkin, SkinTileContainer);
	RebuildAxisTiles(EAFLLoadoutAxis::Beam,       BeamTileContainer);
	RebuildAxisTiles(EAFLLoadoutAxis::Identity,   IdentityTileContainer);
	RebuildAxisTiles(EAFLLoadoutAxis::BodyColor,  BodyColorTileContainer);
	RebuildAxisTiles(EAFLLoadoutAxis::EdgeColor,  EdgeColorTileContainer);
	RebuildAxisTiles(EAFLLoadoutAxis::Facemask,   FacemaskTileContainer);
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
