# Rodin (Hyper3D) Gen-2.5 — Bag_Man genAI mesh tooling

Standalone REST wrapper for [Rodin Gen-2.5](https://developer.hyper3d.ai) — the
new provider replacing Tripo for the weapon/asset gen pipeline. Drives the API
directly with the Business key: **submit (text / image / multiview) → poll →
download**, then the mesh goes into the normal Blender conform + UE hand-off.

> **Two ways to talk to Rodin — we use the API wrapper, not the add-on.**
> The downloaded `blender_rodin_bridge` add-on is **web-driven**: it opens the
> Rodin website in Chrome and imports through a floating window (needs Chrome +
> an interactive Rodin login). Good for manual browsing, but it can't be driven
> headlessly by the agent pipeline. `rodin.py` below is the automation path.

## 1. One-time setup

**Requires a Rodin BUSINESS subscription** (the Gen-2.5 API is gated to it).

Set the API key one of two ways (env wins over the file):

**A. Config file (simplest for the agent):** paste the key into `rodin.config.json`
(already gitignored — the real key never commits):
```json
{ "api_key": "rdn_xxx..." }
```

**B. PowerShell env var (persistent across sessions):**
```powershell
setx RODIN_API_KEY "rdn_xxx..."
```
(`setx` persists for new processes; `$env:RODIN_API_KEY="..."` is session-only.)

Verify:
```bash
python rodin.py balance
```

## 2. Usage

```bash
# text -> 3D (hero hard-surface defaults)
python rodin.py gen --text "matte black sci-fi forearm cannon, red neon vents, single straight barrel" \
  --name GloveCannon_A --out ./out/GloveCannon_A

# image -> 3D  (one ref)
python rodin.py gen --image refs/ref1.png --name RefCannon --out ./out/RefCannon

# multiview -> 3D  (label order MUST match --image order)
python rodin.py gen --image refs/ref1.png refs/ref2.png --label FR F --name RefCannon_MV --out ./out/RefCannon_MV

# retexture an existing mesh
python rodin.py texture --model mesh.glb --image refs/ref1.png --prompt "matte black + red neon" --out ./out/tex

# submit only (get uuid+subkey), poll later, download later
python rodin.py gen --text "..." --no-wait
python rodin.py status   --subkey <subscription_key>
python rodin.py download --task <task_uuid> --out ./out/x
```

## 3. AAA quality recipe (defaults + when to override)

`gen` defaults are tuned for Bag_Man weapon meshes:

| Flag | Default | Notes |
|---|---|---|
| `--tier` | `Gen-2.5-High` | Hero pieces → `Gen-2.5-Extreme-High` (1.0 cr, unlocks `--micro`). Others are 0.5 cr. |
| `--mesh-mode` | `Quad` | Clean topology; we triangulate on the Blender export anyway. `--highpack` gives 16× faces in Quad. |
| `--quality` | `high` | Face-count preset. `--polycount N` sets an explicit count (`quality_override`). |
| `--material` | `PBR` | Albedo + metallic + roughness + normal. |
| `--format` | `fbx` | Straight into the conform. (glb/obj/usdz/stl available.) |
| `--texture-mode` | `high` | Texture fidelity. |
| `--instruct` | auto | `faithful` when `--image` (match the ref), `creative` for `--text`. |
| `--highpack` | off | **4K textures + 16× faces** (Quad). +1 credit. Use for hero bases. |
| `--hd-texture` | off | Extra texture post-process. |
| `--micro` | off | Micro surface detail; **Extreme-High tier only**. |
| `--bbox Y Z X` | — | Size-hint ControlNet. |
| `--seed N` | random | 0–65535, for seed sweeps. |

**Recommended for a hero weapon base:**
```bash
python rodin.py gen --image refs/ref1.png refs/ref2.png --label FR F \
  --tier Gen-2.5-Extreme-High --highpack --hd-texture --micro \
  --name HeroCannon --out ./out/HeroCannon
```

## 4. Endpoints (verified against the docs)

Base `https://api.hyper3d.com/api/v2` · auth `Authorization: Bearer <key>`

| Endpoint | Method | Purpose |
|---|---|---|
| `/rodin` | POST (multipart) | submit Gen-2.5 (text/image) |
| `/rodin_texture_only` | POST (multipart) | retexture an existing model (0.5 cr) |
| `/status` | POST `{subscription_key}` | poll → `jobs[].status` ∈ Waiting/Generating/Done/Failed |
| `/download` | POST `{task_uuid}` | → `{list:[{url,name}]}` |
| `/check_balance` | GET | → `{balance}` |

## 5. Pipeline fit

`rodin.py gen` produces the FBX + PBR maps → the **same Blender conform** as
before (orient muzzle→+Y, cuff origin, single-root `root`/`root_l` rig, locked
FBX export) → UE import → AIK. Rodin's cleaner hard-surface topology + 4K PBR
should remove the silhouette re-roll churn we hit with the previous provider.
