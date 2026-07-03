// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Character/LyraCharacter.h"

#include "AFLCharacter.generated.h"

/**
 * AAFLCharacter
 *
 * Sprint 3 Dash Movement Contract — see §9.6 of Docs/BAG_MAN_MASTER_BUILD_v2.0.md.
 *
 * Minimal ALyraCharacter subclass. Sole purpose at this point in the build:
 * swap the default CharacterMovementComponent class to
 * UAFLCharacterMovementComponent so the dash tag-response contract has a
 * live runtime consumer.
 *
 * All other behavior — health component, pawn extension, ability system
 * routing through PlayerState, camera component, input — is inherited
 * unchanged from ALyraCharacter. This is intentional: do not drag
 * ShooterCore pawn assumptions into Bag Man movement; keep the surface
 * minimal until a real reason emerges to extend it.
 */
UCLASS(Config = Game)
class AFLMOVEMENT_API AAFLCharacter : public ALyraCharacter
{
	GENERATED_BODY()

public:
	AAFLCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
