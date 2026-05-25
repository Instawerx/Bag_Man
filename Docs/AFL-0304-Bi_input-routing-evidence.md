# AFL-0304-Bi: Input routing bug — evidence chain for VS source-stepping session

**Status:** OPEN. Diagnosed to "runtime input routing layer," but the actual broken
function call has not been identified because Python/AIK cannot inspect the
`UPlayerInput::InputKey` path.

**Date of investigation:** 2026-05-24 / 2026-05-25 (single long session, ~8 hours)

**One-line summary:** AFLCharacter possessed by LyraPlayerController in
B_AFL_Test_Experience produces zero player input response (WASD/Shift/Space/LMB all
dead) despite all configuration and init state being correct. Lyra stock maps work
normally on the same engine + same box.

---

## What works (confirmed in this session)

| Fact | How verified |
|---|---|
| `AFLHeroComponent` is installed on `AFLCharacter_0` | Python: `pawn.get_components_by_class(unreal.ActorComponent)` enumerated all 11 components on the live PIE pawn |
| `LyraPawnExtensionComponent` is installed on the pawn | Same enumeration |
| `LyraInputComponent` is installed on the pawn | Same enumeration |
| `LyraPlayerState_0` is attached to the pawn | `pawn.get_player_state()` returned the LyraPlayerState instance |
| Both `LyraPawnExtensionComponent` and `AFLHeroComponent` reach `InitState.GameplayReady` | `comp.get_init_state()` returned the tag; `GameplayTagLibrary.get_debug_string_from_gameplay_tag` resolved to `"InitState.GameplayReady"` for both |
| `LyraPlayerController` (NOT `LyraDebugCameraController`) is the active PC | `GameplayStatics.get_player_controller(world, 0)` returned `LyraPlayerController`; `pc.get_controlled_pawn()` returned `AFLCharacter_0` |
| Both IMCs are registered with EnhancedInputLocalPlayerSubsystem | `showdebug enhancedinput` in PIE displayed: `Context: IMC_AFL_Movement Priority: 0` and `Context: IMC_Default Priority: -1` with full mapping lists |
| Each IMC's mappings include the expected bindings | Same showdebug: `LeftShift → IA_Ability_Dash` from both IMCs (with "OVERRIDDEN BY" cross-references); IMC_Default lists IA_Move, IA_Jump, IA_Weapon_Fire, etc. |
| `InputData_AFL_Hero` has the canonical IA→Tag mappings | Direct asset read post-edit: NativeInputActions=5 (Move/Look.Mouse/Look.Stick/Crouch/AutoRun), AbilityInputActions=2 (Movement.Dash + Jump) |
| `DA_AFL_PawnData_Hero_Default.DefaultCameraMode = CM_ThirdPerson_C` | Asset edit + reload; matches Lyra's `SimplePawnData` |
| `DA_AFL_Combat_AbilitySet` grants `UAFLAG_Laser_Pulse` (C++ class) under `InputTag.Weapon.Fire` | Direct asset read; this was a separate AFL-0214 gap closure committed in `53d6cbf2` |
| `LAS_AFL_HeroComponents.ComponentList[0].ComponentClass = AFLHeroComponent` | Direct asset read at `674696da` baseline (the AFL-0304-Bi fix from prior session is intact) |
| Keyboard input reaches PIE | `~` opens the in-PIE console; `Fly`/`EnableCheats` executed; `ToggleDebugCamera` toggled state |
| Lyra stock maps work normally on this exact engine + box | R1 sanity test: opened a Lyra stock map, WASD/look/jump all work as expected |
| Lyra hero init binds `IA_Move`, `IA_Look_Mouse`, `IA_Look_Stick`, `IA_Crouch`, `IA_AutoRun` natively via `BindNativeAction` | Source read: `Source/LyraGame/Character/LyraHeroComponent.cpp:285-289` |

## What is broken (confirmed)

| Symptom | How verified |
|---|---|
| WASD does not move the character | Direct PIE test, multiple sessions, after `Fly` cheat (eliminates gravity/collision), with `ShowFlag.UI 0` (eliminates UMG focus eat), with cursor captured to viewport |
| Pawn velocity stays (0, 0, 0) when WASD is pressed | `pawn.get_velocity()` after held key press |
| Shift does not trigger dash | PIE test |
| Space does not jump | PIE test |
| LMB does not fire Pulse | PIE test (expected to fail — InputTag.Weapon.Fire binding deferred — but listed for completeness) |
| Mouse-X axis MAY work (rotational movement reported) | User report; not independently verified by velocity-readback. May be debug-camera carryover or `IA_Look_Mouse` Native binding firing in isolation |

## Theories ruled out (theory → disproof)

1. **Init state stalled at DataInitialized** → both components return `InitState.GameplayReady`.
2. **Missing `DefaultCameraMode` on PawnData blocks init advancement** → set to `CM_ThirdPerson_C`, init was already at GameplayReady before AND after. Did not unblock movement.
3. **Feature-name rename `Hero → AFLHero` breaks Lyra's wait-list** → `AFLHeroComponent` inherits `HandleChangeInitState` from `ULyraHeroComponent`; binding logic runs keyed off this component's own GameplayReady transition. State reached GameplayReady regardless of rename. **Falsified by experiment 2026-05-25:** removed the override entirely (kept inherited `"Hero"` name), full UBT rebuild (273 actions, 616s), PIE test — WASD/Shift still dead. The rename was orphaned (zero call sites referenced `"AFLHero"`) AND not the bug. Kept the revert as cleanup since the override was real dead code.
4. **`ClearAllMappings` at `LyraHeroComponent::InitializePlayerInput:244` wipes the subsystem** → `showdebug enhancedinput` confirms BOTH `IMC_AFL_Movement` AND `IMC_Default` are present in the subsystem at runtime, with their mapping lists intact. Subsystem is NOT empty.
5. **HUD widget (`W_OverallUILayout_C_0`) consumes keyboard input** → `ShowFlag.UI 0` and `F1` (HUD toggle) tested; neither restored WASD. HUD is not eating input.
6. **Wrong PlayerController (DebugCameraController) is active** → `get_player_controller(0)` returns `LyraPlayerController`. Red herring; DebugCameraController exists in world but isn't the active controller.
7. **Touch event routing swallowing keys (`bEnableTouchEvents=True`)** → checked but not toggled-off-and-retested. NOT actually disproven; flagged below.
8. **Empty `InputData_AFL_Hero.NativeInputActions`** → was the original state; populated with 5 entries matching Lyra's `InputData_Hero`. Did not restore movement, but is a necessary fix that's been committed regardless.
9. **`LAS_AFL_HeroComponents` only registers `IMC_AFL_Movement` (no IMC_Default)** → was the original state; IMC_Default added at Priority=-1. `showdebug` confirms both IMCs now register at runtime. Did not restore movement, but necessary fix.
10. **Python-leak GC ghost objects polluting subsequent PIE sessions** → editor fully closed and restarted; broken state reproduces on completely fresh process. Not the cause.
11. **`Invalid Primary Asset Id LyraExperienceActionSet:LAS_AFL_HeroComponents` blocks ActionSet activation** → the warning persists across the session and `DefaultGame.ini` config-only fix (added scan paths) reverted with editor restart. But the LAS's actions (AddComponents/AddInputContextMapping/AddInputBinding) all fire successfully despite the warning — IMCs ARE in the subsystem. Warning is cosmetic; not load-bearing for this bug.

## What hasn't been tested (the right next probes)

### Test A — stock PawnData in our Experience

Edit `B_AFL_Test_Experience.DefaultPawnData` to point at `/ShooterCore/Game/HeroData_ShooterGame` (Lyra's working hero PawnData) instead of `DA_AFL_PawnData_Hero_Default`. PIE on `L_AFL_Arena_Test`.

- **If WASD works:** bug is in `DA_AFL_PawnData_Hero_Default` or `AFLCharacter` C++ — narrowed to the AFL pawn/data chain.
- **If WASD still fails:** bug is in the Experience itself, `LAS_AFL_HeroComponents`, or the map's `B_AFL_Test_Experience` setup.

### Test B — AFLCharacter in stock Experience

Spawn an instance of `AFLCharacter` onto a stock Lyra map (e.g., `/Game/Maps/L_ShooterGym`). Confirm whether the stock map's PawnData mechanism can drive AFLCharacter.

- **If WASD works:** AFLCharacter C++ is fine; bug is in our Experience/PawnData/LAS layer.
- **If WASD fails:** bug is in `AFLCharacter` C++ class itself.

### Test C — VS source-stepping

The actual diagnostic that ends this:

- Attach VS to running `UnrealEditor.exe`
- Set a breakpoint in `Source/Runtime/Engine/Private/UserInterface/PlayerInput.cpp` → `UPlayerInput::InputKey`
- Start PIE
- Press W
- Observe what `InputKey` actually receives, whether the call site even fires, and which `InputComponent` is at the top of the controller's input stack when it does

The Python/AIK layer cannot inspect this. Every public-facing property is in the "expected" state. The bug is in the runtime call chain between "key pressed" and "InputAction fires," and only a debugger that can step through `UPlayerInput::InputKey → FInputActionInstance::Tick → ...` will identify it.

## Tool gap (why we couldn't finish via AIK)

- `unreal.PlayerController` Python binding does not expose `InputComponent`, `PlayerInput`, the input stack, or `BoundActionInputs` — all are marked protected by UE's reflection system. Cannot read which actions are bound to what.
- `unreal.EnhancedInputComponent` Python binding does not expose `GetActionEventBindings()` or equivalent — cannot enumerate registered action handlers.
- No Python-exposed way to step into `UPlayerInput::InputKey` or observe its internal state.
- Console commands (`Slate.DebugCurrentlyFocusedWidget`, `EnhancedInput.DumpMappingContexts`, etc.) that would help do NOT exist in this engine version.
- Only `showdebug enhancedinput` is available, and it shows static binding configuration (which is correct) — not runtime action firing.

**Conclusion:** further AIK-layer investigation cannot reach the broken code path. VS or live source-debug is required.

## Files modified during the investigation

Committed in this session:
- `53d6cbf2 feat(AFLCombat): AFL-0209 ... + Pulse grant (AFL-0214 closure)` — AFL-0209 deliverable and AFL-0214 Pulse-grant gap closure

Modified locally but NOT YET COMMITTED at evidence-dump time:
- `Plugins/GameFeatures/AFLMovement/Content/Data/DA_AFL_PawnData_Hero_Default.uasset` — set `DefaultCameraMode = CM_ThirdPerson_C` (matches Lyra working pattern)
- `Plugins/GameFeatures/AFLMovement/Content/Input/InputData_AFL_Hero.uasset` — added 5 `NativeInputActions` (Move/Look.Mouse/Look.Stick/Crouch/AutoRun) and 1 `AbilityInputActions` entry (Jump → IA_Jump) matching Lyra's `InputData_Hero`
- `Plugins/GameFeatures/AFLMovement/Content/Experiences/LAS_AFL_HeroComponents.uasset` — registered `IMC_Default` at `Priority=-1` alongside existing `IMC_AFL_Movement` at `Priority=0`

These three asset edits are necessary-not-sufficient: `showdebug` confirms they register correctly at runtime, but they do not restore input routing alone. They are intended to be committed as part of the input-content baseline so VS investigation starts from a known-correct asset state.

## Outstanding hypotheses (post-rename-falsification)

With the rename ruled out, the remaining candidate causes are narrower:

1. **`bReadyToBindInputs` may be False at the live AFLHeroComponent despite `GameplayReady`.** Python's `get_editor_property` cannot read it (protected field) and `IsReadyToBindInputs()` UFUNCTION is not exposed to the bridge. Test C (VS source-stepping) directly addresses this — set breakpoint inside `ULyraHeroComponent::InitializePlayerInput` and observe whether it fires for the AFL pawn. Source trace says it MUST fire if the component reached GameplayReady (which the runtime probe confirmed), but trace-says doesn't equal runtime-confirms.

2. **`EnhancedInputComponent`'s internal action-binding table may not contain the IA→handler bindings even though IMCs are registered.** `showdebug enhancedinput` shows IMC presence and key→IA resolution but not "does pressing this key actually invoke `Input_Move` on the live HeroComponent." Source-stepping inside `UEnhancedInputComponent::ProcessInput` answers this.

3. **The two isolation tests from Test A/B above remain untried.** Cheapest next-experiment if VS isn't available.

## Followup tickets implied by this work

- **AFL-0107-followup:** add `InputTag.Weapon.Fire → IA_Weapon_Fire` to `InputData_AFL_Hero.AbilityInputActions`. Deferred pending AFL-0304-Bi resolution.
- **AFL-0209-feel-check:** PIE validation of bloom climb/recover/live-swap. Blocked on AFL-0304-Bi.
- **C4996 cleanup:** `AFLAG_Laser_Beam.cpp:53` and `AFLAG_Laser_Pulse.cpp:45` use the now-deprecated `UGameplayAbility::AbilityTags` direct-mutation API. UE 5.6 surfaces these as build warnings; UE 5.7+ will make them errors. Swap to `SetAssetTags(...)` in ctor.
- **Tracker v2.8 audit:** five "the tracker said it shipped but content was incomplete" findings this session (Pulse ungranted; DA_AFL_AbilitySet_Combat_Pulse never authored; InputData_AFL_Hero authored Dash-only; `DefaultCameraMode=None`; this entire input-routing breakage going undetected since AFL-0214 supposedly closed). The AFL content layer needs a deliberate re-audit pass of every ✅ task against actual tree state.

## Investigation context for the next session

- Local main is 3 commits ahead of origin/main at session end. After the input-edit commit lands, it'll be 4 ahead.
- All AFL ability/combat C++ compiles clean. AFL-0215 lint passes.
- Editor must be closed before any `git add` of `.uassets` (lock conflict bites every time).
- Network was flaky throughout this session; pushes deferred until pipe holds.
- The Python `FPyReferenceCollector` GC-leak warnings throughout the log are MY probes (not session-relevant bugs); editor restart clears them.

---

*Written 2026-05-24/25 EOD as the input-routing diagnostic terminated at the AIK tool boundary. Next session resumes with VS attached + breakpoint on `UPlayerInput::InputKey` per Test C above.*
