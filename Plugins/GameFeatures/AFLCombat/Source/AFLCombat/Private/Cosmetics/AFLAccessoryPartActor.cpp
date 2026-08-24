// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLAccessoryPartActor.h"
#include "Components/MeshComponent.h"   // the correction lives on the MESH, not the root

#include "AFLCombat.h"
#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"

AAFLAccessoryPartActor::AAFLAccessoryPartActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// A root of our own, so the correction has something to rotate that is not the mesh itself. Rotating
	// a mesh component would put the correction in a different place per Blueprint depending on which
	// component happened to be first.
	USceneComponent* Scene = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Scene);
}

FName AAFLAccessoryPartActor::GetAttachedSocketName() const
{
	// The customizer attaches a UChildActorComponent at the socket and spawns us as its child actor,
	// so the socket name is on the PARENT COMPONENT, not on anything of ours.
	if (const UChildActorComponent* Parent = Cast<UChildActorComponent>(GetParentComponent()))
	{
		return Parent->GetAttachSocketName();
	}
	// DIRECT ATTACH FALLBACK. A SpawnActor + AttachToComponent has no ChildActorComponent, and reading
	// only the customizer's path would report NAME_None for an actor that is plainly on a socket.
	if (const USceneComponent* Root = GetRootComponent())
	{
		return Root->GetAttachSocketName();
	}
	return NAME_None;
}

void AAFLAccessoryPartActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyWristCorrection();
}


// The visible mesh, which is a CHILD of the actor root in every part BP. The correction goes here
// because the engine owns the root's relative transform for a child actor and overwrites it.
static UMeshComponent* AFLFindPartMesh(const AActor* Self)
{
	TInlineComponentArray<UMeshComponent*> Meshes(Self);
	for (UMeshComponent* M : Meshes)
	{
		if (M && M != Self->GetRootComponent()) { return M; }
	}
	// A BP whose ROOT is the mesh still has to work -- it just cannot survive the snap, and the log
	// below says so rather than pretending the correction held.
	return Meshes.Num() > 0 ? Meshes[0] : nullptr;
}

FVector AAFLAccessoryPartActor::GetPartUpVector() const
{
	if (const UMeshComponent* M = AFLFindPartMesh(this)) { return M->GetUpVector(); }
	return GetActorUpVector();
}

void AAFLAccessoryPartActor::ApplyWristCorrection()
{
	const FName Socket = GetAttachedSocketName();
	const bool bIsWrist = (Socket == RightWristSocket) || (Socket == LeftWristSocket);

	// THE NECK NEEDS ONE TOO, for the same reason the wrists do: the socket's frame is not the world's.
	// spine_03 runs +X up the spine, so a chain that hangs along its own -Z inherits a sideways frame.
	if (Socket == NeckSocket)
	{
		if (UMeshComponent* M = AFLFindPartMesh(this))
		{
			M->SetRelativeRotation(BaseNeckOrientation.Quaternion());
			bWristCorrected = true;
			UE_LOG(LogAFLCombat, Log,
				TEXT("[AFLAccessoryPart] %s at '%s' -- neck %s (socket +X points up the spine; this is what makes it hang)"),
				*GetName(), *Socket.ToString(), *BaseNeckOrientation.ToCompactString());
		}
		return;
	}

	if (!bIsWrist)
	{
		// IDEMPOTENT BOTH WAYS: if a previous call corrected and the actor has since moved to the other
		// wrist, the correction must come OFF, not merely not be re-applied.
		if (bWristCorrected)
		{
			if (UMeshComponent* M = AFLFindPartMesh(this)) { M->SetRelativeRotation(FRotator::ZeroRotator); }
			bWristCorrected = false;
		}
		// Neck and pendant: the mesh's authored orientation is already correct. Saying so in the log
		// matters as much as the correction -- "no rotation applied" and "this code never ran" are
		// different states and must not look alike.
		UE_LOG(LogAFLCombat, Verbose, TEXT("[AFLAccessoryPart] %s at socket '%s' -- no wrist correction needed."),
			*GetName(), *Socket.ToString());
		return;
	}

	// THE MESH, NOT THE ROOT. Writing the root here is what made the correction vanish between
	// BeginPlay and the first frame: the engine snaps a child actor's root to its component
	// immediately afterwards. Measured -- relRot read back R(0) on both wrists while the log said
	// base R=90 and a +180 mirror had been applied.
	if (UMeshComponent* Root = AFLFindPartMesh(this))
	{
		// BASE FIRST, THEN THE SIDE. The base puts the face up (the socket's up-ish axis is +Y, not +Z);
		// the per-side roll undoes the mirroring between the two sockets. Composed as quaternions so the
		// order is unambiguous -- adding two FRotators is not rotation composition.
		FQuat Q = BaseWristOrientation.Quaternion();
		if (Socket == RightWristSocket)
		{
			Q = RightWristCorrection.Quaternion() * Q;
		}
		Root->SetRelativeRotation(Q);
		bWristCorrected = true;
		UE_LOG(LogAFLCombat, Log,
			TEXT("[AFLAccessoryPart] %s at '%s' -- base %s%s"),
			*GetName(), *Socket.ToString(), *BaseWristOrientation.ToCompactString(),
			(Socket == RightWristSocket)
				? *FString::Printf(TEXT(" + right-wrist mirror %s"), *RightWristCorrection.ToCompactString())
				: TEXT(" (left wrist: no mirror needed)"));
	}
}
