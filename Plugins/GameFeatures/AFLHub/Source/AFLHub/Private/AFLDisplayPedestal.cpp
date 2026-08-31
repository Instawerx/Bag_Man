// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDisplayPedestal.h"

#include "AFLHub.h"
#include "AFLHubSignWidget.h"
#include "AFLCosmeticCatalogSubsystem.h"
#include "AFLCosmeticCoreTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDisplayPedestal)

AAFLDisplayPedestal::AAFLDisplayPedestal()
{
	// TIGHT pad (operator law: the item's own footprint, ~1m -- crossing a shop floor never dresses you).
	ShelfBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ShelfBox"));
	ShelfBox->SetupAttachment(RootComponent);
	ShelfBox->SetBoxExtent(FVector(90.f, 90.f, 110.f));
	ShelfBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ShelfBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	ShelfBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	PlateWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PlateWidget"));
	PlateWidget->SetupAttachment(RootComponent);
	PlateWidget->SetRelativeLocation(FVector(0.f, 0.f, 170.f));
	PlateWidget->SetWidgetSpace(EWidgetSpace::Screen);
	PlateWidget->SetDrawAtDesiredSize(true);

	DisplayProp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayProp"));
	DisplayProp->SetupAttachment(RootComponent);
	DisplayProp->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	DisplayProp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AAFLDisplayPedestal::BeginPlay()
{
	Super::BeginPlay(); // the spawner half: registry resolve + pad visuals (the AAA display)

	// COSMETIC-ONLY below (s4 doctrine: retail visuals are client-local, 0% server overhead).
	if (GetNetMode() == NM_DedicatedServer || !PlateWidget)
	{
		return;
	}

	PlateWidget->SetWidgetClass(UAFLHubSignWidget::StaticClass());
	PlateWidget->SetVisibility(false); // at-item only; UpdatePlate raises it inside 4m with LOS
	ShelfBox->OnComponentBeginOverlap.AddDynamic(this, &AAFLDisplayPedestal::OnPadBeginOverlap);
	ShelfBox->OnComponentEndOverlap.AddDynamic(this, &AAFLDisplayPedestal::OnPadEndOverlap);
	GetWorldTimerManager().SetTimer(PlateTimer, this, &AAFLDisplayPedestal::UpdatePlate, 0.5f, true);
	UpdatePlate();
}

void AAFLDisplayPedestal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PlateTimer);
		if (bPawnAtShelf)
		{
			if (UAFLRetailSubsystem* Retail = UAFLRetailSubsystem::Get(this))
			{
				Retail->PadLeft(CosmeticId);
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AAFLDisplayPedestal::AttemptPickUpWeapon_Implementation(APawn* /*Pawn*/)
{
	// RETAIL: the pad never grants by touch. Deliberately empty -- the subsystem owns the verbs.
}

void AAFLDisplayPedestal::OnPadBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}
	bPawnAtShelf = true;
	if (UAFLRetailSubsystem* Retail = UAFLRetailSubsystem::Get(this))
	{
		Retail->PadEntered(CosmeticId, ArmMode, DwellSeconds, Pawn);
	}
	UpdatePlate();
	UE_LOG(LogAFLHub, Log, TEXT("AFL_RETAIL: ON-PAD '%s'."), *CosmeticId.ToString());
}

void AAFLDisplayPedestal::OnPadEndOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}
	bPawnAtShelf = false;
	if (UAFLRetailSubsystem* Retail = UAFLRetailSubsystem::Get(this))
	{
		Retail->PadLeft(CosmeticId);
	}
	UpdatePlate();
}

void AAFLDisplayPedestal::UpdatePlate()
{
	UAFLHubSignWidget* Plate = PlateWidget ? Cast<UAFLHubSignWidget>(PlateWidget->GetWidget()) : nullptr;
	if (!Plate)
	{
		return;
	}

	// AT-ITEM ONLY (plan bug-fix 1): the plate renders inside 4m WITH line of sight -- never through
	// walls, never across the venue. Distance tiers are retired for products (door signs keep theirs).
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const float Meters = Pawn ? FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) / 100.f : 999.f;
	bool bShow = bPawnAtShelf || Meters < 4.f;
	if (bShow && Pawn && PC && PC->PlayerCameraManager)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RetailPlateLOS), false, this);
		Params.AddIgnoredActor(Pawn);
		const FVector From = PC->PlayerCameraManager->GetCameraLocation();
		const FVector To = GetActorLocation() + FVector(0.f, 0.f, 120.f);
		bShow = !GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params);
	}
	PlateWidget->SetVisibility(bShow);
	if (!bShow)
	{
		return;
	}

	FText Name = FText::FromName(CosmeticId);
	FText PriceLine = FText::GetEmpty();
	bool bSellable = false;
	if (const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(GetWorld()))
	{
		if (const FAFLCatalogEntry* Entry = Catalog->FindEntry(CosmeticId))
		{
			Name = Entry->DisplayName.IsEmpty() ? Name : Entry->DisplayName;
			PriceLine = Catalog->GetEntryPriceText(*Entry);
			bSellable = Entry->bTransactable;
		}
	}
	Plate->SetSignData(Name, PriceLine, bSellable, EAFLHubSignTier::AtDoor, Meters, nullptr);
}
