// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDismemberedLimb.h"

#include "AFLDismember.h"
#include "Components/StaticMeshComponent.h"
#include "Cosmetics/AFLSkinColorAsset.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDismemberedLimb)

AAFLDismemberedLimb::AAFLDismemberedLimb()
{
	// The limb prop IS the base PartMesh (UStaticMeshComponent) -- the base ctor already set
	// SetSimulatePhysics(true) + PhysicsActor + bReplicates + SetReplicateMovement on it. We only set
	// the gib mesh when one is assigned (the per-limb BP child sets LimbGibMesh; until the extracted
	// SM_AFL_RobotArm_Gib / SM_AFL_RobotLeg_Gib data exists, a placeholder rides). No velocity-pop
	// override (limbs keep the base force-pop) and no roll-audio default (head-specific). Mirrors the
	// head ctor's mesh-assign, minus those two head-only bits.
	if (UStaticMeshComponent* Body = GetPartMesh())
	{
		if (LimbGibMesh)
		{
			Body->SetStaticMesh(LimbGibMesh);
		}
		Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	}
}

void AAFLDismemberedLimb::ApplyPopImpulse(const FVector& Impulse)
{
	// TUMBLE FIX (velocity, not force) -- IDENTICAL to AAFLDismemberedHead::ApplyPopImpulse. bVelChange=TRUE
	// makes the vector a TARGET VELOCITY (mass/scale-independent) rather than a force the base divides by the
	// gib's (small) mass. Watched in PIE: the base force-pop launched the light limb gib off-screen with no
	// visible tumble; this gives the same gentle pop+tumble the head gib gets. Pops the gib PartMesh (the gib
	// IS the physics body), not a cosmetic child.
	if (UStaticMeshComponent* Body = GetPartMesh())
	{
		Body->AddImpulse(Impulse, NAME_None, /*bVelChange=*/true);
	}
}

void AAFLDismemberedLimb::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Both identity layers replicate as content-asset pointers -> OnRep applies on every client + late-joiner.
	// MIRRORS AAFLDismemberedHead.
	DOREPLIFETIME(AAFLDismemberedLimb, PartSkinColor);
	DOREPLIFETIME(AAFLDismemberedLimb, PartMaterial);
}

void AAFLDismemberedLimb::BeginPlay()
{
	Super::BeginPlay();

	// Apply whatever identity already replicated (server set-then-spawn, or a late-join client that received
	// the replicated values before BeginPlay). Each OnRep re-runs the apply so the look completes whenever both
	// land. MIRRORS AAFLDismemberedHead::BeginPlay (no roll audio).
	ApplyLimbAppearance();
}

void AAFLDismemberedLimb::SetPartSkinColor(UAFLSkinColorAsset* InColor)
{
	// Server-authority: set the replicated value; replication -> OnRep on clients -> apply. Apply locally too
	// (the server is a client on a listen-server and gets no OnRep for its own write). MIRRORS the head.
	if (!HasAuthority())
	{
		return;
	}
	PartSkinColor = InColor;
	ApplyLimbAppearance();
}

void AAFLDismemberedLimb::SetPartMaterial(UMaterialInstanceConstant* InMaterial)
{
	// Server-authority: mirror SetPartSkinColor. InMaterial is the victim's per-skin slot-1 base MIC (a content
	// asset -> replicates safely); UAFLDismemberComponent resolved it from GetMaterial(1)->Parent at spawn.
	if (!HasAuthority())
	{
		return;
	}
	PartMaterial = InMaterial;
	ApplyLimbAppearance();
}

void AAFLDismemberedLimb::OnRep_PartSkinColor()
{
	ApplyLimbAppearance();
}

void AAFLDismemberedLimb::OnRep_PartMaterial()
{
	ApplyLimbAppearance();
}

void AAFLDismemberedLimb::ApplyLimbAppearance()
{
	// Runs on every client. TWO layers, mirroring the live robot exactly (and AAFLDismemberedHead::ApplyHeadAppearance):
	//   1) assign the victim's base MIC (PartMaterial) to EVERY slot; then
	//   2) drive the color params on TOP via the proven AAFLCharacterPartActor::ApplySkinColor pattern --
	//      per-slot CreateAndSetMaterialInstanceDynamic, then SetScalar/Vector/Texture from the color asset.
	// OnRep ORDERING: PartMaterial + PartSkinColor replicate independently; this re-runs on each OnRep so whichever
	// lands second completes the look. Each layer is null-guarded so a partial arrival shows the best available
	// state and the second OnRep fills the rest. Idempotent.
	UStaticMeshComponent* Body = GetPartMesh();
	if (!Body)
	{
		return;
	}

	const int32 NumSlots = Body->GetNumMaterials();

	// (1) Base material: assign the victim MIC to every slot so the whole gib reads as that robot's limb.
	if (PartMaterial)
	{
		for (int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex)
		{
			Body->SetMaterial(SlotIndex, PartMaterial);
		}
	}

	// (2) Color params on top. Require BOTH: the color params are M_Mannequin params that only mean anything over
	// the MIC base -- tinting them over the gib's default material is a no-op, so wait for the MIC. The two-OnRep
	// ordering resolves here, identical end state either order. MIRRORS the head.
	if (PartMaterial && PartSkinColor)
	{
		for (int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex)
		{
			UMaterialInstanceDynamic* MID = Body->CreateAndSetMaterialInstanceDynamic(SlotIndex);
			if (!MID)
			{
				continue;
			}
			for (const TPair<FName, float>& KV : PartSkinColor->GetScalars())
			{
				MID->SetScalarParameterValue(KV.Key, KV.Value);
			}
			for (const TPair<FName, FLinearColor>& KV : PartSkinColor->GetColors())
			{
				MID->SetVectorParameterValue(KV.Key, FVector(KV.Value));
			}
			for (const TPair<FName, TObjectPtr<UTexture>>& KV : PartSkinColor->GetTextures())
			{
				MID->SetTextureParameterValue(KV.Key, KV.Value);
			}
		}
	}

	UE_LOG(LogAFLDismember, Display,
		TEXT("[AFLDismember] limb gib %s: appearance applied (material=%s color=%s slots=%d)"),
		*GetName(), *GetNameSafe(PartMaterial), *GetNameSafe(PartSkinColor), NumSlots);
}
