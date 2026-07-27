// Copyright (c) 2021-2024 by Lukerrr

#include "VolumetricLightSource.h"

#if WITH_EDITOR
#include "IAssetViewport.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "LevelEditor.h"
#endif // WITH_EDITOR
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AVolumetricLightSource::AVolumetricLightSource()
{
	const ConstructorHelpers::FObjectFinder<UMaterialInterface> BillboardBaseMaterialFinder(
		TEXT("'/CloudsLighting/Billboards/Mat_VolumetricLightSourceBillboard_Inst.Mat_VolumetricLightSourceBillboard_Inst'"));

	BillboardBaseMaterial = BillboardBaseMaterialFinder.Object;

	Billboard = CreateDefaultSubobject<UMaterialBillboardComponent>("Billboard");
	SetRootComponent(Billboard);

#if WITH_EDITOR
	FMaterialSpriteElement BillboardElement;
	BillboardElement.bSizeIsInScreenSpace = true;
	BillboardElement.Material = NULL;
	BillboardElement.BaseSizeX = BillboardSizeX;
	BillboardElement.BaseSizeY = BillboardSizeY;
	Billboard->Elements.Add(BillboardElement);
#else
	Billboard->SetHiddenInGame(true);
#endif // WITH_EDITOR

	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

#if WITH_EDITOR
	UpdateBillboard();
#endif // WITH_EDITOR
}

void AVolumetricLightSource::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

#if WITH_EDITOR
	bool bGameView = false;

	EWorldType::Type WorldType = GetWorld()->WorldType;
	bool bInEditor = WorldType == EWorldType::Editor;
	bool bInEditorPreview = WorldType == EWorldType::EditorPreview;
	bool bInAnyEditor = bInEditor || bInEditorPreview;

	if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedPtr<IAssetViewport> ActiveLevelViewport = LevelEditorModule.GetFirstActiveViewport();
		if (ActiveLevelViewport.IsValid())
		{
			bGameView = ActiveLevelViewport->IsInGameView();
		}
	}

	/** Updating billboard */
	UpdateBillboard();

	if (bInAnyEditor)
	{
		Billboard->SetVisibility(!bGameView);
	}
	else
	{
		Billboard->SetVisibility(false);
	}

	if (!bGameView && (IsSelectedInEditor() || bInEditorPreview) && bInAnyEditor && !IsTemporarilyHiddenInEditor())
	{
		DebugDrawContour();
	}
#endif // WITH_EDITOR
}

void AVolumetricLightSource::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AVolumetricLightSource, Color);
	DOREPLIFETIME(AVolumetricLightSource, Intensity);
	DOREPLIFETIME(AVolumetricLightSource, FalloffType);
	DOREPLIFETIME(AVolumetricLightSource, FalloffMult);
	DOREPLIFETIME(AVolumetricLightSource, bIsEnabled);
}

#if WITH_EDITOR
bool AVolumetricLightSource::ShouldTickIfViewportsOnly() const
{
	return true;
}

void AVolumetricLightSource::DebugDrawContour()
{
}

void AVolumetricLightSource::UpdateBillboard()
{
	FMaterialSpriteElement& BillboardElement = Billboard->Elements[0];

	bool bIsBillboardValid = BillboardTexture && BillboardBaseMaterial;

	if (bIsBillboardValid)
	{
		if (!BillboardBaseMaterialInst || BillboardBaseMaterial != BillboardBaseMaterialInst->Parent)
		{
			BillboardBaseMaterialInst = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, BillboardBaseMaterial);
		}

		BillboardBaseMaterialInst->SetTextureParameterValue("Icon", BillboardTexture);
		BillboardBaseMaterialInst->SetVectorParameterValue("Color", Color);

		BillboardElement.Material = BillboardBaseMaterialInst;
		BillboardElement.BaseSizeX = BillboardSizeX;
		BillboardElement.BaseSizeY = BillboardSizeY;
	}
	else
	{
		BillboardElement.Material = NULL;
	}

	Billboard->MarkRenderStateDirty();
}
#endif // WITH_EDITOR

bool AVolumetricLightSource::SerializeData(TArray<float>& Buffer)
{
	// cm to km
	float FalloffScaled = FalloffMult / 100000.f;

	Buffer.Add((float)GetType());
	Buffer.Add((float)FalloffType);
	Buffer.Add(Color.R);
	Buffer.Add(Color.G);
	Buffer.Add(Color.B);
	Buffer.Add(Intensity);
	Buffer.Add(FalloffScaled);

	return true;
}

EVLightSourceType AVolumetricLightSource::GetType()
{
	return EVLightSourceType::VLS_NONE;
}
