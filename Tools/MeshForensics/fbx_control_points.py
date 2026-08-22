"""Count control points (and UV layers) in binary FBX files, without Blender.

THE PARSER PROVES ITSELF BEFORE IT IS BELIEVED. SKM_IRONICS_Blank.fbx is already known to hold 46,603
control points from an earlier measurement, so that file is the POSITIVE CONTROL: if this reader does
not reproduce 46,603 on it, every other number it prints is worthless and the run says so rather than
reporting a "third number" that is really a parser bug.

Binary FBX layout used here:
  header  "Kaydara FBX Binary  \\x00" + 2 bytes + uint32 version
  node    EndOffset, NumProperties, PropertyListLen  (uint32 each; uint64 each when version >= 7500)
          uint8 NameLen, Name, properties..., nested nodes, terminated by a NULL record
  array property ('d','f','i','l','b'):  uint32 ArrayLength, uint32 Encoding, uint32 CompressedLength

"Vertices" is a double array of length 3 * controlPoints. "UV" arrays live under LayerElementUV.
"""
import struct, sys, os, zlib

def read_node(f, end_of_file, wide):
    pos = f.tell()
    if wide:
        hdr = f.read(24)
        if len(hdr) < 24: return None
        end_off, nprops, prop_len = struct.unpack('<QQQ', hdr)
        name_len_off = 24
    else:
        hdr = f.read(12)
        if len(hdr) < 12: return None
        end_off, nprops, prop_len = struct.unpack('<III', hdr)
        name_len_off = 12
    nl = f.read(1)
    if not nl: return None
    name_len = nl[0]
    name = f.read(name_len).decode('utf-8', 'replace')
    if end_off == 0:
        return None                      # NULL record: end of this node list
    props = []
    prop_start = f.tell()
    for _ in range(nprops):
        t = f.read(1)
        if not t: break
        t = t.decode('ascii', 'replace')
        if t in 'YCIFDL':
            sz = {'Y': 2, 'C': 1, 'I': 4, 'F': 4, 'D': 8, 'L': 8}[t]
            raw = f.read(sz)
            props.append((t, raw))
        elif t in 'fdlib':
            al, enc, cl = struct.unpack('<III', f.read(12))
            f.seek(cl, 1)
            props.append((t, al))        # KEEP THE ARRAY LENGTH -- that is the whole point
        elif t in 'SR':
            (ln,) = struct.unpack('<I', f.read(4))
            raw = f.read(ln)
            props.append((t, raw))
        else:
            break
    f.seek(prop_start + prop_len)
    children = []
    while f.tell() < end_off - (25 if wide else 13):
        c = read_node(f, end_of_file, wide)
        if c is None: break
        children.append(c)
    f.seek(end_off)
    return (name, props, children)

def walk(node, out):
    name, props, children = node
    if name == 'Vertices':
        for t, v in props:
            if t == 'd' and isinstance(v, int):
                out.setdefault('vertices', []).append(v // 3)
    if name == 'UV':
        for t, v in props:
            if t == 'd' and isinstance(v, int):
                out.setdefault('uv', []).append(v // 2)
    if name == 'LayerElementUV':
        out['uv_layers'] = out.get('uv_layers', 0) + 1
    if name == 'Model':
        for t, v in props:
            if t == 'S':
                s = v.decode('utf-8', 'replace').replace('\x00\x01', '::')
                if 'Geometry' not in s and s:
                    out.setdefault('models', []).append(s)
    for c in children:
        walk(c, out)

def scan(path):
    sz = os.path.getsize(path)
    with open(path, 'rb') as f:
        # The magic is 21 bytes ("Kaydara FBX Binary   "), then 2 bytes (0x1A 0x00), then the
        # uint32 version at offset 23. Reading 23+2 put the version read 2 bytes late and every
        # subsequent offset with it -- which is why the control returned 0 vertices.
        magic = f.read(21)
        if not magic.startswith(b'Kaydara FBX Binary'):
            return {'error': 'not a binary FBX (ASCII?)'}
        f.read(2)
        (ver,) = struct.unpack('<I', f.read(4))
        if not (6000 <= ver <= 8000):
            return {'error': 'implausible version %d -- header offset wrong' % ver}
        wide = ver >= 7500
        out = {'version': ver, 'bytes': sz}
        while f.tell() < sz - 100:
            n = read_node(f, sz, wide)
            if n is None: break
            walk(n, out)
        return out

TARGETS = sys.argv[1:]
KNOWN = {'SKM_IRONICS_Blank.fbx': 46603}   # the positive control

print("%-46s %10s %12s %8s %s" % ("file", "version", "controlPts", "uvLayers", "note"))
control_ok = None
results = {}
for p in TARGETS:
    base = os.path.basename(p)
    try:
        r = scan(p)
    except Exception as ex:
        print("%-46s %10s %12s %8s %s" % (base, '-', 'PARSE FAIL', '-', type(ex).__name__))
        continue
    if 'error' in r:
        print("%-46s %10s %12s %8s %s" % (base, '-', '-', '-', r['error']))
        continue
    vs = r.get('vertices', [])
    total = sum(vs)
    biggest = max(vs) if vs else 0
    note = ''
    if base in KNOWN:
        ok = (biggest == KNOWN[base] or total == KNOWN[base])
        control_ok = ok if control_ok is None else (control_ok and ok)
        note = 'CONTROL expect %d -> %s' % (KNOWN[base], 'MATCH' if ok else 'MISMATCH')
    results[base] = (total, biggest, vs)
    print("%-46s %10d %12d %8d %s  meshes=%s" % (
        base, r['version'], biggest, r.get('uv_layers', 0), note,
        vs if len(vs) <= 6 else '%d meshes, sum=%d' % (len(vs), total)))

print()
if control_ok is None:
    print("NO CONTROL IN THIS RUN -- numbers above are unverified.")
elif control_ok:
    print("PARSER VERIFIED against the known 46,603. The other counts can be believed.")
else:
    print("PARSER FAILED ITS CONTROL. Every number above is suspect; do NOT use them to reconcile anything.")
