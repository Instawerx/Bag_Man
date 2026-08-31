// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDisplayPedestal.h"

#include "AFLHub.h"
#include "AFLHubSignWidget.h"
#include "AFLCosmeticCatalogSubsystem.h"
#include "AFLCosmeticCoreTypes.h"
#include "Cosmetics/AFLSkinColorAsset.h"     // facemask MIC for the head-bust display
#include "AFLHubMirror.h"                     // UAFLHubMirrorWidget: the RT/texture-plate widget, reused for thumbnails
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceConstant.h"
#include "UObject/ConstructorHelpers.h"
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
	DisplayProp->SetRelativeLocation(FVector(0.f, 0.f, 118.f)); // float the item at chest height
	DisplayProp->SetRelativeScale3D(FVector(1.5f));
	DisplayProp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// The visible platform (lap-2): dark brand cylinder, editor-visible so placement reads instantly.
	PlinthMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlinthMesh"));
	PlinthMesh->SetupAttachment(RootComponent);
	PlinthMesh->SetRelativeLocation(FVector(0.f, 0.f, 12.f));
	PlinthMesh->SetRelativeScale3D(FVector(0.75f, 0.75f, 0.25f));
	PlinthMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cyl.Succeeded())
	{
		PlinthMesh->SetStaticMesh(Cyl.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DarkMI(
		TEXT("/Game/BagMan/Characters/Cosmetics/IRONICS_Blank/MI_AFL_IRONICS_Body.MI_AFL_IRONICS_Body"));
	if (DarkMI.Succeeded())
	{
		PlinthMesh->SetMaterial(0, DarkMI.Object);
	}
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
	ResolveRetailDisplay();
}

void AAFLDisplayPedestal::ResolveRetailDisplay()
{
	if (CosmeticId.IsNone())
	{
		return;
	}
	const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(GetWorld());
	const FAFLCatalogEntry* Entry = Catalog ? Catalog->FindEntry(CosmeticId) : nullptr;
	if (!Entry)
	{
		UE_LOG(LogAFLHub, Warning, TEXT("AFL_RETAIL: pad '%s' has no catalog row -- no display."), *CosmeticId.ToString());
		return;
	}

	// ACCESSORY rows: the part BP IS the product -- spawn it as the display (real chain/watch/pendant
	// mesh; the chain's pendant refresh fails soft to a bare chain with no loadout, which is correct).
	if (!Entry->AccessoryPartClass.IsNull())
	{
		if (UClass* PartClass = Entry->AccessoryPartClass.LoadSynchronous())
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.Owner = this;
			DisplayPartActor = GetWorld()->SpawnActor<AActor>(PartClass,
				DisplayProp->GetComponentTransform(), Params);
			if (DisplayPartActor)
			{
				DisplayPartActor->AttachToComponent(DisplayProp, FAttachmentTransformRules::KeepWorldTransform);
				DisplayPartActor->SetActorEnableCollision(false);
				UE_LOG(LogAFLHub, Log, TEXT("AFL_RETAIL: pad '%s' displaying part actor %s."),
					*CosmeticId.ToString(), *GetNameSafe(DisplayPartActor));
			}
		}
		return;
	}

	// SHOP THUMBNAIL rows (facemasks all carry T_Thumb_Facemask_*): a floating upright thumbnail
	// plate on the plinth -- brand-consistent, readable, spins with the fixture. Replaces the lap-2
	// painted-gib attempt ("flat on floor").
	if (!Entry->ShopThumbnail.IsNull())
	{
		if (UTexture2D* Thumb = Entry->ShopThumbnail.LoadSynchronous())
		{
			ThumbTexture = Thumb;
			ThumbPlate = NewObject<UWidgetComponent>(this, TEXT("ThumbPlate"));
			ThumbPlate->SetupAttachment(DisplayProp);
			ThumbPlate->RegisterComponent();
			ThumbPlate->SetWidgetSpace(EWidgetSpace::World);
			ThumbPlate->SetDrawSize(FVector2D(512.f, 512.f));
			ThumbPlate->SetRelativeScale3D(FVector(0.11f)); // ~84cm plate at the 1.5x DisplayProp scale
			ThumbPlate->SetWidgetClass(UAFLHubMirrorWidget::StaticClass());
			ThumbPlate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			// The widget itself is created late -- UpdatePlate's 0.5s timer pushes the texture (idempotent).
			UE_LOG(LogAFLHub, Log, TEXT("AFL_RETAIL: pad '%s' displaying shop thumbnail plate."), *CosmeticId.ToString());
			return;
		}
	}

	// LAST RESORT (no part class, no thumbnail): facemask MIC painted over the head bust on every
	// slot (the AAFLDismemberedHead recipe).
	if (UAFLCosmeticCatalogSubsystem* MutableCatalog = UAFLCosmeticCatalogSubsystem::Get(GetWorld()))
	{
		if (const UAFLSkinColorAsset* MaskAsset = Cast<UAFLSkinColorAsset>(MutableCatalog->ResolveAsset(CosmeticId)))
		{
			if (UMaterialInstanceConstant* MaskMIC = MaskAsset->GetFacemaskMaterial())
			{
				if (UStaticMesh* Bust = FacemaskBustMesh.LoadSynchronous())
				{
					DisplayProp->SetStaticMesh(Bust);
					const int32 NumSlots = DisplayProp->GetNumMaterials();
					for (int32 i = 0; i < NumSlots; ++i)
					{
						DisplayProp->SetMaterial(i, MaskMIC);
					}
					UE_LOG(LogAFLHub, Log, TEXT("AFL_RETAIL: pad '%s' displaying mask bust (%d slots painted)."),
						*CosmeticId.ToString(), NumSlots);
				}
			}
		}
	}
}

void AAFLDisplayPedestal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The retail turntable: slow spin on the display fixture (and anything attached to it).
	if (DisplayProp && (DisplayProp->GetStaticMesh() || DisplayPartActor))
	{
		DisplayProp->AddLocalRotation(FRotator(0.f, 40.f * DeltaSeconds, 0.f));
	}
}

void AAFLDisplayPedestal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (DisplayPartActor)
	{
		DisplayPartActor->Destroy(); // attached actors do not auto-destroy with their base
		DisplayPartActor = nullptr;
	}
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
	// Feed the thumbnail plate here (2 Hz, idempotent) -- its inner widget is created late.
	if (ThumbPlate && ThumbTexture)
	{
		if (UAFLHubMirrorWidget* PlateImg = Cast<UAFLHubMirrorWidget>(ThumbPlate->GetWidget()))
		{
			PlateImg->SetMirrorTexture(ThumbTexture, FVector2D(512.f, 512.f));
		}
	}

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
