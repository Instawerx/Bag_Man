# Materials & Variants Reference

Phase 5 of the pipeline. The visual customization layer that turns one mesh
into infinite cosmetic variants without re-importing geometry.

---

## Architecture Decision: Master Material vs Material Layers

Lyra uses **Material Layer Stacks** for the mannequin (`ML_Mannequin_Skin`,
`ML_Mannequin_Outfit`, etc.). For a custom skin system, you have three
architectural options:

```
OPTION 1 — Mirror Lyra's Material Layer Stack
  ✓ Plays nicely with existing Lyra material assets
  ✓ Modular: swap individual layers (skin, outfit, accent) independently
  ✗ More complex to author and instance
  ✗ Layer permutations bloat shader compilation times
  → Use when: shipping deep modular cosmetics (L4) with layer-level swaps

OPTION 2 — Custom Master Material + Parameter-Driven MIs
  ✓ Simpler authoring, faster iteration
  ✓ Smaller permutation count
  ✗ Less modular — full material swap, not layer swap
  ✗ Diverges from Lyra's authored material assets
  → Use when: shipping monolithic skin variants (L2) with parameter tinting

OPTION 3 — Master Material with Material Function Calls (Hybrid)
  ✓ Composable like layers, simpler than full layer stack
  ✓ Standard UE5 pattern, easy to onboard new artists
  → Use when: most projects — this is the recommended default
```

**Recommendation**: Start with Option 3 (Material Functions). It's the
right balance of flexibility and simplicity for L2–L4 skin systems.

---

## Master Material Template — M_<Project>_Character

The canonical character master material. Build once, instance for every skin.

```
Domain:             Surface
Blend Mode:         Opaque (Masked for cutout hair/eyelashes — separate slot)
Shading Model:      Default Lit
Two Sided:          False (true only for cape, hair cards)
Used with Skeletal: TRUE  ← critical, easy to forget
Used with Nanite:   TRUE if cosmetic hard-surface armor pieces use Nanite

Inputs:
─────────────────────────────────────────────────────────────────
BaseColor:
  Texture2DParam: BaseColorTex (T_<Skin>_D)
  ScalarParam:    BaseColorBrightness (default 1.0)
  VectorParam:    TintColor (default white)
  Output = BaseColorTex × TintColor × BaseColorBrightness

Metallic:
  Sample ORM_Tex.B (packed)
  Or fall back to ScalarParam MetallicValue if no ORM texture

Roughness:
  Sample ORM_Tex.G (packed)
  ScalarParam: RoughnessMin / RoughnessMax — clamp range per skin

AmbientOcclusion:
  Sample ORM_Tex.R

Normal:
  Texture2DParam: NormalTex (T_<Skin>_N)
  Sample as Normal
  ScalarParam: NormalIntensity (1.0 default)

Emissive:
  Texture2DParam: EmissiveMask (T_<Skin>_E, default black)
  VectorParam:    EmissiveColor
  ScalarParam:    EmissiveIntensity (0.0 default — off unless skin has glow)
  Output = EmissiveMask × EmissiveColor × EmissiveIntensity

Detail Layer (optional):
  Texture2DParam: DetailNormalTex (T_<Project>_DetailNormal)
  ScalarParam:    DetailNormalIntensity
  ScalarParam:    DetailUVScale
  Blend into main normal via DetailNormal Material Function

Team Color (multiplayer support):
  VectorParam (from MPC): TeamColorPrimary
  Texture2DParam: TeamColorMask (R=Primary zone, G=Secondary, B=Trim)
  Blend BaseColor with team color zones based on mask
```

This master material covers L1 (rebrand via texture swap) through L4
(modular parts each instancing the same master).

---

## Material Instance per Skin — MI_<Project>Skin_<SkinName>

Each skin variant is a Material Instance overriding the master's parameters.
Most skins only override texture references and tint color — no shader
recompile cost.

```
Parent: M_<Project>_Character

Texture Overrides:
  BaseColorTex     = T_<Project>_<SkinName>_D
  NormalTex        = T_<Project>_<SkinName>_N
  ORM_Tex          = T_<Project>_<SkinName>_ORM
  EmissiveMask     = T_<Project>_<SkinName>_E (if applicable)
  TeamColorMask    = T_<Project>_<SkinName>_TC (if multiplayer)

Scalar Overrides:
  EmissiveIntensity = 0.0 → 4.0 (matches skin's design intent)
  RoughnessMin / Max = clamp range per skin
  NormalIntensity   = 1.0 default

Vector Overrides:
  TintColor        = base brand color for the skin
  EmissiveColor    = glow color
```

For a skin pack with 5 variants, you ship 5 MIs and 5 sets of textures.
Zero new shaders compiled if all skins use the same master.

---

## Material Instance Dynamic (MID) for Runtime Tinting

For features like team color, damage state, or live player customization
(slider-driven tints), you need a MID created at runtime so each character
has its own parameter values.

```cpp
// In your character class (extends ALyraCharacter)
UCLASS()
class <PROJECT>GAME_API A<Project>CosmeticCharacter : public ALyraCharacter
{
    GENERATED_BODY()

public:
    A<Project>CosmeticCharacter();

    /** Apply a skin to this character at runtime */
    UFUNCTION(BlueprintCallable, Category = "Cosmetics")
    void ApplySkin(const F<Project>SkinDefinition& SkinDef);

    /** Set team color zone via MID */
    UFUNCTION(BlueprintCallable, Category = "Cosmetics")
    void SetTeamColor(const FLinearColor& PrimaryColor);

    /** Drive damage visual state (0.0 = pristine, 1.0 = wrecked) */
    UFUNCTION(BlueprintCallable, Category = "Cosmetics")
    void SetDamageAmount(float Amount);

protected:
    virtual void BeginPlay() override;

private:
    /** Created on BeginPlay so each character has unique parameter values */
    UPROPERTY()
    TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyMIDs;

    void EnsureMIDsCreated();
};
```

```cpp
// A<Project>CosmeticCharacter.cpp
void A<Project>CosmeticCharacter::BeginPlay()
{
    Super::BeginPlay();
    EnsureMIDsCreated();
}

void A<Project>CosmeticCharacter::EnsureMIDsCreated()
{
    USkeletalMeshComponent* MeshComp = GetMesh();
    if (!IsValid(MeshComp))
    {
        return;
    }

    const int32 NumMaterials = MeshComp->GetNumMaterials();
    BodyMIDs.Reset(NumMaterials);

    for (int32 SlotIdx = 0; SlotIdx < NumMaterials; ++SlotIdx)
    {
        // CreateDynamicMaterialInstance returns the existing MID if one already
        // exists at this slot, or creates a new one from the slot's current MI.
        UMaterialInstanceDynamic* MID =
            MeshComp->CreateDynamicMaterialInstance(SlotIdx);

        if (IsValid(MID))
        {
            BodyMIDs.Add(MID);
        }
    }
}

void A<Project>CosmeticCharacter::SetTeamColor(const FLinearColor& PrimaryColor)
{
    for (UMaterialInstanceDynamic* MID : BodyMIDs)
    {
        if (IsValid(MID))
        {
            MID->SetVectorParameterValue(
                FName(TEXT("TeamColorPrimary")), PrimaryColor);
        }
    }
}

void A<Project>CosmeticCharacter::SetDamageAmount(float Amount)
{
    const float Clamped = FMath::Clamp(Amount, 0.0f, 1.0f);
    for (UMaterialInstanceDynamic* MID : BodyMIDs)
    {
        if (IsValid(MID))
        {
            MID->SetScalarParameterValue(
                FName(TEXT("DamageAmount")), Clamped);
        }
    }
}

void A<Project>CosmeticCharacter::ApplySkin(const F<Project>SkinDefinition& SkinDef)
{
    USkeletalMeshComponent* MeshComp = GetMesh();
    if (!IsValid(MeshComp))
    {
        return;
    }

    // Swap the skeletal mesh if the skin requires a different one (L2 skin)
    if (SkinDef.SkeletalMesh.IsValid())
    {
        MeshComp->SetSkeletalMesh(SkinDef.SkeletalMesh.Get());
    }
    else if (!SkinDef.SkeletalMesh.IsNull())
    {
        // Async load if not yet streamed in
        UAssetManager::GetStreamableManager().RequestAsyncLoad(
            SkinDef.SkeletalMesh.ToSoftObjectPath(),
            FStreamableDelegate::CreateUObject(
                this, &A<Project>CosmeticCharacter::OnSkinMeshLoaded, SkinDef));
        return;
    }

    // Re-create MIDs for the new mesh, then apply MI overrides
    EnsureMIDsCreated();
    for (int32 SlotIdx = 0;
         SlotIdx < SkinDef.MaterialInstances.Num() && SlotIdx < BodyMIDs.Num();
         ++SlotIdx)
    {
        if (UMaterialInterface* MI = SkinDef.MaterialInstances[SlotIdx].Get())
        {
            MeshComp->SetMaterial(SlotIdx, MI);
        }
    }
    // Refresh MIDs after material change
    EnsureMIDsCreated();
}
```

---

## Material Parameter Collection (MPC) for Global Effects

For effects that should affect all characters at once (game-wide rain, time
of day, faction war state), use a Material Parameter Collection.

```cpp
// Create MPC_<Project>_GlobalCosmetic asset
// Define params: WetnessAmount, GlobalDamageMod, TimeOfDayTint, etc.

// Drive from C++:
UMaterialParameterCollectionInstance* MPCInstance =
    GetWorld()->GetParameterCollectionInstance(GlobalCosmeticMPC);

if (IsValid(MPCInstance))
{
    MPCInstance->SetScalarParameterValue(
        FName(TEXT("WetnessAmount")), WeatherSubsystem->GetWetness());
}
```

In the master material, sample the MPC and blend its values into your output.
This is far cheaper than setting parameters on every character's MID each frame.

---

## Team Color / Faction Color System

Multiplayer Lyra games typically need each player visually identifiable by
team. Implement via a TeamColorMask texture and TeamColorPrimary vector param.

**TeamColorMask texture authoring:**
```
R channel: Primary team color zone (largest area — chest, helmet)
G channel: Secondary zone (trim, smaller details)
B channel: Tertiary / accent zone (visor, emissive lines)
A channel: Reserved (unused or wear/damage)
```

**Master material blend:**
```
FinalBaseColor =
  lerp(BaseColorTex,
       BaseColorTex × TeamColorPrimary,
       TeamColorMask.R) ×
  lerp(1, TeamColorSecondary, TeamColorMask.G × 0.6) ×
  lerp(1, TeamColorTertiary,  TeamColorMask.B × 0.4)
```

**Runtime team assignment** (driven by Lyra's `ULyraTeamSubsystem`):
```cpp
void A<Project>CosmeticCharacter::ApplyTeamColors()
{
    ULyraTeamSubsystem* TeamSubsystem =
        UWorld::GetSubsystem<ULyraTeamSubsystem>(GetWorld());

    if (!IsValid(TeamSubsystem)) { return; }

    const int32 TeamId = TeamSubsystem->FindTeamFromActor(this);
    const F<Project>TeamColorPalette Palette =
        TeamColorTable->GetPaletteForTeam(TeamId);

    for (UMaterialInstanceDynamic* MID : BodyMIDs)
    {
        if (IsValid(MID))
        {
            MID->SetVectorParameterValue(
                FName(TEXT("TeamColorPrimary")),   Palette.Primary);
            MID->SetVectorParameterValue(
                FName(TEXT("TeamColorSecondary")), Palette.Secondary);
            MID->SetVectorParameterValue(
                FName(TEXT("TeamColorTertiary")),  Palette.Tertiary);
        }
    }
}
```

---

## Mobile Material Variant

The full Material Function-based master can be too expensive on mobile. Ship
a parallel mobile variant for the same skin.

```
M_<Project>_Character          → desktop / console master (8–12 texture samples)
M_<Project>_Character_Mobile   → mobile master (3–4 samples, flattened layers)
```

**Mobile simplifications:**
```
✗ Drop detail normal sampling
✗ Drop emissive sampling (use vertex color emissive flag instead)
✗ Drop team color masking — bake team variants to separate textures
✗ Drop wetness MPC sampling
✗ Reduce instructions: ≤120 on Android Adreno, ≤180 on iOS Apple GPU
✓ Keep BaseColor, Normal, ORM as single-sample
✓ Keep one MID parameter for live tinting (slider in shop preview)
```

For each MI on desktop, ship a parallel `MI_<Project>Skin_<SkinName>_Mobile`
that targets the mobile master. Select at runtime based on platform:

```cpp
UMaterialInterface* GetMaterialForPlatform(
    const F<Project>SkinDefinition& Skin, int32 SlotIdx)
{
#if PLATFORM_ANDROID || PLATFORM_IOS
    if (Skin.MobileMaterialInstances.IsValidIndex(SlotIdx))
    {
        return Skin.MobileMaterialInstances[SlotIdx].LoadSynchronous();
    }
#endif
    return Skin.MaterialInstances[SlotIdx].LoadSynchronous();
}
```

---

## Material Slot Order — Authoring Discipline

Repeating from `pipeline-skeleton-mesh.md` because it's the single biggest
material bug source. When authoring a new skin:

```
✓ Match Lyra's slot order: Body, Outfit, Hair, Eyes, Eyebrows, Eyelashes, Teeth
✓ Confirm slot names in the imported asset match this convention
✓ Document any deviation in the skin's design doc, not in tribal knowledge
✗ Don't reorder slots after the skin has shipped — it breaks every saved MID
  reference in shipped builds
```

If your skin has a different segment breakdown (e.g. armor pieces instead of
outfit/hair), define your own slot order in a project-wide convention doc and
stick to it across every skin in the catalog.

---

## Verification Checklist — End of Phase 5

```
□ M_<Project>_Character master built and compiles
□ One MI authored for the base skin variant and verified in editor
□ MID creation works on BeginPlay; team color test passes
□ Mobile material variant authored and tested on actual device
□ Material slot order documented and consistent across catalog
□ Team color mask textures use R/G/B convention
□ MPC for global effects defined (even if empty initially — wires it for later)
□ Material LOD settings: lower-cost variant on LOD2+ for distant characters
```
