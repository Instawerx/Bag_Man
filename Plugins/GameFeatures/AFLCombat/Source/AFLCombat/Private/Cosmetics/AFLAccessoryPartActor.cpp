// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLAccessoryPartActor.h"

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

void AAFLAccessoryPartActor::ApplyWristCorrection()
{
	const FName Socket = GetAttachedSocketName();
	const bool bIsWrist = (Socket == RightWristSocket) || (Socket == LeftWristSocket);

	if (!bIsWrist)
	{
		// IDEMPOTENT BOTH WAYS: if a previous call corrected and the actor has since moved to the other
		// wrist, the correction must come OFF, not merely not be re-applied.
		if (bWristCorrected)
		{
			if (USceneComponent* Root = GetRootComponent()) { Root->SetRelativeRotation(FRotator::ZeroRotator); }
			bWristCorrected = false;
		}
		// Neck and pendant: the mesh's authored orientation is already correct. Saying so in the log
		// matters as much as the correction -- "no rotation applied" and "this code never ran" are
		// different states and must not look alike.
		UE_LOG(LogAFLCombat, Verbose, TEXT("[AFLAccessoryPart] %s at socket '%s' -- no wrist correction needed."),
			*GetName(), *Socket.ToString());
		return;
	}

	if (USceneComponent* Root = GetRootComponent())
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
