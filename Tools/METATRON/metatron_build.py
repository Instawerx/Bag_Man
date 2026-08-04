"""
METATRON — UE-side greybox BUILDER (run in Unreal via the editor bridge / execute_python).

Builds an ACTUAL WEAVING CONNECTED TUNNEL SYSTEM from a metatron_layout manifest,
deterministically:
  * TUBES (per edge): enclosed tunnels you run INSIDE — floor + walkable ceiling (run ON TOP)
    + two side walls with STAGGERED OPENINGS (ports to the courtyards; break straight LOS).
    Undercroft + primary are enclosed; CROWN is an open on-top walkway (exposed).
  * COURTYARDS (per node): open junction chambers (open-top) where tubes meet = the battle
    "openings". Run OUTSIDE (courtyards) / INSIDE (tubes) / ON TOP (ceilings/crown) / BELOW
    (undercroft).
  * CONNECTORS: ramps + NavLinkProxies weaving the 3 strata together.
  * Spawns (6-fold, side-tagged), extraction objectives, sector-ID markers, central landmark.

Shared engine mesh + SIMPLE collision (nav-gatherable, ISM-suitable) — carries the Duel_01
lesson. Deterministic + self-cleaning (re-runnable). Layout SSOT = the manifest.

Callable: build(manifest_path) -> summary dict.
"""
import unreal, json, math

def _v(x, y, z): return unreal.Vector(float(x), float(y), float(z))

def build(manifest_path):
    with open(manifest_path) as f:
        M = json.load(f)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    cube = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube")
    strata = M["params"]["strata"]
    R = M["params"]["base_radius"]

    for a in list(eas.get_all_level_actors()):        # self-clean prior output
        if str(a.get_folder_path()).startswith("METATRON"):
            eas.destroy_actor(a)

    made = {"courtyard": 0, "tube_floor": 0, "tube_top": 0, "tube_wall": 0, "ramp": 0, "navlink": 0}

    def box(label, loc, scale, folder, rot=(0, 0, 0)):
        a = eas.spawn_actor_from_object(cube, _v(*loc), unreal.Rotator(rot[0], rot[1], rot[2]))
        a.set_actor_scale3d(_v(*scale)); a.set_actor_label(label); a.set_folder_path(folder)
        return a

    def A(q, r): return (R * (q + r * 0.5), R * (math.sqrt(3) / 2 * r))

    # ---- COURTYARD junction chambers (open-top; tubes open into them) ----
    JR = R * 0.42            # junction pad radius (~6.3 m -> 12.6 m open chamber)
    for n in M["nodes"]:
        x, y, z = n["xyz"]
        box("CY_%d_%d_%s" % (n["cell"][0], n["cell"][1], n["stratum"]),
            (x, y, z - 15), (JR * 2 / 100.0, JR * 2 / 100.0, 0.3), "METATRON/Courtyards/%s" % n["stratum"])
        made["courtyard"] += 1

    # ---- TUBES: enclosed tunnels (undercroft+primary) / open walkway (crown) ----
    for e in M["edges"]:
        ax, ay = e["a"]; bx, by = e["b"]; z = e["mid"][2]
        Ax, Ay = A(ax, ay); Bx, By = A(bx, by)
        mx, my = (Ax + Bx) / 2, (Ay + By) / 2
        length = math.hypot(Bx - Ax, By - Ay)
        yaw = math.degrees(math.atan2(By - Ay, Bx - Ax)); yr = math.radians(yaw)
        bore = e["bore"]; enclosed = (e["stratum"] != "crown")
        # floor (full length, overlaps junctions -> no cracks)
        box("TBf_%d_%d__%d_%d_%s" % (ax, ay, bx, by, e["stratum"]),
            (mx, my, z - 15), (length / 100.0, bore / 100.0, 0.3), "METATRON/Tubes/%s" % e["stratum"], rot=(0, 0, yaw))
        made["tube_floor"] += 1
        if not enclosed:
            continue
        # walkable ceiling / run-on-top (middle 70%, leaves junction ends open-top)
        box("TBt_%d_%d__%d_%d_%s" % (ax, ay, bx, by, e["stratum"]),
            (mx, my, z + bore - 15), (length * 0.7 / 100.0, bore / 100.0, 0.3),
            "METATRON/TubeTops/%s" % e["stratum"], rot=(0, 0, yaw))
        made["tube_top"] += 1
        # side walls with STAGGERED openings (perp offset +-bore/2; opening near opposite ends)
        px, py = math.cos(yr + math.pi / 2), math.sin(yr + math.pi / 2)  # perp unit
        # side + covers t in [0.10..0.60]; side - covers [0.40..0.90] -> staggered ports
        for sgn, t0, t1 in ((+1, 0.10, 0.60), (-1, 0.40, 0.90)):
            segc = (t0 + t1) / 2.0
            cx = Ax + (Bx - Ax) * segc + px * sgn * bore / 2.0
            cy = Ay + (By - Ay) * segc + py * sgn * bore / 2.0
            box("TBw_%d_%d__%d_%d_%s_%d" % (ax, ay, bx, by, e["stratum"], sgn),
                (cx, cy, z + bore / 2 - 15), (length * (t1 - t0) / 100.0, 0.3, bore / 100.0),
                "METATRON/Tubes/%s" % e["stratum"], rot=(0, 0, yaw))
            made["tube_wall"] += 1

    # ---- VERTICAL CONNECTORS (ramps + navlinks weave the strata) ----
    zsorted = sorted(strata.items(), key=lambda kv: kv[1])
    def navlink(pf, pt, label):
        mid = ((pf[0] + pt[0]) / 2, (pf[1] + pt[1]) / 2, (pf[2] + pt[2]) / 2)
        n = eas.spawn_actor_from_class(unreal.NavLinkProxy, _v(*mid), unreal.Rotator(0, 0, 0))
        nl = unreal.NavigationLink()
        nl.set_editor_property("left", _v(pf[0] - mid[0], pf[1] - mid[1], pf[2] - mid[2]))
        nl.set_editor_property("right", _v(pt[0] - mid[0], pt[1] - mid[1], pt[2] - mid[2]))
        nl.set_editor_property("direction", unreal.NavLinkDirection.BOTH_WAYS)
        n.set_editor_property("point_links", [nl]); n.set_actor_label(label); n.set_folder_path("METATRON/Connectors")
        made["navlink"] += 1
    for cn in M["connectors"]:
        x, y = cn["xy"]
        for i in range(len(zsorted) - 1):
            z0 = zsorted[i][1]; z1 = zsorted[i + 1][1]
            bx2, by2 = x - 550, y; tx2, ty2 = x + 550, y
            dx, dy, dz = tx2 - bx2, ty2 - by2, z1 - z0
            L = math.sqrt(dx * dx + dy * dy + dz * dz)
            yaw = math.degrees(math.atan2(dy, dx)); slope = math.degrees(math.atan2(dz, math.hypot(dx, dy)))
            a = eas.spawn_actor_from_object(cube, _v((bx2 + tx2) / 2, (by2 + ty2) / 2, (z0 + z1) / 2), unreal.Rotator(-slope, yaw, 0))
            a.set_actor_scale3d(_v(L / 100.0, 4.0, 0.3)); a.set_actor_label("RMP_%d_%d_%d" % (cn["cell"][0], cn["cell"][1], i))
            a.set_folder_path("METATRON/Connectors"); made["ramp"] += 1
            navlink((x, y, z0 + 20), (x, y, z1 + 20), "NL_%d_%d_%d" % (cn["cell"][0], cn["cell"][1], i))

    # ---- SPAWNS (two opposite sectors, 8/side, side-tagged) ----
    def cont(side):
        c = unreal.GameplayTagContainer(); c.import_text('(GameplayTags=((TagName="AFL.Spawn.Side.%d")))' % side); return c
    spawns = {s["sector"]: s for s in M["spawns"]}; secs = sorted(spawns)
    a_sec = 1 if 1 in spawns else secs[0]; b_sec = 4 if 4 in spawns else secs[len(secs) // 2]
    zp = strata["primary"] + 100
    for side, sec in ((0, a_sec), (1, b_sec)):
        cx, cy, _ = spawns[sec]["xyz"]
        for i in range(8):
            off = (i - 3.5) * 150
            s = eas.spawn_actor_from_class(unreal.LyraPlayerStart,
                    _v(cx + (off if abs(cx) < abs(cy) else 0), cy + (0 if abs(cx) < abs(cy) else off), zp), unreal.Rotator(0, 0, 0))
            s.set_actor_label("Start_S%d_%d" % (side, i)); s.set_folder_path("METATRON/Spawns/Side%d" % side)
            s.set_editor_property("start_point_tags", cont(side))

    # ---- OBJECTIVES ----
    ez = unreal.EditorAssetLibrary.load_blueprint_class("/Game/BagMan/Extraction/B_AFL_ExtractionZone")
    for j, o in enumerate(M["objectives"][:3]):
        x, y, z = o["xyz"]
        a = eas.spawn_actor_from_class(ez, _v(x, y, z), unreal.Rotator(0, 0, 0))
        a.set_actor_label("Extract_%s_%d" % (o["role"], j)); a.set_folder_path("METATRON/Extraction")

    # ---- SECTOR-ID markers (distinct height per sector — ID without color) ----
    seen = set()
    for n in sorted(M["nodes"], key=lambda nn: nn["sector"]):
        s = n["sector"]
        if s == 0 or s in seen or n["stratum"] != "primary" or not n["is_outer"]:
            continue
        seen.add(s); x, y, z = n["xyz"]
        box("SectorMarker_%d" % s, (x, y, z + 300 + s * 40), (1.5, 1.5, (600 + s * 80) / 100.0), "METATRON/SectorMarkers")

    # ---- CENTRAL ORIENTATION LANDMARK (suspended; hub floor stays clear) ----
    box("METATRON_Landmark", (0, 0, strata["crown"] + 450), (4, 4, 3.0), "METATRON/Landmark")

    les.save_current_level()
    return dict(built=made, nodes=len(M["nodes"]), edges=len(M["edges"]),
                connectors=len(M["connectors"]), preset=M["preset"], span=M["span"])
