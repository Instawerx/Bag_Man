// Copyright (c) 2021-2024 by Lukerrr

#include "CloudsLightingSystem.h"

#include "VolumetricLightSource.h"

#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "RenderingThread.h"
#include "Runtime/Launch/Resources/Version.h"
#include "TextureResource.h"
#include "UObject/ConstructorHelpers.h"

ACloudsLightingSystem::ACloudsLightingSystem()
{
	const ConstructorHelpers::FObjectFinder<UTexture2D> BillboardSpriteFinder(
		TEXT("'/CloudsLighting/Billboards/T_CloudsLightingSystemBillboard.T_CloudsLightingSystemBillboard'"));
	const ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> LightDataMapFinder(
		TEXT("'/CloudsLighting/Material/RT_LightDataMap.RT_LightDataMap'"));

	Billboard = CreateDefaultSubobject<UBillboardComponent>("Billboard");
	SetRootComponent(Billboard);

#if WITH_EDITOR
	Billboard->bIsScreenSizeScaled = true;
	Billboard->ScreenSize = 0.0015f;
	Billboard->SetSprite(BillboardSpriteFinder.Object);
#else
	Billboard->SetHiddenInGame(true);
#endif // WITH_EDITOR

	LightDataMap = LightDataMapFinder.Object;

	LightDataMap->RenderTargetFormat = ETextureRenderTargetFormat::RTF_R32f;
	LightDataMap->Filter = TextureFilter::TF_Nearest;
	LightDataMap->LODGroup = TextureGroup::TEXTUREGROUP_8BitData;
	LightDataMap->ClearColor = FLinearColor::Black;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ACloudsLightingSystem::BeginPlay()
{
	Super::BeginPlay();

	if (!LightDataMap)
	{
		UE_LOG(LogClass, Warning, TEXT("ACloudsLightingSystem %s: Light data RT is NULL"), *GetName());
	}
}

void ACloudsLightingSystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	bIsValid = (LightDataMap != NULL);

	if (!bIsValid)
	{
		return;
	}

	if (!bIsRtValid)
	{
		InitializeRt();

		// Let RT fully initialize and update light data on next tick
		return;
	}

	if (!bIsBakingLight)
	{
		UpdateLightData();
	}
}

#if WITH_EDITOR
bool ACloudsLightingSystem::ShouldTickIfViewportsOnly() const
{
	return true;
}

void ACloudsLightingSystem::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropName = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : PropertyChangedEvent.GetPropertyName();

	if (PropName == "LightDataRt")
	{
		// Request initialize the render target on the next tick
		bIsRtValid = false;
	}
}
#endif // WITH_EDITOR

void ACloudsLightingSystem::InitializeRt()
{
	uint32 RtSize = GetRequiredRtSize();
	LightDataMap->InitAutoFormat(RtSize, RtSize);

	bIsRtValid = true;
}

void ACloudsLightingSystem::UpdateLightData()
{
	bIsBakingLight = true;
	TArray<AActor*> LightSources;
	UGameplayStatics::GetAllActorsOfClass(this, AVolumetricLightSource::StaticClass(), LightSources);

	/**
	 * The light data array
	 * 
	 * |     0     |     1     |         2         |     ....     |
	 *   RT Size X   RT Size Y   Number of sources   Sources data
	 */
	TArray<float> LightData = { (float)LightDataMap->SizeX, (float)LightDataMap->SizeY, 0.f };

	for (AActor* LightSourceActor : LightSources)
	{
#if WITH_EDITOR
		if (GetWorld()->WorldType == EWorldType::Editor && LightSourceActor->IsTemporarilyHiddenInEditor())
		{
			continue;
		}
#endif // WITH_EDITOR

		if (LightSourceActor->IsHidden() && GetWorld()->WorldType != EWorldType::Editor)
		{
			continue;
		}

		AVolumetricLightSource* LightSource = Cast<AVolumetricLightSource>(LightSourceActor);

		if (LightSource->bIsEnabled)
		{
			if (LightSource->SerializeData(LightData))
			{
				// Increment light sources count
				LightData[2] += 1.f;
			}
		}
	}

	uint32 MapSizeRequired = LightData.Num();
	uint32 MapSize = LightDataMap->SizeX * LightDataMap->SizeY;

	if (MapSize < MapSizeRequired)
	{
		// Expand the render target
		++RequiredRtSizePow;
		bIsRtValid = false;
	}
	else if (MapSize > MapSizeRequired && RequiredRtSizePow > MinRtSizePow)
	{
		// Shrink the render target if too many free space
		float Width = FMath::Sqrt((float)MapSizeRequired);
		float WidthPow = FMath::Log2(Width);
		uint32 NewRequiredRtSizePow = FMath::CeilToInt(WidthPow);
		
		if (NewRequiredRtSizePow < RequiredRtSizePow)
		{
			if (NewRequiredRtSizePow <= MinRtSizePow)
			{
				RequiredRtSizePow = MinRtSizePow;
			}
			else
			{
				RequiredRtSizePow = NewRequiredRtSizePow;
			}
			bIsRtValid = false;
		}
	}

	if (!bIsRtValid)
	{
		bIsBakingLight = false;
		return;
	}

	// Resize array to fit the render target
	LightData.AddZeroed(LightDataMap->SizeX* LightDataMap->SizeY - MapSizeRequired);

	ENQUEUE_RENDER_COMMAND(UpdateLightData) (
		[LightData, this](FRHICommandListImmediate& RHICmdList)
	{

#if ENGINE_MAJOR_VERSION > 4
		FTextureRenderTarget2DResource* Resource = static_cast<FTextureRenderTarget2DResource*>(LightDataMap->GetResource());
#else
		FTextureRenderTarget2DResource* Resource = static_cast<FTextureRenderTarget2DResource*>(LightDataMap->Resource);
#endif

		if (!Resource)
		{
			bIsBakingLight = false;
			return;
		}

		FTexture2DRHIRef Texture2DRHI = Resource->GetTextureRHI();

		if (!Texture2DRHI.IsValid())
		{
			bIsBakingLight = false;
			return;
		}

		uint32 SrcStride = sizeof(float) * LightDataMap->SizeX;
		const void* Src = LightData.GetData();

		uint32 DestStride = 0;
		void* Dest = RHICmdList.LockTexture2D(Texture2DRHI, 0, EResourceLockMode::RLM_WriteOnly, DestStride, false);

		CopyTextureData2D(Src, Dest, LightDataMap->SizeY, EPixelFormat::PF_R32_FLOAT, SrcStride, DestStride);

		RHICmdList.UnlockTexture2D(Texture2DRHI, 0, false);

		bIsBakingLight = false;
	});
}

uint32 ACloudsLightingSystem::GetRequiredRtSize()
{
	return (uint32)FMath::Pow(2.f, (float)RequiredRtSizePow);
}

bool ACloudsLightingSystem::IsValid()
{
	return bIsValid;
}