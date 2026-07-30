"""Tripo animate_prerigcheck + animate_rig on an existing gen task. Reuses tripo_generate helpers."""
import os, sys, json
import tripo_generate as tg

INI=r"C:\Dev\Bag_Man\Saved\Config\WindowsEditor\AgentIntegrationKit.ini"
OUT=r"C:\Dev\Bag_Man\Tools\Generated"

def main():
    task_id = sys.argv[1]
    name    = sys.argv[2] if len(sys.argv)>2 else "IRONICS_rigged"
    spec    = sys.argv[3] if len(sys.argv)>3 else "mixamo"
    key=tg.read_key(INI)

    # 1. prerig check (free)
    body={"type":"animate_prerigcheck","original_model_task_id":task_id}
    r=tg._request("POST",f"{tg.TRIPO_BASE}/task",key,body=body)
    if r.get("code")!=0: sys.exit(f"FATAL prerigcheck submit: {r}")
    ptid=r["data"]["task_id"]; print("[rig] prerigcheck task:",ptid)
    d=tg.poll(key,ptid)
    out=d.get("output",{})
    riggable=out.get("riggable", d.get("riggable"))
    rig_type=out.get("rig_type", d.get("rig_type"))
    print(f"[rig] riggable={riggable} rig_type={rig_type}")
    print("[rig] prerig output:", json.dumps(out)[:500])
    if riggable is False:
        sys.exit("[rig] NOT riggable — stop")

    # 2. animate_rig
    body={"type":"animate_rig","original_model_task_id":task_id,
          "out_format":"fbx","spec":spec,"model_version":"v2.5-20260210"}
    r=tg._request("POST",f"{tg.TRIPO_BASE}/task",key,body=body)
    if r.get("code")!=0: sys.exit(f"FATAL animate_rig submit: {r}")
    rtid=r["data"]["task_id"]; print("[rig] animate_rig task:",rtid,"spec:",spec)
    d=tg.poll(key,rtid)
    out=d.get("output",{})
    print("[rig] rig output keys:", list(out.keys()))
    # find the rigged model URL
    url=None
    for k in ("model","rigged_model","pbr_model","base_model"):
        if out.get(k) and isinstance(out[k],str) and out[k].startswith("http"):
            url=out[k]; print(f"[rig] using output.{k}"); break
    if not url:
        print("[rig] full output:", json.dumps(out)[:2000]); sys.exit("FATAL: no rigged model url")
    ext=".fbx" if ".fbx" in url.lower() else (".glb" if ".glb" in url.lower() else ".fbx")
    dest=os.path.join(OUT,name+ext)
    tg.download(url,dest)
    print(f"[rig] DONE rigged mesh: {dest}")
    print(f"[rig] rig task_id: {rtid}")

if __name__=="__main__":
    main()
