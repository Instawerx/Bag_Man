# IRONICS sign-in — client runbook (Identity Program I-2 / I-3, 2026-09-15)

The Landing card has three doors. All of them end in the same place: a PlayFab player logged in
through the `ironics` OpenID connection (issuer `https://api.ironics.org`, `sub` = the portal
account id), then WHERE TO?. Contract: `Ironics-Platform/apps/api/docs/identity-i1-contract.md`.

| Door | What happens | Where it can refuse (the card names it) |
|---|---|---|
| EMAIL → code | `code-start` (202 + challenge id) → mail from no-reply@ironics.org → `code-verify` → game id_token → `LoginWithOpenIdConnect(ironics, CreateAccount=false)`; NOT-FOUND → create only if the portal knows of no player and no Epic link, else "sign in with Epic once" | rate limit, wrong/expired code, banned, "linked to Epic" |
| SIGN IN WITH EPIC | the existing EAS → PlayFab(`epic`) login, THEN `/v1/auth/epic/exchange` + `LinkOpenIdConnect(ironics)` on the Epic player (so the email door lands on this same player later), then `/welcome` | Epic refusal (EOS code), exchange refused (logged, sign-in still succeeds) |
| PLAY NOW | device credential (minted once, DPAPI-sealed) → `guest/login` → `LoginWithOpenIdConnect(ironics, CreateAccount=true)` | `GUEST_UPGRADED` (the guest was linked; credential retired) |

STAY SIGNED IN keeps the portal refresh token sealed in `%LOCALAPPDATA%\..\IRONICS\identity.bin`
(DPAPI, this Windows user only). Next launch: `TryResumeGameSession` → `/v1/auth/refresh` → PlayFab,
no card. SIGN OUT revokes the token and forgets it; the guest device credential is kept on purpose.

LINK ACCOUNT (System Menu, guests): the same card in link mode. Email → the address is attached
to the guest's account (same PlayFab player, nothing lost). Epic → EAS sign-in, then the Epic subject is
attached to the guest's portal account AND `LinkOpenIdConnect(epic)` on the guest's player.

LINK EMAIL (System Menu, Epic-first accounts with no verified address — I-5a): the same card with the
Epic door collapsed; only an address can be added. The row hides once the portal reports `emailVerified`.

SELF-MERGE (I-5a, both rows above): if the typed address belongs to an EMAIL-ONLY site account (applied
on ironics.org, never launched the game: one `email#` link, no player, no Epic, no game session), that
site account FOLDS INTO the signed-in game account — approval, founder number, entitlements, application,
terms, age and the address move; the site row is retired (`SUSPENDED`, `mergedInto`); the portal writes an
`account.merged` audit row and mails the address a notice. The response carries `merged:true` and the card
says so. Anything else (the address has played on another account, holds Epic, is a guest, two founder
numbers, a banned survivor) is `409 IDENTITY_CONFLICT` and nothing moves ("refused, never moved"). The notice
mail to the absorbed address carries a 7-day, single-use UNDO link (confirm page, then one click) that
puts both accounts back exactly as they were — the guest becomes a guest again and its device credential works.

## Driving it without a click (dev builds only)

```
afl.Identity.PlayNow                 # guest -> PlayFab -> route choice
afl.Identity.Resume                  # stored refresh token -> PlayFab
afl.Identity.EmailStart you@x.com    # sends the code, keeps the challenge id
afl.Identity.EmailVerify 123456      # verifies with the kept challenge id
afl.Identity.Logout
```

Autonomous proof from the log (editor DOWN; the game and a build never run together on this machine):

```powershell
& "$scratch\run_identity_proof.ps1" -Door PlayNow
& "$scratch\run_identity_proof.ps1" -Door Resume
& "$scratch\run_identity_proof.ps1" -Door EmailStart -Email you@x.com   # then read the code from the inbox
& "$scratch\run_identity_proof.ps1" -Door EmailVerify -Code 123456
```

Each run prints ✓ per expected log line and a VERDICT. The lines that matter:
`[AFLOnline] portal /v1/auth/... -> http=...`, `portal session accepted account=... guest=...`,
`LoginWithOpenIdConnect -> ... connectionId=ironics, createAccount=...`, `OK PlayFabId=...`,
`AFL_LANDING: signed in -> route choice`, `[AFLOnline] welcome -> http=200 ... identity=ironics portal=stored`.

## What the operator watches in PIE (the ✅)

1. Cold boot: the card, the rolling neon border, ESC opens the System Menu.
2. PLAY NOW: WHERE TO? within seconds; System Menu says "Playing as guest"; LINK ACCOUNT is there.
3. LINK ACCOUNT → email: code arrives, card says "Linked…", System Menu no longer says guest; the
   wallet keeps its Watts.
4. EMAIL on a fresh account: code → WHERE TO?; the same email then signs into ironics.org.
5. SIGN IN WITH EPIC (vrdivebar): unchanged; log shows `LinkOpenIdConnect(ironics) -> linked` once and
   `already` on later logins; the portal row gets `playFabId` from `/welcome`.
6. Quit, relaunch with STAY SIGNED IN: no card, straight to WHERE TO?.
7. LINK EMAIL (I-5a, 0.2.1): signed in with Epic on an account with no verified address → System Menu
   shows LINK EMAIL (not LINK ACCOUNT); the card has no Epic door; typing the address of a site-only
   application → "Linked. Your site account folded into this one…"; the row is gone on the next System
   Menu open; ironics.org signs into the same account with that address and shows the founder number.

Config: `[AFL.Online] PortalApiBaseUrl` (DefaultGame.ini), `afl.Online.IronicsOidcConnectionId=ironics`
(DefaultEngine.ini). The neon border is `/Game/BagMan/UI/Materials/M_AFL_NeonBorder`
(authored by `Tools/ui/author_neon_border_material.py`; the card falls back to a static hairline without it).
