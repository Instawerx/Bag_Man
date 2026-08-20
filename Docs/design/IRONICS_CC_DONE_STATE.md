# IRONICS CHARACTER CREATOR — DONE STATE

**What a player can and cannot do end-to-end, as of 2026-08-20, basis `7fc55832`.**

Every row below cites a tag or a measurement. Where something is unproven it says so; where it is
proven but *scoped*, the exclusion is stated rather than implied. The point of this document is that
the UI lane can build against it without discovering the gaps by hitting them.

> **HOW TO READ A ✅ HERE.** It means demonstrated in PIE on a controllable pawn, with the arm that
> could have failed identified. It does not mean "compiles" or "looks right". Where a proof used a
> configuration that differs from shipping, the configuration is named.

---

## 1 · The done definition, clause by clause

> *"A player opens the creator, picks a chassis, sets the channels that are real on it, rotates and
> sees them hold, saves a build, equips it, spawns, and the robot in match is the robot in the
> preview. Slots count. Nothing in that loop depends on a cheat."*

| # | Clause | State | Evidence |
|---|---|---|---|
| 1 | **opens the creator** | ❌ **not built — UI lane's, by design** | No widget exists. Spec: `IRONICS_CC_UI_HANDOFF.md` (`14b3d609`, `4b860091`). The C++ base, bindings and interface are shipped; the WBP is not. |
| 2 | **picks a chassis** | ✅ behaviour + interface | `cc-5-1-done` — shell, chassis picker, data-driven channel schema |
| 3 | **sets the channels that are real on it** | ✅ **and "real on it" is literal** | `SCHEMAPROBE DERIVED`: `M_AFL_Character count=2` (Edge, Glow — Body `PresentButInert`, Visor `Absent`) vs `M_Mannequin count=3`. Gamut clamp: `ARC PASS checked=24 outOfGamut=0 keptSV=1 linksDefaultOff=1` |
| 4 | **rotates and sees them hold** | ✅ | `cc-5-3-done` — apply lands, holds measured on the far side, change-while-rotating applies without reversion |
| 5 | **saves a build** | ✅ | CC-5.4 (naming, uniqueness, report path) banked inside `cc-5-1-done`; backend blob storage with a revision guard (`75bc15c`) |
| 6 | **equips, spawns, match == preview** | ✅ **architecturally, not incidentally** | `cc-5-3-done` decisive arm: the preview value equals what would spawn, measured through the **same resolution path** (`BuildColorOverride` → `SetColorOverride`), not reasoned about |
| 7 | **slots count** | ✅ | `cc-4-2-done` (counter accumulates: 0→3→6→14) + `cc-join-done` (a **production purchase** reaches it) |
| 8 | **nothing in that loop depends on a cheat** | ✅ **now — it did not until `cc-join-done`** | See §2 |

---

## 2 · The defect that clause 8 was hiding

Until `cc-join-done`, **a player could buy `AFL.CreatorSlot.x3` for 4,990 Volts and receive nothing.**

It was invisible to two passing tags, and the reason generalises:

- `cc-4-2-done` proved row-data → counter → persistence across `CommitMutation`. True.
- `cc-6-1-done` proved `ClientRequestPurchase` → PlayFab. Also true.
- **They were described as meeting at `CommitMutation`. They never touched.** The shipping purchase
  completes through `ApplyPurchaseResult`, which never enters `CommitMutation`; its only
  `GrantId`-passing caller is inside `ServerPurchaseCosmetic_Implementation`, compiled out by
  `#if !UE_BUILD_SHIPPING`.
- The decisive arm could not see it: it granted through `DebugGrantOwnership`, which routes through
  `CommitMutation` — **the one caller that already worked.**

Two fixes were required and neither was sufficient alone:

| Repo | Fix | Verified |
|---|---|---|
| game | counted-grant hook at `ApplyPurchaseResult` | in `e1de55c4` — ⚠ whose message describes only the `pfid=` work and never mentions it |
| backend | `AFL.CreatorSlot.x1/.x3/.x8` were `IsStackable=false`; a pack that increments a counter cannot be one-shot | `7452fdc`, verified by re-reading the live title |

Measured close: `counter 14 → 17 → 20` against PlayFab's own ledger `VO 200179 → 195189 → 190199`
= exactly 2 × 4,990.

---

## 3 · What a player CANNOT do

Ordered by how likely the UI lane is to trip over it.

| # | Cannot | Why | Status |
|---|---|---|---|
| 1 | **Open the creator at all** | no widget | UI lane's deliverable; spec exists |
| 2 | **Colour the body on an X-line chassis** | `NeonColor` is connected but gated behind `AlbedoRecolor = 0.0` on `M_AFL_Character` — `Body = PresentButInert` | **RULED (CC-X24): disabled and SHOWN, not hidden.** The row renders with its reason. `CC-X25` would restore it and is **not scheduled** — it alters the shipped look of every X-line robot |
| 3 | **Rely on a fixed channel count** | channels are **master-dependent**: 2 on `M_AFL_Character`, 3 on `M_Mannequin` | Read `FAFLCreatorChannelSchema`; never hard-code a rail length |
| 4 | **Buy 263 of the 427 catalog rows** | priced and `Direct` but absent from the PlayFab manifest | `CC-X22` **scoped, deliberately unregistered** pending a product ruling. Weapons are 151 of the 263 |
| 5 | **Trust the wallet mirror mid-session** | it is a **display** value; PlayFab decides. Attribution is now logged (`pfid=`) | `cc-x23-done` — mechanism proven; the `OnLoggedIn` delegate branch is **not exercised in PIE** |

---

## 4 · Notes the UI lane must not have to discover

1. **`SkinColor_Body` has ZERO catalog rows.** The Type histogram sums to exactly 427 and contains no
   such entry; the 10 `AFL.Body.*` rows carry `Type = Finish`. Consistent with #2 above — nothing has
   existed for a body-colour row to drive.
2. **The facemask axis is 38, not 60**, and the edge axis is 37 by prefix / 42 by type (the 5-row
   difference is the `AFL.Character.*` rows carrying the edge type). Corrected in `1b55b8fd`.
3. **Slot cap is a parameter, not a constant**: `AFLResolveEffectiveSlotCap(baseline, purchased,
   tierCeiling, maxUpgrade, hardCap)` — measured `2/5/5/10/10/2`. No literal in the resolver.
4. **Three counted SKUs exist and only three** (`x1`/`x3`/`x8`, one `CountedKey`, quantities 1/3/8,
   zero half-configured). The slot mechanism is complete in scope.
5. **Economy proofs run on a LISTEN SERVER; this is a targeted exception.** Dedicated PIE logs every
   GameInstance into PlayFab separately, so authority and client are different accounts. CC-2.1 step 6
   still governs everything else: replication and client-convergence proofs stay on dedicated, two
   clients. The runner sets `PlayNetMode` in the ini and **restores it**.

---

## 5 · Open, and blocked on someone other than the code

| Item | Blocked on |
|---|---|
| The creator widget | UI lane — spec is written and current |
| `CC-X22` — register 263 SKUs, or rule them not-for-sale | **Product intent**: are weapon cosmetics sold for real money? That is 151 of the 263 |
| `CC-X25` — restore X-line body colour | **Art direction**, and now with a number attached. Measured 2026-08-20: the master has 8 vector / 13 scalar params (`NeonColor` and `AlbedoRecolor` both present, corroborating SSOT 3.1's "2 of 8" and "7 of 13"). Chain is `M_AFL_Character` -> **one** direct child `MI_AFL_IRONICS_Body` -> 4 variants (`_Crimson`, `_FANATICS`, `_Violet`, `_Viridian`), referenced by 6/2/1/1/1 assets. They are bound on the **skeletal mesh material slots** (`SKM_IRONICS_Blank`), not per-robot in Blueprints — which is why "every X body binds it" is true through a shared mesh. So the edit is small in ASSET count and total in VISUAL reach: it moves every X-line robot at once, and partially reverses CC-X24. |
| `CC-X20`, `CC-X21`, `CC-X27` | **Undefined.** These appear nowhere in the repo and `git log -S` across all refs shows they were never committed. They exist only in block text |
| `CC-X26` | Named in conversation as preview/match **lighting parity**; never written to disk. Gates nothing |

**The `CC-X20/21/26/27` gap is itself the lesson**: an item that lives only in block text is an item
that cannot be worked, audited, or closed. Every CC-X item from here is written into
`IRONICS_CHARACTER_CREATOR_SSOT.md` §11 at the moment it is raised.
