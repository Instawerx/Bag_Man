// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDismemberedHead.h"

#include "AFLDismember.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Cosmetics/AFLSkinColorAsset.h"
#include "Cosmetics/AFLSkinColorComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"        // S4 TUMBLE FIX: FObjectFinder<UStaticMesh> (sphere body) needs the complete type
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"   // S4 PHASE 0: FObjectFinder<UMaterialInterface> (BasicShapeMaterial) needs the complete type
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDismemberedHead)

AAFLDismemberedHead::AAFLDismemberedHead()
{
	// S4 TUMBLE FIX (Option 1): RESTORE the base AAFLDismemberedPart prim-as-root. The head ROLLS via the
	// base PartMesh (UStaticMeshComponent) -- the proven limb-prop physics shape the grab + replication were
	// built around -- given a small engine SPHERE. PartMesh stays the simulating root (the base ctor already
	// set SetSimulatePhysics + PhysicsActor + bReplicates + SetReplicateMovement on it); we only make it a
	// HIDDEN proxy + give it the sphere. A skeletal mesh as the simulating body was the bug: IsolateHeadBone's
	// HideBoneByName(PBO_Term) x163 terminated the PA_Mannequin bodies -> a one-body ragdoll can't roll.
	if (UStaticMeshComponent* SphereMesh = GetPartMesh())
	{
		// A SPHERE rolls naturally (a capsule tumbles awkwardly). The engine sphere is 100cm DIAMETER (50cm
		// radius) at scale 1; SphereBodyScale 0.21 -> ~21cm = head-sized. Simple sphere collision -> a clean
		// single rolling body.
		static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
			TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		if (SphereFinder.Succeeded())
		{
			SphereMesh->SetStaticMesh(SphereFinder.Object);
		}
		SphereMesh->SetWorldScale3D(FVector(SphereBodyScale));
		// PHASE 0: the engine sphere ships with DefaultMaterial (NO color param). Assign BasicShapeMaterial,
		// which exposes a "Color" vector param -> ApplyHeadSkinColor tints it with the victim's color. The
		// real extracted head (PHASE 3) drops this -- it carries the robot skin material itself.
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMatFinder(
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (BasicMatFinder.Succeeded())
		{
			SphereMesh->SetMaterial(0, BasicMatFinder.Object);
		}
		// S4 PHASE 0 PLACEHOLDER: the sphere is now the VISIBLE prop (tinted with the victim's skin color in
		// ApplyHeadSkinColor). The skeletal-head-isolate produced a SHARD -- on SK_Mannequin the skull verts are
		// skin-weighted across head+neck+spine, so HideBoneByName("all but head") collapses most of the face
		// (every vert with any neck weight) and leaves only the purely-head-weighted sliver. You CANNOT isolate a
		// head from a shared-weight skinned mesh by hiding siblings. The real head = a head static-mesh gib
		// extracted from SK_Manny (blender_mcp, PHASE 1-4); until then this honest colored ball rolls/grabs/
		// reattaches with the full proven loop. The base ctor set sim/collision/replication on this body already.
		SphereMesh->SetHiddenInGame(false);
		SphereMesh->SetVisibility(true);
	}

	// The SK_Manny head visual is DORMANT for the PHASE 0 placeholder: kept (the prop/color/fit code is reused
	// by the real extracted head in PHASE 3) but HIDDEN, so the shard never renders. The sphere is the visible
	// placeholder. NON-simulating + no collision regardless -- the sphere is the physics body.
	HeadMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(GetPartMesh());           // child of the sphere root -> follows the tumble
	HeadMesh->SetSimulatePhysics(false);                // cosmetic only -- the sphere is the physics body
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // no collision contest with the sphere
	HeadMesh->SetVisibility(false);                     // PHASE 0: dormant (shard-producing isolate not shown)

	// Data-driven default mesh -- BP child may override; the ctor sets SKM_Manny so the slice ships without
	// BP wiring. trap #2: static + Succeeded() + full /Package.Object path (the proven FObjectFinder pattern).
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyFinder(
		TEXT("/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny"));
	if (MannyFinder.Succeeded())
	{
		HeadMeshAsset = MannyFinder.Object;
		HeadMesh->SetSkeletalMeshAsset(MannyFinder.Object);
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
	// Mirror UAFLSkinColorComponent: the color is a replicated UPROPERTY -> OnRep applies it on every
	// client AND on a newly-relevant late-joiner (a replicated prop replicates on first relevancy).
	DOREPLIFETIME(AAFLDismemberedHead, HeadSkinColor);
}

void AAFLDismemberedHead::BeginPlay()
{
	Super::BeginPlay();

	// Resolve the data-driven mesh (BP child may have set a different one); ensure HeadMesh shows it.
	if (HeadMesh && HeadMesh->GetSkeletalMeshAsset() == nullptr && !HeadMeshAsset.IsNull())
	{
		HeadMesh->SetSkeletalMeshAsset(HeadMeshAsset.LoadSynchronous());
	}

	// Show ONLY the head bone region (runs on every client -- it's a local render-state op).
	IsolateHeadBone();

	// S4 TUMBLE FIX: drop the rendered head down onto the sphere body (the 'head' bone is ~165cm up at ref
	// pose). Runs on every client -- it's a local render-transform op on a non-simulating cosmetic child.
	CenterHeadVisualOnBody();

	// Apply the victim's color if it already replicated (server set-then-spawn, or a late-join client that
	// received the replicated value before BeginPlay). OnRep handles the color-arrives-after-BeginPlay case.
	ApplyHeadSkinColor();

	// AFL-0404: roll audio attached to the head mesh so it follows the tumble.
	if (RollSound && HeadMesh)
	{
		UGameplayStatics::SpawnSoundAttached(RollSound, HeadMesh);
	}
}

void AAFLDismemberedHead::IsolateHeadBone()
{
	// INVERSE of UAFLDismemberComponent::GatherZoneMeshes' hide: there, hide ONE bone (the severed limb);
	// here, hide EVERY bone EXCEPT 'head' so only the head geometry renders -- a clean floating head.
	// On SK_Mannequin 'head' is a LEAF (parent neck_02, zero children), so all non-head bones are hidden.
	// S4 TUMBLE FIX: PBO_None, NOT PBO_Term. HeadMesh is now a non-simulating cosmetic visual (the sphere
	// PartMesh is the physics body), so we no longer want to TERMINATE physics bodies -- PBO_None collapses
	// the bone's geometry to zero for the render WITHOUT touching the ragdoll bodies (which terminating was
	// the original tumble-killing bug). The head-only visual is identical; only the physics side-effect is
	// gone. Local-only render state (fine -- the actor itself is replicated; this runs on every client).
	if (!HeadMesh)
	{
		return;
	}
	static const FName HeadBone(TEXT("head"));
	const int32 NumBones = HeadMesh->GetNumBones();
	int32 Hidden = 0;
	for (int32 i = 0; i < NumBones; ++i)
	{
		const FName BoneName = HeadMesh->GetBoneName(i);
		if (BoneName != HeadBone)
		{
			HeadMesh->HideBoneByName(BoneName, PBO_None);
			++Hidden;
		}
	}
	UE_LOG(LogAFLDismember, Display,
		TEXT("[AFLDismember] head prop %s: isolated 'head' bone (hid %d/%d non-head bones on %s)"),
		*GetName(), Hidden, NumBones, *GetNameSafe(HeadMesh->GetSkeletalMeshAsset()));
}

void AAFLDismemberedHead::SetHeadSkinColor(UAFLSkinColorAsset* InColor)
{
	// Server-authority: set the replicated value; replication -> OnRep on clients -> apply. Apply locally
	// too (the server is a client on a listen-server and gets no OnRep for its own write).
	if (!HasAuthority())
	{
		return;
	}
	HeadSkinColor = InColor;
	ApplyHeadSkinColor();
}

void AAFLDismemberedHead::OnRep_HeadSkinColor()
{
	ApplyHeadSkinColor();
}

void AAFLDismemberedHead::ApplyHeadSkinColor()
{
	// S4 PHASE 0 PLACEHOLDER: tint the SPHERE (the visible placeholder body) with the victim's IDENTITY color
	// so the rolling ball reads as WHOSE head it is (purple robot -> purple ball). The engine sphere wears
	// BasicShapeMaterial (set in the ctor), which exposes a single "Color" vector param -- the skin asset's
	// rich GetColors() map (EmissiveColor/EdgeGlowColor/...) targets the robot SKIN material, NOT this sphere,
	// so we pick the asset's PRIMARY color (EmissiveColor, else the first entry) and drive BasicShapeMaterial's
	// Color. The real extracted head (PHASE 3) restores the full AAFLCharacterPartActor-style multi-param apply
	// on its own skin material. Null color -> the ball keeps the default material tint.
	UStaticMeshComponent* Sphere = GetPartMesh();
	if (!HeadSkinColor || !Sphere)
	{
		return;
	}
	const TMap<FName, FLinearColor>& Colors = HeadSkinColor->GetColors();
	if (Colors.Num() == 0)
	{
		return;
	}
	// Primary identity color: EmissiveColor if present, else the first map entry (deterministic enough for a
	// placeholder -- every Edge skin asset carries EmissiveColor as its lead tint).
	const FLinearColor* Primary = Colors.Find(FName(TEXT("EmissiveColor")));
	const FLinearColor Tint = Primary ? *Primary : Colors.CreateConstIterator()->Value;

	UMaterialInstanceDynamic* MID = Sphere->CreateAndSetMaterialInstanceDynamic(0);
	if (MID)
	{
		MID->SetVectorParameterValue(FName(TEXT("Color")), Tint);
	}
	UE_LOG(LogAFLDismember, Display,
		TEXT("[AFLDismember] head prop %s: tinted placeholder sphere with victim color %s -> Color=(%s)"),
		*GetName(), *GetNameSafe(HeadSkinColor), *Tint.ToString());
}

void AAFLDismemberedHead::ApplyPopImpulse(const FVector& Impulse)
{
	// S4 TUMBLE FIX (velocity, not force): the head sphere is HEAD-SIZED (~0.21 scale) and therefore very
	// LIGHT -- volume (and mass) scale with scale^3, so the body is ~1% of a unit sphere's mass. The base's
	// AddImpulse(bVelChange=false) divides force by that tiny mass -> a normal-looking impulse LAUNCHES the
	// head off-screen (the "head vanishes" regression). Pop the sphere with bVelChange=TRUE so the vector is
	// a TARGET VELOCITY (cm/s), mass-independent + scale-independent + predictable: 150 lateral / 120 vertical
	// reads as a controlled roll regardless of the sphere scale. Pops the sphere PartMesh (the head's physics
	// body), NOT the cosmetic skeletal mesh -- the directive's intent. Limbs keep the base force-pop (untouched).
	if (UStaticMeshComponent* Sphere = GetPartMesh())
	{
		Sphere->AddImpulse(Impulse, NAME_None, /*bVelChange=*/true);
	}
}

void AAFLDismemberedHead::CenterHeadVisualOnBody()
{
	// S4 TUMBLE TIER 1: seat the rendered head ON the rolling sphere so it rests CLEAN on the floor (jaw/chin
	// above the sphere's bottom, no clip) -- not floating a metre overhead, not bisected by the ball. Two
	// derived quantities, both from LIVE geometry (deterministic, robust to a head-mesh swap), both logged:
	//   1) CENTER on the 'head' bone: negate the bone's component-space Z so the head-bone region sits at the
	//      component origin. (The proven src=bone path, Z~=-162.6 on SK_Manny.)
	//   2) FLOOR-FIT: drop the head by (headHalfHeight - sphereRadius) so the head's BOTTOM lands at the
	//      sphere's BOTTOM. headHalfHeight is derived from the live mesh bounds TOP (crown) minus the head
	//      bone Z; sphereRadius from the engine sphere (50cm) * SphereBodyScale. headHalfHeight <= sphereRadius
	//      -> head already fits inside the ball -> no drop (clamp 0).
	if (!HeadMesh)
	{
		return;
	}
	static const FName HeadBone(TEXT("head"));
	const int32 BoneIndex = HeadMesh->GetBoneIndex(HeadBone);

	FVector Offset = HeadVisualOffset;   // fallback (~-165cm Z) if the bone/pose can't be resolved
	const TCHAR* Source = TEXT("fallback");
	float HeadBoneZ = 0.f;
	if (BoneIndex != INDEX_NONE)
	{
		// GUARD: a freshly-spawned non-simulating mesh may not have its component-space pose built on the same
		// frame as BeginPlay (it populates on the first RefreshBoneTransforms). A not-yet-built pose reads as
		// ~origin -> use the authored fallback. SK_Manny's head sits >100cm up, so a near-zero Z is the tell.
		const FVector HeadLoc = HeadMesh->GetBoneTransform(HeadBone, RTS_Component).GetLocation();
		if (FMath::Abs(HeadLoc.Z) > 1.0f)
		{
			HeadBoneZ = HeadLoc.Z;
			Offset = -HeadLoc;   // (1) center the head bone on the sphere origin
			Source = TEXT("bone");
		}
	}

	// (2) FLOOR-FIT drop -- bone path only (needs HeadBoneZ + live bounds; the fallback offset is buries-safe).
	float SphereRadius = 0.f;
	float HeadHalfHeight = 0.f;
	float Drop = 0.f;
	if (Source == FString(TEXT("bone")))
	{
		// Engine sphere = 100cm diameter (50cm radius) at scale 1; the PartMesh root carries SphereBodyScale.
		SphereRadius = 50.f * SphereBodyScale;
		// Live mesh bounds TOP in component space = the crown (highest vertex). Bone->crown is the head's
		// half-height (the head is ~symmetric about its bone region). ~18cm on SK_Manny (180.5 crown - 162.6 bone).
		const FBoxSphereBounds Bounds = HeadMesh->GetSkeletalMeshAsset()
			? HeadMesh->GetSkeletalMeshAsset()->GetBounds()
			: FBoxSphereBounds(ForceInit);
		const float CrownZ = Bounds.Origin.Z + Bounds.BoxExtent.Z;
		HeadHalfHeight = FMath::Max(0.f, CrownZ - HeadBoneZ);
		// Post-center the head's bottom sits at (-HeadHalfHeight) relative to the sphere center; the sphere
		// bottom is at (-SphereRadius). When the head is TALLER than the ball (HeadHalfHeight > SphereRadius)
		// its chin pokes BELOW the ball -> lift the head UP by (HeadHalfHeight - SphereRadius) so the chin
		// rises to the sphere bottom (rests flush, no floor clip). Clamp 0 when the head fits inside the ball.
		Drop = FMath::Max(0.f, HeadHalfHeight - SphereRadius);
		Offset.Z += Drop;   // UP: raise the chin to the sphere bottom (NOT down -- down buries it deeper)
	}

	HeadMesh->SetRelativeLocation(Offset);

	// jawClearance = head's lowest point above the sphere bottom after the lift (>=0 -> no clip below the ball).
	const float JawClearance = SphereRadius - (HeadHalfHeight - Drop);
	UE_LOG(LogAFLDismember, Display,
		TEXT("[head-fit] %s src=%s sphereRadius=%.1f headHalfHeight=%.1f drop=%.1f jawClearance=%.1f offset=%s"),
		*GetName(), Source, SphereRadius, HeadHalfHeight, Drop, JawClearance, *Offset.ToString());
}
