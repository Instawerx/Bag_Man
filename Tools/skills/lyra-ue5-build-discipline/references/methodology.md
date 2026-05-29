# Methodology — the sprint loop, worked examples, decision trees

This document expands the Sprint Loop section of SKILL.md with three worked examples from the BAG MAN rebuild, decision trees as plain prose (no Mermaid for portability), and a session-restart checklist.

## The Sprint Loop in detail

```
Phase A: READ
   Inventory the relevant code/data via grep/Read. Identify carry-forward vs net-new.
   Find working siblings if the symptom is "should work but doesn't."

   Failure mode: skipping to authoring because "the change is small." Small changes
   into unread code regularly cost full days when the change-set turned out wrong.

PLAN
   Name the single variable changing. Name the runtime gate that will earn the close.
   If the variable is "multiple," scope it down. If the gate is "compiles + commits,"
   redesign the gate — gates are runtime, not build-time.

EXECUTE
   Author the change. Compile cleanly (PowerShell UBT, not Live Coding for new UCLASS).
   Stop. Do not commit yet.

VERIFY
   Operator runs PIE or otherwise produces runtime evidence. Observes the named gate.
   Reports what they saw. If observed: gate passes. If not: diagnose, do not commit,
   do not bank.

BANK
   Commit with a message that names the runtime observation. Push. Update tracker.
   Write memory for any non-obvious finding (especially distinguishing experiments
   that overturned prior assumptions).
```

The loop's discipline shows up at the joints. Authoring without reading produces speculative fixes. Skipping to commit without runtime evidence produces false-pass banks. Banking without writing memory loses the next-time-it-happens recall. The joints are where prior builds failed; this loop is where the rebuild stayed honest.

## Worked example — BM-0102 (Pulse damage)

### The arc

**Initial misdiagnosis.** First read of `UGE_AFL_Damage_Pulse` looked broken in deep ways: direct `Override 18` Modifier on the `Damage` meta-attribute, no `Executions` array, no SetByCaller for source-side damage seed. Initial conclusion: "the damage path needs a rewrite — author Source.Damage seed + Executions + new ability ctor pattern."

**Pivot to working-sibling-diff (Pillar 3).** The repo had a working sibling: `GE_AFL_Damage_Instant`, used by `UAFLGameplayAbility_DamageTest` for the smoke test. Diffing the two GEs:

- `GE_AFL_Damage_Instant`: `Executions = [UAFLDamageExecCalc]`, no direct Modifier
- `GE_AFL_Damage_Pulse`: direct `Override 18` Modifier, empty Executions

The diff was the misconfig. The fix wasn't a rewrite — it was a one-GE-config swap to match the working sibling's pattern. The ability also needed to seed `Source.Damage` before applying the spec (mirroring `UAFLGameplayAbility_DamageTest::ActivateAbility`).

**Tight scoped fix.**
- `GE_AFL_Damage_Pulse`: drop the direct Modifier, add `Executions = [UAFLDamageExecCalc]`
- `UAFLAG_Laser_Pulse.h`: add `BaseDamage = 18.0f` UPROPERTY
- `UAFLAG_Laser_Pulse.cpp` `ServerApplyTargetData`: seed `Source.Damage` before `MakeOutgoingSpec`, set Headshot/Weakpoint/Distance SetByCallers to 1.0 explicitly

Also needed: a damage sink that wasn't an unpossessed `ALyraCharacter` (which would crash via BM-DEBT-004's CastChecked). Authored `AAFLDamageTarget` — a self-contained `AActor + IAbilitySystemInterface` with its own ASC and `UAFLAttributeSet_Combat`, mirroring `ALyraCharacterWithAbilities`.

**Verification gate (Pillar 1).** Multi-shot test: fire Pulse 4× at the damage target. Watch verbose `LogGameplayEffects` for `InternalUpdateNumericalAttribute Health` lines. Expected and observed:
- Shot 1: `Health OldValue = 100.00  NewValue = 82.00`
- Shot 2: `Health OldValue = 82.00  NewValue = 64.00`
- Shot 3: `Health OldValue = 64.00  NewValue = 46.00`
- Shot 4: `Health OldValue = 46.00  NewValue = 28.00`

Exactly -18/shot. No accumulation (verifies per-shot Override re-seed of Source.Damage works as designed).

### Lessons

- **Pillar 3 saved days.** "Rewrite damage path" → "match working sibling's GE shape" is a 1000× cost reduction.
- The visible-cube trap: `/Game/Effects/Meshes/Cube.Cube` is 1.12cm (FX-sprite mesh, not a real cube). Use `/Engine/BasicShapes/Cube.Cube` (100cm) for level placements. Banked as a trap, not lost in the sprint's noise.
- Reclassified two prior debt tickets: BM-DEBT-003 (Instant+TargetTagsGameplayEffectComponent warning) was cosmetic-listener debt, not damage-blocking. BM-DEBT-007 (hypothetical "damage path needs rewrite") was a misconfig, not a rewrite — closed as never-needed.

## Worked example — BM-DEBT-AUDIT-001 (ASC-lookup audit)

### The arc

**Three sequential sprints surfaced the same bug shape.**

- BM-0102c: experience-side AddAbilities action targeted LyraPlayerState (worked); AFLCombat plugin-side AddAbilities targeted LyraCharacter (didn't deliver). Tracked as BM-DEBT-005.
- BM-0103a (Beam heat): cheat path worked, but `AFL.Combat.GrantBeam` lookup also went through the same pawn-vs-PlayerState ambiguity. Worked by luck (or partial-success window).
- BM-0104 (Pulse tuning): Gate B `AFL.Combat.SetSpread` returned `FAIL — no live UAFLAG_Laser_Pulse instance` despite Pulse demonstrably activating. Diagnosed as `Cheats.cpp`'s `FindPlayerASCFromAnyWorld` and `GetPlayerASC` both calling `Pawn->FindComponentByClass<UAbilitySystemComponent>()`. Tracked as BM-DEBT-008.

**Three instances of the same wrong-assumption (Pillar 4).** Audit triggered.

**Audit scope.**
- Inventory all ASC lookup sites in AFLCombat: 8 found, 5 correct, 3 broken (all pawn-FindComponentByClass).
- Inventory AFLCore: zero lookups (clean by absence).
- Inventory AFLMovement: one lookup, correct (uses `UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerPawn)` with pawn-extension lifecycle retry).

**Unified fix.** All three broken sites rewritten to `PC → PC->PlayerState → UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS)`. Plus the BM-DEBT-005 plugin-side ActorClass changed from `LyraCharacter` to `LyraPlayerState`.

**Verification gate.** PIE on `L_BagMan_Greybox`:
- Grant verification: GiveAbility lines for AFL abilities on LyraPlayerState_0 (achieved)
- Cheat verification: `AFL.Combat.SetSpread 5 20 2` returns `OK SetSpread` (achieved)
- Cheat-payoff verification: subsequent Pulse fire shows `Bloom base=5.00 max=20.00` with math-consistent `cur=7.00` (clamp + per-shot bump). Math-consistency is the BM-DEBT-008 closure proof.

### Lessons

- **Pillar 4 paid off twice.** The fourth broken site (test base) was caught by the audit's inventory step — a per-sprint fix would have missed it for months.
- **Pillar 5 caught itself.** The audit's "1× GiveAbility per ability" verification was BOTH consistent with "two paths delivering, Lyra de-dupes" AND "only one path delivering." The convenient interpretation was banked. The followup-prune sprint constructed the distinguishing experiment and proved the inconvenient interpretation was true.
- Hidden-symptom finding: cheat previously required "fire Pulse once first." Post-fix it doesn't — that requirement was a SYMPTOM of the broken ASC lookup, not a real ability-lifecycle constraint.

## Worked example — BM-DEBT-005-followup-prune (the corrected interpretation)

### The arc

**Premise to test.** The BM-DEBT-AUDIT-001 close documented that plugin-side + experience-side actions both target LyraPlayerState, both reference the same AbilitySet, and produce 1× GiveAbility per ability. Interpretation: "Lyra de-dupes idempotent grants — the experience-side workaround is now redundant, safe to prune."

**Distinguishing experiment (Pillar 5).** Remove the experience-side action; observe whether grants survive. If both paths deliver and Lyra de-dupes → grants still appear (1×). If only experience-side delivered → grants disappear entirely.

**Execution.**
- Pruned `Actions[1]` from `B_Experience_BagMan` via Python `set_editor_property` on the BP CDO, with `BlueprintEditorLibrary.compile_blueprint(bp)` to propagate into the BlueprintGeneratedClass, then `save_asset`. PIE.
- Result: **ZERO AFL ability grants.** ShooterCore Hero kit (Jump/Death/Dash/etc.) still granted. AFLCombat reached `Active` cleanly per LogGameFeatures. But no `GiveAbility AFLAG_Laser_Pulse / Beam / BP_GA_AFL_Damage_Test` lines.

**Recovery.** Python re-author of the action: `new_object(action_class_from_template, outer=cdo)` + read template `AbilitiesList` from AFLCombat plugin's action + mutate the struct's `ActorClass` and `GrantedAbilitySets` in-place + assign to new action's `AbilitiesList` + append action to experience's `Actions` array + compile_blueprint + save_asset. Grants restored.

**Bank correction.** Memory at `project_bm_debt_audit_001_closed.md` corrected: the "Lyra de-dupes" claim was a misinterpretation. Only the experience-side action ever delivered. Plugin-side action's ActorClass fix is semantic-correctness only.

**New architectural-open-question logged.** `BM-PLUGIN-GRANT-LIFECYCLE` — why doesn't the semantically-correct plugin-side action deliver? Hypothesis (untested): timing gap between plugin Active transition and PlayerState spawn, or actor-extension scan misses pre-existing PlayerStates.

### Lessons

- **Pillar 5 in its purest form.** The convenient interpretation was wrong. The experiment was cheap (one PIE). The skill itself was corrected, not just the codebase.
- **Negative findings are bank-worthy.** A failed sprint produced a corrected understanding plus a logged architectural-open-question. That's more valuable than a successful prune would have been (no functional change, just lost a workaround).
- **MCP capability proof.** Python re-author of BP CDO action arrays works if: (a) class fetched from existing template via `.get_class()`, (b) struct mutation done in-place when the struct type has no Python binding, (c) `compile_blueprint` called before `save_asset`. Banked in `feedback_bp_reflection_silent_noop.md` and `project_bm_debt_005_followup_prune_failed.md`.

## Worked example — Pillar 4 in formation (orphan-component anti-pattern)

### The arc

**Pillar 4 has two modes.** The BM-DEBT-AUDIT-001 example above is Pillar 4 *in arrears* — three instances had already accumulated before the audit triggered. This example is Pillar 4 *in formation* — watching the count climb toward the threshold and pre-positioning the audit, so the third instance triggers a prepared response instead of a fresh discovery.

**The shape:** a `UActorComponent` subclass authored with a self-subscribe lifecycle (registers a `UGameplayMessageSubsystem` listener on `BeginPlay`) but with no `AddComponents` grant — so it never instantiates on a live actor, and the event it listens for broadcasts to no subscriber in PIE.

**Instance 1 (BM-0105b).** `UAFLPawnHitboxHistoryComponent` — the lag-comp snapshot publisher. Authored, self-registers with the lag-comp subsystem on BeginPlay if it has authority. But nothing granted it to the player pawn; it was made to work by carrying it in the `AAFLLagTestDummy` ctor (a test actor), with the player-pawn grant explicitly deferred. At the time this read as a one-off scoping choice, not a pattern.

**Instance 2 (BM-0106, commit ab683344).** `UAFLHitConfirmComponent` — listens for `Event.Damage.Confirmed`, fires camera shake + hitmarker. Authored complete, self-subscribes on BeginPlay. But a grep of `Content/**.uasset` + PawnData showed **zero references** — orphaned. The `Event.Damage.Confirmed` broadcast (from the ExecCalc) had no live subscriber in PIE. Fixed by adding an experience-side `AddComponents` entry targeting `LyraPlayerController`.

**The recognition.** Two instances of the identical shape — *authored component, self-subscribe lifecycle, no grant* — in two consecutive sprints. Not yet three (the audit threshold), but the pattern was now named and the count was tracked: `feedback_orphan_component_watch.md` banked the rule "two is coincidence; the third triggers an audit of ALL AFLCombat components for attach coverage."

**Pre-positioning the audit.** Rather than wait to rediscover the pattern on instance 3, the next-session runway (`project_next_session_s4_dismemberment.md`) flagged AFL-0402's `UAFLDismemberComponent` — S4's first new component — as the *likely* third instance, with the audit response pre-loaded: grep every AFLCombat `UActorComponent` subclass against Content + PawnData; zero hits outside source/tests = orphaned.

### Lessons

- **Pillar 4 in formation is cheaper than in arrears.** Naming the pattern at instance 2 and pre-loading the audit response means instance 3 triggers a prepared sweep instead of a from-scratch diagnosis. The cost of tracking a forming pattern is one memory note; the payoff is not rediscovering it.
- **The prior-build habit is the generator.** "Author a component, forget the grant" was a systemic habit, which is exactly why instances recur — every carry-forward component is a candidate. The audit (when it triggers) inventories the whole class, not just the sprint's component.
- **Two is the moment to write the rule, not the moment to audit.** Auditing at two would be premature (Pillar 4 says three); ignoring at two loses the recall. The discipline is to *name and count* at two, *audit* at three.

## Decision trees (prose form)

### Read or author?

When you encounter a "I need to do X" task, ask:

- Has anyone else solved a similar X in this codebase? If yes → READ that solution first. Diff it to your target. The diff is often the entire job.
- Is there carry-forward C++ that already covers part of X? If yes → READ that first. Don't assume it's bug-free, but don't assume it's missing either.
- Is X net-new, no prior art? Then scope authoring carefully. Most "net-new" tasks have at least partial prior art when you look.

The pull toward authoring is itself a signal to spend more time reading first.

### Sprint or audit?

When you encounter a bug, ask:

- Is this the first of its shape? → SPRINT to fix it individually.
- Is this the second? → Sprint, but note the shape in the tracker for next time.
- Is this the third or later? → AUDIT. Inventory ALL sites of this shape. Classify each (correct vs broken). Fix all broken ones with one canonical pattern. Verify the pattern with PIE on at least one of the sites.

The audit costs more upfront but finds sites the sprint never would have. For the BAG MAN rebuild, BM-DEBT-AUDIT-001 found a fourth broken site (in the test base) that no future sprint would naturally have touched.

### Bank or wait?

When considering whether to close a sprint, ask:

- Did an operator observe the gate at runtime? If no → NOT BANK. Run the PIE first.
- Was the gate observable on screen / in the log / via math-consistent runtime state? If no → the gate isn't the right gate. Redesign it.
- Are there alternative interpretations of what was observed? If yes → either run the distinguishing experiment now, or bank with explicit acknowledgement of the ambiguity ("consistent with X, but also with Y; pending distinguishing experiment").
- Half-observed? → NOT BANK. Stop. Complete the gate.

### Suspend a sprint or push through?

When a finding surfaces mid-sprint that's a DIFFERENT bug class:

- Is it blocking the current sprint's close? If no → log it as a separate debt ticket, defer. DO NOT conflate sprints.
- Is it blocking the current sprint's close, but small enough to fold in (one extra blackout, one extra gate)? → Operator decision. Default to splitting unless the extra scope is genuinely trivial.
- Is it blocking AND large? → Suspend the current sprint, fix the blocker first as its own sprint, then resume.

Example: BM-0104 Gate B exposed BM-DEBT-008. It was deferred to its own debt ticket; BM-0104 closed on Gates A + C; BM-DEBT-008 became BM-DEBT-AUDIT-001 the next sprint. Clean split.

### Trust an interpretation or construct a distinguishing experiment?

When the same evidence fits two interpretations:

- Is one interpretation much more convenient than the other? → Build the experiment. Convenience is the signal you need it.
- Is the experiment expensive to run? → Bank with explicit acknowledgement of the ambiguity; flag the deferred experiment.
- Is the experiment cheap (one PIE, one cheat command, one Python read-back)? → Run it. Always. Cheap experiments that prevent banked misinterpretations save days of debugging downstream.

## Session-restart checklist

When starting work in this project after any break:

1. **`git status --short`** — what's in the working tree? Anything uncommitted?
2. **`git log --oneline -N`** — what's the recent commit shape?
3. **Tracker audit against git history** — does the tracker accurately reflect what's been merged? If the tracker says BM-0104 ✅ but `git log` doesn't show a BM-0104 commit, the tracker is lying. Reconcile before planning the next sprint.
4. **Read any new auto-memory entries** — what was banked between the last session and now?
5. **Check for open architectural questions** — are any of them now blocking the next sprint? If yes, defer the next sprint until the question is investigated.

Trackers must not lie about where we are. The session-restart checklist is the discipline that keeps them honest.
