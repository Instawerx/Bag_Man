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

> **CORRECTED 2026-08-19 from measurement (TASK 0).** This table previously claimed four
> channels and named `NeonColor` as the body channel. Both were wrong, and a third error was
> found while correcting them. Every claim below cites a measurement.

**The creator drives THREE colour channels**, carried as one replicated `FAFLColorOverride`
(3 x `FLinearColor` + `bValid`), server-clamped by `AFLCreatorGamut::ClampToNeon`:

| # | Creator field | Material parameter | Renders on `M_AFL_Character` (slot 0)? |
|---|---|---|---|
| 1 | `CreatorBodyColor` | `TeamColor` | **NO - graph-disconnected** |
| 2 | `CreatorEdgeColor` | `EdgeGlowColor` | YES |
| 3 | `CreatorGlowColor` | `EmissiveColor` | YES |

`CreatorBodyColor` also drives `BaseTint` on slot 1 (CC-2.2, `8ca41998`) - the same value
reaching a second parameter, not a fourth channel.

**Written is not rendered.** A dedicated-server two-client run emits exactly three colour keys
per MID - `key=TeamColor`, `key=EdgeGlowColor`, `key=EmissiveColor`, 172 writes each
(`cc-2-colour-core-done` @ `a9222049`). That proves the values are WRITTEN. It does not prove
they render, and for one of them they do not.

**`NeonColor` is not written by the creator at all**, so the earlier claim that CC-2.1 drives
"NeonColor + EdgeGlowColor" is contradicted by every emit in that run.

#### Graph-connectivity audit of `M_AFL_Character` (T3D export, consumers per parameter)

| Vector parameter | Downstream consumers | Verdict |
|---|---|---|
| `NeonColor` (7 nodes) | 1 | LIVE - and the actual body colour source |
| `EmissiveColor` | 1 | LIVE |
| `EdgeGlowColor` | 1 | LIVE |
| `TeamColor` | **0** | INERT - declared, never consumed |
| `EmissiveColor2` | **0** | INERT |
| `EmissiveColor3` | **0** | INERT |

Counted by classifying every reference to each `MaterialExpressionVectorParameter` as
self-declaration, `ExpressionCollection` listing, or a real `B=(Expression=...)` consumer.
`TeamColor`, `EmissiveColor2` and `EmissiveColor3` have only the first two.

**Consequence, and it is a design question not a bug:** the creator's *body* colour writes to a
parameter the chassis master does not consume, so **choosing a body colour does not tint the
chassis body**. It has effect only where the bound master consumes `TeamColor` - `M_Mannequin`
on slot 1 - and via `BaseTint` on the visor masters. Edge and glow render on the body; body
does not. This corroborates the standing note in `IRONICS_PRICING_SSOT.md` that `TeamColor` is
inert on `M_AFL_Character`, and the in-code observation at `AFLCharacterPartActor.cpp:251` that
`EmissiveColor2/3` "were all ruled out by probe". **Unresolved - needs a ruling.**

**`AlbedoRecolor` is NOT a channel - it is a treatment scalar.** It is a
`MaterialExpressionScalarParameter` (`ScalarParameter_13`, no `DefaultValue` line, so `0.0`)
whose sole consumer is `Multiply_16.B`, feeding `LinearInterpolate_0.Alpha`:

```
BaseColor <- LinearInterpolate_0
               A     = BaseColorTex
               B     = Desaturation_0 x NeonColor
               Alpha = Saturate_0 x AlbedoRecolor
```

It carries no colour of its own; it is a blend weight fading the albedo toward a desaturated
tint sourced from `NeonColor` - which the creator does not drive. Raising it today tints the
chassis toward `NeonColor`'s default `(0.5, 0.04, 1.0)`, a fixed purple, not toward the
player's choice. **CC-2.4 is RECLASSIFIED, not implemented**: exposing it needs a material
decision (retarget `Multiply_15.B`, or drive `NeonColor`) plus a scalar write path the
vector-only overlay does not have.

**`EmissiveColor2/3` are a separate axis, not a reduction of the count.** They are held neutral
on unique bodies by the `bUniqueBodyUVs` guard (`AFLCharacterPartActor.cpp:279`) under an
operator ruling, and they are independently graph-disconnected (table above). They were never
among the four this section claimed, so suppressing them does not explain four becoming three.

### 3.4.1 Channel coverage is MASTER-DEPENDENT

A creator channel reaches a slot only if that slot's bound master exposes AND consumes the
parameter. Parameter *names* were enumerated from T3D exports - a value read cannot distinguish
absent from black, which is why `M_Mannequin`'s `BaseTint` reading `(0,0,0)` proved nothing
until the names were listed. Both visor masters served as controls.

| Slot-1 master | Facemask presets binding it | Exposes | Creator channels reaching it |
|---|---|---|---|
| `M_AFL_Visor_Clean` | 28 | `BaseTint`, `EmissiveColor` | Body (via `BaseTint`), Glow |
| `M_AFL_FaceMask_Visor` | 0 (mesh default, unequipped state) | `BaseTint`, `EmissiveColor` | Body (via `BaseTint`), Glow |
| `M_Mannequin` | 32 | `CarbonfiberTint`, `EmissiveColor`, `EmissiveColor2/3`, `TeamColor` | Body (via `TeamColor`), Glow |

**`M_Mannequin` exposes neither `BaseTint` nor `EdgeGlowColor`**, so the 32 facemask presets
binding it receive no creator edge colour and no visor base tint. `SetVectorParameterValue` on
an absent parameter is ignored by design - no error, and no instrument reports it: `WROTEKEYS`
records the call, never the receipt.

This is a pre-existing CC-2.1 limit, not something CC-2.2 introduced, and it is why the stage's
"four visibly different channels each" proof criterion was never achievable as written.

### 3.4.2 Facemask axis - RESOLVED

The facemask axis was **33 of 60 live**: 27 `AFL.Facemask.*` catalog rows carried
`Type = SkinColor_Edge`, so every type-driven consumer (locker, browser, store) excluded them,
while the `AFL.Edge.` prefix narrowing kept them out of the Edge tab as well - invisible on
every surface. **Retyped in CC-X16 (`1f842979`, tag `cc-x16-done`); the axis is now 60 of 60**,
verified on a fresh editor load reading the catalog off disk: `FACEMASKTYPES rows=60
Facemask=60`, edge axis unchanged at 40.

Root cause: `FAFLCatalogEntry::Type` **defaults to `SkinColor_Edge`**, a real and wrong value,
so any row authored without setting `Type` is silently absorbed. The default is unchanged -
see CC-X17.

### 3.4.3 KNOWN STATE - the `_X` identity rows (CC-X18)

27 of 56 `AFL.Character.*` catalog rows - the entire `_X` line (`ARIA_X`, `AKUMA_X`, `ASTRA_X`,
...) - carry the same `Type = SkinColor_Edge` default and are likewise invisible to the
type-driven character picker. **This is documented state, not a defect to fix.** The pivot
retires identity SKUs: the store sells no characters, the creator replaces them, and the roster
cut keeps six identities. Retyping would surface 27 identities the design is removing. All 27
are bundle-coupled and their bundles carry `bTransactable=false`.

Their invisibility currently matches the intended end state **by accident**, and is held there
only by the `AFL.Edge.` prefix narrowing in `AFLCosmeticBrowserLibrary.cpp:99`. That line is
load-bearing for two separate reasons and is commented as such.

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

**BUILD** — the three creator channels (body/edge/glow) plus visor, emblem, and finish. Live preview is the
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
