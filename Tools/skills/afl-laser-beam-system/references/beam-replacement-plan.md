# Beam Replacement Plan — keep Pulse, fix the second beam, standardize the rest

The stance, set by the team:

- **Pulse Carbine (`UAFLAG_Laser_Pulse`) is AAA — keep it.** No gameplay changes. Its
  cosmetic only gets re-routed through the cue/library if it still references a retired
  asset, and only after proving it still looks AAA in PIE.
- **The second beam (the channeled Prism beam, `UAFLAG_Laser_Beam`) passed but is not
  AAA — fix it.** Gameplay stays (it passed); swap its hand-authored Niagara for an
  imported library beam driven through the looping cue.
- **Every other laser/beam adopts the same cosmetic standard**, and any remaining
  hand-authored beam Niagara is retired.

Gameplay (trace, target data, lag-comp, `UAFLDamageExecCalc`, heat, telemetry) is never
touched in this plan — only the cosmetic layer changes. ✅ = watched in PIE, per doctrine.

---

## R0 — Audit current beam usage (know before you cut)

Find every reference to a beam Niagara / beam material and which ability or weapon owns
it, so nothing is swapped blind:

```bat
:: from C:\Dev\Bag_Man — list assets referencing the old/hand-authored beams
:: (editor: Reference Viewer on each; or a content-scan commandlet)
```
Produce a small table: weapon/ability → current beam NS → current material → AAA? →
target imported system. Flag any use of the retired set (`NS_AFL_PulseBeam`,
`NS_AFL_PrismBeam`, `NS_AFL_HitSpark`, `M_AFL_NeonMaster`-as-beam). That table drives R2–R4.

## R1 — Confirm the cosmetic standard exists (prerequisite)

Before replacing anything, the target layer must be in place (from this skill):
`AAFLCueNotify_LaserBeam` (looping), the `BeamFlash`/`Impact` burst cues, the
`IAFLLaserVisualProvider` interface, and the imported `/AFLVFXLibrary/Laser/` library.
If any is missing, run beam prompts 1–7 in `aik-briefing.md` first. Re-verify the beam
PIE matrix (integration-architecture.md) once on ANY weapon so the standard itself is
proven before migrating the real ones.

## R2 — Fix the second beam (priority)

The channeled beam works but looks sub-AAA. Replace the visual, keep the ability.

1. Pick the AAA look from the library — `NS_AFL_Laser_Twist` (spiraling, reads well for a
   sustained beam) is the default; design may pick another. Make the `_Mobile` material
   instance for whatever master it uses.
2. Author `DA_AFL_LaserVisual_PrismBeam` (BeamSystem = chosen NS, MuzzleSystem optional,
   BeamColor, MuzzleSocketName, CosmeticRange = the beam's existing max range).
3. Make the beam weapon implement `IAFLLaserVisualProvider` backed by that data asset.
4. In `UAFLAG_Laser_Beam`: on activate `AddGameplayCue(GameplayCue.Weapon.Laser.Beam)`
   (SourceObject = weapon); on end `RemoveGameplayCue`. Delete any old direct-Niagara spawn
   in the ability.
5. **Retire** the old `NS_AFL_PrismBeam` and its bespoke material once nothing references
   them (verify in Reference Viewer, then delete + Fix Up Redirectors).
6. PIE re-verify the full beam matrix on this weapon: endpoint tracks the wall, muzzle
   attach holds, color correct, replicates to 2 clients, damage unchanged, pawn still
   controls. It must now read AAA AND still pass what it passed before.

## R3 — Pulse Carbine: keep, re-route cosmetic only if needed

Pulse is AAA and hitscan, so its "beam" is a one-frame flash + impact.

- If Pulse currently uses a **retired** asset (`NS_AFL_PulseBeam`/`NS_AFL_HitSpark`):
  re-route its cosmetic to `GameplayCue.Weapon.Laser.BeamFlash` + `Impact` with an
  imported system (e.g. `NS_AFL_Laser_Basic` for the flash), via a
  `DA_AFL_LaserVisual_PulseCarbine`. Change nothing in its trace/damage. Then PIE-verify it
  still looks and feels exactly as AAA as before — if the flash regresses at all, tune the
  cue, do not touch the ability.
- If Pulse does **not** use a retired asset and already looks AAA: leave it entirely. The
  only reason to migrate is consistency, and consistency is not worth risking a feature
  that's already shipping at quality. Re-skin it later in a low-stakes pass.

## R4 — Standardize every other laser/beam + retire leftovers

For each remaining laser weapon (built or future — Ricochet, Nova, Singularity, Chain,
Melt, etc.): give it a `DA_AFL_LaserVisual_<Weapon>` pointing at a library system, drive it
through the same cues, and never author a new beam Niagara. Charge/orb weapons add a
`MuzzleSystem` (`NS_AFL_OrbLaser_Center_*`) and use the charge ability/cue from
`full-weapon-feature-map.md`.

Then close out the old methodology:
- Delete every retired beam asset once unreferenced; Fix Up Redirectors.
- Land the CI lint rule that fails a PR which authors a beam Niagara outside the library or
  drives a beam endpoint from a per-tick trace outside the cue classes.
- Cook audit: zero missing refs, zero retired assets in the cooked set.

---

## Replacement AIK prompts (run after the session bootstrap, §0)

These reuse the cue/provider built by beam prompts 1–7. Numbered RP-* to avoid colliding
with the beam (1–7) and full-weapon (8–14) sets.

### RP-1 — audit
```
Scan the project for references to NS_AFL_PulseBeam, NS_AFL_PrismBeam, NS_AFL_HitSpark,
M_AFL_NeonMaster, and any beam Niagara spawned directly inside a GameplayAbility or driven
by a per-tick LineTrace. Output a table: owning ability/weapon -> current beam NS ->
material -> whether it's a retired asset -> proposed /AFLVFXLibrary/Laser/ replacement.
Do not change anything yet.
```

### RP-2 — fix the second (Prism) beam  (attach: UAFLAG_Laser_Beam + its current Niagara)
```
Re-skin the attached channeled beam to AAA using the imported library, gameplay unchanged:
- Create DA_AFL_LaserVisual_PrismBeam (BeamSystem = NS_AFL_Laser_Twist, BeamColor =
  current beam color, MuzzleSocketName = "Muzzle", CosmeticRange = the ability's max range);
  make a _Mobile instance of the Twist beam material.
- Make the beam weapon instance implement IAFLLaserVisualProvider backed by it.
- In UAFLAG_Laser_Beam: replace any direct Niagara spawn with AddGameplayCue(
  GameplayCue.Weapon.Laser.Beam, SourceObject = weapon) on activate and RemoveGameplayCue on
  end. Do NOT alter the trace, target data, or damage.
Show the diff and confirm no Niagara include remains in the ability.
```

### RP-3 — retire the old Prism assets
```
After confirming nothing references NS_AFL_PrismBeam and its bespoke beam material (Reference
Viewer), delete them and Fix Up Redirectors on /Game/ and /AFLVFXLibrary/. List anything
still referencing them instead of deleting if refs remain.
```

### RP-4 — Pulse cosmetic re-route (ONLY if it uses a retired asset)
```
If and only if UAFLAG_Laser_Pulse currently references a retired asset, create
DA_AFL_LaserVisual_PulseCarbine (BeamSystem = NS_AFL_Laser_Basic) and route Pulse's cosmetic
through ExecuteGameplayCue BeamFlash + Impact with the predicted Location/Normal. Change
nothing in Pulse's trace or damage. If Pulse does not use a retired asset, report that and
make no changes.
```

### RP-5 — standardize the remaining lasers
```
For each remaining laser weapon, create DA_AFL_LaserVisual_<Weapon> pointing at an
appropriate /AFLVFXLibrary/Laser/ system and route its visuals through the existing cues
(charge/orb weapons add a MuzzleSystem and the charge cue). Never author a new beam Niagara.
List the per-weapon mapping you used.
```

### RP-6 — lint + cook audit
```
Add a CI lint rule failing any PR that (a) creates a beam Niagara outside /AFLVFXLibrary/ or
(b) sets a beam-end Niagara variable from a per-tick LineTrace outside the AFLVFX cue classes.
Run a cook audit and report zero missing references and zero retired beam assets in the cook.
```

## PIE gates

- **Second beam:** now reads AAA AND still passes its prior acceptance (endpoint tracks
  surface, muzzle attach, color, replication to 2 clients, damage unchanged, pawn controls).
- **Pulse:** looks/feels identical-or-better to its current AAA bar; zero gameplay change.
- **Each migrated weapon:** beam matrix green; old asset gone; nothing stuck on after fire.
