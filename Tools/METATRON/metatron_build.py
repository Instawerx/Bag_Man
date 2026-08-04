"""
METATRON — UE-side greybox BUILDER (run in Unreal via the editor bridge / execute_python).

Reads a metatron_layout manifest and instantiates the flow greybox DETERMINISTICALLY:
courtyard junction pads + tube-corridor floors x 3 strata, vertical connectors (ramps +
NavLinkProxies), 6-fold side-tagged spawns, extraction objectives, and a central
orientation-landmark proxy. Shared engine mesh + SIMPLE collision (nav-gatherable, ISM-
suitable) — carries the Duel_01 nav lesson (do NOT rely on complex render collision).

Greybox interpretation (noted in the brief): true FoL circles overlap (radius=spacing),
so distinct tubes+courtyards are a hex node+tube GRAPH for flow; the overlapping-arc
tessellation is an art/geometry refinement. Layout (hex positions, strata, sectors,
routes, spawns, objectives) is faithful to the manifest.

Callable: build(manifest_path)  -> returns summary dict (printed).
"""
import unreal, json, math

def _v(x, y, z): return unreal.Vector(float(x), float(y), float(z))

def build(manifest_path):
    with open(manifest_path) as f:
        M = json.load(f)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    cube = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube")

    # cleanup prior METATRON output (deterministic re-runnability — §12 gate)
    removed = 0
    for a in list(eas.get_all_level_actors()):
        fp = str(a.get_folder_path())
        if fp.startswith("METATRON"):
            eas.destroy_actor(a); removed += 1
    strata = M["params"]["strata"]              # name -> z (cm)
    R = M["params"]["base_radius"]

    def box(label, loc, scale, folder, rot=(0, 0, 0)):
        a = eas.spawn_actor_from_object(cube, _v(*loc), unreal.Rotator(rot[0], rot[1], rot[2]))
        a.set_actor_scale3d(_v(*scale)); a.set_actor_label(label); a.set_folder_path(folder)
        return a

    made = {"courtyard": 0, "tube": 0, "ramp": 0, "navlink": 0, "wall": 0}

    # ---- COURTYARD junction pads (per node) ----
    cpad = 7.0  # 14 m pad (radius 7 m) -> distinct from tubes
    for n in M["nodes"]:
        x, y, z = n["xyz"]
        box("CY_%d_%d_%s" % (n["cell"][0], n["cell"][1], n["stratum"]),
            (x, y, z - 15), (cpad * 2, cpad * 2, 0.3), "METATRON/Floors/%s" % n["stratum"])
        made["courtyard"] += 1

    # ---- TUBE corridor floors (per edge) ----
    for e in M["edges"]:
        ax, ay = e["a"]; bx, by = e["b"]
        # world endpoints (axial->xy already baked into node xyz; recompute from mid + cells)
        Ax = R * (ax + ay * 0.5); Ay = R * (math.sqrt(3) / 2 * ay)
        Bx = R * (bx + by * 0.5); By = R * (math.sqrt(3) / 2 * by)
        mx, my, z = e["mid"]
        length = math.hypot(Bx - Ax, By - Ay)      # == R
        yaw = math.degrees(math.atan2(By - Ay, Bx - Ax))
        bore_m = e["bore"] / 100.0
        box("TB_%d_%d__%d_%d_%s" % (ax, ay, bx, by, e["stratum"]),
            (mx, my, z - 15), (length / 100.0, bore_m, 0.3),
            "METATRON/Tubes/%s" % e["stratum"], rot=(0, 0, yaw))
        made["tube"] += 1

    # ---- VERTICAL CONNECTORS (ramps + navlinks between consecutive strata) ----
    zsorted = sorted(strata.items(), key=lambda kv: kv[1])   # undercroft, primary, crown
    def navlink(pf, pt, label):
        mid = ((pf[0] + pt[0]) / 2, (pf[1] + pt[1]) / 2, (pf[2] + pt[2]) / 2)
        nlp = eas.spawn_actor_from_class(unreal.NavLinkProxy, _v(*mid), unreal.Rotator(0, 0, 0))
        nl = unreal.NavigationLink()
        nl.set_editor_property("left", _v(pf[0] - mid[0], pf[1] - mid[1], pf[2] - mid[2]))
        nl.set_editor_property("right", _v(pt[0] - mid[0], pt[1] - mid[1], pt[2] - mid[2]))
        nl.set_editor_property("direction", unreal.NavLinkDirection.BOTH_WAYS)
        nlp.set_editor_property("point_links", [nl]); nlp.set_actor_label(label)
        nlp.set_folder_path("METATRON/Connectors")
        made["navlink"] += 1
    for cn in M["connectors"]:
        x, y = cn["xy"]
        for i in range(len(zsorted) - 1):
            z0 = zsorted[i][1]; z1 = zsorted[i + 1][1]
            # ramp within the courtyard footprint
            bx2, by2, bz2 = x - 650, y, z0
            tx2, ty2, tz2 = x + 650, y, z1
            dx, dy, dz = tx2 - bx2, ty2 - by2, tz2 - bz2
            L = math.sqrt(dx * dx + dy * dy + dz * dz)
            yaw = math.degrees(math.atan2(dy, dx))
            slope = math.degrees(math.atan2(dz, math.hypot(dx, dy)))
            a = eas.spawn_actor_from_object(cube, _v((bx2 + tx2) / 2, (by2 + ty2) / 2, (bz2 + tz2) / 2),
                                            unreal.Rotator(-slope, yaw, 0))
            a.set_actor_scale3d(_v(L / 100.0, 4.0, 0.3)); a.set_actor_label("RMP_%d_%d_%d" % (cn["cell"][0], cn["cell"][1], i))
            a.set_folder_path("METATRON/Connectors"); made["ramp"] += 1
            navlink((x, y, z0 + 20), (x, y, z1 + 20), "NL_%d_%d_%d" % (cn["cell"][0], cn["cell"][1], i))

    # ---- SPAWNS: two opposite sectors (2-team), 8/side, side-tagged ----
    def cont(side):
        c = unreal.GameplayTagContainer(); c.import_text('(GameplayTags=((TagName="AFL.Spawn.Side.%d")))' % side); return c
    spawns = {s["sector"]: s for s in M["spawns"]}
    # pick opposite sectors (1 and 4 if present; else the two most separated)
    secs = sorted(spawns.keys())
    a_sec = 1 if 1 in spawns else secs[0]
    b_sec = 4 if 4 in spawns else secs[len(secs) // 2]
    zp = strata["primary"] + 100
    for side, sec in ((0, a_sec), (1, b_sec)):
        cx, cy, _ = spawns[sec]["xyz"]
        for i in range(8):
            off = (i - 3.5) * 180
            s = eas.spawn_actor_from_class(unreal.LyraPlayerStart,
                    _v(cx + (off if abs(cx) < abs(cy) else 0), cy + (0 if abs(cx) < abs(cy) else off), zp),
                    unreal.Rotator(0, 0, 0))
            s.set_actor_label("Start_S%d_%d" % (side, i)); s.set_folder_path("METATRON/Spawns/Side%d" % side)
            s.set_editor_property("start_point_tags", cont(side))

    # ---- OBJECTIVES: extraction zones at candidates (central + up to 2 peripheral) ----
    ez = unreal.EditorAssetLibrary.load_blueprint_class("/Game/BagMan/Extraction/B_AFL_ExtractionZone")
    for j, o in enumerate(M["objectives"][:3]):
        x, y, z = o["xyz"]
        a = eas.spawn_actor_from_class(ez, _v(x, y, z), unreal.Rotator(0, 0, 0))
        a.set_actor_label("Extract_%s_%d" % (o["role"], j)); a.set_folder_path("METATRON/Extraction")

    # ---- CENTRAL ORIENTATION LANDMARK (SUSPENDED above hub; not a sniper platform,
    #      hub floor + central extract stay clear beneath it) ----
    zc = strata["crown"]
    box("METATRON_Landmark", (0, 0, zc + 450), (4, 4, 3.0), "METATRON/Landmark")

    les.save_current_level()
    return dict(built=made, nodes=len(M["nodes"]), edges=len(M["edges"]),
                connectors=len(M["connectors"]), preset=M["preset"], span=M["span"])
