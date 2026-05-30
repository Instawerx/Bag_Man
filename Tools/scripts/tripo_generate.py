#!/usr/bin/env python3
"""
tripo_generate.py -- standalone Tripo v2 text-to-3D (and image-to-3D) client
for the Bag_Man project. Reads the Tripo key from AgentIntegrationKit.ini,
submits a generation task, polls to completion, downloads the .glb mesh.
Betide-independent: uses the Tripo key directly.

Anchored verbatim to Plugins/AgentIntegrationKit/.../TripoProvider.cpp
(the working sibling) + a proven /balance probe (HTTP 200, 230 credits).

Usage:
  python tripo_generate.py --prompt "..." --name SM_AFL_PulseCarbine_v1 [opts]
  python tripo_generate.py --image-url "https://..." --name ... [opts]

Output: downloads <name>.glb to Tools/Generated/ and prints the path.
The EDITOR-NATIVE IMPORT is a SEPARATE step (run in-editor via the MCP),
NOT done by this script -- this script only talks to Tripo + saves the file.
"""

import argparse
import configparser
import json
import os
import sys
import time
import urllib.request
import urllib.error

TRIPO_BASE = "https://api.tripo3d.ai/v2/openapi"
INI_PATH = r"C:\Dev\Bag_Man\Saved\Config\WindowsEditor\AgentIntegrationKit.ini"
OUT_DIR = r"C:\Dev\Bag_Man\Tools\Generated"

PROMPT_MAX = 1024          # Tripo hard cap (TripoProvider.cpp:425)
NEG_PROMPT_MAX = 255       # (:426)
POLL_INTERVAL_S = 4        # courteous async polling
POLL_TIMEOUT_S = 600       # 10 min ceiling; text_to_model usually < 3 min
TERMINAL_FAIL = {"failed", "banned", "expired", "cancelled"}  # beyond success


def read_key(ini_path):
    """Extract TripoApiKey from the AIK ini. The ini has non-standard
    sections; do a tolerant line-scan rather than strict configparser."""
    if not os.path.isfile(ini_path):
        sys.exit(f"FATAL: AIK ini not found at {ini_path}")
    with open(ini_path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if line.startswith("TripoApiKey"):
                # form: TripoApiKey=tsk_xxxx
                _, _, val = line.partition("=")
                key = val.strip()
                if key:
                    return key
    sys.exit("FATAL: TripoApiKey not found in AIK ini")


def _request(method, url, key, body=None):
    """Single HTTP call. Returns parsed JSON dict. Raises on transport error.
    Tripo returns HTTP 200 even for API errors -- caller MUST check code==0."""
    data = json.dumps(body).encode("utf-8") if body is not None else None
    req = urllib.request.Request(url, data=data, method=method)
    req.add_header("Authorization", f"Bearer {key}")
    if body is not None:
        req.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        # surface the body -- Tripo's error envelope is informative
        detail = e.read().decode("utf-8", errors="replace")
        sys.exit(f"FATAL: HTTP {e.code} from {url}\n{detail}")
    except urllib.error.URLError as e:
        sys.exit(f"FATAL: network error to {url}: {e.reason}")


def check_balance(key):
    """Proven endpoint: /v2/openapi/user/balance -> {code:0,data:{balance,frozen}}"""
    j = _request("GET", f"{TRIPO_BASE}/user/balance", key)
    if j.get("code") != 0:
        sys.exit(f"FATAL: balance check failed: {j}")
    bal = j["data"]["balance"]
    print(f"[tripo] balance: {bal} credits (frozen {j['data'].get('frozen', 0)})")
    if bal <= 0:
        sys.exit("FATAL: zero Tripo balance -- cannot generate")
    return bal


def _apply_common_extras(body, extras):
    """Apply optional fields to a text/image_to_model body if present in extras.
    Only sets keys that are explicitly provided (None = omit, defaults preserved).
    Mirrors TripoProvider.cpp AddCommonGenerationFields (L391-421)."""
    if extras is None:
        return
    for k in ("generate_parts", "auto_size", "smart_low_poly"):
        if extras.get(k) is True:
            body[k] = True
    for k in ("texture_quality", "geometry_quality"):
        v = extras.get(k)
        if v:
            body[k] = v
    if extras.get("model_seed") is not None:
        body["model_seed"] = int(extras["model_seed"])


def submit_text_task(key, prompt, neg_prompt, model_version, face_limit,
                     texture=True, pbr=True, extras=None):
    """POST /task type=text_to_model. Returns task_id.
    Body fields verbatim from TripoProvider.cpp:423-432 + 391-421."""
    body = {
        "type": "text_to_model",
        "prompt": prompt[:PROMPT_MAX],
        "texture": bool(texture),
        "pbr": bool(pbr),
        "model_version": model_version,
    }
    if neg_prompt:
        body["negative_prompt"] = neg_prompt[:NEG_PROMPT_MAX]
    if face_limit:
        body["face_limit"] = int(face_limit)
    _apply_common_extras(body, extras)
    print(f"[tripo] submitting text_to_model body keys: {sorted(body.keys())}")
    j = _request("POST", f"{TRIPO_BASE}/task", key, body)
    if j.get("code") != 0:
        sys.exit(f"FATAL: task submit failed: {j}")
    task_id = j["data"]["task_id"]
    print(f"[tripo] task submitted: {task_id}")
    return task_id


def submit_image_task(key, image_url, model_version, face_limit,
                      texture=True, pbr=True, extras=None):
    """POST /task type=image_to_model via image URL (fallback path).
    Body verbatim from TripoProvider.cpp:434-453."""
    body = {
        "type": "image_to_model",
        "file": {"type": "jpg", "url": image_url},
        "texture": bool(texture),
        "pbr": bool(pbr),
        "model_version": model_version,
    }
    if face_limit:
        body["face_limit"] = int(face_limit)
    _apply_common_extras(body, extras)
    print(f"[tripo] submitting image_to_model body keys: {sorted(body.keys())}")
    j = _request("POST", f"{TRIPO_BASE}/task", key, body)
    if j.get("code") != 0:
        sys.exit(f"FATAL: image task submit failed: {j}")
    task_id = j["data"]["task_id"]
    print(f"[tripo] image task submitted: {task_id}")
    return task_id


def poll(key, task_id):
    """GET /task/{id} until data.status==success or terminal-fail/timeout.
    Status + output locations verbatim from TripoProvider.cpp:62-119."""
    start = time.time()
    last_progress = -1
    while True:
        j = _request("GET", f"{TRIPO_BASE}/task/{task_id}", key)
        if j.get("code") != 0:
            sys.exit(f"FATAL: poll failed: {j}")
        data = j["data"]
        status = data.get("status")
        progress = data.get("progress", 0)
        if progress != last_progress:
            print(f"[tripo] {status} ({progress}%)")
            last_progress = progress
        if status == "success":
            return data
        if status in TERMINAL_FAIL:
            sys.exit(f"FATAL: task {status}: {data.get('error_msg', '(no msg)')}")
        if time.time() - start > POLL_TIMEOUT_S:
            sys.exit(f"FATAL: poll timeout after {POLL_TIMEOUT_S}s (last status {status})")
        time.sleep(POLL_INTERVAL_S)


def _looks_like_url(v):
    return isinstance(v, str) and (v.startswith("http://") or v.startswith("https://"))


def _scan_for_parts(out):
    """Search output dict for a parts-shape: a list of URLs OR a list of
    {url|model|...} dicts. Returns a list of (label, url) tuples or None.
    Pillar-5 diagnostic for when the standard model URLs are absent."""
    candidates = []
    for k, v in out.items():
        if not isinstance(v, list) or not v:
            continue
        # Case 1: list of plain URL strings
        if all(_looks_like_url(x) for x in v):
            return [(f"{k}_{i:02d}", u) for i, u in enumerate(v)]
        # Case 2: list of dicts -- pull any URL-shaped value out
        if all(isinstance(x, dict) for x in v):
            parts = []
            for i, item in enumerate(v):
                name = item.get("name") or item.get("part_name") or f"{k}_{i:02d}"
                url = None
                for url_key in ("pbr_model", "model", "base_model", "url", "download_url"):
                    if _looks_like_url(item.get(url_key)):
                        url = item[url_key]
                        break
                if url:
                    parts.append((str(name), url))
            if parts:
                return parts
    return None


def pick_model_url(data):
    """Output URL priority: pbr_model > model > base_model (single-mesh case).
    If absent, scan for a parts shape (list of URLs / list of dicts) and
    return that as a list of (label, url) tuples. If THAT is absent too,
    dump the real data.output for inspection and exit (Pillar 5)."""
    out = data.get("output", {})
    for field in ("pbr_model", "model", "base_model"):
        url = out.get(field)
        if url:
            print(f"[tripo] using output.{field} (single-mesh shape)")
            return url
    # Diagnostic dump FIRST so the operator sees the real shape no matter what
    print("[tripo] WARN: no pbr_model/model/base_model URL in output.")
    print("[tripo] full data.output JSON for inspection:")
    print(json.dumps(out, indent=2)[:8000])  # cap at 8KB just in case
    parts = _scan_for_parts(out)
    if parts:
        print(f"[tripo] detected parts shape: {len(parts)} parts")
        for label, url in parts:
            print(f"[tripo]   part {label}: {url[:80]}...")
        return parts
    sys.exit("FATAL: no model URL or parts shape recognized in output")


def download(url, dest_path):
    """Download the pre-signed mesh URL (expires ~24h -- download promptly)."""
    os.makedirs(os.path.dirname(dest_path), exist_ok=True)
    print(f"[tripo] downloading -> {dest_path}")
    try:
        with urllib.request.urlopen(url, timeout=120) as resp, open(dest_path, "wb") as f:
            f.write(resp.read())
    except (urllib.error.HTTPError, urllib.error.URLError) as e:
        sys.exit(f"FATAL: download failed: {e}")
    size = os.path.getsize(dest_path)
    if size == 0:
        sys.exit("FATAL: downloaded file is 0 bytes")
    print(f"[tripo] downloaded {size} bytes")
    return dest_path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--prompt", help="text-to-3d prompt")
    ap.add_argument("--image-url", help="image-to-3d source URL (fallback)")
    ap.add_argument("--negative-prompt", default="")
    ap.add_argument("--name", required=True, help="output basename (no ext)")
    ap.add_argument("--model-version", default="v2.5",
                    help="Tripo model version (default v2.5; valid values are dated, e.g. v3.1-20260211)")
    ap.add_argument("--face-limit", type=int, default=0,
                    help="optional polycount cap (0 = Tripo default = adaptive)")
    # quality knobs (schema descriptors at TripoProvider.cpp:668-684)
    ap.add_argument("--texture-quality", choices=["standard", "detailed"], default=None,
                    help="texture_quality: standard or detailed (default = omit)")
    ap.add_argument("--geometry-quality", choices=["standard", "detailed"], default=None,
                    help="geometry_quality: standard or detailed (v3.0+; default = omit)")
    ap.add_argument("--auto-size", action="store_true",
                    help="auto_size: scale to real-world dimensions (meters)")
    ap.add_argument("--smart-low-poly", action="store_true",
                    help="smart_low_poly: hand-crafted low-poly topology")
    ap.add_argument("--model-seed", type=int, default=None,
                    help="model_seed: int RNG for geometry reproducibility")
    # segmentation + interlock
    ap.add_argument("--generate-parts", action="store_true",
                    help="generate_parts: segmented editable parts (FORCES texture=false, pbr=false)")
    ap.add_argument("--no-texture", action="store_true",
                    help="explicitly disable texture (default = true)")
    ap.add_argument("--no-pbr", action="store_true",
                    help="explicitly disable PBR (default = true)")
    ap.add_argument("--balance-only", action="store_true",
                    help="just check balance + exit (the cheap probe)")
    args = ap.parse_args()

    key = read_key(INI_PATH)
    check_balance(key)
    if args.balance_only:
        return

    if not args.prompt and not args.image_url:
        sys.exit("FATAL: need --prompt or --image-url")

    # Resolve texture/pbr with the generate_parts INTERLOCK
    # (TripoProvider.cpp:678 -- "Segmented parts (requires texture=false, pbr=false)")
    texture = not args.no_texture
    pbr = not args.no_pbr
    if args.generate_parts:
        if texture or pbr:
            print("[tripo] generate_parts ON -> forcing texture=false, pbr=false "
                  "(Tripo constraint: segmented output is untextured geometry; "
                  "texture via a separate texture_model task afterward)")
        texture = False
        pbr = False

    extras = {
        "generate_parts": args.generate_parts,
        "auto_size": args.auto_size,
        "smart_low_poly": args.smart_low_poly,
        "texture_quality": args.texture_quality,
        "geometry_quality": args.geometry_quality,
        "model_seed": args.model_seed,
    }

    if args.image_url:
        task_id = submit_image_task(key, args.image_url, args.model_version,
                                    args.face_limit, texture=texture, pbr=pbr, extras=extras)
    else:
        task_id = submit_text_task(key, args.prompt, args.negative_prompt,
                                   args.model_version, args.face_limit,
                                   texture=texture, pbr=pbr, extras=extras)

    data = poll(key, task_id)
    result = pick_model_url(data)
    # result is either a single URL string OR a list of (label, url) tuples (parts)
    if isinstance(result, list):
        saved = []
        for label, url in result:
            safe = "".join(c if c.isalnum() or c in "._-" else "_" for c in label)
            dest = os.path.join(OUT_DIR, f"{args.name}_part_{safe}.glb")
            download(url, dest)
            saved.append(dest)
        print(f"\n[tripo] DONE -- {len(saved)} parts saved:")
        for p in saved:
            print(f"  {p}")
    else:
        dest = os.path.join(OUT_DIR, f"{args.name}.glb")
        download(result, dest)
        print(f"\n[tripo] DONE -- mesh at: {dest}")
    print("[tripo] next: import editor-natively via AssetImportTask "
          "to /Game/BagMan/Equipment/Generated/")


if __name__ == "__main__":
    main()
