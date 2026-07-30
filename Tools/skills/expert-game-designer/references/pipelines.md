# Design Pipelines Reference

## Claude Design Pipeline — Full Workflow

```
STAGE 1: CONCEPT (Claude inline)
  → SVG diagram / wireframe / mood board rendered in chat
  → Design spec written in markdown
  Purpose: Establish layout, structure, color direction, hierarchy

STAGE 2: HI-FI ART (AI Image Generation)
  → Midjourney v6 prompt (primary recommendation)
  → DALL·E 3 prompt (alternative, faster iteration)
  → Stable Diffusion prompt (for local/offline generation)
  Purpose: Photorealistic or stylized concept art for approval

STAGE 3: IN-ENGINE (NeoStack AIK)
  → Agent Integration Kit prompt for UE5 editor
  → Targets: UMG widgets, Materials, BPs, Niagara, BehaviorTrees
  Purpose: AI generates the actual UE5 asset from the approved concept

STAGE 4: POLISH (Human + AI iterate)
  → Review AIK output, refine parameters
  → Test in-game, capture screenshot, feed back into pipeline
  Purpose: Close the gap between concept and shipping quality
```

---

## SVG Mockup Patterns (for inline Claude rendering)

### Glass Panel SVG Pattern
```svg
<!-- Reusable Glass Panel pattern for SVG mockups -->
<defs>
  <filter id="blur"><feGaussianBlur stdDeviation="8"/></filter>
  <linearGradient id="glassGrad" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0%" stop-color="white" stop-opacity="0.18"/>
    <stop offset="100%" stop-color="white" stop-opacity="0.08"/>
  </linearGradient>
</defs>
<!-- Panel body -->
<rect rx="20" fill="url(#glassGrad)" stroke="rgba(255,255,255,0.22)" stroke-width="1"/>
<!-- Top highlight edge -->
<rect rx="20" fill="none" stroke="rgba(255,255,255,0.40)" stroke-width="0.5"
      clip-path="inset(0 0 50% 0)"/>
<!-- Drop shadow via outer rect -->
<rect rx="20" fill="rgba(0,0,0,0.0)" filter="url(#blur)"
      style="box-shadow: 0 16px 48px rgba(0,0,0,0.35)"/>
```

### SVG Color Constants for Mockups
```
DARK_BG:         #0A0F1A  — game world behind glass
GLASS_FILL:      rgba(255,255,255,0.12)
GLASS_BORDER:    rgba(255,255,255,0.22)
GLASS_TINT_BLUE: rgba(100,180,255,0.18)
TEXT_PRIMARY:    rgba(255,255,255,1.0)
TEXT_SECONDARY:  rgba(255,255,255,0.65)
TEXT_ACCENT:     rgba(100,200,255,1.0)
HEALTH_GREEN:    rgba(80,200,120,0.9)
DANGER_RED:      rgba(255,80,80,0.9)
```

---

## Midjourney Prompt Engineering (Game Design)

### Prompt Structure
```
[Subject] + [Style descriptor] + [Lighting/Mood] + [Technical] + [Parameters]
```

### Style Modifier Bank

**Apple Glass / Spatial:**
```
visionOS aesthetic, frosted glass panels, spatial design, translucent UI,
Apple design language, minimal geometry, luminous restraint, depth layers,
gaussian blur background, white glass surfaces, thin emissive edges
```

**AAA Realistic:**
```
Unreal Engine 5, photorealistic, PBR materials, Lumen global illumination,
cinematic composition, film grain, depth of field, 8K render,
game concept art, AAA quality
```

**Stylized 3D:**
```
stylized game art, semi-realistic, painterly, bold outlines, saturated colors,
exaggerated proportions, hand-painted textures, expressive, cartoon-adjacent
```

**Sci-Fi:**
```
futuristic, holographic displays, neon accents, chrome surfaces,
bioluminescent details, scan line effects, chromatic aberration,
hard sci-fi aesthetic
```

### Parameter Reference
```
--ar 16:9    → Landscape (environments, wide HUD shots)
--ar 3:2     → Character sheets, turnarounds
--ar 1:1     → Icons, close-up concepts
--ar 9:16    → Mobile UI concepts
--v 6        → Midjourney v6 (best quality)
--style raw  → Less opinionated, closer to prompt (better for technical concepts)
--q 2        → Quality 2 (slower, better for final concepts)
```

### DALL·E 3 Prompt Adaptations
DALL·E 3 needs more literal description, less art-speak:
```
Instead of: "cinematic depth of field"
Use:        "blurred background with sharp foreground subject"

Instead of: "PBR materials"
Use:        "realistic surface textures with accurate reflections and shadows"

Instead of: "--ar 16:9"
Use:        "wide landscape format, 16:9 aspect ratio"
```

---

## NeoStack AIK Prompt Engineering (UE5 Assets)

### AIK Prompt Structure
```
[What to create] (asset type + name + path)
+ [Base class / parent] (for BPs, always specify)
+ [Component list] (specific, named)
+ [Behavior / logic] (clear steps)
+ [Data bindings] (where data comes from)
+ [Naming conventions] (AFL or project-specific)
+ [Platform notes] (mobile variants if needed)
```

### AIK Output Quality Checklist
After AIK generates an asset, verify:
```
[ ] Correct parent class / base asset used
[ ] All named components exist and are attached correctly
[ ] Properties have correct default values
[ ] Data bindings reference real existing assets/components
[ ] Follows project naming convention
[ ] No obvious compile errors (review BP graph for broken nodes)
[ ] Mobile considerations applied if cross-platform
```

### AIK Profile Selection Guide (AFL NeoStack)
```
UI work:         → AFL Blueprint & Gameplay profile
Materials:       → AFL VFX & Materials profile
Animation:       → AFL Animation profile
General/unknown: → Full Toolkit profile
```

---

## Design-to-Sprint Pipeline

When a design is approved, convert it to AFL sprint tasks:

```markdown
Design Approved: [Design Name]
Date: [Date]
Designer: @name

Generated Tasks:
→ AFL-XXX [Engineering] Implement UMG widget AFLW_[Name] (M)
→ AFL-XXX [Engineering] Integrate widget into LyraPrimaryGameLayout Game layer (S)  
→ AFL-XXX [Art] Create texture atlas for [Name] icons (M)
→ AFL-XXX [Engineering] Wire data bindings to [Component] (S)
→ AFL-XXX [QA] Validate [Name] on all 5 platforms (M)

AIK-Generated Assets (already created via NeoStack):
→ BP_[Name] — review and polish in editor
→ M_[Name] — review material parameters
```
