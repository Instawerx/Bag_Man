# CC_BRANCH_AND_REVERT

**Runbook.** Branching, tagging, and revert procedure for the character creator programme.
**Date:** 2026-08-17 · **Basis:** HEAD `7e79535f`
**Applies to:** all lanes — AIK, Claude Code, Operator.
**Companions:** `IRONICS_CC_ROADMAP.html`, `IRONICS_CHARACTER_CREATOR_SSOT.md`

---

## 0 · The rule

**Trunk plus tags. No long-lived branches. No fork.**

Work lands on `personal/main`. Every phase is bracketed by an immutable tag. Reverting is a
`git revert` of a commit range, not a branch abandonment.

---

## 1 · Why not branches, and why not a fork

`.uasset` files are binary and LFS-tracked. **Git cannot three-way merge them.** When two
branches both touch `DA_AFL_CharacterPartMap`, there is no merge — you pick a side and
discard the other. A long-lived content branch does not isolate risk; it accumulates an
unresolvable merge.

The repo already demonstrates this. Two feature branches have sat at 2026-06-15 for two
months while trunk advanced to `7e79535f`:

- `personal/feat/phase0-characterid-resolver` — `8e459755`
- `personal/feat/phase1-bigsixx-character-axis` — `ecbbdee1`

Any content in those branches is now effectively unmergeable. They are archaeology, not
branches.

**A fork is worse.** 250 GB of LFS duplication, doubled storage billing, and every
integration becomes a cross-repo cherry-pick of binaries.

**Mechanical cost.** `git checkout` across branches with differing `.uasset` versions
triggers LFS materialization. On this repo, branch switching is a multi-minute operation.
Performing it with the editor open corrupts editor state.

### 1.1 The one case where a branch is correct

A **code-only spike** you might genuinely throw away — a persistence experiment in Stage B,
an alternate clamp implementation. Conditions, all of them:

- Touches `Source/` or `Plugins/` only. **Zero files under `Content/`.**
- Merged or deleted within one working day.
- Editor closed for every checkout in and out.

If a spike touches `Content/`, it does not get a branch. Do it on trunk and revert.

---

## 2 · Tagging

Tags are free, immutable, and already the established pattern — eleven exist
(`phase-0-complete`, `stage-1-complete`, `stage-2-complete`, and others). Extend the
pattern; do not replace it.

**Bracket every roadmap phase:**

```
cc-0-pre      tree clean, baseline verified, nothing authored yet
cc-0-done     phase proven, committed, pushed
cc-1-pre
cc-1-done
...
```

```bash
# before a phase — only ever on a clean tree
git status --porcelain          # MUST return zero lines
git tag -a cc-1-pre -m "CC-1 entry: literals untouched, CC-0 proven"
git push personal cc-1-pre

# after a phase, once the proof is green
git tag -a cc-1-done -m "CC-1: visor + emblem literals read from selection. 2-client proof."
git push personal cc-1-done
```

**A `-pre` tag on a dirty tree is worthless.** It captures the commit, not the working
state. Restoring it will silently discard or preserve uncommitted files depending on how
you restore, and you will not know which.

---

## 3 · Commit discipline

> **Never mix code and content in one commit.**

This is the highest-value rule in this runbook.

| Change type | Revert cost |
|---|---|
| C++ / Config only | Trivial |
| `.uasset` only | Heavy but mechanical — restores prior blobs wholesale |
| **Both in one commit** | **Neither can be reverted cleanly** |

Split every phase into at least two commits even when they land in the same hour:

```bash
git add Source/ Plugins/ Config/
git commit -m "CC-1 code: resolve slot-1 visor from FacemaskId"

git add Content/BagMan/Characters/Cosmetics/
git commit -m "CC-1 content: 28 X BPs, visor literal removed from UCS"
```

**Per-file staging.** Never `git add -A` on this repo. Stage explicitly, review
`git status` before every commit, and verify the push with the triple-hash check
(local HEAD, remote HEAD, `git log` confirmation).

**Remote asymmetry — this has bitten multiple times:**

| Repo | Live remote |
|---|---|
| `Bag_Man` (UE) | `personal/main` |
| `Bag_Man_Backend` | `origin/master` |

`origin` / `origin-ssh` on the UE repo point at the dead C12-Ai-Gaming org. **Nothing
pushes there.**

---

## 4 · Reverting

### 4.1 Single phase

```bash
git log --oneline cc-1-pre..HEAD          # what landed
git revert --no-commit <content-sha>
git revert --no-commit <code-sha>
git commit -m "Revert CC-1: <reason>"
```

Revert, do not reset. `git reset --hard` past a pushed commit rewrites shared history and
breaks every other clone.

### 4.2 Inspecting a tagged state without moving

```bash
git diff cc-1-pre --stat                  # what changed since
git show cc-1-pre:Config/DefaultEngine.ini
```

Prefer this to checking out a tag. Checkout triggers LFS materialization.

### 4.3 Full stage rollback

Editor closed. Then checkout the stage's `-pre` tag, build, and verify before doing
anything else.

---

## 5 · Where git protects nothing

This asymmetry is the most important thing in this document.

| Change | Reverts with git? |
|---|---|
| C++, Config, `.uasset` | **Yes** |
| `Bag_Man_Backend` source | **Yes** |
| **A deployed CDK stack** | **No** |
| **PlayFab catalog** (title `1A2077`) | **No — not in any repo** |

### 5.1 Deployed infrastructure

CC-3.4 (build blob schema) and CC-6.4 (mint-cap unwind) both deploy. Reverting the source
does not revert the deployed stack. Rollback is `git revert` **plus** `cdk deploy` — a
forward operation, not an undo.

Before any deploying phase:

1. Capture the current synthesized template and commit it:
   `cdk synth > cdk/snapshots/<stack>-pre-cc-N.json`
2. Confirm the redeploy path is tested, not assumed.

**Related banked trap:** `Environment` is one CloudFormation property. Declaring a variable
as an empty string in CDK silently reverts the live value on any deploy. Deployed state and
declared state drift silently; the snapshot is how you detect it.

### 5.2 PlayFab

Seeding or repricing catalog items leaves **no git trace at all**.

**Before the first economy write in CC-6: export the PlayFab catalog to a file and commit
it.** This gives a diffable record of a system that otherwise has none, and it is the only
way to later answer "what changed, and when."

Repeat the export after each economy change. Treat it as part of the commit, not an
afterthought.

---

## 6 · Verification

**A tag you have never tested reverting to is a hypothesis, not a safety net.**

| Boundary | Verification |
|---|---|
| Phase (`cc-N-done`) | Build + PIE |
| **Stage (A, B, C end)** | **Checkout the tag on a clean tree, build, cook, launch** |

Three full verifications across the programme is proportionate. Doing it at every phase is
not.

**Two-engine rule applies throughout.** Every D:-source session ends with a C: launcher
`LyraEditor` rebuild before the editor reopens — the two share the output path
`C:\Dev\Bag_Man\Binaries\Win64\` and the D: build overwrites launcher binaries. A client
connecting to a dedicated server must be the same engine and CL as that server.

---

## 7 · Blast radius by phase

| Phase | Touches | Revert difficulty |
|---|---|---|
| CC-0 | 1 data asset | Trivial |
| CC-1 | 28 BPs + C++ | Heavy but mechanical — **the argument for splitting commits** |
| CC-2 | Mostly C++ | Easy |
| CC-3 / CC-4 | C++ + **deployed backend** | **Forward-only on the backend side** |
| CC-5 | WidgetBPs + C++ | Moderate |
| CC-6 | Catalog + **PlayFab** | **Partly outside git** |
| CC-7 / CC-8 | New content + C++ | Moderate |

Stage A is safe to attempt and cheap to undo. **Reversibility stops being free at Stage B.**
Know that going in, not during.

---

## 8 · Preconditions before any phase starts

1. `git status --porcelain` returns **zero lines**.
2. The phase's `-pre` tag exists and is pushed.
3. Editor state matches the lane — AIK requires the editor open; Claude Code and Operator
   builds require it closed.
4. For deploying phases: CDK snapshot committed.
5. For economy phases: PlayFab catalog exported and committed.

**Condition 1 is not currently met.** The working tree holds 118 changed or untracked
entries. Until it is zero, the programme has no baseline and no revert story. That is
CC-0.4, and it blocks everything.
