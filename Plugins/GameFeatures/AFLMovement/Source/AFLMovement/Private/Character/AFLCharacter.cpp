// Copyright C12 AI Gaming. All Rights Reserved.

#include "Character/AFLCharacter.h"

#include "GameFramework/Character.h"
#include "Interaction/AFLWeaponIKComponent.h"
#include "Movement/AFLCharacterMovementComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCharacter)

AAFLCharacter::AAFLCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UAFLCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Inherits ASC routing (PlayerState-owned), health/pawn-extension components,
	// camera, input — all from ALyraCharacter. The only AFL-side change is the
	// CMC subclass swap above, which is the §9.6 critical wiring for the dash
	// tag-response contract.

	// Fleet-wide support-hand weapon IK (PATH B). Ticks TG_PostPhysics, resolves the mesh's Control Rig, and
	// pushes the component-space left-hand target/pole/alpha into CR_AFL_IRONICS' LeftHandIK controls.
	WeaponIKComponent = CreateDefaultSubobject<UAFLWeaponIKComponent>(TEXT("WeaponIKComponent"));
}
