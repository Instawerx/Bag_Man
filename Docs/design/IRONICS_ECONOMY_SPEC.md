# IRONICS — Canonical Economy Spec (v1)

<!-- PRECEDENCE BLOCK -- keep at the top; update the commit line when this doc's CONTENT changes. -->
> **PRECEDENCE** (rule ruled 2026-08-24: newest economy doc wins; where it is silent, the prior doc fills the gap)
> - **This doc's last CONTENT commit:** `e7e6ae63` (2026-08-27) — a directory move is not a content change and must not be cited here.
> - **Authority on pricing:** `IRONICS_PRICING_SSOT.md` — Battle Pass is **$5/month, real money** (ruled 2026-08-27). No Volts, no Watts.
> - **Superseded:** **§4 (Battle Pass) is SUPERSEDED IN FULL** by `IRONICS_PRICING_SSOT.md` §6. The rest fills gaps the newer doc is silent on (e.g. Volt packs).

> **PRECEDENCE (ruled 2026-08-24):** the NEWEST economy doc is authoritative; where the newest is SILENT on a question, the prior doc governs. This doc last changed **2026-07-09** (`0c5c88c3`) — corrected 2026-08-27; the previously cited `1c072a49` (2026-08-05) is a directory move with zero line changes, and citing it made this doc look 27 days newer than it is. Today `IRONICS_PRICING_SSOT.md` (2026-08-19) is newest and `IRONICS_ECONOMY_SPEC.md` (2026-08-05) fills its gaps — e.g. Volt packs, on which the newer doc is silent. The rule exists so a later disagreement resolves without re-ruling.

> **This is the canonical economy model.** It was built from operator decisions +
> researched against current (June 2026) proven models (Valorant premium-direct,
> Fortnite battle-pass-spine, the lootbox→battle-pass legal shift). The peg is a
> hard invariant; tier *names* are operator content (proposed here, to be blessed).
> S-ECON-CAT's manifest carries these values. If this doc and the SSOT tracker ever
> disagree, reconcile deliberately — this doc is the economy's home.

---

## 0. Hard invariants (locked, do not drift)

PEG (exact integer math, balances stored as integer units, NEVER floats):
1 Volt = $0.001 · 10 Watts = 1 Volt · $1 = 1,000 Volts = 10,000 Watts.

NO CASH-OUT, EVER. One-way buy-in only. No path converts Watts/Volts back to
real money or out of the system. Breaking this = full compliance re-review.

NO RANDOMIZED ACQUISITION. No lootboxes/gacha. All purchases are direct,
known-item. (Research-validated: lootboxes are legally radioactive — Belgium ban,
NL ban, the $520M Epic/FTC settlement; battle pass is the transparent norm.)

Single global currency (Watts + Volts). IRONICS Bank / "IRONICS Coins" =
branding flavor over the one global currency, NOT per-team coins.

The bank / Hyperledger / Bolts-P2P / real-money-purchase layers remain
designed-but-gated, each needing legal sign-off before enable (see economy
GATE card in the SSOT tracker). This spec governs the in-game economy;
Phase-3 gated layers sit behind it.

---

## 1. Currency roles

| Currency | Type | Earned how | Spends on |
|---|---|---|---|
| Watts | Soft (earned) | Gameplay: per-match base + daily + weekly | Accessible-tier cosmetics + discounts |
| Volts | Hard (premium) | Bought with real money (gated layer) | Battle Pass + premium/prestige cosmetics |
| Bolts | P2P transfer | (gated, Phase 3, legal sign-off) | Player-to-player transfer (Zelle-like) |

Monetization spine: HYBRID — Battle Pass + direct item shop, no randomness.
Volts gate the Battle Pass and premium/prestige cosmetics (the proven legal
structure: premium currency gates the pass).
Watts buy the accessible tier + provide discounts (the earned, free-player path).
This mirrors what BOTH Valorant and Fortnite actually do (pass for lower tiers,
direct purchase for premium) and is the legally-clean path.

---

## 2. Cosmetic tier ladder (prices + PROPOSED brand names)

Prices map the proven Valorant ladder (~$10/$16/$23/$30) onto the peg. Tier NAMES
are operator content — proposed below in the Watts/Volts electrical vocabulary
(charge/voltage escalation), to be blessed/changed/replaced by the operator.

> ⚠️ **SUPERSEDED by `IRONICS_PRICING_SCARCITY_SSOT.md` §1.5 R2 (2026-07-08):** the SPARK/SURGE/ARC/THUNDER BOLT named rungs are replaced by the **cheap-first ladder** — Free · Impulse $0.99–1.99 · Standard $2.99–4.99 · Premium $7.99–14.99 · Grail $500. The peg + soft/hard split (below) are UNCHANGED; the table is retained as history.

| Tier | Volts (hard) | Watts (soft) | ≈USD | Proposed brand name | Notes |
|---|---|---|---|---|---|
| Accessible | 10,000 V | 100,000 W | $10 | SPARK | Watts-buyable (the free-player path) |
| Standard | 16,000 V | — | $16 | SURGE | Volts; Watts-discountable |
| Premium | 23,000 V | — | $23 | ARC | Volts only (the ~3-week generous target) |
| Prestige | 30,000 V | — | $30 | THUNDER BOLT | Volts only; top prestige |

(Proposed name ladder reads as rising electrical intensity: SPARK → SURGE → ARC →
OVERVOLT. Operator to bless or swap. Identity items — Teams/Characters — and the
free base sets sit outside this paid ladder: founding teams + free Character base =
GrantedFree.)

Watts↔tier note: only the Accessible (SPARK) tier is directly Watts-buyable, at
100,000 Watts (= 10,000 Volts at peg). Higher tiers are Volts (real-money) — Watts
can apply discounts to them but not fully purchase them. This is the soft/hard
split that keeps the model legally clean and the premium tiers meaningful.

---

## 3. Earn structure (Watts) — the locked rate, delivered as STRUCTURE not flat

> **R4 reconcile (`IRONICS_PRICING_SCARCITY_SSOT.md` §1.5, 2026-07-08):** this earn structure is **UNCHANGED**. Under the new cheap-first ladder the base wardrobe = **~2.5 matches** (was ~25 at SPARK $10); a Standard signature ≈ 7.5 matches. **Price lever only — no earn inflation.**

Calibration target: a regular player (the anchor: ~5 sessions/week ×
~4 matches = ~20 matches/week) earns a Premium (ARC, ~230,000 Watts-equiv)
cosmetic in ~3 weeks → ~60 matches → ~4,000 Watts/match-equivalent baseline.
But delivered as a structured earn (proven retention shape), NOT a flat per-match
number — the flat ~4,000 is the calibration target the structure sums to:

| Source | Watts | Cadence | Purpose |
|---|---|---|---|
| Match base | ~1,500 (loss/participation) → ~2,500 (win) | every match | skill + outcome matter; losing still pays |
| Daily first-win | ~3,000 | once/day | the "come back today" hook |
| Daily quests (2-3) | ~1,500 each | daily | varied play, return reason |
| Weekly challenges | ~10,000-15,000 pool | weekly | "play across the week" retention layer |

Weekly sum check (regular player, ~20 matches):
Match base: ~20 × ~2,000 avg = ~40,000
Daily first-win: ~5 days × 3,000 = ~15,000
Daily quests: ~5 days × ~3,500 = ~17,500
Weekly challenges: ~12,000
≈ ~84,500 Watts/week → ~3 weeks ≈ ~250,000 → a Premium (ARC) cosmetic. ✓
(Lands inside the generous 2-4 week band; ~4,200 Watts/match-equivalent.)

Design principles (proven): reward variety not just kills (saves/reboosts/loot/
extract all earn — ties to P-SCORING); daily-gated chunk so showing up matters more
than grinding; skill rewarded (win > loss) but losing still pays so it never feels
punishing. The earn STRUCTURE is the engagement; the flat rate is just its sum.

### 3a. COMBAT-LOOT earn source (added 2026-06-16 — finalized assignment)

A retrievable kill/dismember loot source, layered onto the §3 earn structure (it is
"loot" in the "reward variety not just kills" principle above). A severed body PART
persists as a retrievable item; only an OPPOSING player retrieving it banks Watts —
the OWNER retrieving their own part REATTACHES it (no grant; you do not profit from
your own body).

| Combat-loot item | Watts (to the ENEMY retriever) | Notes |
|---|---|---|
| Head | **160** | Severed-head retrieval. The prize -- head>>arm>leg (the v7 ratio). |
| Arm (L/R) | **27** | Per arm (left/right same value; the zone is L/R-specific for reattach). |
| Leg (L/R) | **16** | Per leg. The smallest -- legs drop first under fire (v7 smallest-first), the head clings. |

> **v7 RE-PEG (2026-06-19):** the flat limb=20 (head/8) is superseded by **differentiated arm=27 / leg=16** --
> the operator's 1500:250:150 (head:arm:leg) ratios re-scaled to the proven head=160 peg (integer, 160>27>16, no
> collapse). Arm>leg makes the smallest-first drop order meaningful (legs are the cheap thing you bleed first).
> Each part is now an INDIVISIBLE TOKEN carrying this FIXED value (the v7 PART-TOKEN model -- see
> IRONICS_LOOT_CARRY_MODEL.md STEP 2F); value is summed at extraction, not banked at collect.

- GRANT MECHANISM: `UAFLWalletComponent::EarnWattsAuthority(<value>, "head-loot" | "limb-loot")`
  on the GRABBER's wallet, server-authority. OWNER self-retrieve = reattach via
  `UAFLDismemberComponent::RestoreZone(<zone>)`, **NO Watts**.
- VALUES STORED on the `DA_AFL_DismemberZones` zone rows (`LootWatts` field): Head row = 160,
  each limb row = 20. The DA carries the combat-loot economy; head/8 is visible in the data.
- CALIBRATION BASIS (recorded honestly — head=160/limb=20 is a TUNE-AT-PLAYTEST STARTING value,
  NOT a measured final): calibrated against **~4,000 Watts per MATCH (whole 4-player lobby pool)**.
  At an **ESTIMATED ~10 head-equivalents/match enemy-collected** (8 heads + 16 limbs; **NO
  telemetry — tune at playtest**), combat-loot ≈ 1,600 W/match ≈ **~40% of the per-match budget at
  the mid estimate** — a meaningful second pillar, NOT dominant. **The ~10-head-equivalent collect
  count is the open calibration question; first tune at playtest** (if real collects are ~half,
  combat-loot ≈20%; if double, ≈80%).
- Ties to the §0 invariant carve-out: this is FREE/EARNED gameplay reward, not purchased chance —
  fully clear of the NO-PAID-RANDOMIZED-ACQUISITION line.
- **Economy-integrity cross-ref (2026-07-09):** because the **enemy-collect** grant is an earn SOURCE, it is
  an anti-fraud attack surface (loot-feeding / self-dealing to a colluding **enemy-team** alt to manufacture
  Watts). The **game-wide anti-collusion / anti-fraud system** that guards the earn layer is designed in
  `IRONICS_MATCH_STAKING_SSOT.md §3D` (policy-as-law; detection backend-gated). The earn-layer guard belongs
  to **economy integrity**, not staking alone — illegal Watts stacking devalues the currency + grail scarcity.

---

## 4. Battle Pass

> # ⛔ SECTION 4 IS SUPERSEDED IN FULL — DO NOT SCOPE FROM IT
>
> **RULED 2026-08-27: the Battle Pass is $5/month, REAL MONEY. No Volts. No Watts.**
> `IRONICS_PRICING_SSOT.md` §6 governs. This section's pricing and its economics are dead.
>
> The ~8,000 V figure below is **no longer the pass price** and must not be quoted. Under the
> precedence rule (ruled 2026-08-24: newest economy doc wins, prior fills gaps),
> `IRONICS_PRICING_SSOT.md` governs pricing. Verified 2026-08-27 by last CONTENT commit rather
> than by file date: PRICING_SSOT `a61d9e43` (2026-08-19, +13/−4) against this doc's `0c5c88c3`
> (2026-07-09, +5/−0). This doc's own header cites `1c072a49` (2026-08-05), but that commit is
> `Docs/{ => design}/` — a directory move with **zero line changes** — so this doc is 41 days
> older than its header implies, not 14.
>
> **The ruled price is the subscription: $5/mo · $30/yr · $10/quarter on annual commit**, live in
> the catalog as `AFL.League.Monthly` 5,000 V · `AFL.League.Quarterly` 10,000 V ·
> `AFL.League.Annual` 30,000 V (verified against `Docs/reference/catalog-export.json`).
>
> **The subscription and the Battle Pass are ONE product** — the subscription grants the current
> season's pass. Ruled 2026-08-27.
>
> **NAMING (ruled 2026-08-27):** `LeaguePlay` is the **free, unstaked** match tier. The **paid tier
> is the BATTLE PASS**. Resolved by naming the paid product, not by renaming the free one.
>
> ⚠ **The self-sustaining arithmetic below does NOT survive this change — see §4.1.** Everything
> else in this section (tier count, free track, no paid skips) stands.

Price: ~8,000 Volts (≈$8) — matches the proven ~$8 pass price point.  **← SUPERSEDED, see above.**
Structure: EXACTLY self-sustaining — complete the premium track, earn back
exactly 8,000 Volts (enough for the next season's pass, no surplus).
Rationale (operator-locked): launching exactly-self-sustaining from day one
sets the expectation cleanly with no later takeaway. Fortnite's 2026 backlash
was about removing a margin players had grown used to — IRONICS never creates
that expectation, so never breaks it. The value proposition is "complete the
pass, get the cosmetics" — not currency accumulation.
Free track: grants a meaningful Watts pool + a baseline cosmetic to every
player each season (the proven free-track hook; à la Fortnite's free-track grant).
Tiers: ~100 tiers, free + premium track, unlocked by play (XP/challenges).
No paid tier-skips at launch beyond a standard bundle (pass + N tiers) — keep
it clean; revisit later.

---

### 4.1 · CLOSED — self-sustaining is dead, and so is the Volts pass

> **RULED 2026-08-27. NOT AN OPEN QUESTION. Do not re-derive this.**
>
> The Battle Pass is **$5/month, real money**. It is not bought with Volts, so there is no
> "earn back the pass" arithmetic to balance and **"exactly self-sustaining" no longer applies at
> all**. The trade-off table below is retained only as a record of why the old model was abandoned.
>
> This section previously read as an OPEN economy question. That framing caused the same
> reconciliation to be re-run three times across sessions, each time reaching the same dead end,
> because a stale price sat in a doc that read as current.

§4 locked "exactly self-sustaining" as an operator ruling with a stated rationale: complete the
premium track, earn back exactly the next pass, so IRONICS never creates a currency-surplus
expectation it would later have to remove — explicitly framed against the 2026 Fortnite backlash,
where the harm was the *takeaway*, not the margin.

That design is **price-coupled**, and the price changed:

| | old (§4) | new (`PRICING_SSOT` §6) |
|---|---|---|
| Cost | ~8,000 V once per season | 5,000 V/mo · 10,000 V/qtr · 30,000 V/yr |
| Cadence | seasonal | recurring |
| Season length | — | ~9–13 weeks (`ECON §7`, `LEAGUE_ADVANCEMENT §3`) |
| **Cost per season at monthly** | **~8,000 V** | **~15,000–16,000 V** |

So the premium track would have to pay back roughly **twice** what it was tuned to, and it must now
do so against a *recurring* charge rather than a single seasonal one. Three consequences, none of
them decided here:

1. **Annual breaks it in the other direction.** 30,000 V/yr across ~4 seasons is ~7,500 V/season —
   *below* the old 8,000. A track that returns 8,000 would make the annual plan net-positive, which
   is the currency surplus §4 exists to avoid.
2. **Self-sustaining is now plan-dependent.** The same track is a loss on monthly and a profit on
   annual. "Exactly self-sustaining" no longer names one number.
3. **The anti-backlash rationale is the load-bearing part.** Whatever is chosen, the risk §4 was
   guarding against is *creating an expectation and later removing it*. Setting the payout too
   generously now is the same trap one step later.

**Not resolved here.** Four coherent options exist — retune the payout per plan, decouple the pass
economy from the subscription price, keep the pass as a separate ~8,000 V purchase that the
subscription merely *grants*, or drop exact self-sustainment as a goal — and choosing among them is
an economy ruling, not a documentation fix.

---

## 5. Volts purchase packs (the real-money layer — GATED, Phase 3, legal sign-off)

When the real-money layer is enabled (post legal sign-off), Volts sell in packs at
the peg, with the proven "larger pack = better value" bonus structure:

| Pack | Volts | ≈USD (at peg) | Bonus |
|---|---|---|---|
| Starter | 1,000 | $1 | — |
| Small | 5,000 | $5 | — |
| Standard | 10,000 + bonus | $10 | small bonus Volts |
| Large | 25,000 + bonus | $25 | better bonus |
| Mega | 50,000 + bonus | $50 | best value |

(Exact bonus amounts tuned at enable-time. These are designed now, sold only after
the real-money-purchase gated layer clears legal review.)

---

## 6. What S-ECON-CAT's manifest carries (the build hook)

Each catalog entry carries: `CosmeticId` (immutable FName key) → asset (soft ref),
Tier (Accessible/Standard/Premium/Prestige → SPARK/SURGE/ARC/THUNDER BOLT),
Price (Volts + optional Watts), CurrencyType (Watts-buyable? Volts-only?
Watts-discountable?), AcquisitionMethod (Direct / BattlePass / GrantedFree),
availability / CollectionId. The numbers above ARE those field values — the
manifest is no longer carrying placeholder/free stubs; it carries this model.

---

## 7. Still open / tune-at-build

Tier NAMES — operator to bless SPARK/SURGE/ARC/THUNDER BOLT (Locked)
Exact daily/weekly Watts values — calibrate in playtest against the ~4,000/
match-equivalent target; the structure is locked, the exact splits tune.
Volts pack bonus amounts — tuned at real-money-layer enable.
Season length (drives pass pacing + the weekly-challenge math) — assumed ~9-13
weeks (proven season cadence); confirm at P-MATCH/live-ops. Daily Drops and Challenges Rewards Top 3 players most hours played in a day.
Watts discount depth on Volts tiers — how much can Watts knock off a premium
item; tune in playtest.

(PEG (exact integer math, balances stored as integer units, NEVER floats):
1 Volt = $0.001 · 10 Watts = 1 Volt · $1 = 1,000 Volts = 10,000 Watts.) APPLIED

---

*Materialized 2026-06-16 from the operator's authoritative paste (faithful, not paraphrased) so the
SSOT tracker's `IRONICS_ECONOMY_SPEC.md` reference (the live economy CUR/TIER/EARN/GATE cards name
this as "the economy's home") resolves to a real file. The combat-loot earn source (§3a) is the
first finalized assignment appended to this materialized doc; it is mirrored into the tracker EARN
card per this doc's header ("if this doc and the SSOT tracker ever disagree, reconcile deliberately").*
