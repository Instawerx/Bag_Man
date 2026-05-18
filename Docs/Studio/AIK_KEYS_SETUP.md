# AIK API keys — per-developer setup

The Agent Integration Kit (AIK) plugin needs API keys for the agent backend
(Claude Code/Claude API) and any genAI connectors you use (Meshy, Tripo,
OpenRouter, etc.).

**These keys must never be committed to the repo.** They are per-developer
secrets and a single leaked key can be abused to drain credits or hit rate
limits on the whole team's accounts.

This project's `Config/DefaultAgentIntegrationKit.ini` is committed and only
contains non-secret settings (`bUseBetaChannel`, etc.). Your personal keys
go in a separate file that git ignores.

## Setup

### 1. Create your personal AIK key file

Copy the template and fill in your keys:

```
cp Config/UserAgentIntegrationKit.ini.example Config/UserAgentIntegrationKit.ini
```

Or create `Config/UserAgentIntegrationKit.ini` from scratch with this content:

```ini
[/Script/AgentIntegrationKit.ACPSettings]
MeshyApiKey=msy_your_real_meshy_key_here
TripoApiKey=tsk_your_real_tripo_key_here
; Add any other keys AIK expects (OpenRouterApiKey, etc.)
```

This file is gitignored by the `Config/User*.ini` rule in `.gitignore`.
Verify with `git check-ignore -v Config/UserAgentIntegrationKit.ini`.

### 2. Restart UE5

AIK reads its settings at editor startup. If the editor is running, restart
it so AIK picks up the new keys.

### 3. Verify

In UE5: Edit → Project Settings → Plugins → Agent Integration Kit. The
Meshy/Tripo fields should show your keys (or `***` masked).

## If `UserAgentIntegrationKit.ini` doesn't load

Some AIK versions don't honor the `User*.ini` cascade for plugin configs. If
your keys don't appear in AIK settings after restart, fall back to one of:

**Option A — Environment variables** (most portable across machines):

```
# Windows (PowerShell, permanent)
[System.Environment]::SetEnvironmentVariable("MESHY_API_KEY", "msy_...", "User")
[System.Environment]::SetEnvironmentVariable("TRIPO_API_KEY", "tsk_...", "User")

# Then restart UE5 so it inherits the new env
```

**Option B — Enter via AIK UI** (writes to your local `Saved/Config/` which
is already gitignored by UE convention):

1. Edit → Project Settings → Plugins → Agent Integration Kit
2. Type each key into the appropriate field
3. Click Save — values get stored under `Saved/Config/WindowsEditor/` which
   git already ignores.

## What to do if you accidentally commit a key

1. **Rotate immediately.** Revoke the leaked key at the vendor dashboard
   (Meshy: https://app.meshy.ai/settings/api-keys, Tripo similar) and issue
   a new one.
2. **Treat the old key as compromised** — even if the commit was unpushed,
   any process that read the working tree may have captured it.
3. **Remove from git history** if it was pushed: `git filter-repo` or BFG
   Repo-Cleaner. Force-push only after coordinating with the team.

## Why this matters

Even on a private repo:
- Repos get forked, leaked, or made public by mistake
- CI runners, backup tools, and external agents may scan tracked files
- The git history is forever — deleting a key from the latest commit
  doesn't remove it from history without rewriting

Treat plain-text keys in a committed file the same as posting them to a
public Slack channel.

## Related

- `.gitignore` line 56: `Config/User*.ini` — the rule that hides this file
- `Plugins/AgentIntegrationKit/Config/DefaultAgentIntegrationKit.ini` —
  plugin-shipped defaults, not your project's copy
- `Config/DefaultAgentIntegrationKit.ini` — the project's committed AIK
  config, must contain no secrets
