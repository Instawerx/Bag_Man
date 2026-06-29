// Copyright C12 AI Gaming. All Rights Reserved.

#include "Spectate/AFLSpectateCameraComponent.h"

#include "Camera/LyraCameraComponent.h"        // FindCameraComponent + DetermineCameraModeDelegate
#include "Camera/LyraCameraMode.h"             // ULyraCameraMode (the third-person mode type)
#include "Camera/PlayerCameraManager.h"        // EViewTargetBlendFunction / VTBlend_Cubic
#include "Character/LyraHealthComponent.h"
#include "Character/LyraPawnData.h"            // ULyraPawnData::DefaultCameraMode (the teammate's normal mode)
#include "Character/LyraPawnExtensionComponent.h"  // FindPawnExtensionComponent + GetPawnData
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Round/AFLRoundManagerComponent.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLSpectateCameraComponent)


UAFLSpectateCameraComponent::UAFLSpectateCameraComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);   // required so the Client RPCs route to the owning client connection
}

APlayerController* UAFLSpectateCameraComponent::GetPC() const
{
	return Cast<APlayerController>(GetOwner());
}

void UAFLSpectateCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetPC();
	if (!PC || !PC->HasAuthority())
	{
		return;   // server owns death-detection + the target pick; the client only runs the Client RPC bodies
	}
	PC->OnPossessedPawnChanged.AddDynamic(this, &UAFLSpectateCameraComponent::OnPossessedPawnChanged);
	ServerBindToPawnDeath();
}

void UAFLSpectateCameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearSpectateCameraMode();
	if (APlayerController* PC = GetPC())
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &UAFLSpectateCameraComponent::OnPossessedPawnChanged);
	}
	if (BoundHealth.IsValid())
	{
		BoundHealth->OnDeathStarted.RemoveDynamic(this, &UAFLSpectateCameraComponent::HandleOwnerDeath);
	}
	BoundHealth = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UAFLSpectateCameraComponent::ServerBindToPawnDeath()
{
	APlayerController* PC = GetPC();
	if (!PC || !PC->HasAuthority())
	{
		return;
	}
	if (BoundHealth.IsValid())
	{
		BoundHealth->OnDeathStarted.RemoveDynamic(this, &UAFLSpectateCameraComponent::HandleOwnerDeath);
		BoundHealth = nullptr;
	}
	if (APawn* Pawn = PC->GetPawn())
	{
		if (ULyraHealthComponent* HC = ULyraHealthComponent::FindHealthComponent(Pawn))
		{
			HC->OnDeathStarted.AddDynamic(this, &UAFLSpectateCameraComponent::HandleOwnerDeath);
			BoundHealth = HC;
		}
	}
}

void UAFLSpectateCameraComponent::OnPossessedPawnChanged(APawn* /*OldPawn*/, APawn* NewPawn)
{
	// The round-reset force-restart re-possesses a fresh pawn -> re-bind ITS death; and if we were following,
	// tell the client to return its camera to the new pawn (the unpossess half keeps bFollowing for the repossess).
	ServerBindToPawnDeath();
	if (NewPawn && bFollowing)
	{
		Client_StopFollow();
		bFollowing = false;
	}
}

void UAFLSpectateCameraComponent::HandleOwnerDeath(AActor* /*OwningActor*/)
{
	APlayerController* PC = GetPC();
	if (!PC || !PC->HasAuthority())
	{
		return;
	}

	// Only auto-follow during an ACTIVE round (not warmup / round-end / postgame / non-round modes).
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	const UAFLRoundManagerComponent* RoundMgr =
		GS ? GS->FindComponentByClass<UAFLRoundManagerComponent>() : nullptr;
	if (!RoundMgr || !RoundMgr->IsRoundActive())
	{
		return;   // no active-round context -> leave the hold-cam
	}

	// Teammates-only target, picked SERVER-side (anti-ghosting). Last-alive (no teammate) -> hold-cam (correct).
	APawn* Teammate = FindFirstLivingTeammatePawn();
	if (!Teammate)
	{
		return;   // defensive: never send a null target
	}

	bFollowing = true;
	Client_FollowTarget(Teammate);
}

APawn* UAFLSpectateCameraComponent::FindFirstLivingTeammatePawn() const
{
	const APlayerController* PC = GetPC();
	const UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return nullptr;
	}

	APlayerState* const MyPS = PC->PlayerState;
	const AGameStateBase* const GS = World->GetGameState();
	const ULyraTeamSubsystem* Teams = World->GetSubsystem<ULyraTeamSubsystem>();
	if (!MyPS || !GS || !Teams)
	{
		return nullptr;
	}

	const int32 MyTeam = Teams->FindTeamFromObject(MyPS);
	if (MyTeam == INDEX_NONE)
	{
		return nullptr;
	}

	// Mirror the round FSM's living enumeration (PlayerArray + team + !IsDeadOrDying); teammates only.
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS || PS == MyPS)
		{
			continue;
		}
		if (Teams->FindTeamFromObject(PS) != MyTeam)
		{
			continue;   // enemy -> never a follow target (anti-ghosting)
		}
		APawn* const Pawn = PS->GetPawn();
		if (!Pawn)
		{
			continue;
		}
		const ULyraHealthComponent* HC = ULyraHealthComponent::FindHealthComponent(Pawn);
		if (HC && HC->IsDeadOrDying())
		{
			continue;   // living teammates only
		}
		return Pawn;   // first living teammate
	}
	return nullptr;
}

TSubclassOf<ULyraCameraMode> UAFLSpectateCameraComponent::GetSpectatedCameraMode() const
{
	// The teammate's NORMAL third-person mode -- exactly what ULyraHeroComponent::DetermineCameraMode returns in
	// live play (PawnData->DefaultCameraMode). Bound onto the proxy camera's delegate so its mode stack frames the
	// teammate (GetTargetActor() == the camera's owner == the teammate) behind/over-shoulder.
	if (const APawn* Pawn = FollowedPawn.Get())
	{
		if (const ULyraPawnExtensionComponent* PawnExt = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
		{
			if (const ULyraPawnData* PawnData = PawnExt->GetPawnData<ULyraPawnData>())
			{
				return PawnData->DefaultCameraMode;
			}
		}
	}
	return nullptr;
}

void UAFLSpectateCameraComponent::ClearSpectateCameraMode()
{
	if (APawn* Pawn = FollowedPawn.Get())
	{
		if (ULyraCameraComponent* Cam = ULyraCameraComponent::FindCameraComponent(Pawn))
		{
			Cam->DetermineCameraModeDelegate.Unbind();   // drop our bind (single-cast; nothing else to restore on a proxy)
		}
	}
	FollowedPawn = nullptr;
}

void UAFLSpectateCameraComponent::Client_FollowTarget_Implementation(AActor* Target)
{
	APlayerController* PC = GetPC();
	APawn* TargetPawn = Cast<APawn>(Target);
	if (!TargetPawn || !PC)
	{
		return;
	}

	ClearSpectateCameraMode();        // drop any prior follow's bind
	FollowedPawn = TargetPawn;

	// Drive the proxy teammate's camera to its normal third-person mode + activate it (else base APawn::CalcCamera
	// won't evaluate it). The proxy's DetermineCameraModeDelegate is unbound locally, so we bind it ourselves.
	if (ULyraCameraComponent* TargetCam = ULyraCameraComponent::FindCameraComponent(TargetPawn))
	{
		TargetCam->DetermineCameraModeDelegate.BindUObject(this, &UAFLSpectateCameraComponent::GetSpectatedCameraMode);
		TargetCam->SetActive(true);
	}

	// Stop bAutoManageActiveCameraTarget from re-asserting our own (ragdoll) pawn over the view target.
	PC->bAutoManageActiveCameraTarget = false;
	PC->SetViewTargetWithBlend(TargetPawn, FollowBlendTime, VTBlend_Cubic, /*BlendExp=*/0.f, /*bLockOutgoing=*/false);
}

void UAFLSpectateCameraComponent::Client_StopFollow_Implementation()
{
	ClearSpectateCameraMode();   // unbind the followed proxy's camera-mode delegate

	APlayerController* PC = GetPC();
	if (!PC)
	{
		return;
	}
	// Restore normal management for the respawned pawn, then snap the view back to it.
	PC->bAutoManageActiveCameraTarget = true;
	if (APawn* OwnPawn = PC->GetPawn())
	{
		PC->SetViewTargetWithBlend(OwnPawn, /*BlendTime=*/0.f);
	}
}
