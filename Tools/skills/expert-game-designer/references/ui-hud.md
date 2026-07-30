# UI / HUD Design Reference

## HUD Design Principles (Game UI)

### Information Hierarchy
```
Tier 1 — Always visible, peripheral vision:
  Health / shield, ammo, minimap
  → Minimal, ambient, never demand focus

Tier 2 — Contextual, attention on demand:
  Objective markers, interaction prompts, ability cooldowns
  → Appear/fade based on relevance

Tier 3 — Intentional, player-opened:
  Inventory, map, settings, pause menu
  → Full panel, player has stopped to look
```

### Apple Glass HUD Layout — Standard AFL Template
```
┌──────────────────────────────────────────────────────────┐
│ [Minimap]                              [Objective Text]  │  ← Tier 2 corners
│  Glass/XS                               Glass/Pill       │
│                                                          │
│                                                          │
│                    [Crosshair]                           │  ← Minimal, dot/cross
│                                                          │
│                                                          │
│ [Health Bar]    [Ability Icons]         [Ammo Counter]   │  ← Tier 1 bottom
│  Glass/Bar       Glass/Grid             Glass/Number     │
└──────────────────────────────────────────────────────────┘
```

### Glass HUD Component Specs

**Health Bar (Glass)**
```
Container:  width=240px, height=8px, radius=4px
Background: rgba(255,255,255,0.12), border rgba(255,255,255,0.20)
Fill:       gradient left→right, rgba(80,200,120,0.9) → rgba(60,180,100,0.9)
Danger:     fill color shifts to rgba(255,80,80,0.9) at <25%
Animation:  on damage — flash white 0.1s, then smooth drain
Blur:       BackgroundBlur 16px on container
```

**Ammo Counter (Glass)**
```
Container:  auto-width, height=48px, radius=12px, padding=12px 16px
Background: rgba(255,255,255,0.10), border rgba(255,255,255,0.18)
Current:    Text/Primary, 28px, SemiBold
Separator:  Text/Tertiary, 18px, "/" 
Reserve:    Text/Secondary, 18px, Regular
Animation:  pulse scale 1.0→1.05→1.0 on fire, 0.12s ease
```

**Ability Icon Grid (Glass)**
```
Per Icon:   64×64px container, radius=14px
Background: rgba(255,255,255,0.10)
Ready:      Border rgba(255,255,255,0.25), icon full opacity
Active:     Border rgba(100,200,255,0.8), glow 0 0 12px rgba(100,200,255,0.5)
Cooldown:   Radial overlay rgba(0,0,0,0.55), countdown text center
Spacing:    8px gap between icons
```

---

## Menu Design — Apple Glass Panels

### Main Menu Layout
```
Background:   Full-screen game world (blurred, 60% darkened)
Panel:        Center, 480×600px, Glass/Primary, radius=24px
Header:       Game logo or title, top 80px
Nav Items:    Full-width rows, 64px height, hover = Glass/Secondary
              Left icon (24px) + Label (Text/Primary 17px SemiBold)
              Right chevron (Text/Tertiary)
Footer:       Version string, Text/Tertiary 12px
Entry Anim:   Panel slides up 24px + fade in, 0.35s ease-out-cubic
Exit Anim:    Scale 0.96 + fade out, 0.2s ease-in
```

### Settings Panel (Nested Glass)
```
Outer Panel:  560×700px, Glass/Primary
Section Card: Full-width, Glass/Secondary, radius=16px, padding=16px
              Stacked inside outer panel with 12px gap
Toggle Row:   Label left (Text/Primary) + Toggle right (iOS-style pill)
Slider Row:   Label + value (Text/Accent) + Slider below
              Slider track: Glass/Tertiary, fill: Glass/Tint/Primary
Divider:      1px rgba(255,255,255,0.08) between rows
```

---

## Midjourney Prompts — UI Concepts

### Glass HUD Concept
```
/imagine a futuristic game HUD interface with Apple visionOS-inspired frosted glass 
panels, translucent UI elements floating in 3D space, minimal design, health bar 
and ammo counter in bottom corners, soft gaussian blur behind panels, white text 
on glass, subtle inner glow, dark atmospheric game environment visible through UI, 
concept art, UI/UX design, 8K --ar 16:9 --style raw --v 6
```

### Main Menu Glass
```
/imagine game main menu screen, visionOS spatial design aesthetic, frosted glass 
panel center screen, soft bokeh background with cinematic game world, rounded 
rectangle panel with translucent blur, menu items with chevrons, subtle glass 
border highlight, dark ambient lighting, premium game UI concept, --ar 16:9 --v 6
```

---

## NeoStack AIK Prompts — UMG Widgets

### Glass Health Bar Widget
```
Create a UMG Widget Blueprint called AFLW_HUD_HealthBar_Glass:
- Root: Canvas Panel (full screen anchor)
- Container: Size Box 240×8px, anchored bottom-left, offset 24px from edges
- Background Image: solid white fill, opacity 0.12, BackgroundBlur strength 16
- Fill Progress Bar: 0-1 float, color (0.3, 0.8, 0.47, 0.9), no border
- Border overlay: 1px white at opacity 0.20, rounded corners (border image)
- Bind Fill percent to Health/MaxHealth from owning pawn's LyraHealthComponent
- On health change: play animation — flash white 0.1s, then tween to new value
Follow AFL naming conventions. Use CommonUI base class UCommonUserWidget.
```

### Glass Ammo Counter Widget
```
Create a UMG Widget called AFLW_HUD_AmmoCounter_Glass:
- Container: Horizontal Box, BackgroundBlur 16, background rgba white 0.10, 
  padding 12/16, border radius via 9-slice border image
- CurrentAmmo: CommonTextBlock, font size 28, SemiBold, white opacity 1.0
- Separator: CommonTextBlock "/" font size 18, white opacity 0.45
- ReserveAmmo: CommonTextBlock font size 18, white opacity 0.7
- Bind to weapon component via delegate — update on fire and reload
- Fire animation: scale 1.0→1.05→1.0 over 0.12s on ammo decrement
```
