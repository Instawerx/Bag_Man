# Import, Rebrand & Removal of the Old Methodology

How the marketplace pack lands in BAG MAN cleanly, and how the previous laser/beam
approach is retired so nothing references it again.

## 0. License gate (do first)

Confirm the studio's Fab/Marketplace license for "Laser VFX" permits use in a
distributed product. Standard Epic Marketplace/Fab licenses do; record the seat/order
in the project's third-party asset ledger. Do not proceed until logged — shipping
unlicensed marketplace content is a cert and legal problem, not just a code one.

## 1. Engine version: this is a 5.2 pack, BAG MAN is 5.6

The pack's `.uproject` declares `EngineAssociation 5.2`. Do not point your 5.6 project
at these files cold. Upgrade in isolation first:

1. Open the pack project in **UE 5.2** once (or let 5.6 convert a *copy*). Resave all.
2. Open the copy in **UE 5.6**. Let it recompile shaders + Niagara, accept the
   conversion, then **resave all** (`Tools > resave all` / `ResavePackages` commandlet).
3. **Validate every Niagara system compiles** — open 3–4 (`NS_Laser_Basic`,
   `NS_Laser_Twist`, an `NS_OrbLaser_*`, `NS_OrbLaser_Center_*`). Dynamic-beam and
   ribbon modules are stable across 5.2→5.6, so risk is low, but a deprecated module
   surfaces as a compile error in the system, not at cook — catch it here.
4. Only migrate assets that survive the upgrade clean.

## 2. Where it lands: a content-only plugin

Put the library in its own content-only plugin so it is modular, upstream-mergeable,
and never tangles with Lyra base or the AFL gameplay modules:

```
Plugins/AFLVFXLibrary/
  AFLVFXLibrary.uplugin          (Type: Content, no module)
  Content/Laser/
    Niagara/      Materials/      Materials/Functions/
    Textures/     Meshes/         Curves/
```

Mount point becomes `/AFLVFXLibrary/Laser/...` — matches the `proposed_folder` column
in the rename manifest.

> The C++ cue/interface code from `integration-architecture.md` lives in a **separate
> code plugin/module** (`AFLVFX`), not in this content plugin. Content plugin = art;
> code plugin = wiring.

## 3. Generate the manifest, then rename

Run the read-only auditor on the unzipped pack:

```bash
python3 scripts/audit_laser_pack.py /path/to/unzipped/LaserVFX5_2v --out ./_laser_audit
```

It writes `inventory.json` and `rename_manifest.csv`. The manifest maps every shipped
asset `old_name → proposed_name (AFL_ namespaced) → proposed_folder`. Apply renames
inside the editor (which creates redirectors), then **Fix Up Redirectors** on
`/AFLVFXLibrary/`.

Naming follows the AFL convention (asset-pipeline skill): `NS_AFL_*`, `M_AFL_*_Master`,
`MI_AFL_*`, `T_AFL_*`, `SM_AFL_*`, `CC_AFL_*`. Examples from the real pack:

| Source | AFL name |
|---|---|
| `NS_Laser_Basic` | `NS_AFL_Laser_Basic` |
| `NS_Laser_Twist` | `NS_AFL_Laser_Twist` |
| `NS_OrbLaser_01` | `NS_AFL_OrbLaser_01` |
| `NS_OrbLaser_Center_01` | `NS_AFL_OrbLaser_Center_01` |
| `M_Beam` | `M_AFL_Beam_Master` |
| `M_Laser_Colorful` | `M_AFL_Laser_Colorful_Master` |

## 4. Exclude the dead weight

The auditor flags **47 EXCLUDE assets** and **2 REFERENCE_ONLY**. Do **not** import:

- `Content/LaserFX_BP/Demo/` — demo room, walls, tiles, cinematic, first-person
  template. Game doesn't use any of it; importing it bloats the cook and drags in the
  First-Person template's input/gamemode config.
- `Content/LaserFX_BP/Map/` and the `*_BuiltData` (one is ~12 MB of baked lighting).
- `Config/Default*.ini` from the pack — these are First-Person-template settings and
  will clobber Lyra config. Never copy them in.
- `BP_Laser` / `BP_Laser_Orb` — **reference only.** Keep them open in the *upgrade*
  project to study which Niagara param drives the endpoint, but do not migrate them
  into BAG MAN. Their per-tick `LineTraceSingle` is the anti-pattern this whole skill
  exists to replace.

## 5. LFS + commit

`*.uasset`/`*.umap` are already LFS-tracked in the repo (Phase 0). These Niagara
systems are 0.5–3.6 MB each — LFS is mandatory. After import + rename + fixup:

```bash
git lfs status                       # confirm new .uasset are LFS pointers
git add Plugins/AFLVFXLibrary Plugins/AFLVFX
git commit -m "feat(AFLVFXLibrary): adopt Laser VFX pack (5.2->5.6), AFL-namespaced; add AFLVFX cue layer"
```

Then a cook audit: Fix Up Redirectors on `/AFLVFXLibrary/`, cook the plugin, confirm
zero missing references and zero stray Demo assets in the cooked set.

## 6. Removing the old methodology (the part that prevents relapse)

The previous plan hand-authored per-weapon beam Niagara and a bespoke master material,
and the marketplace demo drives beams from a tick-trace BP. Retire all of it:

**Cancel / delete these planned-or-started assets** (from the earlier roadmap):

- `NS_AFL_PulseBeam`, `NS_AFL_PrismBeam` — superseded by the imported beam library.
- `NS_AFL_HitSpark` — superseded by `GameplayCue.Weapon.Laser.Impact` + an imported
  spark system.
- `M_AFL_NeonMaster` (as the beam master) — superseded by `M_AFL_Beam_Master` and the
  other imported beam masters. (Keep `M_AFL_NeonMaster` only if it's used by non-beam
  environment art; if it was beam-only, delete it.)

**Forbid the tick-trace beam pattern** anywhere in the codebase/blueprints:

- No actor/BP/component may `LineTrace` on Tick to position a beam endpoint as the
  authoritative source. Cosmetic endpoint traces are allowed **only** inside the cue
  notify (clearly cosmetic, never feeding damage).
- The CI lint rule that already rejects "server-side GetPlayerViewPoint" and
  "GiveAbility outside AbilitySet" should gain a check: flag any `BeginPlay`/`Tick`
  that both line-traces and writes a Niagara variable outside the `AFLVFX` cue classes.

**Update the roadmap/tracker tasks:**

- The old beam-authoring tasks (the AFL-02xx "NS_AFL_PulseBeam / NS_AFL_PrismBeam /
  M_AFL_NeonMaster" line items) are **closed as superseded**, not completed. Replace
  with the steps in this skill's workflow, all PIE-gated.
- In STEP-0 terms this is Phase-1 reattachment cosmetic work: it cannot be ✅ until the
  Control Gate (BM-0013) and Pulse reattachment (BM-0101) are green and the beam is
  watched working in PIE.

**Update the AIK VFX profile** so the agent never hand-rolls a beam again — see
`aik-briefing.md`.

## 7. Done-definition for the import step

Import is complete (not the feature — the import) when:

- [ ] All 97 importable assets present under `/AFLVFXLibrary/Laser/`, AFL-named.
- [ ] Zero Demo/Map/Config assets imported; zero broken redirectors.
- [ ] 4+ Niagara systems opened and compiling clean in 5.6.
- [ ] `BP_Laser`/`BP_Laser_Orb` NOT in the project.
- [ ] Old `NS_AFL_PulseBeam`/`PrismBeam`/`HitSpark` plan closed-as-superseded.
- [ ] Committed via LFS; cook audit clean.
