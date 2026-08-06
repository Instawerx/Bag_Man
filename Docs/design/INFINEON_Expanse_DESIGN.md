# INFINEON — L_Expanse 8v8 SIEGE (Build Plan + Design Brief SSOT)

**Config:** INFINEON = **8v8 SIEGE (Tier C)** on **stock `L_Expanse`** (washed). Experience =
`B_AFLExperience_Arena01_Extract4v4` — the **CODENAME that hosts INFINEON**; per the naming discipline do **NOT**
rename it (only the tile FText carries "INFINEON / 8V8 SIEGE"). Playlist DA `DA_AFL_Arena01_Extract4v4`. Companions:
`MAP_DISPLAY_NAME_REGISTRY.md` (display↔asset), `IRONICS_MAP_MODE_SPEC.md` §4 (roster), `WASH_INVENTORY.md` (props).

## Load-bearing constraints (recorded so they're not re-hit)
- **`L_Expanse` is World Partition** — actors are cell-distributed; the editor loads ~24 persistent actors, the rest
  (incl. the 6 UELogo props) stream from WP cells. Inspect/edit map actors only with the relevant cells loaded.
- **The experience trio is a `GameFeatureAction_AddComponents` ComponentList — NOT bridge-editable** (the bridge
  can't read/write ComponentList entries). The trio repoint is an **operator in-editor edit** (details panel),
  exactly like T1's 3v3 repoint was.

## The trio (stock → AFL), verified 2026-07-17
| Stock (4v4 now) | → AFL | Note |
|---|---|---|
| `B_TeamSetup_TwoTeams` | `B_AFL_TeamSetup_TwoTeams` | direct, 8v8-ok (teams {1,2}) |
| `B_TeamSpawningRules` | **`B_AFL_SpawnSelector_Arena3v3` (REUSE)** | **SIZE-AGNOSTIC** — its knobs are LOS/hot-point rejection + side-filter (start-tags); NO team-size property. Reused as-is for 8v8; the "3v3" in the name is misleading |
| `B_ShooterBotSpawner` | **`B_AFLBotFill_Arena8v8` (NEW)** | TeamSize 8 → Target = TeamSize×NumTeams = 16; see #1 |

## Build steps + dependencies
- **#1 — 8v8 siblings [bridge-doable, no map dependency] — DONE 2026-07-17:**
  - **`B_AFLBotFill_Arena8v8`** = structural duplicate of `B_AFLBotFill_Arena3v3`, **ONLY `TeamSize` 3→8**
    (Target = 8×2 = 16; bots = max(0, 16−Humans)). `NumBotsToCreate` left unchanged (**vestigial** — the AFL
    human-aware logic ignores it; it's the stock flat field the component replaces, `AFLBotFillComponent.h:17`).
  - **Spawn selector: REUSE `B_AFL_SpawnSelector_Arena3v3`** (size-agnostic) — **no 8v8 variant needed.**
- **#2 — map-wiring [operator in-editor, WP loading required]:** the prerequisite for the spawn repoint (#3).
  **PROVEN pattern (L_Arena_01):** 8 starts, **4/side** — `PStart_S_0..3` (Y=−3500)→`AFL.Spawn.Side.0`,
  `PStart_N_0..3` (Y=+3500)→`AFL.Spawn.Side.1` (spatial N/S split). The selector reads
  `Start->GetGameplayTags().HasTag(AFL.Spawn.Side.{0,1})` (`AFLPlayerSpawningManagerComponent.cpp:207`); an untagged
  side falls back to **ALL** starts (no side separation). Extraction actor = **`B_AFL_ExtractionZone`** (L_Arena_01's
  `ExtractZone_Central` + an `Extract_Zone_Trigger` TriggerBox).
  **L_Expanse plan:** it has **20 `LyraPlayerStart`** (14 loaded + 6 in WP cells) — **ENOUGH for 8v8 (need 16) → tag,
  don't add.** All are untagged today. Split is **X-based (E-W)** — starts span X[−4700..4700], Y narrow → **West
  (X<0)→Side.0, East (X>0)→Side.1** (mirror L_Expanse's E-W symmetry; confirm once all 20 are loaded). ~10/side. Place
  **2–3 `B_AFL_ExtractionZone`** at SIEGE spots (central + flanks). **WP procedure:** load the cells holding all 20
  starts + the extraction spots → tag/place → save (WP saves each as an external-actor package).
- **#3 — trio repoint [operator in-editor; depends on #1+#2]:** edit `B_AFLExperience_Arena01_Extract4v4`'s
  AddComponents: `B_TeamSetup_TwoTeams`→`B_AFL_TeamSetup_TwoTeams`, `B_TeamSpawningRules`→
  `B_AFL_SpawnSelector_Arena3v3`, `B_ShooterBotSpawner`→`B_AFLBotFill_Arena8v8`.
- **#4 — props [WP cells + swap]:** load the WP cells holding the 6 UELogo props → **replace with flat IRONICS logo
  planes** (`M_AFL_IRONICS_LogoFlat`, the proven front-end recipe; UELogo is the letterform mesh → **replace, not
  reskin**).
- **#5 — PIE-verify 8v8:** AFL teams {1,2}, team-correct (side-mirrored) spawns, bots fire (the bot-fire fix
  applies), extraction works, IRONICS props (no UE logos).

## Bots vs ranked
`B_AFLBotFill_Arena8v8` is for **offline/test fill** (16p is hard to human-fill in testing; bots now fire, useful).
Under the T2 matchmaker provider (ranked/online) bot-fill goes **inert** (`IsAuthoritative()` — Team SSOT §0.2/§3).
Build it for test; it self-disables on the ranked path.

## Status (2026-07-17)
- **#1 DONE** — `B_AFLBotFill_Arena8v8` (TeamSize 8); selector reused (size-agnostic).
- **#2 map-wiring: starts DONE (canary)** — 14/20 tagged 7/7 (West→Side.0, East→Side.1) via bridge, WP-save-verified
  (guard clean, 0 spurious `D`). The 6 cell-bound starts remain (optional polish → 10/side; 7/side sufficed for 8v8).
  Extraction (Phase C) still to place.
- **#3 trio repoint DONE (operator in-editor)** — `B_AFLExperience_Arena01_Extract4v4` AddComponents swapped to the
  AFL trio (TeamSetup/Selector/BotFill8v8).
- **#5 PIE-VERIFIED: 8v8 GREEN** — AFL teams {1,2}, side-separated spawns (tags→selector), bot-fill→8v8, **bots fire**;
  visual + logs. The gameplay wash is PROVEN. ⚠ Must PIE via the **playlist (HOST → INFINEON)**, NOT bare-map PIE /
  Play-From-Here (that loads no experience + spawns at the camera).
- **REMAINING:** extraction zones (Phase C), the 6 cell-bound starts (optional), the props (#4, WP cells), + COMMIT.
