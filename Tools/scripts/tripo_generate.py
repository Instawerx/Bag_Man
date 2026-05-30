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


def submit_text_task(key, prompt, neg_prompt, model_version, face_limit):
    """POST /task type=text_to_model. Returns task_id.
    Body fields verbatim from TripoProvider.cpp:423-432 + 391-421."""
    body = {
        "type": "text_to_model",
        "prompt": prompt[:PROMPT_MAX],
        "texture": True,
        "pbr": True,
        "model_version": model_version,
    }
    if neg_prompt:
        body["negative_prompt"] = neg_prompt[:NEG_PROMPT_MAX]
    if face_limit:
        body["face_limit"] = int(face_limit)
    j = _request("POST", f"{TRIPO_BASE}/task", key, body)
    if j.get("code") != 0:
        sys.exit(f"FATAL: task submit failed: {j}")
    task_id = j["data"]["task_id"]
    print(f"[tripo] task submitted: {task_id}")
    return task_id


def submit_image_task(key, image_url, model_version, face_limit):
    """POST /task type=image_to_model via image URL (fallback path).
    Body verbatim from TripoProvider.cpp:434-453."""
    body = {
        "type": "image_to_model",
        "file": {"type": "jpg", "url": image_url},
        "texture": True,
        "pbr": True,
        "model_version": model_version,
    }
    if face_limit:
        body["face_limit"] = int(face_limit)
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


def pick_model_url(data):
    """Output URL priority: pbr_model > model > base_model.
    Verbatim from TripoProvider.cpp:93-119."""
    out = data.get("output", {})
    for field in ("pbr_model", "model", "base_model"):
        url = out.get(field)
        if url:
            print(f"[tripo] using output.{field}")
            return url
    sys.exit(f"FATAL: no model URL in output: {out}")


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
                    help="Tripo model version (default v2.5; the sibling's default)")
    ap.add_argument("--face-limit", type=int, default=0,
                    help="optional polycount cap (0 = Tripo default)")
    ap.add_argument("--balance-only", action="store_true",
                    help="just check balance + exit (the cheap probe)")
    args = ap.parse_args()

    key = read_key(INI_PATH)
    check_balance(key)
    if args.balance_only:
        return

    if not args.prompt and not args.image_url:
        sys.exit("FATAL: need --prompt or --image-url")

    if args.image_url:
        task_id = submit_image_task(key, args.image_url, args.model_version, args.face_limit)
    else:
        task_id = submit_text_task(key, args.prompt, args.negative_prompt,
                                   args.model_version, args.face_limit)

    data = poll(key, task_id)
    url = pick_model_url(data)
    dest = os.path.join(OUT_DIR, f"{args.name}.glb")
    download(url, dest)
    print(f"\n[tripo] DONE -- mesh at: {dest}")
    print("[tripo] next: import editor-natively via AssetImportTask "
          "to /Game/BagMan/Equipment/Generated/")


if __name__ == "__main__":
    main()
