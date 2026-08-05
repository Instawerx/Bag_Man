"""
ARCANEON P3 — Structural Replacement generator (run in UE via bridge execute_python).
Reads the locked P0 design spec and builds ONE multi-ISM Blueprint (B_ARCANEON_Structure)
that tiles kit art onto every STRUCTURAL greybox footprint (floors/walls/catwalks/rails/
fences/ramps), matched to P0 transforms. Then hides the structural greybox (reversible;
deleted at P9). Gameplay/nav/hero/cover actors are untouched. Performant (ISM) + stable.
"""
import unreal, json, math

FLOOR="/Game/SpaceshipInterior/Meshes/SM_Scifi_Floor_02_8m"      # 10m tile
FLOOR4="/Game/SpaceshipInterior/Meshes/SM_Scifi_Floor_03_4m"     # 4m tile (ramps)
WALL="/Game/DeepWaterStation/Meshes/SM_MetalWall07"              # 13.15 x 6.54m windowed FACE panel (gappy)
WALLBACK="/Game/DeepWaterStation/Meshes/SM_Wall10"              # 13.15 x 2.06 x 6.53m SOLID backing (seals)
SUPPORT="/Game/DeepWaterStation/Meshes/SM_Support01"             # 1.55 x 1.55 x 3.2m frame column
RAIL="/Game/DeepWaterStation/Meshes/SM_Railings01"               # 2.38 x 0.83m
FENCE="/Game/SpaceshipInterior/Meshes/SM_Scifi_Fence_01_Frame"   # 5.84 x 4.95m

# role classification by label prefix
def role(lbl):
    if lbl.startswith(("Floor_Pit","Bridge_","Ring_")): return "floor"
    if lbl.startswith(("Wall_","ClimbFace_")): return "wall"
    if lbl.startswith("Parapet_"): return "rail"
    if lbl.startswith("Div_"): return "fence"
    if lbl.startswith("Ramp_"): return "ramp"
    return None

def build(p0_path):
    M=json.load(open(p0_path))
    at=unreal.AssetToolsHelpers.get_asset_tools()
    sds=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    pkg="/Game/BagMan/ArtPass"; name="B_ARCANEON_Structure"; full=pkg+"/"+name
    if unreal.EditorAssetLibrary.does_asset_exist(full): unreal.EditorAssetLibrary.delete_asset(full)
    fac=unreal.BlueprintFactory(); fac.set_editor_property("parent_class",unreal.Actor)
    bp=at.create_asset(name,pkg,None,fac)
    def add_ism(mesh_path,label):
        root=sds.k2_gather_subobject_data_for_blueprint(bp)[0]
        p=unreal.AddNewSubobjectParams(parent_handle=root,new_class=unreal.InstancedStaticMeshComponent,blueprint_context=bp)
        h,_=sds.add_new_subobject(p)
        try: sds.rename_subobject(h,unreal.Text.cast(label))
        except: pass
        c=unreal.SubobjectDataBlueprintFunctionLibrary.get_object(sds.k2_find_subobject_data_from_handle(h))
        m=unreal.EditorAssetLibrary.load_asset(mesh_path)
        c.set_editor_property("static_mesh",m); return c
    # NOTE: Walls (face panels) + Ramps are placed as INDIVIDUAL editable StaticMeshActors
    # in the level (see arcaneon_individual.py) so the operator can select/move/rotate them.
    # The BP keeps only the performant, non-hand-tuned pieces + the containment seal.
    FL=add_ism(FLOOR,"Floors")
    RA=add_ism(RAIL,"Rails"); FE=add_ism(FENCE,"Fences"); FRM=add_ism(SUPPORT,"Frames")
    BK=add_ism(WALLBACK,"WallBacking")
    GLZ=add_ism("/Engine/BasicShapes/Cube","SideGlaze"); TC=add_ism("/Engine/BasicShapes/Cube","TopCap")
    wl=[max(a.get("bounds_ext",[0,0,0])[0],a.get("bounds_ext",[0,0,0])[1])*2 for a in M["actors"] if role(a["label"])=="wall"]
    maxlen=max(wl) if wl else 1.0
    def T(x,y,z,yaw=0.0,pitch=0.0,roll=0.0,s=(1,1,1)):
        return unreal.Transform(unreal.Vector(float(x),float(y),float(z)),
                                unreal.Rotator(roll=float(roll),pitch=float(pitch),yaw=float(yaw)),
                                unreal.Vector(float(s[0]),float(s[1]),float(s[2])))
    made={"floor":0,"wall":0,"rail":0,"fence":0,"ramp":0,"frame":0,"back":0,"cap":0,"glaze":0,"seal":0}
    walls_data=[]
    def add_box(cx,cy,cz,hx,hy,hz,label):
        root=sds.k2_gather_subobject_data_for_blueprint(bp)[0]
        p=unreal.AddNewSubobjectParams(parent_handle=root,new_class=unreal.BoxComponent,blueprint_context=bp)
        h,_=sds.add_new_subobject(p)
        try: sds.rename_subobject(h,unreal.Text.cast(label))
        except: pass
        b=unreal.SubobjectDataBlueprintFunctionLibrary.get_object(sds.k2_find_subobject_data_from_handle(h))
        b.set_editor_property("box_extent",unreal.Vector(hx,hy,hz)); b.set_editor_property("relative_location",unreal.Vector(cx,cy,cz))
        try: b.set_collision_profile_name("BlockAll")
        except: pass
        b.set_editor_property("hidden_in_game",True); made["seal"]+=1
    for a in M["actors"]:
        r=role(a["label"])
        if not r: continue
        lx,ly,lz=a["loc"]; yaw=a["rot"][1]
        ex,ey,ez=a.get("bounds_ext",[100,100,100])   # world AABB half-extents
        wx,wy=ex*2,ey*2; top=lz+ez
        if r=="floor":
            mg=0.0   # floor flush to the wall line (unified boundary, no apron)
            fwx=wx+mg; fwy=wy+mg
            nx=max(1,int(round(fwx/1000.0))); ny=max(1,int(round(fwy/1000.0)))
            segx=fwx/nx; segy=fwy/ny; fx=segx/1000.0; fy=segy/1000.0   # exact-fit tiles, no seams
            for i in range(nx):
                for j in range(ny):
                    px=lx-fwx/2+segx*(i+0.5); py=ly-fwy/2+segy*(j+0.5)
                    FL.add_instance(T(px,py,top,0.0,0.0,0.0,(fx,fy,1.0)),False); made["floor"]+=1
        elif r=="wall":
            length=max(wx,wy); along_x=wx>=wy
            # yaw so the WHITE decorative face points INTO the map (verified via render: white faces -normal)
            wyaw=(0.0 if ly>0 else 180.0) if along_x else (270.0 if lx>0 else 90.0)
            nrm=(0.0, 1.0 if ly>0 else -1.0) if along_x else (1.0 if lx>0 else -1.0, 0.0)  # outward
            wh=ez*2
            nrm=(0.0, 1.0 if ly>0 else -1.0) if along_x else (1.0 if lx>0 else -1.0, 0.0)  # outward
            # NOTE: visible face panels are spawned individually in the level (arcaneon_individual.py).
            # BP keeps only the thin opaque backing (visual void-seal) below.
            # thin opaque backing just BEHIND the inward-facing white face -> seals see-through, white shows inward
            GLZ.add_instance(T(lx+nrm[0]*30,ly+nrm[1]*30,lz-ez+wh/2, wyaw, s=(length/100.0, 0.30, wh/100.0)),False); made["glaze"]+=1
            walls_data.append((lx,ly,lz,along_x,length,wh,ez))
        elif r=="rail":
            length=max(wx,wy); along_x=wx>=wy; seg=238.0; n=max(1,int(round(length/seg)))
            for i in range(n):
                off=-(n-1)*seg/2+i*seg
                if along_x: RA.add_instance(T(lx+off,ly,top,0.0),False)
                else:       RA.add_instance(T(lx,ly+off,top,90.0),False)
                made["rail"]+=1
        elif r=="fence":
            length=max(wx,wy); along_x=wx>=wy; seg=584.0; n=max(1,int(round(length/seg)))
            for i in range(n):
                off=-(n-1)*seg/2+i*seg
                if along_x: FE.add_instance(T(lx+off,ly,lz-ez,0.0),False)
                else:       FE.add_instance(T(lx,ly+off,lz-ez,90.0),False)
                made["fence"]+=1
        elif r=="ramp":
            # ramps are spawned as INDIVIDUAL editable actors in the level (arcaneon_individual.py)
            continue
    # --- CONTAINMENT SEAL: invisible tall collision over each wall (no jump/dash-out) + catch floor ---
    if walls_data:
        xs=[w[0] for w in walls_data]; ys=[w[1] for w in walls_data]
        for (lx,ly,lz,along_x,length,wh,ez) in walls_data:
            capH=(wh+6000)/2.0; cz=lz-ez+capH
            if along_x: add_box(lx,ly,cz, length/2+150, 200, capH, "SealWall")
            else:       add_box(lx,ly,cz, 200, length/2+150, capH, "SealWall")
        cx=(min(xs)+max(xs))/2; cy=(min(ys)+max(ys))/2
        ex=(max(xs)-min(xs))/2+500; ey=(max(ys)-min(ys))/2+500
        add_box(cx,cy,-900, max(ex,3000), max(ey,3000), 120, "CatchFloor")
    unreal.BlueprintEditorLibrary.compile_blueprint(bp); unreal.EditorAssetLibrary.save_asset(full)
    return {"asset":full,"made":made,"total":sum(made.values())}
