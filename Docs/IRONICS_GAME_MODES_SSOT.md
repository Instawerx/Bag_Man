# IRONICS — GAME MODES SSOT (Haywire / Pro Mod / Melee)

**Status:** DESIGN OF RECORD — operator-approved 2026-07-30. The canonical spec for the tri-mode split.
Tracker: `~/.claude/plans/rustling-sprouting-grove.md`. Extends `IRONICS_MAP_MODE_SPEC.md §5` (mode == Experience).
Siblings: `AFL_PRO_MOD_CHARACTER_BRIEF.md`, `IRONICS_MELEE_RULESET_SSOT.md`.

## 1. Why this exists

Character-mod work kept colliding with the **dismemberment game feature** (zone-HP + gib system baked into
core combat/pawn). Resolution = split the game into **modes** where dismemberment is a *per-mode rule*, not an
always-on coupling. A "mode" here is a stock Lyra **`ULyraExperienceDefinition`** — it lists which GameFeatures
activate + which components/abilities are granted. Therefore **meshes, weapons, hand cannons, cosmetics, movement,
IK, scoring, and economy are mode-agnostic and carry over untouched** — only a thin *rules layer* diverges. This
is an assembly of existing systems, not a rewrite.

## 2. The three modes

| | **Haywire** | **Pro Mod** | **Melee** |
|---|---|---|---|
| One-liner | current dismember gameplay, forward-face renamed | no gore, clean health, FBIK-enhanced, new characters | instant-respawn deathmatch, K+A−D ranking |
| Experience asset | `B_Experience_Haywire` (rename of `B_Experience_BagMan`) | `B_Experience_ProMod` (new) | `B_Experience_Melee` (new) |
| Pawn BP | `B_Hero_BagMan` | `B_Hero_BagMan_Pro` | `B_Hero_BagMan_Pro` |
| PawnData | `DA_AFL_PawnData_Hero_Default` | `DA_AFL_PawnData_Hero_Pro` | `DA_AFL_PawnData_Hero_Pro` |
| `AFLDismember` GF | enabled | **dropped** | **dropped** |
| Zone-HP model + gibs | on | **gated off (clean single-health)** | **gated off** |
| Weapon/hand IK (`UAFLWeaponIKComponent`) | as-is | as-is | as-is |
| **True FBIK** (`CR_ProMod_FBIK`) — new | ❌ | ✅ | ✅ |
| Enhanced movement | base | faster/snappier | faster/snappier |
| Dash / Climb / Grab | optional | granted | granted |
| New characters + 8-color axis | no | yes (1 pilot first) | shared with Pro |
| Ranking authority (K+A−D) | no | no | `UAFLDeathmatchRankComponent` (new) |
| Respawn | round-gated | round default | **instant** (0 delay, mid-round) |
| Weapons | full arsenal | full arsenal | **full arsenal (any weapon — NOT melee-restricted)** |

Home folder: `Plugins/GameFeatures/AFLBagMan/Content/Experiences/`. No-dismember precedent already on disk:
`B_LyraExperience_AFL_Arena_Test` (no `AFLDismember`, non-BagMan pawn) — the structural model to copy.

## 3. Per-mode Experience contents

**Haywire** = existing behavior. Keep `AFLDismember` in `GameFeaturesToEnable`; keep `AFLDismemberLegPenaltyComponent`
in AddComponents; pawn `B_Hero_BagMan` (its `AFLDismemberComponent` rides the pawn BP, not the Experience). This is
a **rename only** — a redirector plus updating any `WorldSettings.DefaultGameplayExperience` refs; zero behavior change.

**Pro Mod** — duplicate Haywire, then:
- `DefaultPawnData` → `DA_AFL_PawnData_Hero_Pro` (→ `B_Hero_BagMan_Pro`, the dismember-free pawn).
- `GameFeaturesToEnable`: **drop `AFLDismember`** (keep `AFLBagMan`, `AFLCombat`, `AFLMovement`, `AFLCharacter`, `ShooterCore`).
- AddComponents: `UAFLWeaponIKComponent` + the new FBIK delivery component; **remove** `AFLDismemberLegPenaltyComponent`.
- Apply `State.Mode.NoDismember` (the gate, §4) to the pawn ASC.
- Grant `DA_AFL_AbilitySet_Movement_Dash/Climb/Interaction` + base weapon set.

**Melee** — duplicate Pro Mod, then add `UAFLDeathmatchRankComponent` (AddComponents) + instant-respawn config
(`IRONICS_MELEE_RULESET_SSOT.md`). Same clean-health gate. Same full arsenal.

## 4. The dismemberment gate (clean single-health — LOCKED)

Two decoupled layers: **Layer 1** zone-HP routing in `AFLDamageExecCalc.cpp` (always-on; broadcasts
`Event.Dismember.Sever.AFL`; **trap:** unseeded zone HP → limb/head shots deal 0 damage). **Layer 2**
`AFLDismemberComponent::OnSever` (gibs/consequences), placed on the pawn BP.

**The one new combat code change** — a gate tag `State.Mode.NoDismember` (register in `Config/DefaultGameplayTags.ini`)
checked in **both**:
- `Plugins/GameFeatures/AFLCombat/Source/AFLCombat/Private/AbilitySystem/AFLDamageExecCalc.cpp` — wrap the zone-routed
  block in `if (!bDismemberDisabled)`; the `else` routes all damage to the base **Health** attribute (conventional
  single-health; sidesteps the zero-damage trap by construction).
- `Plugins/GameFeatures/AFLDismember/Source/AFLDismember/Private/AFLDismemberComponent.cpp` `OnSever` — early-return on the tag.

Applied per-Experience via a `GameFeatureAction` that sets the tag on Pro/Melee pawns. Belt-and-suspenders: Pro/Melee
also use the dismember-free pawn + drop `AFLDismember`, so nothing dismember loads. **Only this gate removes Layer 1's
zone model** → conventional AAA damage feel.

## 5. True FBIK (Pro/Melee) — BUILT (corrected 2026-08-05)

**CORRECTION (2026-08-05, disk-verified):** this section previously read "NEW, currently MISSING" and
asserted the project has **no full-body IK**. Both are **false**. FBIK is built and is permanent project
doctrine (operator ruling): every character, current and future, is FBIK on the rigged Manny/Quinn skeleton
bases. On disk: **`CR_ProMod_FBIK.uasset`**, **`ABP_ProMod_FBIK_PP.uasset`**, **`IK_ProMod_FBIK.uasset`**
(`Content/BagMan/ProMod/`, banked `8367f0b8`), and the **`FullBodyIK` plugin is `"Enabled": true`** in
`Bag_Man.uproject`. `IRONICS_PROMOD_CHARACTER_SSOT.md` §1 independently records FBIK as DONE
(operator-confirmed 2026-07-31). The original design rationale below is retained as the authoring record.

Original text (superseded): `CR_AFL_CoreIK` is a spec alias for
`Content/Characters/Heroes/IRONICS/Animations/CR_AFL_IRONICS.uasset` — only per-limb Two-Bone IK + Aim + Lyra
trace foot-placement. `IK_Mannequin` exists but is retarget-time only. Fix = author a new **`CR_ProMod_FBIK`**
(Full-Body IK / PBIK solver over the shared `SK_Mannequin`) + a `SetControlValue` delivery component (mirror
`UAFLWeaponIKComponent`), layered as an *added* Control Rig node, gated to Pro/Melee, **without touching the
shipped `CR_AFL_IRONICS`**. Full spec in `AFL_PRO_MOD_CHARACTER_BRIEF.md`.

## 6. Mode selection / surfacing

Per `IRONICS_MAP_MODE_SPEC.md §5` (mode == Experience):
1. **Map** `WorldSettings.DefaultGameplayExperience` → per-map default mode.
2. **URL** `?Experience=B_Experience_ProMod` → direct boot / PIE.
3. **Playlist / front-end** → one `LyraUserFacingExperienceDefinition` row + mode button per Experience primary
   asset ID (3 buttons: Haywire / Pro Mod / Melee). Scan dirs already configured in `Config/DefaultGame.ini`
   (`/AFLBagMan/Experiences`, `/Game/BagMan/Playlists`).

## 7. New C++ (the entire code surface for the mode split)

1. `State.Mode.NoDismember` gate — 2 files (§4).
2. FBIK delivery component (§5) — mirrors `UAFLWeaponIKComponent`.
3. `UAFLDeathmatchRankComponent` (Melee ranking) — mirrors `UAFLRoundManagerComponent`
   (`AFLCombat/.../Public/Round/`). See `IRONICS_MELEE_RULESET_SSOT.md`.

Everything else is data/asset authoring in-editor. Build discipline: editor-closed, two-engine (D: source
LyraEditor → C: launcher rebuild).

## 8. Verification

- **Haywire regression:** boot `B_Experience_Haywire` — dismember/leg-penalty/gibs unchanged (rename is behavior-neutral).
- **Pro Mod:** `?Experience=B_Experience_ProMod` — no gibs, single-health damage (headshots normal, limbs don't zero),
  faster movement + Dash/Climb/Grab, weapon IK holds; then FBIK (foot planting on slopes, full-body pose) with stock
  characters unaffected.
- **Melee:** `?Experience=B_Experience_Melee` — kill → instant respawn, K+A−D leaderboard live, ends at kill-target.

## 9. Open items / risks

- Damage: after the gate, PIE-verify no zero-damage on the no-dismember pawn (the zone-HP trap).
- FBIK rides the frozen shared `ABP_Mannequin_Base` — keep `CR_ProMod_FBIK` additive + isolated (a regression breaks
  weapon/grab IK on **every** character).
- New `Cosmetic.Identity.*` / `Cosmetic.Brand.*` / `State.Mode.NoDismember` tags need an **editor relaunch** to resolve.
- Melee durable ranking = a new PlayFab write shape (undemonstrated) + Glicko-2 MMR (unbuilt AFL-2201) → session-local
  MVP first (`IRONICS_MELEE_RULESET_SSOT.md`).
</content>
