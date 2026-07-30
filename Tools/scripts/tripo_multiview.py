"""Tripo multiview_to_model -- reuses tripo_generate helpers.
Files order = [front, left, back, right] (front required, others omittable).
"""
import os, sys, json, time, urllib.request, urllib.error
import tripo_generate as tg

INI = r"C:\Dev\Bag_Man\Saved\Config\WindowsEditor\AgentIntegrationKit.ini"
OUT_DIR = r"C:\Dev\Bag_Man\Tools\Generated"
DL = r"C:\Users\tabor\Downloads"

VIEWS = [  # order matters: front, left, back, right
    ("front", os.path.join(DL, "IR Slug Front.png")),
    ("left",  os.path.join(DL, "IR Slug Left.png")),
    ("back",  os.path.join(DL, "IR Slug Rear.png")),
    ("right", os.path.join(DL, "IR Slug Right.png")),
]

def main():
    name = sys.argv[1] if len(sys.argv) > 1 else "IRONICS_Blank_MV"
    model_version = sys.argv[2] if len(sys.argv) > 2 else "v3.1-20260211"
    use_quad = "--quad" in sys.argv
    use_detailed = "--detailed" in sys.argv

    key = tg.read_key(INI)
    # balance
    try:
        bal = tg._request("GET", f"{tg.TRIPO_BASE}/user/balance", key)
        print("[tripo] balance:", bal.get("data"))
    except Exception as e:
        print("[tripo] balance check skipped:", e)

    files = []
    for label, path in VIEWS:
        token, ext = tg.upload_image(key, path)
        files.append({"type": ext, "file_token": token})
        print(f"[tripo] {label}: token ok ({ext})")

    body = {
        "type": "multiview_to_model",
        "files": files,
        "model_version": model_version,
        "pbr": True,
        "texture": True,
    }
    if use_quad:
        body["quad"] = True
    if use_detailed:
        body["geometry_quality"] = "detailed"
        body["texture_quality"] = "detailed"

    print("[tripo] submitting multiview_to_model body keys:", sorted(body.keys()),
          "| model_version:", model_version, "| quad:", use_quad, "| detailed:", use_detailed)
    resp = tg._request("POST", f"{tg.TRIPO_BASE}/task", key, body=body)
    if resp.get("code") != 0:
        sys.exit(f"FATAL: multiview submit failed: {resp}")
    task_id = resp["data"]["task_id"]
    print("[tripo] multiview task submitted:", task_id)

    data = tg.poll(key, task_id)
    url = tg.pick_model_url(data)
    if isinstance(url, list):
        sys.exit("FATAL: got parts shape, expected single mesh")
    ext = ".fbx" if ".fbx" in url.lower() else (".glb" if ".glb" in url.lower() else ".glb")
    dest = os.path.join(OUT_DIR, name + ext)
    tg.download(url, dest)
    print(f"[tripo] DONE -- multiview mesh at: {dest}")
    print(f"[tripo] task_id: {task_id}")

if __name__ == "__main__":
    main()
