# Traps catalog — diagnostic by symptom

This document lists every trap surfaced during the BAG MAN rebuild. Entries are organized by SYMPTOM (what you observe) so future investigators can grep by their actual problem, not by what they think the cause is.

## Categories (navigation aid — entries keep their numbers below)

- **Build / Git:** #4 (PowerShell commit), #12 (UBT mutex), #13 (editor locks + MCP), #21 (git output mediation)
- **GAS / Abilities:** #2 (FObjectFinder null), #3 (ASC lookup), #8 (AddAbilities delivery), #9 (equipment/AnimBP), #10 (CastChecked crash), #11 (RequestGameplayTag ctor), #16 (input dispatch UNRESOLVED)
- **MCP / Editor / Assets:** #1 (`:set()` no-op), #5 (.uasset churn), #6 (cube size), #14 (WorldSettings EditDefaultsOnly), #18 (BP-CDO persist-fail)
- **GameFeature / activation:** #15 (Registered vs Active)
- **Verification discipline:** #7 (dispatch-path audit), #17 (distinguishing experiment didn't distinguish), #19 (orphan component), #20 (memory bit-rot), #22 (bidirectional tracker drift)

## Table of contents (by symptom)

1. [".uasset edit via MCP `:set()` returns ok=true but doesn't actually change anything"](#1-uasset-edit-via-mcp-set-returns-oktrue-but-doesnt-actually-change-anything)
2. [".cpp constructor's `TObjectPtr<DataAsset>` UPROPERTY is null at runtime even though the asset exists"](#2-cpp-constructors-tobjectptrdataasset-uproperty-is-null-at-runtime)
3. [`AFL.Combat.<X>` cheat returns FAIL "no live ASC" or "no live instance" even though the ability is granted](#3-aflcombatx-cheat-returns-fail-no-live-asc-or-no-live-instance)
4. ["PowerShell git commit with multi-line `-m` mangles characters or hangs"](#4-powershell-git-commit-with-multi-line--m-mangles-characters-or-hangs)
5. [".uasset shows as modified in git after editor save but no logical change was made"](#5-uasset-shows-as-modified-in-git-after-editor-save-but-no-logical-change-was-made)
6. ["I placed a cube at scale 1.0 and it's invisible in the level"](#6-i-placed-a-cube-at-scale-10-and-its-invisible-in-the-level)
7. ["I fixed UFUNCTION(Exec) cheats but FAutoConsoleCommand cheats still broken (or vice versa)"](#7-i-fixed-ufunctionexec-cheats-but-fautoconsolecommand-cheats-still-broken)
8. [GameFeature plugin's `UGameFeatureAction_AddAbilities` doesn't deliver grants even though plugin reaches Active](#8-gamefeature-plugins-ugamefeatureaction_addabilities-doesnt-deliver-grants)
9. ["Pawn T-poses, capsule moves, no animation, no fire"](#9-pawn-t-poses-capsule-moves-no-animation-no-fire)
10. ["Editor crashes on PIE start with `CastChecked<ULyraAbilitySystemComponent>` assertion"](#10-editor-crashes-on-pie-start-with-castchecked-assertion)
11. ["`RequestGameplayTag` in CDO constructor ensures-fail / crashes editor"](#11-requestgameplaytag-in-cdo-constructor-ensures-fail)
12. ["`Build.bat` from PowerShell throws UnauthorizedAccessException on `Global\\UnrealBuildTool_Mutex_*`"](#12-buildbat-from-powershell-throws-unauthorizedaccessexception-on-ubt-mutex)
13. [`git pull` / `git checkout` fails on .uassets with "Permission denied" or "Invalid argument"](#13-git-pull--git-checkout-fails-on-uassets-with-permission-denied)
14. ["Python `set_editor_property` on `ALyraWorldSettings.DefaultGameplayExperience` returns success but doesn't take"](#14-python-set_editor_property-on-alyraworldsettingsdefaultgameplayexperience)
15. ["`+GameFeaturesToEnable` in DefaultGame.ini but the plugin's actions never fire"](#15-gamefeaturestoenable-in-defaultgameini-but-the-plugins-actions-never-fire)
16. ["GiveAbility log shows the ability granted, but firing input does nothing — UNRESOLVED"](#16-giveability-log-shows-the-ability-granted-but-firing-input-does-nothing--unresolved)
17. ["A distinguishing experiment didn't actually distinguish — the variable wasn't isolated"](#17-a-distinguishing-experiment-didnt-actually-distinguish--the-variable-wasnt-isolated)
18. ["Python BP-CDO edit reads back correct in-memory but doesn't persist to disk"](#18-python-bp-cdo-edit-reads-back-correct-in-memory-but-doesnt-persist-to-disk)
19. ["Orphan component — authored UCLASS with a self-subscribe lifecycle that nothing grants"](#19-orphan-component--authored-uclass-with-a-self-subscribe-lifecycle-that-nothing-grants)
20. ["Memory bit-rot — a banked filesystem pointer drifts as the codebase moves"](#20-memory-bit-rot--a-banked-filesystem-pointer-drifts-as-the-codebase-moves)
21. ["Git output mediation — the wrapper's exit code lies about what git actually did"](#21-git-output-mediation--the-wrappers-exit-code-lies-about-what-git-actually-did)
22. ["Bidirectional tracker drift — a stale tracker both overstates AND understates"](#22-bidirectional-tracker-drift--a-stale-tracker-both-overstates-and-understates)

---

### 1. .uasset edit via MCP `:set()` returns ok=true but doesn't actually change anything

**Symptom:** MCP `unreal-editor` tool's `asset:set("PropertyName", value)` returns `ok=true r=true`, but a subsequent `:get("PropertyName")` shows the old value. Saving the asset produces no .uasset diff.

**Diagnostic signature:** The property type is one of:
- `TArray<FStruct>` where the inner struct contains instanced UObject subobjects
- An EditInline-newed inline UObject pointer
- A nested struct within an instanced subobject

**Root cause:** The MCP path goes through `FProperty::ImportText_Direct`, which parses ImportText syntax but does NOT have the subobject-construction harness UE's editor UI uses. ImportText for instanced subobjects requires building UObjects with the right Outer and RF_DefaultSubObject flags; the direct path skips this and returns "ok" after parsing the text without actually applying it.

**Fix pattern:**
- For **flat-struct TArrays** (e.g., `ULyraAbilitySet.GrantedGameplayAbilities` where `FLyraAbilitySet_GameplayAbility` has only UPROPERTY value fields): `:set()` WORKS. Always read-back-verify, but it'll usually take.
- For **instanced-subobject TArrays** (e.g., `ULyraInventoryItemDefinition.Fragments`, `GameFeatureAction_AddComponents.ComponentList`, `GameFeatureAction_AddAbilities.AbilitiesList`): hand off to operator editor-UI. Walk them through explicit click paths.
- ALTERNATIVE for BP CDO arrays (instanced or not): use Python `set_editor_property` on the BP's CDO from `unreal.get_default_object(bp.generated_class())`, then `BlueprintEditorLibrary.compile_blueprint(bp)`, then `EditorAssetLibrary.save_asset(...)`. This works for BP CDOs where MCP `:set()` may not.
- ALWAYS read-back-verify after any `:set()` call. The success-report is the failure surface.

**Prevention:**
- Train recognition: if the property is a TArray of UObject pointers or struct-with-object-pointer, expect silent no-op risk.
- For instanced TArrays you must script, pre-flight via the Python `new_object` + `set_editor_property` path on the CDO, with explicit read-back-and-fail-loud assertions.

**Source evidence:** BM-0021 BP fragment list edits (silent no-op). BM-0102 ID fragment edit (silent no-op → editor UI fallback). BM-0103a `LyraAbilitySet.GrantedGameplayAbilities` (worked clean — flat struct). BM-DEBT-AUDIT-001 `GameFeatureData.Actions[0].AbilitiesList[0].ActorClass` (`:set` worked on hard-class-ptr, but neighboring array fields were silently affected by some UE-side validation). BM-DEBT-005-followup-prune (Python `set_editor_property` on BP CDO `Actions` array WITH `compile_blueprint` worked for both prune AND re-author).

**Related pillar:** Pillar 1 (don't bank on `ok=true`; observe runtime state); Pillar 5 (read-back-verify is the distinguishing experiment that catches the silent no-op).

---

### 2. `.cpp` constructor's `TObjectPtr<DataAsset>` UPROPERTY is null at runtime

**Symptom:** A C++ class has a `UPROPERTY` of type `TObjectPtr<UMyDataAsset>` intended to point at a specific asset. At PIE-time the property is null; the ability/component using it falls through to hardcoded fallback defaults (or crashes if no null-guard). Designers editing the asset in the Content Browser see no effect on runtime behavior.

**Diagnostic signature:**
- Python read of CDO via `unreal.get_default_object(cls).get_editor_property("PropName")` returns `None`
- Asset exists at the expected path (verifiable via `unreal.load_asset(path)` succeeding)
- Constructor body doesn't have a `ConstructorHelpers::FObjectFinder` for the property

**Root cause:** UE doesn't auto-resolve `TObjectPtr<>` defaults from a path string. The asset must be explicitly looked up and assigned at CDO-construction time, OR set on a BP child's CDO, OR set per-instance.

**Fix pattern:**
```cpp
#include "UObject/ConstructorHelpers.h"
#include "Path/To/MyDataAsset.h"

UMyClass::UMyClass()
{
    // ... existing ctor body ...

    // Native default ship-pointer. BP children may override per-CDO.
    // FObjectFinder MUST use the full /package.object path (the .object suffix).
    // Succeeded() guard means a missing/renamed asset gracefully falls through to
    // null (rely on null-fallback in runtime use if applicable).
    static ConstructorHelpers::FObjectFinder<UMyDataAsset> Finder(
        TEXT("/AFLCombat/Tuning/DA_AFLPulseTuning.DA_AFLPulseTuning"));
    if (Finder.Succeeded())
    {
        MyDataAssetProperty = Finder.Object;
    }
}
```

Key points:
- `static` is required — FObjectFinder runs once at engine init, must persist.
- The path needs the full `.object` suffix (the asset name appears twice in the canonical form).
- `.Succeeded()` guard is non-negotiable — missing/renamed assets must fall through gracefully, not crash.

**Prevention:**
- Any time you add a `TObjectPtr<>` UPROPERTY intended to have a default value, audit the constructor for the FObjectFinder pattern.
- Post-blackout, Python-read-back the CDO property. Assert path matches expected. Catch silent null before PIE.

**Source evidence:** BM-0104 Pulse tuning DA live-swap (single 3-line ctor edit closed the entire prior build's feel-check; the architecture was sound but the CDO wire was missing).

**Related pillar:** Pillar 3 (DA + per-shot re-read architecture was sound carry-forward; reading the surrounding code revealed the missing ctor wire was the entire fix).

---

### 3. `AFL.Combat.<X>` cheat returns FAIL "no live ASC" or "no live instance"

**Symptom:** Console command like `AFL.Combat.SetSpread 5 20 2` returns `FAIL — no live UAFLAG_Laser_Pulse instance; fire Pulse once first.` even though the ability is granted and (sometimes) was just activated.

**Diagnostic signature:**
- Verbose log shows `GiveAbility Default__<AbilityClass>` line confirming grant
- Pulse-style ability shows `AFL_PULSE: Activate` (or equivalent) on input fire
- Cheat then immediately fails with FAIL warning
- The cheat's ASC lookup path uses `Pawn->FindComponentByClass<UAbilitySystemComponent>()` somewhere

**Root cause:** Lyra's canonical model has the ASC owned by `ALyraPlayerState`, NOT by the pawn. The pawn does NOT implement `IAbilitySystemInterface` and does NOT have an ASC component. `Pawn->FindComponentByClass<UAbilitySystemComponent>()` returns null. Any code path that uses this pattern for "find the player's ASC" will return null and downstream lookups will fail.

**Fix pattern:**
```cpp
#include "AbilitySystemGlobals.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

UAbilitySystemComponent* GetPlayerASC(APlayerController* PC)
{
    if (!PC) return nullptr;
    APlayerState* PS = PC->PlayerState;
    if (!PS) return nullptr;
    // Engine helper: tries IAbilitySystemInterface (Lyra's PlayerState impls it),
    // falls back to component search for BP-only actors.
    return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS);
}
```

Defensive null-guards at every step. Works for `-game` mode (orchestrator cheat matrix), PIE, and shipping configurations alike.

**Prevention:**
- Any ASC lookup from a PlayerController context: route through `PC->PlayerState → UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS)`.
- For pawn-attached components (CMCs, etc.) where the lookup IS from a pawn-attached site: use `GetAbilitySystemComponentFromActor(OwnerPawn)` BUT pair with pawn-extension lifecycle retry (`ULyraPawnExtensionComponent::OnLyraAbilitySystemInitialized`). The pawn's bound ASC arrives async during init.
- For finding the ASC of any hit actor in an ability: use `UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor)` (BP-friendly wrapper around the engine helper).

**Source evidence:** BM-DEBT-005 (plugin-side AddAbilities ActorClass=LyraCharacter). BM-DEBT-008 (cheat manager pawn lookups). BM-DEBT-AUDIT-001 unified all sites. Three repetitions in three sprints → systemic.

**Related pillar:** Pillar 4 (third instance triggered the audit; the audit found a fourth quietly-broken site that no per-sprint fix would have reached).

---

### 4. PowerShell git commit with multi-line `-m` mangles characters or hangs

**Symptom:**
- Multi-line commit message via `git commit -m "..."` over multiple lines hangs PowerShell waiting for closing quote
- Em-dashes (`—`) and Unicode arrows (`→`) appear mangled or as `?` in commit messages
- Commit message lines have wrong indentation after newlines
- Closing-quote heredoc-style attempts cause PowerShell parser errors

**Diagnostic signature:** PowerShell `>>` continuation prompt won't exit; or `git log -1` shows garbled characters.

**Root cause:** PowerShell's multi-line string handling and character encoding differ from bash. The `-m` flag with multi-line strings hits PowerShell quote parsing edge cases. Unicode characters get re-encoded between PowerShell's UTF-16 internal handling and git's UTF-8 storage.

**Fix pattern:** Use `git commit -F <file>` with an ASCII-only file:

```powershell
# In Claude (or operator): write the commit message to a file
# ASCII-only: use -- instead of em-dash, -> instead of arrow, " for quotes
notepad C:\Dev\Project\.git\COMMIT_MY_SPRINT.txt
# (paste ASCII-only message, save, close)

git commit -F C:\Dev\Project\.git\COMMIT_MY_SPRINT.txt
git push
```

ASCII-only rules:
- `--` instead of `—` (em-dash)
- `->` instead of `→` (right arrow)
- Plain `"` instead of smart quotes
- Plain `'` instead of smart quotes

**Prevention:**
- Default to `git commit -F` for any commit message longer than one line, OR containing any non-ASCII character, OR containing code blocks.
- Write the message via `Write` tool to a file, then `git commit -F <file>`. The file gets cleaned up after commit (or left for re-use on amend).

**Source evidence:** BM-0103a, BM-0104, BM-DEBT-AUDIT-001 banks all used this pattern after initial PowerShell `git commit -m` attempts hung or mangled.

**Related pillar:** Pillar 2 (PowerShell-on-Windows is the canonical platform for this project; choose tools that respect platform reality rather than fighting it).

---

### 5. .uasset shows as modified in git after editor save but no logical change was made

**Symptom:** `git status` shows `M Path/To/Asset.uasset` after opening and saving an asset in the editor, but no property values were intentionally changed. `git diff --stat` shows small byte delta (2-4 bytes).

**Diagnostic signature:**
- Asset was opened in the editor (even if just to read)
- Editor saved (Ctrl+S) intentionally or via auto-save
- `git diff` is small and binary; the .uasset is unreadable as text
- Reloading the asset in the editor shows the same property values as before

**Root cause:** UE's serialization writes internal metadata on save: timestamps, generation counters, GUID rotation, internal serialization order may shift. Logical content is unchanged, but the binary representation differs slightly.

**Fix pattern:**
- **If the change is unintentional cosmetic churn:** `git checkout -- Path/To/Asset.uasset` (BUT requires editor closed — see trap #13).
- **If the change is intentional:** commit it as part of the sprint.
- **Mixed case (intentional logical change + cosmetic churn elsewhere):** stage only the intended files explicitly via `git add <specific paths>`, leave others uncommitted, deal with the cosmetic churn later when editor naturally closes.

**Prevention:**
- Don't open assets you don't intend to edit. Use Python read-only access (`unreal.load_asset` + `get_editor_property`) instead of double-clicking in Content Browser.
- For workflows where editor-save is unavoidable (e.g., Blueprint compile saves the BP), accept the cosmetic churn as cost of doing business and discard at next natural editor-close.

**Source evidence:** BM-0104 (DA_AFLPulseTuning churned after content-browser-edit test, discarded via checkout post-editor-close). BM-DEBT-005-followup-prune (B_Experience_BagMan churned 2 bytes after Python rebuild that produced logically-equivalent state to HEAD).

**Related pillar:** Pillar 1 (cosmetic churn is NOT a runtime behavior change; don't bank a commit on this); Pillar 5 (compare diff size + content against expected logical change — if mismatch, it's churn).

---

### 6. I placed a cube at scale 1.0 and it's invisible in the level

**Symptom:** Static mesh actor placed at scale 1.0 expecting a ~1m cube; nothing visible at the placement location. Logs show the actor exists, has the mesh assigned, but nothing renders.

**Diagnostic signature:**
- Actor's static mesh reference resolves to a non-null `UStaticMesh`
- Mesh path contains `/Game/Effects/Meshes/Cube.Cube` (or similar /Effects/ path)
- Bounds on the placed actor are tiny (centimeters, not meters)

**Root cause:** `/Game/Effects/Meshes/Cube.Cube` is 1.12cm — it's an FX-sprite cube intended to be scaled up by Niagara/Cascade particle effects, NOT a real reference cube. UE has TWO confusingly-named cube meshes:
- `/Game/Effects/Meshes/Cube.Cube` — 1.12cm, FX-sprite
- `/Engine/BasicShapes/Cube.Cube` — 100cm, the real reference cube

The /Effects/ one shows up first in Content Browser searches because it lives in /Game/.

**Fix pattern:** Always use `/Engine/BasicShapes/Cube.Cube` for visible level placements. At scale 1.0, it's a 1m cube. For other primitive placeholders: `/Engine/BasicShapes/Sphere.Sphere`, `/Engine/BasicShapes/Cylinder.Cylinder` — all 100cm reference shapes.

**Prevention:**
- When picking a placeholder mesh, type `Engine` in the Content Browser asset picker to filter to `/Engine/` shapes first.
- In Python: `unreal.load_asset("/Engine/BasicShapes/Cube.Cube")` — explicit path, no ambiguity.

**Source evidence:** BM-0102 AAFLDamageTarget BP setup. The 1.12cm cube hid for hours before the size mismatch was diagnosed.

**Related pillar:** Pillar 1 (visible-in-PIE is a runtime gate; if the cube isn't visible to the operator, that's the gate failing); Pillar 3 (working sibling lookup: `/Engine/BasicShapes/Cube` is the canonical reference cube — diff against it when the placeholder is wrong size).

---

### 7. I fixed UFUNCTION(Exec) cheats but FAutoConsoleCommand cheats still broken

**Symptom:** After fixing a shared helper (e.g., ASC lookup), some console cheats work and others don't, despite all being in the same cheat manager class.

**Diagnostic signature:**
- `UFUNCTION(Exec)` member methods on a `UCheatManagerExtension` use one code path (the helper is called via the cheat manager extension dispatch)
- `FAutoConsoleCommand` static singletons use a different code path (the handler is a free function in an anonymous namespace; the helper is called by that free function)
- The two paths may call DIFFERENT helpers — `UAFLCombatCheats::GetPlayerASC` (member) vs `namespace::FindPlayerASCFromAnyWorld` (free function)

**Root cause:** UFUNCTION(Exec) cheats are dispatched through the CheatManager extension lifecycle (requires the extension to be attached to the player's CheatManager at PIE time; the auto-attach via `RegisterForOnCheatManagerCreated` in the CDO must have run). FAutoConsoleCommand cheats are registered as engine-wide console commands via the CDO's static initialization — they fire regardless of CheatManager extension lifecycle.

If you fix ONE helper, the other path may still be broken.

**Fix pattern:**
- Audit both helpers. If both have the same wrong-assumption (e.g., pawn-FindComponentByClass for ASC lookup), fix both.
- Use parallel exact-same fix shape in both, with the same prevention pattern (defensive nullguards, engine-helper routing).
- Verify BOTH paths in PIE: one UFUNCTION(Exec) cheat call AND one FAutoConsoleCommand cheat call.

**Prevention:**
- In a cheat manager with both UFUNCTION(Exec) and FAutoConsoleCommand cheats, identify the helper used by each and check both during any cheat-path audit.
- If the cheat manager is large enough that this matters: extract the ASC lookup into a single shared static helper. Refactor for one canonical lookup path.

**Source evidence:** BM-DEBT-AUDIT-001 fixed both `GetPlayerASC` and `FindPlayerASCFromAnyWorld`. Gate B (UFUNCTION(Exec) `DumpCombatAttributes`) was inconclusive because the UFUNCTION dispatch path wasn't exercised; Gate C (FAutoConsoleCommand `AFL.Combat.SetSpread`) was the verified gate. Both were fixed; only one was watched. Logged as a future-test opportunity.

**Related pillar:** Pillar 4 (audit BOTH dispatch paths when the bug shape applies to a shared helper); Pillar 1 (only the verified-in-PIE path is banked; the unverified path is logged honestly as "fixed but not yet exercised").

---

### 8. GameFeature plugin's `UGameFeatureAction_AddAbilities` doesn't deliver grants

**Symptom:**
- Plugin's `GameFeatureData` has an `Actions` array containing `UGameFeatureAction_AddAbilities` with semantically-correct ActorClass (e.g., `LyraPlayerState`) and a valid `GrantedAbilitySets`
- Plugin reaches `Active` state cleanly per `LogGameFeatures`
- BUT zero `LogAbilitySystem: <PlayerState>: GiveAbility <AbilityClass>` lines appear in PIE verbose log
- Abilities exist on neither the LyraPlayerState nor the pawn

**Diagnostic signature:**
- `LogGameFeatures: ... Game feature '<Plugin>' transitioned successfully. Ending state: Active [Active, Active]` — plugin IS active
- `LogLyraExperience: ... OnExperienceLoadComplete` fires before or around the plugin Active
- No `GiveAbility` log lines for any of the AbilitySet's abilities
- No `LogGameFeatures: Error: Failed to find/add an ability component` error (the action's failure-log line)
- The function was never CALLED, not called-and-failed

**Root cause: UNCONFIRMED — open architectural question (BM-PLUGIN-GRANT-LIFECYCLE).**

Hypothesis: `UGameFeatureAction_AddAbilities::OnGameFeatureActivating` registers a `UGameFrameworkComponentManager` extension handler keyed on ActorClass. The retroactive scan inside this registration may not reach pre-existing LyraPlayerStates that were spawned BEFORE the plugin Active transition. The experience's `OnExperienceLoadComplete` actions, by contrast, run in a context with fresh access to all live actors.

**Fix pattern (workaround):**
Add a mirror `UGameFeatureAction_AddAbilities` to the experience's own `Actions` array, targeting LyraPlayerState, granting the same AbilitySet. The experience-side action delivers reliably; the plugin-side action becomes defensive correctness only.

```
B_<MyExperience>.Actions[N] = UGameFeatureAction_AddAbilities
  AbilitiesList[0]:
    ActorClass         = /Script/LyraGame.LyraPlayerState
    GrantedAbilitySets = [/MyPlugin/Sets/DA_MyAbilitySet]
```

**Prevention:**
- Do NOT trust plugin-side `UGameFeatureAction_AddAbilities` to deliver grants in any experience without verifying it does. Add an experience-side mirror by default for any plugin-shipped AbilitySet.
- Verify via verbose log `LogAbilitySystem: <PlayerState>: GiveAbility <AbilityClass>` line — the per-actor signature, not the per-plugin signature.
- DO NOT extrapolate from one experience's behavior to another. ShooterCore's setup may deliver via plugin-side; that doesn't mean a new plugin will.

**Source evidence:** BM-0101 Pulse false-pass (banked on the wrong premise; experience-side action was what made BM-0102c work). BM-DEBT-005-followup-prune (distinguishing experiment proved plugin-side does not deliver alone). `architectural_bm_plugin_grant_lifecycle.md` in BAG MAN auto-memory.

**Related pillar:** Pillar 5 (BM-DEBT-005-followup-prune's distinguishing experiment caught this — the "1× GiveAbility per ability" evidence was equally consistent with dedup AND with single-path-delivery; removing the experience-side action distinguished them and revealed the truth).

---

### 9. Pawn T-poses, capsule moves, no animation, no fire

**Symptom:** Lyra hero pawn possessed in PIE. WASD moves the capsule. Camera responds. But the mesh shows no animation — T-pose, frozen arms, no idle. LMB fire produces no visible weapon effect.

**Diagnostic signature:**
- `B_Hero_*` BP has `WeaponID=None` and/or `InitialInventoryItems=()`
- `ABP_Mannequin_Base` is the pawn's animation blueprint
- AnimBP-side state machine has weapon-type-driven transitions

**Root cause:** Lyra's `ABP_Mannequin_Base` keys its locomotion state machine off the equipped weapon. With NO equipment, no anim state is selected → T-pose. Equipment is load-bearing for animation AND input dispatch. Stripping equipment T-poses the pawn AND breaks fire.

**Fix pattern:**
- NEVER set `WeaponID=None` or `InitialInventoryItems=()` on a Lyra hero pawn intended for play.
- To customize a weapon: DUPLICATE `WID_Pistol` (and `ID_Pistol` wrapper) into your namespace, edit the duplicate's `ActorsToSpawn` for new visuals and `AbilitySetsToGrant` for new ability — keep the equipment chain intact. Inherit pistol's proven anim-state mapping by keeping the same `InstanceType` (compatible with ABP_Mannequin_Base's pistol-locomotion state machine).

```
WID_BagMan_PulseCarbine (duplicate of WID_Pistol)
  +-- InstanceType = ULyraEquipmentInstance (keep pistol-compatible)
  +-- AbilitySetsToGrant = [AbilitySet_BagMan_NoFire]  // empty grant
  +-- ActorsToSpawn = [BagMan visuals]

ID_BagMan_PulseCarbine (duplicate of ID_Pistol)
  +-- Fragments[0] = InventoryFragment_EquippableItem
        EquipmentDefinition = WID_BagMan_PulseCarbine
```

**Prevention:**
- Reskin/swap equipment, never remove.
- Any pre-PIE checklist for a hero pawn: confirm `WeaponID` is set AND `InitialInventoryItems` has at least one entry.
- For damage-sink test actors that need to NOT have weapons: don't use a Lyra hero class. Use a plain `AActor + IAbilitySystemInterface` (like `AAFLDamageTarget`).

**Source evidence:** BM-0102b B1 strip approach failed via this trap. Banked in `reference_lyra_equipment_animbp_coupling.md`.

**Related pillar:** Pillar 2 (Lyra-canonical means equipment is a load-bearing dependency of the hero pawn; reskin/swap rather than removing); Pillar 3 (pistol is the working sibling — duplicate it, don't try to author empty equipment from scratch).

---

### 10. Editor crashes on PIE start with `CastChecked` assertion

**Symptom:** PIE start crashes with assertion failure mentioning `CastChecked<ULyraAbilitySystemComponent>`. Editor closes. CrashReport dialog opens.

**Diagnostic signature:**
- Call stack includes `UGameFeatureAction_AddAbilities::AddActorAbilities` or `HandleActorExtension`
- Map contains an unpossessed map-placed Lyra hero pawn (`AutoPossessPlayer=Disabled`, no AI controller, `B_Hero_*` or `ALyraCharacter` subclass)
- AFLCombat (or another GameFeature that calls AddAbilities targeting `ALyraCharacter`) is Active

**Root cause:** Lyra's `UGameFeatureAction_AddAbilities::AddActorAbilities` does `CastChecked<ULyraAbilitySystemComponent>` on every matching actor's ASC component. A POSSESSED Lyra pawn gets its typed `ULyraAbilitySystemComponent` injected via the PlayerState lifecycle — that cast succeeds. An UNPOSSESSED map-placed Lyra hero pawn has only a plain engine `UAbilitySystemComponent` subobject — Lyra never replaces it. `CastChecked` is a hard cast → assertion fail → fatal crash.

**Fix pattern:**
- **NEVER place an unpossessed `ALyraCharacter` (or any subclass like `B_Hero_*`) in any map while AFLCombat or any other AddAbilities-using plugin is Active.**
- For damage-sink recon, options:
  - Fire at greybox geometry (walls/floor) for GE-application logging without an ASC target
  - Spawn a possessed actor at PIE-runtime via console (AI controller — Lyra wires the ASC on possession)
  - Use a non-`ALyraCharacter` damageable actor type (like `AAFLDamageTarget`) that has its own ASC + attribute set self-contained, no PlayerState injection lifecycle

**Prevention:**
- For map-placed test targets: never use Lyra hero classes. Always use plain `AActor + IAbilitySystemInterface` self-contained patterns.
- If you must place a Lyra hero in a map (e.g., a cinematic), ensure `AutoPossessPlayer=Player0` or an AI controller is assigned, AND the GameFeature plugin's AddAbilities action handles the case (defensive `Cast<>` + skip instead of `CastChecked<>`).
- Filed as BM-DEBT-004 in BAG MAN: the Lyra-side action SHOULD use defensive `Cast<>` and skip, not `CastChecked<>`. One-line engine fix that requires blackout build; deferred.

**Source evidence:** BM-0102 Phase B recon. Crashed PIE on placing `Dummy_Target_BM0102` (`B_Hero_ShooterMannequin_C`, `AutoPossessPlayer=Disabled`).

**Related pillar:** Pillar 2 (Lyra-canonical means respecting the PlayerState-injection lifecycle for hero pawns; bypass it only by switching to a different actor pattern, not by trying to make the hero pawn behave outside its expected lifecycle).

---

### 11. `RequestGameplayTag` in CDO constructor ensures-fail

**Symptom:** Editor crash on module load or on first CDO instantiation. Error log shows:
```
Ensure condition failed: false [GameplayTagsManager.cpp:2213]
Requested Gameplay Tag <TagName> was not found, tags must be loaded from config or registered as a native tag
```

**Diagnostic signature:** Crash callstack shows a class constructor (e.g., `UMyAbility::UMyAbility()`) calling `FGameplayTag::RequestGameplayTag(TEXT("..."))`. The tag is declared in the same plugin's `Config/Tags/*.ini`.

**Root cause:** CDO construction runs at module load. Per-plugin `Config/Tags/*.ini` files are scanned and registered by `UGameplayTagsManager` in a separate pass that can race with module-load CDO construction within the same plugin. Same-plugin lookups reliably lose the race. Cross-plugin lookups usually work (load order tends to favor them) but the dependency is fragile.

**Fix pattern:**
Declare tags natively at file scope in the .cpp:
```cpp
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Firing_Pulse, "State.Firing.Pulse");

UMyAbility::UMyAbility()
{
    ActivationOwnedTags.AddTag(TAG_State_Firing_Pulse);  // the static, NOT RequestGameplayTag()
}
```

Key points:
- `UE_DEFINE_GAMEPLAY_TAG_STATIC` registers the tag at module init, strictly before any CDO of a class in the module is constructed.
- The ini still declares the tag as the spec source-of-truth. UE dedups native + ini registrations of the same tag.
- For shared cross-file tags (multiple files in the same module reference the same tag), use Lyra's pattern: `UE_DECLARE_GAMEPLAY_TAG_EXTERN` in a header + `UE_DEFINE_GAMEPLAY_TAG_COMMENT` in a cpp.

**Prevention:**
- For any new `UGameplayAbility`/`UGameplayEffect`/etc. C++ class that references tags in its constructor, declare those tags natively at the top of the .cpp.
- Tags safe to call `RequestGameplayTag` in non-constructor contexts: test specs, cheat managers, runtime `ApplyDamage`/`ActivateAbility` bodies, `ExecCalc::Execute_Implementation`, listener callbacks. All run after module init.
- Lint rule: scan .cpp files for `::ClassName()` constructors containing `RequestGameplayTag` calls. Fail CI.

**Source evidence:** Crashed editor 2026-05-20. Lint rule landed in AFL-0215 lint workflow (`Tools/AFL_Yolo/lint_afl_0215.py`).

**Related pillar:** Pillar 2 (Lyra-canonical convention is `UE_DEFINE_GAMEPLAY_TAG_STATIC` for CDO-ctor-referenced tags; adopt the convention rather than fighting the lifecycle).

---

### 12. `Build.bat` from PowerShell throws UnauthorizedAccessException on UBT mutex

**Symptom:**
```
Unhandled exception: UnauthorizedAccessException: Access to the path
'Global\UnrealBuildTool_Mutex_<hash>' is denied.
```
PowerShell UBT build invocation fails in <1 second. No code compiled.

**Diagnostic signature:**
- Error mentions `Global\UnrealBuildTool_Mutex_<hash>` (the project-specific named mutex)
- Build completes in 0.3-0.5 seconds (didn't actually build anything)
- One of: (a) UE Editor for this project is open, (b) a leftover Build.bat process from a prior session is still running, (c) a stuck `dotnet` process in Session 0 / SYSTEM session holds the mutex

**Root cause:** UBT uses a global named mutex per-project to coordinate Live Coding with external builds. UE Editor acquires the mutex at startup and holds it until process exit. External Build.bat from a different Windows session cannot acquire the mutex while a holder exists.

**Fix pattern:**

Three failure modes, in increasing pain order:

1. **UE Editor open (most common).** Get-Process UnrealEditor* — if present, close UE before any external Build.bat call. Both .uasset locks and the mutex clear within seconds.

2. **Leftover in-session Build.bat (uncommon).** UE Editor closed, but a prior Claude-spawned Build.bat process didn't fully exit. Verify via `Get-Process | Where-Object { $_.ProcessName -match 'UnrealBuildTool|dotnet' }`. Wait 30s and retry (UBT releases on its own), OR Stop-Process if it's been stuck >2 minutes.

3. **Zombie dotnet in Session 0 (rare, painful).** Persistent .NET host process in SYSTEM session won't die at user level. Get-Process dotnet — if anything shows with `SI 0` or large working set (>50MB) that's been there a long time, admin PowerShell + `Stop-Process -Id <pid> -Force`, OR reboot.

**Prevention:**
- Pre-flight check before any orchestrator-spawned compile task: `Get-Process UnrealEditor*` AND `Get-Process dotnet | Where-Object {$_.WS -gt 50MB}`. Halt with actionable message if either matches.
- Never propose blanket admin/full-control just to work around this — it's a much bigger hammer than needed and persists for the session.
- Don't probe mutex state with `Build.bat -NoActions` — that approach was tried (2026-05-22) and observed to itself spawn new Session 0 zombies via cmd.exe → dotnet.exe orphan chains. Process-list heuristics only.

**Source evidence:** Heavily banked in `feedback_ue_open_locks_uassets.md`. Lost work to this trap twice on 2026-05-21 (AFL-0211 destroyed via rollback), once on 2026-05-22 (AFL-0213 destroyed by Session 0 zombie requiring reboot).

**Related pillar:** Pillar 2 (PowerShell + Windows + UE5 has lifecycle constraints; respect the platform's reality — close editor before external builds rather than fighting locks).

---

### 13. `git pull` / `git checkout` fails on .uassets with "Permission denied"

**Symptom:** Git operation fails with one of:
- `unable to unlink ... Invalid argument` (on .uassets)
- `Permission denied` on a specific .uasset
- Stash pop fails partway through

**Diagnostic signature:**
- UE Editor has `<Project>.uproject` loaded
- Git status shows uncommitted .uasset modifications
- The git operation is touching the .uassets that are loaded in the editor

**Root cause:** UE Editor keeps exclusive write handles on every loaded asset. Even tiny editor-metadata changes become unmergeable while UE is open.

**Fix pattern:**
- Ask the operator to close UE before the git operation. Don't force-kill processes.
- After UE closes, both .uasset locks and the UBT mutex (trap #12) clear within seconds.
- Resume git operation.

**Important corollary — the MCP-access prohibition (the other side of this trap).** When Claude is mid-session with the editor open and MCP-dependent, do NOT ask the operator to close the editor for git operations Claude is running — closing the editor severs Claude's MCP access (and the in-editor Python path), so Claude can't run anything until the editor reopens. Two consequences:
- **For git ops on locked .uassets:** use the still-open editor's Python to mutate the asset back to the desired state (the BM-DEBT-005-followup-prune recovery: `set_editor_property` + `compile_blueprint` + `save_asset`), rather than asking for an editor-close that strands the session.
- **For UBT builds (trap #12), the constraint INVERTS:** a build *requires* the editor closed (to release the mutex), but the close severs MCP. So a build is a deliberate close→build→reopen cycle the operator drives — never a mid-MCP-work request. The post-build PIE verification is console-driven (no MCP needed), which is what makes the cycle workable. Distinguish "I need MCP" work (editor stays open) from "I need a build" work (editor closes, MCP unavailable, verification happens via console). Source: BM-0106 (the AddComponents grant was authored via in-editor Python — editor stayed open throughout; BM-0105c's ForceRTT build, by contrast, required the close→build→reopen cycle).

**Prevention:**
- Don't `git pull`/`merge`/`checkout` paths that overlap with loaded .uassets while editor is open.
- For Claude-driven git operations mid-session: stage and commit only .cpp / .h / non-uasset files. Defer uasset cleanup to next natural editor-close.

**Source evidence:** `feedback_ue_open_locks_uassets.md`. Hit during BM-DEBT-005-followup-prune recovery — git checkout couldn't restore the .uasset while editor held the lock; Python re-author was the alternative.

**Related pillar:** Pillar 2 (work WITH the editor's lifecycle constraints rather than against them — pick git operations that don't conflict with the editor's locks, or pick non-git mutations that work through the editor's APIs).

---

### 14. Python `set_editor_property` on `ALyraWorldSettings.DefaultGameplayExperience`

**Symptom:** Python script attempts to wire a level's WorldSettings to a specific Experience BP via `world_settings.set_editor_property("DefaultGameplayExperience", ExpClass)`. Script logs success but the value doesn't change. PIE on the map falls back to `B_LyraDefaultExperience`.

**Diagnostic signature:** Error or warning mentions `Property '<X>' cannot be edited on instances`.

**Root cause:** Lyra's `ALyraWorldSettings.DefaultGameplayExperience` is declared `protected` + `EditDefaultsOnly`. UE5's `PropertyAccessUtil::CanSetPropertyValue` blocks the write when the owner isn't a template (CPF_DisableEditOnInstance). Lyra's editor WorldSettings UI has a custom detail view that bypasses this; Python doesn't.

**Fix pattern:**
- **Manual route (recommended for one-off):** open the map in the editor, set WorldSettings → Default Gameplay Experience in the panel, save. ~30 seconds.
- **No headless route currently exists in BAG MAN.** A reflection-helper helper (e.g., `UAFLArenaAuthoringLibrary::SetWorldSettingsDefaultExperience` using raw FProperty reflection to bypass the EditDefaultsOnly gate) was prototyped during AFL-0110 (2026-05-22) but never compiled into the project — UBT mutex was held by another session and the work was deferred. AFL-0110-style helper authoring is pending work; until it lands, the manual UI route is the only path.

**Prevention:**
- For Lyra-canonical EditDefaultsOnly properties, expect Python `set_editor_property` to refuse. Don't infer the write succeeded from "no exception thrown" — read back via `get_editor_property` and compare.
- If a sprint needs to script this write, the prerequisite is landing the AFL-0110-style reflection helper first. Don't conflate sprints by trying to do both.

**Source evidence:** AFL-0110 (2026-05-22). The duplicated map + new experience BP were correct on disk; WorldSettings wiring deferred to manual UI step.

**Related pillar:** Pillar 1 (read-back-verify, "no exception" is not "succeeded"); Pillar 2 (when Lyra's canonical pattern blocks scripting, defer the headless work to its own sprint rather than fighting the gate inline).

---

### 15. `+GameFeaturesToEnable` in DefaultGame.ini but the plugin's actions never fire

**Symptom:** `Config/DefaultGame.ini` contains `+GameFeaturesToEnable=MyPlugin` under `[/Script/GameFeatures.GameFeaturesSubsystemSettings]`. PIE starts, no errors, BUT the plugin's `Actions` (AddAbilities, AddComponents, etc.) don't fire.

**Diagnostic signature:**
- Verbose log shows `LogGameFeatures: Display: Game feature 'MyPlugin' transitioned successfully. Ending state: Registered [Registered, Active]`
- NOT a later `Ending state: Active [Active, Active]` line for that plugin

**Root cause:** `+GameFeaturesToEnable` pushes a plugin to **Registered** state at editor startup. NOT Active. The plugin's GameFeatureData asset loads, its module DLL initializes, but its `Actions` array does NOT fire. A plugin reaches **Active** only when an experience that lists it in its own `GameFeaturesToEnable` array loads. Activation is per-experience, not per-project.

The bracketed `[Registered, Active]` in the log shows the transition path FROM Registered TOWARD Active — but `Ending state: Registered` is where it actually stops.

**Fix pattern:**
- Edit the experience asset's `GameFeaturesToEnable` array. Add `"MyPlugin"` to the array.
- Re-PIE. Verify `Ending state: Active [Active, Active]` for the plugin AFTER `LogLyraExperience: OnExperienceLoadComplete`.

**Prevention:**
- Don't infer activation from the ini line alone. The ini drives Registered; the experience drives Active.
- Phase-0-style isolation: comment out `+GameFeaturesToEnable=MyPlugin` to prevent Registered (belt-and-braces); also confirm the experience's `GameFeaturesToEnable` doesn't list the plugin (the real gate).

**Source evidence:** BM-0100 false-pass. `+GameFeaturesToEnable=AFLCombat` was in DefaultGame.ini, but `B_Experience_BagMan.GameFeaturesToEnable` only listed `ShooterCore`. The 5-control gate "passed" but tested nothing about AFL attach. Caught by tailing verbose log post-PIE.

**Related pillar:** Pillar 1 (the "passing" gate was measuring the wrong variable; verbose log of the per-actor signature is what the gate should have measured).

---

### 16. GiveAbility log shows the ability granted, but firing input does nothing — UNRESOLVED

**Status: UNRESOLVED; investigation pending.** This trap is a DIAGNOSTIC CHECKLIST, not a fix recipe. The root cause for the specific BM-0102b B1 instance was not converged on — source-read narrowed the failure cause to outside the read dispatch path, but the actual mechanism remains unknown. Future investigation needed.

**Symptom:**
- Verbose log shows `LogAbilitySystem: <PlayerState>: GiveAbility <AbilityClass>` (grant confirmed)
- Input fire (LMB or otherwise) produces zero `<Ability>: Activate` log lines
- Stock weapon may or may not fire

**Diagnostic signature:**
- The ability's `DynamicSpecSourceTags` contains the expected `InputTag` (verifiable via `showdebug abilitysystem`)
- `ULyraAbilitySystemComponent::AbilityInputTagPressed` is the dispatcher
- Equipment may or may not be present on the pawn

**Diagnostic steps — sub-causes to investigate as branches:**

**(a) CommonUI input-mode interception.** Look for `LogUIActionRouter: Display: UIInputConfig being changed. bForceRefresh: 1` at experience load. CommonUI has its own input layer that can capture before ASC sees it.

→ *Diagnostic:* check `LogUIActionRouter` verbose for input-mode changes. If the UI is in `ECommonInputMode::Menu` or has captured input focus, ASC dispatch won't see the press. Look for the input mode at the moment of the failed fire.

**(b) `bReadyToBindInputs` lifecycle.** `ULyraHeroComponent::OnBindAbilityActions` may not have fired if the pawn-extension lifecycle is in a bad state. Native bindings (Move/Look/Jump) can be active while AbilityActions binding hasn't fired.

→ *Diagnostic:* check verbose `LogLyraHero` (if logged) for `OnBindAbilityActions` firing. If absent, the pawn-extension lifecycle didn't reach the binding step — even though the grant landed, the dispatch never wired up.

**(c) Equipment-driven input gate.** If equipment is stripped (`WeaponID=None`), the AnimBP T-poses AND the input chain may be partially broken. See trap #9.

→ *Diagnostic:* confirm `WeaponID` and `InitialInventoryItems` are populated. If both are empty, equipment IS the trap — fix that first before assuming it's deeper.

**(d) The ability's spec wasn't actually granted to THIS PlayerState.** Even though a GiveAbility log line exists, double-check it's on the PlayerState the pawn is reading from (PIE may have multiple players, `PlayerState_1` vs `PlayerState_0`).

→ *Diagnostic:* `showdebug abilitysystem` in PIE console — confirm the ability's spec is on the pawn's currently-resolved ASC. If it's on a different PlayerState, the grant landed but on the wrong target.

**Prevention:**
- After granting an ability, verify it fires via input in PIE before banking. "Grant log line present" doesn't imply "input dispatches to it."
- For new input bindings, smoke-test by adding a verbose log to the ability's `ActivateAbility` — confirm it fires on input.

**Source evidence:** BM-0102b B1 failure investigation. Source-read narrowed the failure cause to outside the read dispatch path; open leads identified but root cause still UNRESOLVED for that specific failure. Banked in `reference_lyra_equipment_animbp_coupling.md`. Future investigation pending.

**Related pillar:** Pillar 1 (don't bank "input wired" without observing the ability activate); Pillar 5 (sub-causes (a)-(d) are alternative interpretations — the diagnostic steps are distinguishing experiments to identify which is true for a specific failure instance).

---

### 17. A distinguishing experiment didn't actually distinguish — the variable wasn't isolated

**Symptom:** You run an experiment meant to prove X by varying one parameter, and the result *looks* like proof — but the parameter you varied wasn't the only thing changing. Two named sub-symptoms:
- **(a) Hand-aim variance:** firing at a moving target by hand to test a verdict (hit/reject, accept/deny). Every shot's impact point differs, so "fired at 0.2 RTT → accept" and "fired at 0 RTT → reject" are two *uncontrolled samples*, not a controlled A/B.
- **(b) Velocity-zero phase:** the moving target is at a sweep extreme (sinusoid peak) where its velocity ≈ 0, so "past position" and "current position" are nearly identical — the verdict can't distinguish them even though the parameter (RTT) changed correctly.

**Diagnostic signature:**
- The experiment's "proof" is a verdict flip (A→B) across a parameter change, but the *other* inputs (aim point, target position, timing) also moved between the two trials.
- Re-running the "same" trial produces inconsistent verdicts (because the uncontrolled inputs vary each run).
- For (b): the logged "past" and "current" coordinates differ by less than the tolerance/box width — e.g., separation ~15cm against a ~115cm box.

**Root cause:** A single-variable proof requires every input *except* the variable-under-test to be held constant. A moving target + hand-aim couples three variables (the test parameter, the aim point, the timing). The verdict flip could be caused by any of them. Convenient interpretation: "the parameter caused it." Pillar 5 inconvenient truth: you didn't isolate it.

**Fix pattern:**
- Drive the verdict path from a **fixed-coordinate debug command**, not the mouse. Latch the coordinate so both trials fire at a byte-identical point (e.g. `afl.LagComp.TestFire` latches the target's past position; `TestFire replay` reuses the exact latched coord). Then only the test parameter (RTT) varies → verdict flip is attributable to it alone.
- For (b): ensure the separation the experiment relies on actually exceeds the tolerance. Fire at the target's *maximum-velocity* phase (sinusoid center-crossing), and **log both the past and current coordinates** so the operator can confirm the separation is real (e.g. 233cm) before trusting the verdict. If the logged separation is small, re-latch — the trial is meaningless.

**Prevention:**
- For any hit-confirmation / geometric-verdict proof that must isolate ONE variable (RTT, box size, pad), remove the human from the input loop. Hand-aim is fine for "does it work at all" smoke checks, never for single-variable proofs.
- Slowing the target down *reduces* but does not *eliminate* the variance — it yields a better uncontrolled sample, not a controlled experiment. Eliminate, don't reduce.
- Make the experiment self-reporting: log the controlled inputs (the latched coordinate, the dt) so a false-isolation is visible in the evidence, not assumed away.

**Source evidence:** BM-0105c. Session 1 (commit bfc8b8c9) deferred the isolated flip because hand-aim could only produce directional evidence (trailing-accept/current-reject at fixed RTT) — proof that the box was at the past pose, but not single-variable isolation. Session 2 (commit 17ce2fc8) authored `afl.LagComp.TestFire` with a latched coordinate; the *first* flip attempt latched at a sweep peak (15cm separation → both legs accepted, a false-pass caught pre-bank); re-latching at center-crossing (233.79cm separation) produced the clean ACCEPT→REJECT flip with C byte-identical across both legs.

**Related pillar:** Pillar 5 (the convenient "the parameter caused the flip" interpretation vs the inconvenient "you didn't isolate the parameter" — the fixed-coordinate latch is the experiment that actually distinguishes); Pillar 1 (the operator must observe the controlled inputs in the log, not just the verdict).

---

### 18. Python BP-CDO edit reads back correct in-memory but doesn't persist to disk

**Symptom:** A Python `set_editor_property` on a Blueprint's CDO (e.g. appending to a `GameFeatureAction_AddComponents.ComponentList`) reads back correctly via `get_editor_property` — the in-memory CDO holds the change — but `git status` shows the `.uasset` unmodified. The change is not on disk; a fresh load wouldn't see it.

**Diagnostic signature:**
- In-memory read-back (`get_editor_property` right after the set) shows the new value. **This is the false-confidence surface** — the edit *is* in memory.
- `EditorAssetLibrary.save_loaded_asset(bp)` / `save_asset` returns without error.
- BUT `git status` shows no `M` on the `.uasset`, OR a reload-from-disk doesn't carry the change.

**Root cause:** A Blueprint's runtime state lives in its `BlueprintGeneratedClass`; the editable CDO and the serialized class can diverge. Mutating the CDO via `set_editor_property` alone does not always propagate into the BlueprintGeneratedClass that gets serialized — so the save writes the *unchanged* class. `compile_blueprint` is what propagates CDO state into the generated class (and marks the package dirty correctly) so the subsequent save persists the change.

**Fix pattern:**
```python
cdo = unreal.get_default_object(gen_class)
# ... mutate cdo via set_editor_property (for instanced-struct TArrays, copy an
#     existing element with element.copy(), mutate the copy, append) ...
unreal.BlueprintEditorLibrary.compile_blueprint(bp)   # propagate CDO -> generated class
unreal.EditorAssetLibrary.save_asset(bp_path, only_if_is_dirty=False)
```
Then **verify persistence via the git working tree**, not the in-memory read:
```
git status -sb -- path/to/Asset.uasset   # expect M (modified on disk)
```
The git working tree is the persistence oracle — it tells you the bytes actually changed on disk, which the in-memory CDO read cannot.

**Prevention:**
- For any Python BP-CDO mutation: `compile_blueprint` BEFORE `save_asset`, always. The compile is not optional.
- NEVER trust the in-memory `get_editor_property` read-back as proof of persistence — it confirms the edit reached memory, not disk. Confirm with `git status` showing the `.uasset` modified.
- This is distinct from trap #1 (MCP `:set()` silent no-op on instanced TArrays — that fails to apply at all, even in memory). Here the edit *applies in memory* and silently fails to *persist*. Different mechanism, same defense: read-back-verify, but the verification must be at the disk layer (git), not the memory layer.

**Source evidence:** BM-0106 (commit ab683344). The `UAFLHitConfirmComponent` grant appended to `B_Experience_BagMan`'s `AddComponents` ComponentList read back correct in-memory, but `git status` showed no `.uasset` change after the first `save_loaded_asset`. Adding `compile_blueprint` before `save_asset` committed it to disk; git then showed `M Content/BagMan/Experiences/B_Experience_BagMan.uasset`. The git check is what caught the non-persistence.

**Related pillar:** Pillar 1 (in-memory read-back is not runtime/persistent truth; the git working tree is the observable proof the bytes changed); Pillar 5 (the convenient "save returned no error so it saved" vs the inconvenient "the bytes aren't on disk" — git status is the distinguishing check). Cross-link: trap #1 (related MCP-edit mechanism, but that one no-ops in memory too; this one persists-fails after applying in memory).

---

### 19. Orphan component — authored UCLASS with a self-subscribe lifecycle that nothing grants

**Symptom:** A `UActorComponent` subclass exists in the codebase, self-subscribes to an event on `BeginPlay` (e.g. `UGameplayMessageSubsystem::RegisterListener`), and has all its logic — but in PIE nothing reacts to the event it listens for. A `BroadcastMessage` fires with no live subscriber: a tree falling in an empty forest.

**Diagnostic signature:**
- The subscriber class exists in source and looks complete (registers on BeginPlay, has the callback).
- Grep the class name across `Content/**.uasset` + PawnData/experience definitions → **zero hits outside its own source files and tests.**
- The broadcast site fires (verifiable in log), but the subscriber's reaction (its log line, its visible effect) never appears in PIE.

**Root cause:** The component was authored but never *granted* to any pawn/PC — no `GameFeatureAction_AddComponents` entry, no PawnData component list, no ctor `CreateDefaultSubobject` on a spawned actor. Its `BeginPlay` never runs because it's never instantiated on a live actor. This is the prior build's habit made concrete: author a component, forget the grant. The class compiles, ships, and looks done — but does nothing at runtime.

**Fix pattern:**
- Grant the component via the canonical path for its role. For player-feedback components → an experience-side `GameFeatureAction_AddComponents` targeting the appropriate actor class (e.g. `LyraPlayerController` for per-player feedback; client-only = `bClientComponent=true, bServerComponent=false`). For self-contained test actors → ctor `CreateDefaultSubobject`.
- Verify the round-trip in PIE: the broadcast fires AND the subscriber observably reacts (its log line + visible effect). Both halves — broadcast-called is not the same as subscriber-reacted (Pillar 5).

**Prevention — the audit trigger (Pillar 4 in formation):**
- This is a *counting* pattern. ONE orphan is a bug; TWO is a coincidence; **THREE triggers a Pillar-4 audit.** Currently at TWO confirmed (see source). When a third surfaces, STOP feature work and audit ALL AFLCombat `UActorComponent` subclasses: grep each class name in `Content/**.uasset` + PawnData; zero hits outside source/tests = orphaned. Bank the full attach-coverage picture rather than discovering orphans one sprint at a time.
- For any new self-subscribing component: author the grant in the SAME sprint as the component. The grant is part of "the component works," not a follow-up.

**Source evidence:** Two confirmed instances. BM-0105b: `UAFLPawnHitboxHistoryComponent` — resolved by carrying it in the `AAFLLagTestDummy` ctor (test actor); the player-pawn grant was deferred. BM-0106 (commit ab683344): `UAFLHitConfirmComponent` — authored, self-subscribes to `Event.Damage.Confirmed`, but no grant existed; grep of Content showed zero references; fixed by adding it to `B_Experience_BagMan`'s `AddComponents` action. A third instance (likely AFL-0402's `UAFLDismemberComponent` in S4) triggers the audit. Banked in `feedback_orphan_component_watch.md`.

**Related pillar:** Pillar 4 (the anticipatory mode — watching the count climb toward the audit threshold, rather than auditing after the fact; see the "Pillar 4 in formation" worked example in methodology.md); Pillar 1 (a component that compiles but is never instantiated is the "✅ on code-authored, not feature-works" failure mode in component form).

---

### 20. Memory bit-rot — a banked filesystem pointer drifts as the codebase moves

**Symptom:** A memory note (or any durable doc) cites a file path or version label that was correct when banked, but the file has since been renamed/moved/version-bumped. Acting on the banked pointer sends you to a path that doesn't exist, or asserts a version that's stale.

**Diagnostic signature:**
- A memory says "the tracker is at `X_v2_2.html`" or "the SSOT is v2.3," and the live filesystem shows a different filename or version.
- A `[[wikilink]]` or path reference resolves to nothing, or to a superseded file.
- The memory's own timestamp is days/weeks old; the cited artifact has a newer mtime.

**Root cause:** A memory is a point-in-time observation, not live state. File paths, version labels, and "current" filenames drift as the codebase evolves (renames, version bumps, agent-edits). The bank was accurate at write-time; the world moved underneath it. This is especially acute for artifacts edited by *other* agents (e.g. an AIK that version-bumps a tracker on each edit — see `project_aik_version_labels_not_verification`).

**Fix pattern:**
- **Verify the pointer at consumption time, not at bank time.** Before acting on a banked path: `ls`/Glob it. Before asserting a banked version: read the live file's label. If they disagree, trust the live read and *correct the memory* (the correction is itself a finding).
- When correcting: note that the memory wasn't *wrong* at write-time — the file moved. Record both the old and new state so the drift history is legible (e.g. "said v2.3; that was the operator-verified label at write-time; AIK edits advanced it to v2.7").

**Prevention:**
- Treat every banked file path / version label as "true as of the bank date," never as "true now." A memory that names a path is a claim it existed *when written*.
- For pointers consumed across sessions: re-verify on the first read of a new session. Cheap (one Glob/Read); the alternative (planning on a stale pointer) is expensive.
- Prefer durable anchors over volatile ones in banks: a git commit SHA is immutable; a versioned filename is not. Cite the SHA when possible.

**Source evidence:** BM-0106 handoff (2026-05-28). The `reference_tracker_and_paths` memory pointed at `BAG_MAN_LIVE_TRACKER_v2_2.html` — a filename that no longer existed (current was `BAG_MAN_LIVE_TRACKER.html`). The same session's handoff banked "v2.3 SSOT"; the next session's read of the live file showed **v2.7** (AIK had version-bumped it between sessions). Both corrected by reading the live file before acting. See `project_aik_version_labels_not_verification`.

**Related pillar:** Pillar 1 (the live filesystem is the source of truth; a banked pointer is a report — verify against the running state before acting); Pillar 5 (the convenient "the memory says X so X is true" vs the inconvenient "the file moved" — the Glob/Read is the distinguishing check).

---

### 21. Git output mediation — the wrapper's exit code lies about what git actually did

**Symptom:** A `git push` (or other git op) run through a tool wrapper or PowerShell reports a result that contradicts what git actually did. Two confirmed, opposite failure modes:
- **(a) Wrapper-exit-0 hides a real failure:** the background-task / wrapper summary reports `exit code 0` (success), but the literal git output is `fatal: ... Connection timed out ... EXIT 128`. The push did NOT happen.
- **(b) PowerShell wraps stderr as a RemoteException hiding a real success:** a native git command's stderr (which git uses for progress, not just errors) gets wrapped by PowerShell into a `NativeCommandError`/RemoteException, making a *successful* operation look failed (and setting `$?` to false even on exit 0).

**Diagnostic signature:**
- (a) The wrapper/summary says success, but `git status -sb` still shows `[ahead N]` and `git log origin/main` doesn't show the new commit.
- (b) PowerShell surfaces a red error block citing the git command, but the operation's effect (the push, the commit) actually landed.
- In both: the *mediated* report (wrapper exit, PowerShell `$?`) disagrees with the *literal git text* and the *independent git state*.

**Root cause:** Layers between you and git — a background-task exit-code summary, PowerShell's native-stderr handling — re-interpret git's output and can invert its meaning. Git communicates via stdout/stderr text and its own exit codes; any wrapper that summarizes those can lose or invert the signal. Git also uses stderr for non-error progress, which trips stderr-means-failure heuristics.

**Fix pattern:**
- **Verify every git mutation by the literal git text + independent git state, never the mediated report.** After a push:
```
git push origin main 2>&1     # READ the literal output lines
git status -sb                 # independent: expect no [ahead]
git log --oneline -1 origin/main   # independent: expect the new SHA at origin
```
Success = the literal `<old>..<new>  main -> main` line AND `git status` clean AND `origin/main` shows the new SHA. All three, from real git — not the wrapper's exit code.
- For (b) specifically: avoid `2>&1` on native git in PowerShell where it wraps stderr as ErrorRecords; or treat a PowerShell red block on a git command as "inspect the literal output + git state," not "it failed."

**Prevention:**
- Bake the independent-verify into every push/commit bank step. The wrapper exit code is an input to suspicion, not a verdict.
- When a wrapper reports success on a network op, *still* check `git status` — network ops are exactly where (a) bites.
- Treat this as potentially systemic: it has fired multiple times. If it recurs, the mitigation (parse literal git lines) is cheap and should be reflexive.

**Source evidence:** Confirmed multiple times in the BAG MAN rebuild. The clearest: the tracker-hygiene push (commit 3489998a, 2026-05-28) reported `exit code 0` on TWO consecutive background pushes while the literal output was `fatal: Connection timed out ... EXIT 128` both times — `git status` showing `[ahead 1]` and `origin/main` still at the prior SHA is what caught it. The push only truly landed a session later when the HTTPS:443 block cleared. Mode (b) (PowerShell stderr-as-RemoteException) is documented in the PowerShell tool guidance and observed on native-exe progress output.

**Related pillar:** Pillar 1 (the literal git text + independent git state is the runtime truth; the wrapper's exit code is a report that can lie in either direction); Pillar 5 (the convenient "exit 0 means it pushed" vs the inconvenient "git timed out" — `git status -sb` is the one-command distinguishing experiment).

---

### 22. Bidirectional tracker drift — a stale tracker both overstates AND understates

**Symptom:** A project tracker has diverged from reality, and the divergence runs *both* directions: some tasks marked ✅ were never actually verified (overstatement), AND some genuinely-completed work shows no-marker or stub (understatement).

**Diagnostic signature:**
- A task shows ✅ but no runtime evidence (no PIE record, no commit demonstrating the feature *works* — only commits showing code authored).
- A different task shows stub/no-marker, but git + PIE record show it was genuinely closed.
- The tracker's ✅ semantics turn out to mean "code shipped," not "feature verified" — for some entries but not others.

**Root cause:** When a tracker and reality diverge, the divergence is not uniformly in one direction. ✅ may have been applied on code-authored (overstatement, the founding failure mode), while real verified work done *outside the tracker's lineage* (e.g. a separate rebuild thread) never got reflected back (understatement). Same root cause — tracker ≠ reality — two opposite surface symptoms.

**Fix pattern — reconcile in TWO passes, not one:**
1. **✅ → runtime:** for every claimed-✅ in the tracker, check it against runtime evidence (git commit + PIE record). Overstated if ✅ but no runtime proof. Downgrade or annotate.
2. **commits → tracker:** for every git commit / verified PIE bank, check it against the tracker's representation. Understated if proven-done but the tracker shows stub/no-marker. Upgrade.

A reconciliation that only does pass 1 (hunting false-✅) "fixes" the tracker into being wrong in the *other* direction — real progress still unrecorded.

**Prevention:**
- Never assume a stale tracker only overstates. Audit both directions by default.
- Preserve forensic honesty markers when reconciling: if an entry self-flags its own uncertainty (e.g. "✅ MECHANICALLY CLOSED" with ⏳ on the runtime gates), keep that nuance rather than flattening it to a plain ✅ or ✗ — it's evidence of where the author already knew the code-vs-verified line.
- Cite the runtime evidence (commit SHA + PIE observation) when upgrading/downgrading, so the corrected tracker is itself traceable.

**Source evidence:** v2.7 SSOT inventory (2026-05-29). OVERSTATED: AFL-0204 (✅ hit-confirm, but the component was orphaned until BM-0106 wired it — trap #19) and AFL-0211 (✅ lag-comp, but compensation was unverified until BM-0105c's RTT-flip). UNDERSTATED: AFL-0209 (no-marker, but BM-0104 closed it via live DA-swap, PIE-verified). The AFL-0304-Bi entry self-flags "✅ MECHANICALLY CLOSED" + ⏳ on its runtime gates — a forensic honesty marker to preserve. Net drift sized MEDIUM. See `feedback_bidirectional_tracker_drift.md` and `project_v2_7_inventory_pointer.md`.

**Related pillar:** Pillar 1 (✅ must mean operator-observed-runtime, not code-authored — the bidirectional drift is what happens when that discipline lapses); Pillar 5 (the convenient "the tracker just overstates" vs the inconvenient "it also understates" — the two-pass audit is the experiment that catches both).
