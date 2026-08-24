# SKELETAL ACCESSORY RECIPE — a SIBLING of the HQ forward standard

**Piloted on the Founders Purps chain canary, 2026-08-23.** Applies to jewellery that must SWAY:
chains and bracelets. Watches and pendants are rigid and go through
`RECIPE_HQ_FORWARD_STANDARD.md` unchanged.

## Why a sibling and not a branch

The weapon recipe is proven and every conformed asset in the project has taken it. Two shapes were
possible:

- **A branch inside it** — discarded. It makes the round-trip assert *conditional*, and that assert is
  the last gate before an asset ships. A conditional in a proven path is the failure shape this
  programme has paid for repeatedly. It would also make every reader of the weapon recipe skip a
  section that does not apply to them.
- **A full copy** — discarded. Duplication drifts, and 90% of the steps are genuinely shared.

So this file states **only the deltas** and defers to the weapon recipe for everything else. **The
static path is byte-unchanged**: `bones == ["root"]` still holds for the six rigid jewellery pieces and
for every prior asset.

## Deltas from `RECIPE_HQ_FORWARD_STANDARD.md` §3

Everything in §3 applies as written — uniform scale to class dims, re-origin to the hold point at world
0, full mesh kept, 150k COLLAPSE LOD0, high-poly backup, `use_tspace=True`, one material, all three maps
unpacked — **except the armature**, which is where this recipe diverges.

### D1 · The hold point is the TOP

A weapon's hold point is its grip. A chain's is the **top**, where it meets `accessory_neck`. Re-origin
so the topmost point sits at world 0; the piece then hangs into −Z from its attach point.

### D2 · Find the hanging axis from the geometry, do not assume it

Slice along each axis and measure the spread of the other two. The hanging axis is the one whose slices
**taper** — narrow at the belly, wide where the piece opens. On the canary chain: Z-slice widths climbed
0.528 → 0.938 while the Y mid-band collapsed to 0.194, because only the belly of the U sits at mid-Y.
That reads as a U in the Y–Z plane, thin in X, hanging down −Z. Assuming the axis from the bounding box
alone would have picked Z anyway *here*, and would be a coin-flip on the next piece.

### D3 · 3–5 bones, heads at PER-BAND CENTROIDS

Divide the hanging axis into N equal bands (N = 3–5; the canary used **4**) and place each bone head at
the **centroid of the mesh in that band**, not on a straight line down the middle.

A necklace is a U. A bone chain down the geometric centre sits in **empty space** at the top, and
proximity weighting then binds the wrong vertices.

Name them `chain_01 … chain_0N`, parented in a connected chain.

### D4 · Near-rigid links, rotation at the joints

Each vertex takes **full weight** from the bone whose band it falls in, with a narrow linear blend
across the boundary (the canary used **a quarter of a band**, 1.56 cm).

A smooth global falloff lets the links stretch and shear, which reads as rubber rather than metal. The
sway must come from the **joints**.

### D5 · `accessory_pendant` on the LOWEST bone — chains only

At the **tail** of the last bone, which is the belly of the U where a pendant actually hangs.

A socket on a static bone makes the chain swing while the pendant hangs motionless inside it. The eye
reads the contradiction rather than the motion, which is worse than no physics at all.

Bracelets get no socket.

### D6 · Name the armature object `root`

**The armature OBJECT becomes the root bone in UE.** Exported as `ARM_Chain`, the canary arrived with a
root bone named after a Blender object. The weapon recipe already calls its single bone `root`, so both
paths agree on what the root is called.

Export with `add_leaf_bones=False` — a leaf bone becomes an extra bone in UE and breaks the assert.

### D7 · The round-trip assert

Weapon / rigid: `bones == ["root"]` — **unchanged**.

Skeletal accessory: `bones == ["root", "chain_01", … , "chain_0N"]`, i.e. **N + 1** bones, root first.

The canary asserts `["root","chain_01","chain_02","chain_03","chain_04"]`.

## Authoring the socket in UE

`USkeletalMeshSocket::SocketName` is **read-only from Python** (CC-X34). The AIK Lua bridge exposes the
C++ path, so no build is needed:

```lua
local a = open_asset("/Game/BagMan/Cosmetics/Accessories/SK_BagMan_Chain_FoundersPurps")
a:add("socket", {name="accessory_pendant", bone="chain_04", location={x=6.25,y=0,z=0}})
a:save()
```

`location` is along the bone's own axis — FBX import puts that on X — so `x = bone length` lands the
socket at the tail.

## Verify by READ-BACK, on a fresh load

`save_asset` returning true is not proof. Per piece:

| check | canary result |
|---|---|
| bone count and names | `['root','chain_01','chain_02','chain_03','chain_04']` |
| socket present, on the lowest bone | `accessory_pendant` bone=`chain_04` ✅ |
| vert count vs source | Blender 96,398 → UE **98,179** (UV/normal splits, expected) |
| materials | 1 |
| post-process ABP | `post_process_anim_blueprint: None` |

## The post-process flag is not called `bEnablePostProcess`

There is no property by that name. What exists is:

- **on the mesh** — `post_process_anim_blueprint` (the assignment; `None` until an ABP exists)
- **on the component** — `disable_post_process_blueprint`, `post_process_anim_bplod_threshold`

So "enable post-process per accessory mesh" means: assign the ABP on the mesh, and leave the
component's disable flag false. **An accessory whose ABP never runs looks exactly like a physics
failure** — no sway, no error, nothing in a log — so check the assignment before suspecting the rig.
