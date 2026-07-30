# Character / Creature Design Reference

## Character Design Fundamentals

### The Silhouette Test
A great game character reads as a unique shape at 64×64px thumbnail:
```
✅ Strong:  Distinctive outline, asymmetric details, clear stance
❌ Weak:    Generic humanoid, symmetric, no visual hooks

Test: Can you identify the character from their black silhouette alone?
If yes → strong design. If no → add a signature shape element.
```

### Visual Weight Hierarchy
```
HEAD:     30% of design attention — face, helmet, expression
TORSO:    40% — silhouette reads, faction colors, material storytelling
HANDS:    20% — weapons, claws, tools. What does this character DO?
LEGS:     10% — stance, mobility read, ground connection
```

### The 3-Color Rule (Readable at Distance)
```
Dominant:   60% — base material, armor, clothing
Secondary:  30% — accent material, faction color, detail
Accent:     10% — highlights, emissive, glowing elements, eyes

Example: Sci-Fi Soldier
  Dominant:  Matte black ceramic armor (60%)
  Secondary: Dark navy undersuit + silver trim (30%)
  Accent:    Cyan visor glow + blue ammo indicator (10%)
```

---

## Apple Glass Applied to Characters

For AFL characters in Glass/Spatial aesthetic contexts:

```
Visor Materials:      Frosted glass visors — translucent, show silhouette of face
                      UE5: Translucent material, opacity 0.25, roughness 0.1, Fresnel edge
Armor Surface:        Polished ceramic-like — near-mirror reflections, clean geometry
                      UE5: Metallic=0.9, Roughness=0.05, high-res normal for micro-detail
Emissive Elements:    Thin illuminated lines, glowing joints, data ports
                      → Glass logic: light defines the form
HUD Integration:      Characters that generate their own UI — floating data panels,
                      health rings around character — Glass panels orbiting them
Surface Contamination: When Glass characters take damage — cracks in the glass surface,
                      opacity changes, visual state degradation
```

---

## Character Material Breakdown (UE5)

### Layered Character Material Architecture
```
M_Character_Master (Master Material)
├── Layer 0: Base Skin / Fabric
│   Inputs: AlbedoTexture, NormalTexture, ORM_Texture
├── Layer 1: Armor / Hard Surface
│   Inputs: ArmorAlbedo, ArmorNormal, ArmorORM
│   Blend: ArmorMask texture
├── Layer 2: Emissive Details
│   Inputs: EmissiveColor (param), EmissiveMask, EmissiveIntensity
├── Layer 3: Damage State
│   Inputs: DamageMask, DamageAlbedo (burns/cracks)
│   Blend: DamageAmount (0-1 scalar, driven by GAS attribute)
└── Layer 4: Wetness / Environment Response
    Inputs: WetnessAmount (from weather system MPC)
    Effect: Roughness reduction + normal puddle overlay
```

### Glass Visor Material
```cpp
// In Material Graph:
// 1. Translucent Blend Mode
// 2. BaseColor: GlassTint param * 0.95 (near-white)
// 3. Metallic: 0.9
// 4. Roughness: lerp(0.02, 0.15, FrostAmount_param)
// 5. Opacity: lerp(0.15, 0.40, ViewAngle) using Fresnel node
// 6. Emissive: VisorGlowColor * VisorGlowIntensity (subtle inner glow)
// 7. Refraction: IOR 1.45
```

---

## Character Concept Midjourney Prompts

### Sci-Fi Soldier (Apple Glass Aesthetic)
```
/imagine futuristic soldier character concept art, full body turnaround, 
Apple visionOS-inspired armor design, frosted glass visor, clean polished ceramic 
white armor with thin cyan emissive lines, minimal geometric armor plates, 
glowing joint details, spatial design aesthetic, game character concept art, 
T-pose, front side back views, white background, 8K --ar 3:2 --v 6
```

### Fantasy Mage (Stylized)
```
/imagine fantasy mage character concept art, stylized game art, full body front 
and back, flowing dark robes with golden geometric rune patterns, magical energy 
orbiting hands, deep purple and gold color palette, expressive silhouette with 
dramatic cape, concept art for AAA game, clean line art with color, 
white background --ar 3:2 --v 6
```

### Creature / Monster (AAA Realistic)
```
/imagine alien creature concept art, 6 limbs, asymmetric bio-mechanical design, 
bioluminescent markings on dark chitinous armor, multiple eyes arrangement, 
intimidating silhouette, strong readable thumbnail shape, full body turnaround, 
Unreal Engine 5 AAA game quality, creature design concept sheet, 
dark background --ar 3:2 --style raw --v 6
```

---

## NeoStack AIK Prompts — Character Setup

### Layered Character Material (Lyra-compatible)
```
Create a Master Material M_AFL_Character_Glass in Content/Materials/Characters/:
- Use Material Layer system (not simple material)
- Layer 0 Body: PBR skin/fabric with Albedo(T_Char_D), Normal(T_Char_N), ORM(T_Char_ORM)
- Layer 1 Armor: separate Albedo/Normal/ORM params blended by ArmorMask texture
- Layer 2 Visor: Translucent sub-material, Fresnel-based opacity 0.15-0.40,
  GlassTint color param, VisorGlowColor emissive param with VisorGlowIntensity
- Layer 3 Emissive Lines: EmissiveMask texture * EmissiveColor * EmissiveIntensity param
- Layer 4 Damage: DamageAmount scalar 0-1, blends in cracks/burns albedo overlay
- Create 3 Material Instances: MI_AFL_CharHero, MI_AFL_CharEnemy, MI_AFL_CharNPC
Compatible with Lyra's SK_Mannequin skeleton. Mobile variant: flatten layers, bake ORM.
```

### Character Blueprint Setup
```
Create a Blueprint BP_AFL_CharacterBase extending ALyraCharacter:
- Add USkeletalMeshComponent for body: assign SK_AFLPlayer_Body
- Add USkeletalMeshComponent for visor: assign SK_AFLPlayer_Visor,
  attach to head socket, assign M_AFL_Character_Glass visor instance
- Add UAFLCombatComponent
- Add ULyraHealthComponent (already in Lyra base — verify present)
- Add float variable DamageAmount (0-1), exposed to Blueprint
- On DamageAmount changed: update MI_AFL_CharHero DamageAmount param
- On death: trigger dissolve via DissolveMaterial timeline on mesh
```

---

## Character Design Spec Template

```markdown
## Character: [Name / Codename]
Game: AFL / [Project]      Style: [AAA / Stylized / Glass-Spatial]

### Role & Story
[2 sentences: who are they, what do they do in the world]

### Silhouette Read
[Describe the unique shape signature — what makes them identifiable]

### Color Palette
Dominant (60%): [color + material description]
Secondary (30%): [color + material description]
Accent (10%):   [color + emissive/glass description]

### Material Breakdown
[List each material zone: head, torso, arms, legs, accessories]

### Emissive / Glass Elements
[What glows, what's translucent, where glass logic applies]

### Damage States
[How does the character visually degrade: cracks, burns, torn fabric]

### Animation Notes
[Idle personality, movement style, weight, signature gestures]

### UE5 LOD Targets
LOD0: [poly count] | LOD1: [poly count] | LOD2: [poly count]

### Skeleton
[Lyra SK_Mannequin / Custom / IK Rig requirements]
```
