// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDismemberComponent.h"

#include "AFLDismember.h"
#include "AFLDismemberTypes.h"
#include "AFLDismemberZoneSet.h"
#include "AFLDismemberedPart.h"
#include "AFLHeadLootBox.h"
#include "Cosmetics/AFLSkinColorComponent.h"   // victim skin color handoff to the head prop (AFLCombat)
#include "Materials/MaterialInterface.h"          // GetMaterial(1) return type
#include "Materials/MaterialInstanceConstant.h"   // the replication-safe per-skin head MIC (layer-2 identity)
#include "Materials/MaterialInstanceDynamic.h"    // recover the MIC via MID->Parent
#include "HUD/AFLDismemberSeverMessage.h"   // FAFLDismemberSeverMessage (AFLCombat)
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
// B-2 FIX: ULyraPawnComponent_CharacterParts::GetCharacterPartActors() is a public UFUNCTION but is
// NOT LYRAGAME_API-exported -> a direct call from this module fails to LINK (unexported symbol).
// We reach it by REFLECTION instead (find the component by class name, call the UFUNCTION via
// ProcessEvent) so no Lyra symbol is needed -- the same ProcessEvent pattern used for the phase
// subsystem. No #include of the Lyra cosmetics header, no Build.cs change.
#include "UObject/UnrealType.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"               // Cast<APawn> for the loot-box Initialize
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayEffect.h"
#include "HUD/AFLOverkillMessage.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDismemberComponent)

namespace
{
	// Native-define mirrors the AFLCoreTags.ini tag (Event.Damage.Overkill.AFL) to avoid
	// first-boot race -- same pattern as AFLHitConfirmComponent's TAG_Event_Damage_Confirmed.
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Damage_Overkill_AFL_Dismember, "Event.Damage.Overkill.AFL");

	// S4-INC3 PHASE B-2: the live sever channel (declared in AFLCoreTags.ini @ B-1 Phase A).
	// UAFLDamageExecCalc broadcasts this when a zone's HP depletes; OnSever consumes it.
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Dismember_Sever_AFL_Dismember, "Event.Dismember.Sever.AFL");

	// S4-INC4 (AFL-0408): the PERSISTENT severed-state cues (declared in AFLCombatTags.ini). SeverZone
	// AddGameplayCue's one of these (server-auth) -> replicated container -> UAFLCueNotify_ZoneSever
	// hides the bone on all clients; RestoreZone RemoveGameplayCue's it -> OnRemove un-hides.
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cue_Dismember_State_Head_Dismember,     "GameplayCue.Combat.Dismember.State.Head");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cue_Dismember_State_LeftArm_Dismember,  "GameplayCue.Combat.Dismember.State.LeftArm");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cue_Dismember_State_RightArm_Dismember, "GameplayCue.Combat.Dismember.State.RightArm");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cue_Dismember_State_LeftLeg_Dismember,  "GameplayCue.Combat.Dismember.State.LeftLeg");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cue_Dismember_State_RightLeg_Dismember, "GameplayCue.Combat.Dismember.State.RightLeg");

	// Zone -> its persistent severed-state cue tag. None/Torso have no state cue (not severable to a loot path).
	FGameplayTag ZoneSeverCueTag(EAFLBodyZone Zone)
	{
		switch (Zone)
		{
		case EAFLBodyZone::Head:     return TAG_Cue_Dismember_State_Head_Dismember;
		case EAFLBodyZone::LeftArm:  return TAG_Cue_Dismember_State_LeftArm_Dismember;
		case EAFLBodyZone::RightArm: return TAG_Cue_Dismember_State_RightArm_Dismember;
		case EAFLBodyZone::LeftLeg:  return TAG_Cue_Dismember_State_LeftLeg_Dismember;
		case EAFLBodyZone::RightLeg: return TAG_Cue_Dismember_State_RightLeg_Dismember;
		default:                     return FGameplayTag();
		}
	}

	// S4-INC3 PHASE B-1: the consequence/state tags RestoreZone removes (declared in AFLCombatTags.ini).
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Dismembered_LeftArm_Dismember,  "State.Dismembered.LeftArm");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Dismembered_RightArm_Dismember, "State.Dismembered.RightArm");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Dismembered_LeftLeg_Dismember,  "State.Dismembered.LeftLeg");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Dismembered_RightLeg_Dismember, "State.Dismembered.RightLeg");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Decapitated_Dismember,          "State.Decapitated");

	// S4-INC4: a [SV]/[CL1]/[CL2] net-mode tag for the all-client cosmetic instrument -- the
	// AFLInteractionTestHarness pattern. Proves the bone-hide RAN on the remote client (not eyeball).
	FString NetTag(const UObject* WorldCtx)
	{
		const UWorld* W = WorldCtx ? WorldCtx->GetWorld() : nullptr;
		if (!W) { return TEXT("[??]"); }
		switch (W->GetNetMode())
		{
		case NM_DedicatedServer: return TEXT("[SV]");
		case NM_ListenServer:    return TEXT("[SV]");
		case NM_Client:          return FString::Printf(TEXT("[CL%d]"), static_cast<int32>(UE::GetPlayInEditorID()));
		case NM_Standalone:      return TEXT("[SP]");
		default:                 return TEXT("[??]");
		}
	}

	// S4-INC4 RESIDENCY FIX: map the AFLVFX cue's FName leaf -> EAFLBodyZone (the leaf names match the
	// enum names directly). This is the IAFLDismemberCosmeticTarget front door's parse, on the component
	// side so AFLVFX stays AFLCore-free. (Moved from the deleted AFLDismember-resident cue notify's
	// ZoneFromCueTag, simplified to leaf-only since the cue already strips the tag to the leaf.)
	EAFLBodyZone LeafToZone(FName Leaf)
	{
		if (Leaf == FName(TEXT("Head")))     { return EAFLBodyZone::Head; }
		if (Leaf == FName(TEXT("LeftArm")))  { return EAFLBodyZone::LeftArm; }
		if (Leaf == FName(TEXT("RightArm"))) { return EAFLBodyZone::RightArm; }
		if (Leaf == FName(TEXT("LeftLeg")))  { return EAFLBodyZone::LeftLeg; }
		if (Leaf == FName(TEXT("RightLeg"))) { return EAFLBodyZone::RightLeg; }
		return EAFLBodyZone::None;
	}

	// Zone -> the state tag SeverZone grants / RestoreZone removes. Head = State.Decapitated;
	// limbs = State.Dismembered.<Zone>. None/Torso have no consequence tag today.
	FGameplayTag ZoneStateTag(EAFLBodyZone Zone)
	{
		switch (Zone)
		{
		case EAFLBodyZone::Head:     return TAG_State_Decapitated_Dismember;
		case EAFLBodyZone::LeftArm:  return TAG_State_Dismembered_LeftArm_Dismember;
		case EAFLBodyZone::RightArm: return TAG_State_Dismembered_RightArm_Dismember;
		case EAFLBodyZone::LeftLeg:  return TAG_State_Dismembered_LeftLeg_Dismember;
		case EAFLBodyZone::RightLeg: return TAG_State_Dismembered_RightLeg_Dismember;
		default:                     return FGameplayTag();
		}
	}
}

UAFLDismemberComponent::UAFLDismemberComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAFLDismemberComponent::BeginPlay()
{
	Super::BeginPlay();

	// Server-authoritative: the overkill broadcast is server-side, and dismemberment is
	// authoritative gameplay. No local-controller gate -- register on the server only.
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
		ListenerHandle = MessageSubsystem.RegisterListener(
			TAG_Event_Damage_Overkill_AFL_Dismember,
			this,
			&ThisClass::OnOverkill);

		// S4-INC3 PHASE B-2: the LIVE sever trigger. The head no longer overkills (B-1) -> OnOverkill
		// never fires for a head; the zone-HP absorber broadcasts Event.Dismember.Sever.AFL instead.
		// OnSever pops the head + grants State.Decapitated + spawns the loot-box.
		SeverListenerHandle = MessageSubsystem.RegisterListener(
			TAG_Event_Dismember_Sever_AFL_Dismember,
			this,
			&ThisClass::OnSever);
	}
}

void UAFLDismemberComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ListenerHandle.IsValid())
	{
		UGameplayMessageSubsystem::Get(this).UnregisterListener(ListenerHandle);
	}
	if (SeverListenerHandle.IsValid())
	{
		UGameplayMessageSubsystem::Get(this).UnregisterListener(SeverListenerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UAFLDismemberComponent::OnOverkill(FGameplayTag Channel, const FAFLOverkillMessage& Payload)
{
	// Victim-side filter: only react if I am the one who was overkilled.
	if (Payload.Target != GetOwner())
	{
		return;
	}

	if (ZoneSet.IsNull())
	{
		UE_LOG(LogAFLDismember, Warning,
			TEXT("[AFLDismember] %s overkilled (bone=%s) but no ZoneSet assigned -- no dismemberment"),
			*GetNameSafe(GetOwner()), *Payload.BoneName.ToString());
		return;
	}

	ResolveAndSever(Payload.BoneName);
}

void UAFLDismemberComponent::OnSever(FGameplayTag Channel, const FAFLDismemberSeverMessage& Payload)
{
	// Victim-side filter: only react if I am the severed one.
	if (Payload.Target != GetOwner())
	{
		return;
	}

	if (ZoneSet.IsNull())
	{
		UE_LOG(LogAFLDismember, Warning,
			TEXT("[AFLDismember] %s zone-severed (bone=%s zone=%d) but no ZoneSet assigned -- no dismemberment"),
			*GetNameSafe(GetOwner()), *Payload.BoneName.ToString(), static_cast<int32>(Payload.Zone));
		return;
	}

	// B-2 SCOPE: the head is the lit path this increment. Limbs still ride OnOverkill (their proven
	// trigger) -- guard so a future all-limbs migration to OnSever does not double-sever them today.
	const bool bIsHead = (Payload.Zone == EAFLBodyZone::Head);
	if (!bIsHead)
	{
		UE_LOG(LogAFLDismember, Verbose,
			TEXT("[AFLDismember] sever zone=%d on %s -- limb sever rides OnOverkill this increment, ignored here"),
			static_cast<int32>(Payload.Zone), *GetNameSafe(GetOwner()));
		return;
	}

	UE_LOG(LogAFLDismember, Display,
		TEXT("[AFLDismember] HEAD SEVER on %s (bone=%s) -- cosmetic detach + State.Decapitated + loot-box"),
		*GetNameSafe(GetOwner()), *Payload.BoneName.ToString());

	// Cosmetic detach (HideBone + prop + cue) AND spawn the loot-box once the row resolves.
	ResolveAndSever(Payload.BoneName, /*bIsHeadSever=*/true);
}

void UAFLDismemberComponent::ResolveAndSever(FName HitBone, bool bIsHeadSever)
{
	// AAA: async-load the ZoneSet (soft), then resolve the bone -> row on the game thread.
	// RequestAsyncLoad keeps the asset off the hard-ref graph; the lambda runs once loaded.
	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	const FSoftObjectPath ZonePath = ZoneSet.ToSoftObjectPath();

	TWeakObjectPtr<UAFLDismemberComponent> WeakThis(this);
	Streamable.RequestAsyncLoad(ZonePath, FStreamableDelegate::CreateLambda([WeakThis, HitBone, ZonePath, bIsHeadSever]()
	{
		UAFLDismemberComponent* Self = WeakThis.Get();
		if (!Self || !Self->GetOwner() || !Self->GetOwner()->HasAuthority())
		{
			return;
		}

		const UAFLDismemberZoneSet* Set = Cast<UAFLDismemberZoneSet>(ZonePath.ResolveObject());
		if (!Set)
		{
			UE_LOG(LogAFLDismember, Warning, TEXT("[AFLDismember] ZoneSet failed to load -- no dismemberment"));
			return;
		}

		const FAFLDismemberZone* Row = Set->FindZoneForBone(HitBone);
		if (!Row)
		{
			UE_LOG(LogAFLDismember, Verbose,
				TEXT("[AFLDismember] sever/overkill on %s but bone=%s maps to no zone -- no dismemberment"),
				*GetNameSafe(Self->GetOwner()), *HitBone.ToString());
			return;
		}

		UE_LOG(LogAFLDismember, Warning,
			TEXT("[AFLDismember] ZONE SEVER on %s -- bone=%s -> zone=%d severed=%s (head=%d)"),
			*GetNameSafe(Self->GetOwner()), *HitBone.ToString(),
			static_cast<int32>(Row->Zone), *Row->SeveredBone.ToString(), bIsHeadSever ? 1 : 0);

		// Copy the row -- the async prop/GE continuations in SeverZone outlive the
		// array-pointer Row returned by FindZoneForBone.
		Self->SeverZone(*Row);

		// HEAD path (B-2): grant State.Decapitated (the survivable consequence -> drives the decap
		// camera ability) and spawn the head loot-box tied to the owner. Limb consequences come from
		// the row's ConsequenceGE inside SeverZone above; the head's consequence is granted here so
		// the head row can keep an empty ConsequenceGE (no double-grant).
		if (bIsHeadSever)
		{
			Self->ApplyDecapitatedEffect();
			Self->SpawnHeadLootBox();
		}
	}));
}

TArray<USkeletalMeshComponent*> UAFLDismemberComponent::GatherZoneMeshes() const
{
	TArray<USkeletalMeshComponent*> Meshes;

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return Meshes;
	}

	// CharacterMesh0 -- the hero's own skel mesh = the anim driver + the prop-transform source.
	if (USkeletalMeshComponent* OwnMesh = Owner->FindComponentByClass<USkeletalMeshComponent>())
	{
		Meshes.AddUnique(OwnMesh);
	}

	// Every VISIBLE CharacterPart mesh (the robot body). These are CopyPose-driven child actors, NOT
	// leader-pose followers, so the CharacterMesh0 hide does not reach them -- hide them directly.
	// REFLECTION reach (the symbol is not exported): find ULyraPawnComponent_CharacterParts by class
	// name, call its GetCharacterPartActors() UFUNCTION via ProcessEvent. Null/absent on non-cosmetic
	// actors (the dummy) -> the set stays CharacterMesh0-only (the proven dummy behavior, preserved).
	// Match by BASE CLASS, not exact name: the cosmetics component on the hero is a BP SUBCLASS
	// (B_MannequinPawnCosmetics_C) of ULyraPawnComponent_CharacterParts -- an exact-name check missed
	// it (the instrument proved this: "no CharacterParts component" while one was present as a subclass).
	// We cannot name the unexported C++ base type, so walk the UClass super-chain by NAME instead.
	static const FName CharacterPartsClassName(TEXT("LyraPawnComponent_CharacterParts"));
	UActorComponent* PartsComp = nullptr;
	TInlineComponentArray<UActorComponent*> OwnerComps;
	Owner->GetComponents(OwnerComps);
	for (UActorComponent* Comp : OwnerComps)
	{
		if (!Comp)
		{
			continue;
		}
		for (const UClass* C = Comp->GetClass(); C; C = C->GetSuperClass())
		{
			if (C->GetFName() == CharacterPartsClassName)
			{
				PartsComp = Comp;
				break;
			}
		}
		if (PartsComp)
		{
			break;
		}
	}
	if (PartsComp)
	{
		if (UFunction* Fn = PartsComp->FindFunction(FName(TEXT("GetCharacterPartActors"))))
		{
			// The function takes no args and returns TArray<AActor*>; the param buffer is just the
			// return value. Mirror it locally and let ProcessEvent fill it.
			struct FGetPartActorsParms { TArray<AActor*> ReturnValue; };
			FGetPartActorsParms Parms;
			PartsComp->ProcessEvent(Fn, &Parms);

			UE_LOG(LogAFLDismember, Display,
				TEXT("[AFLDismember] GatherZoneMeshes: CharacterParts found %d part-actor(s)"),
				Parms.ReturnValue.Num());

			for (AActor* PartActor : Parms.ReturnValue)
			{
				if (!PartActor)
				{
					continue;
				}
				// A part actor may carry MORE than one skel mesh (e.g. a separate helmet/head piece);
				// gather ALL of them, not just the first, so a separately-skinned head is covered.
				TInlineComponentArray<USkeletalMeshComponent*> PartMeshes;
				PartActor->GetComponents(PartMeshes);
				for (USkeletalMeshComponent* PartMesh : PartMeshes)
				{
					if (PartMesh)
					{
						Meshes.AddUnique(PartMesh);
						UE_LOG(LogAFLDismember, Display,
							TEXT("[AFLDismember] GatherZoneMeshes:   part %s mesh %s (skelmesh=%s)"),
							*GetNameSafe(PartActor), *PartMesh->GetName(),
							*GetNameSafe(PartMesh->GetSkeletalMeshAsset()));
					}
				}
			}
		}
		else
		{
			UE_LOG(LogAFLDismember, Warning,
				TEXT("[AFLDismember] GatherZoneMeshes: parts comp found but no GetCharacterPartActors UFUNCTION"));
		}
	}
	else
	{
		UE_LOG(LogAFLDismember, Display,
			TEXT("[AFLDismember] GatherZoneMeshes: no CharacterParts component on %s (non-cosmetic actor -> CharacterMesh0 only)"),
			*GetNameSafe(Owner));
	}

	UE_LOG(LogAFLDismember, Display,
		TEXT("[AFLDismember] GatherZoneMeshes: total %d mesh(es) for zone-bone op"), Meshes.Num());
	return Meshes;
}

void UAFLDismemberComponent::ApplyZoneHideCosmetic(EAFLBodyZone Zone)
{
	// ALL-CLIENT cosmetic (no authority guard -- called by the cue notify on every client). Resolve
	// the bone from the loaded ZoneSet (resident: the cue was added after the server resolved+loaded it),
	// then hide it on the FULL visible mesh set. Net-mode-tagged so the 2-client proof shows it ran on CL.
	FName SeveredBone = NAME_None;
	if (const UAFLDismemberZoneSet* Set = ZoneSet.Get())
	{
		for (const FAFLDismemberZone& Row : Set->Zones)
		{
			if (Row.Zone == Zone) { SeveredBone = Row.SeveredBone; break; }
		}
	}
	const FString Net = NetTag(this);
	if (SeveredBone.IsNone())
	{
		UE_LOG(LogAFLDismember, Warning,
			TEXT("%s ZoneSever OnActive zone=%d -- no bone in ZoneSet (not loaded on this client yet?) -- NO HIDE"),
			*Net, static_cast<int32>(Zone));
		return;
	}

	const TArray<USkeletalMeshComponent*> ZoneMeshes = GatherZoneMeshes();
	for (USkeletalMeshComponent* Mesh : ZoneMeshes)
	{
		const int32 BoneIdx = Mesh->GetBoneIndex(SeveredBone);
		Mesh->HideBoneByName(SeveredBone, PBO_Term);
		UE_LOG(LogAFLDismember, Display,
			TEXT("%s ZoneSever OnActive zone=%d HideBone '%s' on %s (skelmesh=%s boneIdx=%d %s)"),
			*Net, static_cast<int32>(Zone), *SeveredBone.ToString(), *Mesh->GetName(),
			*GetNameSafe(Mesh->GetSkeletalMeshAsset()), BoneIdx,
			BoneIdx == INDEX_NONE ? TEXT("NO-OP") : TEXT("ok"));
	}
	UE_LOG(LogAFLDismember, Display,
		TEXT("%s ZoneSever OnActive zone=%d total=%d mesh(es) hidden on %s"),
		*Net, static_cast<int32>(Zone), ZoneMeshes.Num(), *GetNameSafe(GetOwner()));
}

void UAFLDismemberComponent::ApplyZoneRestoreCosmetic(EAFLBodyZone Zone)
{
	// Symmetric all-client un-hide (cue OnRemove). Same mesh set as the hide (shared GatherZoneMeshes).
	FName SeveredBone = NAME_None;
	if (const UAFLDismemberZoneSet* Set = ZoneSet.Get())
	{
		for (const FAFLDismemberZone& Row : Set->Zones)
		{
			if (Row.Zone == Zone) { SeveredBone = Row.SeveredBone; break; }
		}
	}
	const FString Net = NetTag(this);
	if (SeveredBone.IsNone())
	{
		UE_LOG(LogAFLDismember, Warning,
			TEXT("%s ZoneSever OnRemove zone=%d -- no bone in ZoneSet -- NO UN-HIDE"),
			*Net, static_cast<int32>(Zone));
		return;
	}

	const TArray<USkeletalMeshComponent*> ZoneMeshes = GatherZoneMeshes();
	for (USkeletalMeshComponent* Mesh : ZoneMeshes)
	{
		Mesh->UnHideBoneByName(SeveredBone);
	}
	UE_LOG(LogAFLDismember, Display,
		TEXT("%s ZoneSever OnRemove zone=%d UnHideBone '%s' total=%d mesh(es) on %s"),
		*Net, static_cast<int32>(Zone), *SeveredBone.ToString(), ZoneMeshes.Num(), *GetNameSafe(GetOwner()));
}

void UAFLDismemberComponent::ApplyZoneHideByLeaf_Implementation(FName ZoneLeaf)
{
	// IAFLDismemberCosmeticTarget front door: the AFLVFX persistent cue (AAFLCueNotify_ZoneSever) calls
	// this on EVERY client. Map the FName leaf -> zone (AFLVFX is AFLCore-free), then run the existing
	// UNCHANGED hide. The B-2/0408 GatherZoneMeshes + HideBoneByName logic is entirely inside the call.
	ApplyZoneHideCosmetic(LeafToZone(ZoneLeaf));
}

void UAFLDismemberComponent::ApplyZoneRestoreByLeaf_Implementation(FName ZoneLeaf)
{
	ApplyZoneRestoreCosmetic(LeafToZone(ZoneLeaf));
}

void UAFLDismemberComponent::SeverZone(const FAFLDismemberZone& Row)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !GetWorld())
	{
		return;
	}

	// (1) S4-INC4 (AFL-0408): the bone-hide is now ALL-CLIENT via a PERSISTENT cue. SeverZone no longer
	// hides directly -- it AddGameplayCue's GameplayCue.Combat.Dismember.State.<Zone> (server-auth) into
	// the replicated minimal-cue container; UAFLCueNotify_ZoneSever::OnActive runs the hide
	// (ApplyZoneHideCosmetic = GatherZoneMeshes + HideBoneByName) on EVERY client, incl. the listen-server
	// host (so the host's OWN view is hidden by the cue, not here -> no double-hide) and late-joiners.
	// The cue is REMOVED in RestoreZone -> OnRemove un-hides everywhere. Persistent ADD (not one-shot
	// Execute) because the hide is persistent STATE that must survive late-join / relevancy-rejoin.
	UAbilitySystemComponent* SeverASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	const FGameplayTag SeverCueTag = ZoneSeverCueTag(Row.Zone);
	if (SeverASC && SeverCueTag.IsValid())
	{
		SeverASC->AddGameplayCue(SeverCueTag);
		UE_LOG(LogAFLDismember, Display,
			TEXT("[AFLDismember] %s SeverZone zone=%d -> AddGameplayCue %s (server-auth; all-client hide via cue)"),
			*GetNameSafe(Owner), static_cast<int32>(Row.Zone), *SeverCueTag.ToString());
	}
	else
	{
		UE_LOG(LogAFLDismember, Warning,
			TEXT("[AFLDismember] %s SeverZone zone=%d -- no ASC or no state-cue tag -- bone will NOT hide"),
			*GetNameSafe(Owner), static_cast<int32>(Row.Zone));
	}

	// (2) The severed-bone world transform = where the prop spawns (server-side prop spawn, unchanged).
	// Resolve CharacterMesh0 (the hero's own anim-driver mesh) for the socket xform -- the prop still
	// spawns at the bone position even though the visible hide now rides the cue.
	USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMesh)
	{
		UE_LOG(LogAFLDismember, Warning,
			TEXT("[AFLDismember] %s has no SkeletalMeshComponent -- cannot resolve sever transform"), *GetNameSafe(Owner));
		return;
	}
	const FTransform SeverXform = SkelMesh->GetSocketTransform(Row.SeveredBone, RTS_World);

	// (2/3) Async-load the (soft) prop class, then spawn at the transform + apply the pop impulse.
	if (!Row.PropClass.IsNull())
	{
		FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
		const FSoftObjectPath PropPath = Row.PropClass.ToSoftObjectPath();

		// Capture the spawn-time data by value (the row reference does not outlive the lambda).
		TWeakObjectPtr<UAFLDismemberComponent> WeakThis(this);
		const FVector2D ImpulseXY = Row.ImpulseXYRange;
		const float ImpulseZ = Row.ImpulseZ;

		Streamable.RequestAsyncLoad(PropPath, FStreamableDelegate::CreateLambda(
			[WeakThis, PropPath, SeverXform, ImpulseXY, ImpulseZ]()
		{
			UAFLDismemberComponent* Self = WeakThis.Get();
			if (!Self || !Self->GetOwner() || !Self->GetOwner()->HasAuthority() || !Self->GetWorld())
			{
				return;
			}

			UClass* PropClass = Cast<UClass>(PropPath.ResolveObject());
			if (!PropClass)
			{
				UE_LOG(LogAFLDismember, Warning, TEXT("[AFLDismember] prop class failed to load -- no prop"));
				return;
			}

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnParams.Owner = Self->GetOwner();

			AAFLDismemberedPart* Part =
				Self->GetWorld()->SpawnActor<AAFLDismemberedPart>(PropClass, SeverXform, SpawnParams);
			if (Part)
			{
				// Randomized pop -- matches the proven head (+-100 XY, +500 Z by default).
				Part->ApplyPopImpulse(FVector(
					FMath::FRandRange(ImpulseXY.X, ImpulseXY.Y),
					FMath::FRandRange(ImpulseXY.X, ImpulseXY.Y),
					ImpulseZ));
				UE_LOG(LogAFLDismember, Display,
					TEXT("[AFLDismember] Part prop spawned on %s -- pop + roll"),
					*GetNameSafe(Self->GetOwner()));
			}
		}));
	}

	UAbilitySystemComponent* VictimASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);

	// (4) Fire the cosmetic cue (replicated via the victim ASC) if the row sets one.
	if (VictimASC && Row.CueTag.IsValid())
	{
		FGameplayCueParameters CueParams;
		CueParams.Location     = SeverXform.GetLocation();
		CueParams.Instigator   = Owner;
		CueParams.SourceObject = Owner;
		VictimASC->ExecuteGameplayCue(Row.CueTag, CueParams);
	}

	// (5) Apply the consequence GE (grants State.Dismembered.<Zone>) if the row sets one --
	// EMPTY for lethal-cosmetic zones (head/torso = death, no surviving consequence). Async-load.
	if (VictimASC && !Row.ConsequenceGE.IsNull())
	{
		FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
		const FSoftObjectPath GEPath = Row.ConsequenceGE.ToSoftObjectPath();

		TWeakObjectPtr<UAFLDismemberComponent> WeakThis(this);
		TWeakObjectPtr<UAbilitySystemComponent> WeakASC(VictimASC);

		Streamable.RequestAsyncLoad(GEPath, FStreamableDelegate::CreateLambda([WeakThis, WeakASC, GEPath]()
		{
			UAFLDismemberComponent* Self = WeakThis.Get();
			UAbilitySystemComponent* ASC = WeakASC.Get();
			if (!Self || !ASC || !Self->GetOwner() || !Self->GetOwner()->HasAuthority())
			{
				return;
			}

			UClass* GEClass = Cast<UClass>(GEPath.ResolveObject());
			if (!GEClass)
			{
				UE_LOG(LogAFLDismember, Warning, TEXT("[AFLDismember] consequence GE failed to load"));
				return;
			}

			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddSourceObject(Self);
			const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GEClass, 1.0f, Context);
			if (Spec.IsValid())
			{
				ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
				UE_LOG(LogAFLDismember, Display,
					TEXT("[AFLDismember] consequence GE applied to %s"), *GetNameSafe(Self->GetOwner()));
			}
		}));
	}
}

void UAFLDismemberComponent::RestoreZone(EAFLBodyZone Zone)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || Zone == EAFLBodyZone::None)
	{
		return;
	}

	// (1) Resolve zone -> severed bone from the loaded ZoneSet (the inverse of FindZoneForBone).
	//     The ZoneSet was async-loaded at sever time -> resident here; a reattach without a prior
	//     sever just finds no bone and is a cosmetic no-op (HP/tag steps still run defensively).
	FName SeveredBone = NAME_None;
	if (const UAFLDismemberZoneSet* Set = ZoneSet.Get())
	{
		for (const FAFLDismemberZone& Row : Set->Zones)
		{
			if (Row.Zone == Zone)
			{
				SeveredBone = Row.SeveredBone;
				break;
			}
		}
	}

	// (1) S4-INC4 (AFL-0408): the un-hide is now ALL-CLIENT via REMOVING the persistent cue. RestoreZone
	// no longer un-hides directly -- it RemoveGameplayCue's GameplayCue.Combat.Dismember.State.<Zone>
	// (server-auth); GAS drops it from the replicated container -> UAFLCueNotify_ZoneSever::OnRemove runs
	// ApplyZoneRestoreCosmetic (GatherZoneMeshes + UnHideBoneByName) on EVERY client. Symmetric with the
	// SeverZone AddGameplayCue -> sever-set == restore-set by construction (same cue, same cosmetic path).
	if (UAbilitySystemComponent* RestoreASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
	{
		const FGameplayTag SeverCueTag = ZoneSeverCueTag(Zone);
		if (SeverCueTag.IsValid())
		{
			RestoreASC->RemoveGameplayCue(SeverCueTag);
			UE_LOG(LogAFLDismember, Display,
				TEXT("[AFLDismember] %s RestoreZone zone=%d -> RemoveGameplayCue %s (server-auth; all-client un-hide via cue)"),
				*GetNameSafe(Owner), static_cast<int32>(Zone), *SeverCueTag.ToString());
		}
	}

	// (2) Remove the zone state tag (State.Dismembered.<Zone> / State.Decapitated). Removing the GE
	//     that grants it reverts any attribute modifier too (e.g. arm RecoilMultiplier x1.5); then
	//     clear any loose copy. For the head, clearing State.Decapitated auto-pops the decap camera
	//     mode (tag-driven, the push wiring lands in PHASE B-2).
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	const FGameplayTag StateTag = ZoneStateTag(Zone);
	if (ASC && StateTag.IsValid())
	{
		FGameplayEffectQuery Query =
			FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(StateTag));
		ASC->RemoveActiveEffects(Query);
		ASC->RemoveLooseGameplayTag(StateTag);
	}

	// (3) Restore the zone-HP to full via the ZoneRestoreEffect GE (the same Override values the
	//     InitData GE seeds -- data-driven, not hardcoded twice). Async-load, then apply.
	if (ASC && !ZoneRestoreEffect.IsNull())
	{
		FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
		const FSoftObjectPath GEPath = ZoneRestoreEffect.ToSoftObjectPath();
		TWeakObjectPtr<UAFLDismemberComponent> WeakThis(this);
		TWeakObjectPtr<UAbilitySystemComponent> WeakASC(ASC);
		Streamable.RequestAsyncLoad(GEPath, FStreamableDelegate::CreateLambda([WeakThis, WeakASC, GEPath]()
		{
			UAFLDismemberComponent* Self = WeakThis.Get();
			UAbilitySystemComponent* RASC = WeakASC.Get();
			if (!Self || !RASC || !Self->GetOwner() || !Self->GetOwner()->HasAuthority())
			{
				return;
			}
			if (UClass* GEClass = Cast<UClass>(GEPath.ResolveObject()))
			{
				FGameplayEffectContextHandle Context = RASC->MakeEffectContext();
				Context.AddSourceObject(Self);
				const FGameplayEffectSpecHandle Spec = RASC->MakeOutgoingSpec(GEClass, 1.0f, Context);
				if (Spec.IsValid())
				{
					RASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
				}
			}
		}));
	}

	UE_LOG(LogAFLDismember, Display,
		TEXT("[AFLDismember] RestoreZone zone=%d bone=%s on %s -- reattached (cue removed -> all-client un-hide, tag cleared, HP restoring)"),
		static_cast<int32>(Zone), *SeveredBone.ToString(), *GetNameSafe(Owner));
}

void UAFLDismemberComponent::ApplyDecapitatedEffect()
{
	AActor* Owner = GetOwner();
	UAbilitySystemComponent* ASC =
		Owner ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner) : nullptr;
	if (!ASC || DecapitatedEffect.IsNull())
	{
		UE_LOG(LogAFLDismember, Warning,
			TEXT("[AFLDismember] head sever on %s but no DecapitatedEffect/ASC -- State.Decapitated NOT granted (camera will not push)"),
			*GetNameSafe(Owner));
		return;
	}

	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	const FSoftObjectPath GEPath = DecapitatedEffect.ToSoftObjectPath();
	TWeakObjectPtr<UAFLDismemberComponent> WeakThis(this);
	TWeakObjectPtr<UAbilitySystemComponent> WeakASC(ASC);
	Streamable.RequestAsyncLoad(GEPath, FStreamableDelegate::CreateLambda([WeakThis, WeakASC, GEPath]()
	{
		UAFLDismemberComponent* Self = WeakThis.Get();
		UAbilitySystemComponent* DASC = WeakASC.Get();
		if (!Self || !DASC || !Self->GetOwner() || !Self->GetOwner()->HasAuthority())
		{
			return;
		}
		if (UClass* GEClass = Cast<UClass>(GEPath.ResolveObject()))
		{
			FGameplayEffectContextHandle Context = DASC->MakeEffectContext();
			Context.AddSourceObject(Self);
			const FGameplayEffectSpecHandle Spec = DASC->MakeOutgoingSpec(GEClass, 1.0f, Context);
			if (Spec.IsValid())
			{
				DASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
				UE_LOG(LogAFLDismember, Display,
					TEXT("[AFLDismember] State.Decapitated granted to %s -- decap camera ability will activate"),
					*GetNameSafe(Self->GetOwner()));
			}
		}
	}));
}

void UAFLDismemberComponent::SpawnHeadLootBox()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !GetWorld() || HeadLootBoxClass.IsNull())
	{
		UE_LOG(LogAFLDismember, Warning,
			TEXT("[AFLDismember] head sever on %s but no HeadLootBoxClass -- no retrievable head spawned"),
			*GetNameSafe(Owner));
		return;
	}

	// Spawn at the head bone's world transform (where the cosmetic head prop also spawned). The owner
	// pawn is the retrieve target + death source for Initialize.
	FTransform SpawnXform = Owner->GetActorTransform();
	if (const USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>())
	{
		const FName HeadBone(TEXT("head"));
		if (SkelMesh->DoesSocketExist(HeadBone) || SkelMesh->GetBoneIndex(HeadBone) != INDEX_NONE)
		{
			SpawnXform = SkelMesh->GetSocketTransform(HeadBone, RTS_World);
		}
	}

	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	const FSoftObjectPath LootPath = HeadLootBoxClass.ToSoftObjectPath();
	TWeakObjectPtr<UAFLDismemberComponent> WeakThis(this);
	Streamable.RequestAsyncLoad(LootPath, FStreamableDelegate::CreateLambda([WeakThis, LootPath, SpawnXform]()
	{
		UAFLDismemberComponent* Self = WeakThis.Get();
		if (!Self || !Self->GetOwner() || !Self->GetOwner()->HasAuthority() || !Self->GetWorld())
		{
			return;
		}
		UClass* LootClass = Cast<UClass>(LootPath.ResolveObject());
		if (!LootClass)
		{
			UE_LOG(LogAFLDismember, Warning, TEXT("[AFLDismember] HeadLootBoxClass failed to load -- no head loot-box"));
			return;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.Owner = Self->GetOwner();

		AAFLHeadLootBox* Loot =
			Self->GetWorld()->SpawnActor<AAFLHeadLootBox>(LootClass, SpawnXform, SpawnParams);
		if (Loot)
		{
			// Tie it to the pawn it came from: self-retrieve reattaches, owner-death vanishes it.
			Loot->Initialize(Cast<APawn>(Self->GetOwner()));

			// IDENTITY: hand the victim's replicated skin color to the head so it reads as WHOSE head it
			// is (pink robot -> pink head). Server-set -> replicates to all clients via the head's OnRep.
			if (const UAFLSkinColorComponent* SCC =
					Self->GetOwner()->FindComponentByClass<UAFLSkinColorComponent>())
			{
				Loot->SetHeadSkinColor(SCC->GetSkinColor());
			}

			// IDENTITY (layer 2 -- MATERIAL): hand the victim's per-skin HEAD MATERIAL (slot-1 base MIC) to the
			// gib so it looks like that specific robot's head. Recover the REPLICATION-SAFE MIC, never the
			// transient runtime MID: the skin system MID-ifies slot 1 at runtime, so GetMaterial(1) returns a
			// UMaterialInstanceDynamic whose ->Parent IS the per-skin MIC (e.g. MI_AFL_FaceMask_Pink). Read it
			// off the VISIBLE robot mesh (the CharacterPart SKM_Manny, gathered by GatherZoneMeshes; the
			// invisible CharacterMesh0 anim driver has no 2nd slot / no skin material, so it is skipped).
			{
				UMaterialInstanceConstant* HeadMIC = nullptr;
				for (USkeletalMeshComponent* Mesh : Self->GatherZoneMeshes())
				{
					if (!Mesh || Mesh->GetNumMaterials() < 2)
					{
						continue;   // invisible CharacterMesh0 driver / any 1-slot mesh -> not the robot head
					}
					UMaterialInterface* M = Mesh->GetMaterial(1);   // slot 1 = M_HeadLegs (the head region)
					UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(M);
					if (!MIC)
					{
						if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(M))
						{
							MIC = Cast<UMaterialInstanceConstant>(MID->Parent);   // MID -> its source MIC
						}
					}
					if (MIC)
					{
						HeadMIC = MIC;
						break;   // first visible robot mesh wins
					}
				}
				if (HeadMIC)
				{
					Loot->SetHeadMaterial(HeadMIC);
				}
				else
				{
					// Fallback: leave the gib default material (still spawns/rolls/grabs; color params still drive).
					UE_LOG(LogAFLDismember, Warning,
						TEXT("[AFL_HEADGIB] %s: could not resolve victim slot-1 head MIC -- gib uses default material"),
						*GetNameSafe(Self->GetOwner()));
				}
			}

			// S4 TUMBLE TIER 2: pop the head so it ROLLS, not just free-falls. The loot-box path never applied
			// an impulse (unlike SeverZone's limb props) -- the head dropped ~150cm from the neck and settled.
			// Bias the impulse LATERAL: a randomized horizontal direction at HeadPopLateralImpulse drives the
			// head-sized sphere to roll ~1m before resting; a modest HeadPopVerticalImpulse gives it a small
			// pop off the neck so it arcs out instead of dropping straight down. Physics-driven (no anim);
			// AddImpulse on the sphere PartMesh via the inherited ApplyPopImpulse (the head's restored root).
			FVector LateralDir = FVector(FMath::VRand());
			LateralDir.Z = 0.f;                                  // horizontal only -> a true roll bias
			LateralDir = LateralDir.GetSafeNormal2D();
			if (LateralDir.IsNearlyZero())                       // VRand landed near-vertical -> pick a default
			{
				LateralDir = FVector(1.f, 0.f, 0.f);
			}
			const FVector HeadPop =
				LateralDir * Self->HeadPopLateralImpulse + FVector(0.f, 0.f, Self->HeadPopVerticalImpulse);
			Loot->ApplyPopImpulse(HeadPop);

			UE_LOG(LogAFLDismember, Display,
				TEXT("[AFLDismember] head loot-box %s spawned + tied to owner %s -- pop+roll impulse=%s"),
				*GetNameSafe(Loot), *GetNameSafe(Self->GetOwner()), *HeadPop.ToString());
		}
	}));
}
