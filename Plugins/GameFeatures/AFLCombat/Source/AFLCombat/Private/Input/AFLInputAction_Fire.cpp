// Copyright C12 AI Gaming. All Rights Reserved.

#include "Input/AFLInputAction_Fire.h"

#include "InputTriggers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLInputAction_Fire)


UAFLInputAction_Fire::UAFLInputAction_Fire()
{
	ValueType = EInputActionValueType::Boolean;

	// CreateDefaultSubobject is the supported in-ctor path for Instanced UPROPERTY
	// arrays of UObject* (NewObject is illegal during a UObject constructor).
	UInputTriggerPressed* PressedTrigger =
		CreateDefaultSubobject<UInputTriggerPressed>(TEXT("PressedTrigger"));
	Triggers.Add(PressedTrigger);
}
