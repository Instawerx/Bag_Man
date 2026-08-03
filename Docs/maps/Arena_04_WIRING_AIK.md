# Arena_04 — Experience/Playlist Wiring (AIK task + operator fallback)

**Goal:** make **`/Game/Maps/L_Arena_04`** selectable and playable from the front-end HOST flow in **both GEs (Haywire + ProMod)** at **5v5 and 8v8**, mirroring how NANOWATT and INFINEON are wired. Per [[project-ironics-dual-ge-requirement]]: all maps play in both GEs unless stated.

> **Preferred path:** paste the **AIK prompt** (§A) into UE **Tools → Agent Chat** (Claude Code agent, *AFL Blueprint & Gameplay* profile). AIK can edit the `GameFeatureAction_AddComponents` **ComponentList** (the team/spawn/bot trio) that the Blender/exec bridge **cannot** touch. If AIK can't, use the **operator manual steps** (§B).

---

## Reference wiring (verified 2026-08-03 — the pattern to mirror)

**A config = one playlist DA + one per-map experience.** (`Docs/maps/MAP_DISPLAY_NAME_REGISTRY.md` is the SSOT for display↔asset.)

| Config | Playlist DA (`/Game/BagMan/Playlists/`) | MapID | ExperienceID | Tile | max_players |
|---|---|---|---|---|---|
| NANOWATT | `DA_AFL_Arena01_Extract3v3` | `/Game/Maps/L_Arena_01` | `B_AFLExperience_Arena01_Extract3v3` | NANOWATT / 3V3 SPEED | 6 |
| INFINEON | `DA_AFL_Arena01_Extract4v4` | `/ShooterMaps/Maps/L_Expanse` | `B_AFLExperience_Arena01_Extract4v4` | INFINEON / 8V8 SIEGE | 16 |

**Playlist DA fields** (`ULyraUserFacingExperienceDefinition`): `map_id` (PrimaryAssetId Map), `experience_id` (PrimaryAssetId LyraExperienceDefinition), `tile_title`, `tile_sub_title`, `tile_description` (FText), `tile_icon` (Texture2D), `loading_screen_widget`, `max_player_count`.

**GE experiences** (`/AFLBagMan/Experiences/`): `B_Experience_Haywire`, `B_Experience_ProMod`. Both now carry the AFL weapon-intent registry (`AFLGFA_WeaponSpawns` via `LAS_AFL_ExtractionMatch` — Haywire fixed 2026-08-03). GE delta = Haywire adds `AFLDismember` GF (+ economy); otherwise the profiles match.

**The AFL trio** (per-map experience `GameFeatureAction_AddComponents` ComponentList — the operator/AIK in-editor part, NOT bridge-editable; see `Docs/maps/INFINEON_Expanse_DESIGN.md`):
- `B_AFL_TeamSetup_TwoTeams` (size-agnostic, teams {1,2})
- `B_AFL_SpawnSelector_Dynamic` (**size-agnostic — REUSE as-is for 5v5 & 8v8**; its knobs are LOS/hot-point + side-tag filter, no team-size property)
- `B_AFLBotFill_Arena8v8` (TeamSize 8 → 16 target). **5v5 needs `B_AFLBotFill_Arena5v5`** = duplicate of the 3v3/8v8 bot-fill with **only `TeamSize` = 5** (target = 5×2 = 10).

**L_Arena_04 is already prepared:** map World-Settings `DefaultGameplayExperience = B_Experience_ProMod` (direct-PIE runs ProMod); **16 `LyraPlayerStart` side-tagged** `AFL.Spawn.Side.0`(West)/`.1`(East); **3 `B_AFL_ExtractionZone`**; weapon/heal pads wired. So spawns/extraction/pads are done — only the experience+playlist layer remains.

---

## ⚠ Operator inputs REQUIRED before running (fill these in)

1. **Display names** (Arena_04 is a roster code-name; NANOWATT/INFINEON have front-end names — Arena_04 needs one). Provide `tile_title` + `tile_sub_title`, e.g. `ARCANEON / 5V5 HAYWIRE` etc — RULED Block 150/154.
2. **Tile icon** texture (like `T_IRONICS_MapTile_NanoWatt`), or reuse a placeholder for now.
3. **Confirm scope:** 4 configs (5v5+8v8 × Haywire+ProMod), or a subset.

---

## §A — AIK PROMPT (paste into Agent Chat; Claude Code agent, AFL Blueprint & Gameplay profile)

```
GOAL: Wire /Game/Maps/L_Arena_04 to be HOST-selectable in BOTH GEs (Haywire + ProMod)
at 5v5 and 8v8, mirroring the existing NANOWATT (DA_AFL_Arena01_Extract3v3) and
INFINEON (DA_AFL_Arena01_Extract4v4) configs. Do NOT rename any existing DA /
experience / MapID (host-resolution is naming-fragile — see MAP_DISPLAY_NAME_REGISTRY.md).

STEP 0 — INSPECT FIRST, then show me your plan before creating anything:
- Open and compare: B_Experience_Haywire, B_Experience_ProMod (/AFLBagMan/Experiences),
  the per-map experiences B_AFLExperience_Arena01_Extract3v3 (3v3) and
  B_AFLExperience_Arena01_Extract4v4 (8v8), and the playlist DAs
  DA_AFL_Arena01_Extract3v3 / _4v4 (/Game/BagMan/Playlists).
- Report how the GE (Haywire vs ProMod) is composed into a per-map experience
  (action sets / GameFeaturesToEnable / the AddComponents trio) and how 8v8 vs 3v3
  team size is set. Then implement the pattern you find; ask me if it's ambiguous.

STEP 1 — Bot-fill for 5v5 (data-only, no map dependency):
- Duplicate the existing B_AFLBotFill_Arena8v8 as B_AFLBotFill_Arena5v5, change ONLY
  TeamSize 8 -> 5 (target = 5 x 2 = 10). Leave NumBotsToCreate as-is (vestigial).
- Reuse B_AFL_SpawnSelector_Dynamic unchanged for every size (it is size-agnostic).
- Reuse B_AFL_TeamSetup_TwoTeams unchanged.

STEP 2 — Per-map experiences (4), extending/mirroring the reference per-map experiences,
one per (size x GE), in /AFLBagMan/Experiences/ :
  B_AFLExperience_Arena04_5v5_Haywire, B_AFLExperience_Arena04_5v5_ProMod,
  B_AFLExperience_Arena04_8v8_Haywire, B_AFLExperience_Arena04_8v8_ProMod
  For each, set the AddComponents trio:
   - TeamSetup   = B_AFL_TeamSetup_TwoTeams
   - SpawnRules  = B_AFL_SpawnSelector_Dynamic   (reuse; do not make new)
   - BotFill     = B_AFLBotFill_Arena5v5 (5v5) or B_AFLBotFill_Arena8v8 (8v8)
  Compose the GE profile the SAME way the reference experiences do: ProMod variants
  match the ProMod profile, Haywire variants match Haywire (the only intended GE delta
  is AFLDismember + economy). Ensure AFLGFA_WeaponSpawns (weapon-intent registry) is
  present in ALL FOUR (it already is in both GE bases via LAS_AFL_ExtractionMatch —
  confirm it resolves so L_Arena_04's 15 WeaponIntent pads are not inert).

STEP 3 — Playlist DAs (4), duplicate DA_AFL_Arena01_Extract3v3 into /Game/BagMan/Playlists/:
  DA_AFL_Arena04_5v5_Haywire, DA_AFL_Arena04_5v5_ProMod,
  DA_AFL_Arena04_8v8_Haywire, DA_AFL_Arena04_8v8_ProMod
  For each set:
   - map_id           = Map:/Game/Maps/L_Arena_04
   - experience_id    = the matching B_AFLExperience_Arena04_* from STEP 2
   - max_player_count = 10 (5v5) or 16 (8v8)
   - tile_title       = ARCANEON                  (RULED Block 150)
   - tile_sub_title   = "5V5 PROMOD" / "5V5 HAYWIRE" / "8V8 PROMOD" / "8V8 HAYWIRE"
   - tile_description, tile_icon, loading_screen_widget = copy from the NANOWATT playlist
     (swap icon later when art exists)

STEP 4 — VERIFY (report back):
- Each playlist's map_id/experience_id resolve (no null PrimaryAssetIds).
- The 4 experiences compile; the trio is set; AFLGFA_WeaponSpawns present in each.
- Register the playlists in the front-end HOST list if that is a manual step in this project.
Do NOT run PIE yourself; I will PIE-verify via the HOST flow (HOST -> tile), NOT bare-map PIE.
```

---

## §B — Operator manual fallback (if AIK can't)

1. **Bot-fill 5v5:** duplicate `B_AFLBotFill_Arena5v5` from `B_AFLBotFill_Arena8v8`; set `TeamSize` = 5 (only). (8v8 already exists.)
2. **Experiences (×4):** duplicate the closest reference per-map experience per (size×GE) into `/AFLBagMan/Experiences/` as `B_AFLExperience_Arena04_<size>_<GE>`. In each, set the `AddComponents` trio: TeamSetup=`B_AFL_TeamSetup_TwoTeams`, SpawnRules=`B_AFL_SpawnSelector_Dynamic`, BotFill=5v5/8v8 variant. Match the GE profile of `B_Experience_Haywire` / `B_Experience_ProMod`. Confirm `AFLGFA_WeaponSpawns` present.
3. **Playlists (×4):** duplicate `DA_AFL_Arena01_Extract3v3` into `/Game/BagMan/Playlists/` as `DA_AFL_Arena04_<size>_<GE>`; set `map_id=/Game/Maps/L_Arena_04`, `experience_id`=matching experience, `max_player_count`=10/16, tile FText per above.
4. **Register** the 4 playlists in the front-end HOST list (project's playlist primary-asset list).
5. **PIE-verify via HOST → tile** (NOT bare-map PIE / Play-From-Here — that loads no experience). Check: AFL teams {1,2}, side-separated spawns, bots fill + fire, 3 extraction zones, weapon pads give weapons (registry resolves), heal pads work — in **both** GEs.
6. Update `MAP_DISPLAY_NAME_REGISTRY.md` with the 4 new Arena_04 rows.

---

## Notes / guardrails
- **Naming:** new assets named descriptively is fine; **never rename** existing DA/experience/MapID (the ~16-attempt host-resolution saga). Read `MAP_DISPLAY_NAME_REGISTRY.md`.
- **Weapon registry:** already present in both GE bases (verified) — if AIK's per-map experiences pull the GE profile, pads resolve; if it builds experiences from scratch, it MUST add `AFLGFA_WeaponSpawns` (SpawnTable=`DA_AFL_WeaponSpawnTable`) or the 15 weapon pads go inert.
- **Spawns/extraction/pads on L_Arena_04 are already done** — this task is only the experience+playlist layer.
