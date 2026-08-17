# IRONICS_CHARACTER_CREATOR_SSOT

**Status:** NEW. System and flow definition for the player character creator.
**Date:** 2026-08-17
**Basis:** CC-READ-1 through CC-READ-4. Every mechanism claim cites a file and line, or is
marked UNVERIFIED.
**Companion:** `IRONICS_PRICING_SSOT.md` (economy), `IRONICS_CC_ROADMAP.html` (build order).

---

## 1 · What the creator is

**The creator does not create identities. It produces a selection record over a generic
chassis.**

Every existing identity is *authored*: a robot BP, two MIs, a baked emblem, a brand tag,
and four to seven registrations (`AFL_IDENTITY_PRODUCTION_LINE.md`). A runtime creator
cannot mint assets. So a player's build is not a new identity — it is a set of values on
axes the chassis already exposes.

This is what makes the creator fit without a rewrite. The `#43` selection seam
(`UAFLCosmeticLoadoutComponent` on `PlayerState`) already replicates, already survives
death-respawn via `CopyProperties`, already gates on entitlement, and already persists
through the Phase-1 backend seam. A created robot is a selection whose identity slot points
at a chassis rather than an authored identity.

**Product promise (ironics.org, live):** "There is no roster. You start from a chassis and
resolve it — finish, mask, and the parts you earn."

---

## 2 · The chassis already exists

CC-READ-2 §B2 and CC-READ-3 §3 establish that the X line is a composable chassis that was
built and never driven.

| | Original line (27 BPs) | X line (28 BPs) |
|---|---|---|
| Mesh | `SKM_Manny` | `SKM_IRONICS_Blank` |
| Override materials | 2 baked MIs | **`[]` empty** |
| Body master | `M_Mannequin` (`LogoTexture` + `UseLogo`) | `M_AFL_Character` (`BrandMaskTex`, **no logo channel**) |
| Slot-1 master | `M_Mannequin` | `M_AFL_Visor_Clean` |
| Extra component | — | **`ChestEmblemDecal`** (`UDecalComponent`) |
| Construction Script | 1 node (no-op) | **5 nodes** |

All 55 BPs parent to `AFLCharacterPartActor`; all carry `Cosmetic.Brand.*` +
`Cosmetic.AnimationStyle.Masculine` + `Cosmetic.BodyStyle.Medium`.

### 2.1 The two literals

`B_AFL_Robot_<NAME>_X` `UserConstructionScript`, identical across all 28:

1. `Construction Script` →
2. `AttachComponentToComponent` — `ChestEmblemDecal` → `MeshComponent`,
   `SocketName = spine_04`, all rules `KeepRelative` →
3. `SetMaterial` — `MeshComponent`, `ElementIndex = 1`,
   `Material = MI_AFL_IRONICS_Visor_FANATICS`

**Finding: 28 of 28 hard-code the FANATICS visor.** Material tally across the line: exactly
one distinct value.

**Finding: all 28 `MI_AFL_Branding_<NAME>` decal MICs carry an identical tint** —
`NeonColor = (1.0, 0.05, 0.05, 1.0)`, `BrandIntensity = 4.0`. Only `BrandMaskTex` varies
(28 distinct emblem masks, zero mismatches against BP identity).

**Finding: X robots carry empty override materials**, so they render the blank's defaults
(`MI_AFL_IRONICS_Body` / `MI_AFL_IRONICS_Visor`).

**Net: the X line is currently one robot, wearing one visor, in one colour, with 28
different chest logos.** Every axis intended to differentiate them is hard-coded or
uniform.

That is not a defect to repair before building the creator. **It is the creator's chassis,
already built, waiting for something to drive it.** The two literals in step 3 and in the
decal tint are precisely the values the creator supplies.

### 2.2 Resolution path

`DA_AFL_CharacterPartMap.IdentityToPart` holds **34 keys** as of `cc-0-done`: 28
`AFL.Character.*`, 5 `AFL.Team.*`, and 1 `AFL.Chassis.Creator`.

Every `AFL.Character.*` key resolves to the Original body `B_AFL_Robot_<NAME>_C`, **not** the
`_X`. `FANATICS` is the sole exception, and only because it has no Original counterpart. The 27
remaining `_X` bodies are reachable through their `AFL.Character.<NAME>_X` catalog rows but are
not targets of this resolver.

This was **not a gap to fill.** An earlier reading of CC-READ-2 §C treated "27 of 28 `_X` BPs are
not referenced by `CharacterPartMap`" as missing keys. The keys existed; they pointed at the other
line. Repointing them would have silently changed what every authored identity spawns as —
different mesh, different master, different channel names, empty override materials — and stripped
28 identities of their baked look in one edit.

**Ruling (2026-08-17): the creator gets one chassis, not 28.** CC-READ-3 §3 proved the 28 `_X`
bodies are not 28 things — identical mesh, empty overrides, the same hard-coded visor, differing
only by a baked chest decal. A creator letting players choose among 28 identical robots would be
theatre. The emblem is an axis on one chassis, per §3.4.

`B_AFL_Robot_Chassis_X` was duplicated from `B_AFL_Robot_IRONICS_X` (`cc-0-done`, commit
`58faa746`) and registered under `AFL.Chassis.Creator`. UCS parity proven node-for-node via
`read_graph`. All 28 authored `_X` bodies verified unchanged by mtime. The chassis carries
`MI_AFL_Branding_IRONICS` and the hard-coded FANATICS visor by inheritance — both expected, both
owned by CC-1.

---

## 3 · Channel inventory

CC-READ-3 §7 walked every MI under `/Game` to determine which parameters any instance
actually overrides. "Live" means driven; "inert" means declared and never touched.

### 3.1 `M_AFL_Character` — the X body master

**Live vectors (2 of 8):** `NeonColor`, `EdgeGlowColor`
**Live scalars (7 of 13):** `NeonIntensity`, `EmissiveFloor`, `AlbedoRecolor`,
`EdgeGlowMagnitude`, `BrandUVScale`, `BrandVOffset`, `BrandIntensity`
**Live textures (5 of 5):** `BaseColorTex`, `NormalTex`, `ORMTex`, `EmissiveTex`,
`BrandMaskTex`

**Inert:** `TeamColor`, `EmissiveColor`, `EmissiveColor2`, `EmissiveColor3`,
`HitPosition0`, `HitFlashColor`, `RampBoost`, `ThresholdBias`, `MaskSharpness`,
`GlowBrightness`, `HitFlashRadius`, `HitFlashStrength`

**Critical inversion:** `TeamColor` and `EmissiveColor1-3` are inert here but are *the*
colour axis on `M_Mannequin`. The two masters invert each other. Any code assuming the
`M_Mannequin` channel names will silently no-op on the X line.

Statistics: `num_samplers = 6`, `num_pixel_shader_instructions = 267`. Sampler headroom
against the platform cap is UNVERIFIED.

### 3.2 `M_AFL_Visor_Clean` — slot 1

Every channel live: `EmissiveColor`, `BaseTint`, `EmissiveStrength`, `Roughness`,
`Metallic`.

### 3.3 `M_AFL_Branding_Decal` — the emblem

Complete parameter list is three entries: `NeonColor` (vector), `BrandIntensity` (scalar),
`BrandMaskTex` (texture). `MaterialDomain = MD_DEFERRED_DECAL`,
`BlendMode = BLEND_TRANSLUCENT`.

`NeonColor` is the only colour channel and is proven variable — the five non-identity
branding decals (`C12`, `Flak`, `Railgun`, `SMG`, `Seeker`) each carry a distinct tint. The
identity decals do not use that capability.

### 3.4 The creator's channel set

| # | Channel | Writes to | Master | Status |
|---|---|---|---|---|
| 1 | Neon | `NeonColor` + `NeonIntensity` | `M_AFL_Character` | LIVE |
| 2 | Edge | `EdgeGlowColor` + `EdgeGlowMagnitude` | `M_AFL_Character` | LIVE |
| 3 | Chassis | `AlbedoRecolor` | `M_AFL_Character` | LIVE, graph-wired, undriven |
| 4 | Visor | `EmissiveColor` + `BaseTint` | `M_AFL_Visor_Clean` | LIVE |
| 5 | Emblem | `NeonColor` | `M_AFL_Branding_Decal` | LIVE |

**Four solid channels plus one graph-wired, undriven.** Not the five equal channels earlier
design assumed. `AlbedoRecolor` is registered on `M_AFL_Character` (1 of 13 scalars, default
`0.0`), graph-wired into `MaterialExpressionMultiply_16.B` alongside `Saturate_0`, matching the
wiring pattern of the functional sibling scalars. Zero code writes it — no
`SetScalarParameterValue("AlbedoRecolor")` anywhere in `Plugins/`. The plumbing is real and has no
driver; default `0.0` zeroes its own branch, so wiring a driver is visually inert until exposed.
Chassis is a real channel — CC-2.4 stops being conditional (§3.4 verified `cc-0-done`).

**Treatment scalars stay product, not player-facing.** `NeonIntensity`, `EmissiveFloor`,
`BrandIntensity` are what make a finish a *recipe* rather than a colour. They are set by
the equipped finish, never by a player slider.

---

## 4 · Data model

### 4.1 The guardrail that protects the shipped game

**The gameplay spawn path keeps reading exactly one `FAFLCosmeticSelection`, at the same
read site, with the same shape.**

Saved builds resolve *into* the active selection before anything gameplay-facing reads it.
This is what keeps the proven race-safety work — spawn, respawn, late-join, lag,
`CopyProperties` — completely untouched. Build slots are a front-end and persistence
concept only.

Every field added is additive. No shipped `CosmeticId` is renamed.

### 4.2 Channel value: id or continuum

A discrete colour SKU is an id. A continuum hue is a value. Both must serialise, and the
server must be able to tell which it is, because they have different entitlement rules.

Each colour channel carries provenance plus a resolved value:

```
FAFLChannelValue
    FName        SourceSkuId     // NAME_None when continuum
    FLinearColor ResolvedColor   // authoritative rendered value
    uint8        bContinuum : 1  // true = subscriber-authored
```

**Server validation on write:**
- `bContinuum == false` → the player must own `SourceSkuId`
  (`UAFLWalletComponent::OwnsCosmetic`, `AFLWalletComponent.h:117-120`)
- `bContinuum == true` → the player must hold the League entitlement
- **In both cases** `ResolvedColor` must pass the gamut clamp (§6.3) server-side. The
  client never decides a final colour.

Storing the resolved colour rather than a hue scalar means a lapsed subscriber's build
renders identically with no recomputation — which is what makes the freeze rule in
`IRONICS_PRICING_SSOT.md` §6.2 mechanically trivial.

### 4.3 A build

```
FAFLCreatorBuild
    FName                 BuildId
    FString               DisplayName     // profanity-filtered, see §8
    FName                 ChassisId       // AFL.Character.<NAME>_X
    FAFLChannelValue      Neon, Edge, Chassis, Visor, Emblem
    FName                 FacemaskId      // existing axis, 33 rows
    FName                 EmblemId        // existing axis, 28 rows
    FName                 FinishId        // treatment recipe
```

Saved builds live in an array on the persistence record. `ActiveBuildIndex` selects one.
Resolution writes into the existing `FAFLCosmeticSelection`.

### 4.4 Persistence

Routes through `IAFLCosmeticPersistence` — the seam purchase and earn already use
(`AFLEconomyPersistenceSubsystem.h:85-91`). No bypass, no parallel store.

**This requires new construction.** The seam today stores counted *currency* (`int32
Volts/Watts`) and a boolean owned-set (`TArray<FName> OwnedCosmeticIds`). There is no
per-cosmetic quantity and no record-shaped blob. Health packs are **not** the precedent —
they ride Lyra inventory, match-scoped, never touching this seam
(`AFLHealthPickup.h:53-60,96-98`).

---

## 5 · Flow

### 5.1 States

```
ENTRY ──▶ CHASSIS ──▶ BUILD ──▶ SAVE ──▶ EQUIP ──▶ MATCH
             │           │        │
             └───────────┴────────┘
                  free movement, no commit until SAVE
```

**ENTRY** — full-screen menu, sibling of the Digital Market, pushed via
`UCommonUIExtensions::PushContentToLayer_ForPlayer` (the `afl.Store.Open` pattern).
Empty state is an invitation: no builds yet, one action, "Start a build".

**CHASSIS** — pick the line. Original and X are genuinely different products, so this is a
first-class choice, not a hidden variant. Selecting a chassis determines which channel
schema the panel renders (§7).

**BUILD** — the four live channels plus visor, emblem, and finish. Live preview is the
spawned pawn, not a stand-in. Preview infrastructure already exists: `PreviewRT`
SceneCapture at `AFLW_LoadoutBase.cpp:481`.

**SAVE** — writes a `FAFLCreatorBuild` to a slot. Named. Slot counter always visible.
Nothing is committed to the player's active look until this point; the creator is
free-roaming until save, which makes experimentation costless.

**EQUIP** — sets `ActiveBuildIndex`, resolves into `FAFLCosmeticSelection` via
`ServerSetCosmeticSelection`. Existing path; the creator is another caller.

**MATCH** — spawn reads the active selection exactly as today.

### 5.2 Change timing

The `#43` rule (D6) already governs this and needs no new design: selection is editable
pre-match and out-of-match, **locked in-match** via `bSelectionLocked`. The creator is an
out-of-match surface by construction.

### 5.3 Preview rules

- **The preview is the product.** What the player sees is what spawns. Any divergence —
  different lighting, different pose, a stand-in mesh — reads as bait-and-switch on the
  first match load.
- **Preview at combat range.** A zoom-out state at approximate gameplay distance is
  mandatory, not a nicety. The banked lesson *distinctness is by emblem* came from exactly
  this failure: things that read at portrait distance vanish in play.
- **Every choice reversible without loss.** Undo on placement, revert-to-saved on the
  build.

---

## 6 · Colour control

### 6.1 Interaction

**Not RGB sliders.** Three RGB sliders let players make mud, which is the fastest way to
break a neon brand.

A single **hue arc** per channel with saturation and value clamped into the neon band,
plus three to four chroma stops. The player experiences continuous choice; the renderer
never receives a desaturated or near-black value.

Neon and Edge ship **linked** by default with an unlink toggle — linked gives a coherent
build in one drag, unlinked gives the two-tone looks the best authored identities use.

### 6.2 Free versus subscribed

Per `IRONICS_PRICING_SSOT.md` §7:

- **Free players** buy discrete colour SKUs — 94 `SKIN_COLOR_EDGE` rows and 45 `FINISH`
  rows keep their value as the route to a specific look.
- **Subscribers** get the continuum across the live channels.
- **Treatment is product either way** — a finish is a multi-channel recipe, not a hue.

### 6.3 The clamp is a gameplay requirement

Created configs **survive team mode** (operator ruling, 2026-08-17). This reverses the
prior `P2-MATCHTYPE-SWITCH` model in which team fully overrode body and colour.

Consequences that must be designed, not discovered:

1. **Body colour no longer carries friend-or-foe.** Team signal must move to a non-body
   channel — outline, nameplate, or a reserved edge treatment. This is a required
   deliverable, not an optional polish item.
2. **The gamut floor is a legibility floor.** Near-black and near-white builds degrade
   readability for every other player. The clamp is enforced **server-side** on write.
3. **Team override must be visible in-creator.** If any mode overrides a channel, the
   player learns that in the creator, not in their first match.

---

## 7 · Two lines, one shell

Original and X differ on mesh, master, branding channel, and component set (§2). That is a
fork in the data, not a reason for two screens.

**One creator shell; the channel schema is data-driven by which master the chassis
resolves to.**

| | Original | X |
|---|---|---|
| Colour channels | Fewer live; `M_Mannequin` names | 4 + partial; `M_AFL_Character` names |
| Branding | `LogoTexture` + `UseLogo` | `ChestEmblemDecal` + `BrandMaskTex` |
| Visor | Not supported | Slot-1 swap |
| Stickers | Not supported | Planned |

Two screens would double UI maintenance for one differing column.

**The Original line ships second.** Neutralization is systemic and incomplete — 0 of 12
sampled MIs are clean (CC-READ-3 §6). Body MIs never neutralize `EdgeGlowColor`; Limbs MIs
neutralize only `TeamColor`, leaving `EmissiveColor1-3` at master-default cyan,
`EdgeGlowColor` at default blue, and the **stock `T_UE_Logo_V2` Unreal logo** at
`Scale = 1.0`. `MI_IRONICS_*_Pink` never went through the pass at all. Until that clears,
finishes do not own colour on the Original line and a creator there would fight baked
values.

---

## 8 · Entitlement

| Fact | Shape | Precedent |
|---|---|---|
| Owns a colour SKU | Boolean | `OwnedCosmeticIds` — **exists** |
| League membership | **Conditional** | **NONE** |
| Purchased build slots | **Counted** | **NONE on this seam** |
| Max-slot upgrade | Boolean | `OwnedCosmeticIds` — **exists** |

Two of four shapes are new construction. See `IRONICS_PRICING_SSOT.md` §5.3.

**Naming collision to resolve before implementation:** in code, `LeaguePlay` is the *free,
unstaked* match tier — `IsStaked() = Tier != LeaguePlay`
(`IRONICS_LEAGUE_DOOR_SPEC.md:14`). A paid tier named "League" will collide in code, in the
front end, and in player language.

**Display names** require a profanity filter, a uniqueness rule, and a report path before
any player sees another player's build name. Not glamorous; genuinely required before
launch.

---

## 9 · Blocked axes

**Stickers — blocked on UV work.** `SKM_IRONICS_Blank` has exactly two UV sets: `UV0`
(retopo) and `UV1` (visor front-planar projection). `UV2` through `UV5` return zero
occurrences in the source FBX. Zone UVs do not exist.

Worse: **there is no `.blend` for `SKM_IRONICS_Blank`** — only
`Content/AFL/_Bridge/Blender/pending/20260723-body-IRONICS_Blank/SKM_IRONICS_Blank.fbx`
(16,281,164 bytes). Other identity bodies have conform sources; the chassis does not. Zone
UVs must be re-derived from the FBX rather than edited in a source scene.

Sticker infrastructure is categorically absent: no C++ symbol, no enum member in
`EAFLCosmeticType` or `EAFLCosmeticAxis`, no catalog rows, no textures, no placement or
zone or composite asset. The only sticker-named assets are two vendor office props.

Zone caps when built (operator ruling): **9 total** — 2 chest, 1 stomach, 1 per front leg,
1 per back leg, 1 back, 2 face. A fixed-size array of 9 is directly replicable with no
compact-reference indirection.

**Accessories — declared, unbuilt.** `EAFLCosmeticType::Accessory` exists
(`AFLCosmeticCoreTypes.h:66`); zero rows, zero consumers, no store tab.

---

## 10 · Regression guardrails

1. The `FAFLCosmeticSelection` read at spawn keeps its shape and read site. Builds resolve
   into it.
2. No edits to `ApplySkinColor`, the part-arrival hook, `CopyProperties`, or the skin push
   path during chassis phases.
3. Every field additive. No shipped `CosmeticId` renamed.
4. Any `M_AFL_Character` edit gets a before/after visual on an untouched identity — the
   same discipline that caught the baked-MI masquerade in `#38a`.
5. Commit, push `personal/main`, triple-hash verify, clean tree between phases.
6. Proof standard: demonstrated in PIE on a controllable pawn, two clients where
   replication is claimed. Never "compiles" or "looks right".

---

## 11 · Open items

| # | Item | Blocks |
|---|---|---|
| 1 | `spine_04` on `SK_Mannequin` | RESOLVED `cc-0-done` — present, bone index 5, parent `spine_03`, in a full `spine_01…spine_05` chain. Decal attach is valid. |
| 2 | Is `AlbedoRecolor` functional | RESOLVED `cc-0-done` — graph-wired, undriven. See §3.4. |
| 3 | Team-readability signal once body colour stops carrying it | Team-mode ship |
| 4 | "League" naming collision with the free `LeaguePlay` tier | Entitlement build |
| 5 | Robots and slots — one product or two (`IRONICS_PRICING_SSOT.md` §5.4) | Slot implementation |
| 6 | Original-line neutralization sweep — ~54 MIs | Original-line creator |
