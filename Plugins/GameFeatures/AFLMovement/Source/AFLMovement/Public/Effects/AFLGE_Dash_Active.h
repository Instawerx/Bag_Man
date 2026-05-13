// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "AFLGE_Dash_Active.generated.h"

/**
 * UAFLGE_Dash_Active
 *
 * Sprint 3 Dash Movement Contract — see §9.6 of Docs/BAG_MAN_MASTER_BUILD_v2.0.md.
 *
 * Duration GE (0.12s) that grants State.Movement.Dashing. Owns the dash-active
 * state window. UAFLCharacterMovementComponent listens to State.Movement.Dashing
 * on the owning pawn's ASC and swaps friction/air-control while this GE is
 * active; the tag is auto-removed when the GE expires (GAS rollback replaces
 * manual save/restore timers).
 *
 * Sprint 3 base does NOT grant State.Invulnerable. The i-frame tag is reserved
 * in AFLCoreTags.ini but its gameplay-effect impact is gated on the i-frame
 * toggle which has not been formally enabled (see §9.6 invulnerability rule
 * LOCKED).
 */
UCLASS()
class AFLMOVEMENT_API UAFLGE_Dash_Active : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAFLGE_Dash_Active(const FObjectInitializer& ObjectInitializer);
};
