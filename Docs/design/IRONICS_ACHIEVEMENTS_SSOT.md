# IRONICS — Achievements Design SSOT (parameters of record)

> **Status: DESIGN PASS — v0.1 (2026-07-09).** The **parameter design-of-record** for the achievement
> mechanism: thresholds, cardinality, tiers, rewards, the first achievement set, and the team-ownership
> earn condition — set from **cited game-science + player-satisfaction research**, not opinion. The
> **mechanism** (track → award → persist → gate-check) is scoped separately (League SSOT §4 + the opened
> achievements build workflow); this doc is the **values that build conforms to.** DESIGN ONLY — no code,
> no manifest, no mechanism built.
>
> **Grounds in:** `IRONICS_LEAGUE_ADVANCEMENT_SSOT.md` §4 (the mechanism + taxonomy + the team-ownership
> gate) · `IRONICS_ECONOMY_SPEC.md` + `IRONICS_PRICING_SCARCITY_SSOT.md` (peg, ~4,000 W/match, the
> reward-vs-economy calibration) · `IRONICS_MATCH_STAKING_SSOT.md` (rank/ladder + the no-pay-to-win firewall)
> · `IRONICS_LOOT_SYSTEM_DESIGN.md` §3a (the combat-loot head-collect event) · `IRONICS_MAP_MODE_SPEC.md`
> §1.1 (best-of-13 win / eliminate-or-extract — the trigger events).
>
> **Doctrine inherited (hard):** achievement rewards are **earned + deterministic** (never purchased/random
> — `ECON §0`), **fixed-threshold** (transparent, not variable-ratio), **Watts or earned-cosmetics only**
> (never Volts-for-progress — the League §5.3 rank-not-buyable firewall). Rank/prestige is **earned, not
> bought.**

---

## 1. RESEARCH — the game-science of achievement design (cited principles)

### 1.1 Motivation + the reward schedule (why fixed + deterministic)
- **Self-Determination Theory** (Deci & Ryan; Ryan, Rigby & Przybylski 2006, *"The Motivational Pull of
  Video Games"*): intrinsic motivation runs on **competence, autonomy, relatedness.** Achievements feed
  **competence** (mastery signals) — best when they reward **genuine skill/progress**, not grind.
- **Flow** (Csikszentmihalyi): keep challenge matched to skill — thresholds must sit in the **flow channel**
  (earnable-with-effort), never trivial (boredom) or unreachable (anxiety).
- **Reward schedules** (operant conditioning, Skinner): variable-ratio is the more "compulsive" schedule,
  but our **`ECON §0` no-randomized-acquisition doctrine + ethical stance** mandate **fixed, transparent,
  deterministic** thresholds — a player always knows the goal and the reward. This is both the doctrine-
  aligned and the healthier choice (it avoids the gambling-adjacent loop the no-cash-out design already rejects).
- **Goal-gradient effect** (Hull 1932; Kivetz, Urminsky & Zheng 2006): motivation rises as a goal nears →
  **tiered ladders (Bronze/Silver/Gold) + visible progress bars** sustain the long tail (each tier is a
  fresh near-term goal).
- **Anti-grind:** research on grind-fatigue — thresholds requiring excessive repetition of ONE action read
  as a chore. **Reward VARIETY of play** (the `ECON §3` principle: *"reward variety not just kills"*).

### 1.2 The completion-rate curve (the target distribution)
Live analytics — **Steam Global Achievement distributions** — show a steep **"achievement funnel":** early/
onboarding achievements are near-universal, dropping sharply to a rare long tail (<5%, often <1%). A healthy
set deliberately spans this curve so **every player segment has a live goal.** Target distribution adopted:

| Band | Completion target | Role |
|---|---|---|
| **Onboarding / common** | **> 50%** | first-X markers; teach + hook (F2P "first reward in 2–5 matches", cf. `PSS:60`) |
| **Progression (mid)** | **~15–50%** | earnable over a season with normal play (tier I of ladders) |
| **Mastery (hard)** | **~5–15%** | skill/dedication (streaks, tier III, high-value) |
| **Prestige (rare)** | **< 5%** | the grail badges — seasonal top / deep mastery / team standing |

### 1.3 Threshold-setting — proven real-game structures (cited)
- **Tiered cumulative counts** — the genre-standard **100 / 1,000 / 10,000 ladder** (Halo & Call of Duty
  service-record + medal milestones; Gears). Powers-of-ten-ish tiers ride the goal-gradient.
- **Win counts** — accessible tiers stay **single-to-low-double-digit** (Apex/Fortnite "first win / 5 / 25");
  seasonal ranked goals scale up.
- **Streaks are LOW-threshold** (high variance): 3–5 accessible, 10+ rare (a 10-win streak is far rarer than
  1,000 cumulative wins) — so streak numbers stay well below cumulative numbers.
- **Extraction goals** (Escape from Tarkov, Hunt: Showdown): *"extract with ≥ X value," "survive/extract N
  raids," extraction streaks* — sized to a **wipe/season**, not a session.
- **Seasonal goals** sized to our **~9–13-week season** (`ECON §7`) × our **~20 matches/week** regular-player
  anchor (`ECON §3`) → **~200–260 matches/season.** So: onboarding = 1; common = ~1 week (~20 matches);
  progression tier III = ~a season; prestige = multi-season / high-skill.

### 1.4 Reward calibration (grounded in OUR economy)
Our anchors (`PSS R4`, `ECON §3`): **~4,000 W/match**, base wardrobe **~2.5 matches** (~10,000 W), a Standard
cosmetic **~7.5 matches** (~30,000 W). Live-service economy design: a reward that **dwarfs match-earn distorts
the loop** (players grind achievements, not matches). So achievement rewards are a **fraction of a match to a
few matches**, capped so the match remains the primary earn. **Prestige rewards are STATUS, not currency** —
over-paying a rare badge cheapens its signal (the rarity IS the reward). Adopted reward bands:

| Band | Reward (Watts, ≈ matches) | Note |
|---|---|---|
| Onboarding / common one-time | **1,000–2,000 W** (¼–½ match) | a nudge, not a windfall |
| Progression (tier I–II) | **2,000–10,000 W** (½–2.5 matches) | ≈ up to a base cosmetic |
| Mastery (tier III / streak / high-value) | **15,000–30,000 W** (≈ a Standard cosmetic) **+ an earned account-bound cosmetic** | the prestige-cosmetic signal (League §4/§5.3, earned-only) |
| **Prestige (team standing, seasonal top)** | **STATUS only** (durable standing + leaderboard eligibility + marker) — **no currency** | rarity is the reward |

**Never Volts** (premium/hard) — no pay-to-progress (`ECON §0` + League §5.3 firewall).

---

## 2. THE FIRST ACHIEVEMENT SET (research-distributed, `AFL.Achievement.<Category>.<Name>`)

Distributed across the §1.2 curve + the League §4.1 taxonomy. Thresholds are **research-backed STARTING
values, tune-at-playtest against real completion telemetry** (same discipline as the economy's tuned splits).

| # | Achievement id | Trigger event | Threshold | Cardinality | Tier | Reward | Band / source |
|---|---|---|---|---|---|---|---|
| 1 | `AFL.Achievement.Combat.FirstBlood` | first elimination (`MAP_MODE §1.1`) | 1 | one-time | — | 1,000 W | onboarding (§1.2) |
| 2 | `AFL.Achievement.Extraction.FirstExtract` | first successful extraction (`LSD`) | 1 | one-time | — | 1,000 W | onboarding |
| 3 | `AFL.Achievement.Progression.FirstWin` | win a match (best-of-13) | 1 | one-time | — | 2,000 W | onboarding (Apex/Fortnite first-win) |
| 4 | `AFL.Achievement.Combat.Eliminations.{I/II/III}` | cumulative eliminations | 100 / 1,000 / 10,000 | one-time tiered | Bronze/Silver/Gold | 2,000 / 10,000 / 30,000 W (+ cosmetic @ III) | Halo/CoD 100/1k/10k ladder (§1.3) |
| 5 | `AFL.Achievement.Combat.HeadCollector.{I/II/III}` | enemy **head** retrievals (`LSD §3a`) | 50 / 500 / 5,000 | one-time tiered | B/S/G | 2,000 / 8,000 / 25,000 W (+ cosmetic @ III) | signature-loot tiered ladder |
| 6 | `AFL.Achievement.Extraction.CleanGetaway.{I/II/III}` | successful extractions | 25 / 250 / 2,500 | one-time tiered | B/S/G | 2,000 / 8,000 / 25,000 W | Tarkov/Hunt extraction-count |
| 7 | `AFL.Achievement.Combat.WinStreak.{I/II}` | consecutive match wins | 3 / 10 | one-time tiered | S/G | 5,000 / 20,000 W (+ cosmetic @ II) | streaks stay low (§1.3) |
| 8 | `AFL.Achievement.Extraction.HighRoller` | banked extraction value in one match ≥ X | **X = tune-at-playtest** | repeatable | — | 5,000 W | Tarkov extract-value (needs telemetry) |
| 9 | `AFL.Achievement.Weekly.Varied` | complete N varied weekly objectives | reuse `ECON §3` weekly cadence | **repeatable (weekly)** | — | ties to weekly-challenge pool | variety-reward retention (§1.1) |
| 10 | `AFL.Achievement.Season.RankedRegular` | ranked wins this season | ~20 | **seasonal** | — | 10,000 W + seasonal badge | ~1 week ranked for a regular player |
| 11 | `AFL.Achievement.Season.ClimbTheLadder` | reach rank tier ≥ **ARC** this season | rank gate | **seasonal** | — | earned cosmetic + badge | League-ladder tie-in |
| 12 | `AFL.Achievement.Team.<Name>.Champion` | **the prestige/team-ownership gate — see §3** | §3 | seasonal-earned, **permanent once earned** | — | **STATUS** (team standing) | prestige = rarity (§1.4) |

**Distribution check (§1.2):** onboarding = 1–3 (>50%); progression tier-I = 4/5/6-I (~30–50%); tier-II/III
thin to ~15%/~5%; streak/high-value = 7/8 (~5–15%); seasonal = 10/11 (variable); **team prestige = 12 (<5%,
the grail badge).** Reward ceiling = 30,000 W (~a Standard cosmetic) — meaningful, non-inflating; prestige =
status. Curve + economy both honored.

---

## 3. THE TEAM-OWNERSHIP PRESTIGE GATE (`AFL.Achievement.Team.<Name>.Champion`)
Fulfils **League §4.3 + §6.5 flag + B2 #7** (the `/purchase-bundle` seam's reserved `teamGateNote`/`TeamChildId`).

**Research shape (what makes a prestige/loyalty gate feel earned, not grindy or trivial):** loyalty
achievements read as meaningful when they demand **sustained, SKILLFUL commitment tied to the identity** —
skill-gated (competence, SDT §1.1) + season-scoped (refreshable, not a permanent-trivial one-off) + identity-
tied (you must *rep* the team). Not raw volume (anti-grind), not a single trivial action.

**Proposed earn condition (research-fixed SHAPE; the exact cutoff is operator taste — §4):** in a single
season, an account earns `AFL.Achievement.Team.<Name>.Champion` by **(a) owning the team-axis identity
(`AFL.Team.<Name>`) AND (b) representing it while (c) reaching rank tier ≥ ARC** *(mid-ladder, the
`MATCH_STAKING`/League ladder)* **OR finishing top-K in the season TEAM standing.** Skill-gated + season-scoped
+ identity-tied — earned, not bought (the reward is the durable standing itself). Once earned it is
**permanent** (the season's proof of standing persists as the account-bound `AFL.Achievement.*` id).

**One per paid team** (6): ARIA · SCARLETT · MAKHIAVELLI · AP-9 · MOB-FIGAZ · FANATICS (IRONICS is the free
base — no gate).

---

## 4. FIRM-FROM-RESEARCH vs OPERATOR-TASTE (the split)
**Firm (research-fixed — build to these):** the **completion-rate curve** shape (§1.2); **fixed/deterministic
thresholds** (no variable-ratio, doctrine); **tiered 100/1k/10k cumulative ladders** + **low streak
thresholds** (§1.3); **reward bands scaled to match-earn** (¼-match → ~a cosmetic) + **prestige = status-not-
currency** (§1.4); **Watts/earned-cosmetics only, never Volts** (firewall); **reward variety** over volume.

**Operator taste (decide within the research frame):**
1. **Final first-set roster** — §2 is a research-distributed *proposal*; the operator blesses the exact ship list.
2. **The team-ownership cutoff** — research fixes *skill + season + identity-tied*; the **exact rank tier
   (ARC? higher?) / top-K / whether rank-OR-standing / both** is taste (League §6.5).
3. **Tune-at-playtest values** — the `HighRoller` value threshold (#8) + any threshold once **real completion
   telemetry** lands (the research gives the SHAPE + starting numbers; live data refines — same as the
   economy's tuned splits, `PSS:60`).
4. **Which cosmetics** back the tier-III / seasonal / prestige rewards.

---

## 5. Cross-links
- **`IRONICS_LEAGUE_ADVANCEMENT_SSOT.md` §4** — the achievement MECHANISM (taxonomy, gate, the team-ownership
  design); this doc fills its taxonomy with research-grounded values. §4.3/§6.5 = the team gate (§3 here).
- **`IRONICS_ECONOMY_SPEC.md` / `IRONICS_PRICING_SCARCITY_SSOT.md`** — the ~4,000 W/match + peg the reward
  bands (§1.4) calibrate to; `ECON §0` = the deterministic/earned doctrine; `ECON §3` = variety + challenge cadence.
- **`IRONICS_MATCH_STAKING_SSOT.md`** — the rank ladder (ARC…) the seasonal/prestige gates reference; the
  rank-not-buyable firewall (rewards never Volts).
- **`IRONICS_LOOT_SYSTEM_DESIGN.md` §3a** / **`IRONICS_MAP_MODE_SPEC.md` §1.1** — the trigger events
  (head-collect, extraction, eliminate-or-extract win).
- **The achievements build workflow** (scoped) — definitions manifest → progress tracking → award → gate-check
  (= the existing `IsEntitled` over the owned-set). This doc = the parameters that workflow builds to.

---

*v0.1 DESIGN PASS, 2026-07-09 — achievement parameters set from cited research (SDT · flow · goal-gradient ·
reward-schedule · Steam completion-funnel · Halo/CoD/Apex/Tarkov threshold structures) + calibrated to our
economy; the first achievement set + the team-ownership prestige gate; firm-from-research vs operator-taste
split. DESIGN ONLY — no code, no manifest, no mechanism built; thresholds are research-backed starting values,
tune-at-playtest.*
