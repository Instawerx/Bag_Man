# IRONICS — Front-End / Return Flow (SSOT)

**Purpose:** the ONE place the front-end boot + return routing is recorded, and the home for the **Armory-as-hub**
design. Routing edits burned matchmaking-v1 (scattered flow-jumble) — so this maps the *real* wiring and names the
**single convergence point** any hub change must target. Read this before touching any front-end routing.

Verified on disk 2026-07-17 (config + source + asset registry).

---

## THE CONVERGENCE (the whole point — one funnel, not scattered paths)

Boot **and** all three return paths converge on **one map knob** via **one function**:

| Path | Mechanism | Lands on |
|---|---|---|
| **App launch** | `GameDefaultMap` (`Config/DefaultEngine.ini:71`) | GameDefaultMap |
| **Match exit / post-match** | Surface-#4 CONTINUE → `GI->ReturnToMainMenu()` (`AFLW_MatchScoreboard.cpp:203`) → engine `Super` | **GameDefaultMap** |
| **Disconnect / session-destroy** | `UCommonGameInstance::ReturnToMainMenu()` (`CommonGameInstance.cpp:147`) | **GameDefaultMap** |
| **Error / reset-and-rejoin** | `ReturnToMainMenu()` (`CommonGameInstance.cpp:193`) | **GameDefaultMap** |
| **In-game PAUSE menu → Return** ⚠ | `W_LyraGameMenu` `ReturnButton` → `Open Level (by Object Reference)` fed by its **`FrontEndMapReference`** var (`/Game/UI/Hud/`) — its OWN soft-map, **NOT** GameDefaultMap | **`FrontEndMapReference`** |

⚠ **CORRECTION (runtime-exposed): there are TWO return knobs, not one.** Post-match / disconnect / error go through
`ReturnToMainMenu()` → **GameDefaultMap** (✅ → L_IRONICS_Armory via Path A). But the **in-game pause menu** takes a
SECOND path: `W_LyraGameMenu`'s `ReturnButton` runs `OpenLevel(FrontEndMapReference)` — a widget-level soft-map
**variable** (default was `L_LyraFrontEnd`) that **bypasses GameDefaultMap**. The earlier "single convergence" read
was incomplete; the return-→-`L_LyraFrontEnd` symptom exposed it. **Both knobs must point at the armory:**
1. `GameDefaultMap` (`DefaultEngine.ini:71`) → `L_IRONICS_Armory` ✅ (Path A).
2. `W_LyraGameMenu.FrontEndMapReference` → `L_IRONICS_Armory` (this fix). *(Its `FrontEndExperienceReference` =
   `B_LyraFrontEnd_Experience` — already correct: the armory runs the SAME experience → MAP-only change, no reroute.)*
These are the ONLY two flow refs to Lyra's front-end map. Registration/test/editor entries (`SpecificAssets`,
`MapsToCook`, `MapsToPIETest`, `CommonEditorMaps`) don't drive the flow — optional cleanup.

---

## THE HUB RULE (operator law)

**Every front-end AND back-end return routes to the ARMORY/LOADOUT — for every map.** The Armory is home base:
matches **launch from** it and **return to** it; loadouts + lobby are managed there. NOT the stock Lyra
map-preview menu. (Already started: the load screen is Armory-themed.)

---

## THE ASSETS (distinct — do not conflate)

- **`L_LyraFrontEnd`** (`/Game/System/FrontEnd/Maps/`, World) = the **wired boot hub**. Runs `B_LyraFrontEnd_Experience`
  (bare menu: no pawn, no GF). A lit 3D studio: emblem spinner + camera + **`B_LoadRandomLobbyBackground`** actor that
  streams a backdrop level.
- **`L_IRONICS_Armory`** (`/Game/BagMan/Armory/`, World) = a **separate, proven, UNWIRED** 3D armory scene (bf19b3bb):
  `B_IRONICS_Armory_Experience` + `AAFLLoadoutDisplayPawn` (grounded robot) + `ArmoryHeroCam`. Not in the flow today.
- **Backdrop streaming:** `B_LoadRandomLobbyBackground` (in L_LyraFrontEnd) → the single DA
  **`ShooterGameLobbyBG`** (`/ShooterMaps/Items/Backgrounds/`, a `ULyraLobbyBackground`) → **`BackgroundLevel`**
  (`TSoftObjectPtr<UWorld>`, `LyraLobbyBackground.h:23`) = **`L_ShooterFrontendBackground`** today.
- **Default screen:** `B_LyraFrontendStateComponent` (`/Game/UI/`) → `PressStartScreenClass` = `W_IRONICS_Startup`,
  `MainScreenClass` = **`W_IRONICS_FrontEnd`** (the button menu). Both already IRONICS-logo'd (fork; see
  `WASH_INVENTORY.md`).
- **Hub buttons** (W_IRONICS_FrontEnd refs, verified): **HOST** → `W_ExperienceSelectionScreen` (arena picker →
  UserFacing playlist → ServerTravel) · **STORE** → `AFLW_Menu_CosmeticShop` (wired) · SETTINGS → `W_LyraSettingScreen`
  · REPLAYS → `W_ReplayBrowserScreen`. **LOADOUT = NOT referenced** (still placeholder / cheat-only `afl.Loadout.Open`).
- **Loadout locker** (`UAFLW_LoadoutBase`/`AAFLLoadoutPod`, Inc 1-3 proven) + **Store** (`UAFLW_FrontEndMarket`) =
  full-screen **UI overlays** (push `UI.Layer.Menu`) — work over ANY map.

---

## THE CHOSEN PATH — A (GameDefaultMap → L_IRONICS_Armory)  ⟵ REVISED from B, 2026-07-17

> **Path B was tried and REVERTED (regression, log-proven).** Cut 1 (stream L_IRONICS_Armory as the lobby backdrop
> via `ShooterGameLobbyBG.BackgroundLevel`) made the front-end load **`B_LyraDefaultExperience`** (the no-menu
> fallback) instead of **`B_LyraFrontEnd_Experience`** → **dead menu** (before Cut 1: backup log 2601 = FrontEnd
> experience + menu; after: log 4535 = Default experience + no menu). ROOT: **`L_IRONICS_Armory` declares
> `WorldSettings.DefaultGameplayExperience = B_LyraFrontEnd_Experience` — it's a FULL front-end MAP, not a cosmetic
> backdrop.** Streaming a level that declares its own experience broke the host's experience pick; the bare
> `L_ShooterFrontendBackground` declares None, so it never disrupted. **Backdrop-streaming a front-end map is the
> wrong mechanism.** Cut 1 was reverted (DA restored to L_ShooterFrontendBackground; menu-fix confirmed).

**Path A is CLEAN — not the porting-risk the stale memory implied.** The old rejection assumed the armory ran a
bare/trimmed `B_IRONICS_Armory_Experience` — DISK shows it runs the **full `B_LyraFrontEnd_Experience`**. So booting
*into* L_IRONICS_Armory = the full front-end (menu) **+** the armory scene together (exactly as it works opened
directly). The return convergence (`ReturnToMainMenu → GameDefaultMap`) then lands **every** return in the armory —
the every-map hub rule, satisfied at the one knob, menu intact.

### Path A — the changes
1. **CORE — GameDefaultMap repoint** (`DefaultEngine.ini:71`): `L_LyraFrontEnd` → `L_IRONICS_Armory`. Boot + all
   returns land in the armory; its WS runs `B_LyraFrontEnd_Experience` → menu + armory.
2. **REGISTRATION (the armory lacks it — needed for a cooked build):** L_IRONICS_Armory is at `/Game/BagMan/Armory/`,
   NOT in `/Game/Maps` and NOT listed in `DefaultGame.ini`. Add it to the Map `SpecificAssets` (`DefaultGame.ini:66`)
   **and** `MapsToCook` (line 204). Without these it may run in PIE but fail to cook/ship.
3. **HERO-CAM — now CORRECT (no change):** as the actual boot map, `ArmoryHeroCam` (AutoActivateForPlayer0) is the
   map's OWN camera with no competitor — the Cut-1 conflict came from streaming into L_LyraFrontEnd's camera. Frames
   right (as standalone). PIE-verify.
4. **3 EXTRA `Character_Default` PAWNS (cosmetic cleanup):** the armory has 3 stray default mannequins besides the
   hero robot — hide/remove so only the hero shows. Follow-on, not a blocker.
5. **HOST / ServerTravel — UNCHANGED:** HOST → `W_ExperienceSelectionScreen` → ServerTravel is forward travel FROM
   the hub; Path A doesn't touch it.
6. **RETURN — UNCHANGED (just the knob):** `ReturnToMainMenu → GameDefaultMap` (now the armory). No code change.

### First cut = the GameDefaultMap repoint + registration, PIE-verified full-loop
boot → armory + menu → HOST → match → **return → armory + menu**. Then the cosmetic pawn cleanup + (later) the
LOADOUT button. `L_LyraFrontEnd` becomes the unused stock front-end (kept on disk).
