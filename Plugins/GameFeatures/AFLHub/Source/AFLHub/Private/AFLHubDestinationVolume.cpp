// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLHubDestinationVolume.h"

#include "AFLHub.h"
#include "AFLHubSignWidget.h"
#include "AFLHubTravelComponent.h"
#include "CommonActivatableWidget.h"
#include "CommonUIExtensions.h"
#include "UI/AFLW_LoadoutBase.h"
#include "GameplayTagContainer.h"
#include "InputCoreTypes.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLHubDestinationVolume)

const FAFLHubDestinationRow* UAFLHubDestinationsData::FindRow(FName InDestinationId) const
{
	for (const FAFLHubDestinationRow& Row : Rows)
	{
		if (Row.DestinationId == InDestinationId)
		{
			return &Row;
		}
	}
	return nullptr;
}

AAFLHubDestinationVolume::AAFLHubDestinationVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	PromptBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PromptBox"));
	PromptBox->SetBoxExtent(FVector(220.0f, 220.0f, 160.0f));
	PromptBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PromptBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	PromptBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SetRootComponent(PromptBox);

	SignWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("SignWidget"));
	SignWidget->SetupAttachment(PromptBox);
	SignWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 260.0f));
	SignWidget->SetWidgetSpace(EWidgetSpace::Screen);
	SignWidget->SetDrawAtDesiredSize(true);
}

void AAFLHubDestinationVolume::BeginPlay()
{
	Super::BeginPlay();

	// ROW RESOLUTION RUNS ON EVERY NET MODE: the SERVER validates Travel requests off these
	// resolved fields (GetTravelContract) -- guarding it behind the dedicated-server early-out
	// left every server-side door Disabled and refused every legitimate club hop.
	const FAFLHubDestinationRow* Row = Destinations ? Destinations->FindRow(DestinationId) : nullptr;
	if (Row)
	{
		ResolvedAction   = Row->Action;
		ResolvedName     = Row->DisplayName;
		ResolvedSubtitle = Row->Subtitle;
		ResolvedPayload  = Row->ActionPayload;
	}
	else
	{
		UE_LOG(LogAFLHub, Warning, TEXT("AFL_HUBDOOR: %s has no row for '%s' in %s -- sign shows the raw id."),
			*GetName(), *DestinationId.ToString(), *GetNameSafe(Destinations));
		ResolvedName = FText::FromName(DestinationId);
	}

	// COSMETIC-ONLY guard: a dedicated server has no screen and strips render components
	// (the separate-process PIE server crash law). Everything below is client sign dressing.
	if (GetNetMode() == NM_DedicatedServer || !SignWidget)
	{
		return;
	}

	if (Row)
	{
		ResolvedGlyph = Row->Glyph.LoadSynchronous();
	}

	SignWidget->SetWidgetClass(UAFLHubSignWidget::StaticClass());

	PromptBox->OnComponentBeginOverlap.AddDynamic(this, &AAFLHubDestinationVolume::OnDoorBeginOverlap);
	PromptBox->OnComponentEndOverlap.AddDynamic(this, &AAFLHubDestinationVolume::OnDoorEndOverlap);

	// 4 Hz tier decision: cheap (one distance per sign), and tier changes are slow by nature.
	GetWorldTimerManager().SetTimer(TierTimer, this, &AAFLHubDestinationVolume::UpdateSignTier, 0.25f, true);
	UpdateSignTier();

}

void AAFLHubDestinationVolume::OnExitPressed()
{
	// Pop OUR takeover if it is still up (a screen's own back handler may already have popped it
	// on the same press -- DeactivateWidget on a deactivated widget is a no-op).
	if (UCommonActivatableWidget* Screen = Cast<UCommonActivatableWidget>(PushedScreen.Get()))
	{
		if (Screen->IsActivated())
		{
			Screen->DeactivateWidget();
			UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBDOOR: EXIT closed '%s'."), *DestinationId.ToString());
		}
	}
}

void AAFLHubDestinationVolume::OnInteractPressed()
{
	// Re-entry gate: while our takeover lives, E belongs to the SCREEN, not the door (the at-door
	// state persists behind the UI -- an ungated second press stacked two lockers, and closing one
	// left the other covering the world with menu input mode: "map does not return, no locomotion").
	if (UCommonActivatableWidget* Screen = Cast<UCommonActivatableWidget>(PushedScreen.Get()))
	{
		if (Screen->IsActivated())
		{
			return; // takeover still up -- E belongs to the screen
		}
		PushedScreen.Reset(); // pooled widget lingers deactivated; clear so re-entry works
	}
	if (bPawnInVolume && ResolvedAction != EAFLHubDestinationAction::Disabled)
	{
		ExecuteDoorAction();
	}
}

void AAFLHubDestinationVolume::ExecuteDoorAction()
{
	switch (ResolvedAction)
	{
	case EAFLHubDestinationAction::OpenScreen:
	{
		// Payload: "<widget class path>" or "<widget class path>|Creator" -- the suffix stacks the
		// CC on the pushed loadout via its own OpenCreator (the armory flow, verbatim).
		FString ClassPath = ResolvedPayload;
		FString Mode;
		ClassPath.Split(TEXT("|"), &ClassPath, &Mode);
		if (ClassPath.IsEmpty()) { ClassPath = ResolvedPayload; }
		UClass* WidgetClass = LoadClass<UCommonActivatableWidget>(nullptr, *ClassPath);
		if (!WidgetClass)
		{
			UE_LOG(LogAFLHub, Warning, TEXT("AFL_HUBDOOR: '%s' OpenScreen payload '%s' did not resolve to a UCommonActivatableWidget class."),
				*DestinationId.ToString(), *ResolvedPayload);
			return;
		}
		APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
		if (!LP)
		{
			return;
		}
		// WORLD-OVERLAY hint: over a live map the loadout screen must not leave its display pawn
		// behind (the armory set-piece behavior). One-shot, consumed by the screen.
		UAFLW_LoadoutBase::bNextOpenIsWorldOverlay = true;
		// The proven takeover mount: UI.Layer.Menu fills the viewport; deactivate pops back to the hub.
		PushedScreen = UCommonUIExtensions::PushContentToLayer_ForPlayer(LP,
			FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Menu")), TSubclassOf<UCommonActivatableWidget>(WidgetClass));
		UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBDOOR: OPEN '%s' -> %s."), *DestinationId.ToString(), *WidgetClass->GetName());
		if (Mode == TEXT("Creator"))
		{
			if (UAFLW_LoadoutBase* Loadout = Cast<UAFLW_LoadoutBase>(PushedScreen.Get()))
			{
				Loadout->OpenCreator(INDEX_NONE);
				UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBDOOR: '%s' stacked the Creator on the loadout."), *DestinationId.ToString());
			}
			else
			{
				UE_LOG(LogAFLHub, Warning, TEXT("AFL_HUBDOOR: '%s' |Creator payload needs a UAFLW_LoadoutBase screen."), *DestinationId.ToString());
			}
		}
		break;
	}
	case EAFLHubDestinationAction::Travel:
	{
		// Travel moves the SESSION, and this actor has no owning connection -- the request rides
		// the player-owned pawn's travel component, and the SERVER re-resolves + validates the row.
		APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		UAFLHubTravelComponent* Travel = Pawn ? Pawn->FindComponentByClass<UAFLHubTravelComponent>() : nullptr;
		if (!Travel)
		{
			UE_LOG(LogAFLHub, Warning, TEXT("AFL_HUBDOOR: '%s' Travel refused -- no UAFLHubTravelComponent on the pawn."),
				*DestinationId.ToString());
			return;
		}
		UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBDOOR: TRAVEL '%s' requested."), *DestinationId.ToString());
		Travel->ServerRequestHubTravel(DestinationId);
		break;
	}
	case EAFLHubDestinationAction::WalkIn:
		// An open venue: the door is an ENTRANCE, the experience is inside (the PX walkable store).
		UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBDOOR: '%s' is a walk-in venue -- step inside."), *DestinationId.ToString());
		break;
	default:
		UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBDOOR: '%s' action %d has no backend yet."),
			*DestinationId.ToString(), static_cast<int32>(ResolvedAction));
		break;
	}
}

bool AAFLHubDestinationVolume::GetTravelContract(FName& OutId, EAFLHubDestinationAction& OutAction, FString& OutPayload) const
{
	OutId = DestinationId;
	OutAction = ResolvedAction;
	OutPayload = ResolvedPayload;
	return !DestinationId.IsNone();
}

void AAFLHubDestinationVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TierTimer);
	}
	Super::EndPlay(EndPlayReason);
}

bool AAFLHubDestinationVolume::IsLocalPlayerPawn(const AActor* Actor)
{
	const APawn* Pawn = Cast<APawn>(Actor);
	return Pawn && Pawn->IsLocallyControlled() && Pawn->IsPlayerControlled();
}

void AAFLHubDestinationVolume::OnDoorBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!IsLocalPlayerPawn(OtherActor))
	{
		return;
	}
	bPawnInVolume = true;
	// LAZY interact bind at the first at-door moment (level BeginPlay can precede the PC's input
	// component; binding here is possession-safe). bConsumeInput=false keeps E free for the grab
	// kit. No project-wide IA_Interact exists yet -- consolidate when the interaction pass lands.
	if (!bInteractBound && ResolvedAction != EAFLHubDestinationAction::Disabled)
	{
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			if (PC->IsLocalController() && PC->InputComponent)
			{
				FInputKeyBinding& KB = PC->InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AAFLHubDestinationVolume::OnInteractPressed);
				KB.bConsumeInput = false;
				FInputKeyBinding& GB = PC->InputComponent->BindKey(EKeys::Gamepad_FaceButton_Left, IE_Pressed, this, &AAFLHubDestinationVolume::OnInteractPressed);
				GB.bConsumeInput = false;
				// Uniform door-exit: ESC / gamepad B closes OUR pushed screen even when the screen
				// has no back handler of its own (the front-end Home is a root -- it never needed one).
				FInputKeyBinding& EB = PC->InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AAFLHubDestinationVolume::OnExitPressed);
				EB.bConsumeInput = false;
				FInputKeyBinding& XB = PC->InputComponent->BindKey(EKeys::Gamepad_FaceButton_Right, IE_Pressed, this, &AAFLHubDestinationVolume::OnExitPressed);
				XB.bConsumeInput = false;
				bInteractBound = true;
				UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBDOOR: interact keys bound for '%s'."), *DestinationId.ToString());
			}
		}
	}
	UpdateSignTier();
	UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBDOOR: AT-DOOR '%s' (%s) for %s."),
		*DestinationId.ToString(),
		ResolvedAction == EAFLHubDestinationAction::Disabled ? TEXT("OFFLINE") : TEXT("ONLINE"),
		*GetNameSafe(OtherActor));
}

void AAFLHubDestinationVolume::OnDoorEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	if (!IsLocalPlayerPawn(OtherActor))
	{
		return;
	}
	bPawnInVolume = false;
	UpdateSignTier();
	UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBDOOR: left '%s'."), *DestinationId.ToString());
}

void AAFLHubDestinationVolume::UpdateSignTier()
{
	UAFLHubSignWidget* Sign = SignWidget ? Cast<UAFLHubSignWidget>(SignWidget->GetWidget()) : nullptr;
	if (!Sign)
	{
		return;
	}

	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam)
	{
		return;
	}
	const float DistUnits  = FVector::Dist(Cam->GetCameraLocation(), GetActorLocation());
	const float DistMeters = DistUnits / 100.0f;

	// Ratified tiers: AT-DOOR = the box overlap; MID 15-40m; FAR beyond.
	EAFLHubSignTier Tier = EAFLHubSignTier::Far;
	if (bPawnInVolume || DistMeters < 15.0f)
	{
		Tier = EAFLHubSignTier::AtDoor;
	}
	else if (DistMeters < 40.0f)
	{
		Tier = EAFLHubSignTier::Mid;
	}

	Sign->SetSignData(ResolvedName, ResolvedSubtitle,
		ResolvedAction != EAFLHubDestinationAction::Disabled, Tier, DistMeters, ResolvedGlyph);
}
