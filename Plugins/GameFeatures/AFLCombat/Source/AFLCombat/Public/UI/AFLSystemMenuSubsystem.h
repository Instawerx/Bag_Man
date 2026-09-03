// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/WeakObjectPtr.h"

#include "AFLSystemMenuSubsystem.generated.h"

class UAFLW_SystemMenu;
class FAFLEscapeInputProcessor;

/**
 * UAFLSystemMenuSubsystem -- installs a GLOBAL Escape handler (operator ruling 2026-09-02: "Esc should
 * always pull up the Quit Game option anywhere at anytime").
 *
 * A Slate input pre-processor catches Escape BEFORE gameplay input can swallow it, so even a player
 * stuck in-world (the climb float that prompted this) can always reach RESUME / SIGN OUT / QUIT TO
 * DESKTOP. Client-only (no Slate on a dedicated server).
 *
 * GAMEPLAY-ONLY (operator ruling 2026-09-02: "keep Esc as back inside menus, Quit from gameplay only"):
 *  - Escape in GAMEPLAY, no System Menu open -> open one, and CONSUME the key.
 *  - Escape INSIDE A MENU                     -> NOT consumed: Escape keeps its normal "back" meaning
 *                                                (CommonUI routes it to the focused widget).
 *  - Escape while the System Menu IS open     -> NOT consumed: the menu is itself focused, so its own
 *                                                back handler closes/navigates it (confirm -> menu -> close).
 *
 * Gameplay vs menu is decided by widget focus (the game viewport is focused in gameplay; a UI widget is
 * focused in a menu) -- see IsGameplayContext in the .cpp.
 */
UCLASS()
class AFLCOMBAT_API UAFLSystemMenuSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The pre-processor asks this on Escape: true when no System Menu is open (so it should open one). */
	bool ShouldOpenOnEscape() const { return !OpenMenu.IsValid(); }

	/** Open the System Menu and remember it (so a second Escape closes it rather than stacking another). */
	void OpenSystemMenu();

private:
	TSharedPtr<FAFLEscapeInputProcessor> EscapeProcessor;
	TWeakObjectPtr<UAFLW_SystemMenu> OpenMenu;
};
