// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLGameMode.h"

#include "AFLGameCore.h"                      // LogAFLGameCore
#include "AFLRoundRestartPolicy.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"           // ParseOption -- the ?PlayFabId= and ?District= connect-option reads
#include "Teams/AFLReconcileIdComponent.h"    // the T2 identity-join stash
#include "Teams/AFLMatchmakerDataProvider.h"  // S12-E: IsRosterExternallyOwned -- the conditional on the gate
#include "Online/AFLGameLiftHostSubsystem.h"  // S12-E: validate + resolve the connecting player's session
#include "HAL/PlatformProcess.h"              // Sleep -- the bounded not-yet-visible retry

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGameMode)

bool AAFLGameMode::ControllerCanRestart(AController* Controller)
{
	// Round-based gate: deny mid-round respawn while a round is active. Decoupled from the GameFeature
	// round driver via the IAFLRoundRestartPolicy seam -- NO concrete GameFeature type is referenced, so
	// this always-loaded GameMode carries ZERO dependency into AFLCombat. No-op (falls through to Super)
	// when no policy provider is present, so it is safe to set as a global default game mode.
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GS = World->GetGameState<AGameStateBase>())
		{
			TArray<UActorComponent*> Comps;
			GS->GetComponents(Comps);
			for (const UActorComponent* Comp : Comps)
			{
				if (const IAFLRoundRestartPolicy* Policy = Cast<IAFLRoundRestartPolicy>(Comp))
				{
					if (Policy->ShouldBlockRestart())
					{
						return false;
					}
					break;   // the first policy provider decides
				}
			}
		}
	}
	return Super::ControllerCanRestart(Controller);
}

void AAFLGameMode::Logout(AController* Exiting)
{
	// Open the grace window BEFORE Super, while the PlayerState is still reachable.
	if (const APlayerController* PC = Cast<APlayerController>(Exiting))
	{
		if (const APlayerState* PS = PC->PlayerState)
		{
			const UAFLReconcileIdComponent* IdComp = PS->FindComponentByClass<UAFLReconcileIdComponent>();
			const FString PlayFabId = IdComp ? IdComp->GetReconcileId() : FString();

			if (!PlayFabId.IsEmpty() && ReconnectGraceSeconds > 0.f && GetWorld())
			{
				// NOTE what is NOT here: RemovePlayerSession. The seat stays reserved. Removal is terminal
				// (proven), so releasing it now would make the return trip impossible rather than merely slow.
				FTimerHandle& Handle = ReconnectTimers.FindOrAdd(PlayFabId);
				GetWorld()->GetTimerManager().ClearTimer(Handle);
				GetWorld()->GetTimerManager().SetTimer(Handle,
					FTimerDelegate::CreateUObject(this, &AAFLGameMode::HandleReconnectWindowExpired, PlayFabId),
					ReconnectGraceSeconds, /*bLoop=*/false);

				UE_LOG(LogAFLGameCore, Log,
					TEXT("AFL_RECONNECT: '%s' disconnected -- seat HELD for %.0fs. Player session NOT released."),
					*PlayFabId, ReconnectGraceSeconds);
			}
		}
	}

	Super::Logout(Exiting);
}

void AAFLGameMode::HandleReconnectWindowExpired(FString PlayFabId)
{
	ReconnectTimers.Remove(PlayFabId);

	// NOW the seat is genuinely forfeit, so releasing the GameLift session is correct rather than destructive.
	if (const UAFLGameLiftHostSubsystem* GameLift = UAFLGameLiftHostSubsystem::Get(this))
	{
		if (const FString* Psid = SessionIdByPlayFabId.Find(PlayFabId))
		{
			GameLift->ReleasePlayerSession(*Psid);
		}
	}
	SessionIdByPlayFabId.Remove(PlayFabId);

	// ⚠ INTEGRATION POINT, DELIBERATELY NOT WIRED YET. The operator ruling is CANCELLED-REFUND for a dropout
	// that never returns, and the machinery for that is UAFLRoundManagerComponent::Server_CancelMatch(
	// EAFLMatchCancelReason) -- which exists in the working tree but is UNCOMMITTED by another workstream
	// (replay-cap / abandonment). A dropout is a THIRD cancel reason alongside Abandoned and ReplayCap.
	//
	// Adding a value to that enum now would collide with in-flight work, and building a second, parallel
	// cancel path is exactly what the S12-E scope says not to do. So this logs loudly and stops; wire it to
	// Server_CancelMatch once that change lands.
	UE_LOG(LogAFLGameCore, Warning,
		TEXT("AFL_RECONNECT: grace expired for '%s' -- seat released. MATCH NOT YET RESOLVED: wire this to "
		     "Server_CancelMatch(<dropout reason>) for the cancelled-refund ruling."),
		*PlayFabId);
}

void AAFLGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
	FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;   // stock already refused; do not overwrite its reason
	}

	// THE CONDITIONAL. Gate only when GAMELIFT IS LIVE, because only a GameLift-placed match has player
	// sessions that can be validated at all.
	//
	// ⚠ DO NOT substitute IsRosterExternallyOwned() here, however well it reads. That predicate is ALSO true
	// for the `?MatchmakerData=` launch option -- its documented fallback -- and the local dedicated-server
	// path runs on exactly that. Gating on it rejected every client of a local run, because a launch-option
	// match has no player sessions for anyone to present. Caught by test case 3 of the S12-E matrix, which
	// exists precisely because this is the easy mistake to make.
	//
	// The right question is not "does someone else own this roster" but "is there a session authority to ask".
	UAFLGameLiftHostSubsystem* GameLift = UAFLGameLiftHostSubsystem::Get(this);
	if (!GameLift || !GameLift->IsSdkReady())
	{
		return;   // PIE / listen server / offline / local ?MatchmakerData= -- nothing to validate against
	}

	const FString PlayerSessionId = UGameplayStatics::ParseOption(Options, TEXT("PlayerSessionId"));
	if (PlayerSessionId.IsEmpty())
	{
		ErrorMessage = TEXT("This match requires a player session. Rejoin through matchmaking.");
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AFL_PLAYERSESSION: REFUSED a connection with no ?PlayerSessionId= on a GameLift-hosted match."));
		return;
	}

	// Retry ONLY the not-yet-visible case. Measured propagation was 1.44s, so ~3s of budget is generous
	// without making a genuine rejection slow. A forged or completed id returns Rejected on the first call
	// and never enters this loop.
	FString ResolvedPlayerId;
	UAFLGameLiftHostSubsystem::EPlayerSessionCheck Check = UAFLGameLiftHostSubsystem::EPlayerSessionCheck::NotVisibleYet;
	for (int32 Attempt = 0; Attempt < 6; ++Attempt)
	{
		Check = GameLift->CheckAndAcceptPlayerSession(PlayerSessionId, ResolvedPlayerId);
		if (Check != UAFLGameLiftHostSubsystem::EPlayerSessionCheck::NotVisibleYet)
		{
			break;
		}
		FPlatformProcess::Sleep(0.5f);
	}

	if (Check != UAFLGameLiftHostSubsystem::EPlayerSessionCheck::Valid)
	{
		ErrorMessage = TEXT("Your player session is not valid for this match.");
		UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_PLAYERSESSION: REFUSED session '%s' (%s)."),
			*PlayerSessionId,
			Check == UAFLGameLiftHostSubsystem::EPlayerSessionCheck::NotVisibleYet ? TEXT("never became visible") : TEXT("rejected"));
		return;
	}

	// Hand the RESOLVED identity to InitNewPlayer, which owns the PlayerState.
	ValidatedPlayerIds.Add(PlayerSessionId, ResolvedPlayerId);
}

FString AAFLGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
	const FString& Options, const FString& Portal)
{
	// Stock init first (player name, spectator flag, etc.).
	const FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

	// T2 identity-join: stash the ?PlayFabId= reconcile key onto the PlayerState so UAFLMatchmakerDataProvider can
	// match the matchmaker roster (member.id) to this controller. A pure NO-OP for LocalFill / PIE joins (no
	// ?PlayFabId= present) -> safe on every live join. Not read until the matchmaker provider is active (S12).
	// S12-E: PREFER THE VALIDATED IDENTITY. `?PlayFabId=` is a CLAIM the client types; the resolved id is what
	// GameLift says this player session belongs to. Escrow debits whichever of these lands here, so on an
	// externally-owned roster the claim must never win.
	FString PlayFabId = UGameplayStatics::ParseOption(Options, TEXT("PlayFabId"));

	const FString PlayerSessionId = UGameplayStatics::ParseOption(Options, TEXT("PlayerSessionId"));
	if (const FString* Validated = PlayerSessionId.IsEmpty() ? nullptr : ValidatedPlayerIds.Find(PlayerSessionId))
	{
		// A mismatch is not fatal -- the session already decided who this is -- but it is worth an ERROR. The
		// benign cause is a stale client; the other cause is someone typing a different player's id.
		if (!PlayFabId.IsEmpty() && !PlayFabId.Equals(*Validated, ESearchCase::CaseSensitive))
		{
			UE_LOG(LogAFLGameCore, Error,
				TEXT("AFL_PLAYERSESSION: IDENTITY MISMATCH -- client claimed '%s' but session '%s' belongs to "
				     "'%s'. Using the session's answer."),
				*PlayFabId, *PlayerSessionId, **Validated);
		}
		PlayFabId = *Validated;
		ValidatedPlayerIds.Remove(PlayerSessionId);   // consumed

		// Remember which session this identity came from -- Logout runs too late to read connect options, and
		// releasing the seat when grace expires needs the session id.
		SessionIdByPlayFabId.Add(PlayFabId, PlayerSessionId);
	}

	// RECONNECT: this player was holding a seat. Cancel the grace timer -- they made it back.
	if (!PlayFabId.IsEmpty())
	{
		if (FTimerHandle* Pending = ReconnectTimers.Find(PlayFabId))
		{
			if (GetWorld())
			{
				GetWorld()->GetTimerManager().ClearTimer(*Pending);
			}
			ReconnectTimers.Remove(PlayFabId);
			UE_LOG(LogAFLGameCore, Log,
				TEXT("AFL_RECONNECT: '%s' RETURNED inside the grace window -- seat restored, no re-escrow."),
				*PlayFabId);
		}
	}

	if (!PlayFabId.IsEmpty() && NewPlayerController)
	{
		if (APlayerState* PS = NewPlayerController->PlayerState)
		{
			UAFLReconcileIdComponent* IdComp = PS->FindComponentByClass<UAFLReconcileIdComponent>();
			if (!IdComp)
			{
				IdComp = NewObject<UAFLReconcileIdComponent>(PS);
				IdComp->RegisterComponent();
			}
			IdComp->SetReconcileId(PlayFabId);
			UE_LOG(LogAFLGameCore, Log, TEXT("AFLTeams: identity-join -- stashed reconcile id '%s' on %s."),
				*PlayFabId, *GetNameSafe(PS));
		}
	}

	return Result;
}
