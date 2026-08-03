# IRONICS — Map Display-Name ↔ Disk-Asset Registry (SSOT)

**Purpose:** the ONE place the front-end display name of a "HOST A GAME" config is reconciled to its disk assets.
Read this; do **not** re-derive (that's how NANOWATT/L_Arena_01 and INFINEON/L_Expanse got jumbled — the same
class as the PP9-vs-PulseCarbine label bug). Any lane naming a map/config cites this table.

## Where the display names live (single source)
The "HOST A GAME" tiles read **`ULyraUserFacingExperienceDefinition`** playlist assets (`Content/BagMan/Playlists/DA_AFL_*`).
The card name = that DA's **`TileTitle`**; the subtitle = **`TileSubTitle`**. There is NO separate name asset/DataTable —
`INFINEON`/`NANOWATT`/`8V8 SIEGE`/`3V3 SPEED` exist ONLY as `TileTitle`/`TileSubTitle` FText on the playlist DA.
So: to rename a config, edit its playlist DA; to know what a display name *is*, read the DA's `MapID`+`ExperienceID`.

## The table (verified via bridge 2026-07-17)
| Display (TileTitle / TileSubTitle) | Playlist DA | ExperienceID | MapID (.umap) | Mode/size | State |
|---|---|---|---|---|---|
| **NANOWATT** / 3V3 SPEED | `DA_AFL_Arena01_Extract3v3` | `B_AFLExperience_Arena01_Extract3v3` | **`/Game/Maps/L_Arena_01`** (OURS) | Arena 3v3 Extract | ✅ **BUILT** (greybox PIE-proven, T1; brief `Docs/maps/Arena_01_DESIGN.md`) |
| **INFINEON** / 8V8 SIEGE | `DA_AFL_Arena01_Extract4v4` | `B_AFLExperience_Arena01_Extract4v4` | **`/ShooterMaps/Maps/L_Expanse`** (LYRA STOCK) | 8v8 (Tier C) | ⚠ **WASH IN PROGRESS** on a stock map — NOT a 4v4, NOT Arena_01 |
| **ARCANEON** / 5V5 HAYWIRE | `DA_AFL_Arena04_5v5_Haywire` | `B_AFLExperience_Arena04_5v5_Haywire` | **`/Game/Maps/L_Arena_04`** (OURS) | 5v5 (Tier C), 10 seats | 🚧 playlist BUILT + **HIDDEN**; experience NOT authored |
| **ARCANEON** / 5V5 PROMOD | `DA_AFL_Arena04_5v5_ProMod` | `B_AFLExperience_Arena04_5v5_ProMod` | **`/Game/Maps/L_Arena_04`** (OURS) | 5v5 (Tier C), 10 seats | 🚧 playlist BUILT + **HIDDEN**; experience NOT authored |
| **ARCANEON** / 8V8 HAYWIRE | `DA_AFL_Arena04_8v8_Haywire` | `B_AFLExperience_Arena04_8v8_Haywire` | **`/Game/Maps/L_Arena_04`** (OURS) | 8v8 (Tier C), 16 seats | 🚧 playlist BUILT + **HIDDEN**; experience NOT authored |
| **ARCANEON** / 8V8 PROMOD | `DA_AFL_Arena04_8v8_ProMod` | `B_AFLExperience_Arena04_8v8_ProMod` | **`/Game/Maps/L_Arena_04`** (OURS) | 8v8 (Tier C), 16 seats | 🚧 playlist BUILT + **HIDDEN**; experience NOT authored |

### ARCANEON — why the four tiles are HIDDEN (`bShowInFrontEnd = False`)
Their `ExperienceID`s name experiences that **do not exist yet** — the per-map experience carries a
`GameFeatureAction_AddComponents` ComponentList (the AFL trio) which the bridge **cannot write**, so that step is
AIK-in-editor or the `Arena_04_WIRING_AIK.md` §B manual fallback. A visible tile whose experience does not resolve
is a broken HOST entry for every player, so they ship hidden and the flag flips when the experiences land.
**`max_player_count` is EXACT SEATS** (TeamSize × 2) — 6 / 10 / 16, never headroom.
⚠ **`/Game/Maps/L_Arena_04` was UNTRACKED when these were authored** — four committed playlists pointing at an
uncommitted `.umap` breaks every clean clone. Confirm with the map lane.

## The internal-codename ↔ external-tile split (BY DESIGN — do NOT "fix" by renaming)
The DA file / experience / MapID names are **internal codenames**; `TileTitle`/`TileSubTitle` are the **external
player-facing names**. They deliberately differ: `DA_AFL_Arena01_Extract4v4` (codename) hosts **INFINEON / 8V8 SIEGE
on stock `L_Expanse`** (external + real map). Per the roster-naming discipline (memory
`project_ironics_map_roster_naming_system`), **renaming the internal DA / experience / MapID is FORBIDDEN** — it
re-breaks the host resolution that cost a ~16-attempt saga; only the tile FText carries the external name. So the
codename "Extract4v4" is **not a bug to fix** — it's a deliberate holdover, and there is genuinely **no
4v4-on-Arena_01 config** (that DA is the INFINEON 8v8 wash). **The `MapID` + `TileTitle` are the truth — read THEM
(or this registry), NEVER the DA filename.** The earlier "Arena_01 has 3v3+4v4" jumble came from re-deriving off
the filename instead of the MapID. (Both DAs still carry the stock `TileDescription="Small test level for
ShooterCore"` — placeholder, unwashed.)

## Clean build-state (roster of 10, per IRONICS_MAP_MODE_SPEC §4)
- **1 BUILT net-new AFL map:** **Arena_01 = NANOWATT** (roster #3, Tier B, Arena 3v3 Extract; brief exists).
- **1 WASH-in-progress (stock map reskinned):** **INFINEON 8V8 SIEGE** on stock `L_Expanse` (a Tier-C 8v8 prototype
  on a Lyra map — not a net-new roster map).
- **9 roster maps DESIGN-ONLY** (no map/experience/brief): Duel_01, Duel_02, Arena_02, Arena_03, Arena_04,
  Arena_05, BR_18, BR_36, Shrink_Yard.
- **No phantom second AFL map.** Exactly one built AFL map + one stock-map wash.

## INFINEON remaining wash-work (for its config to be real 8v8) — CODENAMES STAY
1. The experience `B_AFLExperience_Arena01_Extract4v4` team-size config (bot-fill Target + team setup) must be 8v8
   (16p; `MaxPlayerCount` already sourced from `L_Expanse`'s stock `DA_Expanse_TDM`) — verify/repoint the CONFIG
   only, like the T1 3v3 scoping. **Do NOT rename the DA/experience** (host-resolution saga).
2. `L_Expanse` is stock ShooterMaps — the "wash" (IRONICS retheme) + AFL spawns/extraction/round wiring on it.
3. Placeholder metadata: `TileDescription` still the stock ShooterCore string (external-facing, safe to update).
