// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLRetailStickerWall.h"

#include "AFLHub.h"
#include "AFLDisplayPedestal.h"
#include "AFLHubMirror.h" // UAFLHubMirrorWidget: thumbnail-plate fallback (generalized wall)
#include "AFLCosmeticCatalogSubsystem.h"
#include "AFLCosmeticCoreTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLRetailStickerWall)

// --- UAFLStickerCropWidget -------------------------------------------------------------------------

TSharedRef<SWidget> UAFLStickerCropWidget::RebuildWidget()
{
	Window = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Window"));
	WidgetTree->RootWidget = Window;
	Window->SetClipping(EWidgetClipping::ClipToBounds);
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CropCanvas"));
	Window->AddChild(Canvas);
	AtlasImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("AtlasImage"));
	Canvas->AddChildToCanvas(AtlasImage);
	return Super::RebuildWidget();
}

void UAFLStickerCropWidget::SetTile(UTexture2D* Atlas, int32 TileIndex, float PlateSize)
{
	if (!Window || !AtlasImage || !Atlas || TileIndex < 0)
	{
		return;
	}
	Window->SetWidthOverride(PlateSize);
	Window->SetHeightOverride(PlateSize);
	FSlateBrush Brush;
	Brush.SetResourceObject(Atlas);
	Brush.ImageSize = FVector2D(PlateSize * 4.f, PlateSize * 4.f);
	AtlasImage->SetBrush(Brush);
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(AtlasImage->Slot))
	{
		const int32 Col = TileIndex % 4;
		const int32 Row = TileIndex / 4;
		S->SetPosition(FVector2D(-Col * PlateSize, -Row * PlateSize));
		S->SetSize(FVector2D(PlateSize * 4.f, PlateSize * 4.f));
	}
}

// --- AAFLRetailStickerWall -------------------------------------------------------------------------

AAFLRetailStickerWall::AAFLRetailStickerWall()
{
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
	WallMesh->SetupAttachment(Root);
	WallMesh->SetRelativeLocation(FVector(0.f, 0.f, 175.f));
	WallMesh->SetRelativeScale3D(FVector(0.25f, 7.2f, 3.5f)); // 25 x 720 x 350 slab
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		WallMesh->SetStaticMesh(Cube.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DarkMI(
		TEXT("/Game/BagMan/Characters/Cosmetics/IRONICS_Blank/MI_AFL_IRONICS_Body.MI_AFL_IRONICS_Body"));
	if (DarkMI.Succeeded())
	{
		WallMesh->SetMaterial(0, DarkMI.Object);
	}
}

void AAFLRetailStickerWall::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	BuildMural();
}

void AAFLRetailStickerWall::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (AAFLDisplayPedestal* Pad : Pads)
	{
		if (Pad)
		{
			Pad->Destroy();
		}
	}
	Pads.Reset();
	Super::EndPlay(EndPlayReason);
}

void AAFLRetailStickerWall::BuildMural()
{
	const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(GetWorld());
	if (!Catalog)
	{
		UE_LOG(LogAFLHub, Warning, TEXT("AFL_RETAIL: sticker wall '%s' -- no catalog."), *GetNameSafe(this));
		return;
	}
	// Atlas is only needed for TILE rows; a thumbnail-only wall (future MARKS wall) runs without it.
	UTexture2D* Atlas = StickerAtlas.IsNull() ? nullptr : StickerAtlas.LoadSynchronous();

	// GRAFFITI SCATTER -- deterministic per index (same wall every boot): tilts, sizes, loose rows.
	static const float Yoff[]  = { -290.f, -160.f, -35.f, 90.f, 215.f, 300.f, -250.f, -115.f, 15.f, 145.f, 270.f, -200.f, -60.f, 75.f, 205.f, -120.f };
	static const float Zoff[]  = { 250.f, 265.f, 240.f, 258.f, 238.f, 252.f, 150.f, 138.f, 155.f, 142.f, 132.f, 58.f, 52.f, 64.f, 55.f, 95.f };
	static const float Tilt[]  = { -12.f, 8.f, -5.f, 14.f, -9.f, 4.f, 10.f, -14.f, 6.f, -7.f, 12.f, -4.f, 9.f, -11.f, 5.f, -8.f };
	static const float Sizes[] = { 95.f, 78.f, 112.f, 84.f, 100.f, 74.f, 92.f, 106.f, 80.f, 96.f, 86.f, 102.f, 90.f, 112.f, 82.f, 94.f };
	constexpr int32 PatternNum = UE_ARRAY_COUNT(Yoff);

	int32 Placed = 0;
	for (int32 i = 0; i < StickerIds.Num(); ++i)
	{
		const FAFLCatalogEntry* Entry = Catalog->FindEntry(StickerIds[i]);
		if (!Entry)
		{
			continue;
		}
		// GENERALIZED (polish pass): a row renders via its atlas TILE when it has one, else via its
		// SHOP THUMBNAIL -- so the same wall class serves a future MARKS/emblem wall the moment
		// T_Thumb_Emblem_* art lands (no thumbnails exist for emblems today; art ask flagged).
		UTexture2D* Thumb = nullptr;
		if (Entry->StickerAtlasTile < 0)
		{
			Thumb = Entry->ShopThumbnail.IsNull() ? nullptr : Entry->ShopThumbnail.LoadSynchronous();
			if (!Thumb)
			{
				continue; // inert pack row / no visual -- nothing to hang
			}
		}
		else if (!Atlas)
		{
			continue; // tile row with no atlas loaded -- never hang a blank plate
		}
		const int32 P = Placed % PatternNum;
		const float PlateSize = Sizes[P];

		UWidgetComponent* Plate = NewObject<UWidgetComponent>(this);
		Plate->SetupAttachment(RootComponent);
		Plate->RegisterComponent();
		Plate->SetWidgetSpace(EWidgetSpace::World);
		Plate->SetDrawSize(FVector2D(PlateSize, PlateSize));
		// Front face of the 25-thick slab is at x=+12.5; float the plate just off it, tilted.
		Plate->SetRelativeLocation(FVector(14.f, Yoff[P], Zoff[P]));
		Plate->SetRelativeRotation(FRotator(0.f, 0.f, Tilt[P]));
		Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Thumb)
		{
			Plate->SetWidgetClass(UAFLHubMirrorWidget::StaticClass());
			Plate->InitWidget();
			if (UAFLHubMirrorWidget* Img = Cast<UAFLHubMirrorWidget>(Plate->GetWidget()))
			{
				Img->SetMirrorTexture(Thumb, FVector2D(PlateSize, PlateSize));
			}
		}
		else
		{
			Plate->SetWidgetClass(UAFLStickerCropWidget::StaticClass());
			Plate->InitWidget();
			if (UAFLStickerCropWidget* Crop = Cast<UAFLStickerCropWidget>(Plate->GetWidget()))
			{
				Crop->SetTile(Atlas, Entry->StickerAtlasTile, PlateSize);
			}
		}
		++Placed;
	}
	UE_LOG(LogAFLHub, Log, TEXT("AFL_RETAIL: sticker wall '%s' -- %d sticker plates placed."), *GetNameSafe(this), Placed);

	// The sellable pads (credit packs), display-suppressed, spread along the wall front.
	UWorld* World = GetWorld();
	for (int32 i = 0; i < PadCosmeticIds.Num() && World; ++i)
	{
		const float Y = (PadCosmeticIds.Num() == 1) ? 0.f : (-170.f + i * (340.f / FMath::Max(1, PadCosmeticIds.Num() - 1)));
		const FVector PadLoc = GetActorTransform().TransformPosition(FVector(190.f, Y, 0.f));
		const FRotator PadRot(0.f, GetActorRotation().Yaw + 180.f, 0.f);
		const FTransform PadXf(PadRot, PadLoc);
		AAFLDisplayPedestal* Pad = World->SpawnActorDeferred<AAFLDisplayPedestal>(
			AAFLDisplayPedestal::StaticClass(), PadXf, this, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Pad)
		{
			Pad->CosmeticId = PadCosmeticIds[i];
			Pad->bSuppressDisplay = true;
			Pad->FinishSpawning(PadXf);
			Pads.Add(Pad);
		}
	}
}
