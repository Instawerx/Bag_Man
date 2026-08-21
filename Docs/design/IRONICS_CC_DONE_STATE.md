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
| `CC-X22` — store offered rows the backend cannot sell | **DONE 2026-08-21** (`3397229c`). ~~register 263 SKUs~~ — the 263 was stale before it was written down. RE-MEASURED: **183 of 252** non-`GrantedFree` rows were surfacing as buyable and returning `ItemNotFound`. Fixed by intersecting with `GetCatalogItems` at login, cached for offline, and a THREE-STATE set so “not yet answered” shows NOTHING rather than everything. Proven 4 arms incl. a named registered id offered and a named unregistered id hidden. The product question is partly RULED: the 29 credit-pool weapons are registered, 26 of them **credit-only, no direct purchase**. |
| `CC-X25` — restore X-line body colour | **Art direction**, and now with a number attached. Measured 2026-08-20: the master has 8 vector / 13 scalar params (`NeonColor` and `AlbedoRecolor` both present, corroborating SSOT 3.1's "2 of 8" and "7 of 13"). Chain is `M_AFL_Character` -> **one** direct child `MI_AFL_IRONICS_Body` -> 4 variants (`_Crimson`, `_FANATICS`, `_Violet`, `_Viridian`), referenced by 6/2/1/1/1 assets. They are bound on the **skeletal mesh material slots** (`SKM_IRONICS_Blank`), not per-robot in Blueprints — which is why "every X body binds it" is true through a shared mesh. So the edit is small in ASSET count and total in VISUAL reach: it moves every X-line robot at once, and partially reverses CC-X24. |
| `CC-X20`, `CC-X21`, `CC-X27` | ~~**Undefined** — exist only in block text~~ **CORRECTED 2026-08-21: all three are now written into SSOT §11 (rows 12, 13, 15) and worked.** CC-X20 ANSWERED (the 50 unmapped are `AFL.WeaponSkin.NeonCamo.*`; the 10 `AFL.Body.` rows are real `FINISH`/`Direct` rows). CC-X21 **CLOSED 2026-08-21**, handed to the 15-second Find-in-Blueprints — the scripted route is exhausted (Blueprint graphs are protected from Python; the remove-exposure falsification cost a 1h42m mutex deadlock and a system crash for zero information) and the risk is cosmetic-only. CC-X27 CANNOT BE CONFIRMED — no repro on record; recommend closing as obsolete. |
| `CC-X26` | ~~never written to disk~~ **CORRECTED 2026-08-21: written to SSOT §11 row 14 and the premise CONFIRMED in code** (`AFLW_LoadoutBase.cpp:612-618` — render mode set, Atmosphere/Fog/VolumetricFog/Cloud off). A DESIGN call, not a defect. Still gates nothing. |

**The `CC-X20/21/26/27` gap is itself the lesson**: an item that lives only in block text is an item
that cannot be worked, audited, or closed. Every CC-X item from here is written into
`IRONICS_CHARACTER_CREATOR_SSOT.md` §11 at the moment it is raised.

## WHAT REMAINS OF THE ACTUAL PLAN (2026-08-21)

The `CC-X` list is debt from our own work. It is not the plan. Audited against
`IRONICS_CC_ROADMAP.html`, here is every phase of the SCOPED plan that is not closed.

| Phase | State | What it needs |
|---|---|---|
| **CC-5.2** — Hue arc control, clamped, not RGB sliders | **OPEN** (deliberately; the roadmap banner says so) | The UI lane. Neon and Edge linked by default with an unlink toggle. |
| **CC-6.5** — the end-to-end proof | **VERIFY** | *"Buy a robot pack, receive slots, build in the creator, equip, spawn — no cheat path anywhere in the loop."* **This IS the done definition.** |
| **CC-7 · Stickers** | **UNBUILT, whole phase** | 7.1 Blender zone UVs from FBX · 7.2 sticker axis — *"categorically absent today: no C++ symbol, no enum member, no rows, no textures"* · 7.3 placement UI |
| **CC-8 · Accessories** | **UNBUILT, whole phase** | 8.1 hardpoint socket schema — only six sockets exist, all weapon or foot, none for accessories · 8.2 accessory rows + attach path |

**CC-7 and CC-8 were not being tracked in the 2026-08-20/21 sessions at all.** Recorded here because
an item that lives only in a roadmap nobody re-reads is the same failure as one that lives only in
block text — the lesson this document already carries, applied to itself.

### What the UI lane inherits (all of it measured, none of it assumed)

- **The store surface is honest.** `GetPurchasableEntries` intersects with what the backend can
  actually sell. 183 of 252 non-`GrantedFree` rows were previously offered as buyable and returned
  `ItemNotFound`; they are now withheld. The set is THREE-STATE, so "not yet answered" shows NOTHING
  rather than everything, and an offline start shows the last known set from cache.
- **Counted entitlements are account-durable and atomic.** Slots survive a relaunch and any server;
  concurrent grants compose (DynamoDB `ADD`), and redemption reserves → grants → refunds on failure.
- **Weapon credits work end to end**, with four refusals proven: unmarked row, hand cannon, bundle,
  and past-zero.
- **The type lint tells the truth**: `unmapped=0 invalid=0`, and the only 5 mismatches are the
  `AFL.Character.*_X` identity rows, left visible on purpose pending the pivot.
- **Known traps, banked**: `AFL.Body.` is a legacy alias (canonical is `AFL.Finish.`) holding 10
  unreachable duplicates — see CC-X32, a PRICE question; `Type==Weapon` is deliberately overloaded and
  split by id PREFIX, and that filter is load-bearing.

### The honest gap

The economy half of the done definition is proven. The **"build in the creator, equip, spawn"** half
is not, and cannot be until CC-5.2 exists. CC-6.5 is the arm that closes it.
