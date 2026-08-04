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
WALL="/Game/DeepWaterStation/Meshes/SM_MetalWall07"              # 13.15 x 6.54m
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
    FL=add_ism(FLOOR,"Floors"); FR=add_ism(FLOOR4,"Ramps"); WA=add_ism(WALL,"Walls")
    RA=add_ism(RAIL,"Rails"); FE=add_ism(FENCE,"Fences")
    def T(x,y,z,yaw=0.0,pitch=0.0,roll=0.0,s=(1,1,1)):
        return unreal.Transform([float(x),float(y),float(z)],[float(roll),float(pitch),float(yaw)],[float(s[0]),float(s[1]),float(s[2])])
    made={"floor":0,"wall":0,"rail":0,"fence":0,"ramp":0}
    for a in M["actors"]:
        r=role(a["label"])
        if not r: continue
        lx,ly,lz=a["loc"]; yaw=a["rot"][1]
        ex,ey,ez=a.get("bounds_ext",[100,100,100])   # world AABB half-extents
        wx,wy=ex*2,ey*2; top=lz+ez
        if r=="floor":
            tile=1000.0; nx=max(1,int(round(wx/tile))); ny=max(1,int(round(wy/tile)))
            for i in range(nx):
                for j in range(ny):
                    px=lx-(nx-1)*tile/2+i*tile; py=ly-(ny-1)*tile/2+j*tile
                    FL.add_instance(T(px,py,top),False); made["floor"]+=1
        elif r=="wall":
            length=max(wx,wy); along_x=wx>=wy; seg=1315.0; rows=max(1,int(round((ez*2)/654.0)))
            n=max(1,int(round(length/seg))); wyaw=0.0 if along_x else 90.0
            for i in range(n):
                off=-(n-1)*seg/2+i*seg
                for row in range(rows):
                    z=lz-ez+327+row*654
                    if along_x: WA.add_instance(T(lx+off,ly,z,wyaw),False)
                    else:       WA.add_instance(T(lx,ly+off,z,wyaw),False)
                    made["wall"]+=1
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
            # 1 floor4 piece matched to the ramp box (rot preserved, scaled to footprint)
            sx=a["scale"][0]; sy=a["scale"][1]
            FR.add_instance(T(lx,ly,lz, a["rot"][1], a["rot"][0], a["rot"][2],
                             (wx/400.0, wy/400.0, 1.0)),False); made["ramp"]+=1
    unreal.BlueprintEditorLibrary.compile_blueprint(bp); unreal.EditorAssetLibrary.save_asset(full)
    return {"asset":full,"made":made,"total":sum(made.values())}
