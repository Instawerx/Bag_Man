# IRONICS site overhaul + C12 DAO / Creator economy / Sponsors — PROPOSAL

**Status:** PROPOSED 2026-09-07. Nothing shipped this phase — this is the design + proposal the operator asked for (mockup-first, propose -> approve -> implement).  
**Method:** seven research lenses over the live site + the mandate + the content truth pass + the design tokens + the decision log, an architect synthesis, and three reviewers (truth, legal, brand/scope) all returning **sound-with-fixes**. The two C12 documents (operating agreement + C12USD whitepaper) and the funding gate were read after the scope ran and are folded into section A, which governs.  
**Companion mockups:** a /Design canvas of the public screens (built alongside this doc); the admin wallet/issuance screens extend the existing admin console canvas.

## A. Corrections and grounding before this proposal is acted on

The scope ran before I could read the two C12 documents, and its three reviews returned **sound-with-fixes**
with two legal blockers. This section is the authoritative layer: the real C12 grounding from the documents,
plus every review fix folded in. Where it differs from the sections below, this governs.

### A1. What C12 actually is (from the documents, not the message)
C12 is **Carnival 12 AI DAO**, and its flagship is not a game — it is **C12USD**: an omnichain USD-pegged
stablecoin (LayerZero V2, deployed and verified on BSC and Polygon), flash-mint DeFi, a professional trading
exchange, and "robotic banking" for autonomous machines. That is the operator's Robotics-plus-Hyperledgers
focus. IRONICS and three more games are the **gaming intersection**, alongside entertainment and social
equality. The two lanes are therefore not "game vs company brochure" — they are a **game** on one side and a
**regulated-finance venture** on the other, which makes the separation load-bearing, not cosmetic.

### A2. The DAO section summarizes and links out; it does not re-host regulated finance
The public C12 section on the IRONICS site presents C12 at **company/DAO altitude** — mission, the three
pillars named, the governance model, a phase roadmap, and how to engage — and **links to c12usd.com** for the
stablecoin/exchange/bank detail. It does not reproduce reserve figures, revenue projections, APY, "money
transmission" claims, or the word "bank" as a live claim. Contract addresses, chains, and the open-source
repo are public and may be referenced. This keeps the finance venture's regulated surface on its own site.

### A3. The stablecoin, exchange and bank are GATED milestones (funding + legal)
Per the operator: these go live at a **$250,000 raise**, with **$200,000 funding the legal equity asset**
that permits lawful stablecoin issuance and bank operation. So the C12 roadmap shows them as **pending the
legal foundation**, never as live. Deployed pilot contracts are a technical fact, not "the bank is open."
**The $250,000 / $200,000 figures are internal grounding, not public copy.** I will not draft public
fundraising, "invest," or raise-solicitation copy; a raise is a securities offering and that copy is
counsel-authored.

### A4. Blocker fixes — admin currency issuance (A-WALLET, hardened)
The ruling lets admins issue Watts and Volts. The reviews are right that de-novo issuance is not a goodwill
credit and cannot run under the same guards. Adopted:
- **Issuance requires four-eyes ALWAYS — no self-approve at any amount.** It therefore ships only after a
  **second active owner** exists. Add the second owner, or issuance waits.
- **Watts (earned currency) issuance first; Volts (USD-pegged) issuance is gated on the L1 Virtual Currency
  Terms being in force**, mirroring the refund/void gate. Until L1 is in force the console shows Volt
  issuance as read-only "refer to counsel." Every issuance books a **house-liability journal row** (hard
  requirement, not optional).
- **Zero portal holders of the game-server earn key.** Move the claim-mint path onto the admin key with a
  required actor; the grants test asserts *no* portal function holds `bagman/earn/hmac`.
- **Move `register-economy` (the PlayFab title secret) behind the tentpole with the issuance work, not
  later.** Issuance and catalog control are the same threat surface; until moved, catalog editing stays
  disabled and the path is logged in the risk register.

### A5. Creator economy — separation of duties, closed-loop payment, scarcity reconciliation
- **Different approvers across the chain:** the actor who grants a creator cannot approve that creator's
  first integrated mint. A test asserts `grantedBy != integrationApprover`. Otherwise one person could mint
  supply through a captive creator account with a single pair of hands.
- **Creator payment is closed-loop** (creators earn non-cashable Watts/Volts), which honors the standing
  "no cash-out, ever" invariant. A real-money creator payout is a separate, counsel-gated program.
- **Scarcity reconciliation is an operator decision:** the in-force pricing SSOT retired scarcity "with no
  carve-outs," and W-CREATOR reintroduces 1-of-1. One of them has to give; flagged in decisions.

### A6. DAO ownership interest — counsel gates its EXISTENCE, not just token specifics
A public "DAO ownership interest" capture, sitting beside benefits language and a real ERC-20, can itself be
general solicitation regardless of "grants no entitlement" framing. Adopted: **strip all value, upside, and
economic-return language** from any ownership-adjacent surface; reduce the DAO capture to a plain
**interest-and-updates list with zero forward-looking value language** until counsel rules on whether the
capture may exist at all. This is a hard gate on the feature, not a copy note.

### A7. Sponsors — build-status honesty and minor-safety
- **Add a build-status axis (Live / In the build / Roadmap) to the offerings matrix.** In-arena placement
  during a match has a live substrate (ARCANEON matchplay is live); the Super Lobby, collab spaces, and
  player-IP routing do **not** exist yet and must be labeled Roadmap, not described in the present tense.
- **Player-IP / aligned-interest routing to known minors is default-excluded until counsel rules.** The beta
  is 13+ and global; behavioral targeting of 13–17-year-olds triggers minor-privacy and ad regimes. "Player
  IP" is undefined on disk — no routing copy or build until it is defined, and IP-address routing is PII
  processing needing lawful basis and DPAs. All reach and lead-time figures stay bracketed until real.

### A8. Truth precision — the beta is live; its ECONOMY CONTENT is the placeholder
W-BETA does not mean "the beta is fake." The playable Windows beta is genuinely live and operator-proven.
What is **placeholder capability** is the **economy/store content and item set** it shows — those demonstrate
the engine, not the shipped Alpha product. The label goes on the economy content, never on the program.

### A9. Brand — the lane is carried by structure and words, not by color
Green and violet already carry status meaning (live; planned/sponsor). Do **not** repurpose them as a
game-vs-C12 lane axis, and never let the real C12 mission inherit the "planned" violet. The lane is carried
by grouping, the hairline divider, and the literal section label. And the **375px wrapped header state is
designed and proven in the mockup** — a flex-wrap hairline cannot be the sole thing holding the lanes apart.

### A10. Sequencing — the fast, fully-unblocked win goes first
The **truth-pass copy fixes** (removing the ~177 false/stale claims: the Simularent twin fiction, the false
house counts, the dead CTAs, the stale REV, the false press kit) need no DAO docs, no lane debate, and no new
component. They ship first, on the already-green CI, and clear most of the false claims at their source.
C12, Creators, Sponsors, Roles, the wallet/issuance epic, and the submission pipeline are then **separately
approved sub-programs, each behind its own mockup-first gate** — not one big parallel push. Nothing in this
proposal ships without per-screen approval.

## B. Executive summary (architect)

One overhaul, two clean lanes, nothing shipped this phase (M-METHOD: design + proposal only). The site today is a single undifferentiated GAME-flavored nav (Build·Houses·Market·Founders·Roadmap·Press·Creators·Portal, data.ts:161-170) carrying ~177 false/stale claims catalogued in the content-truth pass across 17 pages. The mandate (W-DAO/W-LANES/W-BETA/W-ROLES/W-CREATOR/W-SPONSOR/A-WALLET, decision-log:247-254) reshapes it into a GAME lane (IRONICS: the game, its economy, releases) and a C12 lane (Carnival 12 Ai DAO LLC: the company/DAO, creators, sponsors), separated by a two-zone header rather than a new switcher component (none exists in the shipped system). The overhaul REMOVES the false claims at their source rather than layering new copy over them: HOUSES (90% placeholder, 2-of-18 real) is deleted and its nav slot reassigned to a new /c12 DAO hub; the false streamer /creators page is replaced by the Alpha creator-economy front door; the fabricated Simularent 'twin' story is stripped everywhere before a Sponsors/Partners offerings page can exist. Beta Land is relabeled placeholder capability; Alpha (creator-forward, open-source-inspired, DAO-managed economy) is presented as the thesis. A multi-select /join hub gives the three public roles (DAO ownership interest, Creator, Tester) one honest door. On the admin side, A-WALLET green-lights the four designed-not-wired economy screens by reifying 'issue Watts/Volts' as a guarded maker-checker adjustment (never a free mint), built on the ruled PLAT Workstream E dependency graph. Every DAO-, creator-, sponsor-, ownership-, or money-specific fact that is not on disk ships as a bracketed [placeholder] or is gated behind an operator/counsel decision — the second DAO marketing doc is still unread, and the one doc received (the Operating Agreement) is securities-sensitive and carries a non-negotiable no-publish guardrail on member names, allocations, treasury and any ownership/token offer.

## C. Content lanes

Two lanes held apart by structure, color, route grouping, and separate truth-owners — not by a mode switch. (1) HEADER: one shipped header, nav partitioned into a GAME zone [Build·Market·Founders·Roadmap·Press] and a C12 zone [C12·Creators·Sponsors] split by the existing 1px hair divider (border-hair rgba(255,255,255,.11)), each group carrying a t-data micro-label (GAME / C12). No new interaction pattern — the tokens sheet confirms no dropdown/menu/tab/toggle/segmented-control exists outside /admin, so a switcher would be net-new; the grouped header is built from existing nav links + a divider and keeps every section one click away. (2) COLOR: the shipped system already encodes the lanes — green #5CFF57/#86FF82 = live/shipped GAME facts; violet #7A2BFF/#B57CFF = planned/company/DAO/sponsor. Game pages keep green-text eyebrows; every C12/DAO/creator/sponsor surface uses a violet eyebrow. Green is reserved for watched/demonstrated facts only; violet + an explicit 'Alpha' / 'Placeholder' label marks everything unbuilt (W2/W-BETA). (3) ROUTES: GAME lane = /, /build, /market, /founders, /beta, /portal, /roadmap, /press; C12 lane = /c12 (+ DAO roadmap), /creators (Alpha economy), /sponsors (or /partners). Shared DOORS that belong to neither zone = /join, /signin, /portal, /admin. (4) ROADMAP DISCIPLINE: the game/platform roadmap stays on /roadmap; the DAO roadmap is a SEPARATE surface on /c12 — never merged — so the lanes do not intertwine. (5) NO BLEED: DAO/creator-economy aspiration must not appear on game pages as if shipped (the game home gets at most ONE clean C12 cross-link band, no DAO features), and game facts are not diluted by DAO framing. The game lane inherits the truth-pass rulings W1-W32; the C12 lane carries its own bracketed-placeholder discipline and its own legal gates (L1 VC Terms, DAO securities, creator/UGC + marketplace terms). Footer mirrors the lanes: GAME / C12·DAO / Legal, replacing today's mislabeled Product/Company split.

## D. New navigation

- IRONICS lockup -> / (GAME-lane home)
- GAME zone: Build
- GAME zone: Market
- GAME zone: Founders
- GAME zone: Roadmap
- GAME zone: Press
- -- hair divider --
- C12 zone: C12 (DAO hub, replaces Houses slot)
- C12 zone: Creators (Alpha creator economy)
- C12 zone: Sponsors (or Partners; operator to confirm name)
- Right side CTA: Join -> /join (replaces 'Request access', repoints from /beta)
- Session door (outside NAV array): Sign in / Portal / Console[admin-only] / Sign out
- Net section count stays 8: Houses->C12 (slot reassigned), Portal dropped from NAV array (already renders in the session door, kills the data.ts:169 duplicate), Creators re-homed to C12 zone, Sponsors added

## E. Route map (every current + new route)

| Route | Action | Lane | Mockup | Summary |
|---|---|---|---|---|
| `/` | revise | game | yes | Stays the GAME-lane home. Remove the false Simularent 'twin/network sync' section and the '18 characters / ten colour families' carriers; add the honest 'closed beta live on Windows -> Portal' line; add exactly ONE C12 cross-link band ('C12 - the DAO behind IRONICS') to introduce the company lane without intertwining. Separate the benign existing 'you build it, you own it' copy from the legally-loaded W-CREATOR property claim. |
| `/build` | revise | game | yes | Corrected six-axis creator copy (finish, mask, edge, stickers, accessories, emblems - all live) from a corrected .dc.html canvas; absorbs the honest 'ruled six' character reference (IRONICS, ARIA, SCARLETT, MAKHIAVELLI, BIG SIXX, FANATICS) folded in from the retired /houses page. Data-drive character/finish names from catalog-export.json, never 18/28/colour-families. |
| `/market` | revise | game | yes | Add the ruled 'Preview - items are not sold on ironics.org / prices not final' + 'Beta Lands - placeholder capability' label (W10/W-BETA); replace the hardcoded 4,200V/180W wallet fixture and 12 fixture cards with a catalog-driven export; add a forward panel pointing to the Alpha creator marketplace (links into /creators). Keep rarity as a visual label only - no live 1-of-1/mint-count marketing until the scarcity SSOT is reconciled. |
| `/founders` | revise | game | - | Copy-only truth-pass fixes: 4-digit numbers everywhere, per-tier slot lines, corrected notice box, sold-out CTA, no rewards printed until ruled. No lane change, no layout revamp. |
| `/roadmap` | revise | game | - | Stays the GAME/PLATFORM roadmap; adopt the ruled 37-row honest set tagged [GAME]/[PLATFORM]/[RELEASE]/[VR], add the releases strip + 'Updated <date>' stamp (roadmap model B/D via roadmap.json), fix the false REV/VR/console rows. The DAO roadmap does NOT merge here - it lives on /c12 to hold the lanes apart. |
| `/press` | revise | game | - | Fix the false 'press kit' (no archive, dead CTA, text stand-in); remove the false Simularent 'twin' boilerplate. Recommend it stays GAME-lane (IRONICS press) pending a DAO/company-wide PR scope decision. |
| `/portal` | revise | shared | - | Cross-lane session door. Drop Portal from the NAV array (it already renders in SessionNav - kills the duplicate). Apply ruled copy fixes: remove cohort row, FOUNDER I header, beta-currency line; fix DownloadCard install/link/confidentiality copy. |
| `/signin` | keep | shared | - | Cross-lane door, unchanged. Reused as the sign-in step the /join hub requires (the record-of-truth apply is session-gated). |
| `/beta` | revise | game | - | Stays the Tester deep-flow (age gate, email, platform); becomes the Tester target of /join. Fix stale cohort/waitlist/weekly-review copy (auto-approval is the only path), trim platforms to Windows, add size/unsigned-installer note, use 'Beta Lands - Windows'. |
| `/houses` | remove | game | - | Retire the page (90% placeholder, 2-of-18 real, mislabeled SKUs, false counts). 301 -> /build (the roster content's real successor). Nav slot goes to C12; HouseTile.tsx + HOUSES data become dead. Alternative operator option: keep an out-of-nav /characters page. |
| `/creators` | replace | dao | yes | Delete the 100%-false streamer program (dead href='#', all four fabricated perks). Rebuild as the Alpha creator-ECONOMY front door: what you make (Weapons/Maps/Characters/Accessories), set scarcity, own it, submit->vote->DAO approval/integration, AAA bar. Forward/Alpha-labeled (program not live, opens at 300 approved testers). Entry via /join?role=creator. |
| `/c12` | add | dao | yes | New DAO/company hub replacing the Houses slot. Mission (Robotics + Hyperledgers, intersecting gaming/entertainment/social equality), portfolio (1 AAA in beta + 3 [placeholder] titles), what-the-DAO-does, why-participate, governance MODEL (high-level, from the Operating Agreement), get-involved band. Every feature/benefit/token/ownership specific is a bracketed [placeholder] pending the second DAO doc + counsel. |
| `/c12/roadmap` | add | dao | yes | The DAO roadmap - a section on /c12 (or its own route). Reuses the proven /roadmap presentation with STATUS as the primary axis and relative/'[TBD]' when-labels so it works with no dates; 'Updated <date>' stamp. Sourced from roadmap.json under a c12 namespace with the same CI source-gate. Kept SEPARATE from the game /roadmap. |
| `/sponsors` | add | dao | yes | New B2B sponsor & brand offerings page (recommend route name /partners). Enumerate offerings, varied ways sponsors reach players, and the <72h capability. Company/DAO lane, violet. HARD PREREQUISITE: the W11 Simularent twin-promise removal must land first. All fees/reach/lead-times are [placeholder]; mini-games labeled ROADMAP; sponsors named only per W11 (partner line only until real). |
| `/join` | add | shared | yes | New multi-select roles/apply hub (operator to confirm /join vs /apply vs /roles). Sign-in-gated. One form, three checkboxes (DAO ownership interest / Creator / Tester), per-role field reveal, one submit, per-role fan-out. Tester -> live auto-approve; Creator/DAO -> review queue (capture-only today). Shared door, not inside either zone. DAO card is an expression-of-interest CAPTURE, counsel-gated, never an offer. |
| `/legal + /legal/[slug]` | revise | shared | - | Footer-only, cross-lane. Apply the ruled v0.3 counsel bundle (terms/privacy/beta/community/virtual-currency); hide Virtual Currency until in force; add a Security disclosure row. FLAG new counsel surfaces: DAO participation terms (securities), a Creator/UGC agreement, marketplace/secondary-market terms, minors-and-marketplace posture, and Privacy v0.3 for sponsor player-data routing - do not invent, counsel-owned. |
| `/admin` | revise | shared | yes | Out of public IA (noindex, notFound-gated), migrating off ironics.org to apps/crm on admin.ironics.org (D7). A-WALLET green-lights wiring the four designed-not-wired economy screens + a new Wallet inspector + creator/DAO review queues. Mockups covered under the admin canvas (12 artboards) + the new wallet/queue artboards below. |

## F. Page plans

### / (home)

**Sections:** Creator hero (game-first, 'you build it, you own it' - benign account-ownership meaning, separated from the Alpha property claim), The loop (build -> match -> economy), Corrected character/finish preview (ruled six, catalog-driven), Closed-beta-live band ('Windows closed beta is out - download from the Portal'), Founders band, ONE C12 cross-link band ('C12 - the DAO behind IRONICS', links to /c12) - the only company-lane element on the game home

**Copy direction:** Game-first, watched-facts-only, green lane. Introduce the company lane with a single clean pointer, never DAO features. Do not foreshadow Alpha so hard it reads as shipped.

**Removes these false/stale claims:**
- 'Every build has a twin' + 'the twin their network keeps in sync across every match' (Simularent twin system - does not exist, W11)
- 'Twin & network by SIMULARENT' partner line as a product promise
- '18 House characters' + 'ten colour families' counts
- og:image:alt twin reference

**Placeholders (operator/doc/counsel to fill):**
- C12 cross-link band copy (message-level only until the second DAO doc lands)

### /c12 (DAO hub - replaces Houses)

**Sections:** Hero (violet eyebrow 'C12 AI DAO'; H1 tagline [placeholder/default 'Robotics, ledgers, and the games between']; lead: Robotics + [Hyperledgers], intersecting gaming/entertainment/social equality), What C12 is - four pillar cards (Robotics / [Hyperledgers] / Gaming & Entertainment / Social equality), bodies bracketed, The portfolio - IRONICS (LIVE-BETA badge, links to game lane) + [Game 2]/[Game 3]/[Game 4] placeholder cards, What the DAO does - features grid (Alpha creator economy, submit->vote->integrate, own-what-you-make) + [placeholder] cards; cross-link /creators, Why participate - benefits grid (savings passed to players, creators keep upside) + [placeholder]; ownership benefits deliberately vague behind the counsel gate, Governance MODEL (high-level, safe: proposal by holders, on-chain vote, quorum, approval - NO names/allocations/treasury) with a standing 'not an offer / not financial advice' disclaimer, DAO roadmap section (status-first, date-agnostic), Get involved band -> /join (three role chips)

**Copy direction:** Company/R&D altitude, violet lane, no game mechanics (link out to the game instead). State only mission + governance MODEL; every ownership/token/allocation/treasury/vesting specific is a bracketed placeholder for operator + attorney. Never claim the IRONICS game economy runs on a hyperledger (game-side NOT-PUBLIC gate).

**Removes these false/stale claims:**
- (net-new page - carries no legacy claims; the /houses claims it inherits the slot from are removed by /houses deletion)

**Placeholders (operator/doc/counsel to fill):**
- Hero tagline/positioning line
- 'Hyperledgers' vs 'distributed ledgers' wording
- the three other game titles/genres/status
- every DAO feature body
- every DAO benefit body
- the DAO roadmap milestones + all dates
- the securities/ownership disclaimer wording (counsel)
- entity relationship: Carnival 12 Ai DAO LLC vs C12 AI Gaming vs 'C12 AI DAO' branding

### /creators (Alpha creator economy - replaces streamer page)

**Sections:** Hero (violet eyebrow, 'Alpha - opening after the beta'), What a creator makes (Weapons, Maps, Characters, Accessories for C12 games), How it works (set scarcity 1-of-1 or many; price/limit as you please), Ownership (once it enters the economy it is a normal econ item - the new owner's property to buy/sell/trade), The pipeline (submit planned content -> community/DAO voting -> DAO approval & integration; AAA quality gate), Scarcity + ownership explainer module (shared with a possible /alpha page), Real entry -> /join?role=creator (never a dead href)

**Copy direction:** Forward/vision copy, explicitly Alpha and not-in-the-beta. Describe mechanics as the thesis, not a live capability. No live buy/sell/trade/property/payout claim, no revenue/payout promise, until the legal gates are in force. Do not resurrect the false perks.

**Removes these false/stale claims:**
- 'reviewed weekly alongside beta cohorts' (auto-approval; cohorts dead)
- CTA 'Apply as creator ->' href='#' (dead link)
- CREATOR_PERKS: 'the build a week before the cohort', 'a code drop tracked so you can see what converted', 'pre-embargo art cleared for thumbnails', 'a named contact answers inside a working day' (all fabricated, no backend)
- /market 'Parts, not packs' when repeated here

**Placeholders (operator/doc/counsel to fill):**
- what approved creators actually get when the program opens
- creator revenue/payout model (blocked on the payment-vs-cash-out decision)
- the voting model specifics (who votes, quorum) pending DAO docs

### /sponsors (or /partners)

**Sections:** Hero (positioning + the <72h speed hook as headline), Varied reach grid (Super Lobby, shared collab spaces, mini-game/tournament sponsoring, in-match/venue placement, routed & localized reach, custom feature agreements), Offerings matrix (offering / reach / example / rough lead time), How fast (concept -> live, often <72h, framed 'often' not an SLA), Mini-games & tournaments detail (ROADMAP-labeled), Routing & localization explainer (player-IP, aligned-interest, localized), Illustrative example (clearly fixture-labeled), CTA 'Talk to us' inquiry (not self-serve checkout)

**Copy direction:** B2B/commercial voice aimed at brand teams, violet lane, kept out of the game nav's game zone. Offerings presented as 'available for agreement', not live inventory. Mini-games labeled PLANNED. <72h framed as a claimed capability with 'often'. Sponsors named only per W11.

**Removes these false/stale claims:**
- 'Every build has a twin' / 'their network keeps in sync' / 'Twin-synced livery' (must be stripped from home/footer/press/market first - hard prerequisite)

**Placeholders (operator/doc/counsel to fill):**
- all fees / rate card / packages
- every reach/audience figure
- every per-offering lead time
- which mini-games are greenlit vs 'and more'
- sponsor marks/logos (none on disk)
- real <72h example
- player-IP routing exact definition + data/consent implications

### /join (roles hub)

**Sections:** Sign-in gate (Epic or email magic link), 'What are you here for?' - three multi-select role cards (DAO Ownership Interest [violet/C12], Creator [violet/C12], Tester [green/game]), Shared identity/contact block, Per-role field reveal (Tester: platform/region/hardware/hours; Creator: portfolio/content types/scarcity intent/AAA attestation; DAO: [placeholder fields + expression-of-interest consent]), Per-role honest status (Tester = instant; Creator = 'application on file, opens in Alpha'; DAO = 'interest registered, not an offer'), One Submit -> fan-out

**Copy direction:** One coherent form, per-lane truthful copy. No 'we read every application/cohorts/waitlisted/weekly review' for the Tester lane (auto-approve). Creator/DAO ARE reviewed so 'reviewed by the DAO' is true for those lanes only. DAO card carries the standing 'not an offer of securities / not financial advice' disclaimer.

**Removes these false/stale claims:**
- the dead /creators 'Apply as creator' CTA (re-homed here)
- any cohort/waitlist/weekly-review language carried into a role

**Placeholders (operator/doc/counsel to fill):**
- DAO ownership-interest field set (counsel-gated)
- per-role eligibility thresholds (13+ Tester; likely 18+/KYC/tax for Creator/DAO - operator/legal)

### /market

**Sections:** Beta-placeholder label banner ('Preview - items are not sold on ironics.org'), Catalog-driven item grid (fixtures replaced/labeled), Alpha creator-marketplace forward panel (links to /creators)

**Copy direction:** Stays game lane. Frame today's items as studio-made placeholder capability; frame the real economy as the Alpha creator-forward, DAO-managed system. Rarity is a visual label only.

**Removes these false/stale claims:**
- hardcoded wallet fixture (4,200 Volts / 180 Watts)
- 12 fixture item names/prices
- 'Simularent Livery - Sponsor livery - Twin-synced' card

**Placeholders (operator/doc/counsel to fill):**
- fresh catalog export values until wired

### /beta

**Sections:** Age gate (13+), Email capture, Platform (Windows only), Requirements/size/unsigned-installer note, 'Beta Lands - Windows' framing

**Copy direction:** Straightforward Tester intake, the target of /join?role=tester. State auto-approval honestly; no cohort/waitlist/review process copy.

**Removes these false/stale claims:**
- cohort/waitlist/weekly-review process copy
- multi-platform list beyond Windows

### /roadmap

**Sections:** Game/platform rows (ruled 37-row honest set), Releases strip (0.1.0 -> 0.1.3 from manifests), 'Updated <date>' stamp + 'dates move, we say when they do' line

**Copy direction:** Status-first, sourced rows, no dated NEXT/PLANNED without a source (CI-gated). DAO roadmap explicitly lives elsewhere (/c12).

**Removes these false/stale claims:**
- dated VR row (undate)
- '24/7 autoscaled fleet' server claim
- false console/mobile 'cohort' dating
- 'collapsing zone' as live

**Placeholders (operator/doc/counsel to fill):**
- 2027 Launch date pending operator re-affirmation

### /press

**Sections:** Real press contact/boilerplate, Honest asset availability (no fake kit until one exists)

**Copy direction:** Game-lane IRONICS press; remove the fabricated kit and twin boilerplate.

**Removes these false/stale claims:**
- 'press kit' archive + dead CTA
- 'Simularent, digital twin and network experts, are an IRONICS sponsor' boilerplate

**Placeholders (operator/doc/counsel to fill):**
- press contact mailbox (must be provisioned before it promises a response)

### /portal

**Sections:** Session/status panel, DownloadCard (install/link/confidentiality copy fixed)

**Copy direction:** Cross-lane door; drop from NAV array; apply ruled copy fixes.

**Removes these false/stale claims:**
- cohort row
- FOUNDER I header assumption
- beta-currency line

### /houses (removal)

**Sections:** (page retired)

**Copy direction:** Delete; 301 -> /build; fold the corrected 'ruled six' character reference into /build. Nav slot -> C12.

**Removes these false/stale claims:**
- '18 House characters' (x4)
- 'ten colour families'
- 'Teal.Rift / House IRONICS' (retired FINISH SKU mislabeled as a House)
- 'the remaining sixteen...'
- 'signed-off renders' for placeholder tiles

### /legal/*

**Sections:** Terms, Privacy (v0.3), Beta Agreement (cohort text fixed), Community Guidelines, Virtual Currency (hidden until in force), Security disclosure (new row)

**Copy direction:** Apply the ruled v0.3 bundle; flag - do not draft - the new DAO/creator/marketplace surfaces for counsel.

**Removes these false/stale claims:**
- cohort/review language embedded in the four in-force legal docs

**Placeholders (operator/doc/counsel to fill):**
- DAO participation terms (securities, counsel)
- Creator/UGC agreement (IP license, revenue share, payout + tax, termination)
- Marketplace/secondary-market terms
- minors-and-marketplace posture
- Privacy v0.3 sponsor-routing + chat/PlayFab/GameLift/IP disclosures

## G. Sponsor & brand offerings

_Per section A7: a build-status column (Live / In the build / Roadmap) is added; only in-arena placement has a live substrate today; every reach and lead-time figure is a bracketed placeholder until real; minor-targeting is default-excluded pending counsel._

| Offering | Reach | Example | Lead time |
|---|---|---|---|
| Super Lobby sponsorship | Every player passing through the pre-match Super Lobby | [Branded lobby takeover / hero panel] | [placeholder] |
| Shared sponsor collab space | Co-branded social/hangout zone (two brands share a space) | [Two-brand collab room] | [placeholder] |
| Mini-Game sponsorship (ROADMAP - none shipped) | Players of that mini-game title | 'Drone Racing presented by [Brand]' (mini-games are roadmap, not live: Drone Racing, Drone leagues/tourneys, Shooting, Cart Racing, Space Invaders, + more) | [placeholder] |
| Tournament / league sponsorship (ROADMAP) | Competitors + spectators of a tournament or league | '[Brand] Drone League' | [placeholder] |
| In-match / venue placement | In-arena eyeballs during a match | [Board / skin / pad decal] | [placeholder] |
| Player-IP + aligned-interest routing (incl. localization) | Matched / localized player segments (right brand, right player, right language) | [Region-specific brand shown to aligned players] | [placeholder] |
| Custom feature agreement | Bespoke - any feature we can integrate | [Sponsored game feature]; headline capability: often live in under 72 hours from concept ('often', not a contractual SLA) | often < 72h from concept [capability claim, per-offering real timing is placeholder] |

## H. Roles + submission pipeline

ONE coherent /join surface (operator to confirm name vs /apply, /roles) with a role multi-select, sign-in-gated because the record-of-truth apply is session-gated today. The three PUBLIC applicant roles are entitlement/interest AXES on the account, categorically SEPARATE from the console viewer/operator/owner RBAC so the two never collide. TESTER reuses the existing machinery unchanged (account.status lifecycle + founder entitlement, auto-approved in-request). CREATOR becomes a `creator` entitlement (GTM rule 10 - a separate axis, not a founder tier), written on admin/DAO approval with cohort in its meta. DAO OWNERSHIP INTEREST is an expression-of-interest CAPTURE routed to a DAO/business queue that grants NO entitlement automatically and carries a standing 'not an offer of securities / not financial advice' disclaimer - because C12 is Carnival 12 Ai DAO LLC with a CARNIVAL ERC-20 token and an Aragon-governed treasury, its ownership/token specifics are securities-sensitive and counsel-gated; the role captures interest, never solicits.

DATA: keep ONE IronicsApplications row per account (preserve the single-slot claim invariant); add roles[] (required, >=1) + per-role payload maps (tester/creator/dao); re-applying to ADD a role is an UPDATE, never a second row. Because tester auto-approves but creator and dao need review - and one person can pick creator AND dao together - a single scalar status cannot index both pendings, so fan out on submit into a new IronicsRoleReviews queue {reviewId=`${role}#${accountId}`, role, accountId, state PENDING|APPROVED|REJECTED, cohort?, decidedBy?, reason?} with a state-created-index GSI (PK `${role}#${state}`, SK createdAt) - the admin walks 'creator#PENDING' and 'dao#PENDING' oldest-first exactly like the proven listApplicationsForApproval walks status-created-index. Reuse the entire /v1/beta/apply spine (requireSession, age gate, terms, Turnstile, idempotency, createApplication, one-per-account claim) and extend it to /v1/apply with per-role fan-out: run the tester auto-approve ONLY if 'tester' in roles and the age gate passes; write PENDING review rows for creator/dao. On creator approval write IronicsEntitlements {entitlementId:'creator', meta:{cohort, grantedBy, quality:'attested'}} (reusing the founder-entitlement pattern), finally giving the read-only account.cohort field (rendered 'Not assigned' forever today) a writer.

ADMIN: add permissions applications:review, cohort:assign, creator:grant, submissions:review to the RBAC-as-data map (a one-map edit, not a migration); routes GET /v1/admin/applications?role=&state=, POST /v1/admin/applications/review (creator-approve grants creator+cohort; dao-approve marks qualified + routes; both audited + typed-confirm), POST /v1/admin/accounts/cohort. Screens: an Applications review queue (Creator / DAO tabs, per-application drawer showing roles[] + each role's answers, approve/reject + cohort assignment), Cohort management, Creator roster.

SUBMISSION PIPELINE (Alpha, designed-not-wired): gated behind the creator entitlement. New IronicsSubmissions table {submissionId, accountId, title, contentType (weapon|map|character|accessory), scarcity (1-of-1 | edition N | unlimited), assetRefs, state DRAFT->SUBMITTED->IN_REVIEW->IN_VOTE->APPROVED/REJECTED->INTEGRATED} with by-creator + state-created-index GSIs. Creator routes (POST /v1/creator/submissions, GET .../mine) and admin/DAO routes (GET /v1/admin/submissions?state=, POST .../decision). The AAA quality gate anchors on the existing GTM rule 'creator clearance is an audited admin action, watermark stays on'. The IN_VOTE transition maps to the Operating Agreement's governance MODEL (a Member holding >=1% may submit a Proposal; 7-day Aragon vote; 10% quorum; >=51% carries; Manager disburses per approved Proposals) - but the exact voting eligibility/weighting/on-chain-vs-internal execution is a DAO-docs gap and stays bracketed. On INTEGRATED, the item enters the economy as a normal econ item per W-CREATOR - reusing the PROVEN wallet/catalog/entitlement/lease spine, where resale = a new ownership-transfers transaction type on the existing seam (the lease market already added ownership-boundary transaction types), not a parallel system. The supply-cap mint re-activates the already-built-but-inert backend MintCap primitive; it is booked through the A-WALLET money journal so creator mints are audited, named, and journaled like every other money action.

Per-role eligibility gating: Tester 13+ (existing gate); Creator and DAO almost certainly 18+ with jurisdiction/KYC/tax handling (creators receive economy value; DAO ownership touches securities) - the single form enforces the strictest gate among selected roles and reveals the extra onboarding only for those lanes. Thresholds are operator/legal decisions; the design leaves a slot, it does not invent a rule.

## I. Admin wallet + Watts/Volts issuance — build spec

_Per section A4: issuance is four-eyes ALWAYS (ships after a second owner), Watts first / Volts gated on L1, zero portal holders of the earn key, register-economy moved with it, every issuance a house-liability journal row._

A-WALLET reified as a guarded positive maker-checker adjustment, never a free mint. The doctrine that 'an approver who can type an amount is a currency printer with a UI' (approve-claim.ts:1-11) is preserved: the console copy says 'Issue Watts/Volts', the code path is the same propose->typed-confirm->journal->cap adjustment as any credit/debit, with a new reason=issue_grant. Built ON the ruled IRONICS_PLATFORM_ENGINEERING_PLAN Workstream E (PLAT-E01..E21) and the admin scope (D1-D31), not a parallel scheme. The one delta A-WALLET adds beyond the existing plan: name issuance as a first-class reason, book VO (USD-pegged) issuance as a house liability in the journal, and insert a WA-vs-VO issuance decision gate before the levers ship. The portal today transitively holds the full game-server money key (bagman/earn/hmac authorizes escrow/settle/refund/purchase on the tentpole), so an allowlisted admin session is currently a full-economy principal - the key split is the enabling layer that must land first.

### 3.0 Enabling (zero money-code risk)

- Create bagman/admin/hmac (operator-populated) accepted ONLY by new admin-facing tentpole endpoints, requiring a non-empty actor (the verified approver accountId) or 400
- Add a RouteSpec adminWallet flag in api.ts granting read on bagman/admin/hmac and nothing else (mirrors the mints/flags/releasePointer capability-flag pattern)
- shared/inbound-auth.ts: copy currency-earn's constant-time verifySignature verbatim + endpoint->key-id allowlist + requireActor
- Drop the earn key from the Epic callback; move /resolve-identity onto a dedicated bagman/resolve/hmac so the earn key has ZERO non-mint holders (E02)
- Metric-filter alarms (kms:Sign, UpdateFunctionCode on money fns, GetSecretValue on money secrets, DeleteResourcePolicy/DeleteTable from non-Lambda)
- grants.test.ts: no function holds earn key + admin key together; no function holds a verify key + kms:Sign together; earn-key non-mint holders == 0

**Proof:** Both new secrets exist and currency-earn LastModified is unchanged (money path untouched); an earn-key-signed /resolve-identity returns 401; each alarm driven to ALARM from a canary log line carrying the exact phrase, operator confirms the email.

### 3.1 Read + Ledger

- Tentpole /admin-wallet read (POST-only, admin key, actor required, strictly read-only: VO/WA balances + inventory + entitlements + open escrow + founder tier) + escrow playFabId GSI on MatchEscrowTable
- Portal GET wallet route + Account-detail Wallet & Entitlements panel; resolve playFabId from the ACCOUNT row (never the caller); mask without accounts:pii; write a wallet.viewed audit row per lookup
- bagman-money-journal (LITE): PK playFabId, SK ts#ulid, GSIs day-index/kind-index; PITR + deletionProtection + IAM resource-policy Deny on Update/Delete/BatchWrite; rows {delta,currency,balanceAfter,kind,ref,reason,actor,nonce,outcome}; written AFTER the PlayFab call, never throwing
- PlayStream player_virtual_currency_balance_changed exported to a private S3 bucket with Object Lock (non-optional, D13); nightly journal-vs-PlayFab drift check with an alarm

**Proof:** A signed admin-key call with the operator's playFabId returns real VO/WA cross-checked in PlayFab; the same body signed with the earn key returns 401 and without an actor returns 400; a wallet.viewed audit row names actor/subject/requestId; a canary earn returns 200 with exactly one journal row matching delta/balance/ref/actor/nonce; a live update-item on the journal is DENIED by the resource policy.

### 3.2 Levers (Issue / Adjust)

- GATE: resolve the WA-vs-VO issuance decision before building VO issuance (see decisions_for_operator)
- IronicsWalletAdjustments proposal table; portal POST /v1/admin/wallet/adjust (propose) /approve /reject - require wallet:adjust + a fresh 15-min ironics-admin elevation; 'Issue' button opens the same propose form pre-set to a positive delta + reason=issue_grant
- Guards: amount frozen at propose; server-enforced typed-amount echo on approve; caps as CODE CONSTANTS (self-approve <=10,000 V, hard max 100,000 V per adjustment; a WA cap is an operator decision); maker-checker ConditionExpression approvedBy<>proposedBy arming on >=2 ACTIVE owners (roster-of-one interim rule until then); debits fail-closed; unconfirmed PlayFab outcome is TERMINAL and human-only
- Tentpole /admin-adjust (admin key, required actor, nonce=adjustmentId#attempt) calls Add/Subtract, writes one journal row with actor=approver, and a house-liability row for each VO issuance
- PLAT-E15 Payments record form (Approve gated on economy:mint + fresh elevation); PLAT-E16 Catalog read-only (list proxy + the missing economy.registered audit row + a dryRun on register-economy; editing stays deferred, D16)

**Proof:** A 1-unit issue is proposed and approved with the typed amount; the balance moves by exactly that in the panel and in PlayFab; one journal row + two audit rows exist; a 200,000-unit proposal is refused server-side with NO PlayFab call; a wrong typed amount is refused; an expired elevation is refused.

### 3.3 Monitor + Reconcile

- PLAT-E17 Escrow & KPIs read-only monitor over Status GSIs (add a Status GSI to MatchSettlementLedgerTable) + the 8 KPI tiles (no actions)
- PLAT-E14 nightly drift + KPI roll-up row + alarms
- PLAT-E19 rake/ladder align to the SSOT (confirm-first, PIE-proven settle change) so rake finally accumulates in the house pseudo-account

**Proof:** Each KPI tile equals the journal day-aggregation field by field; an injected 1-unit drift drives the metric to 1 and fires the alarm, then returns to OK after the row is written.

### 4 Gated

- PLAT-E20 Holds (tentpole-enforced spend/stake/earn hold, fail-closed 423; PlayFab-native PurchaseItem + in-flight staked matches documented as limits, D14)
- PLAT-E21 downloads snapshot + held-pending-review emitter (D20)
- Move register-economy's bagman/playfab/secret behind the tentpole (removes the last full-economy portal function) - timing is an operator decision

**Proof:** A hold blocks the held action with a 423 and is visible in the panel and audit; nothing money-moving ships until its legal gate (L1 VC Terms) is in force - the console shows read-only state + 'refer to counsel' for refunds/voids (D17).

**Security baseline:**
- Append-only money journal: IAM resource-policy Deny on Update/Delete/BatchWrite for any principal; PITR + deletionProtection
- Hard caps as CODE CONSTANTS, never SSM flags (10,000 V self-approve line, 100,000 V per-adjustment max; WA cap operator-ruled)
- Named actor required on every admin-key request (400 without it)
- 15-min ironics-admin step-up elevation (own KMS alias; Authorization: Bearer rejected on /v1/admin/*)
- Maker-checker (approvedBy<>proposedBy) arms on the first second-ACTIVE-owner grant; roster-of-one interim = owner + fresh elevation + typed amount + caps + alarm; no self-approval past threshold
- Issuance is a guarded positive adjustment (reason=issue_grant), never an unguarded mint route; the amount is frozen at propose and typed-confirmed at approve
- VO (USD-pegged) issuance booked as a house liability row - admin-created pegged currency is never invisible
- S3 Object Lock PlayStream export is non-optional; nightly journal-vs-PlayFab drift alarm is the control (PlayFab = balance of record, journal = shadow ledger)
- Key split: bagman/admin/hmac (admin, actor-required) + bagman/portal-inbound/hmac (inbound-only) + bagman/resolve/hmac; grants test forbids a verify key + kms:Sign in one function
- Unconfirmed PlayFab outcome is terminal and human-only - never auto-retried
- No refunds/voids/reversals from the console until L1 Virtual Currency Terms are in force (D17)
- Operator prerequisites (not parallel work): populate the three HMAC secrets (32 bytes hex each), enrol MFA before the deny-unless-MFA policy applies

## J. Data-model additions

- IronicsApplications: add roles[] (SS) + per-role payload maps (tester?/creator?/dao?); keep ONE row per account (single-slot claim invariant); adding a role is an UPDATE not a second row
- IronicsRoleReviews (NEW table): reviewId=`${role}#${accountId}`, role, accountId, applicationId, state PENDING|APPROVED|REJECTED, cohort?, decidedBy?, decidedAt?, reason?; GSI state-created-index (PK `${role}#${state}`, SK createdAt). Tester never writes here.
- IronicsEntitlements: write `creator` (with cohort meta) on creator approval and optionally `beta.tester` on tester approval (today only `founder` is written)
- account.cohort WRITER (field is read-only today, rendered 'Not assigned' forever) via POST /v1/admin/accounts/cohort
- IronicsSubmissions (NEW table): submissionId, accountId, title, contentType (weapon|map|character|accessory), scarcity (1-of-1|edition N|unlimited), assetRefs, state DRAFT->SUBMITTED->IN_REVIEW->IN_VOTE->APPROVED/REJECTED->INTEGRATED; GSIs by-creator + state-created-index
- IronicsWalletAdjustments (NEW table): maker-checker proposal rows {accountId, playFabId(resolved from the account row), currency, delta, reason(issue_grant|support_goodwill|bug_compensation|clawback|dispute_settlement|other), ref, proposedBy, approvedBy, state}
- bagman-money-journal (NEW, LITE): PK playFabId, SK ts#ulid, GSIs day-index + kind-index; PITR + deletionProtection + IAM Deny on Update/Delete/BatchWrite; rows carry delta/currency/balanceAfter/kind/ref/reason/actor/nonce/outcome; house pseudo-account rows for rake and for each VO issuance
- MatchEscrowTable: add a playFabId GSI (for the wallet inspector's open-escrow read). MatchSettlementLedgerTable: add a Status GSI (for the escrow monitor)
- permissions.ts: add applications:review, cohort:assign, creator:grant, submissions:review (wallet:adjust + economy:mint already exist); map into bundles; optional data-only reviewer/dao console role (primitive supports it, no migration)
- roadmap.json (model B/D): rows {id, lane, status enum, when, copy, sourceRef, updatedAt} + a c12/dao namespace for the DAO roadmap, same CI gate ('no dated NEXT/PLANNED row without a source'); releases generated from committed manifests
- Secrets (operator-populated): bagman/admin/hmac (admin, actor-required), bagman/portal-inbound/hmac (inbound-only), bagman/resolve/hmac (splits /resolve-identity off the earn key)
- NAV/FOOTER/sitemap wiring: data.ts NAV - remove Houses, add C12/Creators(re-homed)/Sponsors, drop the duplicate Portal; FOOTER_COLS - GAME / C12.DAO / Legal; sitemap - remove /houses (301 -> /build), add /c12, /creators(reframed), /sponsors, /join

## K. Mockups needed (17)

- Header two-zone lane model (GAME | C12) with hair divider + micro-labels, and the restructured footer (GAME / C12.DAO / Legal)
- Home revised (game lane, twin-section removed, corrected character preview, closed-beta-live band, single C12 cross-link band)
- /build revised (six-axis honest creator copy + folded 'ruled six' character reference band, from a corrected .dc.html canvas)
- /c12 DAO hub (hero + four pillars + portfolio + what-the-DAO-does + why-participate + governance-model + get-involved band, placeholders visible as placeholders)
- /c12 DAO roadmap detail (status-first, date-agnostic, 'Updated' stamp)
- /creators Alpha creator-economy front door (Alpha-labeled, forward)
- Creator submission pipeline states - public creator view (DRAFT/SUBMITTED/IN_REVIEW/IN_VOTE/APPROVED/REJECTED/INTEGRATED)
- /sponsors (or /partners) offerings page (8 sections incl. the offerings matrix and the <72h speed section)
- /join roles hub - default state + per-role field-reveal states (Tester / Creator / DAO selected)
- /market revised (Beta-placeholder label + catalog-driven grid + Alpha creator-marketplace forward panel)
- /beta Tester deep-flow revised (auto-approval copy, Windows-only, size/unsigned note)
- /roadmap revised (37-row honest set + releases strip + 'Updated' stamp)
- Admin: Wallet & Entitlements inspector on Account detail (read-only balances/inventory/entitlements/escrow)
- Admin: Adjustments / Issue Watts & Volts queue (propose -> typed-confirm -> approve/reject; self-approve vs second-admin states)
- Admin: Applications review queue (Creator / DAO tabs, per-application drawer, cohort assignment)
- Admin: Submission review queue (DAO approval / vote states)
- Admin: Escrow & KPIs monitor, Catalog read-only, Payments record form (fold into the existing 12-artboard admin canvas per D9)

## L. Decisions for the operator (14)

1. [ ] LANE MODEL: two-zone grouped header (recommended - no new component) vs a lane switcher (maximal separation but net-new component + implies a second home at /c12). This drives whether there is one home or two.
2. [ ] SPONSOR page: route name (/sponsors vs /partners, /partners recommended for the B2B read), nav-vs-footer placement, and which lane it lives in.
3. [ ] CREATORS lane + NAME COLLISION: place the creator economy in the C12/company lane (recommended, DAO-managed) or the game lane; and resolve that the mandate's economy Creator is a DIFFERENT concept from today's streamer /creators - fully replace, or keep a separate streamer program under a different name (both cannot own 'Creators' in nav).
4. [ ] ROLES HUB: route name (/join vs /apply vs /roles) and the header CTA label ('Join' vs 'Apply' vs 'Get the beta').
5. [ ] WA vs VO ISSUANCE ASYMMETRY (blocks E18 VO issuance): is admin issuance of VO (USD-pegged, no offsetting cash) permitted before the L1 Virtual Currency Terms are in force, or WA-only until then? Plus per-currency caps (a WA self-approve line and per-adjustment max - '100,000' is not currency-equivalent) and confirmation VO issuance is booked as a house liability.
6. [ ] SCARCITY SSOT RECONCILIATION: the in-force IRONICS_PRICING_SSOT.md s2 retires scarcity 'with no exceptions and no carve-outs'; W-CREATOR reintroduces creator-set 1-of-1/many. Rule whether creator-minted items are a formal carve-out (SSOT amendment) or a separate econ category - until then no live 1-of-1/limited-mint marketing ships.
7. [ ] CREATOR PAYMENT MODEL vs the 'NO CASH-OUT, EVER' invariant: creators earn non-cashable Volts/Watts (closed loop, honors the invariant, not a real-money creator economy), a real-money revenue share (breaks the invariant, needs a payout rail + KYC/tax/1099), or a hybrid. This is the load-bearing decision under the whole thesis.
8. [ ] 'ALPHA' NAMING: labeling the future flagship economy 'Alpha' while the current playable is 'Beta Lands' reads backwards to gamers - is 'Alpha' a dev-stage or a product name, and how does it order against the existing 'Launch - 2027 - currency reset' roadmap row?
9. [ ] PRESS PLACEMENT: keep in the GAME zone (IRONICS press) or move to the C12/company zone (company-wide PR)?
10. [ ] HOUSES DISPOSITION: fully delete /houses (301 -> /build) or keep an out-of-nav /characters page. Nav slot goes to C12 either way.
11. [ ] SECOND ACTIVE OWNER: add a second admin to IronicsAdmins now (arms the maker-checker four-eyes state machine) or run the roster-of-one interim rule for issuance; and whether creator:grant / dao-approval sit inside owner or a new data-only reviewer/dao role.
12. [ ] COUNSEL GATES (do not draft, do not invent): DAO ownership-interest framing (securities, propose legal item L6); the Creator/UGC agreement (IP license, revenue share, payout + tax); Marketplace/secondary-market terms; the minors-and-marketplace posture (site is 13+); the meaning of 'player IP routing' (player intellectual property vs player data/traffic) and the Privacy Notice v0.3 needed before any sponsor data routing; L1 Virtual Currency Terms (already blocks staking/Volts claims AND all creator resale/property copy).
13. [ ] register-economy title-secret move (the one portal function still holding bagman/playfab/secret): Phase 3 or deferred.
14. [ ] Mailbox provisioning for the new intake addresses the overhaul implies (creator, DAO-interest, sponsor/partners, press) before any form promises a response.

## M. Document gaps (what is still needed)

- The SECOND C12 DAO doc (features / benefits / roadmap / timeline - the marketing content) is still unread (OneDrive, sign-in-gated). Re-share via repo or Google Drive, or /c12 ships on the mission/governance skeleton with bracketed placeholders. NOTE: one doc IS now in hand - the Operating Agreement - which supplies the governance MODEL (safe as high-level framing) but NOT features/benefits/roadmap.
- DAO FEATURES list - the actual capabilities of C12 AI DAO (fills /c12 Section 'What the DAO does')
- DAO BENEFITS list - member/participation benefits and any token utility (fills 'Why participate') - held behind the securities gate
- REAL DAO roadmap/timeline - milestones and their order/dates (operator: intent factual, dates are not) - ships status-first with '[timeline TBD]' until provided
- TOKENOMICS / OWNERSHIP mechanics - CARNIVAL ERC-20 specifics, allocations, treasury/multisig, vesting: NON-PUBLISHABLE per the guardrail; may never appear on the public site - the DAO page states mission + governance MODEL only, everything else bracketed for operator + attorney
- The THREE other games 'in the barrel' - names, genres, scale/skill, status (fills the portfolio cards; nothing on disk)
- ENTITY relationship - Carnival 12 Ai DAO LLC (Wyoming parent) vs C12 AI Gaming (settled publisher, legal.ts:8) vs the public 'C12 AI DAO' branding: which name governs the company lane and whether the DAO is presented as a filed entity
- PUBLISHABILITY confirmation - which robotics/hyperledger facts can go on a public page at company/R&D altitude, and explicit confirmation that company-R&D hyperledger language does NOT breach the game-side NOT-PUBLIC gate on the economy hyperledger/custody
- C12 HERO tagline / positioning line (the second doc may specify one)
- SPONSOR commercial specifics - packages/pricing/rate card, per-offering real lead times, named live sponsors + brand-mark guideline files (none on disk; W11 partner-line-only until real), and a real <72h integration example
- MINI-GAMES status - which of Drone Racing/leagues, Shooting, Cart Racing, Space Invaders are greenlit vs 'and more' (none exist in the map/mode SSOT; only a MiniGame hub ZONE tag) so each is labeled honestly
- player-IP + aligned-interest ROUTING definition and its data/consent/localization mechanics (operator terms, undefined on disk)
- The DAO VOTING model for the submission pipeline - who is eligible to vote, weighting, quorum, on-chain (Aragon) vs internal, and how the DAO's 'ultimate' approval executes over a community vote

## N. Sequencing

Design and proposal only this phase; nothing wires until the operator approves the artboards and rules the decisions (M-METHOD). Suggested order: (1) APPROVALS + MOCKUPS FIRST - produce the /Design artboards listed above, get sign-off, and get the operator to rule decisions_for_operator + supply-or-bracket the DAO facts. (2) TRUTH-PASS FIXES SHIP IN PARALLEL (they are ruled and need no DAO docs and no lane debate): home (twin removal + character counts + beta-live line), /market (Beta-placeholder label + catalog-drive), /founders, /beta, /portal, /press, /build, /roadmap - plus roadmap model B (roadmap.json + CI gate). This removes the bulk of the ~177 false claims at the source before any new copy lands. (3) HOUSES->C12 SWAP: wire NAV/FOOTER/sitemap for the two-zone header, ship /c12 on the mission/governance skeleton with bracketed placeholders, 301 /houses -> /build, retire HouseTile/HOUSES. DAO specifics slot in as data edits when the second doc arrives. (4) NEW COMPANY-LANE PAGES: /creators (Alpha economy front door, forward-labeled), /sponsors|/partners (AFTER the W11 twin-removal prerequisite lands), /join (Tester path live via the existing /beta flow; Creator/DAO capture-only). (5) ROLES DATA MODEL + ADMIN REVIEW QUEUES behind /join (IronicsApplications roles[], IronicsRoleReviews, entitlement writers, cohort writer, admin permissions + screens). (6) ADMIN WALLET BUILD on its own dependency graph, gated on the WA/VO decision and the secret/MFA prerequisites: 3.0 Enabling (key split) -> 3.1 Read + Ledger -> 3.2 Levers (Issue/Adjust) -> 3.3 Monitor/Reconcile -> Phase 4 gated. (7) SUBMISSION PIPELINE (IronicsSubmissions + creator/admin queues) - Alpha, designed-not-wired; the voting/DAO-governance mechanism deferred to the DAO docs. (8) LEGAL-GATE MAP governs go-live across all of it: /c12 ships now on placeholders but the DAO-ownership option + any benefit/return language is blocked on counsel (securities); the creator economy's live buy/sell/trade/property/payout is blocked on L1 + a Creator agreement + Marketplace terms + a minors posture; sponsor player-data routing is blocked on Privacy v0.3 + consent + DPAs; any refund/void from the console is blocked on L1. Each surface is Alpha/placeholder-labeled until its gate is in force. Every check means the control was exercised on the live stack (a beam watched, a balance moved, an alarm fired), not that it compiled.

## O. Reviews (kept in full — section A answers these)

### Legal & money-safety (exposure flagging, not legal advice) — verdict: sound-with-fixes — the design preserves and in several places strengthens the ruled money-safety doctrine (append-only journal, code-constant caps, named actor, step-up, maker-checker, WA-vs-VO liability gate) and correctly funnels every DAO/creator/sponsor legal question to counsel gates before go-live; nothing ships this phase (M-METHOD). But four specifics must be fixed in the design before the corresponding builds are greenlit, chiefly a key-split invariant that as written leaves the portal a transitive full-economy principal, and an issuance path that is de-novo minting wearing an adjustment's guards.

- **[BLOCKER] Admin issuance — zero portal holders of the game earn key (lens Q4)** — The proposal encodes the key-split invariant as `earn-key non-mint holders == 0` (Phase 3.0 grants.test + security_baseline). That explicitly PERMITS the portal claim-mint path (approve-claim rides /earn to mint Volts) to keep bagman/earn/hmac. The ruled target is ZERO portal holders, full stop (decision log D12: '…so the game-server earn key has ZERO portal holders'), and the source scope already puts the mint path on the admin key (IRONICS_ADMIN_DASHBOARD_SCOPE.md:132 — bagman/admin/hmac accepted by '/earn for claim mints'). bagman/earn/hmac is the same key that authorizes /escrow-entry, /settle-match, /refund-purchase, /purchase-bundle on the tentpole (admin scope C:67). If a mint holder keeps it, a portal compromise on the mint path is still a full-economy principal — the exact hole the split exists to close — so the lens question ('zero portal holders of the game earn key') is answered NO as literally specified.  
  Fix: Move claim-mints onto bagman/admin/hmac (actor-required) per admin scope E1, and change the grants assertion to 'no portal function holds bagman/earn/hmac' (zero total portal holders, not zero non-mint). Keep the existing 'no function holds earn key + admin key' and 'no verify key + kms:Sign' assertions alongside it.
- **[BLOCKER] Admin issuance — de-novo minting under adjustment-grade guards + roster-of-one** — reason=issue_grant creates currency from nothing, yet it runs under the SAME self-approve<=10,000 V line as a goodwill credit and under the roster-of-one interim rule (D11/D2). The proposal's own words ('issuance is a guarded positive adjustment … never an unguarded mint route') are contradicted by allowing a single owner to self-approve issuance. The security_baseline restates the 10,000/100,000 caps but drops the per-admin DAILY cap that the underlying scope included (admin scope O3, line 132), so roster-of-one + 10,000/adjustment self-approve = effectively unbounded issuance across many adjustments before a human sees it. De-novo minting is precisely the action four-eyes exists for.  
  Fix: Make reason=issue_grant require four-eyes ALWAYS (no self-approve at any amount) — which means WA/VO issuance ships only after the second ACTIVE owner exists (add the owner now, or hold issuance). If any interim self-approve is kept, set a far lower issue-specific self-approve line and an explicit daily/velocity cap as code constants, separate from the goodwill-credit line.
- **[MAJOR] VO (USD-pegged) issuance vs L1 Virtual Currency Terms** — VO issuance is gated only on the business 'WA-vs-VO decision', not on L1. Admin-created USD-pegged currency with no offsetting cash is a redeemable-value liability created with no governing terms in force — the same legal object D17 blocks for refunds/voids ('none from the console until L1 Virtual Currency Terms are in force', decision log D17). Booking it as a house-liability row (good) does not make it legal to issue before terms exist; it just makes the liability visible.  
  Fix: Gate VO issuance on L1 in force, mirroring D17 exactly. Until L1 is in force, WA-only issuance; the console shows VO issuance as read-only 'refer to counsel'. Confirm the house-liability booking is a hard requirement, not optional.
- **[MAJOR] DAO ownership-interest capture vs securities solicitation** — A public 'DAO ownership interest' apply form on /join and /c12, sitting alongside a 'Why participate — benefits' section ('savings passed to players, creators keep upside'), a governance MODEL, and the existence of a CARNIVAL ERC-20, may itself constitute general solicitation / conditioning the market under Reg D — regardless of the 'grants no entitlement' framing. Building a public investor-interest list can taint a later private placement (forcing 506(c) + accredited verification). A 'not an offer / not financial advice' disclaimer is necessary-not-sufficient: it does not cure solicitation when surrounding copy markets economic upside (the Howey 'expectation of profits' prong). The guardrail hides token allocations/treasury but not the aggregate solicitation risk of the surfaces existing together.  
  Fix: Counsel must gate the EXISTENCE of the public ownership-capture and any value/benefit/upside language, not only token specifics. Strip 'creators keep upside' and any economic-return framing from ownership-adjacent surfaces; reduce the DAO capture to a plain interest/updates list with zero forward-looking value language until counsel rules; treat the governance-model publication (currently marked 'safe') as counsel-gated too, since it describes the investment. Propose the counsel item (the proposal's L6) as a hard blocker on the DAO-ownership option going live.
- **[MAJOR] Creator supply-mint separation of duties** — The creator->submission->IN_VOTE->INTEGRATED->mint chain re-activates the MintCap primitive and books mints through the journal (good), but the APPROVAL chain has no separation of duties. A single actor holding creator:grant + submissions:review could approve a captive/alt creator account and then approve its first submission to INTEGRATED, minting supply through the money journal with one pair of hands. The proposal lists 'where creator:grant/dao-approval sit' as an operator decision but never flags this self-dealing/insider-mint path.  
  Fix: Require different approvers across creator-grant and submission-integration (the actor who granted a creator cannot approve that creator's first integrated mint), or keep INTEGRATED->mint under independent four-eyes. Add a grants/logic test that a creator's grantedBy != the integration approver for their submissions.
- **[MAJOR] Sponsor player-IP / aligned-interest routing to a 13+ audience** — 'Player-IP + aligned-interest routing incl. localization' is behavioral targeting of sponsor content to segments. The audience is 13+ (per the beta age gate) and explicitly global (localization is in scope), so routing to 13-17 year-olds triggers minor-privacy/ad regimes (CA Age-Appropriate Design Act, state 'no targeted advertising to known minors' statutes, UK/EU children's codes, GDPR lawful-basis). The proposal gates this on 'Privacy Notice v0.3', which addresses disclosure but not the lawfulness of targeting minors, and correctly flags the 'player IP' = IP-address(PII)/geo vs intellectual-property ambiguity — but underweights the minor-targeting exposure specifically.  
  Fix: Add a counsel gate specifically on behavioral sponsor targeting of known-minor accounts (default-exclude 13-17 from sponsor routing until ruled); resolve 'player IP' definition before any routing copy or build (IP-address routing = processing PII, needs lawful basis + consent + DPAs with sponsors); keep every reach/lead-time figure bracketed as the proposal already does.
- **[MAJOR] register-economy still holds the PlayFab title secret in the portal** — The proposal's 'zero portal holders of the earn key' claim does not make the portal non-full-economy: register-economy still holds bagman/playfab/secret (the PlayFab title secret) and the proposal sequences moving it as 'Phase 3 or deferred / operator decision'. While it lives in the portal, a portal compromise can mutate the PlayFab catalog directly (create/modify currencies, items, prices) — bypassing the journal AND maker-checker entirely. Catalog mutation is the one economy lever with no dual control in the plan.  
  Fix: Do not indefinitely defer: move register-economy behind the tentpole (admin key + actor + audit) before or with the issuance levers, since issuance and catalog control are the same threat surface. Until moved, record catalog mutation in the risk register as an un-guarded full-economy path and keep catalog editing deferred (D16) — read-only only, as planned.

### BRAND & SCOPE — does every new screen stay in the shipped Glass Black system; is the two-lane separation actually clean or does DAO bleed into game; is the mockup list buildable in one /Design canvas; is the sequencing realistic for one engineer + operator under mockup-first gates? — verdict: sound-with-fixes. On BRAND the proposal is disciplined and grounded: it stays inside Glass Black (green/violet/hair, t-data, Orbitron/Noto — no new tokens or fonts), and it correctly refuses to invent a lane "switcher" because the token sheet confirms no dropdown/menu/tab/toggle/segmented-control exists outside /admin (TOKENS §4 "Tabs: admin only"; "Modal/dialog/toast/tooltip: none exist"). It also keeps the money typed-confirm/modal UI on the admin canvas (which already designed it — ADMIN_SCOPE:152 "Adjustments queue with typed-amount confirmation") rather than on the public site, and it kills the real Portal nav duplicate (data.ts:169 vs SessionNav). Footer GAME/C12/Legal is a clean fix to the mislabeled Product/Company split (data.ts:172-201). The two-lane INTENT is coherent and route-grouping is sensible. But three MAJOR issues keep it from "sound": (1) the two-zone header — the load-bearing structural separator — is bolted onto a flex-wrap header (SH:14-65) that already wraps 8 items + session + CTA; adding a 1px vertical hair divider + two micro-labels means the GAME/C12 grouping collapses on tablet/mobile exactly where separation matters; (2) color-as-lane collides with color-as-status: violet is already the shipped "planned/NEXT" and "sponsor" token (TOKENS §1a, RM:14, build badges), so making violet the C12 lane identity signals "unshipped" on a real company mission and leaves violet "planned" badges on the green GAME roadmap — violet can no longer encode lane; (3) scope/sequencing presents ~8 near-parallel workstreams that for one engineer + one operator is a multi-month program gated by ~20 per-page approval rounds (W30), 5+ counsel gates, the unread 2nd DAO doc, and operator-only prereqs — with no MVP cut or critical path named. None is fatal; all are fixable in the mockup/approval phase (which is all that ships now per M-METHOD).

- **[MAJOR] Two-zone header responsive wrap (brand / lane separation)** — The proposal makes the grouped header the PRIMARY structural mechanism holding the lanes apart ('held apart by structure, color, route grouping'), separated only by the existing 1px vertical hair divider (rgba(255,255,255,.11)) plus two t-data micro-labels. But the shipped header is flex-wrap with min-h-68px and already wraps 8 nav items + SessionNav + a CTA at narrow widths (TOKENS §3 Header, SH:14-65). A vertical hairline and inline group labels are the first things to break on wrap: below ~1024px the GAME and C12 groups collapse into one undifferentiated wrapped list and the lane distinction visually evaporates on tablet/mobile — the population most likely to browse a game site.  
  Fix: Design the narrow/wrapped header state explicitly and prove it at 375px in the mockup: stack the two groups as labeled rows, or replace the 1px vertical hair with a treatment that survives wrap (full-width sub-rows per zone, or a labeled section break). Do not let a flex-wrap hairline be the sole carrier of the lane split.
- **[MAJOR] Color used as the lane axis (design-system semantic overload)** — The proposal assigns green=GAME lane and violet=C12/DAO lane and puts a violet eyebrow on 'every C12/DAO/creator/sponsor surface.' But in the shipped system violet/violet-text already MEAN 'planned/NEXT' and 'sponsor' (TOKENS §1a: violet '#7A2BFF sponsor/planned'; roadmap+build NEXT/PLANNED badges RM:14; sponsor partner divider SF:55), and green already means 'live/shipped.' So violet-as-lane makes a real, shipped company mission on /c12 read as 'not shipped/planned,' while the green GAME /roadmap still shows violet 'planned' badges — violet now signals two different things on the same page and can no longer reliably encode lane. Color cannot carry both lane AND shipped-ness cleanly.  
  Fix: Let structure + the eyebrow WORD (a literal 'C12' / section label) carry the lane, and keep green/violet reserved for their shipped status meaning (live vs planned/sponsor). If a lane accent is still wanted, don't reuse a meaning-bearing token for it; and never let the /c12 mission (a real thing) inherit the 'planned' violet signal.
- **[MAJOR] Scope realism for one engineer + operator under mockup-first gates** — The sequencing lists ~8 workstreams (truth-pass, Houses->C12, 3 new pages, roles data model + 3 admin screens, the 5-phase wallet build, the submission pipeline, the legal map) as if parallelizable. ADMIN_SCOPE already sizes the admin console alone at ~27-38 engineer-days (Phase 1 7-11 + Phase 2 20-27); the overhaul stacks a money-safety wallet epic (key split, append-only journal with IAM deny + PITR, drift alarms, maker-checker), a new roles data model (new table + GSIs + entitlement/cohort writers), and a submission pipeline on top. For one engineer this is a multi-month program, and its critical path is dominated by operator/counsel bottlenecks: ~20 per-page mockup approvals (W30 'one bundled round per page'), 5+ counsel gates, the unread 2nd DAO doc, and operator-only prereqs (3 HMAC secrets, MFA, a 2nd admin). No MVP cut or calendar is named, so 'the overhaul' reads as one deliverable.  
  Fix: Name the fast, fully-unblocked win first: step-2 truth-pass copy fixes ship immediately (they need no DAO docs, no lane debate) and clear the bulk of the ~177 false claims at source. Then present C12/creator/sponsor/roles/wallet/submission as separately-approved sub-programs each behind their own gate, and quantify the engineer-day load so the operator approves a program, not a single 'overhaul.'

### TRUTH — does the overhaul REMOVE the content pass's false/stale claims at their source rather than restyle them, and does it introduce any NEW claim that is not factual, placeholdered, or an explicit operator decision? Beta-as-placeholder must be unambiguous; DAO/portfolio/roadmap claims must be doc-sourced or bracketed. — verdict: sound-with-fixes — The proposal is structurally faithful to the truth pass: it deletes false surfaces at the source rather than papering over them (HOUSES removed not reworded; the 100%-false streamer /creators replaced; the Simularent "twin/sync/livery" fiction stripped as a HARD PREREQUISITE for the new /sponsors page; /market fixtures replaced with a catalog export; the cohort/waitlist/weekly-review copy removed site-wide; the 37-row honest roadmap set adopted). DAO/portfolio/roadmap unknowns are bracketed or gated (portfolio "3 [placeholder] titles", governance MODEL only, token/treasury/allocation held behind the counsel guardrail), and the load-bearing conflicts (scarcity SSOT, creator payout vs no-cash-out, all legal gates) are surfaced as decisions, not resolved by invention. But it introduces two genuine NEW-claim risks that must be fixed before any mockup/build: (1) the /sponsors offerings assert live player reach through surfaces that are not built, and (2) the "Beta = placeholder" boundary is left imprecise enough to risk labeling the genuinely-live shipped beta as a placeholder. Neither is fatal to the design; both are fixable with a status axis and a scope line. Verdict is sound-with-fixes, not unsound.

- **[MAJOR] /sponsors offerings matrix — reach claims on unbuilt surfaces** — The sponsor_offerings table states present-tense player reach through surfaces that do not exist in the shipped game. "Super Lobby sponsorship — reach: Every player passing through the pre-match Super Lobby" and "Shared sponsor collab space" describe an audience/traffic pattern that is not live: the Super Lobby ("Outpost Earth MWR") is a scope document, not a feature — C:\Dev\Bag_Man\Docs\Hub\IRONICS_LOBBY_HUB_SSOT.md:3 "Status: NEW. Architecture and scope definition for the Super Lobby" — and the honest roadmap already classifies it as unbuilt: IRONICS_WEBSITE_CONTENT_ROADMAP_PASS.md:940 "[GAME] Outpost Earth hub + walkable store | IN THE BUILD (PIE-proven; not watched on the shipped client)". Today matchmaking is direct FlexMatch with no walkable lobby, so "every player passes through the Super Lobby" is false about current game structure. The proposal labels ONLY mini-games and tournaments as ROADMAP and brackets fees/lead-times/reach-figures, but leaves the Super Lobby, collab-space and routing/localization SURFACES stated as live reach. This is the exact aspirational-as-live class the truth pass exists to kill (cf. its standing rule against '24/7 autoscaled fleet', IRONICS_WEBSITE_CONTENT_ROADMAP_PASS.md:60/939).  
  Fix: Add a build-status axis to the offerings matrix keyed to each surface's real status (Live / In the build / Roadmap), sourced from the 37-row set: in-match/venue placement has a live substrate (ARCANEON matchplay is LIVE, row 936) so 'in-arena eyeballs during a match' is defensible; Super Lobby, collab spaces, and player-IP/aligned-interest routing are unbuilt and their reach must read 'when live' / ROADMAP, exactly as mini-games already do. Keep the B2B 'available for agreement' framing for the offering, but never state present-tense player traffic through a surface that is not shipped.
- **[MAJOR] Beta-as-placeholder boundary (W-BETA) — imprecise scope risks a new false claim** — The mandate (decision log W-BETA, IRONICS_PLATFORM_DECISION_LOG_2026-09-06.md:249) says label Beta Land CONTENT as placeholder capability. But the honest roadmap treats the beta itself as genuinely LIVE and real: IRONICS_WEBSITE_CONTENT_ROADMAP_PASS.md:935 "[GAME] Beta Lands — Windows closed beta | LIVE", operator-proven on the shipped 0.1.3 client. The proposal's route_map phrases the /market label as "Beta Lands - placeholder capability" and /beta as "'Beta Lands - Windows' framing" without drawing the line between the two meanings of 'Beta Lands' (the ruled NAME of the live program vs. the placeholder economy/items shown). Executed as written, a page could tell readers the live, downloadable, playable beta is a 'placeholder' — a NEW false claim, and the review lens explicitly requires this be unambiguous.  
  Fix: State the boundary in the proposal and on every affected mockup: the playable beta client/program/download is LIVE and real (home keeps 'the Windows beta is live'); what is PLACEHOLDER is the economy/store CONTENT and item set shown (the Beta Land economy demos capability, not the shipped product). Put the 'placeholder capability' label on the market/economy items and the economy panels only — never on the beta program, the download, or the /beta intake.

---
*Generated 2026-09-07 from workflow wf_6b4abf5d-237 (7 lenses + architect + 3 reviews) without paraphrase; section A is the agent's grounding + fix layer after reading the C12 documents and the funding gate. Nothing implemented.*
