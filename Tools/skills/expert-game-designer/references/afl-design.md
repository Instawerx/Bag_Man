# AFL Design Overrides

## When AFL Context is Detected

Apply these AFL-specific overrides on top of the general design system. **They REPLACE the
general Apple-Glass aesthetic entirely for AFL / BAG MAN / IRONICS work — no frosted glass, no
white panels, no `#64B4FF`, no SF Pro.** (Brand corrected to cyber/neon per the tracker's
2026-06-07 store ruling; locked in `Docs/Hub/IRONICS_CC_DESIGN_BRIEF.md` §0 and intent I-24 in
`Docs/Hub/IRONICS_CC_INTEGRATION_PLAN.md`. This file previously carried the retired Apple-Glass
direction — that drift is what AFL-3202 removed.)

---

## AFL Visual Identity

```
Studio/Project:  AFL (BAG MAN · IRONICS)
Engine:          UE5.6 / Lyra Starter Game
Platforms:       PC, PS5, XSX, iOS, Android
UI Stack:        CommonUI + LyraPrimaryGameLayout
Design Language: Cyber / neon — emissive rim-glow, gradient panels, subtle grid, scan-line restraint
```

---

## AFL Design Tokens (Canonical — brand lock, I-24)

### Brand Colors (ruled in `AFLTokenCompiler.cpp`; supersedes anything else you may read)
```
ground:        #222A3A  — page/scene ground, viewport backdrop
surface-card:  #0E122B  — panels, cards, rails, bars
accent:        #1E5AFF  — Electric Neon Blue: focus, primary CTA, active state, selection ring
watts:         #FF00D5  — Magenta: Watts currency, staked/premium marking — NEVER a general accent
```

### Support colors (from the ratified roadmap CSS — data/status use only)
```
green #3DDC84 · amber #FFB020 · red #FF3355 · cyan #00E5FF
HARD RULE: cyan is DATA-ONLY (rarity/colour-identity values) — never UI chrome.
Rarity is a badge axis (frame colour via GetRarityColor), never the identity colour.
Anything not listed here is PROPOSED and must be ratified before use — never invented.
```

### Surface system (replaces the retired Glass Panel System)
```
Panels/cards:  surface-card fills, 1px line borders (#2C3550 family), emissive accent rim on
               focus/active; gradient panels allowed; subtle grid; scan-line restraint.
Mobile:        no blur-dependent surfaces; solid surface-card fallbacks (GPU overdraw rules below).
```

### Typography Scale
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
