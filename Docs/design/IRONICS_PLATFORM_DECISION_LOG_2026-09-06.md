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
| D-DIST-1 | Delivery path | **CloudFront + Origin Access Control + signed URLs in front of the existing `ironics-releases` bucket (D2 Tier 0), on the CloudFront FREE pricing plan; move to the CloudFront Pro plan ($15/month) when monthly egress approaches the free allowance.** Presigned S3 (D1) retires the day Tier 0 is proven. | The CloudFront free allowance is 1 TB/month of egress and 10 M requests (always-free), i.e. ~250 downloads/month at 4 GB for $0; Pro ($15) covers the ramp to 1,000 (verify the current plan limits at implementation - this is the operator's "$15 tier"). Fixes the 15-minute presigned-link expiry (24 h signed URLs), adds edge logs for download telemetry, keeps the API un-proxied. R2 (GTM s7) would require re-hosting 20 GB and a Worker signer for no cost advantage at this scale. |
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
| C1 | S12 server (c6i.large 24/7 ~= $62/month gross, credit-covered) | **RULED:** stay on-demand 24/7 while credits cover it (a live beta needs uptime; no 1-year commitment before product-market fit). AWS Budgets alarm at $50 gross/month. Re-evaluate a 1-yr no-upfront Compute Savings Plan (~$39/month) at 100 approved testers or when remaining credits < 2 months (OPERATOR reads the credit balance in Billing > Credits). |
| C2 | Platform incremental | ~$3-10/month for everything in the admin plan; CloudFront $0 -> $15; DynamoDB/Lambda pennies. Inside the cap. |
| C3 | Vendors | Zero new paid vendors for the beta. Free tiers only: Cloudflare (Free zone + Zero Trust), CloudFront Free plan, PostHog free (after legal), Resend (existing). |

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
*Ruled 2026-09-06 under the operator's delegation. Amend by appending; never rewrite a ruling silently.*
