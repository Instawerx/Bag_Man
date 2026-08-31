// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLRetailWeaponTower.h"

#include "AFLHub.h"
#include "AFLDisplayPedestal.h"
#include "AFLHubMirror.h" // UAFLHubMirrorWidget -- the texture-plate widget (thumbnail fallback)
#include "AFLCosmeticCatalogSubsystem.h"
#include "AFLCosmeticCoreTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Cosmetics/AFLWeaponCosmeticAsset.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Equipment/LyraEquipmentDefinition.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLRetailWeaponTower)

namespace AFLWeaponTower
{
	/** Catalog row -> the equipped DISPLAY ACTOR class + WID attach scale. The actor route replaces
	 *  the SCS mesh walk (lap-5: every mesh lives in a CHILD-class override record the parent's SCS
	 *  templates never carry -- all 24 tiers fell to thumbnails). Equipment actors are cosmetic-only
	 *  (abilities come from the EquipmentManager grant, never the actor), so spawning one IS
	 *  display-safe. OutThumb always fills for the fallback. */
	static UClass* ResolveDisplayActorClass(const UObject* WorldCtx, FName CosmeticId,
		UTexture2D*& OutThumb, FVector& OutScale)
	{
		OutThumb = nullptr;
		OutScale = FVector::OneVector;
		const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(WorldCtx);
		const FAFLCatalogEntry* Entry = Catalog ? Catalog->FindEntry(CosmeticId) : nullptr;
		if (!Entry)
		{
			return nullptr;
		}
		if (!Entry->ShopThumbnail.IsNull())
		{
			OutThumb = Entry->ShopThumbnail.LoadSynchronous();
		}
		const UAFLWeaponCosmeticAsset* WeaponAsset =
			Cast<UAFLWeaponCosmeticAsset>(const_cast<UAFLCosmeticCatalogSubsystem*>(Catalog)->ResolveAsset(CosmeticId));
		if (!WeaponAsset)
		{
			return nullptr;
		}
		UClass* EquipCls = WeaponAsset->EquipmentDefinition.LoadSynchronous();
		const ULyraEquipmentDefinition* Def = EquipCls ? GetDefault<ULyraEquipmentDefinition>(EquipCls) : nullptr;
		if (!Def || Def->ActorsToSpawn.Num() == 0)
		{
			return nullptr;
		}
		// WID AttachTransform scale IS the weapon's size knob (banked law) -- honor it on the display.
		OutScale = Def->ActorsToSpawn[0].AttachTransform.GetScale3D();
		return Def->ActorsToSpawn[0].ActorToSpawn;
	}
}

AAFLRetailWeaponTower::AAFLRetailWeaponTower()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	PoleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PoleMesh"));
	PoleMesh->SetupAttachment(Root);
	PoleMesh->SetRelativeLocation(FVector(0.f, 0.f, 340.f));
	PoleMesh->SetRelativeScale3D(FVector(0.4f, 0.4f, 6.8f));
	PoleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cyl.Succeeded())
	{
		PoleMesh->SetStaticMesh(Cyl.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DarkMI(
		TEXT("/Game/BagMan/Characters/Cosmetics/IRONICS_Blank/MI_AFL_IRONICS_Body.MI_AFL_IRONICS_Body"));
	if (DarkMI.Succeeded())
	{
		PoleMesh->SetMaterial(0, DarkMI.Object);
	}

	Spinner = CreateDefaultSubobject<USceneComponent>(TEXT("Spinner"));
	Spinner->SetupAttachment(Root);
}

void AAFLRetailWeaponTower::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return; // pure display + client-local retail UX; server keeps the bare actor
	}
	BuildDisplay();
}

void AAFLRetailWeaponTower::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (AAFLDisplayPedestal* Pad : Pads)
	{
		if (Pad)
		{
			Pad->Destroy();
		}
	}
	Pads.Reset();
	for (AActor* Disp : DisplayActors)
	{
		if (Disp)
		{
			Disp->Destroy();
		}
	}
	DisplayActors.Reset();
	Super::EndPlay(EndPlayReason);
}

void AAFLRetailWeaponTower::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Spinner)
	{
		Spinner->AddLocalRotation(FRotator(0.f, SpinRateDegPerSec * DeltaSeconds, 0.f));
	}
}

void AAFLRetailWeaponTower::BuildDisplay()
{
	const int32 Num = FMath::Min(CosmeticIds.Num(), 6);
	if (Num == 0)
	{
		UE_LOG(LogAFLHub, Warning, TEXT("AFL_RETAIL: weapon tower '%s' has no SKUs."), *GetNameSafe(this));
		return;
	}
	UWorld* World = GetWorld();
	for (int32 i = 0; i < Num; ++i)
	{
		const FName Id = CosmeticIds[i];
		const float AngleDeg = i * (360.f / Num);
		const float AngleRad = FMath::DegreesToRadians(AngleDeg);

		// Tier arm on the spinner: rising spiral, weapons face outward.
		USceneComponent* Arm = NewObject<USceneComponent>(this);
		Arm->SetupAttachment(Spinner);
		Arm->RegisterComponent();
		Arm->SetRelativeLocation(FVector(
			ArmRadius * FMath::Cos(AngleRad), ArmRadius * FMath::Sin(AngleRad), TierBaseZ + i * TierStep));
		Arm->SetRelativeRotation(FRotator(0.f, AngleDeg + 90.f, 0.f));

		UTexture2D* Thumb = nullptr;
		FVector WidScale = FVector::OneVector;
		UClass* DisplayCls = AFLWeaponTower::ResolveDisplayActorClass(this, Id, Thumb, WidScale);
		AActor* DisplayActor = nullptr;
		if (DisplayCls && World)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.Owner = this;
			DisplayActor = World->SpawnActor<AActor>(DisplayCls, Arm->GetComponentTransform(), Params);
		}
		if (DisplayActor)
		{
			DisplayActor->AttachToComponent(Arm, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			DisplayActor->SetActorRelativeScale3D(WidScale);
			DisplayActor->SetActorEnableCollision(false);
			// The banked hidden-SkeletalMesh trap: the weapon actor's mesh component can sit hidden
			// until an equip unhides it -- force every scene component visible for the display.
			TArray<USceneComponent*> SceneComps;
			DisplayActor->GetComponents<USceneComponent>(SceneComps);
			for (USceneComponent* SC : SceneComps)
			{
				SC->SetHiddenInGame(false);
				SC->SetVisibility(true, /*bPropagateToChildren*/ true);
			}
			DisplayActors.Add(DisplayActor);
			UE_LOG(LogAFLHub, Log, TEXT("AFL_RETAIL: tower tier %d '%s' -> display actor %s (scale %s, %d comps unhidden)."),
				i, *Id.ToString(), *GetNameSafe(DisplayCls), *WidScale.ToCompactString(), SceneComps.Num());
		}
		else if (Thumb)
		{
			// Beam-family (no baked mesh) -> the shop thumbnail plate stands in.
			UWidgetComponent* Plate = NewObject<UWidgetComponent>(this);
			Plate->SetupAttachment(Arm);
			Plate->RegisterComponent();
			Plate->SetWidgetSpace(EWidgetSpace::World);
			Plate->SetDrawSize(FVector2D(512.f, 512.f));
			Plate->SetRelativeScale3D(FVector(0.18f));
			Plate->SetTwoSided(true); // readable from both sides of the spin
			Plate->SetWidgetClass(UAFLHubMirrorWidget::StaticClass());
			Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Plate->InitWidget();
			if (UAFLHubMirrorWidget* Img = Cast<UAFLHubMirrorWidget>(Plate->GetWidget()))
			{
				Img->SetMirrorTexture(Thumb, FVector2D(512.f, 512.f));
			}
			UE_LOG(LogAFLHub, Log, TEXT("AFL_RETAIL: tower tier %d '%s' -> thumbnail plate (no baked mesh)."), i, *Id.ToString());
		}
		else
		{
			UE_LOG(LogAFLHub, Warning, TEXT("AFL_RETAIL: tower tier %d '%s' resolved NOTHING."), i, *Id.ToString());
		}

		// The pad: static ring around the base, one per tier, display-suppressed (the tower IS the
		// display). DEFERRED spawn so CosmeticId/bSuppressDisplay are set BEFORE BeginPlay (the
		// rack's after-spawn assignment quirk, fixed at the source this time).
		if (World)
		{
			const FVector PadLoc = GetActorTransform().TransformPosition(FVector(
				PadRadius * FMath::Cos(AngleRad), PadRadius * FMath::Sin(AngleRad), 0.f));
			const FRotator PadRot(0.f, GetActorRotation().Yaw + AngleDeg + 180.f, 0.f); // plinth faces the pole
			const FTransform PadXf(PadRot, PadLoc);
			AAFLDisplayPedestal* Pad = World->SpawnActorDeferred<AAFLDisplayPedestal>(
				AAFLDisplayPedestal::StaticClass(), PadXf, this, nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Pad)
			{
				Pad->CosmeticId = Id;
				Pad->bSuppressDisplay = true;
				Pad->FinishSpawning(PadXf);
				Pads.Add(Pad);
			}
		}
	}
}
