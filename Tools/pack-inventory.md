# Laser VFX Pack — Inventory & Parameter Surface

Extracted from the actual `LaserVFX5_2v_STV.zip` (UE 5.2, First-Person template base).
Regenerate any time with `scripts/audit_laser_pack.py`.

## Counts

| Category | Count | Action |
|---|---|---|
| Niagara systems | 31 | Import (the product) |
| Materials (masters) | 18 | Import + make `_Mobile` instances for shipped ones |
| Material instances | 11 | Import |
| Material functions | 1 | Import (`MF_SmoothStep`) |
| Textures | 29 | Import |
| Meshes | 4 | Import (beam/orb geometry) |
| Color curves / atlas | 3 | Import |
| **Demo / Maps / built data** | **47** | **EXCLUDE** |
| **BP_Laser / BP_Laser_Orb** | **2** | **REFERENCE ONLY — do not import** |

97 importable, 49 excluded/reference.

## The integration contract (verified in the binaries)

Every beam Niagara system drives its endpoint from `BeamEmitterSetup.BeamEnd =
User.BeamEnd`, and its tint from `Color.Color = User.Color`. So the entire API is:

| Niagara user param | Type | Drive it with |
|---|---|---|
| `User.Beam End` | Position / Vector (world) | the confirmed/predicted impact point |
| `User.Color` | LinearColor | the weapon's beam tint |

OrbType systems additionally use a second system (`NS_OrbLaser_Center_*`) at the muzzle
for the charge/emitter orb. `BP_Laser` (reference only) showed the pattern: it set
`Beam End` from a per-tick `LineTraceSingle` + `BreakHitResult.ImpactPoint` — we keep
the param, discard the tick-trace.

Beam materials expose emissive/opacity with panning noise (`Speed`, `U_Speed_00/01`,
`V_Speed_00/01`) over fractal-noise textures — translucent/additive. That's why mobile
needs trimmed instances.

## Beam-type Niagara (NormalType) — 10

For straight beam weapons (Pulse, Prism, Melt, etc.). `→` shows the AFL name.

```
NS_Laser_Basic            → NS_AFL_Laser_Basic           (clean beam; good default Pulse)
NS_Laser_Basic_2          → NS_AFL_Laser_Basic_2
NS_Laser_Basic_3          → NS_AFL_Laser_Basic_3
NS_Laser_Colorful         → NS_AFL_Laser_Colorful        (gradient/atlas tinted)
NS_Laser_Electric_02_V2   → NS_AFL_Laser_Electric_02_V2  (arc/electric beam)
NS_Laser_Lava             → NS_AFL_Laser_Lava            (heaviest, ~3.6 MB — audit GPU)
NS_Laser_Twist            → NS_AFL_Laser_Twist           (spiraling; good Prism Beam)
NS_Laser_Twist_2          → NS_AFL_Laser_Twist_2
NS_Laser_Twist_3          → NS_AFL_Laser_Twist_3
NS_Laser_evel_2           → NS_AFL_Laser_evel_2
```

## Orb-type Niagara — 11 beams + 10 centers

For charge/orb weapons (Singularity Cannon, Nova). The beam pairs with a center orb.

```
NS_Electric_Orb_02        → NS_AFL_Electric_Orb_02
NS_OrbLaser_01..10        → NS_AFL_OrbLaser_01..10        (orb beams)
NS_OrbLaser_Center_01..10 → NS_AFL_OrbLaser_Center_01..10 (muzzle/charge orb)
```
(Vendor numbering is inconsistent — `_2/_3` vs `_01`; the manifest preserves it.)

## Materials — 18 masters

```
M_Beam              → M_AFL_Beam_Master            (primary beam master)
M_Laser_Basic       → M_AFL_Laser_Basic_Master
M_Laser_Colorful    → M_AFL_Laser_Colorful_Master
M_LaserTwist        → M_AFL_Laser_Twist_Master
M_Laser_evel        → M_AFL_Laser_evel_Master
M_Electric          → M_AFL_Electric_Master
M_Elec_Cylinder     → M_AFL_Elec_Cylinder_Master
M_thunder           → M_AFL_thunder_Master
M_Orb               → M_AFL_Orb_Master             (orb center)
M_Distortion        → M_AFL_Distortion_Master      (PC only; drop on mobile)
M_Spark / M_Splash / M_Smoke_8x8 / M_lighting      (impact + ambient)
M_Lavasplash / M_LavaMeshsplash / M_SW_Mesh / M_SW_Mesh_CCurve
```

## Meshes — 4

```
SM_NoCapCylinder  → SM_AFL_Laser_NoCapCylinder   (the beam tube mesh)
SM_Sphere / SM_Sphere_Labs / SM_HalfSphere       (orb / impact geometry)
```

## Curves — 3

```
CC_Laser_01, CC_Laser_2   → color curves (beam gradient over length/time)
CA_LaserAtlas             → curve atlas
```

## Recommended starter weapon mapping (design can re-pick)

| AFL weapon | Beam system | Muzzle/center | Material |
|---|---|---|---|
| Pulse Carbine (hitscan) | `NS_AFL_Laser_Basic` | — (BeamFlash only) | `M_AFL_Beam_Master` |
| Prism Beam (channeled) | `NS_AFL_Laser_Twist` | — | `M_AFL_Laser_Twist_Master` |
| Singularity Cannon (charge) | `NS_AFL_OrbLaser_05` | `NS_AFL_OrbLaser_Center_05` | `M_AFL_Orb_Master` |
| Nova Burst | `NS_AFL_Laser_Colorful` | `NS_AFL_OrbLaser_Center_01` | `M_AFL_Laser_Colorful_Master` |

These are pointers in each weapon's `DA_AFL_LaserVisual_*` data asset — swapping a look
is a data edit, never a code change.
