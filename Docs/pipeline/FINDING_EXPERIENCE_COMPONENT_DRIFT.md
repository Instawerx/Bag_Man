# Finding — experience component drift, and two map premises that did not hold

Measured 2026-08-24 by reading every `LyraExperienceActionSet` and every `B_*Experience*` CDO through
one reader, with a control.

## The reader had to be fixed first

The first pass reported **NO for every component on every experience** — including
`LAS_AFL_ExtractionMatch`, which demonstrably carries them. 100% of results in one bucket is a broken
instrument, not a finding.

Cause: these properties are **PascalCase** in Python — `Actions`, `ComponentList`, `ComponentClass`.
`get_editor_property('component_list')` *raises*, and a `try/except` turned that into an empty list,
which reads exactly like "grants nothing". A confident null.

The corrected reader runs `LAS_AFL_ExtractionMatch` as a control every time. If the control comes back
empty, every other answer is void.

## What each map actually uses

| Map | Experience (World Settings) | `AFLPlayerIdentityComponent` | `AFLAccessoryPartComponent` |
|---|---|---|---|
| `L_IRONICS_Armory` | **`B_LyraFrontEnd_Experience`** | YES | **NO** |
| `L_Expanse` | `B_Experience_ProMod` | YES | YES |

**The armory map does not use `B_IRONICS_Armory_Experience`.** It runs the front-end experience. And
that experience lacks the accessory consumer, so an accessory proof run there would render nothing
with nothing in the accessory path at fault — the render-only suspect, pointed at the wrong map.

(`B_IRONICS_Armory_Experience` itself is worse for this purpose: it lacks *both* the identity
component and the consumer.)

**`L_Expanse` carries no stale list.** It pulls the shared `LAS_AFL_ExtractionMatch` action set — 28
entries, identity and accessory both present. Corroborated independently: the accessory proof passes
20/21 there, which is impossible if the consumer were dropped.

## The drift defect is real, on a different asset

`B_AFLExperience_Arena01_Extract4v4` does not pull `LAS_AFL_ExtractionMatch` at all. It carries **28
inlined direct entries** — a snapshot of the action set taken before `AFLAccessoryPartComponent`
existed — and is missing the consumer. Accessories will not render in that experience.

Two fixes, and the choice matters:
1. **Root** — repoint it at `LAS_AFL_ExtractionMatch` so it cannot drift again. Riskier: the inlined
   list may differ from the action set deliberately in ways not yet diffed.
2. **Minimal** — add the one missing entry. Safe, and leaves the drift mechanism in place.

Not changed here. It is a different asset from the one the ruling named, so it gets a decision rather
than an assumption.

## Related gap

`B_LyraFrontEnd_Experience` lacks `AFLAccessoryPartComponent`. That is why jewellery cannot be
previewed in the armory or the creator — the map where a player would most want to look at it.
Worth its own commit, and not folded into the accessory work.
