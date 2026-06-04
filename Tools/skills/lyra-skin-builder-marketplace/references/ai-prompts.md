# AI Prompts Reference

Production-ready prompts for accelerating the reskinning pipeline using
NeoStack AIK (in-editor UE5 agent) and Midjourney/DALL·E (concept art).
Copy-paste these directly; substitute `<Project>` / `<Studio>` /
`<SkinName>` placeholders.

---

## NeoStack AIK Prompts

These run inside UE5 via the NeoStack Agent Integration Kit. They produce
actual UE5 assets — Materials, Blueprints, DataTables, GameFeature plugins
— not just code. Use the AIK profile that has Content Browser, Material
Editor, and Blueprint editor tools enabled.

### AIK-01 — Build Master Character Material

**When to use**: Phase 5, authoring the project's master character material.

```
Create a new Material asset at /Game/Cosmetics/Materials/M_<Project>_Character.

Material settings:
- Material Domain: Surface
- Blend Mode: Opaque
- Shading Model: Default Lit
- Two Sided: false
- Used with Skeletal Mesh: true
- Used with Nanite: true

Add these parameters with default values:
SCALAR PARAMETERS:
  BaseColorBrightness = 1.0  (group: BaseColor)
  RoughnessMin = 0.2         (group: Roughness)
  RoughnessMax = 1.0         (group: Roughness)
  NormalIntensity = 1.0      (group: Normal)
  EmissiveIntensity = 0.0    (group: Emissive)
  DetailNormalIntensity = 0.5 (group: Detail)
  DetailUVScale = 8.0         (group: Detail)

VECTOR PARAMETERS:
  TintColor = (1, 1, 1, 1)          (group: BaseColor)
  EmissiveColor = (1, 1, 1, 1)      (group: Emissive)
  TeamColorPrimary = (1, 0, 0, 1)   (group: TeamColor)
  TeamColorSecondary = (0.5, 0, 0, 1) (group: TeamColor)
  TeamColorTertiary = (1, 1, 1, 1)  (group: TeamColor)

TEXTURE PARAMETERS (use default UE5 black/normal placeholder textures
where the user hasn't specified):
  BaseColorTex      (group: BaseColor, type: Color)
  NormalTex         (group: Normal, type: Normal)
  ORM_Tex           (group: PBR, type: Masks — AO/Roughness/Metallic packed)
  EmissiveMask      (group: Emissive, type: Color, default black)
  TeamColorMask     (group: TeamColor, type: Masks)
  DetailNormalTex   (group: Detail, type: Normal)

Graph wiring:
1. BaseColor output = lerp(BaseColorTex × TintColor × BaseColorBrightness,
   that result blended with TeamColorPrimary using TeamColorMask.R)
2. Metallic output = ORM_Tex.B
3. Roughness output = saturate(ORM_Tex.G mapped to [RoughnessMin, RoughnessMax])
4. AmbientOcclusion = ORM_Tex.R
5. Normal output = blend(NormalTex, DetailNormalTex × DetailNormalIntensity)
   using DetailUVScale on the detail UV
6. Emissive output = EmissiveMask × EmissiveColor × EmissiveIntensity

Save and compile. Verify zero shader errors.
```

### AIK-02 — Batch-Create Skin Material Instances

**When to use**: Phase 5, after master material exists, when adding a new
skin pack with multiple variants.

```
At /Game/Cosmetics/Materials/Skins/<SkinPackName>/, create N Material
Instance Constants, each parented to M_<Project>_Character.

For each skin in this list (one MI per row):
  - <SkinName1>: BaseColorTex=T_<Project>_<SkinName1>_D, NormalTex=T_<Project>_<SkinName1>_N, ORM_Tex=T_<Project>_<SkinName1>_ORM, TintColor=(R,G,B,1)
  - <SkinName2>: ...
  - <SkinName3>: ...

Name each MI: MI_<Project>Skin_<SkinName>_Body

For each MI:
- Open the asset
- Set parent to M_<Project>_Character
- Override only the texture and color parameters listed above
- Leave all other parameters at parent defaults
- Save

After all are created, summarize: how many MIs created, any texture refs
that resolved to null (placeholder will be assigned), any compile errors.
```

### AIK-03 — Generate Modular Part Blueprint

**When to use**: Phase 4, authoring a new skinned cosmetic part.

```
Create a new Blueprint at /Game/Cosmetics/Parts/<PartCategory>/BP_<Project>CharacterPart_<PartName>.

Parent class: A<Project>CharacterPart_Skinned

Default settings:
- MeshComponent → SkeletalMesh: SK_<Project>Part_<PartName>
- MeshComponent → Material Slot 0: MI_<Project>Skin_<PartName>_Default
- MeshComponent → Cast Shadow: true
- MeshComponent → Collision Enabled: NoCollision
- MeshComponent → Skin Cache Usage: Enabled
- MeshComponent → bSyncAttachParentLOD: true

AppliedTags container, add:
- Cosmetic.Part.<PartName>
- Cosmetic.Rarity.<Rarity>  (Common/Rare/Epic/Legendary based on skin)
- Cosmetic.Outfit.<OutfitClass>  (Heavy/Light/Stealth/Default based on intent)

If this is a head/helmet part: also set Socket override to head_socket
in the Construction Script.

If this part should trigger heavy locomotion: set AnimLayerToLink to
ABP_<Project>_Locomotion_Heavy.

Save and compile. Verify the BP is spawnable in a test map attached to a
SkeletalMeshComponent via SetLeaderPoseComponent.
```

### AIK-04 — Author Skin Data Asset

**When to use**: Phase 6, every time a new skin is added to the catalog.

```
Create a new Data Asset at /Game/Cosmetics/Skins/<SkinPackName>/DA_<Project>Skin_<SkinName>.

Class: U<Project>SkinAsset

Set Definition struct fields:
- SkinId: <SkinName> (FName, must be unique across the entire catalog)
- DisplayName: "<Designer-facing display name>"
- Description: "<Marketing description, 1-2 sentences>"
- ShopThumbnail: TSoftObjectPtr → T_<Project>Skin_<SkinName>_Thumb
- HeroImage: TSoftObjectPtr → T_<Project>Skin_<SkinName>_Hero
- SkeletalMesh: TSoftObjectPtr → SK_<Project>Char_<SkinName>_Body
- SkeletalMeshMobile: TSoftObjectPtr → SK_<Project>Char_<SkinName>_Body_Mobile (or null if same)
- MaterialInstances: [
    MI_<Project>Skin_<SkinName>_Body,
    MI_<Project>Skin_<SkinName>_Outfit,
    MI_Lyra_Hair,
    MI_Lyra_Eye,
    MI_Lyra_Eyebrows,
    MI_Lyra_Eyelashes,
  ]
- MaterialInstancesMobile: [parallel list with _Mobile variants if any]
- Parts: [] (or list of modular DA refs if this is a parts-based skin)
- RarityTag: Cosmetic.Rarity.<Rarity>
- CurrencyTag: Currency.<Soft|Premium>
- BaseCost: <integer cost>
- SkinTags: container with Cosmetic.Outfit.<Class>, Cosmetic.Season.<S>
- OwningGameFeaturePlugin: "<plugin name>" or empty for base game
- SeasonTag: Cosmetic.Season.<SeasonId>
- AvailableUntilUtc: "<ISO 8601 UTC datetime>" or empty for permanent

Save. Verify Primary Asset ID resolves correctly via Asset Manager.
```

### AIK-05 — Scaffold Cosmetic Shop UI Widget Tree

**When to use**: Phase 7, initial creation of the shop UI. One-time setup.

```
At /Game/UI/Shop/, create the following Widget Blueprints:

1. W_<Project>CosmeticShop_Root (parent: U<Project>CosmeticShopRoot)
   Hierarchy:
     OverlaySlot:
       VerticalBox:
         WalletWidget (Border, named "Wallet")
         HorizontalBox (named "MainContent"):
           BrowserWidget (CommonActivatableWidgetStack, named "Browser")
           PreviewWidget (Overlay, named "Preview")

2. W_<Project>CosmeticShop_Browser (parent: U<Project>CosmeticShopBrowser)
   Hierarchy:
     VerticalBox:
       HorizontalBox (filter tabs):
         CommonButtonBase "All"
         CommonButtonBase "Common"
         CommonButtonBase "Rare"
         CommonButtonBase "Epic"
         CommonButtonBase "Legendary"
       CommonListView (named "SkinListView")
         EntryWidgetClass: W_<Project>SkinTile (BP_TBD if doesn't exist)
         Selection Mode: Single
         Orientation: Horizontal wrap (or use TileView for grid)

3. W_<Project>SkinTile (parent: U<Project>SkinTileEntry)
   Hierarchy:
     Overlay (220×280px):
       Image (rarity frame background)
       VerticalBox:
         Image (named "ThumbnailImage", 200×200)
         TextBlock (named "NameText", font 14pt)
         TextBlock (named "PriceText", font 12pt)
         Image (named "OwnedBadge", visibility Collapsed by default)
   Implements: IUserObjectListEntry

4. W_<Project>CosmeticShop_Preview (parent: U<Project>CosmeticShopPreview)
   Hierarchy:
     Overlay:
       Image (named "PreviewViewport", fills available, render-target driven)
       VerticalBox (top-right, detail panel):
         TextBlock (named "SkinNameText")
         TextBlock (named "DescriptionText")
         CommonButtonBase (named "PurchaseButton")
         CommonButtonBase (named "EquipButton")

5. W_<Project>CosmeticShop_Wallet (parent: U<Project>CosmeticShopWallet)
   Hierarchy:
     HorizontalBox:
       Image (soft currency icon) + TextBlock (named "SoftCurrencyText")
       Image (premium currency icon) + TextBlock (named "PremiumCurrencyText")

All widgets should:
- Use CommonUI base classes, not stock UMG
- Bind their named widgets via meta=(BindWidget)
- Be set up for gamepad navigation (FocusedSlots configured)

Save and compile all five widgets. Verify no compile errors and that
each Bind reference resolves.
```

### AIK-06 — Generate GameFeature Plugin Scaffold

**When to use**: Phase 8, every new seasonal drop or premium pack.

```
Create a new GameFeature plugin at Plugins/GameFeatures/<Project>Cosmetics_<DropName>/.

Plugin .uplugin contents:
{
  "FileVersion": 3,
  "Version": 1,
  "VersionName": "1.0",
  "FriendlyName": "<Project> Cosmetics — <DropName>",
  "Description": "<One-line description of the drop>",
  "Category": "Cosmetics",
  "CreatedBy": "<Studio>",
  "CanContainContent": true,
  "Installed": true,
  "ExplicitlyLoaded": true,
  "EnabledByDefault": false,
  "GameFeatureType": "GameFeature",
  "Plugins": [
    { "Name": "GameFeatures", "Enabled": true },
    { "Name": "ModularGameplay", "Enabled": true }
  ]
}

Folder structure:
  Plugins/GameFeatures/<Project>Cosmetics_<DropName>/
    Content/
      Skins/
      Parts/
      Meshes/
      Materials/
      Textures/
      Data/
    Config/
      DefaultGame.ini

In Config/DefaultGame.ini, add:
[/Script/Engine.AssetManagerSettings]
+PrimaryAssetTypesToScan=(PrimaryAssetType="<Project>Skin",
    AssetBaseClass=/Script/<Project>Game.<Project>SkinAsset,
    bHasBlueprintClasses=False, bIsEditorOnly=False,
    Directories=((Path="/<Project>Cosmetics_<DropName>/Skins")),
    Rules=(CookRule=AlwaysCook))

Create the GameFeatureData asset:
- Right-click in plugin's root content folder → GameFeature → Game Feature Data
- Name: <Project>Cosmetics_<DropName>_GameFeatureData
- Default State: Initial

Add an Action:
- U<Project>GameFeatureAction_RegisterCatalog
- CatalogChunk: TSoftObjectPtr → (will be set after creating the DataTable)
- SeasonTag: Cosmetic.Season.<SeasonId>

Create DT_<Project>SkinCatalog_<DropName> DataTable:
- Row struct: F<Project>SkinCatalogRow
- Save at /Plugins/GameFeatures/<Project>Cosmetics_<DropName>/Content/Data/

Update the GameFeatureData's RegisterCatalog action with the new DataTable.

Verify plugin enables in editor without errors. Test
LoadAndActivateGameFeaturePlugin from console: "GameFeature.LoadAndActivatePlugin <Project>Cosmetics_<DropName>".
```

### AIK-07 — Validate Imported Skin Asset

**When to use**: Phase 3, every time the art team hands over a new
character mesh.

```
For the asset at /Game/Cosmetics/Meshes/SK_<Project>Char_<SkinName>_Body,
run the following validation and report results:

1. Skeleton field: must be exactly SK_Mannequin. If not, FAIL with
   "Skeleton mismatch — needs re-bind in DCC to Lyra Mannequin skeleton".

2. Material slots: read the slot count and slot names. Compare against:
   [Body, Outfit, Hair, Eyes, Eyebrows, Eyelashes]
   - If slot count differs, FAIL with "Wrong slot count — expected 6".
   - If slot names mismatch, WARN with the actual vs expected order.

3. Skin Cache Usage: must be "Enabled". If "Project Default" but project
   setting is off, FAIL with "Enable Skin Cache at project level".

4. LODs: must have at least 2 LODs. Report tri count per LOD.

5. Physics Asset: must be assigned (not None). If None, FAIL with
   "Create PHYS asset — right-click mesh, Create > Physics Asset".

6. Bone count: must match SK_Mannequin's bone count. If off by more than 5,
   WARN — probably has extra cosmetic-only bones, verify they don't conflict.

7. Bounds: check approximate height (1.7–2.0m for human-scale). If outside
   range, WARN.

Output as a checklist with PASS/WARN/FAIL per item and a final verdict:
READY_FOR_PIPELINE / NEEDS_DCC_WORK / NEEDS_UE5_CONFIG.
```

### AIK-08 — Generate Physics Asset For Imported Mesh

**When to use**: Phase 3, after every mesh import.

```
For the SkeletalMesh at <path>, generate a Physics Asset:

1. Right-click the mesh → Create > Physics Asset > Generate
2. Settings:
   - Minimum Bone Size: 5.0
   - Collision Geometry: SingleConvexHull (per-bone)
   - Vertex Weighting Threshold: 0.1
3. Open the generated PHYS asset and tune:
   - Hands (hand_l, hand_r): set body Mode to Kinematic (don't ragdoll)
   - Head: scale collision capsule to 80% of auto-generated size
   - Neck constraint: Angular Motion = Limited, Swing1Motion=Limited (45°),
     Swing2Motion=Limited (30°), TwistMotion=Limited (30°)
   - Spine_01 constraint: similar limits
   - Feet (foot_l, foot_r): set body Mode to Kinematic
4. Test ragdoll: in a test map, spawn the character, kill it, verify the
   ragdoll falls naturally without limb detachment or jitter
5. Save PHYS asset and assign to the SkeletalMesh

Report: PHYS asset created, manual tuning applied, ragdoll test passed/failed.
```

### AIK-09 — Bulk Populate Skin Catalog DataTable

**When to use**: Phase 6, importing N skin definitions into the catalog
in one operation.

```
For the DataTable at /Game/Cosmetics/Data/DT_<Project>SkinCatalog (row
struct F<Project>SkinCatalogRow), add the following rows:

[
  {
    RowName: "NightOps_Default",
    SkinAsset: /Game/Cosmetics/Skins/NightOps/DA_<Project>Skin_NightOps_Default,
    SkinId: "NightOps_Default",
    DisplayName: "Night Ops",
    RarityTag: "Cosmetic.Rarity.Epic",
    CurrencyTag: "Currency.PremiumGem",
    BaseCost: 1200,
    SeasonTag: "Cosmetic.Season.SeasonOne",
    ShopThumbnail: /Game/Cosmetics/Skins/NightOps/T_NightOps_Thumb,
    OwningGameFeaturePlugin: ""
  },
  {
    RowName: "Heritage_Red",
    ...
  },
  ...
]

For each row:
- Verify SkinAsset path resolves to a valid U<Project>SkinAsset
- Verify ShopThumbnail path resolves to a valid UTexture2D
- Verify RarityTag and SeasonTag are registered gameplay tags
- If any reference fails, add a "placeholder" log entry and continue

After all rows added, save the DataTable and report:
- Rows added successfully: N
- Rows with broken references: M (list)
- Total catalog row count after operation
```

### AIK-10 — Generate IK Retarget Chain Setup

**When to use**: Phase 2, when bringing an external rig onto Lyra's
SK_Mannequin via IK retargeting.

```
For the external character at /Game/Characters/External/SK_<SourceRig>:

1. Create IK Rig asset:
   - Right-click → Animation > IK Rig
   - Name: IK_<SourceRig>
   - Preview Mesh: SK_<SourceRig>
   - Retarget Root: pelvis bone (or equivalent root on the source)

2. Add Retarget Chains (each chain is Start Bone → End Bone):
   - Root: pelvis
   - Spine: spine_01 → spine_05 (or equivalent count on source)
   - Head: neck_01 → head
   - LeftArm: clavicle_l → hand_l
   - RightArm: clavicle_r → hand_r
   - LeftLeg: thigh_l → ball_l
   - RightLeg: thigh_r → ball_r

3. Save IK_<SourceRig>.

4. Create IK Retargeter asset:
   - Right-click → Animation > IK Retargeter
   - Name: RTG_<SourceRig>_To_Mannequin
   - Source IK Rig Asset: IK_<SourceRig>
   - Target IK Rig Asset: IK_Mannequin (existing Lyra asset)

5. Configure chain mappings:
   - Root → Root
   - Spine → Spine
   - Head → Head
   - LeftArm → LeftArm
   - RightArm → RightArm
   - LeftLeg → LeftLeg
   - RightLeg → RightLeg

6. Open "Edit Retarget Pose" mode and verify the source's reference pose
   visually aligns with the target's. If misaligned, rotate source bones
   until poses match (this is critical for clean retargeting).

7. Save the retargeter.

8. Test: select a Lyra animation (e.g. /Game/Characters/Heroes/Mannequin/
   Animations/Locomotion/MM_Run_Fwd), right-click → Retarget Animations →
   use RTG_<SourceRig>_To_Mannequin. Output should be a retargeted version
   playable on SK_<SourceRig>.

Report: IK Rig + Retargeter created, retarget test result (pose looks
clean / has visible drift / failed).
```

---

## Midjourney / DALL·E Prompts for Skin Concept Art

Concept art prompts for the design team to generate skin variations
before committing to DCC work. Match the existing project's visual
language.

### MJ-01 — Hero Skin Concept (Sci-Fi)

```
full body character concept art, futuristic exo-suit, sleek hardsurface
armor plating, glowing accent lines in cyan, asymmetric shoulder pauldron,
helmet with visor, weathered metallic finish with chipped paint,
photorealistic rendering, cinematic three-quarter pose, neutral grey
studio background, top-down soft key light, character sheet front and
back view, concept art for AAA game, in the style of Aaron Beck and
Doug Williams, 8k, sharp detail --ar 9:16 --style raw --v 6
```

### MJ-02 — Hero Skin Concept (Fantasy)

```
full body character concept art, fantasy warrior, ornate plate armor
with engraved runes, fur-trimmed cape, long sword sheathed on the back,
helmet with curved horns, weathered bronze with verdigris patina,
dramatic painterly rendering, three-quarter pose, atmospheric fog
background, golden hour rim light, character sheet front view, in the
style of Jaime Jones and Karla Ortiz, 8k --ar 9:16 --style raw --v 6
```

### MJ-03 — Glass/Apple-Aesthetic UI Mockup

```
ui design for character cosmetic shop, frosted glass cards floating
over dark backdrop, soft inner glow on hovered card, character preview
on the left, skin grid on the right, premium currency icon in top
corner, Apple visionOS aesthetic, spatial depth layering, translucent
panels with subtle gaussian blur, SF Pro typography, deep navy
background with star particles, ui concept, ultra clean, 8k --ar 16:9
--style raw --v 6
```

### MJ-04 — Variant Color Exploration

```
character concept variations, same exo-suit silhouette, six color
variants in a single image: arctic white with silver trim, jungle camo
green, desert tan with bronze accents, urban grey with red highlights,
deep navy with gold trim, all-black stealth with subtle blue glow,
clean turntable presentation, front three-quarter view, neutral grey
backdrop, soft studio lighting, character lineup, concept art, 8k
--ar 16:9 --style raw --v 6
```

### MJ-05 — Modular Part Exploration (Helmets)

```
character helmet concept variations, six helmet designs in a single
image grid (3x2), all matching a sleek sci-fi soldier aesthetic, each
with a different silhouette: closed full-face visor, open-jaw helmet
with cyan glow, tactical balaclava-style with goggles, ornate dress
helmet with crest, breathing apparatus mask, ceremonial featureless
faceplate. clean turntable presentation, isolated on neutral background,
front three-quarter angle, prop concept art, sharp detail, 8k --ar 16:9
--style raw --v 6
```

### MJ-06 — Hero Card / Shop Tile Render

```
character beauty shot for a video game shop card, dynamic three-quarter
hero pose, weapon held confidently, dramatic backlit silhouette with
volumetric god rays, atmospheric haze, particle dust motes, deep contrast
lighting, cinematic composition, painterly digital art, used as a hero
banner in a shop ui, in the style of Wojtek Fus, 8k --ar 9:16 --style
raw --v 6
```

### MJ-07 — Rarity Frame Concept

```
ui frame design for character cosmetic, ornate vector border in cool
silver for "rare" tier, rich gold for "epic", iridescent prismatic for
"legendary", art deco geometric motifs, subtle inner glow, transparent
center, isolated on dark background, game ui asset sheet, three variants
side by side, vector style, ultra clean, 8k --ar 16:9 --style raw --v 6
```

### MJ-08 — Texture Reference Sheet

```
PBR material reference sheet for video game character, brushed steel with
slight oxidation, leather strap material, woven nylon fabric, painted
plastic with subtle scratches, glowing emissive panel with dot matrix
pattern, six material samples arranged in a 2x3 grid, photorealistic
microsurface detail, neutral lighting, used as texture reference for 3d
artists, 8k --ar 16:9 --style raw --v 6
```

---

## Prompt Engineering Tips for Cosmetic Workflows

When iterating prompts for cosmetic-specific work, these patterns produce
consistent results:

```
✓ Always specify "concept art" or "video game character" — keeps output
  in the right visual register
✓ Anchor to a specific artist style when you find one that matches your
  brand (e.g. "in the style of Aaron Beck")
✓ For variants, generate the whole set in one image — Midjourney maintains
  visual consistency within a single generation better than across multiple
✓ Use --ar 9:16 for hero character shots (matches shop card aspect)
✓ Use --ar 16:9 for landscape UI mockups
✓ "Three-quarter pose" beats "front pose" for hero art — more dynamic
✓ Specify lighting direction explicitly — "soft key from upper left",
  "rim light from behind", "golden hour" — generic "good lighting" is wasted

✗ Don't ask Midjourney to generate "the existing skin we have" — it can't
  reproduce specific characters; describe attributes instead
✗ Don't ask for UI screenshots with text — text rendering is unreliable;
  describe layout and label content separately, add text in post
✗ Don't expect Midjourney output to be production-ready art; it's the
  starting point for the concept team to refine
```

---

## When to Use AIK vs Manual Authoring

```
USE AIK FOR:
  ✓ Repetitive asset creation (50 MIs from a list)
  ✓ Scaffolding new asset types (one-time per category)
  ✓ Validation passes (check N imported meshes against spec)
  ✓ DataTable bulk population from a spec document
  ✓ Generating boilerplate Blueprints

DON'T USE AIK FOR:
  ✗ Final art tuning (artist's eye still needed for the nuance)
  ✗ Anything that requires testing in PIE between steps (AIK can't
    actually run gameplay tests reliably)
  ✗ Performance optimization (needs profile data + intent)
  ✗ Critical gameplay code (always human-reviewed, AIK is a junior dev
    that needs supervision)
```

---

## Verification Checklist — AI Prompt Usage

```
□ AIK profile has the right tool set enabled for the prompt being run
□ The prompt's expected output is verifiable (asset exists, compiles, etc.)
□ AI-generated assets validated against the gotchas reference before commit
□ Midjourney prompts saved with the resulting concept art in the design doc
□ Iteration history kept — successful prompts archived for reuse
```
