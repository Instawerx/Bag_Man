// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLRetailTill.h"

#include "AFLHub.h"
#include "AFLHubSignWidget.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Pawn.h"
#include "Retail/AFLRetailSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLRetailTill)

AAFLRetailTill::AAFLRetailTill()
{
	CounterBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CounterBox"));
	SetRootComponent(CounterBox);
	CounterBox->SetBoxExtent(FVector(140.f, 140.f, 120.f));
	CounterBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CounterBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	CounterBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	SignWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("SignWidget"));
	SignWidget->SetupAttachment(CounterBox);
	SignWidget->SetRelativeLocation(FVector(0.f, 0.f, 180.f));
	SignWidget->SetWidgetSpace(EWidgetSpace::Screen);
	SignWidget->SetDrawAtDesiredSize(true);
}

void AAFLRetailTill::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return; // client-local surface; the dedicated server strips retail UX entirely
	}
	// LAP-2 (operator): no floating screen-space sign -- the till surfaces ONLY as the pinned
	// lower-right cart chip while you stand at the counter. SignWidget stays a component (saved
	// instances carry it) but never gets a widget class, so nothing renders.
	CounterBox->OnComponentBeginOverlap.AddDynamic(this, &AAFLRetailTill::OnTillBeginOverlap);
	CounterBox->OnComponentEndOverlap.AddDynamic(this, &AAFLRetailTill::OnTillEndOverlap);
}

void AAFLRetailTill::OnTillBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}
	if (UAFLRetailSubsystem* Retail = UAFLRetailSubsystem::Get(this))
	{
		Retail->OpenTill();
	}
	UE_LOG(LogAFLHub, Log, TEXT("AFL_RETAIL: at the TILL."));
}

void AAFLRetailTill::OnTillEndOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}
	if (UAFLRetailSubsystem* Retail = UAFLRetailSubsystem::Get(this))
	{
		Retail->CloseTill();
	}
}
