# IRONICS Platform - ENGINEERING PLAN (128 tickets)

**Status:** PROPOSED 2026-09-07, partially executed (see A1). Built from the ruled decision log and the approved 16-board console designs.  
**Method:** 4 vendor fact-checks against primary sources -> 8 workstream planners -> integration -> 3 adversarial reviews (ordering, security, cost), all returning **sound-with-fixes**. Section A carries my corrections and governs the rest.  
**Totals as planned:** 128 tickets, 183.25 engineer-days, 20 marked destructive/irreversible.  
**Cost delta:** +$2-4/month now (3 new Secrets Manager entries ~$1.20-1.60, edge-log S3 ~$0.05, Athena/DynamoDB/Lambda cents; CloudFront egress $0 on the always-free 1 TB pay-as-you-go tier; Cloudflare Zero Trust $0 at 50 seats; second Worker $0). Rising to ~$10-12/month at full telemetry (alarms past the 10 free, press bucket, extra tables). Trigger-gated additions, each individually operator-approved and never simultaneous by default: CloudFront Pro +$15 (only past ~1 TB/month egress), Workers Paid +$5 (only if the console exceeds Free limits), Azure Trusted Signing +$10 (only at 100 approved testers). Worst case ~$35/month — inside the $50 cap in every week.

## A. Corrections before this plan is executed

Three adversarial reviews (ordering, security, cost) returned **sound-with-fixes** with seven blockers between them. The
plan below is the raw output; this section governs it.

### A1. Seven tickets are already DONE — the plan was written before the work

Executed and verified on the live stack on 2026-09-06/07, so re-baseline the schedule by roughly a week:

| Ticket | State |
|---|---|
| PLAT-A07 (CloudTrail + data events) | **DONE** — trail `ironics-management` logging, multi-region, validation on, data events on four tables |
| PLAT-A08 (deletionProtection + audit condition) | **DONE** — all six portal tables report `true`, verified live |
| PLAT-A09 (per-route reservedConcurrency) | **DONE** — 10 public / 5 admin, 180 of 900 reserved, verified live |
| PLAT-A15 (budget truth) | **DONE** — `ironics-monthly-50` excludes credits; the duplicate credit-inclusive budget still needs deleting |
| PLAT-A12 (CI token + single deploy path) | **DONE** — token replaced, CI green for the first time since 2026-08-25 |
| PLAT-B04 (Cloudflare Access) | **DONE** — Zero Trust team `ironics`, app on `admin.ironics.org`, two-operator policy, 1 h session, binding + httpOnly + SameSite=strict cookie |
| PLAT-F01 (09-08 hotfix) | **DONE** — VR undated, false Beta-Lands claims removed, real Discord link, true build rev; verified on the live page |
| PLAT-B05 (admin CORS origin) | **DONE** — shipped with the Phase 0 API deploy |

### A2. BLOCKER — the machine principal must not be able to assume the CDK deploy role

The plan gives `ironics-agent-deploy` `sts:AssumeRole` on the CDK bootstrap roles. Those roles carry
**AdministratorAccess** and trust the account root, so that grant is administrator-equivalent through `cdk deploy` and
re-creates the exact trust root the admin-scope review called a blocker. **Corrected:** the machine principal gets
`cdk synth` / `cdk diff` plus read-only statements and **no** assume-role on the bootstrap roles; every deploy goes
through the GitHub OIDC role behind required reviewers. A CloudTrail alarm watches `sts:AssumeRole` against those roles.

### A3. BLOCKER — the cost model was wrong, and it is the dominant line

Covered in decision log section 8: the server is Windows at $0.177/hour, so ~$129/month, not ~$62. Scheduled stop/start
around the play windows is adopted (~$31/month); a Linux server build enters the backlog as the largest structural
saving available (~$62/month alone, ~$15 combined).

### A4. BLOCKER — sixty engineer-days of console for a roster of one, ahead of the growth engine

Live footprint: one account, one admin, three audit rows, zero alarms. The plan spends weeks 5–16 on a sixteen-screen
console before the things that would create an estate to administer. **Corrected ordering**, without dropping anything
ruled: the events table, the `launched` and `first_match` emitters, and founder invites move to weeks 6–10, immediately
after the auth spine and **before** the console's read-only screens. Releases, Flags and Escrow move back. The build
order *within* the console is unchanged — that stays as ruled.

### A5. BLOCKER — the content cull's expensive half saves nothing a player can see

The cull audit is explicit: option A saves **zero download bytes**; it is a repository-size win of about 9.5 GB, and the
git history keeps the blobs regardless. Three weeks of critical path for that is misallocated while the beta is trying
to grow. **Corrected:** ship **option C alone** (the 4K texture cap) with 0.1.4-beta — about 2.5 days, and it is the only
half a player experiences. Option A moves to a named trigger: fresh-clone time blocking a second machine, or a quiet
window. Both remain approved; only their order changes.

### A6. BLOCKER — scheduling defects that would stall the programme

Four tickets sit on the critical path but in no week. Three key-split tickets are scheduled seven weeks *after* the
ticket that depends on them. A mockup round is unscheduled while two tickets that depend on its approval are not. The
download path goes live in week 5 with its cost alarm and its logging in week 23. All four are re-sequenced; the money
alarms in particular move to week 3, because the failures they watch for exist in the running system **today**.

### A7. MAJOR — security fixes folded in

Elevation is required on **every** admin route except the one the shell needs to bootstrap, not merely on mutating ones,
so a plain session cannot unmask personal data or export the audit log. The admin origin check moves into the Lambda,
because two subdomains of one registrable domain are same-site and CORS will not separate them. The console gets a
**stricter** header policy than the marketing site, since an injection there is an admin injection. The frozen kill
switch keeps one owner and one fail policy instead of two contradictory ones. The game-server money key ends with
**zero** portal holders, not merely fewer.

### A8. MAJOR — alarm and metric costs, and a polled health route

The design assumed the ten free CloudWatch alarms; the plan creates about twenty-nine plus nineteen custom metrics,
which is roughly $4.60/month rather than $0. Log-phrase filters collapse to one metric per domain with the phrase in the
alarm description. The health route stops calling metered APIs on every request and reads a row the five-minute roll-up
already writes, with the client polling at sixty seconds.

**Net effect:** roughly 183 engineer-days before corrections; the re-level pushes the horizon to about 27 weeks, and the
re-ordering front-loads the growth engine. Cost stays inside the cap for the platform; the server is the open question.

## B. Summary

One engineer (the agent) + operator, 8 workstreams integrated into a single lane. Order of attack: (1) the 09-08 copy hotfix and Phase 0 credential hygiene run in week 1 because they are cheap and remove live risk today; (2) the distribution move to CloudFront (pay-as-you-go always-free tier + OAC + key-group signed URLs) lands in weeks 4-5, killing the 15-minute presigned-link failure mode before any ramp; (3) the admin console spine (Zero Trust + apps/crm + roles + elevation + audit) is the priority build, weeks 5-11; (4) the content cull A+C ships with 0.1.4-beta alongside the distribution work; (5) the metrics spine (events table, roll-up, game-event hook) precedes growth engineering, which starts week 17. CORRECTIONS CARRIED INTO THE PLAN (flagged, not re-opened): D-DIST-1 conflates two CloudFront products — the flat-rate FREE plan is 100 GB/1 M requests with NO access logs (~25 downloads/month at 3.9 GB), while the 1 TB/10 M always-free allowance belongs to pay-as-you-go; Tier 0 therefore runs PAYG (still $0, logs still free to S3) and the ruled "$15 tier" becomes the CloudFront Pro flat plan behind a budget trigger. D12 read literally would put the admin HMAC key inside the public Epic-callback Lambda; a third read-only bagman/resolve/hmac is used instead (+$0.40/month). D4's host-only admin cookie cannot be set by a Worker's server-side fetch, so elevation is split into an Access-verified handoff code redeemed by the browser against api.ironics.org. Deduped across workstreams: C01/C05/C06 fold into B13/B14/B15/B16; C07/C08 fold into E04/E07/E08; C09's metrics route folds into E09 — 11 engineer-days removed, 183 remain. At ~5 productive days/week the full 128-ticket programme runs ~37 weeks; weeks 1-24 are scheduled below and the tail (economy Phase 3-4, growth Phase 3-4, signed installer) is trigger-gated rather than date-gated. Monthly cost delta stays $2-4 now, $10-12 at full telemetry, and never exceeds ~$35 even if CloudFront Pro ($15), Workers Paid ($5) and Azure signing ($10) all trigger — inside the $50 cap every week.

## C. Critical path

`PLAT-A01 -> PLAT-A02 -> PLAT-A03 -> PLAT-A12 -> PLAT-A13 -> PLAT-B01 -> PLAT-B02 -> PLAT-B03 -> PLAT-B04 -> PLAT-B05 -> PLAT-B06 -> PLAT-B07 -> PLAT-B08 -> PLAT-B10 -> PLAT-B11 -> PLAT-B12 -> PLAT-B13 -> PLAT-B14 -> PLAT-B15 -> PLAT-C02 -> PLAT-C03 -> PLAT-E04 -> PLAT-E07 -> PLAT-E09 -> PLAT-H02 -> PLAT-H03 -> PLAT-H04 -> PLAT-H12`

## D. Start immediately (no operator dependency)

`PLAT-A01` `PLAT-F01` `PLAT-A02` `PLAT-A03` `PLAT-G01` `PLAT-D01` `PLAT-A10` `PLAT-A08` `PLAT-A09` `PLAT-F03`

## E. Operator actions (25) - what only you can do

| Action | When | Blocks | How |
|---|---|---|---|
| Enrol an MFA device on IAM user Lead_Developer_Ironics | Week 1, after the agent confirms the ironics-agent-deploy principal works (~15 min) | PLAT-A05, PLAT-A15 | AWS console > IAM > Users > Lead_Developer_Ironics > Security credentials > Assign MFA device (authenticator app or hardware key); record the device ARN in the credentials runbook and confirm you can still reach the ROOT account (root MFA is already enabled and is the break-glass). |
| Add Zone > Workers Routes > Edit to CLOUDFLARE_API_TOKEN on zone ironics.org | Week 1 (~10 min) — this is the single most blocking operator item in the programme | PLAT-F02, PLAT-B03, PLAT-F07, PLAT-F08, PLAT-F09, PLAT-F11, PLAT-F12, PLAT-F13 | Cloudflare dashboard > My Profile > API Tokens > edit the token CI uses > add permission Zone · Workers Routes · Edit scoped to ironics.org. Tell the agent whether the token VALUE changed (if you mint a new token instead, the GitHub repository secret must be updated in the same sitting). |
| Enable Required reviewers + branch policy on the GitHub environments production and production-api | Week 1 (~10 min) | PLAT-A13, PLAT-B03 | GitHub > Settings > Environments > production and production-api > Required reviewers (yourself) + Deployment branches: main. Report whether the control is available on the current plan or asks for an upgrade — do not buy an upgrade silently. |
| Be present for the MFA deny proof and confirm the access-key soak | Week 2 | PLAT-A05 | Supply an MFA code for `aws sts get-session-token` so the agent can show the deny biting and then lifting; confirm in writing that 24h of Inactive has passed before ***REMOVED-AWS-KEY-ID (rotated; see Secrets Manager)*** is deleted. |
| Generate the CloudFront RSA-2048 signing key pair and hand over ONLY the public PEM | Week 2 | PLAT-D04 | On your own machine: `openssl genrsa -out cf-download-2026-09.key 2048` then `openssl rsa -in ... -pubout -out cf-download-2026-09.pub.pem`. Private half goes to the password manager and nowhere else. |
| Write the CloudFront private key into ironics/portal/cf-signing-key | Week 4, right after the distribution deploy | PLAT-D06 | `aws secretsmanager put-secret-value --secret-id ironics/portal/cf-signing-key --secret-string file://cf-key.json` with {keyPairId, privateKey}; shred the temp file after `describe-secret` shows a new AWSCURRENT version. The agent principal is denied Secrets write by design. |
| Move the three secret-bearing Downloads notes into Secrets Manager or a password manager | Week 4 | PLAT-A14 | Follow the runbook procedure under an MFA session; verify each destination with `describe-secret` BEFORE deleting the local file; report the LastChangedDate. The agent will not open those files. |
| Perform the signed-in portal download click for the CloudFront resume proof | Week 5 | PLAT-D06, PLAT-D07 | Sign in at ironics.org/portal, click Download, hand the minted URL to the agent (or run the throttled curl pause/resume yourself). Only your account is APPROVED, so there is no substitute. |
| Create the Cloudflare Zero Trust team and run the 30-minute Access compatibility test | Week 5 (~45 min) | PLAT-B04, PLAT-B10 | Create the team (attach a payment method — Free plan, $0), add the One-time-PIN IdP, turn on Independent MFA at org level with Security key + Authenticator app and a 1h duration, enrol both factors, then test on a throwaway hostname and screenshot both prompts. If the MFA tab is absent on this plan, add Google as the IdP instead and tell the agent. |
| Turn on Access for the ironics-crm Worker and hand over the AUD tag | Week 6, after the first crm deploy | PLAT-B04, PLAT-B10 | Workers & Pages > ironics-crm > Access > Protect this Worker (All traffic); then in Zero Trust author the Allow policy on the roster emails, 1h session, Custom MFA methods; copy the Application Audience (AUD) tag to the agent. |
| Confirm the owner account identity, then approve the watched role downgrade | Week 7 | PLAT-B07, PLAT-B08 | Sign in at ironics.org/portal, read the accountId on screen and confirm it equals 01M06WNV71GP2QWYPBQA5JT4WC before the last-owner guard is armed; later, approve a brief downgrade of that row to viewer (scripted restore) so a real refusal can be proven. |
| Perform the first elevation in person | Week 8 | PLAT-B10, PLAT-B13 | At admin.ironics.org, sign in through Access with the security key and press Elevate — this makes the one-time stepUpSubject bind to your real Access identity. Confirm the bound subject prefix against the Zero Trust log. |
| Provide a SECOND real IRONICS account for the identity proofs | Week 9, before PLAT-B13 | PLAT-B13, PLAT-C03, PLAT-C04 | Sign in once at ironics.org with a second email (this consumes Founder #0002 permanently — numbers are never recycled). It is the target for the grant/revoke, suspend/ban/reinstate and founder retire/reissue proofs; your own row is never the target. |
| Play/watch the cooked ARCANEON lap and give the 4K texture verdict | Week 13 | PLAT-G10, PLAT-G12 | Launch the staged Shipping client onto the cooked Shipping server, play an ARCANEON lap, and say whether the walls read acceptably at the 4K cap — accept, cap at 8192 instead, or ship the migration without the cap. |
| Approve and run the destructive content-pack removal | Week 14 | PLAT-G12, PLAT-G13 | After the cooked lap is green and the slice is pushed to the remote, run `git rm -r Content/DeepWaterStation Content/CyberPunkAssets`, commit, and later `git lfs prune --verify-remote`. Recovery depends on those LFS objects being on the remote, so the push must be verified first. |
| Run the shipped client once against the live S12 server | Week 15 | PLAT-E05 | Launch 0.1.3-beta (or 0.1.4) signed in with your Epic account so the presence heartbeat fires the first-seen hook; the agent asserts the single `launched` event row. |
| Approve the live release-pointer rehearsal and the kill-switch/freeze windows | Weeks 20 | PLAT-C11, PLAT-C12, PLAT-C13, PLAT-C14 | Watch the portal while the agent rolls beta 0.1.3 → 0.1.2.1 → 0.1.3, flips download.enabled off and on, and flips admin.frozen on and off — each reverted in the same sitting. A pointer or kill switch that has never been pulled is a claim, not a control. |
| Confirm the SNS alert destination and click every subscription link | Weeks 3, 22, 23 | PLAT-A07, PLAT-E03, PLAT-D11, PLAT-C15, PLAT-H12 | Give the email address (D18), confirm the AWS subscription emails, and confirm receipt of each fire-drill alarm (control-plane, money, download bytes). An unconfirmed subscription is a silent alarm. |
| Populate the new tentpole HMAC secrets | Week 22 | PLAT-E01, PLAT-E02, PLAT-E05, PLAT-E10 | `aws secretsmanager put-secret-value` with 32 bytes of random hex for bagman/admin/hmac, bagman/portal-inbound/hmac and bagman/resolve/hmac. The agent principal is denied Secrets write. |
| Engage counsel and return the approved legal v0.3 text | Week 24 onward | PLAT-F15, PLAT-H07 | Hand over LEGAL_V0_3_COUNSEL_PACK.md (cohort language, DOB retention, Volts/Watts naming, virtual-currency transferability vs the live staking loop, telemetry truth, chat/DM, enforcement, entity/governing law). Answer the two product questions inside it. |
| Run the two engine builds and play the match-emitter laps | Week 24 | PLAT-E06, PLAT-H12 | Editor closed, build LyraEditor and LyraServer from D:\UE5.6-source by explicit path, set AFL_MATCH_COMPLETE_URL on the S12 Scheduled Task, then play one PIE lap and one live lap to the end of an unstaked LeaguePlay match. |
| Create the IRONICS Discord server, application, bot and roles | Trigger: after the Privacy Notice v0.3 names the Discord user id | PLAT-H07, PLAT-H08 | Create the guild (G-COMM-1); create the Discord application; register the callback redirect; invite the bot with Manage Roles and place its role ABOVE Founder/Tester; write clientId/clientSecret/botToken/guildId/roleIds into ironics/portal/discord. |
| Approve the press kit publication and the press asset split | Trigger: growth Phase 3 | PLAT-H09, PLAT-H10, PLAT-H11 | Rule which reels are published (recommend 3), confirm the 4K originals may go to an S3 press bucket, supply or waive Simularent's approved descriptor, and confirm trademark clearance on IRONICS + the logo before a kit inviting mark reproduction ships. |
| Azure Trusted Signing onboarding (only at 100 approved testers) | Trigger: founders.issued >= 100 (W23) | PLAT-H13, PLAT-H14 | Create the subscription, submit business validation for C12 AI Gaming, create the account + certificate profile, grant the signing role to the build box, approve ~$10/month. If the entity is refused public trust, re-rule before any signing work starts. |
| Approve the CloudFront Pro flip if the download budget triggers | Trigger: sustained egress near 1 TB/month | PLAT-D14 | When the ironics-cloudfront budget crosses 80% of $10 (~1 TB/month), confirm the account is NOT on AWS Free Tier, then run ApprovePaidSubscription for the $15/month Pro plan. It commits the calendar month and cannot be cancelled mid-month. |

## F. Schedule (24 weeks as planned; see A1 and A6 for the re-baseline)

### Week 1 - 09-08 copy hotfix + Phase 0 baseline + scoped machine principal + cull baselines

**Tickets:** `PLAT-F01` `PLAT-A01` `PLAT-A02` `PLAT-A03` `PLAT-G01`  
**Proof gate:** Operator loads https://ironics.org/roadmap before 09-08 and watches the VR row undated, no REV 0.8.14, no dead Discord CTA; then watches `aws sts get-caller-identity` return ironics-agent-deploy and `cdk diff IronicsPortalStack` come back clean under that principal.

**Operator due:**
- MFA on Lead_Developer_Ironics
- Cloudflare token: add Zone > Workers Routes > Edit
- GitHub Required reviewers on production + production-api

### Week 2 - MFA deny + key retirement, budget truth, IP trust fix, CI becomes the only web deploy path

**Tickets:** `PLAT-A04` `PLAT-A05` `PLAT-A15` `PLAT-A10` `PLAT-F02` `PLAT-D01` `PLAT-D03`  
**Proof gate:** Operator watches the old human access key be refused (AccessDenied) and then work after `sts get-session-token` with an MFA code; a green deploy-web.yml run (no code 10000) serves ironics.org; and five portal page loads mint zero download URLs.

**Operator due:**
- Be present for the MFA session-token proof
- Confirm the 24h Inactive soak before key deletion
- Generate the CloudFront RSA-2048 key pair

### Week 3 - CloudTrail + DynamoDB data events, 8 control-plane alarms, table protection, reserved concurrency

**Tickets:** `PLAT-A06` `PLAT-A07` `PLAT-A08` `PLAT-A09`  
**Proof gate:** A benign PutItem from the agent principal appears in /aws/cloudtrail/ironics within 15 minutes and drives the SensitiveTableWriteByHuman alarm to ALARM with an email the operator confirms; `delete-table IronicsAuditLog` is refused by deletion protection.

**Operator due:**
- Confirm the SNS subscription and the fire-drill email

### Week 4 - CSP/HSTS on the site, secrets out of Downloads, publish hardening, CloudFront distribution stood up (no traffic)

**Tickets:** `PLAT-A11` `PLAT-A14` `PLAT-D02` `PLAT-D04` `PLAT-D05` `PLAT-G02`  
**Proof gate:** All 13 site routes load with zero CSP violations in the console and four security headers on curl; the new CloudFront domain returns 403 MissingKey on the release key and 403 on server-build/, while the live portal download still works unchanged.

**Operator due:**
- Move the three Downloads secret notes into Secrets Manager
- Write the CloudFront private key into ironics/portal/cf-signing-key

### Week 5 - Signed-URL cutover (the download failure mode dies) + Zero Trust compatibility check + shared tokens package

**Tickets:** `PLAT-D06` `PLAT-D07` `PLAT-B01` `PLAT-B02`  
**Proof gate:** WATCHED: a portal click mints a CloudFront URL, the download is killed at t+22 minutes and RESUMES with HTTP 206 to a matching sha256 — the 15-minute cliff is gone; and the operator sees the Cloudflare OTP + security-key prompt on a throwaway Access app.

**Operator due:**
- Create the Zero Trust team, OTP IdP, Independent MFA; run the 30-minute compatibility test
- Perform the signed-in download click for the resume proof

### Week 6 - apps/crm on admin.ironics.org behind Access, CORS admitted, nested AdminApiStack + capacity assertion

**Tickets:** `PLAT-B03` `PLAT-B04` `PLAT-B05` `PLAT-B06`  
**Proof gate:** A private window at https://admin.ironics.org demands OTP + security key before rendering; curl with Origin admin.ironics.org now gets access-control-allow-origin echoed back; cdk diff shows exactly one added nested stack and no route churn.

**Operator due:**
- Turn on Access for the ironics-crm Worker and hand over the AUD tag

### Week 7 - Break-glass + owner row migration, roles/permissions, frozen switch + fail-closed admin limiter, download kill switch + quota

**Tickets:** `PLAT-B07` `PLAT-B08` `PLAT-B09` `PLAT-D08`  
**Proof gate:** Operator watches their live row downgraded to viewer: /v1/admin/audit still answers 200 while the mint route returns a refusal byte-identical to anonymous — then restored, both transitions readable in IronicsAuditLog. download.enabled=false makes the portal refuse with no URL minted.

**Operator due:**
- Confirm on /portal that 01M06WNV... is your Epic-linked account
- Approve the watched role downgrade and the kill-switch rehearsal

### Week 8 - Elevation ceremony (Access handoff, 15-min host-only cookie, Bearer rejected) + console shell begins

**Tickets:** `PLAT-B10` `PLAT-B11`  
**Proof gate:** WATCHED in DevTools: Elevate returns Set-Cookie ironics_admin with Path=/v1/admin, SameSite=Strict and NO Domain; the badge counts down from 15:00; a valid game-session Bearer on /v1/admin/me returns 401.

**Operator due:**
- Perform the first elevation in person so the stepUpSubject binds to the real owner

### Week 9 - Console shell states, typed confirmations, admin-management routes

**Tickets:** `PLAT-B11` `PLAT-B12` `PLAT-B13`  
**Proof gate:** A grant/revoke of a second real account is performed from the console: the row changes in IronicsAdmins, refresh families are burned, and revoking the last owner is refused with the owner row provably unchanged.

**Operator due:**
- Provide or create a second IRONICS account for the grant/revoke proof

### Week 10 - Admins & Roles screen, audit hardening (GSIs, request metadata, CSV), Audit screen, runbook

**Tickets:** `PLAT-B14` `PLAT-B15` `PLAT-B16` `PLAT-B17`  
**Proof gate:** Side-by-side with the approved artboards: the roster and audit screens render live rows, and the API's actor-filtered audit answer matches a raw DynamoDB GSI query row for row including the 2026-08-25 bootstrap row.

### Week 11 - Accounts list + PII masking/reveal + account detail lifecycle actions

**Tickets:** `PLAT-C02` `PLAT-C03`  
**Proof gate:** Reveal on the operator's own row unmasks the email and writes an account.viewed audit row within one refresh; approve/suspend/reinstate/ban on the second account is watched changing status, burning the entitlement, and blocking that account's portal download.

**Operator due:**
- Approve suspending and banning the second test account on the live stack

### Week 12 - Founder ladder + cull migration (rename manager, 4K cap) — 0.1.4-beta build starts

**Tickets:** `PLAT-C04` `PLAT-G03` `PLAT-G04` `PLAT-G05`  
**Proof gate:** After an editor RESTART the ARCANEON venue renders with every migrated mesh carrying its real material and a byte grep over all 65 slice packages returns zero references to either content pack.

**Operator due:**
- Confirm the barrier Blueprints are re-pointed rather than deleted

### Week 13 - Cull cooks (client move, 4K cap, server) + PIE + cooked lap

**Tickets:** `PLAT-G06` `PLAT-G07` `PLAT-G08` `PLAT-G09` `PLAT-G10`  
**Proof gate:** WATCHED: the staged Shipping client boots, signs in with Epic and plays an ARCANEON lap on the cooked Shipping server with the venue intact — with both cooks green and zero pack rows in ReferencedSet.txt, AllChunksInfo.csv and the pak listing.

**Operator due:**
- Play/watch the cooked ARCANEON lap and give the 4K quality verdict

### Week 14 - Cull commit + confirm-first pack removal + 0.1.4 handoff; release ledger and roadmap model B

**Tickets:** `PLAT-G11` `PLAT-G12` `PLAT-G13` `PLAT-F03` `PLAT-F04`  
**Proof gate:** After `git rm` of both packs, BOTH Shipping cooks are re-run green and the final staged client is watched booting into ARCANEON; ~9.7 GB reclaimed in the working tree and the measured download delta published.

**Operator due:**
- Approve the destructive pack removal and run the git rm + push

### Week 15 - Metrics spine: events table, emit sink, missing funnel writers, playfab join + game-event hook

**Tickets:** `PLAT-E04` `PLAT-E05` `PLAT-F05` `PLAT-F06`  
**Proof gate:** Operator launches the shipped client once against the live server and a single `launched` row appears in IronicsEvents carrying accountId and no PII; a relaunch adds none.

**Operator due:**
- Run the shipped client once against the live S12 server
- Approve the /portal and /roadmap artboards

### Week 16 - Roll-up, testers/buildRev truth, metrics + health routes, Overview + Telemetry screens

**Tickets:** `PLAT-E07` `PLAT-E08` `PLAT-C09` `PLAT-C10` `PLAT-E09`  
**Proof gate:** Every Overview tile is checked against an independent CLI read in the same minute (buildRev vs latest.json, CCU vs /presence, alarms vs describe-alarms) and one source is deliberately broken to watch that tile print '—' rather than 0.

**Operator due:**
- Confirm whether GameLift Anywhere fleet-40c6b342 emits fleet metrics
- Rule whether TESTERS is published now (inflated ceiling) or held

### Week 17 - Roadmap model B live + copy truth pass (home, header/footer) + growth mockup round 1

**Tickets:** `PLAT-F07` `PLAT-F08` `PLAT-H01`  
**Proof gate:** The live /roadmap prints a REV equal to latest.json's version in the same minute, shows a real releases strip, and a CI test proves no NEXT/PLANNED row can carry a date; home shows 'Join the beta' and the beta-live line.

**Operator due:**
- Approve the founder-invite artboards; confirm N=3 and the issuance trigger

### Week 18 - Founder invites end to end (table, issuance, redeem, portal panel)

**Tickets:** `PLAT-H02` `PLAT-H03` `PLAT-H04`  
**Proof gate:** WATCHED: a share link from the operator's /portal panel is opened in a private window, carried through the magic link, and redeemed — the invite row flips to REDEEMED, a second attempt returns INVITE_ALREADY_USED, and the self-invite is refused.

### Week 19 - Portal + beta/founders copy truth, invites console panel, release admin routes

**Tickets:** `PLAT-F09` `PLAT-F11` `PLAT-H05` `PLAT-C11`  
**Proof gate:** Operator signs in and watches /portal with no Cohort row, FOUNDER I #0001, the beta-currency line and the 15-minute download copy; a revoked invite is then refused by the API with the revocation in the audit log.

**Operator due:**
- Sign in for the portal proof

### Week 20 - Releases screen + flags/kill-switch service + Flags screen

**Tickets:** `PLAT-C12` `PLAT-C13` `PLAT-C14`  
**Proof gate:** Operator watches the beta pointer rolled back to 0.1.2.1-beta and forward again with the portal following both moves and release.pointer audit rows for each; admin.frozen is flipped on and off from the console with reads still working while frozen.

**Operator due:**
- Approve the live pointer rehearsal and the freeze window

### Week 21 - Health alarms, remaining site copy pass, mailboxes + security.txt

**Tickets:** `PLAT-C15` `PLAT-F12` `PLAT-F13`  
**Proof gate:** Operator confirms an alarm email from a real 'admin refused' trigger (not just set-alarm-state), and sends one message to each of legal@/privacy@/press@/security@ ironics.org confirming all four arrive.

**Operator due:**
- Create security@, confirm the four mailboxes deliver, name the human behind security.txt

### Week 22 - Money observability: key split off the public callback, shared inbound verifier, money-line alarms

**Tickets:** `PLAT-E01` `PLAT-E02` `PLAT-E03`  
**Proof gate:** An Epic sign-in still writes playFabId after the key split, the Epic-callback role provably no longer reads bagman/earn/hmac, and each of the 8 money alarms is fired from a canary log line with the operator confirming the emails.

**Operator due:**
- Populate bagman/admin/hmac, bagman/portal-inbound/hmac, bagman/resolve/hmac
- Give the alert destination address

### Week 23 - Distribution telemetry: edge logs, Athena view, nightly snapshot, cost circuit breakers, lifecycle

**Tickets:** `PLAT-D09` `PLAT-D10` `PLAT-D11` `PLAT-D12`  
**Proof gate:** The exact rid from a real download is found in the Athena view with matching bytes and throughput, the nightly snapshot writes the roll-up row, and the operator confirms a test BytesDownloaded alarm email.

**Operator due:**
- Confirm the SNS subscription for the download alarms
- Confirm which releases stay hot before cold tags are written

### Week 24 - Legal v0.3 counsel pack + distribution runbook/key-rotation drill + first-match emitter (game code)

**Tickets:** `PLAT-F14` `PLAT-D15` `PLAT-E06`  
**Proof gate:** The key-rotation drill is watched live: a URL signed by the retired key returns 403 while a fresh one returns 200; and a LeaguePlay match played to the end writes exactly one first_match_completed row for the operator's account.

**Operator due:**
- Engage counsel with the v0.3 pack
- Run the two engine builds and play the PIE + live match laps
- Write key 2 into the signing secret during the drill

## G. All tickets (128)

### PLAT-A01 - Bank the pre-change Phase 0 security baseline as evidence

_Workstream A — Phase 0 hygiene + deploy path | phase 0 | 0.5 engineer-days | depends on: nothing_

**Files / resources:** `C:\Dev\Bag_Man\Docs\ops\PHASE0_SECURITY_BASELINE_2026-09-06.md`, `C:\Dev\Ironics-Platform\apps\api\scripts\security-baseline.ps1`

**Steps:**
1. Write security-baseline.ps1 running only read-only IAM/CloudTrail/CloudWatch/Budgets/Lambda/gh calls; forbid GetSecretValue.
1. Commit the output plus a RE-RUN-AFTER list naming which line each A-ticket flips.

**Proof:** The committed baseline holds the literal command outputs (MFADevices=[], one active key, 10 unrelated group policies, trailList=[], MetricAlarms=[], analyzers=[], quota 1000) produced by a watched run; the post-A15 re-run diff flips every expected line and nothing else.

### PLAT-A02 - Create the scoped machine principal ironics-agent-deploy

_Workstream A — Phase 0 hygiene + deploy path | phase 0 | 1 engineer-days | depends on: PLAT-A01_

**Files / resources:** `apps/api/iam/ironics-agent-deploy.policy.json`, `apps/api/scripts/create-agent-principal.ps1`

**Steps:**
1. Author the 7-statement policy (CDK bootstrap assume, SSM bootstrap version, releases bucket write-no-delete, CloudFront invalidate-only, table reads, ops reads, explicit Deny on IAM/KMS-sign/Secrets/CloudTrail/Lambda-code/budgets).
1. Create the user outside group IRONICS, store the key in a new `ironics-agent` profile, document the cfn-exec admin-equivalence limitation in the policy header.

**Proof:** Under the new profile: get-caller-identity names the user, `cdk diff IronicsPortalStack` prints no differences, head-object on latest.json works, and BOTH `iam create-user` and `secretsmanager get-secret-value` return AccessDenied — eight outputs pasted into the baseline.

### PLAT-A03 - Cut the agent and the release box over to the scoped principal; confirm CI needs no AWS key

_Workstream A — Phase 0 hygiene + deploy path | phase 0 | 1 engineer-days | depends on: PLAT-A02_

**Steps:**
1. Rename the human key block to ironics-human-legacy and make the agent key the default profile; re-run the full working set.
1. Record that deploy-api.yml already federates by OIDC into IronicsPortalDeploy, so no AWS key belongs in CI; start the credentials runbook with the 90-day rotation.

**Proof:** With no AWS_PROFILE set, get-caller-identity returns ironics-agent-deploy, cdk diff is clean and iam create-user is denied — watched in one session; a green api workflow run shows CI resolving to assumed-role/IronicsPortalDeploy.

### PLAT-A04 - OPERATOR: MFA device on Lead_Developer_Ironics

_Workstream A — Phase 0 hygiene + deploy path | phase 0 | 0.25 engineer-days | depends on: PLAT-A03_

**Steps:**
1. Agent prepares the console path and confirms the pre-state ([] devices).
1. Operator enrols the device and confirms root MFA still reachable.

**Proof:** list-mfa-devices returns a device with today's EnableDate and a console sign-out/in prompts for the code — watched.

**Operator:** Enrol MFA in the AWS console (~15 min), record the device ARN, confirm root MFA access still works.

### PLAT-A05 - Deny-all-unless-MFA on group IRONICS, strip 10 unrelated policies, retire the 86-day key

_Workstream A — Phase 0 hygiene + deploy path | phase 0 | 1 engineer-days | depends on: PLAT-A03, PLAT-A04_  
**DESTRUCTIVE / IRREVERSIBLE - confirm first**

**Steps:**
1. Attach the inline DenyAllUnlessMFA policy (carve-outs allow self-enrolment but NOT DeactivateMFADevice), detach the 10 unrelated managed policies by ARN keeping AdministratorAccess.
1. Set the key Inactive, soak 24h, delete it, remove the legacy profile, set an account password policy.

**Proof:** The legacy profile gets AccessDenied and then works after sts get-session-token with an MFA code; list-attached-group-policies returns exactly AdministratorAccess; list-access-keys is empty; the agent profile is unaffected — all watched in one session.

**Operator:** Be present for the session-token proof and confirm the 24h soak before deletion.

### PLAT-A06 - CloudTrail management trail + DynamoDB data events + Access Analyzer in CDK

_Workstream A — Phase 0 hygiene + deploy path | phase 0 | 1.5 engineer-days | depends on: PLAT-A03_

**Files / resources:** `apps/api/lib/security.ts`, `apps/api/test/security.test.ts`, `apps/api/test/capacity.test.ts`

**Steps:**
1. Add lib/security.ts: retained log bucket, log group, multi-region write-only trail with file validation, DynamoDB data events on Admins/AuditLog/Counters, one ACCOUNT analyzer.
1. Add security.test.ts and capacity.test.ts (Resources < 450, Parameters < 200); deploy through CI.

**Proof:** get-trail-status IsLogging true with a recent delivery; a benign PutItem from the agent principal is found in /aws/cloudtrail/ironics within 15 minutes naming userName=ironics-agent-deploy; one ACTIVE analyzer.

### PLAT-A07 - 8 control-plane metric filters + alarms + SNS to the operator

_Workstream A — Phase 0 hygiene + deploy path | phase 0 | 2 engineer-days | depends on: PLAT-A06_

**Steps:**
1. Create topic ironics-security-alerts with the operator email; add filters for KMS Sign by IAMUser, money-Lambda code change, secret read by human, table structure attack, sensitive table write by human, trail tampering, root usage, console login without MFA.
1. Alarm each at >=1 per 5 min, treatMissingData NOT_BREACHING; write the per-alarm response runbook.

**Proof:** The subscription is Confirmed; a real PutItem drives SensitiveTableWriteByHuman to ALARM with the operator confirming the email, then returns to OK; the ConsoleLoginWithoutMfa negative drill does NOT fire after A04.

**Operator:** Click the SNS confirmation link and confirm the fire-drill email arrival time.

### PLAT-A08 - deletionProtection on AuditLog/Accounts/Entitlements + append-only condition on approve-claim's audit write

_Workstream A — Phase 0 hygiene + deploy path | phase 0 | 1 engineer-days | depends on: PLAT-A03_

**Steps:**
1. Set deletionProtection on the three tables; add the attribute_not_exists ConditionExpression to approve-claim's bare PutItem with one ulid retry then a loud AUDIT WRITE LOST error.
1. Add tests: duplicate audit key refused; retry writes exactly one row; template arm requires deletion protection + PITR on every table but RateLimits.

**Proof:** describe-table shows DeletionProtectionEnabled true on all three and `delete-table IronicsAuditLog` is refused; one live claim approval produces exactly one audit row.

### PLAT-A09 - Per-route reservedConcurrency 5/10, delete the dead constant, fix the stale quota docs

_Workstream A — Phase 0 hygiene + deploy path | phase 0 | 1 engineer-days | depends on: PLAT-A03, PLAT-A08_

**Steps:**
1. Add reserved to RouteSpec, default admin 5 / public 10; delete PORTAL_RESERVED_CONCURRENCY; rewrite the '10 concurrency' comment and AWS-SETUP.md to the live 1000/900 reservable and void the -c reservedConcurrency=50 instruction.
1. Extend capacity.test.ts: every function 1..10, sum <= 450.

**Proof:** A scripted loop over get-function-concurrency shows 22/22 set (10 public, 5 admin), get-account-settings shows 820 unreserved, and /v1/stats plus an authenticated /v1/admin/me both still answer 200.

### PLAT-A10 - Stop trusting cf-connecting-ip on the un-proxied API

_Workstream A — Phase 0 hygiene + deploy path | phase 0 | 0.75 engineer-days | depends on: PLAT-A03_

**Steps:**
1. Gate clientIp on a new TRUST_EDGE_IP env (default false) so per-IP limits key on requestContext sourceIp; repair download/status tests that gave each account a distinct spoofed header.
1. Add http.test.ts covering trusted/untrusted/absent.

**Proof:** Twelve POSTs to /v1/auth/email/start with a DIFFERENT spoofed cf-connecting-ip each time now hit the per-IP limit at the 11th where all 12 previously returned 200.

### PLAT-A11 - CSP (nonce + strict-dynamic), HSTS, X-Frame-Options, Permissions-Policy on apps/web

_Workstream A — Phase 0 hygiene + deploy path | phase 0 | 2.5 engineer-days | depends on: PLAT-A12_

**Steps:**
1. Add static headers in next.config.mjs plus public/_headers for ASSETS-served files; add middleware.ts minting a per-request nonce with Web Crypto (no node:crypto — OpenNext has no Node middleware).
1. Ship report-only first, walk all 13 routes with the console open, then flip to enforcing in a second commit.

**Proof:** curl shows all four headers and a DIFFERENT nonce on two consecutive requests; all 13 routes are watched rendering identically with zero 'Refused to' console entries; the 180 KB CI budget stays green.

### PLAT-A12 - OPERATOR: Cloudflare token gains Zone > Workers Routes > Edit; CI becomes the only web deploy path

_Workstream A — Phase 0 hygiene + deploy path | phase 0 | 0.5 engineer-days | depends on: PLAT-A01_

**Steps:**
1. Run the probe workflow to capture the failing routes call, then re-run it after the operator's edit expecting 4/4 200s.
1. Re-run deploy-web.yml to green, retire the local cf:deploy script, close LEGAL_OPEN_ITEMS L4 with the run id.

**Proof:** A green web workflow run with no 'Authentication error [code: 10000]', followed by curl 200 on ironics.org serving that build.

**Operator:** Add Zone > Workers Routes > Edit on zone ironics.org to CLOUDFLARE_API_TOKEN; report whether the token value changed.

### PLAT-A13 - OPERATOR: Required reviewers on the production environments

_Workstream A — Phase 0 hygiene + deploy path | phase 0 | 0.5 engineer-days | depends on: PLAT-A12_

**Steps:**
1. Capture the before-state (production has no protection rules); operator enables Required reviewers + main-only branches on both environments.
1. Rewrite DEPLOYMENT.md/AWS-SETUP.md to one deploy path with a documented break-glass.

**Proof:** A push to main pauses the deploy job at 'Waiting for review' — watched — and completes green after approval; gh api shows the required_reviewers rule.

**Operator:** Enable Required reviewers + Selected branches: main on production and production-api; report if the plan gates it.

### PLAT-A14 - Secrets out of Downloads into Secrets Manager; rotation runbook

_Workstream A — Phase 0 hygiene + deploy path | phase 0 | 0.5 engineer-days | depends on: PLAT-A01_  
**DESTRUCTIVE / IRREVERSIBLE - confirm first**

**Steps:**
1. Agent writes the procedure without opening the notes; operator executes under an MFA session and verifies with describe-secret BEFORE deleting files.
1. Add the 90-day rotation table for every secret and the agent key.

**Proof:** describe-secret shows a LastChangedDate of the day it ran for each destination; a directory listing confirms the named files are gone; the rotation table carries real dates.

**Operator:** Move each secret note into Secrets Manager or a password manager, verify, then delete the local files.

### PLAT-A15 - Fix the $50 budget to measure GROSS spend; doc-truth pass

_Workstream A — Phase 0 hygiene + deploy path | phase 0 | 0.5 engineer-days | depends on: PLAT-A01_

**Steps:**
1. update-budget with IncludeCredit=false keeping name, limit and all three notifications/subscribers.
1. Correct the stale quota, the 21-vs-22 route count and the 'AWS not linked' README items; re-run the baseline and commit the diff.

**Proof:** describe-budgets shows IncludeCredit false and a non-zero ActualSpend matching Cost Explorer for the same period, with all three subscribers intact; the after-state baseline diff shows every Phase 0 line flipped.

**Operator:** Run the single update-budget call under MFA (the agent principal is denied budgets:ModifyBudget).

### PLAT-B01 - Zero Trust team + One-time-PIN IdP + Independent MFA compatibility check

_Workstream B — Console foundation + auth | phase 0 | 0.5 engineer-days | depends on: nothing_

**Steps:**
1. Operator creates the team, OTP IdP and org-level Independent MFA (security key + TOTP, 1h), enrols both factors and tests on a throwaway app.
1. Agent asserts the JWKS endpoint and records the team domain, AUD tag and plan; branches to Google IdP if the MFA tab is absent.

**Proof:** A private window on the throwaway hostname shows the OTP screen and then the security-key prompt before rendering — both screenshotted; the /cdn-cgi/access/certs endpoint returns a JWK set with at least one kid.

**Operator:** Create the Zero Trust team + OTP IdP + Independent MFA, run the 30-minute test, hand over team domain and AUD tag.

### PLAT-B02 - packages/tokens — lift brand tokens so two apps share one design system

_Workstream B — Console foundation + auth | phase 0 | 1 engineer-days | depends on: nothing_

**Steps:**
1. Add packages/* to workspaces; create @ironics/tokens from lib/tokens.ts plus the tailwind fragment and a console.css carrying the approved console additions.
1. Repoint the 14 web importers, delete the old module, add packages/tokens/** to the web CI path filter.

**Proof:** A green web CI run through the JS budget gate, after which ironics.org, /roadmap and /portal are byte-identical in token output to the pre-change capture and watched rendering unchanged — this refactor's proof is that nothing happened.

### PLAT-B03 - apps/crm scaffold: second OpenNext Worker on admin.ironics.org with its own CI

_Workstream B — Console foundation + auth | phase 1 | 2.5 engineer-days | depends on: PLAT-B02, PLAT-A12_

**Steps:**
1. Create apps/crm pinned to web's Next minor with wrangler.jsonc (custom_domain admin.ironics.org, no env blocks), a noindex placeholder and no data fetch.
1. Author deploy-crm.yml from deploy-web.yml with its own paths and concurrency, environment production.

**Proof:** A GREEN deploy-crm run; admin.ironics.org resolves to a Cloudflare proxied record where it did not exist, returns 200 over a valid cert with the crm marker, and ironics.org is still 200.

### PLAT-B04 - Cloudflare Access in front of the crm Worker + prove the assertion reaches the origin

_Workstream B — Console foundation + auth | phase 1 | 0.75 engineer-days | depends on: PLAT-B01, PLAT-B03_

**Steps:**
1. Operator protects the Worker (All traffic), authors the Allow policy with 1h session and Custom MFA, hands over the AUD tag.
1. Agent ships a temporary booleans-only whoami route, records which mechanism carries identity, then deletes it; falls back to a hostname app if the header is absent.

**Proof:** A private window at admin.ironics.org demands OTP + security key before rendering; the whoami route reports assertion:true with the right aud and iss; unauthenticated curl returns 302 to the Access login, not 200.

**Operator:** Turn on Access for ironics-crm, author the policy, hand over the AUD tag.

### PLAT-B05 - Admit https://admin.ironics.org on the portal API CORS allowlist

_Workstream B — Console foundation + auth | phase 1 | 0.5 engineer-days | depends on: PLAT-B03_

**Steps:**
1. Set allowOrigins to the exact two-origin array in CDK (API Gateway ignores integration CORS headers); keep allowCredentials true, no wildcard.
1. Add a template arm asserting both origins and no '*'.

**Proof:** curl with Origin admin.ironics.org now echoes that origin plus allow-credentials true (today it gets no access-control headers at all), the ironics.org origin still echoes its own, and OPTIONS returns 204 with the expected methods.

### PLAT-B06 - Nested AdminApiStack + synth capacity assertion + per-route concurrency wiring

_Workstream B — Console foundation + auth | phase 1 | 1.5 engineer-days | depends on: PLAT-B05_

**Steps:**
1. Add AdminApiStack; route home:'admin' functions and their log groups into it and wire routes with new apigw.HttpRoute in the nested scope (never api.addRoutes, which leaks 3 resources per route back to the parent).
1. Leave the 22 existing routes untouched; make grants.test.ts read both templates; add capacity.test.ts.

**Proof:** cdk diff shows exactly one added CloudFormation::Stack and no logical-id change to existing routes; after deploy the stack is UPDATE_COMPLETE, an admin function shows reserved 5 and a public one 10, and /v1/stats plus /v1/admin/me keep answering through the change.

### PLAT-B07 - Break-glass script + live IronicsAdmins row migration (roles, status, provenance)

_Workstream B — Console foundation + auth | phase 1 | 1 engineer-days | depends on: nothing_  
**DESTRUCTIVE / IRREVERSIBLE - confirm first**

**Steps:**
1. Write break-glass.ts (show/set-roles/bind-stepup/clear-stepup/freeze/unfreeze), dry-run by default, refusing --apply without an MFA session and always writing an audit row.
1. Operator confirms the accountId on /portal; migrate the row to roles={owner}, status=ACTIVE and record that the seed ran 2026-08-25.

**Proof:** get-item --consistent-read shows roles=[owner] status=ACTIVE with grantedAt/grantedBy intact, an admin.breakglass row naming the IAM actor is in IronicsAuditLog, and /v1/admin/me still answers 200 immediately after.

**Operator:** Read your accountId off /portal and confirm it before the apply; run the apply from an MFA session.

### PLAT-B08 - Roles as permission bundles, `requires` on RouteSpec, guard resolution, /v1/admin/me payload

_Workstream B — Console foundation + auth | phase 1 | 2.5 engineer-days | depends on: PLAT-B06, PLAT-B07_

**Steps:**
1. Add permissions.ts with the ten permissions and the three bundles taken from the approved artboard matrix; rewrite the guard to readAdminRow (revokedAt, non-ACTIVE status or empty roles = not an admin, ConsistentRead, no caching).
1. Set requires on all 8 live admin routes; return roles/permissions/status from /v1/admin/me; write the refusal arms first.

**Proof:** Live: /v1/admin/me returns roles=[owner] with all ten permissions; after a watched downgrade to viewer the audit read still returns 200 while the mint route returns 401 byte-identical to anonymous; restored, with both transitions in the audit log.

**Operator:** Approve the temporary downgrade of the live owner row (or supply a second signed-in account).

### PLAT-B09 - admin.frozen honoured + fail-CLOSED admin rate limiter

_Workstream B — Console foundation + auth | phase 1 | 1 engineer-days | depends on: PLAT-B08_

**Steps:**
1. Read the admin.frozen counter with ConsistentRead before every mutating admin route and refuse with a distinct FROZEN code (unreadable table also refuses); grant Counters read to admin routes from the flag, not a hand list.
1. Add consumeRateLimitStrict (fails closed) with 120/5min and 20/5min admin limits; add FROZEN and ELEVATION_REQUIRED error codes.

**Proof:** Live: freeze makes a mutating admin POST return 403 FROZEN while the audit GET still returns 200; unfreeze restores it, both transitions in the audit log; 25 mutating calls show 429 from the 21st.

**Operator:** Approve the watched freeze/unfreeze window.

### PLAT-B10 - Elevation ceremony: own KMS key, 15-min aud=ironics-admin token, host-only cookie, Bearer rejected, Access verified

_Workstream B — Console foundation + auth | phase 1 | 4 engineer-days | depends on: PLAT-B04, PLAT-B08_

**Steps:**
1. Add a SECOND KMS key (alias ironics-admin-elevation) with kms:Sign granted only to the elevate function; implement access-jwt.ts verifying by kid against the team JWKS with iss/aud/exp checks and kid-miss refresh.
1. Two-hop flow: the Access-protected Worker route exchanges the assertion for a single-use 60s code; the browser redeems it against api.ironics.org, which returns the host-only Set-Cookie. Bind stepUpSubject once, conditionally. Reject Authorization: Bearer on every admin route; require the elevation cookie on mutating routes only.

**Proof:** WATCHED in DevTools: Set-Cookie ironics_admin with Path=/v1/admin, Max-Age=900, SameSite=Strict and NO Domain; the badge counts down and a mutating call returns ELEVATION_REQUIRED after it lapses; a valid session Bearer on /v1/admin/me returns 401 byte-identical to anonymous; stepup_bound and elevated rows in the audit log.

**Operator:** Perform the first elevation in person so the stepUpSubject binds to the real owner.

### PLAT-B11 - Console shell, concealment gate, fetch layer, the eight States components

_Workstream B — Console foundation + auth | phase 1 | 3 engineer-days | depends on: PLAT-B02, PLAT-B04, PLAT-B05, PLAT-B08_

**Steps:**
1. Build the 56px bar + 216px rail exactly as the approved artboard with the ruled IA; port the server-side /v1/admin/me concealment (notFound for non-admins, no-store, force-dynamic).
1. Build FrozenBanner/RefusalBanner/ExpiredElevation/Success/StepUp/Loading/Empty/badges and a fetch layer mapping AUTH_REQUIRED, ELEVATION_REQUIRED and FROZEN to them; ship a placeholder page for every rail destination.

**Proof:** Side by side with the artboard at 1280x720: the shell renders with the live role badge and NOT ELEVATED; deleting the session cookie yields the 404 page rather than a redirect; a freeze makes the banner appear on every screen within one reload.

### PLAT-B12 - Confirmations component + server-side typed-confirmation contract

_Workstream B — Console foundation + auth | phase 1 | 1 engineer-days | depends on: PLAT-B10, PLAT-B11_

**Steps:**
1. Build the typed-confirm modal (disabled until the value matches, danger ghost for destructive, triggers step-up when unelevated).
1. Add confirm.ts server-side: a mutating admin route refuses with VALIDATION_FAILED unless body.confirm equals the subject the route computed.

**Proof:** Live: a revoke with the confirm field omitted or wrong returns 400 with NO audit row and the target row unchanged; with the correct typed subject it succeeds — both exercised from the UI, not just curl.

### PLAT-B13 - Admin-management routes: list/grant/revoke/roles with last-owner and self-revoke guards

_Workstream B — Console foundation + auth | phase 1 | 2.5 engineer-days | depends on: PLAT-B06, PLAT-B08, PLAT-B10, PLAT-B12_  
**DESTRUCTIVE / IRREVERSIBLE - confirm first**

**Steps:**
1. Add the four routes (home:'admin', requires admins:manage); grant refuses unknown accounts and non-owner owner-grants; revoke refuses self and the last ACTIVE owner via a counter ConditionExpression in the same transaction, and burns refresh families.
1. Extend the AuditAction union; write every audit row in the same TransactWriteItems as the change.

**Proof:** Live with a second real account: grant appears in the API and the table with an admin.granted row; revoke sets revokedAt, burns that account's refresh families and writes admin.revoked; self-revoke and last-owner revoke are both refused with the owner row provably unchanged.

**Operator:** Provide the second accountId and perform the grant/revoke from the console.

### PLAT-B14 - Settings > Admins & Roles screen wired to the live roster

_Workstream B — Console foundation + auth | phase 1 | 1.5 engineer-days | depends on: PLAT-B11, PLAT-B12, PLAT-B13_

**Steps:**
1. Build the roster table, grant panel and permission matrix rendered from the SAME permissions data the API enforces; last-owner row renders locked.
1. Wire revoke through the typed-accountId confirmation.

**Proof:** Side by side with the artboard: a grant made from the screen is confirmed independently by get-item and by the admin.granted audit row; under a viewer downgrade the screen shows the refusal banner and the network response carries no roster data.

### PLAT-B15 - Audit hardening: actor/subject GSIs, request metadata, filtered range read, owner CSV export

_Workstream B — Console foundation + auth | phase 1 | 2 engineer-days | depends on: PLAT-B06, PLAT-B08_

**Steps:**
1. Add actor-index and subject-index (two deploys — one GSI per UpdateTable) plus deletionProtection; extend AuditEntry with requestId/ip/ua/role/permission/stepUpAt supplied by the guard, storing ip only when proven proxied.
1. Add actor/subject/action/date-range filters with a cursor, and an owner-only CSV export that writes its own audit.exported row.

**Proof:** The API's actor-filtered answer equals a raw actor-index query row for row including the 2026-08-25 bootstrap row; describe-table shows both GSIs ACTIVE and deletion protection on; the CSV download has the same row count and its own audit row.

### PLAT-B16 - Audit screen with filters, metadata drawer and export

_Workstream B — Console foundation + auth | phase 1 | 1.25 engineer-days | depends on: PLAT-B11, PLAT-B15_

**Steps:**
1. Build the filter row, six-column list and click-through drawer per the artboard; footer states the true posture (append-only for application principals; tamper-evident via CloudTrail).
1. Wire the Overview 'recent audit' cross-link.

**Proof:** Rendered rows equal a raw DynamoDB day query row for row; a grant/revoke performed in another tab appears on reload with the correct role, permission and requestId in the drawer.

### PLAT-B17 - Doc truth, one-console supersession guard, and the admin runbook

_Workstream B — Console foundation + auth | phase 1 | 0.5 engineer-days | depends on: PLAT-B11, PLAT-B14_

**Steps:**
1. Add a CI guard failing on new files under apps/web/app/admin/**; record the D7/D1/D4 rulings in the scope docs and READMEs.
1. Write the runbook: Access/JWKS outage break-glass, freeze ceremony, stepUpSubject rebind, 6-week JWKS rotation, elevation triage order, OTP single-use caveats.

**Proof:** The CI guard is proven by adding a dummy admin file, watching the run go red, then reverting; and the break-glass freeze/unfreeze path is executed once live with both transitions read back from the audit log.

### PLAT-C01 - [MERGED into PLAT-B13/B14] Admins roster routes + screen

_Workstream C — Console screens | phase 1 | 0 engineer-days | depends on: PLAT-B13, PLAT-B14_

**Steps:**
1. Do NOT build twice: the roster routes, last-owner counter guard, self-revoke refusal, typed confirmation and the Admins screen are delivered by PLAT-B13 and PLAT-B14.
1. Carry forward only C01's atomic admin.owners counter design into B13's transaction.

**Proof:** Closed by PLAT-B13 and PLAT-B14's live proofs; no separate deliverable.

### PLAT-C02 - Accounts list: status/search routes, PII masking, reveal-with-audit, screen

_Workstream C — Console screens | phase 1 | 2.5 engineer-days | depends on: PLAT-B11, PLAT-B14_

**Steps:**
1. Query the existing status-index per status (never a Scan); exact-match search by ULID, email, epic subject and PlayFabId via one new playfab-index; add mask.ts and return playFabId from getAccount.
1. Put unmasking on a SEPARATE accounts:pii route that writes an account.viewed audit row; the list route holds no AuditLog grant.

**Proof:** The list renders real accounts masked; Reveal on the operator's row unmasks and an account.viewed row appears within one refresh; a viewer-role session is refused with no audit row written; each search form resolves to exactly one row.

### PLAT-C03 - Account detail + lifecycle actions over the existing library

_Workstream C — Console screens | phase 1 | 3 engineer-days | depends on: PLAT-C02_  
**DESTRUCTIVE / IRREVERSIBLE - confirm first**

**Steps:**
1. Build the detail route (account, application, entitlements, links) and one action route (approve/waitlist/suspend/ban/reinstate) reusing approveAccount and revokeAccount; add reinstate and waitlist writers.
1. Fence the approveAccount foot-gun: refuse approve unless the row is APPLICANT/WAITLIST without a founderNumber, so no number is burned before the conditional update.

**Proof:** On the second account, approve→suspend→reinstate→ban is watched with the status badge, the table row and a matching audit row after each; after the ban the founder entitlement is revoked, the counter is UNCHANGED, and that account's portal download is refused; pressing Approve on an approved row refuses without moving the counter.

**Operator:** Approve suspending and banning the second test account on the live stack.

### PLAT-C04 - Founder ladder read view + retire/reissue through the library

_Workstream C — Console screens | phase 1 | 1.5 engineer-days | depends on: PLAT-C03_  
**DESTRUCTIVE / IRREVERSIBLE - confirm first**

**Steps:**
1. Add entitlement-index and a read route sharing the tier arithmetic with /v1/stats; add retire/reissue as conditional updates on the entitlement row with a typed 4-digit confirmation.
1. Never touch the counter and offer no manual number assignment — assert it in grants.

**Proof:** The ladder's issued and tier-remaining figures equal /v1/stats in the same minute; on Founder #0002 retire then reissue is watched returning the SAME number while the counter reads 2 throughout.

**Operator:** Confirm Founder #0002 may be permanently spent by the proof.

### PLAT-C05 - [MERGED into PLAT-B15] Audit filters + request metadata

_Workstream C — Console screens | phase 1 | 0 engineer-days | depends on: PLAT-B15_

**Steps:**
1. Delivered by PLAT-B15 (metadata on every row, filtered range read, ip only when proven proxied).
1. Keep C05's guard-supplied AuditMeta signature change as B15's implementation detail.

**Proof:** Closed by PLAT-B15's live proof.

### PLAT-C06 - [MERGED into PLAT-B15] Audit GSIs + CSV export

_Workstream C — Console screens | phase 4 | 0 engineer-days | depends on: PLAT-B15_

**Steps:**
1. Delivered by PLAT-B15; the two GSIs still ship as two separate deploys (one GSI per UpdateTable).
1. Trigger note retained: the export is owner-only and becomes load-bearing at the first permanent second admin.

**Proof:** Closed by PLAT-B15's live proof.

### PLAT-C07 - [MERGED into PLAT-E07] Roll-up Lambda

_Workstream C — Console screens | phase 2 | 0 engineer-days | depends on: PLAT-E07_

**Steps:**
1. Delivered by PLAT-E04/E07 (IronicsEvents + the idempotent 5-minute recompute).
1. C07's per-account seen rows for D1/D7/D30 retention fold into E07's row set.

**Proof:** Closed by PLAT-E07's live proof.

### PLAT-C08 - [MERGED into PLAT-E08] testers writer + honest buildRev

_Workstream C — Console screens | phase 2 | 0 engineer-days | depends on: PLAT-E08_

**Steps:**
1. Delivered by PLAT-E08 (once-per-account tester counter on the click path, buildRev written by the publish script).
1. W8 rule retained: hide the field when null rather than rendering zero.

**Proof:** Closed by PLAT-E08's live proof.

### PLAT-C09 - GET /v1/admin/health (and the metrics route shared with E09)

_Workstream C — Console screens | phase 2 | 2 engineer-days | depends on: PLAT-E07, PLAT-E08_

**Steps:**
1. Build the health route: CloudWatch 5xx/errors metric math, DescribeAlarms, EC2 instance status, GameLift fleet, and 2s-timeout fetches of the tentpole's public /presence and /population; crashFree returns null.
1. Each source is individually try/caught with a per-source status; the metrics half is delivered by PLAT-E09 reading roll-up rows only.

**Proof:** The route's values match independent CLI reads taken in the same minute (instance status, alarm names/states, CCU, fleet state); breaking one source degrades that tile to 'unavailable' with the route still 200.

**Operator:** Confirm whether GameLift Anywhere fleet-40c6b342 emits fleet metrics.

### PLAT-C10 - Overview screen: five-tile strip, alarms panel, recent audit

_Workstream C — Console screens | phase 2 | 1.5 engineer-days | depends on: PLAT-C09, PLAT-B16, PLAT-B11_

**Steps:**
1. Build the tiles exactly as the artboard; any missing field renders '—' with the missing source named, never 0.
1. Poll at 30s pausing on hidden tabs; alarms panel renders each alarm's own description; inline SVG only.

**Proof:** Every tile is cross-checked against a second source in the same minute (buildRev vs latest.json, approved count vs the status-index query, CCU vs /presence, firing count vs describe-alarms); one source is deliberately broken and that tile is watched printing '—' while the page stays alive.

### PLAT-C11 - Release admin: pointer schema, channels.json, pointer-only routes, publish-script parity, ledger backfill

_Workstream C — Console screens | phase 2 | 3 engineer-days | depends on: PLAT-B10, PLAT-B15_  
**DESTRUCTIVE / IRREVERSIBLE - confirm first**

**Steps:**
1. Extend the pointer with minBuild and notes; add channels.json and one writer route (pointer/promote/rollback/min-build/notes) with typed version confirmation and audit rows; IAM allows PutObject on exactly latest.json and channels.json.
1. Rollback head-objects the target first and never deletes; add -MinBuild/-Notes/-Promote/-Rollback plus a post-upload size assert to publish-release.ps1.

**Proof:** minBuild and notes appear in latest.json and in the download response; beta is rolled back to 0.1.2.1-beta and forward again with the portal following both moves and release.pointer audit rows for each; a rollback to a missing key is refused with latest.json unchanged.

**Operator:** Approve the live pointer rehearsal window.

### PLAT-C12 - Releases screen with promote/rollback/minBuild/notes

_Workstream C — Console screens | phase 2 | 1.5 engineer-days | depends on: PLAT-C11, PLAT-B11_

**Steps:**
1. Build the versions table and channels card per the artboard; rollback is owner-only danger styling behind the typed-version confirmation.
1. Render role-aware controls from the live /v1/admin/me payload; the signing-key revoke panel renders disabled until Tier 0 exists.

**Proof:** The table's sha256 values match the committed manifests and the channel rows match channels.json; as an operator-role session the rollback and minBuild controls are absent AND a hand-crafted POST from that session is refused with no S3 write.

### PLAT-C13 - Flags & kill switches: one SSM parameter, fail-policy reader, GET/PUT routes, consumers

_Workstream C — Console screens | phase 2 | 3 engineer-days | depends on: PLAT-B10, PLAT-B15_  
**DESTRUCTIVE / IRREVERSIBLE - confirm first**

**Steps:**
1. One /ironics/portal/flags JSON parameter (not CloudFormation-managed) with a 30s TTL last-known-good reader; download.enabled fails CLOSED, apply.enabled fails OPEN, admin.frozen fails OPEN with a loud log.
1. Wire the consumers and the PUT route (owner for kill switches, typed confirmation, per-key before/after audit); the writer also owns the flexmatch-cells parameter with ssm:PutParameter scoped to exactly two ARNs.

**Proof:** Live: download.enabled=false makes the portal refuse within 30s with no presign and no download_started; manual_review leaves an application APPLICANT with no founder number; admin.frozen makes every mutating control refuse while reads work AND unfreeze works while frozen; every flip has a flag.changed audit row and revert-to-previous restores it.

**Operator:** Approve exercising the live download kill switch and admin.frozen in one reverted window.

### PLAT-C14 - Flags screen: tiers, before/after preview, typed confirm, revert-from-audit

_Workstream C — Console screens | phase 2 | 1.5 engineer-days | depends on: PLAT-C13, PLAT-B11_

**Steps:**
1. Build the runtime-flags and danger kill-switch panels per the artboard with current value, tier badge and last-changed from the audit log.
1. Add the before→after preview with typed confirm and a one-click revert that posts the audited previous value.

**Proof:** Displayed values match get-parameter in the same minute; an operator-role session sees the kill-switch panel read-only AND a hand-crafted PUT from it is refused with the parameter unchanged; revert writes a second flag.changed row.

### PLAT-C15 - Health alarms + SNS topic behind the Overview panel

_Workstream C — Console screens | phase 2 | 1.5 engineer-days | depends on: PLAT-C09_

**Steps:**
1. Create ironics-alerts and alarms for API 5xx ratio, Lambda errors, EC2 status, plus metric filters for 'admin refused', 'admin allowlist unreadable' and the flags reader failure.
1. Write each alarm a 3am-readable description (the Overview renders it verbatim); do not duplicate the CloudTrail alarms.

**Proof:** The operator confirms an alarm email from a REAL trigger (five refused admin calls incrementing the metric filter), not only from set-alarm-state, and the Overview panel flips that dot to firing within one poll and back to ok on reset.

**Operator:** Confirm the SNS email subscription and the alarm email.

### PLAT-D01 - Metadata-only GET /v1/download/latest/meta — stop minting a URL on page view

_Workstream D — Distribution | phase 0 | 1 engineer-days | depends on: nothing_

**Steps:**
1. Extract the pointer read into a shared module; add a meta handler with the same status ladder that signs nothing and emits nothing; grant S3 read on latest.json only.
1. Point DownloadCard's mount fetch at it; keep the click on /latest.

**Proof:** Five portal page loads produce zero /v1/download/latest calls in the network panel and zero download_started lines in the mint log group; one click produces exactly one of each; the meta response has no url key.

**Operator:** One signed-in portal pass (five loads plus one click) while the agent watches the logs.

### PLAT-D02 - Publish hardening: object headers, sidecars, size assert, hot/cold tag, ledger backfill

_Workstream D — Distribution | phase 0 | 1 engineer-days | depends on: nothing_

**Steps:**
1. Add content-type/disposition/cache-control and lifecycle=hot tagging to the zip upload, a post-upload head-object size assert that exits BEFORE latest.json is written, and manifest/.sha256 sidecar uploads; add a -Cool switch.
1. Self-test on a throwaway prefix including a deliberate size mismatch; commit the missing 0.1.1-0.1.3 ledger records.

**Proof:** head-object on the selftest object shows the three headers and lifecycle=hot; the truncated run exits non-zero with latest.json's LastModified unchanged; git log shows the four new ledger records.

### PLAT-D03 - OPERATOR: generate the CloudFront RSA-2048 key pair

_Workstream D — Distribution | phase 1 | 0.25 engineer-days | depends on: nothing_

**Steps:**
1. Operator runs openssl locally, keeps the private PEM in the password manager and hands over only the public PEM.
1. Agent commits the public PEM and adds a CI grep guard against any private key block.

**Proof:** openssl reports a 2048-bit public key on the committed PEM, a repo-wide grep for PRIVATE KEY returns nothing, and the operator confirms the private half is in the password manager.

**Operator:** Generate the key pair with openssl; hand over the public PEM only.

### PLAT-D04 - CDK: CloudFront distribution + OAC + public key + key group + empty signing secret

_Workstream D — Distribution | phase 1 | 1.5 engineer-days | depends on: PLAT-D03_

**Steps:**
1. Add the distribution with OAC, trusted key group, managed cache policy, no logging yet, plus the empty ironics/portal/cf-signing-key secret; narrow the OAC bucket policy to win64/* so server-build/ is unreachable.
1. Assert the shape in a template test and read cdk diff in full before deploying.

**Proof:** The distribution is Deployed with TrustedKeyGroups enabled; the release key returns 403 MissingKey, server-build/ returns 403, direct S3 returns 403; the bucket policy has one CloudFront statement scoped to win64/*; and the live portal download still works unchanged.

### PLAT-D05 - OPERATOR: write the private signing key into ironics/portal/cf-signing-key

_Workstream D — Distribution | phase 1 | 0.25 engineer-days | depends on: PLAT-D04_

**Steps:**
1. Agent supplies the deployed key-pair id; operator puts {keyPairId, privateKey} via file:// under their own identity and shreds the temp file.
1. Agent records populated-status only in AWS-SETUP.md.

**Proof:** describe-secret shows an AWSCURRENT version created today and the operator confirms the JSON has exactly the two keys with the matching key-pair id; functional proof is D06's 200.

**Operator:** Run put-secret-value with the private PEM (the agent principal is denied Secrets write).

### PLAT-D06 - Mint CloudFront signed 24h URLs behind a flip flag — dual-path zero-downtime cutover

_Workstream D — Distribution | phase 1 | 2 engineer-days | depends on: PLAT-D01, PLAT-D04, PLAT-D05_

**Steps:**
1. Add an injectable signer seam resolved from three env vars; when present mint a canned-policy 24h URL carrying acct hash + rid, otherwise fall back to today's presigned path; stamp rid and version on download_started.
1. Deploy once with the env vars ABSENT (identical behaviour), confirm green, then a second deploy adds them so only NEW clicks change.

**Proof:** THE WORKSTREAM PROOF, watched: a portal click returns a CloudFront URL with expiresIn 86400; a throttled download is killed at ~t+22 minutes and RESUMES with HTTP 206 and the right content-range to a byte count and sha256 matching latest.json; a tampered signature, an expired policy and the raw S3 URL all 403; a presigned URL minted before the cutover still completes after it.

**Operator:** Perform the signed-in click and hand over the URL for the pause/resume run.

### PLAT-D07 - Retire presigned S3: one signing path, narrowed grant, doc status flip

_Workstream D — Distribution | phase 1 | 0.5 engineer-days | depends on: PLAT-D06_

**Steps:**
1. After 24h and one completed CloudFront download, delete the presigned branch and dependency; make the signer required; narrow the S3 grant to latest.json only.
1. Document the `aws s3 presign` break-glass and flip the stale distribution-plan/BPD status lines.

**Proof:** No presigner reference remains, the function's S3 statement is scoped to latest.json alone, and a watched live click still downloads to completion with a matching sha256.

### PLAT-D08 - download.enabled kill switch (fails CLOSED) + per-account mint quota 3/day

_Workstream D — Distribution | phase 2 | 1.5 engineer-days | depends on: PLAT-D01, PLAT-D06_

**Steps:**
1. Create the flags parameter and a 60s last-known-good reader; refuse minting when the flag is false or was never readable; honour a stale-but-known true.
1. Add a 3/day per-account mint limiter (never on the meta route) and the two refusal states in the card copy.

**Proof:** Live: with the flag false the click shows the paused copy, returns 503 and writes no download_started; restored, it mints. A fourth click in a minute returns 429 with the rate-limit row showing count 3.

**Operator:** One signed-in session for the paused-state and four-click checks.

### PLAT-D09 - CloudFront standard logs v2 into a private log bucket

_Workstream D — Distribution | phase 2 | 1 engineer-days | depends on: PLAT-D04_

**Steps:**
1. Create a locked-down 30-day-lifecycle log bucket (it holds live signed URLs — treat it as a credential store) and the DeliverySource/Destination/Delivery trio in us-east-1 with Hive-partitioned paths.
1. Freeze the field list at creation (no cookies, no Parquet).

**Proof:** Within an hour of a real download a .gz object exists under the partitioned path whose row carries the same rid as the minted URL and a non-zero sc-bytes; the bucket shows all four public-access blocks, encryption and the 30-day rule.

### PLAT-D10 - Athena partition-projection table + downloads view + nightly snapshot

_Workstream D — Distribution | phase 2 | 2 engineer-days | depends on: PLAT-D06, PLAT-D09_

**Steps:**
1. Create the results bucket, workgroup, Glue table with partition projection and a v_downloads view computing per-rid bytes, elapsed, Mbps, country and completion, dropping c-ip.
1. Add a nightly snapshot Lambda writing dl#<version>#<date> roll-up rows so /admin never queries Athena live.

**Proof:** A query for the rid from the D06 proof returns exactly one row whose bytes, elapsed and throughput match the throttled run, scanning under 50 MB; the snapshot then writes a roll-up row with completed >= 1.

### PLAT-D11 - Cost circuit breakers: BytesDownloaded alarms, SNS, CloudFront-scoped budget, Pro-flip trigger

_Workstream D — Distribution | phase 2 | 0.75 engineer-days | depends on: PLAT-D04_

**Steps:**
1. Add three CloudFront alarms (6h/day bytes, hourly requests) to the alerts topic and a second $10 CloudFront-only budget that IS the documented Pro-flip trigger.
1. Write the response ladder into the runbook.

**Proof:** All three alarms exist in OK with the right dimensions and the subscription is Confirmed; a temporarily 1-byte threshold plus one download produces an alarm email the operator confirms, then the real threshold is restored by CI.

**Operator:** Confirm the SNS subscription and the test alarm email.

### PLAT-D12 - Version-retention lifecycle on the releases bucket

_Workstream D — Distribution | phase 2 | 0.75 engineer-days | depends on: PLAT-D02_  
**DESTRUCTIVE / IRREVERSIBLE - confirm first**

**Steps:**
1. Add three explicitly filtered rules: MPU abort, tag-scoped cold→Glacier IR at 30 days under win64/ only, and noncurrent expiry on latest.json; nothing may match server-build/.
1. Tag the current pair hot and the three superseded releases cold after reading the filters aloud.

**Proof:** get-bucket-lifecycle-configuration returns exactly three rules with every Filter read out as win64-bound; tags are correct on both hot and cold keys; a server-build binary is still STANDARD with no expiry header. T+31d: the cold key is GLACIER_IR while hot keys and server-build are untouched and the current release still downloads.

**Operator:** Confirm which releases stay hot before the cold tags are written.

### PLAT-D13 - Per-link revocation: CloudFront Function + KVS rid denylist, path guard, replay detector

_Workstream D — Distribution | phase 3 | 1.25 engineer-days | depends on: PLAT-D06, PLAT-D10_

**Steps:**
1. Verify KVS availability under PAYG at implementation time; add a tiny viewer-request function that 403s denylisted rids and any non-/win64/ URI, failing OPEN on error.
1. Extend the nightly query to flag rids seen from multiple ASNs or over-downloaded and write dlflag rows.

**Proof:** A still-valid signed URL returns 200/206, 403s within seconds after its rid is added to the store while a fresh mint for the same account still works, and returns 200 again when the key is deleted; a server-build path with a rid returns 403 from the function.

### PLAT-D14 - Pre-stage the $15 CloudFront Pro flip: WAF web ACL, subscription runbook, post-deploy survival check

_Workstream D — Distribution | phase 3 | 1 engineer-days | depends on: PLAT-D04, PLAT-D11_  
**DESTRUCTIVE / IRREVERSIBLE - confirm first**

**Steps:**
1. Trigger only when the CloudFront budget crosses 80% of $10; author the web ACL behind a CDK flag using individual managed rules plus a high rate rule, associate it, prove downloads still work, then create the subscription with MANUAL approval.
1. Keep ApprovePaidSubscription off the agent principal and record the month-commitment semantics.

**Proof:** The ACL is attached and a full 3.6 GB download still completes through it; after the operator's approval get-subscription shows PRO ACTIVE, and a subsequent cdk deploy is run and a SECOND get-subscription still shows ACTIVE.

**Operator:** Confirm the account is not on AWS Free Tier, then approve the $15 subscription (commits the calendar month).

### PLAT-D15 - Distribution runbook + key-rotation drill + doc truth

_Workstream D — Distribution | phase 2 | 0.75 engineer-days | depends on: PLAT-D06, PLAT-D08_  
**DESTRUCTIVE / IRREVERSIBLE - confirm first**

**Steps:**
1. Write the runbook (publish, cool, rotate, kill-switch ladder, break-glass presign, Athena recipes, credential-store warning, Pro flip) and EXECUTE the rotation once.
1. Fix the stale 'R2 upload' wording, mark the BPD boxes done, and append the CloudFront pricing correction to the decision log.

**Proof:** Watched minutes apart: after adding key 2 a fresh URL downloads 200/206, and after removing key 1 a key-1 URL returns 403 while a key-2 URL still returns 200.

**Operator:** Write key 2 into the signing secret during the drill and pick a quiet window.

### PLAT-D16 - DEFERRED-TRIGGER: dl.ironics.org custom domain in front of the distribution

_Workstream D — Distribution | phase 3 | 0.5 engineer-days | depends on: PLAT-D04, PLAT-D06_

**Steps:**
1. Only on trigger (branded host wanted, or the launcher needs signed cookies): operator gets an ACM cert in us-east-1 and adds a DNS-ONLY CNAME — never orange-cloud a 3.6 GB path.
1. Add the domain to the distribution and flip the signer host; old URLs keep working.

**Proof:** nslookup shows a grey-cloud CNAME to the distribution, the release key on dl.ironics.org returns 403 MissingKey over a valid cert, a fresh mint downloads to a matching sha256, and a pre-flip URL still completes.

**Operator:** Request/validate the ACM cert and add the DNS-only CNAME.

### PLAT-E01 - Tentpole shared inbound HMAC verifier + per-endpoint key allowlist + required actor

_Workstream E — Economy + metrics | phase 2 | 1 engineer-days | depends on: nothing_

**Steps:**
1. Write shared/inbound-auth.ts copying the existing constant-time verify verbatim, with an endpoint→allowed-key-ids map and a requireActor rule.
1. Create bagman/admin/hmac and bagman/portal-inbound/hmac empty; wire NO consumer in this deploy.

**Proof:** Both secrets exist, currency-earn's LastModified is unchanged, and a signed contract-valid /earn canary with amount 0 returns 400 (not 401), proving the money path is untouched.

**Operator:** Populate both new secrets with 32 bytes of random hex.

### PLAT-E02 - Drop the earn key from the public Epic-callback Lambda: dedicated read-only resolve key

_Workstream E — Economy + metrics | phase 2 | 1.5 engineer-days | depends on: PLAT-E01_

**Steps:**
1. Three-deploy cutover: tentpole accepts both keys on /resolve-identity, portal switches to the resolve key, tentpole drops the earn key and its grant.
1. Update the grants test so the earn key has ZERO non-mint holders.

**Proof:** An Epic sign-in still writes playFabId with a fresh timestamp (the resolve is non-fatal, so this must be asserted); afterwards the callback role's live policy has no GetSecretValue on the earn key and an earn-key-signed /resolve-identity returns 401.

**Operator:** Populate bagman/resolve/hmac.

### PLAT-E03 - Metric filters on the existing loud money log lines + 8 alarms to SNS

_Workstream E — Economy + metrics | phase 2 | 1 engineer-days | depends on: nothing_

**Steps:**
1. Add filters for IN DOUBT (escrow and reconcile), payout failed, REFUND FAILED, MIRROR FAILED (x2) and VOLTS MINT UNCONFIRMED across both stacks.
1. One alarm each to the money topic plus a shared Lambda-errors alarm; write the per-alarm response rows.

**Proof:** Each alarm is fired from a canary log stream carrying the exact phrase, driven to ALARM within ~5 minutes with the operator confirming the email, then reset — no money moves and no production stream is faked.

**Operator:** Give the destination address and confirm the alarm emails.

### PLAT-E04 - IronicsEvents table + emit() sink swap + the missing funnel writers

_Workstream E — Economy + metrics | phase 2 | 2 engineer-days | depends on: nothing_

**Steps:**
1. Add the events table (day / ts#id, account-index, 90-day TTL, accountId-only rows); make emit async writing both the log line and a row, never throwing; await it at all call sites.
1. Add the missing email_submitted writer and the 'launched' name.

**Proof:** A hand-run funnel (email, magic link, apply) produces email_submitted, email_verified, application_completed and approved rows with ~90-day TTLs and, asserted by filter, no email/dob/token attribute on any row — with the same lines still on stdout.

### PLAT-E05 - accounts playfab-index + inbound-only tentpole→portal game-event hook (launched)

_Workstream E — Economy + metrics | phase 2 | 2 engineer-days | depends on: PLAT-E01, PLAT-E04_

**Steps:**
1. Add the GSI and an inbound route whose only secret is the portal-inbound key; verify HMAC over the raw body with a 300s skew and enforce once-per-account with a conditional write.
1. Fire the hook from the presence heartbeat only on the first-seen transition, non-fatally.

**Proof:** The operator launches the shipped client once and exactly ONE launched row appears within 30s carrying accountId and no playFabId; a relaunch adds none; a bad-signature POST returns 401.

**Operator:** Run the shipped client once against the live S12 server.

### PLAT-E06 - first_match_completed for UNSTAKED matches: /match-complete endpoint + always-on emitter

_Workstream E — Economy + metrics | phase 2 | 3 engineer-days | depends on: PLAT-E05_

**Steps:**
1. Record the finding: a LeaguePlay match posts nothing at match end (settle is gated on bStaked, rating on isStaked). Add a tentpole /match-complete on the earn key with per-player first-match dedupe firing the portal hook.
1. Add the body builder and post in ReportMatchEnd AFTER validation and INDEPENDENT of bStaked, with a console canary and a spec arm proving the settle return value is unchanged.

**Proof:** A PIE listen-server match played to the end logs a 200 from /match-complete and produces exactly one first_match_completed row; a second match adds none; then the same lap is repeated once on the live S12 server with the shipped client.

**Operator:** Run the two engine builds, set the URL on the server task, and play the PIE + live laps.

### PLAT-E07 - Roll-up Lambda: daily funnel rows + per-account seen rows

_Workstream E — Economy + metrics | phase 2 | 2 engineer-days | depends on: PLAT-E04_

**Steps:**
1. Every 5 minutes recompute (never increment) today's and yesterday's counts per event, version and source plus the launched→first_match ratio, writing rollup#<date> rows and seen#<date>#<account> rows.
1. Add a synth capacity assertion for both templates.

**Proof:** The rollup row's per-event counts equal a hand COUNT query grouped by evt for the same day, and a forced re-run leaves the row byte-identical (idempotent recompute).

### PLAT-E08 - testers counter writer + honest buildRev on /v1/stats

_Workstream E — Economy + metrics | phase 2 | 1 engineer-days | depends on: PLAT-E04_

**Steps:**
1. On the CLICK path only, a conditional tester#<account> put plus an ADD on the testers counter, so a repeat click writes nothing.
1. Have publish-release.ps1 write a buildRev counter row alongside latest.json and have stats read it.

**Proof:** Live /v1/stats gains testers:1 and buildRev 0.1.3-beta (not the 0.8.14 fallback) after one operator click, and stays 1 after a second click, with the tester marker row present.

**Operator:** One download click from the live portal.

### PLAT-E09 - GET /v1/admin/metrics + Telemetry screen (Funnel + Downloads)

_Workstream E — Economy + metrics | phase 2 | 2.5 engineer-days | depends on: PLAT-E07, PLAT-E08, PLAT-B11_

**Steps:**
1. Add the metrics route reading roll-up rows only (never a live scan or Athena), granted counters+admins and nothing else.
1. Build the Telemetry screen on the artboard: 9-row funnel with inline SVG, the >=70% target line, Downloads by day and the honest 'edge data: not yet' card; keep client analytics off.

**Proof:** Every number on the screen equals the matching field of the roll-up row read from DynamoDB in the same minute; a session without metrics:read gets the standard refusal and the response carries no data.

### PLAT-E10 - Tentpole /admin-wallet read endpoint + playFabId GSI on match-escrow

_Workstream E — Economy + metrics | phase 3 | 2.5 engineer-days | depends on: PLAT-E01_

**Steps:**
1. Add the escrow playFabId GSI (there is no per-player access path today) and a POST-only, admin-key, actor-required, strictly READ-ONLY wallet endpoint returning balances, inventory, entitlements, open escrow and sales rows.
1. Grant read-only on those tables plus the PlayFab secret; no writes, no earn key.

**Proof:** A signed call with the operator's playFabId returns their real VO/WA balances cross-checked in game or PlayFab; the same body signed with the earn key returns 401 and without an actor returns 400; the escrow rows equal a direct GSI query.

### PLAT-E11 - Portal wallet route + Account detail Wallet & Entitlements panel

_Workstream E — Economy + metrics | phase 3 | 2 engineer-days | depends on: PLAT-E10_

**Steps:**
1. Fix getAccount to return playFabId; add a wallet route that resolves the id from the ACCOUNT ROW (never the caller), masks it without accounts:pii and writes a wallet.viewed audit row.
1. Build the panel per the artboard with the honest shadow-ledger footnote.

**Proof:** The operator's account detail shows the same VO/WA numbers as the E10 call, an audit query returns the wallet.viewed row naming actor, subject and requestId, and a viewer session sees the PlayFabId masked.

### PLAT-E12 - LITE shadow journal written by the tentpole functions that move money

_Workstream E — Economy + metrics | phase 3 | 2.5 engineer-days | depends on: PLAT-E01_

**Steps:**
1. Add bagman-money-journal (playFabId / ts#ulid, day and kind GSIs, PITR, deletion protection, an append-only resource-policy Deny) with an honest header naming PlayFab as balance of record and the dual-write gap.
1. Write rows from currency-earn and refund-purchase after the grant, never throwing; add an arm proving a journal failure leaves the 200 byte-identical.

**Proof:** A minimal canary earn returns 200 and exactly one journal row matches its delta, balance, ref, actor and nonce; on the live table an update-item is DENIED by the resource policy and delete-table is refused by deletion protection.

**Operator:** Approve the small canary grant or nominate a proof account.

### PLAT-E13 - PlayStream balance-change export to a private S3 bucket with Object Lock

_Workstream E — Economy + metrics | phase 3 | 1.5 engineer-days | depends on: nothing_  
**DESTRUCTIVE / IRREVERSIBLE - confirm first**

**Steps:**
1. Create the export bucket with Object Lock enabled AT CREATION and a Glacier IR transition; operator configures the PlayFab Data Connection (or the legacy Event Archive with a dedicated scoped IAM user).
1. Add a 'zero objects in 24h' alarm so a silently broken export is visible.

**Proof:** One canary balance change produces an object within ~10 minutes whose decompressed content names that playFabId and delta; get-object-retention returns a retain-until date and a delete-object with version-id is DENIED.

**Operator:** Configure the export in PlayFab Game Manager for title 1A2077 and choose GOVERNANCE vs COMPLIANCE retention.

### PLAT-E14 - Nightly drift reconciliation + KPI row + drift and stuck-state alarms

_Workstream E — Economy + metrics | phase 3 | 3 engineer-days | depends on: PLAT-E12, PLAT-E13_

**Steps:**
1. Add an EventBridge-only (no HTTP route) econ-daily Lambda writing the D19 KPI row from the journal's day index and comparing journal deltas against PlayFab balances.
1. Publish MoneyJournalDrift and StuckStates metrics with alarms, and persist reconcile's in-doubt report as rows.

**Proof:** The written KPI row equals a hand aggregation over the journal field by field; then a deliberate one-unit drift is injected, the metric goes to 1, the alarm fires with an email, and after the missing row is written the alarm returns to OK.

**Operator:** Approve the deliberate tiny drift injection as the alarm proof.

### PLAT-E15 - Economy > Payments screen re-cut into the console + the missing Record-payment form

_Workstream E — Economy + metrics | phase 3 | 2 engineer-days | depends on: PLAT-B11, PLAT-B12_

**Steps:**
1. Port Pending and Unattributed into apps/crm on the artboard, carrying the refusal map verbatim (MINT_UNCONFIRMED keeps retry:false — pressing again is how somebody gets paid twice).
1. Add the Record payment tab surfacing attributed/queued/duplicate distinctly; gate Approve on economy:mint plus a fresh elevation.

**Proof:** A real one-cent test payment appears in Unattributed; the identical externalRef submitted again reports 'already have it' with the table count unchanged; the payment.recorded audit row names the actor.

### PLAT-E16 - Economy > Catalog (read-only) + the missing audit row and dry-run on register-economy

_Workstream E — Economy + metrics | phase 3 | 1.5 engineer-days | depends on: PLAT-B11_

**Steps:**
1. Proxy the tentpole's public catalog read (no new secret) and render the artboard with editing deferred pending the pricing SSOT ruling.
1. Add an economy.registered audit row and an AuditLog write grant to the existing register route, plus a dryRun that makes no PlayFab call.

**Proof:** The screen's mint counts equal the mint-ledger rows; a dry run reports 'already present' without a catalog write; one real register writes the economy.registered audit row that has never existed for this route.

### PLAT-E17 - Economy > Escrow & KPIs read-only monitor and the 8 KPI tiles

_Workstream E — Economy + metrics | phase 3 | 2.5 engineer-days | depends on: PLAT-E14_

**Steps:**
1. Add a read-only admin-key tentpole endpoint returning the daily KPI row plus stuck rows (bounded scan at current volume with a stated GSI trigger), and two portal read routes.
1. Build the artboard's tiles, monitor table and reconciliation footer; ship NO actions — disabled with reasons.

**Proof:** Each of the 8 tiles equals the KPI row field for that day, the monitor lists exactly the rows a direct stuck-state query returns (an honest empty state today), and a route-table arm proves no mutating route exists under the escrow path.

### PLAT-E18 - Economy > Adjustments: manual credit/debit under the roster-of-one interim rule

_Workstream E — Economy + metrics | phase 3 | 4 engineer-days | depends on: PLAT-E10, PLAT-E12_  
**DESTRUCTIVE / IRREVERSIBLE - confirm first**

**Steps:**
1. Add the adjustments table and propose/approve/reject routes requiring wallet:adjust and a fresh elevation, with caps as CODE CONSTANTS (never SSM) and a server-enforced typed amount.
1. Ship the maker-checker condition gated on a live count of ACTIVE owners so four-eyes arms itself at the first second admin; the tentpole adjust writes a journal row and treats an unconfirmed outcome as terminal.

**Proof:** A one-unit adjustment is proposed and approved with the typed amount; the balance moves by exactly that in the wallet panel and in PlayFab, one journal row and two audit rows exist; a 200,000-unit proposal is refused server-side with no PlayFab call, a wrong typed amount is refused, and an expired elevation is refused.

**Operator:** Confirm the interim rule and perform the first real adjustment.

### PLAT-E19 - Align the rake and stake ladder to the staking SSOT (tiered 5/10%), PIE-proven

_Workstream E — Economy + metrics | phase 3 | 3 engineer-days | depends on: nothing_  
**DESTRUCTIVE / IRREVERSIBLE - confirm first**

**Steps:**
1. Replace the flat constant with a pool-size rule at the peg, passed through the existing planSettlement parameter; add the SSOT's worked example and 500/501 boundary arms keeping exact integer conservation.
1. Extend the Volts rung ladder and give the CLIENT payout preview the same tiered rule with matching spec cases — a preview that disagrees with the settle is a payout dispute.

**Proof:** A PIE lap on a staked match above the threshold shows the pre-match preview and the result card agreeing, and the settlement ledger row shows the 10% rake with awards plus rake exactly equal to the pot; a second lap under the threshold shows 5%.

**Operator:** Run the engine builds and the PIE lap; ratify the effective date (past settlements are not restated).

### PLAT-E20 - Economy > Holds: tentpole-enforced spend/stake/earn hold, fail-closed

_Workstream E — Economy + metrics | phase 4 | 3.5 engineer-days | depends on: PLAT-E10, PLAT-E12_

**Steps:**
1. Add a holds table and a ConsistentRead check BEFORE the PlayFab call in escrow-entry, purchase-bundle, counted redeem, refund and adjust, returning 423 with a refused journal row; an unreadable table refuses.
1. Add portal hold/unhold routes with mandatory reason, typed confirmation and audit; document the two real limits (native client purchase, in-flight staked match).

**Proof:** With a hold placed, a signed escrow entry returns 423, a refused journal row exists and the player's balance is provably unchanged; releasing the hold makes the identical call succeed; a denied hold-table read is proven to REFUSE rather than proceed.

### PLAT-E21 - Telemetry > Downloads: nightly edge-log snapshot into the roll-up rows

_Workstream E — Economy + metrics | phase 4 | 2 engineer-days | depends on: PLAT-E09, PLAT-D10_

**Steps:**
1. Consume the distribution workstream's Athena view; write a downloads roll-up row per day (minted, clicked, completed, failed, p50 Mbps, by country) into the same rows Telemetry already reads.
1. Replace the 'edge data: not yet' card with real figures keeping the data-source label; treat the log and results buckets as credential stores.

**Proof:** After one real download and the next snapshot run, the roll-up's completed count equals the Athena view's full-object deliveries for that rid, and the panel is watched rendering those numbers with the source label flipped.

### PLAT-F01 - 09-08 copy hotfix: undate VR, kill false Beta-Lands claims, remove the dead Discord CTA, stop printing REV 0.8.14

_Workstream F — Website content | phase 0 | 0.5 engineer-days | depends on: nothing_

**Steps:**
1. Apply the ruled VR wording and the Beta-Lands rewrite, drop the REV kicker and the Discord CTA, and hide the TESTERS/REV live-strip fields that render a label with no value.
1. Add a banned-string test and run it RED first; deploy via the one documented local exception, then re-assert after the CI run settles.

**Proof:** Before 09-08, curl on the live /roadmap returns zero matches for '7 Sep 2026', 'REV 0.8.14', 'discord.gg', 'crash telemetry', 'Founder invites' and 'balance pass' and one each for the ruled VR sentences; the home page has no dangling TESTERS/REV labels — screenshotted at 1280x720 and re-checked after CI.

### PLAT-F02 - Move web content deploys onto CI and close the local-wrangler exception

_Workstream F — Website content | phase 0 | 0.5 engineer-days | depends on: PLAT-F01, PLAT-A12_

**Steps:**
1. Re-run the token probe to 4/4 200s and deploy-web.yml to green on the hotfix commit.
1. Delete the local deploy instructions and close the open item with the run id.

**Proof:** A deploy-web run with conclusion success containing no 'code: 10000', with the live /roadmap still showing the hotfix content afterwards and the newest Worker version authored by the API-token principal.

### PLAT-F03 - Commit the 0.1.1-0.1.3 release ledger records and retire the placeholder-semver note

_Workstream F — Website content | phase 1 | 1 engineer-days | depends on: nothing_

**Steps:**
1. Copy the three existing manifests and sidecars into the repo; compute 0.1.1's records from the artifact itself (no memory notes) in the 0.1.0 key order.
1. Verify every committed hash and size against its zip; ratify 0.1.x-beta as the ruled scheme.

**Proof:** The live latest.json's version, sha256 and sizeBytes are byte-identical to the freshly committed 0.1.3 manifest, four Get-FileHash pairs match on screen, and git shows the 8 new files.

### PLAT-F04 - Mirror the release ledger into the web app and make the publish script write it

_Workstream F — Website content | phase 1 | 1 engineer-days | depends on: PLAT-F03_

**Steps:**
1. Add a generator producing a committed releases.json (CI cannot see the game repo) plus a hand-kept release-notes file, with a --check drift mode and tests.
1. Extend publish-release.ps1 to write the ledger and regenerate the mirror.

**Proof:** --check exits 0 and, in the same shell, the live latest.json's version/sha256/size equal releases.json[0]; a deliberately corrupted hash makes the test suite fail before the fix.

### PLAT-F05 - Mockup round 1: /portal as-built + proposed, including DownloadCard

_Workstream F — Website content | phase 1 | 1.5 engineer-days | depends on: nothing_

**Steps:**
1. Draw the as-built record that has never existed plus the proposed panel (no Cohort row, FOUNDER I with a 4-digit number, the beta-currency line) and the DownloadCard changes (single version designation, extract line, 15-minute validity).
1. Draw the loading/NO_RELEASE/error/terms-required states; terms copy marked v0.3 pending.

**Proof:** Recorded operator approval per artboard with the canvas URL — the doctrine-7 gate; no /portal code is touched before that line exists.

**Operator:** Review and approve or amend the four artboards.

### PLAT-F06 - Mockup round 2: /roadmap model B, home, shared header/footer, plus the v2 canvas correction

_Workstream F — Website content | phase 1 | 1.5 engineer-days | depends on: PLAT-F04_

**Steps:**
1. Draw the roadmap with the ruled badge vocabulary, a releases strip from real data, a known-issues line and an updated stamp; plus home (kicker removed, beta-live line, six houses) and the shared CTA relabel.
1. Correct the stale regions of the v2 design canvas before any code.

**Proof:** Recorded operator approval per artboard plus a committed diff showing the stale roadmap/build regions of the v2 canvas replaced.

**Operator:** Approve or amend; confirm the kicker removal and the Discord CTA decision.

### PLAT-F07 - Build roadmap model B: roadmap.json + drift CI check + rewritten page with releases strip

_Workstream F — Website content | phase 2 | 2 engineer-days | depends on: PLAT-F02, PLAT-F04, PLAT-F06_

**Steps:**
1. Author roadmap.json with sourceRef per row (HOLD rows never written), a typed loader with no new dependency, and the rewritten page; delete the old PHASES array so there is one source.
1. Add the drift guard: unique ids, valid enums, no year token in any NEXT/PLANNED row, REV equals the newest release — run it RED first.

**Proof:** On the live page the printed REV equals latest.json's version read in the same minute, the releases strip shows all four versions with 0.1.2/0.1.2.1 collapsed, a regex over the rendered HTML finds no date inside a NEXT/PLANNED row, and the deliberate red-then-green validator run is recorded.

### PLAT-F08 - Copy truth pass: home + shared header/footer

_Workstream F — Website content | phase 2 | 1.5 engineer-days | depends on: PLAT-F02, PLAT-F06_

**Steps:**
1. One shared 'Join the beta' CTA constant; correct the three-input claim; add the beta-live line and Portal link; fix the house/finish counts from the catalog; strip the twin/sync/livery promise; hide the draft Virtual Currency link off its inForce flag.
1. Extend the banned-string test and run it red first.

**Proof:** Live curl returns zero matches for every banned string and one each for the new ruled lines; the header CTA reads 'Join the beta' on every route and the footer has no virtual-currency href — watched at 1280 and 375.

### PLAT-F09 - Copy truth pass: /portal + DownloadCard

_Workstream F — Website content | phase 2 | 1 engineer-days | depends on: PLAT-F02, PLAT-F05_

**Steps:**
1. Delete the Cohort row and the review sentences; add the shared 4-digit founder formatter and FOUNDER I; add the beta-currency line; fix the doubled version and add the extract and 15-minute lines.
1. Ban 'cohort' and 'TIER I' in the copy test, red first.

**Proof:** Watched signed in: no Cohort row, FOUNDER I #0001, the currency line and the corrected card; a click starts a real download; signed-out curl returns zero matches for 'cohort'.

**Operator:** Sign in on the live portal for the watched proof.

### PLAT-F10 - Mockup round 3: /beta, /founders, /build, /houses, /market, /press, /creators

_Workstream F — Website content | phase 2 | 2 engineer-days | depends on: PLAT-F06_

**Steps:**
1. Draw the auto-approval /beta lede with a requirements line, the 4-digit founder ladder with reward claims removed, six catalog-backed build axes, the ruled house roster, a 'Preview — prices not final' market label, /press without the dead CTA, and both /creators variants.
1. Raise the sticker-series, roster and creators questions on the canvas.

**Proof:** Recorded operator approval per artboard plus written answers to the three questions.

**Operator:** Approve or amend; answer the sticker name, the seventh character row and the creators hold-vs-301 choice.

### PLAT-F11 - Copy truth pass: /beta + /founders

_Workstream F — Website content | phase 2 | 1.5 engineer-days | depends on: PLAT-F02, PLAT-F10_

**Steps:**
1. Rewrite the lede and post-submit copy to auto-approval truth; FIX the false DOB line (the API does store the date — evidence in applications.ts) to match the Privacy Notice and raise the retention question for counsel.
1. Trim the platform select to Windows, add the requirements line, apply 4-digit ranges and strip unearned tier rewards.

**Proof:** Live curl returns zero matches for the cohort/waitlist strings, the false DOB sentence and 'Meta Quest' outside the ruled sentence, and shows the requirements line and 'No. 0001–0100' — with both pages watched at 1280 and 375.

### PLAT-F12 - Copy truth pass: /build, /houses, /market label, /press + /creators dead CTAs

_Workstream F — Website content | phase 2 | 1.5 engineer-days | depends on: PLAT-F02, PLAT-F10_

**Steps:**
1. Six catalog-backed axes with the false render claim removed; the ruled house roster; the market preview label with the fixture wallet removed; delete both dead CTAs and the unbuilt creator perks.
1. Add a test banning a cta-classed href='#'; close the tracker's site-accuracy row.

**Proof:** Live curl returns zero href='#' CTAs on /press and /creators, 'Preview' and 'not final' on /market with no fixture balances, and no 'Eighteen'/'colour families' on /houses — five pages watched.

### PLAT-F13 - Mailboxes + security.txt + /legal index mechanics

_Workstream F — Website content | phase 2 | 1 engineer-days | depends on: PLAT-F02_

**Steps:**
1. Add an RFC 9116 security.txt route with a future Expires plus a test that fails when it lapses; verify it serves from the deployed Worker (fall back to a static asset if the dotted path 404s).
1. Give each legal doc its own version and effective date, add the disclosure row, and render the draft currency doc from its inForce flag.

**Proof:** Live security.txt returns 200 text/plain with a future Expires and the security address; /legal shows five distinct effective/version lines and the disclosure row; and the operator confirms on screen that test messages to all four mailboxes arrive.

**Operator:** Create security@, confirm all four mailboxes deliver, decide on a PGP key.

### PLAT-F14 - Legal v0.3 counsel pack: redlines plus evidence, ready for one counsel pass

_Workstream F — Website content | phase 3 | 2 engineer-days | depends on: PLAT-F09, PLAT-F11, PLAT-F12, PLAT-F13_

**Steps:**
1. Assemble one document quoting the exact live text, its file:line and the evidence it is false: cohort language, DOB retention, Volts/Watts naming, the currency-transfer clause against the live staking loop, the telemetry claim, chat/DM, the second-reviewer and in-game-report claims, founder revoke alignment, entity/governing law and terms-version drift.
1. Add every item to the open-items tracker with an owner and date.

**Proof:** A re-fetch step run the day the pack is sent asserts every quoted 'current text' appears verbatim in the live HTML, with the run output attached; handover date recorded.

**Operator:** Engage counsel and answer the two product questions inside the pack.

### PLAT-F15 - Publish legal v0.3 + versioned re-acceptance and the download terms gate

_Workstream F — Website content | phase 3 | 2.5 engineer-days | depends on: PLAT-F14_  
**DESTRUCTIVE / IRREVERSIBLE - confirm first**

**Steps:**
1. Land counsel's text per document, set one terms-version constant recorded and displayed from the same source, and add a TERMS_REQUIRED gate BEFORE the presign.
1. Add the portal re-acceptance banner and the card's terms state; deploy the API first, then the web; announce before flipping.

**Proof:** Watched with the operator: the legal page shows v0.3, /portal shows the re-acceptance banner and the download refuses with the terms state, status reports required=true, and after accepting, a download mint succeeds.

**Operator:** Return counsel's approved text, give the go-ahead, and watch the re-acceptance flow once.

### PLAT-F16 - Mockup: admin console Roadmap editor panel (model C)

_Workstream F — Website content | phase 4 | 0.5 engineer-days | depends on: PLAT-F07_

**Steps:**
1. Draw one artboard in the approved console shell: row list with lane/status/sourceRef, draft vs published, row editor, Publish behind a typed confirmation, audit strip, release rows read-only.
1. Draw the draft, publish-conflict and public last-good states.

**Proof:** Recorded operator approval with the canvas URL and the answer on which role may edit.

**Operator:** Approve and confirm the editing role.

### PLAT-F17 - Roadmap model C: table + public route + admin write routes + console panel + SSR page

_Workstream F — Website content | phase 4 | 3.5 engineer-days | depends on: PLAT-F07, PLAT-F16, PLAT-B13_

**Steps:**
1. Add the roadmap table seeded from the committed JSON, a cached public GET returning published rows only, and admin write/publish routes in the nested stack with typed confirmation and before/after audit rows.
1. Switch the page to SSR with the committed JSON as build-time last-good; have the publish script post the release row.

**Proof:** The public endpoint returns published rows with a cache header and no drafts; an edit published from the console appears on the live page within the revalidate window with a roadmap.published audit row; and with the API deliberately unreachable the page still renders last-good rather than a skeleton.

### PLAT-G01 - Bank the cull before-baselines and prove the pre-change cooks green

_Workstream G — Content cull | phase 0 | 1 engineer-days | depends on: nothing_

**Steps:**
1. Editor closed, clean tree: bank the client cook manifests, pak listing and the existing server tree snapshot BEFORE anything overwrites them.
1. Run the pre-change SERVER Shipping cook as the control and record the size ledger's before numbers.

**Proof:** Five banked baselines with asserted counts (60 pack rows in the referenced set and chunk info, 107 pak entries totalling 322,815,855 B) plus a control server cook logging Success - 0 error(s) with fresh binaries — the server lane proven green before anything moves.

### PLAT-G02 - Author the authoritative slice manifest and texture dimension census

_Workstream G — Content cull | phase 0 | 0.75 engineer-days | depends on: PLAT-G01_

**Steps:**
1. Boot the editor on the ARCANEON map and walk the transitive hard dependency closure; union it with the 60-package cook set and explain any difference (a texture the cook stripped would strand a material after removal).
1. Census dimensions and derive the 4K cap list mechanically (normal/ORM over 4096 only).

**Proof:** A manifest with 65 rows carrying measured dimensions read from the live editor, an empty or fully explained set-difference against the cook closure, and a cap list containing only oversized normal/ORM maps.

### PLAT-G03 - Migrate the slice with the rename MANAGER and byte-verify the bindings survive a restart

_Workstream G — Content cull | phase 1 | 1.5 engineer-days | depends on: PLAT-G02_

**Steps:**
1. Move all 65 packages in ONE rename_assets call with the world loaded (never rename_asset, which stripped 310 texture bindings on 2026-08-31); save once and confirm redirectors.
1. Byte-verify every material references the new path and none the old; RESTART the editor and open every material; repoint the dev art-pass scripts.

**Proof:** On a restarted editor the pit walls, container, railings, supports, back wall, barrels, cardboards, car and helicopter all render with real materials, the log has zero missing-texture or failed-compile lines naming either pack, and a byte grep over all 65 packages returns zero old-path references.

### PLAT-G04 - Byte audit: zero external references into either pack, zero redirectors

_Workstream G — Content cull | phase 1 | 0.5 engineer-days | depends on: PLAT-G03_

**Steps:**
1. Editor closed, run the reference audit with the MSYS path guard (a leading-slash pattern silently returns false zeros) over Content/Plugins/Config/Source.
1. Assert only the ini lines, the editor-only label asset and the vendor-path false positive remain; assert zero redirectors in the destination and referencer folders; do NOT run a global fixup commandlet.

**Proof:** Both ripgrep commands pasted verbatim with output: the audit returns only the allowed non-driving hits and zero map or Blueprint hits, and the redirector scan returns zero files.

### PLAT-G05 - Land the root NeverCook guards on both pack roots

_Workstream G — Content cull | phase 1 | 0.25 engineer-days | depends on: PLAT-G04_

**Steps:**
1. Append the two root NeverCook lines with a dated comment citing the audit and the slice.
1. Assert the new slice path appears in no AlwaysCook, MapsToCook or scan-directory entry — it must cook only because the map reaches it.

**Proof:** The config diff shows exactly the two added lines plus the comment and the grep proves the slice is pulled by reference only; safety itself is proven by the cooks in G06/G08.

### PLAT-G06 - Client Shipping cook #1 — prove the move alone is byte-identical

_Workstream G — Content cull | phase 1 | 1 engineer-days | depends on: PLAT-G05_

**Steps:**
1. Run the exact 0.1.3 invocation; take the verdict from the Result line, fresh mtimes and a zero-hit grep for every missing-from-cook phrasing naming the packs or the venue.
1. Assert the cooked tree, referenced set, chunk info, cooked-package byte grep and pak listing all show zero pack hits; diff the manifest and run both lint gates.

**Proof:** Success - 0 error(s) with a full cook; 60/60/60/3/107 pack hits in the baseline become 0 everywhere; and the manifest diff shows 167 rows relocated to the slice folder at IDENTICAL byte sizes — which is what proves the move changed only paths.

### PLAT-G07 - Apply the 4K cap to the slice's normal/ORM maps and cook #2 to measure the delta

_Workstream G — Content cull | phase 1 | 1 engineer-days | depends on: PLAT-G06_

**Steps:**
1. Set the per-asset max texture size on the manifest's capped rows (per-asset, not the device-profile LOD group, which would miss these virtual textures and hit the whole project).
1. Re-cook and MEASURE: every non-capped row byte-identical, every capped bulk file strictly smaller; record the loose and pak-compressed deltas, replacing the estimate.

**Proof:** A manifest diff in which only the capped normal/ORM bulk rows changed and all shrank, the measured deltas written into the size ledger, and a second green full cook with zero pack hits everywhere.

### PLAT-G08 - Dedicated-server Shipping cook — prove the S12/GameLift lane

_Workstream G — Content cull | phase 1 | 0.75 engineer-days | depends on: PLAT-G07_

**Steps:**
1. Run the server Shipping cook and apply the same verdict and assertion set to the server tree.
1. Diff against the G01 control manifest and run the provenance verifier on the staged server root.

**Proof:** Success - 0 error(s) with fresh server binaries, zero pack rows in the server referenced set, chunk info and cooked-package grep, a clean diff against the control, and provenance exit 0.

### PLAT-G09 - PIE verification on ARCANEON + collateral sweep

_Workstream G — Content cull | phase 1 | 0.5 engineer-days | depends on: PLAT-G07_

**Steps:**
1. PIE listen server + 2 clients on the venue; watch every migrated mesh render with its real material and assert zero linker/missing-object lines naming the packs.
1. Open the four other roster maps and both barrier Blueprints.

**Proof:** Watched in PIE: the venue renders identically to pre-change with no grey or checkerboard, the log is clean, the four collateral maps open without errors and both barrier Blueprints compile; 4K wall screenshots captured for the operator verdict.

### PLAT-G10 - Cooked ARCANEON lap on the staged Shipping client + cooked server — the checkmark

_Workstream G — Content cull | phase 1 | 0.75 engineer-days | depends on: PLAT-G08, PLAT-G09_

**Steps:**
1. Provenance-verify the staged client, bring up the cooked Shipping server, and run boot → Epic sign-in → venue showcase → ARCANEON playlist → join.
1. Take the operator's 4K verdict and record the lap.

**Proof:** WATCHED on the cooked stack: the staged client plays an ARCANEON lap on the cooked server with every migrated mesh carrying its real material and no missing geometry, with provenance exit 0 beforehand and the operator confirming the 4K quality.

**Operator:** Play or watch the cooked lap and give the 4K verdict.

### PLAT-G11 - Commit the build-side change and reconcile the documents

_Workstream G — Content cull | phase 2 | 0.5 engineer-days | depends on: PLAT-G10_

**Steps:**
1. Stage the slice, referencers, config, repointed scripts and manifest; bank the three new cook manifests and update the README baseline table.
1. APPEND an execution record to the audit and update the plan/tracker rows; operator commits and pushes.

**Proof:** The commit exists with a clean tree afterwards, the 65 slice objects are LFS-tracked, and the foreground push completes with the objects verified on the remote before anything is removed.

**Operator:** Run the commit and confirm the LFS push completed rather than timed out.

### PLAT-G12 - DESTRUCTIVE: git rm the two packs, re-cook client+server, re-verify, prune

_Workstream G — Content cull | phase 2 | 1.25 engineer-days | depends on: PLAT-G11_  
**DESTRUCTIVE / IRREVERSIBLE - confirm first**

**Steps:**
1. Gate on the watched lap, the verified push, a clean tree and a closed editor; operator runs the git rm and commits; re-save the editor-only label asset so its dangling paths drop.
1. Re-run BOTH Shipping cooks on the removed tree (a deleted directory is a different cook input from a NeverCook'd one), re-run the full assertion set, then prune locally with remote verification.

**Proof:** Neither pack directory exists, the byte audit returns only the two config lines, BOTH cooks return Success - 0 error(s) with zero pack rows anywhere, and the final staged client is WATCHED booting into an intact ARCANEON lap; ~9.7 GB working-tree reclaim reported from disk.

**Operator:** Approve the removal explicitly, then run the git rm, commit and prune.

### PLAT-G13 - Size ledger before/after and 0.1.4-beta artifact handoff

_Workstream G — Content cull | phase 2 | 0.5 engineer-days | depends on: PLAT-G12_

**Steps:**
1. Publish all six measured numbers from disk and state the honest attribution: the migration saves zero download bytes (the reachable packages moved, they did not leave) and the whole download win is the 4K cap; the repo win is the migration's alone.
1. Hand the provenance-verified staged root, the Explorer-safe zip, sha256 and committed manifest to the distribution workstream; append the measured result under the ruling.

**Proof:** A committed ledger whose every row is read off disk, provenance exit 0 on the staged root, and the zip's entry list read back through the shell showing real child entries (not an Explorer-empty archive) before handoff.

### PLAT-H01 - Growth mockup round 1: founder invites (/portal panel, /beta redeem, console Invites)

_Workstream H — Growth | phase 1 | 1 engineer-days | depends on: nothing_

**Steps:**
1. Draw four artboards reusing the approved shell and chips: the portal invite panel with per-slot states, the signed-in redeem field with every refusal, the anonymous invited acknowledgement, and the console roster with owner-only revoke.
1. Copy rules: no cohort vocabulary, 4-digit numbers, never print an invitee's identity, never claim the code grants access while auto-approval stands.

**Proof:** Recorded operator approval for all four artboards appended to the decision log; nothing is deployed.

**Operator:** Approve or amend; confirm N=3 and whether issuance waits for a first download click.

### PLAT-H02 - IronicsInvites table + idempotent issuance on approval + GET /v1/invites

_Workstream H — Growth | phase 2 | 2.5 engineer-days | depends on: PLAT-H01_

**Steps:**
1. Add the table (owner/slot key, code GSI, deletion protection) and an ensureInvites that converges on exactly N rows via per-slot conditional writes.
1. Call it after approval OUTSIDE the transaction (a code collision must never cancel an approval or burn a founder number) and self-heal on first read.

**Proof:** The founder account's /v1/invites returns exactly 3 UNUSED codes — watched in the network panel — a raw query returns the same 3 rows and still 3 after three more calls, and a fresh throwaway approval also yields exactly 3.

### PLAT-H03 - Redeem an invite on apply: attribution, self-invite and double-spend refusals, rate limits

_Workstream H — Growth | phase 2 | 2 engineer-days | depends on: PLAT-H02_

**Steps:**
1. Validate after the bot check and before creating the application; consume with a base-table conditional write (never the GSI read) so double-spend is impossible; consume best-effort so a lost race never fails an existing application.
1. Attribute via utm_source and an invited flag — the locked funnel event names do not change; add per-account and per-IP redeem limits and an invite-only constant defaulting false.

**Proof:** Live: a valid redeem flips the row to REDEEMED with the redeemer recorded; the same code from another account returns INVITE_ALREADY_USED leaving the row untouched; the founder's own code is refused; the completion log line carries the founder-invite source; and an apply with no code behaves identically to today.

### PLAT-H04 - Web: portal invite panel, beta redeem field, share-link capture across the magic link

_Workstream H — Growth | phase 2 | 2.5 engineer-days | depends on: PLAT-H01, PLAT-H02, PLAT-H03_

**Steps:**
1. Add a session-storage handoff copying the existing DOB pattern so ?invite= survives the magic link, including a hidden input for the no-JS path.
1. Fetch invites in PARALLEL with status on /portal, render the approved panel with copy-to-share, and mint a fresh idempotency key whenever the code field changes.

**Proof:** WATCHED end to end: a copied share link opened in a private window shows the invited acknowledgement, completes the magic-link round trip and submits with the code attached — the invite row flips to REDEEMED and the panel shows that slot redeemed on reload; the JS budget stays green.

### PLAT-H05 - Console Invites roster + owner-only revoke with audit

_Workstream H — Growth | phase 2 | 1.5 engineer-days | depends on: PLAT-H01, PLAT-H03, PLAT-B13_

**Steps:**
1. Add both routes in the single route array marked home:'admin' and constructed in the nested scope; revoke is a conditional update requiring UNUSED so a redemption can never be erased.
1. Add invite.revoked to the audit vocabulary and write the row in the same transaction; player redemption stays off the operator audit log.

**Proof:** A revoked code posted to apply returns INVITE_REVOKED, the row shows REVOKED with the revoking owner, the invite.revoked row is in the audit tab, and a viewer session gets the byte-identical refusal a non-admin gets.

### PLAT-H06 - Growth mockup round 2: portal Discord panel + Telemetry weekly-cohort block

_Workstream H — Growth | phase 2 | 0.5 engineer-days | depends on: PLAT-H01_

**Steps:**
1. Draw the Discord link panel states naming exactly what is stored and linking the Privacy Notice, and the weekly-cohort block by ISO week against the >=70% target with an explicit 'no data yet' per step.
1. Reuse the existing panel, chip and bar primitives.

**Proof:** Recorded operator approval for both artboards appended to the decision log.

**Operator:** Approve or amend; confirm the digest recipient and the cohort key.

### PLAT-H07 - Discord application secret + account linking (OAuth), shipped flag-off

_Workstream H — Growth | phase 3 | 3 engineer-days | depends on: PLAT-H06, PLAT-F15_

**Steps:**
1. Add an empty discord secret and an OAuth start/callback pair modelled on the Epic flow, storing the link as a provider subject so one Discord identity can never attach to two accounts.
1. Ship behind a kill switch defaulting OFF so the route can exist before the Privacy Notice names the Discord user id.

**Proof:** With the flag on for one session the operator links from /portal and returns showing Linked — watched; the provider-link row resolves to their account; a second account attempting the same identity is refused with the existing conflict copy.

**Operator:** Create the server, application, bot and roles; write the credentials into the secret; land the Privacy Notice v0.3 wording first.

### PLAT-H08 - Discord role sync: approved gains Founder, revoked loses it, hourly reconciler + alerts

_Workstream H — Growth | phase 3 | 3 engineer-days | depends on: PLAT-H07_  
**DESTRUCTIVE / IRREVERSIBLE - confirm first**

**Steps:**
1. Add role add/remove honouring rate-limit retry-after and never retrying a 403; sync inline from the callback and stamp the account.
1. Add a bounded hourly reconciler over the approved status index plus a mandatory dry-run cycle and a failure alarm on the alerts topic.

**Proof:** WATCHED in the live guild: a linked approved account gains the Founder role inline and a second gains it within one reconciler cycle; after a revoke the role is gone on the next run, with the API 204s in the logs and the stamp set then cleared; the dry-run log is attached as pre-flight evidence.

**Operator:** Place the bot role above Founder/Tester, confirm Manage Roles, approve the first live cycle.

### PLAT-H09 - Press assets pipeline: reels, mono mark, venue art, and where they live

_Workstream H — Growth | phase 3 | 2 engineer-days | depends on: nothing_  
**DESTRUCTIVE / IRREVERSIBLE - confirm first**

**Steps:**
1. Record provenance and hashes for the approved reels; transcode to faststart H.264 under the 25 MB ceiling with posters; export the real mono mark and venue key art.
1. Split storage: only what the site serves goes in the repo (which has no LFS); originals go to a locked-down press bucket. Write the fact sheet from settled facts only.

**Proof:** git shows exactly the intended set committed, ffprobe asserts each published reel's size, faststart and duration, the bucket lists the originals with matching hashes, and the before/after repo size is recorded.

**Operator:** Rule which reels are published and approve the originals leaving the machine.

### PLAT-H10 - /press mockup round

_Workstream H — Growth | phase 3 | 1 engineer-days | depends on: PLAT-H09_

**Steps:**
1. One bundled round: kit grid on the real assets, fact-sheet block, reels with posters, restored kit CTA, mailto contact, plus the embargo/empty state.
1. Bake in the corrected sponsor, house, finish and mark-usage copy.

**Proof:** Recorded operator approval appended to the decision log.

**Operator:** Approve or amend; supply or waive the sponsor descriptor; confirm press@ routing.

### PLAT-H11 - Rebuild /press and publish the press kit

_Workstream H — Growth | phase 3 | 2 engineer-days | depends on: PLAT-H10_  
**DESTRUCTIVE / IRREVERSIBLE - confirm first**

**Steps:**
1. Rewrite the page to the artboard, replace the dead CTA with the real kit URL, add per-page OG, and lazy-load reels behind posters.
1. Build the zip naming every child entry explicitly (an archive built from '.' shows EMPTY in Explorer — the 0.1.2 incident) and verify with the shell API; verify the current asset size limit before choosing where it is served.

**Proof:** Live /press and the kit URL both return 200 with the expected size; the downloaded kit opens on Windows with every named child visible and non-empty; zero dead CTAs remain; each reel is watched playing from its poster.

**Operator:** Final go for public publication (brand assets, trademark clearance, sponsor line).

### PLAT-H12 - Weekly cohort report: roll-up, admin route, Telemetry block, email digest

_Workstream H — Growth | phase 3 | 3 engineer-days | depends on: PLAT-H03, PLAT-H06, PLAT-E07, PLAT-E06_

**Steps:**
1. Key cohorts by ISO week of approval; aggregate approved, honest download click, install, first match, the ratio against the target and a source breakdown including founder-invite; write one row per week.
1. Add the admin read route, the Telemetry block with inline SVG, and a plain-text digest to the alerts topic; any step without a writer prints n/a, never 0.

**Proof:** A forced run writes the row, the SCHEDULED Monday run is then observed firing on its own, the route and panel render it, the operator confirms the digest email, and one cohort is reconciled by hand against a control query and matches exactly.

**Operator:** Confirm the digest recipient and the tester definition.

### PLAT-H13 - Azure Trusted Signing onboarding + toolchain dry run (gated at 100 approved testers)

_Workstream H — Growth | phase 4 | 1 engineer-days | depends on: nothing_

**Steps:**
1. Assert the 100-tester trigger on the live stack before any spend; write the operator onboarding checklist and VERIFY every requirement, price and timeline against current vendor documentation at execution time.
1. Prove the toolchain on a throwaway executable and record per-signature wall time and confirmed cost.

**Proof:** Signature status on the throwaway binary is Valid with the entity as signer — output pasted — plus a portal screenshot of the certificate profile; no IRONICS release is touched.

**Operator:** Create the subscription, pass business validation, create the profile, grant the build-box role, approve the spend.

### PLAT-H14 - Sign the shipped build in the packaging path and tell the truth about it on the site

_Workstream H — Growth | phase 4 | 2.5 engineer-days | depends on: PLAT-H13_  
**DESTRUCTIVE / IRREVERSIBLE - confirm first**

**Steps:**
1. Order the path provenance → sign (timestamped) → zip → hash → publish, with the publish script refusing to upload unless the signature validates, so the published hash is the signed artifact's.
1. Rewrite the unsigned warning, the roadmap row and the known-issues line, and commit the signed release's ledger record; publish to the version key first, verify, then move the pointer.

**Proof:** On a CLEAN Windows profile the downloaded build's signature status is Valid with the entity as signer and the launch behaviour is screenshotted exactly as observed (never claimed as 'SmartScreen is gone'); the published object's hash matches the signed manifest.

**Operator:** Run the build, package, sign and publish, and perform the clean-profile download test.

## H. Risks

| Risk | Mitigation |
|---|---|
| D-DIST-1's cost model is wrong: the flat-rate CloudFront FREE plan gives 100 GB + 1 M requests (~25 downloads of a 3.9 GB build) and includes NO access logs, which would make the ruled edge telemetry impossible. The 1 TB/10 M always-free allowance belongs to pay-as-you-go. | Tier 0 runs on pay-as-you-go with OAC + key-group signed URLs (still $0 at current volume, standard logs v2 to S3 still free to deliver). The Pro plan ($15) is pre-staged behind a dedicated $10 CloudFront budget alarm that fires at ~1 TB/month. Amendment appended to the decision log in PLAT-D15. |
| D12 read literally puts bagman/admin/hmac — the key that signs /admin-wallet and /admin-adjust — inside the public unauthenticated Epic-callback Lambda, a WIDER blast radius than the earn key it replaces. | PLAT-E02 uses a third, read-only bagman/resolve/hmac for /resolve-identity (the alternative the scope itself names), satisfying D12's intent (zero portal holders of the game-server earn key) for +$0.40/month. Flagged, not re-opened. |
| D4's host-only admin cookie on api.ironics.org cannot be delivered to the browser by a Worker's server-side fetch, so the elevate flow as described would leave the admin cookie in the Worker. | PLAT-B10 splits the ceremony: the Access-protected Worker route exchanges the Cf-Access-Jwt-Assertion for a single-use 60-second code server-side, and the browser redeems that code directly against api.ironics.org — which is the response that carries the host-only Set-Cookie. Both D3 and D4 stay intact. |
| PLAT-A05 (deny-all-unless-MFA + access-key retirement) is the highest lockout risk in the programme. | Two independent recovery paths are confirmed before it runs: root MFA is already enabled, and ironics-agent-deploy sits outside group IRONICS. The deny is an inline group policy removable in one call from a root session, and A02/A03's eight-point proof must be recorded first. |
| Least privilege is bounded at the CDK bootstrap: CDKToolkit was bootstrapped with empty CloudFormationExecutionPolicies, so cdk-hnb659fds-cfn-exec-role carries AdministratorAccess and the agent principal is admin-equivalent through `cdk deploy`. | Stated openly in the policy file rather than papered over; the path is DETECTED by CloudTrail + the 8 control-plane alarms (PLAT-A06/A07). Re-bootstrapping with scoped execution policies is a separate confirm-first ticket, not assumed. |
| Two writers on the web Worker (local wrangler + red-but-shipping CI) can leave stale bytes on ironics.org. | PLAT-F01 is the one documented local-deploy exception and re-asserts the live page AFTER the CI run settles; PLAT-F02 closes the exception the day the token permission lands and deletes the local deploy instructions. |
| The content-pack removal (PLAT-G12) is irreversible in the working tree and recovery depends on the slice's LFS objects being on the remote. | Hard gate order: watched cooked lap green → commit → verified foreground LFS push → operator approval → git rm. The rename MANAGER (never rename_asset) plus a byte-verified editor restart protects against the 2026-08-31 texture-binding stripping incident. |
| CI, tests and cook logs can all be green while the live thing is broken (the 0.1.2 'Explorer-empty zip' and 'REV 0.8.14' classes of failure). | Every ticket's proof is a live-stack assertion or a watched behaviour, never a build result: cook verdicts are the Result line + fresh mtimes + a manifest diff; zips are verified with Shell.Application; every console number is cross-checked against an independent CLI read in the same minute; any metric with no writer prints '—', never 0. |
| An admin surface built before roles exist would put revoke, mint and kill-switch levers behind a flat allowlist. | Strict order: break-glass + owner-row migration (B07) → roles/requires (B08) → frozen + fail-closed limiter (B09) → elevation (B10) → shell (B11) → mutating routes (B13+). No mutating console route ships before its `requires` and a fresh-elevation check. |
| The North Star metric (installed → first match >= 70%) has no server signal: unstaked LeaguePlay matches post nothing at match end, and download_started counts page renders. | PLAT-D01 splits the metadata-only endpoint in week 2 (before any counter is published), and PLAT-E06 adds an always-on /match-complete emitter independent of bStaked. Until both land, every affected figure renders 'n/a' rather than a fabricated denominator. |
| The 16 GB build box plus six Shipping cooks (cull) and one esbuild bundle per Lambda route make elapsed time, not engineering, the binding constraint in weeks 12-14. | UBA disabled and -MaxParallelActions=2 on every compiling step; cooks sequenced overnight; only the lap and the removal approval need the operator awake. Reserved concurrency capped at 5/10 per function with a synth assertion (sum <= 450) so the portal never starves the tentpole's money Lambdas. |
| Publishing legal v0.3 (PLAT-F15) invalidates every prior terms acceptance and gates downloads fail-CLOSED — a bug in termsAreCurrent locks every tester out of the build. | Deploy the API gate first and prove the accept path on the single live account before any announcement; keep the previous deploy id for rollback; announce to the roster before flipping. |
| The $50/month cap could be breached if CloudFront Pro, Workers Paid and Azure signing all trigger in the same month. | Each is trigger-gated and individually approved: Pro only past ~1 TB/month egress, Workers Paid only if the console measurably exceeds Free CPU/request limits, signing only at 100 approved testers. Worst case ~$35/month; the gross-spend budget alarm (PLAT-A15, IncludeCredit=false) fires at $50 regardless. |
| Alarm fatigue: the 'sensitive table write by a human' filter fires on legitimate operator and break-glass work. | The runbook states which alarms have expected true positives and what containment looks like for each, so nobody mutes them; alarm count is kept at or under the 10-alarm free tier where possible. |

## I. Adversarial reviews (kept in full - section A answers these)

### Security regressions: does any ticket weaken the posture the admin-scope adversarial review demanded (Bearer rejection, host-only cookie, stepUpSubject, key split incl. /resolve-identity, inbound-only hook key, fail-closed limiter, CloudTrail alarms, CFN cap)? Does the CloudFront move keep the API un-proxied and the bucket private?

**Verdict:** sound-with-fixes — the plan preserves most of what the review demanded (host-only Strict cookie with NO Domain and a watched DevTools proof in B10; a second KMS alias for the elevation audience; stepUpSubject bound once and conditionally; fail-CLOSED consumeRateLimitStrict in B09; deletionProtection + append-only ConditionExpression in A08; nested AdminApiStack with HttpRoute built in the nested scope plus a synth capacity assertion in B06; per-route reservedConcurrency 5/10 with a sum guard in A09; TRUST_EDGE_IP default false in A10 so cf-connecting-ip is no longer trusted on the un-proxied API; PII masking on a separate accounts:pii route with an account.viewed row in C02). The CloudFront move is architecturally clean on the two questions asked: nothing proxies api.ironics.org (D16 is explicitly DNS-only and deferred, and A10 removes the spoofable edge-IP trust), and the bucket stays private — Block Public Access is BLOCK_ALL today (verified live) and D04 keeps OAC + trusted key group with the policy scoped to win64/* so the server-build/ prefix in the same bucket (verified: server-build/WindowsServer/.../LyraServer-Win64-Shipping.exe) is unreachable, with D07 narrowing the S3 grant to latest.json alone. But three demanded controls are silently weakened (elevation required on mutating routes only; admin.frozen flipped from fail-closed to fail-open at C13; no Origin check while both origins sit in the credentialed CORS allowlist), one is declared done without being done (the earn-key split stops at EpicCallbackFn), and Phase 0 replaces one unguarded admin-equivalent key with another while claiming CloudTrail covers it. Fix findings 1-6 before the console holds live data; the rest are cheap test/config arms.

**[BLOCKER] PLAT-A02 / PLAT-A03 / PLAT-A05 / PLAT-A07 (Phase 0 credential hygiene)**  
Problem: Phase 0 retires the human key behind deny-all-unless-MFA but creates ironics-agent-deploy OUTSIDE group IRONICS with a long-lived key and 'CDK bootstrap assume'. Verified live: cdk-hnb659fds-cfn-exec-role-302659227808-us-east-1 carries arn:aws:iam::aws:policy/AdministratorAccess, and cdk-hnb659fds-deploy-role trusts arn:aws:iam::302659227808:root — so any principal granted sts:AssumeRole on it is admin-equivalent through `cdk deploy`. That re-creates the exact trust root attack #1 called a BLOCKER (scope line 366-367), now un-rotatable by MFA because a machine key cannot present a token code. The plan's stated mitigation ('DETECTED by CloudTrail + the 8 control-plane alarms') does not hold: A07's filters are principal-typed — 'KMS Sign by IAMUser', 'secret read by human', 'sensitive table write by human' — and an escalation over this path runs as AssumedRole/cdk-hnb659fds-cfn-exec-role/AWSCloudFormation, the same identity legitimate CI uses, so it is either invisible or indistinguishable from a normal deploy. A13's required-reviewers gate is also bypassed, because the agent can deploy locally with that key rather than through the OIDC role.  
Fix: (a) Drop sts:AssumeRole on cdk-hnb659fds-deploy-role / file-publishing role from the ironics-agent-deploy policy — leave it `cdk synth`/`cdk diff` plus the read-only statements, and make every deploy go through the GitHub OIDC role (IronicsPortalDeploy) behind A13's required reviewers. (b) Add a ninth CloudTrail filter+alarm: sts:AssumeRole where requestParameters.roleArn matches cdk-hnb659fds-(deploy|cfn-exec)-role AND userIdentity.arn is not the CI OIDC role ARN, threshold >=1; plus iam:* and secretsmanager:GetSecretValue performed BY the cfn-exec role, which today is silent. (c) Have A01's baseline and A15's after-diff record this escalation path as OPEN, not closed, until CDKToolkit is re-bootstrapped with scoped --cloudformation-execution-policies (the plan already names that as a separate confirm-first ticket — it must be sequenced before Phase 0 is called done, or Phase 0's claim is false).

**[MAJOR] PLAT-B05 + missing Origin/content-type check (scope §D1 lines 98, 123)**  
Problem: B05 widens corsPreflight.allowOrigins to [https://ironics.org, https://admin.ironics.org] with allowCredentials:true (apps/api/lib/api.ts:261-263, already on disk). ironics.org and admin.ironics.org share the registrable domain, so SameSite=Strict is SAME-site between them: a script executing on the marketing site (CursorTrail, 3D, forms) can `fetch('https://api.ironics.org/v1/admin/...', {credentials:'include'})`, the browser attaches ironics_session AND the host-only Path=/v1/admin ironics_admin cookie, and CORS lets it read the response because ironics.org is on the allowlist. The scope demanded exactly the compensating control — 'Origin/Referer check against SITE_ORIGIN and content-type: application/json on every mutating route' (line 98) and 'check Origin + JSON content-type on mutations' (line 123) — and NO ticket in the 128 implements it. Origin isolation (D7/attack #2's fix) is therefore only half-built: the console got its own host, but the API still admits the marketing host to the admin surface.  
Fix: Add to PLAT-B08 (the guard rewrite, before any console route ships): adminHandler refuses with the standard AUTH_REQUIRED byte-identical refusal when an Origin header is present and !== ADMIN_ORIGIN, and refuses mutating requests whose content-type is not application/json. Keep both origins in corsPreflight (public routes need ironics.org) — the admin boundary is enforced in the Lambda, since API Gateway CORS is API-wide. Add an admin-guard.test.ts arm for both refusals and a live curl proof from Origin https://ironics.org returning the refusal.

**[MAJOR] PLAT-B10 (elevation ceremony)**  
Problem: B10 ships 'require the elevation cookie on mutating routes only'. The ruling and the scope both say the admin audience is required on EVERY admin route: decision log D1 ('15-min admin-audience elevation token ... Bearer rejected on /v1/admin/*') and scope line 81 ('adminHandler requires that audience on every admin route'). As planned, a 30-minute ironics_session cookie alone — no second factor, no 15-minute bound — is sufficient to list accounts, unmask PII (C02's accounts:pii reveal is a GET), read and CSV-export the whole audit log (B15), read wallets (E11) and read metrics. Combined with finding 2 that is a full read/exfiltration path from any marketing-site XSS. This is a downgrade of a ruled control, not a scoping choice, and it is not flagged in the plan's corrections list.  
Fix: Require aud=ironics-admin on every /v1/admin/* route with exactly one exemption — GET /v1/admin/me, which the console shell needs to bootstrap the concealment gate. Name accounts:pii reveal, audit read/export, wallet read and metrics explicitly in the elevation-required set. Add the test arm ('every admin RouteSpec except AdminMe is elevation-required') and extend B10's proof: after the 15 minutes lapse, a PII reveal and an audit export must both return ELEVATION_REQUIRED, not 200.

**[MAJOR] PLAT-E01 / PLAT-E02 (key split, D12)**  
Problem: D12 and attack #7's fix require the game-server earn key to have ZERO portal holders. grants.test.ts:284 pins three: "bagman/earn/hmac": ["AdminApproveClaimFn", "AdminResolvePaymentFn", "EpicCallbackFn"]. E02 removes only EpicCallbackFn and rewrites the test to 'the earn key has ZERO non-mint holders' — a weaker predicate than the ruling. No ticket in the programme migrates the two minting Lambdas onto bagman/admin/hmac, even though E01 builds the endpoint→allowed-key-id map and required-actor rule for precisely that purpose and then 'wires NO consumer'. Net result after 128 tickets: an allowlisted admin session is still transitively a full-economy principal, because those two portal Lambdas hold the key that authorizes /escrow-entry, /settle-match, /refund-purchase, /purchase-bundle and /counted-entitlement across 13 tentpole Lambdas.  
Fix: Add a third cutover deploy to E02 (or a new PLAT-E02b): tentpole /earn accepts bagman/admin/hmac with a required actor for claim mints (dual-accept), the portal's approve-claim and resolve-payment sign with the admin key, then the earn-key grant is dropped from both routes. Change the grants EXPECTED entry to "bagman/earn/hmac": [] and add an arm that fails if ANY portal function is added as a holder. Proof: a real claim approval still mints, and `aws lambda get-policy`/the live role policy shows no GetSecretValue on the earn key for any ironics-portal function.

**[MAJOR] PLAT-C13 (flags service) vs PLAT-B09 / PLAT-B07**  
Problem: B09 ships admin.frozen as a fail-CLOSED ConsistentRead on IronicsCounters ('unreadable table also refuses') and proves it in week 7; B07's break-glass script freezes/unfreezes that counter. C13 (week 20) moves the flag into a 30s-TTL SSM parameter where 'admin.frozen fails OPEN with a loud log'. That silently converts the incident kill switch — the control you reach for when an admin credential is suspected compromised — into one an attacker bypasses by making the flag store unreadable, and it leaves two sources of truth unless the counter reader and break-glass writer are repointed in the same commit (C13 does not say they are), which would mean the break-glass freeze stops working while appearing to succeed.  
Fix: Keep admin.frozen fail-CLOSED. In C13: read it with a last-known-good cache but refuse mutations when there is no known-good value; exempt only the unfreeze route and the break-glass script so an outage cannot lock the operator out; repoint break-glass.ts and delete B09's counter reader in the same commit. Extend C13's proof with 'break-glass freeze still halts mutations after the SSM migration' and 'a denied/unreadable flag read REFUSES a mutating admin call' (the same shape the ticket already proves for download.enabled).

**[MAJOR] PLAT-B03 / PLAT-B11 (apps/crm console)**  
Problem: PLAT-A11 scopes CSP nonce/strict-dynamic, HSTS, X-Frame-Options and Permissions-Policy to apps/web only. No ticket adds any security header to apps/crm, yet that is the origin where an XSS is directly an admin XSS — it renders account PII, the audit log, wallet balances and holds the elevated session. Attack #2's whole argument for Option B was that isolating the origin makes the Origin/JSON checks real; shipping the isolated origin with weaker headers than the marketing site inverts that.  
Fix: Port the A11 header block and the Web Crypto nonce middleware into apps/crm as part of B03, before B11 renders any live data. Use a stricter policy than the web's: frame-ancestors 'none', no 'unsafe-inline', connect-src limited to https://api.ironics.org and the Access team domain. Add to B03's proof the same four-header curl assertion plus a zero-'Refused to' console walk of every rail destination.

**[MINOR] PLAT-D04 (CloudFront OAC bucket policy)**  
Problem: D04's proof asserts 'the bucket policy has one CloudFront statement scoped to win64/*' but never asserts the aws:SourceArn condition naming this distribution. A bucket-policy statement granting Principal Service cloudfront.amazonaws.com without SourceArn is not treated as 'public' by Block Public Access (verified live: all four blocks are true on ironics-releases), so it passes every check the ticket makes while allowing ANY CloudFront distribution — including one in another AWS account — configured with this bucket as an OAC origin to read it. The scoping is load-bearing here because the same bucket holds the dedicated-server build (verified: server-build/WindowsServer/Bag_Man/Binaries/Win64/LyraServer-Win64-Shipping.exe and the cooked server content).  
Fix: Assert in the D04 template test and in the live `aws s3api get-bucket-policy` proof that the CloudFront statement carries "Condition":{"StringEquals":{"AWS:SourceArn":"arn:aws:cloudfront::302659227808:distribution/<id>"}} in addition to the win64/* resource scope, and that the pre-existing enforceSSL Deny survives. Re-assert after any later cdk deploy that touches the bucket (D07, D12). Give the D09 log bucket and the D10 Athena results bucket the same BLOCK_ALL + encryption + lifecycle treatment the ticket already promises for the log bucket — query output replays the signed URLs.

**[MINOR] PLAT-B06 / PLAT-B08 (RouteSpec.requires)**  
Problem: B08 sets `requires` on the 8 live admin routes and B06 adds an arm for home:'admin' implying admin:true, but nothing makes `requires` mandatory. An admin route added later (and ~34 are planned) that omits it silently falls back to the flat allowlist the whole D1 ruling exists to replace, and admin-guard.test.ts's text parser would not notice. Separately, Bearer rejection is deferred to B10 (week 8) although it depends on nothing in the elevation ceremony — leaving the currency-minting claims/approve route reachable with a player-grade Bearer JWT for a week after roles ship.  
Fix: Make `requires` non-optional on RouteSpec when admin:true, have adminHandler throw at module load if it is absent (a route with no requires then has no handler, matching the existing adminHandler discipline), and add the enumeration arm to admin-guard.test.ts, which already parses the routes array as text. Move the Bearer rejection (a ~5-line change in the guard) from B10 into B08 and prove it there.

**[MINOR] PLAT-E01 / PLAT-E05 (inbound-only hook key)**  
Problem: D12 and attack #8's fix require a grants-test arm that 'forbids verify+sign in one function' / 'no function holds both an inbound-verify key and an outbound-sign key'. E01 writes the shared verifier and creates bagman/portal-inbound/hmac, E05 wires the hook — but no ticket adds the arm. Without it the property holds only until the next person grants the hook Lambda an outbound key, which is exactly how the current EpicCallbackFn situation arose. The /v1/internal/game-event route is also unauthenticated-until-HMAC and public: a bad-signature flood can consume its reserved concurrency with no rate limit named.  
Fix: Add to apps/api/test/grants.test.ts an EXPECTED entry "bagman/portal-inbound/hmac": ["GameEventFn"] plus an arm asserting no function's environment carries both an inbound-verify secret ARN and an outbound-sign secret ARN (AFL_EARN_HMAC_SECRET_ARN / the admin key). Add a per-source rate limit on the hook route and confirm it is home:'portal', session:false, admin:false so it never lands in the nested admin stack.

**[MINOR] PLAT-B09 (fail-closed admin limiter)**  
Problem: B09 specifies '120/5min and 20/5min admin limits' without naming the key. After A10 the only IP visible on the console's server-side concealment path (/v1/admin/me, fetched by the crm Worker per B11) is a shared Cloudflare Worker egress address, not the operator's. An IP-keyed bucket therefore (a) pools every console user, (b) can be exhausted by traffic belonging to an unrelated Cloudflare customer sharing that egress IP — and because the limiter is deliberately fail-CLOSED, that is a lockout of the admin surface, and (c) does not constrain the real attacker, whose calls go browser-direct to api.ironics.org from their own address.  
Fix: Key admin rate-limit buckets on the verified session sub (accountId) only, never on clientIp; state it in B09's steps and add a test covering both the allow and the fail-closed refusal path. Leave IP keying to the unauthenticated public routes where A10 just fixed it.

**[MINOR] PLAT-B10 (Access→API elevation handoff code)**  
Problem: The two-hop flow correctly keeps any shared secret out of the Worker (attack #14), but the plan states no binding for the 60-second single-use code that crosses the browser. As written the code is a bearer credential that redeems into an elevated admin cookie; if the redeem endpoint accepts it without also requiring a valid ironics_session whose sub matches the account the code was minted for, anyone who observes it (a referrer leak, a query string in a log, a same-origin script) elevates.  
Fix: Specify in B10: the code is bound at mint time to {accountId from the session, Access sub}, stored hashed with a conditional single-use consume and a 60s TTL, redeemed only over POST (never a query string), and the redeem refuses unless the presented ironics_session sub equals the bound accountId. Audit both the issue and the redeem (stepup_issued / elevated) and add the negative arm — a code redeemed from a different session returns the standard refusal.

**[MINOR] PLAT-B15 / PLAT-E13 (audit WORM)**  
Problem: Attack #6's fix made the S3 Object Lock export NON-optional 'for audit + journal' because it is the only WORM available without Organizations SCPs, and because the resource-policy Deny is removable in one DeleteResourcePolicy call by any admin-equivalent principal. The plan builds Object Lock only for the PlayStream export (E13) and never for IronicsAuditLog. With finding 1 open, the console's audit trail — the record of every grant, revoke, ban, pointer move and adjustment — has no tamper-proof copy.  
Fix: Add a scheduled daily export of IronicsAuditLog (DynamoDB PITR export to S3, or a bounded scan writing NDJSON) into the E13 Object-Lock bucket with GOVERNANCE retention, sequenced alongside B15 rather than behind Phase 3, and alarm on 'zero objects in 24h' the same way E13 does. Restate the posture line in B16's footer as 'append-only for application principals; tamper-evident via CloudTrail; WORM copy in S3 Object Lock' only once that export exists.

### ordering + dependency correctness: can every ticket actually start when the plan says (credentials, deploy path, editor availability, operator actions)? any lockout, any proof gate that cannot be watched?

**Verdict:** sound-with-fixes

**[BLOCKER] PLAT-A12, PLAT-A13 (schedule table vs all_tickets)**  
Problem: Both tickets appear in `all_tickets` AND in `critical_path`, but appear in NO week of the 24-week table (machine-checked: 93 of 128 tickets are scheduled; A12 and A13 are among the 35 that are not, and unlike the E/H tail they are not trigger-gated). A12 is the Cloudflare Workers-Routes token edit + "CI becomes the only web deploy path" cutover. Three SCHEDULED tickets declare it as a dependency and therefore cannot start: PLAT-F02 (wk 2), PLAT-A11 (wk 4), PLAT-B03 (wk 6). The week-1 `operator_actions_due` line covers only the operator's dashboard click; the engineer half (re-run probe-cloudflare-tokens.yml to 4/4 200s, re-run deploy-web.yml green, delete the local cf:deploy instructions, close LEGAL_OPEN_ITEMS L4 with the run id) has no slot. A13 (Required reviewers on `production`/`production-api`) is the same shape and is what makes D6's "CI is the only path" enforceable.  
Fix: Add PLAT-A12 (0.5 d) to week 1's ticket list immediately after PLAT-F01, and PLAT-A13 (0.5 d) to week 2. Total +1.0 engineer-day. Re-run the dependency check afterwards: with A12 in week 1, F02/A11/B03 all become legal. Do not rely on the operator_actions_due line to stand in for the ticket — the ticket is what proves the CI path green.

**[BLOCKER] PLAT-E05 / PLAT-E01 (week 15 vs week 22)**  
Problem: Hard 7-week dependency inversion. PLAT-E05 ("inbound-only tentpole->portal game-event hook", week 15) declares depends_on PLAT-E01, which is scheduled in week 22. E05's own steps say the route's "only secret is the portal-inbound key" and its HMAC verification uses the shared verifier — both created by E01. Verified on disk: `grep -rn "portal-inbound|bagman/admin/hmac|resolve/hmac" C:\Dev\Bag_Man_Backend/lambda` returns ZERO matches, so neither the secret nor the verifier exists today, and E01's operator action (populate the secrets) is listed under week 22. Week 15's stated proof gate — "a single `launched` row appears in IronicsEvents" after the operator launches the shipped client — is therefore unachievable as scheduled. The break cascades: PLAT-E06 (wk 24, first_match emitter) depends on E05, and PLAT-H12 (the G-METRICS North Star weekly cohort report, on the critical path) depends on E06.  
Fix: Move PLAT-E01 (1 d), PLAT-E02 (1.5 d) and PLAT-E03 (1 d) from week 22 into week 14 (which is only 4.25 d loaded), ahead of E05. E01 has depends_on: [] and E03 has depends_on: [] — nothing prevents this. Week 22 then absorbs part of the overflow from weeks 15/16. Add the "populate bagman/admin/hmac, bagman/portal-inbound/hmac, bagman/resolve/hmac" operator action to week 14's operator_actions_due (it is a Secrets write the agent principal is denied by PLAT-A02).

**[BLOCKER] PLAT-F10 -> PLAT-F11, PLAT-F12 (mockup-first gate)**  
Problem: PLAT-F10 ("Mockup round 3: /beta, /founders, /build, /houses, /market, /press, /creators", 2 d, operator approval required) is scheduled in no week, yet PLAT-F11 (wk 19) and PLAT-F12 (wk 21) both declare depends_on PLAT-F10 and between them rewrite copy on seven shipped public screens. Authoring those screens without the approved artboards violates the standing mockup-first ruling (CLAUDE.md doctrine 7, which states explicitly "Applies to revamps of existing screens too") and W30 in the decision log ("one bundled round per page"). Unlike the growth/economy tail, F11/F12 ARE scheduled, so this is not a deferred item — it is a scheduled ticket with an unscheduled gate.  
Fix: Schedule PLAT-F10 (2 d) in week 18 (currently 7.0 d loaded — pair the re-level in the schedule-load fix), and add "Approve or amend the seven /beta, /founders, /build, /houses, /market, /press, /creators artboards; answer the sticker-name, seventh-character-row and creators hold-vs-301 questions" to week 18's operator_actions_due. F10's approval turnaround must land before week 19's F11.

**[BLOCKER] PLAT-G08, PLAT-G10, PLAT-G12, PLAT-G13 (0.1.4 publish + live S12 server deploy)**  
Problem: The plan's summary states "the content cull A+C ships with 0.1.4-beta", and week 14's theme says "0.1.4 handoff" — but NO ticket publishes 0.1.4-beta or deploys the re-cooked Shipping server. G13 only "hands the provenance-verified staged root ... to the distribution workstream"; the receiving ticket does not exist. Two live-stack consequences the schedule does not carry: (a) the cull MOVES the ARCANEON closure to /Game/BagMan/ArcaneonArt, so a 0.1.4 client and a 0.1.3 server disagree on package paths for L_Arena_04's imports — client and server must ship together; (b) the live server is not a lab: memory project_s12_gamelift_server_live records the running binaries at C:\game\WindowsServer on EC2 i-0b23133a70f6a55b2 (32.193.28.175), pulled from s3://ironics-releases/server-build/WindowsServer/ and kept up by Scheduled Task `BagManGameLiftServer`, serving live testers 24/7. Replacing them is downtime with a rollback need. G10's step "bring up the cooked Shipping server" never says whether that is the live box or a local one, and week 13/14 operator_actions_due contain no server-deploy or publish action.  
Fix: Add two tickets before PLAT-G13: (1) PLAT-G14 "Deploy the 0.1.4 Shipping server to S12" — upload to server-build/WindowsServer/, stop the Scheduled Task, swap, restart, smoke-test with a create-game-session, keep the 0.1.3 tree for rollback; operator-run, announced window, ~0.75 d; (2) PLAT-G15 "Publish 0.1.4-beta" — publish-release.ps1 (hardened by PLAT-D02) to the version key, verify sha256/size, then move the latest.json pointer; operator-approved, ~0.5 d. Amend PLAT-G10 to state explicitly that the cooked lap runs against a STAGING server instance, not i-0b23133a70f6a55b2, and add both operator actions to week 14.

**[MAJOR] Whole 24-week table (per-week engineer-day load)**  
Problem: The plan states "At ~5 productive days/week ... the full 128-ticket programme runs ~37 weeks", but the scheduled weeks 1-24 total 128.5 engineer-days = 5.35 d/week average BEFORE any slack, and 13 of 24 weeks exceed 5 days: wk 16 = 9.0 d (E07+E08+C09+C10+E09), wks 8/15/18/19 = 7.0 d each, wk 20 = 6.0 d, wks 4/7 = 6.5/6.0 d. Because each week boundary carries a named proof gate AND operator actions, an overpacked week does not just run long — it slips its gate, and every later week is date-anchored to it (e.g. wk 16's Overview/Telemetry gate feeds wk 17's roadmap REV and wk 19's portal copy proof). Week 16 alone is 80 % over its own capacity assumption.  
Fix: Re-level to <= 5.0 d/week before publishing dates. Concretely: split week 16 (move C09+C10 to week 17 and F07/F08 to 18); split week 8 (B10 alone) and week 15 (F05/F06 to week 16); move E01/E02/E03 into week 14 per the E05 fix. That adds roughly 2-3 weeks to the 24-week horizon (~27 weeks to the same point). Alternatively relabel the `weeks` array as an ordered sequence of gates rather than calendar weeks, and stop attaching dated operator commitments to it.

**[MAJOR] PLAT-E03 (money-line alarms, week 22)**  
Problem: PLAT-E03 declares depends_on: [] and is 1.0 d, yet sits at week 22 — 21 weeks after the programme starts. The log lines it filters are LIVE in the tentpole today, verified on disk: lambda/escrow-entry/index.ts:199 `IN DOUBT (status=...)`, lambda/reconcile/index.ts:227 `escrow row(s) IN DOUBT`, lambda/purchase-bundle/index.ts:403 `REFUND FAILED ... (MANUAL RECONCILE)`, lambda/counted-entitlement/index.ts:178 and lambda/conditional-entitlement/index.ts:130 `MIRROR FAILED`. The grounding brief's live read (R2b) shows 16 match-escrow rows and 39 bundle-mint-ledger rows already written. The plan front-loads Phase 0 hygiene for the PORTAL (weeks 1-4) while leaving the money paths that actually move currency without any alerting until week 22 — the plan's own risk register calls out in-doubt escrow as a live hazard.  
Fix: Move PLAT-E03 to week 3, next to PLAT-A07 — it is the identical CloudWatch metric-filter + alarm + SNS pattern and reuses the topic A07 creates. It requires no key split, no secret and no E01. Week 3 then runs 6.5 d, so pair with the re-level fix (move A08 or A09 to week 4). Add "give the alert destination address and confirm the money-alarm fire-drill emails" to week 3's operator_actions_due.

**[MAJOR] PLAT-D09, PLAT-D11 (edge logs + cost breakers, week 23)**  
Problem: The download path goes live on CloudFront in week 5 (PLAT-D06 cutover), but its cost circuit breaker (PLAT-D11: BytesDownloaded alarms + the CloudFront-scoped $10 budget that is the documented Pro-flip trigger) and its telemetry (PLAT-D09: standard logs v2 to S3) are both scheduled in week 23 — 18 weeks of live, signed, 24-hour-valid, 3.9 GB download links with no byte alarm and no logs. Both declare depends_on only PLAT-D04 (week 4), so nothing forces the delay. This contradicts D-DIST-6 in the decision log ("cost circuit breakers before growth"). It is worse for D09 than a delay: per decision log s7.1, the standard-logging output format "is fixed at creation and cannot be changed later", so the first 18 weeks of downloads are permanently unlogged and the format decision is deferred into a week where D10's Athena work depends on it same-week.  
Fix: Move PLAT-D11 (0.75 d) into week 5 with D06 and PLAT-D09 (1 d) into week 4 with D04 (so logging starts the moment the distribution exists and the immutable format is chosen once). Leave D10 (Athena view + snapshot) and D12 (lifecycle) in week 23. Add "confirm the SNS subscription for the download alarms" to week 5's operator_actions_due.

**[MAJOR] PLAT-G01, PLAT-G02 vs PLAT-G03..G08 (11-week stale cull baseline)**  
Problem: PLAT-G01 banks the cull baselines (0.1.3 cook manifests, ReferencedSet.txt, AllChunksInfo.csv, pak listing) and runs the pre-change control SERVER cook in week 1; PLAT-G02 walks the dependency closure and censuses texture dimensions in week 4; but the migration (G03) is week 12 and the diff cooks (G06/G07/G08) are week 13. G06's proof is "the manifest diff shows 167 rows relocated to the slice folder at IDENTICAL byte sizes" and "60/60/60/3/107 pack hits in the baseline become 0 everywhere" — measured against a control that is 12 weeks old. Any unrelated content or engine change in the interval (and week 24's PLAT-E06 proves the game repo is not frozen) shows up as unexplained manifest churn, and the recipe's own STEP 10 says "ANY other added/removed/resized row is an unexplained regression — stop". The proof cannot then distinguish cull effect from drift. G02's closure manifest has the same problem: an 8-week-old census drives an irreversible rename-manager move.  
Fix: Move PLAT-G01 (1 d) and PLAT-G02 (0.75 d) out of weeks 1 and 4 and into the start of week 12, immediately before G03 — the cull audit's STEP 0 explicitly says "bank the baselines you will diff against ... before any cook overwrites D:\BagMan\Cooked", which is a same-window instruction, not a 12-week-earlier one. Week 1 drops to 3.0 d (helping the F01 09-08 deadline) and week 12 rises to 5.5 d, so shed PLAT-C04 from week 12 to week 11 or 13.

**[MAJOR] PLAT-A05 (MFA deny + key retirement)**  
Problem: Two ordering defects in the single highest-lockout ticket. (a) The proof reads "...list-access-keys is empty; the agent profile is unaffected — all watched in one session", but the ticket's own steps mandate "Set the key Inactive, soak 24h, delete it". A 24-hour soak cannot be inside one session, and an STS GetSessionToken MFA session for an IAM user maxes at 36 h (default 12 h), so the deletion needs a SECOND operator MFA code — which week 2's operator_actions_due does not ask for (it asks only to "Be present for the session-token proof" and "Confirm the 24h Inactive soak"). (b) The step order is "Attach the inline DenyAllUnlessMFA policy ..., detach the 10 unrelated managed policies by ARN". The moment the deny attaches, a session built from the plain long-term key is denied every IAM call, so the detach (and the key deactivation, and the deletion) is refused unless an MFA session was obtained first — and PLAT-A02 explicitly denies IAM to `ironics-agent-deploy`, so the agent principal cannot rescue it either. Root MFA is the only remaining path.  
Fix: Reorder A05's steps to: (1) obtain and hold an MFA session (`sts get-session-token` with the operator's code), (2) detach the 10 managed policies, (3) attach DenyAllUnlessMFA, (4) prove the deny bites on the plain key and lifts under the MFA session — this is proof gate 1 and closes week 2; (5) set the key Inactive; (6) 24 h later, under a FRESH MFA session, delete it and re-run the baseline — this is proof gate 2. Add "supply a second MFA code the following day for the key deletion" to week 2's operator_actions_due, and record in the runbook that the deny carve-outs allow MFA self-enrolment but not DeactivateMFADevice, so a lost device is a root-account recovery.

**[MAJOR] PLAT-B09, PLAT-D08, PLAT-C13 (two admin.frozen switches, two flag stores)**  
Problem: The same kill switches are built twice, 13 weeks apart, with contradictory fail policies and no dependency link. PLAT-B09 (wk 7) implements admin.frozen as a DynamoDB IronicsCounters row read with ConsistentRead and fails CLOSED ("unreadable table also refuses"). PLAT-C13 (wk 20) implements admin.frozen as a key inside the SSM /ironics/portal/flags parameter and fails OPEN ("with a loud log"). Decision-log D27 rules flags on SSM, so B09's Counters store is also a ruling deviation. Separately, PLAT-D08 (wk 7) says "Create the flags parameter and a 60s last-known-good reader" and PLAT-C13 (wk 20) says "One /ironics/portal/flags JSON parameter ... with a 30s TTL last-known-good reader" — the same parameter and reader created twice with different TTLs. C13's depends_on is [B10, B15]; it names neither D08 nor B09, so nothing in the plan forces the second build to reconcile with the first.  
Fix: Make PLAT-D08 (wk 7) the sole owner of the SSM parameter and the last-known-good reader, with one ruled TTL. Rewrite PLAT-B09 to consume that reader for admin.frozen with a single ruled fail policy (recommend fail-OPEN with a loud log for admin.frozen — a broken flags read must not lock the operator out of their own console — and fail-CLOSED for download.enabled as D-DIST-6 rules). Reduce PLAT-C13 to "add the remaining flag keys, the PUT route and the consumers" and set its depends_on to [PLAT-D08, PLAT-B09, PLAT-B10, PLAT-B15]. Drop ~1 d from C13's 3.0 d estimate.

**[MINOR] Weeks 12-14 (editor and 16 GB build-box exclusivity)**  
Problem: Weeks 12-13 run PLAT-G03 (editor OPEN, rename manager, world loaded), G04/G05 (editor CLOSED, byte audit + config), G06/G07/G08 (three full Shipping cooks, editor CLOSED), G09 (PIE, editor OPEN) and G10 (staged client + server lap) back to back on the single 16 GB box, and week 14's G12 re-runs BOTH Shipping cooks. No week-12/13/14 operator action reserves the machine or names who runs the cooks — while the project's standing rules are "BUILDS MINE: editor down first" (operator-owned builds), "UE open = locked uassets" and "2 editor PIDs = ALL saves fail", and the cull audit records ~18 min per client cook on this box with UBA off and -MaxParallelActions=2. Week 24's PLAT-E06 correctly carries an operator action for its two engine builds; the six cooks in weeks 13-14 carry none.  
Fix: Add to week 12's operator_actions_due: "Reserve the D:\UE5.6-source editor and the build box for the agent for weeks 12-14; keep the editor closed except for the named G03/G09 windows; confirm Config/Custom/EOS/DefaultEngine.ini is present before the first cook." Name explicitly in each of G06/G07/G08/G12 whether the operator or the agent invokes RunUAT, matching the E06 precedent.

**[MINOR] weeks[].operator_actions_due (three missing entries)**  
Problem: Three scheduled tickets carry an `operator_action` that never surfaces in their week's `operator_actions_due` list, which is the list the operator plans from: PLAT-D01 (wk 2) needs "One signed-in portal pass (five loads plus one click) while the agent watches the logs" — week 2 lists only the MFA proof, the soak confirmation and the CloudFront key pair; PLAT-C04 (wk 12) needs "Confirm Founder #0002 may be permanently spent by the proof" — week 12 lists only the barrier-Blueprint confirmation; PLAT-E08 (wk 16) needs "One download click from the live portal" — week 16 lists only the GameLift metric check and the TESTERS ruling. Each of these is the sole way its week's proof gate can close (only the operator's account is APPROVED).  
Fix: Add the three lines to weeks 2, 12 and 16 of operator_actions_due, and add a validation pass over the plan asserting that every scheduled ticket carrying an `operator_action` has a corresponding entry in its week.

**[MINOR] PLAT-D12 (wk 23) and PLAT-G10 -> PLAT-G12 (proof gates that cannot close in their week)**  
Problem: Two gates cannot be watched when the schedule says. (a) PLAT-D12's proof includes "T+31d: the cold key is GLACIER_IR while hot keys and server-build are untouched and the current release still downloads" — 31 days after week 23 is roughly week 27-28, outside the scheduled horizon, with no follow-up check scheduled and no owner. (b) PLAT-G10's operator 4K verdict (wk 13) is the gate for PLAT-G12's irreversible `git rm` (wk 14), but the operator's ruled options include "cap at 8192 instead" or "ship the migration without the cap" — either answer forces re-running G07 and re-running G06/G08's assertion set, which week 14 has no slack for and which the plan's branchless week-14 theme ("Cull commit + confirm-first pack removal") does not contemplate.  
Fix: Split D12's assertion into a same-week part (three lifecycle rules present, filters read aloud as win64-bound, tags correct, server-build untouched) and a dated follow-up check at T+31d with a named owner and a calendar entry. For the 4K verdict, add an explicit branch to week 14: "if the operator rejects 4K, re-run G07 at the chosen cap, re-run G06+G08, and slip G11-G13 by one week" — and take the verdict from the PIE screenshots in G09 (week 13) rather than only from the cooked lap in G10, so the branch is known a day earlier.

**[MINOR] start_now list; PLAT-B02 and PLAT-E08 declared dependencies**  
Problem: Three smaller correctness defects in the dependency data. (a) `start_now` contradicts the week table: it lists PLAT-A08, A09 and A10 (scheduled weeks 3, 3 and 2, and all three declaring depends_on PLAT-A03 which is itself only starting in week 1), PLAT-D01 (week 2) and PLAT-F03 (week 14 — thirteen weeks later). (b) PLAT-B02 declares depends_on: [] but its proof is "A green web CI run through the JS budget gate, after which ironics.org ... watched rendering unchanged", which is exactly the run PLAT-A12 unblocks. (c) PLAT-E08's proof expects live /v1/stats to show "buildRev 0.1.3-beta (not the 0.8.14 fallback)", but its own step only makes publish-release.ps1 write the buildRev counter row "alongside latest.json" at publish time — and no release publish is scheduled in or after week 16, so the row will not exist when the proof is taken.  
Fix: (a) Reduce start_now to the true week-1 set: F01, A01, A02, A03, G01, A12 — or relabel the field "week 1 batch". (b) Set PLAT-B02 depends_on to ["PLAT-A12"]. (c) Add a one-line backfill step to PLAT-E08 ("write the buildRev counter row for the current pointer version once, from the live latest.json, so the proof does not depend on an unscheduled publish") — or reorder so E08 follows the 0.1.4 publish ticket added by the G13 fix.

### cost + over-engineering: cap breaches, gold-plating for a ~1-tester beta, missed free tiers, wrong vendor facts

**Verdict:** sound-with-fixes

**[BLOCKER] Cost model / C1 / PLAT-A15 / totals**  
Problem: BLOCKER — the dominant cost line is priced at half its real rate and is excluded from the cap arithmetic. I read the live meter: `aws ec2 describe-instances i-0b23133a70f6a55b2` returns PlatformDetails="Windows", UsageOperation="RunInstances:0002" (Windows Server license-included). `aws ce get-cost-and-usage --filter RECORD_TYPE=Usage` for 2026-09-06 returns BoxUsage:c6i.large $3.491669088 over 19.726944 Hrs = **$0.17700/hr** — exactly the published c6i.large *Windows* rate, 2.08x the $0.085 Linux rate that every doc assumed. Decision log C1:84 says "c6i.large 24/7 ≈ $62/month gross"; grounding brief R2a:23 says "$62/month projected"; memory project_s12_gamelift_server_live.md:30 says "~$60/mo". The real 24/7 figure is $129.21/month compute + $3.49 EBS + $3.34 public IPv4 = **$136/month for the server**, and the whole account measures **~$140/month gross** (Secrets $2.62, KMS $0.89, S3 $0.42, DDB/APIGW cents). That is **280% of the $50 cap before a single ticket in this plan is built**. The plan's totals block answers a different question — it reports only the *delta* ("+$2-4/month now… worst case ~$35 — inside the $50 cap in every week") — so the answer to "any week above the cap?" is: every week, week 1 included. Two compounding consequences: (a) PLAT-A15 sets the budget to gross, so the circuit breaker will sit at 280% permanently and be muted within a month; (b) C1's credit-runway trigger ("re-evaluate when remaining credits < 2 months") arrives at **twice the assumed speed** — a 37-week programme burns ~$1,200 of credits at this rate, and nobody has read the balance. This is not re-opening C1; it is flagging that C1 was ruled on a price that is wrong by $74/month.  
Fix: Week 1, ~0.75 day total. (1) Re-publish C1's number from the meter (the single `ce get-cost-and-usage --filter RECORD_TYPE=Usage --group-by USAGE_TYPE` call above) and have the operator read Billing > Credits in the same sitting — the credit balance is now a hard schedule constraint, not a footnote. (2) Add PLAT-A16 'server cost posture', three reversible levers cheapest-first: **EventBridge Scheduler stop/start** around G-EVENTS' already-ruled announced play windows — at 6 h/day compute drops to $32.30/month, saving **$96.91/month**, ~0.5 day, fully reversible, and the box is already SSM-managed with the wrapper on an AtStartup Scheduled Task so it self-heals on boot (memory :28-29); or a Linux server target (saves $67.16/month but is real engine work); or accept and restate the cap. (3) Split the budgets so the cap means something: keep one gross budget at a threshold that reflects the ruled posture, and add a **platform-only budget at $20** filtered to exclude EC2/EBS/VPC — that is the number the '$50 scaling cap' is actually about, and today every platform overrun is invisible behind a $136 server line.

**[BLOCKER] Sequencing: PLAT-B*/C* (weeks 5-16) vs PLAT-E04-E06, H02-H04**  
Problem: BLOCKER — ~60 of the 183 engineer-days build administration for an estate that does not exist, and they sit in front of the two things that would create one. Live footprint (grounding brief R2b:24, re-confirmed today): IronicsAccounts 1, IronicsAdmins 1, IronicsAuditLog 3, IronicsApplications 1, ClaimCodes 0, UnattributedPayments 0, 0 CloudWatch alarms. Weeks 5-16 = twelve weeks on a 16-screen console for a roster of one. The ruled growth engine (G-INVITE, decision log :131 — "make Founder invites TRUE") lands week 18; the ruled North Star signal (G-METRICS :136, installed→first match ≥70%) has no writer until PLAT-E06 in **week 24**. The plan's own risk row concedes the North Star "has no server signal". So the programme spends its first six months, and ~$840 of server credits at the corrected burn rate, building levers to administer users while the mechanism that produces users waits. Every week of that delay also costs $140 of finite credit.  
Fix: Reorder without dropping anything ruled (D9's build order governs the console's internal sequence, not the console's position against growth). Move PLAT-E04/E05 (events table + `launched`), PLAT-E06 (first_match emitter), and PLAT-H02/H03/H04 (founder invites end-to-end) into weeks 6-10, immediately after the auth spine (B07-B11) and before the console's read-only screens. Push PLAT-C12/C14/C17 (Releases, Flags, Escrow screens) and the whole economy console behind them. Adopt one scheduling rule for the rest of the programme: **no console screen ships before its subject table holds ≥10 rows** — it converts 'is this gold-plating?' from a judgement call into a query.

**[MAJOR] PLAT-G01–G13 (content cull, 9.5 days, weeks 12-14)**  
Problem: MAJOR — three full weeks on the critical path for **$0/month and zero download bytes**. The cull audit's own section 5 is unambiguous: "Expected savings: source 11,710,909,627 B, cooked 0 B, zip 0 B"; "this cull is a REPO-SIZE win only (~9.5 GB net source); it saves ZERO download bytes because the 60 reachable packages must be moved, not removed"; and on the destructive half, "GitHub LFS storage unchanged without the rejected history rewrite" — so even the `git rm` returns no vendor saving. The only user-visible win in the ruling is option C, the 4K cap (~-155 MB, ~4% of the download), which the audit says "needs NO cull at all". And the egress arithmetic is now moot: 322 MB x ~25 downloads/month = 8 GB/month, which is $0 inside CloudFront's 1 TB pay-as-you-go free tier. So the migration's cost justification (per-download hosting cost) is exactly $0 — the audit says so itself: "Per-download hosting cost therefore does not move from this cull as scoped." O2 ruled A+C and that stands; what is not ruled is *when*, and weeks 12-14 are entirely blocked by it while the growth loop waits until week 18.  
Fix: Split the ruled A+C by schedule, not by scope. Ship **C alone** with 0.1.4 — PLAT-G07's texture cap plus one client cook, one server cook and the watched lap (~2.5 days, and it is the only half a player can see). Move **A** (G02-G06 migration, G11-G13 commit + destructive `git rm` + prune) to a named trigger — 'fresh-clone time blocks onboarding a second machine' or a low-activity window — and hand weeks 12-14 to the growth loop. This keeps O2 intact, keeps the 12-step verification recipe intact, and removes 3 weeks from the critical path at zero product cost.

**[MAJOR] PLAT-E12, E13, E14, E18, E20 (economy console, ~16.5 days)**  
Problem: MAJOR — a money-control plane for a system through which no money has ever moved. Evidence: the VOLTS scope's open item "A real payment through the flow ('no money moved') — nothing later says otherwise"; live tables UnattributedPayments 0, ClaimCodes 0, match-settlement-ledger 8, bundle-mint-ledger 39 (R2b:24). Three specific over-builds: (a) PLAT-E13 creates an S3 bucket with **Object Lock** for PlayStream export — a COMPLIANCE-mode bucket cannot be emptied or deleted for the retention period, so it is an irreversible permanent cost floor accepted on behalf of a regulator nobody has named; (b) PLAT-E18 ships a maker-checker state machine that D11:49 rules inert by construction ("roster = 1 rule") — four-eyes with one pair of eyes; (c) PLAT-E14's nightly drift reconciliation compares a journal against PlayFab for a ledger that will hold tens of rows. That is 16.5 days of controls built ahead of the transactions they control.  
Fix: Keep the two cheap, high-value halves and trigger the rest. Ship PLAT-E03 (money alarms — cheap, catches the failures that actually exist today) and E10/E11 (read-only wallet view). Gate E12 (journal), E13 (Object Lock export), E14 (drift), E18 (adjustments) and E20 (holds) on a single named trigger: **first non-operator real-money transaction, or the first second-admin grant** — the same trigger D2:40 already uses for treasurer/GSIs/CSV. If E13 ever runs, rule **GOVERNANCE** retention explicitly, never COMPLIANCE. Releases ~13 days.

**[MAJOR] PLAT-A07, C15, D11, E03, E13, E14, H08 (alarms + metric filters)**  
Problem: MAJOR — the design's own cost line assumed the CloudWatch free tier and the plan blows through it ~3x. `aws cloudwatch describe-alarms` returns **0 alarms today**. The plan creates ~29 (A07 8, C15 6, D11 3, E03 8, E13 1, E14 2, H08 1) and ~19 metric-filter custom metrics (A07 8, C15 3, E03 6, E14 2). The admin scope prices the whole design at "$1-3/month … CloudWatch alarms within the 10 free" (IRONICS_ADMIN_DASHBOARD_SCOPE.md:112) — that assumption is now false. Real: (29-10) x $0.10 + (19-10) x $0.30 = **$4.60/month**, which alone is larger than the plan's entire claimed 'now' delta of $2-4. The second cost is worse than the dollars: 29 alarms on a one-person pager, several of which the plan itself admits have expected true positives (the 'sensitive table write by a human' filter fires on the operator's own work), is how a solo on-call learns to ignore the topic.  
Fix: Collapse log-phrase filters into one metric each per domain and put the phrase in the alarm description, which the Overview already renders verbatim. E03's six money phrases (IN DOUBT x2, payout failed, REFUND FAILED, MIRROR FAILED x2) become **one `MoneyAnomaly` metric + one alarm** plus a saved Logs Insights query in the runbook; A07's eight control-plane filters become three (identity, money-code, data-plane). Target ≤10 alarms and ≤10 custom metrics — that holds the line at **$0** and keeps every page meaningful. Add the target as an assertion in PLAT-A06's security.test.ts so the count cannot drift back up.

**[MAJOR] PLAT-C09 + PLAT-C10 (/v1/admin/health and the Overview strip)**  
Problem: MAJOR — the one route in the plan that bills per request, polled every 30 s, with no cache. C09 calls CloudWatch GetMetricData (metric math for 5xx/errors) + DescribeAlarms + EC2 DescribeInstanceStatus + GameLift + two tentpole fetches **per request**, and C10 polls it at 30 s. GetMetricData is metered at $0.01 per 1,000 metrics requested. One visible 8-hour day at 6 metrics = ~173,000 metrics/month ≈ $1.73/month; two tabs, or the ruled five-tile strip growing to the alarm panel's ~29 alarms, puts it at $5-10/month — for numbers that change hourly. It also contradicts the plan's own ruled pattern one ticket over: PLAT-E09 correctly requires /v1/admin/metrics to read "roll-up rows only (never a live scan or Athena)".  
Fix: Apply E09's pattern to health. Have the existing 5-minute roll-up Lambda (PLAT-E07) write a `health#current` row — it is already scheduled, already has the IAM surface, and 5-minute freshness is well inside what an EC2 status or alarm state needs. Make GET /v1/admin/health a single GetItem, drop the client poll to 60 s, and keep the per-source try/catch and 'unavailable' degradation exactly as designed. Zero metered CloudWatch calls, one code path, and C09 drops from 2 days to ~1.

**[MAJOR] PLAT-A15 + PLAT-A01 (budget baseline)**  
Problem: MAJOR — the ticket's premise is already false on the live account, which means the Phase-0 'before state' was not actually read. `aws budgets describe-budgets` returns **two** budgets, not one: `ironics-monthly-50` (limit $50, **IncludeCredit=false**, ActualSpend $3.702, Forecast $7.162) and `ironics-monthly-cost` (limit $50, IncludeCredit=true, ActualSpend $0.0). PLAT-A15 is written as "Fix the $50 budget to measure GROSS spend" with the proof "describe-budgets shows IncludeCredit false and a non-zero ActualSpend matching Cost Explorer" — that proof **passes today, before the ticket runs**, and the plan schedules an operator MFA session for a no-op. Worse, the surviving gross budget forecasts $7.16/month because the server has only run two days; against the corrected $140 run-rate it will breach in mid-September, during week 2, and stay breached. Separately: AWS Budgets bills $0.02/day beyond the first two, and the account already has two — so PLAT-D11's CloudFront budget is budget #3 ($0.60/month) and my recommended platform-only budget would be #4.  
Fix: Rewrite A15 as 'reconcile the budgets' (~0.25 day, no operator MFA needed for the delete): drop the duplicate `ironics-monthly-cost`, keep `ironics-monthly-50` as the gross tracker at a threshold set from the corrected run-rate, and spend the freed slot on the **platform-only $20 budget** from finding 1 — that keeps the account at two-to-three budgets and keeps D11's CloudFront budget nearly free. Then re-run PLAT-A01's baseline script and treat the miss as a signal: every other 'live state' claim in the A-workstream baseline (analyzers, trail, quota) should be captured by the script, not asserted.

**[MAJOR] PLAT-D09 + D10 + D21 (edge-log telemetry, ~5 days)**  
Problem: MAJOR — a data warehouse for a two-digit integer, resting on a premise the plan itself disproved. The admin scope deferred this explicitly: "CloudFront logs -> Athena **DEFERRED behind the $50 cap (ruling R2)**" (scope:37). D25:74 un-deferred it with the reason "no longer deferred - free plan" — and section 7.1 then proved that free-plan claim false (the flat-rate Free plan is 100 GB and **excludes access logs**). The conclusion survives on pay-as-you-go because delivery to S3 is free, but the *engineering* deferral was reversed by a premise that no longer exists and was never re-examined. At ~25 downloads/month, Athena + Glue + partition projection + a results bucket + a nightly snapshot Lambda answers 'how many completed?' — a question `download_started` plus the mint log already answer to within one row.  
Fix: Keep PLAT-D09 (standard logs v2 -> S3; free to deliver, cheap to store, and it preserves history you cannot backfill). Defer D10 (Athena/Glue/view/snapshot) and D21 (the Telemetry Downloads panel's edge half) behind a volume trigger of **>100 downloads/month**. Until then the Downloads panel reads minted vs clicked from the events table and labels the rest 'edge logs retained, not yet queried' — which is the honest state and the scope's original ruling. Releases ~3.5 days.

**[MINOR] PLAT-D12 (release-bucket lifecycle)**  
Problem: MINOR — 0.75 engineer-days on a ticket marked destructive/irreversible to save **$0.23/month**. The live meter shows S3 at $0.0137847948/day = **$0.42/month for the entire 20 GB bucket**. Moving the ~12 GB of superseded releases to Glacier Instant Retrieval takes that portion from ~$0.276 to ~$0.048 — $2.76/year — while adding a 90-day minimum-storage commitment and a $0.03/GB retrieval charge that makes any rollback to an archived release cost ~$0.36 and take an extra step. PLAT-C11 explicitly ships a rollback capability and PLAT-C12 rehearses it live; tiering the rollback targets to Glacier works directly against that.  
Fix: Ship only the free half of D12: the abort-incomplete-multipart-upload rule and the noncurrent-version expiry on latest.json (both genuine hygiene, both $0). Drop the hot/cold tagging and the Glacier IR transition, and with them the destructive flag and the operator's 'confirm which releases stay hot' step. Revisit when the bucket passes ~200 GB, where the saving is $2-3/month rather than $0.23. Releases ~0.5 day.

**[MINOR] PLAT-D04, E01, E02, H07 (Secrets Manager) + totals**  
Problem: MINOR — the secret count and its cost are both wrong, and a free alternative the estate already uses is available for most of them. `aws secretsmanager list-secrets` returns **7** existing secrets (bagman/tentpole/hmac, bagman/playfab/secret, bagman/earn/hmac, ironics/portal/turnstile, ironics/portal/epic, ironics/portal/resend, bagman/eos/client) — measured at $0.0861055624/day = $2.62/month. The plan adds **five**, not three: ironics/portal/cf-signing-key (D04), bagman/admin/hmac + bagman/portal-inbound/hmac (E01), bagman/resolve/hmac (E02), ironics/portal/discord (H07). That is $2.00/month new, taking the line to $4.80 — while the totals block states "3 new Secrets Manager entries ~$1.20-1.60". On a claimed 'now' delta of $2-4/month, the secrets line alone is 50-100% of it.  
Fix: Correct the total to five secrets / $2.00. Then move the three static HMAC keys (bagman/admin, bagman/portal-inbound, bagman/resolve) to **SSM Parameter Store SecureString**, which is free for standard parameters and is already the estate's ruled pattern for exactly this reason (D27:76; scope:187 'SSM standard parameters are free and already the estate's pattern'). They need no rotation machinery — PLAT-A14 already writes a manual 90-day rotation runbook — and the KMS decrypt calls are $0.03/10k. Keep Secrets Manager for the CloudFront private key (D04) and Discord (H07), where resource policies and versioning earn the $0.40. Net: $1.20/month back, no capability lost.

**[MINOR] PLAT-B10 (elevation KMS key)**  
Problem: MINOR — an unbudgeted recurring line. B10 adds a second KMS key (alias ironics-admin-elevation) so the elevation signer is separated from the session signer. The live meter shows KMS at $0.0292266669/day = $0.89/month for the one existing key, so the second is **+$1.00/month** plus $0.03/10k sign requests. It appears in no cost total — neither the plan's block nor the admin scope's "KMS ~$1/key-month (existing key reused)" line (scope:112), which explicitly assumed reuse.  
Fix: Keep the key — blast-radius separation between the session signer and the 15-minute admin signer is worth $1/month and is the right call. Just name it in the totals (now: +$1.00) and correct scope:112's 'existing key reused' clause, so the next cost review does not re-discover it as a surprise.

**[MINOR] PLAT-A09 (reserved concurrency on 22→57 functions)**  
Problem: MINOR — a DoS control sized for traffic that does not exist, which permanently shrinks a pool shared with the money path. A09 reserves 10 per public route and 5 per admin route: 180 units today, ~355 at full scope, out of 900 reservable in a 1,000-unit account limit that the portal shares with the tentpole's proven money Lambdas (scope:390 notes ≥13 of them). Reserved concurrency is not a rate limit and not free capacity — every unit reserved is a unit the tentpole's currency-earn and settle functions can no longer burst into. The live traffic is one operator plus a handful of unauthenticated public routes.  
Fix: Reserve only where a runaway actually costs money or blocks a human: the unauthenticated public routes — auth/email/start, epic/callback, beta/apply, download mint — plus the roll-up Lambda. That is ~6 functions, ~60 units, and leaves 840 unreserved for the tentpole. Keep the synth guard (each 1..10, sum ≤450) as the ceiling. Drops A09 from 1 day to ~0.25 and preserves the parts that matter: the dead PORTAL_RESERVED_CONCURRENCY constant deleted and the stale '10 concurrency' doc corrected against the live 1,000.

**[MINOR] PLAT-A11 (CSP nonce + strict-dynamic, 2.5 days)**  
Problem: MINOR — 2.5 engineer-days of the most delicate change in the web workstream, for a site that loads no third-party JavaScript. The ruled analytics posture keeps it that way for now: D22:71 and D26:75 both hold PostHog and Cloudflare Web Analytics until the Privacy Notice v0.3 lands, and client events stay OFF. A per-request nonce needs middleware.ts on OpenNext with Web Crypto (no node:crypto), which is precisely the surface the plan's own research flags as fragile — and it is being added to the live marketing site to defend against inline-script injection vectors that do not exist yet.  
Fix: Two commits instead of one. Now: static headers in next.config.mjs + public/_headers — `script-src 'self'`, HSTS, X-Frame-Options, Permissions-Policy — walked across the 13 routes report-only then enforced (~0.5 day, no middleware, no OpenNext risk). Later: add the nonce + strict-dynamic middleware in the *same commit that first introduces a third-party script* (PostHog or Web Analytics after F15), where it is both necessary and testable against a real violation. Releases ~2 days.

**[MINOR] D22 / D26 — ruled free-tier wins with no ticket**  
Problem: MINOR — two RULED, zero-cost analytics sources have no ticket anywhere in the 128, and their absence leaves the Telemetry screen without a denominator. D26:75 rules Cloudflare Web Analytics on after the Privacy Notice; D22:71 rules PostHog as a server-side second sink on the same condition. Neither appears in any ticket. Consequence: PLAT-E09's funnel starts at `email_submitted` with **no visitor count above it**, so the conversion rates it renders have no top of funnel — and the free source that would supply it (Cloudflare Web Analytics, $0, one snippet; the zone's own analytics are already on and free) is simply dropped. This is the cheapest missing win in the programme by effort-to-value.  
Fix: Add a 0.25-day ticket sequenced right after PLAT-F15 (legal v0.3 in force): enable Cloudflare Web Analytics on ironics.org and add the visitor count as the funnel's top row on the Telemetry screen, labelled with its source and its cookieless posture. Leave PostHog deferred per the scope's own re-rule (scope:34) — it is the one that carries consent and vendor cost; Web Analytics carries neither.

**[MINOR] PLAT-E13 (PlayFab export) and PLAT-D14 (WAF before the Pro flip)**  
Problem: MINOR — two unverified vendor facts that could each breach a stated constraint. (a) E13 assumes the operator can configure a PlayFab Data Connection (or the legacy Event Archive) on title 1A2077. Whether that feature is available on the title's current PlayFab plan is verified nowhere in the plan or the fact sheet; if it requires a paid PlayFab tier, E13 breaches C3:86 ('Zero new paid vendors for the beta') — and it would be discovered *after* an Object-Lock bucket exists that cannot be emptied or deleted for its retention period. (b) D14 pre-stages a WAF web ACL for the CloudFront Pro flip. The verified facts note WAF on pay-as-you-go bills $5 per web ACL + $1 per rule + $0.60/M requests; with ~5 managed rule groups that is **~$10/month incurred while still on PAYG** — two-thirds of the Pro plan's own price — and the plan's worst-case total omits it. The vendor requirement is that the ACL must exist before CreateSubscription, not before approval.  
Fix: (a) Make 'confirm PlayFab Data Connections are available on the current title tier at $0' a hard precondition on E13, checked before any bucket is created, with an explicit branch: if it is paid, E13 stops at the journal (E12) and the export is re-ruled. (b) In D14, state the rule in the ticket: create and associate the web ACL **in the same operator session that runs ApprovePaidSubscription**, never earlier — and add the $10 PAYG WAF figure to the worst-case table so the number is honest if the sequencing ever slips.

**[MINOR] Totals block — corrected arithmetic**  
Problem: MINOR — every individual understatement above compounds into a totals line that is off by ~4x on the delta and silent on the base. Claimed: '+$2-4/month now', 'rising to ~$10-12 at full telemetry', 'worst case ~$35 — inside the $50 cap in every week'. Measured/corrected: secrets +$2.00 (not $1.20-1.60, and 5 not 3), second KMS key +$1.00 (absent), CloudWatch alarms/metrics +$4.60 (absent; scope:112 assumed the free tier), health-route GetMetricData polling +$1.73 to +$10 (absent), CloudTrail management-events-to-CloudWatch-Logs ingest + DynamoDB data events +$0.50-1.50 (absent), budget #3 +$0.60 (absent), Athena/log/press/export buckets ~+$0.50. Realistic near-term delta is **+$11-13/month**, not $2-4 — and it sits on a measured **$140/month** base, not the implied ~$5.  
Fix: Republish the totals as two numbers that answer two different questions: **base** (measured, $140/month gross today, dominated by the server) and **platform delta** (+$11-13 now, +$40 at trigger-gated worst case incl. CloudFront Pro $15, Workers Paid $5, Azure signing $10, WAF $10 if mis-sequenced). Then bind the platform delta to the $20 platform-only budget from finding 1 so the claim is enforced by a meter rather than a spreadsheet. Note for the verdict: the distribution workstream's own cost posture is the one part that is right — CloudFront pay-as-you-go inside the 1 TB always-free tier genuinely delivers the ruled 'free or ≤$15' downloader, and PLAT-D06 (killing the 15-minute presigned expiry) is the highest-value $0 ticket in the plan.

## J. Verified vendor facts

### Cloudflare Zero Trust / Access fact check for DECISION LOG D3 + D7 + D30 (admin.ironics.org step-up): free-plan seats, OTP + Independent MFA, Cf-Access-Jwt-Assertion validation, Access on a Worker custom domain, Google IdP on free plan (verified: True)

- FREE SEATS (confirmed, vendor page): Cloudflare Access product page plan table shows a "Free Plan" at "$0" "forever" with the row "50 user limit" and the tagline "Best for teams under 50 users or enterprise proof-of-concept tests."; paid plan is "$7 per user/month (paid annually)". URL: https://www.cloudflare.com/zero-trust/products/access/ (same wording at https://www.cloudflare.com/sase/products/access/).
- FREE PLAN payment detail (docs): "If you chose the Zero Trust Free plan, this step is still needed but you will not be charged." (payment method must be added at Zero Trust setup). URL: https://developers.cloudflare.com/cloudflare-one/setup/
- FREE vs PAID plan existence (docs): "Cloudflare Zero Trust offers both Free and Paid plans." and "Access to certain features depends on a customer's plan type." URL: https://developers.cloudflare.com/cloudflare-one/
- SEAT COUNTING (docs): "Cloudflare One subscriptions consist of seats that active users in your account consume." "For Access, this is any Cloudflare Access authentication event, such as a login to the App Launcher or an application." "The user will occupy and consume a single seat regardless of the number of applications accessed or login events from their user account." URL: https://developers.cloudflare.com/cloudflare-one/team-and-resources/users/seat-management/
- IdP ON FREE PLAN (confirmed, vendor plan table): row "Authentication via identity providers (IdPs)" - "Authenticate via enterprise and social IdPs, including multiple IdPs concurrently. Can also use generic SAML and OIDC connectors." - is checked for Free, Pay-as-you-go and Contract columns. URL: https://www.cloudflare.com/zero-trust/products/access/
- GOOGLE IdP (docs): "You can integrate Google authentication with Cloudflare Access without a Google Workspace account." Setup starts "Log in to the Google Cloud Platform console. Create a new project...". No plan requirement is stated on the page. URL: https://developers.cloudflare.com/cloudflare-one/identity/idp-integration/google/  Account limit: "Identity providers | 50" with no per-plan split. URL: https://developers.cloudflare.com/cloudflare-one/account-limits/
- ONE-TIME PIN as IdP (docs): "Cloudflare Access can send a one-time PIN (OTP) to approved email addresses as an alternative to integrating an identity provider." "You can simultaneously configure OTP login and the identity provider of your choice to allow users to select their own authentication method." PIN "expires 10 minutes after the initial request" and is single-use ("This One-Time PIN has already been used"). "By design, blocked users will not receive an email." Setup: Zero Trust > Integrations > Identity providers > Add new identity provider > One-time PIN (API type "onetimepin"). Allowlist sender "noreply@notify.cloudflare.com" / domain "notify.cloudflare.com". Caveat: "If a user authenticates via your Identity Provider, but later authenticates with a different method (such as One-Time PIN), Access will no longer evaluate the user's Identity Provider group memberships." URL: https://developers.cloudflare.com/cloudflare-one/integrations/identity-providers/one-time-pin/ (old path /cloudflare-one/identity/one-time-pin/ redirects)
- INDEPENDENT MFA exists and works with OTP (docs, Last updated Aug 13, 2026): "Independent multi-factor authentication (MFA) allows you to enforce MFA requirements directly in Access without relying on your identity provider (IdP)." Enrollment step 2 reads verbatim: "Log in with your identity provider or with a one-time PIN (OTP)." Org-level rule: "If the user does not have an active MFA session for the required authenticator method, they must complete MFA in addition to IdP authentication." URL: https://developers.cloudflare.com/cloudflare-one/access-controls/access-settings/independent-mfa/  Changelog (Apr 15, 2026): "Cloudflare Access now supports independent multi-factor authentication (MFA), allowing you to enforce MFA requirements without relying on your identity provider (IdP). With per-application and per-policy configuration, you can enforce stricter authentication methods like hardware security keys on sensitive applications..." URL: https://developers.cloudflare.com/changelog/post/2026-04-15-independent-mfa/
- INDEPENDENT MFA methods (docs): "Authenticator application - Time-based one-time passwords (TOTP) ... Access supports one TOTP authenticator per user at a time." "Security key - Hardware security keys that support the WebAuthn standard. Users can enroll multiple security keys." "Biometrics - Built-in device authenticators that use WebAuthn, including Apple Touch ID, Apple Face ID, and Windows Hello." PIV key and FIDO2 key are "infrastructure apps only" ("PIV and FIDO2 key authenticators only work with infrastructure applications."). "Infrastructure applications do not yet support independent MFA." (changelog). URL: https://developers.cloudflare.com/cloudflare-one/access-controls/access-settings/independent-mfa/
- INDEPENDENT MFA per-application enforcement (docs, Last updated Aug 13, 2026): three levels - "Organization: Enforce MFA by default for all applications in your account." "Application: Require or turn off MFA for a specific application." "Policy: Require or turn off MFA for users who match a specific policy." "MFA settings use this precedence: Policy > Application > Organization." Prerequisite: "Before you configure independent MFA on applications or policies, you must turn on independent MFA at the organization level." Application steps: Zero Trust > Access controls > Applications > Configure > Authentication > MFA tab > "Respect global enforcement setting" / "Custom MFA settings" (choose allowed MFA methods + authentication duration) / "Disable MFA". Require-every-login = Authentication duration "Require every login" (API `mfa_config.session_duration` = "0m"). Two MFA modes documented: "Identity provider-based MFA - Require specific MFA methods reported by your identity provider (IdP)." and "Independent MFA - Prompt users for a second factor directly in Access, without relying on a third-party identity provider." URL: https://developers.cloudflare.com/cloudflare-one/access-controls/policies/mfa-requirements/
- INDEPENDENT MFA org-level turn-on (docs): Zero Trust > Access controls > Access settings > "Allow multi-factor authentication (MFA)" > select methods > "Set an Authentication duration. This determines how long a user can log in to Access without being prompted for MFA again." Optional "Use identity provider MFA" checks the IdP AMR value (irrelevant for OTP, which carries no AMR). Note: "The App Launcher is exempt from the global MFA requirement. Users must be able to access the App Launcher without MFA to enroll their authenticators." Enrollment at `<team-name>.cloudflareaccess.com` > Account > MFA devices > Add an MFA device (direct link `<team-name>.cloudflareaccess.com/AddMfaDevice`). URL: https://developers.cloudflare.com/cloudflare-one/access-controls/access-settings/independent-mfa/
- INDEPENDENT MFA plan gating: NOT positively stated either way. None of the three pages (independent-mfa, mfa-requirements, 2026-04-15 changelog) contain the words plan/Free/Enterprise/available-on. Treat as available on Free until the operator's dashboard check proves otherwise.
- JWT DELIVERY (docs): "When Cloudflare sends a request to your origin, the request will include an application token as a `Cf-Access-Jwt-Assertion` request header. Requests made through a browser will also pass the token as a `CF_Authorization` cookie." Recommendation: validate the header, because the cookie "is not guaranteed to be passed." URL: https://developers.cloudflare.com/cloudflare-one/identity/authorization-cookie/validating-json/ (redirects to /cloudflare-one/access-controls/applications/http-apps/authorization-cookie/validating-json/)
- JWT CERTS + ROTATION (docs): public keys at `https://<your-team-name>.cloudflareaccess.com/cdn-cgi/access/certs`; the endpoint holds "two public keys: the current key used to sign all new tokens, and the previous key that has been rotated out" in JWK and PEM formats. "Access rotates the signing key every 6 weeks. This means you will need to programmatically or manually update your keys as they rotate. Previous keys remain valid for 7 days after rotation." Guidance: "Validate tokens using the external endpoint rather than saving the public key as a hard-coded value" and "match the `kid` value in the JWT to the corresponding certificate in `public_certs`" (do not use the single `public_cert` field). Algorithm: RS256. URL: https://developers.cloudflare.com/cloudflare-one/identity/authorization-cookie/validating-json/
- AUD TAG location (docs): Zero Trust > Access controls > Applications > Configure > "From Additional settings, copy the Application Audience (AUD) Tag". Claims (application token): `aud` = "Application audience (AUD) tag of the Access application." (an ARRAY in the example: "aud": ["32eafc76..."]), `email` ("verified by the identity provider"), `exp`, `iat`, `nbf` (Unix time), `iss` = "The Cloudflare Access domain URL for the application." (example "https://yourteam.cloudflareaccess.com"), `type` = "app" (vs "org" for the global session token), `identity_nonce` = "A cache key used to get the user's identity.", `sub` = "The ID of the user.", `country`. URL: https://developers.cloudflare.com/cloudflare-one/identity/authorization-cookie/application-token/
- JWT VALIDATION reference code in a Worker (docs changelog Oct 3, 2025): "To fully secure your application, it is important that you validate the JWT that Cloudflare Access adds to the `Cf-Access-Jwt-Assertion` header on the incoming request." Sample uses jose: `createRemoteJWKSet(new URL(`${env.TEAM_DOMAIN}/cdn-cgi/access/certs`))` then `jwtVerify(token, JWKS, { issuer: env.TEAM_DOMAIN, audience: env.POLICY_AUD })`; env vars `POLICY_AUD` ("Your application's AUD tag") and `TEAM_DOMAIN` ("https://<your-team-name>.cloudflareaccess.com"). URL: https://developers.cloudflare.com/changelog/post/2025-10-03-one-click-access-for-workers/
- ACCESS ON A WORKER CUSTOM DOMAIN (docs, Last updated Aug 18, 2026): "With Cloudflare Access, you can restrict who is authorized to access your application. You decide who is approved, and every request is checked before your Worker runs." You can protect "Specific custom domains and hostnames: restrict access at the hostname or route level." "Use hostname-based Access when only a specific URL that routes to your Worker should require sign-in, such as a `workers.dev` hostname, a Custom Domain, a subdomain, or a path. ... you can protect `my-worker.example.workers.dev`, `admin.example.com`, or a single path" - done by "creating a self-hosted application ... and using the hostname or path as the application domain." Prereq: "Zero Trust enabled on your account." URL: https://developers.cloudflare.com/workers/configuration/cloudflare-access/  Custom Domains page: "Require sign-in for a Custom Domain - To require visitors to sign in before they can access a Custom Domain, use Cloudflare Access." "You cannot create a Custom Domain on a hostname with an existing CNAME DNS record or on a zone you do not own." URL: https://developers.cloudflare.com/workers/configuration/routing/custom-domains/
- WORKER-LEVEL ACCESS (docs, changelog Aug 14, 2026): "Self-hosted applications can also protect a Cloudflare Worker directly by name, rather than by hostname or IP." "This is the safest and most straightforward way to put authentication in front of a Worker. ... Any request to the Worker on any route passes through Access first." API: destinations `[{"type":"worker","worker_id":"..."}]` (or `preview_worker`). Dashboard: Workers & Pages > Worker > Access tab > "Protect this Worker behind Access" > Previews only / All traffic. Hostname-based Access "protects only that exact URL" - workers.dev / preview URLs stay open unless covered. URLs: https://developers.cloudflare.com/cloudflare-one/access-controls/applications/choose-application-type/ ; https://developers.cloudflare.com/changelog/post/2026-08-14-workers-access/
- ctx.access IN WORKERS (docs): "When Cloudflare Access authenticates a request that directly invokes your Worker, the Worker can read the signed-in user's identity ... through `ctx.access`. No extra configuration or JWT parsing is required." "`ctx.access` is `undefined` if Access did not authenticate the request." `ctx.access.aud` = the app's AUD tag; `ctx.access.getIdentity()` returns email/name/groups. Local test via wrangler.jsonc `access.dev { aud, identity }`. Subrequest note: a Worker may "send a `fetch()` subrequest to an Access-protected hostname with valid service token headers or a valid `CF_Authorization` cookie". URL: https://developers.cloudflare.com/workers/configuration/cloudflare-access/
- Quick Access-on-Worker policy options in the Workers dashboard are limited to "Cloudflare account" and "Email domain" (plus emails); "For advanced policy configuration, such as multiple identity providers ... edit the Access application in Zero Trust after you create it." URL: https://developers.cloudflare.com/workers/configuration/cloudflare-access/

**Implications:**
- D30 / D3 cost basis holds: Zero Trust Free = $0, 50-user limit, IdP authentication (enterprise + social, multiple concurrent) included on Free; the paid tier is $7/user/month, so even a 5-admin overflow would be ~$35/month - irrelevant at the ruled admin count. A payment method must still be attached at Zero Trust setup (operator, dashboard).
- D3 primary path (OTP IdP + Independent MFA) is documented as supported: Independent MFA enrollment explicitly allows login "with your identity provider or with a one-time PIN", and MFA is enforced "in addition to IdP authentication". Turn on Independent MFA at ORG level first (Access settings), then set Custom MFA settings on the admin.ironics.org application (or on its Allow policy, which takes precedence). Allowed methods = Security key + Authenticator application; leave Biometrics off unless wanted. There is no ordered "security key, TOTP fallback" - it is a SET of allowed methods the user picks from; only ONE TOTP authenticator per user, multiple security keys allowed (enrol a backup key).
- Do NOT enable "Apply global MFA settings by default" if other Access apps appear later - scope MFA to the admin app/policy. Keep "Use identity provider MFA" OFF (OTP has no AMR; it would never satisfy it anyway). Users must reach the App Launcher (`<team>.cloudflareaccess.com`) without MFA to enrol - that is by design, not a gap.
- Independent MFA plan gating is not documented anywhere fetched; it is not marked Enterprise-only. The operator's 30-minute compatibility check (D3) must confirm the MFA tab appears on the Free-plan application page; if absent, Fallback 1 (Google IdP) is confirmed available on Free and Google needs no Workspace account.
- Set the D3 Access session (1 h) as the application session duration AND set Independent MFA Authentication duration to match (or "Require every login") - they are separate timers.
- Elevate-route verification (api.ironics.org Lambda): fetch JWKS from `https://<team>.cloudflareaccess.com/cdn-cgi/access/certs`, select by `kid` from `public_certs`, RS256, check `iss` == `https://<team>.cloudflareaccess.com`, `aud` (array) contains the app's AUD tag, `exp`/`nbf`. Cache JWKS with refresh-on-unknown-kid; keys rotate every 6 weeks with a 7-day overlap, so a stale cache older than ~7 days after rotation will start failing - refresh at most every few hours and always on kid miss. Store TEAM_DOMAIN and POLICY_AUD as config (SSM), never hard-code the PEM.
- The JWT arrives at the Access-protected ORIGIN (the admin.ironics.org Worker) as `Cf-Access-Jwt-Assertion`; the elevate call to api.ironics.org must carry it explicitly - the CRM Worker should read the header from the incoming request and forward it (e.g. as `Cf-Access-Jwt-Assertion` or `X-Access-Jwt`) on the server-side elevate request; do not rely on the `CF_Authorization` cookie crossing hosts. `ctx.access.getIdentity()` can be used inside the Worker for display/authorisation without parsing, but the Lambda still needs the raw JWT to bind `stepUpSubject` (use JWT `sub` + `email`).
- D7 (apps/crm Worker on admin.ironics.org) is compatible with Access two ways: (a) hostname-based self-hosted app with domain `admin.ironics.org`, or (b) Worker-level protection by worker_id (covers custom domain + workers.dev + previews, "safest and most straightforward"). Prefer (b) or (a)+disable workers.dev so no unauthenticated route to the CRM Worker exists. Custom Domain requires the zone active on Cloudflare and no pre-existing CNAME on `admin.ironics.org`.
- Advanced policy (OTP + Independent MFA, session length) must be edited in Zero Trust > Access controls, not in the Workers-tab quick dialog (which only offers account-membership / email / email-domain).
- OTP operational notes for the runbook: PIN valid 10 min, single-use; email-security link scanners can consume it ("already been used" -> request new code); blocked emails silently get no mail; auth log entries only after a code is submitted; allowlist `noreply@notify.cloudflare.com`. If Google is later added alongside OTP, IdP group claims are not evaluated for sessions that logged in via OTP (no impact - D2 roles live in IronicsAdmins, not IdP groups).
- Docs URL drift: older `/cloudflare-one/identity/...` paths now redirect to `/cloudflare-one/access-controls/applications/http-apps/authorization-cookie/...` and `/cloudflare-one/integrations/identity-providers/...`; cite the new paths in the runbook.

**Sources:** https://www.cloudflare.com/zero-trust/products/access/; https://www.cloudflare.com/sase/products/access/; https://developers.cloudflare.com/cloudflare-one/; https://developers.cloudflare.com/cloudflare-one/setup/; https://developers.cloudflare.com/cloudflare-one/team-and-resources/users/seat-management/; https://developers.cloudflare.com/cloudflare-one/account-limits/; https://developers.cloudflare.com/cloudflare-one/integrations/identity-providers/one-time-pin/; https://developers.cloudflare.com/cloudflare-one/integrations/identity-providers/; https://developers.cloudflare.com/cloudflare-one/identity/idp-integration/google/; https://developers.cloudflare.com/cloudflare-one/access-controls/access-settings/independent-mfa/; https://developers.cloudflare.com/cloudflare-one/access-controls/policies/mfa-requirements/; https://developers.cloudflare.com/changelog/post/2026-04-15-independent-mfa/; https://developers.cloudflare.com/cloudflare-one/identity/authorization-cookie/validating-json/; https://developers.cloudflare.com/cloudflare-one/identity/authorization-cookie/application-token/; https://developers.cloudflare.com/cloudflare-one/identity/authorization-cookie/; https://developers.cloudflare.com/changelog/post/2025-10-03-one-click-access-for-workers/; https://developers.cloudflare.com/workers/configuration/cloudflare-access/; https://developers.cloudflare.com/workers/configuration/routing/custom-domains/; https://developers.cloudflare.com/cloudflare-one/access-controls/applications/choose-application-type/; https://developers.cloudflare.com/changelog/post/2026-08-14-workers-access/; C:\Dev\Bag_Man\Docs\design\IRONICS_PLATFORM_DECISION_LOG_2026-09-06.md (D3 line 41, D7 line 45, D30 line 51, C3 line 86)

### cloudfront-plans: Amazon CloudFront flat-rate pricing plans (Free/Pro/Business/Premium) vs the pay-as-you-go always-free tier; signed URLs/OAC/standard logs v2 on plans; plan switching; S3->CloudFront origin transfer; standard logging v2 cost (verified: True)

- CORRECTION TO THE PREMISE: there are TWO different 'free' things and the decision log conflates them. (a) The flat-rate FREE PLAN is 100 GB + 1 M requests/month: 'Free (USD $0/month) - 1M requests, 100GB transfer' (https://docs.aws.amazon.com/PricingPlanManager/latest/UserGuide/plans.html §Plan Tiers) and the developer-guide allowance table 'Requests | 1 M | 10 M | 125 M | 500 M' / 'Data transfer | 100 GB | 50 TB | 50 TB | 50 TB' (https://docs.aws.amazon.com/AmazonCloudFront/latest/DeveloperGuide/flat-rate-pricing-plan.html §Monthly usage allowances). (b) The 1 TB + 10 M figure is the PAY-AS-YOU-GO Free Tier: 'The CloudFront Free Tier is different – it applies only to CloudFront usage under pay-as-you-go pricing. While it provides 1TB of data transfer and 10 million requests free each month across your account, any usage beyond these limits is billed at standard pay-as-you-go rates.' (https://aws.amazon.com/cloudfront/faqs/, flat-rate plans FAQ).
- Pay-as-you-go Free Tier exact allowance: '1 TB of data transfer out to the internet per month', '10,000,000 HTTP or HTTPS Requests per month', '2,000,000 CloudFront Function invocations per month' (https://aws.amazon.com/cloudfront/pricing/pay-as-you-go/). It is always-free, not 12-month: AWS News Blog 2021-11-24: 'Data Transfer from Amazon CloudFront is now free for up to 1 TB of data per month', 'is no longer limited to the first 12 months after signup', 'This change is effective December 1, 2021' (https://aws.amazon.com/blogs/aws/aws-free-tier-data-transfer-expansion-100-gb-from-regions-and-1-tb-from-amazon-cloudfront-per-month). Free Tier FAQ: 'Services with an Always Free offer allow you to use the product for free up to specified limits as long as you are an AWS customer.' (https://aws.amazon.com/free/free-tier-faqs/). Not confirmed: the CloudFront row on aws.amazon.com/free itself (fetch did not surface it).
- Flat-rate tiers and prices (all primary): 'Free (USD $0/month) - 1M requests, 100GB transfer'; 'Pro (USD $15/month) - 10M requests, 50TB transfer'; 'Business (USD $200/month) - 125M requests, 50TB transfer'; 'Premium (from USD $1,000/month) - 500M requests, 50TB transfer at the default usage level' (https://docs.aws.amazon.com/PricingPlanManager/latest/UserGuide/plans.html). Premium usage levels up to 'CF_PREMIUM_L6 | 600 TB | 6 B | $10,000'. 'Configurable usage allowances are available only on the Premium plan. Usage allowances on Free, Pro, and Business plans are not configurable.' (flat-rate-pricing-plan.html §Configurable usage allowances).
- What every tier bundles: 'CloudFront CDN; AWS WAF and DDoS protection; Bot management and analytics; Amazon Route 53 DNS; Amazon CloudWatch Logs ingestion; TLS certificate; Serverless edge compute; Amazon S3 storage credits each month' (flat-rate-pricing-plan.html intro). 'Each pricing plan covers one CloudFront distribution with up to one apex (root) domain' (§Features by pricing plan tier). Pricing page: 'There are no additional overage charges or usage calculations, even during traffic spikes or attacks.' (https://aws.amazon.com/cloudfront/pricing/).
- Feature matrix, Free vs Pro (flat-rate-pricing-plan.html §Pricing plan features table): Number of cache behaviors 5 (Free) / 10 (Pro) / 50 / 100. 'Custom caching rules ... using cache policies' = Business+ ONLY (blank for Free and Pro); 'Custom origin request rules' and 'Custom response header rules' = Business+ only; Free/Pro get 'Default caching rules' + AWS managed origin-request and response-header policies. Origin Shield / 'Automatic origin failover' = Premium only. 'Number of WAF rules' 5 / 25 / 50 / 75. 'Request body inspection' 16 KB / 16 KB / 64 KB / 64 KB. 'CAPTCHA challenge', 'Header-based threat filtering', 'Custom WAF response', 'Protections for WordPress, PHP, and SQL databases', 'AI traffic analytics' = Pro+. 'JavaScript challenge', 'Regex-based threat filtering', 'Bot management and analytics', 'Advanced DDoS Protection', 'Private origins within VPC', 'Mutual TLS (origin)' = Business+. 'Uptime SLA' = Business+. Route 53 'Records per Hosted Zone' 50 / 100 / 1000 / 5000; 'Additional DNS query allowance per month' 1 M / 5 M / 20 M / 100 M (ALIAS queries to CloudFront 'No limit'). 'Amazon S3 storage' credits 5 GB / 50 GB / 1 TB / 5 TB ('Not limited to CloudFront content or subject to plan usage allowances').
- Signed URLs and OAC on plans: 'Signed URLs ... Create secure URLs that provide temporary access to private content' = Yes on Free, Pro, Business, Premium. 'Origin Access Control (OAC) ... Maintain a private S3 bucket and only allow access through your designated CloudFront distribution' = Yes on all four tiers. 'Free TLS certificate ... through AWS Certificate Manager' = all tiers (flat-rate-pricing-plan.html §Security and Protection rows). Legacy 'Origin access identity (OAI)' is an UNSUPPORTED feature on plans -> 'Use Origin access control (OAC)' (§Unsupported features table).
- Access/standard logs on plans: table row 'Access Logs ... with Amazon CloudWatch Logs ingestion is included at no extra cost' is BLANK for Free and Yes for Pro/Business/Premium; same for 'WAF request logs' (flat-rate-pricing-plan.html §Logging and Monitoring). The public pricing page renders the Logging row as 'x' for Free and check for Pro/Business/Premium (https://aws.amazon.com/cloudfront/pricing/, plan comparison). 'Real-time access logs' are UNSUPPORTED on every tier: alternative 'Use standard access logs or pay-as-you-go pricing'; 'If your current distribution uses any unsupported features, you must disable those features before you can subscribe to the pricing plan. This includes disabling features like real-time access logs.' (§Subscribe an existing distribution). Plan note: 'Your plan includes Amazon CloudWatch Logs ingestion for CloudFront standard logs (access logs) and WAF logs for no added costs. All other CloudWatch costs such as storage and querying are not covered by your plan.' Items billed EXTRA on a plan: 'AWS WAF log delivery to Amazon S3', 'CloudFront or AWS WAF log delivery to Amazon Data Firehose', 'Additional CloudWatch metrics for CloudFront', 'CloudFront access logs in Parquet format', 'Lambda@Edge function invocations' (§Additional features that can affect your pricing plan). CloudFront standard-log delivery to S3 is NOT in that extra-charge list.
- Edge compute on plans: 'Serverless edge compute ... using CloudFront Functions. Lambda@Edge can also be used with all plan tiers, but unlike CloudFront Functions, Lambda@Edge invocations are billed on a pay-as-you-go basis.' = Yes all tiers; 'Edge key-value store' (KeyValueStore) = Pro+ (blank on Free). Unsupported associations: 'CloudFront Functions', 'CloudFront Functions associated with a key value store', 'AWS WAF Web ACLs' that are already associated with OTHER distributions cannot be shared with a plan distribution ('Resources that are associated to a distribution that is subscribed to a pricing plan can only be used for that distribution') (§Unsupported associations).
- Allowances are soft, no overage bill: 'Usage allowances are not hard limits'; 'Most importantly: you will not incur overage charges, regardless of how much you exceed your allowance.'; 'Your first traffic spike up to 3x your monthly allowance won't affect your service that month.'; 'Sustained usage above your allowance is evaluated over 2-3 months or more, not immediately.'; 'If you continue to substantially exceed your plan's usage allowance without upgrading, we may adjust how we deliver your traffic. For example, we might serve your traffic from fewer or more distant edge locations or adjust performance.' Emails at 50%, 80%, 100%. 'Blocked DDoS attacks and requests blocked by AWS WAF never count against your usage allowance.' (flat-rate-pricing-plan.html §What happens when usage exceeds the allowance).
- Eligibility / quotas: 'Pricing plans per AWS account 100; Free plans per AWS account 3; Apex-level domains per plan 1' and 'These quotas can't be increased for your AWS account.' (§Flat-rate pricing plan quotas). Account-level constraints: not eligible if 'You reached the maximum number of subscriptions allowed' or 'Your account is using AWS Free Tier.' (§Account-level constraints); PricingPlanManager guide: 'Free Tier accounts cannot use CloudFront Flat-Rate Plans.' (plans.html §Account Requirements). 'Your historical CloudFront usage may affect your eligibility to sign up for or downgrade to specific plan tiers. If your recent usage exceeds a plan tier's usage allowances, you may need to select a higher tier' (§Eligibility based on historical usage). Resource constraints: distribution with AWS Shield Advanced or Firewall Manager-managed web ACL not eligible. 'Amazon CloudFront flat-rate pricing plans may not be combined with any other offers, promotions, or discounts.' (§Pricing plans vs. pay-as-you-go pricing). Whether AWS promotional credits can pay the $15 was NOT confirmed. The re:Post thread on a Pro-plan signup error returned HTTP 403 (not read).
- WAF web ACL is MANDATORY on any plan incl. Free: 'You must have a AWS WAF Web ACL associated with your distribution if you're using a pricing plan. This resource cannot be removed or disassociated from your distribution unless you switch to pay-as-you-go pricing for that distribution.' (§Unsupported features note). API: 'An AWS WAF web ACL ARN (required) — a plan requires an associated web ACL. You must create the web ACL before you create the subscription; the API doesn't create one for you. The web ACL must be in the CLOUDFRONT scope (Region us-east-1).' (https://docs.aws.amazon.com/PricingPlanManager/latest/UserGuide/getting-started-pricingplanmanager-api.html §Required and optional resources). Other unsupported-on-plan features: Multi-tenant distributions; Continuous deployment/Staging distributions; Anycast IP list; WAF Targeted Bots, Partner Managed Rules, Account Creation Fraud Prevention, Account Takeover Protection, Rule Groups; legacy ForwardedValues, Dedicated IP/SSL, Field level encryption, IAM server certificates, Legacy cache settings (§Unsupported features table).
- Moving between plans (console): 'Select a flat-rate plan when creating a new CloudFront distribution or switch an existing distribution from pay-as-you-go pricing.' (pricing page). Upgrade: 'When you upgrade to a higher plan tier, changes take effect immediately. Your price and usage allowance are prorated.' FAQ: 'Plan signups and upgrades are billed at a prorated price and given a prorated usage allowance'. Downgrade: 'If you downgrade to a lower tier, your billing changes will take effect at the beginning of the next billing cycle.'; 'If your distribution currently exceeds the usage allowance for a plan, you can downgrade once your usage is within the usage allowance for your desired tier.' Cancel: 'When you cancel a paid pricing plan, you will maintain your flat-rate price through the end of your current billing cycle. Your distribution and all associated plan resources will then switch to pay-as-you-go pricing at the start of the next billing cycle. Free plans are cancelled immediately.' 'You can't delete a distribution that is subscribed to a pricing plan. You must first cancel the pricing plan.' 'You can disable a distribution that is subscribed to a pricing plan, but you will still incur charges for that plan.' Pending downgrade/cancel can be reversed via 'Cancel a pending plan change'. (flat-rate-pricing-plan.html §Manage your flat-rate pricing plans). Blog: customers can 'mix flat-rate plans and pay-as-you-go pricing across different distributions' (https://aws.amazon.com/blogs/networking-and-content-delivery/cloudfront-premium-flat-rate-plan-supports-configurable-usage-allowances/, 2026-05-12).
- Moving between plans (API, announced 2026-09-03: 'Starting today, customers can subscribe and manage flat-rate pricing plans programmatically using the AWS CLI, AWS SDKs, CloudFormation, CDK, or the PricingPlanManager API.' https://aws.amazon.com/about-aws/whats-new/2026/09/cloudfront-flat-rate-pricing-plans-api/): endpoint us-east-1 only; operations CreateSubscription (--plan-family CloudFront --plan-tier FREE|PRO|BUSINESS|PREMIUM --resource-arns <distribution ARN> <web ACL ARN> [--approval-mode MANUAL|IMMEDIATE, default MANUAL]), ApprovePaidSubscription, GetSubscription, ListSubscriptions, UpdateSubscription, CancelSubscription, CancelSubscriptionChange, AssociateResourcesToSubscription (Route 53 hosted zone ARN, KeyValueStore ARN), DisassociateResourcesFromSubscription; ETag/--if-match optimistic concurrency. 'Free plan subscriptions are always activated immediately'; paid plans in MANUAL mode sit PENDING_APPROVAL with 'AWS charges usage as pay-as-you-go until you approve'. 'You cannot cancel or revert an active paid subscription during the current billing period (calendar month).' Downgrades: 'Feature availability changes immediately to match the new (lower) tier, but your usage allowances and billing remain at your current tier until the effective date.' Recommended IAM split: automation gets only pricingplanmanager:CreateSubscription; ApprovePaidSubscription reserved for human roles. (getting-started-pricingplanmanager-api.html §Two-phase subscription creation, §Scheduled changes, §Operations reference).
- S3 -> CloudFront origin transfer is free (three primary statements): pay-as-you-go page: 'Free for origin fetches from any AWS origin such as Amazon Simple Storage Service (S3), Amazon Elastic Compute Cloud (EC2), or Elastic Load Balancers' (https://aws.amazon.com/cloudfront/pricing/pay-as-you-go/); FAQ: 'if you are using an AWS origin (e.g., Amazon S3, Amazon EC2, etc.), we no longer charge for AWS data transfer out to Amazon CloudFront. This applies to data transfer from all AWS regions to all global CloudFront edge locations.' (https://aws.amazon.com/cloudfront/faqs/); S3 pricing free-transfer list: 'Data transferred out to Amazon CloudFront (CloudFront).' (https://aws.amazon.com/s3/pricing/). On plans: 'Data transfer from AWS applications running on services such as Amazon S3, AWS Application Load Balancer (ALB), or Amazon API Gateway to CloudFront continues to be free.' (flat-rate-pricing-plan.html §Benefits). Still billed on origin fetches: S3 GET requests '$0.0004 per 1,000 requests' (S3 Standard, us-east-1, s3/pricing) — negligible for multi-GB objects.
- CloudFront standard logging v2 -> S3 cost: 'CloudFront doesn't charge for enabling standard logs. However, you can incur charges for the delivery, ingestion, storage or access, depending on the log delivery destination that you select.' and 'There are no additional charges for log delivery to Amazon S3, though you incur Amazon S3 charges for storing and accessing the log files. If you enable the Parquet option to convert your access logs to Apache Parquet, this option incurs CloudWatch charges.' (https://docs.aws.amazon.com/AmazonCloudFront/latest/DeveloperGuide/standard-logging.html §Pricing). Pay-as-you-go page: 'No additional charge for enabling standard access logs for CloudFront.' Real-time logs (unsupported on plans anyway): 'You pay $0.01 for every 1,000,000 log lines that CloudFront publishes to your log destination.' CloudWatch pricing: 'For each CloudFront request, you get 750 bytes of logs delivery to CloudWatch Logs at no additional charge. Any overages incur Amazon CloudWatch Logs charges.' (https://aws.amazon.com/cloudwatch/pricing/); vended-log delivery to CloudWatch Logs is tiered '0 to 10TB @$0.50 per GB' down to '50TB to 72TB @$0.05 per GB' (same page, worked example). Mechanics: v2 uses CloudWatch vended logs; 'you must specify the US East (N. Virginia) Region (us-east-1)' for the CloudWatch API; 'you can only have one delivery source per distribution'; output format (JSON/Plain/w3c/Raw/Parquet-S3-only) 'can only set ... when you first create the delivery destination'; 'Your changes to logging settings take effect within 12 hours'; S3 partitioning variables {DistributionId}/{yyyy}/{MM}/{dd}/{HH} and Hive-compatible paths supported (standard-logging.html).
- Pay-as-you-go unit prices for the crossover math: US/Mexico/Canada data transfer out 'First 1TB: Free' then $0.085/GB for the next 9 TB, $0.080/GB next 40 TB; 'HTTPS requests: $0.0100' per 10,000 after the first 10 M; CloudFront Functions $0.10 per 1 M after 2 M free; invalidations first 1,000 paths free (https://aws.amazon.com/cloudfront/pricing/pay-as-you-go/). Pay-as-you-go WAF is NOT bundled (on plans the web ACL, rules and request fees are covered: 'The AWS WAF web ACL, custom rules, AWS Managed Rules, and request fees for the web ACL associated with your distribution' §Costs covered by your plan).
- Local doc state that this contradicts: C:\Dev\Bag_Man\Docs\design\IRONICS_PLATFORM_DECISION_LOG_2026-09-06.md:28 (D-DIST-1) says 'on the CloudFront FREE pricing plan' and 'The CloudFront free allowance is 1 TB/month of egress and 10 M requests (always-free), i.e. ~250 downloads/month at 4 GB for $0; Pro ($15) covers the ramp to 1,000 (verify the current plan limits at implementation)'; :30 (D-DIST-3) 'CloudFront standard logs v2 -> private S3'; :74 (D25) 'edge logs per D-DIST-3 (no longer deferred - free plan)'; :85-86 (C2/C3) 'CloudFront $0 -> $15' and 'CloudFront Free plan' listed as a free-tier vendor. C:\Dev\Bag_Man\Docs\PLATFORM_GROUNDING_BRIEF_2026-09-06.md:23 (R2a) 'Egress: AWS free tier = 100 GB/month out, then $0.09/GB' describes S3-direct egress (the current presigned-S3 path), not CloudFront.

**Implications:**
- D-DIST-1 as written is unsafe as a cost model: the flat-rate FREE PLAN gives 100 GB + 1 M requests, i.e. ~25 downloads/month of the 3.9 GB 0.1.3 zip (~40 at 2.5 GB) — not '~250'. The 1 TB / 10 M always-free allowance only exists under PAY-AS-YOU-GO. Re-word D-DIST-1 to: Tier 0 = CloudFront distribution on pay-as-you-go (no plan), OAC + key-group signed URLs, using the always-free 1 TB/10 M tier; adopt the Pro plan ($15) as the scaling step.
- D-DIST-3 / D25 conflict with the flat-rate Free plan: access logs are NOT included on Free (blank/'x'), so 'edge logs ... free plan' cannot both be true. On pay-as-you-go, standard logging v2 -> S3 is free to deliver (S3 storage only; avoid Parquet, which bills CloudWatch); on Pro it is included. Telemetry works on PAYG or Pro, not on the flat-rate Free plan.
- Crossover point for the $15 Pro plan on egress alone: PAYG cost = (egress - 1 TB) x $0.085/GB; Pro is cheaper once monthly egress exceeds ~1 TB + 176 GB ≈ 1.18 TB (~300 downloads at 3.9 GB, ~470 at 2.5 GB). PAYG past the free tier costs ~$0.33 per 3.9 GB download, so the $50 cap funds ~1 TB free + ~150 paid downloads ≈ 400/month; Pro converts that to a flat $15 with 'no overage charges' up to 50 TB (~12,800 downloads at 3.9 GB) — a better fit for the $50 cap once past ~1 TB/month. Set the D-DIST-6 BytesDownloaded alarm threshold at ~0.9 TB/month as the 'flip to Pro' trigger.
- Pre-stage the Pro flip in CDK now so it is one call later: (1) create a WAF web ACL in CLOUDFRONT scope (us-east-1) with the AWS managed Common + IP-reputation + KnownBadInputs rule groups (<= 25 rules on Pro) and attach it to the download distribution (WAF billed on PAYG until the plan is active — or leave unattached until the flip to avoid PAYG WAF fees: $5/ACL + $1/rule + $0.60/M requests; decide); (2) build the distribution with OAC (never OAI), AWS-managed cache/origin-request/response-header policies only (custom policies are Business+), no real-time logs, no ForwardedValues, no continuous-deployment staging, a dedicated (unshared) CloudFront Function if any; (3) add PricingPlanManager CreateSubscription (MANUAL approval) to the deploy tooling and keep pricingplanmanager:ApprovePaidSubscription OFF the `ironics-agent-deploy` principal (D5) — the operator approves the $15 in person; the API is us-east-1 only.
- Commitment semantics to write into the release runbook: the Pro month is committed once approved ('cannot cancel or revert an active paid subscription during the current billing period'); downgrade/cancel only takes effect at the next calendar month and Pro-only features drop immediately on a downgrade request; a distribution under a plan cannot be deleted (cancel the plan first — Free cancels immediately, paid at month end) and a disabled distribution still bills the plan; historical usage can block a downgrade to Free.
- OPERATOR check before any flat-rate subscription (incl. the Free plan): the AWS account must not be an 'AWS Free Tier' account ('Free Tier accounts cannot use CloudFront Flat-Rate Plans'); confirm the account plan in Billing. Also unverified whether the account's promotional credits can pay the CloudFrontPlans $15 line ('may not be combined with any other offers, promotions, or discounts') — assume the $15 is real cash inside the $50 cap until Cost Explorer shows otherwise.
- Signed URLs (key groups) and OAC are supported on every tier including Free and Pro, and S3->CloudFront origin transfer is free on both PAYG and plans, so the D2 Tier 0 architecture (OAC + key-group 24 h signed URLs on the existing ironics-releases bucket) is valid under either pricing mode; only the pricing-mode label and the download-count math in D-DIST-1/C2/C3 need correcting ('CloudFront pay-as-you-go always-free tier', not 'CloudFront Free plan').
- Standard logging v2 configuration notes for D-DIST-3: enable through the CloudWatch Logs delivery API in us-east-1 (PutDeliverySource/PutDeliveryDestination/CreateDelivery or the CloudFormation AWS::Logs::Delivery* resources); one delivery source per distribution; pick the output format (Plain/w3c/JSON) at creation — it cannot be changed later; use {DistributionId}/{yyyy}/{MM}/{dd}/{HH} partitioning + Hive-compatible paths to match the Athena partition-projection design; changes propagate within 12 hours; skip Parquet.

**Sources:** https://aws.amazon.com/cloudfront/pricing/; https://aws.amazon.com/cloudfront/pricing/pay-as-you-go/; https://aws.amazon.com/cloudfront/faqs/; https://docs.aws.amazon.com/AmazonCloudFront/latest/DeveloperGuide/flat-rate-pricing-plan.html; https://docs.aws.amazon.com/PricingPlanManager/latest/UserGuide/plans.html; https://docs.aws.amazon.com/PricingPlanManager/latest/UserGuide/getting-started-pricingplanmanager-api.html; https://aws.amazon.com/about-aws/whats-new/2026/09/cloudfront-flat-rate-pricing-plans-api/; https://aws.amazon.com/blogs/networking-and-content-delivery/cloudfront-premium-flat-rate-plan-supports-configurable-usage-allowances/; https://aws.amazon.com/blogs/aws/aws-free-tier-data-transfer-expansion-100-gb-from-regions-and-1-tb-from-amazon-cloudfront-per-month; https://aws.amazon.com/free/free-tier-faqs/; https://docs.aws.amazon.com/AmazonCloudFront/latest/DeveloperGuide/standard-logging.html; https://aws.amazon.com/cloudwatch/pricing/; https://aws.amazon.com/s3/pricing/; C:\Dev\Bag_Man\Docs\design\IRONICS_PLATFORM_DECISION_LOG_2026-09-06.md:28,30,33,51,74,85-86; C:\Dev\Bag_Man\Docs\PLATFORM_GROUNDING_BRIEF_2026-09-06.md:22-23; Not readable (HTTP 403): https://repost.aws/questions/QUf65WRe97S6mlBOGZklSxAA/error-moving-to-cloudfront-pro-plan

### opennext-second-worker: apps/crm as a second OpenNext Worker on admin.ironics.org (custom_domain, Workers Free vs Paid, shared tokens package in npm workspaces, cookies/CORS against api.ironics.org) (verified: True)

- DISK — apps/web is one OpenNext Worker: wrangler.jsonc name "ironics-web", main ".open-next/worker.js", compatibility_date "2026-08-11", flags ["nodejs_compat","global_fetch_strictly_public"], assets binding ASSETS, images binding IMAGES, routes [{"pattern":"ironics.org","custom_domain":true}] (apex only; www is meant to 301 via a zone Redirect Rule, not declarable in wrangler) — C:\Dev\Ironics-Platform\apps\web\wrangler.jsonc:3-35. No `env` blocks, no vars/secrets, observability logs on at 100%, traces off (wrangler.jsonc:57-71).
- DISK — root package.json workspaces = ["apps/*"] only; scripts run `--workspaces --if-present` (C:\Dev\Ironics-Platform\package.json:5-13). A `packages/tokens` package requires adding "packages/*" to that array. No `packages/` directory exists today (Glob 2026-09-06).
- DISK — apps/crm/README.md: the slot is reserved; 'Do not add it to apps/web' (JS budget is a CI merge gate); 'Shared tokens. Brand colours and type live in apps/web/lib/tokens.ts and apps/web/tailwind.config.ts ... when the CRM needs them, lift them to packages/tokens and have both apps import it' — C:\Dev\Ironics-Platform\apps\crm\README.md:21-27. 15 files in apps/web import `@/lib/tokens` (grep); tsconfig alias `"@/*": ["./*"]` (apps/web/tsconfig.json:17); tailwind.config.ts does NOT import lib/tokens and its `content` globs are only ./app, ./components, ./lib (tailwind.config.ts:1,13).
- DISK — deploy-web.yml triggers only on paths apps/web/**, package.json, package-lock.json, and itself (.github/workflows/deploy-web.yml:7-19); deploy runs `working-directory: apps/web` → `npx opennextjs-cloudflare build && npx opennextjs-cloudflare deploy` with CLOUDFLARE_API_TOKEN / CLOUDFLARE_ACCOUNT_ID repo secrets and hard-coded NEXT_PUBLIC_STATS_URL / NEXT_PUBLIC_EPIC_ENABLED (deploy-web.yml:120-189); concurrency group `web-${{ github.ref }}` (:29-31); `environment: production` (:125-127). Other workflows present: deploy-api.yml, probe-cloudflare-tokens.yml, verify-cloudflare.yml.
- DISK — the CI token gap is recorded: `wrangler deploy` uploads then fails on GET /zones/05d1bd7f17b4e02909f28f2e023c71ea/workers/routes with 'Authentication error [code: 10000]'; fix = add Zone → Workers Routes → Edit on zone ironics.org (apps/web/LEGAL_OPEN_ITEMS.md:67-95). probe-cloudflare-tokens.yml documents the four calls wrangler makes to attach a custom domain: token verify, zones?name=ironics.org, zones/{id}/workers/routes ('the call that fails the deploy'), accounts/{id}/workers/domains (.github/workflows/probe-cloudflare-tokens.yml:11-12,41-68,74).
- DISK — apps/web versions: next ^15.5.23, react 19.1.0, @opennextjs/cloudflare ^1.20.2, wrangler ^4.123.0 (apps/web/package.json:21-38); scripts cf:build/cf:preview/cf:deploy/cf:typegen (:12-15). next.config.mjs calls initOpenNextCloudflareForDev() and sets images.formats ["image/webp"]; no transpilePackages, no outputFileTracingRoot (apps/web/next.config.mjs:1-27). open-next.config.ts uses static-assets-incremental-cache because SSG routes (/legal/[slug]) 404 without an incremental cache (apps/web/open-next.config.ts:4-33).
- VENDOR (Cloudflare custom domains, https://developers.cloudflare.com/workers/configuration/routing/custom-domains/) — 'Custom Domains allow you to connect your Worker to a domain or subdomain, without having to make changes to your DNS settings or perform any certificate management. After you set up a Custom Domain for your Worker, Cloudflare will create DNS records and issue necessary certificates on your behalf.' Config: routes: [{ "pattern": "shop.example.com", "custom_domain": true }]. 'You cannot create a Custom Domain on a hostname with an existing CNAME DNS record or on a zone you do not own.' 'A Worker running on a Custom Domain is treated as an origin.' Custom Domains are 'routes to a domain or subdomain (such as example.com or shop.example.com) within a Cloudflare zone.'
- VENDOR (Workers limits, https://developers.cloudflare.com/workers/platform/limits/) — 'Accounts on the Workers Free plan have a daily request limit of 100,000 requests, resetting at midnight UTC.' (account-wide, so a second Worker shares the same 100k/day). Request limit: Free 100,000/day, Paid 'No limit'. CPU time per HTTP request: Free '10 ms', Paid '5 min (default: 30 seconds)', raised via `"cpu_ms"` in wrangler config; exceeding CPU → 'Cloudflare returns Error 1102 to the client with the message Worker exceeded resource limits'; 'The average Worker uses approximately 2.2 ms per request.' Workers per account: Free 100, Paid 500. Subrequests: Free 50/request, Paid 10,000. Memory 128 MB. Custom domains per zone: 100. Worker size: 64 MiB both plans — 'There is no compressed size limit. Only the uncompressed bundle size counts.'
- VENDOR (Workers size changelog, https://developers.cloudflare.com/changelog/post/2026-09-04-increased-worker-size-limit/, dated 2026-09-04) — 'Previously, Cloudflare checked that compressed size and rejected deploys over 3 MB (Free) or 10 MB (Paid).' 'Cloudflare now only checks the uncompressed size of your bundle, which is 64 MiB across all plans.' 'The gzip value is shown for reference but is no longer a limit.' The 3 MiB-gzip-on-Free constraint cited by third-party monorepo articles (e.g. dev.to/lewiskori, Apr 2026) is therefore obsolete.
- VENDOR (Workers pricing, https://developers.cloudflare.com/workers/platform/pricing/) — Free: '100,000 per day' requests, '10 milliseconds of CPU time per invocation', 'No charge for duration'; when the daily limit is exceeded 'further operations of that type will fail with an error'. Paid: $5 USD minimum monthly subscription per account; '10 million included per month +$0.30 per additional million' requests; '30 million CPU milliseconds included per month +$0.02 per additional million CPU milliseconds'. The $5 covers the whole account's Workers, not per Worker — so a second Worker is $0 incremental on either plan (matches D7 'second Worker, $0').
- UNCONFIRMED — which Workers plan the Cloudflare account is on is not recorded anywhere on disk (grep for 'Workers Paid|Paid plan|Free plan' over Ironics-Platform *.md = only DEPLOYMENT.md:98 workers.dev mention). The admin scope's 'Workers Paid $5/month min already implied' (IRONICS_ADMIN_DASHBOARD_SCOPE.md:261) is an inference, not a verified fact. On Free, OpenNext SSR pages must stay under 10 ms CPU or return 1102.
- VENDOR (OpenNext Cloudflare, https://opennext.js.org/cloudflare and /cloudflare/get-started) — 'All minor and patch versions of Next.js 16 and the latest minors of Next.js 14 and 15 are supported. Next.js 14 support will be dropped Q1 2026.' Node.js runtime only ('Remove any export const runtime = "edge"'); Node Middleware (15.2) not supported. Required wrangler: main .open-next/worker.js, compatibility_date >= 2024-09-23, nodejs_compat + global_fetch_strictly_public, assets .open-next/assets binding ASSETS; wrangler >= 3.99.0. Neither page contains monorepo/workspace guidance. /cloudflare/known-issues lists only the Durable Objects build warning. /cloudflare/howtos/ index returns 404. /cloudflare/howtos/multi-worker is about splitting ONE app across Workers via service bindings and 'cannot be used with ... The standard @opennextjs/cloudflare deploy command' — not the two-independent-apps pattern; two apps each run the standard build+deploy from their own directory, exactly as deploy-web.yml:151-189 already does.
- VENDOR (Next.js transpilePackages, https://nextjs.org/docs/app/api-reference/config/next-config-js/transpilePackages, v16.3.4 page) — 'Turbopack transpiles workspace packages (npm, pnpm, or Yarn workspaces) in your monorepo automatically under both routers. Webpack does the same for the App Router.' Add to transpilePackages when 'A node_modules dependency ships raw TypeScript or JSX' or (Pages Router + webpack) 'the dependency's source lives outside the next app's directory. For example, an apps/web app importing packages/ui in the same monorepo.' Values are package names only ('Paths and glob patterns are not supported'); a package cannot be in both transpilePackages and serverExternalPackages.
- VENDOR (Next.js output tracing, https://nextjs.org/docs/app/api-reference/config/next-config-js/output) — 'While tracing in monorepo setups, the project directory is used for tracing by default. For next build packages/web-app, packages/web-app would be the tracing root and any files outside of that folder will not be included. To include files outside of this folder you can set outputFileTracingRoot in your next.config.js.' (OpenNext consumes the standalone/traced build: 'adapting a Next.js application built via next build (in standalone mode)' — github.com/opennextjs/opennextjs-cloudflare README.) A TS-only tokens package that Next bundles is not affected; runtime node_modules pulled in from packages/* are.
- VENDOR (wrangler issue, https://github.com/cloudflare/workers-sdk/issues/13925, closed with docs PR #13979, wrangler 4.81.0) — top-level `routes` with `custom_domain: true` is an inheritable key: an `env.<name>` block that does not override `routes` 'silently inherits parent' and 'every wrangler deploy --env <name> re-asserts the inherited custom domain on the env Worker, which steals it away from the top-level Worker on every deploy.' Not a risk today (no env blocks in wrangler.jsonc) but a trap if crm/web add staging envs.
- LIVE DNS (Resolve-DnsName, 2026-09-06) — admin.ironics.org: 'DNS name does not exist' for A and CNAME (no conflicting record → custom_domain can be created); ironics.org: A 172.67.135.5 / 104.21.6.162 (Cloudflare proxied); api.ironics.org: CNAME d-or63b861pd.execute-api.us-east-1.amazonaws.com (DNS-only, not proxied, per apps/api/lib/api.ts:437 'CNAME api.ironics.org at this (DNS only, not proxied)'); www.ironics.org: NO A/CNAME record — so the www→apex Redirect Rule described in wrangler.jsonc:30-34 and the 'Add custom domain ... www.ironics.org' step in DEPLOYMENT.md:95-96 are not in effect (side observation).
- DISK cookies — ironics_session and ironics_refresh are set `HttpOnly; Secure; SameSite=Lax` with `Domain=<COOKIE_DOMAIN>` (default `.ironics.org`), refresh additionally `Path=/v1/auth` (apps/api/src/lib/http.ts:7-38; AWS-SETUP.md:269-272). Comment: 'the site's own fetch to a sibling subdomain is same-site' (http.ts:14-15). The web console gate rebuilds the cookie header server-side and fetches ${API_ORIGIN}/v1/admin/me with cache:"no-store" (apps/web/lib/admin-gate.ts:22-42); API_ORIGIN = NEXT_PUBLIC_API_ORIGIN || https://api.ironics.org (apps/web/lib/api.ts:12).
- VENDOR (MDN Set-Cookie, https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/Set-Cookie) — Domain: 'If omitted, the cookie is returned only to the host that set it (i.e., it becomes a "host-only cookie") ... the cookie is not made available to subdomains of the host.' 'if a domain is specified, then subdomains are always included.' SameSite=Strict: 'Send the cookie only for requests originating from the same site that set the cookie.' Lax: same site plus cross-site top-level navigations with safe methods. None: 'The Secure attribute must also be set when using this value.' MDN Glossary Site (https://developer.mozilla.org/en-US/docs/Glossary/Site): site = scheme + registrable domain (public-suffix entry + one label); 'https://developer.mozilla.org' and 'https://support.mozilla.org' are the same site; 'the browser must only send SameSite cookies to the same site that set them.' ⇒ https://admin.ironics.org → https://api.ironics.org is SAME-SITE (both https, registrable domain ironics.org) but CROSS-ORIGIN: Lax AND Strict cookies are sent; SameSite=None is NOT needed; third-party-cookie/ITP blocking does not apply.
- VENDOR (MDN CORS, https://developer.mozilla.org/en-US/docs/Web/HTTP/Guides/CORS) — 'To ask for a fetch() request to include credentials, set the credentials option to "include".' The browser 'will reject any response that does not have the Access-Control-Allow-Credentials header set to true'. 'When responding to a credentialed request: The server must not specify the * wildcard for the Access-Control-Allow-Origin response-header value, but must instead specify an explicit origin' (same for Allow-Headers and Allow-Methods). 'Third-party cookie policies may prevent third-party cookies being sent in requests ... The default policy differs between browsers, but may be set using the SameSite attribute.'
- DISK + VENDOR CORS config — the HTTP API sets `corsPreflight: { allowOrigins: [process.env.SITE_ORIGIN ?? "https://ironics.org"], allowMethods: [GET,POST,OPTIONS], allowHeaders: [content-type, authorization, idempotency-key, cf-turnstile-response], allowCredentials: true, maxAge: 1h }` with the comment 'the origin must be exact — * is not permitted alongside credentials' (apps/api/lib/api.ts:250-261); SITE_ORIGIN is not set in the deploy workflow (AWS-SETUP.md:271; grep). AWS (https://docs.aws.amazon.com/apigateway/latest/developerguide/http-api-cors.html): 'If you configure CORS for an API, API Gateway automatically sends a response to preflight OPTIONS requests ... For a CORS request, API Gateway adds the configured CORS headers to the response from an integration.' 'If you configure CORS for an API, API Gateway ignores CORS headers returned from your backend integration.' 'To return CORS headers, your request must contain an origin header.' allowOrigins accepts explicit origins, `*`, `https://*`, `http://*`. ⇒ the admin origin can only be admitted by changing the CDK allowOrigins array and deploying IronicsPortalStack; a Lambda cannot add it.
- LIVE PROBE (curl, 2026-09-06, read-only GET/OPTIONS on public /v1/stats) — Origin https://ironics.org: GET 200 with `access-control-allow-origin: https://ironics.org` + `access-control-allow-credentials: true`; OPTIONS 204 with allow-methods GET,OPTIONS,POST, allow-headers authorization,cf-turnstile-response,content-type,idempotency-key, max-age 3600. Origin https://admin.ironics.org: GET 200 and OPTIONS 204 with NO access-control-* headers at all. No `Vary: Origin` header observed on either. ⇒ today every credentialed browser call from admin.ironics.org to api.ironics.org would be blocked by CORS until allowOrigins is widened.
- UNCONFIRMED — whether API Gateway HTTP API echoes the matching origin when `allowOrigins` holds two explicit entries is not stated in the developer guide or the apigatewayv2 API reference (Cors.allowOrigins: 'Represents a collection of allowed origins.' — https://docs.aws.amazon.com/apigatewayv2/latest/api-reference/apis-apiid.html). Must be asserted on the live stack after deploy (re-run the probe above with both origins).
- DECISION LOG cross-check — D4 rules the admin token cookie 'host-only on api.ironics.org, Path=/v1/admin, SameSite=Strict' (IRONICS_PLATFORM_DECISION_LOG_2026-09-06.md:42). Per MDN host-only semantics that cookie is never sent to admin.ironics.org, so the crm Worker cannot forward it server-side the way admin-gate.ts forwards ironics_session; elevated /v1/admin/* calls must originate in the browser with credentials:"include". The Set-Cookie that mints it must therefore be received by the BROWSER from api.ironics.org (a cookie with Domain=api.ironics.org cannot be set by a response from admin.ironics.org — the same rule apps/web/lib/api.ts:5-10 and apps/api/lib/api.ts:403-408 already rely on). The log/scope describes the elevate handler as a Next route handler on the admin origin 'forwarding Cf-Access-Jwt-Assertion' (IRONICS_ADMIN_DASHBOARD_SCOPE.md:97, :250) — a server-side call whose Set-Cookie would land in the Worker, not the browser. This is an unspecified seam, not a re-opened ruling.
- VENDOR (Cloudflare Access for Workers, https://developers.cloudflare.com/workers/configuration/cloudflare-access/ and changelog https://developers.cloudflare.com/changelog/post/2026-08-14-workers-access/) — enabling Access on a Worker (Workers & Pages → Worker → Access tab → 'Protect this Worker behind Access'; `access` block in wrangler.jsonc for local dev) 'automatically protects every domain associated with the Worker, including its routes, Custom Domains, workers.dev hostname, and previews.' Destination types: worker, preview_worker, all_workers, all_preview_workers; hostname-based Access uses self-hosted applications. 'When Cloudflare Access authenticates a request that directly invokes your Worker, the Worker can read the signed-in user's identity ... through ctx.access' / 'ctx.access.getIdentity() ... no manual JWT validation required.' Authorization cookie page (https://developers.cloudflare.com/cloudflare-one/identity/authorization-cookie/): CF_Authorization is set on the team domain AND 'on the domain protected by Access' (application domain), HttpOnly/SameSite admin-configurable — it is a separate cookie from ironics_session and lives on admin.ironics.org, not api.ironics.org. The `Cf-Access-Jwt-Assertion` header sentence was NOT present on the pages fetched (application-token page says only 'Cloudflare Access includes the application token with all authenticated requests to your origin') — UNCONFIRMED verbatim for Worker-destination apps.
- THIRD-PARTY (not primary; dev.to/lewiskori 'Deploying a Next.js Monorepo to Cloudflare Workers', Nx 22 + pnpm, Apr 2026) — 'variables set in wrangler.jsonc ... do not automatically appear in process.env unless your compatibility_date is 2025-04-01 or later' (web's 2026-08-11 already satisfies); 'Preview URLs share the same environment variables as production'; the cookies() API from next/headers 'will not work' in middleware.ts on Workers. The article's '3 MiB gzip limit' is superseded by the 2026-09-04 changelog.

**Implications:**
- D7 (apps/crm on admin.ironics.org as a second Worker) is vendor-feasible exactly as ruled: add apps/crm/wrangler.jsonc with name "ironics-crm", main .open-next/worker.js, the same two compatibility flags and a compatibility_date pinned like web's, assets binding, and routes [{"pattern":"admin.ironics.org","custom_domain":true}]; admin.ironics.org has no DNS record today, so Cloudflare will create the record + cert on first deploy. Omit the `images` binding unless the console uses next/image. Do not introduce wrangler `env` blocks without overriding `routes` in each (issue #13925).
- D6 is a hard prerequisite, confirmed by the probe workflow: attaching a NEW custom domain via CI runs the same zones/{id}/workers/routes call that returns code 10000 today, so the OPERATOR token edit (Zone → Workers Routes → Edit on ironics.org) must land before the first crm deploy, or the domain attach fails while the script uploads (the same red-but-shipping state web has been in).
- Cost: $0 incremental on either plan ($5 Paid covers all Workers per account; Free charges nothing). Risk on Free: the 100k requests/day cap is per ACCOUNT (shared with ironics.org) and CPU is 10 ms/request with Error 1102 on overrun — OpenNext SSR pages must be measured against that. Action for the operator/agent: read the account's Workers plan from the dashboard (not on disk) before assuming Paid; if Free, keep console pages light or budget the $5 Paid plan inside the $50 cap (C2/C3 allow it only if ruled; currently 'zero new paid vendors').
- Worker size is no longer a constraint for the console bundle: 64 MiB uncompressed on all plans since 2026-09-04; ignore the 3 MiB-gzip advice in older articles. The apps/web 180 KB initial-JS gate remains a web-only CI gate and does not apply to crm.
- Shared tokens: add "packages/*" to root workspaces; create packages/tokens (@ironics/tokens) holding lib/tokens.ts values (and optionally the Tailwind theme fragment); Next transpiles workspace packages automatically for the App Router (webpack and Turbopack), so `transpilePackages` is optional belt-and-braces; if the package ever ships runtime node_modules dependencies, set outputFileTracingRoot to the repo root in both next.config files (OpenNext consumes the traced standalone build). Tailwind `content` globs need the package path only if it contains class-name strings. Switch the 15 `@/lib/tokens` imports in apps/web to `@ironics/tokens` in one commit and add packages/tokens/** to deploy-web.yml's path filters.
- CI: author .github/workflows/deploy-crm.yml as a copy of deploy-web.yml with paths apps/crm/**, packages/tokens/**, package.json, package-lock.json; working-directory apps/crm; its own concurrency group; same two secrets; `environment: production` with Required reviewers per D6. Pin crm's `next` to the same major/minor as web (^15.5.x) so npm hoists a single copy and both Workers stay inside OpenNext's 'latest minors of 15' support window; Node runtime only, no edge runtime, no Node middleware.
- Cookies/CORS — required backend change: widen `corsPreflight.allowOrigins` in apps/api/lib/api.ts:255 to an exact-origin array ["https://ironics.org","https://admin.ironics.org"] (no wildcards; allowCredentials stays true) and deploy IronicsPortalStack; the Lambda cannot add the origin because API Gateway ignores backend CORS headers. Then PROVE on the live stack by re-running the curl probe with Origin https://admin.ironics.org and asserting `access-control-allow-origin: https://admin.ironics.org` + `access-control-allow-credentials: true` on GET and OPTIONS (the multi-origin echo behaviour is not documented by AWS — assert, don't assume). Consider adding `Vary: Origin` awareness if any cache ever fronts the API.
- Cookies/CORS — no SameSite change is needed: admin.ironics.org → api.ironics.org is same-site (schemeful, registrable domain ironics.org), so the existing Lax ironics_session and the ruled Strict/host-only admin cookie (D4) both travel on `credentials: "include"` fetches; SameSite=None and third-party-cookie mitigations are not in play. ironics_session (Domain=.ironics.org) also reaches the crm Worker, so the admin-gate.ts server-side `/v1/admin/me` concealment pattern ports unchanged.
- Elevation seam to specify before building Settings (D9 first screen): because the D4 admin cookie is host-only on api.ironics.org, the browser — not the crm Worker — must receive the elevate response's Set-Cookie, and all elevated `/v1/admin/*` calls must be browser fetches with credentials:"include" (the Worker never sees that cookie). The Access assertion, however, arrives at the Worker (CF_Authorization on admin.ironics.org, HttpOnly configurable; ctx.access identity). Options to close the seam without re-opening D1/D3/D4: (a) Worker route handler verifies the Access identity and calls /v1/admin/elevate server-side to obtain a one-time, short-lived handoff code, which the page then redeems via a direct browser fetch to api.ironics.org that returns the Set-Cookie; or (b) the browser posts a Worker-minted, audience-bound proof to the elevate route. Flag: the log's 'Next route handler forwarding Cf-Access-Jwt-Assertion' as written would leave the admin cookie in the Worker's fetch response, not the browser — evidence: MDN host-only Domain rule + apps/web/lib/api.ts:5-10. Also confirm on the live Zero Trust setup (OPERATOR's D3 compatibility check) that the assertion header/ctx.access is present for a Worker-destination Access app, since the verbatim header sentence was not found in the fetched docs.
- Side observation for the content pass (W-items, not this fact check): www.ironics.org has no DNS record, so the www→apex 301 described in wrangler.jsonc/DEPLOYMENT.md is not live; a proxied record (or a second custom_domain entry) is needed for the Redirect Rule to fire.

**Sources:** C:\Dev\Bag_Man\Docs\design\IRONICS_PLATFORM_DECISION_LOG_2026-09-06.md:39-56 (D1-D7, D30); C:\Dev\Bag_Man\Docs\design\IRONICS_ADMIN_DASHBOARD_SCOPE.md:13-19, 97, 237-274, 369-379, 445; C:\Dev\Bag_Man\Docs\PLATFORM_GROUNDING_BRIEF_2026-09-06.md:15-28; C:\Dev\Ironics-Platform\apps\web\wrangler.jsonc:1-80; C:\Dev\Ironics-Platform\package.json:1-17; C:\Dev\Ironics-Platform\apps\crm\README.md:1-31; C:\Dev\Ironics-Platform\.github\workflows\deploy-web.yml:1-189; C:\Dev\Ironics-Platform\.github\workflows\probe-cloudflare-tokens.yml:1-75; C:\Dev\Ironics-Platform\apps\web\LEGAL_OPEN_ITEMS.md:67-95; C:\Dev\Ironics-Platform\apps\web\package.json:1-40; C:\Dev\Ironics-Platform\apps\web\next.config.mjs:1-27; C:\Dev\Ironics-Platform\apps\web\open-next.config.ts:1-33; C:\Dev\Ironics-Platform\apps\web\tsconfig.json:17; C:\Dev\Ironics-Platform\apps\web\tailwind.config.ts:1,13; C:\Dev\Ironics-Platform\apps\web\lib\api.ts:1-12; C:\Dev\Ironics-Platform\apps\web\lib\admin-gate.ts:22-42; C:\Dev\Ironics-Platform\apps\web\DEPLOYMENT.md:44-117; C:\Dev\Ironics-Platform\apps\api\src\lib\http.ts:7-38; C:\Dev\Ironics-Platform\apps\api\lib\api.ts:250-261, 398-441; C:\Dev\Ironics-Platform\apps\api\src\lib\ctx.ts:63; C:\Dev\Ironics-Platform\apps\api\AWS-SETUP.md:269-272; https://developers.cloudflare.com/workers/configuration/routing/custom-domains/; https://developers.cloudflare.com/workers/platform/limits/; https://developers.cloudflare.com/workers/platform/pricing/; https://developers.cloudflare.com/changelog/post/2026-09-04-increased-worker-size-limit/; https://developers.cloudflare.com/workers/configuration/cloudflare-access/; https://developers.cloudflare.com/changelog/post/2026-08-14-workers-access/; https://developers.cloudflare.com/cloudflare-one/identity/authorization-cookie/; https://developers.cloudflare.com/cloudflare-one/identity/authorization-cookie/application-token/; https://opennext.js.org/cloudflare; https://opennext.js.org/cloudflare/get-started; https://opennext.js.org/cloudflare/known-issues; https://opennext.js.org/cloudflare/howtos/multi-worker; https://github.com/opennextjs/opennextjs-cloudflare (README: standalone-mode build); https://nextjs.org/docs/app/api-reference/config/next-config-js/transpilePackages; https://nextjs.org/docs/app/api-reference/config/next-config-js/output; https://github.com/cloudflare/workers-sdk/issues/13925; https://developer.mozilla.org/en-US/docs/Web/HTTP/Guides/CORS; https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/Set-Cookie; https://developer.mozilla.org/en-US/docs/Glossary/Site; https://docs.aws.amazon.com/apigateway/latest/developerguide/http-api-cors.html; https://docs.aws.amazon.com/apigatewayv2/latest/api-reference/apis-apiid.html; https://dev.to/lewiskori/deploying-a-nextjs-monorepo-to-cloudflare-workers-lessons-from-the-trenches-1ok8 (third-party); Live probe 2026-09-06: Resolve-DnsName admin/www/api/ironics.org; curl GET+OPTIONS https://api.ironics.org/v1/stats with Origin https://ironics.org and https://admin.ironics.org

### cfn-capacity: IronicsPortalStack resource budget, nested AdminApiStack shape that keeps test/grants.test.ts valid, and how reservedConcurrency is applied / the safe per-function value (verified: True)

- LIVE TEMPLATE COUNT: C:\Dev\Ironics-Platform\apps\api\cdk.out\IronicsPortalStack.template.json (mtime 2026-09-04T22:58:55Z, i.e. synthesized 23 min AFTER the last lib/api.ts commit ffb9829 2026-09-04 15:35 -0700, so it matches the current route table) = 179 Resources, 1 Parameter, 2 Outputs. By type: AWS::DynamoDB::Table 13, KMS::Key 1, KMS::Alias 1, SecretsManager::Secret 3, S3::Bucket 1, S3::BucketPolicy 1, ApiGatewayV2::Api 1, ApiGatewayV2::Stage 1, ApiGatewayV2::DomainName 1, ApiGatewayV2::ApiMapping 1, AWS::CDK::Metadata 1 (= 25 fixed) + Lambda::Function 22, IAM::Role 22, IAM::Policy 22, Logs::LogGroup 22, ApiGatewayV2::Integration 22, Lambda::Permission 22, ApiGatewayV2::Route 22 (= 7 x 22 = 154). 25 + 154 = 179 (counted by script over the JSON, 2026-09-06).
- ROUTE COUNT IS 22, NOT 21: lib/api.ts:66-248 declares 22 RouteSpecs (14 non-admin: Stats, EmailStart, EmailVerify, EpicStart, EpicCallback, Refresh, Logout, BetaApply, BetaAge, BetaStatus, AcceptTerms, DownloadLatest, Me, CheckoutVolts; 8 admin: AdminApproveClaim, AdminMe, AdminListClaims, AdminAudit, AdminRegisterEconomy, AdminRecordPayment, AdminResolvePayment, AdminListPayments). The scope doc's '21 routes' (IRONICS_ADMIN_DASHBOARD_SCOPE.md:61) is off by one; the template's 22 AWS::ApiGatewayV2::Route RouteKeys confirm 22.
- PER-ROUTE RESOURCES = EXACTLY 7, INVARIANT ACROSS ROUTES: Function api.ts:325-346 (NodejsFunction); LogGroup api.ts:339-343 (explicit, FIXED name `/aws/lambda/ironics-portal-<id lowercase>`, ONE_MONTH, DESTROY); Role + one DefaultPolicy are the NodejsFunction's implicit role and its grants; every grant at api.ts:348-388 (tables, secrets grantRead, kms:Sign/GetPublicKey, S3 grantRead) folds into that SAME AWS::IAM::Policy, so routes with secrets (EmailStart, EpicStart, EpicCallback, BetaApply) still cost 7; Integration + Lambda::Permission + Route come from `api.addRoutes` + `HttpLambdaIntegration` at api.ts:391-395 with logical ids `PortalApi<METHOD><path>` / `...<Id>Integration` / `...<Id>IntegrationPermission` (e.g. PortalApiGETv1adminme67E30BC0, PortalApiGETv1statsStatsIntegrationPermission3C4DF80B). Census: AdminMe, AdminApproveClaim, Stats each own exactly 4 function-side resources + 3 api-side.
- HEADROOM: 500 - 179 = 321 => at 7/route the parent can take 45 more routes before the HARD cap; against the ruled synth assertion (< 450, decision log line 53) it is (450-179)/7 = 38 routes, and that is BEFORE the ruled non-route additions (3-4 tables, roll-up Lambda + EventBridge rule, ~20 alarms + ~8 metric filters, SNS topic/subscription, CloudTrail trail + bucket + policy ~= 40-60 resources), which leaves ~30 routes in the parent. The scope's ~30-36 new admin routes therefore cannot all live in the parent under the 450 rule -> nested stack ruling is necessary, not cosmetic.
- VENDOR (CloudFormation quotas, https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/cloudformation-limits.html, fetched 2026-09-06): 'Resources | The maximum number of resources that you can declare in your CloudFormation template. | 500 resources | To specify more resources, separate your template into multiple templates by using, for example, nested stacks.' Also: Parameters '200 parameters'; Outputs '200 outputs'; 'Nested stacks | The maximum number of CloudFormation resources a nested stack can create, update, or delete per operation ... | 2500 resources'; 'Template body size in an Amazon S3 object | 1 MB'.
- VENDOR (CDK NestedStack, https://docs.aws.amazon.com/cdk/api/v2/docs/aws-cdk-lib.NestedStack.html): 'Cross references of resource attributes between the parent stack and the nested stack will automatically be translated to stack parameters and outputs.' and 'this stack will not be treated as an independent deployment artifact (won't be listed in "cdk list" or deployable through "cdk deploy"), but rather only synthesized as a template and uploaded as an asset to S3.' CFN nested-stack page (https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/using-cfn-nested-stacks.html): 'When you update a root stack, only nested stacks with template changes will be updated.'
- EXPERIMENT (read-only synth, aws-cdk-lib 2.265.0 hoisted at C:\Dev\Ironics-Platform\node_modules; apps/api/package.json pins ^2.170.0; script C:\Users\tabor\AppData\Local\Temp\claude\C--Dev-Bag-Man\32b13db2-c71f-48be-802e-68100209d8d8\scratchpad\nested-experiment.mts): building IronicsPortalStack + addApi + `class AdminApiStack extends NestedStack` (child of the stack) with two admin-like NodejsFunctions granted Admins read / AuditLog write / kms:GetPublicKey produced: PARENT 182 resources = 178 (179 minus the CDK::Metadata a plain App omits) + 1 AWS::CloudFormation::Stack (id AdminApiNestedStackAdminApiNestedStackResource135F1E5C, TemplateURL = asset in cdk-hnb659fds-assets-302659227808-us-east-1, Parameters: 5) + 3 leaked by variant B (below). NESTED 11 resources, 5 Parameters named `referencetoIronicsPortalStackAdminsC9C957CBArn`, `referencetoIronicsPortalStackAuditLog7199DC72Arn`, `referencetoIronicsPortalStackSessionSigningKeyA4D2606EArn`, `referencetoIronicsPortalStackAdminsC9C957CBRef`, `referencetoIronicsPortalStackPortalApi8326D12CRef`.
- ROUTE PLACEMENT (experiment): Variant A = `new apigw.HttpRoute(nested, `${id}Route`, { httpApi: api, routeKey: HttpRouteKey.with(path, method), integration: new HttpLambdaIntegration(`${id}Integration`, fn) })` puts ALL 7 per-route resources in the nested template (XAdminARoute197BB703:Route, XAdminARouteXAdminAIntegration3A314C45:Integration, XAdminARouteXAdminAIntegrationPermissionFCF0D86A:Permission + Fn/Role/Policy/Logs) -> parent +0 per route. Variant B = function in the nested scope but wired with the parent's `api.addRoutes` (today's call, api.ts:391) LEAKS Route + Integration + Permission into the PARENT (PortalApiGETv1adminxbXAdminBIntegration18A22801, ...Permission631C7F6D, PortalApiGETv1adminxb route) and adds a nested Output (IronicsPortalStackAdminApiXAdminBFnBBE8D677Arn) -> parent +3 per route. The nested shape MUST construct HttpRoute in the nested scope.
- GRANTS TEST HEURISTIC SURVIVES (experiment): replaying test/grants.test.ts's mapping (bare() strip of the 8-hex suffix, :150; roleOf via Role Fn::GetAtt, :151-156; can() = `s.Action.includes(action) && JSON.stringify(s.Resource).includes(table)`, :180-184) on `Template.fromStack(nested)`: XAdminAFn statements reference `{"Ref":"referencetoIronicsPortalStackAdminsC9C957CBArn"}` and `{"Ref":"referencetoIronicsPortalStackAuditLog7199DC72Arn"}`, so can(GetItem,Admins)=true, can(UpdateItem,AuditLog)=true, can(GetItem,Accounts)=false, can(UpdateItem,Admins)=false, kms:GetPublicKey=true. The auto-generated parameter name embeds the table's construct id, which is exactly the substring the test matches. `Template.fromStack(<NestedStack>)` works in 2.265.0 (proven by the run; the assertions README at https://docs.aws.amazon.com/cdk/api/v2/docs/aws-cdk-lib.assertions-readme.html does not mention nested stacks either way).
- TEST EDITS REQUIRED: grants.test.ts before() builds `policies` from ONE template (`Template.fromStack(stack)` :143, loop :151-171) and envOf() searches one template (:318-324) -> both must iterate [parent, stack.adminApi] (a ~6-line change; every EXPECTED/OWNER/MINTERS/VERIFIERS arm then works unchanged). admin-guard.test.ts parses lib/api.ts AS TEXT (:163-186: `id: "...", path: "..."` regex, SPEC_END `\n  ];`) and requires every module under src/handlers/admin to be declared in THAT file with admin:true and a /v1/admin/ path (:209-222), with a control requiring >= 12 parsed specs (:188-194) -> the RouteSpec entries for nested routes MUST stay in the single `routes` array in lib/api.ts; placement is a per-spec field (e.g. `home: "admin"`), never a second file/array.
- BASELINE (2026-09-06): `git status --porcelain apps/api` empty at HEAD eb138a8; `npx tsx --test test/grants.test.ts` -> tests 76, pass 76, fail 0 (27.7 s, bundling 22 functions with esbuild).
- DO NOT MOVE THE 8 LIVE ADMIN ROUTES: their LogGroups carry fixed names (`/aws/lambda/ironics-portal-adminme` etc., api.ts:340) and their RouteKeys are live on ironics-portal; re-creating them inside the nested stack in the same stack update would create the new copies before CloudFormation removes the old ones (removed resources are deleted in the UPDATE_COMPLETE_CLEANUP phase - this ordering and API Gateway v2's rejection of a duplicate route key could NOT be re-confirmed from a primary page in this pass: the HTTP-API routes page https://docs.aws.amazon.com/apigateway/latest/developerguide/http-api-develop-routes.html defines route keys but says nothing about uniqueness; treat as inference). Safe design: parent keeps today's 22 routes byte-identical (179 stays; `cdk diff` must show only the added Stack resource), every NEW admin route goes nested with log-group prefix `/aws/lambda/ironics-admin-<id>`; migrating the 8 later = a separate two-deploy (remove, then add) with downtime, confirm-first.
- RESERVED CONCURRENCY APPLICATION: api.ts:284 `scope.node.tryGetContext("reservedConcurrency")`; :285 `Number(raw)` else undefined; :334 the SAME value is applied to EVERY route function (uniform, no per-route knob); apps/api has NO cdk.json, so the only input is `-c reservedConcurrency=N` on the CLI; `PORTAL_RESERVED_CONCURRENCY = 50` (ironics-portal-stack.ts:348) is exported and NEVER read (grep: only its definition plus comments at api.ts:277,282 and AWS-SETUP.md:100). Live template: every function's ReservedConcurrentExecutions is undefined (22/22). CDK context resolves up the construct tree, so a NestedStack child of the stack receives the same context value (by construction; nested functions in the experiment carried the ReservedConcurrentExecutions they were given, 7,7).
- VENDOR (Lambda, https://docs.aws.amazon.com/lambda/latest/dg/configuration-concurrency.html, fetched 2026-09-06): 'You can reserve up to the Unreserved account concurrency value minus 100. The remaining 100 units of concurrency are for functions that aren't using reserved concurrency. For example, if your account has a concurrency limit of 1,000, you cannot reserve all 1,000 units of concurrency to a single function.' and 'Reserving concurrency for a function impacts the concurrency pool that's available to other functions. For example, if you reserve 100 units of concurrency for function-a, other functions in your account must share the remaining 900 units of concurrency, even if function-a doesn't use all 100 reserved concurrency units.' and 'To intentionally throttle a function, set its reserved concurrency to 0.'
- LIVE QUOTA (read-only, 2026-09-06, as arn:aws:iam::302659227808:user/Lead_Developer_Ironics): `aws service-quotas get-service-quota --service-code lambda --quota-code L-B99A9384 --region us-east-1` -> Value 1000.0 ('Concurrent executions', QuotaAppliedAtLevel ACCOUNT). Therefore reservable = 1000 - 100 = 900. The comment at api.ts:273-282 ('this account's total limit is 10') and AWS-SETUP.md:87-111 ('total Lambda concurrency limit of 10') are STALE; the scope's D5 text 'fix AWS-SETUP.md's stale 10' (scope:239) is correct.
- SAFE PER-FUNCTION VALUE (arithmetic): 22 x 50 = 1,100 > 900 -> `-c reservedConcurrency=50` (api.ts:282, AWS-SETUP.md:100, scope:239) FAILS the deploy exactly as the quota-10 case did. 22 x 10 = 220 OK. Projected 22 + ~35 new = 57 functions: x10 = 570, x5 = 285; max uniform N = floor(900/57) = 15. The 1,000 pool is shared with BagManTentpoleStack's proven money Lambdas (>= 13 hold the earn key, scope:390), so every unit the portal reserves is subtracted from THEIR unreserved pool. Recommended: public/session routes (14 today) = 10, admin routes (8 + ~35) = 5 -> 180 today, ~355 at full scope, >= 645 left unreserved; synth guard: every function 1..10 (never 0 = throttle), sum <= 450. This is inside the decision-log D5 ruling 'reservedConcurrency per function 5-10' (IRONICS_PLATFORM_DECISION_LOG_2026-09-06.md:43) and matches scope review attack #5 (scope:381-383).
- ROUTE PROJECTION FROM THE RULED SCOPE (16 approved artboards per gen_artboards.py:353-354: Shell+Overview, Accounts, Account detail · Wallet & Entitlements, Accounts · Founder ladder, Economy · Payments, Adjustments, Holds, Escrow & KPIs, Catalog (read-only), Releases, Telemetry · Funnel + Downloads, Settings · Admins & Roles, Settings · Flags & Kill switches, Audit, States & banners, Confirmations; mutating confirmations at :341-346 = elevate, revoke admin, approve adjustment, rollback, resolve in-doubt, release held). Named new admin routes in scope §D / decision log: elevate (1); admins list/grant/revoke/roles (4); accounts list, account detail, approve/ban/suspend/unban, quota-reset (~6); wallet (1); adjustments propose/approve/reject/list (4); holds hold/unhold (2); escrow list/resolve-in-doubt/release-held (3); economy/kpis (1); catalog read (1); founder ladder read (1); metrics (1); health (1); flags GET/PUT (2); releases list/publish-pointer/promote/rollback/set-minbuild (5) = ~34 admin routes -> ~238 nested resources + ~20 parameters + 1 CDK::Metadata ~= 260 of 500 (parameters ~20 of 200). Non-admin additions stay in the parent: GET /v1/download/latest/meta, POST /v1/internal/game-event, later GET /v1/roadmap (+7 each = +21) + 1 Stack + ~40-60 non-route infra -> parent ~= 245-265 of the 450 assertion. Both fit with margin; a second nested stack is not needed for this scope.
- DEPLOY MECHANICS UNCHANGED: package.json:12 `cdk deploy --app "tsx bin/ironics-portal.ts" IronicsPortalStack` still deploys everything (a NestedStack is not a separate artifact); bin/ironics-portal.ts:8-16 constructs the stack then `addApi(stack, stack)`, so the nested stack can be created inside addApi (or the stack ctor) without touching bin. The nested template is uploaded to the CDK bootstrap asset bucket (experiment TemplateURL `cdk-hnb659fds-assets-302659227808-us-east-1/...`), which the D5(b) `ironics-agent-deploy` principal must be allowed to write (it already needs this for Lambda asset zips). Each NodejsFunction is still one esbuild bundle per route (22 took ~25 s in the test run), so cdk deploy time grows ~linearly with routes.

**Implications:**
- Shape (satisfies the ruling 'nested AdminApiStack; synth-time assertion Resources < 450' without re-opening it): add lib/admin-api-stack.ts `export class AdminApiStack extends NestedStack {}`; in addApi create `const adminApi = new AdminApiStack(stack, "AdminApi")` and expose it as `stack.adminApi`; add `home?: "portal" | "admin"` to RouteSpec (default portal); in the route loop `const scope = r.home === "admin" ? adminApi : stack` for NodejsFunction + LogGroup, and wire nested routes with `new apigw.HttpRoute(scope, `${r.id}Route`, { httpApi: api, routeKey: apigw.HttpRouteKey.with(r.path, r.method), integration: new HttpLambdaIntegration(`${r.id}Integration`, fn) })` - NEVER `api.addRoutes` for nested routes (proven to leak 3 resources/route back into the parent). Keep the existing 22 specs and their `api.addRoutes` path untouched so their logical ids do not change; `cdk diff` before the first deploy must show only the new AWS::CloudFormation::Stack.
- Log-group naming for nested routes: `/aws/lambda/ironics-admin-<id lowercase>` (not the parent's `ironics-portal-` prefix) so a later migration of the 8 live admin routes cannot collide with a live group; retention ONE_MONTH + DESTROY as today (api.ts:339-343).
- Tests: (a) grants.test.ts before()/envOf() iterate `[Template.fromStack(stack), Template.fromStack(stack.adminApi)]` and merge `policies` - proven sufficient because nested policy Resources are `{Ref: referencetoIronicsPortalStack<Table>Arn}` which the `includes(table)` heuristic matches; (b) keep every RouteSpec in lib/api.ts's single `routes` array so admin-guard.test.ts's text parser (:163-222) stays valid; add an arm asserting `home === "admin"` implies `admin: true` and path starts with /v1/admin/; (c) new test/capacity.test.ts (picked up by `tsx --test test/*.test.ts`, package.json:10): parent and nested Resources < 450, nested Parameters < 200, plus the reserved-concurrency guard (each function 1..10, sum <= 450). Also a `cdk synth` post-step in CI over cdk.out/*.template.json + the *.nested.template.json asset so a deploy over the cap cannot start.
- Reserved concurrency: replace the single uniform context knob (api.ts:284-285,334) with a per-RouteSpec `reserved` default (portal/session routes 10, admin routes 5) applied at :334; keep `-c reservedConcurrency` only as an opt-out (unset) if desired; delete the dead PORTAL_RESERVED_CONCURRENCY (stack.ts:341-348); rewrite the stale api.ts:273-282 comment and AWS-SETUP.md:87-111 to the live 1,000 quota / 900 reservable; never ship `-c reservedConcurrency=50` (1,100 > 900 = deploy failure). This is the concrete form of D5(f) 'reservedConcurrency per function 5-10'.
- Migration of the 8 live admin routes into the nested stack is NOT part of Phase 1: it needs a two-deploy remove-then-add with downtime on those routes (log-group name + route-key collision risk within one update) and is a destructive/irreversible-shaped step -> confirm-first, scheduled only if the parent ever approaches the 450 assertion (it does not under this scope's projection: parent ~245-265).
- Capacity is not the binding constraint once nested: ~34 admin routes -> nested ~260/500, parent ~250/450; the binding constraints become deploy time (one esbuild bundle per route) and the shared 1,000 Lambda pool with the tentpole (portal reservations subtract from the tentpole's unreserved pool) - keep the portal's total reservation <= 450.
- Doc-truth fixes to carry: scope §C '21 routes' -> 22; AWS-SETUP.md quota section (87-111) and api.ts:273-282 comment are stale (live L-B99A9384 = 1000); the `-c reservedConcurrency=50` instruction in api.ts:282 / AWS-SETUP.md:100 / scope:239 is superseded by D5 (5-10) and would fail if run.
- No decision-log ruling is unsafe on this topic; the only unverified vendor points are (1) CloudFormation's cleanup-phase ordering for removed resources and (2) API Gateway v2's duplicate-route-key behavior - both only matter for the deferred migration of the 8 live routes, which the recommended shape avoids.

**Sources:** C:\Dev\Ironics-Platform\apps\api\lib\api.ts:18-56 (RouteSpec), 66-248 (22 routes), 250-268 (HttpApi + stage throttle), 273-285 (reservedConcurrency context + stale quota comment), 324-396 (per-route loop: Function 325-346, LogGroup 339-343, reserved 334, grants 348-388, addRoutes 391-395); C:\Dev\Ironics-Platform\apps\api\lib\ironics-portal-stack.ts:18-37 (tables/keys/secrets/bucket), 341-349 (PORTAL_RESERVED_CONCURRENCY = 50 dead constant, PORTAL_TIMEOUT); C:\Dev\Ironics-Platform\apps\api\bin\ironics-portal.ts:6-16; C:\Dev\Ironics-Platform\apps\api\cdk.out\IronicsPortalStack.template.json (mtime 2026-09-04T22:58:55Z; 179 Resources; census by script 2026-09-06); C:\Dev\Ironics-Platform\apps\api\test\grants.test.ts:36-131 (EXPECTED), 137-172 (before(): single Template.fromStack), 150 (bare), 180-184 (can), 232-303 (wildcard/KMS/secret arms), 318-324 (envOf); C:\Dev\Ironics-Platform\apps\api\test\admin-guard.test.ts:121-148 (wrapped-handler arm), 150-222 (route table parsed as TEXT from lib/api.ts; SPEC_END; >= 12 specs control); C:\Dev\Ironics-Platform\apps\api\package.json:8-13 (test/synth/deploy scripts), 25-33 (aws-cdk-lib ^2.170.0); hoisted C:\Dev\Ironics-Platform\node_modules\aws-cdk-lib 2.265.0; C:\Dev\Ironics-Platform\apps\api\AWS-SETUP.md:87-111 (stale 'total limit of 10', L-B99A9384 increase command); C:\Users\tabor\AppData\Local\Temp\claude\C--Dev-Bag-Man\32b13db2-c71f-48be-802e-68100209d8d8\scratchpad\nested-experiment.mts (read-only synth experiment; output quoted in facts; run 2026-09-06); Command outputs 2026-09-06: `aws service-quotas get-service-quota --service-code lambda --quota-code L-B99A9384 --region us-east-1` -> 1000.0; `aws sts get-caller-identity` -> user/Lead_Developer_Ironics; `npx tsx --test test/grants.test.ts` -> 76 pass / 0 fail; `git status --porcelain apps/api` -> empty at eb138a8; C:\Dev\Bag_Man\Docs\design\IRONICS_PLATFORM_DECISION_LOG_2026-09-06.md:43 (D5 incl. 'reservedConcurrency per function 5-10'), 53 (CFN cap: nested AdminApiStack; Resources < 450), 54-56; C:\Dev\Bag_Man\Docs\design\IRONICS_ADMIN_DASHBOARD_SCOPE.md:17 (D5), 44 (CFN cap row: live 179, ~7/route), 61 ('21 routes' - off by one), 239 (Phase 0 hygiene incl. stale '-c reservedConcurrency=50'), 381-383 (attack #5), 390 (13 tentpole holders of the earn key), 421-423 (attack #15), 447 (missing #9 capacity plan); C:\Users\tabor\AppData\Local\Temp\claude\C--Dev-Bag-Man\32b13db2-c71f-48be-802e-68100209d8d8\scratchpad\console-mockups\gen_artboards.py:341-346 (confirmations), 353-354 (16 artboards + titles); https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/cloudformation-limits.html (500 resources / 200 parameters / 200 outputs / nested 2500 per operation / 1 MB S3 template); https://docs.aws.amazon.com/lambda/latest/dg/configuration-concurrency.html ('You can reserve up to the Unreserved account concurrency value minus 100 ...'; reserved 0 = throttle); https://docs.aws.amazon.com/cdk/api/v2/docs/aws-cdk-lib.NestedStack.html (cross references -> parameters/outputs; not independently deployable; template uploaded as an asset); https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/using-cfn-nested-stacks.html (AWS::CloudFormation::Stack resource; only changed nested stacks update); https://docs.aws.amazon.com/cdk/api/v2/docs/aws-cdk-lib.assertions-readme.html (Template.fromStack usage; silent on NestedStack - proven empirically instead); https://docs.aws.amazon.com/apigateway/latest/developerguide/http-api-develop-routes.html (route key definition; silent on uniqueness -> flagged as inference)

## K. Workstream assumptions and open questions

### A — Phase 0 credential hygiene + deploy path + platform hardening (D5, D6, scope §D must-have 4)

**Assumptions:**
- D5's ordering is treated as a hard gate, not a preference: PLAT-A02/A03 (create + cut over the machine principal, with the eight-point proof) MUST be signed off before PLAT-A05 attaches the MFA deny and retires the key. No ticket in this workstream is allowed to run out of order.
- CORRECTION TO D5(b)'s premise, not a re-opening: 'a NEW least-privilege machine principal becomes the key used by CI and by the agent's sessions' — CI does not use, and does not need, an AWS key. deploy-api.yml:75-90 federates by GitHub OIDC into role IronicsPortalDeploy (trust scoped to repo:Instawerx/Ironics-Platform:environment:production-api, one inline AssumeCdkBootstrapRoles policy, zero attached managed policies — read live). deploy-web.yml uses a Cloudflare token, not AWS. So ironics-agent-deploy's real consumers are the agent's local sessions and the operator's publish-release.ps1 box. The ruling's intent (nothing the automation depends on gets locked out) is satisfied; the wording is just wider than the estate.
- LEAST PRIVILEGE IS BOUNDED AT THE CDK BOOTSTRAP, and the plan says so instead of pretending otherwise. Stack CDKToolkit was bootstrapped with CloudFormationExecutionPolicies empty (read live), so cdk-hnb659fds-cfn-exec-role carries AdministratorAccess. Granting ironics-agent-deploy sts:AssumeRole on the bootstrap roles — which D5(b) explicitly rules — makes it admin-equivalent through CloudFormation. The explicit Deny in statement 7 of its policy binds only its own direct calls, never the assumed-role session. That path is DETECTED by PLAT-A06/A07, not prevented. Re-bootstrapping with scoped execution policies or a permissions boundary is the only real fix and is NOT proposed here: it rewrites the roles both live stacks deploy through, which is a confirm-first change of its own.
- Reserved-concurrency sizing is arithmetic on live numbers, not a guess: quota L-B99A9384 = 1000, UnreservedConcurrentExecutions = 1000, 58 functions across both stacks, so reservable = 900. 14 non-admin × 10 + 8 admin × 5 = 180. The instruction '-c reservedConcurrency=50' still written at api.ts:282, AWS-SETUP.md:100 and IRONICS_ADMIN_DASHBOARD_SCOPE.md:239/251 would request 1100 > 900 and fail the deploy; D5(f)'s 5-10 supersedes it and the stale text is corrected in PLAT-A09.
- The $50 AWS Budget already exists (name ironics-monthly-cost, MONTHLY, USD 50, three notifications, subscriber vrdivebar@gmail.com — read live 2026-09-06) but is configured with IncludeCredit=true, so it reads $0.00 while credits absorb the S12 server. C1 says 'gross'. PLAT-A15 flips it; this is a correction to a live artefact, not to a ruling.
- Root-account MFA is already enabled (AccountMFAEnabled=1, MFADevices=1, and Lead_Developer_Ironics has none — so that device is root's). This is what makes PLAT-A05 safe to run at all; if the operator cannot demonstrate root access, A05 must not proceed.
- Zero CloudWatch alarms exist today, so PLAT-A07's eight consume most of the 10-alarm free tier. Every alarm after that (D-DIST-6's BytesDownloaded, the economy workstream's money alarms) costs $0.10/month. Still inside C2's $3-10/month.
- PLAT-A11 is priced at 2.5 days, not the scope's '1' for all of Phase 0 hygiene, because a nonce+strict-dynamic CSP on OpenNext/workerd has a real white-screen failure mode and the proof requires a watched browser pass over 13 routes in report-only and then enforcing mode. The scope's Phase 0 estimate of '~2 days' (IRONICS_ADMIN_DASHBOARD_SCOPE.md:50) is not survivable for this list; this workstream honestly totals ~14.5 engineer-days.
- D2 review fix #2 (metadata-only GET /v1/download/latest/meta) is listed under Phase 0 in the scope but belongs to the metrics/distribution workstream (D25) and is deliberately NOT ticketed here — this workstream would only duplicate it. PLAT-A10 does touch the same handler family, so sequence the two so they do not collide in src/handlers/download/.
- The CFN resource budget is tracked as this workstream grows the stack: live template = 179 resources; PLAT-A06 adds ~7, PLAT-A07 adds ~18, ending near 204 against the ruled < 450. test/capacity.test.ts is introduced here (PLAT-A06) rather than waiting for the nested AdminApiStack work, so the assertion exists before the admin routes start landing.
- PLAT-A10 changes a security-relevant default (per-IP rate limits stop being header-driven), which resets every live per-IP rate-limit bucket once. At one founder and a handful of testers this is invisible; it is stated because it is a real user-visible side effect.

**Open questions:**
- GitHub plan: the repo Instawerx/Ironics-Platform is PRIVATE and owned by a User account. Environment protection rules on private repos generally require GitHub Pro/Team. The environments already exist and production-api carries a branch_policy rule, but 'Required reviewers' specifically may be gated. If enabling it asks for an upgrade (~$4/month), that is a NEW recurring vendor line against C3's 'zero new paid vendors' — do you want it bought, or should D6's approval gate be met another way (branch protection + a CODEOWNERS review on .github/workflows and apps/web, which is free)?
- Cloudflare token: after adding Zone → Workers Routes → Edit, does the token VALUE stay the same? If you create a new token instead, the CLOUDFLARE_API_TOKEN repository secret must be updated in the same sitting or the next web deploy fails. Also: the repo carries a second, apparently unused secret named CLOUDFLARE — may I delete it?
- Human CLI access after PLAT-A05: the plan retires access key ***REMOVED-AWS-KEY-ID (rotated; see Secrets Manager)*** entirely, leaving you console+MFA and `aws sts get-session-token` for the rare CLI job (moving secrets, editing the budget). Confirm you are happy with no standing human key, or say if you want a fresh MFA-conditioned key instead.
- Break-glass confirmation before PLAT-A05: can you sign in as the ROOT account today (root MFA is enabled — device present)? A05 must not run until you confirm that path works, because it is the recovery route if the MFA deny is mis-scoped.
- CDK bootstrap blast radius: cdk-hnb659fds-cfn-exec-role has AdministratorAccess (CloudFormationExecutionPolicies was left empty at bootstrap), so ironics-agent-deploy is admin-equivalent through `cdk deploy` no matter how tight its own policy is. Do you want a follow-up ticket to re-bootstrap with scoped execution policies or a permissions boundary? It touches the roles BOTH live stacks deploy through, so it is confirm-first and carries real deploy risk.
- Alarm destination and noise budget: PLAT-A07 subscribes vrdivebar@gmail.com (already your Budgets subscriber). Filter (5) 'sensitive table write by a human' will fire on legitimate operator/agent work — the A05 proofs, the future break-glass script, any manual fix. Do you want it as-is (accurate but occasionally self-inflicted), or suppressed for a named principal? D18 says Discord webhook when the server exists (G-COMM-1); confirm email-only for now.
- Secrets in Downloads (PLAT-A14): I will not open those files. Tell me for each note whether the destination is an existing portal secret, a new Secrets Manager entry, or a password manager (the super-admin password note sounds like a human credential, which belongs in a password manager, not Secrets Manager). Also confirm you will run the put-secret-value calls yourself under an MFA session — the agent principal is denied secretsmanager:* by design.
- Budget update (PLAT-A15): flipping IncludeCredit to false requires budgets:ModifyBudget, which the agent principal is explicitly denied. Do you want to run that one call under MFA, or grant a one-off exception?
- CSP fallback: if OpenNext does not propagate the per-request nonce into Next's bootstrap scripts on workerd, the honest fallback is script-src with 'unsafe-inline' alongside 'strict-dynamic' (modern browsers ignore unsafe-inline when strict-dynamic is present; old ones do not). Accept that documented compromise, or hold CSP until the console moves to admin.ironics.org where the surface is smaller?

### C — Admin console screens (D9 build order: Settings·Admins & Roles → Accounts + Account detail + Founder ladder → Audit → Overview (+ metrics spine + health) → Releases + Flags & Kill switches). Scope §D must-haves 1, 3, 4. 15 tickets, 30.0 engineer-days.

**Assumptions:**
- OWNERSHIP BOUNDARIES assumed for the other workstreams, so nothing here is double-built: PLAT-A owns Phase 0 hygiene (D5) and the deploy path (D6 — CI token Zone→Workers Routes→Edit, Required reviewers on `production`), including the CloudTrail trail and its control-plane alarms and the reservedConcurrency correction. PLAT-B owns the auth spine (D1-D4: roles/status/stepUpSubject on IronicsAdmins, the role→permission map, RouteSpec.requires + adminHandler({requires}), the 15-min ironics-admin elevation cookie, Bearer rejection on /v1/admin/*, the fail-CLOSED admin limiter, the nested AdminApiStack + the <450 synth assertion, the MFA-gated break-glass script) AND the console home (D7: apps/crm Worker on admin.ironics.org, packages/tokens, the shell with left rail / role badge / elevation countdown / frozen banner, the shared States and Confirmations components, Cloudflare Access, and widening apps/api/lib/api.ts corsPreflight.allowOrigins to include https://admin.ironics.org). Workstream D owns distribution (CloudFront Tier 0, GET /v1/download/latest/meta, the per-account mint quota + its reset route, CloudFront logs → Athena, BytesDownloaded + Budgets alarms) and the tentpole→portal game-event hook (launched / first_match). The economy workstream owns the Wallet & Entitlements panel, adjustments, holds, escrow, KPIs and catalog. Every C ticket that touches those areas ships an explicit 'not wired yet' state rather than a stub number.
- C DOES NOT take the metadata-only download endpoint. GET /v1/download/latest/meta (D-DIST-2 / D2 fix #2) is assigned to the distribution workstream; C consumes only latest.json (via the roll-up's buildRev counter) and therefore needs no route from it. Consequence stated in PLAT-C08: until that endpoint ships, download_started counts portal renders as well as clicks, so 'testers' is a ceiling.
- Every new admin route in this workstream declares home:'admin' and lands in PLAT-B's nested AdminApiStack, constructed with `new apigw.HttpRoute(nestedScope, ...)` rather than `api.addRoutes` (which leaks Route + Integration + Permission back into the parent). C adds 15 routes ≈ 105 resources, all but the roll-up/alarms/table in the nested stack; the parent grows by ~21 (Events table, roll-up Lambda + rule, ~10 alarms + SNS) to ≈200 of the ruled <450 assertion.
- Reserved concurrency is set per route at 5 (admin) / 10 (public) per D5, never the stale `-c reservedConcurrency=50` in api.ts:282 and AWS-SETUP.md:100 — 22 x 50 = 1,100 exceeds the 900 reservable at the live 1,000 quota and fails the deploy.
- DynamoDB permits ONE GSI creation per UpdateTable per table, so the new indexes are sequenced: playfab-index (Accounts) in C02, entitlement-index (Entitlements) in C04 — different tables, same deploy is fine — and actor-index then subject-index (AuditLog) in TWO separate deploys in C06.
- The 16 approved artboards satisfy mockup-first for every screen in this workstream (Main, Accounts, AccountDetail, FounderLadder, Releases, Admins, Flags, Audit, States, Confirmations). Any control this workstream adds that is NOT on an artboard (there is one: the account-detail 'reinstate' action, and the releases 'staging' channel row) needs an amended artboard before it is wired, per doctrine 7.
- IronicsAdmins already carries roles={owner} / status=ACTIVE on 01M06WNV71GP2QWYPBQA5JT4WC when C01 starts (PLAT-B's migration), and the operator has confirmed on screen at /portal that this is their Epic-linked account (review attack #11).
- Proofs that mutate identity need a SECOND real account. One is created once (email magic-link at ironics.org) and reused across C01-C04; the operator's own account and Founder #0001 are never the target of a revoke, ban or retire.
- Audit rows written before this workstream (the 3 live rows) carry no requestId/role/permission; the drawer renders those as '—' for historical rows rather than back-filling anything into an append-only table.
- Cost: DynamoDB IronicsEvents at beta volume ≈ $0.01-1/month, SSM Standard parameters $0, CloudWatch alarms within the 10 free, SNS email free, the second Worker $0 — total well inside C2's ~$3-10/month and the $50 cap. No new vendor.
- The console polls; there is no websocket, no push and no live Athena. Every number on every screen comes from a roll-up row, a table read, or a CloudWatch/GameLift/EC2 Describe — the same rule the scope applies to Athena.
- publish-release.ps1 stays the ONLY thing that uploads bytes; the console moves pointers only, and the ledger commit lands in the Bag_Man game repo (C:\Dev\Bag_Man\releases\win64\), which is where 0.1.0-beta's manifest already lives.

**Open questions:**
- Second test account for the identity proofs: create one (sign in once at ironics.org with a second email — 10 minutes) or nominate an existing person? PLAT-C01 through C04 cannot be proven without one, because the proofs revoke admin, ban an account and retire a founder number, and none of those may be pointed at your own row.
- Founder #0002 is permanently spent by the PLAT-C04 proof (numbers are never recycled, founder.ts:37-71). Acceptable?
- Live release-pointer rehearsal (PLAT-C11): may we move the beta pointer 0.1.3-beta → 0.1.2.1-beta → 0.1.3-beta on the live bucket with you watching the portal, or should the proof be confined to a staging channel? A pointer-only console that has never moved the live pointer is not proven.
- Live kill-switch rehearsal (PLAT-C13): may we flip download.enabled off and admin.frozen on for a few minutes, reverted in the same sitting? Same argument — a kill switch that has never been pulled is a claim.
- 'Testers' publication (PLAT-C08, D23): publish the counter now knowing it is inflated by the DownloadCard mount fetch (it counts portal renders as well as clicks) until the distribution workstream ships GET /v1/download/latest/meta, or keep the field hidden (W8) until then?
- Manual WAITLIST action (PLAT-C03): auto-approval is permanent (W6) and no cohort process exists, so nothing will ever set WAITLIST except this button. Keep it, or drop the control and the state from the console?
- GameLift Anywhere fleet-40c6b342 metric coverage (D29): does the fleet emit ActiveGameSessions / HealthyServerProcesses in the AWS console? If not, the Overview FLEET tile falls back to the EC2 status check plus tentpole presence and prints 'fleet metrics: n/a'.
- Alert destination (D18): the exact email address for the SNS subscription, and confirmation of the subscription link from the inbox. Discord webhook waits for G-COMM-1.
- apply.invite_cap (PLAT-C13): under permanent auto-approval, does the invite cap stay null, or do you want a daily ceiling on new applications as a spend guard before the ramp?
- Audit CSV export + actor/subject GSIs (PLAT-C06): confirmed as TRIGGERED by the first second-admin grant rather than scheduled — i.e. the C01 proof's temporary second admin does not open it, only a permanent second human does?
- GET /v1/admin/releases and /v1/admin/metrics readable by VIEWER, or operator-and-up? The draft gives viewer read on releases via metrics:read; say if release history should be operator-only.
- 0.1.1-beta has no manifest.json on disk (only the zip). PLAT-C11 proposes committing a reconstructed record (sizeBytes 3928491536, sha256 28c241fe…) labelled 'reconstructed 2026-09-06' rather than leaving a hole in the ledger. Approve, or leave 0.1.1 out of the committed ledger?

### F — Website content + public roadmap (decision log §5 W1–W32; hotfix, roadmap model B→C, copy truth pass, legal v0.3 prep, mailboxes/security.txt)

**Assumptions:**
- Decision-log §5 (W1–W32) is in force and settles the content pass's 32 decisions; the agent does not re-open any of them. Sub-choices the log does not settle (sticker series name, sponsor sentence sign-off, launch-year re-affirmation, FANATICS_X, /creators hold-vs-301) are raised as open questions rather than decided.
- PLAT-F01 is the ruled mockup-exempt hotfix (W5 + recommendation §8). I read 'copy-only' to include removing the two live-strip labels (TESTERS, REV) that render a label with no value — nothing new is drawn. If the operator reads the exemption more narrowly, drop the LiveStrip step from F01 and fold it into PLAT-F08.
- Local wrangler can ship the hotfix without operator hands: `npx wrangler whoami` on 2026-09-06 returned an OAuth session (instawerx@outlook.com, account 814f6c9f24ebcafe1c5df0c972854ff2) with workers (write) and workers_routes (write). Everything after PLAT-F02 goes through CI only.
- The CI token edit (Zone → Workers Routes → Edit) and Required reviewers on `production` are D6 / Phase-0 items owned by the admin workstream. Every F ticket from PLAT-F02 onward is blocked on them; F does not duplicate that work, it consumes and proves it.
- `apps/web/data/**` is already inside deploy-web.yml's `apps/web/**` path filter, so the roadmap/release data files need no workflow change.
- CI runners cannot see C:\Dev\Bag_Man, so the release ledger is mirrored into apps/web/data/releases.json by a local generator and COMMITTED; CI validates the committed file and publish-release.ps1 regenerates it, which is what stops drift.
- REV comes from the committed ledger (which mirrors latest.json) rather than a runtime read, because the web app has no AWS credentials at the edge. The seam for switching to an SSR read of GET /v1/download/latest/meta is left in lib/roadmap.ts; that endpoint belongs to the download workstream and is not a hard dependency of F.
- DOB retention (W26) is RESOLVED BY READING THE CODE, not by asking: apps/api/src/lib/applications.ts:190-210 runs `SET dob = :dob, ageVerifiedAt = :now` on the account row, called from src/handlers/auth/email-verify.ts:42 and src/handlers/beta/age.ts:57. So /beta's 'We do not store the full date after the check.' (lib/data.ts BETA_FACTS[1]) is the false statement and the Privacy Notice is the true one. /beta is corrected in PLAT-F11; the retention posture itself goes to counsel in PLAT-F14.
- /market stays a labelled preview (W10). No fresh catalog export is required for F. Identity/type counts come from C:\Dev\Bag_Man\Docs\reference\catalog-export.json (2026-08-25, 333 rows: FINISH 12, FACEMASK 38, SKIN_COLOR_EDGE 42, STICKER 15, ACCESSORY 10, EMBLEM 6, CHARACTER 7); its PRICING is stale, so no price is printed from it.
- Real FINISH names from that export are Blue, Green, Purple, Pink, Red, Black, Yellow, BigSixx, Aria, Scarlett, Makhiavelli, Gloss Black — there is no 'Volt Blue' or 'Reactor Green' anywhere in the catalog, which is why W13 (catalog names only) is a rename, not an alias map.
- 0.1.1-beta has no manifest.json and no .sha256 on D:\BagMan\releases\win64\0.1.1-beta (zip only, 3,928,491,536 B); its ledger record must be computed from the artifact, not copied from a memory note. 0.1.2, 0.1.2.1 and 0.1.3 have both files on D: and are copied verbatim.
- Ironics-Platform has no Git LFS and a 16 MB .git. Press reels / venue art storage (W17) is therefore NOT in workstream F and needs an LFS-vs-S3 ruling before anything ≥25 MB is committed there.
- Roadmap model C (PLAT-F16/F17) is gated on the admin console's release admin, roles and audit (D9 build order) landing in another workstream; F17's route work must land in the nested AdminApiStack and re-run the synth assertion (Resources < 450).
- No new vendor and no new paid service anywhere in F: static content plus, for model C, one on-demand DynamoDB table and two Lambda routes — pennies, inside the $50/month cap. F does not depend on CloudFront in any form.
- The COMMS-1/2 PIE gate has not passed, so the text-chat row ships as 'In the build' (W21), and voice is never mentioned on any public surface.
- No second beta account will be created for testing: there is exactly one account, and a signup would consume Founder #2 and move the public /v1/stats counter. Signed-in proofs (PLAT-F09, F15) are scheduled with the operator.

**Open questions:**
- STICKER SERIES (W15): print 'Renaissance Over Revolution' as the public series name, or print 'ROR'? The SeriesName field is empty on disk and the phrase has zero hits in the catalog — the name currently exists only in website copy.
- LAUNCH YEAR (W19): the row goes undated per the ruling — confirm the Launch row still prints at all (undated), or comes off the public roadmap until a date is defensible.
- SPONSOR (W11): does Simularent need to approve the new one-line descriptor before we remove the 'digital twin / twin-synced livery' promise from home, /market and every footer, and is the simularent.com → ir4.io redirect theirs? Removing a sponsor's product claim is outward-facing brand copy about a third party.
- HOUSE ROSTER (W12): the catalog export has SEVEN CHARACTER rows (BigSixx, ARIA, IRONICS, SCARLETT, MAKHIAVELLI, FANATICS, FANATICS_X) while the ruled public roster is six. Is FANATICS_X printed, folded into FANATICS, or held?
- /CREATORS (W9): the dead CTA is ruled out. Does the route survive as a future-tense hold page, or 301 to /beta until the W9 entry gate (and therefore leave NAV, the footer and the sitemap)?
- DISCORD (G-COMM-1 vs W9): the bare discord.gg CTA is removed in the hotfix. When the server exists, does the roadmap CTA come back immediately, or only after the moderation/appeal policy is written (GTM §13)?
- COUNSEL (W25 / PLAT-F14): who is counsel, when are they engaged, and does the download terms-gate go live together with the v0.3 re-acceptance or after? PLAT-F15 changes in-force legal text and can lock testers out of the build if it ships alone.
- SECURITY MAILBOX (W28): who is the named human behind security@ironics.org, and should /.well-known/security.txt advertise a PGP key? Publishing an address nobody reads is worse than publishing none.
- PORTAL PROOFS (PLAT-F09, PLAT-F15): will you sign in on the live portal for the watched proofs? There is exactly one account and creating a second consumes Founder #2 and moves the public counter, so there is no substitute.
- HOTFIX SCOPE (PLAT-F01): the roadmap page will still read 'Next · Q4 2026 · Console cohorts' and 'Planned · 2027 · Launch' after the hotfix — those rows are held for the mockup-gated rewrite (PLAT-F07). Do you want the word 'cohorts' and the 2027 date pulled in the hotfix too, or is the mockup gate worth the extra days of stale copy?

### B — Admin console foundation: apps/crm on admin.ironics.org (D7 Option B), packages/tokens, console shell + States/Confirmations, roles/permissions + `requires` on RouteSpec, admin-management routes, elevation token + Bearer rejection + Cloudflare Access verify, audit filters (decision log §2: D1-D12, D30, + CFN cap, + rate limiting, + PII, + break-glass)

**Assumptions:**
- ROLE MATRIX. D2 rules three roles and ~9 permissions but does not enumerate the bundles. I take the bundles from the operator-APPROVED artboard matrix (scratchpad/console-mockups/gen_artboards.py:298), which is the only ruled-and-approved statement of them: viewer = accounts:read + audit:read; operator = viewer + accounts:pii, accounts:approve, accounts:ban, release:write, flags:write; owner = operator + economy:mint, wallet:adjust, admins:manage. Money is inside owner, exactly as D2 says.
- PERMISSION MAPPING OF THE 8 LIVE ROUTES. AdminMe = any active admin (the console calls it on every render); AdminAudit = audit:read; every claims/payments/economy route = economy:mint, because D2 keeps money inside owner until a second human is on the roster and the ruled ~9-permission set has no separate 'economy:read'. A treasurer split and a read-only economy permission are triggered by the first second-admin grant, per D2, not scheduled here.
- admin.frozen STORE. D27 lists admin.frozen among the SSM flags. I implement it as an IronicsCounters row instead, because adminHandler must read it on every admin request and the estate's own rule (adversarial review #17, adopted in D27's scope) is that no portal function other than the flags Lambda holds any ssm:* permission. The Flags screen still writes it through an audited admin route, so there is one writer and one reader. This is an implementation choice inside D27, not a re-opening of it; flag it if the operator wants the SSM literal.
- ELEVATION HANDOFF. D1/D4 rule a host-only cookie on api.ironics.org and the scope describes a Next route handler forwarding Cf-Access-Jwt-Assertion. Those two cannot both work as written: a Set-Cookie received by the crm Worker's server-side fetch never reaches the browser, and a response from admin.ironics.org cannot set a cookie for api.ironics.org. PLAT-B10 therefore keeps both rulings intact by splitting the ceremony — the Worker (under the Access-protected /admin/ prefix) exchanges the assertion for a single-use 60-second code server-side, and the browser redeems that code directly against api.ironics.org, which is the response that carries the host-only cookie. The raw Access assertion never reaches page JavaScript. This is a refinement of the mechanism, not a change to D1, D3 or D4.
- ELEVATION SCOPE. Elevation is required on MUTATING admin routes only; GET reads need session + role. The approved States artboard shows a 'NOT ELEVATED' badge as a normal console state, which only makes sense if reading does not require elevation.
- EXTERNAL PREREQUISITES owned by other workstreams, assumed to land before or beside this one: D5's credential split (ironics-agent-deploy before the deny-all-unless-MFA policy), the CloudTrail management trail + DynamoDB data events + control-plane alarms (without them 'tamper-evident' is a claim, not a control — PLAT-B16's footer copy must say whichever is true on the day), and D6's Cloudflare token permission plus Required reviewers, which PLAT-B03 physically cannot pass without.
- EFFORT. 27 engineer-days across 17 tickets against the scope's 17-24 for the same surface (D1 9-12 + D4 shell 5-7 + Option B scaffold 3-5). The difference is that every ticket here carries its own deploy and its own watched live-stack proof, including the two operator-in-the-loop ceremonies, rather than ending at a green test run.
- TEST DISCIPLINE inherited, not invented: RouteSpecs must all stay in the single array in apps/api/lib/api.ts because admin-guard.test.ts parses that file as TEXT (:163-222); every module under src/handlers/admin must export adminHandler(...) or the enumeration arm fails; grants.test.ts must read BOTH the parent and nested templates once AdminApiStack exists, or its per-route IAM assertions go vacuously green for every new admin route.
- NOT IN THIS WORKSTREAM, but flagged because it is a live safety issue in the same decision log: D-DIST-1 conflates two different CloudFront allowances. The flat-rate FREE PLAN is 100 GB + 1 M requests/month (~25 downloads of the 3.9 GB build), not 1 TB — the 1 TB / 10 M figure is the pay-as-you-go always-free tier. D-DIST-3/D25's edge logs are also NOT included on the flat-rate Free plan. The distribution workstream owns the correction; nothing in workstream B depends on it.

**Open questions:**
- D3 outcome: does the MFA tab actually appear on this account's Zero Trust plan, and does an Independent MFA prompt follow a One-time-PIN login? If not, is the fallback Google as the Access IdP (still $0, PLAT-B01's branch) or WebAuthn-in-Lambda (+3-4 days, re-plan before PLAT-B10)?
- Which email addresses go on the Access Allow policy, and is the operator's Access identity to be bound to the owner row by the one-time bind on first elevation (PLAT-B10) or pre-set by break-glass before the first login?
- Confirm on screen at ironics.org/portal that 01M06WNV71GP2QWYPBQA5JT4WC is the operator's own Epic-linked account and not the 2026-08-24 email test account. A wrong owner row becomes unrecoverable through the console once the last-owner guard exists (PLAT-B07).
- Is the Cloudflare account on Workers Paid or Workers Free? It is recorded nowhere on disk. On Free, admin.ironics.org shares ironics.org's account-wide 100k requests/day and a 10 ms CPU ceiling per request; if the console shell exceeds it, viewers get Error 1102 and the fix is the $5/month Workers Paid minimum — which needs a ruling against 'zero new paid vendors' (C3).
- PLAT-B08's enforcement proof needs an identity that is an admin without owner permissions. Acceptable to briefly downgrade the live owner row to viewer in a watched window (scripted restore, break-glass as recovery), or should a second account be created first and the proof deferred until PLAT-B13 grants it?
- For PLAT-B13's grant/revoke proof: may a throwaway second account be created (sign in with a second email on /portal) purely to be granted and revoked, or must that ceremony wait for a real second admin — which would also trigger D2's treasurer/GSI/CSV work?
- Who is the Required reviewer on the GitHub `production` environment for the new crm pipeline? With one engineer, a required reviewer means the operator clicks approve on every console deploy; confirm that is intended and not a bottleneck you want removed later.
- Does ironics.org/admin stay live until the economy workstream ports Payments into the console (D9's build order implies yes), or should it return 410 the day admin.ironics.org has a shell — accepting that Payments is then unreachable in the interim?

### D — Distribution: CloudFront + OAC + key-group signed URLs in front of s3://ironics-releases, metadata-only endpoint, mint quota + download.enabled kill switch, standard logs v2 → Athena → nightly snapshot, cost/abuse alarms, retirement of presigned S3, version-retention lifecycle

**Assumptions:**
- LIVE STATE READ 2026-09-06 (read-only CLI, acct 302659227808, us-east-1) and used as the baseline for every ticket: ZERO CloudFront distributions, ZERO key groups, ZERO public keys, ZERO CloudWatch alarms, NO bucket lifecycle configuration on ironics-releases. AWS Budget 'ironics-monthly-cost' ALREADY EXISTS at $50/month with three notifications (ACTUAL>50 %, ACTUAL>90 %, FORECASTED>100 %) and one confirmed EMAIL subscriber vrdivebar@gmail.com — so PLAT-D11 verifies and extends it rather than creating it.
- s3://ironics-releases holds MORE than releases: win64/ has 6 objects / 18.6 GiB (0.1.0, 0.1.1-beta, 0.1.2-beta, 0.1.2.1-beta, 0.1.3-beta + latest.json, 356 object versions, 5.8 KB of non-current data), and a `server-build/` prefix of ~350 objects (~1.1 GB) holds the LIVE S12 dedicated-server artifact. Every OAC bucket-policy scope and every lifecycle filter in this workstream is written so the CDN cannot read, and no rule can archive, `server-build/`.
- COST BASIS CORRECTED: Tier 0 runs on CloudFront PAY-AS-YOU-GO using the always-free 1 TB egress + 10 M requests, NOT the flat-rate FREE plan. The flat-rate Free plan is 100 GB + 1 M requests (≈25 downloads of a 3.6 GB build) and includes NO access logs — it cannot carry D-DIST-3/D25 edge telemetry. The '$15 tier' in O3/D-DIST-1 is the CloudFront PRO flat plan, and PLAT-D14 pre-stages it behind a budget trigger. See open question 1.
- Steady-state incremental cost of this entire workstream at current volume: ≈$0.45-0.60/month (Secrets Manager $0.40 for the signing key + edge-log storage and Athena cents), while REMOVING today's S3-direct egress line (~$5/month now, ~$70/month at 250 downloads). $15/month applies only if the Pro flip triggers. Inside the $50 cap with room.
- ZERO-DOWNTIME MIGRATION is a property of the ticket ORDER, not a step: PLAT-D04 is additive infrastructure with no request-path change; PLAT-D06 is a two-deploy cutover driven by three env vars (absent = today's presigned path), so a tester mid-download is never touched and a rollback is a variable removal, not a code revert; PLAT-D07 removes the presigned branch only after ≥24 h on CloudFront, by which time every 900 s presigned URL has expired; PLAT-D12's Glacier IR transitions are tag-scoped away from the hot releases and are instant-retrieval anyway; PLAT-D16's host flip leaves both hostnames answering.
- D25 in the DECISION LOG ('edge logs per D-DIST-3, no longer deferred') supersedes the earlier admin-scope line that deferred CloudFront→Athena behind the $50 cap, so PLAT-D09/D10 are in scope. The scope doc's ~$1-6/month telemetry envelope still holds.
- The snapshot Lambda writes roll-up rows into IronicsCounters as `dl#<version>#<yyyy-mm-dd>` (and `dlflag#<date>#<rid>` for replay candidates). If the metrics workstream ships a dedicated roll-up table first, PLAT-D10 targets that table instead — the contract is the row shape, not the table name. The admin console's Telemetry > Downloads panel and the Releases > Revoke-link button are the ADMIN workstream's; this workstream ships the data and the mechanism.
- src/lib/flags.ts (SSM reader, TTL cache, last-known-good) is created here if the admin workstream has not shipped it yet; if it has, PLAT-D08 imports it and only adds the download.enabled consumer + the fail-CLOSED rule. The PUT /v1/admin/flags write route stays in the admin workstream; until it exists, the parameter is edited with `aws ssm put-parameter`.
- Resource-count arithmetic: this workstream adds ~7 (D01 route) + ~6 (D04) + ~5 (D09) + ~12 (D10) + ~6 (D11) + ~2 (D13) + ~2 (D14) ≈ 40 resources to the parent IronicsPortalStack, taking the live template from 179 to roughly 218 — comfortably under the ruled synth assertion of <450, so the distribution constructs stay in IronicsPortalStack rather than a nested DistributionStack (the bucket, the secret and the mint Lambda all live there; nesting would only buy cross-stack parameters).
- Deploy mechanics unchanged: `deploy-api.yml` (GitHub OIDC, no stored key) deploys IronicsPortalStack on every push to main touching apps/api/**; the agent runs `npm test -w @ironics/api` + `npm run synth` + `npx cdk diff` locally first. The web half (DownloadCard copy) rides the web pipeline and is gated on D6's Cloudflare token permission — an API-only deploy is always safe on its own because the card degrades to today's copy.
- Key material is operator-only: the agent never generates, holds or writes the CloudFront private key (PLAT-D03/D05), consistent with D5(b) giving `ironics-agent-deploy` no Secrets/IAM/KMS write. Every ticket needing a secret write is an operator ticket.
- Estimates are one engineer, and each includes writing the tests, the CI deploy and the watched/asserted proof — not just the code. They exclude operator wall-clock waits (ACM validation, SNS confirmation, the 31-day Glacier check) and exclude any UE/game work (this workstream touches no engine, no cook and no build box).
- The 24 h bearer-URL policy, no IP binding, 3/day mint quota and metadata-only endpoint are taken as RULED (D-DIST-2) and implemented as written; the review's 12 h suggestion is raised as an open question, not silently substituted.
- Release semver stays 0.1.x-beta (D28/W4) and `latest.json` remains the single mutable pointer; the pointer-editing routes (publish/promote/rollback/set-minBuild) are the admin workstream's — this workstream only hardens the publish SCRIPT and the ledger records it produces.

**Open questions:**
- RULING SAFETY FLAG (evidence, needs an appended amendment): D-DIST-1 and C3 say Tier 0 runs 'on the CloudFront FREE pricing plan' with 'the CloudFront free allowance … 1 TB/month of egress and 10 M requests (always-free), i.e. ~250 downloads/month'. Those are two different products. The flat-rate FREE plan is 100 GB + 1 M requests/month (~25 downloads of the 3.6 GB build) and its feature table shows NO access logs — which would make D-DIST-3/D25 edge telemetry impossible. The 1 TB + 10 M always-free allowance exists only under PAY-AS-YOU-GO. Also: any flat-rate plan requires an attached WAF web ACL and is refused to accounts 'using AWS Free Tier'. RECOMMENDED AMENDMENT: Tier 0 = pay-as-you-go on the always-free tier (OAC + key-group signed URLs, standard logs v2 → S3 free to deliver); Pro ($15) stays the ruled scaling step, triggered by the new CloudFront budget. Approve?
- CloudFront price class: PRICE_CLASS_100 (US/Canada/Europe, cheapest) or PRICE_CLASS_ALL (global edges)? Where are the first 1,000 testers expected to be? Default in PLAT-D04 is 100 pending your answer.
- Edge-log PII/credential posture: D-DIST-3 rules 'c-ip dropped at the Athena view (kept 30 days raw)'. The same log rows also hold LIVE signed URLs in cs-uri-query for up to 24 h, so the log and Athena-results buckets are credential stores. Confirm: (a) keep c-ip + cs-uri-query for 30 days with the bucket locked down (as ruled), (b) shorten log retention to 7 days, or (c) drop cs-uri-query entirely — which kills per-link (rid) attribution and reduces the Downloads panel to per-object counts.
- Link TTL: 24 h bearer with no IP binding is ruled (D-DIST-2) and implemented. The adversarial review suggested 12 h (3.6 GB at 3 Mbps is still only ~3 h). Keep 24 h?
- Mint quota 3/day is ruled. Confirm it is per ACCOUNT per rolling 24 h and that a tester who exhausts it should be told to reuse their existing 24 h link (that is the copy PLAT-D08 ships), rather than having the quota auto-reset.
- AWS Free Tier status of account 302659227808: any flat-rate CloudFront plan (including the free one) is refused to accounts 'using AWS Free Tier', and plans 'may not be combined with any other offers, promotions, or discounts' — which may mean the $15 cannot be paid with the existing promotional credits. Please read the account's plan/credit status in Billing before PLAT-D14 is triggered.
- `server-build/` lives in the same bucket as the releases (~350 objects, the live S12 dedicated-server artifact). Confirm it must remain (a) unreachable through the CDN and (b) untouched by any lifecycle rule — and confirm whether the EC2 box still pulls from that prefix, so archiving it is never safe.
- Retention: PLAT-D12 keeps 0.1.3-beta (current) and 0.1.2.1-beta (rollback target) hot and cools 0.1.0, 0.1.1-beta and 0.1.2-beta to Glacier IR after 30 days. Confirm that pair, or name a different rollback target.
- OPTIONAL, needs approval because it rewrites a shipped object's metadata: the live 0.1.3-beta zip has no Content-Disposition/Cache-Control (they only apply to new uploads). A single CopyObject with MetadataDirective=REPLACE would add them (3.6 GB < the 5 GB single-copy limit; bytes and sha256 unchanged, a new version of the same key). It is NOT needed — the filename already comes from the URL path. Do it, or leave the shipped object alone?
- AWS WAF on pay-as-you-go costs ~$5/ACL + $1/rule + $0.60/M requests, and is only bundled once the Pro plan is active. PLAT-D14 therefore keeps the ACL behind a CDK flag and attaches it only in the hours before the plan approval. Confirm that sequencing, or say if you want WAF on from day one at ~$8-10/month against the cap.
- Roll-up destination: the nightly Athena snapshot writes `dl#<version>#<date>` rows into IronicsCounters. If the admin/metrics workstream ships a dedicated roll-up table, which one owns the Downloads panel's numbers? (Two writers with different shapes is the drift risk.)
- dl.ironics.org (PLAT-D16) is NOT in the ruled Tier 0 scope and is not needed for signed URLs. Build it now for branding, or leave it deferred until the launcher (which needs signed cookies and therefore a same-site host)?
- Alert destination: PLAT-D11 sends to SNS → your email (D18). Confirm the address to subscribe, and confirm you will click the SNS confirmation link (an unconfirmed subscription is a silent alarm).

### H — Growth engineering (founder invites, Discord role sync, press kit, signed installer, weekly cohort report)

**Assumptions:**
- Workstream H sits ON TOP of, and never rebuilds, work owned by other workstreams: the console shell + roles + 15-min elevation (D1-D4, D7), the metrics spine (IronicsEvents + roll-ups + the server-signed tentpole->portal game-event hook, D22/D24), the SSM flags service (D27), the honest download metric (D25 / D2 fix #2 metadata-only endpoint), the nested AdminApiStack, and the CI-only deploy path (D6). Every H ticket that needs one names it rather than duplicating it — H05 needs roles, H12 needs the spine and the download split.
- The '$50/month cap' is read as incremental platform spend excluding the credit-covered S12 EC2 baseline (C1 itself records ~$62/month gross for that server against a $50 alarm). H's own additions are DynamoDB/Lambda/EventBridge pennies plus one small S3 press bucket; the only real money is Azure Trusted Signing (~$10/month, from PLAT-H13 only, and only at 100 testers).
- An invite code confers ATTRIBUTION and — only if invite-only mode is later switched on — a bypass. It cannot confer approval, because auto-approval is permanent (W6). No surface may say otherwise, and no in-game reward is printed until G-FOUNDER defines one (W7).
- Codes are issued only to accounts holding a founder number, so the total is bounded at 3 x FOUNDER_CAP = 3,000 (apps/api/src/lib/founder.ts:3).
- No new server event name is introduced. SERVER_EVENTS (apps/api/src/lib/events.ts:17-26, mirrored in apps/web/lib/analytics.ts, pinned by test/events.test.ts) stays exactly as it is; invites ride utm_source plus an `invited` prop on application_completed.
- AuditAction (src/lib/audit.ts:5-13) gains only `invite.revoked` — an admin mutation. A player redeeming a code is recorded on the invite row, not in the operator's audit log, so the Audit tab stays an operator record.
- C:\Dev\Ironics-Platform has NO Git LFS (no .gitattributes), so anything committed under press/ is permanent weight in every clone and every CI checkout. That is why H09 splits 2-3 published reels into git and everything else into an S3 press bucket, inside W17's ruling.
- Nothing about Azure Trusted Signing — pricing, tiers, eligibility, validation timeline, dlib usage — is verified in this plan. PLAT-H13 verifies all of it against Microsoft's documentation before any spend, and PLAT-H14 does not start until it does.
- The Discord server does not exist and no Discord integration exists in either repo (the only hit is the placeholder https://discord.gg CTA at apps/web/app/roadmap/page.tsx:56). PLAT-H07/H08 are inert until the operator creates the server, the application, the bot and the roles.
- The 16 approved artboards contain no invite, Discord or press screen (gen_artboards.py:353-354), so PLAT-H01, H06 and H10 are doctrine-7 gates, not optional polish. Their checkmark is a recorded operator approval, which is the one legitimate exception to 'PROVEN = watched on the live stack' because they deploy nothing.
- Estimates are single-engineer days including tests, deploy and the live proof, and assume the CI deploy path is green (web CI has been red-but-shipping since 2026-08-25 pending the Zone -> Workers Routes -> Edit token fix, D6).
- No ruling in the decision log was found unsafe for this workstream. The one cost correction already known to the session — D-DIST-1's conflation of the CloudFront flat-rate FREE plan (100 GB / 1 M requests) with the pay-as-you-go always-free tier (1 TB / 10 M) — belongs to the distribution workstream, not H, and is left to it.

**Open questions:**
- Invite issuance trigger: on approval, or only after the founder's first honest download click (D23)? The second is a real anti-farming control and a real gratification delay — it changes one condition in PLAT-H02.
- N = 3 confirmed? And does it step up at a milestone (say 5 at 300 testers)? It ships as a code constant with an SSM override.
- Should a redeemed invite ever be worth something material to the inviter — a founder-only cosmetic (G-FOUNDER), ladder position, extra codes? Nothing is printed anywhere until this is ruled (W7).
- Invite-only mode: should `apply.invite_only` (a valid code bypasses a closed door) ship in the first cut, or stay a later flag flip?
- Discord: do you accept storing a Discord user id per account, and will the Privacy Notice v0.3 counsel pass (W25) name it? PLAT-H07 ships flag-off until it does.
- Discord scope: is `guilds.join` acceptable (we add the member and role in one call), or should linking be identify-only with members joining by invite link?
- Which of the 8 approved matchplay reels are published on /press, and may the 4K originals leave the build machine into an S3 press bucket?
- Trademark clearance status on IRONICS + the logo — the press kit invites reproduction of the mark and the content pass records clearance as an unchecked GTM item. Yes/no blocks PLAT-H11.
- Sponsor line for the kit: the approved one-line descriptor and mark for Simularent (the current boilerplate sentence has no source on disk), given the game-side sponsor is FANATICS.
- press@ and security@ mailbox routing (W28) — confirmed before the kit ships with a contact address?
- Signing: is C12 AI Gaming eligible for Azure Trusted Signing public trust? If validation is refused, is a CA OV/EV certificate approved instead, or do we stay unsigned and keep the SmartScreen guidance?
- Weekly cohort digest: which address, and do you want it in Discord as well once the server exists (D18)?
- Does the $50 cap tolerate Trusted Signing (~$10) + CloudFront Pro ($15) + a Workers Paid plan ($5) simultaneously, or is signing traded against one of them when the 100-tester trigger fires?

### G — Content-pack cull A + C: ARCANEON slice migration to /Game/BagMan/ArcaneonArt, 4K cap on the slice's 8K/16K normal+ORM maps, client AND server Shipping cook proof, watched cooked ARCANEON lap, then the confirm-first repo removal. Ships in 0.1.4-beta with workstream D.

**Assumptions:**
- DESTINATION: the slice lands at /Game/BagMan/ArcaneonArt with Meshes/Materials/Textures subfolders (the audit's suggested path, adopted by O2). It sits under no DirectoriesToAlwaysCook / MapsToCook / PrimaryAssetTypesToScan entry, so it cooks only by being reached from L_Arena_04 — identical mechanics to today.
- BARRIERS: the two unreachable barrier Blueprints Content/BagMan/Barriers/B_BagMan_CyberBarrier_BR.uasset and B_BagMan_CyberBase_BR.uasset are RE-POINTED, not deleted. That adds 5 packages to the slice (SM_Column01 125,721 B, M_Column02 133,504 B, T_Column01_Base 2,964,864 B, T_Column01_Normal 6,253,994 B, T_Column01_ORM 5,834,976 B = 15,313,059 B) and 0 download bytes (none of them cook today). Re-pointing is the non-destructive default and it permanently removes the audit's latent 're-drag both packs' trap; deleting them is an operator call.
- MIGRATION SET: 65 packages = the 60-package cooked closure from cook_20260902_windowsclient_postprune.csv (5 DWS meshes + 9 DWS materials/MIs + 23 DWS textures + 4 CPA meshes + 4 CPA materials + 15 CPA textures) plus the 5 barrier extras. PLAT-G02 re-derives this from the live editor dependency graph and the set is allowed to GROW if the editor closure exceeds the cook closure — the cook manifest cannot see a texture bound to a material parameter the cook stripped.
- 4K CAP MECHANISM: option C is implemented as the per-asset UTexture::MaxTextureSize=4096 on the slice's normal + ORM textures whose measured source dimension exceeds 4096 — NOT the D2 Tier-1B(vi) [Windows DeviceProfile] TextureLODGroups MaxLODSize approach. Two reasons on disk: (a) the D2 approach caps every World-group texture project-wide, spreading the 'don't re-tune proven' exposure far beyond ARCANEON; (b) these DWS maps are virtual textures (VirtualTextureStreaming is serialised in T_Mod02_Int_N, T_MetalWall07_N, T_Ladder01_N, T_MOD02_N, T_Wall09_N) and the LOD-group cap for VT is the separate MaxLODSize_VT field, which Config/DefaultDeviceProfiles.ini leaves at 0/unset. The per-asset property does reach the VT build path (TextureDerivedData.cpp:1174-1177 and :1427).
- CAP SCOPE: normal and ORM maps only, per ruling O2/option C. BaseColor (_BC/_Base) and Emissive (_E/_Emissive) are untouched, so T_Mod02_Int_BC (277,976,404 B cooked) and T_MOD02_BC stay at full resolution. The CPA normals are already ~4096 (22,364,160 B cooked) and are expected to be no-ops.
- ARCANEON STAYS IN THE ROSTER. Option B (re-dress the venue off the packs) and 'retire ARCANEON' are out of scope; no playlist, experience, venue-showcase entry or C++ map string is touched.
- OWNERSHIP: the agent owns the editor, the builds, the cooks, the staging and the provenance/zip mechanics end-to-end (operator ruling 2026-08-27, reaffirmed 2026-09-05). The operator's parts are exactly three: the watched cooked lap and the option-C visual verdict (PLAT-G10), the routine commits/push (PLAT-G11), and the confirm-first destructive removal (PLAT-G12).
- BUILD ENVIRONMENT: one engine, D:\UE5.6-source, invoked by explicit path; 16 GB box with UBA disabled in BuildConfiguration.xml and -MaxParallelActions=2 on any compiling step; the gitignored Config/Custom/EOS/DefaultEngine.ini must be present for every Shipping client cook; C: has 65.9 GB free and D: 442.4 GB free as measured 2026-09-06.
- COOK COUNT AND TIME: this workstream runs SIX Shipping cooks (server control, client post-move, client post-cap, server post-change, and the client+server final pair after the removal) at roughly 20-40 minutes each on this box. That deliberate layering — cook the move alone, then cook the cap alone — is what makes the move provably byte-identical and the cap's saving measurable rather than estimated; collapsing them into one cook would make a regression unattributable.
- EXPECTED SIZE OUTCOME: source/fresh-clone −9,741,663,749 B (11,710,909,627 removed minus 1,969,245,878 retained); git-LFS history +≈1.95 GB (the slice packages are rewritten by the move, so they are new LFS objects) with GitHub-side pack storage unchanged (no history rewrite — BUILD_PRUNE_DEPLOY_PLAN §4 rule 5); download −120..−170 MB estimated from option C alone (zip 3,906,132,933 B → ~3.74-3.79 GB), to be REPLACED by the measured number in PLAT-G07. Option A contributes exactly 0 download bytes.
- RELEASE: 0.1.4-beta ships this together with workstream D. Workstream G ends at a provenance-verified staged build plus a committed size ledger; the publish (S3/CloudFront pointer, latest.json, portal copy) is D's gate.
- OUT OF SCOPE, separately ruled by the audit: the stale git worktree C:\Dev\Bag_Man\.claude\worktrees\elastic-jennings-11d538 (66,299,276,697 B duplicate checkout, never built from) and C:\Dev\Bag_Man\Saved\Autosaves_parked_20260827_cce (837,006,616 B untracked). Also out of scope: the ~3.3 GB of demo/showreel/HDRI content inside the USED AFLHub/MegaBase/US_Military plugin.

**Open questions:**
- BARRIER BLUEPRINTS: re-point B_BagMan_CyberBarrier_BR and B_BagMan_CyberBase_BR onto the slice (the plan's default — costs 15.3 MB of retained source, 0 download bytes, keeps the assets working), or DELETE them? They are unreachable in every cook since 09-02 and were cooked in every August manifest; deleting removes the latent 're-drag both packs' trap permanently but discards work.
- SLICE FOLDER NAME: confirm /Game/BagMan/ArcaneonArt (audit's suggestion) — the path is baked into 65 package names, both dev art-pass scripts and every cooked manifest from here on, so renaming it later is another full migration.
- Once the two ROOT NeverCook lines land, do the now-redundant subfolder lines Config/DefaultGame.ini:344 (/Game/CyberPunkAssets/Maps) and :345 (/Game/DeepWaterStation/Maps) stay or go? Purely cosmetic; the plan keeps them.
- APPROVAL FOR THE DESTRUCTIVE STEP (PLAT-G12): `git rm -r Content/DeepWaterStation Content/CyberPunkAssets` (940 LFS-tracked files, 11.7 GB) plus `git lfs prune --verify-remote`. Recovery after this depends on the LFS objects remaining on the remote. Confirm before it runs.
- OPTION-C FALLBACK: if the ARCANEON walls read soft at the 4K cap during the cooked lap, which fallback do you want — (a) revert the cap on only the offending textures, (b) cap at 8192 instead of 4096 (smaller saving), or (c) ship A without C (zero download saving from this workstream)?
- SEPARATELY RULED CLEANUP: remove the stale worktree .claude/worktrees/elastic-jennings-11d538 (66.3 GB, branch claude/elastic-jennings-11d538 @ 74aee050, never built from) and Saved/Autosaves_parked_20260827_cce (837 MB untracked)? Together they are ~6x the disk this whole cull reclaims, and neither ships.
- 0.1.4-beta DATE: is there a fixed ship date paired with workstream D? Six Shipping cooks plus a watched lap is ~10 engineer-days of elapsed work on this box; if the date is tight, the cooks can be sequenced overnight but the lap and the removal approval need you awake.
- PUSH: PLAT-G11 pushes ~1.95 GB of new LFS objects. Confirm the remote (origin vs `personal`) and that a foreground push of that size is acceptable — the slice objects must be on the server before the packs are removed locally.

### E — Economy LITE + metrics spine (decision log §3: D13 lite shadow journal, D12 key split, D15 rake alignment, D18 alerts, D19 KPI contract, D22-D25 metrics/funnel/telemetry; scope §D.2 E1/E2/O1 and §D.3 layer 1; adversarial fixes #7, #8, #9)

**Assumptions:**
- Cross-workstream prerequisites I depend on but do not own: (a) the console shell on admin.ironics.org (apps/crm, D7 Option B) plus roles/permissions, the 15-minute elevation token and the fail-closed admin limiter — every screen ticket (E09, E11, E15, E16, E17, E18) needs them; (b) the nested AdminApiStack + the synth assertion Resources < 450 — every new admin route lands there, and the parent template is at 179 resources with 7 per route today; (c) D2 review fix #2, the metadata-only GET /v1/download/latest/meta — PLAT-E08's 'honest download click' and PLAT-E09's Downloads panel are both meaningless until the mount-fetch stops minting (DownloadCard.tsx fetches on mount AND click; latest.ts:71-78 mints and emits on both); (d) CloudFront Tier 0 for PLAT-E21.
- Route count is 22, not the 21 recorded in the admin scope (lib/api.ts:66-248; the live template carries 22 AWS::ApiGatewayV2::Route). This workstream adds roughly 10 admin routes plus 1 inbound route plus 3 non-route Lambdas; at 7 resources per route they must go in the nested AdminApiStack, and nested routes must be constructed with apigw.HttpRoute inside the nested scope — api.addRoutes from the parent leaks Route + Integration + Permission back into the parent template.
- Only 4 of the 8 locked funnel events have a writer today (email-verify.ts:50, apply.ts:84, admin.ts:117, latest.ts:78). email_submitted, install_completed, first_match_completed and waitlist_joined have none; install_completed has no possible source at all because the beta ships a zip, not an installer.
- A LeaguePlay (unstaked, unrated) match posts nothing to any backend endpoint at match end: /settle-match is gated on Result.bStaked (AFLMatchReporter.cpp:1117-1119) and /update-rating on isRated = isStaked (queue-registry/registry.ts:73). Since staking is HOLD/not-public, every beta match is unstaked — so the North Star metric (installed -> first match >= 70 %) requires the new emitter in PLAT-E06, which is a game-code change with an engine build and a PIE lap, not a backend-only ticket.
- The tentpole repo has NO CI (Bag_Man_Backend/.github does not exist; package.json:9 'deploy': 'cdk deploy'), so every tentpole deploy in this workstream is hand-run under the ironics-agent-deploy principal, and every coordinated change is sequenced tentpole-first (accept both keys) -> portal -> tentpole-again (drop the old key). Portal deploys go through .github/workflows/deploy-api.yml (OIDC).
- The 'append-only' claim is stated honestly throughout: append-only for APPLICATION principals, tamper-EVIDENT via CloudTrail. The live audit table's resource policy denies Update/Delete but not DeleteResourcePolicy or DeleteTable, so the money journal gets deletionProtection + PITR + the S3 Object Lock export as the only WORM available without Organizations SCPs.
- The lite journal is written tentpole-side only (currency-earn, refund-purchase, admin-adjust) so the portal never writes a tentpole table and the 'shares no construct, no table and no function' invariant (ironics-portal-stack.ts:13-16) survives. The portal's Volts mints are captured because they ride /earn (approve-claim.ts:107-121, matchId 'claim:<code>').
- Dual-control thresholds are code constants (D11), not SSM flags, and the four-eyes rule is gated on a live count of ACTIVE owners rather than a switch — so it arms itself at the first second-admin grant without anyone remembering to turn it on.
- Proofs that would move money are kept to the smallest possible amounts (1 V = $0.001 per the locked economy) and are named as operator decisions rather than assumed: PLAT-E12's canary earn, PLAT-E14's deliberate 1 V drift injection, PLAT-E18's +1 V adjustment.
- Cost: PLAT-E13 is the only new bucket; three new Secrets Manager entries at $0.40/month each (admin, portal-inbound, resolve); DynamoDB on-demand for IronicsEvents, bagman-money-journal, bagman-econ-daily, bagman-first-seen, bagman-first-match, bagman-account-holds and IronicsWalletAdjustments is cents at beta volume; alarms are kept at or under 10 to stay inside the CloudWatch free tier. Total well inside the $50/month cap and inside C2's ~$3-10 estimate.
- PLAT-E02 deviates from D12's literal wording (see its risk field) by using a third read-only key rather than extending bagman/admin/hmac into the public Epic-callback Lambda. This preserves D12's stated intent and is the alternative the scope itself names; it is flagged, not re-opened.

**Open questions:**
- PlayStream export (PLAT-E13): is the AWS S3 Data Connection PREVIEW available on title 1A2077 in Game Manager? Its overview page names 'Amazon Web Services S3' as a destination but the dedicated how-to page 404s, so the IAM/trust shape is unverified. If it is not available, do you accept the legacy Event Archive path, which requires a dedicated IAM access key handed to PlayFab (a new long-lived credential, against the spirit of D5)?
- S3 Object Lock mode for the PlayStream export: GOVERNANCE (a privileged principal can override, so a mistake is recoverable) or COMPLIANCE (nobody, including root, can delete before expiry)? D13 says the WORM export is non-optional but not which mode; COMPLIANCE is irreversible for the retention period.
- D12 wording (PLAT-E02): 'bagman/admin/hmac ... extended to /resolve-identity' — confirm this means the key SPLIT is extended to cover /resolve-identity (a third, read-only bagman/resolve/hmac), not that the admin key itself goes into the public unauthenticated Epic-callback Lambda. The literal reading gives that public Lambda the ability to sign /admin-adjust.
- install_completed (PLAT-E09 / the Telemetry artboard): the beta ships a zip, not an installer, so this locked funnel step can never be emitted. Drop the row from the rendered funnel, or alias it to 'launched'? D24 locks the 8 names, so this is a rendering decision, not a rename.
- D18 alert destination: which email address receives the money and health alarms, and is there a second recipient? (You must click the SNS confirmation link before PLAT-E03 can be proven.)
- Watts stake rungs (PLAT-E19): the SSOT's six-rung ladder is stated in Volt-equivalent per seat, while R80 deliberately puts the Watts ladder BELOW peg (registry.ts:78-84 today: 250/1000/5000/25000). Extend Watts to six rungs at your chosen values, or leave Watts at four and extend only the Volts ladder?
- D15 effective date (PLAT-E19): settlements already paid at flat 5 % will not be restated when the tiered 5/10 % rule ships. Confirm the effective date and that no back-correction is wanted.
- Money proofs: do you approve the three small real currency movements used as checkmark evidence — a canary earn (PLAT-E12), a deliberate 1 V drift injection to prove the drift alarm fires (PLAT-E14), and a +1 V adjustment on your own account (PLAT-E18)? Naming a throwaway PlayFab account instead is fine, but then the wallet-inspector proof shows a zero balance.
- PLAT-E18 ships the first console control that moves currency on free-form operator intent, with no void or refund path (D17 blocks one until the L1 Virtual Currency Terms are in force). Confirm you want it in this pass rather than deferring adjustments until counsel lands.
- Elevation/roles dependency: PLAT-E09/E11/E15/E16/E17/E18 assume the console shell, roles and the 15-minute elevation from the Phase-1 workstream. If Phase 1 slips, do you want the read-only screens (Telemetry, Catalog, Escrow & KPIs) shipped behind the existing flat allowlist as an interim, or held?

---
*Generated 2026-09-07 from workflow wf_88bcaaa3-b6e (4 fact-checks, 8 planners, integration, 3 reviews) without paraphrase; section A is the agent's correction set.*
