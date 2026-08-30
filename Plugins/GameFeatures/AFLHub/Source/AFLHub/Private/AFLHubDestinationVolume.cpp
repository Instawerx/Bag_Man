// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLHubDestinationVolume.h"

#include "AFLHub.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Pawn.h"

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

	NameText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameText"));
	NameText->SetupAttachment(PromptBox);
	NameText->SetRelativeLocation(FVector(0.0f, 0.0f, 210.0f));
	NameText->SetHorizontalAlignment(EHTA_Center);
	NameText->SetWorldSize(44.0f);
	NameText->SetTextRenderColor(FColor(90, 220, 255));
	NameText->SetVisibility(false);

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
	StatusText->SetupAttachment(PromptBox);
	StatusText->SetRelativeLocation(FVector(0.0f, 0.0f, 160.0f));
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetWorldSize(24.0f);
	StatusText->SetVisibility(false);
}

void AAFLHubDestinationVolume::BeginPlay()
{
	Super::BeginPlay();

	// COSMETIC-ONLY guard: a dedicated server has no screen, and it STRIPS render components --
	// the TextRender pointers are null there (separate-process PIE server crashed on SetText;
	// single-process PIE masked it). Everything below is client prompt dressing.
	if (GetNetMode() == NM_DedicatedServer || !NameText || !StatusText)
	{
		return;
	}

	const FAFLHubDestinationRow* Row = Destinations ? Destinations->FindRow(DestinationId) : nullptr;
	if (!Row)
	{
		// Loud, once, and the prompt stays generic -- a missing row is an authoring error, not a crash.
		UE_LOG(LogAFLHub, Warning, TEXT("AFL_HUBDOOR: %s has no row for '%s' in %s -- prompt shows the raw id."),
			*GetName(), *DestinationId.ToString(), *GetNameSafe(Destinations));
		NameText->SetText(FText::FromName(DestinationId));
		StatusText->SetText(NSLOCTEXT("AFLHub", "DoorNoRow", "OFFLINE"));
		StatusText->SetTextRenderColor(FColor(255, 90, 90));
		return;
	}

	ResolvedAction = Row->Action;
	NameText->SetText(Row->DisplayName);
	if (ResolvedAction == EAFLHubDestinationAction::Disabled)
	{
		StatusText->SetText(NSLOCTEXT("AFLHub", "DoorOffline", "OFFLINE"));
		StatusText->SetTextRenderColor(FColor(255, 90, 90));
	}
	else
	{
		StatusText->SetText(NSLOCTEXT("AFLHub", "DoorOnline", "ONLINE"));
		StatusText->SetTextRenderColor(FColor(120, 255, 150));
	}

	PromptBox->OnComponentBeginOverlap.AddDynamic(this, &AAFLHubDestinationVolume::OnDoorBeginOverlap);
	PromptBox->OnComponentEndOverlap.AddDynamic(this, &AAFLHubDestinationVolume::OnDoorEndOverlap);
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
	NameText->SetVisibility(true);
	StatusText->SetVisibility(true);
	UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBDOOR: prompt SHOWN '%s' (%s) for %s."),
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
	NameText->SetVisibility(false);
	StatusText->SetVisibility(false);
	UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBDOOR: prompt hidden '%s'."), *DestinationId.ToString());
}
