// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDismemberedHead.h"

#include "AFLDismember.h"
#include "Components/StaticMeshComponent.h"
#include "Cosmetics/AFLSkinColorAsset.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDismemberedHead)

AAFLDismemberedHead::AAFLDismemberedHead()
{
	// S4 REAL-HEAD-GIB (PHASE 3): the head prop IS the base PartMesh (UStaticMeshComponent) -- the proven
	// prim-as-root physics body the grab + replication were built around -- showing the VICTIM's actual head
	// (SM_AFL_RobotHead_Gib, origin-centered head-only static mesh + convex collision). The base ctor already
	// set SetSimulatePhysics(true) + PhysicsActor + bReplicates + SetReplicateMovement on PartMesh; we only set
	// the gib mesh on it. No skeletal mesh, no bone-isolate, no centering offset (the gib origin IS its center).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> GibFinder(
		TEXT("/Game/BagMan/Characters/Dismember/SM_AFL_RobotHead_Gib.SM_AFL_RobotHead_Gib"));
	if (GibFinder.Succeeded())
	{
		HeadGibMesh = GibFinder.Object;
	}
	if (UStaticMeshComponent* Body = GetPartMesh())
	{
		if (HeadGibMesh)
		{
			Body->SetStaticMesh(HeadGibMesh);
		}
		// The gib's imported convex hull is the sim shape (auto_collision). Use it for simulation + query so the
		// head collides + rolls; keep the base's PhysicsActor profile. A real-head-sized convex at default density
		// gives a sane mass (no scale^3 blow-up like the engine sphere had), so no MassInKg override needed.
		Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	}

	// AFL-0404: default-load the rolling-head audio so the slice ships without BP wiring.
	static ConstructorHelpers::FObjectFinder<USoundBase> RollSoundFinder(
		TEXT("/Game/Effects/Audio_Sound_Effects/Head_Roll_1.Head_Roll_1"));
	if (RollSoundFinder.Succeeded())
	{
		RollSound = RollSoundFinder.Object;
	}
}

void AAFLDismemberedHead::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Both identity layers replicate as content-asset pointers -> OnRep applies on every client + late-joiner.
	DOREPLIFETIME(AAFLDismemberedHead, HeadSkinColor);
	DOREPLIFETIME(AAFLDismemberedHead, HeadMaterial);
}

void AAFLDismemberedHead::BeginPlay()
{
	Super::BeginPlay();

	// Apply whatever identity already replicated (server set-then-spawn, or a late-join client that received the
	// replicated values before BeginPlay). Each OnRep re-runs the apply so the look completes whenever both land.
	ApplyHeadAppearance();

	// AFL-0404: roll audio attached to the gib so it follows the tumble.
	if (RollSound)
	{
		if (UStaticMeshComponent* Body = GetPartMesh())
		{
			UGameplayStatics::SpawnSoundAttached(RollSound, Body);
		}
	}
}

void AAFLDismemberedHead::SetHeadSkinColor(UAFLSkinColorAsset* InColor)
{
	// Server-authority: set the replicated value; replication -> OnRep on clients -> apply. Apply locally too
	// (the server is a client on a listen-server and gets no OnRep for its own write).
	if (!HasAuthority())
	{
		return;
	}
	HeadSkinColor = InColor;
	ApplyHeadAppearance();
}

void AAFLDismemberedHead::SetHeadMaterial(UMaterialInstanceConstant* InMaterial)
{
	// Server-authority: mirror SetHeadSkinColor. InMaterial is the victim's per-skin slot-1 base MIC (a content
	// asset -> replicates safely); UAFLDismemberComponent resolved it from GetMaterial(1)->Parent at spawn.
	if (!HasAuthority())
	{
		return;
	}
	HeadMaterial = InMaterial;
	ApplyHeadAppearance();
}

void AAFLDismemberedHead::OnRep_HeadSkinColor()
{
	ApplyHeadAppearance();
}

void AAFLDismemberedHead::OnRep_HeadMaterial()
{
	ApplyHeadAppearance();
}

void AAFLDismemberedHead::ApplyHeadAppearance()
{
	// Runs on every client. TWO layers, mirroring the live robot's head exactly:
	//   1) assign the victim's base MIC (HeadMaterial) to BOTH gib slots (the head geometry is split across
	//      sections 0+1 -> tinting only one would leave a two-tone head); then
	//   2) drive the color params on TOP via the proven AAFLCharacterPartActor::ApplySkinColor pattern --
	//      per-slot CreateAndSetMaterialInstanceDynamic, then SetScalar/Vector/Texture from the color asset.
	// OnRep ORDERING: HeadMaterial and HeadSkinColor replicate independently; this is re-run on each OnRep, so
	// whichever lands second completes the look. Each layer is independently guarded (null -> skip that layer),
	// so a partial arrival shows the best available state and the second OnRep fills the rest. Idempotent.
	UStaticMeshComponent* Body = GetPartMesh();
	if (!Body)
	{
		return;
	}

	const int32 NumSlots = Body->GetNumMaterials();

	// (1) Base material: assign the victim MIC to every slot so the whole gib reads as that robot's head.
	if (HeadMaterial)
	{
		for (int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex)
		{
			Body->SetMaterial(SlotIndex, HeadMaterial);
		}
	}

	// (2) Color params on top (the identity tint). Require BOTH: the color params are M_Mannequin params that only
	// mean anything over the MIC base -- tinting them over the gib's default WorldGridMaterial (material not yet
	// replicated) is a no-op, so wait for the MIC. The two-OnRep ordering resolves here: material-first shows the
	// untinted MIC then this adds the tint; color-first shows the default mesh then the material OnRep brings the
	// MIC and re-runs this to tint. End state identical either way.
	if (HeadMaterial && HeadSkinColor)
	{
		for (int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex)
		{
			UMaterialInstanceDynamic* MID = Body->CreateAndSetMaterialInstanceDynamic(SlotIndex);
			if (!MID)
			{
				continue;
			}
			for (const TPair<FName, float>& KV : HeadSkinColor->GetScalars())
			{
				MID->SetScalarParameterValue(KV.Key, KV.Value);
			}
			for (const TPair<FName, FLinearColor>& KV : HeadSkinColor->GetColors())
			{
				MID->SetVectorParameterValue(KV.Key, FVector(KV.Value));
			}
			for (const TPair<FName, TObjectPtr<UTexture>>& KV : HeadSkinColor->GetTextures())
			{
				MID->SetTextureParameterValue(KV.Key, KV.Value);
			}
		}
	}

	UE_LOG(LogAFLDismember, Display,
		TEXT("[AFLDismember] head gib %s: appearance applied (material=%s color=%s slots=%d)"),
		*GetName(), *GetNameSafe(HeadMaterial), *GetNameSafe(HeadSkinColor), NumSlots);
}

void AAFLDismemberedHead::ApplyPopImpulse(const FVector& Linear, const FVector& Angular)
{
	// S4 TUMBLE FIX (velocity, not force): pop the gib PartMesh (the head's physics body) with bVelChange=TRUE
	// so the vector is a TARGET VELOCITY (mass/scale-independent), not a force the base divides by mass. Pops the
	// gib, NOT a cosmetic child -- the gib IS the body. PRESENTATION PASS: + a modest angular impulse (also a
	// velocity-change) so the head TUMBLES end-over-end as it travels (a real roll, not a slide).
	if (UStaticMeshComponent* Body = GetPartMesh())
	{
		Body->AddImpulse(Linear, NAME_None, /*bVelChange=*/true);
		if (!Angular.IsNearlyZero())
		{
			Body->AddAngularImpulseInRadians(Angular, NAME_None, /*bVelChange=*/true);
		}
	}
}
