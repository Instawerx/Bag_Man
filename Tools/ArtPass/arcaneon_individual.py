"""
ARCANEON — individual editable structure (run in UE via bridge execute_python).
Spawns the WALL face panels and RAMPS as individual StaticMeshActors in the level
(organized in folders) so the operator can select / move / rotate / flip each piece.
The rest of the structure (floor/rails/fences/opaque backing/containment seal) stays
in the performant B_ARCANEON_Structure ISM Blueprint.

- Walls: SM_MetalWall07, exact-fit tiled per wall, grouped in ARCANEON_Art/Walls/<Wall>.
         Facing FLIPPED 180 vs the old ISM default (operator: panels were inside-out).
- Ramps: SM_Scifi_Floor_03_4m, placed on CLEAN bottom->top geometry (proper single-axis
         incline, no messy roll), grouped in ARCANEON_Art/Ramps.
Idempotent: clears prior ARCANEON_Art/Walls + /Ramps actors before respawning.
"""
import unreal, json, math

WALLMESH="/Game/DeepWaterStation/Meshes/SM_MetalWall07"     # windowed face panel, ~13.15 x 6.54m
RAMPMESH="/Game/SpaceshipInterior/Meshes/SM_Scifi_Floor_03_4m"

# CLEAN ramp geometry (bottom -> top), from the validated greybox definition.
RAMPS=[
 ("Ramp_Pit_BridgeA_E", (4650,2400,0),  (3500,2400,800)),
 ("Ramp_Pit_BridgeA_W", (-4650,2400,0), (-3500,2400,800)),
 ("Ramp_Pit_BridgeB_E", (4650,-2400,0), (3500,-2400,800)),
 ("Ramp_Pit_BridgeB_W", (-4650,-2400,0),(-3500,-2400,800)),
 ("Ramp_BridgeA_RingNE",(3200,2600,800), (4100,3200,1600)),
 ("Ramp_BridgeA_RingNW",(-3200,2600,800),(-4100,3200,1600)),
 ("Ramp_BridgeB_RingSE",(3200,-2600,800),(4100,-3200,1600)),
 ("Ramp_BridgeB_RingSW",(-3200,-2600,800),(-4100,-3200,1600)),
]
RAMP_WIDTH=600.0   # cm (6m walkable width)

def _role_wall(lbl): return lbl.startswith(("Wall_","ClimbFace_"))

def _mesh_size(path):
    m=unreal.EditorAssetLibrary.load_asset(path)
    b=m.get_bounds().box_extent
    return m, max(b.x*2,1.0), max(b.y*2,1.0), max(b.z*2,1.0)

def place(p0_path):
    M=json.load(open(p0_path))
    eas=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world() or unreal.EditorLevelLibrary.get_editor_world()
    assert w and w.get_name()=="L_Arena_04", "WRONG LEVEL: %s"%(w.get_name() if w else None)

    # --- idempotent clear ---
    removed=0
    for a in list(eas.get_all_level_actors()):
        try: fp=str(a.get_folder_path())
        except: fp=""
        if fp.startswith("ARCANEON_Art/Walls") or fp.startswith("ARCANEON_Art/Ramps"):
            eas.destroy_actor(a); removed+=1

    xs=[]; ys=[]; made={"wall":0,"ramp":0}

    # --- WALLS: individual exact-fit panels, per-wall folder, flipped facing ---
    wmesh,wsx,wsy,wsz=_mesh_size(WALLMESH)   # panel base size (cm)
    for a in M["actors"]:
        lbl=a["label"]
        if not _role_wall(lbl): continue
        lx,ly,lz=a["loc"]; ex,ey,ez=a.get("bounds_ext",[100,100,100])
        wx,wy=ex*2,ey*2; length=max(wx,wy); along_x=wx>=wy; wh=ez*2
        # FLIPPED 180 from old ISM default so the decorative (white) face reads INWARD
        wyaw=(180.0 if ly>0 else 0.0) if along_x else (90.0 if lx>0 else 270.0)
        rows=max(1,int(round(wh/wsz))); n=max(1,int(round(length/wsx))); seg=length/n; fit=seg/wsx
        for i in range(n):
            off=-length/2+seg*(i+0.5)
            for row in range(rows):
                z=lz-ez+wsz/2+row*wsz
                px,py=(lx+off,ly) if along_x else (lx,ly+off)
                act=eas.spawn_actor_from_object(wmesh, unreal.Vector(px,py,z),
                        unreal.Rotator(roll=0.0,pitch=0.0,yaw=wyaw))
                act.set_actor_scale3d(unreal.Vector(fit,1.0,1.0))
                act.set_actor_label("%s_%d_%d"%(lbl,i,row))
                act.set_folder_path("ARCANEON_Art/Walls/%s"%lbl)
                made["wall"]+=1
                xs+=[px-length/2, px+length/2]; ys+=[py-length/2, py+length/2]

    # --- RAMPS: individual, clean single-axis incline ---
    rmesh,rsx,rsy,rsz=_mesh_size(RAMPMESH)
    for name,(bx,by,bz),(tx,ty,tz) in RAMPS:
        dx,dy,dz=tx-bx,ty-by,tz-bz
        L=math.sqrt(dx*dx+dy*dy+dz*dz); horiz=math.sqrt(dx*dx+dy*dy)
        slope=math.degrees(math.atan2(dz,horiz)); yaw=math.degrees(math.atan2(dy,dx))
        mid=((bx+tx)/2.0,(by+ty)/2.0,(bz+tz)/2.0)
        act=eas.spawn_actor_from_object(rmesh, unreal.Vector(*mid),
                unreal.Rotator(roll=0.0,pitch=-slope,yaw=yaw))
        act.set_actor_scale3d(unreal.Vector(L/rsx, RAMP_WIDTH/rsy, 1.0))
        act.set_actor_label(name); act.set_folder_path("ARCANEON_Art/Ramps")
        made["ramp"]+=1
        xs+=[bx,tx]; ys+=[by,ty]

    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    ext={"x":[round(min(xs),0),round(max(xs),0)],"y":[round(min(ys),0),round(max(ys),0)]} if xs else {}
    return {"removed":removed,"made":made,"extent":ext,
            "ramp_slopes":[round(math.degrees(math.atan2((t[2]-b[2]),
                math.sqrt((t[0]-b[0])**2+(t[1]-b[1])**2))),1) for _,b,t in RAMPS]}
