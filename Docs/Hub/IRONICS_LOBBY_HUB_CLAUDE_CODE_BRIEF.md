# IRONICS_LOBBY_HUB_CLAUDE_CODE_BRIEF

**Purpose:** how Claude Code runs this programme as fast as it safely can — worktrees for parallel
code lanes, subagents for fan-out, one editor for everything that needs an editor — without loops,
drift, binary conflicts, or a dirty tree. Drop into `Docs/Hub/` and reference from `CLAUDE.md`.
**Amended 2026-08-26:** Claude Code has a direct editor connection. AIK is a fallback, not a lane.
**Governing document:** `IRONICS_LOBBY_UX_FLOW_SSOT.md` — every ticket cites the node/edge it implements.
**Companions:** `IRONICS_LOBBY_HUB_SSOT.md` · `IRONICS_LOBBY_HUB_ROADMAP.html` · `IRONICS_LOBBY_HUB_TASKS.md` ·
`IRONICS_CC_INTEGRATION_PLAN.md` (Track C — creator; intent lock §1 governs every creator/loadout/store UI ticket).

---

## 1 · Lane model

| Lane | What it is | Runs where | Parallel-safe? |
|---|---|---|---|
| **CC-W** | Claude Code in a git **worktree** — C++, config, docs, harness scripts, log ledgers, HUB-READ passes | `C:\Dev\Bag_Man_wt\<branch>` (one folder per branch) | **Yes** — any number, disjoint file sets |
| **CC-E** | Claude Code through its **direct editor connection** — asset edits, actor placement, BP/DA/GE/MI authoring, PIE harness pre-arm, log reads after PIE, **Fab panel adds** (operator's Epic session) | The editor checkout `C:\Dev\Bag_Man` on the **C: launcher engine — never D:** | **No** — one editor, one CC-E session at a time |
| **CC-B** | Claude Code in `Bag_Man_Backend` — Lambdas, DynamoDB, CDK, unit tests, canaries | Separate repo | **Yes** — always |
| **Operator** | PIE watching · the editor-checkout UBT build · commits, pushes, tags · AWS console · Epic verification · §11 rulings | — | — |
| **Claude Design** | Visual layer from CC handoff docs — first job: `IRONICS_CC_DESIGN_BRIEF.md` §8 (creator / loadout / product page mock + spec); later Landing, HUD elements | — | Yes |
| **AIK** | Used **only** for an operation the editor connection cannot perform, as proven by the HUB-READ-0 probe (e.g. Niagara module internals, Tripo/genAI). Each use is named on its ticket. | Editor | No |

**The only serialising resource is the editor.** Everything else parallelises. Plan every phase so
CC-E work is a short, batched tail behind CC-W/CC-B work, not the critical path.

---

## 2 · Worktree model — how CC-W stays fast *and* clean on a UE/LFS project

### 2.1 Why worktrees and not clones

`git worktree add` shares the object store (no second 250 GB LFS copy) and gives each branch its own
working directory, `Intermediate/`, `Binaries/`, and `.uproject` — so two code sessions never race on
UHT generated headers or the same build output, and neither can dirty the editor checkout.

### 2.2 The rules

1. **Worktrees are for text.** C++, `.Build.cs`, `.uplugin`, `.ini`, `.md`, scripts, harness code.
   Create each worktree with sparse-checkout excluding binary content so it stays small and cannot
   hold a stray asset edit:
   ```
   git worktree add -b feature/hub-h1-spine C:\Dev\Bag_Man_wt\hub-h1-spine personal/main
   cd C:\Dev\Bag_Man_wt\hub-h1-spine
   git sparse-checkout init --cone
   git sparse-checkout set Source Config Plugins Docs Tools   # then exclude Content dirs:
   git sparse-checkout add !Content !Plugins/*/Content !Plugins/GameFeatures/*/Content
   git lfs install --skip-smudge                              # LFS pointers only in worktrees
   ```
   **[VERIFY at H0: the exact sparse pattern that leaves every `.Build.cs`/`.uplugin` present and
   every `Content/` absent — confirm with `git ls-files | rg '\.uasset$' | wc -l` → 0.]**
2. **Binary assets are edited in exactly one place**: the editor checkout, by CC-E, with the editor
   open. Never in a worktree. `.uasset/.umap` do not merge; there is nothing to reconcile if they only
   ever change in one tree.
3. **Builds in a worktree are editor-closed by construction** (no editor is ever opened on a worktree).
   Ruling requested (§7): Claude Code may run `UnrealBuildTool` / `Build.bat` for `LyraEditor` and
   `LyraServer` targets *inside a worktree*, by explicit engine path, to compile-verify a diff before
   handing it over. The editor-checkout build stays operator-owned; that is the build that the editor
   loads.
4. **Landing a worktree branch:** rebase on `personal/main` → push branch → operator merges (or CC
   fast-forwards `personal/main` if ruled) → the editor checkout pulls → operator rebuilds the editor
   checkout → CC-E continues. Code lands *before* the content that references it.
5. **One worktree per branch, one Claude Code session per worktree.** Remove it when the branch
   lands (`git worktree remove`). Never two sessions in one tree.
6. **Engine rule still holds — and it is the GameLift rule.** D: source exists because GameLift dedicated
   servers (`LyraServer`) and shipping cooks are built there. The editor, PIE, Fab, and every content
   commit happen on the **C: launcher** engine. Worktree builds use C: for `LyraEditor` and D: for
   `LyraServer`, invoked by explicit path. **Never launch the editor on D:.** A D: `LyraEditor` build is never
   run from a worktree either (it overwrites the C: editor binaries — same output-path trap).
   **[VERIFY: does a worktree's `Binaries/` isolate that trap? Output path is per-project-dir, so
   yes for the project; the engine-side binaries are shared. Confirm before the first D: worktree build.]**

### 2.3 What is safe to parallelise (and what is not)

| Safe in parallel | Not safe |
|---|---|
| Two CC-W sessions on disjoint file sets (e.g. `AFLHubNetProfileComponent` vs `AAFLHubZoneVolume`) | Two sessions touching the same header or the same `.ini` section |
| Any CC-W with any CC-B | Two CC-E sessions (one editor) |
| CC-W building in its worktree while CC-E edits assets in the editor checkout | A worktree build against the D: engine `LyraEditor` target (never) |
| Read-only subagent fan-out (HUB-READ passes) | A subagent that writes — writes stay in the parent session |
| CC-E editing assets while the operator builds *a worktree* | CC-E editing while the operator builds *the editor checkout* (editor must be closed) |

---

## 3 · Agents — where fan-out pays

Claude Code subagents are used for **reads and analysis**, never for writes:

- **HUB-READ passes:** one read-only subagent per acceptance criterion, each returning `fact + path:line`
  or `UNVERIFIED`; the parent reconciles into `Docs/Hub/HUB-READ-n.md`. Six ACs → six agents → one
  reconciled report in a fraction of the serial time.
- **Proven-Sibling diffs:** a subagent produces the structural diff of a new class against its cited
  proven sibling and lists divergences; the parent justifies or rejects each.
- **Regression grep:** at every gate, a subagent runs the §6 `git diff --stat` fences and the
  "no PlayFab/FlexMatch/GameLift symbol in `AFLHub`" grep and reports.
- **Log ledgers:** after PIE closes, a subagent per client log extracts the `AFL_HUB[...]`/`AFL_TEST[...]`
  markers; the parent builds the verdict table.

Writes are single-threaded per worktree. A subagent that would need to edit a file hands the edit
back to the parent.

---

## 4 · Session protocol — every session, every lane

```
0. READ    tracker rows for the phase; the ticket; SSOT §; the HUB-READ report the ticket cites.
1. GATE    print: lane (CC-W/CC-E/CC-B) · worktree path or "editor checkout" · HEAD ·
           git status --porcelain (must be empty) · engine · phase. Dirty → stop and report.
2. SCOPE   restate the ticket's ACs verbatim; the flow node/edge ids the ticket implements
           (IRONICS_LOBBY_UX_FLOW_SSOT.md); the lane's allowed file set (§2.3 / §5). For any
           creator/loadout/store UI ticket, print the intent IDs (I-n) each AC cites; an AC with
           none is deleted before work starts. Out = out.
3. WORK    CC-W: edit → (optional compile-verify in the worktree if ruled) → show full diffs → stop.
           CC-E: edit assets over the connection → save → list every asset touched with its path →
                 stop. Pre-arm the harness if the ticket has a PIE proof.
           CC-B: edit → run unit tests → show diffs → stop.
4. PROVE   spec the proof the operator runs: commands, harness command, expected markers.
5. LEDGER  after PIE closes: read logs (subagent fan-out), verdict per AC with path:line / log line.
6. TRACKER write the row (lane, hash-to-be, evidence). Commit plan: per-file list, expected count
           first, code / content / backend separated. Operator commits.
```

Reads before writes: every ticket that calls an existing system starts with `rg` of the symbols it
will call and quotes the lines. UNVERIFIED stops the session; it never guesses.

---

## 5 · Phase-by-phase session map

| Phase | CC-W worktrees (parallel) | CC-E (serial, batched) | CC-B |
|---|---|---|---|
| **H0** | `hub-h0-read` — subagent fan-out for READ-1/2/3; triage list for the operator ∥ `cc-c0-read` — CC-READ-5 true-state pass + divergence table + AFL-3202 skill-brand fix; then AFL-3203 bind-map verify of the Claude Design spec | HUB-READ-0 capability probe (scratch asset, reverted) · CC widget inspection for F1–F7 | READ-3 backend facts |
| **H1** | `hub-h1-spine` (plugin shell, net profile) ∥ `hub-h1-fireblock` (blocked tag ×N) ∥ `hub-h1-zones` (zone volume) ∥ `cc-c1-preview` (preview component, anchors, mirror) | **M1 import + migrate + census → M2 sanitise + rename + experience → M3 zone assignment (screenshots, proposal) → operator ratifies → M4 plaza + zone volumes.** These four editor sessions are the critical path and run in this order; the worktree code lands around them. | — |
| **H2** | `hub-h2-doors` (destination volume + data types) ∥ `hub-h2-return` (join subsystem, match-end branch) ∥ `cc-c2-kit` (hue arc, pickers, slot strip, action bar) → `cc-c3-creator` (shell) | Destination volumes ×11, `DA_AFL_HubDestinations` rows, Landing widget + backdrop sublevel, travel canary | — |
| **H3** | `hub-h3-retail` (pedestal, rack, channel) ∥ `cc-c4-loadout` ∥ `cc-c5-productpage` | Pedestal canary + pad MIs → racks → shells wired → cutover | — |
| **H4** | `hub-h4-chat` (chat + text filter — shared with C3 build-name filter) ∥ `hub-h4-clubs` (club component + mask) ∥ `hub-h4-party` (client side) ∥ `cc-c6-accessories` then `cc-c6-stickers` | Nameplates, chat widget, lounge door rows | `party-ticket` (start-matchmaking field) · EOS binding when C2 green |
| **H5** | `hub-h5-join` (resolver swap) ∥ `hub-h5-load` (headless client harness) ∥ `hub-h5-repgraph` (only if measured) | HISM / NavMesh / WP sizing pass | `hub-join` Lambda + `HubPresence` ∥ CDK hub fleet/queue |
| **H6** | `hub-h6-doors` | Partition canary placement, prompts, audio hooks | Assigned-match session supply |

H3, H4, H5 worktrees all run concurrently after the H2 gate. Their CC-E tails queue on the editor in
the order their code lands.

---

## 6 · Commit, branch, and regression discipline (unchanged, restated)

- Code and content never share a commit; product and instrument never share a commit; backend is its
  own repo and its own commits. Push to `personal/main` only; tags after push, measured claims only;
  triple-hash verify; clean tree before any D: session; D: sessions end with the C: `LyraEditor` rebuild.
- Regression pack at every gate from H1: 2-client match fire/damage/dismember/respawn proof; a solo
  FlexMatch match completes; `git diff --stat` fences on match-experience, matchmaking, and door paths
  print nothing; shipping cook still succeeds (H3+).
- Proof standard: watched in PIE on a controllable pawn; two clients where replication is claimed;
  dedicated server where join/travel is claimed. Never "compiles".

---

## 7 · Rulings — closed, plus two new ones requested

Closed (do not reopen): operator intent from `Lobby_Upgrade_Doc` · purchase via `ClientRequestPurchase` ·
Lyra hero pawn + net profile, no `AHubCharacter` · fail-closed fire tag · shards @64 with friend-follow
and a separate hub queue · no stub doors · stream-within / travel-at-seam · Replication Graph and
Linux/Graviton by measurement / separate programme.

**Requested (default proceeds if silent):**

| # | Ruling | Default |
|---|---|---|
| 6 | Claude Code may run UBT compile-verification **inside worktrees** (editor closed by construction). The editor-checkout build stays operator-owned. | Yes |
| 7 | Claude Code may fast-forward `personal/main` from a landed worktree branch after the operator has watched its proof, and pull it into the editor checkout. Commits themselves stay operator-signed per file list. | Operator merges; CC prepares |

If a session believes a ruling is wrong, it writes one paragraph on the tracker with the disk fact
that contradicts it and stops. It does not build the alternative.

---

## 8 · Traps this brief specifically guards against

- An asset edited in a worktree (binary conflict with no merge path). Sparse-checkout + skip-smudge
  make it impossible; the §4 GATE line prints the lane so it's visible.
- Two CC-E sessions racing the editor. One editor, one session; the session map queues them.
- Building the editor checkout while the editor is open. Only worktrees build during editor sessions.
- A D: `LyraEditor` build from anywhere but the sanctioned end-of-D:-session rebuild — and **never the editor launched on D:** (2026-08-27: it happened; recovery = close, C: launcher `LyraEditor` rebuild, relaunch on C:).
- A subagent that writes. Reads only.
- Content committed before the code it references has landed and been rebuilt into the editor.
- AIK used for something the connection can do — every AIK use names the probe failure it covers.
- Creator/loadout/store UI work with no intent ID, a stand-in preview, an RGB slider, a new widget
  family, or a seam bypass. Quarantined at the next gate, not merged.
- Everything already in the traps list of the SSOT §9 and the previous brief: second store instead
  of an entrance; direct PlayFab/FlexMatch/GameLift calls from hub code; fused evidence; invented
  paths; editing a match experience; single-client proofs of multi-client behaviour; mid-PIE log reads.

---

## 9 · First sessions — exact opening prompts

**Session A (CC-W, editor checkout with editor closed — tree hygiene):**
```
Read Docs/Hub/IRONICS_LOBBY_HUB_CLAUDE_CODE_BRIEF.md §2 and §4, then IRONICS_LOBBY_HUB_TASKS.md AFL-3001.
Print HEAD, git status --porcelain, git stash list, git worktree list. Author nothing.
Produce the AFL-3001 per-file triage list, expected count first, each entry marked
revert | gitignore | operator-decision. Stop for the operator's stash@{0} answer.
```

**Session B (after the tree is clean — create the read worktree and fan out):**
```
Read the brief §2.2 and §3. Create worktree hub-h0-read from personal/main with the sparse-checkout
recipe; confirm zero .uasset files are checked out. Execute AFL-3002 (HUB-READ-1) with one read-only
subagent per acceptance criterion; reconcile into Docs/Hub/HUB-READ-1.md with path:line or UNVERIFIED
per fact. Close SSOT §12 rows 1, 2, 7, 8 by quoting the fact under each. No other file changes. Stop.
```

**Session C (CC-E, editor open — capability probe, runs in parallel with B):**
```
Read the brief §1 and the HUB-READ-0 block in AFL-3002. Over the editor connection, on a scratch
asset that you delete afterwards, attempt each listed operation and record PASS/FAIL with the exact
call used. Output Docs/Hub/HUB-READ-0.md: the PASS list, the FAIL list, and the derived
"AIK-necessary operations" list (FAIL items only). Leave the project exactly as you found it. Stop.
```
