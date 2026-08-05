# ShantyTown — Pack Provenance

**Why this file exists:** the pack ships **no LICENCE and no README of its own**. All 407 files
are `.uasset`/`.umap` — there is not a single text file in it. Without this record the repo
carries 2.4 GB of third-party content with no on-disk statement of where it came from or what
we are permitted to do with it. That is the gap this file closes.

## Source and licence

| | |
|---|---|
| **Source** | **Fab Marketplace** |
| **Cost** | **Free** |
| **Licence** | **Free to use, convert and modify** |
| **Authority** | **Operator, 2026-08-04** — recorded verbatim from the operator's confirmation |
| **Files** | 407 (all `.uasset`/`.umap`; zero text/licence files) |
| **On-disk size** | 2.4 GB |
| **Tracked** | Yes — all 407 in git, via the blanket `*.uasset` / `*.umap` LFS rules (`.gitattributes:1-2`) |

## Pack identity — INFERRED, not confirmed

The exact Fab listing name is **not recorded in any asset**. No embedded copyright, author,
vendor or URL string was found in a representative asset scan.

The vendor is identifiable from asset naming with reasonable confidence:

- `Content/ShantyTown/DKO_Widget/` — a vendor-prefixed subfolder
- `M_Dokyo_Green_01`, `MI_Dokyo_01` — "Dokyo" appears in material names
- `DKO` reads as an abbreviation of `Dokyo`

> **TODO — confirm the exact Fab listing title and publisher against the account's Fab library,
> and record the listing URL here.** The inference above rests on asset naming alone and should
> not be cited as the licence source; the licence authority is the operator confirmation above.

## Layout

```
Audio/            Audio/, Sound_Cues/            Ambient_Birds_01, SC_Ambient_Birds_01
Blueprints/       Buildings/, Doors/, Fence/, Misc/
DKO_Widget/       Blueprints/, Materials/, Models/, Textures/   (vendor demo widget)
Maps/             Demo_Map.umap                  the vendor demo level
```

Mesh families present: brick / concrete / iron / metal / chain-link, grass and rock textures —
consistent with the modular shanty + perimeter-fence kit described in
[`Docs/maps/ShantyTown_BR_DESIGN.md`](../../Docs/maps/ShantyTown_BR_DESIGN.md) §1.

## Integration status at time of writing

- **Nothing outside `Content/ShantyTown/` references this pack.** A `git grep` across all tracked
  files for `/Game/ShantyTown` and `Content/ShantyTown`, excluding the pack itself, returns exactly
  one hit — the design brief. No shipped AFL content depends on it.
- `Maps/Demo_Map.umap` is the **vendor demo level**, monolithic (no `__ExternalActors__`,
  no `__ExternalObjects__`, no HLODs), 57.78 MB.

Related: [`Docs/maps/ShantyTown_BR_DESIGN.md`](../../Docs/maps/ShantyTown_BR_DESIGN.md)
