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

> **COLUMN SPLIT 2026-08-20 (CC-X24).** This table used one column headed "Verdict" whose values
> read `LIVE`. `LIVE` meant *has a consumer* - it did NOT mean *carries signal to an output*. That
> single word caused a wrong ruling: `NeonColor` was read as a working body-colour source and a
> re-point of the body channel onto it was authorised, when its only path to `BaseColor` is gated by
> a weight measured at zero. A column heading is an instrument, and an ambiguous one produces
> confident wrong readings at every future use. The two questions are now separate columns.

| Vector parameter | Consumers | Connected? | Carries signal? |
|---|---|---|---|
| `NeonColor` (7 nodes) | 1 | YES - `Desaturation_0` -> `LinearInterpolate_0.B` | **NO** - that lerp's `Alpha` is `Saturate_0 x AlbedoRecolor`, and `AlbedoRecolor` measures **0.0**, so `B` contributes nothing |
| `EmissiveColor` | 1 | YES | YES |
| `EdgeGlowColor` | 1 | YES | YES |
| `TeamColor` | **0** | **NO** - declared, never consumed | NO |
| `EmissiveColor2` | **0** | **NO** | NO |
| `EmissiveColor3` | **0** | **NO** | NO |

**Measured 2026-08-20:** `get_material_default_scalar_parameter_value(M_AFL_Character,
AlbedoRecolor)` returns `0.0`. So `NeonColor` is connected-but-silent, which is a THIRD state and
not the same as either `EdgeGlowColor` (connected and carrying) or `TeamColor` (never connected).

Counted by classifying every reference to each `MaterialExpressionVectorParameter` as
self-declaration, `ExpressionCollection` listing, or a real `B=(Expression=...)` consumer.
`TeamColor`, `EmissiveColor2` and `EmissiveColor3` have only the first two.

**Consequence:** the creator's *body* colour writes to a parameter the chassis master does not
consume, so **choosing a body colour does not tint the chassis body**. It has effect only where the
bound master consumes `TeamColor` - `M_Mannequin` on slot 1 - and via `BaseTint` on the visor
masters. Edge and glow render on the body; body does not. This corroborates the standing note in
`IRONICS_PRICING_SSOT.md` and the in-code observation at `AFLCharacterPartActor.cpp:251` that
`EmissiveColor2/3` "were all ruled out by probe".

**RULED 2026-08-20 (CC-X24): body colour is DISABLED on the X-line chassis** - shown, not hidden,
with the reason. `FAFLCreatorChannelSchema` reports it as `PresentButInert` rather than collapsing
it into "unavailable", because hiding the control would make an absent channel indistinguishable
from one that was never designed. **The X-line offers TWO creator channels: edge and glow.**
Measured: `DERIVED master=M_AFL_Character body=0 edge=1 glow=1 visor=0 count=2 audited=1`, against
`M_Mannequin body=1 ... count=3` - the same `TeamColor` reading `found=1` on both and yielding
opposite verdicts, because inertness is keyed on the (master, parameter) pair.

**Two remedies were considered and rejected**, recorded so they are not re-proposed as new ideas:
re-pointing body onto `NeonColor` (rejected - it is connected-but-silent, so this moves from one
inert parameter to another), and adding a scalar axis to the overlay to drive `AlbedoRecolor`
(rejected as a phase, not a fix - `FAFLColorOverride` is vector-only, so this is a replicated
struct change plus clamp plus apply path plus a default-weight decision). The material retarget
that WOULD restore the channel is logged as **CC-X25** in section 11.

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
| `M_Mannequin` | 32 | `CarbonfiberTint`, **`EdgeGlowColor`**, `EmissiveColor`, `EmissiveColor2/3`, `HitPosition0`, `TeamColor` | Body (via `TeamColor`), **Edge**, Glow |

> **CORRECTED 2026-08-19.** An earlier revision of this row claimed `M_Mannequin` exposes
> **neither** `BaseTint` **nor** `EdgeGlowColor`. The second half was **wrong**, and the error is
> instructive enough to record rather than quietly fix.

**`M_Mannequin` exposes `EdgeGlowColor`. Only `BaseTint` is absent.** Enumerating the master's
vector parameters returns seven: `CarbonfiberTint`, `EdgeGlowColor`, `EmissiveColor`,
`EmissiveColor2`, `EmissiveColor3`, `HitPosition0`, `TeamColor`.

**Practical consequence, corrected:** the 32 facemask presets binding `M_Mannequin` **do** receive
the creator's edge colour. They receive body colour (via `TeamColor`), edge, and glow - three of
three. The only channel that does not reach them is the visor **base tint**, because `BaseTint`
genuinely is absent (0 occurrences anywhere in the master's T3D export, including inside its 36
material-function calls). Creator coverage on those presets is **better** than this document
previously recorded, not worse.

**How the wrong claim happened, and the rule it produced.** Two lanes disagreed: CC-READ-2 §F1
enumerated the parameter names and reported seven including `EdgeGlowColor`; CC-X12 parsed the T3D
export counting `MaterialExpressionVectorParameter` nodes and reported five, missing it. CC-READ-2
was right. The node-regex parse could only see parameters whose declaring node it managed to map
back to a class line, and it silently dropped one. A whole-file string search was run for
`BaseTint` (correctly finding zero) but **not** symmetrically for `EdgeGlowColor` - a gap in
coverage, not in method.

**THE RULE: when the question is EXISTENCE, use an API that can return "not found."** Enumeration
(`GetAllVectorParameterInfo`, `get_vector_parameter_names`) answers it. A value lookup that
manufactures a default cannot - `MaterialEditingLibrary.get_material_default_vector_parameter_value`
was measured returning `PRESENT (0,0,0)` for `BaseTint` and `NeonColor` on this very master, where
both are genuinely absent. Names are provenance; values are not. (C++
`UMaterialInterface::GetVectorParameterValue` *does* honour a found-flag - it returns `false` when
`GetParameterValue` fails, `MaterialInterface.cpp:841-850` - so it is safe; its Python-helper
sibling is not. Conflating the two is what produced this correction.)

The `BaseTint` limit remains a pre-existing CC-2.1 scope note, not something CC-2.2 introduced.

### 3.4.2 Facemask axis - RESOLVED

The facemask axis was **33 of 60 live**: 27 `AFL.Facemask.*` catalog rows carried
`Type = SkinColor_Edge`, so every type-driven consumer (locker, browser, store) excluded them,
while the `AFL.Edge.` prefix narrowing kept them out of the Edge tab as well - invisible on
every surface. **Retyped in CC-X16 (`1f842979`, tag `cc-x16-done`).**

**CORRECTED 2026-08-20 - the axis is 38 of 38, not 60 of 60.** Measured on a fresh editor load
reading the catalog off disk: `AFL_TEST[DOCS] FACEMASK prefix 'AFL.Facemask.' = 38 rows | typed
correctly = 38 | rows carrying that Type anywhere = 38`, corroborated in the same run by
`AFL_TEST[FACEMASKTYPES] rows=38 Facemask=38`.

**The axis did not lose typing - it lost ROWS.** All 38 that remain are correctly typed, which is
what "resolved" meant and still means. The count fell by exactly 22 between the CC-X16 reading and
this one, and CC-6.3 (`82fac4d7`) retired 22 identities in between. That arithmetic is CONSISTENT
with the retirement having taken 22 facemask rows with it; it is **not verified** - the catalog is
a binary `.uasset`, so the deletion diff cannot be read row-wise - and it is recorded here as an
open correspondence rather than a cause.

**The edge axis is likewise not 40.** Measured: `AFL.Edge.` prefix = **37 rows**, while **42** rows
carry `Type = SkinColor_Edge`. The 5-row difference is not a discrepancy: it is exactly the 5
`AFL.Character.*` rows described in §3.4.3, which carry the edge type under a character id. 37 + 5
= 42, and the two readings agree.

Root cause, and **CLOSED**: `FAFLCatalogEntry::Type` used to default to `SkinColor_Edge`, a real and
wrong value, so any row authored without setting `Type` was silently absorbed. **The default is now
`Invalid`** (`4eb4e1c9`, verified an ancestor of HEAD). Both halves of CC-X17 have shipped - the
`CreatorSlot` enumerator appended after `Invalid` so nothing renumbers, and the default changed,
which is the half that actually closes the trap. Verified still closed 2026-08-20, AFTER CC-6.3's
deletions and CC-4.2's three new SKUs: `AFL_TEST[TYPELINT] checked=427 mismatch=15 unmapped=50
invalid=0` and `AFL_TEST[DOCS] INVALID = 0`. No row rides the default.

### 3.4.3 KNOWN STATE - the `_X` identity rows (CC-X18)

**CORRECTED 2026-08-20: 5 of 12, not 27 of 56.** Measured: `AFL_TEST[DOCS] CHARACTER prefix
'AFL.Character.' = 12 rows | typed correctly = 7`, with the off-type remainder reported as
`{'SKIN_COLOR_EDGE': 5}`. Independently corroborated the same run by
`AFL_TEST[TYPELINT] ... AFL.Character.=5`. Both figures in the original claim are stale: the
prefix holds 12 rows, not 56, and 5 of them ride the old `SkinColor_Edge` default, not 27.

Both numbers moved for the same reason the facemask count did - CC-6.3 (`82fac4d7`) retired 22
identities and CC-6.4 re-typed the colour registry. The DIRECTION of the original finding is
unchanged: some `AFL.Character.*` rows carry the edge type and are invisible to the type-driven
character picker.

**This remains documented state, not a defect to fix.** The pivot retires identity SKUs: the store
sells no characters, the creator replaces them, and the roster cut keeps six identities. Retyping
would surface identities the design is removing. Their bundles carry `bTransactable=false`.

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
    FName                 FacemaskId      // existing axis, 38 rows (measured 2026-08-20)
    FName                 EmblemId        // existing axis, 6 rows  (measured 2026-08-20)
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
| 5 | Robots and slots — one product or two (`IRONICS_PRICING_SSOT.md` §5.4) | **RESOLVED `cc-4-2-done`** — ONE mechanism, measured. `AFL.CreatorSlot.x1/.x3/.x8` all carry the same `CountedKey` and accumulate into one counter: baseline 0 → x3 → 3 → x3 again → 6 → x8 → 14. The second x3 is the decisive arm; a boolean entitlement would have sat at 3 and the player would have paid twice for one slot. Cap is a parameter (`AFLResolveEffectiveSlotCap`, no literal in the resolver): 2/5/5/10/10/2. |
| 8 | **CC-X17** — `FAFLCatalogEntry::Type` defaulting to a real-and-wrong value | **RESOLVED** — default is now `Invalid` (`4eb4e1c9`). Verified still closed 2026-08-20 after CC-6.3's deletions and CC-4.2's new SKUs: `TYPELINT invalid=0`, `DOCS INVALID = 0`. See §3.4.2. |
| 9 | **CC-X22** — UE catalog rows with no PlayFab manifest entry | **SCOPED, DELIBERATELY UNREGISTERED.** Measured 2026-08-20: 427 catalog rows, 11 manifest items, 9 overlapping, so the gap is **263** priced-and-`Direct` rows — reproducing the independently-known "9 of 272". By prefix: Weapon 108, WeaponSkin 43, Beam 43, Finish 27, Facemask 21, Body 10, Edge 5, Bundle 5, Ability 1. **Weapons are 151 of 263 by Type**, so the ruling is mostly "are weapon cosmetics sold for real money" — product intent, not an engineering call. The 155 `GRANTED_FREE` rows are unpriced and correctly absent; priced and `Direct` coincide exactly in the data. Registering nothing until that intent is ruled.<br><br>**STORE-SURFACE READ 2026-08-20 — NOT INERT, IT IS LIVE.** `GetPurchasableEntries` filters on **`Acquisition != GrantedFree` and nothing else**; the game **never** calls `GetCatalogItems`/`GetStoreItems`, so it cannot know what PlayFab holds (the only `CatalogVersion` uses are in `PurchaseItem` request bodies — buying, not listing). `AFLW_FrontEndMarket` then filters by axis + not-owned, sets `bPurchasable=true`, and **BUY is fully wired** at `AFLW_FrontEndMarket.cpp:476` → `Wallet->ClientRequestPurchase`. So a player can press BUY on a row PlayFab has never heard of. **9 of 272 purchasable rows are registered — ~97% of store BUYs would fail.**<br>**Money is safe: the path fails CLOSED.** On `!bOk` the callback logs `REJECTED … pfid=` and returns *before* `ApplyPurchaseResult`, so nothing is deducted, granted or persisted. This is a UX/trust defect, not a theft defect.<br>**The fix is NOT bulk registration** (ruled out). It needs a way for the store to know what is transactable, and that choice is unruled — see item 18. |
| 10 | **CC-X23** — wallet mirror loaded once at BeginPlay, never re-read | **DONE `cc-x23-done`** with a stated exclusion. Mechanism proven: a mirror poisoned to 323,635 corrected back to PlayFab's authoritative 200,179 through the shipping path. `ShouldUsePlayFab()` now tests `IsLoggedIn()` rather than object existence. **The `OnLoggedIn` delegate branch is NOT exercised in PIE** — the dev CustomID login always resolves before wallet BeginPlay (all 8 wallets logged "already logged in at BeginPlay -> no subscription needed"), and shipping's EOS/OIDC path is not reproducible here. A login-delay harness was considered and rejected: it would prove the delegate fires when delayed, not that it fires in shipping. |
| 11 | **`SkinColor_Body` has ZERO catalog rows** | Measured 2026-08-20 — the Type histogram sums to exactly 427 and contains no `SKIN_COLOR_BODY` entry. The 10 `AFL.Body.*` rows carry `Type = Finish`, which `TYPELINT` independently flags (`AFL.Body.=10`, part of `mismatch=15`). Consistent with §3.4.1: body colour is `PresentButInert` on the X-line master, so there has been nothing for a body-colour row to drive. Coupled to CC-X25 — if the retarget lands, this axis needs rows. Recorded as known state, not scheduled. |
| 18 | **CC-X22-FIX — how does the store learn what is transactable?** | **UNRULED, ASKED.** Bulk registration is ruled out, so the store must stop offering rows PlayFab cannot sell. Two shapes, and the choice is the operator's: **(a)** a catalog property (e.g. `bTransactable`) maintained by the same lint that already checks price drift — cheap, but it is DATA and data drifts, and a stale `true` reproduces exactly this defect; **(b)** query PlayFab `GetCatalogItems` once at startup and intersect — self-correcting and cannot go stale, but it is a new online dependency on the front end and needs a defined offline behaviour (show nothing? show all? show cached?). **RULED 2026-08-20: (b) — `GetCatalogItems` intersect at startup, show CACHED offline.** The store learns what is transactable by asking PlayFab, not by trusting a local flag, so it cannot go stale. Offline behaviour is defined: show the cached intersect rather than nothing (a player offline sees the store they saw last) and never show all. Scheduled work, not yet built. |
| 12 | **CC-X20** — type-lint residue | `TYPELINT checked=427 mismatch=15 (AFL.Character.=5, AFL.Body.=10) unmapped=50`, none in the lint's known-positives list. **No `AFL.Body` row was touched by the roster cut, so these predate CC-6.3.** OPEN: determine what the 50 unmapped are, and whether the 10 `AFL.Body.` mismatches are real. |
| 13 | **CC-X21** — inert-but-exposed catalog fields | `MintCap`, `Tier`, `ContentTier`, `bIntactOnlyBundle` are inert in C++ yet `BlueprintReadOnly`, so a Blueprint could be reading them. **Cosmetic risk only — price is server-read on BOTH purchase paths.** OPEN: Find-in-Blueprints for `MintCap`, and Ctrl+F it in `AFLW_Menu_CosmeticShop`. Fifteen seconds while an editor is open. |
| 14 | **CC-X26** — preview/match lighting parity | The preview uses `PRM_UseShowOnlyList` with atmospherics off against a flat backdrop, so it renders the pawn in isolation. Parity would turn the preview's scale check into the **legibility** check §5.3 describes. **Gates nothing.** |
| 15 | **CC-X27** — packaged loadout reverted while the store stayed current | Operator notes the loadout was changing anyway. **Lowest priority, likely obsolete.** OPEN only to confirm or close. |
| 16 | **Weapon credits** — `AFL.WeaponCredit.x3` at $0.99, `CountedKey=AFL.WeaponCredit`, `GrantQuantity=3` | Redeemable pool ruled at **194 rows**: `AFL.Weapon.*` (108), `AFL.WeaponSkin.*` (43), `AFL.Beam.*` (43). Hand cannons are **premium and excluded**, and must be excluded **by a property, not by name matching** — name matching has nearly destroyed the wrong asset four times this programme. Conforms to the `AFL.CreatorSlot.x3` template proven by `cc-join-done`.<br><br>**MEASURED 2026-08-20 — BLOCKED, TWO UNRULED QUESTIONS.**<br>*(i) No property excludes hand cannons.* Every candidate discriminator OVERLAPS: `ContentTier` (all 147 HC are `Base`; the only 6 `Premium` rows are **not** hand cannons), `Tier` (all HC `SPARK`), `Rarity` (all HC `Common`), `bTransactable` (all `True`), `CollectionId` (all `None`). Inferring `Premium ⇒ hand cannon` would have excluded 6 wrong rows and left all 147 hand cannons redeemable at a third of a credit. The property must be **authored**; it does not exist.<br>*(ii) The ruled 194 is confirmed but contains hand cannons.* 275 rows carry the three prefixes; priced+`Direct`+unregistered = **194** exactly as ruled — **but 105 of those 194 are hand cannons**. Excluding them leaves **89**: `AFL.Weapon` **3**, `AFL.WeaponSkin` 43, `AFL.Beam` 43. So a weapon credit would redeem from a pool containing **three actual weapons**. **SUPERSEDED — see the POOL READ below.**<br><br>**POOL READ 2026-08-20, linked by `ItemDefClass` (never by name).**<br>*The roster:* **218 `ID_AFL_*` item definitions** in `Plugins/GameFeatures/AFLBagMan/Content/Equipment/` — 150 hand cannon, **68 non-hand-cannon weapons**. (The `B_AFL_*` assets under `Content/BagMan/Equipment` are the 160 weapon actors; the `ID_AFL_*` item-defs are what a catalog row grants.)<br>*The gap:* of the 68 non-HC weapons, **29 have a catalog row** and **39 have none at all**. Of those 29 rows, only **3** are priced+`Direct`. So the weapon roster is 68 and exactly **3 are purchasable today**.<br>*Skins and beams:* all 50 `AFL.WeaponSkin.*` and all 49 `AFL.Beam.*` rows carry **no `ItemDefClass`** — expected, not a defect: they are cosmetic modifiers applied to a weapon, not equipment item-defs.<br>*Hand cannons:* **49 identities = 7 guns × 7 colourways** (DRAGON_SOUL, FANATICS2, FUTURE_WARRIOR, IRONICS, JAGUAR_NEON, RUN_IT_BACK, SIMULARENT), each with an `_L` and an `_R` Blueprint, skeletal mesh, skeleton and physics asset. 98 HC actor BPs split exactly 49 `_L` / 49 `_R`.<br>**VARIANT QUESTION RULED: `.L` and base are INDEPENDENTLY EQUIPPABLE, not facets.** Decided by separate BP classes + separate skeletal meshes/skeletons/physics assets + the dual-equip design (`RefreshHandCannonsForPawn`, "hold two, don't unequip"). **A credit redeems a ROW, and a row is ONE ARM.** This matches the ruling that both are usable and either is usable alone.<br>**A PAIR = base row (R) + `.L` row (L) of the same identity.** Measured, not assumed.<br>**⛔ THE "49 ORPHAN `.XT` ROWS" CLAIM IS WITHDRAWN — IT WAS WRONG, TWICE OVER.** Corrected 2026-08-20 by reading the ROW instead of the filesystem. `.XT` is a **third equippable variant** with its own item definition: `ItemDefClass = /AFLBagMan/Equipment/ID_AFL_HandCannon_<GUN>_<Colour>_XT`, and **49 such `ID_AFL_*_XT` assets exist** — matching `_L` 49 and `_R` 49 exactly, so 49 identities × 3 defs = the 147 rows. It is `Type=Weapon`, `Direct`, priced 990 VO / 9900 WA identically to base and `.L`, DisplayName "… Hand Cannon **XT** — Neon Blue".<br>**HOW THE CLAIM WAS WRONG:** (1) I searched `Content/BagMan/Equipment` for `_XT`; item definitions live in `Plugins/GameFeatures/AFLBagMan/Content/Equipment/`. Wrong directory. (2) I later *discovered* the right directory while counting 218 `ID_AFL_*` defs — and did not revisit the conclusion the first search had produced. A wrong search, then evidence that should have invalidated it, and no return to the claim.<br>**AND `.XT` IS NOT THE PAIR:** `ContainedEntitlementIds` is EMPTY on all 49 — indeed on all 147 hand cannon rows. **No pair row exists in the catalog today.**<br>⚠ Minor: FANATICS2's meshes are misfiled at `HandCannon/L/` and `HandCannon/R/` rather than `HandCannon/FANATICS2/`. || 20 | **READ A — the sponsor discriminator IS `Acquisition`, and one family contradicts it** | Measured 2026-08-20, families derived from `ItemDefClass` (never the id string). Every family is **internally uniform** — 100% one `Acquisition` value each:<br>`DRAGON_SOUL / FUTURE_WARRIOR / JAGUAR_NEON / RUN_IT_BACK` — 21 rows each, `Direct`, 990 VO ✓ sold<br>`FANATICS` 3 + `FANATICS2` 18 — `GrantedFree`, 0 VO ✓ free<br>`IRONICS` 21 — `GrantedFree`, 0 VO ✓ **house line confirmed already free**<br>`SIMULARENT` 21 — **`Direct`, 990 VO ✗ RULED FREE BUT PRICED — DEFECT, reported not fixed**<br>**CONTROL PASSES:** `Acquisition` classifies none of the four sold families as free, so its only error is in the free direction. Every other candidate (`Type`, `Tier`, `ContentTier`, `Rarity`, `CollectionId`, `SeriesName`, `bTransactable`, `MintCap`, `bIntactOnlyBundle`, `VisualIntensity`, `GlowImpact`) is uniform across sold and free and discriminates nothing. **No marker needs authoring** — `Acquisition` is the discriminator once SIMULARENT is ruled on. |
| 21 | **`FANATICS` / `FANATICS2` — one family split across two item-def stems** | 3 rows resolve to `ID_AFL_HandCannon_FANATICS_*`, 18 to `FANATICS2_*`; together 21, one family's worth. The ruling names `FANATICS`. Reported, not resolved — and invisible to any name-based read, which is why families were derived from `ItemDefClass`. |
| 22 | **READ B — there is NO catalog-side "reserved" concept** | Measured: no field on `FAFLCatalogEntry` contains `reserv` in any case. `reserved` exists **only** in the mint ledger's `ChildrenJson` `{id, reserved}` and means *a child not yet built or not yet in the PlayFab catalog*. For hand cannons all three ids resolve to real `ID_AFL_*` item defs, so none would be reserved. **No category to fold into sponsor, and none invented.** |
| 23 | **CC-X28 — "free pairs are not granted"** | **WITHDRAWN 2026-08-20. It was my probe, not the product.** Measured: `ARM4 free OWNED kid0=0 kid1=0` **and** `ARM4b free ENTITLED kid0=1 kid1=1`. `IsEntitled` returns `bGrantedFree \|\| OwnedCosmeticIds.Contains(id)` — it honours the catalog's `GrantedFree` assertion, so a sponsor item is entitled and **never needs delivering**; an empty owned SET is by design. I raised the item by reading `OwnsCosmetic` when the gate is `IsEntitled`, and reported a working design as a defect. Scope does not widen either: `IsEntitled` is *the* gate for every axis, so "asserted everywhere, delivered nowhere" is the uniform intended design. |
| 24 | **CC-X29 — an owned pair can be re-bought indefinitely, for nothing** | **MEASURED 2026-08-20, PRODUCT INTENT, not decided.** Second purchase of `DRAGONSOUL.XT` while already owned: `mintNo=2`, `vo 188709 → 187219` (charged 1490 again), `owned 13 → 15` — PlayFab granted two more instances. Routing, ledger, deduct and grant are all correct; **nothing refuses a purchase of something already owned**, up to the 1,000,000 cap. For `AFL.CreatorSlot.x3` that shape is intentional (more slots is more slots); for a hand cannon pair the second purchase buys nothing the player can use. Options are refuse server-side, grey out in the store, or leave re-buyable — all product calls. |
| 19 | **BUNDLE PURCHASE SEAM — the game cannot reach the grant** | **MEASURED 2026-08-20. BLOCKS the 49 hand-cannon pair bundles.** The grant mechanism EXISTS and is good: `lambda/purchase-bundle` does atomic MINT → DEDUCT → atomic GRANT of all children with REFUND-on-fail, server-authoritative, reading price/children/cap from the **mint-ledger row** (`bagman-bundle-mint-ledger`, `ChildrenJson`) and never from the request. **But the game has NO path to it** — zero references to `/purchase-bundle` in any `.cpp`, `.h` or `.ini`, and `EAFLCosmeticType::Bundle` appears in exactly one place in the codebase: a lint table in the cheats file. Nothing in the purchase path branches on Bundle.<br>So a bundle bought in-game routes `ClientRequestPurchase → PurchaseThroughBackend → PlayFab PurchaseItem`, which grants **the bundle id and nothing else**. Authoring the 49 pair rows now would ship *pay $1.49, receive no arms* — the exact shape `cc-join-done` closed, one phase later.<br>**Note the UE field is a red herring**: `ContainedEntitlementIds` has no C++ consumer either, but it is not what drives the grant — the ledger is. Filling it changes nothing by itself.<br>**Routing Bundle purchases to the bundle endpoint IS new code**, which is a scope change on the ruling rather than a question about it. Not built. |
| 17 | **Hand cannon sets** — $1.49, premium, outside the credit pool | Straight `AFL.Bundle.*` with `ContainedEntitlementIds`, the mechanism the identity bundles already proved. No credits, no new code. Rows plus manifest entries. |
| 6 | Original-line neutralization sweep — ~54 MIs | Original-line creator |
| 7 | **CC-X25** — material retarget: rewire `Multiply_15.B` on `M_AFL_Character` so `NeonColor` reaches `BaseColor` ungated by `AlbedoRecolor`. **Restores body colour on the X-line** (returns it to three channels). Cost: content change to the master every X body binds, so it alters the shipped look of every X-line robot and needs a regression check on an untouched identity. Real future capability, not a dead end. **RULED 2026-08-20: DO NOT REWIRE.** Highest-risk class — it moves every X-line robot's shipped look through a shared mesh binding (`M_AFL_Character` → one direct child `MI_AFL_IRONICS_Body` → 4 variants, bound on `SKM_IRONICS_Blank`'s material slots) and partially reverses CC-X24. Stays here as **available future capability**, not scheduled work. | X-line body colour |
