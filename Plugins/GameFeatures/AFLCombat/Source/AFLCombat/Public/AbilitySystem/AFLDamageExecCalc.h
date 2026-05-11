// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffectExecutionCalculation.h"

#include "AFLDamageExecCalc.generated.h"

class UObject;


UCLASS()
class AFLCOMBAT_API UAFLDamageExecCalc : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:

	UAFLDamageExecCalc();

protected:

	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
