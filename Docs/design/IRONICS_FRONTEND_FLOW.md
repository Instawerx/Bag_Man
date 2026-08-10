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
  `MainScreenClass` = **`W_IRONICS_Home`** (the R98 door split) — ⚠ **repointed 2026-08-09**, was
  `W_IRONICS_FrontEnd`. Both already IRONICS-logo'd (fork; see `WASH_INVENTORY.md`).
- **Root nav** (`UAFLW_HomeScreen::GetNavRoutes`, verified in `-game`): two **doors** — LEAGUE PLAY →
  `WBP_IRONICS_Lobby_League`, STAKED PLAY → `WBP_IRONICS_Lobby_Staked` (disabled until S4 TicketReview exists) —
  over a five-item **footer**: LOADOUT → `WBP_AFL_Loadout` · **STORE** → `AFLW_Menu_CosmeticShop` · VENUES → *(no
  asset; item disabled)* · CAREER → *(no asset; item disabled)* · SETTINGS → `W_LyraSettingScreen`.
- ⚠ **HOST IS DEPRECATED and REPLAYS IS DEFERRED (operator ruling, 2026-08-10).** The old button set is gone with
  the old root. Match allocation now runs door → queue → allocator, so a client-side arena picker could originate
  a session the allocator never authorised: `W_ExperienceSelectionScreen` moved to `/Game/DeveloperUtils/Host/`
  (never cooked) and is summoned by `afl.Debug.SummonHostMenu` in non-shipping builds only. `W_ReplayBrowserScreen`
  is untouched at `/Game/UI/Menu/Replays/` and was denied a sixth footer slot — it becomes a **sub-tab inside
  Career**, so it is unreachable until that surface lands. The two dead roots (`W_IRONICS_FrontEnd`,
  `W_LyraFrontEnd`) moved to `/Game/DeveloperUtils/Host/` with the screen, because a hard reference from a cooked
  package outranks any cook filter.
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
   > **SUPERSEDED 2026-08-10 — HOST is deprecated.** Path A still doesn't touch ServerTravel, but the *player-facing*
   > entry to it is gone: the route is door → queue → allocator, and HOST survives only as `afl.Debug.SummonHostMenu`
   > in non-shipping builds. Read this step as historical.
6. **RETURN — UNCHANGED (just the knob):** `ReturnToMainMenu → GameDefaultMap` (now the armory). No code change.

### First cut = the GameDefaultMap repoint + registration, PIE-verified full-loop
boot → armory + menu → HOST → match → **return → armory + menu**. Then the cosmetic pawn cleanup + (later) the
LOADOUT button. `L_LyraFrontEnd` becomes the unused stock front-end (kept on disk).

> **✅ STEPS 1 AND 2 ARE APPLIED (verified on disk 2026-08-08).** `DefaultEngine.ini:71` already reads
> `GameDefaultMap=/Game/BagMan/Armory/L_IRONICS_Armory`, and the armory is registered in **both** places
> step 2 requires — the Map `SpecificAssets` list (`DefaultGame.ini:66`) and `MapsToCook`
> (`DefaultGame.ini:205`). Steps 3, 5 and 6 were no-change by design. **Still open from this cut:** step 4,
> the three stray `Character_Default` mannequins (cosmetic).

### ✅ RESOLVED — the R98 split is BUILT (2026-08-06 → 2026-08-10)

**This section said the split existed "as no widget at all". That is no longer true and the whole passage is
superseded.** What shipped, in order: the `UAFLW_Lobby_Root` chassis and both door WBPs, the `/queues` +
`/population` join, the wallet bind, the S2 detail panel, the `/band` and presence endpoints, `W_IRONICS_Home`
itself, the `MainScreenClass` repoint, and the footer nav. All of it is exercised headlessly by
`afl.Home.Door`, `afl.Home.Nav` and the `AFL.Home` / `AFL.Lobby` / `AFL.Payout` suites.

Both §9 blockers named here were closed by ruling, not by working around them: the **type ramp is approved**
(`IRONICS_UI_STYLE_SSOT.md` §4, with the `UFont` dropped and the token compiler re-run) and the **§2
colour-coding decision is R100** — the doors are *not* colour-coded, they differ by density, motion rate and
content.

**Still unbuilt behind the split**, and each blocks something specific rather than the structure: **S4
TicketReview** (R22, unskippable — the only reason the staked door is disabled), the **Venues** surface (S8
VenueShowcase) and the **Career** surface, which also owns REPLAYS as a sub-tab.
