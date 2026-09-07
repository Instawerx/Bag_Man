# IRONICS — GO-TO-MARKET & PLATFORM INFRASTRUCTURE ROADMAP
### v2.3 — CONTINUOUS-AVAILABILITY · 1,000-TESTER BETA · AWS-ACCURATE · FOUNDER LADDER · BUILD AUDIT MEASURED

**Tier-2 SSOT · Business / Platform lane**
Owner: C12 AI Gaming · Studio Lead (Sam)
Sibling SSOTs: `Docs/DOCTRINE`, `Docs/IRONICS_MARKETPLACE_MASTER_ARCHITECTURE.md`, **`Docs/LIVE_TRACKER.html`** (note: `BAG_MAN_LIVE_TRACKER.html` is **archived** at `Docs/_archive/` — it is not the live tracker; any block or doc naming it is stale)
Repos: game `Instawerx/Bag_Man` (`personal/main`) · backend `Instawerx/Bag_Man_Backend` (`origin/master`) · **new** `Instawerx/Ironics_Web` (to create)

> **v2.1 supersedes v1.** v1 proposed scheduled ops windows capped at 36 concurrent — that model is **retired**. IRONICS beta is **continuously available**, sized for **1,000 registered testers** (§1).
> **v2.1 corrects service attribution:** matchmaking and hosting are **Amazon GameLift Servers FlexMatch**, not PlayFab. See **§4.1** for the authoritative vendor map and **§4.3** for the open identity-consolidation fork.

---

## 0. HOW TO READ THIS DOCUMENT

**No timelines.** Every unit of work is a **Phase** with an **entry gate**, a **work plan**, and an **exit gate**. A phase is done when its exit gate is proven, not when it was worked on.

**Proof standard carries over unchanged.** ✅ = demonstrated against a live system with cited evidence (an HTTP response, a CloudWatch line, a Lighthouse run, a real tester completing the action). "It's deployed" is not ✅.

**Lane discipline carries over.**

| Lane | Owns |
|---|---|
| **CLAUDE CODE** | Backend/CDK/Lambda/DynamoDB, web app, patcher service, CI, telemetry, admin API |
| **AIK** | In-editor UE5 only (watermark widget, gate UI, bug-report widget, telemetry hooks) |
| **OPERATOR** | All builds, all pushes, all provisioning, all approvals, all GO calls |
| **CLAUDE** | Architecture, phase specs, gate adjudication, ledger updates |

**Conform-to-proven.** The beta auth gate is not new architecture — it is the same server-authoritative pattern already proven by `EarnWattsAuthority → EarnThroughBackend → /earn` with A1.4 identity resolve. Structurally diff every new endpoint against that chain and justify each divergence.

---

## 1. THE RECONCILIATION — 1,000 AND 36 ARE THE SAME PLAN

The two numbers were never in tension. They are the same population measured at opposite ends.

**Standard live-service ratios:**

```
1,000 registered beta accounts
        │
        ├── active-in-beta rate           ~30%   →   ~300 DAU
        │
        ├── peak CCU : DAU                ~12%   →   ~36 PEAK CONCURRENT
        │
        └── patch-day / event spike       2–3×   →   ~80–120 PEAK CONCURRENT
```

**36 concurrent is not a ceiling to ration against. It is the organic peak that 1,000 registered testers naturally produces.** Provisioning for ~40 sustained and ~120 burst *is* provisioning for a 1,000-tester beta.

### Why continuous availability is the correct model, and windows were wrong

I proposed windows in v1 to solve empty lobbies. Sam's correction is right, and the reasoning is worth stating explicitly because it changes what the beta is *for*:

1. **Windows fabricate concurrency and hide the exact failure you ship into.** A scheduled window guarantees a full queue. Launch day has no scheduled window. The thing you most need to learn — does matchmaking hold at 9am Tuesday, at 3am, on a Wednesday between patches — is precisely what a window makes unmeasurable.
2. **Real retention requires continuous availability.** D1/D7/D30 are undefined if play is gated to announced sessions. Window-over-window return is a proxy invented to work around a constraint that doesn't need to exist.
3. **The population fixes the problem the windows were papering over.** 1,000 registered sustains a real queue most hours. Bot fill (already proven, already shipped) covers the thin tails.
4. **Server capacity was never the constraint.** ~40 sustained concurrent at 3v3 is ~7 server processes. That is a rounding error on a GameLift Servers fleet, not an engineering problem. See §9.
5. **Continuous use is what proves the marketplace.** Transaction volume, equip rates, catalog browsing — economy signal needs sustained sessions, not six concentrated hours a week.

### What the real constraint actually was

The 36 number was never system capacity. It was **manual observation capacity** — one person can watch six matches. The AAA answer is not to shrink the population to fit the observer. It is to **replace observation with instrumentation** so the population can be continuous and unattended.

> **This becomes a hard requirement, not a nice-to-have:** IRONICS beta runs unattended. Every failure a human would have caught by watching must be caught by an alert, a dashboard, or an automated digest. That requirement is specced in Phase W6 and gates the ramp to 1,000.

---

## 2. OBJECTIVES

### Beta objective (v2)

> Recruit and activate **1,000 registered IRONICS accounts** into a **continuously available** beta, sustaining **~40 peak concurrent with ~120 burst headroom**, and exit with: **≥97% crash-free sessions**, **≥99% login success**, **p50 match-fill ≤60s during peak and ≤180s off-peak**, **≥70% of installed testers completing a first match**, **≥35% D7 retention**, and **≥60% completing a marketplace transaction**.

### Launch objective (unchanged)

> 1,000 users in the first 24 hours · ≥300 returning within 7 days · ≥100 completing a marketplace transaction.

**Note the strategic consequence of Sam's call:** if the beta itself reaches 1,000 registered, the launch-day objective stops being a cold-start problem. Launch day becomes *activating a list you already own* — 1,000 people with accounts, founder numbers, Discord roles, and installed clients. That is a fundamentally stronger position than launching to strangers, and it is the strongest argument for the 1,000-tester beta.

---

## 3. THE HARD PART IS ACQUISITION, NOT INFRASTRUCTURE

This is where the plan's center of gravity moves in v2. Servers are cheap. Getting 1,000 real installed testers is the actual work.

### Funnel math, run backward from the objective

```
1,000 INSTALLED ACTIVE TESTERS
   ÷ 0.70   installed → first match completed        ← the make-or-break step
   ÷ 0.90   downloaded → installed                   ← code signing, install friction
   ÷ 0.85   approved → downloaded
   ÷ 0.70   verified → approved (quality + capacity filtering)
   ÷ 0.70   email submitted → verified
   ─────────────────────────────────────────────────
   ≈ 3,400–4,000 APPLICATIONS REQUIRED
   ÷ 0.08   visit → application (good landing-page conversion)
   ─────────────────────────────────────────────────
   ≈ 45,000–50,000 QUALIFIED SITE VISITS
```

**~50,000 visits is the real target hiding behind "1,000 testers."** Every drop-off rate above is an assumption that must be replaced with a measured number as soon as the funnel is live (W4 exit gate). If `installed → first match` comes in at 45% instead of 70%, the visit requirement jumps by half — which is why that single metric is the one to watch above all others.

### Where 50,000 visits realistically comes from

| Source | Realistic contribution | Effort profile |
|---|---|---|
| **Short-form video (TikTok + YT Shorts)** | 40–60% | Highest leverage, highest volume requirement. One outlier clip can carry 20k+ alone. Requires daily posting cadence — this is what the §W7 content pipeline exists to make survivable. |
| **Reddit (authentic participation)** | 15–25% | r/gamedev, r/unrealengine, r/IndieGaming, extraction-shooter subs. Devlog-format posts convert well; promo posts get removed. |
| **Creators (10k–150k tier)** | 10–20% | Lower volume than short-form but far higher intent — these visitors convert to *installed and playing*, not just email. |
| **Steam page (coming-soon + wishlist)** | 5–15% | Set up early. Steam's own discovery surfaces feed the site. |
| **Discord + word of mouth** | 5–10% | Compounding once the first few hundred testers exist. |
| **X / Instagram Reels** | 3–8% | Repurposed short-form; near-zero marginal cost. |

**Structural consequence:** the content pipeline (W7) is no longer a Tier-3 nicety. At 1,000 testers it is **load-bearing infrastructure** and is promoted into the critical path. A solo studio cannot hand-produce a daily short-form cadence; the render/clip/copy pipeline has to be scripted off the catalog.

---

## 4. VENDOR MAP & THE IDENTITY LAW

### 4.1 What each service actually does today (accuracy statement)

Vendor-agnostic, but precise. This table is the authoritative attribution; anywhere else in this document that conflicts, this wins.

| Function | Service **today** | Status |
|---|---|---|
| **Matchmaking** | **Amazon GameLift Servers FlexMatch** — standalone integration pattern | ✅ **Proven.** Migrated off PlayFab matchmaking. Phase 6 complete; a real 3v3 was played by two humans on cooked binaries, FlexMatch-matched, with bot fill and phase gate working |
| **Match placement** | Own placement service (Lambda) → `StartGameSessionPlacement` → GameLift Servers queue | ✅ Proven. Fix A: out-of-band ready rows via **SNS notifier**. Fix B: `/claim-session` mints player sessions **at travel time**. Fix B2: `DesiredPlayerSessions` removed for claim-based creation |
| **Dedicated server hosting** | **Amazon GameLift Servers** managed fleets, us-east-1 | ✅ Proven |
| **Backend / API / state** | Own AWS: API Gateway + Lambda + DynamoDB + CDK (`Instawerx/Bag_Man_Backend`, stack `BagManTentpoleStack`, acct `302659227808`) | ✅ Proven |
| **Player identity anchor** | **PlayFab** title `1A2077` — `GetResolvedPlayFabId()` | ✅ Proven (A1.4 anti-spoof resolve) |
| **Catalog / inventory / currency** | **PlayFab** — `PurchaseItem` enforces catalog price server-side | ✅ Proven (Marketplace Phase 1 + Phase 2 Visors canary, buy→deduct→grant→entitled→equip→render) |
| **Economy write seam** | `IAFLCosmeticPersistence` → `EarnThroughBackend` / `PurchaseThroughBackend` → `/earn` | ✅ Proven, committed |
| **Auth / voice / friends / anti-cheat** | **EOS** — this scope only | Partial (EOS-AUTH-C2 blocked on Epic App verification) |

**The correction:** PlayFab matchmaking is **retired**. GameLift Servers FlexMatch is the matchmaking and hosting spine. Any tracker row still describing "PlayFab Matchmaking ticket flow" (S11 `AFL-1100`/`AFL-1107`, S12 `AFL-1204`, and the P3 exit-gate line) is **SUPERSEDED** and owes an annotation pass — those rows predate the FlexMatch migration and will mislead a future reader.

**What PlayFab is *not*:** it is not the matchmaker, not the server allocator, not the session broker. It is the **identity anchor and the economy database**, and only because that is what is proven and committed today.

**Product-name accuracy:** AWS renamed the service to **Amazon GameLift Servers** (with GameLift Streams as a separate product). Use the current name in CDK comments, runbooks, and anything external-facing.

### 4.2 The identity law

> **ONE IDENTITY. Everything else is an entitlement on it.**

```
IRONICS ACCOUNT  (permanent, created at beta application)
   ├── entitlement: beta.applicant
   ├── entitlement: beta.tester        cohort=RAMP-C
   ├── entitlement: founder            number=#0047
   ├── entitlement: creator
   ├── role:        moderator
   └── role:        player
```

The website does **not** stand up a second user database with its own passwords. Web signup creates/links the same account the game client authenticates against; the portal stores only *workflow* state (application, cohort, approvals, feedback) in DynamoDB, keyed by the identity anchor.

**Why the law matters more than the vendor:** your entire marketplace anti-spoof ladder resolves ownership through one identity anchor. Two identity systems = two answers to "who owns this," and the A1.4 ladder stops being a proof.

### 4.3 The open fork — does identity consolidate onto AWS?

This is a real decision and it is **not** made by this document. Both paths are defensible:

| | **Keep PlayFab as identity + economy** | **Consolidate onto AWS (Cognito + DynamoDB economy)** |
|---|---|---|
| Cost to adopt | Zero — already proven and committed | High — re-keys `GetResolvedPlayFabId()`, `EarnThroughBackend`, `PurchaseThroughBackend`, the `IAFLCosmeticPersistence` seam, catalog price enforcement, and both dev test accounts |
| Risk | Two vendors, but one is doing a job it's good at | Re-proving the entire A1.4 anti-spoof ladder from scratch, mid-beta |
| Operational surface | PlayFab console + AWS console | One console, one IAM model, one bill, one on-call surface |
| Catalog price enforcement | Free (PlayFab enforces server-side) | You build and prove it yourself |
| Strategic | Vendor dependency on a Microsoft product | Full ownership; aligns with "no third-party platform controls who plays IRONICS" |

**Recommendation:** **keep PlayFab for the beta, and build the seam that makes consolidation a swap rather than a rewrite.** Concretely — the portal API defines an `IIronicsIdentity` interface (`resolveIdentity`, `verifyToken`, `createAccount`, `linkAccount`) with a PlayFab implementation behind it, exactly the way `IAFLCosmeticPersistence` abstracted the persistence backend before PlayFab existed. Nothing in the web app, the launcher, or the admin console calls PlayFab directly.

That gives you the agnosticism you're asking for as *architecture* rather than as *intention*, and it means the consolidation decision can be made on evidence after the beta instead of being forced now, when re-keying the anti-spoof ladder would be the single riskiest thing on the board.

**Corollary (unchanged either way):** founder numbers, beta badges, and launch rewards are **marketplace catalog entitlements** granted through the proven write seam. Founder badge = a catalog row + an asset + the Phase-2 Visors flow. Conform-to-proven; zero new grant architecture.

### 4.4 THE FOUNDER LADDER — three tiers, 100 / 300 / 1,000 (LOCKED)

Founder is not a flat badge. It is a **three-tier nested ladder** keyed to a permanent, sequential, server-issued number.

| Tier | Founder numbers | Slots in tier | Cumulative | Proposed name |
|---|---|---|---|---|
| **I** | `#0001–#0100` | 100 | 100 | **THUNDERBOLT FOUNDER** |
| **II** | `#0101–#0300` | 200 | 300 | **ARC FOUNDER** |
| **III** | `#0301–#1000` | 700 | 1,000 | **SURGE FOUNDER** |

Names map onto the existing economy rarity ladder (SPARK → SURGE → ARC → THUNDERBOLT) so the founder reward for each tier simply *is* an item of that rarity — no parallel naming scheme to maintain. Naming is the Studio Lead's call; the **numbers are locked**.

#### Rules (these are correctness requirements, not policy preferences)

1. **Nested and cumulative.** Tier I holds everything Tier II and III hold, plus its own. A lower tier never receives something a higher tier lacks. `#0047` is a Tier I holder and gets all three grant sets.
2. **Tier is DERIVED, never stored.** `tier = f(founderNumber)`. Storing tier alongside the number creates two sources of truth that will drift. One SSOT — the number.
3. **The number is issued at APPROVAL, not application.** Applying earns nothing. Admission earns the number.
4. **Issuance is ONE atomic server operation.** A single DynamoDB `UpdateItem` on a counter item with `ADD` and `ReturnValues=UPDATED_NEW`, guarded by a `ConditionExpression` that refuses above 1,000. Never read-then-write — two simultaneous approvals would collide on the same number. This is the same law already recorded for the economy: the deduct must be a single atomic DB op.
5. **The cap is enforced server-side, in that same atomic op.** Approval `#1001` receives **no** founder entitlement. If the cap is soft, the scarcity claim is a lie, and it is a lie you will be caught in publicly.
6. **Bulk approval assigns in deterministic order** — `applicationCreatedAt` ascending, tie-broken by `applicationId`. When someone asks why they are `#0101` and their friend is `#0100`, the answer must be a rule, not a shrug.
7. **Numbers are immutable and never recycled.** A ban revokes the entitlement and **retires** the number. Reissuing `#0047` to a second person creates a dispute you cannot win.
8. **Public counters must be truthful.** The site's "N Tier I slots remaining" reads the live counter. Never a hardcoded or decorative number.
9. **Cosmetic and commemorative only.** Zero gameplay advantage, at any tier. This is already project doctrine; the ladder does not get to be the exception.
10. **Creator is a separate axis, not a founder tier.** Creators recruited at RAMP-C would naturally land in Tier III. Do not reserve Tier I numbers for them — grant a `creator` entitlement instead. Conflating the two makes both meaningless.

#### Grant contents (all conform-to-proven: catalog row + asset + Phase-2 flow)

| | Tier III (SURGE) | Tier II (ARC) | Tier I (THUNDERBOLT) |
|---|---|---|---|
| Numbered founder title | ✓ | ✓ | ✓ |
| Permanent founder badge | ✓ | ✓ | ✓ |
| Discord founder role | ✓ | ✓ | ✓ |
| Profile frame | SURGE frame | ARC frame | THUNDERBOLT frame |
| Founder facemask | SURGE variant | ARC variant | THUNDERBOLT variant |
| Founder skin colorway | ✓ (SURGE family) | ✓ (ARC family) | ✓ (THUNDERBOLT family) |
| Nameplate FX | — | ✓ | ✓ (distinct) |
| Founder weapon skin | — | — | ✓ |

Every row above is an existing part type — facemask, skin colorway, weapon skin — already proven end-to-end by the Phase-2 Visors canary. **No new grant architecture, no new part type.** The founder ladder is a content task and a counter, not an engineering project.

#### Open decision
- **Reserved block:** whether `#0001–#0005` are held for internal/team. Recommendation: **do not reserve.** The internal cohort is RAMP-A and is genuinely first — let the numbers be honestly earned. A quietly reserved block is exactly the kind of thing that surfaces later and costs trust.

---

## 5. SYSTEM MAP

```
        TikTok / YT Shorts / Reddit / Creators / Steam page  ── ~50k visits
                                    │
                                    ▼
                        ┌───────────────────────┐
                        │   IRONICS.COM (web)   │  Next.js @ Cloudflare
                        │  3D hero · conversion │
                        └───────────┬───────────┘
                                    ▼
                        ┌───────────────────────┐
                        │   BETA PORTAL         │  authed
                        │  status · launcher    │
                        └───────────┬───────────┘
        ┌───────────────────────────┼───────────────────────────┐
        ▼                           ▼                           ▼
┌───────────────────┐    ┌────────────────────┐      ┌──────────────────────┐
│  IDENTITY         │    │  PORTAL API        │      │  DISTRIBUTION        │
│  IIronicsIdentity │◄──►│  API GW + Lambda   │─────►│  R2 + Worker signer  │
│   └ PlayFab impl  │    │  DynamoDB          │      │  IRONICS LAUNCHER    │
│     (swappable)   │    │  IronicsPortalStack│      │  (chunked/delta)     │
└─────────┬─────────┘    └─────────┬──────────┘      └──────────┬───────────┘
          ▼                        ▼                            ▼
┌───────────────────┐    ┌────────────────────┐      ┌──────────────────────┐
│ ENTITLEMENTS      │    │  ADMIN CONSOLE     │      │  IRONICS CLIENT      │
│ beta / founder    │    │  Beta Control Ctr  │      │  boot auth gate      │
└───────────────────┘    └────────────────────┘      └──────────┬───────────┘
                                                                │
     ┌──────────────────┬──────────────────┬────────────────────┤
     ▼                  ▼                  ▼                    ▼
 ┌──────────────┐   PostHog           Sentry            Discord bot
 │ MATCH SPINE  │   funnels/cohorts   crash + release   role sync + alerts
 │ (AWS-native) │   retention         health            moderation
 └──────────────┘
  GameLift Servers FlexMatch (standalone)
        │  match result → SNS notifier
        ▼
  Placement Lambda → StartGameSessionPlacement → GameLift Servers queue
        │
        ▼
  Managed fleet, us-east-1 · autoscaled 24/7 · ~7 → ~25 processes
        │
        ▼
  /claim-session mints player sessions at travel time
        │
        ▼
 UNATTENDED OPS: CloudWatch alarms → PagerDuty/Discord → automated digests
```

---

## 6. STACK DECISIONS (v2 deltas marked)

| Layer | Choice | Rationale |
|---|---|---|
| Web framework | **Next.js (App Router) + TypeScript** | SSR/SSG for SEO; RSC keeps JS small so the 3D budget isn't eaten by framework weight. |
| 3D | **React Three Fiber + drei + postprocessing** | Declarative scene in the same React tree as UI. Not Pixel Streaming — §8.4. |
| Hosting / edge | **Cloudflare Pages + Workers** | Same platform as R2. |
| **Build distribution** | **R2 + IRONICS Launcher (chunked + delta)** | **Δ v2:** at 1,000 testers a full-build redownload per patch is untenable. See §7. |
| API / state | **AWS API GW + Lambda + DynamoDB**, new `IronicsPortalStack` in CDK | You own the account, deploy path, and auth pattern. Separate stack so a portal deploy can never touch `BagManTentpoleStack`. |
| **Matchmaking** | **Amazon GameLift Servers FlexMatch** (standalone integration) | ✅ Proven. PlayFab matchmaking retired. Match results via SNS → own placement Lambda. |
| **Server hosting** | **Amazon GameLift Servers** managed fleets, us-east-1, queue placement | ✅ Proven. Autoscaled 24/7 per §W6. |
| Identity | **`IIronicsIdentity` seam → PlayFab impl (`1A2077`)** | §4.3. Vendor-swappable by construction; PlayFab kept because it is proven, not because it is chosen forever. |
| Catalog / economy | **PlayFab catalog + `IAFLCosmeticPersistence` seam** | ✅ Proven through the Phase-2 Visors canary. Server-side price enforcement comes free. |
| Product analytics | **PostHog** | Funnels, cohorts, replay, flags, A/B in one tool. |
| Web vitals / traffic | **Cloudflare Web Analytics** | Cookieless, edge-accurate. |
| Crash reporting | **Sentry** (UE SDK + Crash Reporter) | Symbol upload + release health — crash-free-sessions is literally an exit-gate number. |
| Transactional email | **Resend** | Verification, approval, patch notes, password reset. |
| Marketing email / CRM | **Loops** | Kept separate so a marketing deliverability issue can never break account verification. |
| Community | **Discord** + custom bot | Entitlement→role sync, alerts, moderation tooling. |
| Support | In-client F8 → R2 + DynamoDB → Discord; **+ knowledge base** | **Δ v2:** at 1,000 testers a KB is required to keep the same questions from consuming the day. |
| **Anti-cheat** | **EAC via EOS — moved into beta scope** | **Δ v2:** 1,000 continuously-playing strangers is a cheating population. At 36 supervised testers it wasn't. |
| **Moderation** | **Volunteer mod team recruited from early cohorts** | **Δ v2:** 1,000-member Discord needs 3–5 mods across timezones. |
| Code signing | **Azure Trusted Signing** | SmartScreen blocks unsigned installers — a measurable conversion cliff at the install step. |
| Legal | Counsel-reviewed | Virtual currency + real-money marketplace. §13. |

**Still not building:** own CDN/DDoS/object storage, own email delivery, own crash symbolication, ticket system, data warehouse, Steam-as-beta-gate (Steam is discovery + launch channel; the gate stays IRONICS-owned).

---

## 7. DISTRIBUTION AT 1,000 TESTERS — THE COST WALL

This is the single largest v2 change and it is a hard number, not a preference.

> **MEASURED, not assumed (W2A-1 interim).** The 15 GB figure this section was originally built on was wrong by ~5×. Development cook measures **2,757 MB compressed / 3,116 MB staged paks**. Shipping-cook figure pending; prune projection lands ~1.2–1.6 GB. All arithmetic below is restated on the measured number.

```
2.76 GB × 1,000 testers × 1 initial download            ≈  2.8 TB
2.76 GB × 1,000 × 6 patch cycles (full redownload)      ≈ 16.6 TB
                                                  TOTAL ≈ 19.4 TB
        (pre-measurement assumption was ≈ 105 TB)
```

| Path | Egress cost |
|---|---|
| S3 + CloudFront @ ~$0.085/GB | **≈ $1,650** (was projected $8,900) |
| **Cloudflare R2 (zero egress)** | **≈ $0** (storage only, single-digit $/mo) |
| R2 + engine-native patch paks | ≈ $0 egress, and a patch is minutes not tens of minutes |

**What survives the correction, and what does not:**

1. **R2 is still correct.** $1,650 avoided is still real money for a solo studio, R2 costs nothing to adopt, and the number scales with every tester added past 1,000. Unchanged.
2. **~~The launcher moves into critical path.~~ REVERSED — see §7.2.** That call was made *because* of the 15 GB figure. At 2.76 GB (and ~1.4 GB post-prune) a full re-install is roughly 2–7 minutes on typical broadband, not 40. That is an annoyance, not a retention cliff, and it does not justify launcher engineering before RAMP-C.

### 7.1 Patching: use the engine's native release/patch system, NOT a custom differ

**Correction to the earlier draft.** v2 called for a custom chunked binary-diff system. That is the wrong first answer — UE already ships the mechanism:

```
BASE BUILD:   RunUAT BuildCookRun ... -stage -pak -createreleaseversion=IRONICS_B1
PATCH BUILD:  RunUAT BuildCookRun ... -stage -pak -generatepatch -basedonreleaseversion=IRONICS_B1
                                      [-addpatchlevel to stack multiple patches]
```

The cooker diffs post-cook content against the retained release, and emits a `_P` pak/container holding only changed packages. It mounts automatically ahead of the base pak. The launcher then only has to **download the base once and each `_P` file after** — no custom diffing, no chunk store, no integrity system beyond a hash per file.

**The four things that will bite, stated up front:**

1. **Patch granularity is the whole package.** Change one byte in a `.uasset` and the entire package ships in the patch. Patch size is therefore governed by **asset hygiene**, not by the patcher: no gratuitous resaves, no bulk re-saves of untouched maps, no "save all" before a release build. This is the single biggest lever on the <10% gate, and it is a *discipline* lever, not a technical one.
2. **The released cooked content and its metadata are the diff basis and must be retained forever.** Archive `Releases/<version>/<platform>/` to R2 alongside the build. Lose it and you can never patch against that version again — every tester takes a full redownload.
3. **Patch stacks accumulate.** Each `-addpatchlevel` adds a mounted layer. Rebase to a new release version when the cumulative patch payload exceeds ~30% of base, or every ~6–8 patches, whichever comes first. Rebase = one full redownload, so schedule it, announce it, and never let it surprise a tester.
4. **IoStore + patching has known engine friction** (e.g. missing `.sig` files in staged patch output when pak signing is on — UE-207430). **Validate patch generation on the exact IoStore/signing config before RAMP-C**, not on the day of the first content patch. Fallback if it misbehaves: `-skipiostore` legacy paks, at a load-time cost.

### 7.2 Launcher — DEFERRED again on evidence

The measured build size removes the justification that put the launcher on the critical path. Revised position:

| | Decision |
|---|---|
| **Engine-native patch paks (§7.1)** | **IN SCOPE NOW.** Near-zero cost to adopt, engine-supported, and it shrinks every patch regardless of launcher. |
| **Custom launcher** | **DEFERRED to RAMP-C review.** Re-open if: post-prune Shipping build >4 GB, patch cadence exceeds ~1/week, or measured `downloaded → installed` drops below 90%. |
| **Interim delivery** | Portal-authenticated download of the signed installer, plus a small **patch applier** shipped inside the installer that fetches `_P` files and drops them into the pak search path. Days of work, not weeks. |

If the launcher is later built, scope stays: login via `IIronicsIdentity` → entitlement check → manifest → base + ordered `_P` files → SHA-256 verify → launch; self-update; patch notes. Never: custom binary diffing, chunk store, store, social, mod support, streaming.

### 7.3 Build size reduction is a prerequisite, not a cleanup

Every GB removed multiplies by 1,000 testers and by every patch cycle. It also shrinks the *diff surface* — fewer packages means fewer packages that can change. **See Phase W2A: this is now a gated phase with its own exit criteria, not a line item.**

---

## 8. PHASE PLAN

### PHASE W0 — FOUNDATION & PROVISIONING
**Lane: OPERATOR** · Gates everything.

1. Domain + Cloudflare DNS. Brand resolution: BAG MAN retired forward-facing; every public surface is IRONICS. Trademark clearance before brand spend.
2. Company of record for ToS/Privacy.
3. Provision: Cloudflare, PostHog, Sentry (UE project), Resend (verified sending domain, SPF/DKIM/DMARC), Loops, Discord, code-signing identity, EOS/EAC credentials.
4. Create `Instawerx/Ironics_Web`, branch `main`. **State the branch explicitly** — game is `personal/main`, backend is `origin/master`; do not let a third repo inherit an assumed default.
5. Legal drafts commissioned (§13).

**Exit gate:** domain resolves · email domain passes SPF/DKIM/DMARC (cite output) · all accounts recorded in a secrets manager (**never** in either repo) · repo + branch documented here · legal drafts received.

---

### PHASE W1 — IDENTITY & PORTAL API
**Lane: CLAUDE CODE** · Entry: W0.

**DynamoDB tables:** `Testers` (PK `playFabId`) · `Applications` · `Cohorts` · `Builds` · `Entitlements` (PK `playFabId`, SK `entitlementId`) · `AuditLog` (append-only) · `Feedback`.

**Endpoints** (same gateway family as `/earn`):

| Method | Path | Auth | Purpose |
|---|---|---|---|
| POST | `/beta/apply` | none + rate limit + Turnstile | Create application, send verification |
| GET | `/beta/verify` | signed token | Confirm email, create identity account (`IIronicsIdentity.createAccount`) + `Testers` row |
| GET | `/beta/me` | identity token | Portal status |
| POST | `/beta/accept-terms` | identity token | Record NDA/terms acceptance + version |
| POST | `/beta/manifest` | identity token | Authorize, return build manifest + signed chunk URLs |
| POST | `/beta/authorize` | identity token | **In-client boot gate** → session token + watermark payload, or 403 |
| POST | `/beta/heartbeat` | session token | Re-validate; enables mid-session revocation |
| POST | `/beta/feedback` | session token | Bug report intake |
| ADMIN | `/admin/*` | separate admin auth + IAM | Approve, revoke, cohort, publish, metrics |

**`/beta/authorize` logic — the security core:**
```
verify identity token server-side (never trust client claims)
→ resolve identity id      (reuse the A1.4 resolve — do not reimplement)
→ assert status == APPROVED · not banned · terms version current
→ assert cohort OPEN · buildId PUBLISHED and >= cohort.minBuild
→ assert reported build sha256 matches Builds row
→ issue session token (TTL 30 min, refreshed by heartbeat)
→ return { sessionToken, testerNumber, cohort, watermark, ttl }
FAIL → 403 with machine-readable reason code
       (UPDATE_REQUIRED and REVOKED must read differently to the tester)
```

**Scale requirements (Δ v2):** every endpoint load-tested to **10× expected peak** — `/beta/authorize` at 400 rps, `/beta/heartbeat` at sustained 1,000-session load. DynamoDB on-demand capacity. Lambda reserved concurrency set so a portal spike cannot starve the proven tentpole functions.

**Exit gate:** every endpoint correct against a live curl script (output in ledger) · non-approved account gets 403 · revoked session refused at next heartbeat · stale build gets `UPDATE_REQUIRED` · `AuditLog` row per approve/revoke/download · **load test at 10× peak passes with p99 <400ms** · `IronicsPortalStack` deploys/destroys without touching `BagManTentpoleStack`.

---

### PHASE W2A — BUILD AUDIT & PRUNE (ground truth before anything ships)
**Lane: CLAUDE CODE (analysis) + AIK (in-editor reference work) + OPERATOR (cooks/builds)** · Entry: W1. **Blocks W2.**

We do not currently know what the shipping game weighs or what is in it. Every downstream number — install-abandon rate, patch size, the <10% gate, the R2 bill, the launcher's job — currently rests on an assumed 15 GB. **Replace the assumption with a measurement before building distribution around it.**

#### The measurement (engine-native, no third-party tooling required)

1. Shipping cook, then read the artifacts the cook already produces:
   - `Saved/Cooked/Windows/<Project>/Metadata/DevelopmentAssetRegistry.bin`
   - `Saved/Logs/Cook-*.txt`
2. Per-asset and per-class cooked size CSVs, via the engine commandlet:
   ```
   UnrealEditor-Cmd.exe -run=AssetSizeQuery
       -AssetRegistry="...\Metadata\DevelopmentAssetRegistry.bin"
       -CSV="...\AssetSize_Assets.csv"  -CSVType=Assets
   UnrealEditor-Cmd.exe -run=AssetSizeQuery
       -AssetRegistry="...\Metadata\DevelopmentAssetRegistry.bin"
       -CSV="...\AssetSize_Classes.csv" -CSVType=Classes
   ```
3. Cross-check against the staged `Content/Paks` on-disk footprint (`.pak` / `.utoc` / `.ucas`) so compressed reality is recorded, not just uncompressed asset weight.
4. In-editor corroboration (AIK lane, UE 5.4+): **Tools → Audit → Asset Disk Size**, loading the same `DevelopmentAssetRegistry.bin`; **Size Map** and **Reference Viewer** for individual suspects.

#### MEASURED — W2A-1 interim (Development cook) · the three predicted suspects were WRONG

**Baseline: 2,757 MB compressed / 3,116 MB staged paks.** Not 15 GB.

| Predicted suspect | Measured | % of build | Verdict |
|---|---|---|---|
| (a) `AFLBagMan` GameFeature content | 0.85 MB | 0.03% | **Wrong.** GameFeature content *does* cook wholesale, but AFLBagMan's `Content/` is nearly empty — the content lives in `/Game` |
| (b) Always-cook directories | 35.78 MB | 1.3% | Minor |
| (c) Lyra residue | ~91 MB | 3.3% | Minor |
| **All three combined** | **~127 MB** | **~4.6%** | Not the problem |

**What actually dominates — 61% of the build in two blocks nobody named:**

| Block | Size | % |
|---|---|---|
| **Five marketplace environment packs** — Sci_FI_Valley_Village 475 MB · ShantyTown 387 MB · DeepWaterStation 277 MB · SpaceshipInterior 82 MB · CyberPunkAssets 58 MB | **1,279 MB** | **46%** |
| **MetaHuman + NNEDenoiser editor ML models** | **406 MB** | **14.7%** |

#### The one real force-cook leak (AIK, W2A-2)

`PrimaryAssetTypesToScan` for `Map` uses `Directories=((Path="/Game/Maps"))` with **`CookRule=AlwaysCook`**. Every `.umap` under `Content/Maps` is force-cooked **with its full reference graph**, whether or not any playlist binds it.

`L_ValleyVillage` is bound by **no playlist** and drags in a 2.5 GB source marketplace pack → **475 MB cooked, 17% of the build, for a map nobody can play.** `L_Arena_05` rides the same rule.

**Important nuance that prevents wasted work:** `Content/` on disk is 65 GB, `DirectoriesToNeverCook` has exactly one entry, and `bOnlyCookProductionAssets=False` — which *looks* alarming. It isn't. 65 GB of source producing 2.76 GB cooked means the reference graph is already doing the exclusion work correctly. **Do not spend a week building an exclusion list.** The AlwaysCook map-directory rule is the leak; fix that one rule and the unbound maps stop pulling their packs in.

#### Prune targets, in measured order of return

1. **Unbind the force-cook rule from unplayable maps** — `L_ValleyVillage`, `L_Arena_05`, `L_BagMan_Greybox`. Expected ≥475 MB.
2. **MetaHuman + NNEDenoiser** — 406 MB of *editor* ML models in a game build. NNEDenoiser is the path-tracer denoiser; it has no runtime role. MetaHuman is only needed if a MetaHuman actually ships, and IRONICS characters are robots. Verify, then disable for the game target.
3. **`DeepWaterStation` / `SpaceshipInterior` / `CyberPunkAssets`** — 417 MB. Establish which *shipping* map references each; if the referencing map is not in the playable set, they leave with it.
4. **`TopDownArena` / `ShooterTests`** — enabled, unreferenced by the AFL playable set. `ShooterExplorer` is on disk but absent from the `.uproject`.
5. **`B_IRONICS_Armory_Experience`** — confirmed true orphan (0 hard refs, 0 soft refs).
6. Textures/audio/animation compression pass — *after* the above, since it applies to whatever survives.

#### Prune order (reversible, never destructive-first)

**Disable → re-cook → measure → only then delete.** A deletion that turns out to be load-bearing costs a day; a disabled plugin costs a re-cook. Every prune step is one change with a measured before/after — the same fix-to-diagnose discipline that got the shipping cook green in one line rather than a migration.

Known hazards while pruning:
- Test/dev modules must be `DeveloperTool`, not `Runtime`. This exact class of mistake (`AFLCombatTests`) was the real shipping-cook blocker once already.
- `mem-#21` `/Game` → GameFeature leftovers: content still sitting in `/Game` may still be cooking. Previously demoted to hygiene; **in this phase it is in scope**, because hygiene is now measured in gigabytes.
- Client-cull ≠ server-cull. Do not prune anything the server target needs under the two-engine partition.

#### Define the minimum playable set

**ESTABLISHED (W2A-2).** Derived from all 26 playlists in `/Game/BagMan/Playlists`, read by `MapID` / `ExperienceID`:

- **Maps that ship — 6:** `L_ShantyTown` (18 playlists) · `L_Arena_04` (4) · `L_Arena_01` (2) · `L_Duel_01` (1) · `/ShooterMaps/Maps/L_Expanse` (1, **Lyra stock still in the shipping set — operator ruling owed**) · `L_IRONICS_Armory` (GameDefaultMap / front end)
- **Not in the set:** `L_Arena_05`, `L_ValleyVillage`, `L_BagMan_Greybox`, `L_LyraFrontEnd`
- **Experiences that ship — 21:** 19 × `B_AFLExperience_*` + `EXP_AFL_Duel01_Shootout` + `B_LyraFrontEnd_Experience`. Note `B_Experience_BagMan` / `_Haywire` / `_ProMod` are still bound via map `WorldSettings` on `L_Arena_04` and `L_BagMan_Greybox` despite no playlist naming them — pruning a map can therefore orphan an experience, and vice versa. Sequence accordingly.
- **GameFeature plugins — 10, all `ExplicitlyLoaded`:** AFL — `AFLCore`, `AFLCombat`, `AFLMovement`, `AFLDismember`, `AFLBagMan`. Lyra stock still enabled — `ShooterCore`, `ShooterMaps`, `ShooterTests`, `TopDownArena`, `ShooterExplorer`.
- **Primary asset labels — 3:** `Content/DefaultGame_Label` (`bLabelAssetsInMyDirectory=True` at `/Game` root — claims the whole tree) · `ShooterMaps_Label` · `TopDownArena_Label`
- **Always-loaded non-GameFeature modules:** `AFLOnline` (owns `AFLMatchmakingSubsystem` and the `ClientTravel` path) and `AFLGameCore` — both load unconditionally, independent of experience.

**Exit gate:**
- [x] Development-cook size recorded: **2,757 MB compressed / 3,116 MB staged** (W2A-1 interim). ✅
- [ ] **Shipping-cook** size recorded — the authoritative number; Development is not it. In flight.
- [ ] `AssetSize_Assets.csv` + `AssetSize_Classes.csv` produced and committed to `Docs/`. Cited.
- [ ] Top 50 assets and full class breakdown classified **SHIPPING-REQUIRED / DEV-ONLY / LYRA-SAMPLE / ORPHAN**. Each ORPHAN verdict backed by a Reference Viewer result, not an assumption.
- [x] **Minimum playable set written down** — 6 maps, 21 experiences, 10 GameFeatures, 3 labels, 2 always-loaded modules (W2A-2). ✅
- [ ] Prune executed; before/after size delta recorded per step.
- [ ] Shipping cook still succeeds post-prune. Cited.
- [ ] **Game still plays: 2-client watched extraction match on the pruned build.** Nothing is pruned-clean until this passes.
- [ ] Texture / audio / animation compression settings pass completed and recorded.

---

### PHASE W2 — DISTRIBUTION & LAUNCHER
**Lane: CLAUDE CODE (service + launcher) + OPERATOR (builds, signing, publish)** · Entry: **W2A** (you cannot size distribution against an unknown build).

**Doctrine carry-over:** beta builds are Shipping cooks → D: source engine lane → operator-owned. Every D: session ends with the C: launcher `LyraEditor` rebuild before the editor reopens. No exception for "just a beta build."

1. R2 bucket, private. Worker mints signed chunk URLs (TTL ≤15 min), one `AuditLog` row per manifest issuance.
2. Build size **already reduced and measured in W2A** — record the final number in `Builds`.
3. **Engine-native release/patch versioning** (§7.1): base cooked with `-createreleaseversion`, patches with `-generatepatch -basedonreleaseversion`. Release cooked content archived to R2 **permanently** — it is the diff basis. Manifest lists base + ordered `_P` files with SHA-256 each.
4. **IRONICS Launcher** (scope in §7), code-signed installer, ships the Sentry crash handler.
5. **Publish flow as an operator-run script**, not manual steps: cook → chunk → hash → upload → `POST /admin/builds` → flip published → Discord + Resend announcement.
6. **Rollback**: publishing N+1 never deletes N; `minBuild` is the only kill switch, revertible in one admin call.

**Exit gate:** clean-machine install with no SmartScreen block (screenshot) · signed URL 403s after TTL · URL shared unauthenticated fails · patch pak measured **<10% of full build size** on a real content change · patch generation validated on the exact IoStore/signing config · `minBuild` bump forces `UPDATE_REQUIRED` · rollback exercised · release metadata archived and restore-tested.

---

### PHASE W3 — IN-CLIENT GATE, WATERMARK, ANTI-CHEAT & FEEDBACK
**Lane: AIK (UI) + CLAUDE CODE (C++) + OPERATOR (build)** · Entry: W2.

1. **Boot gate** in an always-loaded non-GameFeature module (same law that governs `AFLNetTypes` — it must exist before any experience loads). Chain: identity login → token → `POST /beta/authorize` → session token held in memory only. On 403: blocking modal with the reason code and no path into the main menu.
2. **Heartbeat** every 5 min; two consecutive failures → graceful return to the gate screen. Never a hard crash.
3. **Watermark widget** (AIK, CommonUI, into `LyraHUDLayout` — never a standalone HUD): `IRONICS CLOSED BETA · TESTER #00418 · BUILD 0.8.14 · <UTC>`. Payload is **server-sourced**, never local config. Opacity 12–18%, repositions every 45–90s, present in-match *and* in menus/marketplace. Cheat-guarded.
4. **EAC integration (Δ v2)** — 1,000 continuously-playing strangers is a cheating population. Already scoped in the game lane (AFL-1208); it moves from "public launch" to "before the ramp passes ~250 testers."
5. **F8 bug report** (AIK): severity + text + last 200 log lines + screenshot + buildId + testerNumber + map/phase → `/beta/feedback` → Discord `#bug-reports`.
6. **Telemetry schema committed to the repo** — session start/end, first match completed, match completed, marketplace open, item view, purchase attempt/success, equip, crash. **Renaming events later destroys your funnels.**

**Exit gate:** non-entitled account cannot reach main menu (operator-watched) · watermark legible and server-sourced in a screenshot · admin revoke ejects a live client within one heartbeat (2-machine) · EAC blocks a known-bad injection in a controlled test · F8 report lands in Discord with attachments · telemetry lands in PostHog with correct `playFabId` + `buildId`.

---

### PHASE W4 — THE AAA 3D WEBSITE
**Lane: CLAUDE CODE** · Entry: W0. Runs parallel to W1–W3; must not block them.

> A 3D hero is a conversion liability until proven otherwise. The AAA answer is not less 3D — it is **3D that never delays first paint or the primary CTA.** That's an engineering constraint enforced by CI, not a taste argument. At ~50,000 required visits, a 15% conversion penalty from a heavy hero costs ~150 testers.

#### 8.1 Performance budget (CI merge gate)

| Metric | Gate | Enforcement |
|---|---|---|
| LCP (mobile p75) | ≤ 2.0 s | Lighthouse CI per PR |
| INP | ≤ 200 ms | Lighthouse CI + PostHog RUM |
| CLS | ≤ 0.05 | Lighthouse CI |
| Initial JS (pre-3D) | ≤ 180 KB gz | bundle-analyzer gate |
| 3D payload (hero total) | ≤ 2.5 MB gz | asset budget check |
| Time to CTA interactive | ≤ 1.2 s | Lighthouse CI |
| Lighthouse Perf (mobile) | ≥ 90 | **merge blocker** |

If the 3D hero can't hit these, it ships as video. The number decides, not the argument.

#### 8.2 Progressive 3D architecture

```
1. HTML + CSS + AVIF poster + headline + CTA   → paints immediately, zero JS
2. CTA interactive with no JS dependency on the primary path
3. After LCP + requestIdleCallback + IntersectionObserver:
      → dynamic import R3F canvas → crossfade poster to live scene
4. Bail out entirely (poster or looping AV1 video) on:
      prefers-reduced-motion · saveData / <4g · deviceMemory <4 · no WebGL2
```

**The CTA is never inside the canvas.** The conversion path must survive total WebGL failure.

#### 8.3 3D asset pipeline

```
UE5 / Blender source → glTF 2.0
   → Meshopt or Draco geometry compression
   → KTX2 / BasisU textures (~6–8× smaller than PNG, GPU-native)
   → web LOD0: ≤80k tris, ≤4 materials, 2K max
   → baked lighting / single env map — no realtime shadow-casting lights
   → bake a turntable to AV1 as the guaranteed fallback
```

Reuse `afl-blender-bridge` export discipline. **The web mesh is a derived asset with its own budget** — reusing the game mesh is how a 40 MB hero happens.

**Hero concept:** a **live robot configurator**, not a passive scene. Visitor rotates a robot and swaps colorway/facemask in-browser. It demonstrates "build your identity" in the first ten seconds and creates a reason to make an account (save your build). Your 48-color system and facemask parts are already authored — this is data reuse, not new art.

#### 8.4 Why not Pixel Streaming

Per-visitor GPU cost, cold-start latency, concurrency limits, total failure on mobile data. Wrong tool for top-of-funnel. Revisit only for a gated in-browser demo behind an account.

#### 8.5 Page inventory & conversion rules

| Page | Primary CTA |
|---|---|
| `/` Home | **Request Beta Access** — 3D configurator hero, live tester counter |
| `/beta` | Application — **single email field first**, then progressive profiling |
| `/market` | Request Beta Access — the marketplace story, your actual differentiator |
| `/robots` | Request Beta Access — customization, 48-color system, parts |
| `/roadmap` | Join Discord |
| `/press` | Download kit |
| `/creators` | Apply as creator |
| `/portal` | (authed) status, launcher, terms, feedback |
| `/legal/*` | ToS, Privacy, Beta Agreement, Community Guidelines, VC Terms |

Rules applied without exception: one primary CTA per page, repeated never competing · email in one field, everything else progressive · social proof above the fold once real · **scarcity must be true** (real remaining slots) — fabricated scarcity is a trust bomb with a delayed fuse · exit intent → email capture · every page SSR'd and crawlable · full OG/Twitter cards (a link dropped in Discord that renders as a bare URL is wasted acquisition).

#### 8.6 Measurement

- **UTM taxonomy locked before the first post ships**, documented here, enforced by a link-builder script. Retro-fixing attribution is impossible.
- Server-side capture for critical funnel steps — ad blockers eat 15–30% of client events and your denominators must not be fiction.
- Funnel instrumented end to end: `visit → CTA → email → verified → approved → downloaded → installed → first match completed`.
- **A/B testing on the hero and CTA is in scope at this volume.** At 50k visits, a 1-point conversion lift is 500 extra applications. PostHog flags make this nearly free.

**Exit gate:** Lighthouse mobile ≥90 **with 3D enabled** (report cited) · CTA interactive with JS disabled · bail-outs verified on reduced-motion, no-WebGL2, throttled 3G · full funnel visible in PostHog from a real walk-through · legal pages live and linked · OG cards render in Discord, X, iMessage.

---

### PHASE W5 — BETA CONTROL CENTER
**Lane: CLAUDE CODE** · Entry: W1.

```
IRONICS ADMIN
──────────────────────────────────────────────────────────
TESTERS     Applications ####  Approved ####  Active ####  Suspended ##
POPULATION  Registered ####    DAU ###    CCU now ##    Peak 24h ##
FLEET       Servers up ##/##   Autoscale ACTIVE   Queue depth ##
BUILD       Current 0.8.14     minBuild 0.8.14    Published ✓
HEALTH      Crash-free ##.#%   Login ##.#%   API 5xx #.#%   p50 fill ##s
FEEDBACK    New ##   Critical #   High #        MOD QUEUE  ##

[APPROVE] [BULK APPROVE N FROM WAITLIST] [REVOKE] [BAN]
[OPEN/CLOSE COHORT] [PUBLISH BUILD] [SET MINBUILD] [BROADCAST]
```

Separate admin auth (not a player token) · every mutating action writes `AuditLog` · destructive actions require typed confirmation · **bulk approve throttled server-side** so the ramp can't outrun the fleet.

**Exit gate:** approve → email → portal flips → Discord role granted (end-to-end) · revoke ejects a live client · bulk approve respects the throttle · `AuditLog` row for every action in the test run.

---

### PHASE W6 — UNATTENDED OPERATIONS
**Lane: CLAUDE CODE + OPERATOR** · Entry: W2 + FlexMatch Phase 7 prerequisites (game lane). **This phase is what makes continuous availability possible.**

1. **Fleet autoscaling**: **Amazon GameLift Servers** target-tracking on available-session headroom. Floor of 2 warm processes 24/7 (off-peak testers must never hit a cold start), scaling to 25+ on spike. Spot with on-demand fallback. Placement stays on the proven path — SNS notifier → placement Lambda → `StartGameSessionPlacement` → queue → `/claim-session` at travel time.
2. **Single-region start (us-east-1)** to concentrate the matchmaking pool. Adding a second region before the population justifies it splits the pool and makes off-peak worse, not better. Revisit at measured regional demand.
3. **Off-peak fill policy**: bot fill (proven) is the tail-hours backstop, with `humanFillRatio` tracked per match. Widening FlexMatch latency/skill bands over ticket age (rule-set expansions) keeps queues moving; FlexMatch automatic backfill is the second lever once matches are live. **If a real 3v3 never happens off-peak, that is a finding to surface, not a number to hide.**
4. **Alerting** — the replacement for a human watching six matches:
   - Crash-free rate drops below threshold → alert
   - Login success below 99% → alert
   - p95 match-fill above threshold → alert
   - API 5xx above 0.5% → alert
   - Fleet at capacity / scaling failure → **page**
   - Zero sessions started in N minutes during expected-active hours → **page** (catches silent total failure, the one a dashboard won't show you)
5. **Automated daily digest** to a private Discord channel: DAU, peak CCU, new registrations, funnel conversion per step, crash-free, top 5 errors, new critical feedback, retention cohort curve.
6. **Runbooks** for: fleet exhausted, placement failures / queue timeouts, FlexMatch ticket backlog, bad build published, identity-provider outage, economy-backend degradation, mass-crash event, leak/ban response.

**Exit gate:** fleet autoscales under synthetic 120-CCU load, verified in CloudWatch · every alarm fires in a deliberately induced failure (cite each) · daily digest generates automatically · **soak test: 48 hours unattended with synthetic traffic, zero manual intervention** · runbooks written and one rehearsed.

---

### PHASE W7 — COMMUNITY, CRM & CONTENT PIPELINE
**Lane: OPERATOR (owner) + CLAUDE CODE (automation)** · Entry: W0; continuous. **Δ v2: promoted to critical path** — §3 makes this the acquisition engine, not a support function.

**Discord:** `#announcements` `#patch-notes` `#general` `#marketplace` `#screenshots` `#suggestions` `#bug-reports` (bot-fed) `#support` `#creators`, cohort-gated ramp channels, staff `#ops` `#alerts`.

**Bot:** entitlement→role sync, patch announcements, digest posting, feedback relay, `/status`, `/mybuild`, mod tooling.

**Moderation (Δ v2):** recruit **3–5 volunteer mods from the earliest cohorts** before the population passes ~300. Give them a Moderator entitlement, a written escalation policy, and a private channel. A 1,000-member Discord without timezone coverage becomes a liability overnight.

**CRM:** every applicant enters Loops tagged `{status, cohort, referralSource, utm_source, platform, region}`. Waitlisted applicants are **not discarded** — they are the launch list. Segments: `applicant-waitlist`, `tester-active`, `tester-lapsed`, `founder`, `creator`.

**Content pipeline — scripted off the catalog, one process per drop:**
```
new catalog item →
  ├── marketplace listing        (already automatic — data-driven catalog)
  ├── 1024² thumbnail            (scripted render)
  ├── promo still × 3 aspects    (scripted render)
  ├── 6–12s vertical clip        (turntable / equip moment)
  ├── social copy × 3 variants   (Shorts/TikTok, X, Reddit)
  ├── patch-note entry
  └── creator asset drop         (transparent PNG + clip)
```
Your catalog is already the SSOT and pedestals already spawn from it. Extend that same data-driven property to marketing assets. **Script the renders** — hand-producing a daily cadence is what stops a solo studio from reaching 50,000 visits.

**Channel priority:** Tier A — YT Shorts, TikTok, Discord, Reddit, Steam page (wishlists early). Tier B — X, Instagram Reels. Tier C — LinkedIn (studio credibility), Facebook.

**Exit gate:** Discord live with bot role sync proven · ≥3 mods onboarded with written policy · Loops receiving tagged applicants · **one full content-pipeline run executed end-to-end on a real cosmetic, artifacts cited** · Steam coming-soon page live with wishlists enabled.

---

### PHASE W8 — RAMP TO 1,000
**Lane: OPERATOR (GO calls) + all** · Entry: W1–W7 exits.

The ramp exists to **de-risk scale**, not to ration seats. Each stage is a technical gate; once passed, the population stays live continuously and the next stage opens. **No stage closes behind you.**

| Stage | Cumulative registered | Expected peak CCU | What this stage proves | Exit gate |
|---|---|---|---|---|
| **RAMP-A** Internal | 10 | ~6 | Install → auth gate → match on a shipped build outside the dev machine | 1 full match · 0 blocking bugs · install works on a machine that has never had UE5 |
| **RAMP-B** Technical | 50 | ~8 | Hardware/driver spread, install friction, SmartScreen, first-run onboarding, crash reporting actually reporting | ≥90% install success · ≥2 GPU vendors · Sentry symbolicating · crash-free ≥90% |
| **RAMP-C** Continuous | 250 | ~15 | **First real continuous availability.** Off-peak queues, unattended overnight operation, patch cadence, EAC live | crash-free ≥95% · login ≥98% · p50 fill ≤90s peak · **one full unattended weekend, zero manual intervention** · EAC live |
| **RAMP-D** Scale | 600 | ~25 | Support/moderation load, retention curve emerges, marketplace transaction volume, delta patching under real load | crash-free ≥96% · D7 ≥30% · ≥50% transaction rate · support response p50 <24h · delta patch <10% full size at scale |
| **RAMP-E** Full | **1,000** | ~40 (≈120 burst) | The beta objective. Launch dress rehearsal. | **All §10 North Star targets met** |

**Ramp gate rule:** open the next stage only when the current stage's gate is proven **and** the fleet has ≥2× current peak headroom. Approvals are throttled by the admin console so the ramp cannot outrun the infrastructure.

**Waitlist is a feature.** With ~4,000 applications and 1,000 seats, the waitlist is both real scarcity (honest, verifiable) and the launch-day list. Waitlisted applicants get patch notes, roadmap updates, and early Steam wishlist prompts — they are not dead records.

---

### PHASE W9 — CREATOR PROGRAM
**Lane: OPERATOR** · Entry: RAMP-C exit (never expose creators to a pre-stable build).

Target 10k–150k-audience creators: UE5/gamedev, robotics/mech, extraction-shooter mid-tier streamers, VR/tech, indie YouTubers. They convert far better per-viewer than 1M+ generalists and they will actually reply.

Offer: early access · Founder + Creator entitlement · creator code · press/creator kit · direct line to the developer. **Watermark stays on** unless a build is explicitly cleared for coverage — clearance is an admin action with an audit row.

**Exit gate:** ≥25 creators onboarded with codes · ≥10 published pieces · per-code referral attribution visible in PostHog · **measured contribution to the 50k visit target**.

---

### PHASE W10 — LAUNCH READINESS
**Lane: all** · Entry: RAMP-E exit.

Inherits every proven system above, plus: trailer, press kit, Steam page complete, payment/tax provisioning (Stripe/Xsolla + Stripe Tax), refund policy, incident runbooks, rollback plan, status page, war-room schedule.

**The strategic payoff of the v2 model:** launch day is not a cold start. It is activating 1,000 people who already have accounts, founder numbers, Discord roles, and an installed, patched client.

---

## 9. CAPACITY & COST AT 1,000 TESTERS

**Fleet sizing:**
```
~40 sustained peak CCU ÷ 6 players per match  =  ~7 concurrent server processes
~120 burst peak CCU    ÷ 6                    =  ~20 concurrent server processes
+ headroom                                     =  provision to 25
```
At several server processes per instance, that is a handful of EC2 instances behind a GameLift Servers managed fleet. **Server capacity was never the constraint.**

| Item | Monthly estimate | Note |
|---|---|---|
| GameLift Servers fleet + FlexMatch, ~7 sustained / 25 burst, spot + on-demand fallback | $150–400 | Dominant line item; scales with real CCU. FlexMatch is billed per matchmaking ticket-event — negligible at this volume but non-zero |
| Lambda + API GW + DynamoDB (on-demand) | $20–60 | 1,000 clients heartbeating every 5 min |
| R2 storage + egress | $10–25 | **Zero egress** — vs. **~$8,900** on CloudFront (§7) |
| Cloudflare Pages/Workers | $5–20 | |
| PostHog | $50–150 | Event volume rises with population |
| Sentry | $30–80 | |
| Resend + Loops | $40–80 | List of ~4,000 |
| Code signing (amortized) | ~$10 | |
| Domain | ~$2 | |
| **Total** | **≈ $320–830/mo** | Verify every line against live billing before treating as fact |

Roughly **$0.30–0.80 per tester per month.** The 1,000-tester beta is affordable; the constraint was never money or servers.

---

## 10. NORTH STAR METRICS (v2 — standard live-service, no window proxies)

| Category | Metric | Beta target |
|---|---|---|
| **Acquisition** | Qualified visits | ~50,000 cumulative |
| | Visit → application | ≥8% |
| | Email → verified | ≥70% |
| **Activation** | Approved → downloaded | ≥85% |
| | Downloaded → installed | ≥90% |
| | **Installed → first match completed** | **≥70%** |
| **Engagement** | DAU / registered | ≥30% |
| | Peak CCU | ≥40 sustained |
| | Median session length | ≥25 min |
| | Matches per active per day | ≥3 |
| **Retention** | D1 | ≥40% |
| | D7 | ≥35% |
| | D30 | ≥20% |
| **Marketplace** | Testers completing ≥1 transaction | ≥60% |
| | Cosmetic equipped after purchase | ≥90% |
| **Technical** | Crash-free sessions | ≥97% |
| | Login success | ≥99% |
| | p50 match fill — peak / off-peak | ≤60s / ≤180s |
| | Human fill ratio (peak) | ≥80% |
| | API 5xx | ≤0.3% |
| **Ops** | Unattended uptime | ≥99% |
| | Support response p50 | <24h |

**The one number above all others:** *installed → first match completed*. It is the compound measure of auth gate, launcher, matchmaking, fill time, onboarding, and stability — and it sets the entire top-of-funnel requirement in §3. If it lands at 45% instead of 70%, the visit target jumps by half. Instrument both ends before the ramp opens.

---

## 11. WHAT ACTUALLY GOT HARDER GOING 36 → 1,000

Honest accounting of where the v2 risk moved:

| Got easier | Got harder |
|---|---|
| Match fill (real population, no scheduling) | **Acquisition — 50k visits is the new project** |
| Retention measurement (real D1/D7/D30) | **Patch distribution → launcher now required** |
| Off-peak truth (measurable, not hidden) | **Support & moderation load → needs a mod team** |
| Marketplace signal (sustained volume) | **Cheating → EAC moves into beta scope** |
| Launch-day cold start (eliminated) | **Unattended reliability → alerting/runbooks are load-bearing** |
| | **Leak surface → more testers, same watermark discipline** |

Server capacity and cost were never on the "harder" side of this table.

---

## 12. RISK REGISTER (v2)

| Risk | Severity | Mitigation |
|---|---|---|
| **Cannot reach 50k visits** | **Highest** | Content pipeline promoted to critical path (W7); creator program; measure funnel early and adjust the visit target off real rates, not assumed ones |
| **Patch distribution kills retention** | **Medium** (downgraded on measurement) | Engine-native patch paks (§7.1); <10% gate; asset-hygiene discipline, since patch granularity is whole-package. At 2.76 GB a full redownload is minutes, not tens of minutes |
| **Release metadata lost** | High | Archive `Releases/<version>/` to R2 permanently and restore-test it. Losing it forces a full redownload on every tester |
| ~~**Shipping build far bigger than assumed**~~ | ~~High~~ | **CLOSED — inverted.** Measured 2.76 GB, not 15 GB. Distribution math and the launcher decision both restated on the measurement (§7) |
| **Founder number collision or cap breach** | Medium | Single atomic conditional counter op (§4.4 rules 4–5); numbers never recycled |
| **Install cliff** (unsigned / SmartScreen / AV) | High | Code signing in W2 gate; downloaded→installed measured explicitly |
| **Egress cost blowout** | High | R2 (§7) — the difference is ~$8,900 |
| **Unattended failure at 3am** | High | W6 alerting + 48h soak gate + zero-sessions page |
| **Cheating at scale** | High | EAC before ~250 testers; server-authoritative economy already proven |
| **3D website tanks conversion** | High | CI-enforced budget; CTA independent of WebGL; poster/video fallback (W4) |
| **Second identity system creeps in** | High | §4 is law. Any PR introducing a web-only password store is rejected |
| **Support/moderation overload** | High | Volunteer mod team before 300; KB; in-client F8 automation |
| **Off-peak queues stay empty** | Medium | Single region; bot fill; widening bands; **surface it as a finding, don't hide it** |
| **Leaked build/screenshots** | Medium | Watermark + gate + revocation + audit. NDA is deterrence, **not** security |
| **Legal exposure on virtual currency** | High | Counsel review before real-money enable; no cash-out; no paid randomized acquisition (already locked in the economy ADR) |
| **Minors + marketplace** | High | Age gate at application; regional thresholds; decide explicitly in W0 |
| **Portal deploy breaks the tentpole stack** | Medium | Separate `IronicsPortalStack` — W1 gate item |
| **Attribution lost** | Medium | UTM taxonomy locked before first post; server-side events |

---

## 13. LEGAL & COMPLIANCE (operator, counsel-reviewed)

- [ ] Terms of Service (entity, jurisdiction, termination, EULA scope)
- [ ] Privacy Policy (GDPR + CCPA; data inventory across AWS/GameLift Servers, PlayFab, PostHog, Sentry, Resend, Loops, Discord, R2; DSAR process; retention schedule)
- [ ] Beta Test Agreement / NDA — **versioned**, acceptance recorded per tester with version; confidentiality, streaming/screenshot clearance, feedback IP, revocation rights
- [ ] Community Guidelines + moderation/appeal policy (**required before a 1,000-member Discord**)
- [ ] Virtual Currency Terms (Watts/Volts: no cash-out, no monetary value, forfeiture on termination, price-change rights)
- [ ] Age gate policy + regional thresholds
- [ ] Cookie/consent posture (Cloudflare Analytics cookieless; PostHog needs a decision)
- [ ] Trademark clearance on IRONICS + logo before brand spend
- [ ] DPAs with each processor
- [ ] Security contact / vulnerability disclosure page

---

## 14. IMMEDIATE OWED DECISIONS (operator — these block W0)

1. **Domain** — exact domain, plus fallback.
2. **Company of record** — entity name/address/contact for ToS and Privacy.
3. **Age gate** — minimum age, and whether under-18 testers are permitted at all given the marketplace.
4. ~~**Founder cap**~~ — **RESOLVED: three-tier ladder, 100 / 300 / 1,000. See §4.4.** Remaining sub-decisions: tier *names* (proposal maps onto the SPARK→THUNDERBOLT rarity ladder) and whether any low numbers are reserved for internal (recommendation: no).
5. **Beta VC policy** — is marketplace currency granted free in beta (recommended: you need transaction volume to test the seam), and does it **carry to launch or reset**? Decide before the first purchase, because reversing it later is a trust problem.
6. ~~**Launcher build vs. buy**~~ — **RESOLVED by measurement: DEFERRED to RAMP-C review (§7.2).** Patch paks in scope now; launcher re-opens only on the three named triggers.
7. **Web repo** — confirm `Instawerx/Ironics_Web`, branch `main`.
8. **Playlist ↔ venue naming (W2A-2)** — twelve `DA_AFL_ShantyTown_*` playlists bind `L_ShantyTown` to experiences named for other venues (`3v3`→`Arena01_Extract3v3`, `5v5`/`8v8`→`Arena04_*`). If the experience is the *ruleset* and the map is the *venue*, this is correct reuse with misleading names. If the venue is meant to be encoded in the experience, six playlists are cross-wired. **Read vs. bug — operator ruling, not an agent call.**
9. **`L_Expanse`** — a Lyra stock map is still in the shipping set, bound by one playlist. Keep as a real venue, replace, or drop the playlist?
10. **Identity consolidation (§4.3)** — confirm the recommendation: keep PlayFab behind an `IIronicsIdentity` seam for the beta, revisit consolidation onto AWS after RAMP-E on evidence. A "consolidate now" call is legitimate but re-proves the entire A1.4 anti-spoof ladder and should be made deliberately, not by drift.

---

## 15. LEDGER INTEGRATION

Tier-2 SSOT. Phases and gates mirror into `Docs/LIVE_TRACKER.html` under a new pillar (`P-PLATFORM`) so the business lane obeys the same read-before-prove / write-verdict-after discipline as the game lane.

**Rule:** no phase is marked ✅ in the tracker without cited evidence, and no phase is re-proven once ✅.

**OWED — tracker annotation pass (accuracy debt, game lane):** the following rows still describe the retired PlayFab-matchmaking architecture and will mislead any future reader of the SSOT. They need a `🔄 SUPERSEDED — matchmaking migrated to GameLift Servers FlexMatch (standalone)` annotation with the migration evidence cited, following the same reversible-audit-trail pattern used for `AFL-1100` and `P1-RESCOPE`:
- `AFL-1100` — already annotated for the placement-mechanics correction, but still frames PlayFab as the matchmaker
- `AFL-1104` — "party rosters travel with PlayFab matchmaking ticket"
- `AFL-1107` — sprint-exit integration test written against a PlayFab ticket
- `AFL-1204` — "PlayFab Matchmaking ticket flow (client → PlayFab queue → …)"
- `S11` / `S12` sprint goal text
- `P3` exit-gate line: "PlayFab Matchmaking + Lambda + GameLift end-to-end"
- The EOS-AUTH-C1/C2 firewall note: "matchmaking stays PlayFab->GameLift" — the firewall itself still holds (EOS = auth/voice/friends/EAC only), only the left-hand side of that arrow changed

Do **not** delete these rows. Annotate them, exactly as the P1 re-scope was annotated, so the migration is a visible audit trail rather than a silent rewrite.

**v2.2 → v2.3 change record (measurement, not opinion):** W2A returned real numbers and they overturned three positions held in v2.2. (1) **Build size: 2,757 MB compressed, not 15 GB** — off by ~5×. All distribution arithmetic restated (§7). (2) **The three predicted structural suspects were wrong** — combined ~4.6% of the build; the real weight is five marketplace environment packs (46%) and MetaHuman/NNEDenoiser editor ML models (14.7%). (3) **The launcher is deferred again (§7.2)** — it was put on the critical path *because of* the 15 GB figure, and that figure was an assumption. Engine-native patch paks stay in scope. Also recorded: the AlwaysCook map-directory rule is the single real force-cook leak, and doctrine **X33** (Reference Viewer cannot see `PrimaryAssetId` bindings).

**v2.1 → v2.2 change record:** (1) Founder resolved to a **three-tier nested ladder at 100 / 300 / 1,000** with derived tiers, atomic server-side issuance, and a hard cap (§4.4). (2) Custom chunked delta patching **replaced** by UE's native `-createreleaseversion` / `-generatepatch` system (§7.1) — smaller launcher, engine-supported path, with whole-package patch granularity named as the real size lever. (3) New gated **Phase W2A — Build Audit & Prune**: shipping size is currently an assumption and every distribution number depends on it, so W2A blocks W2.

**v2 → v2.1 change record:** service attribution corrected throughout. Matchmaking and hosting are **Amazon GameLift Servers FlexMatch** (standalone integration; SNS notifier → own placement Lambda → `StartGameSessionPlacement` → queue → `/claim-session`), not PlayFab. PlayFab's real and proven scope is narrowed in writing to **identity anchor + catalog/inventory/currency**, and placed behind an `IIronicsIdentity` seam so the consolidation question (§4.3) can be decided on evidence later. Tracker annotation debt named in the OWED block above.

**v1 → v2 change record (audit trail):** v1's scheduled-ops-window model and 36-concurrent cap are retired by explicit Studio Lead decision. Rationale: windows fabricate concurrency, make D1/D7/D30 undefined, and hide off-peak matchmaking failure — the exact condition launch day presents. 36 concurrent is retained not as a cap but as the **expected organic peak** of a 1,000-registered population, and therefore the capacity floor. If v2 is ever reverted, v1's cohort table (BETA-00 through BETA-04, 100 pool / 36 concurrent) is restored verbatim.
