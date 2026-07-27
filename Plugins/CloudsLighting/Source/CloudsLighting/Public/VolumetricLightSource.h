// Copyright (c) 2021-2024 by Lukerrr

#pragma once

#include "CoreMinimal.h"

#include "Components/MaterialBillboardComponent.h"
#include "GameFramework/Actor.h"

#include "VolumetricLightSource.generated.h"

enum class EVLightSourceType : uint8
{
	VLS_POINT,
	VLS_SPOT,

	VLS_NONE,
};

UENUM(BlueprintType)
enum class EVLightFalloffType : uint8
{
	VLF_EXPONENT	UMETA(DisplayName = "Exponential"),
	VLF_INV_SQUARE	UMETA(DisplayName = "Inverse square"),
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CLOUDSLIGHTING_API AVolumetricLightSource : public AActor
{
	GENERATED_BODY()

public:	
	AVolumetricLightSource();

	/**
	* =============================================================
	* AActor interface
	*/
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
#if WITH_EDITOR
	virtual bool ShouldTickIfViewportsOnly() const override;
#endif // WITH_EDITOR
	/**
	* =============================================================
	*/


	/**
	* =============================================================
	* AVolumetricLightSource interface
	*/
	/**
	* Store all necessary data about the light source into the given buffer
	* Returns true if data of the light source is valid.
	*/
	virtual bool SerializeData(TArray<float>& Buffer);
private:
	virtual EVLightSourceType GetType();
#if WITH_EDITOR
	virtual void DebugDrawContour();
#endif // WITH_EDITOR
	/**
	* =============================================================
	*/

#if WITH_EDITOR
	void UpdateBillboard();
#endif // WITH_EDITOR

	UPROPERTY(EditAnywhere, Category = Billboard)
	UMaterialInterface* BillboardBaseMaterial;

protected:
	UPROPERTY(EditAnywhere, Category = Billboard)
	UTexture2D* BillboardTexture;

private:
	UPROPERTY(EditAnywhere, Category = Billboard)
	float BillboardSizeX = 0.1f;

	UPROPERTY(EditAnywhere, Category = Billboard)
	float BillboardSizeY = 0.18f;

	UMaterialBillboardComponent* Billboard = NULL;
	UMaterialInstanceDynamic* BillboardBaseMaterialInst = NULL;

public:

	/** Light source color */
	UPROPERTY(EditAnywhere, Replicated, Interp, BlueprintReadWrite, Category = LightSource)
	FLinearColor Color = FLinearColor::White;

	/** Light source intensity */
	UPROPERTY(EditAnywhere, Replicated, Interp, BlueprintReadWrite, Category = LightSource)
	float Intensity = 1.f;

	/** Light source falloff type */
	UPROPERTY(EditAnywhere, Replicated, Interp, BlueprintReadWrite, Category = LightSource)
	EVLightFalloffType FalloffType = EVLightFalloffType::VLF_EXPONENT;

	/** 
	* Intensity falloff multiplier.
	* Defines attenuation of the light.
	*/
	UPROPERTY(EditAnywhere, Replicated, Interp, BlueprintReadWrite, Category = LightSource)
	float FalloffMult = 1.f;

	/** Whether the light source enabled */
	UPROPERTY(EditAnywhere, Replicated, Interp, BlueprintReadWrite, Category = LightSource)
	bool bIsEnabled = true;
};
