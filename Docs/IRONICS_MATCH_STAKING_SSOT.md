# IRONICS — Match Types, Matchmaking & Staking SSOT

> **Status: SCOPING / DESIGN PASS — v0.2 (2026-07-09).** The owed **driver #3** doc
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
> **v0.2 — TWO RULINGS RECORDED AS LAW (2026-07-09, operator):** **R1 — HOUSE RAKE** (flag #2 → ✅ RULED):
> rake off the top of the **TOTAL pool**, tiered by the pool's **Volt-equivalent** (Watts convert at peg
> **10 W = 1 V** so the bracket is currency-agnostic) — **pool ≤ 500 V-equiv → 5% · pool ≥ 501 V-equiv →
> 10%** (§3B). **R2 — NO CASH-OUT** (flag #1 → ✅ RULED): staking is poker STRUCTURE on **non-cashable,
> in-game-spend-only** Watts/Volts (inherits `ECON §0`); winnings never convert to real money — the
> real-money / custody tier stays Phase-3 legal-gated, deferred, not designed. **The other four flags
> (stake-tier names/cutoffs · tournaments · ranked-on-staked · lobby-creation/anti-collusion) remain OPEN.**

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
- **Stake tiers** `AFL.Stake.<Tier>` — buy-in bands that also sort matchmaking. Candidate ladder (flavor names,
  operator to bless — flag #3; electrical/poker blend, namespace-distinct from price/rank tiers):
  **Trickle · Feed · Main · High · Prime** (micro → nosebleed). Cutoffs (Watts thresholds) = flag #3.
- **Naming/ids (ADR `AFL.<Type>.<Name>` doctrine):** `AFL.MatchType.<Name>`, `AFL.Stake.<Tier>`, `AFL.Format.
  <SNG|MTT|Bounty|HeadsUp>`. Stake/pool/escrow *state* is entry metadata on the match record, never in an id.

### 3C. Matchmaking (queue model on the existing transport)
- **Three queue classes** (all ride the existing PlayFab-ticket → Lambda → GameLift pipeline; the class + stake
  tier ride in the ticket attributes / `MatchmakerData`):
  1. **Casual** — free, MMR-loose, fast fill. The default free-player path (normal Watts earn, no entry).
  2. **Ranked** — free entry, **strict MMR bands** (League AFL-2205), stake-agnostic (the firewall).
  3. **Staked** — entry required, sorted by `AFL.Stake.<Tier>` **and** MMR within the tier (fairness inside a
     stake band). "Tables" = staked lobbies (SNG) or scheduled tournaments (MTT).
- **Skill × stake:** MMR is the fairness axis *within* a queue; stake is a commitment/sorting axis *across*
  staked lobbies. They compose (a High-stake lobby still MMR-matches its seats) but **never substitute** — a
  bigger stake never buys an easier lobby or rank credit.
- **Lobby/table creation:** player-initiated (create-a-table) + browse/join open tables + quick-join a tier;
  parties travel via the existing EOS-party → PlayFab-ticket path (AFL-1104/1204).
- **PokerStars flavor:** a **table browser** (open staked lobbies by mode/tier/pool), **scheduled tournaments**
  (flag #4), and a **lobby** surface (the League doc's pre-match "rank, loadout, social" view, `MASTER_ARCH:128`).

---

## 4. INTEGRATION MATRIX

| Existing system | MATCH TYPES (3A) | STAKING (3B) | MATCHMAKING (3C) |
|---|---|---|---|
| **Economy — currencies** (`ECON §1`, no-cash-out) | — | buy-in/pool/payout in **Watts** (clean) / **Volts** (flag #1); **never real-money** | staked queue gated on wallet balance |
| **Economy — atomic wallet (B2 layer)** | — | escrow = the **B2 deduct/refund pattern** (`/purchase-bundle`, atomic, refund-on-fail) | balance check pre-ticket |
| **League / ranked** (`LEAGUE`) | ranked runs on Arena/Team/BR (`MAP_MODE`) | **staking ⟂ rank** — staked can be unranked; rank never keys off stake (firewall) | **consumes** League Glicko-2 MMR (AFL-2201) for fairness |
| **Scoring substrate** (`MAP_MODE §1.1`, built) | win = eliminate-OR-extract; 1-v-many = bust-or-survive | payout curve reads the match result + bounty (combat-loot values) | — |
| **Combat-loot / bounty** (`LSD §3a`) | 1-v-many Mark bounty; bounty format | head/limb values = the **bounty unit** | — |
| **Identity / teams** | team modes team-pooled; Mark = one identity | team stakes pooled + split by contribution | party rosters (EOS) → ticket |
| **Matchmaking backend** (PlayFab→Lambda→GameLift) | modes = `LyraExperience` variants (`MAP_MODE §5`) | stake tier + buy-in ride `MatchmakerData` | the transport for all 3 queues |
| **Maps** (`MAP_MODE`) | each mode → its map tier | — | queue → certified map/size |

**Through-line:** the match SUBSTRATE (`MAP_MODE`) + the wallet/escrow (B2 economy layer) + the MMR (League)
already exist — staking + the queue model compose them; nothing here is a new spine.

---

## 5. LEAGUE CROSS-REFERENCE (what this doc resolves for the sibling)
`IRONICS_LEAGUE_ADVANCEMENT_SSOT.md` names "Matchmaking driver #3" as an **owed sibling** (its §5.1, §7.3,
flag #7). This doc **is** that sibling. The cross-reference (a doc-link edit applied alongside this file):
- **Ranked runs on:** Arena (1v1–4v4) + Team (5v5–8v8) + BR (separate pool) — the mirror/ranked-integrity
  modes (`MAP_MODE §1`). Shrink/party = unranked; 1-v-many ranked = flag.
- **Staking ⟂ rank:** ranked is **stake-agnostic** (the League rank-not-buyable firewall holds); a separate
  **"high-roller" standing** may track staked performance without touching skill rank (flag #5).
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
3. **Stake-tier names + cutoffs.** Bless/adjust the candidate ladder (Trickle·Feed·Main·High·Prime) and set
   the Watts buy-in thresholds per tier.
4. **Tournaments — scheduled vs player-run (or both).** PokerStars-style operator-scheduled MTTs, player-spun
   tournaments, or both. Which ships first?
5. **Ranked-on-staked policy.** Confirm **staking ⟂ rank** (recommended — protects the firewall), and whether
   a separate **high-roller staked leaderboard** exists. Also: is **1-v-many** ranked?
6. **Player-initiated match authority.** Who can create staked lobbies (anyone / rank-gated / level-gated), and
   any anti-abuse (collusion/chip-dumping detection — the staked analogue of the League's anti-throw AFL-2206).
> **Four flags remain OPEN** (await later operator rulings — NOT decided here): **#3** stake-tier names/cutoffs
> · **#4** tournaments scheduled-vs-player-run · **#5** ranked-on-staked · **#6** lobby-creation/anti-collusion.
> *(Responsible-play guards are folded under R2 — the no-cash-out law is the primary safeguard; optional
> session/spend caps deferred.)*

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

*v0.2 SCOPING/DESIGN PASS, 2026-07-09 — what-exists audit (cited) · poker→shooter research · match-type
overlay (+ 1-v-many) · staking economy (buy-in→pool→payout, formats, tiers) · queue-model matchmaking ·
integration matrix · League cross-reference. **2 rulings recorded as LAW** (R1 tiered rake 5%/10% by pool
Volt-equiv; R2 no-cash-out / in-game-spend-only); **4 flags remain open** (stake-tier names/cutoffs ·
tournaments · ranked-on-staked · lobby/anti-collusion). DESIGN ONLY — no code, no catalog/asset/infra, nothing built.*
