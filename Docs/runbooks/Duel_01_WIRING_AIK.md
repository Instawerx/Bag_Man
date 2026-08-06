# Duel_01 — Experience/Playlist Wiring + Caveat Handoff (AIK task + operator fallback)

**Goal:** make **`/Game/Maps/L_Duel_01`** HOST-selectable and playable at **1v1 and 2v2**, **ProMod-only**, mirroring the NANOWATT/ARCANEON wiring pattern — and resolve the two greybox caveats AIK is better placed to fix in-editor than the exec bridge was.

> **⚠ ProMod-ONLY (RULED, Block 164).** Duel_01 is a **stated exception** to the dual-GE rule ([[project-ironics-dual-ge-requirement]]): its core micro is **Vault**, granted only on `HeroData_BagMan_Pro`. In Haywire the vault cover is inert and the map's reset mechanic doesn't exist. **Do NOT create Haywire configs.** 2 configs total.

> **Preferred path:** paste **§A** into UE **Tools → Agent Chat** (Claude Code agent, *AFL Blueprint & Gameplay* profile). AIK can edit the `GameFeatureAction_AddComponents` **ComponentList** (the team/spawn/bot trio) and the map **World Settings** — neither of which the exec bridge could write. If AIK can't, use **§B**.

---

## What is already built on L_Duel_01 (committed, e9f79f0d)

- **Geometry:** `SM_BagMan_Duel01_Greybox` (arena — floor + walls + central extract dais + vault-cover ring + wing cover + switchback catwalks + crow's-nest + bridges) and `SM_BagMan_Duel01_Dome` (separate/toggleable), in `/Game/Maps/Duel01/`. Vertical domed duel, ~40 m plan × ~24 m tall.
- **Spawns:** 4 `LyraPlayerStart` — 2 N (`AFL.Spawn.Side.0`) + 2 S (`AFL.Spawn.Side.1`), verified.
- **Extraction:** 1 central `B_AFL_ExtractionZone`.
- **Nav:** RecastNavMesh (convex-decomp collision on the arena mesh) + NavLinkProxies; spawns→extract (both sides) and spawns→nest all non-partial.
- **No weapon/ability POI pads** (duel design — pads are a later economy decision; **nothing to wire for the weapon-intent registry here**, unlike ARCANEON).

So this task is only: (1) the **experience + playlist** layer, (2) the **World-Settings default experience**, (3) the optional **ramp-nav** refinement.

---

## Reference pattern to mirror (verified 2026-08-03)

**A config = one playlist DA + one per-map experience.** SSOT: `Docs/reference/MAP_DISPLAY_NAME_REGISTRY.md`.

| Ref config | Playlist DA (`/Game/BagMan/Playlists/`) | MapID | ExperienceID | max_players |
|---|---|---|---|---|
| NANOWATT (3v3, ProMod-profile) | `DA_AFL_Arena01_Extract3v3` | `/Game/Maps/L_Arena_01` | `B_AFLExperience_Arena01_Extract3v3` | 6 |

- **Closest small-size ProMod reference = `B_AFLExperience_Arena01_Extract3v3`** (its action-set/GF profile matches `B_Experience_ProMod`). Mirror it for the ProMod profile.
- **Playlist DA fields** (`ULyraUserFacingExperienceDefinition`): `map_id`, `experience_id`, `tile_title`, `tile_sub_title`, `tile_description`, `tile_icon`, `loading_screen_widget`, `max_player_count`.
- **The AFL trio** (per-map experience `AddComponents` ComponentList — NOT bridge-editable):
  - `B_AFL_TeamSetup_TwoTeams` (size-agnostic, teams {1,2})
  - `B_AFL_SpawnSelector_Dynamic` (size-agnostic — reuse as-is; reads the `AFL.Spawn.Side.*` tags already on the starts)
  - **BotFill:** duplicate the existing bot-fill and set `TeamSize` only — **1v1 → `B_AFLBotFill_Duel_1v1` (TeamSize 1 → target 2)**, **2v2 → `B_AFLBotFill_Duel_2v2` (TeamSize 2 → target 4)**.

---

## ⚠ Operator input REQUIRED before running

1. **Display name** — Duel_01 is a roster code-name and needs a front-end name (like NANOWATT / INFINEON / ARCANEON). Provide `tile_title` (e.g. `«DUEL01_NAME»`) + `tile_sub_title` (`1V1` / `2V2`). Placeholder used below until ruled.
2. **Tile icon** — a `T_IRONICS_MapTile_*` texture, or reuse the NANOWATT tile as placeholder.

---

## §A — AIK PROMPT (paste into Agent Chat; Claude Code agent, AFL Blueprint & Gameplay profile)

```
GOAL: Wire /Game/Maps/L_Duel_01 to be HOST-selectable at 1v1 and 2v2, PROMOD-ONLY
(2 configs, NO Haywire), mirroring NANOWATT (DA_AFL_Arena01_Extract3v3 +
B_AFLExperience_Arena01_Extract3v3), then resolve two caveats. Do NOT rename any
existing DA/experience/MapID (host-resolution is naming-fragile).

STEP 0 — INSPECT FIRST, show me your plan before creating:
- Open B_Experience_ProMod and B_AFLExperience_Arena01_Extract3v3 (/AFLBagMan/Experiences)
  and DA_AFL_Arena01_Extract3v3 (/Game/BagMan/Playlists). Report how the ProMod profile
  (action sets / GameFeaturesToEnable / AddComponents trio) and team size are composed.
  Confirm HeroData_BagMan_Pro is the ProMod hero (it grants Vault; Duel_01 depends on it).

STEP 1 — Bot-fills (data-only): duplicate the existing arena bot-fill twice ->
  B_AFLBotFill_Duel_1v1 (set TeamSize = 1, target 2) and
  B_AFLBotFill_Duel_2v2 (set TeamSize = 2, target 4). Change ONLY TeamSize.
  Reuse B_AFL_SpawnSelector_Dynamic and B_AFL_TeamSetup_TwoTeams unchanged.

STEP 2 — Per-map experiences (2), mirror B_AFLExperience_Arena01_Extract3v3's ProMod
  profile, in /AFLBagMan/Experiences/ :
    B_AFLExperience_Duel01_1v1_ProMod, B_AFLExperience_Duel01_2v2_ProMod
  AddComponents trio each:
    TeamSetup  = B_AFL_TeamSetup_TwoTeams
    SpawnRules = B_AFL_SpawnSelector_Dynamic
    BotFill    = B_AFLBotFill_Duel_1v1 (1v1) / B_AFLBotFill_Duel_2v2 (2v2)
  (Duel_01 has no weapon pads, so AFLGFA_WeaponSpawns is not required — but keeping the
   ProMod profile intact is fine.)

STEP 3 — Playlist DAs (2), duplicate DA_AFL_Arena01_Extract3v3 into /Game/BagMan/Playlists/:
    DA_AFL_Duel01_1v1_ProMod, DA_AFL_Duel01_2v2_ProMod
  Set each:
    map_id           = Map:/Game/Maps/L_Duel_01
    experience_id    = the matching B_AFLExperience_Duel01_* from STEP 2
    max_player_count = 2 (1v1) / 4 (2v2)
    tile_title       = «DUEL01_NAME»          (ASK OPERATOR)
    tile_sub_title   = "1V1" / "2V2"
    tile_description, tile_icon, loading_screen_widget = copy from the NANOWATT playlist

STEP 4 — CAVEAT A (map default experience, bridge could not set it):
  Open L_Duel_01 World Settings and set DefaultGameplayExperience = B_Experience_ProMod
  (so bare-map PIE and Play-From-Here run ProMod, which grants Vault).

STEP 5 — CAVEAT B (ramp nav, optional but preferred):
  Bots currently reach the crow's-nest via NavLinkProxies, not by walking the switchback
  ramps -- the imported greybox mesh's complex collision isn't gathered by Recast, so nav
  runs on convex-decomposition collision + links. If reasonable, author simple/box or
  proper walkable collision on the switchback ramp + landing surfaces of
  SM_BagMan_Duel01_Greybox so Recast generates continuous navmesh up the ramps, then
  remove the redundant nest-access NavLinkProxies. If not clean, LEAVE the links -- the
  §6 goalvalid playtest metric will confirm reachability either way.

STEP 6 — VERIFY (report back):
  - Both playlists' map_id/experience_id resolve (no null PrimaryAssetIds).
  - Both experiences compile; trio set correctly per size.
  - Register the 2 playlists in the front-end HOST list if that is a manual step here.
  - World Settings default experience = ProMod.
  Do NOT run PIE yourself; I (operator) PIE-verify via HOST -> tile, NOT bare-map PIE.
```

---

## §B — Operator manual fallback (if AIK can't)

1. **Bot-fills:** duplicate the arena bot-fill → `B_AFLBotFill_Duel_1v1` (TeamSize 1) and `B_AFLBotFill_Duel_2v2` (TeamSize 2).
2. **Experiences (×2):** duplicate `B_AFLExperience_Arena01_Extract3v3` → `B_AFLExperience_Duel01_1v1_ProMod` / `_2v2_ProMod`; set the `AddComponents` trio (TeamSetup / `B_AFL_SpawnSelector_Dynamic` / size-matched BotFill).
3. **Playlists (×2):** duplicate `DA_AFL_Arena01_Extract3v3` → `DA_AFL_Duel01_1v1_ProMod` / `_2v2_ProMod`; set `map_id=/Game/Maps/L_Duel_01`, `experience_id`=matching, `max_player_count`=2/4, tile FText.
4. **World Settings:** L_Duel_01 → `DefaultGameplayExperience = B_Experience_ProMod`.
5. **Register** the 2 playlists in the front-end HOST list.
6. **PIE-verify via HOST → tile** (NOT bare-map PIE). Check: ProMod hero (Vault works — vault the center cover), AFL teams {1,2}, side-separated N/S spawns, bots fill + fire, central extraction works. **1v1 and 2v2.**
7. Update `MAP_DISPLAY_NAME_REGISTRY.md` with the 2 new Duel_01 rows.

---

## Notes / guardrails
- **Naming:** new assets named descriptively is fine; **never rename** existing DA/experience/MapID.
- **ProMod-only is load-bearing, not stylistic** — Vault is the map's core mechanic and is ProMod-exclusive. A Haywire config would ship a broken map. Do not add one.
- **Spawns/extraction/nav on L_Duel_01 are done** — this task is the experience+playlist layer + the 2 caveats.
- Brief: `Docs/design/Duel_01_DESIGN.md` (§2–§5 still describe the pre-redesign flat duel; the Build Record documents the as-built vertical version — reconcile at the next brief pass, not AIK's job).
