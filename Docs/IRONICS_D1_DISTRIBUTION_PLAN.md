# IRONICS — D1 Distribution Plan (signed archive + website download)

**Status: IMPLEMENTED in the Ironics-Platform repo (2026-09-04), awaiting operator deploy.**
Delivered through the existing **`ironics.org` portal** (sign-up → age gate → approval → download),
NOT a green-field site — the earlier "no website" assumption was wrong; the platform already exists.
Code: `Instawerx/Ironics-Platform`, branch `feat/beta-auto-approval-and-download`
(commits `ba3f7e3` auto-approval, `ffb9829` download). `Docs/BUILD_PRUNE_DEPLOY_PLAN.md` remains the
design SSOT.

**What was built (the two gaps the operator named):**
- **Automatic approval** — `POST /v1/beta/apply` now runs the portal's own `approveAccount` on a
  genuine application (the design-handoff funnel `application_completed → approved`): a 13+ applicant
  is APPROVED + issued a Founder number instantly. It was the *only* approval path (no admin route is
  deployed). Reused §5.4, no rework.
- **Download at `/portal`** — `GET /v1/download/latest` (session-guarded, APPROVED-only) mints a
  short-TTL **S3 presigned** URL from a private (`block-all-public`) `ironics-releases` bucket via the
  live `latest.json` pointer; the `PortalStatus` download card shows version / size / SHA-256 + the
  SmartScreen "More info → Run anyway" guidance.

**D1 delivery choice (deviation from §11, on purpose):** presigned S3, **not** CloudFront-signed URLs
— the genuinely simplest live path (no distribution to propagate, no signing-key secret to provision),
bucket still private, client contract unchanged if CloudFront/OAC is layered in later.

**Go-live (operator, in order):** (1) merge the branch through the Lighthouse gate; (2)
`npm run deploy -w @ironics/api` (creates the bucket + route + widened BetaApply IAM; run `cdk diff`
first — synth needs Docker/esbuild); (3) `apps/api/scripts/publish-release.ps1 -Zip <0.1.0 zip>
-Version 0.1.0 -ProvenanceScript <game>/Tools/verify_ship_provenance.ps1` to upload the build + point
`latest.json`. Then the first authentic test: sign up → approved → download.

---

*Original proposal below (kept for the record; the green-field assumptions in it are superseded).*
Governs the *simplest live distribution path* (Track D1): host the packaged Shipping client behind
the website, gated by short-TTL signed URLs. **No patching. No launcher app. No Authenticode cert.**

## 0 · Constraints (operator-ruled 2026-09-04)
- **D1 only** — signed archive + website download. Not D2 (itch), D3 (Epic Store), D4 (patcher).
- **No code-signing certificate.** Windows SmartScreen "unknown publisher" warning is **accepted**;
  the download page must guide the user past it. ("Signed" here = SHA-256 integrity + CloudFront
  **signed URLs**, not Authenticode.)
- **Testing deferred to LAST.** Real QA (voice audio-proof, cooked live-flow lap, authentic play)
  happens *only* once a distributed package is downloadable — it does not gate any other phase.
- **Reuse the existing AWS estate.** Account `302659227808`, `us-east-1`, one CDK stack
  `BagManTentpoleStack`, HTTP API `bagman-tentpole-api` (`1xm466mdc5`). The download infra is a new
  construct on that stack — not a new account, not hand-rolled. (`docs/AWS_ARCHITECTURE.md` in the
  backend is the estate SSOT; keep it true in the same commit.)
- **Provenance is a hard gate** (BPD §11 step 0 / ENGINE_DOCTRINE §3): only a `UE5-CL-0`-verified
  D:-source build may be uploaded. Enforced in the publish script, refuses the launcher-built W2A1 beta.

## 1 · What already exists (done)
- Cooked, provenance-clean Shipping client staged at `D:\BagMan\StagedBuilds\Windows` (6.15 GB).
- Packaged artifact `D:\BagMan\releases\win64\0.1.0-beta\IRONICS-0.1.0-beta-Win64.zip` (4.05 GB,
  pdb-excluded, store-mode) + `.sha256` + `manifest.json`; ledger record committed (`9ff73c3d`).
- `Tools/verify_ship_provenance.ps1` — the mandatory pre-upload gate, validator-law proven.

## 2 · Release versioning (PROPOSAL — needs operator ruling)
`0.1.0-beta` in the current artifact is a **placeholder**. Proposed scheme:
- **`MAJOR.MINOR.PATCH`**, pre-1.0 (`0.x`) = pre-release. First public build = **`0.1.0`**, **channel `beta`**.
- **Immutable** releases: `releases/win64/<semver>/IRONICS-<semver>-Win64.zip`. Never overwrite a
  version; bump PATCH (`0.1.1`) per new build. `latest.json` is the ONLY mutable pointer (channel → semver).
- Filename drops the channel suffix (`IRONICS-0.1.0-Win64.zip`); channel lives in `manifest.json`/`latest.json`.
- **Decision for you:** confirm `0.1.0`/`beta`, or give the scheme you want (e.g. the `W2A1` code).
  On confirm I re-stamp the manifest + re-name the artifact (no re-cook — same bytes, provenance unchanged).

## 3 · Architecture (D1)
```
[operator publish]  provenance gate -> S3 put(object-locked) -> write latest.json
                                          |
UE Shipping client zip (4.05GB) -----> S3 releases bucket (PRIVATE, versioned, object-lock)
                                          |  (Origin Access Control -- bucket never public)
                                       CloudFront distribution (ranged GET, resumable)
                                          ^
website download page --- GET /download/latest (existing HTTP API) --> mint-url Lambda
   (version/size/SHA-256,          (returns a CloudFront SIGNED URL, TTL ~15 min)
    SmartScreen guidance)
```
Every element maps to BPD §11: private bucket + OAC (§11.2), signed-URL mint on the existing API
(§11.2), integrity UX (§11.3), CloudFront logs → metrics + WAF rate-limit on the mint (§11.4),
immutable layout a future patcher reuses (§11.5).

## 4 · Phases (each stops for its own gate; ✅=done, ⛔=needs operator)
- **D1.1 — Version + repackage** (mine): on the §2 ruling, re-stamp manifest, rename to the final
  semver, refresh sha256, update the committed ledger record. ~minutes.
- **D1.2 — Publish tooling** (mine): `Tools/publish_release.ps1` — provenance gate → `aws s3 cp`
  (object-lock, no ACL) → write/patch `latest.json` → verify round-trip. Idempotent; refuses on
  provenance fail or an existing-version overwrite. Dry-run mode first.
- **D1.3 — AWS download stack** ⛔ (CDK in Bag_Man_Backend; **deploy needs operator authorization**):
  new construct on `BagManTentpoleStack` — S3 releases bucket (private, versioned, object-lock,
  RETAIN + deletion-protection), CloudFront + OAC + a signing key-group, `GET /download/latest`
  route + `mint-download-url` Lambda (CloudFront signed URL, ~15 min TTL, WAF rate-limit). `cdk diff`
  + changeset `Replacement` probe (§8 trap) before any deploy. Update `docs/AWS_ARCHITECTURE.md` same commit.
- **D1.4 — Download page mockup** ⛔ (MOCKUP-FIRST doctrine): 1:1 mockup of the download page —
  version/size/SHA-256, the Download button (calls `/download/latest`), and the **SmartScreen
  walkthrough** ("More info → Run anyway", why it is unsigned, the hash to verify). Brand-locked to
  `Docs/Hub/IRONICS_CC_DESIGN_BRIEF.md` §0. **Operator approves the mock before any page is built.**
- **D1.5 — Website download page** ⛔ (**BLOCKED: where is the website?** see §6): build the approved
  page in the website repo, wire it to the mint endpoint, ship the SmartScreen guidance + hash.
- **D1.6 — First publish + smoke** (mine, then operator): publish `0.1.0` via D1.2, fetch a signed
  URL, download on a clean machine, checksum-verify, launch. This is the first **authentic** test —
  the deferred QA (voice audio-proof, cooked live-flow lap) rides on this download.

## 5 · Security review (carry into implementation)
- Bucket is **never public** — OAC only; no bucket ACLs, no public-read. Verified post-deploy.
- Signed URLs are **short-TTL** (~15 min) and per-request; the CloudFront private key lives in Secrets
  Manager (created empty, populated out-of-band — same discipline as the 3 existing secrets; never in
  source/CFN state). The Lambda reads it at runtime, never logs it.
- **No game/economy secret touches this path.** The download mint is its own trust surface; do not
  reuse the earn/tentpole HMAC keys.
- Optional EOS sign-in gate on the mint (§11.2): ties downloads to accounts. **Decision for you** —
  recommend OFF for open beta (frictionless), ON later. Either way the bucket stays private.
- Rate-limit the mint (WAF) so the endpoint cannot be scraped into a public mirror.
- The unsigned-binary reality is disclosed honestly on the page + the published SHA-256 is the user's
  own integrity check — the mitigation for no Authenticode.

## 6 · Gates / open decisions (need you)
1. **Website location.** No website repo/hosting is present locally or referenced in the docs. Where
   is "the existing website" — a repo I should be given, an existing host/domain, or is the download
   page the first thing on a green-field site? D1.4/D1.5 are blocked on this.
2. **AWS deploy authorization + cost.** D1.3 stands up S3 + CloudFront + a Lambda + WAF in the live
   account. Near-zero at rest (per BPD §11.4 / arch §6) but not free (CloudFront egress scales with
   downloads). Approve standing it up? (I will `cdk diff` + changeset-probe and show you before deploy.)
3. **Release semver** (§2) — confirm `0.1.0`/`beta` or give the scheme.
4. **EOS download-gate** (§5) — open beta OFF (recommended) or account-gated ON?

## 7 · Explicitly NOT in D1 (deferred, per ruling)
Delta patching (D4), a launcher app, Authenticode signing, itch/Epic channels (D2/D3), the admin/
live-ops website surfaces (BPD §12 — sequenced after D1 proves), and all authentic-play QA (last).
