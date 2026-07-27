// Copyright (c) 2021-2024 by Lukerrr

#pragma once

#include "CoreMinimal.h"
#include "VolumetricLightSource.h"
#include "VolumetricPointLight.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CLOUDSLIGHTING_API AVolumetricPointLight : public AVolumetricLightSource
{
	GENERATED_BODY()

public:
	AVolumetricPointLight();

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
	/**
	* A radius from the source at which the intensity
	* is constantly equal to its maximum value
	*/
	UPROPERTY(EditAnywhere, Replicated, Interp, BlueprintReadWrite, Category = LightSource)
	float SourceRadius = 10000.f;

private:
	/** Debug sphere color */
	UPROPERTY(EditAnywhere, Category = Debug)
	FColor LineColor = FColor::Cyan;

	/** Debug sphere thickness */
	UPROPERTY(EditAnywhere, Category = Debug)
	float LineThickness = 0.01f;
};
