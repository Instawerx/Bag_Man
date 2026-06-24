// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"

#include "AFLAG_Laser_Base.generated.h"

class APawn;

/**
 * UAFLAG_Laser_Base
 *
 * Shared base for the AFL laser abilities -- UAFLAG_Laser_Pulse (single-shot
 * hitscan) and UAFLAG_Laser_Beam (held channel). It owns the ONE muzzle
 * resolver both abilities call, folding what used to be a verbatim copy
 * (AFL-0208 lifted Pulse's ResolveMuzzleLocation into Beam by hand) into a
 * single inherited implementation -- so no third laser weapon can grow a
 * fourth twin. Subclasses keep their own fire logic and their own per-weapon
 * CharacterFireMontage; only the muzzle resolution + socket-candidate policy
 * live here.
 *
 * Abstract: never granted directly. The granted abilities are the Pulse/Beam
 * subclasses (or their BP children).
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLAG_Laser_Base : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:

	UAFLAG_Laser_Base();

protected:

	/**
	 * Ordered muzzle socket-name candidates. ResolveMuzzleLocation returns the world location of the
	 * FIRST of these that exists on the avatar's attached weapon mesh. Default {"Muzzle","Barrel",
	 * "Slide"} spans the AFL roster: Rifle/Carbine + Shotgun author a "Muzzle" socket; the Pistol has
	 * none (its barrel-tip socket is "Barrel"/"Slide"). One ordered list resolves every weapon, and a
	 * future mesh that names its barrel-tip socket differently just adds a name here -- never a
	 * per-ability resolver fork. "Muzzle" stays first so Rifle/Carbine/Shotgun resolve exactly as
	 * before this list existed (zero behaviour change to the proven weapons).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Laser|FX")
	TArray<FName> MuzzleSocketCandidates;

	/**
	 * Resolve the weapon MUZZLE world location off the avatar's attached weapon. Walks the pawn's
	 * (recursively) attached actors and the character mesh's descendant components for the first
	 * UMeshComponent carrying any MuzzleSocketCandidates socket -- UMeshComponent is the shared base of
	 * BOTH USkeletalMeshComponent and UStaticMeshComponent, so it covers the harvest-clone SKELETAL
	 * weapons (SK_Rifle / SK_Pistol / SKM_Shotgun) as well as the older static beam mesh. Falls back to
	 * the weapon_r hand socket (NEVER world origin). Logs the resolved socket + world offset at Verbose
	 * (AFL_LASER/MUZZLE) -- flip to Log to confirm the barrel-tip distance in PIE, how 6e9f2d21 proved
	 * the rifle muzzle at +69.8cm. Shared by Pulse (Fire/Tracer cues) and Beam (PublishMuzzle each tick).
	 */
	FVector ResolveMuzzleLocation(APawn* AvatarPawn) const;
};
