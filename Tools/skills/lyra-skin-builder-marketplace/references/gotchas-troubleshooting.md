# Gotchas & Troubleshooting Reference

Phase 10 of the pipeline, but continuously relevant from Phase 3 onward.
The failure modes below are what bite teams in production — listed by
symptom (what the user sees first), then root cause, then verified fix.

When debugging a reskin issue, start here. Most "we tried this 3 times and
it keeps breaking" stories trace to one of these.

---

## G01 — Imported Mesh Shows As T-Pose Only / No Animations Play

**Symptom**: Character imports cleanly, drops into the world, but only ever
T-poses. Lyra's anim BP appears to be running, but the mesh doesn't move.

**Diagnosis steps**:
```
1. Open the SkeletalMesh asset → Skeleton field
2. If it reads anything other than "SK_Mannequin", you have the wrong
   skeleton bound
3. Open the assigned Skeleton asset → check bone tree
4. Compare bone names against SK_Mannequin's bone tree
```

**Root cause**: Bone names in the imported FBX don't match SK_Mannequin
exactly, so UE5 created a brand-new skeleton on import. The Lyra AnimBP
targets SK_Mannequin's bones by name — they don't exist on the new
skeleton, so no animation drives the mesh.

**Fix**:
1. Re-export the FBX from DCC with Mannequin bone names (`pelvis`, `spine_01`,
   `spine_02`, ..., `head`, `upperarm_l`, etc.)
2. Re-import into UE5, this time selecting `SK_Mannequin` in the Skeleton
   field — do NOT let it create a new skeleton
3. Delete the orphan skeleton asset that was created during the first import

**Prevention**: Always export from DCC with the Mannequin skeleton as the
target. Use Epic's official Mannequin reference FBX in your DCC scene as the
canonical bind target. Never rename bones during binding.

---

## G02 — Mesh Renders Black / Checkerboard / Wrong Textures Per Region

**Symptom**: Character imports and animates, but appears as solid checker
material, or has the hair texture on the body, or the body texture on
the eyes, etc.

**Diagnosis steps**:
```
1. Open the SkeletalMesh asset → Material slots section
2. Note the slot names and order
3. Compare against Lyra's expected slot order:
   [0] Body  [1] Outfit  [2] Hair  [3] Eyes  [4] Eyebrows  [5] Eyelashes
4. Check each slot's assigned material — if Slot 0 has the hair material
   assigned, you've found the issue
```

**Root causes** (in order of likelihood):
1. Material slot order in the FBX doesn't match the Lyra convention
2. Texture references on the master material are stale (point to /Game/
   textures that don't exist)
3. Skin Cache disabled at project level (checkerboard is the fallback)
4. Material assignment was never done — UE5 imported with default M_Default

**Fix**:
```
Option A — Reorder slots in DCC and re-import (cleanest)
  Open the FBX scene, ensure materials are assigned in this order:
    Body material first, then Outfit, then Hair, etc.
  Re-export and re-import.

Option B — Remap slots in the SkeletalMesh asset (faster)
  Open the SkeletalMesh → Asset Details → Material section
  Drag slots into the correct order
  Reassign correct MI to each slot

Option C — Author a custom slot order convention
  If your skin legitimately doesn't fit Lyra's slot order (different
  segmentation), document the convention and ship a custom master material
  expecting your order. Apply consistently across all skins.
```

**Prevention**: Hand DCC artists a slot-order spec doc. Validate slot
naming on import via an asset validator that warns on mismatch.

---

## G03 — Mesh Renders But Casts No Shadows (Or Has Weird Lighting)

**Symptom**: Character is visible but appears flat-lit, casts no shadow on
the ground, or has obviously wrong specular highlights.

**Diagnosis steps**:
```
1. Project Settings → Rendering → Optimizations
2. Look for "Support Compute Skin Cache" — is it checked?
3. Look for "Default Skin Cache Behavior" — is it "Inclusive"?
4. Per-mesh: open the SkeletalMesh → Asset Details → Optimization
5. "Skin Cache Usage" — is it Enabled or "Project Default"?
```

**Root cause**: GPU Skin Cache is required for Lyra-style characters to
participate in modern UE5 rendering features (shadow casting via skinned
mesh, distance fields, certain post processing). Without it, characters
fall back to a legacy CPU skinning path that doesn't shadow correctly.

**Fix**:
1. Project Settings → Rendering → Optimizations:
   - ☑ Support Compute Skin Cache: ON
   - Default Skin Cache Behavior: Inclusive
2. Restart editor (this setting affects engine init)
3. Verify character now casts shadows

**Mobile note**: Skin Cache is more expensive on mobile. Enable for hero
characters only — disable per-mesh for NPCs by setting Skin Cache Usage
to Disabled on those specific assets.

---

## G04 — Ragdoll Crashes / Limbs Detach / Physics Is Wrong

**Symptom**: Character dies, ragdoll triggers, and either crashes the
editor / game, or limbs fly off into the distance, or the character
collapses into a flat puddle on the floor.

**Diagnosis steps**:
```
1. Check what PHYS_ asset is assigned to the SkeletalMesh
2. If it's PHYS_Mannequin (Lyra's stock), but your mesh has different
   proportions, that's the issue — collision capsules don't match the new
   mesh
3. If it's missing entirely (None), ragdoll has no constraints and limbs
   detach
```

**Root cause**: A new SkeletalMesh needs its own PHYS_ asset matched to its
proportions. Reusing PHYS_Mannequin works only when the new mesh has near-
identical bone lengths to Manny/Quinn.

**Fix**:
```
1. Right-click SK_<Project>Char_<SkinName>_Body
2. Asset Actions → Create → Physics Asset
3. Settings:
   - Minimum Bone Size: 5.0 (default OK)
   - Collision Geometry: Single Convex Hull or Capsule per bone
   - Vertex Weighting Threshold: 0.1
4. After generation, open the PHYS asset and:
   - Inspect each body — auto-gen rarely gets hands/feet/head right
   - Manually adjust the capsule for the head (often too large)
   - Set hand bones to "Kinematic" or "Default" — usually NOT simulated
     for ragdoll (hands flap weirdly otherwise)
   - Tune neck/spine constraints to "Limited" angular motion
5. Save and assign to the SkeletalMesh
6. Test ragdoll in PIE — verify no crash and reasonable physical behavior
```

**Prevention**: Bake into asset pipeline — every imported character mesh
automatically triggers PHYS generation as an asset validator step.

---

## G05 — GameFeature Plugin's Cosmetics Don't Appear In Shop

**Symptom**: Plugin shows as Active in the GameFeature management UI, but
the shop UI doesn't show any of the plugin's skins.

**Diagnosis steps**:
```
1. Check the plugin's GameFeatureData asset
2. Look at its Actions array
3. Is there an action that registers the catalog DataTable?
4. Check Output Log for "Activated cosmetic plugin: <url>" log line
5. In console, run: ListCosmeticCatalogRows (if you implemented this debug cmd)
6. Verify the plugin's DT rows are present
```

**Root cause**: The plugin activated (asset registry picked up its content)
but the catalog subsystem doesn't know about the new rows because no action
explicitly registers them.

**Fix**:
1. Open the plugin's `<Project>Cosmetics_<DropName>_GameFeatureData` asset
2. Add an action:
   - Add `U<Project>GameFeatureAction_RegisterCatalog`
   - Set `CatalogChunk` to the plugin's `DT_<Project>SkinCatalog_<DropName>`
   - Set `SeasonTag` if applicable
3. Save the GameFeatureData
4. Deactivate and reactivate the plugin to trigger the new action
5. Verify catalog subsystem now has the rows

**Bonus issue**: If you have multiple GameFeature plugins shipping at once
and they reference the same `SkinId`, you have a collision. The catalog
will use whichever activated last. Prefix every plugin's skin ids with
the plugin name: `SeasonOne_AlphaHelmet`, `SeasonTwo_AlphaHelmet`.

---

## G06 — Modular Parts Spawn At Wrong Location / Rotation

**Symptom**: Helmet spawns at the character's feet, backpack appears
floating 50cm above the spine, holster is attached to the wrong hand.

**Diagnosis steps**:
```
1. Open the SK_Mannequin asset → Skeleton tab → Sockets section
2. Note the exact socket names (case-sensitive)
3. Open the part's data asset → SocketTag value
4. Check that the GameplayTag's leaf name matches a real socket
```

**Root cause**: Either the socket name doesn't exist on the skeleton, or
the `FLyraCharacterPart::SocketTag` resolves to a different socket than
expected.

**Fix**:
```
1. Confirm the socket exists on SK_Mannequin:
   weapon_r          ← right hand weapon
   weapon_l          ← left hand weapon
   head_socket       ← helmets, hats
   spine_socket      ← backpacks, capes
   pelvis_socket     ← holsters
2. If the socket doesn't exist, add it:
   Open SK_Mannequin → Skeleton tab → right-click parent bone → Add Socket
   Name it appropriately, position via the gizmo to match the part
3. Make sure the GameplayTag maps to the socket name:
   "Cosmetic.Socket.HeadAttach" should map to socket "head_socket"
4. The mapping happens in the parts component — check ULyraPawnComponent_
   CharacterParts's tag-to-socket resolution logic
```

**Custom socket placement**: If a part needs a unique attach point not on
the stock skeleton, add a new socket. Sockets are non-destructive — they
don't break SK_Mannequin's mergeability with upstream Lyra.

---

## G07 — Cosmetic Preview Shows Base Manny Instead Of Selected Skin

**Symptom**: Shop preview viewport always shows the default mannequin even
when the player clicks different skins. The thumbnails change, but the 3D
preview doesn't.

**Diagnosis steps**:
```
1. Set a breakpoint in U<Project>CosmeticShopPreview::ShowSkin
2. Verify the function is called when the player clicks a tile
3. Check whether ApplySkin gets called on the preview actor
4. Check whether the async load completes (lambda fires)
```

**Root cause options** (in likelihood order):
1. The preview actor's ApplySkin is called, but EnsureMIDsCreated is not
   re-run after the mesh swap — MIDs still point to the old material set
2. The selection event isn't wired — clicking a tile changes the focused
   row but ShowSkin is never invoked
3. The async load lambda captures `this` weakly and the preview widget
   was deactivated/destroyed before the load completed (no-op)
4. The preview actor was destroyed by sub-level unload between tile click
   and load completion

**Fix**:
```cpp
// In ShowSkin, fully refresh the preview after async load
void U<Project>CosmeticShopPreview::OnSkinAssetLoaded(U<Project>SkinAsset* SkinAsset)
{
    if (!IsValid(SkinAsset)) { return; }
    if (!PreviewActor.IsValid()) { return; }

    // ApplySkin handles mesh swap; re-create MIDs after
    PreviewActor->ApplySkin(SkinAsset->Definition);

    // Force MIDs to refresh — this is the step everyone forgets
    PreviewActor->EnsureMIDsCreated();

    // Re-apply any preview-specific tints (team color preview slider)
    PreviewActor->SetTeamColor(CurrentPreviewTint);
}
```

**Selection wiring**: In the browser widget, ensure:
```cpp
SkinListView->OnItemSelectionChanged().AddDynamic(
    this, &UThisClass::HandleSelectionChanged);
```
Not `OnItemClicked` — selection changes via gamepad navigation too.

---

## G08 — Server Kicks Client On Skin Equip / "Cheater" False Positive

**Symptom**: Player equips a skin in the loadout menu, returns to gameplay,
and gets disconnected with an anti-cheat warning. Server logs show
"player attempted to equip unowned cosmetic".

**Diagnosis steps**:
```
1. Check server log around the equip event
2. Verify the entitlement subsystem on the SERVER has a record of this
   skin id for this player
3. Check the local save data — does it list this skin as owned?
4. Check if save data diverges from backend
```

**Root cause**: The client believes it owns the skin (cached in local save
data), but the server's entitlement subsystem has no record. The server
correctly rejects the equip request, but the rejection path is too
aggressive (kicks the player instead of falling back to default skin).

**Fix paths**:
```
A. The player actually doesn't own it (save data corrupt or tampered):
   - Server falls back to default skin silently
   - Don't kick — log the attempt, count toward soft anti-cheat metric
   - Re-sync entitlements from backend on next opportunity

B. The player does own it but the server hasn't synced:
   - Force the EntitlementSubsystem to query backend before kicking
   - Reject the equip request, return a "syncing — try again" response
   - Client retries after sync completes

C. The entitlement was just granted (within seconds) and replication race:
   - Add a brief grace period (1–2 seconds) after grant before validating
     equip requests for that skin
```

```cpp
// In the equip RPC, prefer fallback over kick
void A<Project>CosmeticCharacter::ServerEquipPart_Implementation(
    U<Project>CharacterPartDefinition* PartDef)
{
    if (!HasEntitlementForPart(PartDef))
    {
        // Don't kick. Fall back. Log for analytics.
        UE_LOG(LogCosmetics, Warning,
            TEXT("Player %s attempted to equip unowned %s — falling back"),
            *GetPlayerName(), *PartDef->GetName());

        // Force sync from backend in case our cache is stale
        EntitlementSubsystem->SyncLocalPlayerEntitlements();

        // Notify client to refresh its UI
        ClientEntitlementRefreshNeeded();
        return;
    }

    // ... actual equip
}
```

---

## G09 — Mobile Build Crashes On Shop Open / Out Of Memory

**Symptom**: Game runs fine on PC. On iOS or Android, opening the shop
either crashes immediately or causes a hitched scroll, then crashes after
~10 seconds of scrolling.

**Diagnosis steps**:
```
1. Run with -trace=memory and inspect spike on shop open
2. Check Memreport.exe output: which assets jumped from 0 → many MB
3. Look at Texture LOD streaming — are skin thumbnails loaded at full mip?
4. Check if hero preview images are loading on grid display (they should not)
```

**Root cause options**:
1. ShopThumbnail textures are too large (4K when they only need 256×256)
2. Texture compression format is wrong for mobile (DXT/BC instead of ASTC)
3. All thumbnails load synchronously on grid display
4. The grid is non-virtualized — every row's entry widget exists in memory
5. HeroImage (large preview) loads on grid hover, not on preview activate

**Fix**:
```
A. Texture compression per platform:
   Open each T_<Skin>_Thumb texture
   Asset Details → Compression Settings: TC_Default (BC1/BC3 on PC, ASTC
     on mobile — UE5 handles per-platform automatically if texture group
     is set correctly)
   Texture Group: UI (set to UI_Mobile for shop thumbnails)
   Max Texture Size: 256 (or 512 for higher rarities)

B. DefaultEngine.ini override for mobile:
   [/Script/Engine.TextureLODGroup]
   +TextureLODGroups=(Group=TEXTUREGROUP_UI, MinLODSize=64, MaxLODSize=512,
     LODBias=0, MinMagFilter=aniso, MipFilter=point, MipGenSettings=
     TMGS_FromTextureGroup, NumStreamedMips=-1)

C. Verify virtualization:
   Shop browser must use UCommonListView, not a fixed UGridPanel
   Confirm entry widgets are destroyed when scrolled off-screen
   Use insights to verify constant entry count regardless of catalog size

D. HeroImage loads on demand only:
   In the entry widget, only load ShopThumbnail soft ref
   HeroImage soft ref loads only when ShowSkin is called in preview widget
```

---

## G10 — Cooked Game Can't Find Skin Assets / "Asset Not Found"

**Symptom**: Editor and PIE work perfectly. Cooked Development or Shipping
build runs fine in gameplay, but opening the shop shows blank tiles or
crashes with "asset path not found".

**Diagnosis steps**:
```
1. Check the cook log for the missing asset path — was it cooked?
2. Open the cooked .pak with a pak browser — is the asset present?
3. If missing, check why the cooker excluded it
```

**Root cause**: AssetManager's primary asset rules don't include your skin
content paths, so the cooker doesn't include them in the cooked output.
Soft references to assets outside the cooked set fail at runtime.

**Fix**:
```ini
; DefaultGame.ini

[/Script/Engine.AssetManagerSettings]

+PrimaryAssetTypesToScan=(PrimaryAssetType="<Project>Skin",
    AssetBaseClass=/Script/<Project>Game.<Project>SkinAsset,
    bHasBlueprintClasses=False,
    bIsEditorOnly=False,
    Directories=((Path="/Game/Cosmetics/Skins")),
    SpecificAssets=,
    Rules=(Priority=-1, ChunkId=-1, bApplyRecursively=True,
           CookRule=AlwaysCook))

+PrimaryAssetTypesToScan=(PrimaryAssetType="<Project>CharacterPart",
    AssetBaseClass=/Script/<Project>Game.<Project>CharacterPartDefinition,
    bHasBlueprintClasses=False,
    bIsEditorOnly=False,
    Directories=((Path="/Game/Cosmetics/Parts")),
    SpecificAssets=,
    Rules=(Priority=-1, ChunkId=-1, bApplyRecursively=True,
           CookRule=AlwaysCook))

; Also ensure /Game/Cosmetics/Materials and Textures cook:
+DirectoriesToAlwaysCook=(Path="/Game/Cosmetics/Materials")
+DirectoriesToAlwaysCook=(Path="/Game/Cosmetics/Textures")
+DirectoriesToAlwaysCook=(Path="/Game/Cosmetics/Meshes")
```

**For GameFeature plugin content**: each plugin's own `DefaultGame.ini` (in
the plugin's `Config/` folder) must register its content paths too. The
plugin's content isn't automatically inherited under the base game's rules.

**Verification**: Run a cook with `-log` and grep for your skin asset
paths. They should appear in the "AlwaysCook" set. If they appear under
"Excluded", the rule didn't match.

---

## G11 — Two Skins Cause Shader Compilation To Spike

**Symptom**: Game cook takes 4 hours instead of 30 minutes. Shader compile
warns about 8,000+ shader permutations.

**Root cause**: Each Material Instance with a different parameter combination
that affects the shader (static switches, quality overrides) generates a
new shader permutation. Authoring habits like "each skin gets its own master
material" balloon permutation counts.

**Fix**:
```
✓ ONE master material M_<Project>_Character for ALL skin variants
✓ MIs only override parameters that don't fork the shader (scalars,
  vectors, textures)
✓ Static switches OFF on per-skin MIs — only flip them on the master
✓ Use Material Function Calls for variant features (detail layer, etc.)
  rather than static branches in the master
✗ Don't author 50 master materials "to keep skins organized"
✗ Don't use static switches per skin to toggle features
```

Inspect permutation count:
```
Window → Statistics → Shader Cooker Stats
Sort by permutation count
The top entries are your problem materials — flatten them
```

---

## G12 — Equipped Skin Doesn't Replicate To Other Players

**Symptom**: In multiplayer, the local player sees their equipped skin
correctly. Other players see them as the default Manny/Quinn.

**Diagnosis steps**:
```
1. Verify ULyraPawnComponent_CharacterParts is on the pawn (not the controller)
2. Check the parts component has bReplicates = true
3. In the equip RPC, verify it actually calls PartsComponent->AddCharacterPart
4. Use NetDormancy debugging: are the parts being replicated at all?
```

**Root cause**: Either the parts component isn't replicated, or the equip
happened on the client-side only without going through the server RPC.

**Fix**:
```cpp
A<Project>CosmeticCharacter::A<Project>CosmeticCharacter()
{
    PartsComponent = CreateDefaultSubobject<ULyraPawnComponent_CharacterParts>(
        TEXT("PartsComponent"));

    // Critical: enable replication on the component
    PartsComponent->SetIsReplicated(true);

    // Set net update frequency appropriate for cosmetic changes
    // Cosmetics don't need 60Hz updates — equip events are infrequent
    NetUpdateFrequency = 10.0f;
}
```

And ensure all equip paths go through `ServerEquipPart` RPC, never via
direct calls on the client side.

---

## G13 — Vaulted Skins Disappear From Owned Loadout

**Symptom**: A season ends. A player who owned a skin from that season can
no longer equip it — the skin doesn't even appear in their owned list.

**Root cause**: Deactivating the GameFeature plugin removed the catalog
rows entirely. The shop UI correctly hides them (vaulted). But the loadout
UI uses the same catalog query and incorrectly hides them too.

**Fix**: Distinguish "available to purchase" from "available to equip".

```cpp
// Shop UI — filter to currently active plugins only
TArray<F<Project>SkinCatalogRow> ShopRows =
    Catalog->GetAllRows()
    .FilterByPredicate([](const F<Project>SkinCatalogRow& Row)
    {
        // Only show in shop if plugin is active OR base game skin
        return IsBaseGameSkin(Row) || IsPluginActive(Row.OwningGameFeaturePlugin);
    });

// Loadout UI — show everything the player owns, even from vaulted plugins
TArray<FName> OwnedIds = EntitlementSubsystem->GetOwnedSkins(LocalPlayerId);
TArray<F<Project>SkinCatalogRow> LoadoutRows;
for (const FName& Id : OwnedIds)
{
    F<Project>SkinCatalogRow Row;
    if (Catalog->GetCatalogRow(Id, Row))
    {
        LoadoutRows.Add(Row);
    }
}
```

For vaulted plugins, the catalog row needs to still be accessible. Solution:
keep vaulted plugin metadata in the catalog (read-only) even when the plugin
is deactivated. The actual mesh/material assets stay loaded for owned skins
via the entitlement-driven keepalive pattern (force-keep loaded for owned).

Alternative simpler solution: don't actually deactivate the plugin — just
filter shop UI to hide vaulted plugins' skins from purchase. Player can
still equip what they own because the plugin's content is still loaded.

---

## G14 — Modular Part Animations Drift From Body

**Symptom**: Heavy outfit equipped. Body anim and outfit anim look fine at
LOD0 close-up. From a distance (LOD2+), the outfit shoulders float behind
the body's shoulder motion by a few frames.

**Root cause**: The outfit's skeletal mesh component has its own AnimInstance
that ticks independently of the parent body. Different LOD tick rates
desync the two.

**Fix**:
```cpp
// In A<Project>CharacterPart_Skinned::OnAttachedToPawn
MeshComponent->SetLeaderPoseComponent(ParentMesh, true);
MeshComponent->SetAnimInstanceClass(nullptr);  // NO independent anim

// Also enable LOD sync — the part follows the parent's LOD
MeshComponent->bSyncAttachParentLOD = true;
```

LeaderPoseComponent means the outfit copies the parent's bone transforms
exactly per frame — no independent anim tick, no drift, much cheaper.

---

## G15 — Cosmetic Plugin Won't Activate On Console Builds

**Symptom**: Plugin activates fine on Win64 / PC. On PS5 or Xbox builds,
`LoadAndActivateGameFeaturePlugin` returns an error: "Plugin not found".

**Root cause**: Plugin's pak file wasn't included in the console build. This
happens when:
1. The plugin's `Installed` property is false in the .uplugin (the plugin
   is treated as a separate install on consoles)
2. Console-specific cook settings exclude the plugin
3. The Project Launcher cook command didn't list the plugin

**Fix**:
```json
// In <Project>Cosmetics_<DropName>.uplugin
{
  ...
  "Installed": true,    ← include in main build
  ...
}
```

For genuinely separate cosmetic DLC on console, the workflow is different:
the plugin ships through the platform's DLC system (PSN, Xbox Live), not
in the base game pak. That's a more complex setup involving platform
SDKs and isn't covered here — see your platform's DLC documentation.

---

## G16 — IK Retarget Output Has Bent Knees / Twisted Shoulders

**Symptom**: Retargeted character animation runs but knees are slightly
bent in idle, shoulders are visibly rotated forward, hands don't grip
weapons cleanly.

**Root cause**: The retarget pose for the source rig wasn't adjusted to
match Mannequin's reference pose. The IK retargeter applies pose deltas
on top of the source's bind pose — if the bind poses differ, animations
inherit those differences.

**Fix**:
```
1. Open RTG_<SourceRig>_To_Mannequin
2. Top-left mode dropdown: switch to "Edit Retarget Pose"
3. Two viewports: Source on left, Target on right
4. Rotate the source's bones until its pose visually matches the target's
   reference pose:
   - Arms straight down at sides (or T-pose, whichever Mannequin uses)
   - Spine vertical
   - Legs straight, feet flat
   - Hands relaxed
5. The deltas you apply here are baked into the retarget calculation
6. Save and re-test — animations should now look natural on the retargeted
   character
```

This is the single highest-impact step in IK retargeting. Skipping it is
why most people think "IK retarget doesn't work well" — it works fine,
it just needs the pose adjustment.

---

## G17 — Custom Material Function Crashes Cook With "Cyclic Dependency"

**Symptom**: A material function used in M_<Project>_Character references
another material function that references back, and cook errors out with
"Cyclic dependency detected".

**Fix**: Audit your Material Function Call graph. Material functions cannot
reference each other in a cycle. Break the cycle by inlining one of the
shared sub-functions, or by using Material Layers (which have a different
dependency model) for the shared piece.

---

## G18 — Cosmetic Particle FX (GameplayCues) Don't Trigger From Plugin

**Symptom**: A GameFeature plugin's skin includes a unique particle effect
(e.g. legendary skin has a glow trail). The skin equips correctly but the
particle effect never plays.

**Root cause**: Lyra's `ULyraGameplayCueManager` only searches default paths
for cue assets. Plugin content lives outside those paths.

**Fix**: In the plugin's GameFeatureData, add a `UGameFeatureAction_AddGameplayCuePath`
action pointing to the plugin's `/Effects/` content path. The cue manager
picks up new paths on plugin activation.

---

## G19 — Eyes / Eyelashes / Hair Look Wrong After Reskin

**Symptom**: Body looks great. Eyes are flat / dead. Eyelashes are solid
black squares. Hair clips through the head.

**Root cause**: Lyra's stock eye/eyelash/hair materials use specific
shading models (Eye, Hair) and translucency settings that don't translate
to a generic master material.

**Fix**: For eyes, eyelashes, and hair, **don't replace the stock Lyra
materials** unless you have artist expertise in those shaders. Just use
Lyra's stock `MI_Eye`, `MI_Eyelashes`, `MI_Hair` and tweak the texture
parameters per skin (eye color, hair color).

```
Slot 0: Body         → MI_<Project>Skin_<SkinName>_Body
Slot 1: Outfit       → MI_<Project>Skin_<SkinName>_Outfit
Slot 2: Hair         → MI_Lyra_Hair (parent), instanced with skin-specific color
Slot 3: Eyes         → MI_Lyra_Eye (parent), instanced with skin-specific iris
Slot 4: Eyebrows     → MI_Lyra_Eyebrows (parent)
Slot 5: Eyelashes    → MI_Lyra_Eyelashes (parent)
```

This is the rare case where reuse beats authoring — Epic's eye shader is
not trivial to replicate.

---

## G20 — Verification: How to Know Your Reskin Actually Shipped Correctly

A final-pass check after all the above:

```
□ Boot to main menu — Manny/Quinn or your default skin appears in lobby
□ Open shop — catalog populates within 1 frame, thumbnails load progressively
□ Scroll catalog — no hitches, memory stays bounded
□ Click a skin — 3D preview rotates to focus, applies within 200ms
□ Preview a Common, Rare, Epic, Legendary — all rarity frames visible
□ Purchase a skin — currency deducts, modal confirms, entitlement persists
   after game restart
□ Equip the new skin — character mesh swaps in lobby
□ Enter gameplay — character renders correctly, animations play correctly
□ Take damage / die — ragdoll behaves correctly
□ Multiplayer game — other players see your equipped skin
□ Disconnect / reconnect — equipped skin persists
□ Quit game, restart — owned skins still show as owned
□ Activate a GameFeature plugin — new skins appear in shop
□ Deactivate the plugin — skins vault from shop but stay equippable
□ Cook a Shipping build — no asset-missing errors, package size sane
□ Run on PS5 / Xbox / iOS / Android device — no platform crashes
□ Profile mobile memory — under platform limit during shop browse
```

If all 17 boxes check, the reskin & marketplace stack is shipped.
