// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLAG_Laser_Base.h"

#include "AFLCombat.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Equipment/LyraEquipmentInstance.h"   // side-scoped resolver: GetSpawnedActors()
#include "GameFramework/Character.h"
#include "NativeGameplayTags.h"

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
