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
	if (SignWidget)
	{
		SignWidget->SetWidgetClass(UAFLHubSignWidget::StaticClass());
		if (UAFLHubSignWidget* Sign = Cast<UAFLHubSignWidget>(SignWidget->GetWidget()))
		{
			Sign->SetSignData(NSLOCTEXT("AFLRetail", "TillName", "TILL"),
				NSLOCTEXT("AFLRetail", "TillSub", "CHECKOUT — X confirms the cart"),
				true, EAFLHubSignTier::AtDoor, 0.f, nullptr);
		}
	}
	CounterBox->OnComponentBeginOverlap.AddDynamic(this, &AAFLRetailTill::OnTillBeginOverlap);
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
