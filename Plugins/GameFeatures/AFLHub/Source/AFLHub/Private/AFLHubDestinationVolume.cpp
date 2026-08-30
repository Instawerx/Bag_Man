// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLHubDestinationVolume.h"

#include "AFLHub.h"
#include "AFLHubSignWidget.h"
#include "CommonActivatableWidget.h"
#include "CommonUIExtensions.h"
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

	// COSMETIC-ONLY guard: a dedicated server has no screen and strips render components
	// (the separate-process PIE server crash law). Everything below is client sign dressing.
	if (GetNetMode() == NM_DedicatedServer || !SignWidget)
	{
		return;
	}

	const FAFLHubDestinationRow* Row = Destinations ? Destinations->FindRow(DestinationId) : nullptr;
	if (Row)
	{
		ResolvedAction   = Row->Action;
		ResolvedName     = Row->DisplayName;
		ResolvedSubtitle = Row->Subtitle;
		ResolvedGlyph    = Row->Glyph.LoadSynchronous();
		ResolvedPayload  = Row->ActionPayload;
	}
	else
	{
		UE_LOG(LogAFLHub, Warning, TEXT("AFL_HUBDOOR: %s has no row for '%s' in %s -- sign shows the raw id."),
			*GetName(), *DestinationId.ToString(), *GetNameSafe(Destinations));
		ResolvedName = FText::FromName(DestinationId);
	}

	SignWidget->SetWidgetClass(UAFLHubSignWidget::StaticClass());

	PromptBox->OnComponentBeginOverlap.AddDynamic(this, &AAFLHubDestinationVolume::OnDoorBeginOverlap);
	PromptBox->OnComponentEndOverlap.AddDynamic(this, &AAFLHubDestinationVolume::OnDoorEndOverlap);

	// 4 Hz tier decision: cheap (one distance per sign), and tier changes are slow by nature.
	GetWorldTimerManager().SetTimer(TierTimer, this, &AAFLHubDestinationVolume::UpdateSignTier, 0.25f, true);
	UpdateSignTier();

}

void AAFLHubDestinationVolume::OnInteractPressed()
{
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
		UClass* WidgetClass = LoadClass<UCommonActivatableWidget>(nullptr, *ResolvedPayload);
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
		// The proven takeover mount: UI.Layer.Menu fills the viewport; deactivate pops back to the hub.
		UCommonUIExtensions::PushContentToLayer_ForPlayer(LP,
			FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Menu")), TSubclassOf<UCommonActivatableWidget>(WidgetClass));
		UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBDOOR: OPEN '%s' -> %s."), *DestinationId.ToString(), *WidgetClass->GetName());
		break;
	}
	default:
		UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBDOOR: '%s' action %d has no backend yet."),
			*DestinationId.ToString(), static_cast<int32>(ResolvedAction));
		break;
	}
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
