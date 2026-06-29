// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_SpectateOverlay.h"

#include "Character/LyraHealthComponent.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Round/AFLRoundManagerComponent.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_SpectateOverlay)


void UAFLW_SpectateOverlay::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);   // hidden until the local player is dead in a round
	TryArm();
}

void UAFLW_SpectateOverlay::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ArmRetryTimer);
	}
	Super::NativeDestruct();
}

void UAFLW_SpectateOverlay::TryArm()
{
	UWorld* World = GetWorld();
	AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	UAFLRoundManagerComponent* Resolved = GS ? GS->FindComponentByClass<UAFLRoundManagerComponent>() : nullptr;
	if (!Resolved)
	{
		// The GameState round component can replicate after widget construct -- bounded poll (mirrors Surface 1/2).
		if (World)
		{
			World->GetTimerManager().SetTimer(ArmRetryTimer,
				FTimerDelegate::CreateWeakLambda(this, [this] { TryArm(); }), 0.5f, false);
		}
		return;
	}
	Round = Resolved;
	Refresh();
}

void UAFLW_SpectateOverlay::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Round.IsValid())
	{
		Refresh();
	}
}

void UAFLW_SpectateOverlay::Refresh()
{
	UAFLRoundManagerComponent* R = Round.Get();
	if (!R)
	{
		return;
	}

	// Resolve the CURRENT local pawn's health comp -- re-resolve only on a pawn swap (respawn replaces the pawn,
	// so a cached delegate would dangle; reading the live pawn each tick clears the overlay on respawn for free).
	APawn* P = GetOwningPlayerPawn();
	if (P != LastPawn.Get())
	{
		LastPawn = P;
		Health = ULyraHealthComponent::FindHealthComponent(P);
	}
	const bool bDead = Health.IsValid() && Health->IsDeadOrDying();

	// Show only while dead AND in a round-play / round-end phase. Alive, WarmUp, or MatchEnd -> hidden
	// (the MATCH COMPLETE banner owns match end; warmup has no elimination state to surface).
	const EAFLRoundPhase Phase = R->Phase;
	const bool bRoundContext =
		(Phase == EAFLRoundPhase::RoundActive
		|| Phase == EAFLRoundPhase::RoundEnd
		|| Phase == EAFLRoundPhase::HalfTime);
	const bool bWantShown = bDead && bRoundContext;

	// Transition guard: nothing to do if the shown-state is unchanged AND (still hidden, or same phase while shown).
	const uint8 PhaseByte = static_cast<uint8>(Phase);
	if (bWantShown == bShown && (!bWantShown || PhaseByte == LastShownPhase))
	{
		return;
	}
	const bool bWasShown = bShown;
	bShown = bWantShown;
	LastShownPhase = PhaseByte;

	if (!bWantShown)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// Dead + in round context -> set the two lines by phase. RoundEnd / HalfTime = the ~5s respawn tail.
	const bool bResetTail = (Phase != EAFLRoundPhase::RoundActive);
	if (StatusText)
	{
		StatusText->SetText(bResetTail
			? NSLOCTEXT("AFL", "SpecRespawning", "RESPAWNING...")
			: NSLOCTEXT("AFL", "SpecEliminated", "ELIMINATED"));
	}
	if (DetailText)
	{
		DetailText->SetText(bResetTail
			? FText::GetEmpty()
			: NSLOCTEXT("AFL", "SpecRespawnNext", "RESPAWN NEXT ROUND"));
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);   // passive overlay -- never eats input
	if (!bWasShown)
	{
		OnSpectateShown();   // entrance anim ONLY on the hidden->shown transition (not on a phase-only re-text)
	}
}
