// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/GameplayMessageSubsystem.h"
#include "Subsystems/WorldSubsystem.h"

#include "AFLOverkillListener.generated.h"

struct FLyraVerbMessage;


/**
 * UAFLOverkillListener
 *
 * Sprint 1 / AFL-0105 test subsystem. Subscribes to Event.Damage.Overkill
 * broadcasts from UAFLDamageExecCalc and logs them to LogAFLCombat.
 * Verifies that the §8.3 Overkill emission path actually reaches a listener.
 *
 * Lifecycle: UWorldSubsystem — auto-instantiated per world, listener
 * registration happens in Initialize and is cleaned up in Deinitialize.
 * In PIE this re-binds on every Play press, which is what we want for
 * repeatable acceptance-matrix runs.
 *
 * Server-only: registration is gated on net authority. The Overkill
 * broadcast in UAFLDamageExecCalc fires inside #if WITH_SERVER_CODE,
 * so client-side listeners would never trigger anyway. Skipping the
 * registration on clients keeps the log noise-free.
 */
UCLASS()
class AFLCOMBAT_API UAFLOverkillListener : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

protected:

	void OnOverkill(FGameplayTag Channel, const FLyraVerbMessage& Message);

private:

	FGameplayMessageListenerHandle ListenerHandle;
};
