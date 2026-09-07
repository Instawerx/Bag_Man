# IRONICS Platform - Grounding Brief (Website · Admin Dashboard · Roadmap · Distribution)

**Status:** GROUNDING, 2026-09-06 - nothing decided, nothing implemented. Operator mandate: *get grounded -> scope -> plan -> then decisions.*  
**Method:** 5 gather lenses (platform repo + design/, game Docs/, memory notes, admin controls today, live site/roadmap) -> planner -> completeness critic (verdict: **grounded-with-gaps**) -> the agent's corrections below.

## 0. Corrections applied after the critic (read these first)

- **The Tier-1 roadmap EXISTS.** `IRONICS_GTM_PLATFORM_ROADMAP.md` v2.3 (2026-08-26) lived only in `C:\Users\tabor\Downloads`; it is now committed at `Docs/IRONICS_GTM_PLATFORM_ROADMAP.md` (canonical) with a citation stub in `Ironics-Platform/design/`. Its **Phase W5 'Beta Control Center'** is the fullest admin-dashboard spec on record (panels TESTERS / POPULATION / FLEET / BUILD / HEALTH / FEEDBACK+MOD QUEUE; actions APPROVE / BULK APPROVE / REVOKE / BAN / OPEN-CLOSE COHORT / PUBLISH BUILD / SET MINBUILD / BROADCAST; separate admin auth, AuditLog on every mutation, typed confirmation, server-side bulk throttle). BPD §12 is the shorter, later (09-01) operator-approved restatement.
- **VR is not a phantom.** `IRONICS_VR_SSOT.md` + `IRONICS_VR_TASKS.md` (2026-09-02, LIVE, rulings R-VR-1..6: PCVR-first, mixed pools, phases VR-0..VR-5 / AFL-34xx) also lived only in Downloads; now at `Docs/design/`. The public roadmap's 'VR - 7 Sep 2026' row has an engineering trail but VR-0 (capability probe) has not started - the DATE is the problem, not the item.
- **Analytics 'warehouse' is already decided:** GTM §6 rules **PostHog** (product analytics), Cloudflare Web Analytics, Sentry, Resend, Loops (CRM), a Discord bot. The API README's 'structured log until a warehouse is chosen' predates/ignores that ruling.
- **Distribution has THREE positions on record:** GTM §7 (08-26) rules **R2 + Worker signer + engine-native patch paks, launcher deferred with triggers**; BPD §11 (09-01, operator-approved) rules **CloudFront + OAC + signed URLs**; D2 (09-06) proposes CloudFront over R2 with numbers. D1 shipped **presigned S3** (neither). This is an operator ruling, listed under decisions.
- **CI web deploy:** the cause is a missing zone permission (Workers Routes read) on the CI token - NOT an expired token as memory and D2 decision 13 recorded. CI has been red-but-shipping since 08-25 and races local wrangler deploys.
- **Other Downloads-only docs:** `IRONICS_AAA_BUILD_TODO.md` (17-item operator build list, now at `Docs/`); an older export of `IRONICS_ECONOMY_SPEC.md` (repo copy is newer - NOT imported); Lobby-Hub SSOT/TASKS/CC-integration exports that differ from the repo copies (repo canonical - diff not reconciled). **Secret-bearing notes** (a super-admin password note, an `aws secretsmanager put-secret-value` snippet, a wallets note) were NOT opened - they should leave Downloads for a secrets manager.

## 1. Where we are

The closed-beta client is SHIPPED-LIVE and operator-proven: 0.1.3-beta (3,906,132,933 B, sha256 b2b78947...) is the live latest.json pointer in s3://ironics-releases, downloaded through ironics.org/portal and played through a round transition and the BR end card on 2026-09-06 (memory project_ironics_portal_download_live_state.md:40-46; Bag_Man commits 639e5c91, a04ac752). The live API says the beta has exactly one member -- https://api.ironics.org/v1/stats returned founders.issued=1 / tier1 remaining 99 when fetched 2026-09-06 -- so every 'cohort', 'invite', 'review' and 'testers' word on the public site describes a process that has never run, and the roadmap's 'crash telemetry' does not exist (apps/api/src/lib/events.ts:17-26 emits only download_started). The two-week detour ran Track C cook -> release 0.1.0 (9ff73c3d 09-04) -> D1 portal download (Ironics-Platform ba3f7e3/ffb9829/b41313a 09-04) -> first playtest Batches A-D (61e9ecce, 5c85b5d7, d2ddba2e, 0f23e2f3) -> S12 EC2 24/7 server (memory 09-05) -> 0.1.1/0.1.2/0.1.2.1/0.1.3 fix-patches incl. the bsdtar empty-zip incident -> the D2 proposal (77dd8443 09-06); it consumed the lane that Docs/BUILD_PRUNE_DEPLOY_PLAN.md section 13 item 6 reserved for the section 12 admin/website final pass, which is still four bullets (lines 260-271) with no design, task list, mockup or tracker row, and whose D1 deferral ('sequenced after D1 proves', IRONICS_D1_DISTRIBUTION_PLAN.md section 7) has now expired. The Ironics-Platform repo (HEAD b41313a, main == origin/main, clean) has had no website, admin, roadmap, design or analytics commit since 2026-08-25 apart from the 09-02 Beta Lands copy (784444e) and the 09-04 download slice. The only admin UI is a payments console at /admin (claim codes -> mint Volts, unattributed queue, audit; 8 API routes at apps/api/lib/api.ts:180-241) built 2026-08-24 without a mockup, with no account/release/live-ops surface, never exercised with real money, and with no record anywhere that apps/api/scripts/seed-first-admin.ts was ever run -- so it is UNKNOWN whether /admin renders for the operator today. The public /roadmap is a hand-edited 7-row array (apps/web/lib/data.ts:116-124) printing 'REV 0.8.14' (hardcoded fallback, lib/stats.ts FALLBACK_BUILD_REV), a bare https://discord.gg link (app/roadmap/page.tsx:56) and 'Next . 7 Sep 2026 -- VR Meta Quest and PC VR' (data.ts:121) due tomorrow with every VR plugin disabled in Bag_Man.uproject and no VR work in any doc or tracker. Web CI has been red on main since 2026-08-25 but is actually shipping: the 2026-09-04 run (gh run 33928400546) logs a valid Account API Token, 'Uploaded ironics-web (4.77 sec)', then 'Authentication error [code: 10000]' on the zone Workers-Routes read -- a missing zone permission (apps/web/LEGAL_OPEN_ITEMS.md L4, lines 67-95), NOT the 'expired token' recorded in memory (portal note line 15) and D2 decision 13 -- so CI and the operator's local wrangler OAuth deploys now race on the same Worker. Nothing was edited, deployed or committed by this brief; every claim below cites a path or commit.

## 2. Document inventory (30)

| Path | What | Status | Last touched | Lane |
|---|---|---|---|---|
| `C:\Dev\Bag_Man\Docs\BUILD_PRUNE_DEPLOY_PLAN.md (s11 L232-258, s12 L260-271, s13 L273-286)` | Operator-approved (2026-09-01) plan of record: s11 CloudFront+OAC+signed-URL download-manager DESIGN; s12 'Admin systems + website final pass' SCOPE (release admin / hardened account admin with roles / live-ops / quality gate) -- the ONLY written admin-dashboard scope; s13 remaining sequence with s12 as item 6 | s12 DOCUMENTED-PLANNED (unstarted); s11 IMPLEMENTED-with-deviation (presigned S3, not CloudFront); s13 item 5 text SIDETRACKED/STALE (download page is live) | 5dbfd474 2026-09-01 (v2 addendum); 9ff73c3d / 61e9ecce 2026-09-04 | admin |
| `C:\Dev\Bag_Man\Docs\IRONICS_D1_DISTRIBUTION_PLAN.md` | D1: auto-approval on /beta/apply + APPROVED-gated /portal download via S3 presigned URL (deliberate s11 deviation); original proposal kept below; s7 defers BPD s12 'until D1 proves' | SHIPPED-LIVE (operator-proven 0.1.0 -> 0.1.3); header 'awaiting operator deploy' (L3) and 'no admin route is deployed' (L13) STALE; D1.4 mockup gate (L92-95) skipped; semver ruling (L56-62) never given | 3d2efd29 2026-09-04 (proposal ad035567 same day) | distribution |
| `C:\Dev\Bag_Man\Docs\dist\D2_DOWNLOAD_DISTRIBUTION_SCOPE.md` | D2 PROPOSAL: Tier 0 CloudFront+OAC+key-group 24 h signed URLs + publish sidecars + DownloadCard copy; Tier 1 dl.ironics.org, logs->Athena telemetry, mint quota, WAF, CloudFront Pro, package cull; Tier 2 launcher/delta/per-venue chunks; Tier 3 R2 conditional; 14 operator decisions (L103-118); adversarial fixes folded in (L120-165) incl. metadata-only endpoint, per-link denylist, cost alarms | DOCUMENTED-PLANNED (L3: nothing implemented); Tier 1B 'published as 0.1.3' (L54,L57) already STALE (0.1.3 shipped without the cull); decision 13 'expired token' cause is WRONG (see contradictions) | 77dd8443 2026-09-06 | distribution |
| `C:\Dev\Bag_Man\Docs\design\IRONICS_VOLTS_PURCHASE_ADMIN_SCOPE.md` | Cash-App Volts purchase + admin console scope: claim codes IRN-XXXXXX, idempotent HMAC mint, PlayFabId on portal account, auth models A/B/C (recommends 'C now, A later' L73), minimum console surface s1.4, three owed rulings (L193-200) | IMPLEMENTED the same day it was written (console + 8 routes, Ironics-Platform 4da6a25..975ef7a 2026-08-24) but doc SIDETRACKED/STALE: s1.2 'no admin HTTP surface' false, auth-model ruling unrecorded, owed items 2-3 unruled | 2537bd67 2026-08-24 | admin |
| `C:\Dev\Ironics-Platform\design\IRONICS_WEB_DESIGN_HANDOFF.md (v3.2)` | Website design brief: identity ('player character creator'), locked decisions s2 (Beta Lands, VR by 2026-09-07, Founder 100/300/1000), tokens s3, 11 routes s5, live-strip rule s7, perf gates s14, funnel names s15 | IMPLEMENTED (all 11 routes live); header STALE (parent IRONICS_GTM_PLATFORM_ROADMAP.md absent from disk, repo 'Instawerx/Ironics_Web', domain 'ironics.com', 'React Three Fiber'); s3 tokens describe the v1 design, site ships v2 Glass Black; s14 gate demoted | 784444e 2026-09-02 (s2 amended) | website |
| `C:\Dev\Ironics-Platform\design\project\IRONICS Site v2 Cinematic.dc.html + IRONICS Site.dc.html + chats\chat1.md + CLAUDE-DESIGN-HANDOFF.md` | Claude Design canvases (v1 corporate blue, v2 Glass Black = implemented) and the design conversation with operator rulings (chat1.md:499, :621, :707) | IMPLEMENTED; canvas SIDETRACKED/STALE vs code (roadmap phases differ); NO artboards exist for /signin, /auth/verify, /admin, DownloadCard, changelog, post-launch home | 6864c0d 2026-08-16 (never edited since) | website |
| `C:\Dev\Ironics-Platform\apps\web\app\roadmap\page.tsx + apps\web\lib\data.ts PHASES (L116-124) + lib\stats.ts` | THE public roadmap: static 7-row PHASES array (DONE x3 / ACTIVE Windows beta / NEXT VR 7 Sep 2026 / NEXT Q4 consoles / PLANNED 2027 launch); header 'REV {FALLBACK_BUILD_REV}'; 'Join Discord' -> https://discord.gg | SHIPPED-LIVE but SIDETRACKED/STALE content: REV 0.8.14 vs 0.1.3-beta; VR date tomorrow with no work; 'crash telemetry'/'invites' untrue; no 0.1.x release rows; placeholder Discord | 784444e 2026-09-02 | roadmap |
| `C:\Dev\Ironics-Platform\apps\web\app\admin\{layout,page}.tsx + components\AdminConsole.tsx + lib\admin.ts + lib\admin-gate.ts` | The existing admin console: /admin 'Payments' -- Pending claim codes (Approve = mint), Unattributed payments (Resolve by code), Audit (per-day, 200 rows); 404 unless GET /v1/admin/me passes; calls exactly 5 routes (AdminConsole.tsx:26-28,46,62) | IMPLEMENTED on main since 975ef7a; deployed by the web deploys since; LIVE-REACHABILITY UNKNOWN (allowlist seeding unrecorded); no mockup ever existed | 975ef7a / d8945c8 2026-08-24 | admin |
| `C:\Dev\Ironics-Platform\apps\api\lib\api.ts (route table L66-248) + src\handlers\admin\*.ts + src\lib\admin-guard.ts + src\lib\admin.ts + scripts\seed-first-admin.ts` | 21 deployed Lambda routes incl. 8 /v1/admin/* (me, claims, claims/approve, audit, economy/register, payments/record, payments/resolve, payments); allowlist guard = per-request read of IronicsAdmins (no JWT role); approveAccount/revokeAccount/listApplicationsForApproval library-only; one-shot seed script that refuses twice | SHIPPED-LIVE (deploy-api.yml green on every main push since 08-25; stack UPDATE_COMPLETE 2026-09-04 per memory); seed execution UNRECORDED | b41313a 2026-09-04; admin routes 4da6a25..1ae2cf7 2026-08-24/25 | admin |
| `C:\Dev\Ironics-Platform\apps\api\src\handlers\beta\apply.ts + scripts\backfill-approve-applicants.ts` | POST /v1/beta/apply auto-approves every age-verified applicant in-request (apply.ts:92-131, operator ruling 2026-09-04 hardcoded); bulk backfill script for stranded APPLICANTs (run once 09-04 -> operator = Founder #1) | SHIPPED-LIVE | ba3f7e3 / b41313a 2026-09-04 | admin |
| `C:\Dev\Ironics-Platform\apps\api\src\handlers\download\latest.ts + apps\web\components\DownloadCard.tsx + apps\api\scripts\publish-release.ps1` | APPROVED-gated mint of a 900 s role-signed S3 presigned URL from latest.json; DownloadCard fetches on mount AND click; PowerShell publish (provenance-gated, immutable keys, BOM-less latest.json; no sidecars, no promote/rollback, no release notes) | SHIPPED-LIVE (0.1.0 -> 0.1.3 published through it); latent defect documented in D2 (resume after 15 min -> 403; mount fetch inflates download_started) | b41313a 2026-09-04 | distribution |
| `C:\Dev\Ironics-Platform\apps\api\src\handlers\stats.ts + apps\web\components\LiveStrip.tsx + lib\stats.ts + FounderLadder.tsx` | Public /v1/stats (founder counter, per-tier remaining from FOUNDER_CAP=1000) rendered by the live strip and /founders; getMockStats fallback when NEXT_PUBLIC_STATS_URL unset | SHIPPED-LIVE and truthful today (issued 1); 'testers' counter is read (stats.ts:37) but has NO writer in apps/api/src; BUILD_REV never set in api.ts commonEnv so buildRev always omitted | 784444e 2026-09-02; handler 2026-08-16 | website |
| `C:\Dev\Ironics-Platform\apps\web\app\{page,build,houses,market,founders,beta,press,creators}\page.tsx + components\{BetaComplete,PortalStatus}.tsx + lib\data.ts copy blocks` | Public site copy: home creator pitch; /founders tier promises ('day one', 'cohort two week two', 'as server capacity allows'); /beta 'Cohorts go out weekly ... approved, waitlisted'; portal 'Cohort assignment follows review'; PLATFORMS lists Quest/PC VR/PS5/Xbox/iOS/Android; /market 12 fixture items + hardcoded 4,200 V wallet; /press 'Download kit' href='#'; /creators 'Apply as creator' href='#' | SHIPPED-LIVE; copy SIDETRACKED/STALE vs auto-approval (apply.ts:92-114) and vs the shipped client; three dead CTAs | 6864c0d 2026-08-16; data.ts 784444e 2026-09-02 | website |
| `C:\Dev\Ironics-Platform\apps\web\LEGAL_OPEN_ITEMS.md + apps\web\lib\legal.ts` | Legal tracker L1 (Virtual Currency terms contradict staking -> draft, counsel), L2 (docs never name Volts/Watts), L3 (no subscription/auto-renew terms -> blocks League), L4 (CI token lacks Zone->Workers Routes read), L5 (Lighthouse BP pin); 4 of 5 docs inForce, entity C12 AI Gaming | IN-PROGRESS (IMPLEMENTED docs; open items valid); no review of whether Privacy/Community Guidelines cover the now-shipped text chat/DM and cooked-in voice | 863a690 / 311ae4d 2026-08-23/24 | website |
| `C:\Dev\Ironics-Platform\.github\workflows\deploy-web.yml + deploy-api.yml + verify-cloudflare.yml + probe-cloudflare-tokens.yml + apps\web\wrangler.jsonc` | Web CI (lint/types/tests/cf:build -> JS budget hard gate -> Lighthouse advisory -> wrangler deploy with NEXT_PUBLIC_STATS_URL + NEXT_PUBLIC_EPIC_ENABLED baked); API CI via OIDC cdk deploy | API SHIPPED-LIVE (green); web SIDETRACKED: every main run red since 2026-08-25 (gh runs 32817362619, 32817817021, 33584640605, 33928400546) yet uploads succeed -- red-but-shipping; two deploy paths in use | 647853d 2026-08-24 (Lighthouse advisory); wrangler.jsonc 3c33d9e 2026-08-16 | website |
| `C:\Dev\Ironics-Platform\README.md + apps\web\README.md + apps\web\DEPLOYMENT.md + apps\api\README.md + apps\api\AWS-SETUP.md + apps\crm\README.md` | Repo documentation: root 'Open items' (AWS not linked, secrets empty, CF deploy never run), web 'Known stubs', api 'Eight routes ... nothing deployed ... W5 console remains', AWS-SETUP 'State as of 2026-08-16' (secrets empty, Lambda concurrency 10), crm 'reserved -- do not add the back office to apps/web' | SIDETRACKED/STALE across the board (all 2026-08-16); apps/crm never started and its guidance was overridden by building /admin inside apps/web | ad1cd1c / 1fdcc4d 2026-08-16 | other |
| `C:\Dev\Ironics-Platform\apps\api\src\lib\events.ts + apps\web\lib\analytics*.ts + app\api\events\route.ts + components\Analytics.tsx` | s15 funnel plumbing: server emit() = CloudWatch stdout JSON; client POST /api/events collector; client track() no-op unless NEXT_PUBLIC_ANALYTICS_ENABLED (unset in deploy-web.yml) | IMPLEMENTED but inert: no warehouse, no query, client off in prod, install_completed / first_match_completed never emitted, download_started fires on portal page view | 2026-08-16 era | admin |
| `C:\Dev\Bag_Man\releases\README.md + releases\win64\0.1.0-beta\manifest.json + .zip.sha256` | Release-ledger convention (every release commits manifest + sha256; '0.1.0-beta is a PLACEHOLDER pending the operator's real release semver' L9-13) | SIDETRACKED/STALE: only 0.1.0-beta committed (git log -- releases/ = 9ff73c3d alone) while 0.1.1, 0.1.2, 0.1.2.1, 0.1.3-beta shipped; semver placeholder became the de-facto scheme | 9ff73c3d 2026-09-04 | distribution |
| `C:\Dev\Bag_Man\Docs\LIVE_TRACKER.html` | Tier-3 status board, ~130 rows; grep for website/portal/admin/D1/D2 finds only hub-district 'D1/D2' and the prune row's remains text (L169, L177, L821) | SIDETRACKED/STALE: no rows for anything after 2026-09-03 (D1 live, 0.1.x releases, playtest batches, S12 EC2 server, D2); rows 036/027/108/000 contradict later state; the whole Ironics-Platform lane is untracked | 724594de 2026-09-03 (Track C DONE mark 9ff73c3d 09-04) | other |
| `C:\Dev\Bag_Man\Docs\design\IRONICS_KILL_TELEMETRY_SCOPE.md + C:\Dev\Bag_Man_Backend\lambda\kill-telemetry\index.ts` | Kill telemetry ruled ADMIN-ONLY read (2026-08-10): POST /kills ingest + GET /kills/top HMAC-signed admin read; intended consumer 'Top 10 this week' / tournament standings | IMPLEMENTED API-only (Bag_Man_Backend); DOCUMENTED-PLANNED admin surface (no UI, no consumer); team-kill predicate unruled | 089fdd66 2026-08-10 | admin |
| `C:\Dev\Bag_Man\Docs\COMMS_VOICE_AND_SOCIAL_BUILD_PLAN.md + COMMS_PIE_PROOF_RUNBOOK.md + memory project_comms_programme_plan_of_record.md + project_comms2_chat_ui_mockup.md + project_ironics_voice_aaa_scope.md` | COMMS programme AFL-3400 (operator-issued 2026-09-03, R1-R10 FINAL): text spine COMMS-1 committed; COMMS-2 C++ chat panel built, 4-artboard mockup delivered AWAITING approval; 4B/5B backend PROVEN LIVE; voice V0-V7 with V0 operator-only ([EOSVoiceChat] secret + Epic portal) | IN-PROGRESS; PIE proofs owed; voice INERT at runtime despite 'voice ON' cooked in 0.1.x; SIDETRACKED by prune/deploy arc then installer | f474573d / 926ed316 2026-09-03 | comms |
| `C:\Dev\Bag_Man\Docs\S12_GAMELIFT_INTEGRATION_PLAN.md + RUNBOOK_GAMELIFT_COMPUTE.md + SPRINT_PLAN_S12E_TO_FRONTEND.md + memory project_s12_gamelift_server_live.md` | GameLift plan/runbook/sprint (2026-08-08/09) vs the live 24/7 EC2 Anywhere server (i-0b23133a70f6a55b2, 32.193.28.175, Scheduled Task) proven 2026-09-05 | Server SHIPPED-LIVE; docs SIDETRACKED/STALE ('DRAFT, not started'; LAN compute listed as current) -- misled an agent on 2026-09-05; S12-E steps 4-6 + Sprint-3 hardening unclosed | docs ef180290/da075667 2026-08-09; server 2026-09-05 | admin |
| `C:\Dev\Bag_Man_Backend\docs\AWS_ARCHITECTURE.md + lambda\reconcile\index.ts + lambda\create-ticket\index.ts + lambda\queue-registry\registry.ts + scripts\*` | Tentpole estate doc ('one HTTP API, 14 Lambdas', self-declared must-stay-true) + the HMAC-signed economy/entitlement endpoints, reconcile Lambda (in-doubt rows counted in logs only), SSM FlexMatch routing flag, FLEX_COMMIT_DEADLINE_SECONDS=300, PlayFab seed/verify scripts | SHIPPED-LIVE code; doc SIDETRACKED/STALE (36 lambda dirs, ~28 routes now); all operator levers are script/console-only | doc 2026-08-08; scripts 8a90ea5 2026-09-03 | admin |
| `C:\Dev\Bag_Man\Docs\ssot\economy-store.md (R71 L902) + Docs\design\PX_STORE_BUILD_RULINGS.md (D-1 L22) + Docs\design\IRONICS_PRICING_SSOT.md (L119-121) + Docs\Hub\LOBBY_UPGRADE_DOC.md (L50)` | Game-side rulings that bind the website: R71 'the store sells on BOTH surfaces' (web checkout, one catalog/pricing/mint source, console-cert read first); D-1 '$5/mo is the WEBSITE price of the Volts'; Pricing SSOT 'Founders ... no backend representation'; operator: 'We never even integrated web store and matchmaking UI' | DOCUMENTED-PLANNED obligations with no owner doc on the website side; Founders ruling CONTRADICTED by the live portal (founder numbers + entitlement) | 7f1a05eb 2026-08-24; 7dc82fcc 2026-08-31; 6c9b09f9 2026-08-27; 1215c302 2026-08-30 | website |
| `C:\Dev\Bag_Man\Docs\Hub\IRONICS_CC_DESIGN_BRIEF.md s0 + Docs\ssot\ui-frontend.md s12 (L642-645) + memory feedback_mockup_first_approval_workflow.md` | Brand lock tokens (ground #222A3A, accent #1E5AFF, Orbitron/NotoSans/DroidSansMono, no Apple-Glass/cyan) and the rule that game + web share exactly one thing -- the design tokens; mockup-first doctrine (ruled 2026-08-27, CLAUDE.md doctrine 7) | IMPLEMENTED in-game; the shared-token emission to CSS was never built; mockup-first NOT applied to any website surface built after 08-16 | bf4f0420 / 7825a509 2026-08-28/29; ui-frontend 5ebebac9 2026-08-07 | website |
| `C:\Dev\Bag_Man\Docs\IRONICS_CC_ROADMAP.html + Docs\Hub\IRONICS_LOBBY_HUB_ROADMAP.html + Docs\BAG_MAN_MASTER_BUILD_v2.0.md` | The only 'roadmap' files in the game repo: creator programme (stale, superseded by CC_INTEGRATION_PLAN I-26), lobby-hub H0-H6 programme (in-progress), and the pre-reset 50-sprint master build (stale). CC-X6 'ironics.org accuracy -- site says 18 builds, disk 28' still OPEN | Game-side engineering roadmaps; NONE is the public roadmap; no pipeline links tracker state -> /roadmap | 3ff8cf95 2026-08-21; 443ad897 2026-08-27; c0a78eb7 2026-06-11 | roadmap |
| `C:\Dev\Bag_Man\Docs\reference\IRONICS_BETA_LAUNCH_ASSETS_ON_HAND.md (L17-18)` | Procurement bank; records that 'the Beta-launch master-plan DRAFT does not yet exist as its own doc on disk' | SIDETRACKED/STALE (packs partly git-rm'd in Track B); the beta-program plan still does not exist anywhere on disk | 1c072a49 2026-08-05 (content 2026-07-01) | roadmap |
| `C:\Dev\Bag_Man\Docs\LIVE_TRACKER.html row 'Matchplay promo reels' (L808-812) + memory reference_cine_capture_pipeline.md + Docs\design\handovers\VENUE_KEY_ART.md` | 8 operator-approved 60 s matchplay reels + 4K start loop (a7d85419 2026-09-01) delivered from a session scratchpad; ARCANEON venue key art wired in-game; NANOWATT blocked; INFINEON rejected | IMPLEMENTED assets, SIDETRACKED: no committed/documented storage location, not on the website, no social/marketing plan doc in either repo | a7d85419 2026-09-01; 9cbe741d 2026-08-11 | website |
| `C:\Dev\Bag_Man\Docs\ENGINE_DOCTRINE.md (s3 L47-49) + Tools\verify_ship_provenance.ps1:7,89 + Docs\DOCTRINE.md (B2 L118, G9 L109) + Docs\reference\cook-manifests\README.md (L7)` | Doctrine/gate wording: 'MANDATORY before any R2 upload' (R2 never provisioned; releases go to S3); DOCTRINE B2 still says build on C: (retired 2026-09-03); cook baseline README names 08-10 while fe246c68 banked the 09-02 baseline | SIDETRACKED/STALE wording on live doctrine files | f474573d 2026-09-03; 79c116d1 2026-08-12; fe246c68 2026-09-02 | distribution |
| `C:\Users\tabor\.claude\projects\C--Dev-Bag-Man\memory\project_ironics_portal_download_live_state.md + project_prune_build_deploy_state.md + project_ironics_first_client_playtest_findings.md` | Agent memory for the platform/deploy lane: D1 live + 0.1.x ledger + deploy LAW (build-time NEXT_PUBLIC_* flags) + operator pivot mandate; REMAIN list with s12 as item 6; playtest Batches A-D (D Option B backend 30 s still open now that S12 is live) | IMPLEMENTED record; 'expired CLOUDFLARE_API_TOKEN' claim (portal note L15) is WRONG (CI log shows auth OK, upload OK, zone-routes permission missing); memory has no note on the admin console, founder tiers, beta program or website roadmap | 2026-09-06 23:54Z; 2026-09-04; 2026-09-05 | other |

## 3. State of every plan (19)

### BPD s11 -- institutional download manager (CloudFront + OAC + signed URLs + logs->Athena + WAF)
_Source:_ C:\Dev\Bag_Man\Docs\BUILD_PRUNE_DEPLOY_PLAN.md L232-258  
**Still valid?** Yes -- D2 Tier 0/1 is literally s11 executed; latest.ts:35-37 and ironics-portal-stack.ts:304-305 both say 'CloudFront + OAC layered later with no client change'  
**Sidetracked by:** D1's 'simplest live path' choice on 2026-09-04 to get the first tester download out; the 0.1.2 delivery-class defect (resume after 15 min -> 403) is the cost of that deviation and is why D2 exists

**Done:**
- [x] Step 0 provenance gate (Tools/verify_ship_provenance.ps1, UE5-CL-0 proven on every 0.1.x publish)
- [x] Step 1 immutable release layout + latest.json single pointer (publish-release.ps1; s3://ironics-releases/win64/<ver>/)
- [x] Private bucket + APPROVED-gated mint Lambda + portal download page (as D1, presigned S3)

**Undone:**
- [ ] CloudFront + OAC + key-pair signed URLs (D1 deviated on purpose, IRONICS_D1_DISTRIBUTION_PLAN.md L20-22)
- [ ] CloudFront standard logs -> Athena per-release counters
- [ ] WAF rate limit on the mint endpoint
- [ ] Release notes on the download page
- [ ] manifest.json / .sha256 sidecars beside the zip (exist locally, never uploaded -- D2 L31)

### BPD s12 -- Admin systems + website final pass (operator addition 3)
_Source:_ C:\Dev\Bag_Man\Docs\BUILD_PRUNE_DEPLOY_PLAN.md L260-271; s13 item 6 L286; memory project_prune_build_deploy_state.md REMAIN (6)  
**Still valid?** Yes as a headline; it is four bullets with no acceptance criteria, mockups, task breakdown, data sources (metrics need D2 1A.2), or auth-model ruling -- and it assumes a web console (Model A) while the VOLTS scope recommended Model C first  
**Sidetracked by:** Sequenced 'after Track C proves' (closed 2026-09-04) and then displaced by D1 -> playtest batches -> S12 server -> 0.1.1/0.1.2/0.1.2.1/0.1.3 fix-patches + bsdtar incident -> D2 proposal (Bag_Man commits 9ff73c3d..77dd8443)

**Done:**

**Undone:**
- [ ] Release admin: publish/promote/rollback (writes latest.json), release-notes editor, download metrics panel
- [ ] Player/account admin hardened: audit on every mutation, roles viewer/operator/owner, session timeout, IP allowlist
- [ ] Live-ops: PlayFab entitlement lookup/grant/revoke, GameLift session inspection, wallet adjustments with dual-entry audit
- [ ] Quality gate: Lighthouse/a11y pass, Epic sign-in re-test, legal current, download page copy + real IRONICS art

### BPD s13 -- amended remaining sequence
_Source:_ C:\Dev\Bag_Man\Docs\BUILD_PRUNE_DEPLOY_PLAN.md L273-286  
**Still valid?** Yes; item 5 wording is stale and item 6 is the operator's current mandate  
**Sidetracked by:** The 0.1.x fix-patch cadence and the packaging incident; item 5's text still says the download stack is pending

**Done:**
- [x] 1 Track C cook (client+server, provenance-clean)
- [x] 4 First Shipping server build
- [x] 5 s11 steps 0-1 + the S3/Lambda mint + website download page (via D1)

**Undone:**
- [ ] 1 manifest-diff lint vs cook_20260810g + new-package attribution
- [ ] 2 operator one-engine PIE lap (AIK)
- [ ] 3 ARCANEON texture-budget pass (A3 revised)
- [ ] 4 full COOKED live-flow WATCHED gate (EOS sign-in -> hub/store lap -> every roster map -> GameLift match w/ PlayFab persistence) -- 0.1.x playtests covered parts, never the whole lap
- [ ] 5 real release semver ruling (0.1.x-beta shipped as the de-facto scheme)
- [ ] 6 Admin/website final pass (s12)

### D1 distribution plan (signed archive + website download)
_Source:_ C:\Dev\Bag_Man\Docs\IRONICS_D1_DISTRIBUTION_PLAN.md  
**Still valid?** Delivered; the doc is stale by four releases and its own s7 deferral of BPD s12 has expired  
**Sidetracked by:** n/a -- D1 WAS the sidetrack; it shipped in one day and then absorbed four fix-patches

**Done:**
- [x] Auto-approval on /beta/apply (ba3f7e3)
- [x] APPROVED-gated GET /v1/download/latest + private ironics-releases bucket (ffb9829)
- [x] Portal DownloadCard with SmartScreen guidance + sha256 (ffb9829)
- [x] publish-release.ps1 provenance-gated immutable upload (ad00f69, f46764a)
- [x] Backfill of the stranded operator account -> Founder #1 (b41313a)
- [x] First authentic test + 0.1.0 -> 0.1.3-beta operator-proven downloads (memory)

**Undone:**
- [ ] s2 release-semver ruling (L56-62) -- never given
- [ ] D1.4 download-page 1:1 mockup + operator approval before build (L92-95) -- skipped, no record of a waiver
- [ ] Status header update (L3 'awaiting operator deploy'; L13 'no admin route is deployed')

### D2 client-distribution scope (Tier 0 CloudFront signing -> Tier 1 hardening/telemetry/cull -> Tier 2 launcher/delta/chunks -> Tier 3 R2 conditional)
_Source:_ C:\Dev\Bag_Man\Docs\dist\D2_DOWNLOAD_DISTRIBUTION_SCOPE.md (77dd8443)  
**Still valid?** Yes as a proposal; it needs re-versioning (next cull build != 0.1.3), the 'website-zip only for now' ruling (BPD L183-184) re-opened before Tier 2, and decision 13's cause corrected (permission, not expiry)  
**Sidetracked by:** Nothing yet -- proposed on 2026-09-06, the same day the operator pivoted; note Tier 1B already says 'published as 0.1.3' (L54, L57) while 0.1.3-beta shipped that day WITHOUT the cull

**Done:**

**Undone:**
- [ ] All tiers (L3: nothing implemented, deployed or provisioned)
- [ ] 14 operator decisions (L103-118)
- [ ] Adversarial fixes that are part of the proposal (L120-165): metadata-only endpoint, per-link rid denylist + cost alarms, WAF-before-Pro-plan, bCompileCEF3 instead of UDS edit, explicit MetaHuman/NNEDenoiser disables, two-armed 2A spike, matchmaking join-path ruling for per-venue chunks, log-bucket-as-credential-store hygiene

### Volts purchase + admin console scope (Cash App claim codes, HMAC mint, PlayFabId on account, auth model A/B/C)
_Source:_ C:\Dev\Bag_Man\Docs\design\IRONICS_VOLTS_PURCHASE_ADMIN_SCOPE.md (2537bd67 2026-08-24)  
**Still valid?** Design yes; the doc's premise (no admin surface, C-first) is superseded by what shipped; the three owed rulings are still owed and block the money surface  
**Sidetracked by:** Built the same day it was scoped, then the lane moved to Epic sign-in/legal (08-25) and the deploy arc; never revisited

**Done:**
- [x] s1.4 minimum console surface (Pending / Unattributed / Audit) -- Ironics-Platform 975ef7a
- [x] Claim-code + unattributed queue + refusal codes + admin allowlist table/guard/seed -- a0968ba..4da6a25, ee06f45, deda479
- [x] PlayFabId LOOKUP at Epic link time, never create -- b9ca521/91931a3
- [x] Economy registration route -- 1ae2cf7
- [x] Checkout route /v1/checkout/volts (api.ts:171) -- backend only

**Undone:**
- [ ] Auth-model ruling recorded (what shipped is a per-request allowlist table: not A's JWT role, not B's MFA identity, not C's CLI)
- [ ] Owed item 2: Volts-per-dollar bonus tiers (L197-198) -- unruled; VOLT_PACKS is a code table
- [ ] Owed item 3: refuse Epic-unlinked at checkout vs block at approval (L199-200) -- unruled
- [ ] A real payment through the flow ('no money moved' L3 -- nothing later says otherwise)
- [ ] Any web page that calls /v1/checkout/volts; /market has no buy action
- [ ] Doc update: s1.2 'no admin HTTP surface' is false

### Website design brief v3.2 + Claude Design v2 Cinematic canvas (the site plan)
_Source:_ C:\Dev\Ironics-Platform\design\IRONICS_WEB_DESIGN_HANDOFF.md; design\project\IRONICS Site v2 Cinematic.dc.html; design\chats\chat1.md  
**Still valid?** s1-s2 identity/decisions yes (with the VR date now a liability); s3 tokens no; s14 partly  
**Sidetracked by:** Site shipped 2026-08-16; the design bundle was never re-opened; every later surface (auth, admin, download) was built without a mockup despite the 2026-08-27 mockup-first ruling

**Done:**
- [x] All 11 s5 routes implemented + /signin, /auth/verify, /portal auth, /admin (code-first)
- [x] v2 Glass Black implemented per chat1.md:621 ruling
- [x] s2 amendments (creator complete, Beta Lands, VR by 09-07) applied to copy (784444e)
- [x] s15 funnel names typed (events.ts, analytics.ts)
- [x] Epic federated sign-in live (brand review cleared 2026-08-25; EAS PROVEN 2026-09-02)

**Undone:**
- [ ] Header/s3 re-cut: repo/domain/stack/parent-doc corrections; tokens re-documented for v2 Glass Black (web README:24-37 is the only record)
- [ ] Artboards for /signin, /auth/verify, /admin, DownloadCard, changelog/releases, VR/console pages, post-launch home
- [ ] s7 live REV (BUILD_REV never wired)
- [ ] s14 Lighthouse merge gate (demoted 647853d; needs PR previews + re-baseline)
- [ ] s5 press kit archive and sponsor mark guideline files (none exist)
- [ ] s16 entity: settled in code (legal.ts:8) but crm README still says open

### Legal open items L1-L5
_Source:_ C:\Dev\Ironics-Platform\apps\web\LEGAL_OPEN_ITEMS.md; lib\legal.ts  
**Still valid?** Yes  
**Sidetracked by:** Counsel-owned items have no recorded owner/date; L4 was re-diagnosed as 'expired' in memory on 09-04 and never acted on

**Done:**
- [x] Terms, Privacy, Beta Agreement, Community Guidelines in force (legal.ts:64,156,296,353)
- [x] Entity C12 AI Gaming (legal.ts:8)
- [x] L4 diagnosed correctly on 2026-08-24 (token lacks Zone->Workers Routes)

**Undone:**
- [ ] L1 Virtual Currency Terms vs live staking loop (counsel) -- blocks contract-clean Volts sales
- [ ] L2 docs never name Volts/Watts
- [ ] L3 subscription/auto-renew terms before any League charge
- [ ] L4 token permission edit (operator, Cloudflare dashboard)
- [ ] Unlogged: whether Privacy/Community Guidelines cover text chat/DM (shipped 0.1.1+) and voice (cooked ON, runtime inert)

### apps/crm reserved back-office workspace
_Source:_ C:\Dev\Ironics-Platform\apps\crm\README.md (2026-08-16)  
**Still valid?** Open question -- worst-route JS is 118 KB vs the 180 KB gate (DEPLOYMENT.md:156), so growing the console in apps/web will press on the gate the README warned about  
**Sidetracked by:** The 2026-08-24 console was built where the auth cookie and gate already lived

**Done:**

**Undone:**
- [ ] Everything; the seams it lists (apply persistence, stats source, portal fixture) were absorbed into apps/api instead, and the admin console landed in apps/web/app/admin against the README's 'Do not add it to apps/web' (L22-23)

### s15 funnel analytics + s14 Lighthouse gate + PR previews
_Source:_ design handoff s14/s15; apps\web\DEPLOYMENT.md:229-232; lighthouserc.json; .github\workflows\deploy-web.yml:89-106  
**Still valid?** Yes; there is no funnel data to put on any dashboard today  
**Sidetracked by:** Lighthouse demoted 2026-08-24 after blocking a security fix for a week; analytics deferred pending a warehouse decision that was never scheduled

**Done:**
- [x] Event names typed; server emit to CloudWatch; client collector; GPC/DNT honoured
- [x] JS budget hard gate (180 KB gz) still enforced

**Undone:**
- [ ] Warehouse choice; NEXT_PUBLIC_ANALYTICS_ENABLED in prod
- [ ] install_completed / first_match_completed emission (nothing in the game client calls the portal)
- [ ] Honest download_started (metadata-only endpoint, D2 fix)
- [ ] PR preview deployments (wrangler versions upload) -> Lighthouse re-baseline -> restore s14 as a gate

### Release ledger convention (commit manifest + sha256 per release; real semver)
_Source:_ C:\Dev\Bag_Man\releases\README.md L1-13  
**Still valid?** Yes; D2 0.4 also wants the sidecars uploaded beside the zip  
**Sidetracked by:** Four releases in 48 h

**Done:**
- [x] 0.1.0-beta record (9ff73c3d)

**Undone:**
- [ ] 0.1.1-beta, 0.1.2-beta, 0.1.2.1-beta, 0.1.3-beta records (sidecars exist at D:\BagMan\releases\win64\<ver>\ per D2 L31,L178)
- [ ] Semver ruling ('placeholder' never resolved)

### Kill telemetry -- admin-only read (tournament standings, 'Top 10 this week')
_Source:_ C:\Dev\Bag_Man\Docs\design\IRONICS_KILL_TELEMETRY_SCOPE.md; Bag_Man_Backend\lambda\kill-telemetry\index.ts  
**Still valid?** Yes -- a natural row for a live-ops admin surface  
**Sidetracked by:** Never scheduled after 08-10

**Done:**
- [x] Both rulings (2026-08-10)
- [x] POST /kills ingest + GET /kills/top HMAC-signed read exist in Bag_Man_Backend

**Undone:**
- [ ] Any consumer/UI (the 'admin highlight' surface)
- [ ] Team-kill predicate ruling (L97-103)
- [ ] Deploy state of the two routes not recorded in AWS_ARCHITECTURE.md

### COMMS programme AFL-3400 (text chat, DMs, voice V0-V7, social) -- carry-over
_Source:_ memory project_comms_programme_plan_of_record.md; Docs\COMMS_VOICE_AND_SOCIAL_BUILD_PLAN.md; Docs\COMMS_PIE_PROOF_RUNBOOK.md; memory project_comms2_chat_ui_mockup.md; project_ironics_voice_aaa_scope.md  
**Still valid?** Yes (operator-issued 2026-09-03); any website/roadmap claim of 'voice' would be false today  
**Sidetracked by:** Prune/deploy arc (09-01..09-04) then the installer/fix-patch cycle

**Done:**
- [x] COMMS-0; COMMS-1 spine committed+compiles; COMMS-2 pure-C++ chat panel + Enter binding shipped in 0.1.1 (Batch C d2ddba2e)
- [x] COMMS-4B /rtc/token + COMMS-5B DM WebSocket/inbox PROVEN LIVE (Bag_Man_Backend tag comms-4b-5b-proven)
- [x] COMMS-2 4-artboard mockup delivered (artifact 049d14a3...)

**Undone:**
- [ ] COMMS-1/2 PIE proof (1 dedicated + 3 clients, P1-P6/E1-E3) -- operator session
- [ ] COMMS-2 mockup ratification + WBP_AFL_* skin pass
- [ ] V0 provider bring-up (operator-only: [EOSVoiceChat] secret + Epic portal Voice/Lobby/P2P) -- voice is INERT at runtime though 'voice ON' is cooked
- [ ] V1-V7 client core proof, server rooms (now unblocked by S12), PTT/VAD, voice UI (mockup-first), party, moderation, console-cert
- [ ] Friends/presence/block/reports/sanctions (COMMS-5/6)
- [ ] Step 0 tracker sync 0b/0c/0d

### First-playtest fix batches A-D + Q4
_Source:_ memory project_ironics_first_client_playtest_findings.md; commits 61e9ecce, 5c85b5d7, d2ddba2e, 0f23e2f3  
**Still valid?** Yes  
**Sidetracked by:** 0.1.2/0.1.3 respawn root-cause work and the packaging incident

**Done:**
- [x] A (login-modal suppress, IRONICS.exe), B (wallet in System Menu + ESC hint), C (Enter opens chat) -- shipped 0.1.1
- [x] D Option A: client-side 30 s -> offline bot match (0.1.1); universal offline fill Fix 2 (0.1.2)

**Undone:**
- [ ] D Option B: backend FlexMatch 30 s (versioned rule-set name + FLEX_COMMIT_DEADLINE_SECONDS=30 at Bag_Man_Backend/lambda/queue-registry/registry.ts:178 + setup:flexmatch --apply) -- unblocked by S12, not done
- [ ] Q4 Map_3v3 queue has no authored experience (route 3v3 -> Arena)
- [ ] Real 2-human matchmaking -> GameLift end-to-end with a live client (unwatched)
- [ ] Bag_Man/ folder (.uproject) rename deferred

### Track C sign-off residuals
_Source:_ C:\Dev\Bag_Man\Docs\BUILD_PRUNE_DEPLOY_PLAN.md L215-216, L275-282; LIVE_TRACKER row 000 remains  
**Still valid?** Yes  
**Sidetracked by:** Fix-patch cadence

**Done:**
- [x] Shipping client + server cooks, provenance-clean; 0.1.x published

**Undone:**
- [ ] Cooked live-flow WATCHED lap per s5 (whole loop in one session)
- [ ] Manifest-diff lint vs cook_20260810g (cook-manifests/README.md still names 08-10 as baseline; fe246c68 banked 09-02)
- [ ] ARCANEON texture-budget pass
- [ ] One-engine PIE lap with AIK

### LIVE_TRACKER sync + doc move pass (row 112 'NEXT' since 2026-08-05; AFL-3062 docx archive)
_Source:_ C:\Dev\Bag_Man\Docs\LIVE_TRACKER.html; Docs\Hub\IRONICS_LOBBY_HUB_TASKS.md L871-875  
**Still valid?** Yes -- the operator's SSOT does not know the platform lane exists  
**Sidetracked by:** Open since 2026-08-05; each arc since added rows for itself only

**Done:**
- [x] +ENGINE-CONSOLIDATION row (724594de); Track C DONE mark (9ff73c3d)

**Undone:**
- [ ] Rows for D1 live, 0.1.0..0.1.3, playtest batches, Fix 1/2/3, S12 EC2 server, D2 PROPOSED, COMMS-2 code state
- [ ] Corrections: rows 036 (EOS blocked vs PROVEN aa550a1d), 027 (LAN-only compute), 108 (plan awaiting approval), 000 ('R2 upload')
- [ ] Any row at all for the Ironics-Platform website/portal/admin lane

### Game-SSOT obligations on the website (R71 web checkout; D-1 website price; CC-X6 site accuracy; shared design tokens)
_Source:_ Docs\ssot\economy-store.md L902; Docs\design\PX_STORE_BUILD_RULINGS.md L22; Docs\IRONICS_CC_ROADMAP.html CC-X6; Docs\ssot\ui-frontend.md L642-645  
**Still valid?** Yes; gated by L1 legal and the Volts pricing rulings  
**Sidetracked by:** No owner doc on the website side by design (ui-frontend s12 scopes browser surfaces out of the game SSOTs)

**Done:**
- [x] In-game store surface PROVEN (tracker 103/105); /market page exists with fixture data

**Undone:**
- [ ] Web checkout page for /v1/checkout/volts; single catalog/pricing/mint source for both surfaces; console-cert read before the web path ships
- [ ] /market real catalog (990 V singles / 1490 V twins per tracker vs fixture prices, data.ts:52-67) and the hardcoded 4,200 V wallet pill
- [ ] CC-X6 identity-count accuracy (site: 18 House characters)
- [ ] Token emission to CSS from the UE compiler (AFLTokenCompiler emits UE only)

### Beta-launch master plan / beta program (target 1,000 testers, 36-concurrent peak; which doors ship Disabled; AFL-3063 first-beta walk-through)
_Source:_ Docs\reference\IRONICS_BETA_LAUNCH_ASSETS_ON_HAND.md L17-18; Docs\Hub\IRONICS_LOBBY_HUB_SSOT.md L166-167, L482; Docs\Hub\IRONICS_LOBBY_HUB_TASKS.md L878-882; LIVE_TRACKER row 081  
**Still valid?** Open question for the operator -- this is the missing parent of the public roadmap  
**Sidetracked by:** Never authored; the tracker and BPD carried the sequencing

**Done:**
- [x] The beta is technically open: auto-approval + download live; 1 member

**Undone:**
- [ ] The plan itself has never existed as a doc
- [ ] Tester acquisition, invites/creator codes (data.ts CREATOR_PERKS promises 'code drop' -- no backend), what 'testers' counts, staked BR held 'until enough testers'
- [ ] AFL-3063 operator first-beta walk-through on the dedicated fleet
- [ ] Founder in-game meaning (Pricing SSOT L119-121 says none exists)

### Marketing capture assets (8 approved matchplay reels + 4K start loop; venue key art)
_Source:_ LIVE_TRACKER row 'Matchplay promo reels' L808-812 (a7d85419); Docs\design\handovers\VENUE_KEY_ART.md; memory project_venue_keyart_state_per_venue.md  
**Still valid?** Yes -- the only real gameplay marketing material on hand  
**Sidetracked by:** Delivered the day the prune arc started

**Done:**
- [x] 8 reels + loop operator-approved 2026-09-01; ARCANEON key art wired in-game

**Undone:**
- [ ] Committed/documented storage location for the reels (delivered from a session scratchpad; D:\BagMan has no reels folder per lens 5)
- [ ] Publication (site /press has one static render; no social plan doc; Postiz skill available but no plan)
- [ ] NANOWATT art (blocked on greybox); INFINEON identity decision

## 4. Admin controls TODAY (26)

| Control | Surface today | Gap |
|---|---|---|
| Beta application -> approval | UI /beta -> POST /v1/beta/apply auto-approves every 13+ applicant in-request (apps/api/src/handlers/beta/apply.ts:92-131; ruling 2026-09-04) | No switch to re-enable manual review (code change + deploy); a persistent auto-approve failure strands an APPLICANT with only a CloudWatch line as signal |
| Approve a stranded APPLICANT by hand | Script only: apps/api/scripts/backfill-approve-applicants.ts (bulk, no per-account argument, no dry-run, hardcoded table names; run once 2026-09-04) | No route, no UI; listApplicationsForApproval (src/lib/admin.ts) has no caller outside tests, so there is no pending-applications view |
| Deny / waitlist an applicant | MISSING -- WAITLIST exists in the AccountStatus type (accounts.ts:15) but nothing writes it | The /beta copy promises 'approved, waitlisted, or not this round'; no such outcome can be produced |
| Ban / suspend / unban an account | MISSING -- revokeAccount (src/lib/admin.ts:145) has zero production callers; read side honours BANNED/SUSPENDED (status.ts, latest.ts, apply.ts, PortalStatus 'blocked' view) | A hand DynamoDB status write would skip refresh-token-family revocation and the audit row |
| Founder number / tier / founder entitlement | Automatic on approval (approveAccount transaction); counter inspect/adjust = DynamoDB CLI (apps/api/AWS-SETUP.md:286-306); cap = code constant FOUNDER_CAP=1000 (founder.ts:3) | No manual grant/revoke/reassign; no admin view of the ladder beyond the public /v1/stats |
| Cohort assignment | MISSING -- accounts.ts:148 and beta-status.ts:63 only READ cohort; no writer anywhere in apps/api/src | Portal shows 'Not assigned' forever while copy says 'Cohort assignment follows review' |
| Add / revoke an administrator | Script only: apps/api/scripts/seed-first-admin.ts (refuses if the IronicsAdmins table is non-empty); any second admin or a revocation = hand DynamoDB write | The script's own comment (L20-22,50) defers to an 'audited admin route' that does not exist in api.ts; whether the seed was ever run on prod is recorded nowhere (memory grep: no hits) -- /admin may 404 for everyone |
| Claim codes pending -> approve (mint Volts) | UI /admin Pending tab -> POST /v1/admin/claims/approve (AdminConsole.tsx:46; api.ts:180) behind the allowlist; refusal table lib/admin.ts:52-85 | Never exercised with real money (VOLTS scope L3 'no money moved'; nothing later); single flat role -- anyone on the allowlist can mint |
| Record an incoming Cash App payment | API only: POST /v1/admin/payments/record (api.ts:221) | No form in AdminConsole.tsx -- the Unattributed queue can only be populated by calling the API by hand |
| Resolve an unattributed payment | UI /admin Unattributed tab -> POST /v1/admin/payments/resolve (AdminConsole.tsx:62; api.ts:232) | Depends on the missing record form above |
| Audit log view | UI /admin Audit tab: GET /v1/admin/audit, one day at a time, 200 rows (AdminConsole.tsx:28) | The ONLY admin-facing data view in the product; no filter by account/actor/action |
| Economy / catalog registration (PlayFab) | API only: POST /v1/admin/economy/register (api.ts:214, carries the PlayFab title secret); plus Bag_Man_Backend scripts setup:economy:catalog / seed-* / verify-* | No console UI; catalog seeding still happens by script (commits 3d27504, 46d84d1) |
| Publish a client release | Script only: apps/api/scripts/publish-release.ps1 (provenance gate in child PowerShell, refuses existing key, writes BOM-less win64/latest.json) | No UI; no release notes field anywhere; -Channel is a label only (one pointer key); manifest.json/.sha256 sidecars never uploaded; no post-upload size assert |
| Promote / rollback a release | MANUAL: hand-author latest.json + aws s3 cp | No script flag, no UI, no audit row -- BPD s12 bullet 1 entirely unbuilt |
| Publish / refresh the dedicated server build | MANUAL: staged from D:\BagMan\StagedBuilds\WindowsServer to s3://ironics-releases/server-build/ + aws ssm send-command on EC2 i-0b23133a70f6a55b2 (memory project_s12_gamelift_server_live.md) | No script equivalent of publish-release.ps1; RUNBOOK_GAMELIFT_COMPUTE.md still lists the dev LAN compute as current |
| Download metrics | MISSING -- only download_started CloudWatch lines (latest.ts:78), fired on every /portal page view via the DownloadCard mount fetch (DownloadCard.tsx:39-55); bucket has no access logging (ironics-portal-stack.ts:307-314) | BPD s12 'download metrics panel' has no data source until D2 1A.2 telemetry + the metadata-only endpoint |
| Public stats (founders / testers / build rev) | API GET /v1/stats (public, 30 s cache) -> LiveStrip / FounderLadder (NEXT_PUBLIC_STATS_URL baked at build) | 'testers' counter has no writer (stats.ts:37 reads only); BUILD_REV never set in api.ts commonEnv (handlers/stats.ts:40 reads it) -> TESTERS and REV never populate; getMockStats() fallback is one missing env var from prod (regressed 2026-09-04) |
| Funnel analytics | Server: JSON log lines to CloudWatch (events.ts:12-16); client: POST /api/events -> Workers logs, transmits nothing unless NEXT_PUBLIC_ANALYTICS_ENABLED (unset in deploy-web.yml) | No warehouse, no query, no dashboard; install_completed / first_match_completed never emitted |
| Kill telemetry read (tournament standings / Top 10) | API only: GET /kills/top on the tentpole, HMAC-signed with the earn secret (Bag_Man_Backend/lambda/kill-telemetry/index.ts:135-149) | No UI/consumer; effectively a signed curl by whoever holds the secret |
| Entitlement lookup / grant / revoke, wallet adjustments, refunds | Tentpole HMAC server-signed endpoints (/earn, /counted-entitlement, /conditional-entitlement, /refund-purchase, /purchase-bundle) -- scripts with the earn secret | No UI, no dual-entry audit view (BPD s12 bullet 3); cross-stack auth from a portal admin to the tentpole is undesigned |
| Stuck settlements / escrow | reconcile Lambda (rate 5 min) retries safe partials; in-doubt rows are COUNTED AND LOGGED only (Bag_Man_Backend/lambda/reconcile/index.ts:14-30) | No stuck-row view; the 'dashboard' the code comment refers to does not exist |
| Matchmaking routing (FlexMatch vs PlayFab cells) and commit deadline | MANUAL: SSM /bagman/matchmaking/flexmatch-cells (create-ticket/index.ts:95-118, outside CloudFormation by design); FLEX_COMMIT_DEADLINE_SECONDS=300 code constant (queue-registry/registry.ts:178) + setup:flexmatch --apply | No portal surface, no audit/history; the 2026-09-04 '30 s then bot-fill' ruling is met client-side only (Option A); setup-flexmatch --prune is destructive |
| GameLift fleet / compute / 24/7 server ops (register, restart, redeploy, logs) | MANUAL: aws CLI + SSM send-command + Scheduled Task BagManGameLiftServer + AWSPowerShell on the box | No health/uptime view; no session inspection (BPD s12 bullet 3); recipe lives in memory + C:\game\run-server.ps1, not in Docs |
| Secrets (EOS, Resend, Turnstile, PlayFab, HMAC, EOSVoiceChat, future CF signing key) | MANUAL: put-secret-value; all secrets created empty with RETAIN and populated out-of-band (ironics-portal-stack.ts:277-294) | AWS-SETUP.md:18 still says all three secrets are empty; no rotation runbook (D2 1A.3 proposes one) |
| Terms version bump / TERMS_REQUIRED enforcement | Code constant CURRENT_TERMS_VERSION (lib/terms.ts) | accept-terms is recorded but never gates download/console (apps/api/README.md:263-273 s5.3 gap) |
| Public roadmap / site copy | Source code (apps/web/lib/data.ts PHASES + copy blocks) -> commit -> web deploy | No admin editing, no data source; every change rides a deploy path that CI reports as failed |

## 5. Contradictions and stale content (31)

- CI TOKEN CAUSE: memory project_ironics_portal_download_live_state.md:15 and D2 decision 13 (L117) say CI web deploy is BLOCKED by an EXPIRED CLOUDFLARE_API_TOKEN; the 2026-09-04 run log (gh run 33928400546) shows 'logged in with an Account API Token', 'Uploaded ironics-web (4.77 sec)', then 'Authentication error [code: 10000]' only on GET /zones/05d1bd7f.../workers/routes -- exactly apps/web/LEGAL_OPEN_ITEMS.md L4 (L67-95): a missing Zone->Workers Routes->Edit permission, a dashboard edit not a renewal. Net: CI uploads and reports failure; two deploy paths (CI 23:19Z and local wrangler the same evening) race on one Worker; which build is live cannot be read from disk (the live stats being real suggests the flag-bearing CI build).
- D1 STATUS: Docs/IRONICS_D1_DISTRIBUTION_PLAN.md:3 'awaiting operator deploy' and BUILD_PRUNE_DEPLOY_PLAN.md:283-285 'Remaining: stand up the S3/CloudFront/Lambda mint + website download page' vs deployed + operator-proven 2026-09-04..06 through 0.1.3-beta (memory L23, L46).
- D1 L13 'no admin route is deployed' vs apps/api/lib/api.ts:180-241 eight /v1/admin/* routes deployed since 2026-08-25 -- true only for ACCOUNT-approval routes.
- ADMIN SCOPE DOC: Docs/design/IRONICS_VOLTS_PURCHASE_ADMIN_SCOPE.md:3 'Nothing authored' and s1.2 'There is no admin HTTP surface at all' vs the console + 8 routes built the same day (Ironics-Platform 4da6a25..975ef7a 2026-08-24); its 'C now, A later' recommendation was not followed and no ruling is recorded; what shipped is a per-request allowlist table, none of A/B/C exactly.
- ADMIN SEED: apps/api/scripts/seed-first-admin.ts:20-22,50 says later changes go 'through the audited admin route' -- no admin-management route exists; and no doc, commit or memory note records the seed being run, so /admin reachability is UNKNOWN.
- BPD s12 (L265-267) hardens 'the existing Volts purchase admin scope' with roles/session timeout/IP allowlist -- i.e. assumes a web console -- while that scope doc recommends no console (Model C) first. No ruling picks one.
- ROADMAP REV: apps/web/app/roadmap/page.tsx:21 prints FALLBACK_BUILD_REV '0.8.14' (lib/stats.ts) vs live client 0.1.3-beta; the API cannot supply a real rev because BUILD_REV is never set in api.ts commonEnv while handlers/stats.ts:40 reads it.
- ROADMAP VR: data.ts:121 'Next . 7 Sep 2026 -- VR Meta Quest and PC VR' (handoff s2 ruling 2026-09-02) vs Bag_Man.uproject VR plugins all Enabled:false and zero VR docs/tracker rows -- a public date due tomorrow with no engineering trail.
- ROADMAP CLAIMS: data.ts:120 'Founder invites, crash telemetry, and the first balance pass' vs auto-approval (no invites) and no crash telemetry anywhere (events.ts emits only download_started).
- BETA COPY vs AUTO-APPROVAL: app/beta/page.tsx ('Cohorts go out weekly ... approved, waitlisted, or not this round'), BetaComplete.tsx:45-49 ('You are in the queue'), PortalStatus.tsx:190 ('Cohort assignment follows review'), data.ts:106-108 tier copy ('cohort two week two', 'as server capacity allows'), PLATFORMS (data.ts:155 lists Quest/PC VR/PS5/Xbox/iOS/Android) vs apply.ts:92-114 instant approval, Windows only, cohort never written.
- FOUNDERS: Docs/design/IRONICS_PRICING_SSOT.md:119-121 'Founders is out of scope ... no backend representation' vs the portal issuing Founder numbers + a founder entitlement (approveAccount; memory: operator = Founder #1 tier I) -- and no Bag_Man doc says what a Founder receives in-game.
- DESIGN HANDOFF HEADER: design/IRONICS_WEB_DESIGN_HANDOFF.md:6-8 cites parent 'IRONICS_GTM_PLATFORM_ROADMAP.md (Phase W4)' (absent everywhere under C:\Dev -- Glob 2026-09-06), repo 'Instawerx/Ironics_Web' (real: Instawerx/Ironics-Platform), domain 'ironics.com' (real: ironics.org, wrangler.jsonc:35), stack 'React Three Fiber' (2D pedestal by ruling, DEPLOYMENT.md:179); s3 tokens describe v1 while the site ships v2 Glass Black (chat1.md:621-685; web README:24-37).
- DESIGN CANVAS vs CODE: the v2 .dc.html roadmap still says 'Next . Sep 2026 Sticker placement + scale' while code says 'Shipped . Sep 2026 Creator complete' (784444e) -- no design pass after the ruling.
- MOCKUP-FIRST: doctrine (CLAUDE.md #7, ruled 2026-08-27) and D1.4 (L92-95 'operator approves the mock before any page is built') vs the DownloadCard (ffb9829) and /admin (975ef7a, pre-ruling) shipped with no mockup; COMMS-2 code shipped (Batch C) while its mockup approval is still 'NEXT' in memory.
- REPO DOCS: Ironics-Platform/README.md 'Open items' ('AWS is not linked', 'three secrets are empty', 'Cloudflare deploy never run', '12 routes, 94 tests'), apps/api/README.md:118 'Eight routes' + :275-289 'Nothing has been deployed ... W5 admin console remains', apps/web/README.md:60-75 'Known stubs' (portal fixture, apply not persisting, /legal not built), apps/crm/README.md ('entity open', fixtures), AWS-SETUP.md:18 'secrets not done' -- all superseded by the deployed 21-route stack, live console, 4 legal docs in force, Epic sign-in proven live 2026-09-02, entity settled (legal.ts:8).
- apps/crm/README.md:22-23 'Do not add [the back office] to apps/web' vs the admin console living at apps/web/app/admin.
- LIGHTHOUSE: handoff s14 and root README:23-26 'blocks merge' vs deploy-web.yml:89-106 / 647853d 'reporting, not blocking' (2026-08-24).
- RELEASE LEDGER: releases/README.md L9-13 'every release commits manifest + sha256' vs git log -- releases/ = 9ff73c3d only (0.1.0-beta) while 0.1.1/0.1.2/0.1.2.1/0.1.3-beta shipped; '0.1.0-beta is a PLACEHOLDER pending real semver' never resolved.
- D2 INTERNAL DRIFT: Tier 1B 'published as 0.1.3' (L54, L57) vs 0.1.3-beta published 2026-09-06 12:55Z without the cull (3.64 GB, memory L40).
- LETTER COLLISION 'D2': BPD s6 L153 defines D2 = itch.io channel (and D1 plan L37 'Not D2 (itch)') while Docs/dist/D2_... uses D2 for the CloudFront/launcher tiers (itch rejected there, L65).
- 'WEBSITE-ZIP ONLY FOR NOW' ruling (BPD L183-184) and D1 s0 'No launcher app' vs D2 Tier 2 launcher + decision 10 (interim PowerShell downloader) -- a new ruling is required before Tier 2.
- R2 WORDING: Docs/ENGINE_DOCTRINE.md:47-49, Tools/verify_ship_provenance.ps1:7,89, LIVE_TRACKER row 000 gate uploads 'to R2' -- R2 was never provisioned; releases live in S3 ironics-releases.
- ENGINE DOCTRINE: Docs/DOCTRINE.md B2 (L118) 'build against the C: launcher engine' VERIFIED-ON-DISK vs ENGINE_DOCTRINE.md (2026-09-03) retiring C:; DOCTRINE G9 'never prune mid-flight' vs the approved Track A/B prune; DOCTRINE.md never amended.
- LIVE_TRACKER: row 036 'EOS-AUTH-C2 BLOCKED' vs commit aa550a1d 'PROVEN LIVE 2026-09-02' and row 100 remains text; row 027 'GameLift compute LAN-only' vs the 24/7 EC2 server (memory 09-05); row 108 'plan SCOPED awaiting approval' vs BPD L180-184 approved 09-01 + Tracks done; no rows after 09-03; zero rows for the website/portal/admin lane.
- S12 DOCS: Docs/S12_GAMELIFT_INTEGRATION_PLAN.md:3 'DRAFT ... Not started' + L36-49 'Does NOT exist' and RUNBOOK_GAMELIFT_COMPUTE.md:9-17 (dev LAN compute) vs the live EC2 fleet/compute; memory records this stale section misleading an agent on 2026-09-05.
- Bag_Man_Backend/docs/AWS_ARCHITECTURE.md:44 'one HTTP API, 14 Lambdas' (self-declared must-stay-true) vs 36 lambda dirs / ~28 routes today.
- VOICE: BPD/memory 'COMMS-3 voice ON cooked in' vs memory project_ironics_voice_aaa_scope.md: runtime provider INERT (no [EOSVoiceChat] secret -> IVoiceChat null). Both true; no public surface may claim voice.
- WEBSITE REPO NAME across Bag_Man docs: 'Instawerx/Ironics_Web' (CC roadmap CC-X6, 08-21), 'ironics-live (apps/api)' (VOLTS scope, 08-24), 'Instawerx/Ironics-Platform' (D1/D2, memory) -- one monorepo, three names.
- LENS PATH ERRORS corrected here: IRONICS_VOLTS_PURCHASE_ADMIN_SCOPE.md and IRONICS_BETA_LAUNCH_ASSETS_ON_HAND.md DO exist (Docs/design/, Docs/reference/); lens 3's 'missing on disk' claims were path mistakes. IRONICS_GTM_PLATFORM_ROADMAP.md genuinely does not exist.
- cook-manifests/README.md:7 names cook_20260810g as the reference baseline vs fe246c68 (2026-09-02) banking the post-prune manifest as 'the new ship baseline'; BPD s13 item 1 still diffs against 08-10.
- AGENTS.md ('Title: BAG MAN', 'Use BAG_MAN_LIVE_TRACKER.html'), README.md ('YOLO BUILD V1'), SKILLS_REGISTRY.md:26 ('Apple-Glass-inspired'), CLAUDE.md /AFLVFXLibrary/Laser/ path (dead per BPD L174) -- retired names/paths still in root/agent-facing files.

## 6. Candidate scope - OPTIONS per workstream (nothing chosen)

### Website

**W1 -- Truth pass (copy-only, no design change)** - _Estimate: hours to 1 day of edits + one deploy_  
Why: The live site describes a review/cohort/invite process that has never run (api /v1/stats issued=1) and three CTAs go nowhere; cheapest credibility fix, and it is the BPD s12 'download page copy' bullet  
Depends on: Deploy path decision (token permission fix vs local wrangler with both NEXT_PUBLIC_* flags); Rulings: cohort dead or alive; founder tier meaning; whether a press kit / creator program will exist; Discord existence

- Rewrite beta-flow copy for auto-approval: app/beta/page.tsx:41-43, BetaComplete.tsx:45-49, PortalStatus.tsx:190, beta/received/page.tsx:23, data.ts BETA_FACTS L140-145
- Retire cohort promises in TIERS (data.ts:106-108) or write a cohort
- Trim PLATFORMS (data.ts:155) to what exists or label the rest
- Fix dead CTAs: /press 'Download kit' (press/page.tsx:104), /creators 'Apply as creator' (creators/page.tsx:34), /roadmap Discord (roadmap/page.tsx:56)
- /market: label as fixture, hide, or repoint to real catalog prices
- Home: one line acknowledging the playable Windows beta + download path for approved testers

**W2 -- Post-launch site revision (mockup-first)** - _Estimate: 2-4 days design + 3-6 days build_  
Why: Every surface built after 2026-08-16 skipped the mockup gate; the site still reads as a pre-beta creator pitch with no gameplay on any public page  
Depends on: W1 rulings; Reels storage location + publication ruling; venue art state (NANOWATT blocked, INFINEON undecided); Mockup-first approval per screen

- Re-cut the design handoff header/s3 for v2 Glass Black and author the missing parent roadmap doc
- New artboards: post-beta home (real gameplay: reels, venue key art), releases/changelog page, download page, VR/console pages (only if ruled), /signin + /auth/verify + DownloadCard as-built records
- Implement after operator approval; real IRONICS art only (naming ruling)

**W3 -- Doc-truth + CI/deploy repair** - _Estimate: 1-2 days + one operator dashboard step_  
Why: CI is red-but-shipping (a real regression went unnoticed 08-17..08-23 per L4); every later workstream ships through this path; stale docs actively misled agents (S12) and will mislead the admin build  
Depends on: Operator Cloudflare dashboard access; Ask-before-implementing approval for each doc edit

- Operator: add Zone->Workers Routes->Edit to the CI token (L4) OR retire CI web deploy; pick ONE deploy path
- Reconcile README.md, apps/web/README.md, DEPLOYMENT.md, apps/api/README.md, AWS-SETUP.md, apps/crm/README.md to deployed state
- PR preview deployments (wrangler versions upload) -> Lighthouse re-baseline -> restore s14 gate or formally retire it
- Analytics: choose warehouse or formally defer; decide NEXT_PUBLIC_ANALYTICS_ENABLED

**W4 -- Money surface (web checkout + real /market)** - _Estimate: 3-6 days build after rulings_  
Why: R71 and D-1 rule the store sells on both surfaces and $5/mo is the website price; the backend route is deployed with no front end  
Depends on: L1 Virtual Currency Terms in force (counsel); Volts-per-dollar bonus tier ruling; Epic-unlinked refusal ruling (VOLTS scope L193-200); Admin baseline verified (A0)

- Checkout page calling /v1/checkout/volts (api.ts:171) with the Cash App claim-code flow (VOLTS scope s2)
- /market from the real catalog (catalog-export.json / PlayFab) with one pricing source per R71
- Console-cert read before the web path ships (R71)
- Payments console record-payment form (see Admin A2) so the loop closes

**W5 -- Quality gate (BPD s12 bullet 4) as a checklist run** - _Estimate: 1 day per pass_  
Why: The only written website checklist in the plan of record  
Depends on: W3 preview URLs for a meaningful Lighthouse number

- Lighthouse/a11y pass on all public routes
- Epic sign-in flow re-test (post any deploy)
- Legal pages current incl. chat/DM/voice coverage review
- Download page copy + art per naming ruling

_D2 items that belong here:_
- D2 0.5 DownloadCard copy + verify block (exact bytes, 24 h resume note, Get-FileHash line) -- apps/web/components/DownloadCard.tsx:26-33,109-125
- D2 review fix #2 metadata-only endpoint (GET /v1/download/latest?meta=1) so the DownloadCard mount fetch stops minting and emitting download_started -- prerequisite for honest funnel data and any quota
- D2 decision 13 (corrected): CI token needs the Zone->Workers Routes permission, not a renewal; 'DownloadCard copy ships via local wrangler' is the symptom of the unfixed L4

### Admin Dashboard & Controls

**A0 -- Verify the baseline before building on it** - _Estimate: hours (read-only checks + one script run if approved)_  
Why: Nothing on disk proves the existing console is reachable or has ever processed a payment  
Depends on: Operator approval to run the seed script if the table is empty

- Confirm whether seed-first-admin.ts was run and the operator's account (01M06WNV71GP2QWYPBQA5JT4WC) is on IronicsAdmins; confirm /admin renders
- Dry-run the payments loop end-to-end with the record-payment API (no money)
- Check Lambda concurrency quota state (AWS-SETUP.md:87-111) since admin polling adds load

**A1 -- Account & beta admin (thin slice, mockup-first)** - _Estimate: 2-4 days (library is transactional + tested; work is routes, grants tests, UI, mockup)_  
Why: Account lifecycle is 'library-complete, surface-absent'; ban/suspend has no operator control at all today  
Depends on: Auth-model ruling (keep flat allowlist vs roles); Home decision: apps/web/app/admin vs apps/crm (JS budget 180 KB gate); Mockup approval

- Routes + console tabs over the EXISTING library: listApplicationsForApproval, approveAccount, revokeAccount (ban/suspend/unban), founder ladder view
- Admin add/revoke route (the one seed-first-admin.ts assumes)
- Cohort: either a writer or remove the field/copy
- Optional manual-review switch for /beta/apply if the auto-approval ruling is ever reversed

**A2 -- Release admin (BPD s12 bullet 1)** - _Estimate: 1-2 days script-level; 3-5 days with UI + metrics_  
Why: Every 0.1.x release was published by PowerShell and rolled back by hand-editing JSON; four releases have no committed ledger record  
Depends on: D2 Tier 0/1 decisions for any real metrics; Retention ruling (D2 decision 12); Semver ruling

- publish/promote/rollback of latest.json (script flags first, UI second); release-notes field in LatestPointer + DownloadCard
- Upload manifest.json/.sha256 sidecars + post-upload size assert (D2 0.4)
- Download metrics panel over the Athena view (needs D2 1A.2) or, minimally, honest download_started counts after the metadata-only split
- Server-build publish script parity

**A3 -- Hardening to institutional grade (BPD s12 bullet 2)** - _Estimate: 3-5 days_  
Why: Today one flat allowlist row can mint currency and register the PlayFab catalog with a 30-min JWT and no second factor  
Depends on: Auth-model ruling (A vs C; MFA scope); A1 in place (otherwise there is little to protect beyond minting)

- Roles viewer/operator/owner in the allowlist row + per-route requirement
- Session timeout policy for admin sessions; IP allowlist option; second factor
- Audit on every mutation (existing routes already audit; extend to new ones)

**A4 -- Live-ops surfaces (BPD s12 bullet 3 + kill telemetry + ops views)** - _Estimate: 1-3 weeks; each panel is a cross-stack integration_  
Why: Every game-side lever is script/console-only with no audit; BPD s12 names these explicitly  
Depends on: Cross-stack auth design (portal admin -> tentpole HMAC secret) -- undesigned; GameLift/AWS-over-PlayFab ruling scope; A3 roles (these are the dangerous controls)

- Entitlement lookup/grant/revoke and wallet adjustments via the tentpole HMAC endpoints with dual-entry audit
- GameLift session/compute inspection; EC2 server health; reconcile in-doubt rows view
- FlexMatch cell-routing view (SSM) with an audit trail
- Kill telemetry 'Top 10 / standings' read (ruled admin-only)

**A5 -- Relocate the console to apps/crm** - _Estimate: 1-2 days move + CI wiring_  
Why: apps/crm was reserved for exactly this and told builders not to grow it inside the budget-gated site bundle  
Depends on: Decision that the console will grow beyond payments

- Move /admin into the reserved apps/crm workspace per its README; lift tokens to packages/tokens when a second consumer exists

_D2 items that belong here:_
- D2 1A.2 telemetry: CloudFront logs -> Athena partition-projection table + saved view (per-download bytes/elapsed/Mbps/country/outcome) + new server event download_link_minted -- the only proposed data source for the s12 'download metrics panel'
- D2 1A.3 anti-abuse controls: per-account mint quota (IronicsRateLimits pattern), key-rotation runbook, key-group kill switch; review fix per-link rid DENYLIST (CloudFront Function + KVS), CloudWatch BytesDownloaded + AWS Budgets alarms, 'replayed rid' detector -- all admin-shaped controls
- D2 0.4 publish hardening (sidecars, headers, size assert) and 2D manifest/Compactify tooling -- release-admin items
- D2 decision 12 RETENTION (how many versions stay hot; latest.json stays the one pointer) -- a release-admin policy
- D2 log-hygiene note: log + Athena buckets hold live bearer URLs for 24 h -- constrains any metrics panel that reads them

### Roadmap

**R1 -- Correct the static page before 2026-09-07 passes** - _Estimate: hours + one deploy_  
Why: A public 'Next . 7 Sep 2026' will read as missed on 09-08; REV 0.8.14 contradicts the shipped 0.1.3-beta  
Depends on: VR ruling; Discord existence; Deploy path

- VR row: ruling to keep/move/remove (data.ts:121)
- REV: set BUILD_REV in api.ts commonEnv (from latest.json or deploy env) or drop the header field
- Discord: real invite or remove the CTA
- Add the shipped 0.1.0->0.1.3 rows / a known-issues line; remove 'crash telemetry' and 'invites' (data.ts:120)

**R2 -- Roadmap and releases as data** - _Estimate: 1-3 days after A2 exists_  
Why: Today a roadmap edit is a code commit + a deploy CI reports as failed; the page cannot reflect releases that happen four times in 48 h  
Depends on: A2 release admin (release-notes field); Publication-policy ruling

- Serve PHASES from the API or an admin-editable table (ties to A2)
- Auto-derive a releases/changelog section from latest.json + the release ledger (release notes field)
- Publication policy: which internal tracker states are public

**R3 -- Author the missing website/beta programme roadmap doc (SSOT)** - _Estimate: 1 day authoring + operator rulings_  
Why: No website plan, beta-program plan or public-roadmap SSOT exists anywhere on disk (IRONICS_BETA_LAUNCH_ASSETS_ON_HAND.md:17-18; Glob 2026-09-06); the design brief's parent is a phantom  
Depends on: Operator rulings on beta programme scope and public/private boundary

- Write the parent the design handoff cites (IRONICS_GTM_PLATFORM_ROADMAP.md or equivalent): beta programme (1,000 testers / 36-peak target, what 'testers' means, invites/creator codes, doors shipping Disabled, AFL-3063 walk-through), website phases, admin phases, distribution tiers
- Map internal tracker rows -> public roadmap items; add the platform lane to LIVE_TRACKER

_D2 items that belong here:_
- D2 Tier 2 launcher / delta patching / per-venue chunking and Tier 1B smaller package are public-roadmap-shaped promises IF approved -- they should appear on /roadmap only after the ruling, never before (the VR row is the cautionary case)
- D2 decision 14 (tester geography + testers x patches/month forecast) is a beta-programme number the roadmap doc (R3) must own

### Distribution (D2 fold-in)

**D0 -- Tier 0: CloudFront + OAC + key-group 24 h signed URLs on the existing bucket (kill the incident class)** - _Per D2: 4-8 engineer-hours + operator key-pair/secret steps_  
Why: The role-signed 900 s presign is the documented root cause of the delivery-class defect; this is BPD s11 as designed  
Depends on: D2 decisions 1-2 (approve Tier 0; 24 h bearer URL, no IP binding); Review fix: per-link denylist + cost alarms shipped with it

- D2 0.1-0.3 CDK distribution/OAC/public key/key group/secret; latest.ts signer swap; narrowed S3 grant; tests
- Watched acceptance: throttled download paused past 15 min, resumed as 206, sha256 verified (0.6)
- cdk diff + Replacement probe shown before deploy

**D1 -- No-regret hardening independent of the CDN choice** - _Estimate: 1 day_  
Why: All of it is required by Tier 0/1 later and fixes the inflated funnel today  
Depends on: Ask-before-implementing approval; Deploy path for the web half

- Metadata-only endpoint (review fix #2)
- Publish sidecars + headers + size assert (0.4)
- DownloadCard copy (0.5)
- Release ledger: commit 0.1.1..0.1.3 manifests; update D1 header, BPD s13 item 5, R2 wording (1A.5)

**D2 -- Tier 1A: dl.ironics.org, telemetry->Athena, mint quota, WAF, Pro plan, rotation runbook** - _Per D2: 2-3 engineer-days + 3 operator steps_  
Why: Only source of per-download speed/outcome data; flat cost ceiling before any tester growth  
Depends on: D0; D2 decisions 3-5 (Pro vs PAYG; DNS/ACM; PII retention)

- ACM cert + 2 grey-cloud CNAMEs (operator)
- Logging v2 -> private bucket -> Athena view + download_link_minted event
- WAF web ACL BEFORE plan subscription; confirm Free-Tier eligibility; prove cdk deploy keeps the plan
- Quota via IronicsRateLimits after the metadata split

**D3 -- Tier 1B package cull (Bag_Man lane, cook-gated)** - _Per D2: 1 operator-day + one Shipping cook (hours) + lap_  
Why: ~0.9 GB of non-game payload ships in every patch until this lands  
Depends on: D2 decision 6 per item; Operator-owned D: build lane

- Explicit MetaHumanCharacter/MetaHumanSDK/NNEDenoiser disables or DirectoriesToNeverCook (review-corrected mechanism)
- bCompileCEF3=false (LoginFlow/OnlineFramework is the CEF3 cause, not UDS)
- Drop SF_VULKAN_SM6; HDRI_6 compress; Windows World-group 4K cap (touches 'don't re-tune proven')
- Cook + manifest diff + cooked 2-client lap; publish as the NEXT version (not 0.1.3)

**D4 -- Tier 2: BuildPatchTool spike -> launcher -> per-venue chunks** - _Per D2: 1-2 days spike; 3-6 weeks launcher; ~1 week chunking_  
Why: Only lever that makes weekly patches ~minutes and fresh installs ~1-2 GB; also collapses egress cost  
Depends on: Re-opening the 'website-zip only for now' / 'no launcher app' rulings (BPD L183-184; D1 s0); D2 decisions 7-10; Protected-surface ruling for the join path

- 2A two-armed spike (BPS PatchGeneration/DiffManifest + UE-native -generatepatch) to MEASURE the weekly delta
- 2B launcher core decision (BPS/Slate vs Tauri/Rust) + code-signing decision + launcher sign-in design (unscoped)
- 2C per-venue chunking + mount-before-travel (MOCKUP-FIRST) + matchmaking join-path ruling

**D5 -- Tier 3 R2 (shelved unless triggers fire)** - _Per D2: 3-5 days when triggered_  
Why: Documented cost lever only; not worth a second byte path below 50 TB  
Depends on: Sustained >50 TB/month AND measured >=100 Mbps

- Confirm trigger conditions (decision 11); optionally measure R2 single-stream throughput now

_D2 items that belong here:_
- Tier 0 (0.1-0.3, 0.6), Tier 1A.1/1A.4/1A.6, Tier 1B, Tier 2A-2F, Tier 3 -- the distribution-only remainder after the website/admin/roadmap fold-ins above
- D2 decisions 1-12 and 14 (13 is a website/CI item, corrected)
- Letter-collision cleanup: BPD s6 'D2 = itch' vs Docs/dist 'D2' -- rename one before both plans are cited together

### Comms/Voice carry-over

**C1 -- Operator-only gates (no engineering)** - _Operator minutes; unlocks V1+ and the chat skin pass_  
Why: Voice is cooked ON but INERT; every voice phase and the chat skin are hard-gated on these two operator actions  
Depends on: -

- V0: [EOSVoiceChat] block + real 64-hex ClientEncryptionKey in the gitignored EAS overlay; enable Voice/Lobby/P2P in the Epic Dev Portal
- Ratify (or amend) the COMMS-2 4-artboard chat mockup (artifact 049d14a3-d1f8-441f-8aaf-1da86b78debe)

**C2 -- COMMS-1/2 PIE proof session** - _One operator-attended session (~hours)_  
Why: Text chat shipped in 0.1.1 unproven by the programme's own gate  
Depends on: Editor/PIE availability; S12 server optional

- 1 dedicated + 3 clients per Docs/COMMS_PIE_PROOF_RUNBOOK.md (P1-P6, E1-E3); tracker COMMS-1 -> PROVEN + tag comms-1-text-spine

**C3 -- Website/legal coverage of comms** - _Estimate: hours (legal review is counsel-owned)_  
Why: COMMS-1/5 shipped after the 2026-08-24 legal pass; nothing on disk says the docs cover them  
Depends on: Legal owner

- Review Privacy + Community Guidelines for text chat, DMs, voice (recording/retention/moderation) before any public voice claim
- Roadmap/site language: no 'voice' until V1 is proven

**C4 -- V1-V2 voice core now that S12 exists** - _Per plan: multi-session engineering after V0_  
Why: S12 removed the shared blocker on V2/V5/V6  
Depends on: C1; 2-client/2-device audio proof harness

- V1 2-client core proof; V2 server-owned rooms via the live dedicated server; then V3+ per the plan

_D2 items that belong here:_
- None directly; D2's per-venue chunking (2C) touches the same protected travel/matchmaking surface COMMS-4 server rooms will hook, so sequencing both against the S12 server matters

## 7. Decisions needed from the operator (26 + 4)

1. [ ] DEPLOY PATH: Will you add Zone->Workers Routes->Edit (zone ironics.org) to the CI CLOUDFLARE_API_TOKEN so deploy-web.yml goes green, and is CI then the ONLY web deploy path -- or does local wrangler remain the path (with the NEXT_PUBLIC_EPIC_ENABLED + NEXT_PUBLIC_STATS_URL law)? Which build is live right now?
2. [ ] ADMIN BASELINE: Was apps/api/scripts/seed-first-admin.ts ever run against prod? If not, may it be run for your account (01M06WNV71GP2QWYPBQA5JT4WC) before any admin work starts?
3. [ ] ADMIN AUTH MODEL: Keep the flat IronicsAdmins allowlist as shipped, adopt roles/session-timeout/IP-allowlist/second factor per BPD s12, or fall back to the VOLTS scope's Model C (signed CLI, no console) for privileged actions? Which controls (minting, ban, release rollback, wallet adjustments) require the stronger tier?
4. [ ] ADMIN HOME: Does the console stay in apps/web/app/admin (inside the 180 KB JS budget) or move to the reserved apps/crm workspace as its README instructs?
5. [ ] ADMIN SCOPE ORDER: Which of account/beta admin, release admin, hardening, live-ops is first -- and does mockup-first apply to every new admin screen (and retroactively as an as-built record for /admin and the DownloadCard)?
6. [ ] AUTO-APPROVAL: Is instant approval permanent for the closed beta? If yes, is the cohort field/copy removed and the 'approved, waitlisted, or not this round' promise dropped; if no, what re-enables manual review and who reviews?
7. [ ] FOUNDERS: What does a Founder receive in-game (Pricing SSOT says nothing exists), and do the tier promises ('day one', 'cohort two week two', 'as server capacity allows') stand or get rewritten?
8. [ ] TESTERS COUNTER: What counts as a tester (approved account / completed download / first match), and should the counter be written -- or should TESTERS leave the live strip?
9. [ ] VR ROW: Does 'VR -- Meta Quest and PC VR . Next . 7 Sep 2026' stay, move to a new date, or come off /roadmap before 2026-09-08? Is there VR work anywhere this brief did not find? **[SUPERSEDED by §0: the VR programme exists (Docs/design/IRONICS_VR_SSOT.md + _TASKS.md); the open question is #29, the DATE.]**
10. [ ] ROADMAP CONTENT: Do client releases (0.1.0->0.1.3) and known issues appear on the public roadmap? What internal tracker state is public? Does the roadmap become data (API/admin-edited) or stay code?
11. [ ] DISCORD / PRESS KIT / CREATOR PROGRAM: Do a Discord server, a press-kit archive, and a creator application process exist or get built -- or are those CTAs removed?
12. [ ] MARKET: Does /market show the real catalog and prices now, carry a fixture disclaimer, or hide until a checkout exists?
13. [ ] MONEY: May the Cash App claim-code flow take real payments before L1 Virtual Currency Terms are in force? Volts-per-dollar bonus tiers -- yes/no and what? Does checkout refuse Epic-unlinked accounts? Who owns L1/L3 with counsel and by when?
14. [ ] BETA PROGRAMME DOC: Should a website/beta-programme roadmap SSOT be authored (the parent the design handoff cites does not exist)? **[CORRECTED by §0: the parent EXISTS — it is Docs/IRONICS_GTM_PLATFORM_ROADMAP.md, recovered 09-06; remaining question is only the targets/geography/patches figures.]** What are the tester acquisition targets (1,000 / 36-peak per hub SSOT still current?), geography and patches/month (D2 decision 14)?
15. [ ] DESIGN HANDOFF: Re-cut v3.2's header/s3 tokens to the shipped v2 Glass Black, and does the design canvas get updated to match code (roadmap phases)?
16. [ ] ANALYTICS: ~~Choose a warehouse (or formally defer)~~ **[already RULED = PostHog, GTM §6 → confirm under #28]**, and should NEXT_PUBLIC_ANALYTICS_ENABLED go on in prod? Does the game client ever emit install_completed / first_match_completed?
17. [ ] LIGHTHOUSE GATE: Restore s14 as a merge blocker after PR previews + re-baseline, or retire it formally?
18. [ ] D2 DECISIONS 1-12, 14 (Docs/dist/D2_DOWNLOAD_DISTRIBUTION_SCOPE.md L103-118): approve Tier 0; 24 h bearer URL + no IP binding; Pro vs PAYG (+ eligibility); Tier 1A DNS/ACM steps; PII retention; Tier 1B cull items (i)-(vi) individually; 2A spike; launcher core + code-signing; per-venue chunking + hub-bundled-or-not + join-path precondition; interim PowerShell downloader; R2 triggers; version retention; tester geography/forecast. Decision 13 is re-stated: permission edit, not renewal.
19. [ ] LAUNCHER RULING: Does 'website-zip only for now' / 'no launcher app' (BPD L183-184, D1 s0) still stand, or is Tier 2 open for scoping after the 2A spike?
20. [ ] SEMVER: Is 0.1.x-beta the release scheme (it was a placeholder), and should the 0.1.1..0.1.3 ledger records be committed?
21. [ ] DOC-TRUTH PASS: Approve edits to the stale docs listed in contradictions (READMEs, AWS-SETUP, D1 header, BPD s13 item 5, R2 wording, S12 docs, tracker rows, DOCTRINE B2/G9) -- each is an edit under ask-before-implementing.
22. [ ] TRACKER: Should the Ironics-Platform lane get rows in Docs/LIVE_TRACKER.html, or is the platform tracked elsewhere?
23. [ ] REELS: Where are the 8 approved matchplay reels + 4K loop stored, and do they go on the site / social (no plan exists)?
24. [ ] COMMS: Will you do V0 (secret + Epic portal) and ratify the chat mockup now; when is the COMMS-1/2 PIE session; do Privacy/Community Guidelines need a chat/voice review before any public voice claim?
25. [ ] PLAYTEST CARRY-OVER: Do Batch D Option B (backend 30 s FlexMatch commit + versioned rule set) and the Map_3v3 -> Arena routing get scheduled now that S12 is live, or wait?
26. [ ] INFINEON / NANOWATT: Is the INFINEON venue identity decided (reconcile brief vs L_Expanse or change map), and does NANOWATT art wait -- both affect what gameplay art the site can show?

**Added by the corrections above:**

27. [ ] DISTRIBUTION RULING: R2 + patch paks (GTM §7) vs CloudFront + OAC (BPD §11 / D2) - which stands, and does GTM §7.1 engine-native patch generation enter scope now?
28. [ ] ANALYTICS: confirm PostHog per GTM §6 (and the cookie/consent posture GTM §13 flags), or re-rule.
29. [ ] VR DATE: reconcile the public '7 Sep 2026' row with the VR programme (VR-0 not started).
30. [ ] DOWNLOADS INBOX: adopt 'SSOTs live in the repo' as a rule; move the three secret-bearing notes to a secrets manager.

## 8. Suggested sequencing (SUGGESTION - accept, reorder or reject)

SUGGESTION ONLY -- an order proposed for the operator to accept, reorder, or reject; nothing here is decided. (0) UNBLOCK THE PIPE FIRST: the token permission edit (L4) + a single-deploy-path ruling, and the A0 allowlist check -- because every website, roadmap and admin change ships through deploy-web.yml and lands behind /v1/admin/me, and today neither is known-good (CI red-but-shipping; seeding unrecorded). Cost: minutes of operator dashboard time + hours of verification. (1) TIME-BOXED TRUTH PASSES (W1 + R1 + D1 + the doc-truth subset): the VR row is due 2026-09-07 and the beta copy contradicts auto-approval; these are copy/config edits with no design dependency, and D1's metadata-only endpoint + sidecars + ledger records are prerequisites both D2 tiers and the release admin need anyway. Reason for putting docs here: the stale S12/README/AWS-SETUP state already misled an agent and will mislead the admin build. (2) RULINGS BATCH: auth model, admin home, auto-approval permanence, founders meaning, D2 Tier 0 approval, semver -- before any admin or CDN code, per ask-before-implementing; R3 (the missing programme roadmap doc) can be authored in this window because it is the container for those rulings. (3) DISTRIBUTION D0 (Tier 0) if approved -- 4-8 h, kills the resume-403 class, and it is BPD s11 as designed; run in parallel with (4) because they touch different files. (4) ADMIN THIN SLICE A1 with mockup-first, then A2 release admin -- A1 reuses a tested library so it is the cheapest real control gain (ban/suspend/approve/list) and A2 is what four hand-published releases already demanded; A3 hardening follows once there is more than minting to protect; A4 live-ops last because it needs a cross-stack auth design that does not exist. (5) D2 Tier 1A telemetry after Tier 0 and the metadata split -- it is the only data source for the s12 download-metrics panel and for sizing the beta; Tier 1B cull runs in the Bag_Man cook lane in parallel, published as a NEW version. (6) W2 post-launch site revision + R2 roadmap-as-data once reels/art rulings and A2's release-notes field exist, so the public site can show the game and the releases without hand-edits. (7) W4 money surface only after L1 legal + pricing rulings. (8) COMMS carry-over runs beside all of this where it is operator-gated (V0, mockup ratification, PIE session scheduling) and does not compete for the website lane; Tier 2 launcher work waits for the 2A spike number and a re-opened launcher ruling. Dependency summary: (0) gates everything; (1) gates nothing but expires on 09-07; (2) gates (3)-(7); (3)+D1 gate (5); (5) gates A2's metrics; A1 gates A3; A3 gates A4; legal gates W4.

## 9. Critic's findings (kept for the audit trail)

**Missing (addressed in §0 where possible):**
- PARENT ROADMAP EXISTS: C:\Users\tabor\Downloads\IRONICS_GTM_PLATFORM_ROADMAP.md (v2.3, 72,384 B, mtime 2026-08-26 21:29, 882 lines) is the parent the design handoff cites (design/IRONICS_WEB_DESIGN_HANDOFF.md L6 'Parent: IRONICS_GTM_PLATFORM_ROADMAP.md (Phase W4)'). The brief's Glob was scoped to C:\Dev, so it calls it a phantom and proposes R3 'author the missing parent'. The platform code was built to its phase names: apps/api/README.md L1 'W1 portal API', L80/L203/L268/L277 'W5 admin console' / 'W2 download'; root README.md L8; apps/web/lib/stats.ts L2 'W1 Portal API spec'. It is committed to NO repo (git log -S GTM_PLATFORM in Ironics-Platform hits only the handoff reference) -- a doc-truth item in itself.
- GTM PHASE W5 'BETA CONTROL CENTER' (Downloads GTM L617-640) is a fuller admin-dashboard scope than BPD s12: panels TESTERS / POPULATION / FLEET / BUILD (current, minBuild, published) / HEALTH (crash-free, login, API 5xx, p50 fill) / FEEDBACK + MOD QUEUE; actions APPROVE, BULK APPROVE N FROM WAITLIST, REVOKE, BAN, OPEN/CLOSE COHORT, PUBLISH BUILD, SET MINBUILD, BROADCAST; separate admin auth (not a player token), AuditLog on every mutation, typed confirmation for destructive actions, server-side throttled bulk approve; exit gate approve->email->portal flips->Discord role. The brief's admin candidate_scope, decisions (ADMIN AUTH MODEL / SCOPE ORDER) and 'BPD s12 is the ONLY written admin scope' claim never cite it.
- GTM §7 DISTRIBUTION (Downloads GTM L305-366) already rules on what D2 re-proposes: R2 zero-egress chosen (L316-323); §7.1 engine-native '-createreleaseversion / -generatepatch -basedonreleaseversion' patch paks IN SCOPE NOW with four stated traps (asset hygiene, retain release content forever, rebase every ~6-8 patches, IoStore .sig friction UE-207430); §7.2 launcher DEFERRED to RAMP-C review with three explicit re-open triggers (post-prune Shipping build >4 GB; patch cadence >1/week; downloaded->installed <90%) and an interim 'portal download + small patch applier' (L348-358); W2 (L509-523) specifies publish flow as operator script incl. POST /admin/builds, minBuild kill switch, rollback never deletes N. D2 (Docs/dist/D2_DOWNLOAD_DISTRIBUTION_SCOPE.md L10, L101) picks CloudFront over R2, lists UE-native patching as 'not evaluated' (L154) and scopes a launcher (Tier 2) without reconciling §7. The brief's LAUNCHER RULING and D2 fold-in cite only BPD L183-184 / D1 s0.
- GTM PHASES W3/W6/W7/W8/W9/W10 + §10/§13/§14/§15 are uninventoried plans that map onto the brief's workstreams: W3 in-client boot gate + heartbeat + server-sourced watermark + minBuild UPDATE_REQUIRED + F8 bug report + committed telemetry schema (L525-537 -- the origin of install_completed / first_match_completed in apps/api/src/lib/events.ts L17-26); W6 unattended ops alarms, daily Discord digest, runbooks, 48 h soak (L640-658); W7 Discord channel plan, bot role sync, Loops CRM segments incl. waitlist-as-launch-list, scripted content pipeline off the catalog, Steam coming-soon page (L660-688); W8 ramp RAMP-A..E at 10/50/250/600/1,000 registered with approvals throttled by the admin console (L690-707); W9 creator program with creator codes + watermark clearance as an audited admin action (L709-718); §10 North Star metrics table incl. 'installed -> first match completed >=70%' (L756-787); §13 legal checklist incl. versioned Beta Agreement, DPAs, cookie posture (L829-843); §14 owed decisions 1-10 (L844-857) several since resolved but never marked (domain=ironics.org, entity=C12 AI Gaming per apps/web/lib/legal.ts L8, age 13+, founder ladder, launcher deferral, web repo); §15 says phases mirror into Docs/LIVE_TRACKER.html under a 'P-PLATFORM' pillar (L859-861) -- never done, which is the root of the brief's 'zero platform rows in the tracker'.
- GTM §4.3-4.4 (Downloads GTM L161-223) answers part of the brief's FOUNDERS decision: founder ladder 100/300/1,000 LOCKED with atomic issuance, and the corollary 'founder numbers, beta badges, and launch rewards are marketplace catalog entitlements ... Founder badge = a catalog row + an asset + the Phase-2 Visors flow'. The live contradiction is therefore GTM vs Docs/design/IRONICS_PRICING_SSOT.md L119-121 ('no backend representation'), not merely Pricing SSOT vs portal; also §4.3's open identity-consolidation fork (PlayFab vs AWS/Cognito) is a standing decision the brief does not list.
- OPERATOR MUST-HAVES ALREADY ON RECORD: C:\Users\tabor\.claude\projects\C--Dev-Bag-Man\memory\project_website_admin_phase_scope.md (modified 2026-09-07T00:21Z) quotes the operator: admin needs (1) add admins + scoped roles, (2) wallet & economy controls, (3) game metrics + download/sign-up tracking & controls, (4) 'institutional grade AAA ... built on the existing stack, NOT a new vendor pile', plus the mandated process (grounding -> admin scope pass seeded with the must-haves -> options -> decisions -> mockups -> build). The brief's inventory states memory 'has no note on the admin console'; its ADMIN AUTH MODEL decision ('keep flat allowlist vs roles') is partly pre-answered (scoped roles are a must-have); its suggested_sequencing puts A1 account/beta admin first and A4 live-ops (wallet/economy) last, the inverse of the operator's named order.
- OPERATOR PLANNING DOCS THAT LIVE ONLY IN Downloads (no repo copy, not in the inventory): 'IRONICS AAA BUILD TODO.md' (1,823 B, 2026-09-02 00:17 -- 17-item operator build list incl. '16. Final Check Servers and Stores', '17. Release Packaging', 'Steam/Apple/Android/Xbox/PlayStation Explore other compatible options including VR', 'Platforms: Web, Mobile, Console, Devices (not all are necessary...)' -- directly relevant to the VR row / PLATFORMS list / public roadmap rows); IRONICS_DISPLAY_SYSTEM_SSOT.md (07-05, in-game player dashboard + store SSOT); 'IRONICS Design System Spec.html' (07-05). (C12_FUNDING_ROADMAP_FINAL.md there is a C12 Robotics hardware venture doc -- unrelated.)
- SECURITY NOTE IN Downloads: 'IMPORTANT Change the super admin pa.txt' (464 B, 2026-09-02 00:18). Not opened. Its title says a super-admin password must be changed; which system (PlayFab title 1A2077, AWS root, Epic dev portal, Cloudflare) belongs in the A0 baseline / A3 hardening lane and in the SECRETS control row. Absent from the brief.
- REELS LOCATION IS KNOWN: the operator-approved reels and loop are at C:\Users\tabor\Downloads\ -- MV_IRONICS_StartLoop_4K_v9_review.mp4 (09-01 15:22), 'MV_IRONICS_BR9_v3_review 60s.mp4', MV_IRONICS_ARCANEON_ProMod_review.mp4 (09-02), MV_IRONICS_Vert_BR36_9x16.mp4 (09-02, a vertical social cut), 'BR 36 IRONICS ST Base Chassis 60s.mp4', 'INFINEON Beta Ironics 60s.mp4', 'INFINEON Clip 1.mp4', '10s Shanty Town Still.mp4' (8 files); plus marketing PNGs there ('IRONICS YT Banner.png', 'Fanatics Ironics Logo.png', ICON_IRONICS_EXTRACTION_ZONE/HUB_CORE/MARKETPLACE_STORE.png). The brief says storage location unknown and 'D:\BagMan has no reels folder'. Still uncommitted/undocumented, but the REELS decision should cite this path.
- GAMELIFT LAUNCH QUESTIONNAIRES: C:\Dev\AmazonGameLift_LaunchQuestionnaire_BagMan_C12AI_COMPLETED.xlsx and Downloads AmazonGameLiftServersLaunchQuestionnaire.xlsx / AmazonGameLiftServersFleetIQLaunchQuestionnaire.xlsx (2026-06-20) -- a completed AWS launch questionnaire that holds the tester/CCU/geography forecast D2 decision 14 asks the operator to 'provide'. Not in the inventory.
- LEGAL DOCS IN FORCE ALSO PROMISE COHORTS/REVIEW: apps/web/lib/legal.ts L93 ('Beta access is granted in cohorts'), L171 ('how we tell you a cohort decision'), L175 ('Cohort planning -- deciding which builds go to whom'), L309 ('granted per cohort'). W1's truth pass lists only page copy. Rewriting legal text implies a versioned re-acceptance (apps/api/src/lib/terms.ts L11 CURRENT_TERMS_VERSION='beta-agreement-2026-08-01'; handlers/beta/accept-terms.ts L26) -- an unlisted decision. Further copy sites W1 omits: components/BetaApplication.tsx L296 ('Cohorts are built per platform'), BetaComplete.tsx L166, app/creators/page.tsx L33 ('reviewed weekly alongside beta cohorts'), app/portal/page.tsx L7 metadata, apps/api/README.md L41.
- THE $5/MONTH RULING TRIANGLE (gates W4 money surface and legal L3) is not in contradictions_and_stale or decisions: Docs/design/IRONICS_PRICING_SSOT.md §6 L241-312 and IRONICS_ECONOMY_SPEC.md L162-177 rule 'Battle Pass = $5/month REAL MONEY' as a recurring subscription ($5/mo, $30/yr, $10/qtr; catalog AFL.League.Monthly 5,000 V etc.), and PRICING_SSOT L398-401 makes recurring billing a legal gate; Docs/design/PX_STORE_BUILD_RULINGS.md D-1 L22 (08-30) says '$5/mo is the WEBSITE price of the Volts that buy [League terms]. One flow'; Docs/design/IRONICS_VOLTS_PURCHASE_ADMIN_SCOPE.md L11-13 (08-24) says 'there is no subscription lifecycle at the payment layer, no recurring-billing state machine'. Whether the website sells a recurring subscription (needs auto-renew terms + a billing engine) or one-off Volts is unresolved.
- STANDING RULINGS ON /market NOT INVENTORIED: Docs/design/IRONICS_CC_UI_HANDOFF.md R3.4 L104-116 -- 'The web at ironics.org/market is a TEMPLATE -- card structure only. The catalog rules every value ... stickers are credit-packs and must show no price', and the ruled product list (robot packs, slot SKUs, weapon credits, sticker credits, hand-cannon pairs, emblems, Battle Pass subscription, jewellery). Also Docs/design/IRONICS_CHARACTER_CREATOR_SSOT.md L33 binds the live ironics.org product promise ('There is no roster...'). The MARKET decision should cite these.
- BACKEND OPERATOR LEVERS NOT INVENTORIED (the actual A4 live-ops control set): C:\Dev\Bag_Man_Backend\scripts\* (setup-flexmatch.ts, setup-playfab-economy.ts, setup-playfab-oidc.ts, seed-population.ts incl. --purge, seed-facemasks/jewellery/handcannon-bundles, export-playfab-catalog.ts, verify-league-title/verify-jewellery-title/verify-manifest-coverage/verify-handcannon-ledger, canary-*.ts, jewellery-unreserve.ts, populate-eos-secret.mjs, prove-comms.mjs; package.json L13-28 npm targets) and Bag_Man_Backend/docs/{test-accounts.md (dev PlayFab accounts + client override), bundle-purchase-checklist.md, earn-endpoint-contract.md, resolve-identity-contract.md}.
- Docs/design/START_SCREEN_FLOW_PLAN.md (1735ddcc 2026-08-31; mock-gated start screen + Epic sign-in + route choice) L33-40 ties the game's sign-in to the portal's epic#<sub> spine and strikes email+password -- the game-side half of the identity spine the website's sign-in/portal depends on; not in the inventory. Same for Docs/design/IRONICS_MATCH_STAKING_SSOT.md and IRONICS_LEAGUE_ADVANCEMENT_SSOT.md (staking loop behind legal L1; League tiers the roadmap/market describe).
- BUILD-TIME FEATURE FLAGS AS AN ADMIN CONTROL ROW: apps/web/lib/flags.ts (STICKERS_ENABLED, SHOW_MOCK_MARKER default true, EPIC_ENABLED default false) + NEXT_PUBLIC_ANALYTICS_ENABLED (apps/web/lib/analytics.ts L63-65) -- every toggle is a rebuild + deploy through the red CI path; deploy-web.yml sets only NEXT_PUBLIC_STATS_URL (L174) and NEXT_PUBLIC_EPIC_ENABLED (L188). Not in admin_controls_today.
- A SECOND UNLOCATED PARENT: the 'W1 Portal API spec' the API code cites section-by-section (apps/web/lib/stats.ts L2 '§5.1'; apps/api/src/lib/admin.ts L23 '§5.4', L26 '§6'; founder.ts L24 '§7'; apps/api/README.md L277 '§9 exit gate') has different numbering from both the GTM (§4.4 founders, §14 decisions) and the design handoff (§5 = PAGES) and is on disk in neither repo nor Downloads under an obvious name.
- GTM §14 items 8-9 (playlist<->venue naming for DA_AFL_ShantyTown_*; keep/replace/drop L_Expanse) are game-side decisions still owed that touch what venues the public roadmap/press can name; GTM §14 item 10 (identity consolidation onto AWS) is a standing platform architecture fork. None are in decisions_needed.

**Wrong or unsupported in the first draft:**
- 'IRONICS_GTM_PLATFORM_ROADMAP.md genuinely does not exist' / 'the design brief's parent is a phantom' / R3 'author the parent the design handoff cites' -- WRONG. It exists at C:\Users\tabor\Downloads\IRONICS_GTM_PLATFORM_ROADMAP.md (v2.3, mtime 2026-08-26). The brief's Glob was limited to C:\Dev. R3 should be 'commit + reconcile the existing GTM v2.3', not 'author'.
- 'memory has no note on the admin console, founder tiers, beta program or website roadmap' -- WRONG. C:\Users\tabor\.claude\projects\C--Dev-Bag-Man\memory\project_website_admin_phase_scope.md (2026-09-07) records the operator's admin must-haves and the pivot process.
- 'BPD s12 ... the ONLY written admin-dashboard scope' -- WRONG. GTM Phase W5 Beta Control Center (Downloads GTM L617-640) is a fuller scope; apps/api/README.md L203/L277 and the code's 'W5 admin console' comments were built against it; BPD s12 L265 itself only points at 'the existing Volts purchase admin scope'.
- 'The plan itself has never existed as a doc' (beta programme, plans_state) and R3 'No website plan, beta-program plan or public-roadmap SSOT exists anywhere on disk' -- UNSUPPORTED. GTM §1-§3 (1,000 registered / 36 peak / 120 burst, funnel math, acquisition), W8 ramp stages and §10 metrics are that plan. Docs/reference/IRONICS_BETA_LAUNCH_ASSETS_ON_HAND.md L17-18 ('does not yet exist') is dated 2026-07-01, before GTM v1.
- 'D2 ... still valid: Yes as a proposal; it needs ... decision 13's cause corrected' and the LAUNCHER RULING decision framed against only BPD L183-184 / D1 s0 -- INCOMPLETE. D2 also contradicts GTM §7 (R2 chosen vs CloudFront; UE-native patch paks ruled IN SCOPE NOW vs D2 L154 'not evaluated'; launcher DEFERRED with three named re-open triggers vs D2 Tier 2). The fold-in should reconcile D2 against GTM §7/W2 before any Tier decision.
- Marketing capture assets 'no committed/documented storage location for the reels (delivered from a session scratchpad; D:\BagMan has no reels folder per lens 5)' and the REELS decision 'Where are the 8 approved matchplay reels stored' -- UNSUPPORTED as 'unknown'. Eight mp4s incl. the 4K start loop are in C:\Users\tabor\Downloads (dated 2026-09-01/02).
- Contradiction 'LIVE_TRACKER row 036 EOS-AUTH-C2 BLOCKED vs commit aa550a1d PROVEN LIVE' -- PARTLY WRONG. Docs/LIVE_TRACKER.html L779 already reads 'unblocked by EOS-AUTH-C2 (EAS sign-in PROVEN LIVE 2026-09-02)'; only an older carried-over row (L370 'the full EOS integration is open and C2 ...') still reads open. It is an intra-tracker inconsistency, not tracker-vs-commit. The row numbers '036/027/108/000/081' are not a field in the file (rows are t:/what:/remains: objects), so those citations are unverifiable as written; the LAN-only (L312-313), 'awaiting operator approval' (L819) and 'R2 upload' (L135) claims do hold.
- Beta programme plan undone item 'doors shipping Disabled' and decisions treating it as unruled -- OVERSTATED. Docs/Hub/IRONICS_LOBBY_HUB_SSOT.md L482 row 5 already records the answer 'Tournaments/Mini Games Disabled until a partition map exists'; Docs/Hub/IRONICS_LOBBY_HUB_TASKS.md L134 only shows it unchecked, i.e. unratified, not absent.
- 'Web CI has been red on main since 2026-08-25' -- MINOR. The first red main run of deploy-web.yml is 32677856733 at 2026-08-24T00:49Z (gh run list -w deploy-web.yml). The L4 cause itself is confirmed: run 33928400546 log shows 'logged in with an Account API Token', 'Uploaded ironics-web (4.77 sec)', then 'Authentication error [code: 10000]' on /zones/05d1bd7f.../workers/routes.
- Decision 'FOUNDERS: What does a Founder receive in-game (Pricing SSOT says nothing exists)' -- INCOMPLETE. GTM §4.3 corollary (Downloads GTM L176-178) already specifies founder badge = catalog entitlement via the Visors flow; the decision should be framed as GTM vs Pricing SSOT L119-121.
- Ironics-Platform 'no website, admin, roadmap, design or analytics commit since 2026-08-25' -- TRUE for git, but the design lane's own parent (GTM v2.3) was edited 2026-08-26 outside git, so 'nothing moved' understates it; the real gap is that the platform's planning docs are uncommitted.
- Every other line-cited claim I checked held on disk: apps/api/lib/api.ts L66-248 (21 routes, 8 /v1/admin/*); AdminConsole.tsx L26-28/46/62; admin.ts L145 revokeAccount and L226 listApplicationsForApproval with test-only callers (grep confirms); seed-first-admin.ts L20-22/L50 and no memory record of a run (grep IronicsAdmins in memory = 0 hits); apply.ts L92-131; events.ts L17-26; DownloadCard.tsx L39-55; handlers/stats.ts L37/L40 vs api.ts commonEnv (no BUILD_REV); data.ts L106-124/L155; roadmap/page.tsx L21/L56; press L104 and creators L34 href='#'; legal.ts L8; LEGAL_OPEN_ITEMS.md L67-95; apps/api/README.md L118/L275-289; web README known stubs; AWS-SETUP.md L18/L87-92/L286-306; crm README L22-23; D1 plan L3/L13/L20-22/L56-62/L92-95/L125-127; BPD L180-184/L232-286; D2 L3/L54/L57/L103-118/L120-165; VOLTS scope L3/L11-13/L73/L193-200; Pricing SSOT L119-121; economy-store R71; LOBBY_UPGRADE L50; ui-frontend L642-645; DOCTRINE B2 L118/G9 L109; ENGINE_DOCTRINE L47-49; verify_ship_provenance.ps1 L7/L89; cook-manifests README L7; releases/README L9-13 (git log -- releases = 9ff73c3d only); AWS_ARCHITECTURE.md L44 (36 lambda dirs incl. shared; 28 unique route paths in cdk); reconcile L14-30; registry.ts L178; kill-telemetry L135-149; S12 plan L3/L36-40; RUNBOOK L9-17; chat1.md L621/L707; HUB_SSOT L167; TASKS L878 AFL-3063; CC-X6 (IRONICS_CC_ROADMAP.html L548); Ironics-Platform HEAD b41313a clean == origin/main, no open PRs; Bag_Man.uproject VR entries (only OpenXREyeTracker, OpenXRHandTracking, SteamVR listed, all Enabled:false); live https://api.ironics.org/v1/stats re-fetched 2026-09-06 -> founders.issued=1, tier1 remaining 99, no testers/buildRev keys; tracker has no dates after 2026-09-03 and 1 'portal/website' hit.

---
*Generated from workflow wf_2dca726a-5d4 (5 gather lenses + planner + critic) without paraphrase; §0 corrections and the four appended decisions were added by the agent after reading the recovered SSOTs.*
