# Runbook — Git history secret sanitize (filter-repo)  [STANDING PROCEDURE]

Reusable procedure for scrubbing a committed secret from git history. Instantiated here for the
2026-09-09 incident, but written to be the standing workflow for any future occurrence.

> **2026-09-09 execution — SUCCEEDED.** Steps 1–8 run against `C:\Bag_Man`. The AWS key ID was purged from
> the 20-commit unpushed range (scoped `--refs personal/main..main`, `python -m git_filter_repo`); AKIA
> occurrences in the pushed range = 0, placeholder present ×8, working tree byte-identical (`git diff` empty),
> code commit intact. Pushed `77dd8443..f2ec646f main -> main` — GitHub push-protection passed. Replacements
> file shredded. Local backup tag `pre-sanitize-2026-09-09` (old history, holds the key ID) retained for
> rollback until the operator OKs a local purge (`git tag -d` + reflog expire + gc). Rotation of the exposed
> key ID (IAM, §Rotation) remains the operator's call.

## Incident scope (measured, not assumed)

- **Repo:** `C:\Dev\Bag_Man` (game, Git LFS ~250 GB). Push target: `personal` → `Instawerx/Bag_Man.git`.
- **Secret:** one AWS **access key ID** `AKIAUM57FK…63HC` (masked). It is the *identifier*, not the 40-char
  secret access key (the secret half is **not** in history — confirmed by scan). Owner: IAM user
  `Lead_Developer_Ironics`; key is **Active**, last used today (this session's own `aws` calls).
- **Blast radius:** present in the tree of **11 commits** (`ae9eaaa3` … `146d0bde`), added across several
  decision/scope docs, all deleted by `cd193dac`. It is only in history, not the working tree.
- **Other secrets:** NONE. Scan for AKIA/ASIA, `BEGIN … PRIVATE KEY`, `aws_secret*`, GitHub PAT, Slack,
  Stripe, Google API keys across the 20 unpushed commits → the one key ID above is the only hit.
- **Divergence:** local `main` is **20 ahead / 0 behind** `personal/main`. None of the 20 commits are on the
  remote, so the rewrite only touches unpushed history → a normal push after, **no force over shared history.**
- **Tooling:** `git filter-repo` not on PATH; Python module `git_filter_repo` **is** installed → run via
  `python -m git_filter_repo`.

## Key-handling protocol (institutional)

1. The search term (the key ID) is **never typed into a command or printed**. It is extracted from git
   programmatically into a replacements file written to the **scratchpad (outside the repo, gitignored path)**.
2. The replacements file is **shredded** immediately after the rewrite.
3. The rewrite is **scoped to the unpushed range** (`personal/main..main`) so the deep 250 GB history is not
   walked and LFS pointer/binary blobs are not rewritten (the key never appears in them).
4. A **backup tag** is taken before the rewrite; rollback is a single `git reset --hard`.
5. **Rotation is decoupled and operator-timed** (see §Rotation) — the exposed value is a key *ID*, and the
   active key is the credential THIS session is using, so deactivating it mid-work would break tooling.

## Procedure (each step is a discrete command; nothing runs until you approve)

**0. Preconditions.** Working tree clean (`git status` empty — it is). No other process using the repo.

**1. Backup + provenance.**
```
git -C C:/Dev/Bag_Man tag pre-sanitize-2026-09-09 main
git -C C:/Dev/Bag_Man rev-parse main            # record the pre-rewrite HEAD
```

**2. Generate the replacements file securely (value never printed).**
```
# writes  <scratch>/replace.txt  containing exactly:  literal:<KEYID>==>***REMOVED-AWS-KEY-ID (rotated; see Secrets Manager)***
kid=$(git -C C:/Dev/Bag_Man show 4c9bd0b3:Docs/design/IRONICS_PLATFORM_ENGINEERING_PLAN.md | grep -oE 'AKIA[A-Z0-9]{16}' | head -1)
printf 'literal:%s==>***REMOVED-AWS-KEY-ID (rotated; see Secrets Manager)***\n' "$kid" > "<scratch>/replace.txt"
unset kid
```

**3. Rewrite — scoped to the unpushed commits only.**
```
cd C:/Dev/Bag_Man
python -m git_filter_repo --force --refs personal/main..main --replace-text "<scratch>/replace.txt"
```
- `--refs personal/main..main` limits the rewrite to the 20 unpushed commits (does not walk 250 GB of history).
- `--force` is required because this is the live working clone, not a fresh clone.
- filter-repo **removes remotes** after running (a safety feature) — re-added in step 4.
- If filter-repo refuses the partial range on this version, fallback is `--refs main` (rewrites all of `main`'s
  history; same end result, slower; still LFS-safe because the key isn't in any pointer/binary blob).

**4. Re-add remotes (filter-repo stripped them).**
```
git -C C:/Dev/Bag_Man remote add personal     https://github.com/Instawerx/Bag_Man.git
git -C C:/Dev/Bag_Man remote add personal-ssh  git@github.com:Instawerx/Bag_Man.git
git -C C:/Dev/Bag_Man fetch personal --quiet
```

**5. Verify (all must pass before pushing).**
```
# a) the key is gone from ALL reachable history:
git -C C:/Dev/Bag_Man log --all -p -S 'AKIAUM57FK' | grep -c AKIA          # expect 0
# b) working tree unchanged (docs were already deleted; rewrite only edits historical blobs):
git -C C:/Dev/Bag_Man status --porcelain                                    # expect empty
# c) my code change is byte-identical (only its SHA changed):
git -C C:/Dev/Bag_Man show <new-HEAD> --stat                                # 4 files, +109/-13, same as e3d07217
# d) the pre-strip build is unaffected (no source touched by the rewrite — docs only).
```

**6. Shred the replacements file.**
```
shred -u "<scratch>/replace.txt" 2>/dev/null || rm -f "<scratch>/replace.txt"
```

**7. Push clean history.**
```
git -C C:/Dev/Bag_Man push personal main          # fast-forward from personal/main; GitHub push-protection passes
```

**8. Confirm** GitHub accepts it (no secret-scanning block) and the code commit is on `personal/main`.

## Rotation (separate decision — operator-timed)

The exposed value is a key **ID**, not the secret access key, so it is not directly exploitable and this is
**not** an emergency. If your protocol nonetheless mandates rotation-on-exposure for `Lead_Developer_Ironics`
(your own PLAT-A05 plan already intends to retire it):
- **Create the replacement first**, or you cut off this session's AWS access mid-work (the old key is what my
  `aws` calls currently authenticate with).
- Order: create new key → update wherever it's consumed (this box's AWS profile / any launcher) → verify →
  deactivate → later delete the old. Purely an IAM operation; no history rewrite involved.
- I will **not** create, rotate, or enter any key material — that is an operator action per the security rules.

## Rollback

Anything looks wrong before step 7:
```
git -C C:/Dev/Bag_Man reset --hard pre-sanitize-2026-09-09
git -C C:/Dev/Bag_Man remote add personal https://github.com/Instawerx/Bag_Man.git   # if not re-added
```
`personal/main` is untouched until step 7, so rollback is purely local.

## What I need from you
Approve the procedure (steps 1–8). I execute 1–8; **you** handle any IAM rotation (§Rotation). Once approved
I'll also drop this file into the repo as `Docs/RUNBOOK_GIT_HISTORY_SECRET_SANITIZE.md` so it's the standing
workflow, not a one-off.
