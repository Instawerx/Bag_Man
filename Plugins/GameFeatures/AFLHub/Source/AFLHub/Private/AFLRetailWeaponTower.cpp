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
	/** Catalog row -> the equipped actor's skeletal mesh, WITHOUT spawning gameplay. Returns null for
	 *  beam-family weapons (their mesh slot is legitimately empty); OutThumb/OutScale always fill. */
	static USkeletalMesh* ResolveDisplayMesh(const UObject* WorldCtx, FName CosmeticId,
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
		UClass* ActorCls = Def->ActorsToSpawn[0].ActorToSpawn;
		// SCS TEMPLATE WALK up the super chain: the shared BP parent owns an INHERITED SkeletalMesh
		// component that CDO component iteration cannot see (the banked hidden-component trap).
		for (UClass* C = ActorCls; C && C != AActor::StaticClass(); C = C->GetSuperClass())
		{
			const UBlueprintGeneratedClass* BPC = Cast<UBlueprintGeneratedClass>(C);
			if (!BPC || !BPC->SimpleConstructionScript)
			{
				continue;
			}
			for (const USCS_Node* Node : BPC->SimpleConstructionScript->GetAllNodes())
			{
				if (const USkeletalMeshComponent* Tmpl = Node ? Cast<USkeletalMeshComponent>(Node->ComponentTemplate) : nullptr)
				{
					if (USkeletalMesh* Mesh = Tmpl->GetSkeletalMeshAsset())
					{
						return Mesh;
					}
				}
			}
		}
		return nullptr;
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
		if (USkeletalMesh* Mesh = AFLWeaponTower::ResolveDisplayMesh(this, Id, Thumb, WidScale))
		{
			USkeletalMeshComponent* MeshComp = NewObject<USkeletalMeshComponent>(this);
			MeshComp->SetupAttachment(Arm);
			MeshComp->RegisterComponent();
			MeshComp->SetSkeletalMeshAsset(Mesh);
			MeshComp->SetRelativeScale3D(WidScale);
			MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			UE_LOG(LogAFLHub, Log, TEXT("AFL_RETAIL: tower tier %d '%s' -> mesh %s (scale %s)."),
				i, *Id.ToString(), *GetNameSafe(Mesh), *WidScale.ToCompactString());
		}
		else if (Thumb)
		{
			// Beam-family (no baked mesh) -> the shop thumbnail plate stands in.
			UWidgetComponent* Plate = NewObject<UWidgetComponent>(this);
			Plate->SetupAttachment(Arm);
			Plate->RegisterComponent();
			Plate->SetWidgetSpace(EWidgetSpace::World);
			Plate->SetDrawSize(FVector2D(512.f, 512.f));
			Plate->SetRelativeScale3D(FVector(0.12f));
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
