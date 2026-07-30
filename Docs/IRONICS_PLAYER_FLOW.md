# IRONICS — Player Flow & Entitlement SSOT

> **Status: APPROVED SSOT — locked 2026-06-22 (operator-approved).** Authored 2026-06-21,
> revised 2026-06-22 (locked answers), finalized 2026-06-22 (TBDs filled-or-flagged + completeness
> audit, §9–§11). The flow + entitlement model below is **law**; items still marked
> **`PROPOSED — needs operator confirm`** (a TBD with no value in any doc) are the only parts
> awaiting a decision — they are flagged, never baked as law.
> This is the **ground truth the entitlement + identity-selection + store + trade code must
> satisfy.** It **reconciles against the existing specs** — citing every refinement, recording
> every approved change (§7), flagging every gap (§8–§11). It does **not** silently overwrite a
> spec or invent a number.
>
> **Reconciled against:** `IRONICS_ECONOMY_SPEC.md` · `AFL_ECONOMY_ARCHITECTURE_ADR.md`
> (Decisions 1/3/5/10/11, Ruling 1) · `Tools/IRONICS_STORE_COLOR_SPEC.md` ·
> `bagman-brand-default-mapping.md` · `AFL_IDENTITY_PRODUCTION_LINE.md` ·
> `Tools/AFL_SKIN_L5_2C_CPP_SPEC.md` (CONFIRM 1) · skill basis
> `lyra-skin-builder-marketplace/references/{entitlement-backend,marketplace-ui,data-architecture}.md`.
> **Code surface:** `AFLWalletComponent.h` · `AFLCosmeticCoreTypes.h` · `AFLCosmeticLoadoutComponent` ·
> `AFLSkinColorControllerComponent.cpp`.
>
> **Operator-locked this revision:** (1) IRONICS is the **only** free identity [C1 APPROVED, §7];
> (2) the level gate is the **existing** Battle-Pass/tier model [C3 RESOLVED by citation, §4d];
> (3) currency = the **PEG** (10 Watts = 1 Volt); (4) starter kit unchanged; (5) base color
> **palette is always-on for everyone**, only extra **sets** are priced [§1/§3].

---

## 0. Precedence & invariants

- This is the **player-facing flow + entitlement + trade** SSOT. Where it refines an existing spec
  it cites it; where the operator has **approved a change** to an existing spec, §7 is the
  **authoritative change record** (the older spec/doc-string is updated when we execute — not before).
- **Three invariants this flow never breaks** (all pre-existing, carried forward):
  - **Ruling 1 (ADR Decision 5):** *"the applied `AFL.Finish.*` preset is the **SOLE color source**."*
  - **CONFIRM 1 (`AFL_SKIN_L5_2C_CPP_SPEC`):** *"do **NOT** use `AsyncAction_ObserveTeamColors`.
    **No team-system coupling whatsoever**."* — body color is never a team layer.
  - **NO CASH-OUT (`IRONICS_ECONOMY_SPEC` §0):** *"**NO CASH-OUT, EVER.** One-way buy-in only. **No
    path converts Watts/Volts back to real money or out of the system.**"* — binds the marketplace (§8).

---

## 1. THE STARTER GRANT — every player, first entry

| Slot | Grant | Notes |
|---|---|---|
| **Identity** | **IRONICS** (the house robot) — **the ONLY free identity** | The roster template (`AFL_IDENTITY_PRODUCTION_LINE`: *"Clone `B_AFL_Robot_IRONICS` → `B_AFL_Robot_<NAME>`"*). **C1 APPROVED** — the 5 other founding brands flip to priced (§7-C1). |
| **Colors** | **The entire BASE PALETTE — always-on for EVERYONE, permanent, freely swappable** | **Refined (operator):** not a one-time grant — an **inherent always-available palette** every player carries forever, swapped from HUD / Wallet / Menu. The 7 base `AFL.Finish.*` (ADR Decision 5: *"the **free base tier**. Blue/Green/Purple/Pink/Red/Black/Yellow"*). Only **ADDITIONAL color SETS** are entitlements (§2). |
| **Mask** | **1 base mask** | `AFL.Facemask.<base>`, GrantedFree. Exact id **TBD** (candidate `AFL.Facemask.IroVisor`). |
| **Weapons** | **CHOOSE 5** from the base weapon catalog | A **choice from a larger base set**; the unchosen are acquirable later (§2a). |
| **Beams** | **CHOOSE 2** from the base beam catalog | Same choose-N-from-base pattern. |

### 1a. The choose-N-from-base PATTERN (future categories inherit it)
> For any base category: the player **chooses N from a base set of M** at first entry; the unchosen
> `(M−N)` are **acquirable later** (§2a). New categories (skins/masks/guns/gadgets) inherit
> base = choose-N-free, rest = priced. **(Refinement — the economy spec defines `GrantedFree` and the
> tiers but not a starter *choice* mechanic; this is new design. `M` = TBD, §9.)**

---

## 2. PRICED — everything beyond the starter

`Acquisition == Direct` (shop) or `BattlePass`. Per `IRONICS_ECONOMY_SPEC` §0: *"**NO RANDOMIZED
ACQUISITION.** … All purchases are **direct, known-item**."*

| Priced category | Covers |
|---|---|
| **Non-IRONICS identities** | The other 29+ roster characters (ARIA · Akuma · Astra · Draco · MAKHIAVELLI · SCARLETT · …, ADR Decision 11). **All priced** (C1, §7). |
| **Additional color SETS** | Color packs **beyond the always-on base palette** — premium/signature finishes, `ContentTier=Premium` (ADR Decision 8). The base palette itself is never sold (§1/§3). |
| **Additional masks** | Beyond the one base mask. |
| **Additional weapons / beams** | Beyond the chosen starter — **including the base-set items not chosen** (§2a). |
| **Future categories** | base-free-choose-N / rest-priced (§1a). |

**Currency (the PEG — `IRONICS_ECONOMY_SPEC` §0, exact integer math, never floats):**
> *"1 Volt = $0.001 · **10 Watts = 1 Volt** · $1 = 1,000 Volts = 10,000 Watts."*

- **Watts** — soft, **earned by play** (§3 earn structure). Buys the **Accessible (SPARK)** tier
  directly (*"only the Accessible (SPARK) tier is directly Watts-buyable, at 100,000 Watts"*) +
  discounts higher tiers.
- **Volts** — hard, real-money (gated layer). Gate the Battle Pass + premium/prestige cosmetics.
- **Tier ladder (§2 of the economy spec):** Accessible/**SPARK** (10,000 V / 100,000 W / ~$10) ·
  Standard/**SURGE** (16,000 V / ~$16) · Premium/**ARC** (23,000 V / ~$23) · Prestige/**THUNDER BOLT**
  (30,000 V / ~$30).

### 2a. The unchosen base-set items
Not lost — **acquirable later.** Because they are **base-tier**, they are buyable with **Watts** at
the Accessible/SPARK rung (earned through play), keeping the starter a meaningful **choice** without
permanently locking base content. **Exact base-item price = TBD** (SPARK's 100,000-W point is for
premium cosmetics; a base weapon sits lower).

---

## 3. Color model (consistency, not new design)

- **The base palette is always-on for everyone (§1).** A player can switch among all base colors at
  any time; nobody ever lacks the base palette. **Additional color SETS** layer on top as priced
  entitlements — they ADD selectable finishes, never gate the base palette.
- IRONICS spawns wearing its **brand-default finish**; resolution is the existing chain
  (`AFLSkinColorControllerComponent.cpp:119-124`): **`selection > brand-default > persistent`** — a
  swap sets tier-1 selection.
- **Color is the finish, full stop (Ruling 1). No team-color body layer (CONFIRM 1).** Team
  readability moves **off the body** — **outline/rim + nameplate** (operator-confirmed; §9). The live
  stock-Lyra `TeamDA_Red/Blue` body paint is a **defect to remove** (§7-C4).

---

## 4. ENTITLEMENT CHAIN — tied to the code path

Spine (ADR Decision 1): **`UAFLWalletComponent`** = server-authoritative entitlement source —
replicated **`OwnedCosmeticIds`** (`AFLWalletComponent.h:151`), mutated only via server RPCs;
`IsEntitled` = *"is this CosmeticId in the player's owned set **(or GrantedFree)**?"* (`:42`). The
loadout **selection** is gated by it.

### 4a. Starter grant → entitlement (first entry)
- **Always-on base palette** → entitled by **class**, universal; no `OwnedCosmeticIds` write needed
  (the most-free tier — always true for everyone).
- **IRONICS + base mask** → `GrantedFree` (entitled by `Acquisition`, no explicit owned-set entry).
- **Chosen 5 weapons + 2 beams** → a per-player choice → **must be written to `OwnedCosmeticIds`.**
  **CODE TARGET (does not exist yet):** a server-auth, idempotent, persisted **`ServerGrantStarter`** —
  today the only owned-set writers are `ServerPurchaseCosmetic` / `ServerEarn*`.

### 4b. Free base-color swap — **no purchase**
`HUD/Wallet/Menu` → **`ServerSetCosmeticSelection(EdgeId=AFL.Finish.<base>)`** → gate `IsEntitled`
**passes** (base palette always-on) → commits `Selection.EdgeId` → `ApplySkinColor` tier-1. **A
selection change, not a transaction.**
> This is why `afl.Cosmetic.SetEdge NeonBlue` committed `edge=None` on the record — `AFL.Edge.NeonBlue`
> is **not** base/owned, so the gate **correctly** rejected it. A base-palette swap passes; an un-owned
> premium color does not. The gate was working.

### 4c. Priced purchase
`Store` → **`ServerPurchaseCosmetic(CosmeticId, PayWith)`** (`AFLWalletComponent.h:118`): atomic
check-and-deduct of Volts/Watts (ADR Decision 1 ACTION ITEM 1 — verified atomic; skill basis: *"`TrySpend`
must be atomic — never split into check then deduct"*), **rejects `GrantedFree` + already-owned** →
adds id to `OwnedCosmeticIds` → **then** `ServerSetCosmeticSelection` (now entitled) equips it.

### 4d. The level gate — **the EXISTING Battle-Pass / tier model (C3 RESOLVED by citation)**
I previously mis-scoped this as a new "locked-until-level" system — **retracted.** The level/progression
gate is **already defined** in `IRONICS_ECONOMY_SPEC`:

- **Battle Pass progression (§4, quoted):** *"Tiers: **~100 tiers, free + premium track, unlocked by
  play (XP/challenges)**."* → premium-track content is gated behind **play-progression** (the level/XP gate).
- **Priced tier ladder (§2):** availability is **shop tiers priced by currency** (SPARK/SURGE/ARC/THUNDER
  BOLT), not a hard per-item level-lock.
- **Earn-through-play (§3):** the player "levels up" their **Watts** by playing (match base + daily +
  weekly), which funds Accessible/SPARK purchases + discounts.

**So, exactly how level gates availability/pricing (per the existing spec):** *availability* of
premium content is gated by **Battle-Pass XP-tier progression** (§4) and by **shop tier** (§2);
*pricing* is the **PEG tier ladder** (§2) funded by **Watts earned through play** (§3). **There is no
separate level system — and none is invented.** **GrantedFree / always-on items are never gated.**

---

## 5. The identity-selection deviation (this run)
This run spawned **two `B_AFL_Robot_ARIA`** (`reqIdentity=Team/AFL.Team.ARIA` for both). This flow
says base = **IRONICS** → a fresh player should spawn **IRONICS**, not ARIA. **Two-ARIA-at-spawn is a
selection-chain deviation, fixed separately** from color:
- Default/unselected identity must resolve to **IRONICS** (ARIA is now a **priced** identity, §2 —
  a player is only ARIA if they **own + selected** it).
- This is the **identity** axis (`Selection.CharacterId/TeamId` → `CharacterPartMap` → robot BP),
  **distinct from** the **finish/color** axis (§3). Two separate fixes; this flow is the spec both satisfy.

---

## 6. Acceptance contract (what the code must satisfy — no code here)
1. **`ServerGrantStarter` exists** (§4a): IRONICS + base mask GrantedFree; base palette always-on;
   chosen 5+2 written to `OwnedCosmeticIds`; idempotent; persisted.
2. **Default identity = IRONICS** (§5).
3. **Base-palette swap is a pure selection** (§4b) — never `ServerPurchaseCosmetic`.
4. **Priced path = buy-then-equip** (§4c).
5. **Level/availability gate = the existing Battle-Pass/tier model** (§4d); always-on items never gated.
6. **Body color = finish only** (§3); team read off-body.
7. **P2P trade = a new server-auth atomic two-party path** (§8) — does not exist yet.

---

## 7. RECONCILIATION (approved changes + remaining conflicts)

### C1 — FREE IDENTITY SET → **IRONICS ONLY (APPROVED — authoritative change record)**
Operator-approved: IRONICS is the only free identity; the 5 other founding brands flip
`GrantedFree → priced`. The following doc-strings/spec lines are **now wrong** and must be corrected
**when we execute** (recorded here as the authoritative change list; **CODE NOT TOUCHED yet**):

| Source | OLD (now wrong) | CORRECTED |
|---|---|---|
| `AFLCosmeticCoreTypes.h` (`EAFLAcquisition::GrantedFree` doc) | *"GrantedFree = owned by everyone (**all founding teams**, free Character base)"* | "GrantedFree = owned by everyone (**the IRONICS base identity only**, the always-on base color palette, the base mask)" |
| `AFLCosmeticCoreTypes.h` (`EAFLContentTier::Base` doc) | *"Base = the free base layer (the 7 `AFL.Finish.*` + **free base identities**; Acquisition GrantedFree)"* | "Base = the free base layer (the 7 `AFL.Finish.*` always-on palette + **the single free identity IRONICS** + base mask; Acquisition GrantedFree)" |
| `IRONICS_ECONOMY_SPEC` §2 | *"founding team**s** + free Character base = GrantedFree"* | "the **IRONICS** base identity + the always-on base palette + base mask = GrantedFree; all other founding brands are **priced**" |
| `IRONICS_STORE_COLOR_SPEC` | *"`AFL.Team.*` (**6 brands**) … **GrantedFree identity bases**"* | "`AFL.Team.IRONICS` = the free base; the other 5 founding brands = **priced** catalog cards" |
| `FAFLCatalogEntry` rows (data) | 5 founding brands `Acquisition=GrantedFree`, `CostVolts=0` | flip to `Acquisition=Direct` + a priced tier (`ContentTier`/`CostVolts` per §2) |

### C2 — DEFAULT SPAWN IDENTITY (still active) — **base = IRONICS** vs run/precedent ARIA-first
Fix the **default-identity resolution to IRONICS** (§5). The ARIA row in `bagman-brand-default-mapping`
stays valid as **ARIA's own** default once ARIA is a priced, owned, selected identity.

### C3 — LEVEL GATE → **RESOLVED by citation** (no longer a conflict)
The economy spec already defines the gate (Battle-Pass §4 + tier ladder §2 + earn §3); §4d cites it.
My earlier "new locked-until-level system" is **retracted** — nothing new is introduced.

### C4 — TEAM COLOR ON THE BODY (still active) — **defect to remove**
Stock-Lyra `TeamDA_Red/Blue` (via `B_TeamSetup_TwoTeams` → `ObserveTeamColors` → `ApplyToMeshComponent`)
overwrites the finish — the exact path CONFIRM 1 rejects. **Remove the team-color body paint**; move
team readability to **outline/rim + nameplate** (§9). This doc is the authority that makes the removal
correct-by-design.

---

## 8. P2P MARKETPLACE (grounded in existing docs; gaps flagged, not filled)

Wallets + items are designed as **P2P tradeable/sellable** (operator), with **seller-set asking price
AND accept-offers.** The model below is **extracted from the existing docs** — where the docs only
*name* a requirement without designing the mechanism, it is a **GAP** (pending design, unfilled).

### 8.1 What the docs already specify
- **Trade is in scope + ownership-based.** ADR scope: *"all buy/earn/use/**trade**"*; *"the owned-set is
  the source of truth for **OWNERSHIP** (own/buy/earn/trade)"* (Decision 2). Keys are the full
  `AFL.<Type>.<Name>` id (Decision 3) — the unit a trade moves.
- **Trade is its own server-auth subsystem (named, not detailed).** ADR cross-cutting #3, verbatim:
  > *"Trade is its own ownership-transfer subsystem (**server-auth atomic transfer + escrow/confirm +
  > anti-dupe + per-SKU tradeable flag**) — scoped as a named workstream, not a checkbox."*
- **Durable persistence is a hard prerequisite.** ADR cross-cutting #1: *"Durable server persistence is
  the silent prerequisite under everything tradeable"*; *"trade follows persistence, not entitlement"*
  — so trade is **Phase-3-gated** (the `IAFLCosmeticPersistence` stub must become real).
- **Currency P2P transfer is a separate, legally-gated rail.** Economy §1: *"**Bolts | P2P transfer** |
  (gated, Phase 3, legal sign-off) | Player-to-player transfer (Zelle-like)"*; §0: bank/Bolts-P2P layers
  are *"designed-but-gated, each needing legal sign-off before enable."*
- **The atomicity pattern to extend.** Decision 1 + skill basis: `ServerPurchaseCosmetic` is a **single
  atomic server-side check-and-deduct** (*"never split into check then deduct… textbook race condition…
  double-spend"*). The two-party trade **extends this one-party pattern** to a two-party transfer.
- **HARD CONSTRAINT.** Economy §0 NO-CASH-OUT binds the marketplace: items trade for **in-game currency
  only**; **no path may convert value back to real money or out of the system.**

### 8.2 The transaction model the docs imply
Items (cosmetics — identities, finishes/sets, masks, weapons/beams) trade **for in-game currency**,
**server-authoritative**, **atomic**, with **escrow/confirm**, **anti-dupe**, and a **per-SKU tradeable
flag** — durable only once server persistence is real (Phase 3). Currency-for-currency P2P (Bolts) is a
**separate** legally-gated rail. The seller sets an **asking price** or **accepts an offer** (operator) —
but the **offer/escrow/concurrency mechanics are not designed** (8.3).

### 8.3 Grounded-vs-Gap (the safe-transaction invariants)

| Invariant | Status | Evidence (COVERED) / what's missing (GAP) |
|---|---|---|
| **AUTHORITY** — client requests, server decides + executes | **COVERED** | ADR Decision 1: *"The client requests…; the server decides."* Extend `ServerPurchaseCosmetic` to a two-party transfer. |
| **ATOMICITY** — item-out + currency-out + both-arrive = ONE txn, no partial | **PARTIAL** (principle COVERED, **mechanism GAP**) | Principle named: cross-cutting #3 *"server-auth **atomic** transfer"* + the one-party atomic check-deduct to extend. **GAP:** the two-party **escrow/atomic-swap mechanism is undesigned.** |
| **OFFER / ASKING-PRICE LOCKING** — buyer currency reserved while pending; no double-spend across two offers; no accepting a stale/withdrawn offer | **GAP** | The docs specify *no* offer mechanics. Asking-price + accept-offers is operator intent only. **Pending: offer reservation/locking + stale-offer invalidation.** |
| **RACE — two buyers, exactly one wins** | **PARTIAL** (named, **detail GAP**) | *"anti-dupe"* named (cross-cutting #3) + Decision 1's concurrent-purchase race warning. **GAP:** two-buyer **listing concurrency** (single-winner on simultaneous accept/buy) is undesigned. |
| **ENTITLEMENT INTEGRITY** — `OwnedCosmeticIds` moves seller→buyer atomically; per-SKU tradeable flag | **PARTIAL** (mechanism COVERED, **policy GAP**) | *"per-SKU tradeable flag"* + *"server-auth atomic transfer"* (cross-cutting #3) cover the move. **GAP:** the **bound-vs-sellable policy** for GrantedFree/always-on items is unstated (8.4). |
| **SCAM / ABUSE** — confirm, cooldown, value display | **PARTIAL** (confirm named, rest GAP) | *"escrow/**confirm**"* names a trade confirmation. **GAP:** cooldowns, value display, anti-fraud detail undesigned. |
| **NO-CASH-OUT compliance** | **COVERED (hard invariant)** | Economy §0: *"NO CASH-OUT, EVER… No path converts Watts/Volts back to real money or out of the system."* Marketplace must never become a cash-out path; Bolts stays legal-gated. |

### 8.4 Bound vs sellable — **LOCKED (operator-approved 2026-06-22)**
The bound-ness rule is **by ACQUISITION**, not by SKU tier:

- **Free-granted = account-bound (`tradeable=false`).** The IRONICS base identity, the always-on base
  palette, the base mask, **and the chosen 5 weapons + 2 beams** are bound — they kill the alt-farming
  dupe vector. **A free grant is never sellable.**
- **Purchased / earned = tradeable (`tradeable=true`).** Non-IRONICS identities, additional color sets,
  extra masks, and **extra/unchosen base weapons+beams once bought** (§2a) are sellable (subject to the
  per-SKU flag + persistence). *"Sell a purchased identity, but not your free IRONICS base."*

**CODE IMPLICATION (flag):** today `OwnedCosmeticIds` is a flat `TArray<FName>` — it records *that* you
own an id, not *how* you got it. The locked rule needs the system to distinguish a **free-granted** copy
(bound) from a **purchased** copy (tradeable). Since one player can't both free-get and buy the same id,
this is resolvable with a **per-id acquisition/bound marker** (a `BoundCosmeticIds` companion set, or an
acquisition tag per owned id) in the persistence model. The ADR's `per-SKU tradeable flag` still gates
whether a SKU is *ever* tradeable; the acquisition-bound rule layers on top. **New persistence field —
does not exist yet.**

### 8.5 The new code path (flag)
`ServerPurchaseCosmetic` is the **only** transaction RPC today (one-party buy). P2P trade needs a **new,
server-authoritative, atomic two-party path** — e.g. `ServerCreateListing` / `ServerMakeOffer` /
`ServerAcceptOffer` with **escrow** — that **DOES NOT EXIST.** Gated behind durable persistence (Phase 3).

---

## 9. TBDs — FILLED (cited) or FLAGGED (`PROPOSED — needs operator confirm`)

Inventory taken read-only on disk 2026-06-22 (Glob over `Content/` + `Plugins/GameFeatures/*/Content/`).
No number is invented as law; where no doc defines a value, the real asset count is the basis for a flagged decision.

### 9.1 Base-set sizes (weapons / beams) — **WEAPONS-PHASE item** (content not at the implied size)
**Real on-disk inventory:**
- **AFL-authored weapons (`/AFLBagMan/Content/Equipment`):** `WID_BagMan_PulseCarbine` (1 gun) ·
  `WID_BagMan_Beam` + `WID_BagMan_Beam_v2` (**1–2 beams**, `_v2` reads as an iteration of one beam).
  → **1 AFL gun + 1–2 AFL beams.** (No `ID_AFL_*`/`ED_AFL_*` exist — AFL weapons use `WID_BagMan_*`.)
- **Reusable ShooterCore demo guns (Lyra):** `WID_Pistol` · `WID_Rifle` · `WID_Shotgun` (+ `NetShooter` proto).
- **Total realistically available base guns ≈ 4** (PulseCarbine + 3 ShooterCore), **beams ≈ 1–2.**

**Verdict:** **"choose 5 weapons / 2 beams from a base set" is NOT satisfiable from existing content** —
there is no 6+-gun base catalog to choose 5 from, and only 1–2 beams (so "choose 2" = take all). The
"base set" is **not formally defined** (no `ContentTier=Base` weapon/beam designation confirmed) **and is
under-populated.** **DECISION (`PROPOSED — needs operator confirm`):** either **(a)** author more base
weapons/beams so choose-5/choose-2-from-M (M > N) is a real choice, or **(b)** reduce the starter to the
content that exists (e.g. choose from {PulseCarbine + Pistol + Rifle + Shotgun} = 4 guns, and 1–2 beams).
**No `M` is baked** — the count is a content+design decision, with the real basis = **~4 guns / 1–2 beams today.**

### 9.2 Base-mask id — **CONFIRMED: `AFL.Facemask.IroVisor`** (operator-confirmed 2026-06-22)
**Real on-disk inventory:** 33 `DA_AFL_Facemask_*` exist. **The base/default mask = `AFL.Facemask.IroVisor`**
(the Ironics Visor — `IRONICS_STORE_COLOR_SPEC` wires `AFL.Facemask.IroVisor → Cosmetic.Identity.IronicsVisor`,
and IRONICS is the free base identity). Granted free at first entry, **account-bound** (§8.4). The other 32
masks are priced extras (§9.3 → pricing SSOT). **LAW.**

### 9.3 Exact prices — **RESOLVED** → see `IRONICS_PRICING_SCARCITY_SSOT.md` (operator-confirmed 2026-06-22)
The category→rung mapping and the base-extras low-rung gap are **resolved + locked** in the dedicated
**Pricing & Scarcity SSOT**. In brief (that doc carries the cited curve + the rarity model):
- **Non-IRONICS identity → ARC** (23,000 V / $23; prestige → THUNDER BOLT $30).
- **Premium finish set / extra mask → SPARK** (10,000 V / 100,000 W / $10) — identities sit ~2× a finish.
- **Extra/unchosen base weapon or beam → the new `FLICKER` sub-Accessible rung** (10,000–25,000 W / $1–$2.50,
  Watts-only) — **closes the prior "no fitting low rung" gap.**
- **Limited editions** ride the **rarity ladder** Static→Singularity ($1 → $500; the $500 **1-of-1 is a
  bundle** SKU granting identity + mask + color set + weapon combo atomically). Never-discount ≥ Bolt;
  never-reissue all tiers.
- **Standing dependency:** the pricing *model* is law now; limited-edition *enforcement* (`MintedCount`,
  mint caps, bundle grant) rides the FOUNDATIONAL **persistence backend** (§10 #7 / §11, still open).

### 9.4 Still-open items (unchanged, carried)
- **`ServerGrantStarter`** (§4a) — first-entry grant path **does not exist** (§10).
- **Team-readability spec** — operator-confirmed **outline/rim + nameplate**; the team-layer color fix
  (§7-C4) **depends on it** — spec before removing the team body paint in team modes.
- **P2P gaps (§8.3/§8.5)** — atomic escrow · offer-locking · two-buyer concurrency · trade anti-fraud ·
  `ServerCreateListing/MakeOffer/AcceptOffer`. All Phase-3-gated.

---

## 10. COMPLETENESS AUDIT — the full player lifecycle (COVERED / GAP)

| # | Lifecycle stage | Status | Detail |
|---|---|---|---|
| 1 | **First launch → starter grant** | **GAP** | `ServerGrantStarter` doesn't exist (§4a). Idempotent + persisted across re-entry/new-device/re-install **requires persistence** (#7, stubbed). Today: session-only — a re-entry would re-fire or lose the grant. |
| 2 | **Starter choice (5 weapons / 2 beams)** | **GAP** | No front-end flow specifies *when/where* the player chooses, nor whether the choice is **re-selectable or permanent**. (Also blocked by §9.1 content.) |
| 3 | **Equip / swap** | **PARTIAL** | Finish swap **COVERED** (§4b). **Identity swap** (own multiple identities, switch active) = **GAP** — the selection chain is specified+proven for *finish*, not for *identity* (→ #8). |
| 4 | **Earn (Watts/Volts/Bolts → wallet)** | **PARTIAL** | Mechanism **COVERED** — economy §3 earn structure + `ServerEarnWatts/Volts`. **Cross-session persistence = GAP** (#7). |
| 5 | **Purchase** | **PARTIAL** | `ServerPurchaseCosmetic` atomic check-deduct **COVERED** (§4c). **Refund/error paths + insufficient-funds UX = GAP.** |
| 6 | **Trade (P2P)** | **GAP** | Entire §8 — two-party atomic path, escrow, offer-locking, concurrency, anti-fraud; listings/offers persistence. Phase-3-gated. |
| 7 | **Persistence (cross-cutting)** | **GAP — FOUNDATIONAL** | `IAFLCosmeticPersistence` is **stubbed `nullptr` → session-only** (ADR Decision 1). **Nothing is durable:** wallet balance, `OwnedCosmeticIds`, selections, starter-grant flag, the §8.4 bound-marker, trade listings. No backend today (PlayFab = Phase 3). **This blocks 1, 4, 6 and durable ownership entirely.** |
| 8 | **Identity-selection at spawn** | **GAP** | The two-ARIA bug: the pawn isn't receiving the selected identity. Chain: `Selection.CharacterId/TeamId` → possession → `CharacterPartMap` → robot BP. The **finish** path is proven; the **identity→pawn** application is breaking/unspecified (both default ARIA). Base=IRONICS (§5) can't render until this resolves. |
| 9 | **Else (not in any doc)** | **GAP** | Cross-platform entitlement backend (PlayFab, Phase 3) · store **catalog versioning** + **granted-item migration** when base sets change later · Battle-Pass/level **implementation** (gate is *spec'd* §4d, build status unknown) · front-end store/wallet/loadout **UI**. |

---

## 11. PRIORITIZED — "missing for complete + working"

**FOUNDATIONAL (blocks everything else — sequence first):**
1. **Persistence backend** (`IAFLCosmeticPersistence` impl) — durable wallet, `OwnedCosmeticIds`,
   selections, starter-grant flag, §8.4 bound-marker. The keystone under #1/#4/#6 and all durable ownership.
2. **`ServerGrantStarter`** (first-entry grant) — depends on the persisted grant-flag (#1).
3. **Identity-selection → pawn** (the two-ARIA fix) — base=IRONICS and any owned-identity swap need the
   identity selection to reach the pawn.
4. **Team-color-body defect removal + team-readability replacement** — the color bug that opened this
   thread (§7-C4 + the outline/nameplate spec, §9.4).

**FEATURE (real systems, after the foundation):**
5. Front-end flows — starter-choice UI, store UI, loadout/identity-swap UI, wallet UI.
6. **P2P trade subsystem** — escrow + offer/listing + `ServerCreateListing/MakeOffer/AcceptOffer`
   (Phase-3, behind persistence).
7. **Base weapon/beam catalog** — formally define the base set + **author enough content** for a real
   choose-5/choose-2 (§9.1).
8. Battle-Pass / level-progression **implementation** (the gate is spec'd; the system may not be built).

**POLISH (correctness/UX, last):**
9. Refund/error paths · insufficient-funds UX · trade anti-fraud (cooldown/value-display) · catalog
   versioning + granted-item migration · cross-platform entitlement.

---

## 12. Consistency confirmations
- **Ruling 1** (finish = sole color): ✅ §3 — all color from the finish; no baked/team color.
- **CONFIRM 1** (no team body coupling): ✅ §3/§7-C4 — team read off-body.
- **ADR Decision 1** (server-auth ownership): ✅ §4/§8 — every grant/purchase/equip/trade server-side.
- **Economy §0** (no randomization / **no cash-out**): ✅ §2 direct-only; §8 marketplace is in-game-
  currency-only, no cash-out.

---

*Finalized read-only 2026-06-22: operator approvals locked (§7-C1 record, §8.4 bound policy), TBDs
filled-from-disk or flagged `PROPOSED` (§9), full-lifecycle completeness audit + prioritized gap list
(§10–§11). **APPROVED SSOT** — the entitlement / identity-selection / store / trade fixes must satisfy
it. The §7-C1 doc-string corrections remain **pending-execute** (not applied to code). Items marked
`PROPOSED — needs operator confirm` (§9.1 base-set sizing, §9.2 base mask, §9.3 price-rung assignment)
are the only open decisions; everything else is law. No code, no build, nothing staged.*
