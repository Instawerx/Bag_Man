# Hand-rolled PNG encoder (no PIL/numpy in UE python). RGBA 8-bit.
import zlib, struct

def png_rgba(path, w, h, pixfn):
    raw = bytearray()
    for y in range(h):
        raw.append(0)  # filter type 0 (None) per scanline
        for x in range(w):
            r,g,b,a = pixfn(x,y,w,h)
            raw += bytes((r&255, g&255, b&255, a&255))
    def chunk(typ, data):
        c = typ + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)
    sig = b'\x89PNG\r\n\x1a\n'
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)  # 8-bit, color type 6 = RGBA
    idat = zlib.compress(bytes(raw), 9)
    with open(path, "wb") as f:
        f.write(sig)
        f.write(chunk(b'IHDR', ihdr))
        f.write(chunk(b'IDAT', idat))
        f.write(chunk(b'IEND', b''))
    print("wrote", path, w, "x", h)

import os
OUT = os.path.dirname(os.path.abspath(__file__)) + os.sep

# --- grid_v2: bold white lines, full alpha, transparent field. 128px, 32px cells, 2px lines.
def grid(x,y,w,h):
    cell = 32
    line = (x % cell) < 2 or (y % cell) < 2
    if line:
        return (255,255,255,255)
    # faint interior dot grid for extra tech texture at cell centers
    return (255,255,255,0)
png_rgba(OUT+"grid_v2.png", 128, 128, grid)

# --- gradient_v: vertical light from top. White, alpha = 1 at top -> 0 at bottom (smooth).
def grad_v(x,y,w,h):
    a = int(255 * (1.0 - (y/(h-1))))
    return (255,255,255,a)
png_rgba(OUT+"gradient_v.png", 64, 256, grad_v)

# --- gradient_radial: centered glow pool. alpha peaks at center, falls to 0 at edge.
def grad_radial(x,y,w,h):
    cx, cy = (w-1)/2.0, (h-1)/2.0
    dx, dy = (x-cx)/cx, (y-cy)/cy
    d = (dx*dx + dy*dy) ** 0.5     # 0 center -> ~1.41 corner
    t = max(0.0, 1.0 - d)          # 1 center -> 0 at edge midpoints
    a = int(255 * (t*t))           # squared = softer, glassier falloff
    return (255,255,255,a)
png_rgba(OUT+"gradient_radial.png", 256, 256, grad_radial)

print("done")
