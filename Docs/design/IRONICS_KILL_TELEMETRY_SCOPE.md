# Kill telemetry as a SECONDARY system — SCOPED, then RULED

**Status: BOTH RULINGS LANDED 2026-08-10. Implementation may begin.**

> **RULING 1 — ADMIN-ONLY.** Kill telemetry does **not** fill R10's career-volume axis. The Career VOLUME
> tab stays unbuilt and R10's axis stays explicitly unsourced. No player-facing kill ladder.
>
> **RULING 2 — A KILL IS A CONFIRMED ELIMINATION ONLY.** See §5.1 for the exact definition this expands to.

The original scope follows unchanged below, so the reasoning behind the rulings stays readable.

**Original status: SCOPE ONLY. Nothing had been built or changed.** Operator direction, 2026-08-10: *"track kills
as a secondary system not affecting ranking, but allowing us to highlight and reward from an Admin or
tournament standpoint. Fully scope impact before changing any of Game Science designed systems."*

---

## 1. THE ONE-LINE ANSWER

**It can be done without touching a single Game Science system, and the reason is R10.** R10 already
requires that volume and rating be *separate axes that must never be conflated* — so a kill counter that
does not feed rating is not a compromise, it is **the shape R10 already mandates**. The risk is not
architectural. It is that a number, once visible, starts being optimised.

---

## 2. WHAT WOULD BE TOUCHED — AND WHAT WOULD NOT

### Not touched (the Game Science systems, all unchanged)

| System | Why it is unaffected |
|---|---|
| **OpenSkill rating** (R64/R65) | Its inputs are BR placement and MATCH PLAY series outcome. A kill count is not an input and must not become one. `update-rating` needs **no change**. |
| **Payout curve** (§5.2, R37) | Denominated in stake units against finishing position. Kills are not in the formula. |
| **Matchmaking** (§2.1, R59/R60) | Consumes rating and stake band. Never kills. |
| **Escrow / settlement** | Money moves on entries and finishing positions. Untouched. |
| **Play limits** (§7/R23) | Exposure and loss. Unrelated. |
| **Career volume** (R10) | ⚠ See §5 — this is the one place a decision is genuinely required. |

### Touched (all additive)

1. **Game server → backend reporting.** The server already knows kills for scoring. One integer per
   player per match, alongside what it already sends.
2. **A new store.** Per-player cumulative + per-match rows.
3. **A read.** Extend `GET /career`, which already carries a `volume` block waiting to be filled.
4. **Admin/tournament read.** The actual purpose — see §4.

---

## 3. THE ARCHITECTURAL RULE THAT MAKES IT SAFE

> **Kill telemetry is written on a path that has no ability to affect rating, payout or matchmaking, and
> is read on a path that no gameplay system consumes.**

Concretely: a **separate table**, a **separate endpoint**, and **no import** from the kill module into
`update-rating`, `settle-match`, or the allocator. If a future change wants kills to influence any of
those, that is a **ruling against R10**, and the import it would require is the tripwire that makes it
visible in review.

Idempotency is not optional. Settlement is retried by design; a double-counted kill inflates a permanent
number. The existing precedent is exact: the play-limits table's `(playFabId, eventKey)` + conditional
write, where `eventKey` carries the `matchId`.

---

## 4. WHAT IT BUYS — the actual requirement

| Use | Needs |
|---|---|
| **Tournament standings** | Per-match kills, queryable by match set. A tie-break or a side-board. |
| **Admin highlight** | "Top 10 this week" — a windowed read, the same shape as the play-limits window. |
| **Rewards** | Kills → an existing grant path (`/earn`, achievements). ⚠ See §6. |
| **Match summary** | A player seeing their own kills post-match. Cheapest and most obviously wanted. |

---

## 5. THE DECISION — RULED: ADMIN-ONLY

**Ruled 2026-08-10: (b).** Kills do not fill R10's volume axis. The reasoning below stood and the
recommendation in §8 was taken.

### 5.1 RULED — what counts as a kill

> **A kill is a CONFIRMED ELIMINATION ONLY.**

That expands to exactly this, and it is the definition any reporter must implement:

| Case | Counts? | Why |
|---|:--:|---|
| Player A deals the damage that eliminates player B | **YES** | The confirmed elimination. The only positive case. |
| Assist (damage, no final blow) | **no** | "Confirmed elimination only" excludes soft credit. |
| A knock/down that is revived | **no** | Not an elimination — nothing was confirmed. |
| A knock/down that later becomes an elimination | **YES**, to whoever confirmed it | The confirming blow is the kill, not the knock. |
| Environmental / world death, no player cause | **no** | No eliminator to credit. |
| Self-elimination (fall, own explosive) | **no** | Same. |
| Death by disconnect / timeout | **no** | Not an elimination. |
| **Team kill** | **no** ⚠ | **My conservative reading, not stated in the ruling** — see below. |

⚠ **TEAM KILLS ARE THE ONE CASE THE RULING'S WORDING DOES NOT SETTLE.** A teammate elimination *is* a
confirmed elimination by the letter of it. Excluded here on the grounds that crediting one would reward
the single behaviour nobody wants and is trivially farmable by two cooperating accounts — which matters
more than usual because §4's whole purpose is tournament standings and rewards. **Flip it if that reading
is wrong; it is one predicate.**

⚠ **The definition is the reporter's job, not the backend's.** The game server is the only thing that
knows how an elimination happened; the backend receives an integer it cannot audit. So this table has to
be implemented at the source, and any later change to it produces a discontinuity in the data that no
migration can repair.

### 5.2 The original analysis (retained)

The question was: does this become R10's career volume?

R10 names volume as **cumulative eliminations** with thresholds at 100 · 500 · 1,000 · 5,000 · 10,000.
A kill counter *is* that quantity. So building this either:

- **(a) Fills R10's volume axis** — the Career VOLUME tab lights up, thresholds become real, and the
  telemetry is player-facing progression; or
- **(b) Stays admin-only** — an internal number for tournaments and highlights, with the VOLUME tab still
  unbuilt and R10's axis still unsourced.

**These are different products and the difference is not a config flag.** (a) makes kills a visible
progression axis, which is exactly what makes players optimise for it; (b) keeps the incentive invisible.

⚠ **Worth raising against R10 either way:** R10 dates from 2026-08-05, before R41 settled BR as N-way
placement and MATCH PLAY as a series outcome. **An attendance axis measured in eliminations rewards
fragging in a game whose ranked outcomes are placement and series wins** — a BR player who farms kills
and places 15th advances the volume ladder while losing the match. If (a) is chosen, that tension is
inherited; if the axis were matches or placements instead, it would not be.

---

## 6. RISKS, WORTH STATING PLAINLY

1. **Visible kills change how people play**, whatever the rules say. This is the whole risk and it is a
   design decision, not a technical one. (b) contains it; (a) accepts it.
2. **"Not affecting ranking" is only true while nothing reads it.** The §3 rule and the absent import are
   what keep it true; a code-review habit will not.
3. **Rewarding kills is rewarding a behaviour.** If kills reach `/earn`, they are an incentive regardless
   of which ladder they sit on. Recommend: **highlight first, reward later**, once real distributions
   exist to set a threshold nobody can farm.
4. **Retries inflate a permanent number.** §3's idempotency requirement; the precedent already exists.
5. **Kill attribution is not free.** Assists, environmental deaths, self-eliminations and disconnects all
   need a ruling before a number means anything. Two servers disagreeing about what a kill is produces a
   leaderboard nobody trusts. **This is the cheapest thing to get wrong and the most expensive to fix
   later**, because early data cannot be re-derived.

---

## 7. RECOMMENDED SEQUENCE

1. **Rule §5** — (a) or (b). Everything downstream depends on it and nothing should start first.
2. **Rule the kill definition** (§6.5) before any code. Write it down.
3. Report + store, idempotent per match, admin-read only. **No player-facing surface yet.**
4. Watch real distributions for a season.
5. *Then* decide highlights and rewards, with numbers instead of guesses.

**Estimated effort once ruled:** report + store + admin read is small — comparable to the play-limits
window, which was one table, one endpoint and two writers. The player-facing half in (a) is a Career
VOLUME tab, which is already stubbed and disabled with a stated reason.

---

## 8. WHAT I RECOMMENDED — AND WHAT WAS RULED

✅ **Taken. Ruled admin-only on 2026-08-10.** The recommendation as written:

**(b) — admin-only, for now**, and keep R10's volume axis explicitly unsourced rather than quietly
filling it with kills.

It gets you everything you named — tournament standings, admin highlights, a basis for rewards — with
none of the behavioural risk, and it leaves (a) fully available later, because the data will already be
there. The reverse is not true: once players can see a kill ladder, taking it away is a visible loss, and
`league-play.md` §9.1 is emphatic that a permanent-looking axis must never go backwards.
