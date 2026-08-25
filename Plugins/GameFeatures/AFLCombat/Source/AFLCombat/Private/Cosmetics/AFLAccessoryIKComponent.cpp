// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLAccessoryIKComponent.h"

#include "AFLCombat.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Cosmetics/AFLSurfaceProviderInterface.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

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

	const FVector SocketLoc = SocketSourceMesh->GetSocketLocation(SocketName);

	// OUTWARD DIRECTION. Away from the surface mesh's centre, which is the only definition that
	// survives an arbitrary pose: a fixed axis is correct in bind pose and wrong the moment the
	// skeleton animates -- measured, when a bind-pose-derived offset threw a piece into open air.
	const FVector Centre = SearchSurfaceMesh->Bounds.Origin;
	FVector Outward = (SocketLoc - Centre);
	if (!Outward.Normalize())
	{
		Outward = SocketSourceMesh->GetComponentTransform().GetUnitAxis(EAxis::X);
	}

	const FVector Start = SocketLoc + Outward * TraceStartDistance;

	FCollisionQueryParams Q(SCENE_QUERY_STAT(AFLAccessoryIKProbe), /*bTraceComplex=*/true);
	if (const AActor* Owner = SocketSourceMesh->GetOwner())
	{
		// The pawn's OWN mesh is the invisible base and must never answer for the visible shell.
		if (const ACharacter* C = Cast<ACharacter>(Owner))
		{
			Q.AddIgnoredComponent(C->GetMesh());
			if (const UCapsuleComponent* Cap = C->GetCapsuleComponent()) { Q.AddIgnoredComponent(Cap); }
		}
	}

	FHitResult Hit;
	const bool bHit = W->LineTraceSingleByChannel(Hit, Start, SocketLoc, ECC_Visibility, Q);
	if (!bHit || Hit.GetComponent() != SearchSurfaceMesh)
	{
		UE_LOG(LogAFLCombat, Verbose,
			TEXT("[AFLAccessoryIK] '%s' probe found no %s surface (hit=%s) -- offset left at zero, marked INVALID."),
			*SocketName.ToString(), *GetNameSafe(SearchSurfaceMesh),
			bHit ? *GetNameSafe(Hit.GetComponent()) : TEXT("nothing"));
		return FVector::ZeroVector;
	}

	bOutValid = true;
	return Hit.Location - SocketLoc;
}

void UAFLAccessoryIKComponent::EvaluateAccessoryOffsets(ACharacter* OwnerChar)
{
	CurrentOffsets = FAFLAccessoryIKTargets();

	if (!OwnerChar || !OwnerChar->Implements<UAFLSurfaceProviderInterface>())
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[AFLAccessoryIK] %s does not provide surfaces -- every offset stays zero and INVALID."),
			*GetNameSafe(OwnerChar));
		return;
	}

	USkeletalMeshComponent* PawnMesh = OwnerChar->GetMesh();
	USkeletalMeshComponent* BodyMesh =
		IAFLSurfaceProviderInterface::Execute_GetVisibleMeshForSlot(OwnerChar, AFLAccessoryIK::BodySlot);
	USkeletalMeshComponent* ChainMesh =
		IAFLSurfaceProviderInterface::Execute_GetVisibleMeshForSlot(OwnerChar, AFLAccessoryIK::NeckSlot);

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
