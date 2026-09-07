# IRONICS Platform - DECISION LOG (2026-09-06) - SSOT for the website / admin / roadmap / distribution phase

**Authority.** On 2026-09-06 the operator approved the 16-board admin-console designs and cull options A + C, and delegated
every remaining decision in `PLATFORM_GROUNDING_BRIEF_2026-09-06.md`, `IRONICS_ADMIN_DASHBOARD_SCOPE.md` and
`IRONICS_WEBSITE_CONTENT_ROADMAP_PASS.md` to the agent, to be ruled by engineering, computer/data-science and business best
practice ("Make decisions best for our success and needs"). Constraints in force: **$50/month scaling cap; downloader free or
inside the $15/month tier for the first 1,000 testers; AAA quality; mockup-first for new screens; PROVEN = watched.**
Destructive/irreversible steps and anything the operator must do in person remain confirm-first.

Status legend: **RULED** (in force) · **RULED-CONDITIONAL** (in force, with a named fallback) · **OPERATOR** (needs the
operator's hands or signature) · **DEFERRED** (named trigger).

---

## 0. Operator rulings (verbatim intent)

| # | Ruling | Status |
|---|---|---|
| O1 | The 16 admin-console mockups (artifact 4b4a4163) are approved; build may proceed screen by screen. | RULED |
| O2 | Cull option **A** (slice-migrate the ARCANEON closure to `/Game/BagMan/ArcaneonArt`, then NeverCook + remove the other 852 packages) **+ C** (4K cap on the slice's 8K normal/ORM maps) - verified by the audit's 12-step recipe on the client AND server cooks and a watched ARCANEON lap. | RULED (repo `git rm` stays confirm-first) |
| O3 | Downloader/installer: free for now, or inside the $15/month tier; acceptable for the first 1,000. | RULED -> D-DIST below |
| O4 | Priority: admin controls + dashboard first, then content + roadmap. $50/month scaling cap. | RULED |

## 1. Distribution (supersedes the three prior positions)

| # | Decision | Ruling | Rationale |
|---|---|---|---|
| D-DIST-1 | Delivery path | **AMENDED 2026-09-06 after primary-source verification (see s7).** CloudFront + Origin Access Control + key-group signed URLs in front of the existing `ironics-releases` bucket (D2 Tier 0), **on PAY-AS-YOU-GO pricing (no flat-rate plan)**, using the always-free 1 TB / 10 M-request tier; **flip to the flat-rate Pro plan ($15/month, 50 TB) when monthly egress passes ~1.18 TB (~300 downloads).** Presigned S3 (D1) retires the day Tier 0 is proven. | The 1 TB / 10 M always-free allowance exists ONLY under pay-as-you-go. The flat-rate FREE plan is 100 GB + 1 M requests (~25 downloads of the 3.9 GB zip) **and excludes access logs**, so it would break the download telemetry in D-DIST-3. Pay-as-you-go costs ~$0.33 per 3.9 GB download past the free tier; Pro converts that to a flat $15 with no overage up to 50 TB (~12,800 downloads) - it is the operator's "$15 tier" and covers the whole ramp to 1,000. Fixes the 15-minute presigned-link expiry (24 h signed URLs), keeps the API un-proxied. R2 (GTM s7) would require re-hosting 20 GB and a Worker signer for no cost advantage at this scale. |
| D-DIST-2 | Link policy | 24 h bearer signed URL, no IP binding; per-account mint quota 3/day on the click path; metadata-only `GET /v1/download/latest/meta` so page renders never mint (D2 fix #2). | Honest metrics + resumable downloads; quota stops link farming without hurting retries. |
| D-DIST-3 | Telemetry | CloudFront standard logs v2 -> private S3 (30-day lifecycle) -> Athena partition projection -> nightly snapshot into the admin roll-up rows. c-ip dropped at the Athena view (kept 30 days raw). | Pennies at this volume; gives completed/failed/Mbps/country for the Downloads panel. |
| D-DIST-4 | Launcher / patch paks | DEFERRED. Triggers: > 2 patches/month, or median download > 45 min, or > 300 active testers. | GTM s7.2 triggers stand; a launcher is a product, not a beta need. |
| D-DIST-5 | Version retention | Latest + previous hot in the bucket; older versions to Glacier Instant Retrieval after 30 days; manifests committed to `releases/`. | 20 GB -> ~8 GB hot; history kept. |
| D-DIST-6 | Kill switches | `download.enabled` flag (fails closed) + CloudFront key-group revocation as the nuclear option; BytesDownloaded 2x-daily alarm + AWS Budgets alarm at $50 gross. | Cost circuit breakers before growth. |

## 2. Admin console - auth, roles, home (scope D1-D12, D30-D31)

| # | Decision | Ruling |
|---|---|---|
| D1 | Auth model | **RULED: Option A** - roles as permission bundles on `IronicsAdmins` + `requires` on RouteSpec + owner-only admin-management routes + 15-min admin-audience elevation token (own KMS alias), Bearer rejected on `/v1/admin/*`, Access identity bound to the row (`stepUpSubject`). |
| D2 | Roles | **RULED:** viewer / operator / owner; ~9 permissions (accounts:read, accounts:pii, accounts:approve/ban, economy:mint, wallet:adjust, release:write, flags:write, admins:manage, audit:read). Money inside owner until a second human is granted; that first grant triggers treasurer + audit GSIs + CSV export. Owner = 01M06WNV71GP2QWYPBQA5JT4WC after the on-screen confirmation on /portal (OPERATOR, 15 min). |
| D3 | Step-up | **RULED-CONDITIONAL:** Cloudflare Access (Zero Trust free) on `admin.ironics.org` with One-time PIN as IdP + Independent MFA (security key, TOTP fallback), assertion verified on the elevate route. Fallback 1: Google as the Access IdP. Fallback 2: WebAuthn passkeys in Lambda. Access session 1 h; no IP rule. OPERATOR creates the Zero Trust team and runs the 30-minute compatibility check. |
| D4 | Admin token | **RULED:** aud=ironics-admin, 15 min, no refresh, cookie host-only on `api.ironics.org`, `Path=/v1/admin`, `SameSite=Strict`. |
| D5 | Phase 0 hygiene | **RULED, with a credential split:** (a) OPERATOR enrols an MFA device on IAM user `Lead_Developer_Ironics`; (b) a NEW least-privilege machine principal `ironics-agent-deploy` (scoped: CDK deploy roles via `cdk bootstrap` trust, S3 releases write, CloudFront invalidation, DynamoDB read on Ironics*/bagman-*, Cost Explorer read, SSM read; NO IAM/KMS/Secrets write) becomes the key used by CI and by the agent's sessions; (c) only THEN the deny-all-unless-MFA policy goes on the human group and the 86-day key is rotated/retired; (d) strip the 10 unrelated managed policies; (e) CloudTrail management trail + DynamoDB data events + control-plane alarms (kms:Sign, lambda:UpdateFunctionCode on money functions, secretsmanager:GetSecretValue outside granted roles, DeleteResourcePolicy/DeleteTable from non-Lambda principals); (f) deletionProtection x3, approve-claim ConditionExpression, CSP nonce/strict-dynamic + HSTS + X-Frame-Options + Permissions-Policy, cf-connecting-ip only when proxied, reservedConcurrency per function 5-10, AWS-SETUP.md quota fix, secrets out of Downloads, IAM Identity Center noted as the medium-term path. Order matters: (b) before (c) so nothing the automation depends on is locked out. |
| D6 | Deploy path | **RULED:** CI is the only web deploy path once the token has Zone -> Workers Routes -> Edit (OPERATOR, Cloudflare dashboard) + Required reviewers on `production`. Until then, local wrangler is the documented exception for the 09-08 hotfix only. |
| D7 | Console home | **RULED: Option B** - `apps/crm` on `admin.ironics.org` (second Worker, $0), tokens lifted to `packages/tokens`; Cloudflare Access on that origin. |
| D8 | Brand lock | **RULED:** the shipped web Glass Black; the console additions listed on the canvas note (left rail, card heading, mono value style, danger ghost, badge variants, status dots, inline SVG bars, 44px inline buttons) are ADOPTED into the web design system - re-extract the token sheet when built. |
| D9 | Build order | **RULED:** Settings (Admins & Roles) -> Accounts + Founder ladder -> Audit -> Overview -> Releases + Flags -> Economy (Payments, Adjustments, Holds, Escrow & KPIs, Catalog) -> Telemetry; States/Confirmations are shared components built with the shell. |
| D10 | Invitation | **RULED:** invite by accountId. |
| D11 | Dual control | **RULED:** roster = 1 rule (owner + fresh elevation + typed amount + code-constant caps 10,000 V self / 100,000 V max + alarm); maker-checker state machine ships on the first second-admin grant. |
| D12 | Key split | **RULED:** `bagman/admin/hmac` with required actor; extended to `/resolve-identity`; inbound-only `bagman/portal-inbound/hmac` for the game-event hook; grants test forbids verify+sign in one function. |
| D30 | Cloudflare plan | **RULED:** Free zone plan (Access free <= 50 seats). Cloudflare Pro is not needed; the "$15 tier" in the operator's ruling is the CloudFront Pro plan (D-DIST-1). |
| D31 | Deferrals | **RULED:** the scope's consciously-deferred list stands. |
| + | CFN cap | **RULED:** nested `AdminApiStack`; synth-time assertion Resources < 450. |
| + | Rate limiting | **RULED:** fail-closed limiter for admin routes. |
| + | PII | **RULED:** viewer sees masked identifiers; `accounts:pii` unmasks with `account.viewed` audit. |
| + | Break-glass | **RULED:** MFA-gated script + `admin.frozen`; runbook covers an Access/JWKS outage path. |

## 3. Admin console - economy, metrics, releases, flags, health (scope D13-D29)

| # | Decision | Ruling |
|---|---|---|
| D13 | Ledger | **RULED:** LITE shadow journal (admin paths + PlayStream -> S3 export + nightly drift alarm), S3 Object Lock export non-optional; full 7-Lambda journal when staked volume exists. |
| D14 | Holds | **RULED:** tentpole-enforced spend/stake/earn hold + separate "cannot play" (PlayFab ban). Phase 4. |
| D15 | Rake + ladder | **RULED:** align CODE to the staking SSOT (tiered 5/10 %, 6 rungs) in Phase 3 with a PIE-proven settle test; flat 5 % documented as the live rule until then. |
| D16 | Mint caps | **RULED:** keep the live MintCap enforcement; catalog editing deferred until the PRICING SSOT retirement is re-ruled. |
| D17 | Refunds | **RULED:** none from the console until L1 Virtual Currency Terms are in force. |
| D18 | Alerts | **RULED:** SNS -> the operator's email now; Discord webhook when the server exists (G-COMM-1). Best-effort SLA in the runbook. |
| D19 | KPI contract | **RULED** as proposed; money never enters third-party analytics. |
| D20 | Held-pending emitter | DEFERRED (Phase 4). |
| D21 | Founder | **RULED:** tier as entitlement; retire/reissue via library; in-game grants = content task (G-FOUNDER below). |
| D22 | Analytics | **RULED:** first-party `IronicsEvents` table + roll-ups = system of record. PostHog as a SERVER-SIDE second sink (accountId-keyed, no cookies) **after** the Privacy Notice v0.3 names it (legal bundle); Cloudflare Web Analytics likewise after the notice. Sentry deferred. |
| D23 | Tester | **RULED:** APPROVED + >= 1 honest download click; `first_match` once the hook exists. |
| D24 | Funnel | **RULED:** 8 locked steps + launched + first_match via the server-signed hook; 90-day retention; accountId-only. |
| D25 | Download telemetry | **RULED:** D2 fix #2 now; edge logs per D-DIST-3 (no longer deferred - free plan). |
| D26 | Web traffic | **RULED:** Cloudflare Web Analytics after the Privacy Notice update; client events stay OFF. |
| D27 | Flags | **RULED:** SSM; first switches download.enabled, apply.enabled / invite cap, manual-review, mint quota 3/day, FlexMatch cells (revert-from-audit), admin.frozen. |
| D28 | Release admin | **RULED:** minBuild + releaseNotes on the pointer, channels.json, pointer-only publish/promote/rollback/set-minBuild; semver 0.1.x-beta; retention per D-DIST-5; commit the 0.1.1-0.1.3 ledger records. |
| D29 | Health | **RULED:** read-only `/v1/admin/health`; OPERATOR checks GameLift Anywhere metric coverage; crash-free shows n/a. |

## 4. Cost posture

| # | Decision | Ruling |
|---|---|---|
| C1 | S12 server - **CORRECTED 2026-09-07, see s8** (c6i.large **Windows**, ~$129/month gross, credit-covered) | **RULED:** stay on-demand 24/7 while credits cover it (a live beta needs uptime; no 1-year commitment before product-market fit). AWS Budgets alarm at $50 gross/month. Re-evaluate a 1-yr no-upfront Compute Savings Plan (~$39/month) at 100 approved testers or when remaining credits < 2 months (OPERATOR reads the credit balance in Billing > Credits). |
| C2 | Platform incremental | ~$3-10/month for everything in the admin plan; CloudFront $0 (pay-as-you-go inside the 1 TB free tier) -> $15 (flat-rate Pro) at ~300 downloads/month; DynamoDB/Lambda/SSM pennies; Secrets Manager $0.40/secret. Inside the cap. |
| C3 | Vendors | Zero new paid vendors for the beta. Free tiers only: Cloudflare (Free zone + Zero Trust free to 50 seats), CloudFront pay-as-you-go free tier, PostHog free (after legal), Resend (existing). The only planned paid lines are CloudFront Pro ($15) at ~300 downloads/month and Azure Trusted Signing (~$10) at 100 testers - both inside the $50 cap. |

## 5. Website content + public roadmap (content pass 1-32)

| # | Decision | Ruling |
|---|---|---|
| W1 | Roadmap model | **RULED: D** - model B now (roadmap.json + committed manifests + REV from latest.json + CI schema check), model C (admin Roadmap panel) after release admin. |
| W2 | Status vocabulary | **RULED** as proposed (Shipped/Live · In the build · Next/Planned undated; HOLD never printed). |
| W3 | Releases + known issues | **RULED:** yes; 0.1.2 + 0.1.2.1 collapsed. |
| W4 | Semver + ledger | **RULED:** 0.1.x-beta; commit manifests; REV from latest.json. |
| W5 | VR row | **RULED - HOTFIX before 2026-09-08:** undated "PC VR (OpenXR / SteamVR, Quest 3 via Link) - in design, engineering not started. Standalone Meta Quest: not planned for the first VR release." Amend handoff s2. |
| W6 | Auto-approval | **RULED:** permanent for the closed beta; CTA relabel + cohort/review copy removed site-wide; legal text in the v0.3 bundle. |
| W7 | Founders | **RULED:** no in-game rewards printed until G-FOUNDER defines them; tier copy = "priority access + founder number". |
| W8 | TESTERS | **RULED:** hide when null; written from the D23 definition when the metrics spine exists. |
| W9 | Dead CTAs | **RULED:** remove now; Discord returns with G-COMM-1, press kit with G-PRESS-1, creator apply with G-CREATOR-1. |
| W10 | /market | **RULED:** "Preview - prices not final" label now; data-driven from a fresh catalog export when the store ships. |
| W11 | Sponsor | **RULED:** partner line only (Simularent); twin/sync/livery promise removed; FANATICS when real. |
| W12 | House roster | **RULED:** the ruled six; close CC-X6; retire "colour families". |
| W13 | Finish naming | **RULED:** catalog names only. |
| W14 | Creator placement | **RULED:** wording per disk ("one-click auto-zone in the creator; loadout lists"). |
| W15 | Sticker series | **RULED:** name printed only if the SeriesName field is set; count 14. |
| W16 | Home | **RULED:** remove the kicker; add "The Windows beta is live - Portal" now. |
| W17 | Reels + venue art | **RULED:** store under `Ironics-Platform/press/` (git, WebP/MP4 <= 25 MB each) + S3 press bucket for originals; publish with G-PRESS-1. |
| W18 | Consoles / mobile | **RULED:** undated + reworded; mobile "not in the beta". |
| W19 | Launch year | **RULED:** undated. |
| W20 | Server wording | **RULED:** "Dedicated servers (US-East)". |
| W21 | Text chat row | **RULED:** "In the build"; Live after the COMMS PIE gate. |
| W22 | Crash reporting | **RULED:** no row. |
| W23 | Signed installer | **RULED:** Planned row; Azure Trusted Signing (~$10/month) enters scope at 100 testers (SmartScreen is a conversion killer - see growth plan). |
| W24 | Money / legal | **RULED:** unpublished until L1/L3; draft VC link hidden. |
| W25 | Legal v0.3 bundle | **RULED:** one counsel pass after the copy truth pass (OPERATOR engages counsel). |
| W26 | DOB retention | **RULED:** engineer reads the handler first. |
| W27 | Build confidentiality | **RULED:** beta PUBLIC by default. |
| W28 | Mailboxes | **RULED:** confirm routing; add security@ + security.txt. |
| W29 | Founder number | **RULED:** 4-digit everywhere. |
| W30 | Mockup scope | **RULED:** one bundled round per page; as-built /portal + DownloadCard first. |
| W31 | Deploy path | = D6. |
| W32 | Roadmap panel | **RULED:** OPERATOR role edits; draft -> publish with audit; after release admin. |

## 6. Growth (new, delegated) - headline rulings; detail in the growth plan

| # | Decision | Ruling |
|---|---|---|
| G-COMM-1 | Community home | **RULED:** create the IRONICS Discord server now (OPERATOR creates; agent configures roles/channels/bot later) - it is the beta's retention and support surface and the destination of every CTA. |
| G-FOUNDER | Founder value | **RULED:** founder number + priority access + a founder-only cosmetic (helmet finish or emblem) defined as a content task; scarcity ladder 100/300/1000 is the campaign's engine. |
| G-INVITE | Referral | **RULED:** make "Founder invites" TRUE - each approved founder gets N invite codes (start 3) redeemable on /beta; tracked as a funnel source. Engineering item in Phase 2. |
| G-PRESS-1 | Press kit | **RULED:** real kit (logo, key art, 8 reels, fact sheet) on /press before any outreach. |
| G-CREATOR-1 | Creator program | **RULED:** GTM W9 shape; opens at 300 approved testers. |
| G-EVENTS | Play sessions | **RULED:** scheduled weekly play windows (announced in Discord) so lobbies fill; bot fill covers the rest. |
| G-SIGNING | Installer trust | **RULED:** = W23. |
| G-METRICS | North Star | **RULED:** installed -> first match >= 70 % (GTM s10); weekly cohort report from the admin Telemetry panel. |


---

## 7. Corrections and additions after primary-source verification (2026-09-06, same day)

Four fact-checks were run against vendor documentation before any code was written. One ruling was wrong and is amended
above; the rest are additions that bind implementation.

### 7.1 CloudFront - D-DIST-1 AMENDED (my error, corrected)

| What I ruled | What the docs say | Effect |
|---|---|---|
| "CloudFront FREE pricing plan, 1 TB + 10 M requests" | Two different things were conflated. The **flat-rate Free plan** is **100 GB + 1 M requests** and **excludes access logs**. The **1 TB + 10 M** allowance is the **pay-as-you-go always-free tier**. | Tier 0 ships on **pay-as-you-go**, not the flat-rate Free plan. |
| "Pro $15 when egress approaches the free allowance" | Pro = $15/month, **50 TB + 10 M requests**, access logs included, 25 WAF rules, KeyValueStore, no overage charges. | Crossover is ~**1.18 TB/month (~300 downloads)**; Pro then covers the entire ramp to 1,000 for a flat $15. |

Implementation notes now binding: signed URLs (key groups) and OAC are available on every pricing mode, so the Tier 0
architecture is unchanged. Standard logging **v2** is configured through the CloudWatch Logs delivery API in us-east-1
(`AWS::Logs::Delivery*`); the output format is fixed at creation and cannot be changed later; avoid Parquet (bills
CloudWatch). Real-time logs are unsupported under any flat-rate plan and must be disabled before subscribing. A Pro month
cannot be cancelled mid-billing-period, and a distribution under a plan cannot be deleted until the plan is cancelled.
**Check before any flat-rate subscription:** the account must not be an "AWS Free Tier" account, and it is unverified
whether promotional credits can pay the CloudFrontPlans line - assume they cannot and budget $15 cash.

### 7.2 Cloudflare Zero Trust / Access (D3, D7, D30 - confirmed, with detail)

Free plan is $0 for 50 users; a payment method is still attached at setup. Independent MFA explicitly supports logging in
"with your identity provider or with a one-time PIN" and enforcing MFA on top, so the ruled OTP + security-key path is
documented - the compatibility check now only has to confirm the MFA tab is present on a Free-plan application. Scope MFA
to the admin application or its Allow policy; do **not** turn on "apply global MFA settings by default", and leave "use
identity provider MFA" off (OTP carries no AMR claim). Application session duration and MFA authentication duration are
**separate timers** - set both to 1 h. Elevate-route verification: JWKS at
`https://<team>.cloudflareaccess.com/cdn-cgi/access/certs`, RS256, check `iss`, `aud` contains the app's AUD tag, refresh
on unknown `kid` (keys rotate every 6 weeks with a 7-day overlap). Protect the CRM by **worker_id** (covers custom domain,
workers.dev and previews) rather than hostname alone. Fallback 1 (Google IdP) is confirmed available on Free.

### 7.3 apps/crm as a second Worker (D7 - confirmed, with two required changes)

Feasible exactly as ruled ($0 incremental; `admin.ironics.org` has no DNS record today, so Cloudflare creates it on the
first custom-domain deploy). Two things this adds:

1. **D6 is a hard prerequisite, not a parallel task.** Attaching a new custom domain calls the same
   `zones/{id}/workers/routes` endpoint that returns code 10000 today, so the token permission edit must land *before* the
   first CRM deploy.
2. **The API's CORS origin list must widen** to `["https://ironics.org", "https://admin.ironics.org"]` in
   `apps/api/lib/api.ts` and be deployed - API Gateway ignores CORS headers a Lambda sets itself.

No `SameSite` change is needed: `admin.ironics.org -> api.ironics.org` is same-site, so both the existing session cookie
and the ruled host-only admin cookie travel on credentialed fetches. Elevation seam to build into the first screen: the
**browser** must receive the elevate response's `Set-Cookie` (the Worker never sees a host-only api cookie), while the
Access assertion arrives at the **Worker** and is forwarded server-side. Worker size is no longer a constraint (64 MiB
since 2026-09-04); the 180 KB initial-JS gate stays a web-only CI gate.

### 7.4 CloudFormation capacity (nested AdminApiStack - confirmed, shape fixed)

`AdminApiStack extends NestedStack`, a `home?: "portal" | "admin"` field on RouteSpec, and the route loop choosing the
scope keeps **both** existing test suites valid (grants.test.ts iterates parent + nested templates; admin-guard.test.ts
keeps parsing the single `routes` array). Projection: parent ~250/450, nested ~260/500 - capacity stops being the binding
constraint; deploy time and the shared 1,000-Lambda pool take over. Reserved concurrency becomes per-route (portal 10,
admin 5), replacing the uniform context knob; the `-c reservedConcurrency=50` instruction in the code comment and
AWS-SETUP.md is superseded and would fail. **Migrating the 8 live admin routes into the nested stack is explicitly NOT in
Phase 1** - it needs a remove-then-add with downtime and is confirm-first if ever needed. Doc-truth: the route table holds
22 routes, not 21.


## 8. Cost correction (2026-09-07) - the server is Windows, and it is 2.08x what I reported

**Read from the live billing meter, not a price list:** `BoxUsage:c6i.large` is metering at **$0.17700/hour**, and
`describe-instances` returns `PlatformDetails="Windows"`, `UsageOperation="RunInstances:0002"` - Windows Server,
license-included. Every earlier figure in this log and in the growth plan used the **Linux** rate of $0.085/hour.

| | Reported earlier | Actual |
|---|---|---|
| Hourly | $0.085 | **$0.177** |
| Monthly, 24/7 | ~$62 | **~$129** |
| Against the $50 cap | 1.24x over | **2.58x over** |

Credits are absorbing it today, so no invoice has been paid, but the run-rate is what decides how long the credits last.
The growth campaign itself still costs $0; this is entirely the game server.

**Levers, cheapest first (C1 is re-opened on this basis):**

1. **Scheduled stop/start around the ruled play windows.** The growth plan schedules two play windows a week. A server
   running ~40 hours a week instead of 168 costs **~$31/month** - inside the cap today, no commitment, fully reversible,
   and it can be automated with EventBridge Scheduler. Cost of being wrong: a tester finds the server down off-window,
   which the Discord schedule and a status line mitigate.
2. **A Linux server build.** UE dedicated servers run on Linux, and the same instance on Linux is $0.085/hour =
   **~$62/month**. This is the single largest structural saving available to the business, and combined with lever 1 it
   is **~$15/month**. It is a real engineering project (LinuxServer target, cross-compile toolchain, GameLift Anywhere
   re-registration), so it is scoped rather than assumed.
3. **A 1-year no-upfront Savings Plan on Windows** - roughly 35% off, ~$84/month. Still over the cap, and it commits a
   year to an instance shape the game may outgrow. Not recommended before lever 1 or 2.

**Ruled:** lever 1 is adopted now as the default posture once play windows begin; lever 2 enters the engineering backlog
as a costed investigation; lever 3 stays rejected. The AWS Budget already alerts on gross spend, so the next threshold
crossing is visible rather than discovered on an invoice.

**Process note:** this is the second ruling in two days corrected by reading a primary source instead of trusting a
remembered price (the first was the CloudFront plan allowance in s7). Cost figures in this programme are quoted from the
meter or the vendor's own page, with the query recorded, or they are marked as estimates.

---
*Ruled 2026-09-06 under the operator's delegation. Amend by appending; never rewrite a ruling silently.*
