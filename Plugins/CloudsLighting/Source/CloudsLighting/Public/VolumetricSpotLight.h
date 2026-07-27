// Copyright (c) 2021-2024 by Lukerrr

#pragma once

#include "CoreMinimal.h"
#include "VolumetricLightSource.h"
#include "VolumetricSpotLight.generated.h"

UCLASS()
class CLOUDSLIGHTING_API AVolumetricSpotLight : public AVolumetricLightSource
{
	GENERATED_BODY()

public:
	AVolumetricSpotLight();

	/**
	* =============================================================
	* AActor interface
	*/
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/**
	* =============================================================
	*/

	/**
	* =============================================================
	* AVolumetricLightSource interface
	*/
	virtual bool SerializeData(TArray<float>& Buffer) override;
private:
	virtual EVLightSourceType GetType() override;
#if WITH_EDITOR
	virtual void DebugDrawContour() override;
#endif // WITH_EDITOR
	/**
	* =============================================================
	*/

public:
	/** Length of the source cone */
	UPROPERTY(EditAnywhere, Replicated, Interp, BlueprintReadWrite, Category = LightSource)
	float SourceLength = 10000.f;

	/** Radius of the source cone */
	UPROPERTY(EditAnywhere, Replicated, Interp, BlueprintReadWrite, Category = LightSource)
	float SourceRadius = 7500.f;

	/** Light source cut distance (radius from center) */
	UPROPERTY(EditAnywhere, Replicated, Interp, BlueprintReadWrite, Category = LightSource)
	float CutDistance = 0.f;

	/** Intensity angular falloff multiplier */
	UPROPERTY(EditAnywhere, Replicated, Interp, BlueprintReadWrite, Category = LightSource)
	float AngFalloffMult = 10.f;

private:
	/** Debug sphere color */
	UPROPERTY(EditAnywhere, Category = Debug)
	FColor LineColor = FColor::Cyan;

	/** Debug sphere thickness */
	UPROPERTY(EditAnywhere, Category = Debug)
	float LineThickness = 0.005f;
};
