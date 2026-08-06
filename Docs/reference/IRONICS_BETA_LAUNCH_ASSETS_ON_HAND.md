# IRONICS -- BETA-LAUNCH ASSETS ON HAND (Procurement SSOT)

Single source of truth for the operator's PURCHASED assets that back specific Beta-launch
sections. This is the STEP-3 bank: the inventory, which section each serves, the version-
migration flags, and the grab protocol. It is cited BY the Beta-launch master plan; it does
not replace it.

---

## STATUS / CHARTER (read before acting)

- **GROUNDWORK ONLY.** This pass is read/plan. NOTHING here has been grabbed, imported, or
  built. The zips stay at `C:\Users\tabor\Downloads\` until their section is ready.
- **GRAB PROTOCOL is LAW** (see bottom): AIK ASKS the operator to confirm before grabbing an
  asset for a section; the operator confirms; only then does AIK pull from Downloads and run
  the import through `afl-asset-pipeline`. Never grab ahead of confirmation.
- **Created 2026-07-01.** Where it folds: the Beta-launch master-plan DRAFT does not yet exist
  as its own doc on disk (the overarching sequencing lives in `Docs/BAG_MAN_MASTER_BUILD_v2.0.md`
  Section 5, Phases/Sprints). This bank is organized by the operator's section names so it drops
  straight into that draft when it is written/located.
- **Owning skills:** import + version-migration + LFS + cook -> `afl-asset-pipeline`;
  traversal (mantle/vault/ledge/climb) -> `ue5-interaction-ik-expert`; weapon/beam -> 
  `afl-laser-beam-system`; weapon/skin cosmetic axis -> `lyra-skin-builder-marketplace`.

---

## THE INVENTORY (8 assets on hand)

| # | Asset (zip at Downloads\) | Declared UE ver | Section | Role (one line) |
|---|---|---|---|---|
| 1 | `Modern Guns Bundle 5.7.zip` | 5.7 | WEAPONS | Weapon meshes / content -- more gun bodies to convert to AFL weapons |
| 2 | `Customizable Weapon Pack 5.4.zip` | 5.4 | WEAPONS | Modular weapon mesh/parts -- variant + attachment base |
| 3 | `FPS Animation Ultimate Pack.zip` | (none in name) | WEAPONS | First-person / weapon animation set (fire, reload, ADS, etc.) |
| 4 | `Parkour Animations 5.1.zip` | 5.1 | TRAVERSAL/MOVE | Mantle / vault / traversal animation set |
| 5 | `Ledge Animation Set 4.27.zip` | 4.27 (UE4) | TRAVERSAL/MOVE | Ledge grab / hang / climb-up animation set |
| 6 | `Climb and Vaulting Component V2 5.3.zip` | 5.3 | TRAVERSAL/MOVE | Traversal COMPONENT (climb/vault logic) -- code/BP, not just anim |
| 7 | `Military Base Megapack 5.0.zip` | 5.0 | MAPS | Environment/prop kit -- original-arena building blocks |
| 8 | `Battle Arena.zip` | (none in name) | MAPS | Arena environment kit -- original-arena building blocks |

---

## SECTION MAPPINGS

### WEAPONS section  (assets 1, 2, 3)
- **Build MORE weapons on these** rather than harvest-from-scratch: the gun bodies (Modern Guns,
  Customizable Weapon Pack) become AFL weapon meshes; **beam-color-matching + AFL brand material**
  are built ON TOP (the proven weapon spine + tint-as-parameter-swap laws already exist).
- FPS Animation Ultimate Pack supplies weapon/first-person anims; must **retarget to the Lyra /
  UE5 mannequin skeleton** and hook the AFL fire montage contract (custom fire GA needs a
  CharacterFireMontage), not dropped in raw.
- **Cross-links:** `Docs/IRONICS_WEAPONS_SSOT.md` (List A real weapon TYPES to build + brand/beam
  contract + tint-swap efficiency law + weapons-as-a-paid-cosmetic-axis) and
  `Docs/BAG_MAN_MASTER_BUILD_v2.0.md` Sprints 15 (#4-5), 18 (#6-9), 20 (#10-12).
- **Doctrine gate:** each shipped weapon material needs a PC master + a `_Mobile` instance
  (master build 4.3, multi-platform rule). Stay Lyra-canonical (ItemDef -> Equipment -> WeaponInstance
  + AbilitySet); do NOT hand-build a weapon actor.
- **Version flags:** #1 is **5.7 (NEWER than the 5.6 project)** -- HIGH risk, see table below.
  #2 (5.4) is a normal older-to-5.6 upgrade. #3 version unknown -> check on grab.

### CHARACTER ANIM + MOVEMENT section (traversal)  (assets 4, 5, 6)
- Big accelerant for the movement upgrades: mantle / vault / ledge / climb come as content, not
  hand-authored. Ties directly to the `ue5-interaction-ik-expert` skill (mantle/vault/ledge
  doctrine) and layers onto the already-PROVEN CLIMB two-layer ability + MotionWarping substrate.
- Asset 6 (Climb and Vaulting Component V2) is a **COMPONENT** (logic), not just animation --
  evaluate it against the AFL "component-reads-stock-CMC / stay-within-Lyra" rule before adopting;
  it may be a reference for our own ability rather than a drop-in (do not wire a foreign movement
  component in raw -- that is the STEP-0 hazard class).
- **Cross-links:** `Docs/BAG_MAN_MASTER_BUILD_v2.0.md` Sprint 3 (Movement: Dash + Mobility) as the
  home section; existing proven DASH + CLIMB work is the layer these extend.
- **Version flags:** #5 is **4.27 (UE4)** -- HIGHEST migration gap (UE4->UE5 + anim retarget to the
  UE5 mannequin/Lyra skeleton). #4 (5.1) and #6 (5.3) are normal older-to-5.6 upgrades; #6 being
  code may show 5.3->5.6 API drift.

### MAPS + IN-GAME-ASSETS section  (assets 7, 8)
- **Partly answers the open "Lyra-harvest+reskin vs build-original" question -- the answer is BOTH:**
  - (a) **Lyra-harvest + reskin** = speed / fast roster fill / prototyping greyboxes.
  - (b) **ORIGINAL IRONICS arenas built from these kits** (Military Base Megapack, Battle Arena) =
    the AAA original flagship arenas.
- **When each fits:** harvest to hit roster COUNT fast and to greybox for the telemetry loop; the
  purchased kits feed the **"net-new"** arenas the map spec already calls for (map spec 4:
  "Existing Arena_01-06 fold in, PLUS net-new" -- the kits are the net-new source).
- **Cross-links:** `Docs/IRONICS_MAP_MODE_SPEC.md` (the 10-map tiered roster, greybox-telemetry
  loop, per-map DESIGN gate -- no map enters greybox until its brief is operator-approved);
  `Docs/BAG_MAN_MASTER_BUILD_v2.0.md` Sprints 5 / 16 / 18 / 20 (map production).
- **Version flags:** #7 (5.0) normal older-to-5.6 upgrade (large kit -> material/Nanite/Lumen +
  collision pass). #8 (Battle Arena) version unknown -> check on grab.

---

## SEQUENCE IMPACT (planning note)

- **Weapons and Traversal now have content ON HAND** -> those sections are LOWER-RISK and can move
  EARLIER than a build-everything-from-scratch assumption; the bottleneck shifts from "author the
  content" to "migrate + retarget + Lyra-canonical wire" (asset-pipeline work, not authoring).
- **The STORE is still the big dependency-heavy item** -> there is NO asset shortcut for it (it is
  backend + economy + entitlements, not content). It remains the long pole
  (`BAG_MAN_MASTER_BUILD_v2.0.md` Sprint 21 Battle Pass + Store). Do not let content-on-hand for
  weapons/maps create the illusion the store got shorter.

---

## VERSION-MIGRATION FLAGS (consolidated -> afl-asset-pipeline on grab)

| Asset | Ver | vs 5.6 | Risk | Action on grab |
|---|---|---|---|---|
| Modern Guns Bundle | 5.7 | **NEWER** | **HIGH** | 5.6 editor generally REFUSES a package saved by 5.7. Prefer a 5.6 download variant from the store page; if none, DEFER until the project upgrades, or re-acquire. Do not assume it imports. |
| Customizable Weapon Pack | 5.4 | older | LOW-MOD | Standard 5.4->5.6 upgrade on import; verify materials/sockets. |
| FPS Animation Ultimate Pack | unknown | ? | CHECK | Read the actual engine ver on grab; anims -> retarget to Lyra/UE5 mannequin. |
| Parkour Animations | 5.1 | older | LOW-MOD | Upgrade on import; retarget check. |
| Ledge Animation Set | 4.27 | **UE4** | **HIGH** | UE4->UE5 migration + full anim retarget to the UE5 mannequin/Lyra skeleton; biggest gap of the set. |
| Climb and Vaulting Component V2 | 5.3 | older | MOD | Upgrade on import; it is CODE/BP -> watch 5.3->5.6 API drift; evaluate vs Lyra-canonical rule before adopting. |
| Military Base Megapack | 5.0 | older | MOD | Upgrade on import; large env kit -> material/Nanite/Lumen + collision pass; LFS-size aware (250GB repo). |
| Battle Arena | unknown | ? | CHECK | Read the actual engine ver on grab; treat as MOD until known. |

Rule of thumb: OLDER-than-5.6 = normal upgrade-on-import (test in an isolated content-only folder
first). NEWER-than-5.6 (the 5.7 gun bundle) = may not load at all -> resolve the version before
counting on it. UE4 (the 4.27 ledge set) = the heaviest migration + retarget.

---

## GRAB PROTOCOL (LAW for this inventory)

1. The plan reaches a section that has an on-hand asset (weapons / traversal / maps).
2. **AIK ASKS** the operator: "Ready to grab `<asset>` for the `<section>` section?"
3. The operator CONFIRMS.
4. AIK grabs `C:\Users\tabor\Downloads\<asset>.zip`, runs it through `afl-asset-pipeline`
   (version-migrate FIRST in an isolated folder -> LFS -> Lyra-canonical wire -> cook check).
5. **NEVER** grab or import ahead of an operator confirmation. This bank is read-only until then.

---

## CROSS-LINKS

- `Docs/BAG_MAN_MASTER_BUILD_v2.0.md` -- Phase/Sprint sequencing (Sprint 3 movement, 15/18/20
  weapons, 5/16/18/20 maps, 21 store) + 4.2 Asset Acceptance Pipeline + 4.3 multi-platform materials.
- `Docs/IRONICS_WEAPONS_SSOT.md` -- weapon spine, List A/B, brand/beam contract, cosmetic axis.
- `Docs/IRONICS_MAP_MODE_SPEC.md` -- 10-map roster, net-new slots, per-map DESIGN gate.
- Skills: `afl-asset-pipeline`, `ue5-interaction-ik-expert`, `afl-laser-beam-system`,
  `lyra-skin-builder-marketplace`, `lyra-ue5-build-discipline`.
