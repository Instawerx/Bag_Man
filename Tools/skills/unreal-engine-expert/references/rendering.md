# Rendering & Materials Reference (UE5)

## Lumen

- **Global Illumination**: Enabled via `r.Lumen.Reflections.Allow 1` and Project Settings > Rendering
- **Software vs Hardware Lumen**: Hardware Ray Tracing preferred for AAA (requires DX12/Vulkan RT)
- **Performance knobs**:
  - `r.Lumen.DiffuseIndirect.Allow` — toggle GI
  - `r.Lumen.MaxTraceDistance` — reduce for interior scenes
  - `r.Lumen.Scene.SurfaceCacheResolution` — memory vs quality tradeoff
- **Emissive meshes as light sources**: Set `Use Emissive for Static Lighting = true` on material
- **Lumen does NOT support** (important for AAA planning):
  - Forward shading pass — must use Deferred Rendering
  - Mobile platforms — use DFAO (Distance Field Ambient Occlusion) as fallback
  - Very small/thin geometry details (use screen-space fallback)
  - Translucent surfaces receiving GI (workaround: capture actors)

```cpp
// Force Lumen settings at runtime (e.g., quality scalability)
static IConsoleVariable* CVarLumenGI =
    IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.DiffuseIndirect.Allow"));
if (CVarLumenGI) CVarLumenGI->Set(1, ECVF_SetByCode);
```

---

## Nanite

- **When to use**: Static meshes with high poly counts (rocks, architecture, props)
- **When NOT to use**: Skeletal meshes (not supported), foliage with alpha clip (use Masked carefully), meshes needing custom depth for outlines
- **Enable per mesh**: StaticMesh editor > Nanite Settings > Enable Nanite
- **LOD**: Nanite manages its own micro-LOD; disable traditional LODs on Nanite meshes
- **Materials**: Fully supported except Vertex Interpolator nodes and some WPO caveats
- **World Position Offset (WPO)**: Supported in UE5.1+, perf cost — use `r.Nanite.MaxPixelsPerEdge` to tune

```cpp
// Check Nanite support at runtime
if (GRHISupportsNanite)
{
    // Enable Nanite workflow
}
```

---

## Niagara (VFX)

### C++ Spawning
```cpp
// Spawn attached Niagara system
UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
    HitEffectAsset,          // UNiagaraSystem*
    MeshComponent,           // Attach parent
    NAME_None,               // Socket
    FVector::ZeroVector,
    FRotator::ZeroRotator,
    EAttachLocation::SnapToTarget,
    true                     // Auto destroy
);

// Set Niagara parameters from C++
NiagaraComp->SetVariableFloat(FName("ImpactForce"), 500.f);
NiagaraComp->SetVariableLinearColor(FName("HitColor"), FLinearColor::Red);
```

### Performance Rules
- Use **GPU simulation** for >1000 particles
- Use **Pooling** (`System > Fixed Bounds + Auto Manage Attachment`) to avoid spawn overhead
- Avoid Niagara → Blueprint event callbacks in hot paths; prefer C++ binding

---

## Materials & HLSL

### Custom HLSL Node
```hlsl
// Custom node in Material Editor — inputs: UV (float2), Time (float)
float2 AnimatedUV = UV + float2(sin(Time * 2.0), cos(Time * 1.5)) * 0.05;
return AnimatedUV;
```

### Material Instancing Best Practices
```cpp
// Create dynamic material instance (avoid per-frame)
void AMyActor::BeginPlay()
{
    DynMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
    MeshComponent->SetMaterial(0, DynMaterial);
}

// Update parameter (cheap, just sets scalar buffer)
void AMyActor::SetGlowIntensity(float Intensity)
{
    if (DynMaterial)
    {
        DynMaterial->SetScalarParameterValue(FName("GlowIntensity"), Intensity);
    }
}
```

### Material Parameter Collections (Global params)
```cpp
// Set MPC value from C++ (affects ALL materials using it)
UMaterialParameterCollectionInstance* MPCInstance =
    GetWorld()->GetParameterCollectionInstance(TimeOfDayMPC);
MPCInstance->SetScalarParameterValue(FName("SunAngle"), CurrentSunAngle);
```

---

## Post Processing

```cpp
// Blend post process settings at runtime
PostProcessComponent->Settings.bOverride_BloomIntensity = true;
PostProcessComponent->Settings.BloomIntensity = 2.0f;
PostProcessComponent->BlendWeight = 0.5f; // Blend with global PP volume
```

Key Post Process features in UE5:
- **TSR** (Temporal Super Resolution) — use over TAA for quality upscaling
- **Substrate Materials** (UE5.2+) — next-gen layered material system
- **Path Tracing** — reference quality, not real-time; useful for cinematics
