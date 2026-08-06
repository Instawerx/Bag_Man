# IRONICS — Match Types, Matchmaking & Staking SSOT

> **Status: SCOPING / DESIGN PASS — v0.6 (2026-07-09).** The owed **driver #3** doc
> (`IRONICS_MARKETPLACE_MASTER_ARCHITECTURE.md:143`: *"Matchmaking & Game Types — PokerStars-inspired…
> gates matches by rank/type — needs a matchmaking/game-types doc"*). **Design deliverable only** — no
> game-system code, no catalog/asset/infra writes, no matchmaking/staking mechanism is built here.
> Refine-and-finish is iterative.
>
> **Grounds in (read + reconciled):** `IRONICS_MAP_MODE_SPEC.md` (the EXISTING mode ladder + win
> conditions — the match substrate) · `IRONICS_ECONOMY_SPEC.md` + `IRONICS_PRICING_SCARCITY_SSOT.md`
> (currencies = Watts/Volts, peg, **the §0 NO-CASH-OUT doctrine**) · `IRONICS_LOOT_SYSTEM_DESIGN.md` +
> `IRONICS_LOOT_CARRY_MODEL.md` (the combat-loot **bounty** substrate) · `IRONICS_LEAGUE_ADVANCEMENT_SSOT.md`
> (the ranked/MMR sibling this keys against) · `BAG_MAN_MASTER_BUILD_v2.0.md` (the PlayFab→Lambda→GameLift
> matchmaking backend) · `BAG_MAN_LIVE_TRACKER.html` (MATCH-SPINE-C1, P-MATCH).
>
> **The one law that governs everything below:** `IRONICS_ECONOMY_SPEC.md §0` — *"NO CASH-OUT, EVER.
> One-way buy-in only. No path converts Watts/Volts back to real money."* **Staking is the poker
> STRUCTURE (buy-in → pool → payout) applied to non-cashable in-game currency — NOT real-money gambling.**
> Any real-money / custody wagering stays Phase-3 legal-gated and is **out of scope here** (see flag #1).
>
> **RULINGS RECORDED AS LAW (2026-07-09, operator):** **R1 — HOUSE RAKE** (flag #2 → ✅ RULED): rake off the
> top of the **TOTAL pool**, tiered by the pool's **Volt-equivalent** (Watts convert at peg **10 W = 1 V** so
> the bracket is currency-agnostic) — **pool ≤ 500 V-equiv → 5% · pool ≥ 501 V-equiv → 10%** (§3B). **R2 —
> NO CASH-OUT** (flag #1 → ✅ RULED): staking is poker STRUCTURE on **non-cashable, in-game-spend-only**
> Watts/Volts (inherits `ECON §0`); winnings never convert to real money — the real-money / custody tier
> stays Phase-3 legal-gated, deferred, not designed. **v0.3 — STAKE-TIER LADDER** (flag #3 → ✅ RULED,
> structure): the micro→high ladder `AFL.Stake.{Trickle·Ante·Live·Main·HighVolt·Nosebleed}` — per-seat
> 100/500/2,500/10,000/50,000/250,000 V-equiv (~5× geometric, Watts floor → Volts top), §3B.1; the specific
> values are design-proposals with sub-decisions #3a–#3d flagged. **v0.4 — GAME-WIDE ECONOMY-INTEGRITY /
> ANTI-COLLUSION** (flag #6 → ✅ RULED, system designed): one `AFL.Integrity.*` layer over **free AND staked**
> play — prohibited-behavior catalog + signals + a **T0–T3 enforcement ladder** + clawback-of-spent model
> (§3D); **policy is law now, the detection engine is backend-gated** (a recorded B2 integrity work-item);
> sub-decisions #6a–#6c flagged. **v0.5 — INTEGRITY LIMITS SET** (#6a + #6b → ✅ RULED, research-grounded +
> sourced, §3D.1): the **penalty-ladder thresholds** (offense/confidence triggers per T0–T3, 30d→90d→perm
> stake-bans, 14-day-before-permanent, 90-day window / 12-month decay, human review before T2/T3, 14-day
> appeal) and the **lobby rules** (1 seat/account, same-party off opposing sides, same-device flag-and-hold,
> host gate ≥7d + ≥25 matches + no active flag), each cited to major-platform practice (poker / Riot / Valve /
> Valorant). **v0.6 — LAST FLAGS CLOSED:** **6c** grail clawback = **admin master control** (privileged +
> audited; E1 preserved); **#4** tournaments = **both player-run + special admin** events; **#5** ranked
> **CAN be staked** behind the **rank-not-buyable firewall** (rank/MMR on outcome+skill only, never on stake;
> soft overlay on the ranked pool, no thinning; precedent: chess rated+prize / poker / FACEIT). **ZERO open
> flags remain — the staking SSOT is fully closed.**

---

## 0. WHAT THIS DOC IS

Driver #3 has three parts; two exist and one is greenfield:
- **MATCH TYPES** — largely DESIGNED already (`IRONICS_MAP_MODE_SPEC.md` v2). This doc **references** that as
  the authority and **adds** the asymmetric **1-v-many** format + the **stakeable-set** overlay.
- **MATCHMAKING** — the transport backend EXISTS (PlayFab tickets → Lambda → GameLift). This doc designs the
  **queue model** (casual / ranked / staked), stake-tier sorting, and the skill × stake interaction.
- **STAKING** — **fully greenfield.** The PokerStars-inspired player-initiated economy: buy-in → prize pool →
  payout, in Watts/Volts, under the no-cash-out law.

---

## 1. WHAT ALREADY EXISTS — the audit (cited)

### 1.1 Match structure + types — DESIGNED (and partly built)
- **The mode ladder** (`IRONICS_MAP_MODE_SPEC.md §1`, RESOLVED v2): **Arena PvP** {1v1, 2v2, 3v3, 4v4} —
  round-based **best-of**, win-by-**eliminate-OR-extract** (2v2–4v4 = first-to-7 = **best-of-13**), mirror
  maps, **ranked (skill core)**; **Team** {5v5–8v8} — objective (control/hardpoint/payload/team-extraction-
  threshold), ranked; **Battle Royale / Extraction** {18, 36} — placement + banked-extraction hybrid, ranked
  (separate pool). Plus **Offline** (bot fill, no ticket) and a **Tournament layer** — *"bracketing layered
  on whichever certified sizes; not a distinct map archetype"* (`:31`). **Shrink** party mode {4v4–8v8}.
- **The universal extraction primitive** (`:44-55`): fight → drop energy → collect → risk extraction → cash
  out; payout follows risk. **Built spine:** `UAFLMatchPhaseComponent` (Warmup→Active→Extraction Window→End,
  MATCH-SPINE-C1) + per-player StatTag scoring (elimination/assists, K/D/A).
- **10-map tiered roster** (A–D + Shrink), greybox-telemetry loop (`:89-104`, §6).

### 1.2 Matchmaking transport — the BACKEND EXISTS
- **PlayFab matchmaking is polling-only** (no `OnMatchFound` webhook): client → PlayFab `Match/GetMatch`
  queue → **API Gateway (+HMAC) → Lambda → GameLift `StartGameSessionPlacement`** (queue-based) → poll →
  IP:Port (`BAG_MAN_MASTER_BUILD_v2.0.md:498`, `:70`; the AFL-1100 tentpole in the `Bag_Man_Backend` repo —
  the same infra the B2 spine extends). The dedicated server reads context from GameLift
  `onStartGameSession` (`MatchmakerData` JSON = players + attributes + **team assignments**, `:511`).
- **EOS = voice / friends / parties / EAC ONLY — EOS matchmaking is explicitly out of scope** (`:491`,
  `:523`). Party rosters travel with the PlayFab ticket (AFL-1104/1204). Module: `AFLOnline` (P3 S1).
- **What this gives staking:** a working ticket → placement pipeline + `MatchmakerData` attributes to carry
  **stake tier + buy-in** into the match, and PlayFab currency (Watts/Volts) already live for the escrow.

### 1.3 The ranked/MMR sibling — DEFINED (League v0.2)
- `IRONICS_LEAGUE_ADVANCEMENT_SSOT.md`: rank = an earn-and-gate driver; **Glicko-2 MMR** (AFL-2201) drives
  matchmaking fairness; **rank is NOT buyable** (the firewall this doc must not break). This doc **consumes**
  that MMR; the League doc names *this* doc as its matchmaking owner (§5 cross-reference).

### 1.4 What does NOT exist — the genuine gap
- **STAKING — zero prior design.** A full-corpus sweep for stake/wager/buy-in/entry/tournament/prize-pool/
  payout/rake/house-cut/PokerStars found **nothing** (the only "stakes" hit is the *"extraction stakes"*
  tagline, `MASTER_BUILD:4`). Greenfield.
- **The queue MODEL** — casual vs ranked vs staked queues, stake-tier sorting, lobby/"table" creation: not
  designed (only the transport pipeline exists).
- **The 1-v-many asymmetric format** — named in the operator's spectrum, absent from the mode ladder.

---

## 2. RESEARCH-GROUNDED PRINCIPLES (poker structure → extraction shooter)

The operator's "PokerStars-inspired" = adopt poker's **table/stake/tournament STRUCTURE**, not its cards.
Grounded in real poker economics + skill-competition-law best practice, under the no-cash-out law.

### 2.1 The poker structures that convert (each mapped to our loop)
| Poker structure | Our conversion |
|---|---|
| **Buy-in → prize pool → payout curve** | Each seat pays an entry (Watts/Volts) into an escrowed pool; the match result distributes it. Winner-take-all (heads-up/small) or top-heavy (tournaments). |
| **Sit-n-Go (SNG)** — table starts when it fills | A **staked lobby that launches when its N seats fill** — single match, single "table" (any Arena/Team size). |
| **MTT (multi-table tournament)** — many tables, eliminations, final table | A **staked bracket/series** across many matches → maps naturally onto the **BR pool** (many players → collapse → final) OR a bracket over Arena matches. |
| **Heads-up** — 1v1 | **Staked 1v1 duel** (best-of, `MAP_MODE §1.1`). |
| **Bounty** — a price on each player; busting them pays it | **Native fit:** our **combat-loot head-bounty** already prices heads (head 160 / limb, `LSD §3a`). A **bounty match** puts a Watts bounty on each seat; eliminating/extracting them pays their bounty. The dismember loop IS a bounty system. |
| **Rake / house-cut** — the room takes a % | An optional **house-cut** on the pool → a **Watts SINK** (economy hygiene: removes earned currency, counters inflation). Flag #2. |
| **Blind escalation** — blinds rise to force action | Our **collapsing extraction zone + match clock** already force convergence (`MAP_MODE §10.2` strand) — the blind-timer analogue is **already built**. |
| **Stake tiers** — micro/low/mid/high tables | `AFL.Stake.<Tier>` queues sort by **commitment** (and implicitly skill — higher stakes attract better players); matchmaking-by-stake. |

### 2.2 Principles we ADOPT
1. **Entry buys a SEAT + a share of the POOL — NEVER gameplay power.** Every player in a staked match has
   identical gameplay. Stake changes *what's at risk*, never *how strong you are*. (Preserves `ECON §0` no-P2W.)
2. **Skill decides the pot; the pot never decides skill.** Staking is orthogonal to rank — a staked match can
   be unranked, and ranked never keys off stake (§5, flag #5). This keeps the League *rank-not-buyable* firewall.
3. **Transparent, deterministic economics.** Buy-in, pool, rake, and payout curve are shown up front, fixed
   before the match, server-authoritative, escrowed. No hidden odds, no variable-ratio hooks.
4. **Watts-first (the clean tier).** The primary staking currency is **Watts** (earned, soft) — the
   structurally-safest (see 2.4). Volts-staking is a flagged extension (2.4 / flag #1).
5. **Stake tiers as skill/commitment sorting.** Higher-stake queues self-select for committed/skilled players
   — a natural second matchmaking axis alongside MMR.

### 2.3 Anti-patterns we REJECT
- **Pay-to-win via stake** — paying more for gameplay advantage. Rejected by principle 1 (entry = seat only).
- **Whale-crushing / farming the poor** — high-rollers hunting low-skill players for their stake. Mitigated:
  stake tiers + MMR sorting *within* a tier; and no-cash-out means a "loss" is in-game currency only.
- **Addiction-loop / gambling-harm design** — near-miss, loss-chasing, variable-ratio, opaque odds. Rejected:
  no randomness (skill-only outcomes), transparent economics, and **the no-cash-out law removes the financial-
  harm vector entirely** (you cannot lose money, only non-cashable tokens). Optional session/spend guards are a
  lightweight later refinement, folded under the R2 no-cash-out ruling (deferred, not decided).
- **Real-money wagering with cash-out** — that is licensed gambling. **Forbidden by `ECON §0`**; any real-money
  path stays Phase-3 legal-gated and out of this design (flag #1).

### 2.4 The legal line (why Watts-first, and the big flag)
Skill-based competition for a **non-cashable** in-game prize is structurally NOT gambling (no monetary prize
to redeem; outcomes are skill, not chance). **Watts** (earned) staking is the cleanest expression of this.
**Volts** carry a real-money *acquisition* path (`ECON §1`), but that path is itself the pre-existing Phase-3
legal gate; **staking already-owned Volts stays inside the in-game economy** and is covered by the R2
no-cash-out ruling (§6 flag #1 → RULED). **Real-money direct wagering is never in scope** (Phase-3
legal-gated). The design keeps the safe answer — non-cashable, in-game-only — as the law; the only real-money
surface (buying Volts) remains gated exactly as it already was.

---

## 3. THE DESIGNS

### 3A. Match-type spectrum (MAP_MODE is the authority; this adds two things)
- **Authoritative modes:** defer to `IRONICS_MAP_MODE_SPEC.md §1` (Arena 1v1–4v4 · Team 5v5–8v8 · BR 18/36 ·
  Shrink · Offline · Tournament). This doc does **not** re-spec them — it tags each for staking (below).
- **NEW — 1-v-many (asymmetric "Mark" / bounty-target mode).** One high-value target vs many hunters. The
  "1" is buffed (reuses the **built Shrink "Grown" state**, `MAP_MODE §8.1` — bigger/stronger/more-health),
  carrying a large bounty; the "many" compete to eliminate/extract the Mark (and each other). Win = bust the
  Mark **or** the Mark survives/extracts to the clock. Keys off the **existing eliminate-or-extract**
  substrate; scoring off the built StatTags. Staking-native (the Mark seat = a high-buy-in / high-payout seat;
  hunters split the Mark's bounty on the bust). `AFL.MatchType.Mark` (proposed).
- **The STAKEABLE SET (the overlay this doc owns):** which modes accept a stake —
  | Mode | Casual | Ranked | **Stakeable** | Poker analogue |
  |---|---|---|---|---|
  | 1v1 duel | ✓ | ✓ | ✓ | heads-up |
  | 2v2–4v4 Arena | ✓ | ✓ | ✓ (team-pooled) | SNG (single table) |
  | 5v5–8v8 Team | ✓ | ✓ | ✓ (team-pooled) | SNG (team) |
  | BR 18/36 | ✓ | ✓ (sep. pool) | ✓ | MTT-shaped |
  | 1-v-many (Mark) | ✓ | flag | ✓ | bounty |
  | Tournament layer | — | flag | ✓ | MTT / bounty-MTT |
  | Shrink party | ✓ | ✗ | flag | — |

### 3B. Staking economy (the greenfield core)
- **Player-initiated.** An initiator creates a staked lobby, choosing: mode/size, **currency** (Watts | Volts-
  behind-legal), **buy-in B**, and payout shape. It launches SNG-style when seats fill (or at a scheduled time
  for tournaments — flag #4).
- **Escrow → pool → payout (server-authoritative, atomic).** On join, each seat's buy-in is **escrowed**
  (deducted + held) — reusing the **B2 atomic deduct/refund pattern** (`/purchase-bundle`: conditional
  deduct, refund-on-failure, never strand). Pool = `N × B`. On result, the pool (minus rake) pays out per the
  curve; on cancel/no-fill, **full refund** (the B2 refund path). **No cash-out — payouts are Watts/Volts.**
- **Payout curves (proposed defaults; tunable):**
  - *Heads-up / small SNG:* **winner-take-all** (pool − rake → winner / winning team, split by contribution).
  - *BR / MTT:* **top-heavy curve** (~top 15% paid, steep — poker-standard), placement-ranked (`MAP_MODE §1.1`
    BR = placement first, banked value tiebreaker).
  - *Bounty / Mark:* **per-elimination bounty** (bust a seat → claim its bounty from the pool) + a finish bonus.
    Directly reuses the combat-loot head/limb values as the bounty unit.
- **House-cut / rake — ✅ RULED (R1, law).** The house takes a rake **off the top of the TOTAL prize pool**
  (sum of all buy-ins), standard poker structure; the remainder is the payout pool. The rate is **tiered by
  the pool's VOLT-EQUIVALENT** — currency-agnostic, since Watts stakes convert at the peg (**10 W = 1 V**) to
  set the bracket, so it cannot be gamed by currency choice. **Pool ≤ 500 V-equiv → 5% · pool ≥ 501 V-equiv →
  10%.** The bracket keys off the **total pool**, not per-player buy-in. *Worked example:* a 4-seat match at
  **200 V each → 800 V pool** → ≥ 501 V-equiv → **10% rake (80 V)** → **720 V payout pool.** *(Also a currency
  sink — rake removes earned currency, aiding economy hygiene. Supersedes the v0.1 "0% Watts" proposal.)*
- **Stake tiers** `AFL.Stake.<Tier>` — the buy-in ladder + queue-sort axis, **✅ RULED (§3B.1 below).**
- **Naming/ids (ADR `AFL.<Type>.<Name>` doctrine):** `AFL.MatchType.<Name>`, `AFL.Stake.<Tier>`, `AFL.Format.
  <SNG|MTT|Bounty|HeadsUp>`. Stake/pool/escrow *state* is entry metadata on the match record, never in an id.

### 3B.1 Stake-tier ladder — ✅ RULED (flag #3; structure ruled, specific values design-proposed)
Real poker stakes run **micro → low → mid → high → nosebleed**, spaced **geometrically** (each tier a multiple
of the last, not linear), **dense at the bottom and thinning upward** — an **accessibility floor + an
aspirational climb**, and a skill/commitment sorter. Translated to our Volt-equivalent (peg **10 W = 1 V**;
no cash-out, R2):

| `AFL.Stake.<Tier>` | Poker analogue | Per-seat buy-in (V-equiv) | ≈USD (peg ref, NOT cash) | Denomination | Note |
|---|---|---|---|---|---|
| **Trickle** | micro | **100 V** (1,000 W) | $0.10 | **Watts** | low-friction floor (~¼ match's earn) |
| **Ante** | low | **500 V** (5,000 W) | $0.50 | **Watts** | the on-ramp (~1¼ matches' earn) |
| **Live** | low-mid | **2,500 V** | $2.50 | **Watts** | last comfortably Watts-earned tier |
| **Main** | mid | **10,000 V** | $10 | **Volts** | the Watts→Volts crossover |
| **HighVolt** | high | **50,000 V** | $50 | **Volts** | high stakes |
| **Nosebleed** | nosebleed | **250,000 V** | $250 | **Volts** | ceiling (proposed) |

- **Geometric spacing ~5×** (100 → 500 → 2,500 → 10,000 → 50,000 → 250,000) — a clean legible climb; dense
  micro floor, thinning top (matches real poker population + the aspiration curve). Six tiers (best-practice band).
- **Denomination:** Trickle/Ante/Live are **Watts-friendly** (earnable — a new player earns ~4,000 W/match, so
  the Trickle floor ≈ ¼ match); Main/HighVolt/Nosebleed are **Volts-territory** (premium). All stake in the
  common **Volt-equivalent** (Watts convert at 10 W = 1 V) so the tier is currency-agnostic (the R1 pattern).
- **Rake-break (R1) alignment:** the 5%/10% break is on the **pool** (≤ 500 V → 5%). Across the ladder that
  break falls **inside the Trickle tier** — a small-field Trickle match (≤ 5 seats × 100 V ≤ 500 V pool) pays
  **5%**; every other tier, and large-field Trickle, pays **10%**. So **5% is effectively "the micro floor in
  small fields."** Whether to *align* the break to the tier boundary (a flat 5% Trickle tier regardless of
  field) or keep it strictly pool-based (current R1) is a **sub-flag (§6 #3a)** — not forced here.

**Worked examples (4-seat SNG; pool = 4 × buy-in; rake per R1):**

| Tier | 4-seat pool | Rake bracket | Rake | Payout pool |
|---|---|---|---|---|
| Trickle | 400 V | ≤ 500 → **5%** | 20 V | **380 V** |
| Ante | 2,000 V | ≥ 501 → 10% | 200 V | 1,800 V |
| Live | 10,000 V | 10% | 1,000 V | 9,000 V |
| Main | 40,000 V | 10% | 4,000 V | 36,000 V |
| HighVolt | 200,000 V | 10% | 20,000 V | 180,000 V |
| Nosebleed | 1,000,000 V | 10% | 100,000 V | 900,000 V |

**Recorded as working law; these SUB-DECISIONS are design-proposals flagged for operator confirmation (§6 #3):**
tier **count** (6), the exact **cutoffs** + ~5× spacing, the **Watts/Volts denomination split** (at Main), the
**top ceiling** (Nosebleed 250,000 V — cap, or add an invite-only apex above?), and the **rake-break alignment**
(#3a). The ladder **shape + approach** is ruled; the exact numbers await confirmation.

### 3C. Matchmaking (queue model on the existing transport)
- **Three queue classes** (all ride the existing PlayFab-ticket → Lambda → GameLift pipeline; the class + stake
  tier ride in the ticket attributes / `MatchmakerData`):
  1. **Casual** — free, MMR-loose, fast fill. The default free-player path (normal Watts earn, no entry).
  2. **Ranked** — **strict MMR bands** (League AFL-2205); an **optional stake overlay** may ride a ranked match
     (#5), but **rank/MMR move on outcome + skill only, never on stake** (the firewall).
  3. **Staked** — entry required, sorted by `AFL.Stake.<Tier>` **and** MMR within the tier (fairness inside a
     stake band). "Tables" = staked lobbies (SNG) or scheduled tournaments (MTT).
- **Skill × stake:** MMR is the fairness axis *within* a queue; stake is a commitment/sorting axis *across*
  staked lobbies. They compose (a High-stake lobby still MMR-matches its seats) but **never substitute** — a
  bigger stake never buys an easier lobby or rank credit.
- **Lobby/table creation:** player-initiated (create-a-table) + browse/join open tables + quick-join a tier;
  parties travel via the existing EOS-party → PlayFab-ticket path (AFL-1104/1204).
- **PokerStars flavor:** a **table browser** (open staked lobbies by mode/tier/pool), **tournaments**
  (#4 RULED — **player-run** + **special admin** events), and a **lobby** surface (the League doc's pre-match
  "rank, loadout, social" view, `MASTER_ARCH:128`).

### 3D. Economy-integrity — anti-collusion / anti-fraud (GAME-WIDE; ✅ RULED, resolves + expands flag #6)
**Why game-wide, not staking-only.** Watts earned in **FREE** play feed the same one-way economy (no cash-out,
R2); illegal Watts stacking **devalues the currency and erodes grail scarcity** (the mint-cap product). So
integrity is **one layer (`AFL.Integrity.*`) over ALL gameplay — free AND staked** — two attack surfaces.

**Research grounding (poker platforms + competitive games).** Fraud/collusion types — **chip-dumping**
(intentional loss to transfer value), **soft-play** (colluders don't compete), **win-trading** (arranged
outcomes), **multi-accounting** (one person, many seats), **boosting rings**, **bots / RTA**. Signals mature
systems use — shared **IP/device/session fingerprints**, improbable **transfer/benefit graphs** (A always feeds
B), **win-pattern outliers**, **timing/behavioral anomalies**, **account-linkage graphs**. Enforcement is a
**ladder** (flag → review → penalty → appeal) with **high-confidence thresholds + human review for severe
actions** to manage **false positives** (never punish legit friends or genuine skill); detection logic is
**never exposed** (cheaters adapt).

**A — FREE-PLAY earn integrity (the currency SOURCE).** *Prohibited:* **win-trading** (arranging Match-base /
Daily-first-win outcomes, `ECON §3`), **boosting** (playing to inflate another account's earn), **loot-feeding /
self-dealing** (feeding your own dismember parts to a colluding **enemy-team** alt to manufacture combat-loot
Watts — exploits the `ECON §3a` **enemy-collect** grant), **manufactured earn** (bot / AFK / scripted matches,
quest-farming), **multi-accounting** to farm daily/weekly across controlled accounts. *Signals:* **earn-rate
anomaly** (Watts/hour + Watts/match vs the calibrated ~4,000 W/match baseline), **benefit graphs** (whose
deaths / heads / losses consistently feed whom), **fingerprint overlap among "opposing" seats**, **loot-feed
pattern** (one enemy repeatedly collecting one player's parts), **no-combat / timing anomalies**. *Earn-layer
guards (some enforceable now):* the existing **nonce dedupe** (replay) + **MAX_GRANT ceiling** in the earn
Lambda, extended by an **earn-rate / window cap** + **AFK / no-participation → reduced earn**.

**B — STAKED-MATCH integrity.** *Prohibited:* **stake-dumping** (throwing a staked match to transfer the pool
to a confederate — the chip-dump analogue), **soft-play**, **multi-accounting a lobby** (one person on several
seats to control the pool), **party-stacking** a competitive staked lobby to fix it. *Lobby-creation controls:*
who may open staked lobbies (**rank / level / account-age** gate — anti-throwaway-account), **seat + same-party
limits** in competitive staked matches (flag #6b), **multi-account fingerprinting at the seat**. Reuses the
League **anti-throw** signal (AFL-2206) for stake-dump detection.

**C — Enforcement ladder + penalties + clawback.**

| Tier | Trigger | Action |
|---|---|---|
| **T0 Warn** | low-confidence / first minor | notice + watchlist |
| **T1 Clawback** | confirmed fraudulent earn | reverse the fraudulently-earned Watts |
| **T2 Stake-ban** | staked collusion | barred from staked queues; casual/ranked continue |
| **T3 Account action** | egregious / repeat | temp suspension → permaban |

- **Review-gated:** automated flags for T0–T1; **human adjudication before T2–T3** (the false-positive guard).
  **Appeal path** at every tier.
- **Clawback-of-SPENT currency (the economy-integrity model — you can't un-spend a grail):** Watts **still in
  wallet** → reversed directly. Watts **already spent** → can't un-spend; model = a **negative-balance debt**
  (wallet goes negative; future earn repays before any new spend) and/or **reverse the purchase where the
  entitlement is un-consumed and un-traded**. A fraudulently-obtained **grail** (1-of-1, container-locked,
  never-reissue) cannot be cleanly un-minted without breaking the 1-of-1 truth → remedy is **account
  seizure / ban** + operator adjudication on whether the mint is re-released. **The grail-clawback policy is a
  genuine decision — flag #6c.**

**D — Naming (ADR doctrine).** `AFL.Integrity.<System>` · `AFL.AntiFraud.<Rule|Signal>` · `AFL.Penalty.T{0-3}`.
Integrity *state* (flags, penalty status, watchlist) is player-record metadata, never encoded in an id.

**Build-vs-gated split (what's policy-now vs backend-gated).**
- **DESIGN-POLICY — recordable NOW (this doc, law):** the prohibited-behavior catalog (A/B), the penalty ladder
  (C), the clawback model, the lobby-creation rules, the earn-rate-ceiling policy.
- **DETECTION + ENFORCEMENT — BACKEND-GATED (rides the B2 / PlayFab spine):** signal collection (fingerprints,
  transfer/benefit graphs), anomaly analysis, the flag → review → penalty pipeline, clawbacks / bans. Extends
  the earn Lambda (dedupe + MAX_GRANT → + earn-rate / window caps) and the `IAFLCosmeticPersistence` / PlayFab
  layer. **Recorded as a B2 backend integrity work-item** (`Bag_Man_Backend/docs/bundle-purchase-checklist.md`)
  so it is not lost. Largely Phase-3/5 (post-persistence): the **policy is law now; the detection engine is gated.**

### 3D.1 Integrity thresholds — 6a penalty ladder + 6b lobby rules ✅ RULED (research-grounded)
Values below are **translated from major-platform practice** (cited), fit to our Watts/Volts, **no-cash-out**,
best-of-13 model — defensible, not invented. (Key adaptation: we borrow poker's *collusion* controls but
**not** its money-laundering/KYC apparatus — no-cash-out means **not gambling**, so no identity verification.)

**6a — PENALTY LADDER thresholds (✅ RULED).** Case dimensions: **confidence** (LOW = single weak signal /
MED = multiple corroborating / HIGH = unambiguous, e.g. same-device on opposing seats + a lopsided transfer
graph), **severity** (MINOR soft-play/marginal anomaly · MAJOR win-trade/loot-feed/stake-dump · EGREGIOUS
multi-account/ring/bots), and **repeat count** in a rolling window.

| Tier | Trigger (threshold) | Enforcement | Source-practice |
|---|---|---|---|
| **T0 Warn** | LOW confidence **or** 1st MINOR | automated; notice + watchlist | escalating-penalty systems open with a warning/correction step (Riot / Blizzard) |
| **T1 Clawback** | MED+ confidence of fraudulent **earn**; 1st MAJOR earn offense | automated **reversal of ill-gotten Watts** + watchlist | poker rooms **confiscate collusion gains** (PokerStars / GGPoker game-integrity) |
| **T2 Stake-ban** | 2nd MAJOR in window **or** HIGH-confidence staked collusion | **human review first**; barred from staked queues (casual/ranked continue); **30d → 90d → permanent** on repeat | escalating competitive cooldowns (CS / Overwatch abandon ladders) |
| **T3 Account action** | EGREGIOUS at any count (**zero-tolerance**) **or** repeat T2 | **human review first**; **14-day suspension precedes permanent** (zero-tolerance → straight to permanent) | Riot LoL **"one 14-day ban before permaban"**; Valve **VAC / multi-account = immediate permanent** |

- **Rolling offense window = 90 days**; **penalty history decays after 12 months clean** (repeat counter resets)
  — grounded in escalating-cooldown-with-decay practice (competitive games lower penalty level over clean time).
- **Review-gate (false-positive protection):** T0/T1 automated (low-harm + reversible); **T2/T3 require human
  adjudication before enforcement** — never auto-permaban on a single signal (the universal severe-action rule).
- **Appeal window = 14 days** per action, human-reviewed (industry-standard appeal window).
- **Confidence floor:** no restrictive action (T2+) on **LOW** confidence alone — MED+ with corroboration.

**6b — LOBBY / COLLUSION RULES (✅ RULED).**

| Rule | Value | Source-practice |
|---|---|---|
| **Seats per account per staked match** | **exactly 1** | poker **one-seat-per-table**; multi-seating one table is banned |
| **Multi-account a lobby** (same fingerprint on ≥ 2 seats) | **T3 zero-tolerance** | poker / Valve multi-account = immediate severe |
| **Same-party across OPPOSING sides in a staked match** | **BANNED** (the win-trade/soft-play vector); same-party on the **same team** allowed up to team size | competitive party rules keep colluding pairs off opposing sides |
| **Same-device / same-IP among opposing seats** | **auto-flag → hold pending review** (not auto-ban — households/LAN are legit) | poker **same-IP-at-a-table flag/block**; staked bar higher than casual |
| **Party clustering in staked BR / FFA** | a party may fill **≤ one team's worth** of seats; excess flagged | Valorant / Apex party-size + rank-spread limits |
| **Who may OPEN a staked lobby** (anti-throwaway host gate) | account age **≥ 7 days** **+ ≥ 25 matches** **+ no active integrity flag** (no T1+ in the last 90 days) | skill-money platforms gate hosting on **verified standing**; phone-verify-for-ranked (Valorant) as the anti-smurf analogue |
| **High-stake tiers (HighVolt / Nosebleed)** | **stricter host gate** — no integrity flag ever + longer tenure (candidate: invite/verified-only) | "high-stakes tables require verified players" |

- **Rank-based** host/seat gating is deferred to **flag #5** (ranked-on-staked); the age + match-count gate above
  is rank-independent and stands regardless.

**6c — GRAIL CLAWBACK = ADMIN MASTER CONTROL (✅ RULED).** A fraudulently-obtained **Singularity 1-of-1** is
**NOT** clawed by the automated T0–T3 ladder — its reversal is a **manual operator/admin master-control**
action, invoked **only when integrity requires it** (normal operation preserves **E1 intact-only /
never-reissue**). **Best-practice guardrail:** it is a **PRIVILEGED, AUDITED** capability — every use is
**logged (immutable audit trail)** and **restricted to authorized admins** (a master override on a $500 1-of-1
asset demands accountability — the least-privilege + audit-log principle for high-value admin actions).
**Mint-slot handling:** the **admin decides case-by-case** whether the reclaimed mint slot **stays consumed**
(preserving the 1-of-1 truth) or **frees for re-issue** to a legitimate buyer. *Small sub-note (only if a
default is ever wanted — not forced):* recommend the slot **stays consumed** unless an admin explicitly
re-releases — the safer default for 1-of-1 integrity.

---

## 4. INTEGRATION MATRIX

| Existing system | MATCH TYPES (3A) | STAKING (3B) | MATCHMAKING (3C) |
|---|---|---|---|
| **Economy — currencies** (`ECON §1`, no-cash-out) | — | buy-in/pool/payout in **Watts** (clean) / **Volts** (flag #1); **never real-money** | staked queue gated on wallet balance |
| **Economy — atomic wallet (B2 layer)** | — | escrow = the **B2 deduct/refund pattern** (`/purchase-bundle`, atomic, refund-on-fail) | balance check pre-ticket |
| **League / ranked** (`LEAGUE`) | ranked runs on Arena/Team/BR (`MAP_MODE`) | **ranked CAN be staked** (#5) — but rank/MMR key on outcome+skill only, **never on stake** (firewall) | staking = a soft overlay on the ranked MMR pool (no pool-thinning); **consumes** Glicko-2 MMR |
| **Scoring substrate** (`MAP_MODE §1.1`, built) | win = eliminate-OR-extract; 1-v-many = bust-or-survive | payout curve reads the match result + bounty (combat-loot values) | — |
| **Combat-loot / bounty** (`LSD §3a`) | 1-v-many Mark bounty; bounty format | head/limb values = the **bounty unit** | — |
| **Identity / teams** | team modes team-pooled; Mark = one identity | team stakes pooled + split by contribution | party rosters (EOS) → ticket |
| **Matchmaking backend** (PlayFab→Lambda→GameLift) | modes = `LyraExperience` variants (`MAP_MODE §5`) | stake tier + buy-in ride `MatchmakerData` | the transport for all 3 queues |
| **Maps** (`MAP_MODE`) | each mode → its map tier | — | queue → certified map/size |
| **Economy-integrity (§3D, game-wide)** | free-play earn guards apply to ALL modes (the currency source) | staked collusion / stake-dump controls + lobby gates | fingerprint / anomaly signals at ticket + seat |

**Through-line:** the match SUBSTRATE (`MAP_MODE`) + the wallet/escrow (B2 economy layer) + the MMR (League)
already exist — staking + the queue model compose them; **and one `AFL.Integrity.*` layer (§3D) protects the
currency across free AND staked play.** Nothing here is a new spine.

---

## 5. LEAGUE CROSS-REFERENCE (what this doc resolves for the sibling)
`IRONICS_LEAGUE_ADVANCEMENT_SSOT.md` names "Matchmaking driver #3" as an **owed sibling** (its §5.1, §7.3,
flag #7). This doc **is** that sibling. The cross-reference (a doc-link edit applied alongside this file):
- **Ranked runs on:** Arena (1v1–4v4) + Team (5v5–8v8) + BR (separate pool) — the mirror/ranked-integrity
  modes (`MAP_MODE §1`). Shrink/party = unranked; 1-v-many ranked = flag.
- **Ranked CAN be staked, behind the firewall (#5 RULED):** ranked and staked are **independent axes that may
  combine** (staked-ranked allowed); **rank/MMR move on outcome + skill ONLY, never on stake size** (the
  rank-not-buyable firewall holds — a staked ranked win = identical RP to an unstaked one). Staking is a **soft
  opt-in overlay on the ranked MMR pool** (no separate queue → no pool-thinning). An optional **high-roller
  money board** may track staked performance without touching skill rank.
- **MMR ownership:** the League doc's Glicko-2 MMR (AFL-2201) is **consumed** by this doc's matchmaking; this
  doc owns the queue model + stake sorting, not the rating math.

---

## 6. 🚩 FLAGGED OPERATOR DECISIONS

1. **✅ RULED (R2, law) — NO CASH-OUT, in-game-spend-only.** Currency (Watts/Volts) is **one-way and stays
   inside the in-game economy** — players stake, win, and spend entirely in-game; **no path converts winnings
   to real money or out of the system.** Staking is poker STRUCTURE on **non-cashable** in-game currency, NOT
   real-money gambling — inheriting the `IRONICS_ECONOMY_SPEC.md §0` no-cash-out doctrine. The **real-money /
   custody tier remains PHASE-3 LEGAL-GATED** if ever revisited (deferred, not designed now). This no-cash-out
   law is itself the primary responsible-play safeguard (it removes the financial-harm vector); optional
   session/spend guards are a later lightweight refinement (deferred, not decided).
   *[History — v0.1 open question: "does staking EVER touch real money, or Watts/Volts-only forever? … confirm
   Watts/Volts-only, or route any real-money ambition to a separate legal track."]*
2. **✅ RULED (R1, law) — HOUSE RAKE, tiered by pool Volt-equivalent** (full statement + worked example in §3B):
   off the top of the **total pool**, **≤ 500 V-equiv → 5% · ≥ 501 V-equiv → 10%**, currency-agnostic (Watts
   convert at 10 W = 1 V). *[History — v0.1 open question: "house-cut yes/no + rate; recommended 0% on Watts +
   a small optional rake on Volts."]*
3. **✅ RULED (structure) — the micro→high stake-tier ladder (§3B.1).** `AFL.Stake.{Trickle · Ante · Live ·
   Main · HighVolt · Nosebleed}`, per-seat buy-ins **100 / 500 / 2,500 / 10,000 / 50,000 / 250,000 V-equiv**
   (~5× geometric), Watts-friendly floor → Volts top. The ladder shape + approach is ruled; the **specific
   values are design-proposals recorded as working law**, with sub-decisions flagged for confirmation:
   **(3a)** rake-break alignment — the 500 V-pool break falls inside Trickle (small-field = 5%, else 10%);
   align the whole micro tier to 5%, or keep strictly pool-based (current R1)? **(3b)** tier count (6 proposed).
   **(3c)** top ceiling (Nosebleed 250,000 V — cap, or add an invite-only apex?). **(3d)** Watts/Volts
   denomination split (proposed at Main). *[History — v0.1/v0.2 open question: "stake-tier names + cutoffs;
   candidate Trickle·Feed·Main·High·Prime."]*
4. **✅ RULED — TWO tournament classes (both).** **(a) Player-run / initiated:** players create + stake their
   own tournaments, riding the staking ladder (stake tiers §3B.1 + the R1 rake apply). **(b) Special admin
   tournaments:** official scheduled events stated + staked by admins (admin-set structure, prize, cadence).
   *Later-refinement sub-notes (not forced):* player-tournament format/size limits; admin-event cadence + prize
   sourcing. *[History — v0.1 open: "scheduled vs player-run — which ships first?"; ruled = both.]*
5. **✅ RULED — RANKED MATCHES CAN BE STAKED, behind the rank-not-buyable firewall (designed).** Staking and
   ranking are **independent axes that may COMBINE** (staked-ranked is allowed — real stakes on the games that
   matter = engagement). **THE FIREWALL (hard constraint; `ECON §0` no-pay-to-win + the League rank-not-buyable
   doctrine):** rank / MMR (Glicko-2, AFL-2201) move on **match OUTCOME + opponent SKILL ONLY** — **stake size
   has ZERO input to the rating update.** A staked ranked win grants the *identical* RP as an unstaked ranked
   win of the same result; the stake only moves the Watts/Volts **pot**. You can never buy rank. **Precedent
   (sourced):** chess **rated + prize tournaments** (rating from result vs opponent-rating, prize from placement
   — independent) · **poker** (table stakes ≠ any skill rating) · **FACEIT / Skillz** (entry fee → pool, skill
   matchmaking unaffected). **No separate staked-ranked QUEUE (anti-pool-thinning):** staking is a **soft opt-in
   overlay on the existing ranked MMR pool** — ranked matchmaking forms by skill as today (pool health +
   fairness first); a pot settles among matched players who opted into a compatible stake, stake being a *soft*
   secondary preference, never a hard segregation. *FLAGGED conflict + handling (surfaced, not forced):* a HARD
   stake filter would thin the ranked pool → **recommend the soft overlay** (skill-first, stake-opt-in) so the
   pool is never fragmented. An optional **high-roller money board** may track staked performance without
   touching skill rank. (1-v-many ranked = a later refinement.) *[History — v0.1 open: "ranked-on-staked; staking
   ⟂ rank?"; ruled = allowed + firewall.]*
6. **✅ RULED (system designed; detection backend-gated) — GAME-WIDE economy-integrity / anti-collusion (§3D).**
   One `AFL.Integrity.*` layer over free AND staked play (free-play Watts feed the same economy): the
   prohibited-behavior catalog (win-trading · boosting · loot-feeding/self-dealing · stake-dumping ·
   multi-accounting · bots), the signal set, a **T0–T3 enforcement ladder** (warn → clawback → stake-ban →
   account action; review-gated + appeal), and the clawback-of-spent model. **Policy is law now; the detection
   engine is backend-gated** (a recorded B2 integrity work-item). Sub-decisions: **(6a) ✅ RULED** (penalty
   ladder thresholds — T0 warn / T1 clawback / T2 stake-ban 30d→90d→perm / T3 account action, 14-day-before-
   permanent, 90-day window, 12-month decay, human review before T2/T3) and **(6b) ✅ RULED** (lobby rules —
   1 seat/account, same-party-off-opposing-sides, same-device flag-and-hold, host gate ≥7d + ≥25 matches +
   no active flag) — both **research-grounded + sourced in §3D.1.** **(6c) ✅ RULED — grail clawback = ADMIN
   MASTER CONTROL** (§3D.1): a fraudulent Singularity 1-of-1 is **not** on the automated ladder — reversal is a
   **manual, privileged, AUDITED** admin master-control action, used only when integrity requires it (E1
   intact-only / never-reissue preserved normally; mint-slot kept/freed at admin discretion). *[History —
   v0.1/v0.2 open: "player-initiated match authority + anti-collusion"; GAME-WIDE (v0.4); 6a/6b best-practice
   (v0.5); 6c admin master control (v0.6).]*
> **✅ ZERO FLAGS REMAIN OPEN — the staking SSOT is fully closed (v0.6).** All rulings recorded as law:
> **R1** rake · **R2** no-cash-out · **#3** stake-tier ladder · **#4** tournaments (player-run + admin) ·
> **#5** ranked-can-be-staked behind the firewall · **#6** game-wide anti-collusion (incl. **6a** penalty
> ladder, **6b** lobby rules, **6c** admin master-control grail clawback). Design-proposed specifics
> (stake-ladder values #3a–#3d) remain open to operator tuning, but no *decision* flags are outstanding.

---

## 7. DEPENDENCIES + CROSS-LINKS
**Rides existing spines (not a new build):** the match substrate (`MAP_MODE`, built) · the atomic wallet +
escrow (the **B2 economy/persistence layer**, live) · the matchmaking transport (PlayFab→Lambda→GameLift,
designed) · the League MMR (defined). **Genuinely new build work:** the staking escrow/pool/payout service
(extends the B2 Lambda pattern), the queue model + table browser, and the 1-v-many mode. All Phase-3/5
(post the core loop + persistence), consistent with `MAP_MODE §7` and `MASTER_ARCH §8`.

- **`IRONICS_MAP_MODE_SPEC.md`** — the authoritative mode ladder + win conditions this doc overlays.
- **`IRONICS_ECONOMY_SPEC.md` / `IRONICS_PRICING_SCARCITY_SSOT.md`** — currencies + the §0 no-cash-out law.
- **`IRONICS_LEAGUE_ADVANCEMENT_SSOT.md`** — the ranked/MMR sibling (§5 cross-reference; this doc is its
  owed matchmaking owner).
- **B2 backend spine** (`Bag_Man_Backend`) — the atomic deduct/refund/escrow pattern staking reuses; the
  PlayFab currency + matchmaking pipeline live there.
- **`BAG_MAN_MASTER_BUILD_v2.0.md`** — the PlayFab→Lambda→GameLift matchmaking backend + `AFLOnline` module.

---

*v0.5 SCOPING/DESIGN PASS, 2026-07-09 — what-exists audit (cited) · poker→shooter research · match-type
overlay (+ 1-v-many) · the micro→high stake-tier ladder (§3B.1) · the GAME-WIDE economy-integrity /
anti-collusion system (§3D) with **research-grounded 6a penalty-ladder + 6b lobby-rule thresholds (§3D.1,
sourced to poker/Riot/Valve/Valorant practice)** · queue-model matchmaking · integration matrix · League
cross-reference. **Rulings recorded as LAW:** R1 tiered rake · R2 no-cash-out · the stake ladder · the
anti-collusion system (policy-as-law, detection backend-gated; 6a penalty ladder + 6b lobby rules sourced;
**6c admin master-control grail clawback**) · **#4** tournaments (player-run + special admin) · **#5** ranked
CAN be staked behind the rank-not-buyable firewall. **ZERO decision flags remain open — the staking SSOT is
fully closed** (only design-proposed stake-ladder values #3a–#3d await operator tuning). DESIGN ONLY — no code,
no catalog/asset/infra, nothing built.*
