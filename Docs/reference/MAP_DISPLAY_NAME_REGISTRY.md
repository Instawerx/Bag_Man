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
**VENUE CLASS is now a registry field (R97, `ssot/map-build-system.md` §2.1).** ARENA = purpose-built,
symmetric, one contained space. MAP = district-scale. It is a **queue dimension**, so a level filed under the
wrong class is offered in a queue it cannot serve — and the symptom is players getting the style they did not
choose. **Authored here, never inferred from a level name** (that is the same failure mode as NANOWATT/INFINEON
below; it has already happened once to this field, when the backend queue registry was first seeded by reading
class off `L_Duel_01` / `L_Arena_04` / `L_Expanse`).

| Display (TileTitle / TileSubTitle) | Playlist DA | ExperienceID | MapID (.umap) | Mode/size | **Venue class** | State |
|---|---|---|---|---|---|---|
| **NANOWATT** / 3V3 SPEED | `DA_AFL_Arena01_Extract3v3` | `B_AFLExperience_Arena01_Extract3v3` | **`/Game/Maps/L_Arena_01`** (OURS) | Arena 3v3 Extract | ARENA | ✅ **BUILT** (greybox PIE-proven, T1; brief `Docs/maps/Arena_01_DESIGN.md`) |
| **INFINEON** / 8V8 SIEGE | `DA_AFL_Arena01_Extract4v4` | `B_AFLExperience_Arena01_Extract4v4` | **`/ShooterMaps/Maps/L_Expanse`** (LYRA STOCK) | 8v8 (Tier C) | **MAP** | ⚠ **WASH IN PROGRESS** on a stock map — NOT a 4v4, NOT Arena_01 |
| **ARCANEON** / 5V5 HAYWIRE | `DA_AFL_Arena04_5v5_Haywire` | `B_AFLExperience_Arena04_5v5_Haywire` | **`/Game/Maps/L_Arena_04`** (OURS) | 5v5 (Tier C), 10 seats | ARENA | ✅ **LIVE** — playlist + experience built, PIE-watched |
| **ARCANEON** / 5V5 PROMOD | `DA_AFL_Arena04_5v5_ProMod` | `B_AFLExperience_Arena04_5v5_ProMod` | **`/Game/Maps/L_Arena_04`** (OURS) | 5v5 (Tier C), 10 seats | ARENA | ✅ **LIVE** — playlist + experience built, PIE-watched |
| **ARCANEON** / 8V8 HAYWIRE | `DA_AFL_Arena04_8v8_Haywire` | `B_AFLExperience_Arena04_8v8_Haywire` | **`/Game/Maps/L_Arena_04`** (OURS) | 8v8 (Tier C), 16 seats | ARENA | ✅ **LIVE** — playlist + experience built, PIE-watched |
| **ARCANEON** / 8V8 PROMOD | `DA_AFL_Arena04_8v8_ProMod` | `B_AFLExperience_Arena04_8v8_ProMod` | **`/Game/Maps/L_Arena_04`** (OURS) | 8v8 (Tier C), 16 seats | ARENA | ✅ **LIVE** — playlist + experience built, PIE-watched |

### ARCANEON — LIVE (`bShowInFrontEnd = True`, 2026-08-03)
All four configs PIE-watched before the flag was flipped. `L_Arena_04` is committed (62f7fe63) and the whole
chain — 4 playlists → map → 4 experiences → `LAS_AFL_Teams_5v5/8v8` → bot-fills → `B_AFL_BotController` — is
tracked, so a clean clone resolves.
**`max_player_count` is EXACT SEATS** (TeamSize × 2) — 6 / 10 / 16, never headroom.

**The gore-free split is STRUCTURAL and experience-driven — proven on the same map, at both sizes:**

| | `NoDismember` | `AFL_DISMEMBER` | `GIB` |
|---|---|---|---|
| ProMod 5v5 / 8v8  | `=1` | 0 | 0 |
| Haywire 5v5 / 8v8 | `=0` | 45 / 61 | 87 / 96 |

ProMod drops `AFLDismember` from `GameFeaturesToEnable` **and** carries an empty ComponentList, so
`AFLDismemberLegPenaltyComponent` is never attached. There is no runtime switch to get wrong.

⚠ **THE AI-3 MOVEMENT KIT IS PROMOD-ONLY.** Sprint, Slide, Vault and Roll are granted on `HeroData_BagMan_Pro`
alone; Haywire runs `HeroData_BagMan` — Dash, Climb and Grab only. So the human-vs-bot movement parity AI-3 was
built for exists in ProMod and **does not exist in Haywire**. Haywire logging zero Sprint/Slide/Roll is CORRECT,
not a defect — and any probe asserting those against a Haywire config is asserting the wrong thing.

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
- **2 BUILT net-new AFL maps:** **Arena_01 = NANOWATT** (roster #3, Tier B, Arena 3v3 Extract) and
  **Arena_04 = ARCANEON** (Tier C, 5v5 + 8v8 × Haywire + ProMod — 4 configs, all PIE-watched, briefs on disk).
- **1 WASH-in-progress (stock map reskinned):** **INFINEON 8V8 SIEGE** on stock `L_Expanse` (a Tier-C 8v8 prototype
  on a Lyra map — not a net-new roster map).
- **8 roster maps DESIGN-ONLY:** Duel_01, Duel_02, Arena_02 (brief on disk, unapproved), Arena_03
  (netcode-blocked), Arena_05, BR_18/BR_36 (now have a WP base map — see ShantyTown below), Shrink_Yard.
- **ShantyTown = `L_ShantyTown` (fills BR_18 / BR_36):** vendor `Demo_Map` forked to `/Game/Maps/L_ShantyTown`
  and **World-Partition-converted (commit `c6db6c4d`)**; **District Program OPERATOR-APPROVED 2026-08-05**
  (`Docs/design/ShantyTown_BR_DESIGN.md` §11 + Gate — one map covers the whole matrix via `District_Duel/Arena/Team`
  data layers). **No geometry authored yet; no front-end configs** (no playlist/experience). BR mode layer gated
  on the shrinking Zone system. District PLACEMENT within the town core still OWED.
- **Arena_02 (Tier B, 3v3 + 4v4) — BRIEF BANKED, BUILD NOT APPROVED:** `Docs/design/Arena_02_DESIGN.md`
  committed as-is (`a241cc49`). **Gate = DRAFT awaiting operator approval (2026-08-02) — banking the brief
  did NOT approve the build.** On disk: **no `.umap`**, **no front-end config** (no playlist/experience), and
  its signature mechanic **MOVING LASER WALLS is UNBUILT** (disk-verified: no `LaserWall`/`FLaserWallState`/
  `WallSweep` C++, no assets). Operator ruling 2026-08-05: **kept as a future marquee Tier-B venue, not
  archived** — its signature mechanic is distinct from the ShantyTown **D2 Arena** district, which covers the
  same 3v3/4v4 bracket without one. Brief specifies a single level, explicitly **NOT** World Partition.
- **The picker now shows 6 tiles**, all AFL, no stock leakage: NANOWATT 3V3 SPEED · INFINEON 8V8 SIEGE ·
  ARCANEON ×4.
- **Bracket coverage:** Arena = 1 venue (NANOWATT). Team = 2 (ARCANEON + INFINEON). **Duel = NOTHING** — 1v1 and
  2v2 have no map at all, which is the largest hole in the matrix.

## INFINEON remaining wash-work (for its config to be real 8v8) — CODENAMES STAY
1. The experience `B_AFLExperience_Arena01_Extract4v4` team-size config (bot-fill Target + team setup) must be 8v8
   (16p; `MaxPlayerCount` already sourced from `L_Expanse`'s stock `DA_Expanse_TDM`) — verify/repoint the CONFIG
   only, like the T1 3v3 scoping. **Do NOT rename the DA/experience** (host-resolution saga).
2. `L_Expanse` is stock ShooterMaps — the "wash" (IRONICS retheme) + AFL spawns/extraction/round wiring on it.
3. Placeholder metadata: `TileDescription` still the stock ShooterCore string (external-facing, safe to update).
