# SSOT — ECONOMY + STORE (Tier 2)

> **PRECEDENCE (ruled 2026-08-24):** the NEWEST economy doc is authoritative; where the newest is SILENT on a question, the prior doc governs. This doc last changed **2026-08-06** (`c8e21989`). Today `IRONICS_PRICING_SSOT.md` (2026-08-19) is newest and `IRONICS_ECONOMY_SPEC.md` (2026-08-05) fills its gaps — e.g. Volt packs, on which the newer doc is silent. The rule exists so a later disagreement resolves without re-ruling.

**What this is:** what the economy, store and loot systems **are** and **why**. It changes when the system is
redesigned.
**What this is not:** a status board. **This document contains no status claims** — nothing here says what is
built, proven, done or owed, and no commit hash appears as evidence of progress. Those belong to Tier 3
(`LIVE_TRACKER`).

> **Why the separation matters here in particular.** Economy documents accumulate phase tables and per-item
> completion marks faster than any other kind, because the work is naturally itemised. The result is a design
> document that reads as a burndown chart and is wrong within a week. **The model is the design; what has been
> authored against it is not.**

**Doctrine is cited, never restated.** Laws live in [`Docs/DOCTRINE.md`](../DOCTRINE.md) and are referenced by id
(**N1**, **N11**, **N12**, **NM5**, **G5**, **P8**).

---

## 1. SCOPE

This SSOT governs: the hard economic invariants · currencies and their roles · the pricing model (both ladders) ·
the earn structure · the cosmetic axis system and its address scheme · the catalog and ownership spine ·
server-authoritative purchase and entitlement · the persistence seam · in-match loot (taxonomy and carry
mechanic) · the store surface · prizes and collection mechanics · and the interfaces owed to matchmaking and
league.

It does not govern: how a player reaches a match or how a stake is banded (→ `ssot/matchmaking.md`), rank and
season structure (→ `ssot/league-play.md`), what a ruleset is (→ `ssot/match-modes.md`), character content
(→ `ssot/character-system.md`), or the state of any implementation (→ Tier 3).

---

## 2. HARD INVARIANTS

These are locked. Breaking any of them is a compliance re-review, not a design change.

| # | Invariant | Why |
|---|---|---|
| **E1** | **PEG, in exact integer units.** `1 Volt = $0.001` · `10 Watts = 1 Volt` · `$1 = 1,000 Volts = 10,000 Watts`. **Balances are integers. Never floats.** | Float currency accumulates rounding error that becomes either a duplication exploit or an unexplainable missing balance. Integers make the ledger exact by construction. |
| **E2** | **NO CASH-OUT, EVER.** One-way buy-in only. **No path converts Watts or Volts back to real money or out of the system.** | This is the load-bearing legal property. It is what makes staking a *structure* applied to in-game tokens rather than real-money wagering. |
| **E3** | **NO RANDOMISED ACQUISITION for purchase.** No loot boxes, no gacha. **All purchases are direct and known-item** — the player sees exactly what they are buying before they buy it. | Randomised paid acquisition is legally radioactive in multiple territories and has produced bans and large settlements. Direct purchase is the transparent norm. *(This governs PURCHASE. Any randomised **reward** path is bound by §13's mitigations.)* |
| **E4** | **One global currency system** (Watts + Volts). Branding may dress it; there are **no per-team or per-region currencies**. | Multiple currencies multiply the ledger surface, the exchange logic, and the ways a balance can be wrong. |

---

## 3. CURRENCIES, AND THE ONE WALLET

| Currency | Type | Source | Spends on |
|---|---|---|---|
| **Watts** | Soft, **earned** | Gameplay (§5) — **and nothing else** | Accessible-tier cosmetics; discounts on higher tiers; **WATTS PLAY stakes** (§3.1) |
| **Volts** | Hard, premium | **Real-money purchase ONLY** (R81) — no earn path exists | Premium and prestige cosmetics; the pass; **VOLTS PLAY stakes** (§3.1) |

### 3.1 BOTH currencies stake, in SEALED pools — and there is still only one wallet

> **WATTS PLAY and VOLTS PLAY are the two staked tiers** (R78). They are the play-money / real-money split every
> card room and every brokerage runs, in our currencies. **A Watts player is a full participant**, not a
> spectator waiting to convert.

**The pools are SEALED.** Two players staking different currencies cannot settle one pot, so the pools never
meet — and **nothing converts between them**: Volts are purchase-only and there is no Watts→Volts path (R81).
That seal is what makes the rest of this section safe, and it is the property to protect. **Every proposal to
"let players grind into Volts" is a proposal to merge the two economies**, because any Watts advantage would
become a Volts stake.

> **⚠ THIS SUPERSEDES R9's HEADLINE.** This section previously read *"Volts are the stake currency, and there is
> only one wallet."* **The first clause is now false; the second is unchanged.** Note the document already
> disagreed with itself: §3's Watts row has always said *"lower stake tiers"*, which was the only sentence in
> the SSOT putting Watts into stakes. That cell was either a fossil or a seed. It is now normative.

**The wallet is still one wallet.** A player holds Watts and Volts, not "stake Watts" and "shop Watts". Three
consequences follow, and all three are design constraints rather than implementation details:

**(a) Each currency's two economies compete for the same balance.** A Volt committed to a stake is a Volt not
spent in the store, and vice versa — and the same is now true of Watts. This is intended: it is what makes a
stake a real decision rather than a free roll. **Neither system can be balanced in isolation**, and after R78
that is a 2×2 rather than a 1×1 — a Watts price change alters Watts staking, and a Watts stake-tier change
alters Watts store conversion. Any tuning pass that touches one must state its expected effect on the other
**three**.

**(b) Sinks and sources must be reckoned per currency.** Staking is a **transfer**, not a sink: it moves
currency sideways and removes none. **The rake is the only true sink in a staked match, in EITHER pool**, which
is why R79 rakes both at 5%. Store purchases are the primary sink. **Earned Watts are the primary source.**

> **⚠ THE HAZARD R78 CREATES, RECORDED SO IT IS NOT REDISCOVERED.** Watts are the faucet. Making the faucet
> currency also a transfer medium means that **without a Watts rake, the primary source would gain a
> circulation loop with ZERO drain.** R79 exists precisely to close that, and an un-raked Watts pool must never
> be reintroduced as a player-friendliness measure — it is the one change that would make the Watts economy
> unbalanceable.

> **⚠ R38's ENFORCEMENT WAS FREE AND NO LONGER IS.** Until R78, faucet-versus-transfer was enforced *by currency
> type*: Watts earned, Volts staked, and the boundary needed no bookkeeping. **Watts are now both.** R38's
> stated purpose — being able to answer *"how much currency did the game create this week"* — now requires
> **explicit faucet/transfer tagging on the ledger**, because the currency type no longer answers it. This is a
> real obligation created by this ruling, not a pre-existing one.

**(c) Server authority over the ledger is absolute** (**N1**, **N11**). One wallet means a defect in *either*
system corrupts *both*: a stake settlement that credits twice inflates the same balance that buys grails, and a
purchase that fails to deduct funds the next stake. **There is exactly one mutation path** (§9), and every
economic operation — purchase, earn, escrow, settlement, reward grant — goes through it. A second write path is
not a shortcut; it is a second place for the ledger to be wrong.

### 3.2 Player-to-player transfer

Any direct transfer capability is a **separate, gated subsystem**, and it inherits **N12** in full:
transactional locking, rollback, anti-duplication, escrow/confirm, and a per-SKU tradeable flag. A transfer path
is not a feature of the wallet; it is its own subsystem with its own failure surface.

### 3.3 The two staked tiers, and why the Watts ladder is cheaper

| Tier | Stake currency | Entry | Rake | Leagues |
|---|---|---|---|---|
| **LEAGUE PLAY** | — | **No buy-in** (R85). Unrated; bots permitted. | — | HAYWIRE + PRO MOD |
| **WATTS PLAY** | Watts | **Earned through play** (§5). Effectively free to anyone who plays. | 5% (R79) | PRO MOD only (R86) |
| **VOLTS PLAY** | Volts | **Purchased. There is no earn path and no conversion** (R81). | 5% (R68) | PRO MOD only (R86) |

**THE WATTS RUNGS ARE `250 · 1,000 · 5,000 · 25,000 W` (R88).** Peg-equivalence to the Volts ladder would be
`1,000 · 5,000 · 25,000 · 100,000 W`, so these sit at **a quarter of it** — comfortably inside R80's rule.
Against the earn structure (§5), the entry rung is **a small fraction of a single match**, which is the point:
a player can stake on their first session without accumulating anything first.

**THE WATTS LADDER SITS BELOW PEG-EQUIVALENCE (R80).** Its rungs are *not* 10× the Volts rungs. A Watts stake
is deliberately cheaper in real terms than the Volts stake beside it.

**This does not touch E1, and the distinction matters.** The peg (`10 Watts = 1 Volt`) is a **store-pricing
conversion** — it says what an item costs in either currency. A **stake tier** is a chosen entry amount, not a
conversion, so setting the Watts rungs low changes no exchange rate and creates no inconsistency. Every card
room with both tables does exactly this: play-money buy-ins are not the real-money buy-ins at an exchange rate,
they are their own ladder.

**It is safe ONLY because the pools are sealed.** With no Watts→Volts path (R81), a cheap Watts stake cannot be
arbitraged into an expensive Volts one — there is no trade to make. **If a conversion path were ever
introduced, R80 would become an exploit rather than a courtesy**, because the cheaper ladder would be a
discount on the dearer one. That is the dependency to remember: R80 rests on R81, not on its own merits.

**The rungs themselves are TUNING and are not set here.** They are calibrated against the real earn rate (§5)
so the entry rung is reachable in a session or two — the same discipline as R39, where the ordering is the
ruling and no rates are fixed. What *is* ruled: four rungs, geometric, below peg-equivalence.

---

## 4. THE PRICING MODEL — TWO LADDERS, AN ITEM USES ONE

An item is priced on **exactly one** of two ladders. Which ladder an item sits on is a product decision made
once, per item.

### 4.1 The standard ladder — unlimited stock, cheap-first

| Rung | USD band | Buy with | Typical contents |
|---|---|---|---|
| **Free** | — | — | Base finish set + base edges + the free base identity |
| **Impulse** | ~$1–2 | Volts **or** Watts | A single colour or edge |
| **Standard** | ~$3–5 | Volts **or** Watts | Signature finishes, masks, standard weapon skins |
| **Premium** | ~$8–15 | **Volts only** | Signature weapon skins, exclusive beams, event masks |
| **Grail** | $500 | **Volts only** | The 1-of-1 container bundle (§4.2, §7.3) |

**Cheap-first is the design, not a discount.** The lowest paid rung must sit at an impulse price reachable in a
handful of matches (§5), because the first purchase is the conversion event that matters; everything above it
is a decision the player makes *after* they already trust the store.

**The Volts-only wall begins at Premium.** Below it, everything is payable either way. This is the soft/hard
split that keeps the free path genuinely complete while leaving premium tiers meaningful — and it is **enforced
at the purchase layer, not merely hidden in the UI** (§8).

### 4.2 The rarity ladder — limited mint, scarcity as the product

| Mint cap | Label | Currency | Discountable | Reissue |
|---|---|---|---|---|
| 10,000 | Static | Watts | yes | **never** |
| 1,000 | Charge | Watts | yes | **never** |
| 100 | Surge | Watts | yes | **never** |
| 50 | Bolt | Watts | **never** | **never** |
| 10 | Tempest | Volts | **never** | **never** |
| **1** | **Singularity** | Volts | **never** | **never** |

**NEVER-REISSUE applies to every limited tier.** The mint count is a hard permanent cap — **the fixed count
*is* the product.** An edition that reopens was never limited, and a single reissue retroactively devalues
every limited item ever sold, including the ones the reissue did not touch. This is the one policy in the
economy that cannot be relaxed "just once."

**NEVER-DISCOUNT cuts in at Bolt (50).** Below that, discounts are a legitimate volume-conversion tool. At and
above it, a discount contradicts the scarcity being sold.

**The curve is deliberately stretched at the top.** The entry tiers cluster low for mass conversion; the 1-of-1
stands at roughly 100× the top Watts tier rather than a flat multiple. A grail priced on a linear extension of
the volume tiers is not a grail — the gap *is* the signal.

### 4.3 Two "rarity" concepts that must never share a field

- **Shop-frame rarity** (Common → Legendary) — a *badge*, drives presentation and base pricing.
- **Mint tier / cap** (the `1-of-N`) — the *scarcity product*.

They are independent: a Common-framed item can be 1-of-50, and a Legendary-framed item can be unlimited.
Collapsing them into one field makes both meaningless and is not reversible once content is authored against it.

---

## 5. THE EARN STRUCTURE

**Watts are earned through a structure, not a flat per-match payout.** The structure is the engagement design;
the flat rate is only its sum.

| Source | Cadence | Purpose |
|---|---|---|
| Match base (win pays more; a loss still pays) | every match | Outcome matters, but losing is never punishing |
| Daily first-win | once per day | The return hook |
| Daily quests | daily | Varied play, a reason to come back |
| Weekly challenges | weekly | Play-across-the-week retention |
| **Combat loot** (§10) | in-match | Rewards varied play rather than kills alone |

**Design principles:** reward variety, not just kills — saves, extraction, loot and objectives all earn. Gate a
meaningful chunk behind *showing up* rather than *grinding*, so a player with an hour is not structurally behind
a player with six. Reward skill, but never make losing feel like it cost you.

**The earn rate is the anchor, and the price ladder is the lever.** When the grind feels wrong, the correct
adjustment is almost always **price**, not earn inflation. Inflating earn devalues every balance already held
and every price already set; adjusting a price affects only the item in question. **Earn stays locked; prices
move.**

### 5.1 BATTLE ROYALE earns on placement (R36)

> **Finishing position determines payout.**

**This is a payout SHAPE, not new machinery.** Placement is already computed, replicated and consumed by
league rating (`ssot/league-play.md` §4). Paying on it adds a consumer to an existing signal rather than a
system.

It also aligns the two things a BATTLE ROYALE match already produces: `ssot/match-modes.md` §2 defines it as
last-standing with no timer and no respawn, so **placement is the only outcome the ruleset generates**. Any
other payout basis would have to invent a second measure of how the match went.

### 5.2 The payout ladder (R37)

**Paid places are keyed on FINISHING POSITIONS, never on player count:**

```
paid places = 1                      if positions < 10
            = ceil(0.15 x positions) otherwise
```

**Depth is 15%, top-heavy (R40).** Poker-standard. It was 30%, and the change is the subject of §5.3.

> **⚠ THE STRUCTURAL POINT, AND IT IS THE WHOLE REASON THIS KEYS ON POSITIONS.**
> **A TEAM MODE HAS EXACTLY TWO FINISHING POSITIONS REGARDLESS OF PLAYER COUNT.** A 5v5 is ten players and
> **two** positions. So `ceil(0.15 × 2) = 1`, and **every team mode is winner-takes-all automatically** — with
> no special-casing, no mode flag, and no branch anyone can forget. **Scaled payouts exist only in FFA
> formats**, because only FFA formats have more than two positions to scale across.
>
> Keying on player count would have required a team-mode exception, and an exception is a thing that gets
> missed. Keying on positions makes the correct behaviour fall out of the arithmetic.

**The payout is a GENERATING RULE computed per exact field size — not a table.** Given `N` finishing positions
and a rake `k`:

```
p(N) = 1  if N < 10,  else  ceil(0.15 x N)      paid places
B(N) = N x (1 - k)                              budget, in STAKE UNITS
M    = 1.4                                      min cash, FIXED, in stake units

Places decay geometrically from M at the last paid place up to 1st,
with ratio r solved so the places sum to exactly B(N):

    M x (r^p - 1) / (r - 1) = B(N)      solve for r > 1
    place i (1 = last paid)  =  M x r^(i-1)
```

**Why a rule beats tables, and it is one property doing the work:**

> **MIN CASH IS AN INPUT, NOT AN OUTCOME.** In a percentage table the min cash is whatever the lowest
> percentage happens to yield against whatever the pot happens to be — so it *drifts*, and it drifts most
> exactly where the pot is smallest. Here the floor is fixed at `M` and the ratio `r` absorbs the variation.
> **The floor cannot be missed, because it is the thing being solved around.**

Two consequences follow for free: **there are no bands, so there are no band-bottom failures** — every field
size gets its own exact solve rather than inheriting a neighbour's percentages. And **the whole structure is
one rule rather than four tables that must be kept mutually consistent**, which removes an entire class of
transcription and drift error.

**Reference values at 5% rake — DERIVED FROM THE RULE, not authoritative in themselves.** The rule is the
source of truth; these are spot checks recomputed from it.

| Positions | Paid | Winner | Min cash |
|---|---|---|---|
| 2 | 1 | 1.90× | 1.90× |
| 9 | 1 | 8.55× | 8.55× |
| 10 | 2 | 8.10× | **1.40×** |
| 13 | 2 | 10.95× | **1.40×** |
| 18 | 3 | 11.66× | **1.40×** |
| 20 | 3 | 13.29× | **1.40×** |
| 26 | 4 | 13.84× | **1.40×** |
| 33 | 5 | 14.72× | **1.40×** |
| 36 | 6 | 13.29× | **1.40×** |

At `p = 1` there is no ratio to solve: the single paid place takes the whole budget, which is why small fields
show the winner and the min cash as the same figure.

> **⚠ EVERY PAID-PLACES THRESHOLD STEPS THE WINNER DOWN — a QUEUE-DESIGN concern, not a player-facing one.**
> At 9 positions the winner takes **8.55×**; at 10 they take **8.10×**. That discontinuity cannot be smoothed
> away: **any jump in paid places creates one**, because the same budget is suddenly divided more ways. It is a
> property of paying more places, not a defect in the curve.
>
> **9→10 IS THE MILDEST OF THE FIVE, AT −5.3%** — it is the one place where the small-field clause hands off to
> the formula, a single paid place becoming two. **The four that matter are N = 14, 21, 27 and 34, where the
> winner drops 17–23%** (§5.3, §15.9). Read 9→10 as the exception, not the representative case.
>
> **It never reaches a player, because a player never meets both sides of a threshold in the same queue.**
> Queues have fixed field sizes, so a given queue always resolves on one side, never both.
>
> > **⚠ CORRECTED 2026-08-06.** This paragraph previously justified itself with *"bot-fill lands them full
> > (R34, `ssot/ai-bots.md` §8.2.1)"*. **That citation was wrong twice.** §8.2.1 says nothing of the kind — R34
> > is about capability gaps on new features — and **`ai-bots.md` §6.3 forbids bot-fill outright in any match
> > whose result carries stake or rating**, which is precisely the set of matches this ladder governs.
> > `ssot/ai-bots.md` **R74** now closes the door completely: bots never enter a population count. **The
> > mitigation never needed bots** — the fixed-field-size property and the design constraint below carry it
> > alone. Recorded rather than silently deleted, because a mitigation resting on a prohibited mechanism is the
> > kind of error that gets re-introduced by someone reading only this section.
>
> **DESIGN CONSTRAINT, recorded so it stays true: NO QUEUE MAY SIT AT A FIELD SIZE WHERE THE PAID-PLACES
> THRESHOLD STRADDLES IT.** Cheap to honour when queue sizes are chosen, and it keeps the steps permanently
> away from players. The thresholds are wherever `p(N)` increments — **N = 10**, where the small-field clause
> hands off, **and N = 14, 21, 27 and 34**, where `ceil(0.15 × N)` steps — so a queue sized at one of those
> should be moved a position either way. **R40's 15% depth more than halved this list**, from nine thresholds
> to five, which makes the constraint materially easier to honour.

### 5.3 The two invariants — one now holds by construction, one does not

**These matter more than any particular number.**

**1 — MIN CASH NEVER BELOW ~1.4× STAKE.** Paying 30% of a field is deeper than poker convention (10–15%), so
the bottom of the curve thins fast. **A min cash near 1.0× means cashing feels like nothing happened** — the
player survived most of the field and got their stake back, which reads as a loss of time rather than a win.

**2 — THE WINNER'S MULTIPLE GROWS WITH FIELD SIZE.** A 36-position win should feel larger than a 10-position
win, or field size stops meaning anything.

> **✅ INVARIANT 1 HOLDS FOR EVERY FIELD SIZE.** Verified by solving the rule at 5% rake for **every N from 2
> to 36**: min cash is exactly **1.40×** at every N ≥ 10, and at N < 10 the single paid place takes the whole
> budget (1.90× at N=2, rising to 8.55× at N=9). **Zero failures.** It holds *by construction* — the floor is
> an input, so the only way to miss it would be a field so small the budget cannot cover one minimum payout,
> which first occurs below N=2 and is therefore unreachable.

> **◑ INVARIANT 2 — SUBSTANTIALLY IMPROVED BY R40, BUT NOT RESOLVED. Recorded honestly rather than closed.**
>
> **The trend now genuinely rises.** Both the floor and the peak of each paid-places band increase
> monotonically, which was not true at 30% depth:
>
> | Paid | N range | Band floor | Band peak |
> |---|---|---|---|
> | 1 | 2–9 | 1.90× | 8.55× |
> | 2 | 10–13 | 8.10× | 10.95× |
> | 3 | 14–20 | 8.46× | 13.29× |
> | 4 | 21–26 | 10.46× | 13.84× |
> | 5 | 27–33 | 11.25× | 14.72× |
> | 6 | 34–36 | 12.29× | 13.29× |
>
> **But it still sawtooths — five drops, not zero**, at exactly the paid-places thresholds:
>
> | Crossing | Before | After | Drop |
> |---|---|---|---|
> | 9 → 10 | 8.55× | 8.10× | −5.3% |
> | 13 → 14 | 10.95× | 8.46× | **−22.8%** |
> | 20 → 21 | 13.29× | 10.46× | **−21.3%** |
> | 26 → 27 | 13.84× | 11.25× | −18.8% |
> | 33 → 34 | 14.72× | 12.29× | −16.5% |
>
> **A smaller sawtooth is still a sawtooth, and this one is not small.** Drops of 17–23% mean a player in a
> 14-position field wins less than one in a 13-position field, by a margin they would notice. What changed is
> the count (8 → 5) and the direction of the trend, not the existence of the discontinuity.
>
> **It remains structural.** Every increment of `p` divides the same budget one more way, so the winner's share
> must fall at each threshold — that is arithmetic, not tuning. Reducing depth raised the whole curve and made
> the trend rise; it could not remove the steps. **Eliminating them entirely requires a continuous paid-places
> function**, which `ceil()` is not. Kept open as §15.9 with these numbers, because the arithmetic has not
> closed it.

> **HOW THE PREVIOUS STRUCTURE FAILED, kept so the reasoning is not lost.** The superseded version used
> **fixed percentage tables across four bands**. That produced min-cash figures of **1.06×** at 14 positions
> and **0.80× at 21 positions — a "cash" that was a net loss** — plus a winner sawtooth where 9 positions paid
> 8.55× against 10 positions paying 4.75×.
>
> **The root cause is worth carrying forward as a general lesson: a PERCENTAGE OF A POT CANNOT HOLD A FLOOR
> WHEN POT SIZE VARIES WITHIN THE BAND.** The percentage is fixed; the pot is not; so the product drifts, and
> it drifts furthest at the band's small end where the floor matters most. Any future payout structure keyed
> on percentages rather than absolute floors will reproduce this.
>
> It was found by checking the arithmetic against the stated invariants rather than transcribing the tables —
> the tables were internally consistent (all four rows summed to exactly 100) and still wrong against their own
> stated goal.

### 5.4 Staking is orthogonal to earning (R38)

> **Earning is what a match PAYS. Staking is what a wager SETTLES. They are separate in the model.**

A stake is a **closed loop**: participants put up Volts, the server escrows them, and the pot settles per the
§5.2 ladder minus rake. Nothing enters or leaves that loop except the rake.

**Why the separation is worth enforcing rather than merging.** They look similar — both end with currency
moving on match conclusion — but they answer different questions and have different failure modes. Earning is
a **faucet**: the game creates currency for play, and its risk is inflation. Staking is a **transfer**: players
move currency between themselves, and its risk is an unbalanced loop. Merging them makes a change to one
silently a change to the other, and makes it impossible to answer "how much currency did the game create this
week" — which is the question economy health depends on.

**This reinforces `ssot/league-play.md` §5 from the economy side.** That section forbids stake from influencing
*rating*; R38 forbids it from being conflated with *earning*. **No conflict** — both isolate the wager from
something it must not contaminate.

### 5.5 The earn ladder across modes (R39)

> **Extraction pays most. Placement pays modestly.**

Extraction carries the **highest risk and the longest commitment** — a player holds value through a match and
can lose it at the end (§11).

**The design constraint, stated without inventing rates:**

> **If a duel out-earns an extraction run per minute, extraction stops being worth its risk and loot becomes
> decorative.**

> **⚠ R41 EXPOSED AN AMBIGUITY IN THIS LADDER — flagged, not resolved.** `ssot/match-modes.md` §2.2 records
> that completing a central-extract bank is **one of MATCH PLAY's two round-win routes**, not only a mode of
> its own. So "extraction" names two things in this document: a **round-win route inside a ruleset**, and the
> **hold-value-through-a-match loop** §11 describes. **The ladder above means the second.** Whether banking a
> round-win should earn at all, and how that relates to the run, is **not ruled here** — it is a new question
> R41 surfaced rather than one it answered.

That is the failure to design against. Extraction's entire tension is *carrying something you can lose*, and
that tension only exists if what you are carrying is worth more than what a safer mode pays for the same time.
Get the ratio wrong and players rationally stop extracting — the loot system keeps running, keeps generating
drops, and stops mattering. **Per-minute is the comparison that matters**, not per-match: a duel is short, so
a modest duel payout can still out-earn extraction on rate.

**No rates are set here.** The ordering is the ruling; the numbers are tuning.

---

## 6. THE COSMETIC AXIS SYSTEM

**Cosmetic axes are independent, individually-ownable categories — not sub-attributes of an item.** Each axis
has its own SKU namespace, its own selection field, and its own consumer. A look is **composed at runtime** from
one selection per axis.

| Axis | Owns | Composed by |
|---|---|---|
| **Identity** (character / team) | The *who* — the emblem, the body | The part-selection path |
| **Finish** | **Colour, and only colour.** Logo-less. The sole colour source | The material-parameter apply |
| **Mask** | Face geometry / material | The part-attach path |
| **Weapon** | The weapon as an owned item | A soft reference to its equipment definition |
| **Accessory** | Per-identity attachments | The same part-attach mechanism |
| **Bundle** | A set of the above | The entitlement grant loop |

### 6.1 The conflict-prevention rule

> **An identity NEVER encodes colour.** There is no "red Draco" identity — there is *Draco* (identity, owns the
> emblem) composed with *Red* (finish, owns the colour).

This is enforced structurally rather than by convention: the finish data carries no logo, mesh or identity
field, and the identity's logo parameter is untouched by a recolour. **Colour physically cannot live in an
identity, and an identity cannot carry a colour name without it being dead data.**

**Why this matters more than it looks:** it is the property that lets a large roster share a colour palette and
stay distinct. **Distinctness is by emblem; colour is a free axis anyone applies.** It also collapses the
authoring surface from *identities × colours* to *identities + colours* — the difference between authoring a
few dozen assets and a few hundred, for an identical result.

### 6.2 Independence is the general rule

The same logic governs every axis: a weapon skin is not a property of a weapon, an edge is not a property of a
finish. Each is separately ownable, separately priced, separately tradeable, and separately selected. **A new
cosmetic category is a new axis — never a field on an existing one.**

> **⚠ Where the store's boundary now falls on weapons (R94, `ssot/combat-arsenal.md`).** The independence
> above is exactly what lets **weapon FUNCTION leave the store while weapon APPEARANCE stays in it**. Every
> combat-affecting weapon is free or earned through play — because **R86** makes staked play PRO MOD only, and
> a purchasable combat advantage inside matches that settle a balance is the one thing this product cannot
> sell. **Weapon skins are unaffected**, precisely because §6.2 already holds that a skin is a separate
> ownable axis rather than a property of the weapon. **The rule to carry: the store may sell how a weapon
> looks, never what it does.**

### 6.3 Metadata is not the address

Rarity, content tier, collection/family, and colour-family are **descriptive metadata on the catalog entry**,
never encoded in the id. Colour-family is a *filter* ("show me the red-family identities"); the finish is the
*address*. They never collide, because one describes and the other selects.

---

## 7. THE CATALOG AND THE OWNERSHIP SPINE

### 7.1 The id is the join key

Every ownership, entitlement and selection key is the **fully type-qualified id** (`AFL.<Type>.<Name>`), never
a bare name. The type qualifier is part of the key, so an identity and a team of the same name are different
ids that cannot collide.

**A shipped id is never renamed** (**NM5**). Grouping and re-categorisation happen in metadata (§6.3), never by
changing an id — a renamed id orphans every ownership record that references it.

### 7.2 References are soft

Catalog entries reference their assets **softly**. A catalog that hard-references its assets loads the entire
content set whenever the catalog loads — and the catalog is loaded by the front end. This is the same
constraint the matchmaking pool layer operates under (`ssot/matchmaking.md` §3.2), for the same reason.

### 7.3 Bundles are a grant-many wrapper, not a second system

A **bundle** is a SKU whose ownership grants a **set of child SKU ids**. One purchase deducts once and grants
every child id into the owned set, atomically — or grants none. Children resolve individually afterwards,
exactly as if bought separately.

**The grail exception — container-locked children.** A 1-of-1 bundle trades **only as an intact unit**; its
children are locked to the container and cannot be separated out while bundled. This is not a contradiction of
general tradeability — it is what keeps "1-of-1" *true*. If the children could be sold off individually, the
1-of-1 would quietly become several items with several owners, and the scarcity being sold would evaporate.

### 7.4 Complete registration — no item may depend on a fallback

**Every item must carry a complete, explicit set of registrations.** A fallback exists to catch *unregistered*
ids; it is never the intended path for a registered one.

> **An item that behaves correctly only because a global fallback happens to match its intent is a BUG** —
> correct-by-accident, not correct-by-registration. It must be flagged and fixed, because the day the fallback
> changes for an unrelated reason, the item silently breaks and nothing points at the cause.

This applies to every per-item map and registration, current and future. **G5** is the related law: a data asset
with no consumer is inert — and an item with no registration is worse, because it *appears* to work.

---

## 8. SERVER-AUTHORITATIVE PURCHASE AND ENTITLEMENT

### 8.1 The server decides what you own

The client **requests**; the server **decides** (**N11**). A client-side cache is permitted for offline display
only — read-only, overwritten by the server on login. It never decides ownership.

### 8.2 Price is enforced by the catalog, and the failure mode is specific

**The price charged is the price in the catalog, read server-side at purchase time. A client-supplied price is
never trusted** — a client that names its own price will eventually name zero.

**The failure mode to design against: a seed price and a catalog price that disagree.** When an item's price
exists in more than one place — a seeding/bootstrap value and the authoritative catalog entry — they drift, and
the drift is silent. The player is shown one price and charged another; whichever is lower becomes an
unintended discount, and whichever is higher becomes a support ticket and a refund. Worse, the discrepancy
appears only for items whose price was *changed*, so it survives every test written against the original value.

> **Rule: exactly one authority for price — the catalog entry.** Any seed or bootstrap data is a *fixture for
> an empty catalog*, never a parallel source of truth, and must be either derived from the catalog or absent.

### 8.3 The check-and-deduct must be atomic

Splitting *check balance* and *deduct* into two operations is the textbook double-spend race: two simultaneous
purchases both pass the check, and both deduct. **The balance test and the deduction are one indivisible
operation** — and when a networked store backs the wallet, "indivisible" must mean a single conditional
database write that succeeds only if the balance still covers it, not two calls that happen to be adjacent.

### 8.4 Entitlement is a real gate, never a permissive stub

**An entitlement check that returns "yes" while unimplemented is worse than no check at all.** It looks like
enforcement in every test, in every review, and in every playthrough — and it is enforcement nowhere. The
moment it is relied upon, everything is free and nothing reports an error.

> **Rule: an unimplemented gate FAILS CLOSED.** If entitlement cannot be resolved, the answer is *no*. A
> permissive stub is a security hole wearing the costume of a feature.

The same rule governs ability-bearing cosmetics: where a cosmetic grants an ability, the grant is
server-authoritative, so an ability can never be spoofed onto an unentitled item.

---

## 9. THE PERSISTENCE SEAM — ONE WRITE PATH

**All economy mutation flows through a single persistence seam.** Purchase, earn, escrow, settlement, reward
grant, consumable decrement — every operation that changes what a player owns or holds goes through the one
interface.

**Why a single seam is a requirement and not a preference:**

1. **It is the only place invariants can be enforced.** The peg, integer-only balances, non-negative clamps and
   atomicity are properties of *the write*. Two write paths mean two implementations of the same invariants, and
   the second one is always the one that is subtly wrong.
2. **It is the audit boundary.** A staked economy must be able to answer *"why does this player have this
   balance"*. One path produces one ordered history; two paths produce two partial histories that must be
   reconciled — and reconciliation of an economic ledger is exactly the work nobody has time for during an
   incident.
3. **It makes the backend swappable.** Session-only, local, or a networked store are implementations behind the
   same seam. Anything that writes *around* the seam is pinned to whatever backing existed when it was written,
   and becomes the reason the backend cannot be changed.
4. **A bypass is invisible until it matters.** Code that writes directly still *works* — until an operation
   needs to be durable, audited, or rolled back, at which point the bypass is discovered in production.

> **Rule: no economic write bypasses the seam. A direct write is a defect regardless of whether it currently
> produces the right number.**

**Durability is a distinct property from authority.** Ownership can be fully server-authoritative and still be
session-scoped. Everything durable — trade, limited-edition mint counts, sold-out state, cross-session
ownership — depends on the seam having a durable implementation behind it, and **a mint cap cannot be enforced
without durable state**: a counter that resets cannot make a 1-of-1 true. **P8** applies to that backend: it is
proven standalone before anything integrates against it.

---

## 10. LOOT: TAXONOMY

In-match loot is a generalised system with one shared core, not a family of bespoke pickups.

### 10.1 Two value domains

| Domain | What it grants | Budget impact |
|---|---|---|
| **Economy** | Watts / carried energy | Counts against the per-match earn budget (§5) |
| **Gameplay-resource** | Ammo, health, equipment | **Not currency.** No peg impact; affects combat balance and needs its own balance pass |

**Keeping these separate is the point.** A gameplay resource that quietly grants currency breaks the earn
budget; a currency drop that quietly restores health breaks combat balance. Every loot category declares which
domain it is in.

### 10.2 The four axes — every category is a point in this space

| Axis | Options |
|---|---|
| **Retrieval mode** | **INSTANT** (walk-over) · **CARRY** (deliberate, contestable) · **HARVEST** (channel-over-time yield) |
| **Value model** | Instant-Watts · carry-to-extract value · reattach/no-grant · gameplay-resource · item/SKU grant |
| **Eligibility** | Anyone · enemy-only · team-only |
| **Lifetime** | Persist-until-looted · timed despawn · owner-death-vanish · regenerating |

### 10.3 The shared core

- A **loot contract** any object honours regardless of its base class — value, value model, eligibility, an
  on-looted hook, and a lifetime policy. An interface rather than a base class, because loot objects are
  otherwise unrelated (a severed part, an energy mote, a placed cache) and reparenting them to a common
  ancestor would be the wrong coupling.
- A **grant component** owning value dispatch, the eligibility branch, and a **grant-once guard**.
- **Retrieval substrates** per mode, one each for instant, carry and harvest.
- **Server-authoritative throughout** — spawn, eligibility, grant and consumption are server decisions; objects
  replicate, and rest dormant when idle.

**A loot object is therefore: a retrieval substrate + the grant component + a config.** New loot is a
configuration, not a new system — which is the property that keeps the category list open-ended.

### 10.4 The owner branch

Dismemberment loot is **enemy-only**: an opposing player retrieving a severed part banks value, while the
**owner** retrieving their own part **reattaches it and is granted nothing**. You do not profit from your own
body.

This is a design rule with an economic consequence: because the enemy-collect grant is an **earn source**, it is
an anti-fraud surface. Feeding parts to a colluding opponent manufactures currency, and the integrity layer
that guards it belongs to the economy (it is the currency being debased), not to staking alone.

> **⚠ R78 ESCALATED THIS FROM AN ECONOMY PROBLEM TO A STAKING ONE (R83).** When Watts bought only cosmetics,
> manufactured Watts inflated a shop balance. **Watts now fund a wager.** The same collusion loop that was a
> debasement is now a way to arrive at a staked table with currency that was not earned — which is a different
> class of harm, because the counterparty is another player rather than the pricing curve.
>
> **Consequence, stated as a gate rather than a wish: the integrity layer guarding this surface is a
> PREREQUISITE for shipping WATTS PLAY, not a follow-on.** This ruling does not design the detection — that is
> deliberately not specified here, for the reason `ssot/league-play.md` §11 gives about publishing heuristics.
> It moves the work from *"economy's problem, eventually"* to *"blocks a tier from shipping"*, so it cannot be
> discovered after launch. Note the loop is **Watts-only**: Volts are purchase-only (R81), so there is no
> manufacturing path into the VOLTS PLAY pool at all.

**Value ordering is deliberate**: the head is worth many times a limb, and arms outrank legs. The ordering
makes the drop sequence meaningful — the cheap parts go first under fire, and the prize clings longest.

---

## 11. LOOT: THE CARRY MECHANIC

### 11.1 Two kinds of grabbable, de-conflated

| | **COLLECT (loot)** | **CARRY-OBJECT (map object)** |
|---|---|---|
| Destination | A fungible carried pool | The hero's hand |
| Hands occupied? | **No** — move and shoot freely | Yes — carry pose, weapon stowed |
| At risk? | **Yes** — a portion on hit, the remainder on death | Per-object policy |
| Banked when | **Extraction** | n/a |

**Collecting loot must not occupy your hands.** A player carrying value should still be able to fight — the
tension comes from *risk*, not from being disarmed. Disarming the carrier converts a risk decision into a
chase, which is a different and worse game.

### 11.2 The carried pool is fungible

Collected value goes into a **single fungible carried pool**, not a set of discrete objects. One pool is the
carried-at-risk value. **The wallet is banked and safe; the pool is carried and at risk; extraction is the
bridge between them.** That three-way distinction is the whole extraction loop in one line.

### 11.3 Friction is the channel, not the hands

CARRY retrieval is a brief **timed, interruptible channel** — approach, hold briefly, complete. Taking a
confirmed hit cancels it; so does moving away. INSTANT retrieval has no channel.

This restores the meaningful distinction between instant and deliberate loot as **exposure time** rather than
occupied hands: you stand still and vulnerable for a moment to claim the higher-value thing. The channel needs
visible progress feedback, because an interruptible action without progress feedback reads as a bug.

**The channel is a general substrate, not a loot feature** — timed, interruptible, progress-reporting actions
recur (harvesting, capturing, extracting). It is authored once and consumed by each.

### 11.4 At-risk behaviour: a portion on hit, the remainder on death

A carrier who takes a confirmed hit **scatters a portion** of the carried pool as recoverable world loot; death
scatters the remainder. A grace period prevents a single burst from stripping the whole pool.

**Why a portion rather than all-or-nothing:** dropping everything on the first hit makes carrying value
unplayable under any pressure, and dropping nothing until death makes the carry risk-free until it is
total. A portion on hit creates the actual decision — *keep pushing, or disengage and bank what is left* —
which is the moment the extraction loop exists to produce. It also keeps the scattered value **in play** and
contestable rather than deleted.

---

## 12. THE STORE SURFACE — SIMPLIFY THE OFFERING

**Fewer, clearer choices. The store is not a catalogue dump.**

**The principle:** choice overload reduces both conversion *and* satisfaction. Past a small number of options a
buyer stops comparing and starts deferring — and a deferred purchase is usually an abandoned one. Worse, the
buyer who *does* choose from an overwhelming set is measurably less happy with what they picked, because the
alternatives they did not evaluate remain live as regret.

**What this implies for surface design:**

- **The catalog is large; the store surface must not be.** These are different things, and the store is a
  *curated view* onto the catalog, never a rendering of it.
- **A visit should present a small, comprehensible set** — enough to find something, few enough to decide.
- **Rotation is how breadth is served over time** rather than depth-per-visit. Content earns its turn in front
  of the player instead of permanently occupying a grid cell.
- **Owned items leave the buying surface.** Anything already entitled is not merchandise; showing it is noise
  that pushes real options off the screen.
- **Search and filter serve intent, not browsing.** A player who knows what they want should reach it directly;
  that path is separate from the curated surface and does not justify expanding it.
- **Every surface has one job.** A screen that sells, showcases, and manages inventory simultaneously does none
  of them well.

*(Specific counts, rotation cadence and slot layouts are product decisions made against this principle, not
fixed here.)*

---

## 13. PRIZES AND COLLECTION MECHANICS

### 13.1 The design

A **series-collection mechanic with a monthly prize**: players complete a collection over a period, and
completion enters them for a prize. **Difficulty target: hard but achievable for a dedicated player** — a
prize nobody wins is a broken promise, and one everybody wins is not a prize.

**Prizes are donated items with no cash value, and are not redeemable for cash through us.** Eligibility is
**territory-gated**.

### 13.2 The mitigations are first-class requirements, not afterthoughts

Any mechanic that combines *paid participation*, *randomness*, and *a prize of value* attracts regulatory
attention in a number of territories, and the specific rules vary and change. The following are **designed in
from the start**, not bolted on if challenged:

| Requirement | What it means |
|---|---|
| **Published odds** | **Any randomised reward with a paid path publishes its odds**, plainly and before participation — not buried, not expressed only as a rarity word. |
| **A free entry route** | There is a **genuine, non-purchase path to enter** — comparable in practical terms, not a token gesture that exists only on paper. |
| **Territory eligibility gating** | Eligibility is enforced by territory (§`ssot/matchmaking.md` §6 is the region source), and the gate is **applied at award time**, not merely disclosed. |

> ### ⚠ THE FRAMING WARNING — READ BEFORE BUILDING ON THIS
> **"No cash value" is NOT, on its own, a sufficient justification, and this document does not record it as
> one.** It is one relevant fact among several. Whether a mechanic is regulated turns on the combination of
> paid entry, chance, and prize — and "the prize cannot be cashed out" does not by itself remove a mechanic
> from that combination in every territory.
>
> **This design is recorded as designed-pending-counsel-review.** The mitigations above are included because
> they are the standard, defensible baseline — *not* because a legal conclusion has been reached here. **Anyone
> building on this section must treat counsel review as a prerequisite, not a formality.** A future reader who
> finds only "no cash value, therefore fine" has been given an incomplete justification and would be exposed by
> relying on it — which is precisely why that framing is rejected here in writing.

### 13.3 Relationship to the invariants

**E3 forbids randomised *purchase*** — you cannot buy a randomised box of cosmetics. A prize draw is a
different shape: the item is not sold, participation has a free route, and odds are published. **The two must
not be allowed to blur**: a "prize" that is functionally a paid randomised item purchase is an E3 violation
wearing different words, regardless of what it is called.

---

## 14. INTERFACES OWED

### 14.1 To matchmaking — the settlement interface

Economy consumes, per match: the **match id** (the binding key), the **participants and their escrowed
entries**, the **outcome ordering** the payout curve reads, and the **terminal state** — settled,
cancelled-refund, or **held-pending-review**. The third state is required so an integrity review can hold a
settlement without either paying out or refunding.

Economy provides: **escrow at seat-take** and **settlement at result**, both atomic and both reversible
(**N12**). A match that never forms returns every entry in full and strands none.

### 14.2 To league — the reward grant interface

League determines *who has earned what*; economy performs the grant. The interface carries the **recipient**,
the **entitlement or currency granted**, and an **idempotency key** — season rewards are computed in bulk and
retried, and a retry that grants twice is an inflation bug that is invisible until someone audits totals.

**Economy never decides eligibility, and league never writes the ledger.** Each system's authority stops at the
other's boundary, and the grant crosses it exactly once, through the seam (§9).

---

## 15. OPEN DESIGN QUESTIONS

1. ~~**Sink/source balance for Volts under staking.**~~ — **CLOSED: the SIGNAL is ruled, the threshold is
   tuning.** The question asked *what signal indicates the rake is insufficient*. Three instruments, in
   priority order: **(a) total float per currency**, **(b) the staked-to-spent ratio**, **(c) float velocity**.
   The actionable condition is **float growing faster than store conversion across a full season** — a
   season-length window because staking volume is spiky and any shorter window reports noise as a trend. Naming
   the instrument is the design decision; the trigger value cannot be set before there is a distribution to set
   it against, and inventing one now would be a number nobody could defend. **After R78 all three are measured
   PER CURRENCY**, and the Watts series is the one to watch — it is the faucet.
2. ~~**The pricing-ladder rationale, revisited against real behaviour.**~~ — **CLOSED: the response was
   already ruled; only the trigger was missing.** §5 fixes the mechanism — ***"earn stays locked; prices
   move"*** — so the answer to a mis-converting ladder is never earn inflation, which devalues every balance
   already held. What was open is *when*. The trigger is **impulse-rung conversion**: if the lowest paid rung
   is not the most-purchased rung by unit count, the ladder is mis-anchored at the bottom, and the bottom is
   where the conversion event lives. **Bands move; content does not re-tier** — re-tiering breaks the address
   discipline **NM5** protects.
3. ~~**How randomised-reward odds are surfaced.**~~ — **CLOSED on disclosure best practice, which is also
   the regulatory floor.** Odds appear **on the same screen as the action, before it is taken, as plain
   numbers** — not in a help page, not behind a link, not as a rarity word. Two properties make it real rather
   than nominal: the figures are **per-item**, not per-tier bucket, and they are shown **at the point of
   commitment** rather than at first launch — a disclosure a player saw once and cannot re-find is a disclosure
   that did not happen. This is what *genuinely understood rather than merely disclosed* means in practice.
   Multiple territories now require published odds; meeting the strictest of them everywhere is cheaper than
   maintaining a per-region surface, and it is what a player deserves anyway.
4. ~~**Whether prize-series inventory is finite or generated.**~~ — **CLOSED by R90: HYBRID.** A small
   **finite headline set**, with a hard end and a real scarcity story, over a **generated long tail** that
   sustains indefinitely. To this item's own third question: **"sold out" therefore exists for some rows and
   not others**, and the UI must show which is which — a generated prize presented as scarce is exactly the
   dishonesty §4.2's never-reissue policy exists to prevent. The headline set inherits never-reissue in full;
   the tail never claims a count.
5. ~~**The free-entry route's shape.**~~ — **CLOSED by R89: EARNED THROUGH PLAY.** Entries accrue from
   playing, at a rate that makes the free path **practically comparable** to the paid one — which is the exact
   standard this item set, and the reason the alternatives lost. A periodic grant is a pure faucet with no play
   requirement; a request path is the most conventional legally but is a manual process, and the option most
   likely to be *nominally* available while not being *practically* comparable. **Earned entry is automatic,
   needs no review, and cannot be gamed further than the earn structure already can** — and it reinforces the
   whole tier design, where play is already the way in.
6. ~~**Whether the two economies ever need a firewall.**~~ — **ANSWERED by R78, and not in the direction this
   item anticipated.** It expected a possible *store-versus-staking* firewall inside one currency. What landed
   instead is a **currency-versus-currency** seal: WATTS PLAY and VOLTS PLAY are separate pools with no
   conversion between them (R81), while store and staking continue to share a wallet **within** each currency.
   The standard this item set was met — the move was *"entered deliberately"* — and its warning about
   reintroducing multi-currency complexity is answered by **E4's actual text**, which forbids *per-team and
   per-region* currencies and has always blessed Watts + Volts as one system. **Two staked pools are not two
   currency systems.** What the item was right about: the complexity is real, and §3.1's faucet/transfer tagging
   obligation is where it lands.
7. ~~**⚠ THE RAKE RATE**~~ — **CLOSED by R68: FLAT 5%.** The working assumption is now the ruling. It was
   taken deliberately rather than optimised: **the rake is the only true sink in a staked match** — in EITHER
   pool, which is why R79 rakes both (the phrase here read *"the only true Volt sink"* until R78 made Watts a
   staked currency too) — so
   it carries an economy-health job alongside the revenue one, and a single predictable number is worth more
   than a tuned curve before any volume exists. **Tiering is a revenue-optimisation move and it needs the stake
   distribution first — you cannot tune a curve you have never observed.** Revisit with data. ⚠ **If it is ever
   tiered, industry practice is REGRESSIVE** (lower % at higher stakes); the legacy
   `design/IRONICS_MATCH_STAKING_SSOT.md` R1 specifies a **progressive** 5%/10%, which is backwards and is
   **superseded by R68**. And whatever rate is chosen, **§5.3's 1.40× min-cash floor must be re-verified**,
   because the generating rule solves against `B(N) = N × (1 − k)`.


8. ~~**The band-edge behaviour.**~~ **RETIRED** — the generating rule (§5.2) resolves it. Bands no longer
   exist, so band-bottom failures cannot occur, and min cash holds at exactly 1.40× for every field size.
   What the question found is preserved in §5.3 as the general lesson: *a percentage of a pot cannot hold a
   floor when pot size varies.*
9. ~~**◑ INVARIANT 2 — the winner's multiple sawtooths at paid-places thresholds.**~~ — **CLOSED by R91: it is
   real in the abstract and UNREACHABLE IN PLAY.** §5.3 is right that removing it needs a *continuous*
   paid-places function and that `ceil()` is not one — so the sawtooth cannot be designed away. **But §5.2
   already carries the mitigation, and it is complete:** queue field sizes are FIXED, and *"no queue may sit at
   a field size where the paid-places threshold straddles it"* (N = 10, 14, 21, 27, 34). A player therefore
   never meets both sides of a step, because a given queue always resolves on one side of it. **The remaining
   17–23% drops are a property of the curve, not an experience anyone has.** Reopen only if variable-size
   queues are ever introduced — at which point the mitigation genuinely lapses and this becomes live again.

---

## 16. RULINGS OF RECORD

| Ruling | Date | Content |
|---|---|---|
| **R89 — THE FREE-ENTRY ROUTE IS EARNED THROUGH PLAY** | **2026-08-06** | §13.2 requires a no-purchase route; it accrues **from playing**, at a rate that makes the free path **practically comparable** to the paid one — the standard §15.5 set. Beat a periodic grant (a pure faucet with no play requirement) and a request path (conventional, but manual, and the option most likely to be nominally available while not practically comparable). **Automatic, no review burden, no new exploit surface beyond the earn structure's own** — and it reinforces the tier design, where play is already the way in. |
| **R90 — PRIZE INVENTORY IS HYBRID: FINITE HEADLINE, GENERATED TAIL** | **2026-08-06** | A small **finite** prestige set with a hard end, over a **generated** common pool that sustains indefinitely — so the series has both a reason to chase and a reason to keep playing once the headline is gone. **"Sold out" exists for some rows and not others, and the UI must distinguish them:** a generated prize presented as scarce is exactly the dishonesty §4.2's never-reissue policy exists to prevent. The headline set inherits never-reissue in full; the tail never claims a count. |
| **R91 — THE SAWTOOTH IS CLOSED: UNREACHABLE UNDER FIXED QUEUE SIZES** | **2026-08-06** | §5.3's winner's-multiple sawtooth (17–23% drops at N = 14, 21, 27, 34) **cannot be designed away** — removing it needs a continuous paid-places function and `ceil()` is not one, exactly as §5.3 says. **It does not need to be.** §5.2's design constraint — *no queue may sit at a field size where the threshold straddles it* — combined with **fixed queue field sizes** means a player never meets both sides of a step. **The drops are a property of the curve, not an experience anyone has.** Recorded as closed rather than carried indefinitely, with one reopening condition: **variable-size queues**, which would make the mitigation lapse. |
| **R88 — THE WATTS RUNGS ARE 250 · 1,000 · 5,000 · 25,000 W** | **2026-08-06** | A **quarter** of peg-equivalence (which would be 1,000 · 5,000 · 25,000 · 100,000 W), satisfying R80. Chosen against the earn rate rather than against the Volts ladder: **the entry rung is a small fraction of one match**, so a new player can stake in their first session with nothing saved up — which is the accessibility R78 exists to deliver, made real rather than nominal. **Accepted cost:** the top Watts rung is reachable in a handful of matches, so the WATTS PLAY ceiling is deliberately low. That is correct — the ceiling is what VOLTS PLAY is for, and a play-money tier with a punishing top rung would be neither play money nor accessible. |
| **R78 — BOTH CURRENCIES STAKE, IN SEALED POOLS** | **2026-08-06** | **WATTS PLAY and VOLTS PLAY are the two staked tiers** (§3.1) — the play-money / real-money split, in our currencies, so a Watts player is a full participant rather than a spectator waiting to convert. **AMENDS R9:** Volts are no longer *the* stake currency, they are one of two. **The one-wallet property survives untouched** — §3.1(c) argues for one write *path*, not one balance *field*, and separate pools if anything **contain** the cross-contamination it warns about. The pools never meet: two players staking different currencies cannot settle one pot. **The document already disagreed with itself here** — §3's Watts row has always read *"lower stake tiers"*, the only sentence in the SSOT putting Watts into stakes; that cell is now normative rather than a fossil. **Answers §15.6**, though not in the direction it anticipated: the firewall that landed is currency-versus-currency, not store-versus-staking. |
| **R79 — THE RAKE IS 5% ON BOTH POOLS** | **2026-08-06** | Extends **R68**'s `k = 0.05` to WATTS PLAY unchanged. **Not a revenue decision — a drain.** Watts are `the primary source` (§3.1b), and staking is a **transfer**, so an un-raked Watts pool would give the faucet currency a circulation loop with **zero sink**. That is the one change that would make the Watts economy unbalanceable, and it must not be reintroduced as a player-friendliness measure. **Because `k` is unchanged, §5.3's 1.40× min-cash verification carries over exactly** rather than re-opening at a second rate — which is the practical reason for matching R68 rather than tuning a second number. |
| **R80 — THE WATTS LADDER SITS BELOW PEG-EQUIVALENCE** | **2026-08-06** | The Watts rungs are **not** 10× the Volts rungs; a Watts stake is deliberately cheaper in real terms (§3.3). **E1 is untouched** — the peg is a *store-pricing conversion*, a stake tier is a *chosen entry amount*, and setting one low changes no exchange rate. **⚠ THIS RULING RESTS ON R81, NOT ON ITS OWN MERITS:** it is safe only because the pools are sealed. Introduce any Watts→Volts conversion and the cheap ladder becomes a **discount on the dear one**, i.e. an arbitrage. The four rungs are geometric and their values are **tuning against the real earn rate, not set here** — same discipline as R39. |
| **R81 — VOLTS ARE PURCHASE-ONLY; NO CONVERSION** | **2026-08-06** | No earn drip, no achievement payout, **no Watts→Volts path**. Stated plainly rather than left implicit: Volts already had exactly one source and no earn path, so VOLTS PLAY is **pay-to-enter by construction** — which is the real-money table in every card room and is not a defect. **THIS IS THE SEALING RULING.** It keeps the faucet entirely on the Watts side where §3.1(b)'s accounting lives, and it is what makes R80 safe and R83's manufacturing loop **Watts-only**. **E2 is untouched**: money goes in, nothing comes out — which is exactly what makes this a structure applied to tokens rather than real-money wagering. **Every future proposal to "let players grind into Volts" is a proposal to merge the two economies** and must be read as one. |
| **R83 — MANUFACTURED WATTS ARE A STAKING INTEGRITY CONCERN** | **2026-08-06** | §10.4's collusion surface — *"feeding parts to a colluding opponent manufactures currency"* — **changed class under R78**. Manufactured Watts once inflated a shop balance; they can now fund a wager, where the harm lands on another player rather than on the pricing curve. **The integrity layer guarding it is a PREREQUISITE for shipping WATTS PLAY, not a follow-on.** This does not design the detection (deliberately — see `ssot/league-play.md` §11 on publishing heuristics); it moves the work from *"economy's problem, eventually"* to *"blocks a tier from shipping"*. The loop is **Watts-only**: R81 leaves no manufacturing path into VOLTS PLAY. |
| **R68 — THE RAKE IS FLAT 5%** | **2026-08-06** | `k = 0.05`, uniform at every stake and field size. Promotes §5.2's working assumption to a ruling, so **every derived figure in this document stands unchanged**. Chosen for predictability, not yield: **the rake is the only true sink in a staked match** (§3.1b), so it is an economy-health lever before it is a revenue one, and a curve cannot be tuned against a stake distribution that does not exist yet. **Revisit with volume data; if tiered then, tier REGRESSIVELY** — the legacy `design/IRONICS_MATCH_STAKING_SSOT.md` R1's progressive 5%/10% is backwards and is **superseded**. **⚠ Any change re-opens §5.3's min-cash verification at every tier.** **Closes §15.7.** **EXTENDED BY R79** — the same `k = 0.05` applies to the WATTS PLAY pool; because `k` is unchanged, §5.3's verification carries over rather than re-opening. |
| **R69 — STAKE TIERS ARE 100 · 500 · 2,500 · 10,000 V** | **2026-08-06** | Four rungs, ~5× geometric — the lower four of the legacy ladder (Trickle · Ante · Live · Main). **The higher tiers are withheld deliberately, not forgotten:** `matchmaking.md` §7 requires an empty band to *look* empty, so a 50,000 or 250,000 V tier with no population would advertise its own failure on the front page of the lobby. Add them when the top rung is genuinely hot. Presets are primary, numeric entry secondary (R20), and the server bands whatever is entered (**R59**). |
| **R70 — A SQUAD'S PAYOUT SPLITS EVENLY** | **2026-08-06** | A squad holds **one** finishing position and receives **one** payout, divided equally among its members. Contribution-weighting was rejected on two grounds: it creates competition **inside** the squad, which removes the reason to queue as one, and **once money is attached it becomes a griefing surface** where a teammate can profit from your death. A leader-distributes model is worse — it invents a trust relationship the game does not otherwise need, with strangers. **A useful property falls out:** because entry and payout both scale with team size, the multiple reads identically per-player and per-position, so the UI can show `× stake` with no footnote. |
| **R71 — THE STORE SELLS ON BOTH SURFACES** | **2026-08-06** | Full checkout **in-game AND on the web portal**; neither is a shop window. **⚠ THIS IS THE EXPENSIVE OPTION AND WAS TAKEN KNOWINGLY.** It does **not** reduce in-game UI scope — the complete in-game checkout is built, *plus* the web store. **Two consequences are now standing obligations:** (a) **catalog, pricing and mint state must be served from one source to both surfaces or they will drift**, and drift in a limited-mint economy is an E-invariant breach, not a cosmetic bug; (b) **console platform holders have specific rules about steering players to external purchases — this needs a cert read before the web path ships**, not after. |
| **R4 — the two loot documents merge here** | **2026-08-05** | Loot taxonomy (§10) and the loot carry mechanic (§11) are two sections of this SSOT. They previously lived in separate documents with **zero cross-citations between them** — drift by construction. The superseding decision history of the carry model is preserved by archiving that document verbatim; only decisions whose **reasoning is still load-bearing** are carried forward here. |
| **R9 — one wallet** ⚠ *first clause superseded by R78* | **2026-08-05** | The staking and cosmetics economies share a single wallet (§3.1), with the three consequences recorded as design constraints: the two economies compete for the same balance and cannot be balanced independently; sinks and sources are reckoned across both, with rake the only true sink in a staked match; and server authority over the ledger is absolute, because a defect in either system corrupts both. |
| **R36 — BATTLE ROYALE earns on placement** | **2026-08-05**, **renamed 2026-08-06** | Finishing position determines payout (§5.1). **A payout shape, not new machinery** — placement is already computed, replicated and consumed by league rating, so this adds a consumer to an existing signal. It is also the only outcome the ruleset generates, having no timer and no respawn. **RENAMED BY R41** — SHOOTOUT is BATTLE ROYALE; the payout basis is unchanged. |
| **R37 — the payout ladder, keyed on POSITIONS** | **2026-08-05**, **amended 2026-08-05** | **INTENT UNCHANGED:** winner-takes-all for small fields, scaled payouts above, `paid = 1 if positions < 10, else ceil(0.30 × positions)`. **Keyed on finishing positions, never player count: a team mode has exactly TWO positions regardless of player count, so every team mode is winner-takes-all automatically, with no special-casing.** Scaled payouts exist only in FFA. **AMENDED — MECHANISM ONLY:** the four banded percentage tables are replaced by a **generating rule** solved per exact field size (§5.2), because fixed percentages over a variable pot could not hold the min-cash floor — they produced 1.06× at 14 positions and **0.80× at 21**, a cash that lost money. Under the rule, **min cash is an INPUT (1.4×) and holds at every field size**; §5.3 records that the second invariant, a growing winner's multiple, is **not** satisfied and why. |
| **R38 — staking is orthogonal to earning** | **2026-08-05** | A stake is a **closed loop** — participants escrow Volts, the pot settles per the R37 ladder minus rake. **Earning is what a match PAYS; staking is what a wager SETTLES** (§5.4). Kept separate because earning is a faucet whose risk is inflation, while staking is a transfer whose risk is an unbalanced loop — merging them makes a change to one silently a change to the other. Reinforces `ssot/league-play.md` §5 from the economy side; **no conflict**. |
| **R40 — payout depth is 15%, not 30%** | **2026-08-05** | `p(N) = ceil(0.15 × N)` for N ≥ 10, replacing 0.30 (§5.2). **Poker-standard and top-heavy.** Forced by a proven incompatibility: **30% depth + a 1.4× floor + a scaling winner cannot all hold**, because the extra budget a larger field brings is consumed by the extra paid places it creates. **The winner's multiple is what a staked game trades on**, so depth was the constraint to give up. **THE TRADE-OFF ACCEPTED: FEWER PLAYERS CASH** — at 36 positions, 6 paid instead of 11 (16.7% of the field, down from 30.6%); at 20 positions, 3 instead of 6. **What it bought:** the winner's multiple roughly doubled (36 positions: 5.69× → 13.29×), the trend now rises monotonically by band instead of running flat, and the queue-threshold list halved from nine values to five. **What it did not buy:** the sawtooth still exists — five drops of 17–23% remain (§5.3, §15.9). |
| **R39 — the earn ladder across modes** | **2026-08-05**, **noted 2026-08-06** | **Extraction pays most** (highest risk, longest commitment); **placement pays modestly** (§5.5). The design constraint: **if a duel out-earns an extraction run per minute, extraction stops being worth its risk and loot becomes decorative.** Per-minute is the comparison that matters, not per-match. The ordering is the ruling; no rates are set. |

---

## 17. RELATED

- [`Docs/DOCTRINE.md`](../DOCTRINE.md) — laws cited here: **N1** server authority · **N11** the client never
  decides ownership or balances · **N12** transactional transfer with escrow, rollback and anti-duplication ·
  **NM5** a shipped id is never renamed · **G5** a data asset with no consumer is inert · **P8** a backend is
  proven standalone before integration.
- `ssot/matchmaking.md` — stake as a ticket parameter, banding, the settlement interface, and the region source
  that territory gating reads.
- `ssot/league-play.md` — what earns a reward; this document grants it.
- `ssot/match-modes.md` — the rulesets whose outcomes settle a pool.
- `ssot/character-system.md` — the character content the identity and finish axes dress.
- `ssot/combat-arsenal.md` — the weapons the weapon axis owns as items.
