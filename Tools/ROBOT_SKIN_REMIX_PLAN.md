# BAG MAN — Robot Skin Remix Plan (Manny baseline → Blue / Green / Purple / Pink)

**Status:** PLAN ONLY. No authoring. Grounded in live material data + the operator's editor screenshots (2026-06-03).
**Goal:** Remix Lyra's Manny material into 4 BAG MAN robot skins, leaving the proven material flow intact — only swapping the chest logo (correctly sized) + the team/emissive colors per robot.

---

## ⭐ THE ROOT CAUSE the operator found (quantified)

The IRONICS "R" spilled across the torso because **our texture does NOT match the spec the logo slot expects.** The placement params are correct and shared — the *texture* is wrong.

| Property | ORIGINAL `T_UE_Logo_V2` (works) | Our `T_IRONICS_Logo_BC` (spills) | Verdict |
|---|---|---|---|
| **Dimensions** | **1024 × 1024** (power-of-2) | **1254 × 1254** (NOT pow-2) | ❌ FIX — must be 1024×1024 |
| **Compression** | `TC_Grayscale` | `TC_EditorIcon` | ❌ FIX — must be a proper body texture format |
| **sRGB** | **false** (linear) | **true** | ❌ FIX — must be false |
| **Mip Gen** | `TMGS_Blur5` (blurred mip chain) | `TMGS_NoMipmaps` | ❌ FIX — **this is the size bug** (see below) |
| Address X/Y | Wrap | Wrap | ✅ OK |

### Why "no mipmaps" = "logo too big"
The stock logo path samples the texture at **three mip levels** (`MipLevel_01=1`, `MipLevel_02=3`, `MipLevel_03=5`) and blends them into the 3-tier glow. That mip blend is what confines the mark to a small, soft chest emblem. **Our texture has no mip chain → all three "MipLevel" samples read full-res → the size-confining blend can't happen → the logo renders at full size and bleeds across the torso.** Power-of-2 (1024) is also required for clean mip generation + UV tiling; 1254 breaks both.

**→ Fix is the texture import settings, NOT the material wiring. The placement math already works.**

---

## The proven placement values (SHARED — do NOT change per robot)

From stock `MI_Manny_02_Blue` — these make the logo sit correctly on the chest. Every robot reuses these unchanged:

| Param | Value | Role |
|---|---|---|
| `LogoPos_X` | **-0.241** | chest UV horizontal position |
| `LogoPos_Y` | **0.259** | chest UV vertical position |
| `Scale` | **0.076** | logo size in UV space |
| `MipLevel_01 / 02 / 03` | **1 / 3 / 5** | the 3-tier mip blend (needs a real mip chain to work) |
| `HeightRatio_Input / _02 / _03` | 0.135 / 0.032 / 0 | parallax layer ratios |
| `BO_Height / _02 / _03` | 0 / 0 / 0 | bump-offset depth (off) |

---

## The TWO things that change PER ROBOT (the remix surface)

Everything else stays identical to Manny. Only two categories differ between Blue/Green/Purple/Pink:

### 1. The chest LOGO texture (`LogoTexture` param)
- Each robot/sponsor gets its own mark, **authored to the spec above** (1024×1024, TC_Grayscale OR a proper masked color format, sRGB=false, mips ON).
- ⚠️ **DECISION NEEDED** (see "Open question: mono vs color logo" below) — the stock slot is built for a **grayscale mask** recolored by the emissive. A full-color logo needs either (a) acceptance that it's recolored mono, or (b) our `MF_BagMan_ColorLogo` path, re-evaluated *with a correctly-sized texture* (the spill may have been the size bug, not the path).

### 2. The COLORS (vector + a few scalar params)
These are the per-robot identity. Lyra **already ships this exact pattern** — `MI_Manny_01_Blue/_Red` + `MI_Manny_02_Blue/_Red` exist; Green/Purple/Pink are the same recipe with new values.

| Param | Lives on | Manny Blue value | What it controls |
|---|---|---|---|
| **`TeamColor`** | `MI_*_01` (head/legs) | (0.055, 0.360, 0.969) blue | **the main suit accent color** ← primary per-robot knob |
| **`EmissiveColor`** | `MI_*_02` (torso) | (0.000, 0.420, 1.000) | logo/seam glow tier 1 |
| **`EmissiveColor2`** | `MI_*_02` | (0.000, 0.896, 1.000) | logo/seam glow tier 2 (brightest) |
| **`EmissiveColor3`** | `MI_*_02` | (0.000, 0.723, 1.000) | logo/seam glow tier 3 |
| **`EdgeGlowColor`** | master | blue (screenshot) | TRON edge-glow rim color |
| `CarbonfiberTint` | `MI_*_01` | (0.27,0.27,0.27) grey | suit base tint (can stay neutral) |
| `EmissiveStrength / 2 / 3` | `MI_*_02` | 16 / 64 / 256 | glow intensity tiers (keep) |
| `GlowBrightness` | `MI_*_02` | 0.5 | master glow level (keep) |

**Per-robot, you set ~5 color values** (`TeamColor` + `EmissiveColor1-3` + `EdgeGlowColor`) to the robot's hue. That's the whole color remix.

### Proposed color palettes for the 4 robots (starting points — operator tunes by eye)
| Robot | TeamColor (suit) | EmissiveColor glow family |
|---|---|---|
| **Blue** (IRONICS baseline) | (0.055, 0.360, 0.969) | the existing blue trio (keep as-is) |
| **Green** | ~(0.05, 0.90, 0.25) | (0,1,0.3) / (0.2,1,0.5) / (0,0.85,0.35) |
| **Purple** | ~(0.55, 0.10, 0.95) | (0.6,0,1) / (0.8,0.3,1) / (0.5,0,0.9) |
| **Pink** | ~(1.0, 0.10, 0.55) | (1,0.1,0.6) / (1,0.4,0.75) / (0.95,0.05,0.5) |

---

## Full parameter reference (from live `M_Mannequin` master)

**Texture params:** `Base_Tex`, `N_tex`, `DetailN_metal_Tex`, `LogoTexture` ←, `MASKTex`, `MaskTex1`, `MaskTex2`, `MaskTex3`
**Vector params:** `TeamColor` ←, `EmissiveColor` ←, `EmissiveColor2` ←, `EmissiveColor3` ←, `EdgeGlowColor` ←, `CarbonfiberTint`, `HitPosition0` (gameplay hit-fx, ignore)
**Logo/glow scalars:** `LogoPos_X` ←, `LogoPos_Y` ←, `Scale` ←, `MipLevel_01/02/03` ←, `BO_Height*`, `HeightRatio*`, `EmissiveStrength/2/3`, `GlowBrightness`, `ChromaticCurve`, `EdgeGlow*` (Frequency/Magnitude/FreqPow/Fresnel), `KillEdge*` (dissolve-death fx, ignore for skin)
**Suit scalars:** `metallic`, `LineBrightness`, `LineDesat`, `Carbon*` (carbon-fiber look)
**Static switch:** `UseLogo` (true = stock logo path draws)

---

## Architecture decision — where the skins live (clean, Lyra-canonical)

Mirror Lyra's own pattern (`MI_Manny_02_Blue` etc.) but BagMan-owned:

```
M_BagMan_CharacterBody          (master — already duplicated from M_Mannequin; generic, no per-robot values)
  ├─ MI_BagMan_Body_Blue        torso slot: blue EmissiveColor1-3 + IRONICS logo
  ├─ MI_BagMan_Body_Green       torso slot: green family + (sponsor) logo
  ├─ MI_BagMan_Body_Purple
  ├─ MI_BagMan_Body_Pink
  └─ (matching _HeadLegs instances OR one instance per robot covering both slots, carrying TeamColor)
```
- Master stays **generic** (no robot identity baked in) — every robot is just an instance with different color + logo values.
- `SKM_Manny` mesh + `SK_Mannequin` skeleton **untouched** (cardinal rule) — skins are material-only.
- This is exactly how Lyra ships Blue/Red — proven pattern, zero new architecture.

---

## ⚠️ Open question to settle BEFORE authoring: mono vs full-color logo

The stock `LogoTexture` slot treats the texture as a **grayscale mask** and recolors it via `EmissiveColor*`. Two paths:

- **Path M (mono, simplest, matches Epic):** author each mark as a **grayscale 1024² mask** (like `T_UE_Logo_V2`). The logo glows in the robot's `EmissiveColor` hue. Clean, proven, but the mark is single-hue (can't show e.g. Google's 4 colors verbatim). **Recommended for the 4 baseline robots** — they're team-colored anyway, so a mono mark in the team hue is on-brand.
- **Path C (full-color):** keep our `MF_BagMan_ColorLogo` additive path, **re-test it with a correctly-sized 1024² texture** — the torso spill may have been the size/mip bug, not the path itself. Only needed if a robot's mark must be multi-color verbatim (sponsor logos like Google/Cesium).

**The size fix (1024² + mips + grayscale + sRGB-off) must happen FIRST regardless** — it's the prerequisite for either path to place correctly.

---

## Proposed execution sequence (when approved — NOT now)

1. **Revert the experiments** (operator's call): either revert `MF_logo` to baseline (`0762549…`) and rely on our BagMan master, OR keep operator's `MF_logo` edits — decide which `MF_logo` is the baseline.
2. **Re-author the IRONICS texture to spec** (1024×1024, mips ON, correct compression, sRGB off) — re-import or resize the source PNG. Repeat for each robot/sponsor mark.
3. **Pick Path M or Path C** (logo color mode) per the decision above.
4. **Author the 4 instances** (`MI_BagMan_Body_<Color>`) — each = stock placement values + robot color set + its logo texture. (Copy Lyra's `MI_Manny_02_Blue` as the template; change colors + logo.)
5. **View each on Manny** (scratch level, not sphere) — confirm one chest mark, correct color, blue/green/purple/pink suit.
6. **Then** wire the chosen skin to the pawn → Block B (PIE watch).

---

## What we are NOT changing (leave intact — the whole point)
- `SKM_Manny` geometry / `SK_Mannequin` skeleton / ref pose — untouched (animations preserved).
- The 84-node `MF_logo` placement/parallax/mip machinery — it works; we feed it a correctly-sized texture.
- The suit's PBR/carbon-fiber/edge-glow structure — keep; only re-tint via params.
- Placement values (`LogoPos_X/Y`, `Scale`, `MipLevel*`) — shared, proven, identical across all robots.
