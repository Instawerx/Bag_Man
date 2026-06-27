// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLAG_Laser_Base.h"

#include "AFLCombat.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAG_Laser_Base)

UAFLAG_Laser_Base::UAFLAG_Laser_Base()
{
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
