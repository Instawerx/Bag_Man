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

Color lives in ONE place: `DA_AFL_ColorIdentityRegistry`. Every surface (Skins,
Visors, Weapons, Beams) resolves `ColorIdentityTag -> registry -> {Primary, Accent}`
and drives its own material/Niagara sink from that value.

- Adding a color = ONE registry row + ONE ini tag.
- Adding a SKU = pick a tag.
- No surface stores a color.

The audit (this session) proved we are the opposite today: color is FIXED
PER-SURFACE, stored in SIX places, the registry is PARTIAL (5 of 11) and BYPASSED by
all 4 surfaces. This plan converges them.

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

### 3. VISORS (facemask) -- behind its proven baseline
- CURRENT: 32 baked `MI_AFL_FaceMask_*` (baked `EmissiveColor` + per-visor `LogoTexture`).
  PROVEN on screen.
- TARGET: emblem (texture) stays per-visor; COLOR migrates to the registry. One base
  visor MI with the emblem texture + a registry-driven `EmissiveColor` (MID).
- STEPS:
  (a) RE-PROVE the current baseline first (IroVisor on screen) -- regression gate.
  (b) split emblem-texture from color in the MI.
  (c) resolver drives `EmissiveColor` from the SKU tag, emblem from the visor's texture.
  (d) migrate one visor, prove, then the rest.
- PROOF GATE: equip visor -> select a registry color -> VISOR SHOWS IT on screen, emblem
  intact; baseline IroVisor unchanged when no color override.
- RISK: high regression -- proven + shipped. Never remove a working visor color without
  the re-prove. Migrate incrementally (one visor at a time).

### 4. SKINS (body) -- last, highest regression
- CURRENT: ~38 baked `UAFLSkinColorAsset` presets (Edge + Finish, each baked
  `Emissive` / `EdgeGlow` / `TeamColor`). FULLY PROVEN (convergence, Race A/B/C,
  replication).
- TARGET: preset = PARAM-SHAPE ONLY (which params / masks); the COLOR comes from the
  registry via the SKU tag.
- STEPS:
  (a) RE-PROVE the full baseline first (incl. the replication races) -- regression gate.
  (b) separate param-shape from color in `UAFLSkinColorAsset`.
  (c) the controller component resolves the SKU tag -> registry -> drives the existing
      param writes in `ApplySkinColor`.
  (d) collapse the ~38 presets to {shape + registry color}.
- PROOF GATE: equip skin -> select a registry color -> SKIN SHOWS IT on screen; then
  RE-RUN Race A/B/C + replication (server-only change converges to clients) -- the
  migration must not regress the proven multiplayer behavior.
- RISK: highest -- the most-proven pillar. Behind its full baseline; re-prove the races
  before declaring done.

### REGRESSION GATE (Skins + Visors -- the 2 proven-baked surfaces)
Both must RE-PROVE their baseline BEFORE migration (so we know the starting state is
green on THIS build) AND AFTER migration (so the registry path did not regress what
ships). For SKINS this explicitly includes Race A/B/C + replication (server-only color
change converges to clients). Never rip out a working color path without the
before-and-after re-prove.

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
- [ ] VISORS migrated. Proof: visor shows a registry color ON SCREEN, emblem intact.
      Baseline IroVisor re-proved BEFORE and AFTER.
- [ ] SKINS migrated. Proof: skin shows a registry color ON SCREEN. Race A/B/C +
      replication re-proved BEFORE and AFTER.
- [ ] GLOBAL SCALE TEST. Proof: add the 12th color = 1 registry row + 1 ini tag -> all 4
      surfaces show it in PIE with ZERO per-surface asset edits.

---

*This file is the execution checklist. Each phase is proven ON SCREEN before the next.
No phase is "done" off a build or a log line -- only off a PIE watch of the registry
color on that surface.*
