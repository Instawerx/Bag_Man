// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLAccessoryPartActor.h"
#include "Components/MeshComponent.h"   // the correction lives on the MESH, not the root

#include "AFLCombat.h"
#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"
#include "Cosmetics/AFLAccessoryIKComponent.h"

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

static UMeshComponent* AFLFindPartMesh(const AActor* Self);

void AAFLAccessoryPartActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyWristCorrection();

	// ASK FOR A FRESH PROBE FIRST. This actor existing is proof the part list has materialised, which
	// is exactly what the spawn-side hook could not guarantee -- it ran before any ChildActorComponent
	// existed. The evaluate re-applies to every piece, including this one, so no separate call is
	// needed after it.
	// A CHILD ACTOR HAS NO OWNER AT BeginPlay -- measured: the walk printed "[ (none) ]" for every
	// piece. The route to the pawn is the PARENT COMPONENT, which this file's own
	// GetAttachedSocketName and AAFLAccessoryChainActor::ResolvePendantId already use and comment on:
	// "the ChildActorComponent's owner is the pawn; our own GetOwner may be that component's owner".
	// Start from the parent component's owner and fall back to GetOwner for the direct-attach case.
	AActor* Start = nullptr;
	if (const UChildActorComponent* PC = Cast<UChildActorComponent>(GetParentComponent()))
	{
		Start = PC->GetOwner();
	}
	if (!Start) { Start = GetOwner(); }

	FString Walked;
	for (AActor* A = Start; A; A = (Cast<UChildActorComponent>(A->GetParentComponent())
			? Cast<UChildActorComponent>(A->GetParentComponent())->GetOwner()
			: A->GetOwner()))
	{
		Walked += FString::Printf(TEXT("%s "), *GetNameSafe(A));
		if (UAFLAccessoryIKComponent* IK = A->FindComponentByClass<UAFLAccessoryIKComponent>())
		{
			UE_LOG(LogAFLCombat, Log, TEXT("[AFLAccessoryPart] %s found IK on %s -- requesting probe."),
				*GetName(), *GetNameSafe(A));
			IK->RefreshFromOwner();
			return;
		}
	}

	// NOT SILENT. The owner walk finding nothing and the walk never running were indistinguishable,
	// which is how an entire IK path stayed inert across two clean 22/22 runs. Name the chain that was
	// searched so the next reader knows where to look.
	UE_LOG(LogAFLCombat, Error,
		TEXT("[AFLAccessoryPart] %s: NO UAFLAccessoryIKComponent found walking owners [ %s] -- piece stays "
		     "at its authored pose."), *GetName(), Walked.IsEmpty() ? TEXT("(none) ") : *Walked);
	ApplyIKOffset();
}

/**
 * STATIC PIECES ONLY -- watches and pendants, which have no skeleton and therefore no bone for a
 * Transform (Modify) Bone node to reach. Measured across all ten pieces: 2 chains (5 bones) and 2
 * bracelets (4 bones) are skeletal and go through the AnimGraph; 2 watches and 4 pendants are static
 * meshes and come through here.
 *
 * THREE THINGS THIS DOES DIFFERENTLY, each guarding a defect already paid for in this codebase:
 *
 *  1. WRITES THE MESH, NEVER THE ROOT. The engine snaps a child actor's root to its parent component
 *     on the frame after BeginPlay -- measured, when a root-written wrist correction read back as
 *     R(0) while the log said it had been applied. ApplyWristCorrection writes the mesh for exactly
 *     this reason and so does this.
 *  2. APPLIES TO THE AUTHORED POSE, not to wherever the mesh currently sits. Re-evaluating on a
 *     re-equip would otherwise stack offset on offset and walk the piece off the body over time.
 *  3. USES GetAttachParent(), not GetParentComponents()[0]. That array can be empty for a
 *     freshly-registered component, and indexing it is a crash rather than a wrong answer.
 */
/**
 * IS THIS PIECE ACTUALLY ON THE CHARACTER YET?
 *
 * Walks mesh -> Root -> ChildActorComponent -> CharacterMesh0. Returns the pawn mesh, or null if the
 * chain does not reach one.
 *
 * THIS REPLACES THE OLD GUARD, which asserted that the socket NAME resolved. A name resolves on a
 * free-standing actor -- GetAttachedSocketName reads the parent ChildActorComponent, and that component
 * knows its socket long before the actor's own link into the hierarchy exists. Measured: 22 of 50
 * applications ran on pieces whose chain terminated at Root, every one of them with a perfectly valid
 * socket name, and the suite passed 21 arms throughout. Name-resolves and is-attached are different
 * claims and only one of them means the piece is on the body.
 */
static const USkeletalMeshComponent* AFLFindAttachedPawnMesh(const USceneComponent* From)
{
	for (const USceneComponent* C = From; C; C = C->GetAttachParent())
	{
		if (const USkeletalMeshComponent* Sk = Cast<USkeletalMeshComponent>(C))
		{
			if (Cast<ACharacter>(Sk->GetOwner())) { return Sk; }
		}
	}
	return nullptr;
}



void AAFLAccessoryPartActor::ApplyIKOffset()
{
	UMeshComponent* Mesh = AFLFindPartMesh(this);
	if (!Mesh) { return; }

	// GATE ON THE ATTACH. A write before the ChildActorComponent link exists lands a WORLD position on
	// a free-standing actor, which then never moves because nothing parents it -- that is what the
	// +/-51cm wrist pieces were. Defer rather than write; the re-drive below runs the moment the link
	// appears, so nothing is lost by waiting.
	if (!AFLFindAttachedPawnMesh(Mesh))
	{
		if (!bAwaitingAttach)
		{
			bAwaitingAttach = true;
			UE_LOG(LogAFLCombat, Log,
				TEXT("[AFLAccessoryPart] %s: not attached to a character mesh yet -- DEFERRING the IK write."),
				*GetName());
			// Re-drive on the next tick, by which time the child actor has been linked in. One-shot:
			// the timer is cleared as soon as a write succeeds.
			GetWorldTimerManager().SetTimer(AttachRetryHandle, this,
				&AAFLAccessoryPartActor::ApplyIKOffset, 0.0f, /*bLoop=*/true, /*FirstDelay=*/0.0f);
		}
		return;
	}
	if (bAwaitingAttach)
	{
		bAwaitingAttach = false;
		GetWorldTimerManager().ClearTimer(AttachRetryHandle);
		UE_LOG(LogAFLCombat, Log, TEXT("[AFLAccessoryPart] %s: attach landed -- applying the deferred IK write."),
			*GetName());
	}

	// SKELETAL PIECES COME THROUGH HERE TOO, and that is a deviation worth stating.
	//
	// The design routes chains and bracelets through a Transform (Modify) Bone node on `root`
	// (BCS_WorldSpace, BMM_Additive, translation only). That node is not reachable from script: an
	// AnimationGraph's Nodes array is protected, so creating and wiring it is a manual editor
	// operation.
	//
	// A world-space additive translation of the ROOT bone and a translation of the whole MESH
	// COMPONENT are the same transform -- both shift the entire piece by the delta, and AnimDynamics
	// simulates the bone chain RELATIVE to the component, so the sway survives either way.
	//
	// WHAT IS LOST: per-bone correction. The node route could later fit a chain to a curved chest by
	// moving individual links; this cannot. For "sit on the surface instead of inside it", which is
	// the defect being fixed, a whole-piece translation is the entire requirement.
	//
	// This does not fight the AnimGraph: the component transform and the bone poses are independent,
	// so adding the node later needs this call removed, not rewritten.

	if (!bAuthoredMeshLocationCaptured)
	{
		AuthoredMeshRelativeLocation = Mesh->GetRelativeLocation();
		bAuthoredMeshLocationCaptured = true;
	}

	// PARENT COMPONENT, not GetOwner -- a child actor has no Owner at BeginPlay. Same route the
	// socket resolver in this file already takes.
	const AActor* Start = nullptr;
	if (const UChildActorComponent* PC = Cast<UChildActorComponent>(GetParentComponent()))
	{
		Start = PC->GetOwner();
	}
	if (!Start) { Start = GetOwner(); }

	const UAFLAccessoryIKComponent* IK = nullptr;
	for (const AActor* A = Start; A && !IK; A = (Cast<UChildActorComponent>(A->GetParentComponent())
			? Cast<UChildActorComponent>(A->GetParentComponent())->GetOwner()
			: A->GetOwner()))
	{
		IK = A->FindComponentByClass<UAFLAccessoryIKComponent>();
	}
	if (!IK) { return; }


	bool bValid = false;
	const FVector WorldOffset = IK->GetOffsetForSocket(GetAttachedSocketName(), bValid);
	if (!bValid)
	{
		// INVALID is not zero. Leaving the piece at its authored pose is the honest answer when the
		// probe never reached the shell; silently applying a zero would look identical to a hit.
		UE_LOG(LogAFLCombat, Verbose,
			TEXT("[AFLAccessoryPart] %s: no valid IK offset for '%s' -- left at authored pose."),
			*GetName(), *GetAttachedSocketName().ToString());
		return;
	}

	const USceneComponent* Parent = Mesh->GetAttachParent();
	const FVector LocalOffset = Parent
		? Parent->GetComponentTransform().InverseTransformVector(WorldOffset)
		: WorldOffset;

	Mesh->SetRelativeLocation(AuthoredMeshRelativeLocation + LocalOffset);

	// READ THE OUTCOME, NOT THE INTENT. Everything up to this line is what we ASKED for; this is what
	// actually happened. The offset value and the write landing are different failures and have been
	// indistinguishable in every log so far.
	{
		// WALK THE WHOLE CHAIN. The previous cut read only Mesh->GetAttachParent() and reported
		// "attachParent=Root, ownedBy=self" as though the piece were free-standing. That is the NORMAL
		// hierarchy for a Lyra character part -- the socket attach happens two levels up:
		//     pawn CharacterMesh0 <- ChildActorComponent(socket) <- child actor Root <- Mesh
		// Reading one hop could never see it. Print every level so the question is answerable.
		{
			FString Chain;
			int32 Depth = 0;
			for (const USceneComponent* C2 = Mesh; C2 && Depth < 8; C2 = C2->GetAttachParent(), ++Depth)
			{
				// SCALE AT EVERY LEVEL, and the RELATIVE scale beside the accumulated world scale.
				// A pendant that renders enormous while every socket reads ~1.9 has to be gaining it
				// somewhere in this chain -- most likely a NON-UNIFORM scale on a ROTATED socket, which
				// does not scale but SHEARS, and the shear compounds through every child.
				const FVector RelS = C2->GetRelativeScale3D();
				const FVector WldS = C2->GetComponentScale();
				Chain += FString::Printf(TEXT("%s(%s owner=%s sock='%s' relS=%.2f,%.2f,%.2f wldS=%.2f,%.2f,%.2f) <- "),
					*GetNameSafe(C2), *C2->GetClass()->GetName(),
					*GetNameSafe(C2->GetOwner()), *C2->GetAttachSocketName().ToString(),
					RelS.X, RelS.Y, RelS.Z, WldS.X, WldS.Y, WldS.Z);
			}
			UE_LOG(LogAFLCombat, Display, TEXT("[AFLIK-CHAIN] %s : %s(end)"), *GetName(), *Chain);
		}

		const USceneComponent* AttachParent = Mesh->GetAttachParent();
		const AActor* AttachOwner = AttachParent ? AttachParent->GetOwner() : nullptr;

		// The socket lives on the PAWN's mesh, which is at the TOP of the chain -- find it by walking
		// up rather than assuming it is the immediate parent.
		const USkeletalMeshComponent* PawnMeshTop = nullptr;
		for (const USceneComponent* C3 = Mesh; C3; C3 = C3->GetAttachParent())
		{
			if (const USkeletalMeshComponent* Sk = Cast<USkeletalMeshComponent>(C3))
			{
				if (Cast<ACharacter>(Sk->GetOwner())) { PawnMeshTop = Sk; break; }
			}
		}

		// Where the socket says the piece should be, vs where it actually is.
		FVector SocketWorld = FVector::ZeroVector;
		bool bHaveSocket = false;
		{
			const FName Sock = GetAttachedSocketName();
			if (PawnMeshTop && !Sock.IsNone() && PawnMeshTop->DoesSocketExist(Sock))
			{
				SocketWorld = PawnMeshTop->GetSocketLocation(Sock);
				bHaveSocket = true;
			}
		}
		const FVector Actual   = Mesh->GetComponentLocation();
		const FVector Expected = SocketWorld + WorldOffset;
		const double  Delta    = bHaveSocket ? (Actual - Expected).Size() : -1.0;

		// IS THE PAWN ANIMATING, OR STILL IN THE SPAWN POSE? A socket queried on a mesh that has not
		// begun animating returns the BIND/A pose, and every offset computed from it is correct for a
		// character standing with its arms out and wrong for a moving one. Measured this session: the
		// wrist socket sat at the exact bind-pose hand position to three decimals. The A-pose-until-
		// moved bug is a known regression that returned; this names the condition so a measurement
		// taken under it is never again mistaken for a measurement of the product.
		if (PawnMeshTop)
		{
			const UAnimInstance* AI = PawnMeshTop->GetAnimInstance();
			UE_LOG(LogAFLCombat, Display,
				TEXT("[AFLIK-POSE] %s pawnMesh=%s animInstance=%s poseTicked=%d lastRender=%.2f bones=%d"),
				*GetName(), *GetNameSafe(PawnMeshTop), *GetNameSafe(AI),
				PawnMeshTop->IsComponentTickEnabled() ? 1 : 0,
				PawnMeshTop->GetLastRenderTime(),
				PawnMeshTop->GetNumBones());
		}

		UE_LOG(LogAFLCombat, Display,
			TEXT("[AFLIK-READBACK] %s socket='%s' | attachParent=%s ownedBy=%s | meshWorld=%s "
			     "socketWorld=%s expected=%s DELTA=%.2fcm"),
			*GetName(), *GetAttachedSocketName().ToString(),
			*GetNameSafe(AttachParent), *GetNameSafe(AttachOwner),
			*Actual.ToCompactString(),
			bHaveSocket ? *SocketWorld.ToCompactString() : TEXT("(socket unresolved)"),
			bHaveSocket ? *Expected.ToCompactString() : TEXT("-"),
			Delta);

		// Where the piece sits relative to the PAWN, so "on the body" and "across the room" are
		// separable at a glance.
		{
			const ACharacter* C = PawnMeshTop ? Cast<ACharacter>(PawnMeshTop->GetOwner()) : nullptr;
			if (C)
			{
				UE_LOG(LogAFLCombat, Display,
					TEXT("[AFLIK-READBACK] %s   relativeToPawn=%s (pawn %s at %s)"),
					*GetName(), *(Actual - C->GetActorLocation()).ToCompactString(),
					*GetNameSafe(C), *C->GetActorLocation().ToCompactString());
			}
		}
	}
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
