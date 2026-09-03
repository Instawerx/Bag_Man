# AFL Design Overrides — IRONICS

> **Identity.** **IRONICS** is the player-facing game name; **AFL** is the internal code/asset prefix
> (`AFLW_`, `M_AFL_`, `ID_AFL_`, …) and never appears on a player-facing surface. Real IRONICS
> logos/art only on surfaces. **Partner-brand exception:** **Simularent** carries its own brand
> treatment where it appears — named here, specced separately (not in this file).

## When AFL Context is Detected

Apply these AFL-specific overrides on top of the general design system. The AFL / BAG MAN / IRONICS
look is **cyber / neon — electric neon blue + purple.** Values below are ratified; **do not invent —
every hex traces to the cited source line.**

> ### ⛔ SUPERSEDED 2026-09-03 — retired Apple-Glass direction
> *Kept visible for the trail, not for use. Superseded by `Docs/Hub/IRONICS_CC_DESIGN_BRIEF.md` §0 +
> `Source/LyraEditor/AFL/AFLTokenCompiler.cpp` / `AFLUITheme.h` — see the ratified system below.*
>
> ~~Frosted-glass surfaces · white panels · accent `#64B4FF` · SF Pro type~~ — **all retired.** No
> frosted glass, no white panels, no `#64B4FF`, no SF Pro. (Brand corrected to cyber/neon per the
> tracker's 2026-06-07 store ruling; locked in `IRONICS_CC_DESIGN_BRIEF.md` §0 + intent I-24 in
> `IRONICS_CC_INTEGRATION_PLAN.md`. This file previously carried the Apple-Glass tokens inline —
> AFL-3202 supersedes them here.)

---

## AFL Visual Identity

```
Studio/Project:  AFL (BAG MAN · IRONICS)
Engine:          UE5.6 / Lyra Starter Game
Platforms:       PC, PS5, XSX, iOS, Android
UI Stack:        CommonUI + LyraPrimaryGameLayout
Design Language: Cyber / neon — electric neon blue + purple, emissive rim-glow, gradient panels,
                 subtle grid, scan-line restraint
```

---

## AFL Design Tokens (Canonical — brand lock, I-24)

Every value cites the source line it greps back to. The **brand palette** is ruled in
`Docs/Hub/IRONICS_CC_DESIGN_BRIEF.md` §0; the **chrome / type / geometry** layer is ruled in
`Source/LyraEditor/AFL/AFLTokenCompiler.cpp`. The palette does NOT live in the style-compiler code —
do not read palette hexes out of `AFLUITheme.h` (see the DRIFT note at the end of this section).

### Brand palette — `IRONICS_CC_DESIGN_BRIEF.md` §0
```
ground:        #222A3A   page/scene ground, viewport backdrop              [brief §0 · L28]
surface-card:  #0E122B   panels, cards, rails, bars                        [brief §0 · L29]
accent:        #1E5AFF   Electric Neon Blue — focus, primary CTA,          [brief §0 · L30]
                         active/selected, selection ring                    [+ AFLTokenCompiler.cpp L498/L514 "UI.House.Electric"]
watts:         #FF00D5   Magenta — Watts currency / staked-premium mark,   [brief §0 · L31]
                         NEVER a general accent
```

### Chrome accents — electric neon + purple (NO CYAN)
```
Electric  #1E5AFF   core / fill / active / selection                       [AFLTokenCompiler.cpp L498, L514]
Violet    #A855F7   purple — rim / focus / hover; never on readable        [AFLTokenCompiler.cpp L499, L515]
                    core/fill/text (size-gated on at >=64px)
HARD RULE — NO CYAN: cyan is never a UI colour, an accent, or chrome. The chrome accents are
   electric neon blue + purple, full stop. Cyan exists ONLY as an internal rarity / colour-
   identity DATA value resolved through the registry (GetRarityColor) — never design furniture.
ONE-BLUE RULE: #1E5AFF is the only chrome blue; #00ADFF (free base identity) is RETIRED from
   chrome — a second chrome blue is a compile failure.                     [AFLTokenCompiler.cpp L460-465]
```

### Support colours (data / status only — never chrome, never accents)
```
green #3DDC84 · amber #FFB020 · red #FF3355
Rarity is a badge axis (frame colour via GetRarityColor), never the identity colour.
Anything not listed here is PROPOSED and must be ratified before use — never invented.
```

### Surface system (replaces the retired Glass Panel System)
```
Panels/cards:  surface-card fills, 1px line borders (accent-tinted, #2C3550 family), emissive accent
               rim on focus/active; gradient panels allowed; subtle grid; scan-line restraint.
Mobile:        no blur-dependent surfaces; solid surface-card fallbacks (GPU overdraw rules below).
```

### Geometry — house constants (`AFLTokenCompiler.cpp`)
```
radius:  panel 20 · button 12 · input 8      blur: 28                      [L56, L206]
border:  1px solid on every structural edge — neon rims are OUTLINES, not fills   [L258]
```

> **⚠ CODE DRIFT — owed, separate code correction (NOT this docs commit):** `AFLUITheme.h` still
> hard-codes values that contradict the ratified system and must be purged in a follow-up commit:
> `CyanActive` (L37) + `TabFill` (L43) are **cyan chrome** — the active/selected colour must be
> Electric `#1E5AFF`, hover Violet `#A855F7`; and `WattsNeon = #FF2D9E` (L34) is stale — ratified
> Watts is `#FF00D5`. This doc is authoritative; the theme header is the outlier.

### Typography Scale — RULED 2026-08-10 (`AFLTokenCompiler.cpp` L107-126, L937)
```
Display: Orbitron        — titles, region headers, build name, big numbers (identity-carrying text)
Body:    NotoSans        — labels, descriptions, prompts
Data:    DroidSansMono   — prices, slot counters "n / cap", ids, stats (tabular; digits must not jitter)

Display:    48px / SemiBold  — hero headers, game over screens
H1:         32px / SemiBold  — menu headers
H2:         24px / Medium    — section titles
Body:       17px / Regular   — descriptions, instructions
Caption:    14px / Regular   — labels, tags
Micro:      12px / Regular   — version numbers, fine print
HUD Large:  28px / SemiBold  — ammo current, health value
HUD Small:  16px / Regular   — ammo reserve, stat labels
```

---

## AFL UI Layer Routing

All AFL UI goes through `ULyraPrimaryGameLayout`:

```
UI.Layer.Game      → AFLW_HUD_Root (health, ammo, minimap, abilities)
UI.Layer.GameMenu  → AFLW_Menu_Pause, AFLW_Menu_Inventory
UI.Layer.Menu      → AFLW_Menu_Main, AFLW_Menu_Settings, AFLW_Menu_Lobby
UI.Layer.Modal     → AFLW_Modal_Confirm, AFLW_Modal_Alert, AFLW_Modal_Reward
```

---

## AFL Asset Naming (Design Assets)

| Asset | Convention | Example |
|---|---|---|
| Widget Blueprint | `AFLW_[Layer]_[Name]` | `AFLW_HUD_HealthBar` |
| Widget Style | `AFLS_[Type]_[Name]` | `AFLS_Btn_Primary` |
| UI Texture Atlas | `T_AFL_UI_[Category]` | `T_AFL_UI_Icons` |
| UI Material | `M_AFL_UI_[Name]` | `M_AFL_UI_NeonPanel` |
| Environment Material | `M_AFL_Env_[Name]` | `M_AFL_Env_WetConcrete` |
| Character Material | `M_AFL_Char_[Name]` | `M_AFL_Char_PlayerArmor` |
| Post Process Material | `M_AFL_PP_[Name]` | `M_AFL_PP_ScanLine` |
| MPC | `MPC_AFL[Scope]` | `MPC_AFLWorld`, `MPC_AFLUI` |

---

## AFL Platform Design Rules

### Mobile (iOS / Android) — Non-Negotiable
```
[ ] BackgroundBlur disabled → LowQualityFallback solid panel
[ ] Minimum touch target: 44×44pt
[ ] SafeZone widget wraps all root layouts
[ ] Text min size: 14px (12px absolute floor)
[ ] No transparency-on-transparency stacking (GPU overdraw)
[ ] Simplified materials: M_[Name]_Mobile variants required for all UI materials
[ ] Orientation: locked landscape (unless game is portrait)
```

### Console (PS5 / XSX) — Design Rules
```
[ ] Overscan safe area: 5% inset from all edges (TV safe zone)
[ ] Button prompts use UCommonActionWidget — platform-adaptive icons
[ ] No hover states — only focus states (no mouse cursor)
[ ] Focus highlight: accent (#1E5AFF) rim brightens + subtle scale 1.02
[ ] Navigation: all menus fully navigable with D-pad
[ ] Font minimum: 20px at 1080p for comfortable 3-meter viewing
```

### PC — Design Rules
```
[ ] Scalable UI: test 1280×720 through 4K
[ ] Hover states enabled for all interactive elements
[ ] Keyboard shortcut indicators shown (Esc, Tab, etc.)
[ ] Adjustable HUD scale option in settings (0.8x – 1.4x)
```
