#!/usr/bin/env python
"""
METATRON — deterministic Flower-of-Life map-family LAYOUT generator (author-time).

Pure Python, NO Blender dependency. Computes the full node/tube/strata/route/spawn/
objective layout for a preset and emits a machine-readable manifest + validation report.
Identical inputs -> identical output (deterministic: sorted iteration, no RNG on gameplay).

Presets (circles = 1 + 3k(k+1)):  METATRON_07 (k1), _19 (k2), _37 (k3), _61 (k4)
Roster: Arena_05=19 (build), BR_18=37, BR_36=61 (Phase B), 07 = generator fixture.

Companion: metatron_build.py (Blender) reads a manifest and instantiates modular meshes.
Design SSOT: Docs/maps/FlowerOfLife_DESIGN.md (LOCKED AMENDMENTS).

Usage:  python metatron_layout.py <PRESET|all> [--out DIR] [--r CM]
"""
import json, math, sys, os, hashlib

GEN_VERSION = "1.0.0"
SQRT3 = math.sqrt(3.0)

# ---- LOCKED parameters (Studio Lead amendments) — units = cm (UE) ----
DEFAULTS = dict(
    base_radius=1500.0,                 # R ~15 m courtyard radius (tunable)
    bore_primary=800.0,                 # 8 m arteries / major junctions
    bore_standard=600.0,                # 6 m standard playable tubes (MIN)
    bore_flank=550.0,                   # 5-6 m shortcuts / flank passages
    strata={"undercroft": -600.0, "primary": 0.0, "crown": 700.0},  # -6 / 0 / +7 m
    courtyard_opening=450.0,            # vesica/opening clear width
    sector_rotation_deg=0.0,
    vertical_connector_stride=2,        # place a connector on a sub-lattice (~26 m apart)
    sightline_break_max=4000.0,         # ~40 m enclosed-tube sightline cap
    outer_break_every=1,                # break/expose outer crown every N outer cells
    seed=0,                             # non-gameplay variation only
)
PRESETS = {"METATRON_07": 1, "METATRON_19": 2, "METATRON_37": 3, "METATRON_61": 4}
ROSTER = {"METATRON_07": "(generator fixture — not a roster slot)",
          "METATRON_19": "Arena_05 (build target)",
          "METATRON_37": "BR_18 (Phase B)",
          "METATRON_61": "BR_36 (Phase B)"}

# ---- hex lattice (Flower-of-Life circle centres; neighbour spacing = R) ----
def hex_cells(k):
    cells = [(q, r) for q in range(-k, k + 1) for r in range(-k, k + 1)
             if max(abs(q), abs(r), abs(q + r)) <= k]
    return sorted(cells)

def hex_dist(q, r):
    return max(abs(q), abs(r), abs(q + r))

def axial_to_xy(q, r, R):
    return (R * (q + r * 0.5), R * (SQRT3 / 2.0 * r))

NEI = [(1, 0), (-1, 0), (0, 1), (0, -1), (1, -1), (-1, 1)]

def sector_of(q, r, rot_deg):
    if q == 0 and r == 0:
        return 0  # hub
    x, y = axial_to_xy(q, r, 1.0)
    ang = (math.degrees(math.atan2(y, x)) - rot_deg) % 360.0
    return 1 + int(ang // 60.0)  # sectors 1..6

def on_main_axis(q, r):
    # the 3 hex axes through centre: r==0, q==0, q+r==0
    return (r == 0) or (q == 0) or (q + r == 0)

def edge_route_class(a, b, k):
    (q0, r0), (q1, r1) = a, b
    outer = (hex_dist(q0, r0) == k) or (hex_dist(q1, r1) == k)
    # primary artery: both endpoints on the same main axis line through centre
    same_axis = ((r0 == 0 and r1 == 0) or (q0 == 0 and q1 == 0) or
                 (q0 + r0 == 0 and q1 + r1 == 0))
    if same_axis:
        return "primary"
    if outer:
        return "flank"
    return "standard"

def build_layout(preset, params):
    k = PRESETS[preset]
    R = params["base_radius"]
    rot = params["sector_rotation_deg"]
    cells = hex_cells(k)
    idx = {c: i for i, c in enumerate(cells)}

    # nodes (courtyards) per stratum
    nodes = []
    for (q, r) in cells:
        x, y = axial_to_xy(q, r, R)
        for sname, sz in sorted(params["strata"].items(), key=lambda kv: kv[1]):
            nodes.append(dict(
                cell=[q, r], stratum=sname, xyz=[round(x, 1), round(y, 1), sz],
                sector=sector_of(q, r, rot), ring=hex_dist(q, r),
                is_hub=(q == 0 and r == 0), is_outer=(hex_dist(q, r) == k),
                radius=round(R, 1)))
    nodes.sort(key=lambda n: (n["cell"][0], n["cell"][1], n["xyz"][2]))

    # edges (tubes): hex-adjacent in-flower pairs, per stratum
    edge_pairs = set()
    for (q, r) in cells:
        for (dq, dr) in NEI:
            nb = (q + dq, r + dr)
            if nb in idx:
                edge_pairs.add(tuple(sorted([(q, r), nb])))
    edge_pairs = sorted(edge_pairs)
    edges = []
    for (a, b) in edge_pairs:
        rc = edge_route_class(a, b, k)
        bore = {"primary": params["bore_primary"], "standard": params["bore_standard"],
                "flank": params["bore_flank"]}[rc]
        ax, ay = axial_to_xy(a[0], a[1], R)
        bx, by = axial_to_xy(b[0], b[1], R)
        length = round(math.hypot(bx - ax, by - ay), 1)  # == R
        for sname, sz in sorted(params["strata"].items(), key=lambda kv: kv[1]):
            edges.append(dict(
                a=[a[0], a[1]], b=[b[0], b[1]], stratum=sname, route_class=rc,
                bore=bore, length=length,
                mid=[round((ax + bx) / 2, 1), round((ay + by) / 2, 1), sz]))

    # vertical connectors (sub-lattice so a transition is within ~1 cell of everywhere)
    stride = params["vertical_connector_stride"]
    connectors = []
    for (q, r) in cells:
        if ((q - r) % stride == 0):
            x, y = axial_to_xy(q, r, R)
            connectors.append(dict(cell=[q, r], xy=[round(x, 1), round(y, 1)],
                                   connects=sorted(params["strata"].keys()),
                                   sector=sector_of(q, r, rot)))
    connectors.sort(key=lambda c: (c["cell"][0], c["cell"][1]))

    # spawn candidates: outer-ring cells, one per sector (opposite sectors pair for 2-team)
    outer = [(q, r) for (q, r) in cells if hex_dist(q, r) == k]
    spawn_by_sector = {}
    for (q, r) in sorted(outer):
        s = sector_of(q, r, rot)
        # pick the most-radial outer cell per sector (deterministic: farthest, then sorted)
        spawn_by_sector.setdefault(s, (q, r))
    spawns = []
    for s in sorted(spawn_by_sector):
        q, r = spawn_by_sector[s]
        x, y = axial_to_xy(q, r, R)
        spawns.append(dict(sector=s, cell=[q, r],
                           xyz=[round(x, 1), round(y, 1), params["strata"]["primary"] + 100.0]))

    # objective candidates: hub + mid-ring distributed (Tier-C wants 2-3; provide candidates)
    objectives = [dict(role="central_hot", cell=[0, 0],
                       xyz=[0.0, 0.0, params["strata"]["primary"] + 60.0])]
    midring = max(1, k - 1)
    for (q, r) in sorted(c for c in cells if hex_dist(*c) == midring):
        if sector_of(q, r, rot) in (1, 3, 5):  # 3 alternating sectors
            x, y = axial_to_xy(q, r, R)
            objectives.append(dict(role="peripheral", cell=[q, r],
                                   xyz=[round(x, 1), round(y, 1), params["strata"]["primary"] + 60.0]))

    # bounds
    xs = [n["xyz"][0] for n in nodes]; ys = [n["xyz"][1] for n in nodes]
    zmin = min(params["strata"].values()); zmax = max(params["strata"].values())
    ext = R + params["bore_primary"] / 2.0
    bounds = dict(min=[round(min(xs) - ext, 1), round(min(ys) - ext, 1), round(zmin - 200, 1)],
                  max=[round(max(xs) + ext, 1), round(max(ys) + ext, 1), round(zmax + 300, 1)])
    span = [round(bounds["max"][i] - bounds["min"][i], 1) for i in range(3)]

    # module-instance + collision/nav-proxy counts (modular: 1 courtyard + 1 tube mesh, instanced)
    n_courtyard = len(nodes)
    n_tube = len(edges)
    n_conn = len(connectors) * (len(params["strata"]) - 1)
    counts = dict(
        cells=len(cells), circles_expected=1 + 3 * k * (k + 1),
        courtyard_instances=n_courtyard, tube_instances=n_tube,
        connector_instances=n_conn, spawn_candidates=len(spawns),
        objective_candidates=len(objectives),
        render_module_types=3,  # courtyard, tube, connector (+ crown variant handled in build)
        nav_proxy_instances=n_courtyard + n_tube + n_conn,
        collision_proxy_instances=n_courtyard + n_tube + n_conn)

    manifest = dict(
        preset=preset, roster=ROSTER[preset], generator_version=GEN_VERSION,
        params={k2: v for k2, v in params.items()},
        rings=k, bounds=bounds, span=span, counts=counts,
        nodes=nodes, edges=edges, connectors=connectors,
        spawns=spawns, objectives=objectives)
    manifest["determinism_hash"] = hashlib.sha256(
        json.dumps({k2: manifest[k2] for k2 in
                    ("preset", "params", "rings", "nodes", "edges", "connectors",
                     "spawns", "objectives")}, sort_keys=True).encode()).hexdigest()[:16]
    return manifest

def validate(m):
    warns, fails = [], []
    k = m["rings"]; c = m["counts"]
    if c["cells"] != c["circles_expected"]:
        fails.append("cell count %d != expected %d" % (c["cells"], c["circles_expected"]))
    # connectivity of the primary-stratum graph
    prim_nodes = {tuple(n["cell"]) for n in m["nodes"] if n["stratum"] == "primary"}
    adj = {cc: set() for cc in prim_nodes}
    for e in m["edges"]:
        if e["stratum"] == "primary":
            a, b = tuple(e["a"]), tuple(e["b"]); adj[a].add(b); adj[b].add(a)
    seen = set(); stack = [(0, 0)]
    while stack:
        x = stack.pop()
        if x in seen: continue
        seen.add(x); stack.extend(adj[x] - seen)
    if seen != prim_nodes:
        fails.append("primary graph not fully connected (%d/%d)" % (len(seen), len(prim_nodes)))
    # sightline cap: tube length must be <= cap (single segment = R)
    R = m["params"]["base_radius"]
    if R > m["params"]["sightline_break_max"]:
        warns.append("R (%.0f) exceeds sightline cap (%.0f) — segments need mid-breaks" %
                     (R, m["params"]["sightline_break_max"]))
    # bore minimums
    if m["params"]["bore_standard"] < 600.0:
        fails.append("standard bore < 6 m minimum")
    # strata count
    if len(m["params"]["strata"]) != 3:
        fails.append("must be exactly 3 strata, got %d" % len(m["params"]["strata"]))
    # vertical connector coverage: every cell within 1 ring of a connector
    conn_cells = {tuple(cn["cell"]) for cn in m["connectors"]}
    uncovered = 0
    for n in m["nodes"]:
        if n["stratum"] != "primary": continue
        cc = tuple(n["cell"])
        near = any(max(abs(cc[0] - x), abs(cc[1] - y), abs((cc[0] + cc[1]) - (x + y))) <= 1
                   for (x, y) in conn_cells)
        if not near: uncovered += 1
    if uncovered:
        warns.append("%d primary cells > 1 ring from a vertical connector" % uncovered)
    # spawns: need >= 2 opposite sectors
    if len(m["spawns"]) < 2:
        fails.append("need >= 2 spawn candidates")
    return dict(passed=(len(fails) == 0), failures=fails, warnings=warns)

def run(preset, out_dir, overrides=None):
    params = dict(DEFAULTS)
    if overrides: params.update(overrides)
    m = build_layout(preset, params)
    m["validation"] = validate(m)
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, "manifest_%s.json" % preset)
    with open(path, "w") as f:
        json.dump(m, f, indent=1)
    return m, path

if __name__ == "__main__":
    which = sys.argv[1] if len(sys.argv) > 1 else "all"
    out = "."
    ov = {}
    for i, a in enumerate(sys.argv):
        if a == "--out" and i + 1 < len(sys.argv): out = sys.argv[i + 1]
        if a == "--r" and i + 1 < len(sys.argv): ov["base_radius"] = float(sys.argv[i + 1])
    targets = list(PRESETS) if which == "all" else [which]
    for p in targets:
        m, path = run(p, out, ov)
        v = m["validation"]; c = m["counts"]
        print("%-13s rings=%d circles=%d courtyards=%d tubes=%d conn=%d span=%sm hash=%s -> %s"
              % (p, m["rings"], c["cells"], c["courtyard_instances"], c["tube_instances"],
                 c["connector_instances"],
                 "x".join("%.0f" % (s / 100.0) for s in m["span"]), m["determinism_hash"],
                 "PASS" if v["passed"] else "FAIL"))
        for w in v["warnings"]: print("    warn:", w)
        for fl in v["failures"]: print("    FAIL:", fl)
