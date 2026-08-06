# IRONICS — League, Advancement, Achievements & Rankings SSOT

> **Status: SCOPING / DESIGN PASS — v0.2 (2026-07-09).** This is the **owed "progression / ranking
> definitions doc"** that `IRONICS_MARKETPLACE_MASTER_ARCHITECTURE.md` §6 driver #2 (`:142`) and §10
> (`:222`) flag as required **before Phase 5**. It is a **design deliverable only** — no game-system code,
> no catalog/asset/infra writes, no rank/achievement mechanism is built by this doc. Refine-and-finish is
> iterative from here.
>
> **v0.2 CORRECTIONS (2026-07-09, operator — two v0.1 misreads fixed):** **(A) the match/scoring substrate
> EXISTS** — v0.1 wrongly concluded "no win-condition/scoring" from a narrow `LCM:598-600` per-team-aggregation
> note. On disk: `IRONICS_MAP_MODE_SPEC.md` §1.1 (RESOLVED) = round-based **best-of** (2v2–4v4 = **best-of-13**),
> win-by-eliminate-OR-extract; the match-phase spine + per-player StatTag scoring are BUILT (MATCH-SPINE-C1).
> Ranked keys off THIS; only per-team aggregation + a fuller stats component + the RP mapping remain. **(B)
> ARC is a TIER name, not a currency** — the currencies are **Watts** (earned) + **Volts** (premium), peg
> 10W=1V (`ECON §1`); SPARK/SURGE/**ARC**/THUNDER BOLT are **price TIERS** (`EAFLCosmeticTier` enum,
> `SKU_CATALOG:28`). "ARC" is **APPROVED as a rank name**, namespace-distinct from the ARC price tier (a
> display-clarity note, not a collision). The false "scoring prerequisite" + "ARC currency collision" flags
> are dropped.
>
> **Grounds in (read + reconciled):** `IRONICS_ECONOMY_SPEC.md` (peg · currencies · earn structure ·
> Battle Pass) · `IRONICS_PRICING_SCARCITY_SSOT.md` (tier/rarity vocabulary · no-cash-out · persistence
> gate §5.1) · `AFL_ECONOMY_ARCHITECTURE_ADR.md` (ownership spine · `IAFLCosmeticPersistence` · id
> doctrine) · `IRONICS_MARKETPLACE_MASTER_ARCHITECTURE.md` (driver #2, the earn-and-gate stance) ·
> `AFL_IDENTITY_PRODUCTION_LINE.md` + `IRONICS_PRODUCT_SKU_CATALOG.md` (roster · 7 teams) ·
> `IRONICS_LOOT_SYSTEM_DESIGN.md` + `IRONICS_LOOT_CARRY_MODEL.md` (the extraction/loot earn substrate) ·
> `IRONICS_MAP_MODE_SPEC.md` (the EXISTING match structure + win conditions — best-of round-based) ·
> `BAG_MAN_LIVE_TRACKER.html` (the P-SCORING pillar + Sprint 22 ranked backlog + the built match/scoring spine).
>
> **Standing dependency (NOT closed here):** rank/achievement/season/leaderboard state is **durable state**
> that rides the **consolidated `IAFLCosmeticPersistence` seam** (PlayFab, Phase-1 write-side done
> `f010a0c1`; the **B2 backend spine** — mint-ledger + `/purchase-bundle` — is the live proof of that
> layer). Persisting *new* state shapes (MMR, rank, achievement flags, season progress) is a new write
> type through the same seam — **not yet demonstrated**. Rankings key off the **EXISTING best-of match /
> scoring structure** (`IRONICS_MAP_MODE_SPEC.md` §1.1, RESOLVED; the match-phase spine + per-player scoring
> are already BUILT — MATCH-SPINE-C1). The narrow remaining piece is **per-team score aggregation** + a
> fuller per-player stats component + the RP mapping — a scoped addition, NOT a from-scratch substrate. See §7.

---

## 0. WHAT THIS DOC IS (and the one-line architectural law it obeys)

Three player-facing systems, **one economy spine.** Per MASTER_ARCH §6 (`:136-138`): *"Rank is not a
separate spine — it's an **earn-and-gate driver**… rank rewards = catalog entries, rank unlocks =
entitlement grants."* This doc honours that law exactly:

- **LEAGUE & ADVANCEMENT** (System A) — the competitive **rank ladder** (skill tiers/divisions, seasons).
- **ACHIEVEMENTS** (System B) — an **earned-goal taxonomy** that grants rewards and **gates** things
  (critically the B2 #7 **team-ownership gate**).
- **RANKINGS** (System C) — **leaderboards + the MMR/skill layer**, kept deliberately distinct from
  economic progression.

All three are **drivers on the existing economy**: their rewards are `AFL.<Type>.<Name>` catalog entries,
their unlocks are entitlement grants through the proven wallet owned-set, their values reference
`IRONICS_PRICING_SCARCITY_SSOT.md`, and their state rides `IAFLCosmeticPersistence`. **None is a new spine.**

---

## 1. WHAT ALREADY EXISTS — the audit (cited)

The operator's "we started early" is accurate: substantive **first-pass design** exists, but as tracker
cards + backlog stubs, **not** a definitions doc. A filesystem glob for `*PROGRESSION*/*RANK*/*LEAGUE*/
*ACHIEVEMENT*/*SCORING*` returns **zero docs** — this file is the first.

### 1.1 The early design that EXISTS (build on this)
- **The named driver #2** — `IRONICS_MARKETPLACE_MASTER_ARCHITECTURE.md:142`: *"Leagues & Advancement —
  achievements, ARC/APEX/COD-style ranking, upgrade | earn source (rank rewards = catalog entries) + gate
  (rank unlocks) — needs a progression-definitions doc."* Placement: **Phase 5 driver**, docs-first
  (`:193-194`, `:222`).
- **The P-SCORING pillar first-pass** (`BAG_MAN_LIVE_TRACKER.html:2344-2351`, `1490-1495`, `1765-1769`):
  - **Multi-arc points** — the same event accrues to **per-match** (placement/MVP) · **day / week / month**
    (rolling, season cadence) · **lifetime** (career prestige).
  - **Dual subject** — every event credits **individual** (own progression + rank) **and team** (Team
    standing → social competition).
  - **Open ranking-model decision** (`:2374`): *"points-derived rank, or hybrid (points for rewards +
    separate skill-rating for matchmaking)?"* — resolve at P-SCORING / P-MATCH.
  - **Data model** — a **server-authoritative, replayable event STREAM**, not opaque counters (ties to
    Replay G6 + anti-cheat G4).
  - **Scored events** — kills, assists, saves/reboosts, QUANTUM head-collection ("Head Collector" stat).
- **The concrete ranked backlog — Sprint 22 (P5, W43-44)** (`BAG_MAN_LIVE_TRACKER.html:1691-1699`, mirror
  `BAG_MAN_MASTER_BUILD_v2.0.md:627-631`): **AFL-2201** MMR = **Glicko-2 derivative** · **AFL-2202** season
  reset + soft-reset · **AFL-2203** ranked rewards (placement icons, end-season cosmetics) · **AFL-2204**
  progression dashboard (career stats, ranked history) · **AFL-2205** ranked-specific matchmaking pool,
  stricter MMR bands · **AFL-2206** anti-throw detection + reporting.
- **Shipped scaffolding to consume** — per-player scoreboard K/D/A via `B_AFLMatchScoring`
  (AFL-0904/0905, commits `733a1699`/`c936814e`); an **Apex-style match-end full-screen takeover**
  (`IRONICS_UI_WASH_QUEUE.md:104`, done `c936814e`).
- **The MATCH STRUCTURE + WIN CONDITIONS — RESOLVED + partly BUILT (the substrate ranked keys off).**
  `IRONICS_MAP_MODE_SPEC.md` §1.1 (`:34-40`, RESOLVED — *"proven-popular standard"*): **Arena PvP
  (1v1–4v4) = round-based best-of** (CS/Valorant-proven) — **a round is won by ELIMINATING the enemy team
  OR banking a successful EXTRACTION**; **match = first-to-N** (1v1 ~Bo11/first-to-6; **2v2–4v4 ~first-to-7
  = best-of-13**), side/spawn swap. **Team (5v5–8v8):** control/hardpoint, payload, or
  team-extraction-to-threshold. **BR (18/36):** placement + banked-extraction hybrid. **Built in code:** the
  `UAFLMatchPhaseComponent` spine (Warmup→Active 8min→Extraction Window→End — MATCH-SPINE-C1 18/18,
  `a9b3333f`); **per-player scoring feeds Lyra StatTags** — elimination (`9096f1e8`) + assists (`a1c51997`),
  K/D/A ledger 2-client; match-end results post to PlayFab (AFL-1304). **The win/outcome/scoring substrate
  EXISTS** — the prior pass misread `LCM:598-600` (a narrow per-team-aggregation note) as "no scoring."
- **The engagement-progression spine that already exists** — the **Battle Pass** (`IRONICS_ECONOMY_SPEC.md`
  §4): ~100 tiers, free + premium track, XP/challenge-unlocked, exactly self-sustaining; and the **earn
  structure** (§3): match base with **win > loss** weighting, daily first-win, daily/weekly challenges.
  `IRONICS_PLAYER_FLOW.md:149`: *"There is no separate level system — and none is invented."*
- **The ownership + persistence spine** — replicated `OwnedCosmeticIds` on `UAFLWalletComponent`, atomic
  `CommitMutation` grant funnel (ADR Decision 1/4); `IAFLCosmeticPersistence` /
  `UAFLEconomyPersistenceSubsystem` write-side consolidated (`f010a0c1`), backed by PlayFab (the **B2**
  layer). Roster: **30 `AFL.Character.*`** (IRONICS free; 29 = $500 Singularity) + **7 `AFL.Team.*`**
  (ARIA · IRONICS · SCARLETT · MAKHIAVELLI · AP-9 · MOB-FIGAZ · FANATICS).

### 1.2 What is ONLY a name / placeholder (this doc defines it)
- **The League / rank ladder** — no tiers, divisions, thresholds, promotion/demotion, decay, or names.
- **Achievements** — one word (`:142`); no taxonomy, no list, no state model, no gate design.
- **Leaderboards / rankings** — zero design; no standings model.
- **Per-TEAM score aggregation + a fuller per-player stats component** — the *narrow* genuine gap (the match
  structure + win conditions + per-player scoring EXIST, §1.1). Not yet built: **per-team score aggregation**
  (`LCM:598-600` — *"the bank is PER-PLAYER; team aggregation is a future piece"*, a BR-scale note, **not**
  "no scoring"), a **fuller per-player stats component** (kills/energy-extracted beyond the current K/D/A
  ledger — the tracker "S-later" stats-component debt), and the **RP↔outcome mapping** this doc proposes.
  These are scoped build items on top of a live substrate, not a from-scratch substrate.
- **The team-ownership achievement gate (B2 #7)** — greenfield; nearest hook is the identity rule *"prune
  team axis 29→7, **operator picks the 7, don't auto-grant teams**"* (`AFL_IDENTITY_PRODUCTION_LINE.md:212-214`).

---

## 2. RESEARCH-GROUNDED PRINCIPLES (AAA competitive progression)

Grounded in the operator's named references (ARC Raiders, Apex Legends, Call of Duty) + the broader
competitive-design canon, reconciled to our locked economy doctrine.

### 2.1 The reference systems
- **Apex Legends (PvP skill ladder).** 7 tiers (Rookie→Bronze→Silver→Gold→Platinum→Diamond→Master) each
  with **4 divisions (IV→I)**, plus **Apex Predator** = a **leaderboard-defined top tier** (top N players).
  RP from **placement + kills**, with a **ranked entry cost that scales with tier** (anti-farm: high tiers
  risk RP to play). **Split seasons** with **soft resets** + **placement/provisional** calibration.
  **Demotion protection** buffers a drop. *Lesson: divisions give frequent legible milestones; entry cost
  stops smurf-farming; the top tier is a leaderboard, not a threshold.*
- **Call of Duty (three separated layers).** MP **level 1-55 + Prestige** (engagement/time), a **seasonal
  Battle Pass** (engagement), and **Ranked Play** with **Skill Divisions** (Bronze→…→Crimson→Iridescent→
  **Top 250** leaderboard) on an SR (skill rating). *Lesson: the cleanest proof that **engagement
  progression and skill rank are separate ladders** — a grinder levels up; only a skilled player ranks up.*
- **ARC Raiders (PvPvE extraction progression).** Advancement via **quests / gear / skill-tree +
  extraction success**, not a hard PvP rank ladder. *Lesson (most relevant to OUR extraction-arena loop):
  progression can be **earn-through-extraction + goal-driven**, distinct from a pure MMR ladder — and our
  win conditions already fold extraction INTO the round (`MAP_MODE §1.1`: win-by-eliminate-OR-extract). The
  operator's "ARC/APEX/COD-style" names the ranking-system inspiration; **"ARC" is ALSO an APPROVED
  electrical-theme rank name** (brand-fit), namespace-distinct from the `EAFLCosmeticTier::ARC` **price
  tier** (ARC is a tier name, **not** a currency — the currencies are Watts/Volts). See flag #1.*

### 2.2 Principles we ADOPT (each cited to a "why")
1. **Separate skill from engagement — two ladders, never one.** MMR/rank measures **skill** (fair
   matchmaking + earned prestige); the Battle Pass / Watts earn measures **time/engagement** (retention).
   Conflating them makes hours masquerade as skill (unfair) — the CoD three-layer split is the model, and
   it fits us because the Battle Pass **already exists** (`ECON §4`) and *"no separate level system is
   invented"* (`PLAYER_FLOW:149`). **Ranked is the new skill layer on top, not a second grind.**
2. **Clear, frequent, legible goals.** Tiers **×** divisions give a "just one more match" milestone cadence;
   the rank tier is the **legible face of the hidden MMR** (players see the tier, not the raw number).
3. **Earnable prestige — rewards are EARNED, never bought.** Placement icons + end-season cosmetics
   (AFL-2203) signal skill precisely because they **cannot be purchased**. This is *mandated*, not
   optional, by our doctrine: `ECON §0` **NO-CASH-OUT** + **NO-PAID-RANDOMIZED-ACQUISITION**, and MASTER_ARCH's
   earn-and-gate stance. **Rank must not be buyable** (§5.3).
4. **Seasonal soft reset + placement.** A **soft** reset (compress toward the mean, not wipe) each season
   keeps the ladder fresh and re-engages without demoralizing; placement matches recalibrate. Matches the
   Battle-Pass season cadence (`ECON §7`: ~9-13 weeks).
5. **Promotion/demotion with buffers.** Promotion feels earned (threshold or promo series); demotion has a
   protection buffer to avoid tilt yo-yo (Apex model).
6. **Anti-grind + anti-exploit.** Skill progress reflects skill, not hours (entry-cost/decay only at the
   top). Anti-throw/boost detection (AFL-2206) protects ladder integrity.
7. **Variety-rewarding, long-tail achievements.** Achievements reward **breadth** of play (the earn-structure
   principle, `ECON §3`: *"reward variety not just kills"*), give long-tail goals, and can **gate** prestige.

### 2.3 Anti-patterns we REJECT (cited)
- **Pay-to-rank** (buying RP/tiers/skill) — forbidden by `ECON §0` (no cash-out, no P2W); reputationally +
  legally radioactive. Volts may **never** buy rank or rank rewards.
- **Opaque MMR with no legible face** — always surface the tier ladder; never show a bare hidden number as
  the whole progression.
- **Punishing decay** — aggressive inactivity decay drives churn/anxiety. If decay exists at all, gentle,
  top-tiers-only (§6.1 flag).
- **Engagement-as-skill** — a grind ladder dressed as a skill rank (unfair; demoralizing).
- **Reward removal / expectation whiplash** — the explicit `ECON §4` Fortnite-2026 lesson: never create an
  earned expectation then take it away. Rank rewards, once defined, are stable.

---

## 3. SYSTEM A — LEAGUE & ADVANCEMENT (the rank ladder)

**Scope:** the competitive **skill** ladder. Distinct from the Battle Pass (engagement) — a player advances
the pass by *playing*, and the rank ladder by *winning/performing*.

### 3.1 Ladder structure (PROPOSED shape; names FLAGGED §6.1)
- **7 skill tiers**, ascending, each (except the top) with **4 divisions (IV → I)** — the Apex/CoD-proven
  cadence of frequent legible steps.
- **A top "leaderboard tier"** above the 7 — a **top-N standings** slot (Apex Predator / CoD Top-250 model),
  membership defined by the RANKINGS leaderboard (System C), not a fixed threshold.
- **Names — the ELECTRICAL THEME is the brand theme and is REUSED for ranks** (namespace + context
  disambiguate — the established doctrine; `PRICING_SSOT` already notes rarity "Surge" ≠ the SURGE **price
  tier**). The taken vocabularies are **price TIERS** (SPARK/SURGE/**ARC**/THUNDER BOLT — the
  `EAFLCosmeticTier` enum) · **rarity labels** (Singularity/Tempest/Bolt/Surge/Charge/Static) ·
  **currencies** (Watts/Volts/Bolts) — sharing a *word* across namespaces is intended reuse, not collision.
  **"ARC" is APPROVED as a rank name** (brand-fit), namespace-distinct from the `EAFLCosmeticTier::ARC`
  price tier — the id (`AFL.Rank.ARC.*`) + UI context separate them. Candidate ascending ladder (ARC
  anchored per the operator; the rest fresh electrical words to minimise overload; full set operator-blessed
  — flag #1): **Filament · Circuit · ARC · Dynamo · Tesla · Overload → OVERVOLT** (top leaderboard tier;
  OVERVOLT = the operator's floated top-name, `IRONICS_ECONOMY_SPEC.md:66`).
- **Ids (doctrine-conformant, ADR D3 + the two-segment `<Family>.<Variant>` precedent):**
  `AFL.Rank.<Tier>.<Division>` for the ownable rank-badge SKUs (e.g. `AFL.Rank.Dynamo.II`), and gameplay
  tags `Progression.Rank.<Tier>` (mirroring `Cosmetic.Rarity.*`). Rank *state* (current tier/division/MMR)
  is **entry metadata on the player record, never encoded in an id** (ADR Decision 6).

### 3.2 Advancement mechanic
- **Rank points (RP) derive from the EXISTING match outcome** — the best-of round result (win-by-eliminate-
  OR-extract, `MAP_MODE §1.1`) + per-player performance (the built K/D/A + assists StatTags; extraction
  success/value), win > loss (consistent with `ECON §3`). **The exact RP↔outcome mapping and whether rank
  is points-derived or a points+MMR hybrid is the OPEN decision (flag #2).**
- **Entry-cost at high tiers** (Apex anti-farm): higher tiers risk RP to queue, so farming low lobbies
  nets nothing — keeps the top meaningful.
- **Promotion** by threshold (optionally a short promo series at tier boundaries); **demotion** with a
  one-division protection buffer.

### 3.3 Seasonal structure
- **Seasons align to the Battle-Pass cadence** (`ECON §7`: ~9-13 weeks — one live-ops calendar, not two).
- **Soft reset** at season start (compress toward mean) + **placement matches** to recalibrate.
- **End-season rewards** (AFL-2203): placement icons + an **earned-only** end-season cosmetic per tier
  reached — `AFL.Rank.<Tier>.SeasonN` catalog entries, **account-bound** (§4.3 / §6.4).

---

## 4. SYSTEM B — ACHIEVEMENTS (earned-goal taxonomy + the gates)

**Scope:** discrete earned goals that (a) reward, (b) give long-tail direction, and (c) **gate** prestige —
including the B2 #7 team-ownership gate.

> **Parameters of record:** the concrete thresholds · tiers · reward values · first achievement set · and the
> team-ownership earn cutoff are set from cited game-science research in **`IRONICS_ACHIEVEMENTS_SSOT.md`** —
> the values the achievement mechanism builds to. §4 below is the *mechanism*; that doc is the *parameters*.

### 4.1 Taxonomy
- **Categories** (id family `AFL.Achievement.<Category>.<Name>`): **Combat** (kills/dismember/head-collector),
  **Extraction** (got-away-clean streaks, value-extracted), **Progression** (reach rank tier, season
  participation), **Collection** (own N finishes/identities), **Social/Team** (team-standing contribution),
  **Mastery** (per-identity / per-weapon milestones).
- **Cardinality:** **one-time** (a fixed milestone) · **repeatable** (re-earnable, e.g. daily/weekly —
  reuse the existing challenge cadence, `ECON §3`) · **seasonal** (reset each season, tie to the ladder).
- **Achievement tiers** (the `<Family>.<Variant>` precedent): Bronze/Silver/Gold **or** I/II/III on the
  same base id (e.g. `AFL.Achievement.Combat.HeadCollector.III`).
- **Data:** each earn is a record on the **P-SCORING replayable event stream** (server-authoritative,
  anti-cheat-visible) — **not** an opaque flag. State rides `IAFLCosmeticPersistence` (§7).

### 4.2 How achievements GATE (the earn-vs-buy boundary)
- **Reward grant:** completing an achievement **grants a catalog entry** (a cosmetic and/or a Watts payout)
  through the same entitlement grant loop as everything else. Watts payouts are safe (earned, pegged,
  deterministic — `LSD:140-143`). **Never a purchased/random grant.**
- **Gate:** an achievement can be a **precondition** on another entitlement or capability (owned-set
  membership check), exactly as rank-unlocks gate (MASTER_ARCH `:210`).

### 4.3 THE TEAM-OWNERSHIP GATE (B2 #7) — designed concretely
**The reconciliation.** A **Team-axis identity** (`AFL.Team.SCARLETT`, etc.) is a **cosmetic brand you own
and wear** — there is **no player-membership/guild system** (`ADR`, `PLAYER_FLOW §3` confirm). B2 #7 says
**persistent team ownership is achievement-EARNED, not purchase-granted.** Design:

- **Two separable things, deliberately split:**
  1. **The team cosmetic SKU** (`AFL.Team.<Name>`) — a **tradeable child line-item** of the Singularity
     bundle (D1/R5). Buying/owning it lets you **wear the brand**. This is commerce.
  2. **Persistent team STANDING** (`AFL.Achievement.Team.<Name>.Champion`, proposed) — an **earned,
     account-bound** achievement that confers **durable team representation**: eligibility for the **team
     leaderboard** (System C), team-standing contribution, and a prestige marker. **This cannot be bought.**
- **The gate:** the Singularity bundle’s team-axis grant makes you *look* like the team; the **achievement**
  makes you *represent* it durably. This is exactly the *"don't auto-grant teams; teams are earned, operator
  picks"* hook (`IDENTITY:212-214`) turned into a concrete earn condition.
- **The B2 seam this fulfils:** the B2 `/purchase-bundle` Lambda already **records** the team-gate seam
  (`teamGateNote` on the response; `TeamChildId` on the mint-ledger) and does **not** grant persistent team
  ownership on purchase. **This system is the earn mechanism that seam was reserved for.**
- **FLAG (§6.5):** the *specific* earn condition for `AFL.Achievement.Team.<Name>.Champion` is an operator
  decision (candidates: reach rank tier X while repping the team · win N matches repping it · finish top-K
  in the season team-standing).

---

## 5. SYSTEM C — RANKINGS (leaderboards + the MMR/skill layer)

**Scope:** the **standings** surface + the **hidden skill rating** that drives fair matchmaking — kept
**separate** from economic progression (principle 2.2.1).

### 5.1 The MMR / skill layer
- **Glicko-2 derivative** (AFL-2201) — a hidden per-player skill rating updated from match outcomes. It
  **drives matchmaking fairness** and **defines the top leaderboard tier**; the **rank tier is its legible
  face** (§3.1). The Glicko-2 rating is the **ranked system's** skill measure (AFL-2201); the **matchmaking
  sibling** (`IRONICS_MATCH_STAKING_SSOT.md`, driver #3) **consumes** it for queue fairness (§7 dep #3).

### 5.2 Leaderboards (the multi-arc, dual-subject model)
- **Time arcs** (from P-SCORING): **daily · weekly · monthly · seasonal · lifetime (career)**. The
  `ECON §7` note *"Top 3 players most hours played in a day"* is absorbed here as one **daily** board (but
  keyed on **performance points**, not hours — hours are engagement, not skill, per 2.2.1).
- **Subjects:** **individual** (per-player scoring is built) and **team** (needs the narrow **per-team score
  aggregation** on top of the existing per-player scoring — `LCM:598-600`; §7).
- **Standings** are durable state on `IAFLCosmeticPersistence` (§7).

### 5.3 Rank ↔ economy: the FIREWALL (doctrine, non-negotiable)
- **Rank is NOT buyable.** No Volts/Watts path buys RP, tiers, MMR, or any rank reward. `ECON §0`
  (no-cash-out, no-P2W) + the earn-and-gate stance make this **law**, not preference.
- **Rank *rewards* ARE economy objects** — placement icons / end-season cosmetics are `AFL.<Type>.<Name>`
  catalog entries, granted as **earned, account-bound** entitlements (§6.4). Their *value tier* references
  `IRONICS_PRICING_SCARCITY_SSOT.md` (MASTER_ARCH `:52`, `:209`) — **priced-for-reference, sold-never.**
- **The only economy↔rank interaction:** rank *earns* cosmetics (an earn source) and *gates* their access
  (a gate). That is the entire surface. No other coupling (no trade↔rank, no reputation) exists or is
  introduced.

---

## 6. INTEGRATION MATRIX — how the three compose with the existing systems

| Existing system | LEAGUE (A) | ACHIEVEMENTS (B) | RANKINGS (C) |
|---|---|---|---|
| **Economy — currencies** (`ECON §1`) | RP is its **own** unit (not Watts/Volts); **not buyable** | Watts payouts (earned, OK); **never Volts-for-progress** | MMR is non-economic; boards are non-economic |
| **Economy — Battle Pass** (`ECON §4`) | **Parallel ladder** — season-aligned, but skill ≠ pass XP | repeatable achievements reuse the challenge cadence | — |
| **Economy — pricing/rarity SSOT** | end-season reward **value tier** references SSOT | achievement reward value references SSOT | reward-only; boards priceless |
| **Ownership spine** (`ADR` wallet owned-set) | rank badges + rewards = entitlement grants | reward + gate = owned-set grants/checks | — |
| **Identity roster (30)** | per-identity **Mastery** achievements (B) feed rank cosmetics | `AFL.Achievement.<...>` per-identity milestones | per-identity leaderboards (optional) |
| **Teams (7)** | — | **team-ownership gate (§4.3)** = the B2 #7 mechanism | **team leaderboard** (needs team-score, §7) |
| **Singularity bundles (B2)** | — | team-axis SKU grant ≠ persistent team standing (§4.3) | — |
| **Persistence `IAFLCosmeticPersistence` (B2 layer)** | rank/MMR/season state = **new write shapes** on the seam | achievement flags = new write shapes | standings = new write shapes |
| **P-SCORING event stream** | RP derives from per-match arc | achievement earns are stream records | boards aggregate the stream (multi-arc) |
| **Matchmaking driver #3** | tier = face of #3's MMR | — | #3 **owns** MMR; C consumes it |
| **Match outcome / scoring** (`MAP_MODE §1.1`, EXISTS) | RP derives from the best-of round result (built) | combat/extraction score events exist (StatTags) | per-player boards built; **team boards need per-team aggregation** |

**The through-line:** P-SCORING's server-authoritative event stream is the **single source** all three read;
`IAFLCosmeticPersistence` (the B2 PlayFab layer) is the **single durable home** all three write; the wallet
owned-set is the **single grant channel** all rewards flow through. Three player-facing systems, one spine.

---

## 7. DEPENDENCIES — what must exist first (build order)

Rank/Achievements/Rankings sit **late** (Phase 5 / Sprint 22, `MASTER_ARCH §8`, `MASTER_BUILD:627`). The
match/scoring **substrate they key off EXISTS** (§1.1) — the remaining prerequisites are scoped additions on
top of it, not a from-scratch build:

1. **The match / win / scoring substrate — EXISTS (NOT a blocker).** Win conditions are RESOLVED
   (`MAP_MODE §1.1`: best-of round-based, win-by-eliminate-OR-extract, 2v2–4v4 = best-of-13) and the phase
   spine + per-player StatTag scoring are BUILT (MATCH-SPINE-C1, `9096f1e8`/`a1c51997`). Ranked computes RP
   from this. **What this doc adds is the RP↔outcome mapping** (flag #2) — a definition, not a substrate.
2. **Per-team score aggregation + a fuller per-player stats component** (the *narrow* real build gap). Team
   leaderboards (C) + team-standing (B) need per-team aggregation on top of the existing per-player scoring
   (`LCM:598-600`); a fuller stats component extends today's K/D/A ledger (the tracker "S-later" debt).
3. **Matchmaking & Game-Types driver #3** — its doc now EXISTS: **`IRONICS_MATCH_STAKING_SSOT.md`** (match-type
   spectrum + staking economy + queue model). It **consumes** this doc's Glicko-2 rating (AFL-2201) for
   matchmaking fairness and owns the ranked pool wiring (AFL-2205). **Ranked runs on** Arena (1v1–4v4) + Team
   (5v5–8v8) + BR (separate pool); Shrink/party = unranked. **A ranked match CAN be staked** (`MATCH_STAKING`
   #5 RULED), but **rank/MMR move on outcome + skill ONLY, never on stake size** — the rank-not-buyable firewall
   holds (a staked ranked win = identical RP to an unstaked one; staking is a soft overlay on the ranked MMR
   pool, no pool-thinning). An optional separate high-roller *money* board may track staked performance.
   This doc consumes matchmaking; it does not define it.
4. **Persistence new-state-shapes** on `IAFLCosmeticPersistence` — the write-seam is consolidated
   (`f010a0c1`, the B2 layer), but **MMR / rank / achievement / season / standings are new write types** not
   yet demonstrated. They follow the proven earn/purchase write pattern through the **same B2 backend**.
5. **Replay (G6) + anti-cheat (G4)** — the event-stream data model is chosen precisely to serve these; ranked
   integrity (AFL-2206 anti-throw) leans on them.

**The substrate (1) is live; (2)-(4) are scoped additions on top.** This doc is the **definitions** the
Phase-5 wiring consumes — authored now so the build is not a discovery loop.

---

## 8. 🚩 FLAGGED OPERATOR DECISIONS (rulings owed before build)

1. **§3.1 — Bless the rank ladder NAMES + shape.** **"ARC" is APPROVED as a rank name** (electrical-theme
   brand-fit). It is a **rank name**, distinct from the `EAFLCosmeticTier::ARC` **price tier** — and neither
   is a currency (currencies = Watts/Volts). The id namespace (`AFL.Rank.ARC.*` vs the tier enum) + UI
   context separate them: a **display-clarity note, not a collision**. **Decide:** bless or adjust the
   candidate ladder (Filament·Circuit·ARC·Dynamo·Tesla·Overload·OVERVOLT), how far to lean into theme-reuse
   of price/rarity words vs. fresh ones, and confirm the 7-tier × 4-division + top-leaderboard shape.
2. **§3.2 / §5.1 — Ranking MODEL: points-derived vs hybrid.** The tracker's own open question (`:2374`):
   is rank purely points-derived, or a **hybrid** (points for rewards + a separate Glicko-2 skill rating for
   matchmaking)? *Design-recommended: hybrid* (skill for matchmaking fairness, points for reward pacing) —
   but operator rules.
3. **§3.3 — Season cadence + reset depth.** Confirm seasons align to the Battle-Pass ~9-13-week calendar,
   and the **soft-reset** compression depth (how hard the mean-regression at season start).
4. **§4.2 / §5.3 — Achievement/rank reward BINDING: bound vs tradeable.** §8.4 policy is *free-granted =
   bound, purchased/earned = tradeable* (`PLAYER_FLOW:254-259`). Earned **rank/achievement** rewards are a
   third case. *Design-recommended: **account-bound** (earned prestige is non-sellable — protects the signal
   that a placement icon means skill, not wealth)* — confirm, since it's an exception to "earned = tradeable".
5. **§4.3 — The team-ownership gate's SPECIFIC earn condition.** What concretely earns
   `AFL.Achievement.Team.<Name>.Champion` (persistent team standing)? Candidates: reach rank tier X repping
   the team · win N matches repping it · finish top-K in the season team-standing. Operator picks.
6. **§6.1 top tier — Decay.** Is there rank decay for inactivity at all? *Design-recommended: none, or
   gentle + top-leaderboard-tier-only* (avoid the churn anti-pattern) — confirm.
7. **Scope handoff — Matchmaking + team-aggregation ownership.** Confirm this doc **stops at** consuming the
   EXISTING match/scoring substrate (§1.1) and the driver-#3 MMR, and that **per-team score aggregation**
   (§7.2) and **matchmaking** get their own design passes (this doc defines neither the matchmaking impl nor
   the team-aggregation impl).

---

## 9. Cross-links
- **`IRONICS_MARKETPLACE_MASTER_ARCHITECTURE.md`** — this doc **is** the owed §6 driver-#2 progression/ranking
  definitions doc (`:142`, `:222`); honours the earn-and-gate stance (`:136-138`, `:209-210`).
- **`IRONICS_ECONOMY_SPEC.md`** — Battle Pass (§4) = the parallel engagement ladder; earn structure (§3) =
  the variety principle; §0 no-cash-out/no-P2W = the rank firewall (§5.3).
- **`IRONICS_PRICING_SCARCITY_SSOT.md`** — reward value tiers reference it; §5.1 persistence gate applies.
- **`AFL_ECONOMY_ARCHITECTURE_ADR.md`** — ownership spine, `IAFLCosmeticPersistence`, `AFL.<Type>.<Name>` id
  doctrine (Decisions 1/3/4/6) all reused; progression is greenfield relative to it.
- **`BAG_MAN_LIVE_TRACKER.html`** — P-SCORING pillar + Sprint 22 (AFL-2201..2206) = the buildable backlog this
  doc gives definitions for.
- **B2 backend spine** (`Bag_Man_Backend`, `docs/bundle-purchase-checklist.md`) — the PlayFab persistence
  layer rank/achievement state rides; the team-gate seam (#7) this doc's §4.3 fulfils.
- **`IRONICS_MATCH_STAKING_SSOT.md`** — the driver-#3 sibling (match types + matchmaking + staking); consumes
  this doc's Glicko-2 rating, keeps staking orthogonal to rank (§5 cross-reference). **Owed sibling still
  open:** per-team score aggregation.

---

*v0.2 SCOPING/DESIGN PASS, 2026-07-09 — what-exists audit (cited, incl. the EXISTING best-of-13 match/scoring
substrate) · research-grounded principles · the three system designs (League/Achievements/Rankings) · the
integration matrix · dependencies (substrate live; per-team aggregation + RP mapping remain) · 7 flagged
operator decisions. Currencies = Watts/Volts; ARC = tier/approved-rank name, not a currency. DESIGN ONLY —
no code, no catalog/asset/infra, no mechanism built. Refine-and-finish is iterative.*
