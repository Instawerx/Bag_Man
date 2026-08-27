// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLAccessoryIKComponent.h"

#include "AFLCombat.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Cosmetics/AFLSurfaceProviderInterface.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"                 // D1: ref skeleton for the hop test
#include "Engine/SkeletalMeshSocket.h"           // D1: the socket -> attach bone
#include "PhysicsEngine/PhysicsAsset.h"          // D1: SkeletalBodySetups
#include "PhysicsEngine/BodySetup.h"             // D1: GetClosestPointAndNormal
#include "PhysicsEngine/SkeletalBodySetup.h"     // D1: USkeletalBodySetup
#include "GameFramework/Character.h"
#include "Components/ChildActorComponent.h"
#include "Cosmetics/AFLAccessoryPartActor.h"

// MUTATION SWITCH FOR THE FILTER, because the filter has never been observed doing anything.
//
// In reference pose the nearest physics body already IS the correct limb, so constrained and
// unconstrained agree exactly (measured: same bone, same offset to 0.01cm on all three sockets). That
// null says nothing about whether the constraint works -- an instrument that has never returned a
// positive cannot be trusted to return a meaningful negative.
//
// Pushing the probe origin far out makes a DIFFERENT limb the globally nearest body, which is the
// condition the constraint exists for. With it live, the engine query and ours must then DISAGREE:
// the engine follows the wrong limb, ours stays on the socket's own. That disagreement is the proof.
// 0 = off, and off is the shipping path.
static float GAFLAccessoryProbeDistanceOverride = 0.0f;
static FAutoConsoleVariableRef CVarAFLAccessoryProbeDistance(
	TEXT("afl.AccessoryIK.ProbeDistanceOverride"),
	GAFLAccessoryProbeDistanceOverride,
	TEXT("Diagnostic ONLY: override the surface probe start distance in cm. Large values force a ")
	TEXT("different limb to be globally nearest, which is how the bone constraint is proven live. ")
	TEXT("0 = use the component's TraceStartDistance (shipping)."),
	ECVF_Cheat);

namespace AFLAccessoryIK
{
	static const FName NeckSocket   (TEXT("accessory_neck"));
	static const FName WristLSocket (TEXT("accessory_wrist_l"));
	static const FName WristRSocket (TEXT("accessory_wrist_r"));
	static const FName PendantSocket(TEXT("accessory_pendant"));

	// Slot keys handed to the surface provider. The pendant asks for the CHAIN, not the body.
	static const FName BodySlot (TEXT("Body"));
	static const FName NeckSlot (TEXT("Neck"));
}

/**
 * Hierarchy distance between two bones: steps up from each to their common ancestor, summed.
 * INDEX_NONE when they share no ancestor (different skeletons, or a bad index).
 *
 * STRUCTURAL RATHER THAN A LITERAL LIST. The alternative was a hand-written allow-list per socket
 * ("wrist_l may hit lowerarm_l or hand_l"), which is correct only for the skeleton it was typed
 * against and goes silently stale the first time a bone is renamed or a body is added. Hops are
 * derived from the skeleton actually being probed, so the rule maintains itself.
 */
static int32 AFLHierarchyDistance(const FReferenceSkeleton& Ref, int32 A, int32 B)
{
	if (A == INDEX_NONE || B == INDEX_NONE) { return INDEX_NONE; }

	TMap<int32, int32> AncestorsOfA;
	int32 Depth = 0;
	for (int32 I = A; I != INDEX_NONE; I = Ref.GetParentIndex(I))
	{
		AncestorsOfA.Add(I, Depth++);
	}

	int32 Steps = 0;
	for (int32 I = B; I != INDEX_NONE; I = Ref.GetParentIndex(I), ++Steps)
	{
		if (const int32* DepthOfA = AncestorsOfA.Find(I))
		{
			return *DepthOfA + Steps;
		}
	}
	return INDEX_NONE;
}

UAFLAccessoryIKComponent::UAFLAccessoryIKComponent()
{
	PrimaryComponentTick.bCanEverTick = false;   // driven by equip events, not per-frame
}

FVector UAFLAccessoryIKComponent::ProbeSurface(const USkeletalMeshComponent* SocketSourceMesh,
                                               const USkeletalMeshComponent* SearchSurfaceMesh,
                                               FName SocketName,
                                               bool& bOutValid) const
{
	bOutValid = false;
	if (!SocketSourceMesh || !SearchSurfaceMesh) { return FVector::ZeroVector; }
	if (!SocketSourceMesh->DoesSocketExist(SocketName))
	{
		// LOUD, not silent. A socket that is not on the mesh once produced a piece parented at the
		// component origin with nothing erroring anywhere.
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[AFLAccessoryIK] socket '%s' is not on %s -- cannot probe a surface for it."),
			*SocketName.ToString(), *GetNameSafe(SocketSourceMesh));
		return FVector::ZeroVector;
	}

	const UWorld* W = GetWorld();
	if (!W) { return FVector::ZeroVector; }

	// THE SOCKET CARRIES ITS OWN OUTWARD NORMAL. Authored in 5dce58ad: every accessory socket's +X is
	// its true outward vector, so this is one axis for all four with no per-socket switch and no
	// left/right branch -- the wrist mirror lives in the asset (+0.884 vs -0.884), not here.
	//
	// It also tracks animation for free: +X is read from the socket's CURRENT world transform, so a
	// twisting forearm keeps the spike perpendicular to the shell. The two directions tried before
	// this both failed on geometry -- bounds-centre gave an oblique ray (neck +44cm), and a fixed
	// local axis runs ALONG the forearm (dot(limb)=0.999).
	const FTransform SocketXf = SocketSourceMesh->GetSocketTransform(SocketName, RTS_World);
	const FVector SocketLoc   = SocketXf.GetLocation();
	const FVector Outward     = SocketXf.GetUnitAxis(EAxis::X);

	// Spike straight through: start outside the heaviest silhouette so the query begins clear of the
	// geometry it is trying to find, rather than inside it.
	const float EffectiveProbeDistance = (GAFLAccessoryProbeDistanceOverride > 0.0f)
		? GAFLAccessoryProbeDistanceOverride
		: TraceStartDistance;
	const FVector Probe = SocketLoc + Outward * EffectiveProbeDistance;

	// WHICH LIMB DOES THIS SOCKET BELONG TO? Resolved from the socket's own attach bone, never assumed.
	const USkeletalMeshSocket* Sock = SocketSourceMesh->GetSocketByName(SocketName);
	const FName SocketBone = Sock ? Sock->BoneName : NAME_None;
	if (SocketBone.IsNone())
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[AFLAccessoryIK] '%s': socket has no attach bone -- cannot tell which limb its surface "
			     "should be on, so the search cannot be constrained. INVALID."), *SocketName.ToString());
		return FVector::ZeroVector;
	}

	const USkeletalMesh* SurfAsset = SearchSurfaceMesh->GetSkeletalMeshAsset();
	const UPhysicsAsset* PhysAsset = SearchSurfaceMesh->GetPhysicsAsset();
	if (!SurfAsset || !PhysAsset)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[AFLAccessoryIK] '%s': %s has no %s -- INVALID."), *SocketName.ToString(),
			*GetNameSafe(SearchSurfaceMesh), SurfAsset ? TEXT("physics asset") : TEXT("mesh asset"));
		return FVector::ZeroVector;
	}

	const FReferenceSkeleton& Ref = SurfAsset->GetRefSkeleton();
	const int32 SocketBoneIdx = Ref.FindBoneIndex(SocketBone);
	if (SocketBoneIdx == INDEX_NONE)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[AFLAccessoryIK] '%s': its bone '%s' is not on the SURFACE skeleton (%s). The socket "
			     "mesh and the body part do not share a skeleton, so hops are meaningless. INVALID."),
			*SocketName.ToString(), *SocketBone.ToString(), *GetNameSafe(SurfAsset));
		return FVector::ZeroVector;
	}

	// CONSTRAIN THE SEARCH, DO NOT MERELY VALIDATE ITS RESULT.
	//
	// GetClosestPointOnPhysicsAsset returns the nearest point on ANY body in the asset and reports the
	// bone it chose -- which this code used to ignore. Measured consequence: the left wrist probe
	// returned an offset of 61cm with Z=-47, a surface point most of a half-metre BELOW the wrist. That
	// is the thigh. In reference pose the arms hang beside the hips, so the leg genuinely IS the nearest
	// body to a point 45cm off the wrist, and the unconstrained query was answering correctly -- it was
	// simply being asked the wrong question.
	//
	// Rejecting a wrong bone would only convert a wrong fit into NO fit. Running the engine's own loop
	// over the RELEVANT bodies instead yields the closest point on the correct limb, which is the
	// answer the fit actually needs.
	float BestDist = FLT_MAX;
	FVector BestPos = FVector::ZeroVector;
	FName  BestBone = NAME_None;
	int32  BestHops = INDEX_NONE;

	// The unconstrained winner is tracked PURELY as diagnosis: when the constrained search finds
	// nothing, "what would it have picked" is the difference between a legible failure and a silent one.
	float UnconstrainedDist = FLT_MAX;
	FName UnconstrainedBone = NAME_None;

	for (const USkeletalBodySetup* BS : PhysAsset->SkeletalBodySetups)
	{
		if (!BS) { continue; }
		const int32 BoneIdx = Ref.FindBoneIndex(BS->BoneName);
		if (BoneIdx == INDEX_NONE) { continue; }

		const FTransform BoneTM = SearchSurfaceMesh->GetBoneTransform(BoneIdx);
		FVector P = FVector::ZeroVector, N = FVector::ZeroVector;
		const float D = BS->GetClosestPointAndNormal(Probe, BoneTM, P, N);

		if (D < UnconstrainedDist) { UnconstrainedDist = D; UnconstrainedBone = BS->BoneName; }

		const int32 Hops = AFLHierarchyDistance(Ref, SocketBoneIdx, BoneIdx);
		if (Hops == INDEX_NONE || Hops > MaxBoneHops) { continue; }

		if (D < BestDist) { BestDist = D; BestPos = P; BestBone = BS->BoneName; BestHops = Hops; }
	}

	if (BestBone.IsNone())
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[AFLAccessoryIK] '%s' (bone '%s'): NO physics body within %d hop(s) -- the limb this "
			     "socket sits on has no collision geometry on %s. Unconstrained, the query would have "
			     "picked '%s' at %.2fcm, which is the wrong limb. INVALID rather than wrong."),
			*SocketName.ToString(), *SocketBone.ToString(), MaxBoneHops, *GetNameSafe(SurfAsset),
			*UnconstrainedBone.ToString(), UnconstrainedDist);
		return FVector::ZeroVector;
	}

	// THE CONTROL, AND IT IS THE POINT OF THIS BLOCK. The filtered-vs-unfiltered comparison above runs
	// inside the NEW transform code, so it compares the fix to itself and can attribute nothing. This
	// asks the ENGINE'S OWN query the same question at the same probe point in the same frame -- the
	// only comparison that can say whether the bone filter is load-bearing or merely insurance.
	//
	// It matters because the engine's loop mixes spaces when the mesh has a leader pose component:
	//   BoneTM = bHasLeaderPoseComponent ? GetBoneTransform(i) : BoneTransforms[i];   // world : component
	//   Dist   = GetShortestDistanceToPoint(ComponentPosition, BoneTM);               // component
	// If that is what produced the 61cm wrist, the filter is NOT the fix and removing it later would
	// look safe while quietly restoring the defect.
	{
		FClosestPointOnPhysicsAsset EngineAnswer;
		if (SearchSurfaceMesh->GetClosestPointOnPhysicsAsset(Probe, EngineAnswer, /*bApproximate=*/false))
		{
			const float EngineOffset = (EngineAnswer.ClosestWorldPosition - SocketLoc).Size();
			const float OursOffset   = (BestPos - SocketLoc).Size();
			UE_LOG(LogAFLCombat, Log,
				TEXT("[AFLAccessoryIK] '%s' CONTROL: engine bone='%s' offset=%.2fcm | ours bone='%s' "
				     "offset=%.2fcm | %s"),
				*SocketName.ToString(), *EngineAnswer.BoneName.ToString(), EngineOffset,
				*BestBone.ToString(), OursOffset,
				(EngineAnswer.BoneName == BestBone)
					? TEXT("SAME BONE -- the filter is not what changed the answer")
					: TEXT("DIFFERENT BONE -- the filter IS load-bearing"));
		}
	}

	const FVector HitLoc = BestPos;

	// PRESENCE OF OUTPUT, and it names what changed: when the constrained answer differs from the
	// unconstrained one, that difference IS the defect being fixed, printed rather than inferred.
	UE_LOG(LogAFLCombat, Log,
		TEXT("[AFLAccessoryIK] '%s' bone='%s' hops=%d dist=%.2fcm%s"),
		*SocketName.ToString(), *BestBone.ToString(), BestHops, BestDist,
		(UnconstrainedBone != BestBone)
			? *FString::Printf(TEXT("  (unconstrained would have taken '%s' at %.2fcm)"),
				*UnconstrainedBone.ToString(), UnconstrainedDist)
			: TEXT(""));

	const FVector ToSurface = HitLoc - SocketLoc;
	if (ToSurface.SizeSquared() < FMath::Square(0.5f))
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[AFLAccessoryIK] '%s': surface is %.2fcm from the socket -- too small to be a fit, INVALID."),
			*SocketName.ToString(), ToSurface.Size());
		return FVector::ZeroVector;
	}

	bOutValid = true;
	return ToSurface;
}

void UAFLAccessoryIKComponent::EvaluateAccessoryOffsets(ACharacter* OwnerChar)
{
	CurrentOffsets = FAFLAccessoryIKTargets();

	if (!OwnerChar)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("[AFLAccessoryIK] no owning character -- offsets stay zero and INVALID."));
		return;
	}

	USkeletalMeshComponent* PawnMesh = OwnerChar->GetMesh();
	USkeletalMeshComponent* BodyMesh  = GetVisibleMeshForSlot_Implementation(AFLAccessoryIK::BodySlot);
	USkeletalMeshComponent* ChainMesh = GetVisibleMeshForSlot_Implementation(AFLAccessoryIK::NeckSlot);

	// LAZY RESOLVE, because registration and existence are not the same moment. AddCharacterPart
	// QUEUES the parts: measured, the spawn-side hook ran with "scanning 0 child-actor component(s)"
	// because no ChildActorComponent existed yet when RefreshAccessoriesForPawn returned. Registering
	// on that path is correct in place and wrong in time, so the surface is also resolved here, when
	// the question is actually being asked.
	if (!BodyMesh)
	{
		TArray<UChildActorComponent*> CACs;
		OwnerChar->GetComponents<UChildActorComponent>(CACs);
		for (UChildActorComponent* CAC : CACs)
		{
			AActor* Child = CAC ? CAC->GetChildActor() : nullptr;
			if (!Child || Child->IsA<AAFLAccessoryPartActor>()) { continue; }   // ours, not the body
			if (USkeletalMeshComponent* M = Child->FindComponentByClass<USkeletalMeshComponent>())
			{
				BodyMesh = M;
				RegisterMeshForSlot_Implementation(AFLAccessoryIK::BodySlot, M);
				break;
			}
		}
	}

	if (!BodyMesh && !ChainMesh)
	{
		// PRESENCE OF OUTPUT. "nothing registered yet" and "registered but nothing found" are
		// different states and must not both read as four zero offsets.
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[AFLAccessoryIK] registry EMPTY for %s -- no spawn hook has published a visible mesh. "
			     "Offsets stay zero and INVALID."), *GetNameSafe(OwnerChar));
		return;
	}

	// A provider that hands back the invisible base has answered the wrong question; refuse it rather
	// than pin every piece to a boundary nothing draws.
	if (BodyMesh && BodyMesh == PawnMesh)
	{
		UE_LOG(LogAFLCombat, Error,
			TEXT("[AFLAccessoryIK] provider returned the pawn's OWN mesh for 'Body' -- that is the "
			     "invisible base (SKM_Manny_Invis). Refusing it; the visible body is a character part."));
		BodyMesh = nullptr;
	}

	if (BodyMesh)
	{
		CurrentOffsets.LeftWristOffset  = ProbeSurface(PawnMesh, BodyMesh, AFLAccessoryIK::WristLSocket, CurrentOffsets.bLeftWristValid);
		CurrentOffsets.RightWristOffset = ProbeSurface(PawnMesh, BodyMesh, AFLAccessoryIK::WristRSocket, CurrentOffsets.bRightWristValid);
		CurrentOffsets.NeckOffset       = ProbeSurface(PawnMesh, BodyMesh, AFLAccessoryIK::NeckSocket,   CurrentOffsets.bNeckValid);
	}

	// TWO-LEVEL. The pendant's surface is the CHAIN, and its socket lives on the chain's own mesh --
	// AddCharacterPart can only attach to the pawn, so the chain spawns the pendant itself.
	if (ChainMesh)
	{
		CurrentOffsets.PendantOffset = ProbeSurface(ChainMesh, ChainMesh, AFLAccessoryIK::PendantSocket, CurrentOffsets.bPendantValid);
	}

	// PUSH THE NEW OFFSETS TO THE PIECES. They applied theirs at BeginPlay, which runs BEFORE a
	// surface can be registered -- the body part and the chain both spawn after. Without this the
	// first evaluation is always the one nobody consumed.
	{
		int32 Reapplied = 0;
		TArray<UChildActorComponent*> CACs;
		OwnerChar->GetComponents<UChildActorComponent>(CACs);
		for (UChildActorComponent* CAC : CACs)
		{
			AActor* Child = CAC ? CAC->GetChildActor() : nullptr;
			if (AAFLAccessoryPartActor* Part = Cast<AAFLAccessoryPartActor>(Child))
			{
				Part->ApplyIKOffset();
				++Reapplied;
				// the pendant hangs off the chain, not the pawn -- reach it through its parent
				TArray<UChildActorComponent*> Inner;
				Child->GetComponents<UChildActorComponent>(Inner);
				for (UChildActorComponent* IC : Inner)
				{
					if (AAFLAccessoryPartActor* Pend = Cast<AAFLAccessoryPartActor>(IC->GetChildActor()))
					{
						Pend->ApplyIKOffset();
						++Reapplied;
					}
				}
			}
		}
		UE_LOG(LogAFLCombat, Log, TEXT("[AFLAccessoryIK] re-applied offsets to %d piece(s)"), Reapplied);
	}

	UE_LOG(LogAFLCombat, Log,
		TEXT("[AFLAccessoryIK] %s wristL=%s(%d) wristR=%s(%d) neck=%s(%d) pendant=%s(%d)"),
		*GetNameSafe(OwnerChar),
		*CurrentOffsets.LeftWristOffset.ToCompactString(),  CurrentOffsets.bLeftWristValid  ? 1 : 0,
		*CurrentOffsets.RightWristOffset.ToCompactString(), CurrentOffsets.bRightWristValid ? 1 : 0,
		*CurrentOffsets.NeckOffset.ToCompactString(),       CurrentOffsets.bNeckValid       ? 1 : 0,
		*CurrentOffsets.PendantOffset.ToCompactString(),    CurrentOffsets.bPendantValid    ? 1 : 0);
}

FVector UAFLAccessoryIKComponent::GetOffsetForSocket(FName SocketName, bool& bOutValid) const
{
	if (SocketName == AFLAccessoryIK::WristLSocket)  { bOutValid = CurrentOffsets.bLeftWristValid;  return CurrentOffsets.LeftWristOffset; }
	if (SocketName == AFLAccessoryIK::WristRSocket)  { bOutValid = CurrentOffsets.bRightWristValid; return CurrentOffsets.RightWristOffset; }
	if (SocketName == AFLAccessoryIK::NeckSocket)    { bOutValid = CurrentOffsets.bNeckValid;       return CurrentOffsets.NeckOffset; }
	if (SocketName == AFLAccessoryIK::PendantSocket) { bOutValid = CurrentOffsets.bPendantValid;    return CurrentOffsets.PendantOffset; }
	bOutValid = false;
	return FVector::ZeroVector;
}


// ---------------------------------------------------------------------------------------------
// THE REGISTRY. Two entry points, because Lyra's spawn paths are asymmetric.
// ---------------------------------------------------------------------------------------------

USkeletalMeshComponent* UAFLAccessoryIKComponent::GetVisibleMeshForSlot_Implementation(FName SlotName)
{
	if (const TWeakObjectPtr<USkeletalMeshComponent>* Found = SlotMeshMap.Find(SlotName))
	{
		if (USkeletalMeshComponent* M = Found->Get()) { return M; }
		// A stale entry is a re-equip that destroyed the publisher. Drop it rather than hand back a
		// dangling surface -- re-equipping a chain respawns its pendant component every time.
		SlotMeshMap.Remove(SlotName);
	}
	return nullptr;
}

void UAFLAccessoryIKComponent::RegisterMeshForSlot_Implementation(FName SlotName, USkeletalMeshComponent* VisibleMesh)
{
	if (SlotName.IsNone()) { return; }

	if (!VisibleMesh)
	{
		SlotMeshMap.Remove(SlotName);
		UE_LOG(LogAFLCombat, Log, TEXT("[AFLAccessoryIK] slot '%s' cleared."), *SlotName.ToString());
		return;
	}

	// REFUSE THE INVISIBLE BASE. Accepting it would pin every piece to a boundary nothing renders,
	// which is the exact failure the whole surface-provider indirection exists to prevent.
	if (const ACharacter* C = Cast<ACharacter>(GetOwner()))
	{
		if (VisibleMesh == C->GetMesh())
		{
			UE_LOG(LogAFLCombat, Error,
				TEXT("[AFLAccessoryIK] REFUSED slot '%s': that is the pawn's own mesh (the invisible "
				     "base). The visible body is a character part."), *SlotName.ToString());
			return;
		}
	}

	SlotMeshMap.Add(SlotName, VisibleMesh);
	UE_LOG(LogAFLCombat, Log, TEXT("[AFLAccessoryIK] slot '%s' -> %s"),
		*SlotName.ToString(), *GetNameSafe(VisibleMesh));
}

void UAFLAccessoryIKComponent::RegisterSurface(AActor* PawnOwner, FName SlotName, USkeletalMeshComponent* VisibleMesh)
{
	if (!PawnOwner)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("[AFLAccessoryIK] RegisterSurface('%s') with no owner."), *SlotName.ToString());
		return;
	}
	if (UAFLAccessoryIKComponent* IK = PawnOwner->FindComponentByClass<UAFLAccessoryIKComponent>())
	{
		IK->RegisterMeshForSlot_Implementation(SlotName, VisibleMesh);
		IK->RefreshFromOwner();
		return;
	}

	// NOT SILENT. Without this, "the hook never ran" and "the component is not on the pawn" produce
	// the identical observable -- nothing in the log at all -- and there is no way to tell them apart
	// from the outside. That is the exact confident-null shape this session has paid for repeatedly,
	// and it was in code written to avoid it.
	UE_LOG(LogAFLCombat, Error,
		TEXT("[AFLAccessoryIK] RegisterSurface('%s'): %s has NO UAFLAccessoryIKComponent. The experience "
		     "AddComponents entry is missing or did not reach this pawn -- every accessory will stay at "
		     "its authored pose."),
		*SlotName.ToString(), *GetNameSafe(PawnOwner));
}

void UAFLAccessoryIKComponent::RefreshFromOwner()
{
	EvaluateAccessoryOffsets(Cast<ACharacter>(GetOwner()));
}
