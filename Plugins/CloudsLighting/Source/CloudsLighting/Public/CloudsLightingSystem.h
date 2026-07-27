// Copyright (c) 2021-2024 by Lukerrr

#pragma once

#include "CoreMinimal.h"

#include "Components/BillboardComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/Actor.h"

#include "CloudsLightingSystem.generated.h"

UCLASS()
class CLOUDSLIGHTING_API ACloudsLightingSystem : public AActor
{
	GENERATED_BODY()
	
public:
	ACloudsLightingSystem();

	/**
	* =============================================================
	* AActor interface
	*/
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
#if WITH_EDITOR
	virtual bool ShouldTickIfViewportsOnly() const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	/**
	* =============================================================
	*/

	UFUNCTION(BlueprintCallable, Category = CloudLightingSystem)
	bool IsValid();

private:
	/** Fill necessary params of the render target and generate its resource */
	void InitializeRt();

	/** Collect all light sources data and serialize it into the render target */
	void UpdateLightData();

	/** Returns currently required size of render target in pixels */
	uint32 GetRequiredRtSize();

	UPROPERTY(EditAnywhere, Category = Billboard)
	UBillboardComponent* Billboard = NULL;

	/** A render target to store light sources data */
	UPROPERTY(EditAnywhere, Category = LightDataMap)
	UTextureRenderTarget2D* LightDataMap = NULL;

	/** Minimal allowed size of render target (power of two) */
	uint32 MinRtSizePow = 3;

	/** Currently required size of render target (power of two) */
	uint32 RequiredRtSizePow = MinRtSizePow;

	bool bIsValid = false;
	bool bIsRtValid = false;

	std::atomic<bool> bIsBakingLight{ false };
};
