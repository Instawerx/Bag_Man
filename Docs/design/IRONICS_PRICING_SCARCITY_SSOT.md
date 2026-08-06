# IRONICS — Pricing & Scarcity SSOT

> **Status: APPROVED SSOT — locked 2026-06-22 (operator-confirmed).** Rarity ladder, labels, the
> stretched price curve, the FLICKER base rung, category→rung mapping, the bundle SKU, and the
> discount/reissue policy are **doc law**. All values are in the **real economy units**
> (`IRONICS_ECONOMY_SPEC`).
>
> **Grounds in:** `IRONICS_ECONOMY_SPEC.md` (peg, §2 tier ladder, currencies) · `AFL_ECONOMY_ARCHITECTURE_ADR`
> (Decision 4 — bundles). **Resolves:** `IRONICS_PLAYER_FLOW.md` §9.3 (now RESOLVED).
> **Standing dependency (NOT closed here):** limited-edition *enforcement* (mint caps, sold-out,
> bundle grant) rides the **FOUNDATIONAL persistence backend**, still open (`IRONICS_PLAYER_FLOW` §11).

---

## 1. GROUND — the peg + units (quoted, `IRONICS_ECONOMY_SPEC`)

> **§0 PEG (exact integer math, integer units, NEVER floats):**
> *"1 Volt = $0.001 · **10 Watts = 1 Volt** · $1 = 1,000 Volts = 10,000 Watts."*

**Conversions:** Watts → Volts ÷10 · Volts → Watts ×10 · **Volts → USD ÷1,000** · **Watts → USD ÷10,000**.

> **§1 currencies:** **Watts** (soft, earned) · **Volts** (hard, real-money, gated) · **Bolts** (P2P, gated Phase 3).

> **§2 tier ladder (existing rungs — verbatim):** **SPARK** 10,000 V / 100,000 W / $10 (Watts-buyable) ·
> **SURGE** 16,000 V / $16 · **ARC** 23,000 V / $23 · **THUNDER BOLT** 30,000 V / $30.

> **Two namespaces, kept distinct:** the **currency RUNGS** above (SPARK…THUNDER BOLT) price *unlimited*
> stock; the **rarity LABELS** below (Static…Singularity) name *limited* editions. Context disambiguates
> the one electrical-theme reuse (rarity "Surge" ≠ the SURGE rung; rarity "Bolt" ≠ the THUNDER BOLT rung).

---

## 1.5 SUPERSEDING RULINGS — 2026-07-08 (operator, R1–R5)
**These OVERRIDE the conflicting lines below** (§1 tier ladder, §2/§3 rungs, §4.2 category map) **and `IRONICS_PRODUCT_SKU_CATALOG.md:126`.** Superseded text is retained as history (RECONCILED-banner pattern). **DOCS-ONLY** — the disk/catalog changes are the persistence-gated BUILD phase (§5.1), a separate reviewed block.

**R1 — ROSTER RECLASSIFY.** IRONICS is the **sole free base identity**. All **29 non-IRONICS named robots = $500 / 500,000 V 1-of-1 Singularity Exclusive Bundles** (mint cap 1). SUPERSEDES `SKU:126` (roster GrantedFree) + §4.2 (identities → ARC $23) **for the named roster**: the three identity strata (free base · ARC identity · grail) COLLAPSE to **free base (IRONICS) + $500 grail (every other named robot)** — no ARC-identity middle rung. Rows (disk, HEAD 0cf0661e): 30 `AFL.Character.*` (29 → Singularity; `AFL.Character.IRONICS` free) + 7 `AFL.Team.*` (`AFL.Team.IRONICS` free; the 6 others below). ✅ **RESOLVED — D1 (2026-07-08):** every asset is an INDIVIDUALLY-OWNED, INDIVIDUALLY-TRADEABLE SKU (econ resale/trade structure). The Character-axis identity and the Team-axis identity are **SEPARATE entitlement ids** — NOT one merged both-axis grant. Both are DISTINCT tradeable child line-items of the Singularity **container** bundle (see R5 / §4.5). No `Acquisition` flipped here (build).

**R2 — CHEAP-FIRST LADDER** (retires the named rungs SPARK/SURGE/ARC/THUNDER BOLT — all sat above impulse). Peg `$1=1,000 V / 1 V=10 W`, `$0.99` floor (R3):

| Rung | USD | Volts | Watts | Buy with | Contents |
|---|---|---|---|---|---|
| **Free** | $0 | — | — | — | 7 base `AFL.Finish.*` + base edges + **IRONICS** (R1) |
| **Impulse** | $0.99–1.99 | 990–1,990 | 9,900–19,900 | Volts **OR** Watts | single color `AFL.Body.*` / single edge `AFL.Edge.*` |
| **Standard** | $2.99–4.99 | 2,990–4,990 | 29,900–49,900 | Volts **OR** Watts | 21 signature finishes · masks · std weapon skins |
| **Premium** | $7.99–14.99 | 7,990–14,990 | — | **Volts ONLY** | signature weapon skins · exclusive beams · event masks |
| **Grail** | $500 | 500,000 | — | **Volts ONLY** | the 1-of-1 Singularity bundle (R1 / R5) |

Volts-OR-Watts on Free→Standard; **Volts-only paid wall at Premium+** — generalizes the existing rule `IRONICS_ECONOMY_SPEC.md:69-70` ("only the Accessible tier is directly Watts-buyable; higher tiers Volts, Watts discount only"). V/W are the mechanical peg derivation of the USD band (not a monetization call). The FLICKER rung (§4.3) is ABSORBED into Impulse. ✅ **RESOLVED — D3 (2026-07-08):** the FREE edge set = the edges matching the 7 base-color finishes (base color + its matching edge = a complete free starter look). Mapping the 11 `AFL.Edge.*` (all Direct-priced on disk) to the 7 base colors {Blue·Green·Purple·Pink·Red·Black·Yellow}: **FREE (6, base-matching):** `NeonBlue · NeonGreen · NeonPurple · NeonPink · NeonRed · NeonYellow`; **IMPULSE (5, non-base):** `Crimson · Indigo · Solar · Magenta · Lime`. ✅ **RESOLVED — E2 = AUTHOR-FREE (2026-07-08):** author `AFL.Edge.Black` (a black/dark edge-glow) as a FREE base edge → the free starter set = **7 finishes + 7 matching FREE edges** (restores the 7-free target). **FREE (7):** `NeonBlue · NeonGreen · NeonPurple · NeonPink · NeonRed · NeonYellow · Black`(new); **IMPULSE (5, non-base):** `Crimson · Indigo · Solar · Magenta · Lime`. **BUILD-OWED:** the `DA_AFL_Edge_Black` DA + its catalog row are authored in the build block, conforming to the 6 existing base edges — this doc records the decision, not the build.

**R3 — FLOOR:** cheapest paid item = **$0.99 = 990 V = 9,900 W**.

**R4 — EARN↔PRICE RECONCILE** (matches-to-own). Structured earn = **~4,000 W/match-equiv** (`IRONICS_ECONOMY_SPEC.md:80,97`; combat-loot head 160 W / limb 20 W, `:127`):

| Item (Watts path) | OLD | NEW |
|---|---|---|
| base color / edge | SPARK $10 = 100,000 W ≈ **25 matches** | Impulse $0.99 = 9,900 W ≈ **2.5 matches** |
| signature finish | SPARK $10 (§4.2) ≈ **25 matches** | Standard $2.99 = 29,900 W ≈ **7.5 matches** |
| top Standard | — | $4.99 = 49,900 W ≈ **12.5 matches** |

**Recommendation — PRICE lever only; keep earn locked at ~4,000 W/match.** The R2 drop ALONE reconciles the grind: base wardrobe = **~2.5 matches** (was 25), inside the F2P best-practice hook (first cosmetic in ~2–5 matches). The proven retention shape (`ECON:76-102`) is preserved; splits stay "tune-at-playtest." (If Standard's ~7.5 matches proves slow at playtest, the operator-signable lever is a modest Standard-Watt trim, **NOT** earn inflation — flagged, not applied.)

**R5 — BUNDLE COMPOSITION** (records at §4.5). Doc §4.5 = `{identity · mask · color-set · unique weapon}`; operator = `{character · logo · finish · edge}`. **Merged canonical** (logo ⊂ identity; color-set = finish+edge; doc adds mask+weapon): the Singularity bundle is a **buy-once→grant-N CONTAINER** (`EAFLCosmeticType::Bundle`, `ContainedEntitlementIds[]`, `ADR:126`) whose children are **DISTINCT, individually-tradeable SKU ids** (D1): **{ Character-axis SKU · Team-axis SKU · signature finish · signature edge · signature mask · unique weapon }** — a COMPLETE 1-of-1 look assembled from separable inventory assets (owner holds e.g. MOB-FIGAZ on both axes as two tradeable ids; "color-set" = finish+edge; logo ⊂ identity). ✅ **D2 (2026-07-08) — WEAPON RETAINED** per §4.5: the unique weapon is its own distinct child SKU; its exclusive 1-of-1 SKIN is scoped to the upcoming **WEAPONS PHASE** (the bundle reserves the weapon + exclusive-skin slot now; skin content authored that phase — cross-ref `IRONICS_WEAPONS_SSOT.md` / roadmap). ✅ **RESOLVED — E1 = OPTION B (INTACT-ONLY, 2026-07-08):** the Singularity 1-of-1's child SKUs are **LOCKED TO THE CONTAINER** — the grail trades ONLY as an intact atomic unit; children are non-separably-tradeable while bundled; only the whole container transfers (one owner; mint-cap-1 stays truthful). Persistence (§5.1): the durable backend **LOCKS grail children to the container** (no per-child transfer for grail children). **Distinct from D1's general rule:** individually-acquired assets remain freely tradeable; **ONLY grail-bundle children are container-locked** — the grail is a distinct **atomic asset class**, not a contradiction of D1.

**R6 — GENERAL-CATALOG WEAPON ECONOMY** (2026-07-10, operator-approved; closes the general-catalog build's pricing). The 13 general-catalog weapons + the universal skin/beam axes are priced on the R2 cheap-first ladder — **mostly-free base roster, exclusives premium, NO B2 dependency** (grails stay the separate premium track):
- **Weapons (13):** **10 FREE** (`GrantedFree` — the batch pack-conversions = accessible base roster) + **3 Impulse** ($0.99 = 990 V / 9,900 W, pay-either) = the custom-mesh pilot signatures (**Arclight · Voltaic · Ioncaster**, the only own-skeleton weapons).
- **Skins (50):** **7 FREE** base-palette (the 6 NeonCamo family flagships `ElectricBlue · ToxicGreen · ArcViolet · CyanMagenta · CrimsonArc · Amber` + the newly-authored **`Black`** MI/row — mirrors the free finish/edge starter, the E2 Black-edge precedent) · **30 SPARK at Impulse** ($0.99 pay-either) · **13 SURGE at Premium** ($7.99 = 7,990 V, **Volts-only**; the Prism/Glitch exclusives + GlitchLegend stay premium).
- **Beams (49):** **6 FREE** base-palette (the same 6 flagships; **no `Black` beam — the 7-skins / 6-beams asymmetry is intentional**, operator-ruled, do NOT author a 7th to force symmetry) · **30 SPARK at Impulse** · **13 SURGE at Premium** (Volts-only).
- **Enforcement (already wired, no code change):** the proven `UAFLWalletComponent` loop reads these verbatim — `GrantedFree` auto-entitles (owned by all, excluded from the shop grid, `AFLCosmeticCatalogSubsystem.cpp:158`); Impulse is pay-either; the **Premium Volts-only wall is enforced at the PURCHASE layer** (an explicit Watts-pay on a `PriceWatts==0` item is denied, `AFLWalletComponent.cpp:342-344`), not merely at display. Applied + disk-verified + committed 2026-07-10.

---

## 2. RARITY TIERS — the limited-edition mint ladder (LAW)

| Mint tier | Copies | Rarity label | Currency | Discount | Reissue |
|---|---|---|---|---|---|
| **1-of-1** | 1 | **Singularity** | Volts | **NEVER** | **NEVER** |
| **1-of-10** | 10 | **Tempest** | Volts | **NEVER** | **NEVER** |
| **1-of-50** | 50 | **Bolt** | Watts | **NEVER** | **NEVER** |
| **1-of-100** | 100 | **Surge** | Watts | discountable | **NEVER** |
| **1-of-1,000** | 1,000 | **Charge** | Watts | discountable | **NEVER** |
| **1-of-10,000** | 10,000 | **Static** (bottom limited tier) | Watts | discountable | **NEVER** |

*(Labels = operator-confirmed Option A. "Bolt" chosen for 1-of-50 over an "Arc" candidate to avoid
collision with the ARC currency rung.)*

### 2.1 Policy — **DOC LAW**
- **NEVER-REISSUE — ALL six limited tiers.** The mint count is a **hard, permanent cap**; an edition is
  minted **once** and **never reopens** once sold out / window-closed. The fixed count *is* the product.
- **NEVER-DISCOUNT — cutoff at 1-of-50.** **Singularity (1-of-1) + Tempest (1-of-10) + Bolt (1-of-50)**
  are **never discounted** — full price always, outside all sales/bundles/Watts-discounts.
- **DISCOUNTABLE — Surge / Charge / Static** (1-of-100 / 1-of-1,000 / 1-of-10,000) may take
  Watts-discounts + sales (volume conversion products).

---

## 3. THE PRICE CURVE — stretched, ADOPTED (literal kept for reference)

**Operator anchors:** bottom (Static) ≈ 10,000 W · rarest Watts-tier (Bolt) ≈ 50,000 W · 1-of-1 ≈ 100,000 V.
**Why literal is too flat:** `100,000 V = 1,000,000 W`, so the literal top spans **50,000 W → 1,000,000 W
= only ~20×** across the three rarest tiers — far too flat for a one-of-one.

### 3.1 Side-by-side (USD-equiv via peg)

| Mint tier (label) | LITERAL-anchor (reference) | **STRETCHED — ADOPTED (law)** |
|---|---|---|
| 1-of-10,000 **Static** | $1 (10,000 W) | **$1 · 10,000 W · 1,000 V** |
| 1-of-1,000 **Charge** | $2 | **$1.50 · 15,000 W · 1,500 V** |
| 1-of-100 **Surge** | $3.50 | **$3 · 30,000 W · 3,000 V** |
| 1-of-50 **Bolt** | $5 | **$5 · 50,000 W · 5,000 V** |
| 1-of-10 **Tempest** | $52.50 | **$50 · 500,000 W · 50,000 V** |
| 1-of-1 **Singularity** | $100 | **$500 · 5,000,000 W · 500,000 V** *(BUNDLE, §3.3/§4.5)* |
| **top ratio (vs Bolt $5)** | **20×** | **100×** |

**What ADOPTED changes:** the base is identical ($1–$5, mass-affordable for conversion); the top ramps to
**$500 — a 100× premium over the Bolt tier** (vs the flat 20× literal). The grail stands well clear of
the Tempest (10× below it).

### 3.2 Currency split
- **Watts (soft, earnable):** Static · Charge · Surge · Bolt ($1–$5).
- **Volts (hard):** Tempest ($50 / 50,000 V) · Singularity ($500 / 500,000 V).

### 3.3 Per-tier rationale (CONFIRMED)
| Tier | Price (law) | Rationale |
|---|---|---|
| Static (1-of-10,000) | **$1 / 10,000 W** | Entry limited — priced for **volume sell-through**. |
| Charge (1-of-1,000) | **$1.50 / 15,000 W** | Clustered low — conversion product. |
| Surge (1-of-100) | **$3 / 30,000 W** | 10× rarer, ~2× the price — **affordability over linear escalation.** |
| Bolt (1-of-50) | **$5 / 50,000 W** | **Rarest Watts-tier** — top of the earnable band. |
| Tempest (1-of-10) | **$50 / 50,000 V** | **First true exclusive** — a 10× step into hard currency. Never-discount. |
| **Singularity (1-of-1)** | **$500 / 500,000 V** | The **grail — a BUNDLE SKU** (§4.5: identity + mask + color set + unique weapon combo in one atomic purchase), so $500 is fair value, not a bare cosmetic. **100× the Bolt.** Never-discount, never-reissue. |

---

## 4. INTEGRATE WITH THE PLAYER FLOW + CATEGORY PRICING

### 4.1 Two pricing axes — a cosmetic uses ONE
**Standard ladder** (unlimited stock, priced by rung, FLICKER→THUNDER BOLT) **or** **rarity ladder**
(limited mint, priced by §3). The same product line can ship both (an unlimited ARIA at ARC **and** a
separate limited `Surge` ARIA variant).

### 4.2 Category → rung map — **CONFIRMED (law)** (resolves `IRONICS_PLAYER_FLOW` §9.3)

> ⚠️ **SUPERSEDED for the NAMED ROSTER + the rung names by §1.5 R1/R2 (2026-07-08):** non-IRONICS identities are **$500 Singularity bundles** (not ARC); the SPARK/SURGE/ARC/THUNDER BOLT rungs are replaced by the cheap-first ladder (§1.5 R2). The table below is retained as history + still frames the NON-identity categories (finishes/masks/weapons → the §1.5 R2 rungs).

| Priced category | Standard rung | Price | If LIMITED |
|---|---|---|---|
| **Non-IRONICS identity** | **ARC** | **23,000 V / $23** (prestige signature → THUNDER BOLT $30) | any rarity tier (§2) |
| **Premium finish SET** | **SPARK** | **10,000 V / 100,000 W / $10** | any rarity tier |
| **Extra mask** | **SPARK** | **10,000 V / 100,000 W / $10** | any rarity tier |
| **Extra / unchosen base weapon or beam** | **FLICKER** (§4.3) | **10,000–25,000 W / $1–$2.50** | (base content — normally unlimited) |
| **Future skins / guns** | by tier | — | any rarity tier |

*Identities sit **~2× a finish** ($23 vs $10) — as intended.*

### 4.3 The FLICKER base rung — **CONFIRMED (law)**
A rung **below SPARK** for base-tier extras (the §2 ladder's cheapest, SPARK at 100,000 W, is premium-priced):

| Rung | Volts | Watts | ≈USD | For |
|---|---|---|---|---|
| **FLICKER** (sub-Accessible) | 1,000–2,500 V | **10,000–25,000 W** | **$1–$2.50** | base extras: unchosen base weapons/beams, cheap base items — **Watts-only** (earnable) |

### 4.4 ASSET-STORE DATA SHAPE — how content scales (design-level; catalog implements; no code here)
| Field | Purpose | Status |
|---|---|---|
| `CosmeticId` · `Type` (`EAFLCosmeticType`) · `Acquisition` · `ContentTier` | address + class + acquisition + base/premium | ✅ exists |
| **`PriceRung`** (FLICKER/SPARK/SURGE/ARC/THUNDER BOLT) | standard-ladder rung → price | 🔴 NEW (or derive from cost) |
| `CostVolts` / `CostWatts` | actual price (integer units) | ✅ exists |
| **`bIsLimitedEdition`** + **`RarityTier`/`MintCap`** (the `1-of-N`) | rarity-ladder slot + permanent cap | 🔴 NEW |
| **`MintedCount`** (runtime) | minted/sold → "X of N left" + sold-out | 🔴 NEW · **persistence-gated** |
| **`bDiscountable`** | sales/discount eligible (FALSE for Bolt/Tempest/Singularity, §2.1) | 🔴 NEW |
| **`bIsBundle`** + **`ContainedEntitlementIds[]`** | bundle/pack SKU — atomically grants its contained ids (§4.5) | 🔴 NEW (= ADR Decision 4 `EAFLCosmeticType::Bundle` + `ChildCosmeticIds`) |
| `bTradeable` (per-SKU) | ever tradeable (ADR flag); bound policy = `IRONICS_PLAYER_FLOW` §8.4 | ✅ exists |
| `Rarity` (`EAFLCosmeticRarity`) · `ColorIdentityTag` · `CollectionId` | shop badge + filters | ✅ exists |

> **Two "rarity" axes — separate:** `EAFLCosmeticRarity` (Common→Legendary) = the **shop-frame badge**;
> **`RarityTier`/`MintCap`** (the `1-of-N`) = the **limited-edition scarcity product**. Never share a field.
> New content slots in by **declaring fields** — pick a standard rung *or* a rarity tier, set
> discountable/tradeable/bundle, done. No per-item pricing re-decision.

### 4.5 The Singularity BUNDLE SKU — **CONFIRMED (law)**
**Singularity (1-of-1) is a BUNDLE, not a bare cosmetic.** It is an `EAFLCosmeticType::Bundle` SKU (ADR
Decision 4: *"a bundle is a SKU whose ownership **grants a SET of child SKU ids**… buying the bundle →
the entitlement grant loop adds **each child id** into the owned-set"*) that **atomically grants** its
contained entitlements in one purchase:
- `ContainedEntitlementIds[]` = **{ Character-axis identity SKU · Team-axis identity SKU · a mask · a signature finish · a signature edge · a unique weapon }** — each a **DISTINCT, individually-tradeable child SKU id** (D1 2026-07-08: every asset is individually owned/tradeable; the identity is TWO separable ids, not one merged grant; "color set" = finish + edge, explicit)
  (`= ADR Decision 4 ChildCosmeticIds`).
- **Atomic grant:** one purchase deducts once and grants **all** contained ids, or none (ties to the
  ADR Decision 1 atomic pattern). The $500 prices the **pack**, justifying the 100× top-end premium.
- Single-item SKUs are unchanged (`bIsBundle=false`, no contained ids).
- **INTACT-ONLY TRADE (E1, 2026-07-08 — law):** a Singularity (mint-cap-1) bundle trades **only as a whole atomic unit** — its child SKUs are **container-locked** (non-separably-tradeable while bundled; no per-child transfer). Only the whole container moves to a new owner, keeping the 1-of-1 truthful. Distinct from the general D1 rule (individually-acquired assets stay freely tradeable); grail children are the exception — an atomic asset class. Enforcement rides §5.1 persistence.

---

## 5. CONFIRMED — now law (was PROPOSED)
All previously-open pricing decisions are **resolved + locked**: rarity labels (§2) · the stretched curve
incl. **Singularity $500** (§3) · the **FLICKER** base rung (§4.3) · category→rung (§4.2, identities→ARC,
finishes/masks→SPARK, base→FLICKER) · never-discount cutoff at **1-of-50 / Bolt** (§2.1) · the
**Singularity bundle** SKU (§4.5). **Nothing in this doc remains PROPOSED.**

### 5.1 Standing dependency — the MODEL is law; ENFORCEMENT is persistence-gated (state plainly)
The pricing **model** above is law and can be authored into the catalog as data **now**. But every
**limited-edition mechanic — `MintedCount`, mint-cap enforcement, sold-out, never-reissue, and the
bundle atomic grant — requires DURABLE state**, which rides the **FOUNDATIONAL persistence backend that
is still open** (`IRONICS_PLAYER_FLOW` §10 #7 / §11). Without persistence, a "1-of-1" cannot be *enforced*
as one (the mint counter resets per session). **So: the pricing model ships as law; limited-edition
enforcement waits on persistence — do not treat "pricing done" as "limited editions working."**

---

## 6. Cross-links
- **`IRONICS_PLAYER_FLOW.md`** — this doc **resolves §9.3** (category→rung + base low-rung); §8.4
  tradeable/bound policy + §4 store read against this model. `MintedCount`/bundle-grant ride its §11
  FOUNDATIONAL persistence.
- **`IRONICS_ECONOMY_SPEC.md`** — peg / currencies / §2 ladder (cited). · **ADR Decision 4** — the bundle SKU shape.
- **Separate open work (not pricing):** team-readability spec + persistence backend (`IRONICS_PLAYER_FLOW` §11).

---

*Finalized read-only 2026-06-22 — operator-confirmed values made law (labels, stretched curve incl.
Singularity $500 bundle, FLICKER rung, category→rung, discount/reissue policy). Structure + numbers =
APPROVED; limited-edition enforcement = persistence-gated (§5.1). No code, no build, nothing staged.*
