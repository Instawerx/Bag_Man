# IRONICS — Weapons Roster & Grip/Anim SSOT

> **Status: APPROVED SSOT — locked 2026-06-22 (operator-confirmed).** Goal: generated weapons that
> **HOLD and ANIMATE correctly the first time** (no pose-fixing iteration), produced **without
> over-building** (color = parameter swap, not a new build — §2). Recon-grounded: the proven
> Pulse/Beam triad was read read-only on disk (`/AFLBagMan/Equipment/`).
>
> **Four locked decisions:** (1) weapons = a **separate PAID cosmetic axis**, no body-finish inherit (§6);
> (2) **magnet = an EMP-type proven sibling**, not net-new (§3); (3) **⚠ COLOR VARIANTS = PARAMETER
> SWAPS off one proven system — the efficiency law** (§2/§4); (4) **visual brand = liquid-glass /
> electric-neon / energy + tracers** (§4.3).
>
> **Cross-ref (D2, 2026-07-08 · `IRONICS_PRICING_SCARCITY_SSOT.md` §1.5/§4.5):** each $500 Singularity grail bundle reserves a **unique weapon + an exclusive 1-of-1 weapon SKIN** (distinct tradeable child SKU ids). The bundle reserves the slot now; the exclusive-skin content is authored in **this weapons phase**.
>
> **Grounds in:** the proven `ID/WID/B_WeaponInstance` Pulse + Beam triad · the proven **EMP grenade**
> (`GA_BagMan_EMP`) · `afl-laser-beam-system` (weapon contract + `User.Color` beam tint) ·
> `ue5-interaction-ik-expert` (socket/IK) · `afl-asset-pipeline` (import/LOD) · the Lyra anim layers.
> **Resolves:** `IRONICS_PLAYER_FLOW` §9.1. **Defers:** gravity/shrink *mechanics* → the Gravity SSOT.
>
> **[SUPERSEDED-AS-BASE-SET -- 2026-07-01, operator ruling W1]** This 10-TYPE roster is now the
> **BASE-TYPE SET** that a **50+ weapon Claude/Blender MODIFICATION-FACTORY** derives FROM. Each base
> type (the purchased packs + the shipped Pulse / Beam_v2 / Pistol / Shotgun-Beam) is a PARENT that
> Claude Desktop + Blender remesh / re-beam / modify into unique derived variants (target 50+ total).
> Both this 10-type roster AND the master-build 12-weapon ability-named roster are superseded as the
> *final* roster; they persist as the parent/base set the factory derives from -- **not deleted**. The
> factory pipeline is its own deliverable (next). See `BAG_MAN_LIVE_TRACKER` amendments banner 2026-07-01.

---

## 1. THE PROVEN WEAPON SPINE — the template every TYPE inherits

The Pulse Carbine + Beam hold and fire correctly in PIE. New weapon **types** inherit this spine
(or diverge only per the **Proven-Sibling rule**, divergence specced).

### 1.1 The per-weapon asset triad (CONFIRMED live on disk)
`/AFLBagMan/Equipment/`: `ID_BagMan_PulseCarbine` + `WID_BagMan_PulseCarbine` (data BPs) +
`B_WeaponInstance_AFL_PulseCarbine` (the instance graph). The Beam repeats it. Canonical chain
(`afl-laser-beam-system`, the AFL weapon contract):
```
ULyraInventoryItemDefinition (item def)
   └─ InventoryFragment_EquippableItem ──► ULyraEquipmentDefinition (equipment def)
                                              ├─ ActorsToSpawn: weapon mesh + AttachSocket + transform
                                              ├─ muzzle socket "Muzzle" (trace/beam origin)
                                              └─ AbilitySetsToGrant: fire + cooldown abilities
   B_WeaponInstance_AFL_* : child of ULyraRangedWeaponInstance
```

### 1.2 Grip / sockets (what makes it hold right)
- **AttachSocket** on the **character** (SK_Mannequin/SKM_Manny): right-hand grip the weapon attaches to.
- **`Muzzle`** on the **weapon mesh**: the trace/beam/projectile origin (all weapons).
- **Left-hand IK socket** on the **weapon mesh**: **two-handed only** — the foregrip the layer IK-pins to.
- DCC sockets export as **`SOCKET_`-prefixed bones**.

### 1.3 Animation — the hold pose comes from the weapon's CLASS (the linked anim layer)
The equipped weapon selects its Lyra linked layer → that drives hold + locomotion + left-hand IK:
- **Rifle/2H → `ABP_RifleAnimLayers`** (the proven Pulse/Beam pose). **Pistol/1H → `ABP_PistolAnimLayers`.**
- **Class it right → it poses right, zero pose-fixing.**

### 1.4 IK
Rifle layer drives the left hand to the foregrip (two-bone/FBIK); recoil + ADS procedural (Control Rig),
thread-safe. One-handed skips the foregrip pin.

### 1.5 ⚠ VERIFY-IN-EDITOR — the 5-min reads off proven Pulse BEFORE generation (LAW)
Confirm (do not invent): the **ID-vs-WID role split** (item def vs equipment def) · the **`AttachSocket`
name** · the **anim-layer declaration site** · the weapon-mesh **socket names** · the **AbilitySet** asset.

---

## 2. ⚠ THE EFFICIENCY LAW — color variants are PARAMETER SWAPS, not separate builds

**The single most important production rule. Do NOT over-produce.** Two costs, kept strictly separate:

| | **COST A — REAL BUILD (careful, recon-grounded)** | **COST B — PARAMETER SWAP (cheap)** |
|---|---|---|
| What | the weapon **TYPE**: mesh + grip + the left/right sockets + **the anim-layer class** + the triad + **one proven Niagara/material system** | the **COLOR VARIANTS** of a type |
| How | built **once** against the proven Pulse/Beam spine (§1) | **copy** the type's proven Niagara + set **`User.Color`** (and material color params); tint **N times** |
| Output | a working, holdable, firing weapon type | N color **SKUs** off the **one** system — **no new mesh, no new system** |

**Explicit law:** **do NOT generate 15 distinct beam meshes/systems.** Generate the beam/pulse **type
once**; the color SKUs are **`User.Color` / material parameter sets** off that one proven system. (Memory
+ the laser library confirm this is already the pattern — "each beam is multiple beams" = copy-and-tint;
`User.Color` is the beam's tint input.)

**The variant set (per tintable type):** **7 base-palette tints** (the 7 base `AFL.Finish.*`
Blue/Green/Purple/Pink/Red/Black/Yellow) **+ 5 NEW brand colors** = **12 color SKUs** — all parameter
swaps. (The operator's "~15" ≈ this.) The 5 new colors = **MAGENTA · INDIGO · SOLAR · CRIMSON · LIME** (§8, law).

> **Mesh/grip/anim = a real build. Color = a cheap parameter swap. Never conflate them.**

---

## 3. THE ROSTER — each TYPE specced against the spine

| Type | Hands | Tier | Grip/anim vs spine |
|---|---|---|---|
| **Rifle Pistols** (Pulse class) | 2H | base | **= THE TEMPLATE** (Pulse). Right-hand attach + left foregrip IK + `ABP_RifleAnimLayers`. |
| **Beams** (Beam class) | 2H | base | **= the proven Beam.** Rifle-class 2H; the beam cue system. |
| **Hand Pistols** | **1H** | base | **1H divergence:** grip + `Muzzle` only (**no foregrip**); `ABP_PistolAnimLayers`; left hand free. |
| **Grenades** (EMP) | throw | base | **Proven EMP** (`GA_BagMan_EMP`) on ShooterCore `GA_Grenade`/`B_Grenade`; toss montage. |
| **Magnet** | throw | base | **EMP-TYPE PROVEN SIBLING (§3.1)** — NOT net-new. |
| **Gravity Guns** (OmniWalk) | 2H | **ULTRA-RARE, map-gated** | rifle template + IRONICS brand. **Mechanics → Gravity SSOT.** |
| **Shrink Guns** | *(form factor → Gravity SSOT)* | **ULTRA-RARE, map-gated** | grip template per its confirmed hands. **Hands + mechanics → Gravity SSOT.** |

### 3.1 Magnet = a Proven-Sibling of the EMP grenade (the cheap framing — LAW)
The magnet is **NOT** a net-new grab weapon. It **templates from the proven EMP grenade**: a **thrown
projectile that STICKS to a target or wall, then applies its effect.** Structural diff vs `GA_BagMan_EMP`:
- **SAME (inherit verbatim):** the throw/arc, the **stick-on-impact** behavior (target or wall), the
  grenade triad (`GA_Grenade`/`B_Grenade` base), the toss montage, the projectile mesh pattern.
- **DIVERGES (the only new work):** the **on-stick EFFECT** — a magnet *pull/attract* instead of the
  EMP's disable. The pull can reuse the proven `AFL_GRAB` pull math as the effect payload.
- So the magnet = **EMP shell + a different effect**, in the choose-from base pool. Much cheaper than
  the prior "net-new magnet" framing.

### 3.2 One-handed vs two-handed grip divergence
- **2H (rifle/beam/Pulse/gravity):** mesh = grip + `Muzzle` + **foregrip socket**; `ABP_RifleAnimLayers`.
- **1H (hand pistol):** mesh = grip + `Muzzle` **only**; `ABP_PistolAnimLayers`; left hand free.

### 3.3 SPECIAL-GUN category — LOCKED SIGNATURE BEAM (operator-directed 2026-07-03)
A **special gun** is a weapon whose **beam is a locked signature**: the independent `BeamId` cosmetic axis
(a player's owned beam applies to **ANY** weapon, overriding its default beam — see the beam re-grounding)
**does NOT apply** to it. A special gun **keeps its own signature beam** regardless of the equipped beam.
- **A defined category (more members will be added); FIRST member = the Gravity Gun** (`OmniWalk`, §3
  roster — ULTRA-RARE, map-gated). Its signature-beam *spec* is authored when the gravity gun is built
  (→ Gravity SSOT); this section lands the **category + the mechanism** only, not the gravity gun's beam.
- **Mechanism (wired in AFLCombat 2026-07-03):** a per-weapon-instance bool **`bLockedSignatureBeam`**. The
  beam consumer (`UAFLSkinColorComponent::ApplyBeamColorToEquipped`, driven by
  `UAFLSkinColorControllerComponent::RefreshBeamColorForPawn`) **reflection-reads** it on the equipped
  `B_WeaponInstance_AFL_*`: **`true` → SKIP the `BeamId` override** (keep the authored signature beam);
  **absent / `false` → the beam override applies** (every normal weapon). A special gun opts in by setting
  `bLockedSignatureBeam=true` on its weapon-instance BP.
- **Rationale:** a signature beam is part of an ultra-rare weapon's identity — not overridable by a common
  owned beam. This is the beam-axis analogue of "a base weapon's original color is its identity" (the
  high-quality-base principle): the special gun's beam is baked-identity, the `BeamId` axis is opt-in
  cosmetic, and identity wins.

---

## 4. THE TWO GENERATION LISTS — build A once, tint B (never conflate)

### LIST A — REAL WEAPON TYPES to build (Cost A, the careful proven-spine work)
**7 new types** (+ 3 proven reused). Each = mesh + grip + anim-layer class + triad + one Niagara/material system:

| # | New type | Hands | Anim layer | Notes |
|---|---|---|---|---|
| 1–3 | **3 Hand Pistols** | 1H | `ABP_PistolAnimLayers` | Compact Sidearm · Hand Cannon · Machine Pistol (§8, law) |
| 4 | **1 Rifle Pistol** (the 2nd; Pulse is #1) | 2H | `ABP_RifleAnimLayers` | clone the Pulse spine |
| 5 | **1 Magnet** (EMP-type) | throw | grenade | EMP shell + pull effect (§3.1) |
| 6–7 | **2 Beams** (the 2nd + 3rd; Pulse-Beam is #1) | 2H | `ABP_RifleAnimLayers` | clone the Beam spine |
| — | *(reused, already proven)* | | | **Pulse Carbine · Pulse-Beam · EMP grenade** |

### LIST B — COLOR VARIANTS as parameter swaps (Cost B, the cheap work)
For each tintable type (beams, pulses, and the FX of the others): **12 color SKUs** = **7 base-palette
tints + 5 new brand colors**, produced as **`User.Color` + material-param sets off the ONE proven
system per type.** **Zero new meshes, zero new systems.** Sold as weapon-skin SKUs (§6, any rarity/bundle).

> Generation does **A once** (7 careful builds) and **B as tints** (parameter sets) — it must **never**
> build a mesh/system per color.

### 4.3 Visual brand contract (the look every type + variant inherits — LAW)
**Liquid-glass / electric-neon / energy-themed** lasers + pulses **WITH TRACERS**: translucent,
glass-like energy; neon palette; **tracer trails**. This is the aesthetic the proven Niagara/material
system carries; every type inherits it, every color variant tints it (`User.Color`). Generation targets
this look — translucent energy + neon + tracers, not opaque/metallic.

---

## 5. BASE-SET SIZING — resolves `IRONICS_PLAYER_FLOW` §9.1 (LAW)

- **Base weapon pool → choose 5:** **3 hand pistols + 2 rifle pistols (incl. proven Pulse) + EMP grenade
  + magnet (EMP-type)** = **7 → choose 5.**
- **Beams → choose 2:** **proven Pulse-Beam + 2 new** = **3 → choose 2.**
- **New TYPES to build (LIST A):** **3 pistols + 1 rifle + 1 magnet + 2 beams = 7.** Reused proven: Pulse,
  Pulse-Beam, EMP. *(Color variants are LIST B parameter swaps — not counted as builds.)*

---

## 6. WEAPONS = a SEPARATE PAID COSMETIC AXIS (Decision 1 — LAW)

- Weapon appearance is its **own purchasable axis** (its own monetization lane) — **weapons do NOT
  auto-inherit the player body finish** (Ruling 1 governs the **body only**).
- **Default:** every weapon ships **broadly brand-aligned** (a default IRONICS color, free with the weapon).
- **Matching-the-player-finish, or any other color/skin = EXTRA (paid).**
- **A weapon skin is a cosmetic SKU under the existing model** (`IRONICS_PRICING_SCARCITY_SSOT`): it
  carries the **same data shape** (PriceRung / RarityTier / MintCap / bundle / discountable / tradeable)
  and can be **any rarity tier or part of a bundle** — exactly like a finish or identity SKU. The color
  variants (§4 LIST B) are these SKUs.
- **⚠ INDEPENDENT-AXIS ALIGNMENT (2026-07-03, operator-ruled):** a weapon skin is its **OWN item axis**
  `AFL.WeaponSkin.<Pattern>.<Color>` (`WeaponSkinId`) — **ONE skin applies to ANY weapon**, overriding the
  weapon's baked original color. This SUPERSEDES the earlier per-weapon `AFL.Weapon.<W>.<Color>` coupling (a
  skin was wrongly a weapon property — the same drift class as beam-into-skin). The weapon's own SKU
  `AFL.Weapon.<W>` = the gun + its baked ORIGINAL color (identity); the skin OVERRIDES. Canonical model:
  `IRONICS_CATALOG_MATRIX.md` → **THE INDEPENDENT-AXIS ECONOMY MODEL** (every cosmetic = an independent owned+applied item).

---

## 7. GENERATION-READINESS (mesh / sockets / deliverables / import)

- **Poly budget** (`afl-asset-pipeline`): **20k tris LOD0 · 10k LOD1 · Nanite = No** (FPP weapons).
- **REQUIRED mesh sockets:** `Muzzle` (all) · grip (all) · **left-hand foregrip (2H only)** — `SOCKET_`-prefixed.
- **Pivot/orientation:** UE **Forward = −Y, Up = Z**; grip aligned to the AttachSocket snap (match proven Pulse).
- **Deliverables per TYPE (List A):** mesh + LODs + textures (`_D/_N/_ORM`, ASTC mobile) + the **triad**
  (cloned from Pulse) + **anim-layer declaration** (rifle/pistol) + AbilitySet + **Cooldown GE** + HUD icon +
  the **one Niagara/material system** (brand-visual, §4.3) + the catalog SKU `AFL.Weapon.<Name>`.
- **Deliverables per VARIANT (List B):** a **color parameter set** (`User.Color` + material params) + its
  catalog skin-SKU. **No mesh/system.**
- **Import** per `afl-asset-pipeline`: SK_Mannequin skeleton (if skeletal), `Use T0AsRefPose`, LFS binaries, redirector-clean.
- **Acceptance (PIE):** attaches in-hand at the grip, hands land (foregrip for 2H), fires from `Muzzle`,
  cooldown gates over-fire — **inherited the spine with zero pose-fixing.**

---

## 8. BRAND CONTENT — CONFIRMED LAW (was PROPOSED)
- **The 5 NEW brand-aligned weapon colors** (the §2/§4 variant set, on top of the 7 base-palette tints):
  **MAGENTA · INDIGO · SOLAR · CRIMSON · LIME.** Rationale: **spread across the color wheel for in-play
  distinguishability** — these are separate **paid weapon-skin SKUs**, so players must see the difference
  at a glance — and all **hold up under the liquid-glass / electric-neon emissive look.** *(Cyan + Teal
  were dropped: they wash out in the blue-green band under neon/glass bloom.)* So the **variant tint set =
  the 7 base-palette tints + these 5 = 12 color SKUs per tintable type** (§2 / §4 LIST B).
- **The 3 hand-pistol archetypes** (LIST A #1–3) — a **play-feel spread, NOT cosmetic clones:**
  **Compact Sidearm** (fast/light) · **Hand Cannon** (heavy) · **Machine Pistol** (burst).
- **Shrink-gun hands (1H/2H) — DEFERRED → the Gravity SSOT.** Its form factor follows its mechanics; it
  is **not** an open item in this doc (do not guess it here).

---

## 9. Cross-links
- **`IRONICS_PLAYER_FLOW.md`** — §9.1 resolved here; weapon skins are their OWN `AFL.WeaponSkin.<Pattern>.<Color>`
  axis (`WeaponSkinId` — independent item, applies to ANY weapon; aligned 2026-07-03, superseding the retired
  per-weapon `AFL.Weapon.<W>.<Color>` coupling), the separate paid axis (§6), bound-if-free (§8.4).
- **`IRONICS_PRICING_SCARCITY_SSOT.md`** — weapon **and weapon-skin** SKUs ride this (base→FLICKER;
  limited skins → rarity ladder; same data shape).
- **Gravity SSOT (to be authored)** — gravity + shrink **mechanics** (this doc declares the shells).
- **Skills:** `afl-laser-beam-system` · `ue5-interaction-ik-expert` · `afl-asset-pipeline`.

---

*Finalized read-only 2026-06-22 — all 4 decisions + the brand content locked as law (5 weapon colors
MAGENTA/INDIGO/SOLAR/CRIMSON/LIME · 3 pistol archetypes Compact Sidearm/Hand Cannon/Machine Pistol).
Two generation lists (A build TYPES once, B tint variants) so the phase never over-produces. **No open
PROPOSED items remain** — the only deferred item is the shrink-gun form factor (→ Gravity SSOT). No code,
no build, no generation.*
