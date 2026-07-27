// Copyright (c) 2021-2024 by Lukerrr

#include "VolumetricPointLight.h"

#if WITH_EDITOR
#include "DrawDebugHelpers.h"
#endif // WITH_EDITOR
#include "Engine/Texture2D.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AVolumetricPointLight::AVolumetricPointLight()
{
	const ConstructorHelpers::FObjectFinder<UTexture2D> BillboardTextureFinder(
		TEXT("'/CloudsLighting/Billboards/T_VolumetricPointLightBillboard.T_VolumetricPointLightBillboard'"));

	BillboardTexture = BillboardTextureFinder.Object;
}

#if WITH_EDITOR
void AVolumetricPointLight::DebugDrawContour()
{
	DrawDebugSphere(GetWorld(), GetActorLocation(), SourceRadius, 16, LineColor, false, -1.f, 0, SourceRadius * LineThickness);
}
#endif // WITH_EDITOR

void AVolumetricPointLight::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AVolumetricPointLight, SourceRadius);
}

bool AVolumetricPointLight::SerializeData(TArray<float>& Buffer)
{
	Super::SerializeData(Buffer);

	FVector Location = GetActorLocation();

	Buffer.Add(Location.X);
	Buffer.Add(Location.Y);
	Buffer.Add(Location.Z);
	Buffer.Add(SourceRadius);

	return true;
}

EVLightSourceType AVolumetricPointLight::GetType()
{
	return EVLightSourceType::VLS_POINT;
}
