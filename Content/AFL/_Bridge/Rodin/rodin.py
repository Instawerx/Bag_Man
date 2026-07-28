#!/usr/bin/env python3
"""
Rodin (Hyper3D) Gen-2.5 API wrapper for the Bag_Man genAI mesh pipeline.

Replaces the ad-hoc Tripo curl flow. Drives the REST API directly with the
Business API key: submit (text / image / multiview) -> poll -> download.
Docs: https://developer.hyper3d.ai  (Gen-2.5 = POST /api/v2/rodin)

API KEY resolution (first hit wins):
  1. env  RODIN_API_KEY
  2. rodin.config.json  (next to this script)  ->  {"api_key": "..."}
The config file is .gitignored so the real key never commits.

Endpoints (verified against the docs, base https://api.hyper3d.com/api/v2):
  POST /rodin                 submit text/image-to-3D  (multipart/form-data)
  POST /status                poll  {"subscription_key": ...} -> jobs[].status
  POST /download              {"task_uuid": ...} -> {"list":[{url,name}]}
  GET  /check_balance         -> {"balance": N}
  POST /rodin_texture_only    retexture an existing model (see `texture` cmd)

Usage examples:
  python rodin.py balance
  python rodin.py gen --text "matte black sci-fi forearm cannon ..." --name GloveCannon_A --out ./out
  python rodin.py gen --image ref1.png ref2.png --label FL FR --name RefCannon --out ./out
  python rodin.py texture --model mesh.glb --image ref.png --prompt "matte black + red neon" --out ./out
Any submit flag can override the AAA defaults (see --help).
"""
import argparse, json, os, sys, time, mimetypes
import requests

BASE = "https://api.hyper3d.com/api/v2"
HERE = os.path.dirname(os.path.abspath(__file__))

# ---- AAA hard-surface defaults (tuned for Bag_Man weapon assets) -------------
DEFAULTS = dict(
    tier="Gen-2.5-High",          # hero -> Gen-2.5-Extreme-High (enables --micro, 1.0 cr)
    mesh_mode="Quad",             # clean topology; we triangulate on the Blender export anyway
    quality="high",               # face-count preset
    material="PBR",               # albedo + metallic + roughness + normal
    geometry_file_format="fbx",   # straight into the conform
    texture_mode="high",
)


def api_key():
    k = os.environ.get("RODIN_API_KEY", "").strip()
    if not k:
        cfg = os.path.join(HERE, "rodin.config.json")
        if os.path.exists(cfg):
            try:
                k = (json.load(open(cfg)).get("api_key") or "").strip()
            except Exception as e:
                sys.exit(f"rodin.config.json unreadable: {e}")
    if not k or k.upper().startswith("REPLACE"):
        sys.exit("No API key. Set env RODIN_API_KEY or put it in rodin.config.json "
                 "(copy rodin.config.example.json). Needs a Rodin BUSINESS subscription.")
    return k


def auth_hdr():
    return {"Authorization": f"Bearer {api_key()}"}


def _bool(b):
    return "true" if b else "false"


# ---- commands ----------------------------------------------------------------
def cmd_balance(_a):
    r = requests.get(f"{BASE}/check_balance", headers=auth_hdr(), timeout=30)
    r.raise_for_status()
    print(json.dumps(r.json(), indent=2))


def submit(a):
    """Submit a generation. Returns (task_uuid, subscription_key).

    Everything is sent through the `files` param (text fields as (None, value)
    parts) so the request is ALWAYS multipart/form-data, which the API requires
    even for text-to-3D (with no image upload, plain data= would url-encode).
    """
    is_image = bool(a.image)
    # instruct mode: faithful matches an image ref; creative for pure text
    instruct = a.instruct or ("faithful" if is_image else "creative")
    fields = [
        ("tier", a.tier), ("mesh_mode", a.mesh_mode), ("quality", a.quality),
        ("material", a.material), ("geometry_file_format", a.geometry_file_format),
        ("texture_mode", a.texture_mode), ("geometry_instruct_mode", instruct),
    ]
    if a.prompt:            fields.append(("prompt", a.prompt))
    if a.seed is not None:  fields.append(("seed", str(a.seed)))
    if a.polycount is not None: fields.append(("quality_override", str(a.polycount)))
    if a.highpack:          fields.append(("addons", "HighPack"))   # plain field (NOT json) -- proven working; 4K+16x faces
    if a.hd_texture:        fields.append(("hd_texture", "true"))
    if a.delight:           fields.append(("texture_delight", "true"))
    if a.micro:             fields.append(("is_micro", "true"))      # Extreme-High only
    if a.preview:           fields.append(("preview_render", "true"))
    # image_label / bbox_condition are JSON.parse()d server-side -> send as JSON strings (a lone "R" is invalid JSON)
    if a.label:             fields.append(("image_label", json.dumps(a.label)))   # order matches --image order
    if a.bbox:              fields.append(("bbox_condition", json.dumps([int(v) for v in a.bbox])))  # [Y, Z, X]

    # multipart parts: text fields -> (None, value) ; images -> (fname, fh, mime)
    parts = [(k, (None, v)) for k, v in fields]
    fhandles = []
    if is_image:
        for p in a.image:
            if not os.path.exists(p):
                sys.exit(f"image not found: {p}")
            fh = open(p, "rb"); fhandles.append(fh)
            mt = mimetypes.guess_type(p)[0] or "application/octet-stream"
            parts.append(("images", (os.path.basename(p), fh, mt)))
    elif not a.prompt:
        sys.exit("Text-to-3D needs --text; Image-to-3D needs --image.")

    print(f"[rodin] submit  mode={'image' if is_image else 'text'}  tier={a.tier}  "
          f"mesh={a.mesh_mode}  instruct={instruct}  highpack={a.highpack}", file=sys.stderr)
    r = requests.post(f"{BASE}/rodin", headers=auth_hdr(), files=parts, timeout=120)
    for fh in fhandles:
        fh.close()
    try:
        j = r.json()
    except Exception:
        sys.exit(f"submit failed [{r.status_code}]: {r.text[:400]}")
    if r.status_code >= 300 or j.get("error"):
        sys.exit(f"submit error [{r.status_code}]: {json.dumps(j)}")
    task = j["uuid"]
    subkey = j["jobs"]["subscription_key"]
    print(f"[rodin] task_uuid={task}  subscription_key={subkey}", file=sys.stderr)
    return task, subkey


def poll(subkey, interval=8, max_wait=1800):
    """Block until all jobs Done, or raise on Failed / timeout."""
    waited = 0
    while True:
        r = requests.post(f"{BASE}/status", headers=auth_hdr(),
                          json={"subscription_key": subkey}, timeout=30)
        r.raise_for_status()
        jobs = r.json().get("jobs", [])
        sts = [j.get("status") for j in jobs]
        done = sum(s == "Done" for s in sts)
        print(f"[rodin] {waited:>4}s  {done}/{len(sts)} Done  {sts}", file=sys.stderr)
        if sts and all(s == "Done" for s in sts):
            return
        if any(s == "Failed" for s in sts):
            raise RuntimeError(f"a job Failed: {sts}")
        if waited >= max_wait:
            raise TimeoutError(f"gave up after {max_wait}s; last {sts}")
        time.sleep(interval)
        waited += interval


def download(task, outdir):
    os.makedirs(outdir, exist_ok=True)
    r = requests.post(f"{BASE}/download", headers=auth_hdr(),
                      json={"task_uuid": task}, timeout=60)
    r.raise_for_status()
    items = r.json().get("list", [])
    saved = []
    for it in items:
        name, url = it["name"], it["url"]
        dst = os.path.join(outdir, name)
        with requests.get(url, stream=True, timeout=300) as g:
            g.raise_for_status()
            with open(dst, "wb") as f:
                for chunk in g.iter_content(1 << 16):
                    f.write(chunk)
        saved.append(dst)
        print(f"[rodin] saved {name}  ({os.path.getsize(dst)} bytes)", file=sys.stderr)
    return saved


def cmd_gen(a):
    task, subkey = submit(a)
    if a.no_wait:
        print(json.dumps({"task_uuid": task, "subscription_key": subkey}))
        return
    poll(subkey, interval=a.interval, max_wait=a.max_wait)
    outdir = a.out or os.path.join(HERE, "out", a.name or task[:8])
    saved = download(task, outdir)
    print(json.dumps({"task_uuid": task, "outdir": outdir, "files": saved}, indent=2))


def cmd_status(a):
    r = requests.post(f"{BASE}/status", headers=auth_hdr(),
                      json={"subscription_key": a.subkey}, timeout=30)
    print(json.dumps(r.json(), indent=2))


def cmd_download(a):
    saved = download(a.task, a.out or os.path.join(HERE, "out", a.task[:8]))
    print(json.dumps({"files": saved}, indent=2))


def cmd_texture(a):
    """Retexture an existing model via /rodin_texture_only (0.5 cr)."""
    if not (os.path.exists(a.model) and os.path.exists(a.image)):
        sys.exit("texture: --model and --image must both exist")
    data = [("geometry_file_format", a.geometry_file_format),
            ("material", "PBR"), ("resolution", a.resolution)]
    if a.prompt:
        data.append(("prompt", a.prompt))
    if a.seed is not None:
        data.append(("seed", str(a.seed)))
    with open(a.model, "rb") as fm, open(a.image, "rb") as fi:
        files = [("model", (os.path.basename(a.model), fm, "application/octet-stream")),
                 ("image", (os.path.basename(a.image), fi, mimetypes.guess_type(a.image)[0] or "image/png"))]
        r = requests.post(f"{BASE}/rodin_texture_only", headers=auth_hdr(),
                          data=data, files=files, timeout=120)
    j = r.json()
    if r.status_code >= 300 or j.get("error"):
        sys.exit(f"texture error [{r.status_code}]: {json.dumps(j)}")
    task, subkey = j["uuid"], j["jobs"]["subscription_key"]
    print(f"[rodin] texture task={task} sub={subkey}", file=sys.stderr)
    if a.no_wait:
        print(json.dumps({"task_uuid": task, "subscription_key": subkey})); return
    poll(subkey, interval=a.interval, max_wait=a.max_wait)
    saved = download(task, a.out or os.path.join(HERE, "out", "tex_" + task[:8]))
    print(json.dumps({"task_uuid": task, "files": saved}, indent=2))


# ---- arg parsing -------------------------------------------------------------
def add_submit_flags(p):
    p.add_argument("--text", dest="prompt", help="text prompt (Text-to-3D)")
    p.add_argument("--image", nargs="+", help="1-5 reference images (Image-to-3D / multiview)")
    p.add_argument("--label", nargs="+",
                   help="orientation labels matching --image order: F FL FR L R B BL BR U D ?")
    p.add_argument("--name", help="output subfolder / asset name")
    p.add_argument("--out", help="output directory (default ./out/<name>)")
    p.add_argument("--tier", default=DEFAULTS["tier"],
                   help="Gen-2.5-Extreme-Low|Low|Medium|High|Extreme-High")
    p.add_argument("--mesh-mode", dest="mesh_mode", default=DEFAULTS["mesh_mode"], choices=["Raw", "Quad"])
    p.add_argument("--quality", default=DEFAULTS["quality"], choices=["high", "medium", "low", "extra-low"])
    p.add_argument("--polycount", type=int, help="quality_override: explicit polygon count")
    p.add_argument("--material", default=DEFAULTS["material"], choices=["PBR", "Shaded", "All", "None"])
    p.add_argument("--format", dest="geometry_file_format", default=DEFAULTS["geometry_file_format"],
                   choices=["glb", "usdz", "fbx", "obj", "stl"])
    p.add_argument("--texture-mode", dest="texture_mode", default=DEFAULTS["texture_mode"],
                   choices=["legacy", "extreme-low", "low", "medium", "high"])
    p.add_argument("--instruct", choices=["creative", "faithful"],
                   help="geometry_instruct_mode (default: faithful for image, creative for text)")
    p.add_argument("--highpack", action="store_true", help="4K textures + 16x faces (Quad); +1 credit")
    p.add_argument("--hd-texture", dest="hd_texture", action="store_true", help="post-process texture enhance")
    p.add_argument("--delight", action="store_true", help="remove baked lighting from textures")
    p.add_argument("--micro", action="store_true", help="micro detail (Gen-2.5-Extreme-High only)")
    p.add_argument("--no-preview", dest="preview", action="store_false", help="skip the preview render")
    p.add_argument("--bbox", nargs=3, type=int, metavar=("Y", "Z", "X"), help="bbox_condition size hint")
    p.add_argument("--seed", type=int, help="0-65535")
    p.add_argument("--no-wait", action="store_true", help="submit only; print uuid+subkey")
    p.add_argument("--interval", type=int, default=8, help="poll seconds")
    p.add_argument("--max-wait", dest="max_wait", type=int, default=1800, help="poll timeout seconds")
    p.set_defaults(preview=True)


def main():
    ap = argparse.ArgumentParser(description="Rodin (Hyper3D) Gen-2.5 wrapper")
    sub = ap.add_subparsers(dest="cmd", required=True)

    sub.add_parser("balance").set_defaults(func=cmd_balance)

    g = sub.add_parser("gen", help="submit + poll + download (end to end)")
    add_submit_flags(g); g.set_defaults(func=cmd_gen)

    s = sub.add_parser("status"); s.add_argument("--subkey", required=True); s.set_defaults(func=cmd_status)

    d = sub.add_parser("download")
    d.add_argument("--task", required=True); d.add_argument("--out")
    d.set_defaults(func=cmd_download)

    t = sub.add_parser("texture", help="retexture an existing model (/rodin_texture_only)")
    t.add_argument("--model", required=True); t.add_argument("--image", required=True)
    t.add_argument("--prompt"); t.add_argument("--seed", type=int)
    t.add_argument("--resolution", default="High", choices=["Basic", "High"])
    t.add_argument("--format", dest="geometry_file_format", default="fbx",
                   choices=["glb", "usdz", "fbx", "obj", "stl"])
    t.add_argument("--out"); t.add_argument("--no-wait", action="store_true")
    t.add_argument("--interval", type=int, default=8); t.add_argument("--max-wait", dest="max_wait", type=int, default=1800)
    t.set_defaults(func=cmd_texture)

    a = ap.parse_args()
    a.func(a)


if __name__ == "__main__":
    main()
