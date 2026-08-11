# HANDOVER — Venue key art (NANOWATT · ARCANEON · INFINEON)

**Process:** `IRONICS_ART_HANDOVER_PROCESS.md`. Hand this file to `/design` as written.
**Raised:** 2026-08-10, when S8 VenueShowcase shipped with three venues and no art.

---

## What and where

Three pieces of key art, one per **venue**. A venue is a **level**, not a playlist — ARCANEON runs four
playlists on one map and appears **once**, because a list of configurations is a config picker
(`ui-frontend.md` §8).

They are consumed by **S8 VenueShowcase** in two places at two very different sizes:

| Consumer | Size | What it needs to survive |
|---|---|---|
| `WBP_IRONICS_VenueTile` → `TileArt` | **280 × 170** | Reading as a distinct place at thumbnail scale |
| `WBP_IRONICS_VenueShowcase` → `VenueArt` | Full-bleed detail panel, ~1180 wide | Holding up large with text over its lower third |

Landing property: `FAFLVenueEntry::KeyArt` (`TSoftObjectPtr<UTexture2D>`), set per row on the
`WBP_IRONICS_VenueShowcase` CDO.

Asset paths — one per venue, following the surrounding folder's convention:

```
/Game/UI/IRONICS/Venues/T_Venue_Nanowatt
/Game/UI/IRONICS/Venues/T_Venue_Arcaneon
/Game/UI/IRONICS/Venues/T_Venue_Infineon
```

⚠ **Nothing is blocked.** The slots are soft pointers and the detail panel **hides** the image when unset
rather than showing the previous venue's art under a new venue's name. An empty slot is a correct state.

---

## Reference Artifact

`Docs/reference/IRONICS_Styling_Sample.html` — take the **glass-and-neon treatment at component scale**:
how Electric reads as a core light source and how Violet sits on an edge without ever becoming the
subject.

Secondary: `Docs/design/IRONICS_Home_Screen_Mockup.html` for the density and calm of the chrome these
tiles sit inside. **The art must not out-shout the surface it lives on** — the showcase is the one screen
where the art is the content, but it still sits under IRONICS chrome.

---

## Locked constraints — cited, not restated

- **Palette:** `IRONICS_UI_STYLE_SSOT.md` §2.1 house colours. `UI.House.Electric` is the light source;
  `UI.House.Black` is the ground; `UI.House.Blue` is available as atmosphere.
- **Blend rule (LAW):** Electric = core / fill / active. **Violet is rim, edge, focus and hover only, and
  never touches readable core, fill or text.** Rim is **size-gated: on at ≥64px, core-dominant at ≤32px**
  — which matters here because the same image serves a 280px tile and a full-bleed panel.
- **Panels:** §3. The detail image sits **behind** a `BS_IRONICS_Glass_Primary` border with a 20–40px
  blur band, so the art's lower third will be read **through glass with text over it**.
- **Readability:** §2.4. The bottom ~30% of the detail image carries `VenueClassLabel`, `VenueName` and
  `VenueBlurb`. That region must stay quiet enough for white text at display weight.
- **Type:** §4 owns all type. **No words baked into the art** — not the venue name, not a logo lockup.
  The name is a `CommonTextBlock` and must remain the only place it exists.

---

## Composition

- **One recognisable place per image.** A player who has seen this should recognise the level in play;
  that is the entire justification §8 gives for the surface existing.
- **Focal point in the upper two thirds.** The lower third is a text shelf.
- **Safe area:** keep the subject clear of the outer 8% — the tile crops harder than the panel.
- **Distinguish the three at thumbnail size.** Squint at 280px: if two of them read as the same blue
  corridor, the set has failed regardless of how each looks alone.

### Per venue — the identity to hit

| Venue | Level | R97 class | What the image has to say |
|---|---|---|---|
| **NANOWATT** | `L_Arena_01` | **ARENA** | A contained speed arena. Short sightlines, tight rotations, everything close. Read: *fast*. |
| **ARCANEON** | `L_Arena_04` | **ARENA** | The flagship. Symmetric, multi-tier, built for the Pro Mod line. Read: *the main stage*. |
| **INFINEON** | `L_Expanse` | **MAP** | District-scale. Long lanes, open ground, sky. Read: *a different game to the arenas*. |

⚠ **The ARENA/MAP distinction must be visible.** R97 makes venue class a real queue dimension, and the
registry warns that getting it wrong means "players getting the style they did not choose". The two
arenas should feel enclosed and built; INFINEON should feel like territory. If a player cannot tell which
is which from the art alone, the art is under-selling a rule the matchmaker enforces.

---

## Deliverables

- **3 images**, 16:9, **2560 × 1440** master (downsampled in-engine for the 280 × 170 tile).
- PNG, no alpha, no baked text.
- Named `T_Venue_<Name>` exactly as listed above.

---

## What this asset must NOT do

- **No text, no logos, no venue names.** §4 owns type.
- **No UI furniture** — no frames, no glass, no HUD elements. The surface supplies all of that; art that
  brings its own frame will double up against the `BS_IRONICS_Glass_Primary` border.
- **No Violet as a fill or a light source.** Rim and edge only. This is the constraint most likely to be
  broken by a striking-looking result, and it is the one that has already cost this project a full
  palette sweep (§2.6).
- **No players, no characters.** These are places. A figure makes the image about a moment.
- **No new hues.** If the composition seems to need one, that is a §2 conversation, not a brush.
- **No busy lower third.** Text goes there.
