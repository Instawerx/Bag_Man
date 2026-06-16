# AFL Identity Production Line — the repeatable per-identity build

**Status:** canonical recipe · **Date:** 2026-06-15 · **Proven base:** FANATICS @`f3ef4100` (the
first identity built under the Complete-Registration checklist; all 7 registrations passed,
runtime-PIE-verified). This doc is the repeatable line for the 30-name PREMIUM roster.

> One identity = **emblem + logo-baked, color-NEUTRALIZED body+limbs MI + a default finish +
> all 7 registrations.** Color comes from the FINISH (not baked); the LOGO is the per-identity
> differentiator. ~37 assets for the whole 30 (30 identity MIs + the shared finish presets),
> NOT 210 — because finishes compose at runtime. See ADR Decisions 5-7
> (`Docs/AFL_ECONOMY_ARCHITECTURE_ADR.md`).

Read this BEFORE building any identity. Read the ADR's Complete-Registration audit + the
`f3ef4100` tracker entry first if anything here is unclear — disk is truth (Pillar 5).

---

## The Complete-Registration checklist (the GATE — all 7 or flagged INCOMPLETE)

Every identity MUST satisfy ALL seven. Nothing ships riding a fallback (ADR Decision 7).

1. **Brand tag** — `Cosmetic.Brand.<NAME>` in `Config/DefaultGameplayTags.ini`.
   ⚠ **Tags load at STARTUP** — a freshly-added tag does NOT register until an editor
   RELAUNCH, and `import_text` SILENTLY DROPS an unregistered tag. So the two brand-tag-KEYED
   items (#3 BrandToEdge row + the robot's `StaticGameplayTags` brand tag) must be set
   POST-RELAUNCH. (Proven: FANATICS — set its tag in the ini, relaunched, then the keyed items took.)
2. **CharacterPartMap rows — BOTH axes** — `AFL.Character.<Name>` AND `AFL.Team.<Name>` →
   `B_AFL_Robot_<NAME>_C` in `DA_AFL_CharacterPartMap` (dual-register precedent, `f3ef4100`).
   Map value = assign the generated `UClass` directly (`m[FName]=bp.generated_class()`).
3. **BrandToEdge signature row** — `Cosmetic.Brand.<NAME>` → its signature `DA_AFL_Finish_*`
   in `DA_AFL_BrandEdgeMap`. EXPLICIT; never the fallback. (Tag-keyed → post-relaunch.)
4. **Robot BP** — `B_AFL_Robot_<NAME>` (clone of `B_AFL_Robot_IRONICS`, parent
   `AAFLCharacterPartActor`); its `StaticGameplayTags` carry AnimationStyle.Masculine +
   BodyStyle.Medium + `Cosmetic.Brand.<NAME>` (the brand tag → post-relaunch).
5. **Body MI + Limbs MI** — `MI_<NAME>_Body` (slot 0 / M_torso) + `MI_<NAME>_Limbs`
   (slot 1 / M_HeadLegs), **both identity-owned**, **both color-NEUTRALIZED** (Ruling 1).
6. **Logo** — the identity's emblem as `LogoTexture` param on the body MI (see EMBLEM below).
7. **Catalog rows** — `FAFLCatalogEntry` for both `AFL.Character.<Name>` + `AFL.Team.<Name>`
   in `DA_AFL_CosmeticCatalog`, with `ContentTier=Premium`, `CollectionId` (lane/family),
   `ColorIdentityTag` (descriptive color-family), `Rarity`.

---

## STEP A — EMBLEM (the per-identity mark)

The logo is a 1024² grayscale texture the stock LogoTexture slot recolors via the emissive
(the mip-blend confines it to a chest emblem). **Spec — verified against the shipped
`T_BigSixx_Logo_BC`** (and `ROBOT_SKIN_REMIX_PLAN.md`):

| Setting | Value | Why |
|---|---|---|
| Dimensions | **1024 × 1024** (power-of-2) | clean mip chain + UV tiling |
| Compression | **`TC_GRAYSCALE`** | the slot treats it as a mask recolored by emissive |
| sRGB | **false** (linear) | mask, not color |
| Mip gen | **`TMGS_BLUR5`** (mips ON) | the 3-tier mip blend (MipLevel 1/3/5) CONFINES the mark; no-mips = it bleeds across the torso |
| Position/size | MI scalars `LogoPos_X`, `LogoPos_Y`, `Scale` | place on the chest |

### LOCKED STEP-A sub-recipe (PROVEN on ONYX PRIME, `T_ONYXPRIME_Logo_BC`): Tripo text_to_image → import → spec-process

The winning, repeatable emblem path for the 30-name roster:
1. **PREREQUISITE — direct-provider mode.** `bUseBetideCredits=False` in
   `Config/DefaultAgentIntegrationKit.ini` (+ live settings). The default `true` routes ALL
   providers through `https://betide.studio` (proxy 404s — `GenerativeProviderBase.cpp:67-79`),
   which blocks Tripo + every generator. OFF = direct `api.tripo3d.ai/v2/openapi` with the real
   `TripoApiKey`. (Diagnosed + fixed this session.)
2. **Generate** — `generate({provider="tripo", action="text_to_image", prompt=..., wait=true})`
   → returns `job_id`. Prompt for: *bold flat WHITE letterforms on solid PURE BLACK, centered,
   geometric sans-serif, no gradient/3D/bevel/shadow/texture.* Tripo `text_to_image` returns a
   real standalone JPEG (`result_url`/`image_urls`), NOT a mesh — confirmed.
3. **Poll** — `generate({provider="tripo", action="check", job_id=...})` until `100% / SUCCEEDED`;
   read `result_url`.
4. **Download + INSPECT** — `curl` the `result_url` to `Saved/_<x>.jpg`; **Read the image** and
   judge the CLEAN criteria (legible monogram · white-on-black · flat · centered/tight). Time-box
   2-3 gens. (ONYX PRIME: iter-1 = clean legible OP, slightly low-left margin → ACCEPTED, because
   `LogoPos_X/Y`+`Scale` reposition at apply-time; iter-2 "fill the frame" over-zoomed/clipped →
   rejected. Lesson: prompt for *centered with even margins*, NOT "fill the entire frame".)
5. **Import + spec-process** — `AssetImportTask(filename=the .jpg, destination=.../Textures,
   name=T_<NAME>_Logo_BC, replace_existing, save)`, then set `compression=TC_GRAYSCALE`,
   `srgb=False`, `mip_gen=TMGS_BLUR5`, `max_texture_size=1024`,
   `power_of_two_mode=PAD_TO_POWER_OF_TWO` → 1024² spec-exact. (The bridge `action="import"`
   auto-names `gen_<hash>`; prefer the explicit AssetImportTask for a clean name.)

This SUPERSEDES the rejected in-engine render-target bake (sky/show-flag contamination, lost its
time-box) AND the BigNiagaraBundle SM-glyph bake (viable geometry exists — `Alphabet/SM_A..Z`,
`Numbers/SM_0..9` — but the geometry→texture step was never cleanly proven; Tripo is simpler +
proven). Tier-2 (operator-drop PNG) remains the fallback if Tripo is unavailable.

**Emblem source — TIERED decision rule (historical; Tripo-text_to_image is now the locked default):**
- **Tier 1 — BigNiagaraBundle glyph bake (PROVEN path, the DEFAULT).** Letters
  `/Game/BigNiagaraBundle/NiagaraSymbols/StaticMeshes/Alphabet/SM_A … SM_Z`, numbers
  `…/Numbers/SM_0 … SM_9`, symbols `…/Symbols/SM_Star` etc., runes `…/Runes/*` — all exist as
  STATIC MESHES (verified). Big Sixx's star came from here. Use Tier 1 for any emblem
  expressible as letters / numbers / the bundle's symbols/runes (covers all monograms).
- **Tier 2 — Tripo via the bridge** — for emblems the bundle can't express (custom logos,
  mascots). The blender_mcp/Tripo bridge (proven for the head gib). Reserve for non-glyph marks.
- **Tier 3 — generate an image** — last resort, only when 1+2 can't.
- **Decision rule:** monogram/letters/numbers/symbol → Tier 1. Custom non-text mark → Tier 2.
  Neither feasible → Tier 3. The priority-5 (OP/NK/V/A/R1) are ALL Tier 1.

**⚠ THE ONE UNPROVEN LINK (the canary establishes it):** the bundle gives the glyph
*geometry*; the existing logos (`T_BigSixx_Logo_BC`, `T_FANATICS_Logo_BC`) were already-baked
textures (FANATICS reused its ready texture; Big Sixx's star texture was authored). The
geometry→1024²-grayscale-texture BAKE has NOT been done via MCP in a cited build. The ONYX
PRIME canary must PROVE this step: render the SM glyph(s) to a render target → export/save as
`T_<NAME>_Logo_BC` at the spec, OR (fallback) compose the glyph(s) externally to the spec and
import. Whichever the canary proves becomes the locked bake sub-recipe here. Candidate MCP path:
`SceneCapture2D`/`TextureRenderTarget2D` (orthographic, white-on-black, glyph mesh) →
`RenderTargetLibrary.export_render_target` / `render_target_create_static_texture_2d` → set
compression/sRGB/mip to spec. Confirm at the canary; do not assume it works until watched.

---

## STEP B — ROBOT BP

Clone `B_AFL_Robot_IRONICS` → `B_AFL_Robot_<NAME>` (`AssetTools.duplicate_asset`). It inherits
`AAFLCharacterPartActor` + `SKM_Manny` + CopyPose. Set its mesh-component override materials
(SCS template — the persisting path): slot 0 = `MI_<NAME>_Body`, slot 1 = `MI_<NAME>_Limbs`.
Set `StaticGameplayTags` via `import_text('(GameplayTags=((TagName="Cosmetic.AnimationStyle.Masculine"),(TagName="Cosmetic.BodyStyle.Medium"),(TagName="Cosmetic.Brand.<NAME>")))')`
— the brand tag only takes POST-RELAUNCH (#1). `compile_blueprint` + `save_asset`.

## STEP C — BODY + LIMBS MI (neutralized, logo baked)

- `MI_<NAME>_Body` = clone `MI_IRONICS_Body_Red`; set `LogoTexture` = the new emblem
  (`set_material_instance_texture_parameter_value`); NEUTRALIZE color params
  (`set_material_instance_vector_parameter_value` → `TeamColor`/`EmissiveColor`/`EmissiveColor2`/
  `EmissiveColor3` = `(0.5,0.5,0.5)`). KEEP masks/normals/`Base_Tex`.
- `MI_<NAME>_Limbs` = clone `MI_IRONICS_Limbs_Red`; NEUTRALIZE `TeamColor`; KEEP `CarbonfiberTint`.
- Color now comes ONLY from the applied finish (Ruling 1).

## STEP D — DEFAULT FINISH

Pick the identity's signature finish from the existing presets
(`/Game/BagMan/Characters/Cosmetics/Finishes/DA_AFL_Finish_*`: Pink/Green/Purple/Blue/Red/
Black/Yellow/Crimson/Scarlet/BurntOrangeCyan/GlossBlack). If the signature color isn't covered,
author a NEW finish. **NAMING (ADR Decision 8):** new finishes use the patterned
`AFL.Finish.<Family>.<Variant>` (e.g. `AFL.Finish.Violet.Indigo`, `AFL.Finish.Cyan.Azure`) —
`<Family>` = color lane (grouping), `<Variant>` = the sellable SKU distinction. Duplicate a near
preset, set `axis=unreal.AFLCosmeticAxis.FINISH`, the patterned `cosmetic_id`, `TeamColor` +
`EmissiveColor` + `EdgeGlowColor`, and the commerce stamp (`ContentTier=Premium` for signatures,
`Rarity`, and `ColorIdentityTag`=family — the tag is post-relaunch, like brand tags). The 12
existing shipped finishes keep flat ids (`AFL.Finish.<Color>`) — NEVER renamed (Decision 3),
grouped via `ColorIdentityTag` only. DATA only — the enums shipped in `f3ef4100`, so a new finish
needs NO C++ build.

## STEP E — REGISTER (the 7-point checklist above)

PartMap both axes (#2), catalog both axes (#7), BrandToEdge row (#3, post-relaunch),
brand tag (#1, ini), robot tag (#4, post-relaunch). Catalog: build fresh `unreal.AFLCatalogEntry()`
structs and reassign the WHOLE `entries` array (in-place struct edits don't persist).

## STEP F — VERIFY

- **Every write → disk readback** (git status + dep-graph + reload-read; the bridge `return`
  table doesn't serialize — use `print`/`json.dumps`).
- **Dep-graph**: the robot hard-refs `MI_<NAME>_Body` + `MI_<NAME>_Limbs`; BrandToEdge resolves
  the signature finish; PartMap resolves both axes.
- **CANARY only — PIE**: spawn/possess the identity → renders its emblem + signature finish
  (color from the FINISH, baked MI neutral), replicates, dismemberment intact. Log proof:
  `LogAFLSkinDiag ... ApplySkinColor(DA_AFL_Finish_<X>)` + `slot[n] MID CREATED`.

---

## MCP API (banked from `f3ef4100`)

- New UENUM Python alias **strips the leading E** → `unreal.AFLCosmeticAxis.FINISH`,
  `unreal.AFLCosmeticType.CHARACTER/TEAM`, `unreal.AFLContentTier.PREMIUM`. Set enum props with
  the VALUE object (a string raises NativizeEnumEntry).
- MIC params: `MaterialEditingLibrary.set_material_instance_{vector,texture,scalar}_parameter_value`.
- `TArray<FStruct>` (catalog entries): edit by REBUILDING the whole array with fresh structs +
  reassign (element-wrapper edits revert on reload).
- TMap `TSoftClassPtr<AActor>` value = assign the generated `UClass` directly.
- FGameplayTag: `t=unreal.GameplayTag(); t.import_text('(TagName="X")')`; container via
  `import_text('(GameplayTags=(...))')` — DROPS unregistered tags silently (→ relaunch rule).
- SCS component template: `SubobjectDataSubsystem.k2_gather_subobject_data_for_blueprint` →
  `SubobjectDataBlueprintFunctionLibrary.get_object` → set `override_materials` → compile + save.
- Slot map: **0 = M_torso = Body, 1 = M_HeadLegs = Limbs.**
- **NO C++ build needed** for new identities (enums shipped); new finish presets are DATA.
- Editor-idle (not-in-PIE) before ANY MCP; verify via `Saved/Logs` after PIE stops.

---

## The 30-name roster (PREMIUM backlog)

Priority-5 (all Tier-1 monogram bakes): **ONYX PRIME** (OP) · **NOVA KAI** (NK) · **VANTA** (V) ·
**AZURA** (A) · **RIFT ONE** (R1). ONYX PRIME is the canary (first identity whose emblem we
CREATE — proves STEP A's bake link). Remaining 25 follow once the line is proven.
