# AFL Economy — Architecture Decision Record (ADR)

**Status:** LOCKED · **Date:** 2026-06-15 · **Scope:** the unified customizable-item economy
(CHARACTERS · WEAPONS · SKIN-COLOR COMBOS · FACE MASKS — all buy/earn/use/**trade**).

This ADR records four architectural decisions that gate the economy build. Each cites its
skill basis (verified present on disk under `Tools/skills/`, Pillar 5) so it is **defensible,
not assertion**. It is the companion to the FULL ECOSYSTEM AUDIT (the 4×6 matrix). The
build plan (Tier 1→4) lives in the tracker `S-IDENTITY-AXIS` / economy cards; this records
the *decisions* those tiers are built against.

> One-line frame: **the server decides what you own; the catalog id is the join key;
> equipment/behavior stays Lyra's; bundles grant many ids on the one ownership spine.**

---

## DECISION 1 — Ownership is SERVER-AUTHORITATIVE, backend-shaped. No client SaveGame as source of truth for tradeable items.

**Decision.** The server is the sole authority over what a player owns. The client *requests*
purchases/equips; the server *decides*. Persistence stays on the **server side** of the
`IAFLCosmeticPersistence` seam; the current `nullptr` stub is replaced by a **server-held
store** (PlayFab, Phase 3) — **never** a client store. A client-side cache is permitted for
**offline display only** (read-only, overwritten by the backend on login); it never decides
ownership.

**Skill basis.** `lyra-skin-builder-marketplace/references/entitlement-backend.md`:
- "The CLIENT requests purchases and equips. The SERVER decides what the player owns.
  Save data is a cache. The backend is the database." (Core Principle, §Server Is Source of Truth.)
- "Client save data is trivially editable (a JSON file on PC)… for anything with multiplayer
  or monetization, the server-authoritative model is non-negotiable."
- "Save Data — Cache Only, Never Source of Truth": on login, load local cache → sync from
  backend → "Backend response is authoritative — overwrite local cache… the backend wins.
  Never the other way."

**Our current state (already conformant).** `UAFLWalletComponent` *already* implements this:
it is the real `IAFLEntitlementSource` (replicated `OwnedCosmeticIds` set), all mutation is
through server-only `ServerPurchaseCosmetic` / `ServerEarn*` (authority-guarded, clamps,
funnels through `CommitMutation`), and the loadout gate resolves it
(`AFLCosmeticLoadoutComponent.cpp:256` → `PS->FindComponentByClass<UAFLWalletComponent>()`).
The client never writes the balance or owned-set. The `IAFLCosmeticPersistence` seam is
async-shaped and fully declared (`LoadBalance/SaveBalance/LoadOwnedSet/SaveOwnedSet`); only
the impl is stubbed (`GetPersistence()` returns `nullptr` → session-only today).

**Consequence.** Trade + durable cross-session ownership **wait for server-side persistence**.
They CANNOT ship on a client cache. (Entitlement is already real; the gate is *durability*,
not *authority*.)

**ACTION ITEM 1 (verify, do not change) — atomic check-and-deduct.** The skill warns
(entitlement-backend.md §Wallet): "Splitting 'check balance → deduct' into two operations is
the textbook race condition that lets a player double-spend with two simultaneous purchases."
**VERIFIED:** `ServerPurchaseCosmetic_Implementation` (`AFLWalletComponent.cpp:200-258`)
performs the balance check (`Volts < CostVolts` / `Watts < CostWatts`) and the deduct
(`CommitMutation(-CostVolts, -CostWatts, …)`, `:258`) inside **one synchronous server-side
function with no `await` / no RPC split** — atomic within the server frame (single-threaded
game thread, no yield between check and deduct). **No fix needed now.** NOTE for Phase 3: when
PlayFab becomes the backing, the deduct must become a **single atomic DB operation**
(`UPDATE … SET balance = balance - $amount WHERE … AND balance >= $amount`, succeed iff
rowcount==1) per the skill — the in-memory stub's frame-atomicity does not carry to a
networked DB. Recorded as a Phase-3 acceptance criterion.

---

## DECISION 2 — Weapon-as-economy-item vs Lyra-equipment: clean one-directional separation. The SKU grants the equipment, soft-ref'd.

**Decision.** A weapon has **two identities that never overlap**:
- **ECONOMY identity** = a catalog SKU (`EAFLCosmeticType::Weapon`, id `AFL.Weapon.<Name>`)
  carrying a **`TSoftClassPtr`** to the Lyra equipment item-def. The owned-set is the source
  of truth for **OWNERSHIP** (own/buy/earn/trade).
- **GAMEPLAY identity** = the existing Lyra item-def chain
  (`ULyraInventoryItemDefinition` → `InventoryFragment_EquippableItem` →
  `ULyraEquipmentDefinition`). It is the source of truth for **BEHAVIOR** and stays
  **untouched**.

Owning the SKU **GRANTS** the equipment — economy → gameplay, **one direction only**. No
gameplay-weapon rewrite; Pulse becomes `AFL.Weapon.Pulse` soft-pointing at the existing
`WID_BagMan_PulseCarbine` chain.

**Skill basis.**
- `lyra-skin-builder-marketplace` lists `ULyraEquipmentDefinition` / `ULyraInventoryItemDefinition`
  as the canonical homes for "weapons-as-cosmetics."
- `data-architecture.md` §Soft vs Hard References: every catalog reference flows through
  `TSoftObjectPtr` / `TSoftClassPtr` — "hard refs in DataTable rows or skin definitions defeat
  the async loading model and blow up memory" (`✓ TSoftClassPtr<AActor>`).
- `unreal-engine-expert` (general AAA UE5): Data Assets + Primary Asset IDs + soft refs as the
  catalog/streaming idiom.

**Why this is the proven shape.** It is the SAME pattern skins already use (catalog id →
soft-ref'd asset; `FAFLCatalogEntry.Asset` is `TSoftObjectPtr<UPrimaryDataAsset>`). The
weapon SKU just soft-points at an equipment class instead of a skin asset. Ownership and
behavior never diverge because each has exactly one owner of truth.

**Consequence.** Catalog must gain a full-weapon type (`EAFLCosmeticType::Weapon`) — today it
has only `WeaponAccessory` (attachment) and `Beam` (VFX variant), **not** a weapon-as-ownable-
item type. Built at Tier 2; recorded here so the catalog is designed for it.

---

## DECISION 3 — Ownership keys on the FULL TYPE-QUALIFIED id, never a bare name.

**Decision.** Every ownership / entitlement / selection key is the fully-qualified
`AFL.<Type>.<Name>` id. Never key on a stripped/bare name. The `AFL.<Type>.<Name>` convention
IS the type-qualifier, so `AFL.Character.BigSixx` and `AFL.Team.BigSixx` are **different ids**
that cannot collide — even though `UAFLWalletComponent::OwnsIdentity` currently ignores its
separate `EAFLIdentityType` param (it keys on the id, which is already type-qualified, so the
ignored enum is harmless given this discipline).

**Skill basis.** `data-architecture.md`: keys are stable `FName` ids — "use stable `FName`
SkinId`s, not asset references — survives asset path changes" (§Save Data Schema); "Stable
identifier used as the asset key — never localized" (`F<Project>SkinDefinition.SkinId`). The
id is the join key everywhere; the type-qualifier prefix is part of that stable key.

**ACTION ITEM 2 (verify) — all ids fully-qualified, nothing keys on a bare name.**
**VERIFIED CLEAN:** `FAFLCosmeticSelection::GetActiveIdentityId()`
(`AFLCosmeticSelectionTypes.h:82`) returns the stored `CharacterId`/`TeamId` **directly** —
both documented as the fully-qualified catalog keys (`AFL.Character.<Name>` / `AFL.Team.<BRAND>`,
lines 51/55) — with **no stripping, no `RightChop`, no prefix removal**. The wallet owned-set
stores these same ids; `IsEntitled`/`OwnsIdentity` do `OwnedCosmeticIds.Contains(Id)` on the
qualified id. No bare-name keying found anywhere in the cosmetic selection/entitlement path.
**No fix needed.** Discipline to maintain: future axes (Weapon/Bundle) must likewise store and
key on `AFL.<Type>.<Name>`, never a stripped name.

---

## DECISION 4 — Catalog models BOTH single items AND bundles.

**Decision.** A **bundle** is a SKU whose ownership **grants a SET of child SKU ids**. Single
items and bundles ride the **SAME ownership spine** — a bundle is a *grant-many wrapper*, not a
parallel system. Implementation shape (built later): either a new
`EAFLCosmeticType::Bundle` + a `TArray<FName> ChildCosmeticIds` field on `FAFLCatalogEntry`,
or an equivalent child-id-list. Buying the bundle → the entitlement grant loop adds **each
child id** into the owned-set (so the gate/selection resolve them individually, exactly as if
bought separately). Example: `AFL.Bundle.BigSixxComplete` grants
`{AFL.Character.BigSixx, AFL.Weapon.<his>, AFL.Facemask.<his>}`.

**Skill basis.** `data-architecture.md` defines the catalog as one-id→one-asset
(`FAFLCatalogEntry.Asset` is a single `TSoftObjectPtr`), with **no bundle/multi-asset shape** —
the audit flagged this gap. Operator confirmed BOTH single items and bundles are required.
This decision extends the proven one-id→one-asset model with a grant-many wrapper rather than
inventing a second catalog.

**Consequence / scope.** This is a **design decision recorded now**; the bundle entry shape is
**BUILT when catalog work lands (Tier 2/3)**, not this pass. Recorded so the catalog is
designed with bundles in mind from the start (the child-id-list rides the same owned-set, the
same `ServerPurchaseCosmetic` funnel — a bundle purchase deducts once and grants N ids).

---

## Cross-cutting consequences (the dependency spine these four imply)

1. **Durable server persistence is the silent prerequisite under everything tradeable.**
   (Decision 1.) Today ownership is session-only. Buy/earn/trade are not durable until the
   server-side persistence impl is real (local SaveGame stub at minimum; PlayFab Phase 3).
2. **Catalog must represent all 4 types + bundles before SKUs can be stocked.**
   (Decisions 2 + 4.) Characters/skins/masks are representable now; weapons need
   `EAFLCosmeticType::Weapon`; bundles need the child-id shape.
3. **Trade depends on REAL ownership (✅ done) AND durable ownership (🔴 stubbed).**
   So **trade follows persistence**, not entitlement. Trade is its own ownership-transfer
   subsystem (server-auth atomic transfer + escrow/confirm + anti-dupe + per-SKU tradeable
   flag) — scoped as a named workstream, not a checkbox.
4. **Discipline carried into every future axis:** fully-qualified ids (Decision 3),
   soft-refs only (Decision 2), server-auth mutation through the one `CommitMutation` funnel
   (Decision 1).

---

## DECISION 5 — COMPOSABLE ADDRESS SCHEME (the identifier system). LOCKED 2026-06-15.

**Decision.** Every customizable entity is addressed by **exactly ONE namespace** on the
`AFL.<Type>.<Name>` convention; the runtime **composes** axes. **Color is ALWAYS a finish,
NEVER part of an identity's name.**

**The axes:**
- `AFL.Character.<Name>` + `AFL.Team.<Name>` — **IDENTITY axis, OWNS THE LOGO** (the *who*).
  The emblem is baked on the identity's body MI (`LogoTexture` param); survives recolor.
- `AFL.Finish.<Color>` — **FINISH axis**: color-only, **logo-less**, the **SOLE color source**
  (Ruling 1), the **free base tier**. Blue/Green/Purple/Pink/Red/Black/Yellow, electric-neon-glass.
- `AFL.Facemask.<Name>` — mask axis (material reskin).
- `AFL.Weapon.<Name>` — weapon axis (soft-ref'd equipment, Decision 2).
- `AFL.Accessory.<Name>` — **NEW dedicated axis** (Ruling 3): per-identity attachments,
  composed onto the identity via the proven `AddCharacterPart` mechanism. **New axis, no new
  mechanism** (the Phase-0 `UAFLCharacterPartSelectorComponent` part-add path generalizes).
- `AFL.Bundle.<Name>` — grants a SET of the above (Decision 4).

**Additive enum changes (no shipped id renamed — Decision 3 "never change a shipped id"):**
- `EAFLCosmeticAxis` gains **`Finish`** (alongside `Edge`/`Body`) — lets a `UAFLSkinColorAsset`
  self-describe as a full finish vs an edge-only preset.
- `EAFLCosmeticType` gains **`Finish`, `Weapon`, `Accessory`, `Bundle`** (the existing
  `WeaponAccessory` stays — `Weapon` is the full-weapon SKU, `Accessory` is the new
  per-identity-attachment axis, distinct from weapon-attachments).
- **Keep `AFL.Edge.*` shipped presets** — they remain a valid edge-only sub-cosmetic;
  `AFL.Finish.*` is the full base finish that *includes* an edge value (for base colors a
  Finish supersedes the edge-only preset).

**CONFLICT-PREVENTION RULE (self-enforcing).** *An identity id NEVER encodes color.* There is
no `AFL.Character.DracoRed` — a red Draco is `AFL.Character.Draco` (identity, owns the emblem)
**composed at runtime** with `AFL.Finish.Red` (finish, owns the color). Enforced by:
- **(a)** namespace / `EAFLCosmeticType` separation — a compound `<Name>` is malformed;
- **(b)** the data itself — `UAFLSkinColorAsset` (finish) has **no logo/mesh/identity field**,
  and the identity MI's `LogoTexture` is **untouched by recolor** (verified: `ApplySkinColor`
  drives only Scalars/Colors/Textures from the color asset) → color *physically cannot* live in
  identity and identity *cannot* carry a finish-name without it being dead data;
- **(c)** an `asset_naming.py` lint extension that **rejects color-family tokens in
  `Character`/`Team` ids**.
Distinctness is **by EMBLEM**; color is a free finish anyone applies. This is the property that
keeps ~10 red-family identities (DRACO/TALON/RONIN/AKUMA/MOB-FIGAZ/AP-9) and the ARIA-lane
(ARIA/ASTRA/AURELIA) **distinct** while all can wear any `AFL.Finish.*`.

**Skill basis + verified crux.** `UAFLSkinColorAsset` is a logo-less color/finish param bag
(its own header: *"Mark x color stay independent: MARK = which part actor; COLOR = which
UAFLSkinColorAsset is applied"*); the logo is baked on the identity MI and survives recolor
(`AFLCharacterPartActor.cpp:145-156` drives only color-asset params, not `LogoTexture`); the
skin recolor skips non-`AAFLCharacterPartActor` parts (*"Skin never bleeds onto them"*,
`AFLSkinColorComponent.cpp:127`). **The engine already behaves this way** — this decision
formalizes it. Stable-FName-id keying per `data-architecture.md`.

### Ruling 1 (locked) — COLOR-NEUTRALIZE TO FINISH.
The baked `TeamColor`/`EmissiveColor*`/`EdgeGlowColor` on every identity body+limbs MI is
**NEUTRALIZED** so the applied `AFL.Finish.*` preset is the SOLE color source. No
signature-default-fighting-a-selection. (The logo/`LogoTexture` stays baked — only color is
neutralized.) See the DEFAULT-FINISH BINDING section below for how a neutralized identity still
renders correctly on spawn.

### Ruling 3 (locked) — ACCESSORY is a NEW dedicated axis (recorded above).

### 30-NAME ROADMAP BUILD APPROACH (locked).
Each new identity = **emblem (logo texture) + a logo-baked, COLOR-NEUTRALIZED body+limbs MI +
a default finish** — **NOT** per-color MI variants. Composing finishes at runtime makes the
roster **37 assets (30 identity MIs + 7 finish presets), not 210** (7 colors × 30 names) — a
~6× authoring reduction, and shared color families are **solved by construction** (identities
differ by emblem, share finishes). The 30-name set is the **PREMIUM identity-axis content
backlog** (`ContentTier=Premium`); priority order: **ONYX PRIME, NOVA KAI, VANTA, AZURA,
RIFT ONE**.

---

## DECISION 6 — MANAGEMENT-METADATA LAYER (descriptive, distinct from the address). LOCKED 2026-06-15.

**Decision.** Management metadata lives on `FAFLCatalogEntry`, **NEVER in `CosmeticId`**. It is
for filtering / merchandising; it is not the address. Mapping onto disk (most fields already
exist):

| Metadata | Catalog field | Status |
|---|---|---|
| **Rarity** (shop frame + base pricing) | `EAFLCosmeticRarity` (Common→Legendary) + `FGameplayTag RarityTag` | ✅ exists |
| **ContentTier** {Base/Premium/Event/Seasonal} | **NEW `EAFLContentTier` field** (Ruling 2) | 🔴 ADD |
| **Acquisition** | `EAFLAcquisition` (GrantedFree=base-grant path, Direct=premium, BattlePass) | ✅ exists |
| **Collection / Family** (e.g. the ARIA lane) | `FName CollectionId` | ✅ exists |
| **Color-family** (DESCRIPTIVE filter "show all red-family") | `FGameplayTag ColorIdentityTag` | ✅ exists — header: *"SEPARATE axis from RarityTag… replaces the retired ColorTheme FName"* |

### Ruling 2 (locked) — CONTENT-TIER IS EXPLICIT.
Add a small **`EAFLContentTier { Base, Premium, Event, Seasonal }`** enum as a field on
`FAFLCatalogEntry`. Content-tier is a **fact stamped on the entry**, NOT inferred from
`Acquisition`+`CollectionId`. `Acquisition`/`CollectionId`/`Rarity`/`ColorIdentityTag` stay
as-is — `ContentTier` is **additive**.

**Color-family is metadata, NOT the address.** `ColorIdentityTag` (filter: "show all red-family
identities") is descriptive; applying `AFL.Finish.<Color>` (the selectable finish) is the
address. They never collide — you filter by family and independently apply a finish.

**Authoring stamp per identity:** `ContentTier` + `CollectionId` (lane/family) +
`ColorIdentityTag` (descriptive color-family) + `Rarity` + `Acquisition`.

**Basis.** The catalog was already designed with these fields (`ColorIdentityTag` documented as
separate-from-address); `entitlement-backend.md` (server-auth ownership the metadata describes).

---

## DEFAULT-FINISH BINDING (the open thread, resolved read-only 2026-06-15)

**The problem.** Ruling 1 neutralizes baked color → every identity is **colorless until a finish
applies** → each identity needs a **default finish** so it renders correctly the instant it
spawns (before any player selection).

**Resolution: the per-identity default-finish binding ALREADY EXISTS — reuse `UAFLBrandEdgeMap`.**
- `UAFLBrandEdgeMap.BrandToEdge` is a `TMap<FGameplayTag, UAFLSkinColorAsset*>` mapping
  `Cosmetic.Brand.<NAME>` → the brand's **factory-default color preset** (its header:
  *"each robot carries a Cosmetic.Brand.* tag… resolves to a COLOR preset that is the brand's
  factory default"*). The controller's `RefreshSkinForPawn` already calls
  `BrandEdgeMap->ResolveEdge(BrandTag)` as the **`#38a`** step of the resolve chain.
- **The resolve chain (verified, `AFLSkinColorControllerComponent.cpp:119-124`):**
  **#43 player selection > #38a brand-default (`BrandToEdge`) > persistent default.** A
  freshly-spawned identity with no selection falls to the **#38a brand-default** → renders in
  its default finish; a finish **selection (#43) overrides it cleanly** (higher priority, same
  proven MID push). **This is exactly the spawn-default + selectable-override behavior required.**
- **Binding point chosen:** **`UAFLBrandEdgeMap` (the existing `#38a` map)** — least new
  machinery (the resolve chain, the map, and the apply are all proven). Under the new model its
  values become the default **`AFL.Finish.*`** preset per brand (today they point at
  `DA_AFL_Edge_*`; they will point at `AFL.Finish.*`). *Rejected alternatives:* a new
  `DefaultFinishId` field on the part (duplicates what `BrandToEdge` already does);
  `CharacterPartMap` carrying it (that map is body-class resolution, a different concern —
  keep it single-responsibility).

**THE GAP (report for the build phase).** The current `BrandToEdge` values are **edge-glow
presets that do NOT match each identity's intended signature color**, and **BigSixx is missing
from the map**:
- `Cosmetic.Brand.ARIA → DA_AFL_Edge_NeonPurple` — but ARIA's *body* bakes **pink**
  (`TeamColor=(0.9,0.1,0.45)`). Mismatch.
- `Cosmetic.Brand.IRONICS / AP-9 / MOB-FIGAZ → DA_AFL_Edge_NeonGreen` (three brands share one
  default edge; not their signature body colors).
- `Cosmetic.Brand.SCARLETT → NeonPink`, `MAKHIAVELLI → NeonBlue`.
- **No `Cosmetic.Brand.BigSixx` row** (the `None`-key fallback → `BurntOrangeCyan` is a global
  fallback, not a BigSixx default).
This mismatch is the *exact* "baked color vs finish muddiness" Ruling 1 removes. **Build-phase
work:** author the 7 `AFL.Finish.*` presets, then **repoint `BrandToEdge` so each brand's
default = its true signature finish** (ARIA→Pink, IRONICS→its signature, BigSixx→BurntOrangeCyan
[add the row], etc.), and neutralize each identity MI's baked color so the default finish is the
sole color source. Data-only (+ the `EAFLCosmeticAxis::Finish` enum value).

**ARIA canary impact (verified).** The ARIA limbs MI that landed (`MI_ARIA_Limbs`) is a
**structural** completeness fix (slot-1 had the facemask material; now a proper limbs MI) and
**STILL STANDS** — orthogonal to color. Under Ruling 1 it (and `MI_ARIA_Body_Pink`) will have
its **baked color neutralized** in the build phase, and **ARIA's default finish = Pink** (repoint
`Cosmetic.Brand.ARIA` in `BrandToEdge` from `NeonPurple` → the Pink `AFL.Finish.*` preset, to
match ARIA's intended signature). So: the canary's *structure* is correct and kept; only its
*baked color* gets neutralized + a Pink default finish bound — no rework of the limbs-MI wiring.

---

## What's DATA vs CODE (Decisions 5-6 + rulings)

- **CODE (small, additive):** `EAFLCosmeticAxis::Finish`; `EAFLCosmeticType::{Finish,Weapon,Accessory,Bundle}`;
  `EAFLContentTier` enum + the `ContentTier` field on `FAFLCatalogEntry`; the `asset_naming.py`
  lint extension (no-color-token-in-identity-id). No new runtime mechanism — composition, the
  resolve chain, `AddCharacterPart`, and the MID apply are all proven.
- **DATA (the build):** 7 `AFL.Finish.*` presets (electric-neon-glass); neutralize baked color on
  identity MIs; repoint `BrandToEdge` to true signature finishes (+ BigSixx row); catalog rows
  with `ContentTier`/`CollectionId`/`ColorIdentityTag`/`Rarity` stamped; the 30 premium identities
  (emblem + neutralized MI + default finish), priority ONYX PRIME / NOVA KAI / VANTA / AZURA / RIFT ONE.

---

## DECISION 7 — COMPLETE-REGISTRATION RULE (uniformity doctrine). LOCKED 2026-06-15.

**Decision.** Every identity (Character/Team) must have a **COMPLETE and EXPLICIT** set of
registrations. **No identity may depend on a FALLBACK for its correct behavior.** A fallback is a
safety net for **UNREGISTERED / unknown ids only** — never the intended path for a registered
identity. An identity that renders correctly *only because the global fallback happens to match
its color* is a **BUG** (correct-by-accident, not correct-by-registration) and must be flagged
and fixed. *"Ignoring these details always causes us later issues."*

**Applies to EVERY per-identity map/registration** (current and future): brand-tag declaration,
`CharacterPartMap` row (identity→robot BP), `BrandToEdge` default-finish row, robot BP + correct
class, body+limbs MI (both slots), logo texture, catalog row — and every future per-identity map.

**The per-identity registration checklist (every identity must satisfy ALL, or be flagged incomplete):**
1. **Brand tag** — `Cosmetic.Brand.<NAME>` declared in `DefaultGameplayTags.ini`.
2. **CharacterPartMap row** — `AFL.<Type>.<Name>` → `B_AFL_Robot_<NAME>_C` (resolves).
3. **BrandToEdge / default-finish row** — `Cosmetic.Brand.<NAME>` → its **SIGNATURE** finish
   (not missing, not mismatched, not shared with another identity).
4. **Robot BP** — `B_AFL_Robot_<NAME>` exists, parented to `AAFLCharacterPartActor` (branded).
5. **Body MI (slot 0 / M_torso) + Limbs MI (slot 1 / M_HeadLegs)** — both present, both
   identity-owned (a limbs MI borrowed from another identity is flagged SHARED until owned).
6. **Logo texture** — the identity's emblem (`LogoTexture` param on the body MI).
7. **Catalog row** — `FAFLCatalogEntry` with `CosmeticId = AFL.<Type>.<Name>`, correct `Type`.

The unregistered-safety-net fallback (`None` → `BurntOrangeCyan` in `BrandToEdge`) **stays ONLY as
the unknown-id net, documented as such — never an identity's intended path.**

**Basis.** The verified composition model (Decisions 5-6): correctness must come from explicit
registration, not from a fallback coinciding with intent. `data-architecture.md` stable-FName-id
keying (every id explicitly registered, not inferred).

---

## THE COMPLETE-REGISTRATION AUDIT (read-only, definitive, 2026-06-15)

Every cell read from the live assets via the editor bridge (no PIE, no writes): `DefaultGameplayTags.ini`,
`DA_AFL_CharacterPartMap`, `DA_AFL_BrandEdgeMap`, the robot BPs, the body/limbs MIs, `DA_AFL_CosmeticCatalog`.

### Identity × Registration matrix

Legend: ✅ present+correct · ❌ missing · ⚠ mismatch/collision · 🔶 shared-with-another.

| Identity | 1 Brand tag | 2 PartMap row | 3 BrandToEdge (signature?) | 4 Robot BP (class) | 5 Body MI / Limbs MI | 6 Logo | 7 Catalog row |
|---|---|---|---|---|---|---|---|
| **ARIA** | ✅ | ✅ `AFL.Team.ARIA` | ⚠ →NeonPurple, but body is **Pink** (0.9,0.1,0.45) — **MISMATCH** | ✅ AFLCharacterPartActor | ✅ Body_Pink / **Limbs (own, canary)** | ✅ T_ARIA_Logo_BC | ✅ `AFL.Team.ARIA` (TEAM) |
| **IRONICS** | ✅ | ✅ `AFL.Team.IRONICS` | ⚠ →NeonGreen, but body is **deep-red** (0.75,0.06,0.06) — **MISMATCH + COLLISION** | ✅ AFLCharacterPartActor | ✅ Body_Red / Limbs_Red | ✅ T_IRONICS_Logo_BC | ✅ `AFL.Team.IRONICS` (TEAM) |
| **SCARLETT** | ✅ | ✅ `AFL.Team.SCARLETT` | ✅ →NeonPink ≈ signature purple? **⚠ body is purple** (0.4,0.1,0.7), pink default — **MISMATCH** | ✅ AFLCharacterPartActor | ✅ Body_Purple / 🔶 Limbs=**IRONICS_Limbs_Purple** (borrowed) | ✅ T_SCARLETT_Logo_BC | ✅ `AFL.Team.SCARLETT` (TEAM) |
| **MAKHIAVELLI** | ✅ | ✅ `AFL.Team.MAKHIAVELLI` | ⚠ →NeonBlue, but body is **green** (0.06,0.55,0.12) — **MISMATCH** | ✅ AFLCharacterPartActor | ✅ Body_Green / 🔶 Limbs=**IRONICS_Limbs_Green** | ✅ T_MAKHIAVELLI_Logo_BC | ✅ `AFL.Team.MAKHIAVELLI` (TEAM) |
| **MOB-FIGAZ** | ✅ | ✅ `AFL.Team.MOB-FIGAZ` | ⚠ →NeonGreen, body is **red** (0.75,0.06,0.06) — **MISMATCH + COLLISION** | ✅ AFLCharacterPartActor | ✅ Body_Red / 🔶 Limbs=**IRONICS_Limbs_Red** | ✅ T_MOB-FIGAZ_Logo_BC | ✅ `AFL.Team.MOB-FIGAZ` (TEAM) |
| **AP-9** | ✅ | ✅ `AFL.Team.AP-9` | ⚠ →NeonGreen, body is **red** (0.75,0.06,0.06) — **MISMATCH + COLLISION** | ✅ AFLCharacterPartActor | ✅ Body_Red / 🔶 Limbs=**IRONICS_Limbs_Red** | ✅ T_AP-9_Logo_BC | ✅ `AFL.Team.AP-9` (TEAM) |
| **BigSixx** | ✅ | ✅ `AFL.Character.BigSixx` | ❌ **NO ROW** — rides `None`→BurntOrangeCyan fallback (**correct-by-accident**) | ✅ AFLCharacterPartActor | ✅ Body / Limbs (own) | ✅ T_BigSixx_Logo_BC | ✅ `AFL.Character.BigSixx` (CHARACTER) |
| **FANATICS** | ❌ | ❌ | ❌ | ❌ (no BP) | ❌ / ❌ | ✅ T_FANATICS_Logo_BC only | ❌ |
| *generic Blue/Green/Purple/Pink* | ❌ (none) | ❌ (none) | n/a | ✅ **LyraTaggedActor** (NOT branded) | reuse IRONICS MIs | n/a | ❌ | *= color-test shells, NOT identities; correctly logo-less/tag-less* |

### `BrandToEdge` current map — verbatim (the default-finish registration), every anomaly flagged
```
Cosmetic.Brand.ARIA        -> DA_AFL_Edge_NeonPurple    ⚠ MISMATCH (ARIA signature = Pink)
Cosmetic.Brand.SCARLETT    -> DA_AFL_Edge_NeonPink      ⚠ MISMATCH (SCARLETT signature = Purple)
Cosmetic.Brand.MAKHIAVELLI -> DA_AFL_Edge_NeonBlue      ⚠ MISMATCH (MAKHIAVELLI signature = Green)
Cosmetic.Brand.IRONICS     -> DA_AFL_Edge_NeonGreen     ⚠ MISMATCH+COLLISION (IRONICS signature = deep Red)
Cosmetic.Brand.AP-9        -> DA_AFL_Edge_NeonGreen     ⚠ COLLISION (3 brands share NeonGreen)
Cosmetic.Brand.MOB-FIGAZ   -> DA_AFL_Edge_NeonGreen     ⚠ COLLISION
None (unregistered net)    -> DA_AFL_Edge_BurntOrangeCyan   ← the SAFETY NET (keep as net only)
                              [BigSixx has NO row -> rides this net = correct-by-ACCIDENT, FLAG]
```
**Anomaly summary:** the `BrandToEdge` map is currently **edge-glow defaults essentially unrelated
to each identity's signature body color** — 5 of 6 rows MISMATCH the body, 3 COLLIDE on NeonGreen,
and BigSixx is absent (rides the net). This is the exact correct-by-accident pattern Decision 7
forbids. (Note: body-color collisions also exist at the MI layer — IRONICS/MOB-FIGAZ/AP-9 share
deep-red `(0.75,0.06,0.06)` — distinct only by emblem today, which is *acceptable* under the
emblem-distinctness rule, but their *finish defaults* must still be explicit + distinct.)

### Other registration findings (beyond BrandToEdge)
- **AXIS MISMATCH (flag):** the 6 branded identities are registered as **`AFL.Team.*`** in both
  `CharacterPartMap` and the catalog (TEAM type), while only BigSixx is `AFL.Character.*`. Per the
  composable model these 6 are *teams* (the proven roster) — consistent — but if any are intended
  as *characters* too, they need parallel `AFL.Character.*` registration. **Decide per identity.**
- **LIMBS SHARED (🔶, flag):** SCARLETT/MAKHIAVELLI/MOB-FIGAZ/AP-9 borrow `MI_IRONICS_Limbs_*`
  for slot 1 (only their *body* MI is identity-owned). Under Complete-Registration each should own
  `MI_<NAME>_Limbs` (ARIA + BigSixx already do; ARIA via the canary).
- **TYPE MISCLASS (minor flag):** `AFL.Facemask.IroVisor` is typed `SKIN_COLOR_BODY` in the
  catalog, not a facemask/`Helmet`-style type. Cosmetic-only, but worth correcting for filtering.
- **FANATICS:** texture-only (logo exists; everything else missing) — full build needed.

### Uniform-correction worklist (state only; build on GO)
Once the 7 `AFL.Finish.*` presets exist (Decisions 5-6):
1. **Every identity gets an EXPLICIT `BrandToEdge` row → its SIGNATURE finish:** ARIA→Pink,
   IRONICS→Red, SCARLETT→Purple, MAKHIAVELLI→Green, MOB-FIGAZ→(its own red-family finish),
   AP-9→(its own red-family finish — break the 3-way NeonGreen collision), BigSixx→BurntOrangeCyan
   (**ADD the row**). No identity left on the fallback.
2. **Own the limbs:** author `MI_SCARLETT_Limbs / MI_MAKHIAVELLI_Limbs / MI_MOB-FIGAZ_Limbs /
   MI_AP-9_Limbs` (stop borrowing IRONICS limbs).
3. **Neutralize baked color** on every identity body+limbs MI (Ruling 1) so the finish is the sole
   color source; logo stays.
4. **FANATICS full build:** brand tag + PartMap row + BrandToEdge row + robot BP
   (AFLCharacterPartActor) + body+limbs MI + catalog row (logo already exists). Color = operator pick.
5. **Fix the IroVisor catalog type;** decide the Team-vs-Character axis for the 6 branded.
6. **Document `None`→BurntOrangeCyan** as the unregistered-safety-net only.

### Roster checklist (the 30-name build gate)
Each new identity (ONYX PRIME / NOVA KAI / VANTA / AZURA / RIFT ONE first) must satisfy **ALL 7
registrations** (brand tag · PartMap row · explicit BrandToEdge signature-finish row · robot BP
[AFLCharacterPartActor] · own body+limbs MI [color-neutralized] · logo · catalog row [+ ContentTier
/CollectionId/ColorIdentityTag/Rarity stamped]) **or be flagged INCOMPLETE.** Nothing ships riding
a fallback. The `asset_naming.py` lint enforces no-color-token-in-identity-id (Decision 5).

---

## DECISION 8 — FINISH-LIBRARY NAMING (patterned, manageable, commerce-ready). LOCKED 2026-06-15.

**Decision.** As the finish library scales (30+ identities, each finish a sellable SKU), finishes
follow a **two-part patterned id** with the family-grouping in metadata.

- **NEW finishes:** `AFL.Finish.<Family>.<Variant>`.
  - `<Family>` = the color lane (management: "show all Violets") — `Blue / Cyan / Violet / Teal /
    Red / Green / Pink / Yellow / Orange / Black`.
  - `<Variant>` = the sellable distinction within the family (commerce: each a SKU) — e.g.
    `Indigo / Azure / Stealth / Rift`.
  - Cool-lane examples (authored): `AFL.Finish.Violet.Indigo`, `AFL.Finish.Cyan.Azure`,
    `AFL.Finish.Violet.Stealth`, `AFL.Finish.Teal.Rift`.
- **EXISTING 12 shipped finishes** (`AFL.Finish.Blue`, `…OnyxChrome`, `…GlossBlack`, `…Crimson`,
  `…Scarlet`, `…BurntOrangeCyan`, the 7 base, etc.) are SHIPPED/committed → **NEVER renamed**
  (Decision 3). They keep their flat ids; their family grouping lives in **`ColorIdentityTag`**
  (Decision 6 metadata), NOT in the id. New finishes carry both the patterned id AND the tag.
- **Commerce stamp per finish** (all Decision 6 fields, no new mechanism):
  `ContentTier` (Base for the 7 base electric-neon colors; Premium for signatures —
  OnyxChrome, the cool-lane four, the distinct reds) · `ColorIdentityTag` (the family /
  grouping key) · `Rarity` (Common base → Rare/Epic signature) · optional `CollectionId` (set).

**Why this is sound:** zero shipped-id renames (Decision 3), uses the existing metadata layer
(Decision 6) for grouping (no new field/code), and gives new finishes a clean `<Family>.<Variant>`
SKU pattern. Migration path for existing = stamp metadata, never rename.

**TRACKED FOLLOW-UP (the management/commerce HALF — do NOT let it drop):** stamping
`ColorIdentityTag` on ALL finishes (12 existing + cool-lane + future) requires `Cosmetic.ColorFamily.*`
gameplay tags declared in the ini → a tag-keyed (post-relaunch) pass. It is the queryable grouping
that makes the library manageable at 30+. FOLDED INTO the cool-lane batch's Phase-2 relaunch (same
relaunch as the brand tags + BrandToEdge rows) so the grouping goes LIVE in this batch, not "someday."

---

## DECISION 9 — EMBLEMS HONOR HERITAGE (Latin / symbol / NATIVE-SCRIPT). LOCKED 2026-06-15.

**Decision.** An identity's emblem (the `LogoTexture`) is NOT English-letter-only. Emblems may be:
Latin monograms, abstract SYMBOLS, or **native-script glyphs** (Japanese kanji, Korean hangul,
Vietnamese, Spanish/Latin-script accents, Arabic, etc.) when a name implies HERITAGE — the goal is
**authentic global representation** ("we truly want to represent all"). Decision rule: a name with
cultural/heritage origin → consider a native-script emblem faithful to that origin (e.g.
KAGE → 影 "shadow"; RAIJIN/AKUMA/RONIN → Japanese; regional names → their script) over a Latin
fallback.

**Tier note (extends the production-line emblem path):** the BigNiagaraBundle supplies **Latin
letters / numbers / symbols ONLY** — a native-script glyph CANNOT be bundle-baked. It routes through
**Tier-2 (Tripo `text_to_image`)** or Tier-3 (image-gen). **VERIFIED 2026-06-15:** Tripo produces a
clean, legible single kanji (影) white-on-black with heavy strokes that survive the spec
(1024² / TC_GRAYSCALE / sRGB=false / TMGS_BLUR5). Caveat: a DENSE glyph needs a tuned `Scale`/`LogoPos`
so the mip-confinement keeps it readable at chest size — verify in PIE per identity. Heavy-stroke
prompting is required (fine strokes mush). KAGE ships the 影 kanji as the first heritage emblem.

---

## DECISION 10 — FINISH STYLE: flat OR gradient-feel; TRUE UV gradient is deferred material work. LOCKED 2026-06-15.

**Decision.** Finishes may be **flat** OR use a **neon gradient-feel** — "creative but elegant," used
where it elevates a premium signature, NOT universally (flat has its place).

**What the material supports TODAY (verified — `M_Mannequin` param surface):** the body material
exposes `TeamColor` (body) + `EmissiveColor1/2/3` (a 3-tier MIP-BLENDED emissive ramp) + `EdgeGlowColor`
+ `CarbonfiberTint`. There is **NO second body-color param and NO gradient/lerp node.** Therefore:
- **Gradient-FEEL is buildable NOW as DATA** via the emissive ramp: set `EmissiveColor1/2/3` + `EdgeGlow`
  to a graded sequence (e.g. ORION navy body → brighter-blue mid → star-white edge; SOLARA deep-orange
  → gold → white-hot edge). Reads as an elegant glow-gradient without a true UV gradient. No build.
- **A TRUE UV body gradient** (color-A→color-B across the mesh) **requires a MATERIAL/SHADER change**
  (a gradient node + new params on `M_Mannequin` or a new master) = a **C++/material build**.
  RECORDED-BUT-DEFERRED as a future material-upgrade task; NOT done as data. (Flag any future request
  for it as a build, not a content pass.)

**Applied:** ORION (`AFL.Finish.Blue.Cosmic`) + SOLARA (`AFL.Finish.Orange.Solar`) use the
emissive-ramp gradient-feel; all other finishes stay flat. True-gradient deferred.

---

*Verifications in this ADR were performed read-only against the working tree on 2026-06-15
(`AFLWalletComponent.cpp`, `AFLCosmeticServices.h`, `AFLCosmeticSelectionTypes.h`,
`AFLCosmeticCoreTypes.h`, `AFLCosmeticLoadoutComponent.cpp`, `AFLSkinColorAsset.h`,
`AFLCharacterPartActor.cpp`, `AFLSkinColorComponent.cpp`, `AFLSkinColorControllerComponent.cpp`,
`AFLBrandEdgeMap.h`, `AFLCosmeticTypes.h`), plus live reads of `DA_AFL_BrandEdgeMap`,
`DA_AFL_CharacterPartMap`, `DA_AFL_CosmeticCatalog`, and the character/material assets via the
editor bridge (no PIE, no writes). Skill citations verified present under
`Tools/skills/lyra-skin-builder-marketplace/references/`.*

---

## Decision 11 — Authoritative ROSTER SPEC (corrects the loose "30 names")

*Recorded 2026-06-16. Supersedes the informal "30-name roster" framing everywhere it appears.*

**Target: 37 character-axis identities total**, of which **7 are ALSO registered as TEAMS**
(the dual-register precedent — a single identity carrying both an `AFL.Character.<Name>` and an
`AFL.Team.<Name>` registration). This is **37 characters, 7 of them dual-registered as teams** —
NOT 37 + 7. Team registration is a *flag on a character identity*, not a separate roster slot.

**Live count reconciled against disk (`DA_AFL_CharacterPartMap`, 2026-06-16, including the
pending `b3691d52` commit):**

| Axis | Live count | Target | Backlog |
|---|---|---|---|
| `AFL.Character.*` identities | **30** | **37** | **7 remaining** |
| `AFL.Team.*` rows | **7** ✅ | **7 (intended)** | ✅ **RECONCILED** — prune reflected on disk (HEAD 2241ba79) |

The **7-remaining** number on the character axis governs the remaining roster batches. **Count
against 37, not 30.**

**✅ RECONCILED 2026-07-08 (HEAD 2241ba79):** disk now shows exactly **7** `AFL.Team.*` rows — ARIA · IRONICS ·
SCARLETT · MAKHIAVELLI · AP-9 · MOB-FIGAZ · FANATICS — the **29→7 prune is REFLECTED on disk**; the +22-over debt is
**CLOSED**. (Verified via MCP catalog dump: `Type=Team` = 7.) [history retained below]

**⚠️ FLAG — team-registration count is 29, not the intended 7.** The "dual-register all branded"
pass (commits `f3ef4100` → `b3691d52`) registered **nearly the entire roster** on both axes. Disk
shows **29 of 30** characters hold an `AFL.Team.*` row; the lone character-only identity is
**BigSixx**. The operator's intent is **only 7 identities should be teams**. This is a real
divergence between intent (7) and disk (29), recorded here as the governing discrepancy — the team
axis must be **pruned from 29 → 7** (decide which 7 are the canonical teams; de-register the other
22 `AFL.Team.*` rows). Tracked as open debt; **not** auto-resolved (which 7 stay is an operator call).

**Disk roster (30 character identities, 2026-06-16):** AP-9 · ARIA · Akuma · Astra · Aurelia ·
Azura · BigSixx · Cielo · Draco · Ember · FANATICS · Halo · IRONICS · Kage · MAKHIAVELLI ·
MOB-FIGAZ · NovaKai · OnyxPrime · Orion · RiftOne · Ronin · Ryu · SCARLETT · Sable · Solara ·
Talon · Valkyr · Vanta · Volt · Zen. *(BigSixx = the only character-only identity; the other 29
also hold a team row pending the 29→7 prune.)*
