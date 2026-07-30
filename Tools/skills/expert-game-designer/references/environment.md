# Environment Design Reference

## Environment Design Framework

### The 3-Layer Lighting Model
Every AFL environment is designed around three lighting layers:

```
Layer 1 — AMBIENT (sets the mood, never changes)
  Lumen sky light, HDRI dome, global fog color
  → The emotional baseline. Cool night? Warm golden hour? Overcast dread?

Layer 2 — KEY (tells the story, guides the player)
  Directional light (sun/moon), major spotlights, emissive hero props
  → Where is the player supposed to look? What's important?

Layer 3 — FILL (prevents flatness, adds richness)
  Bounce lights, rect lights behind walls, Lumen GI doing the work
  → Depth, shadow detail, material richness. Player feels "in" the world.
```

### Biome Color Palette System
Design palettes in three zones per environment:

```
SHADOW ZONE:    Dark values — what the player walks through
MIDTONE ZONE:   Mid values — the world's primary read
HIGHLIGHT ZONE: Bright values — where attention goes, where story lives

Example: Urban Night
  Shadow:    #0D1117 (near-black blue)
  Midtone:   #1E2A3A (deep steel blue)
  Highlight: #FF8C42 (sodium lamp orange), #4FC3F7 (neon cyan)

Example: Ancient Forest
  Shadow:    #0A1A0D (forest floor dark)
  Midtone:   #2D5A27 (moss green)
  Highlight: #FFE066 (dappled sunlight), #B3FFD9 (magical bioluminescence)
```

---

## Apple Glass Applied to Environments

The Glass aesthetic isn't just UI — it informs environment design:

```
Reflective Surfaces:    Polished floors, wet stone, glass walls
                        → Mirror the world, suggest depth beneath
Translucent Materials:  Frosted panels, thin ice, glowing membranes
                        → Light bleeds through; hint at what's beyond
Geometric Minimalism:   Clean architectural silhouettes, purposeful negative space
                        → The absence of detail IS the design
Luminous Accents:       Strategic emissive lines, illuminated edges, glowing joints
                        → Glass and light define form, not surface texture
Atmospheric Depth:      Volumetric fog, god rays, depth haze
                        → Layers of depth reinforce the spatial Glass metaphor
```

---

## Level Design Principles

### The 3C Rule (Camera, Character, Controls)
Always design spaces around:
1. **Camera** — does the player see what they need to? Any blind spots?
2. **Character** — can the character navigate naturally? No janky geometry
3. **Controls** — does the space reward / teach the control set?

### Spatial Rhythm
```
TENSION → RELEASE → TENSION → RELEASE

Tight corridor (tension) → Open arena (release)
Dark maze (tension) → Lit safe room (release)
Combat gauntlet (tension) → Loot room / vista (release)
```

### Player Guidance Without Arrows
```
Light:       Brightest point = direction of progress
Color:       Warm = go here | Cool = already visited (or danger)
Sound:       Audio cues get louder as player approaches objective
Geometry:    Paths are clear, walls are walls. Leading lines in architecture
Props:       Breadcrumb trail of relevant objects (fire pits, bodies, tracks)
```

---

## Midjourney Prompts — Environment Concepts

### Sci-Fi Interior (Glass + Minimal)
```
/imagine interior of a futuristic space station, Apple visionOS design aesthetic, 
frosted glass walls with soft inner glow, minimal architecture, polished white 
floors with reflections, geometric light panels on ceiling, atmospheric depth haze, 
translucent panel doors, cool blue-white lighting, ultra-clean design, Unreal Engine 5 
rendered, cinematic composition, concept art --ar 16:9 --style raw --v 6
```

### Dark Fantasy Environment
```
/imagine dark fantasy dungeon environment, ancient stone architecture, god rays 
through cracked ceiling, bioluminescent moss on walls, deep shadows with warm 
torch light, atmospheric fog at ground level, detailed PBR materials, Unreal Engine 5, 
game environment concept art, cinematic lighting --ar 16:9 --v 6
```

### Urban Night (Neon Glass)
```
/imagine futuristic urban night scene, rain-slicked streets reflecting neon lights, 
glass skyscrapers with frosted panels, sodium lamp orange vs cyan neon color contrast, 
volumetric fog, wet pavement reflections, deep blue shadows, cinematic game environment, 
Unreal Engine 5 Lumen lighting, concept art --ar 16:9 --style raw --v 6
```

---

## NeoStack AIK Prompts — Environment Setup

### Glass Architecture Material
```
Create a Material M_GlassArchPanel_Master in Content/Materials/Environment/:
- Base: translucent blend mode, two-sided
- Opacity: scalar param GlassOpacity default 0.15
- BaseColor: param GlassTint default (1.0, 1.0, 1.0)
- Metallic: 0.0, Roughness: scalar param GlassRoughness default 0.05
- Normal: subtle frosted noise texture, param FrostIntensity default 0.3
- Emissive: thin edge glow using Fresnel node, param EdgeGlowColor + EdgeGlowIntensity
- Refraction: index 1.45, enable screen-space refraction
Create mobile variant M_GlassArchPanel_Mobile: simplified, no refraction, baked frost
```

### Atmospheric Fog Setup
```
Set up atmospheric environment in the current level:
- Add ExponentialHeightFog: FogDensity=0.02, FogHeightFalloff=0.2,
  StartDistance=500, FogCutoffDistance=200000
  InscatteringColor=(0.05, 0.08, 0.15) for cool night mood
- Add VolumetricClouds component with default settings
- Set DirectionalLight (Sun): intensity=8 lux, temperature=6500K, 
  DynamicShadowDistance=50000, CascadeShadowMaps=4
- Add SkyAtmosphere component, default settings
- Enable Lumen: r.Lumen.DiffuseIndirect.Allow=1, 
  r.Lumen.Reflections.Allow=1
```
