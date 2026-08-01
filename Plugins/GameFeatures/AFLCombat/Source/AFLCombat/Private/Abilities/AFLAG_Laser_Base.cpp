// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLAG_Laser_Base.h"

#include "AFLCombat.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Equipment/LyraEquipmentInstance.h"   // side-scoped resolver: GetSpawnedActors()
#include "GameFramework/Character.h"
#include "NativeGameplayTags.h"
#include "Weapons/LyraWeaponInstance.h"        // UpdateFiringTime() -- weapon-state hygiene (not the aim driver)

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAG_Laser_Base)

// EMP DISABLE (map-play EMP device, block-fire): the AAFLEMPDevice pulse applies a GE granting this
// tag to caught enemies. It sits in EVERY AFL weapon's ActivationBlockedTags because the whole AFL
// roster (Pulse/Beam/Charge/Hitscan trio/Projectile Rocket+Seeker/Deployable Shield+EMP) descends
// from THIS base -- so one AddTag here disables the entire roster while a disabled pawn's movement/
// dash (separate abilities) stay free to flee. Same shape as the SMG overheat lockout, turned on
// enemies. Native-registered (no ini needed). NOTE: the stock Lyra weapons (Rifle/Pistol/Shotgun via
// ULyraGameplayAbility_RangedWeapon) do NOT descend from here -- to disable those too, add this same
// tag to their fire GA's ActivationBlockedTags in DATA (operator decision, flagged at scope).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Weapon_Disabled_LB, "State.Weapon.Disabled");

UAFLAG_Laser_Base::UAFLAG_Laser_Base()
{
	// EMP disable is a roster-wide fire lockout -- see the tag comment above. Subclass ctors AddTag
	// their own blocked tags (Carrying/ThrowRecovery/Warmup/Ended/Overheated) ON TOP of this, so the
	// container ends up with Disabled + the per-ability set (AddTag appends; the base runs first).
	ActivationBlockedTags.AddTag(TAG_State_Weapon_Disabled_LB);

	// Ordered muzzle socket candidates, first existing wins. "Muzzle" first keeps Rifle/Carbine +
	// Shotgun resolving exactly as the old hardcoded FName("Muzzle") did (zero change to proven
	// weapons); "Barrel"/"Slide" cover the Pistol, which has no "Muzzle" socket. A future mesh with a
	// new front-socket name just appends here -- the resolver is shared, so both Pulse and Beam (and
	// any later laser) pick it up with no code edit.
	MuzzleSocketCandidates = { FName("Muzzle"), FName("Barrel"), FName("Slide") };
}

void UAFLAG_Laser_Base::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Stamp "last fired" on the equipped weapon instance BEFORE the shot runs, so the AnimBP's
	// HipFireUpperBodyOverrideWeight raises the upper body to aim for RaiseWeaponAfterFiringDuration after
	// each shot -- this is what makes firing-while-running snap the weapon up instead of staying at the
	// relaxed aim-offset. The spec SourceObject IS the equipment/weapon instance (the WID AbilitySet grant
	// sets it; same handle ResolveLaserVisualProvider reads). Guarded so bot GameplayEvent / activate-by-class
	// fire paths with no weapon instance still activate. Runs per shot (each sustained-fire shot re-activates)
	// on the owning client + server; mirrors ULyraGameplayAbility_RangedWeapon::ActivateAbility.
	// Keep the equipped weapon instance's "last fired" time honest (ULyraWeaponInstance::UpdateFiringTime),
	// mirroring ULyraGameplayAbility_RangedWeapon::ActivateAbility. NOTE: run-and-shoot's upper-body raise is
	// driven by the State.Firing.* tag path in the AnimBP's GameplayTagPropertyMap (GameplayTag_IsFiring), NOT
	// by this stamp -- the AnimBP self-accumulates TimeSinceFiredWeapon and never reads the weapon instance. So
	// this is weapon hygiene, not the aim driver. It STAYS because GetTimeSinceLastInteractedWith() feeds other
	// consumers (idle-break timing, telemetry) and a clean rebuild carrying this stamp is what cleared the
	// "weapon dead on first spawn until you cycle" bug -- do not drop it. One site covers the whole laser tree
	// (every subclass calls Super::ActivateAbility; each sustained-fire shot is its own activation).
	if (const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec())
	{
		if (ULyraWeaponInstance* Weapon = Cast<ULyraWeaponInstance>(Spec->SourceObject.Get()))
		{
			Weapon->UpdateFiringTime();
		}
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

FVector UAFLAG_Laser_Base::ResolveMuzzleLocation(APawn* AvatarPawn) const
{
	// Fallback to the weapon_r hand socket so the resolved muzzle is NEVER world origin (un-armed, or
	// a weapon whose mesh authors none of the candidate sockets). Worst case the FX emits from the
	// hand -- never vanishes, never shoots from origin. This is the same safety the Pulse/Beam copies
	// had; it stays.
	FVector MuzzleLocation = FVector::ZeroVector;
	if (!AvatarPawn)
	{
		return MuzzleLocation;
	}

	if (ACharacter* AvatarChar = Cast<ACharacter>(AvatarPawn))
	{
		if (USkeletalMeshComponent* CharMesh = AvatarChar->GetMesh())
		{
			MuzzleLocation = CharMesh->GetSocketLocation(FName("weapon_r"));
		}
	}

	// First candidate socket that exists on a given mesh component. UMeshComponent::DoesSocketExist /
	// GetSocketLocation live on the primitive, so they resolve mesh sockets, skeleton sockets, AND bone
	// names on either a skeletal or a static mesh. Logs the resolved socket + world offset so the
	// barrel-tip distance can be confirmed in PIE (Verbose -> Log), exactly how 6e9f2d21 confirmed the
	// rifle muzzle at +69.8cm.
	auto TryMesh = [this, &MuzzleLocation](UMeshComponent* MeshComp) -> bool
	{
		if (!MeshComp)
		{
			return false;
		}
		for (const FName& SocketName : MuzzleSocketCandidates)
		{
			if (!SocketName.IsNone() && MeshComp->DoesSocketExist(SocketName))
			{
				MuzzleLocation = MeshComp->GetSocketLocation(SocketName);
				UE_LOG(LogAFLCombat, Verbose,
					TEXT("AFL_LASER/MUZZLE: resolved socket '%s' on %s at world=%s"),
					*SocketName.ToString(), *MeshComp->GetName(), *MuzzleLocation.ToString());
				return true;
			}
		}
		return false;
	};

	// Path A: pawn->GetAttachedActors (root-attached weapons), RECURSIVE -- the harvest-clone display
	// actors nest their mesh a level down. UMeshComponent covers static AND skeletal.
	TArray<AActor*> AttachedActors;
	AvatarPawn->GetAttachedActors(AttachedActors, /*bResetArray=*/true, /*bRecursivelyIncludeAttachedActors=*/true);
	for (AActor* Attached : AttachedActors)
	{
		TInlineComponentArray<UMeshComponent*> MeshComps;
		Attached->GetComponents<UMeshComponent>(MeshComps);
		for (UMeshComponent* MeshComp : MeshComps)
		{
			if (TryMesh(MeshComp))
			{
				return MuzzleLocation;
			}
		}
	}

	// Path B: Lyra equipment attaches the weapon to Char->GetMesh() (not the pawn root), so walk the
	// character mesh's descendant components too.
	if (ACharacter* AvatarChar = Cast<ACharacter>(AvatarPawn))
	{
		if (USkeletalMeshComponent* CharMesh = AvatarChar->GetMesh())
		{
			TArray<USceneComponent*> MeshChildren;
			CharMesh->GetChildrenComponents(/*bIncludeAllDescendants=*/true, MeshChildren);
			for (USceneComponent* Child : MeshChildren)
			{
				if (UMeshComponent* MeshComp = Cast<UMeshComponent>(Child))
				{
					if (TryMesh(MeshComp))
					{
						return MuzzleLocation;
					}
				}
			}
		}
	}

	UE_LOG(LogAFLCombat, Verbose,
		TEXT("AFL_LASER/MUZZLE: no candidate socket resolved -> weapon_r fallback at world=%s"),
		*MuzzleLocation.ToString());
	return MuzzleLocation;
}

FVector UAFLAG_Laser_Base::ResolveMuzzleLocation(UObject* SourceEquipment, APawn* AvatarPawn) const
{
	// SIDE-SCOPED overload (dual arm-cannons). The pawn-scoped sibling above returns the FIRST
	// candidate socket found across EVERY attached mesh -- correct while exactly one weapon is held,
	// and silently wrong the moment a second one is: both cannons' abilities resolved the SAME first
	// match, so the left cannon fired from the right cannon's barrel. This overload confines the search
	// to the actors THIS equipment instance spawned (ULyraEquipmentInstance::GetSpawnedActors), so each
	// side resolves its own muzzle no matter what else is attached to the pawn.
	//
	// Callers pass ResolveLaserVisualProvider() -- the spec's SourceObject, which the WID AbilitySet
	// grant already sets to the equipment instance. No reparent to ULyraGameplayAbility_FromEquipment:
	// that would impose an equipment requirement on the activate-by-class test path
	// (AFLMatchTestRunner) and the five GameplayEvent bot-fire triggers, which have no spec/source.
	if (const ULyraEquipmentInstance* Equipment = Cast<ULyraEquipmentInstance>(SourceEquipment))
	{
		for (AActor* Spawned : Equipment->GetSpawnedActors())
		{
			if (!IsValid(Spawned))
			{
				continue;
			}

			TInlineComponentArray<UMeshComponent*> MeshComps;
			Spawned->GetComponents<UMeshComponent>(MeshComps);
			for (UMeshComponent* MeshComp : MeshComps)
			{
				if (!MeshComp)
				{
					continue;
				}
				for (const FName& SocketName : MuzzleSocketCandidates)
				{
					if (!SocketName.IsNone() && MeshComp->DoesSocketExist(SocketName))
					{
						const FVector Resolved = MeshComp->GetSocketLocation(SocketName);
						UE_LOG(LogAFLCombat, Verbose,
							TEXT("AFL_LASER/MUZZLE(side): '%s' on %s (equip %s) at world=%s"),
							*SocketName.ToString(), *MeshComp->GetName(),
							*GetNameSafe(SourceEquipment), *Resolved.ToString());
						return Resolved;
					}
				}
			}

			// ⚠ DELIBERATE DEVIATION from "no fallback" -- flagged to the operator. A weapon whose mesh
			// authors no candidate socket would otherwise return ZeroVector and emit FX from WORLD
			// ORIGIN. Falling back to THIS equipment's own spawned-actor location keeps the failure
			// mode side-correct and on the pawn: never the other cannon's barrel (the bug being fixed),
			// never the shared weapon_r hand socket, never origin.
			UE_LOG(LogAFLCombat, Verbose,
				TEXT("AFL_LASER/MUZZLE(side): no candidate socket on %s -> its actor origin"),
				*Spawned->GetName());
			return Spawned->GetActorLocation();
		}
	}

	// No equipment resolved at all (activate-by-class, bot GameplayEvent, un-granted spec): fall through
	// to the proven pawn-scoped resolver so those paths behave EXACTLY as before this change.
	return ResolveMuzzleLocation(AvatarPawn);
}

FVector UAFLAG_Laser_Base::ResolveMuzzleLocationForActor(AActor* WeaponActor) const
{
	// PER-ACTOR form (XT: one equipment definition, two mounted actors). Deliberately looks at NOTHING
	// but the actor handed in -- no pawn walk, no equipment walk -- so it stays correct when called in a
	// loop over GetSpawnedActors() and can never return the other mount's barrel.
	if (!IsValid(WeaponActor))
	{
		return FVector::ZeroVector;
	}

	TInlineComponentArray<UMeshComponent*> MeshComps;
	WeaponActor->GetComponents<UMeshComponent>(MeshComps);
	for (UMeshComponent* MeshComp : MeshComps)
	{
		if (!MeshComp)
		{
			continue;
		}
		for (const FName& SocketName : MuzzleSocketCandidates)
		{
			if (!SocketName.IsNone() && MeshComp->DoesSocketExist(SocketName))
			{
				const FVector Resolved = MeshComp->GetSocketLocation(SocketName);
				UE_LOG(LogAFLCombat, Verbose,
					TEXT("AFL_LASER/MUZZLE(actor): '%s' on %s (%s) at world=%s"),
					*SocketName.ToString(), *MeshComp->GetName(), *WeaponActor->GetName(), *Resolved.ToString());
				return Resolved;
			}
		}
	}

	// Same deliberate deviation as the equipment-scoped resolver: this actor's own origin, so a mesh
	// with no candidate socket still emits from ITS mount rather than world origin or the other barrel.
	UE_LOG(LogAFLCombat, Verbose,
		TEXT("AFL_LASER/MUZZLE(actor): no candidate socket on %s -> its actor origin"), *WeaponActor->GetName());
	return WeaponActor->GetActorLocation();
}

UObject* UAFLAG_Laser_Base::ResolveLaserVisualProvider() const
{
	// The WID AbilitySet grant sets the spec's SourceObject to the equipment/weapon instance (which
	// implements IAFLLaserVisualProvider directly -- the BP weapon implements the interface). The FX cues cast it
	// and read GetBeamColor. Folds AFLAG_Laser_Beam::GetAFLLaserWeaponInstance's per-ability copy into
	// the shared base, exactly like ResolveMuzzleLocation -- one tint contract for every laser weapon.
	if (FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec())
	{
		return Spec->SourceObject.Get();
	}
	return nullptr;
}
