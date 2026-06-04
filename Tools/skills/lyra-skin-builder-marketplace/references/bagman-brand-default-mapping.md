# BAG MAN — Robot Brand-Default Color Mapping

> Durable spec for the per-robot **brand default** body×edge pairing. Authored
> 2026-06-04 (operator-specified). This is the **design intent** for the
> per-robot brand-default-binding feature (a future prove-then-commit cycle — it
> is NOT wired yet; see "Status" below). Lives here because the mapping otherwise
> existed only in a transient chat thread.

## The three axes (recap)

Robot skin = **mark × body-color × edge-color**, independent:
- **MARK** = which CharacterPart actor (`B_AFL_Robot_<name>`) / its LogoTexture.
- **BODY color** = `TeamColor` — from the robot's base team MI (`MI_<team>_Body_<color>`). The glow-only preset does NOT override it.
- **EDGE/glow color** = `EmissiveColor1-3` + `EdgeGlowColor` — from the applied glow-only `UAFLSkinColorAsset` preset (`/Game/BagMan/Characters/Cosmetics/SkinColors/DA_AFL_SkinColor_<color>`).

So a robot's appearance = (its base-MI body color) × (whichever preset its controller's `PersistentSkinColor` holds). Body and edge are chosen independently.

## Brand-default pairings (operator-specified, 2026-06-04)

| Robot (mark) | Body color (base MI) | Edge color (preset) |
|---|---|---|
| **ARIA** | Pink | **Purple** |
| **SCARLETT** | Purple | **Pink** |
| **IRONICS / Blue-base** | Blue | **Green** |
| **MAKHIAVELLI** | Green | **Blue** |
| **AP-9** | Red | **Green** |
| **MOB-FIGAZ** | Red | **Green** |

Edge presets resolve to: Purple=`DA_AFL_SkinColor_Purple`, Pink=`DA_AFL_SkinColor_Pink`,
Green=`DA_AFL_SkinColor_Green`, Blue=`DA_AFL_SkinColor_Blue`. (Red preset exists too
but no robot defaults to a Red edge in this mapping.)

The intent is **contrast pairings** (body ≠ edge), not monochrome — a body color with a
deliberately different edge color, the "Pink body + Purple edge" look the operator
approved as ideal.

## Status (honest)

- **CAPABILITY: PROVEN + shipped** (commit `f8088ebe`). Any robot wears any edge color,
  live + replicated on a 2-client wire. The body×edge independence is real.
- **PER-ROBOT BRAND-DEFAULT BINDING: NOT wired.** Currently ONE controller
  (`B_AFL_SkinColorController`) holds ONE shared edge default (committed = Purple, =
  ARIA's brand pairing). Auto-binding each robot to its brand edge on mark-change needs
  a mark→edge lookup (~15 lines + this map) + a wire-watch that it fires correctly
  across clients. That is its own prove-then-commit cycle (likely player-overridable —
  the player picks body + edge from two dropdowns, brand pairing is just the default).

## When you wire it

The lookup keys off the equipped mark (the spawned `B_AFL_Robot_<name>` class) → this
table → the matching `DA_AFL_SkinColor_<edge>` preset → `SetPersistentSkinColor`. Prove
on a 2-client wire (mark-change → correct brand edge on both clients) BEFORE committing.
Keep it separable from player-override (override wins over brand default).
