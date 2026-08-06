# IRONICS — HAND LASER CANNON LINE — BUILD SCOPE (of record)

**Status:** SCOPE LOCKED (decisions D1–D7 operator-ruled 2026-07-27). A NEW **worn** weapon archetype —
the forearm-mounted gauntlet cannon (ref: FANATICS dragon-cannon). Build the TYPE once (Cost A); the line
+ colors are tints (Cost B), per `IRONICS_WEAPONS_SSOT.md` §2/§4. **No new fire C++** — reuses the proven
Beam / Pulse / Rocket bases. The one genuinely new system is the **dual-mount (akimbo) equipment model**.

**Grounds in (conform, invent nothing):**
- Fire bases (proven): `UAFLAG_BeamChannel_v2` (beam, heat-gated) · `UAFLAG_Laser_Pulse` (pulse) ·
  `UAFLAG_Projectile_Base` (straight / homing / **arc-lob**, `bArcLob` added 2026-07-27).
- Weapon spine: `ID_AFL_→WID_→B_WeaponInstance_(child of _Rifle/_Pistol)→AbilitySet→B_AFL_@socket` + `Muzzle` socket.
- Grip taxonomy: `IRONICS_WEAPONS_SSOT.md` §3.2 (2H rifle / 1H pistol). **This doc adds the 3rd class: ARM-WORN.**
- Loadout: `FAFLCosmeticSelection.WeaponId` (single today) → extended to **Left/Right** here (`LOADOUT_DESIGN.md` §5 Loadout-v2, made *simultaneous* not swap).
- IK: `UAFLWeaponIKComponent` + Control Rig; **FBIK** solver (D7) — `ue5-interaction-ik-expert` skill.
- Gen pipeline: Tripo → conform → one-`root`-bone SK → sockets (this doc changes the conform *origin* only).
- Identity separation: the suit (e.g. FANATICS full-body) is the **identity/skin axis**; the cannon is the **weapon axis**. No coupling.

---

## 1. THE ARCHETYPE — ARM-WORN (0-hand-held), the defining divergence
Every current weapon is *held* in `weapon_r`. The hand cannon is **WORN over the lower arm, extending over
the full hand** (D1), maw = the muzzle, fired fist-forward. Consequences that reshape the build:
- **Attach = an arm bone**, NOT the hand grip. New socket on `lowerarm_r`/`lowerarm_l` (mirrored) that seats
  the gauntlet over the forearm→fist. **No `GripPoint_L`** (worn, not gripped) — the whole two-bone
  support-hand IK problem *disappears*.
- **Both arms become mount points** → native dual-wield (§4).
- **`Muzzle`** = the dragon-maw bore = the beam/pulse/rocket origin.

## 2. THE BUILD LAW (Cost A once, Cost B tints)
- **Cost A (real build, once):** (a) the gauntlet mesh TYPE + forearm-attach + `Muzzle`; (b) the FBIK aim +
  fire anim layer (§3); (c) the **dual-mount equipment model** (§4 — the one new C++/BP system).
- **Cost B (cheap, the line):** brand maws (FANATICS dragon first, then other brand creature-heads) + the
  neon color tints — swapped mesh + `User.Color`/material params off the ONE proven system. A whole LINE off one type.

## 3. ANIMATION — FBIK-driven aim (D7), minimal hand-authored poses
- **Aim = procedural (FBIK).** A Full-Body IK solver (Control Rig) drives **both arms' cannon muzzles to
  point at the aim-converge target** (§4). Arm-extend/fist-forward is the solved pose, not a baked stance —
  cheaper than a full pose set AND smoother across movement/strafe. Recommended over extending
  `ABP_PistolAnimLayers`; if a base layer is still wanted, `ABP_GauntletAnimLayers` hosts the FBIK + additives.
- **Hand-authored (thin):** idle-lowered → raise, **additive recoil** (kick *up the arm* per shot, layered
  over FBIK), beam overheat-vent, pulse/rocket reload/charge. Both arms posed independently for akimbo.
- **Muzzle FX:** the maw opens/glows on fire (anim-notify + Niagara at `Muzzle`) — the ref's glowing bore.

## 4. DUAL-MOUNT (AKIMBO) — the core new system (D2/D3/D4/D5/D6)
**Two live weapon instances, one per forearm — both active at once, individually controlled (D2).**
- **Model:** extend the selection to **`LeftWeaponId` + `RightWeaponId`** (each resolves its own
  `B_WeaponInstance` + GA + `Muzzle`, attached to its forearm socket). This is `LOADOUT_DESIGN.md` §5's
  Loadout-v2 Primary/Secondary — but **simultaneous**, not a swap.
- **Controls (D3):** **independent triggers — one per hand** (LMB = right, RMB = left). Fire either or both;
  both-held = akimbo. A second fire `InputTag` (left) is the input add.
- **Fire type = per-hand equipped weapon (D4).** The cannon is a **MOUNT that hosts whatever weapon that
  hand has on** — the fire base (Beam/Pulse/Rocket) comes from each hand's `WeaponId`. **Mixed is the default
  and the point:** left beam + right rocket, dual-pulse, etc. all fall out of the Left/Right pair. Combos are free.
- **Aim-converge:** both muzzles toe-in to the crosshair point via FBIK so dual-fire **lands on target** —
  the thing that makes akimbo feel good, not spray.
- **Single-cannon allowed (D5):** one arm + free hand is a valid loadout — **player choice, gated only by
  what they own** (own one cannon → single; own two → dual). Free hand stays open for utility/melee later.
- **Balance = FUN-FIRST, least-restricted (D6):** do NOT gate dual behind a premium wall or hard lockout.
  Tune feel, not access — cadence, aim-converge, and (reused) per-arm **heat** on beams give natural
  counterplay (over-hold one arm → vent) without taking the toy away. Dual is open to anyone who owns two.

## 5. FIRE — 100% REUSE (no new fire C++)
Each cannon = the proven spine with GA = one of the bases, spawning from its `Muzzle`:
- **Beam** = `UAFLAG_BeamChannel_v2` (dragon "breath," heat-gated) · **Pulse** = `UAFLAG_Laser_Pulse` (maw bolts)
  · **Rocket** = `UAFLAG_Projectile_Base` (straight/homing/**arc-lob**). The arc-lob work already covered the projectile side.
- Per-hand GA activation is instanced (Left GA + Right GA); `User.Color` = the cannon's neon per the color axis.

## 6. MESH + SOCKETS (our pipeline, one change)
Tripo gen → conform → one-`root`-bone SK → sockets, with:
- **Origin = the forearm-attach seat** (not a grip). **`Muzzle`** = the maw bore. **No `GripPoint_L`.**
- **Left/right mirror** from one gen. Muzzle-FX socket at the bore. Native neon in-mesh; brand via emissive decal.

## 7. THE LINE (roster)
TYPE built once → **FANATICS dragon-cannon** (ref) first → other brand creature-maw gauntlets → universal neon
tints. Each = mesh swap + color tint off the one proven type + the dual-mount model.

## 8. LANES
- **Claude Code:** Tripo gen + forearm-conform the gauntlet TYPE (forearm-origin, `Muzzle`, mirror) + rig spec
  + the line; C++-scope the **dual-mount model** (Left/Right `WeaponId`, per-arm GA, second fire InputTag, aim-converge) — editor-closed engine work.
- **AIK (in-editor):** the FBIK Control-Rig aim + additive recoil + forearm sockets + PIE-watch (akimbo feel).
- **Operator:** the dual-wield PIE feel + calls/merges.

## 9. RULED DECISIONS (locked 2026-07-27)
- **D1 Attach:** lower-arm mount over the full hand (per ref). ✅
- **D2 Dual model:** both at once, individually controlled (both or one). ✅ (simultaneous akimbo)
- **D3 Triggers:** independent, one per hand (LMB right / RMB left). ✅
- **D4 Fire types:** mixed — each hand fires whatever weapon it has on. ✅
- **D5 Single:** allowed — player choice, gated by ownership. ✅
- **D6 Balance:** funnest / least-restricted — no premium gate; tune feel not access. ✅
- **D7 Anim/IK:** new gauntlet layer OK; **use FBIK** for the arm aim (appropriate). ✅

## 10. OPEN SUB-QUESTIONS (flag at build-time, not blockers)
- Off-hand/left-arm input binding + HUD reticle for dual (two heat bars? one converged reticle?).
- Movement/ADS feel with two arms up (procedural lower on sprint via FBIK?).
- Does a cannon-hand still allow melee/grab when not firing (free-hand utility on the single-cannon path)?
- Recoil convergence tuning (how much toe-in) — a playtest dial.

---

## APPENDIX A — DUAL-MOUNT C++ ENGINEERING SCOPE (disk-grounded 2026-07-27)

**The equip path today (read):** `AFLSkinColorControllerComponent::RefreshWeaponForPawn` (L415) resolves
`FAFLCosmeticSelection.WeaponId` → `UAFLWeaponCosmeticAsset` carrier → its `ULyraEquipmentDefinition` →
`ULyraEquipmentManagerComponent::EquipItem` — **first UNEQUIPPING the current primary so the selection
REPLACES rather than stacks**. Server-only (`EquipItem` is authority); Lyra's `FLyraEquipmentList` fast-array
replicates the equipped weapon to clients. Fire = the single `InputTag.Weapon.Fire` → the fire GA; the GA
resolves the muzzle via `ResolveMuzzleLocation` (inherited from `UAFLAG_Laser_Base`). `FAFLCosmeticSelection`
(`AFLCosmeticSelectionTypes.h`) is a plain replicated struct — `WeaponId`/`BeamId`/`WeaponSkinId`/… all single FNames.

**The core divergence:** single-active → **two simultaneously-held, per-hand, individually-fired** instances.
Five changes, each additive (single-weapon path stays byte-identical when the left slot is unset):

1. **Selection (additive struct field).** `FAFLCosmeticSelection` gains **`FName LeftWeaponId`** (existing
   `WeaponId` = the RIGHT hand). Replicated with the struct (already `ReplicatedUsing`). Left unset (`NAME_None`)
   → single-cannon = today's behavior exactly (D5).

2. **Equip = hold two, DON'T unequip (the key change).** A `RefreshHandCannonsForPawn` path: resolve
   `WeaponId` → right forearm slot, `LeftWeaponId` → left forearm slot, and **EquipItem BOTH without the
   "unequip primary" step** — they coexist (stack), not replace. Each carrier's `ULyraEquipmentDefinition`
   sets its `ActorsToSpawn[].AttachSocket` = the forearm socket (`weapon_lowerarm_r` / `weapon_lowerarm_l` —
   new sockets on the mannequin, NOT `weapon_r`). Guard: only ARM-WORN weapon types dual-mount; held weapons
   keep the single replace-path. ⚠ Lyra's QuickBar assumes one active weapon — the dual pair is managed as a
   **paired equipment set** (both equipped, neither is "the holstered other"); the held-weapon quick-swap is
   bypassed while a cannon pair is active.

3. **Two fire inputs (the input add).** Add **`InputTag.Weapon.Fire.Left`** (existing fire = the RIGHT).
   Bind in the AbilitySet/InputConfig. RIGHT cannon's fire GA triggers on `InputTag.Weapon.Fire` (unchanged);
   LEFT cannon's on `InputTag.Weapon.Fire.Left` (D3 — independent per-hand triggers; fire either/both, D2).

4. **Per-hand GA + muzzle (the fire side, reuse the bases).** Each hand's fire GA is one of the proven
   bases (Beam/Pulse/Rocket — **whatever that hand's `WeaponId` resolves to, D4 mixed**). The only base tweak:
   `ResolveMuzzleLocation` must pick the correct hand's `Muzzle` (the GA already reads the equipped
   instance's mesh — per-hand instances give per-hand muzzles for free; verify the left instance's socket
   resolves). Cooldown/heat are per-instance already (the beam heat GEs live on the weapon instance), so
   per-arm heat/vent (D6 counterplay) falls out — no lockout needed.

5. **Aim-converge (FBIK + trajectory).** The fire GAs already aim off `GetBaseAimRotation` (control rotation)
   — so BOTH hands' projectiles/beams aim at the crosshair by default; the only new work is the **cosmetic
   toe-in** so the muzzles visually point at the converge point. Drive that with **FBIK** (D7): the Control-Rig
   solver points both forearm effectors at the aim-converge target; the GA trajectory stays server-authoritative
   off control rotation (no client-view trust — same doctrine as every existing fire path).

**Lanes:** Claude Code = the struct field + `RefreshHandCannonsForPawn` (no-unequip dual equip) + the second
`InputTag.Weapon.Fire.Left` binding + per-hand muzzle resolve (editor-closed C++). AIK = the forearm sockets +
FBIK Control-Rig aim/recoil + PIE. **Additive + guarded** — the 7 existing projectile/beam/pulse weapons and the
single-held path are untouched (dual only engages for ARM-WORN types with a left slot set).

**Risk notes:** (a) the QuickBar "one active weapon" assumption is the main integration risk — the paired-set
management must not let one cannon holster the other; prototype the two-EquipItem coexistence first. (b) replication:
both instances ride the proven `FLyraEquipmentList` fast-array (already client-replicated) — no new net path.
(c) bots: `BTS_Shoot` fires `InputTag.Weapon.Fire` only → bots fire the RIGHT cannon; a left-fire bot event is a
follow-up (not launch-blocking).
