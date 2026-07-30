# AFL Design Overrides

## When AFL Context is Detected

Apply these AFL-specific overrides on top of the general design system.

---

## AFL Visual Identity

```
Studio/Project:  AFL
Engine:          UE5 / Lyra Starter Game
Platforms:       PC, PS5, XSX, iOS, Android
UI Stack:        CommonUI + LyraPrimaryGameLayout
Design Language: Apple Glass — translucent, spatial, luminous, minimal
```

---

## AFL Design Tokens (Canonical)

### Brand Colors
```
AFL/Brand/Primary:    #64B4FF  — main accent, CTA buttons, highlights
AFL/Brand/Secondary:  #8066FF  — secondary accent, special abilities
AFL/Brand/Danger:     #FF5050  — damage, warnings, enemy indicators
AFL/Brand/Success:    #50FF8C  — pickups, healing, positive feedback
AFL/Brand/Gold:       #FFD060  — premium, legendary, ranked
```

### Glass Panel System (AFL-tuned)
```
AFL/Glass/Panel:      rgba(255,255,255, 0.11) + blur 24px
AFL/Glass/Card:       rgba(255,255,255, 0.08) + blur 16px
AFL/Glass/Button:     rgba(255,255,255, 0.16) hover→0.22, border 0.24
AFL/Glass/Input:      rgba(255,255,255, 0.07) focus→border AFL/Brand/Primary
AFL/Glass/Overlay:    rgba(10, 14, 24, 0.72) — modal/menu background scrim
AFL/Glass/Blur:       24px standard | 16px compact | 36px hero panels
```

### Typography Scale
```
AFL uses SF Pro Display (or Nunito Sans as web fallback)
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
| UI Material | `M_AFL_UI_[Name]` | `M_AFL_UI_Glass` |
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
[ ] Focus highlight: Glass/Button border brightens + subtle scale 1.02
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

---

## AFL Design → Sprint Handoff Template

```markdown
## AFL Design Handoff: [Feature Name]
Date:       [Date]
Designer:   @name
Sprint:     AFL Sprint [N]

### Approved Designs
- [Link to SVG/mockup artifact]
- [Link to Midjourney hi-fi concept]

### Generated AIK Assets (needs polish)
- AFLW_[Name] — created via NeoStack, needs review
- M_AFL_[Name] — material params to verify

### Sprint Tasks Generated
AFL-XXX Implement AFLW_[Name] UMG widget (M) @engineer
AFL-XXX Create T_AFL_UI_[Name] texture atlas (S) @artist
AFL-XXX Wire AFLW_[Name] to LyraPrimaryGameLayout Game layer (S) @engineer
AFL-XXX Mobile: implement LowQualityFallback for AFLW_[Name] (S) @engineer
AFL-XXX QA: AFLW_[Name] on Win64 + PS5 + iOS (M) @qa

### Open Questions
- [ ] [Any unresolved design decision]
```
