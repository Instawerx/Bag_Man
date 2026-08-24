# Volts purchase + admin console — SCOPE

Nothing authored. Payment provider not wired, no money moved, per the ruling.

Measured against `ironics-live` (`apps/api`) and `Bag_Man_Backend` on 2026-08-24.

---

## 0 · What the ruling simplifies

Cash App buys **Volts** on the website; League is bought **in-game with Volts**. One payment
destination. That removes a whole branch that was previously scoped: there is no subscription
lifecycle at the payment layer, no recurring-billing state machine, and no second currency path.

What remains at the payment layer is one operation: **mint N Volts to a game account, once, on manual
approval.** Everything downstream already exists and already spends Volts.

It also means the League SKU is an ordinary catalog row bought through the proven wallet path — not a
payment integration. That is a large reduction and it is the reason this scope is short.

---

## 1 · The admin console

### 1.1 What already exists — more than expected

`apps/api/src/lib/admin.ts` is real and tested (`apps/api/test/admin.test.ts`):

| Piece | State |
|---|---|
| `approveAccount(ddb, tables, runner, accountId, actor)` | transactional; conditional so a repeat cannot reassign a founder number |
| `revokeAccount(...)` | retires the number, burns every refresh-token family |
| `listApplicationsForApproval(...)` | queries a `status-created-index` GSI, **ascending by `createdAt`** — the queue already has a deterministic order |
| `auditItem(...)` → `t.auditLog` | append-only audit rows written **inside the same transaction** as the state change |
| `emit("approved", …)` | event after commit, never before |

So the queue, the transactional approve, and the audit trail are built. What does not exist is the
part that matters most.

### 1.2 What does not exist — and it is the blocker

**There is no admin HTTP surface at all.** `apps/api/src/handlers/` contains `auth/` and `beta/`.
No `admin/`. The library above is unreachable from outside the process.

**There is no role, scope, or admin claim anywhere.** Measured — the session token is:

```ts
type SessionClaims = { sub, iss, aud, iat, exp, status }
```

`status` is `APPLICANT | WAITLIST | APPROVED | SUSPENDED | BANNED` — an account-lifecycle state, not a
permission. `requireSession` is the only guard in `lib/session-guard.ts`.

**Consequence, stated plainly: today an ordinary approved player's JWT is byte-for-byte
indistinguishable from an administrator's.** There is no server-side fact that separates them. Any
`/admin/*` route added now would be protected by nothing but the URL.

`approveAccount` already takes an `actor` and writes it to the audit log — so the *shape* of
accountability exists, but nothing establishes who the actor is or whether they may act. The audit
log would faithfully record an unauthorised mint.

### 1.3 The auth question, which is first and not last

A console that mints currency is the highest-privilege surface in the product. Three models, and this
needs a ruling before any endpoint is written:

| Model | How it enforces | Cost | Risk |
|---|---|---|---|
| **A · Admin claim in the session JWT** | add `role` to `SessionClaims`; `requireAdmin` checks it | small; reuses the existing session path | a stolen admin session is a mint key for its 30-minute TTL; revocation already exists via refresh-family burn |
| **B · Separate admin identity + hardware MFA** | admin accounts are not player accounts; distinct token audience | larger; a second auth path to maintain | strongest separation — a compromised player account can never escalate |
| **C · No console; approval by signed CLI run by the operator** | no web surface exists to attack | smallest build | approval requires a workstation; no queue UI; does not scale past one operator |

**Recommendation: C now, A later.** The manual-approval volume at launch is small, the mint path can
be exercised and audited immediately, and it adds **zero** internet-facing privileged surface. Model A
becomes worth building when approval volume outgrows one person — and by then the mint path is proven,
so the console is only a UI over a mechanism that already works.

If a console is wanted immediately, it is **A plus** an IP allowlist and a second factor, and
`role` must be a server-side account attribute — never a client-supplied field, never derived from
email domain.

### 1.4 Minimum console surface (whichever model)

- **Pending payments queue** — claim code, amount, payer name, received time, status
- **Approve** → mints Volts through the signed backend path (§3)
- **Unattributed queue** — payments whose note matched no claim code; resolved by hand only
- **Audit view** — read-only over the existing append-only log

Every one of these is a *read* plus one *write*, and the write is §3.

---

## 2 · The claim code flow

As ruled. Nothing here contradicts what exists.

```
IRN-XXXXXX          Crockford base32 (no I, L, O, U — misreads are the failure mode)
row created         status = awaiting_payment, tied to accountId, BEFORE any money moves
TTL                 the row expires; an abandoned checkout does not linger as a claimable code
never auto-match    on payer name. Ever.
unmatched           -> the unattributed queue, resolved by hand
```

**Why the row is created before payment**, restated because it is the load-bearing part: the code must
already be bound to an account when the payment note arrives, or matching becomes guesswork over a
free-text field. Creating it after is how payer-name matching gets reinvented.

**States:** `awaiting_payment → paid_unconfirmed → approved → minted`, plus `expired` and
`unattributed`. `minted` is terminal and is what §3 keys idempotency on.

Crockford base32 also decodes case-insensitively and treats `0/O` and `1/I/L` as equivalent — so a
human transcribing a code from a phone screen into a payment note cannot produce a near-miss that
silently matches a *different* code.

---

## 3 · Volts minting

**Approval writes Volts through the same HMAC-signed backend path as every other currency movement.**
Never a direct PlayFab write from a web session — a web session holding a PlayFab title secret is the
same class of defect as a client naming its own currency.

The discipline is already proven twice in this codebase and should be copied, not re-derived:

```
mint      ConditionExpression on the CLAIM CODE row: status = 'approved' AND attribute_not_exists(MintedAt)
          SET MintedAt = :t, MintedVolts = :n
```

**Idempotent on the claim code, so a double-approve cannot mint twice.** Identical in shape to
`refund-purchase`'s per-sale claim and `grant-once`'s `attribute_not_exists(SeededAt)` — in both cases
the idempotency lives in the *write*, because a read-then-check loses the race two clicks create.

Two failure modes worth naming now:

1. **Mint succeeds, PlayFab credit fails.** Same shape as the refund path: compensate and roll the
   claim row back, or the operator sees `minted` with no Volts delivered. Fail closed.
2. **Approve succeeds, mint fails.** The row must not sit in `approved` looking done. Either the
   transition is part of the same conditional write, or `approved` is explicitly not terminal and the
   queue shows it as still owed.

---

## 4 · PlayFabId on the portal account — still the piece with no fallback

**Confirmed by reading the type.** `Account` in `apps/api/src/lib/accounts.ts`:

```ts
{ accountId, email?, emailVerified, status, ageVerifiedAt?, cohort?, founderNumber?,
  termsVersion?, termsAcceptedAt?, createdAt }
```

No `playFabId`. **A payment that cannot resolve to a game account has no destination**, and a minted
Volt with nowhere to go is worse than a refused payment.

**The join exists and does not need building.** The portal already writes a `providerLinks` row keyed
`epic#${sub}` (`lib/accounts.ts`), and the game anchors through PlayFab
`LoginWithOpenIdConnect` on connection `epic`, title `1A2077` — **the same Epic `sub` on both sides**.
So the resolution is:

```
portal accountId -> providerLinks epic#<sub> -> PlayFab LoginWithOpenIdConnect identity -> PlayFabId
```

What is missing is only the **persisted field and the write that fills it**, plus the fail-closed rule:
if `playFabId` is absent at approval time, the approval is **refused**, not queued optimistically. A
mint with no destination must be unrepresentable, not recoverable.

Open, and it is a real gap rather than a detail: an account that has linked email but **never linked
Epic** has no `sub`, therefore no PlayFabId, therefore no destination. That is a legitimate state and
the checkout must refuse it up front with a readable reason — not at approval time, after the player
has paid.

---

## 5 · AwaitingActivation as a fourth condition state

The existing condition states grant or refuse. `AwaitingActivation` sits between: the entitlement is
**owed** but not yet live.

**Grants fail closed on it.** A player in `AwaitingActivation` does not hold the thing. The state
exists so the difference between "paid, not yet activated" and "not entitled" is representable — today
they would be the same answer, and a support question about the difference would have no data behind
it.

It must be a first-class value in the condition enum, not a null or an absent row, for the reason
`bPlaceholderArt` was a field rather than an overload: a state that has to be inferred from the
*absence* of something else cannot be distinguished from a bug.

---

## Owed before any of this is authored

1. **The auth model** — A, B, or C. Nothing privileged should be written before this is ruled; it
   determines whether an HTTP surface exists at all.
2. **Volts-per-dollar** for the mint. The peg is `1 V = $0.001`, so $5 = 5,000 V — but whether
   purchased Volts carry a bonus at higher tiers is a pricing decision, not arithmetic.
3. **Whether checkout refuses an Epic-unlinked account** (recommended) or creates the claim row and
   blocks at approval (worse: the player has paid by then).
