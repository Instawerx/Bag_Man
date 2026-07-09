# IRONICS — Marketplace System: Master Architecture & Build Plan

**Status:** Guiding-doc SSOT for the remaining AAA build. Dated 2026-07-08.
**Rule of use:** Every future AIK / Claude Code block on a marketplace/economy/content item reads this doc first, then we expand the relevant section with disk-verified facts when we scope that item to build. This doc is the *architecture*; the *facts* get filled in at scope-time, item by item.

**Core principle (operator-stated):** *One system, some matrixes, many parts — everything symbiotic.* Nothing here is greenfield; every item extends proven foundations. We build the shared spine once and slot items in as instances.

---

## 0. What is already PROVEN (the foundation we extend)

These are committed, disk-verified, and live-proven — the substrate everything below plugs into. (Per the re-anchor checkpoint, HEAD `47b862ea`.)

| System | State | Where |
|---|---|---|
| **Wallet** — server-authoritative, anti-spoof (5/5 assertions) | ✅ PROVEN LIVE | `UAFLWalletComponent` (AFLCombat) |
| **Economy anti-spoof ladder** A1.2–A1.4 (purchase, earn, identity) | ✅ 5/5 CLOSED | earn Lambda, `/earn`, `/resolve-identity`, dedupe |
| **Persistence write-side** — one seam owns load + authoritative write + cache | ✅ DONE (`f010a0c1`) | `IAFLCosmeticPersistence` / `UAFLEconomyPersistenceSubsystem` |
| **Catalog** — `FAFLCatalogEntry` + `AFL.<Type>.<n>` address scheme | ✅ PROVEN | `AFLCosmeticCore`, `UAFLCosmeticCatalogSubsystem` |
| **Rarity / pricing SSOT** — `EAFLCosmeticRarity`, IRONICS_CATALOG_MATRIX | ✅ PROVEN | AFLCosmeticCore |
| **Cosmetic loadout** — independent color / finish / beam / facemask axes | ✅ PROVEN | `UAFLCosmeticLoadoutComponent` |
| **AbilitySet-grant pattern** — experience `AddComponents`/`AddAbilities` | ✅ PROVEN | Lyra experience seam |
| **IRONICS hero + movement** — `B_Hero_BagMan`, CMC-component (CLIMB/DASH/GRAB) | ✅ PROVEN | AFLMovement |
| **Server identity** — `/resolve-identity`, per-player verified PlayFabId | ✅ PROVEN LIVE | `AFLOnline` |
| **Ship cook** — shipping-client cook SUCCEEDS, staged artifact exists | ✅ RESOLVED | mem#21 demoted to hygiene |

**Phase 1 persistence consolidation — ✅ DONE (committed `f010a0c1` + tracker `483a5842`).** The two authoritative PlayFab writes (A1.2 purchase, A1.3 earn) now route **through** `IAFLCosmeticPersistence` (`PurchaseThroughBackend` / `EarnThroughBackend` in `UAFLEconomyPersistenceSubsystem`), no longer bypassing it inline. Both halves proven live: purchase seam `AFL_TEST[SEAM] PASS` (ClientRequestPurchase→PurchaseThroughBackend→PlayFab, VO deducted + granted + spoof-rejected), earn seam `AFL_A13S3 earn ok +50 newBal=105` through `EarnThroughBackend` on the dedicated server. → *One seam now owns all economic state (load + authoritative write + cache); leasing and every "+Economy" part wire to it, not a bypass.* See §8 Phase 1.

---

## 1. THE ONE SYSTEM — the Marketplace

Everything on the roadmap is a facet of **one marketplace/economy system**. Its anatomy:

- **Asset/part model** — the parts/assets already tracked for gameplay ARE the economic units. One registry; gameplay and economy share it. No parallel inventory.
- **Wallet** — server-authoritative, anti-spoof. Handles *all* value flow: purchases, earns, lease payments, revenue-share. One wallet, every transaction type.
- **Catalog** — `FAFLCatalogEntry` + `AFL.<Type>.<n>`. Every sellable/leasable/earnable thing is a catalog entry.
- **Entitlement** — who has the right to use what. Gains a **grant-type dimension** (see §4): permanent (owned) vs. time-bounded lease.
- **Persistence** — `IAFLCosmeticPersistence` — the clean seam all reads/writes route through (Phase 1 consolidation ✅ done).
- **Anti-spoof** — server-authoritative, the exact discipline proven on earn/purchase, applied to every new transaction type (including leasing).

**The mental model:** *one system → a few matrixes → many parts → surfaced by views → fed by drivers → extended by the lease market.*

---

## 2. THE MATRIXES (organizing structures within the one system)

The matrixes are *how the system is organized*, not separate systems. They already exist; we extend them.

- **Catalog matrix** (`IRONICS_CATALOG_MATRIX`) — the master table of every entry: id (`AFL.<Type>.<n>`), type, rarity, price, metadata. Every new part type = new rows / a new type dimension. **This is the SSOT for what exists.**
- **Cosmetic axes** — independent, composable, **per-axis-owned**: **body-finish (`BodyId`, the sole color source / TeamColor) × edge-glow (`EdgeId`) × weapon-skin × beam × facemask** (all proven). A "skin" is a **main × edge composition** assembled at loadout, runtime-composed and locked-at-selection — **NOT a pre-baked combo SKU**; ownership is per-axis-id, so any owned main × any owned edge composes free. New parts add **new axes or slots** — but note **visors are NOT a new slot: they ARE the proven Facemask axis** (`FacemaskId` → `RefreshFacemaskForPawn`); a helmet would be the new slot. Same matrix logic, more dimensions.
- **Rarity / pricing SSOT** — `EAFLCosmeticRarity` + the pricing table. Every priced thing (sale, lease floor, rank reward) references this one SSOT. Keeps value uniform.

**Extension rule:** a new part type is a *matrix extension* (a type/slot/axis + rows), never a new system. This is why the 17 items collapse — most are matrix rows + assets.

---

## 3. THE PARTS (populate the matrixes — the bulk of the item list)

Each is an **instance** against the proven catalog + cosmetic-axes + wallet spine. None is a new system; each = matrix extension + assets + (where tagged) an economy wiring that routes through the Phase-1-consolidated persistence seam.

| # | Part (from TODO) | Plugs into | New work |
|---|---|---|---|
| 8 | **Health Packs & Accessories** (Lyra harvest/recolor/reskin/wash) | catalog + cosmetic axes | new slot/type + assets |
| 9 | **Skin Extractors** (upgrade) | catalog + entitlement | extractor as catalog type; defines how skins are *obtained* |
| 10 | **Weapons — Boost/Use Packs** (5.7 pack ready) + Economy | catalog + wallet + weapon axes | pack type + boost mechanic + economy wiring |
| 11 | **Skins — main-finish × edge, composable** + Economy | **PROVEN composable Skins axes, per-axis-owned** (BodyId finish [=TeamColor, sole color source] × EdgeId glow → RefreshSkinForPawn; own-main AND own-edge ⇒ any combo composes free at loadout, locked-at-selection — NOT a pre-baked combo SKU) | **seed PRIMITIVES + signature FINISHES, never combos.** Seedable: 11 AFL.Body.* + 11 AFL.Edge.* (priced V10000/W100000) + 21 premium AFL.Finish.<Name> signatures. 7 base AFL.Finish.* = GrantedFree (compose-free, not seeded). Render/gate/store PROVEN (#39 f8088ebe) — a SEED, not a build. **ALL 21 signature `DA_AFL_Finish_<Name>` EXIST on disk** — the owed work is **WIRING** (point each `AFL.Finish.<Name>` catalog row's Asset at its DA; rows read Asset=None today) **+ PRICING + BrandToEdge re-point**, NOT authoring. **PRE-SEED GATES (inventoried, not yet seed-ready):** (1) PRICE the 21 signatures — per the **cheap-first ladder** (`IRONICS_PRICING_SCARCITY_SSOT.md` §1.5 R2, 2026-07-08): signatures = **Standard $2.99–4.99** (2,990–4,990 V / 29,900–49,900 W, Volts OR Watts); base colors/edges = **Impulse $0.99–1.99**. (Supersedes the earlier SPARK/W100000 figure + the CAT:119-122 SURGE conflict — R2 wins.); (2) resolve the `AFL.Body.*` vs `AFL.Finish.*` prefix overlap (both Type=Finish/BodyId; Crimson/Indigo/Solar names collide) + wire the `AFL.Finish.<Name>` rows to their DAs. A Finish DA's `EdgeGlow*` is **INTENTIONAL** (`AFL_ECONOMY_ARCHITECTURE_ADR.md:190-192` — the finish's default edge, overridden when an EdgeId is selected), **not** orphaned. **Singularity 1-of-1 = the $500 / 500,000V complete-bundle grail — a buy-once→grant-N container of DISTINCT tradeable child SKUs (Character-axis · Team-axis · finish · edge · mask · weapon, D1); law at `IRONICS_PRICING_SCARCITY_SSOT.md` §1.5/§4.5.** ("Boost Amounts" = separate economy driver, orthogonal.) |
| 12 | **Visors — Enhance AAA** + Economy | **the PROVEN Facemask axis** (`FacemaskId` → `RefreshFacemaskForPawn`) | seed a real facemask SKU into PlayFab + cheat-prove buy→entitled→equip→render — **backend seed + cheat proof, NOT a new-slot build** (slot/gate/grant/render all EXIST) |
| 13 | **Helmets — Add/Remove, +Abilities?, +Economy** | cosmetic slot **+ AbilitySet-grant** | helmet slot; the *one* part that crosses into abilities — uses the proven AddAbilities seam |
| 15 | **Character Robots — animation/IK/slide** | IRONICS hero + IK skill | animation/IK extension (the `ue5-interaction-ik-expert` skill); the body skins render on |

**Note on #13 Helmets +Abilities:** this is the only part that is *not purely cosmetic* — an ability-bearing cosmetic. It composes the cosmetic-slot pattern with the proven AbilitySet-grant pattern. Architected as: cosmetic slot for the visual + optional AbilitySet reference granted on equip (server-authoritative, so the ability can't be spoofed onto an unentitled helmet). Flagged for its own mini-scope when we reach it.

---

## 4. THE LEASE MARKET (the one NEW capability — built INTO the system)

Skin Leasing is the standout new primitive. It is **not a new inventory or a parallel system** — it is a **new entitlement grant-type + a new wallet transaction-type + a negotiation/market layer**, all built on proven seams. Operator intent: *a way for players to earn and spread popularity; exclusive items become socially and economically alive.*

### 4.1 The core insight (keeps it clean & uniform)
The **same parts/assets tracked for gameplay ARE the leasable units.** A leased visor is the same tracked visor entity — leasing just issues a *temporary entitlement* against it while the **owner-of-record is retained**. One asset model, one entitlement system; leasing adds a grant *type*, not a registry.

### 4.2 Entitlement gains a grant-type dimension
| Grant type | Meaning | Owner-of-record | Duration |
|---|---|---|---|
| **Owned** | permanent right to use | the holder | forever |
| **Leased-out** | owner has leased use to another; retains ownership | original owner | until lease term |
| **Leased-in** | temporary right to use, no ownership | (points to owner) | until lease term, auto-expiry |

Auto-expiry returns full use to the owner. The owner **never loses ownership** — only temporarily grants use.

### 4.3 The two-sided negotiation market
Bidirectional price discovery (operator-specified):
- **Supply side:** an owner **lists** an asset for lease (term, price / revenue terms).
- **Demand side:** a player **requests** a lease from an owner and **proposes an offer**; owner can **accept / counter-offer / decline** → **negotiation** loop.
- On acceptance → server creates the time-bound sublease → lessee gets *leased-in* entitlement → payment flows through the wallet → **revenue to the owner** → auto-expiry on term end.

### 4.4 Server-authoritative anti-spoof (same discipline as earn/purchase)
The lease system inherits the proven anti-spoof model. Invariants the server enforces (client never trusted):
- Can't lease an asset you don't own (entitlement check, server-side).
- Can't create/extend a lease past its term (server-authoritative clock).
- Can't forge the revenue or the payment (wallet is server-auth, all value flow anti-spoof).
- Negotiation is **server-arbitrated** — offers/counters/acceptance are validated server-side; a client can't forge an accepted offer or a price.
- Lease grants and revenue-share route through the **consolidated `IAFLCosmeticPersistence` seam** (Phase 1), not inline — so leasing is uniform with purchase/earn.

### 4.5 Why it matters (design intent → mechanics)
- **Earn:** owners monetize exclusivity by leasing out.
- **Spread popularity:** a leased exclusive is *seen* on more players → desirability compounds → the item becomes a social asset.
- **Exclusivity preserved:** ownership never transfers on a lease — scarcity is maintained even as visibility grows.

### 4.6 New pieces leasing needs (scoped in detail at build-time)
- Entitlement grant-type + owner-of-record field (extends existing entitlement records).
- Lease record: asset id, owner, lessee, term, price/revenue terms, state (listed / offered / active / expired).
- Negotiation state machine (listed → offer → counter → accept/decline → active → expired).
- A **backend lease service** (mirrors the proven earn/resolve Lambda pattern — HMAC-signed, server-auth, likely a `/lease-*` endpoint set) for the authoritative transaction + revenue-share, with a dedupe/idempotency store like the earn dedupe.
- A **lease-market view** (see §5).

---

## 5. THE VIEWS (surfaces onto the one system)

Views *display and drive* the system; they hold no economic authority (server does). All read the catalog + wallet + entitlement + rank state.

| # | View (from TODO) | Surfaces |
|---|---|---|
| 1 | **Store Front-End — Skin Marketplace** (separate, full-screen, follow design) | catalog browse + purchase; the primary economy view |
| 7 | **Loadout Design & Screens** + Economy | equipped parts + owned/leased entitlements + buy/lease entry points |
| 5 | **Lobby Upgrades / Screens** | pre-match; rank, loadout, social |
| 6 | **Menus / Match-End — Neon Glowing** (reassess neon usage) | match-end rewards (catalog earns), progression display |
| — | **Lease-Market View** (NEW, for §4) | listings, make/receive offers, negotiate, active leases + revenue |

**Design system note:** neon usage is being *reassessed* (item 6) — the Apple-Glass/neon aesthetic direction is a design-doc sub-task (the `expert-game-designer` skill applies). Views share a design language; worth a **UI/design-system guiding doc** so all views are consistent (flagged, §8).

---

## 6. THE DRIVERS (feed the one system — earn & gate)

Drivers are *sources into* and *gates on* the same economy. Rank is not a separate spine — it's an earn-and-gate driver.

| # | Driver (from TODO) | Role |
|---|---|---|
| 2 | **Leagues & Advancement** — achievements, ARC/APEX/COD-style ranking, upgrade | earn source (rank rewards = catalog entries) + gate (rank unlocks) — needs a **progression-definitions doc** |
| 3 | **Matchmaking & Game Types** — PokerStars-inspired, upgrade | gates matches by rank/type — needs a **matchmaking/game-types doc** |
| 10/11 | **Boost / Use Packs, Boost Amounts** | earn accelerators (multiply earn) — economy-wired |

**Two drivers need design docs before build** (definitions, not just wiring): progression/ranking (#2) and matchmaking/game-types (#3). Flagged in §8.

---

## 7. THE SHIP TRACK (now unblocked)

The working shipping cook (staged artifact exists) unblocks the release track. Its own sequencing conversation.

| # | Item | Note |
|---|---|---|
| 16 | **Final Check — Servers & Stores** | end-to-end audit: dedicated servers (S12/GameLift path), store/economy live-check |
| 17 | **Release Packaging** | ship cook proven; per-platform packaging |
| — | **Platforms** — Steam / Apple / Android / Xbox / PlayStation / VR; Web, Mobile, Console, Devices | each has cert requirements + input/UI adaptation; a **platform-matrix doc** sequences which first + what each needs |

**Full Audit & Assessments** (stated goal) — a pre-release audit pass across all systems; naturally lands late, before packaging.

---

## 8. BUILD SEQUENCE (spine-first, instances-second, mapped to lanes)

Ordered by dependency. Each phase: Read-Scope → build → prove → commit clean → tracker-sync (per shared-ledger doctrine #23). Lanes per the three-lane doctrine (#22): **AIK** = in-editor; **Claude Code** = editor-closed / backend / cook; **Operator** = calls/merges/machine.

### Phase 1 — PERSISTENCE CONSOLIDATION — ✅ COMPLETE (`f010a0c1`)
Routed the proven PlayFab economy writes (A1.2 purchase, A1.3 earn) **through** `IAFLCosmeticPersistence` (`PurchaseThroughBackend` / `EarnThroughBackend` in `UAFLEconomyPersistenceSubsystem`), replacing the inline bypass. Result: one clean write seam that leasing and every "+Economy" part wires to.
- **Lane (done):** Claude Code (C++ seam + call-site repoint) + operator build/PIE + dedicated-server earn proof.
- **Proof (done):** purchase seam `AFL_TEST[SEAM] PASS` in PIE (ClientRequestPurchase→PurchaseThroughBackend→PlayFab, VO deducted + granted + spoof-rejected); earn seam `AFL_A13S3 earn ok +50 newBal=105` on the dedicated server through `EarnThroughBackend`. Committed `f010a0c1` + tracker `483a5842`.
- **Result:** the foundation is clean — every downstream phase wires to the seam, not the bypass.

### Phase 2 — MATRIX & PARTS FOUNDATION (the Visors canary)
With the seam clean, prove the part-pattern once via the **Visors canary**. KEY REFRAME (disk-verified): **Visors ARE the already-proven Facemask axis** (`FacemaskId` → `RefreshFacemaskForPawn`), not a new slot — the buy→entitled→equip→render chain already exists and is wired (loadout `FacemaskId` slot, real entitlement gate `AxisEntitled`→wallet `IsEntitled`→owned-set, purchase grant via `ApplyPurchaseResult`, slot-1 material-swap render). So the canary is **not a new-slot build**; it:
1. **Seeds one real facemask SKU into PlayFab `AFL_Main`** (only 3 test items are seeded today) — Claude Code (backend `economy-catalog.json` + `setup:economy`), with a VO reseed so the SKU is affordable.
2. **Confirms the DA row's price** so the PlayFab price matches (`PurchaseItem` enforces the catalog price) — AIK/verified-in-editor.
3. **Cheat-proves buy→entitled→equip→render** end-to-end on that SKU — Claude Code, conform to `afl.Online.VerifyPurchaseSeam`.

That joins the proven purchase seam to the proven facemask chain on one real cosmetic — the pattern the remaining parts then conform to. (Other part *types* — extractor, pack, helmet slot — extend the matrix as they land; those ARE new slots/types.)

### Phase 3 — THE PARTS (fast, as instances)
Health packs (8), extractors (9), weapon packs (10), enhanced skins (11), remaining cosmetic slots (12 done as canary, 13 helmets with the ability-cross), character/IK (15). Each = matrix rows + assets + economy wiring to the seam. Batchable once Phase 2 proves the pattern.

### Phase 4 — THE LEASE MARKET (the new capability, its own detailed arc)
The one primitive that needs real new architecture. Build order within it:
1. Entitlement grant-type + owner-of-record (extends entitlement records) — AIK.
2. Backend lease service (`/lease-*`, HMAC-signed, server-auth, dedupe) — Claude Code, mirrors the proven earn/resolve Lambda pattern.
3. Lease record + negotiation state machine (server-arbitrated) — AIK + Claude Code.
4. Anti-spoof proof set (mirror the earn assertions: can't-lease-unowned, can't-extend-term, can't-forge-revenue, server-arbitrated-negotiation) — proven live on the dedicated server.
5. Lease-market view — AIK.

### Phase 5 — DRIVERS (need docs first)
Progression/ranking (2) and matchmaking/game-types (3) — **build their design docs first**, then wire as earn-and-gate drivers into the economy.

### Phase 6 — VIEWS (throughout, consolidated here)
Store front-end (1), loadout (7), lobby (5), menus/match-end (6), lease-market view (4). Share a **UI/design-system doc** (neon reassessment, Apple-Glass direction) so views are consistent. Some views build alongside their system (e.g., store with Phase 2, lease-market with Phase 4); this phase is the consolidation/polish pass.

### Phase 7 — SHIP & PLATFORM
Final audit (16), release packaging (17), platform matrix (Steam/Apple/Android/Xbox/PS/VR). **Platform-matrix doc first** to sequence.

---

## 9. SYMBIOSIS MAP (build shared pieces ONCE)

What's shared, so we never build it twice:

- **The wallet** — every transaction (purchase, earn, lease payment, revenue-share, boost) uses the one server-auth wallet. Built once (proven). ✅
- **The catalog + `AFL.<Type>.<n>`** — every sellable/leasable/earnable/rewardable thing is one catalog entry. Parts, rank rewards, lease listings all reference it.
- **The entitlement system** — ownership, leases, rank-unlocks are all entitlement grants (grant-type dimension unifies them). Built once, extended by grant-type.
- **The persistence seam** (`IAFLCosmeticPersistence`, Phase-1 consolidated ✅) — every read/write routes through it. Consolidated once, everything uniform.
- **Anti-spoof discipline** — the proven earn/purchase server-auth pattern is the template for leasing and every new transaction type. Proven once, applied everywhere.
- **The asset/part registry** — gameplay tracking = economic units. One registry serves both. No parallel inventory (the key leasing insight).
- **The design system** — one UI language across all views. One doc, consistent surfaces.

**Bottom line:** one system, a few matrixes, many parts. Build the spine (Phase 1) clean, prove the pattern (Phase 2 canary), and the roadmap becomes instances — with the lease market as the one genuinely new, high-value arc built cleanly into the same seams.

---

## 10. GUIDING DOCS THIS SPAWNS (build at scope-time)
- **This doc** — master SSOT (done).
- **Progression / Ranking definitions** (driver #2) — before Phase 5.
- **Matchmaking / Game-Types** (driver #3) — before Phase 5.
- **UI / Design-System** (neon reassessment, Apple-Glass) — before/with Phase 6 views.
- **Platform matrix** (cert + input + sequencing) — before Phase 7.
- **Lease-market detailed spec** (expand §4 with disk facts) — at Phase 4 scope-time.

*Each expands with disk-verified facts when we scope its build. This doc holds the architecture; scope-time holds the facts.*
