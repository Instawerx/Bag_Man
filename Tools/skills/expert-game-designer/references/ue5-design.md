# UE5 Design-to-Engine Reference

## Materials as Design Language

### Glass / Translucency Material Setup
```
Blend Mode:       Translucent
Lighting Model:   Surface TranslucencyVolume (for Lumen interaction)
Two Sided:        true (glass seen from both sides)

Key Nodes:
  Fresnel(0.5, 4.0)     → Opacity variation: more opaque at edges
  BackgroundBlur(24.0)  → Screen-space blur behind panel (UMG only; 
                           in world use post-process blur or translucency)
  CameraVector·Normal   → View-dependent effects

Parameters to expose:
  GlassOpacity:     0.05 – 0.35 scalar
  GlassTint:        color (default near-white)
  FrostAmount:      0.0 – 1.0 (drives roughness + noise)
  EdgeGlowColor:    color
  EdgeGlowIntensity: 0.0 – 2.0 scalar
```

### PBR Material Template (AAA Realistic)
```
BaseColor:  T_Asset_D (sRGB on)
Normal:     T_Asset_N (sRGB off, BC5 compression)
ORM Pack:   T_Asset_ORM (sRGB off, R=Occlusion, G=Roughness, B=Metallic)
Emissive:   T_Asset_E * EmissiveColor_param * EmissiveIntensity_param

Detail Normal: lerp(flat, T_DetailNormal, DetailNormalWeight) for close-up reads
Damage Blend:  lerp(Base, DamageAlbedo, DamageMask * DamageAmount)
```

### Material Parameter Collection — AFL Global Params
```
MPC_AFLWorld (global — all materials can read):
  TimeOfDay:        0.0-24.0 float (drives sky, fog, ambient intensity)
  WeatherWetness:   0.0-1.0 float (drives roughness reduction, puddles)
  FogDensity:       0.0-1.0 float
  GlobalWindSpeed:  float (foliage, cloth, particles)
  PlayerPosition:   Vector (proximity effects, e.g., snow compaction)

MPC_AFLUI (UI materials):
  UIBlurStrength:   16.0-40.0 float (glass panels)
  UIPulseTime:      float (animated glow effects)
  UIAccentColor:    color (team/faction tint)
```

---

## Lumen for Design Intent

### Controlling Lumen for Mood
```cpp
// Cinematic interior: soft, diffuse, film-like
r.Lumen.DiffuseIndirect.Allow 1
r.Lumen.MaxTraceDistance 5000       // Short traces — tight interior
r.Lumen.Scene.SurfaceCacheResolution 1.0
r.Lumen.Reflections.Allow 1

// Outdoor open world: long range, sky bounce
r.Lumen.MaxTraceDistance 50000
r.Lumen.DiffuseIndirect.CardInterpolateInfluenceRadius 200

// Performance mode (mobile-adjacent):
r.Lumen.DiffuseIndirect.Allow 0    // Fall back to SSAO + skylight
r.Lumen.Reflections.Allow 0        // Fall back to SSR
```

### Emissive as Light Source (Glass UI in World Space)
```
On emissive materials (glass panels, holographic displays):
  ✅ Enable "Use Emissive for Static Lighting" on material
  ✅ Set emissive to sufficient intensity (> 2.0) to contribute to Lumen GI
  ✅ Use rect lights for reliable, controllable glass panel illumination
  ❌ Don't rely solely on emissive for fill — supplement with rect lights
```

---

## Post-Process for Design Styles

### Apple Glass / Clean Sci-Fi
```
PostProcessVolume settings:
  Bloom:          Intensity=0.5, Threshold=1.0 (subtle, only bright emissives bloom)
  Lens Flares:    disabled
  Film Grain:     0.0 (no grain — clean aesthetic)
  Chromatic Aberration: 0.0 (no aberration — precision aesthetic)
  Vignette:       0.2 (very subtle edge darkening)
  Color Grading:  Slight desaturation (Saturation=0.9), cool shadows (Shadows tint blue-cool)
  Exposure:       Auto, MinBrightness=0.8, MaxBrightness=3.0
  DOF:            Cinematic, FocalDistance=600, Aperture=f/4 (light DOF)
  TSR:            Enabled (default UE5)
```

### AAA Cinematic Realistic
```
  Bloom:          Intensity=0.8, Threshold=0.8
  Film Grain:     0.35 (cinematic grain)
  Chromatic Aberration: 0.5 (subtle, cinematic)
  Vignette:       0.4
  Color Grading:  Film stock LUT (apply via Texture LUT parameter)
  DOF:            Strong, f/1.8, focused on subject
  Motion Blur:    0.5 (subtle)
```

### Stylized / Painterly
```
  Bloom:          Intensity=1.2, Threshold=0.6 (big, expressive bloom)
  Film Grain:     0.0
  Outline:        Custom post-process material for cel outline
  Color Grading:  High saturation (1.3), lifted shadows, crushed highlights
  DOF:            Disabled or very subtle
```

---

## UMG Design-to-Engine Patterns

### BackgroundBlur (Core Glass Component)
```
Widget:     UBackgroundBlur
Strength:   16–32 (design spec drives this)
LowQualityFallback: Solid panel at rgba(20,24,35,0.85) — mobile fallback
Apply to:   Any Glass panel container. Wrap children in BackgroundBlur widget.

Performance note: Max 2-3 BackgroundBlur widgets active on screen at once.
For mobile: globally disable BackgroundBlur, use LowQualityFallback.
```

### CommonUI Input Display (Platform-Adaptive Prompts)
```cpp
// Automatically shows correct button icon per platform
// In UMG: add UCommonActionWidget, set InputAction property
// Runtime: reads current input device from UCommonInputSubsystem
// No code needed — CommonUI handles PS5/Xbox/PC/Touch automatically
```

### Responsive Layout (PC + Console + Mobile)
```
Anchor to:   Corners / edges with fixed padding (NOT center with fixed size)
Use:         USafeZone → handles notch, home bar, overscan automatically
Size:        DPI-scale-aware: use UUserInterfaceSettings::GetDPIScaleBasedOnSize()
Min tap:     44×44pt minimum touch target (mobile compliance)
Test at:     1280×720 (min), 1920×1080, 2560×1440, 3840×2160 + mobile sizes
```
