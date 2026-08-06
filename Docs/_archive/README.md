# Docs/_archive — superseded documents, retained

**Nothing here is a source of truth.** Every file in this directory has been superseded. It is retained
rather than deleted so the decision record survives — several of these documents are the origin of ideas
that *did* survive, and a few are decision records in their own right.

**Where truth lives now:**

| Tier | Where | What it holds |
|---|---|---|
| **1** | `Docs/DOCTRINE.md` | The laws. Changes rarely, requires an operator ruling. |
| **2** | `Docs/ssot/` (eight documents) | What each system **is** and **why**. No status claims. |
| **3** | `Docs/LIVE_TRACKER.html` | Status, and only status. No design decisions. |

**Rule of use:** if something in this directory is not reflected in Tier 1 or Tier 2, it was **not
adopted** — treat it as an idea that was considered, not a decision that was made. Do not cite an
archived file as authority, and do not re-adopt from one without an operator ruling.

**Only one file here was edited on archival:** `CORE_GAME_CONCEPT.md`, which received a prepended
provenance header under ruling R2. Every other file is byte-identical to its last live state.

---

## Index

| File | What it was | Superseded by | Last live commit |
|---|---|---|---|
| `CORE_GAME_CONCEPT.md` | An unratified ChatGPT transcript exploring the game concept. Never reviewed or adopted, but read as authoritative because of its length and voice. | **Ruling R2.** Its load-bearing content was reconciled into `Docs/DOCTRINE.md`; Appendix B there records its provenance and Appendix A records the claims disk contradicted. A provenance header was prepended on archival — the one authorised edit. | `3665d15b` · 2026-07-30 |
| `IRONICS_MELEE_RULESET_SSOT.md` | The melee / hand-to-hand ruleset design. | **Ruling R1 — melee is CUT project-wide.** The game is dual-mode, not tri-mode. Nothing replaces this; the scope was removed. | `e170a2d6` · 2026-07-30 |
| `IRONICS_LOOT_CARRY_MODEL.md` | The v7 loot carry model — the wallet/cache/extract mental model and the carry mechanic. | **Ruling R4.** Merged into `Docs/ssot/economy-store.md` and archived **VERBATIM as a decision record**. Not edited in any way. | `713e325a` · 2026-06-18 |
| `IRONICS_TRIMODE_TRACKER.md` | Progress tracker for the tri-mode build (Haywire / Pro Mod / Melee). | **Ruling R3.** Status moved to `Docs/LIVE_TRACKER.html`. Its canonical naming ruling (`HeroData_BagMan_Pro`, `HeroData_BagMan`) was carried into `Docs/ssot/character-system.md` §11. Its tri-mode framing is dead under R1. | `4b89557b` · 2026-08-01 |
| `IRONICS_UI_WASH_QUEUE.md` | The queued front-end retheme work — surfaces, buckets, execution order. | **Split.** The wash **standards** (what makes a surface conformant) are now `Docs/ssot/ui-frontend.md` §11. The **queue** was Tier 3 scaffolding and was deliberately not carried. | `e8d6a577` · 2026-07-01 |
| `WASH_INVENTORY.md` | Per-surface inventory of what had and had not been rethemed. | Same split as above — Tier 3 scaffolding, superseded by `Docs/LIVE_TRACKER.html` plus the §11 standards. | `700c6c4c` · 2026-07-18 |
| `SESSION_LOG.md` | Phase 0 bootstrap session log (May 2026) — environment, LFS migration, first push. | Superseded by `Docs/LIVE_TRACKER.html` and by git history itself. | `f56bfbdc` · 2026-05-11 |
| `AFL_YOLO_PLAYBOOK.md` | The YOLO autonomous-run playbook — runner provisioning and queue operation. | **The YOLO concept is ABANDONED** (operator, 2026-08-05); parts of the tooling may already have been removed. Archived rather than kept as a runbook, because a live runbook for an abandoned concept invites someone to follow it. The surviving tooling under `Tools/AFL_Yolo/` was left untouched — see the note below. | `7ccc3062` · 2026-05-18 |
| `BAG_MAN_LIVE_TRACKER.html` | The main tracker — 416 tasks, 50 sprints, 6 phases, 6 pillars. Stopped 2026-07-30. | **Ruling R3.** `Docs/LIVE_TRACKER.html`, **rebuilt from git log** rather than merged forward — this file was stale for August and merging it would have imported a false picture. Its genuinely-open items were carried; the sprint/phase/pillar scaffolding was not. | `8283c218` · 2026-07-30 |
| `BAG_MAN_LIVE_TRACKER_STEP0.html` | STEP-0 doctrine-reset tracker. | **Ruling R3** → `Docs/LIVE_TRACKER.html`. | `64e5088f` · 2026-06-01 |
| `BAG_MAN_LIVE_TRACKER_STEP0_Rebuild.html` | STEP-0 rebuild tracker. *(Renamed on archival: the original filename `BAG_MAN_LIVE_TRACKER_STEP0 Rebuild.html` contained a space, which broke path handling in several tools. Content unchanged.)* | **Ruling R3** → `Docs/LIVE_TRACKER.html`. | `3665d15b` · 2026-07-30 |
| `AFL_NEON_ARENA_LIVE_TRACKER_v1.1.html` | The v1.1-era tracker, from the project's previous name. | **Ruling R3** → `Docs/LIVE_TRACKER.html`. Previously sat in `Docs/_archive_v1.1/`; consolidated here so there is one archive. | `f883f51e` · 2026-05-10 |
| `AFL_NEON_ARENA_MASTER_BUILD_v1.1.md` | The v1.1-era master build document. | Superseded by `BAG_MAN_MASTER_BUILD_v2.0.md` (which remains live and is CI-exempt), and by the Tier 1 / Tier 2 split. Consolidated here from `Docs/_archive_v1.1/`. | `f883f51e` · 2026-05-10 |

---

## What was NOT archived, and why

Most documents merged into a Tier 2 SSOT were **moved, not archived** — into `Docs/design/`,
`Docs/runbooks/` or `Docs/reference/`.

The test applied: **does the Tier 2 SSOT now fully answer the questions the source document was written
to answer?** The eight SSOTs are deliberately *design distillations* — they carry the decisions and the
reasoning, not every parameter table, recipe step or measurement. A source document that still holds
unmerged specifics is therefore still live, and archiving it would bury content nothing else records.

Examples: `IRONICS_ACHIEVEMENTS_SSOT.md` retains completion-rate bands and the first achievement set that
`ssot/league-play.md` §10 does not reproduce; `IRONICS_WEAPON_AUTHORING_SPEC.md` retains disk-verified
per-asset values that `ssot/combat-arsenal.md` §3 states only as a contract. Both were moved to
`Docs/design/`, not archived.

Only documents that are **wholly** superseded, or that an operator ruling retired, are here.
