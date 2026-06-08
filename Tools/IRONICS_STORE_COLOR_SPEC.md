# IRONICS Neon-Glass — Store Color-Identity Spec (SSOT)

> Durable spec for the **cosmetic color-identity registry** (`DA_AFL_ColorIdentityRegistry`).
> Authored 2026-06-08 (operator-approved, option B per-product edges). This is the
> DECLARED color data — the registry is the asset, this doc is the human SSOT for the
> values. Architecture: colors DECLARED here → items reference by `ColorIdentityTag`
> → every surface (card / Slice-2 showroom / equipped char) RESOLVES the tag uniformly.
> No per-card derivation (that was the pooled-tile bug class). Matches the
> lyra-skin-builder-marketplace TeamColorPalette pattern.

## The registry rows (IdentityTag → Primary + Accent)

Primary = the dominant neon (card frame/border glow, showroom pedestal key).
Accent = the contrast/secondary neon (card edge accent, inner detail, showroom rim).
Primary values mirror each edge's in-game `EdgeGlowColor` (`DA_AFL_Edge_*::ColorParameters["EdgeGlowColor"]`)
so the store card matches the skin worn on the robot. Accent = a deliberate contrast pair.

| IdentityTag | Product(s) | Primary (RGB) | Accent (RGB) | Note |
|---|---|---|---|---|
| `Cosmetic.Identity.NeonBlue`   | Neon Blue Edge   | `(0.00, 0.42, 1.00)` blue   | `(0.00, 0.94, 1.00)` cyan   | blue + cyan glass |
| `Cosmetic.Identity.NeonGreen`  | Neon Green Edge  | `(0.00, 1.00, 0.30)` green  | `(0.00, 0.94, 1.00)` cyan   | green + cyan |
| `Cosmetic.Identity.NeonPink`   | Neon Pink Edge   | `(1.00, 0.10, 0.60)` pink   | `(0.60, 0.00, 1.00)` purple | pink + purple (the operator's ideal pairing) |
| `Cosmetic.Identity.NeonPurple` | Neon Purple Edge | `(0.60, 0.00, 1.00)` purple | `(1.00, 0.10, 0.60)` pink   | purple + pink (inverse of NeonPink) |
| `Cosmetic.Identity.NeonRed`    | Neon Red Edge    | `(1.00, 0.05, 0.05)` red    | `(1.00, 0.45, 0.00)` orange | red + orange (warm pair) |
| `Cosmetic.Identity.EMP`        | EMP Burst        | `(0.00, 0.42, 1.00)` blue   | `(0.00, 0.94, 1.00)` cyan   | EMP is a blue energy burst |
| `Cosmetic.Identity.IronicsVisor` | Ironics Visor  | `(0.00, 0.94, 1.00)` cyan   | `(0.00, 0.42, 1.00)` blue   | cyan visor + blue |
| `Cosmetic.Identity.NeonEdge`   | (collection header, reserved) | `(0.00, 0.42, 1.00)` blue | `(0.00, 0.94, 1.00)` cyan | not item-assigned; kept for a future collection view |

All alpha = 1.0 (RGB neons; the card applies its own alpha per surface).

## Catalog wiring (each entry's `ColorIdentityTag`)

| CosmeticId | ColorIdentityTag |
|---|---|
| `AFL.Edge.NeonBlue`   | `Cosmetic.Identity.NeonBlue` |
| `AFL.Edge.NeonGreen`  | `Cosmetic.Identity.NeonGreen` |
| `AFL.Edge.NeonPink`   | `Cosmetic.Identity.NeonPink` |
| `AFL.Edge.NeonPurple` | `Cosmetic.Identity.NeonPurple` |
| `AFL.Edge.NeonRed`    | `Cosmetic.Identity.NeonRed` |
| `AFL.Ability.EMP`     | `Cosmetic.Identity.EMP` |
| `AFL.Facemask.IroVisor` | `Cosmetic.Identity.IronicsVisor` |
| `AFL.Team.*` (6 brands) | (none yet — GrantedFree identity bases, not shop cards; add when they get cards) |

## Card surface → color role (the uniform reader, applied identically to every card)

- **Frame / border glow** → `PrimaryColor` (via `GetEntryPrimaryColor`)
- **Edge accent / inner detail / price chip** → `AccentColor` (via `GetEntryAccentColor`)
- **Background glass** → `PrimaryColor × ~0.12` (faint tint, not flat dark)
- **Rarity badge (LEGENDARY label / small pip)** → SEPARATE axis: `GetRarityColor` / `GetRarityText` (NEVER shares a slot with identity — the stray-yellow bug was rarity bleeding into the identity slot)

## Scaling (why this is the AAA answer)

System is FIXED (registry + tag-reference + uniform readers); DATA varies (these rows).
Re-theming a season = edit this registry. New product = author 1 row + set its tag.
Grows in BULK with the catalog per the tracker's asset-batch sequencing. Mobile-safe
(color values are free; zero per-item materials). Identity carries store → showroom → in-match.

Related: economy values = `IRONICS_ECONOMY_SPEC.md`; brand robot pairings (separate, in-game skin) =
`Tools/skills/lyra-skin-builder-marketplace/references/bagman-brand-default-mapping.md`.
