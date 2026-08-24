# IRONICS JEWELLERY — ART BRIEF

**Ten pieces.** Two chains, four pendants, two watches, two bracelets. Source art is the operator's,
in Google Drive (`ironicsjewelry`, 12 PNGs of which two — `RU Chain 1` and `1776 Chain` — were
reference-only and are not SKUs).

Everything below marked **REQUIREMENT** is a constraint the mesh must satisfy, not a question for the
generator to answer. The two at the top are the ones that will break the system if missed, because the
code that consumes these meshes is already written and already committed against them.

---

## REQUIREMENT 1 — every chain mesh carries a pendant socket

Two chains × four pendants = **eight combinations, and all eight must hang correctly.**

A pendant is a separate SKU in a separate slot (`EAFLAccessorySlot::Pendant`); it is not baked into a
chain. It attaches to `accessory_pendant` on `spine_03` and inherits that socket's transform. So the
chain mesh must leave a pendant-sized volume clear where the socket sits, and its lowest link must
read as something a pendant hangs *from*.

A chain modelled with its own integral pendant, or with the lowest link closed flush against the
chest, produces intersection on six of the eight pairings and there is no per-item transform to
correct it with — `FAFLAccessoryPlacement` deliberately carries no transform, because offsets belong
to the socket, authored once.

**Both chains must clear the same volume.** They share one socket. A chain that needs its own
pendant offset cannot have one.

## REQUIREMENT 0 — THE SOURCE IMAGE POSE IS THE 3D RESULT

**Tripo reconstructs what it is shown, not what the object is.** Measured on our own two chains, from
identical prompts and settings — the only difference was how the piece was photographed:

| source pose | resulting mesh |
|---|---|
| **Founders Link** — hanging in a V, tapering to a point | X 15.4% · Y 79.1% · **Z 100%**. A real hanging drape. Slice-taper 0.34 → 0.79. Rigged first time. |
| **Founders Purps** — laid flat in a closed circle, top-down | X 100% · Y 98.9% · **Z 8.2%**. A flat ring. **No hanging axis exists to rig along.** |

So for anything that must sway:

- **PHOTOGRAPH IT HANGING.** A vertical drape gives the bone chain an unambiguous axis. A flat or
  coiled presentation gives none, and no conform can invent one — bending a flat ring into a U is
  authoring geometry that was never generated.
- **THE PIECE ALONE.** No pendant on the chain, no bust, no mannequin, no prop. A chain worn on
  something reconstructs that something.
- **PLAIN BACKGROUND.** Background content is reconstructable.

**`Ironics Founders Link Chain.png` is the reference for how to shoot these.** It already satisfies all
three, which is why it worked.

Bracelets are the exception that proves the rule: a bracelet **is** a ring, so a flat-ish result
(Z 35–39% of the longest axis, measured on both) is the correct shape rather than a defect. What a
bracelet lacks is not depth but a hanging direction — its sway axis comes from how it sits on the
wrist, not from the mesh.

## REQUIREMENT 2 — chains and bracelets are SKELETAL with a bone chain; watches are RIGID

Chains and bracelets get secondary motion — game-realism bounce and sway, driven by **AnimDynamics in
a post-process AnimBP** on the accessory's own mesh. That is a spring-mass solver on a bone chain: with
no bones there is nothing to swing, and a static mesh cannot be made to sway later without re-rigging.

**This is why the requirement is here and not after the run.** Sway is only visible on a bone chain.
Proving the mechanism on a static mesh proves nothing, and re-authoring afterwards costs a second pass
on all ten pieces.

| piece | rig | why |
|---|---|---|
| **Chains** | skeletal, **3–5 bones down the hanging length** | the swing happens along that chain |
| **Bracelets** | skeletal, **small bone chain** | sway at the wrist |
| **Watches** | **rigid — static mesh is correct** | a watch does not sway, and this is what keeps the budget honest |

### `accessory_pendant` goes on the LOWEST SIMULATED bone

Not the root, not a static bone. **A socket on a static bone makes the chain swing while the pendant
hangs motionless inside it** — which reads worse than no physics at all, because the eye reads the
contradiction rather than the motion.

### Every accessory mesh needs `bEnablePostProcess` ON

`bEnablePostProcess` is **false** on the character's part meshes today. An accessory whose post-process
AnimBP never runs looks exactly like a physics failure — no sway, no error, nothing in a log. Set it
per accessory mesh, and treat a still chain as a wiring question before an authoring one.

## REQUIREMENT 3 — wrist pieces symmetric, or supplied as an L/R pair

The server picks which wrist a piece goes on. **The rule is: first open side, left before right** —
the order is fixed so the same equip twice lands the same way, and it applies to whatever is equipped,
regardless of what kind of piece it is. There is no player-facing side choice.

That order is arbitrary and carries no meaning. It is **not** the handedness fallback below, which is a
different rule for a different reason: one is "which side is free", the other is "which side this mesh
was built for".

That mechanism assumes the mesh reads correctly on either arm. A watch is the usual failure: the
crown sits on one side, the clasp runs one way, and mirrored it reads as broken.

- **Symmetric single mesh** — preferred. One asset, either wrist, `bWristEitherSide = true`.
- **Handed L/R pair** — acceptable. Two meshes, server picks the matching one.
- **Handed single mesh** — the fallback. `bWristEitherSide` goes **false** on that row and it becomes
  fixed: **watches right, bracelets left.** The piece then refuses to equip when its own wrist is
  taken, even if the other is free.

All four wrist rows currently ship with `bWristEitherSide = true`. If the art comes back handed and
single, those rows must be flipped, and a player who owns two watches can only wear one.

---

## The ten

| id | display name | slot | supply |
|---|---|---|---:|
| `AFL.Accessory.Chain.FoundersPurps` | Ironics Founders Purps Chain | Neck | 1,000 |
| `AFL.Accessory.Chain.FoundersLink` | Ironics Founders Link Chain | Neck | 1,000 |
| `AFL.Accessory.Pendant.TTG` | Ironics TTG Pendant | Pendant | 100 |
| `AFL.Accessory.Pendant.RareUniverse` | Ironics Rare Universe Pendant | Pendant | 100 |
| `AFL.Accessory.Pendant.BigSixx` | Ironics Big Sixx Pendant | Pendant | 100 |
| `AFL.Accessory.Pendant.1776` | Ironics 1776 Pendant | Pendant | 100 |
| `AFL.Accessory.Watch.RareUniverse` | Ironics Rare Universe Watch | Wrist | 100 |
| `AFL.Accessory.Watch.Quantum` | Ironics Quantum Watch | Wrist | 100 |
| `AFL.Accessory.Bracelet.QuantumUniverse` | Ironics Quantum Universe Bracelet | Wrist | 1,000 |
| `AFL.Accessory.Bracelet.BigSixx` | Big Sixx Bracelet | Wrist | 1,000 |

**4,600 units total.** All ten at 990 VO ($0.99). Scarcity is the cap, not the price — a chain and a
pendant cost the same.

`Big Sixx Bracelet` carries no *Ironics* prefix in the source art and does not gain one. Casing was
normalised across siblings; words were not added.

---

## Hardpoints

Authored on `SK_Mannequin`, the skeleton both character lines share, via
`afl.Dev.AuthorAccessorySockets`. All seven verified present.

| socket | bone | note |
|---|---|---|
| `accessory_neck` | `spine_03` | chains |
| `accessory_pendant` | `spine_03` | pendants — same bone, own socket, hangs lower |
| `accessory_wrist_l` | `hand_l` | |
| `accessory_wrist_r` | `hand_r` | |

**`spine_03`, not a neck bone.** This skeleton has 164 bones and stops at `spine_03` — there is no
`neck_01`, no `spine_04`, no `spine_05`, whatever the UE5 mannequin naming would suggest. Verified
against the reference skeleton, not inferred. `weapon_holster_back` already uses the same bone.

**`hand_*`, not `lowerarm_*`.** A watch turns with the wrist, and the wrist joint is `hand_*`.
Parented to the forearm it stays level while the hand rotates.

Offsets are socket-side content work and are all zero today. A piece that sits wrong is corrected on
the socket, once, for every item that uses it — never per item.

---

## What consumes these

Nothing yet, and that is the honest state. `FAFLAccessorySet` is written by the server, replicates,
and refuses correctly — but **no code reads it to attach a mesh.** The attach consumer is unbuilt.

So a purchased piece today is owned, appears in the loadout, and resolves to a slot; it does not
render. The art landing does not by itself make it visible — the consumer is a separate piece of work.

Logged as **CC-X37** in `Docs/design/IRONICS_CHARACTER_CREATOR_SSOT.md` §11.1, where it belongs: it is
a programme-level gap, not an art note. Third instance of built-correct-unreachable, after CC-X21 and
CC-X32.

**The physics layer sits on top of that attach path, not instead of it.** AnimDynamics runs inside the
accessory mesh's own anim graph; it needs the mesh to exist and be attached first. Nothing in this
section blocks the art — it blocks the pieces being *seen*.

---

## Style

Per standing project rule, the silhouette reads as real jewellery **first**; brand motif is accent
only. A chain reads as a chain across a room before it reads as IRONICS. The same rule that rejected
the Makhiavelli turret and the too-organic Talon applies here.

Core palette is the locked Neon 6. These are one design per piece, not a six-colour family — the
colour is part of the named piece (`Founders Purps` is purple by name), so the
one-design × six-native-colours model does **not** apply.
