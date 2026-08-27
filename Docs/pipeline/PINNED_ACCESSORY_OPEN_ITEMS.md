# PINNED — accessory open items

Parked 2026-08-26. Everything here is measured, not suspected. Placement law lives in
`ACCESSORY_PLACEMENT_LAW.json`; do not write those sockets automatically.

---

## 1. Pendant ORIENTATION — operator to set (next session)

**State:** size and position are correct in game (6.3 × 6.2 × 2.7 cm, world scale 1.88/1.38/1.19
matching the chain). Orientation is wrong — the pendant does not hang the way the operator set it.

**Why the obvious fix does not work:** the pendant is spawned by the CHAIN actor onto the CHAIN's own
`accessory_pendant` socket. The operator tuned the socket of the same name on the BODY, which the
game never reads for this piece.

**Do this:** open `SK_BagMan_Chain_FoundersLink` (and `_FoundersPurps`), select the asset's
`accessory_pendant` socket, rotate with the gizmo until the pendant hangs correctly. The gizmo
handles the socket's axis permutation; typed values do not (see §3). Current in-game orientation for
reference: `(P −83.9, Y 17.0, R −108.8)`.

Automated delta-computation was ATTEMPTED and failed — see §2 for why.

---

## 2. `accessory_pendant` is NOT reachable at runtime on the pawn — REAL BUG

Measured in PIE: the diagnostic asked the pawn's mesh for socket `accessory_pendant` and got
`<body socket missing>`, while `accessory_neck` resolves on that same mesh without trouble.

The pawn renders `SKM_Manny_Invis` (invisible collision base) plus a visible body character part. The
neck socket is reachable there; the pendant socket is not. So the two sockets are stored differently
— one on the shared skeleton, one on the `SKM_Manny` mesh asset — and only the skeleton-level one
survives to runtime.

**Consequence:** any tuning of `accessory_pendant` on the body is editor-only and silently does
nothing in game. Worth confirming which sockets live where before more placement work is spent.

---

## 3. Chain rig carries a baked 100× bone scale — WORKAROUND IN PLACE

The procedural chain was exported from Blender in metres without unit scale applied. Its bones carry
100×, which leaks into anything attached to a chain socket:

- socket `relative_location`: one unit moves **100 cm** (compensated empirically when the pendant
  socket was placed)
- socket `relative_scale`: multiplies by **100** — this made the in-game pendant 6.3 METRES across
  until `accessory_pendant` scale was set to `0.01` (commit `29497997`)

**Real fix (deferred):** re-export from
`Content/AFL/_Bridge/Blender/pending/20260826-chain-closed/BagMan_Chain_Closed.blend` with unit scale
applied so bones are 1.0, then reset the socket to `1.0` scale and re-derive its location. Deferred
because a re-import invalidates the operator's placement. Do it when the chain is next rebuilt anyway.

Until then `0.01` looks like a magic number — this file is why it is there.

---

## 4. Chain does not lie flat on the body — ORIGINAL UNSOLVED PROBLEM

AnimDynamics cannot do it (no collision input — a structural limit of the node, not a tuning issue).
Chaos Cloth was attempted and **reverted in full**: the pipeline worked at asset level (mask
18.9% pinned / 65% free, `boundSections=1`, `AddClothCollisionSource` attaching to `PA_Mannequin`),
but applying cloth broke the chain's socket preview and cost the operator three days of placement.
All cloth code was removed. Do not retry without an explicit request and a placement backup that is
re-captured AFTER the operator's last edit.

---

## 5. Smaller items

- **Master materials still default `Metallic = 1.0`** (`M_BagMan_Accessory_Master`, `_Solid`). Inert
  today because every instance overrides, but a NEW instance would inherit metal and render skinless.
- **`SK_BagMan_Chain_FoundersPurps` has no `_Color`/`_NormalGL`** on disk — renders as tinted solid
  metal rather than its own skin. Predates this work; needs a re-export from source.
- **Triangle budget:** Tripo pieces are ~150k tris each (a 5 cm watch is 149,990). Ten pieces is
  ~1.5M triangles of jewellery per character. Not blocking, but not shippable either.
- **Bracelet dropped by the operator** — `accessory_wrist_r` left at `(0,0,0)` deliberately.
