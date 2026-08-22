# Mesh forensics — which source produced this mesh?

Two readers that answer that question from bytes on disk, with **no Blender and no UE**.

- `fbx_control_points.py <fbx...>` — control-point count and UV-layer count per binary FBX.
- `blend_vertex_counts.py <blend...>` — vertex counts from zstd-compressed `.blend` files.

## Both refuse to be believed without a control

Each carries a KNOWN value and reports `PARSER FAILED ITS CONTROL` rather than printing numbers it
cannot vouch for. This is not ceremony — both readers were wrong on their first run:

- the FBX reader read the version 2 bytes late (the magic is 21 bytes, not 23) and returned **0
  vertices for every file**;
- the `.blend` reader guessed the Blender 5.0 block header and walked **0 blocks**.

Both failures printed as failures because a control was in the same run. A reader like this that
silently returns a plausible-but-wrong count is worse than no reader, because the number gets used.

## The `.blend` block layout was derived, not guessed

Blender 5.0 (`BLENDER17-01v0502`): 17-byte file header, then 32-byte block headers laid out
`code[4], pad[4], old(i64)@8, size(i64)@16, nr(i32)@24, sdna(i32)@28`.

Candidate `(headerSize, sizeOffset)` pairs were walked across the whole file and only one traversed
all 97,603 blocks and landed **exactly** on the file's real `ENDB` offset. A wrong stride
desynchronises within a few blocks and never arrives, so arriving is the proof.

## Counts are necessary, not sufficient

Two meshes can share a vertex count and be different geometry. To settle CC-7.1 the vertex POSITIONS
were compared directly: `symm_final.blend` reproduces the shipped FBX to `2.38e-07`, which is float32
rounding — the blend stores float32, the FBX float64. That is what made the answer decisive rather
than circumstantial.

## What it settled (2026-08-21)

| file | verts | UV layers |
|---|---|---|
| `SKM_IRONICS_Blank.fbx` (SHIPPED) | 46,603 | UV0, UV1 |
| `IRONICS_Blank_symm_final.blend` | **46,603** | UV0, UV1 | ← **the source**
| `IRONICS_Blank_conform.blend` / `_prerefit` | 45,398 | UV0, UV1 | earlier conform pass
| `IRONICS_Blank_rootfix.blend` / `_visorcut` | 66,394 | UV0 | dead branch
| `SKM_IRONICS_Blank_UV1FIX.fbx` / `_ROOTFIXED` | 66,394 | UV0, UV1 | dead branch
