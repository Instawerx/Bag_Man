# AFL Color -> One-Registry Migration Plan

**Status: PLAN -- not started.** Each phase proven ON SCREEN (PIE watch of a registry
color on that surface) before the next. "Proven" = seen, never log-asserted.

**Known state (2026-06-25):** Beam tint is UNSOLVED. It is Phase 1 / BEAMS step (a) --
the `[TINTTRACE]` HOP1-6 instrumentation is IN CODE (`AFLBeamVisualComponent.cpp` +
`AFLCombatCheats.cpp`), awaiting the editor-closed rebuild + ONE PIE watch that finds
the hop where the color value dies. NOTHING color-related has shipped on any surface --
do not read any surface (beam included) as "done."

---

## Goal architecture

**SCOPE (corrected 2026-06-26):** this plan is the **PALETTE/COLOR axis ONLY** -- the free,
always-on, registry-driven color *attribute* that overlays certain products. It is NOT the
PRODUCT/CATALOG axis (the SKU economy). The two axes are distinct (confirmed from the SSOTs +
`FAFLCatalogEntry`, where `ColorIdentityTag` is ONE field alongside `Type`/`Acquisition`/price/
`RarityTag`/`MintCap`/`bTradeable`/`Asset`):

- **PALETTE/COLOR axis = THIS plan.** One registry (`DA_AFL_ColorIdentityRegistry`), free,
  swappable, no entitlement, no `OwnedCosmeticIds` write. Applies ONLY where color is a FREE
  attribute: **WEAPONS** (`AccentColor` -- proven), **BEAMS** (`User.Color` -- proven), **SKINS
  body palette** (the always-on `AFL.Finish.*` -- TBD). A SKU's `ColorIdentityTag` resolves here.
- **PRODUCT/CATALOG axis = NOT this plan** (separate Store/economy workstream). The named
  entitled SKUs (`FAFLCatalogEntry`: `Type`, `Acquisition`, price-rung, `MintCap`, `bTradeable`,
  `Asset`), one base free + rest priced/minted/tradeable per the pricing SSOT. The product unit
  is the named DESIGN. **VISORS and LOGO EMBLEMS live here** -- design catalogs whose color is
  INTRINSIC to the design, not a free registry axis.

For the palette-axis surfaces, color lives in ONE place; each resolves `ColorIdentityTag ->
registry -> {Primary, Accent}` and drives its sink from that value.
- Adding a palette color = ONE registry row + ONE ini tag. No palette-axis surface stores a color.

The audit (this session) proved the opposite today: palette color is FIXED PER-SURFACE, stored
in SIX places, the registry PARTIAL/BYPASSED. This plan converges the **3 palette-axis surfaces**
(weapons + beams done; skins body TBD). Visors/emblems are out of scope -- see section 3.

### What the audit found (the baseline this plan corrects)

| Surface | Reads registry? | Own baked colors | Proven on screen | Cost to add 1 color today |
|---|---|---|---|---|
| SKINS (body) | NO -- baked presets | ~38 `UAFLSkinColorAsset` (6 Edge + 32 Finish) | YES | +1 `DA_AFL_Finish_*` (+ catalog row) |
| VISORS (facemask) | NO -- baked MICs | 32 `MI_AFL_FaceMask_*` + 32 DA + 32 texture | YES | +3 assets + catalog row + tag |
| WEAPONS (body) | NO -- baked AccentColor MIs (registry->BrandColor is cheat-only) | 5 per-color Carbine MIs | NO | +1 MI per weapon-type per color |
| BEAMS / FX | NO authored -- baked `LaserTintColor` (registry path cheat-only) | 3 per-weapon `LaserTintColor` defaults | NO | per-weapon `LaserTintColor` x N |

Registry today: NeonBlue, NeonGreen, NeonPink, NeonPurple, NeonRed (+ EMP,
IronicsVisor, NeonEdge specials). Color stored in 6 places: registry, ~38 skin
presets, 32 visor MICs, 5+ weapon MIs, 3 beam BP defaults, `DA_AFL_BrandEdgeMap`.

---

## PHASE 0 -- Complete the registry (universal prerequisite)

Everything depends on this. Add the 6 missing identities so the registry holds all
11: NeonYellow + brand Magenta, Indigo, Solar, Crimson, Lime.

- Source each value FROM WHERE IT IS BAKED TODAY so the registry matches what ships
  -- do not invent values:
  - NeonYellow <- RGBA from `DA_AFL_Finish_Yellow` / `MI_AFL_Weapon_Carbine_Yellow.AccentColor`.
  - Crimson <- `DA_AFL_Finish_Crimson` (exists).
  - Magenta / Indigo / Solar / Lime -- if a baked `DA_AFL_Finish_*` exists, source it;
    if NOT yet baked anywhere it is a genuinely new color and its value comes from the
    brand spec (`IRONICS_WEAPONS_SSOT` brand-color section). Flag EXPLICITLY which were
    sourced-from-baked vs new-from-spec.
- Each row = `IdentityTag` (`Cosmetic.Identity.<Name>`) + `PrimaryColor` + `AccentColor`.
- Register the 6 tags in `Config/DefaultGameplayTags.ini`. EDITOR RESTART REQUIRED
  before they resolve (TagManager scans ini at startup -- known trap).
- PROOF GATE (Phase 0): `GetEntryPrimaryColor` returns the correct RGBA for all 11;
  assert against the baked source values for the 5 that already ship. Data-level, no
  surface yet.

### Phase 0 -- EXECUTION LOG (2026-06-25, commit 8814e7c9 = ini tags only)

STEP A sourced values (4 grounded from baked, 2 pending -- no invented values):
- NeonYellow  primary (1.00,0.90,0.10)  accent (1.00,0.90,0.10*) -- SOURCED-FROM-BAKED:
  `MI_AFL_Weapon_Carbine_Yellow.AccentColor` == `DA_AFL_Finish_Yellow.EmissiveColor`.
  (*accent mirrors primary; the Finish EmissiveColor2 is a green copy-artifact, not used.)
- Crimson     primary (0.70,0.00,0.10)  accent (1.00,0.25,0.15) -- SOURCED-FROM-BAKED:
  `DA_AFL_Finish_Crimson` EmissiveColor / EmissiveColor2.
- Indigo      primary (0.35,0.25,1.00)  accent (0.80,0.35,1.00) -- SOURCED-FROM-BAKED:
  `DA_AFL_Finish_Violet_Indigo` EmissiveColor / EmissiveColor2.
- Solar       primary (1.00,0.45,0.00)  accent (1.00,0.75,0.10) -- SOURCED-FROM-BAKED:
  `DA_AFL_Finish_Orange_Solar` EmissiveColor / EmissiveColor2.
- Magenta     RGBA PENDING -- NOT baked anywhere; IRONICS_WEAPONS_SSOT s8 names it (law)
  but gives NO RGBA. No invented value. Needs operator RGBA or an authored baked Finish.
- Lime        RGBA PENDING -- same as Magenta.

STEP B done: 6 `Cosmetic.Identity.*` tags registered in `Config/DefaultGameplayTags.ini`
(NeonYellow/Crimson/Indigo/Solar + Magenta/Lime). EDITOR RESTART REQUIRED to resolve.

BLOCKED until restart (PROVEN, not assumed): the registry ROWS + the data-proof GATE. A
new identity tag is unconstructable via the bridge before it is registered -- the struct
ctor(tag_name=), `request_gameplay_tag` (absent), `import_text('(TagName=...)')`, and
existing-tag-copy ALL refuse an unregistered tag (tested 4 ways). So rows must be written
AFTER the operator restarts the editor (tags then resolve via `import_text`).

POST-RESTART STEPS (next turn): (1) write the 4 grounded rows via `import_text` tags + the
values above; (2) decide Magenta/Lime RGBA (operator or baked asset) + add those 2 rows;
(3) run the gate. NOTE for the gate: the existing 5 registry rows are designer-rounded vs
baked (e.g. NeonBlue registry (0.00,0.42,1.00) vs baked (0.10,0.40,1.00); Green/Red/Purple
similar; Pink matches exactly). Pre-existing -- do NOT re-tune (guardrail); report the
deltas, don't silently overwrite.

### Phase 0 -- POST-RESTART RESULT (2026-06-25)

Editor restarted; all 6 ini tags resolve (valid=True). 5 rows written to
`DA_AFL_ColorIdentityRegistry` (8->13 identities, readback-verified valid + correct):
NeonYellow (1.00,0.90,0.10), Crimson (0.70,0.00,0.10), Indigo (0.35,0.25,1.00),
Solar (1.00,0.45,0.00) -- the 4 sourced-from-baked -- plus:
- Magenta (1.00,0.10,0.70) accent (0.70,0.20,1.00) -- ROBOT-SOURCED: the Rift brand edge
  (`MI_SHOWFM_RIFTONE`/`MI_SHOWMASK_RIFTONE` EdgeGlowColor == `DA_AFL_Finish_Teal_Rift`
  edge), the most-saturated magenta on-screen. NOTE: close to NeonPink (1.00,0.10,0.60);
  the distinct-from-Pink alternative is SABLE (0.70,0.20,1.00) if separability is preferred.
- Lime (0.80,1.00,0.00) accent (0.80,1.00,0.00) -- OPERATOR-PROVIDED Electric Lime
  (RGB 204,255,0 = #CCFF00). No yellow-green existed on ANY robot/finish asset (223 MIs +
  32 finishes scanned: greens pure-green R<0.3, yellows R>=G), so this is an explicit
  operator value -- not asset-sourced, not invented. (The operator's #7FFF00 was
  inconsistent with the RGB+name, which agree on #CCFF00 = Electric Lime; used #CCFF00.)

GATE (registry readback = the authoritative data proof): 10/11 palette colors present, all
valid=True, all PrimaryColor/AccentColor correct. The `GetEntryPrimaryColor` /
`resolve_color_identity` API returns None in-editor (needs a live game world/subsystem =
PIE, out of scope) -- the readback proves the same data directly. Existing-5 vs baked deltas
confirmed pre-existing (NeonBlue 0.00,0.42,1.00 vs baked 0.10,0.40,1.00; etc.) -- report-only,
NOT re-tuned.

Phase 0 = 11 of 11 DONE (2026-06-25). Registry = 14 identities; gate 11/11
readback-verified, all valid=True. Phase 0 COMPLETE.

---

## PREREQUISITE BUG FIXES (gated -- named, fixed before the surface that needs them)

### P-BUG-1 -- Weapons param split
`M_AFL_Weapon_Master` exposes BOTH `BrandColor` and `AccentColor`. The baked per-color
MIs override `AccentColor` (the visible color); the cheat writes `BrandColor`. They are
different params.
- FIX: decide the canonical param by reading the master graph (which one feeds the
  visible body color -- almost certainly `AccentColor`, since the shipped MIs use it),
  then converge: the resolver writes the canonical one.
- GATE: a MID set on the canonical param visibly changes the gun body.
- Required before WEAPONS migration.

### P-BUG-2 -- Tracer no-op (two layers)
`NS_AFL_Pulse_Tracer` exposes `User.Linear Color` (with a space); the cue writes
`User.Color` -> no-op. AND the tracer color is curve-baked (`ColorFromCurve` /
`ScaleColor`), so even the right param name would not tint it.
- FIX: not a rename -- the clean `User.Color` ribbon on `M_AFL_FX_Master` (the earlier
  gap-3 Niagara-editor task).
- GATE: the pulse tracer renders a forced `User.Color` red.
- Required before the PULSE-TRACER half of Beams. The live beam does NOT depend on it.

---

## ORDER BY RISK (ranked from the map, defended)

**1st: BEAMS** -- least risky to prove, by all three criteria:
- Mechanism cleanest: the live beam is ONE tint-driven NS reading `User.Color` -- no
  per-color baked Niagara to rip out. The `User.Color -> render` sink is ALREADY PROVEN
  (STEP 1: the beam shows its color via `User.Color`). Only the chain to it is unproven.
- Blocker is separable: P-BUG-2 (tracer) is a distinct sub-surface. The live beam
  (`UAFLBeamVisualComponent`) proves WITHOUT the tracer.
- Resolver already targets the right input: sets `LaserTintColor` -> `GetBeamColor`; the
  beam re-reads per-activation; the `[TINTTRACE]` HOP1-6 instrumentation is already in
  place to pinpoint the one remaining hop (suspected HOP4 BNE-dispatch).

**2nd: WEAPONS** -- non-separable blocker (P-BUG-1 must be resolved before ANY weapon
tint), and the resolver currently writes the wrong param. The MID mechanism itself is
sound, so once the param converges it is predictable.

**3rd: VISORS, 4th: SKINS** -- both PROVEN-BAKED and shipped; migrate LAST, each behind
its proven baseline, never ripping out working color without re-proving. Skins last =
highest regression exposure (convergence + Race A/B/C + replication all PIE-proven).

---

## PER-SURFACE MIGRATION (execution order)

### 1. BEAMS / FX
- CURRENT: baked per-weapon `LaserTintColor` (3 BP defaults) -> `GetBeamColor` ->
  `User.Color`. Beam reads it; tracer is curve-baked (no-op).
- TARGET: `LaserTintColor` is SET FROM THE REGISTRY by the resolver (SKU's
  `ColorIdentityTag -> PrimaryColor`); both beam and (fixed) tracer consume `User.Color`.
- STEPS:
  (a) run the `[TINTTRACE]` watch -> identify the dying hop -> fix it (if HOP4: ensure
      the adopted `GetBeamColor` BP override actually dispatches, else promote
      `LaserTintColor` to a C++ field the interface returns).
  (b) make the resolver a SHIPPING caller (not the cheat) reading the SKU tag.
  (c) P-BUG-2 for the pulse tracer.
- PROOF GATE: equip beam weapon -> select NeonBlue -> beam BLUE on screen; select
  NeonGreen -> GREEN; no-color -> original. Visual, not a log line.
- RISK: chain unproven; possible BNE-dispatch wall at HOP4 -- but the instrumentation
  makes it bounded (find -> fix). The render sink is already proven, which caps the risk.

### 2. WEAPONS (body)
- CURRENT: baked per-color MIs (`MI_AFL_Weapon_Carbine_{Blue,Red,Purple,Pink,Yellow}`,
  override `AccentColor`) assigned to the mesh.
- TARGET: ONE neutral base MI off `M_AFL_Weapon_Master`; the resolver creates a runtime
  MID + `SetVectorParameterValue(<canonical param>, registry-color)`.
- STEPS:
  (a) P-BUG-1 -- converge on the canonical param.
  (b) resolver drives that param from the SKU tag.
  (c) repoint weapon meshes to the single base MI; RETIRE the per-color MIs (keep until
      proven).
  (d) wire the shipping caller (`FAFLCosmeticSelection.WeaponId -> resolver`).
- PROOF GATE: equip weapon -> select NeonBlue -> GUN BODY BLUE on screen; another color
  -> changes; no per-color MI in the loop.
- RISK: moderate; the MID mechanism is proven, the fix is the known param convergence.
  Regression low (per-color MIs are static content).

### 3. VISORS (facemask) -- NOT a palette-axis surface (mis-scoped; CORRECTED 2026-06-26)
- **CORRECTION.** Visors are a **DESIGN / ENTITLEMENT catalog**, not a color migration. The
  earlier "migrate `EmissiveColor` to the registry" framing was WRONG and is removed.
- **ASSET REALITY** (verified on disk): the 33 `DA_AFL_Facemask_*` are NAMED DESIGNS (national
  flags, JollyRoger, Anarchy, Kawaii, TechCircuit, pattern masks: Stripe/Dot/Grille...). Each MI
  carries its OWN `LogoTexture` (`T_AFL_Visor_<name>`) -- the emblem IS the product -- plus a
  themed `EmissiveColor` that MATCHES the design by intent (BrazilVerde green *because Brazil*;
  JapanSolar red *because Japan*; MexicoEagle green; Kawaii pink). Color does NOT vary
  independently -- it is baked because it is MEANT to be that color. Overriding it from the
  registry would BREAK the design.
- **THE SKU:** a visor is the emblem/design itself, keyed `AFL.Facemask.<Name>`, entitlement-gated
  (1 base free + account-bound; the other 32 priced at the SPARK rung -- $10 / 10,000 V / 100,000 W
  per the pricing SSOT, `bTradeable` once bought). Color is an INTRINSIC attribute, not a
  migratable axis.
- **ONLY registry link:** the IroVisor *base* mask is wired `AFL.Facemask.IroVisor ->
  Cosmetic.Identity.IronicsVisor` as the IRONICS free-brand default -- one base default, not a
  per-visor color axis. (`Cosmetic.Identity.IronicsVisor` stays in the registry only for that.)
- **OWNER:** the PRODUCT/CATALOG axis (Store/economy workstream, `FAFLCatalogEntry`), NOT this
  palette plan. Logo emblems are the same shape (design = the SKU). Nothing to do here for color.

### 4. SKINS (body) -- SPEC LOCKED 2026-06-26; behind its full baseline, highest regression
CURRENT: ~38 baked `UAFLSkinColorAsset` presets in TWO axes -- 6 Edge (`DA_AFL_Edge_*`:
EmissiveColor1/2/3 + EdgeGlow, NO Team) + ~33 Finish (`DA_AFL_Finish_*`: + TeamColor). FULLY
PROVEN (convergence, Race A/B/C, replication). Color is read straight from the baked
`ColorParameters`; the registry is NOT consulted by the skin path.

DECISION (operator, 2026-06-26): **ONE BRAND IDENTITY PER COLOR.** Each color is a SINGLE
registry identity holding BOTH proven looks -- the edge tones from `Edge_<color>` AND the body
tone from `Finish_<color>`, every value transcribed VERBATIM from its proven preset. Not a
blended compromise: both looks, one identity. The edge reader reads the emissive ramp +
EdgeGlow; the body reader reads TeamColor. **Option A (flatten to 2 colors, leave the ramp
baked) is REJECTED** -- it contradicts the design (THE MODEL below) and leaves color scattered.

THE MODEL (grounded, not invented):
- A color identity = a FULL coherent finish, NOT a flat hue. `AFL_ECONOMY_ARCHITECTURE_ADR`
  Decision 10: the body material exposes "`TeamColor` (body) + `EmissiveColor1/2/3` (a 3-tier
  MIP-BLENDED emissive ramp) + `EdgeGlowColor`", set to a "graded sequence (ORION navy ->
  brighter-blue mid -> star-white edge)". `AFLCosmeticTypes.h:25`: a color = "a FULL base finish
  (body TeamColor + emissive + edge-glow together)".
- The marketplace skill (lyra-skin-builder-marketplace) models the same two separable axes:
  "EDGE/glow color = EmissiveColor1-3 + EdgeGlowColor" and "BODY color = TeamColor" -- with the
  registry named as the "TeamColorPalette pattern (Fortnite/Valorant/CS2)".
- So the flat `{Primary, Accent}` was the INCOMPLETE thing. AAA = full-tone, single-sourced.

THE STRUCT EXTENSION (additive C++ change -- EXECUTION PHASE, operator build; NOT done here):
FAFLColorIdentity gains a SkinFinish bundle; Primary/Accent stay top-level UNTOUCHED (weapons +
beams SHIP reading them -- byte-identical, additive only):

    FAFLColorIdentity {
      FGameplayTag IdentityTag;       // key -- unchanged
      FLinearColor PrimaryColor;      // cross-surface dominant (weapon AccentColor, beam tint) -- UNTOUCHED
      FLinearColor AccentColor;       // cross-surface contrast (pink<->purple, red<->orange)   -- UNTOUCHED
      FAFLSkinFinish SkinFinish;      // NEW -- the full body look
    }
    FAFLSkinFinish {
      FLinearColor TeamColor;         // BODY axis -- body base shade  (from Finish_<color>)
      FLinearColor EmissiveColor1;    // EDGE axis -- emissive base    (from Edge_<color>)
      FLinearColor EmissiveColor2;    // EDGE axis -- emissive bright  (from Edge_<color>)
      FLinearColor EmissiveColor3;    // EDGE axis -- emissive mid     (from Edge_<color>)
      FLinearColor EdgeGlowColor;     // EDGE axis -- rim glow          (from Edge_<color>)
    }

Scalars/textures (`EmissiveStrength*`, `EdgeGlowMagnitude`, masks) STAY in the preset -- they
are SHAPE/intensity, not color. The registry carries color only. Store tones EXPLICITLY (not
derived): the ramp is art-directed AND the baked tones are not a clean midpoint, so deriving
would re-tune the proven look.

THE UNIFIED IDENTITY TABLE -- the 5 fully-covered Neon (verbatim from baked, zero shift):

  | Identity   | EmissiveColor1 | EmissiveColor2 | EmissiveColor3 | EdgeGlow     | TeamColor       |
  |------------|----------------|----------------|----------------|--------------|-----------------|
  | NeonBlue   | 0,0.42,1       | 0,0.896,1      | 0,0.723,1      | 0,0.42,1     | 0.05,0.25,0.85  |
  | NeonGreen  | 0,1,0.25       | 0.2,1,0.45     | 0,0.9,0.35     | 0,1,0.30     | 0.06,0.55,0.12  |
  | NeonPink   | 1,0.1,0.6      | 1,0.4,0.75     | 0.95,0.05,0.5  | 1,0.1,0.6    | 0.9,0.1,0.45    |
  | NeonPurple | 0.6,0,1        | 0.8,0.35,1     | 0.5,0,0.9      | 0.6,0,1      | 0.4,0.1,0.7     |
  | NeonRed    | 1,0.05,0.05    | 1,0.25,0.15    | 0.9,0.05,0.05  | 1,0.05,0.05  | 0.75,0.06,0.06  |

  EmissiveColor1/2/3 + EdgeGlow <- `Edge_<color>` exact; TeamColor <- `Finish_<color>` exact.
  NeonGreen carries the BAKED 0.25 (NOT the registry's rounded 0.30) -- every value verbatim =
  ZERO shift. Primary/Accent already in the registry, unchanged.

  THE 6 PHASE-0 COLORS -- partial coverage; flag the absent axis, DO NOT invent the tone:

  | Identity   | Edge ramp (emissive)   | TeamColor                          |
  |------------|------------------------|------------------------------------|
  | NeonYellow | NO Edge preset -- GAP  | Finish_Yellow = 0.95,0.85,0.05     |
  | Crimson    | NO Edge preset -- GAP  | Finish_Crimson = 0.55,0,0.08       |
  | Indigo     | NO Edge preset -- GAP  | Finish_Violet_Indigo? (map TBD)    |
  | Solar      | NO Edge preset -- GAP  | Finish_Orange_Solar? (map TBD)     |
  | Magenta    | NO Edge preset -- GAP  | NO Finish (robot-sourced) -- GAP   |
  | Lime       | NO Edge preset -- GAP  | NO Finish (robot-sourced) -- GAP   |

  These 6 have NO `Edge_<color>` preset, so their emissive RAMP is a DESIGN GAP -- author it (or
  source it from a baked asset) at execution; do not fabricate tones. The 5 Neon (full coverage)
  migrate FIRST; the 6 Phase-0 follow once their ramps are design-resolved.

MIGRATION SHAPE (replication BYTE-IDENTICAL -- confirmed by the read):
- Add `FGameplayTag ColorIdentityTag` to `UAFLSkinColorAsset`. The preset becomes SHAPE + TAG.
- `ApplySkinColor` resolves `preset.ColorIdentityTag -> registry -> SkinFinish` and writes the
  tones to the OWNED MID (Emissive1/2/3 + EdgeGlow; TeamColor for body presets) INSTEAD of the
  baked `GetColors()`. `GetColors()` stays as the `A<=0` fallback for the first migration (the
  proven beam/weapon sentinel pattern).
- REPLICATION UNTOUCHED: the `FLinearColor` never crosses the wire. Selection FNames
  (`EdgeId`/`BodyId`) replicate on PlayerState; the resolved `UAFLSkinColorAsset*` replicates on
  the Pawn; each client resolves the color LOCALLY in `ApplySkinColor`. The proven convergence
  path (selection + pointer + OnRep + both apply triggers) is byte-identical. Scalars/textures
  stay in the preset.

PROOF GATES (LOCKED):
  1. RE-PROVE BASELINE FIRST -- skin renders its current FULL look + Race A/B/C + replication
     convergence, on screen, BEFORE any edit.
  2. MIGRATE ONE -- `Edge_NeonBlue` (EXACT delta). Tag it; resolve from the registry.
  3. WATCH THE FULL LOOK -- the 3-tone gradient + edge, pixel-match to baseline (NOT just the
     dominant hue -- the registry carries Emissive2/3, so it can).
  4. RE-PROVE THE RACES AFTER -- server-only color change converges to clients. The gate that
     matters MOST; multiplayer must not regress.
  5. ZERO-SHIFT GATE -- every tone transcribed verbatim from baked (incl. NeonGreen 0.25,
     Emissive2/3, Team); any nonzero delta is a BUG, not a re-tune.

SCALING TEST: +1 color = ONE registry row (full identity: Primary/Accent + the 5 SkinFinish
tones) -> skin reads SkinFinish (full look), weapon reads Primary (AccentColor), beam reads
Primary (User.Color), UI reads Primary/Accent. NO per-preset edits -- presets are shape + tag.

---

## GLOBAL PROOF -- "the system scales"

END-STATE TEST: add a brand-new 12th color as ONE registry row + ONE ini tag (no
surface edits). Then in PIE, on a SKU per surface, select the new color -> ALL 4
surfaces render it on screen -- skin, visor, gun body, beam -- driven by ONE resolver
reading the tag. If any surface needs a per-surface asset edit to show the new color,
that surface is not migrated.

RESOLVER CONVERGENCE: one color-resolution (`ColorIdentityTag -> registry`) feeds four
per-surface sinks (`Emissive` / `AccentColor` / `LaserTintColor` / `User.Color`). The
shipping callers (`FAFLCosmeticSelection.{EdgeId, FacemaskId, WeaponId}`) hand the tag
in; no surface bakes color.

---

## EXECUTION CHECKLIST (tracks state across sessions -- check a box only after ON-SCREEN proof)

- [x] PHASE 0 -- Registry holds all 11. DONE 2026-06-25. Proof: registry readback = 11/11
      palette colors present, all valid=True. 6 rows written (8->14 identities):
      NeonYellow/Crimson/Indigo/Solar (sourced-from-baked) + Magenta (1.00,0.10,0.70 robot
      Rift edge) + Lime (0.80,1.00,0.00 operator Electric Lime). Existing-5 designer-rounded
      deltas vs baked = report-only, not re-tuned. See Phase 0 POST-RESTART RESULT above.
- [ ] P-BUG-1 -- Weapons param converged to one canonical param. Proof: a MID set on the
      canonical param visibly changes the gun body.
- [ ] P-BUG-2 -- Tracer clean `User.Color` ribbon. Proof: pulse tracer renders a forced
      `User.Color` red.
- [x] BEAMS (live beam) migrated -- DONE 2026-06-26, commit 0abaa729. Proof: ShotgunBeam
      tinted NeonPurple ON SCREEN, green default held with no selection. Fix = reflection-read
      LaserTintColor at the consumer (the bridge-wired BNE GetBeamColor override does NOT
      dispatch at runtime). CAVEATS, not yet done: (1) pulse TRACER gated on P-BUG-2
      (curve-baked NS exposes "User.Linear Color" not "User.Color" -- dispatch fixed, NS can't
      consume yet); (2) the ShotgunBeam flash + impact ALSO render purple (operator-confirmed
      2026-06-26) -- its full FX is the beam component NS, so the 3 laser cue-notifies
      (LaserBeam/BeamFlash/Impact, which still call the phantom Execute_GetBeamColor) do NOT
      fire for it; their live-status (Pulse? legacy?) is TBD before treating as a follow-up.
- [x] WEAPONS (Pistol body) migrated -- DONE 2026-06-26. Proof: Pistol body tinted NeonPurple
      ON SCREEN (green default held); log "AccentColor SET via runtime MID on SkeletalMesh slot 0".
      P-BUG-1 resolved: AccentColor is canonical (master graph -> EmissiveColor; per-color MIs
      override ONLY AccentColor); BrandColor is VESTIGIAL (feeds no output) -- the cheat used to
      write the dead BrandColor = silent no-op. CAVEATS: (1) CARBINE body is ~20 Tripo part-meshes
      NOT on M_AFL_Weapon_Master -> can't tint via this MID (needs reskin-to-master or tint-Tripo,
      separate); (2) BrandColor retire (strip from master + base MIs) = follow-up; (3) shipping
      resolver (FAFLCosmeticSelection.WeaponId) writes AccentColor (canonical, recorded).
- [--] VISORS -- REMOVED from this plan (mis-scoped). A DESIGN/ENTITLEMENT catalog (33 named
      masks, themed color intrinsic by design), NOT a palette-axis surface. Belongs to the
      PRODUCT/CATALOG (Store/economy) workstream. Only IroVisor-base links to the registry
      (`AFL.Facemask.IroVisor -> Cosmetic.Identity.IronicsVisor`, the IRONICS free default).
- [SPEC LOCKED] SKINS -- spec locked 2026-06-26 (one-identity-per-color, full-tone SkinFinish;
      Edge emissive + Finish Team verbatim, zero shift). The struct extension
      (FAFLColorIdentity += FAFLSkinFinish) is a C++ change flagged for the EXECUTION phase
      (operator build). NOT STARTED. Proof when migrated: skin shows the registry color's FULL
      look ON SCREEN (3-tone gradient + edge); Race A/B/C + replication re-proved BEFORE & AFTER.
- [ ] GLOBAL SCALE TEST. Proof: add the 12th color = 1 registry row + 1 ini tag -> all 4
      surfaces show it in PIE with ZERO per-surface asset edits.

---

*This file is the execution checklist. Each phase is proven ON SCREEN before the next.
No phase is "done" off a build or a log line -- only off a PIE watch of the registry
color on that surface.*
