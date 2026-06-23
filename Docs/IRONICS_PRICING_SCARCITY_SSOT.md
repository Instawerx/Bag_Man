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
- `ContainedEntitlementIds[]` = **{ a unique identity · a mask · a color set · a unique weapon combo }**
  (`= ADR Decision 4 ChildCosmeticIds`).
- **Atomic grant:** one purchase deducts once and grants **all** contained ids, or none (ties to the
  ADR Decision 1 atomic pattern). The $500 prices the **pack**, justifying the 100× top-end premium.
- Single-item SKUs are unchanged (`bIsBundle=false`, no contained ids).

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
