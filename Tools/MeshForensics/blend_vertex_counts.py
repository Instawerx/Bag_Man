"""Count mesh vertices in zstd-compressed .blend files, without Blender.

THE READER PROVES ITSELF FIRST. IRONICS_Blank_conform.blend is the control: it was measured at 45,398
in an earlier session. If this reader does not reproduce 45,398 there, its number for symm_final.blend
is worthless and the run says so instead of quietly "resolving" the blocker with a parser artefact.

METHOD, and why it does not need full DNA parsing. A .blend is a sequence of file blocks:
    char code[4], int32 size, void* old, int32 SDNAnr, int32 nr
followed by `size` bytes of payload. Blender stores mesh vertex positions as a generic float[3]
attribute layer, so the position array appears as a DATA block whose element count `nr` IS the vertex
count and whose size is exactly nr * 12. Scanning for blocks satisfying size == nr * 12 gives the
candidate vertex counts directly, with no need to resolve struct field offsets.

Reported as a HISTOGRAM, not a single answer: a mesh file legitimately holds several float[3] arrays
(positions, normals, and so on), so the honest output is every count that appears and how often --
picking one and calling it "the" vertex count is where a reader like this would lie.
"""
import struct, sys, os, zstandard, collections

def load(path):
    raw = open(path, 'rb').read()
    if raw[:4] == b'\x28\xb5\x2f\xfd':
        d = zstandard.ZstdDecompressor()
        # Blender writes a multi-frame zstd stream; stream_reader handles the frames in order.
        return d.stream_reader(raw, read_across_frames=True).read()
    return raw

def blocks(buf):
    # Blender 5.0 header is 17 bytes ("BLENDER17-01v0502"), and the block header is 32 bytes laid out
    # code[4], pad[4], old(i64)@8, size(i64)@16, sdna(i32)@24, nr(i32)@28.
    # THIS LAYOUT WAS NOT GUESSED: candidate (headerSize, sizeOffset) pairs were walked across the whole
    # file and only one traversed 97,603 blocks and landed EXACTLY on the file's real ENDB offset. A
    # wrong stride desynchronises within a few blocks and never arrives, so arriving is the proof.
    if buf[:7] != b'BLENDER':
        return None, []
    ver = buf[12:17].decode('ascii', 'replace')
    off, n, out = 17, len(buf), []
    while off + 32 <= n:
        code = buf[off:off+4]
        (size,) = struct.unpack_from('<q', buf, off+16)
        sdna, nr = struct.unpack_from('<ii', buf, off+24)
        if size < 0 or off + 32 + size > n:
            break
        out.append((code, size, sdna, nr))
        if code == b'ENDB':
            break
        off += 32 + size
    return ver, out

KNOWN = {'IRONICS_Blank_conform.blend': 45398}
print("%-42s %8s %10s  %s" % ("file", "ver", "blocks", "float3 arrays (count x howMany)"))
control_ok = None
found = {}
for p in sys.argv[1:]:
    base = os.path.basename(p)
    try:
        buf = load(p)
    except Exception as ex:
        print("%-42s %8s %10s  DECOMPRESS FAIL: %s" % (base, '-', '-', type(ex).__name__)); continue
    ver, bl = blocks(buf)
    if ver is None:
        print("%-42s %8s %10s  not a .blend after decompression" % (base, '-', '-')); continue
    hist = collections.Counter()
    for code, size, sdna, nr in bl:
        if nr > 1000 and size == nr * 12:
            hist[nr] += 1
    top = hist.most_common(6)
    found[base] = set(hist)
    note = ''
    if base in KNOWN:
        ok = KNOWN[base] in hist
        control_ok = ok if control_ok is None else (control_ok and ok)
        note = '  <- CONTROL expect %d: %s' % (KNOWN[base], 'FOUND' if ok else 'ABSENT')
    print("%-42s %8s %10d  %s%s" % (base, ver, len(bl),
          ', '.join('%d x%d' % (k, v) for k, v in top) or '(none)', note))

print()
if control_ok is None:
    print("NO CONTROL -- unverified.")
elif control_ok:
    print("READER VERIFIED: it reproduced the known 45,398 in conform.blend.")
else:
    print("READER FAILED ITS CONTROL -- do not use these numbers to reconcile anything.")
print()
for a in found:
    for b in found:
        if a < b:
            shared = found[a] & found[b]
            if shared:
                print("shared counts %-38s & %-38s -> %s" % (a, b, sorted(shared)[:6]))
