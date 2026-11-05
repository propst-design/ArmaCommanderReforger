"""Blockout aircraft for the recon flight and the bomb run: 1989 jets as coloured silhouettes, seen from the
ground at 300-500 m (ACR_LocalFlyover flies them). Not cockpits or detail: the right planform, the right
paint and a red star where there should be one, so a player can tell an A-10 from a Frogfoot overhead.

Run headless (Blender 4.1+):
  blender -b -P tools/blender/build_aircraft.py -- <addon root> [<name> ...]
Then tools/Build-AircraftAssets.ps1 imports the FBXs in Workbench (hidden).

Writes, under <addon root>/Assets/Vehicles/Aircraft:
  ACR_<name>.fbx, Data/ACR_Aircraft_Palette_BCR.png (one shared paint palette: every face is UV-mapped onto one
  16 px swatch, so the jets need one material and no unwrapping), the Workbench job
  (.local-workbench/profile/aircraft_job.txt) and, local only, .local-aircraft/ACR_<name>.blend and
  .local-aircraft/previews/ACR_<name>_*.png. The FBX carries no texture link (it would hold this PC's path):
  Workbench links the material by name.

Axes: modelled in metres with the nose along Blender +X, up +Z, exported with Blender's default FBX axes (as the
launcher mods do), which Workbench imports as nose +X, up +Y: the model axes ACR_LocalFlyover.Orient assumes for a
prefab without the FA-18's bone. Origin near the centre of mass.
"""
import bpy, bmesh, math, os, sys
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
ROOT = os.path.abspath(argv[0])
ONLY = set(argv[1:])
OUT = os.path.join(ROOT, "Assets", "Vehicles", "Aircraft")
DATA = os.path.join(OUT, "Data")
PREV = os.path.join(ROOT, ".local-aircraft", "previews")
for d in (OUT, DATA, PREV):
    os.makedirs(d, exist_ok=True)

# ---------- paint: sRGB colour + roughness, one swatch each on a 16 x 16 grid ----------
# FS 595 / Soviet enamel approximations, a little lifted so they still read against a bright sky.
PAINT = {
    "euro_dkgreen": ((0x33, 0x40, 0x33), 0.75),   # FS 34092 European I dark green
    "euro_green": ((0x56, 0x63, 0x42), 0.75),     # FS 34102 medium green
    "euro_gray": ((0x45, 0x49, 0x4d), 0.75),      # FS 36081 dark gray
    "sov_tan": ((0xb3, 0x98, 0x66), 0.7),         # 1980s Frontal Aviation upper camouflage
    "sov_brown": ((0x6e, 0x4c, 0x30), 0.7),
    "sov_green": ((0x63, 0x70, 0x3c), 0.7),
    "sov_dkgreen": ((0x37, 0x47, 0x2b), 0.7),
    "sov_blue": ((0x9c, 0xb8, 0xcc), 0.65),       # light blue undersides
    "pod_gray": ((0xa9, 0xad, 0xa8), 0.6),        # KKR recon pod
    "radome": ((0x8a, 0x8f, 0x89), 0.5),          # Su-17 shock cone
    "canopy": ((0x24, 0x31, 0x3c), 0.15),
    "nozzle": ((0x2d, 0x2b, 0x29), 0.5),
    "intake": ((0x1c, 0x1c, 0x1c), 0.9),
    "red": ((0xc4, 0x24, 0x1c), 0.55),            # star
    "white": ((0xe8, 0xe8, 0xe4), 0.55),          # star border
    "od": ((0x4b, 0x53, 0x32), 0.7),              # Mk 82 / FAB olive drab
    "fab_gray": ((0x5c, 0x62, 0x55), 0.7),
    "yellow": ((0xd6, 0xae, 0x2c), 0.6),          # Mk 82 nose band (filled, high explosive)
    "fab_light": ((0xa6, 0xae, 0xae), 0.6),       # FAB-250 light grey
    "fab_seam": ((0x86, 0x8c, 0x8a), 0.65),       # weld seams and bands on a grey bomb
    "chute": ((0xdc, 0xd6, 0xc4), 0.9),           # brake parachute (kapron, off-white)
    "cord": ((0xb8, 0xb0, 0x98), 0.9),            # its lines and riser
}
SWATCH = {name: i for i, name in enumerate(PAINT)}
GRID, SW = 16, 16  # 16 x 16 swatches of 16 px: one texel per swatch even at the 16 px mip


def write_palette():
    path = os.path.join(DATA, "ACR_Aircraft_Palette_BCR.png")
    img = bpy.data.images.new("ACR_Aircraft_Palette_BCR", GRID * SW, GRID * SW, alpha=True)
    img.colorspace_settings.name = "sRGB"
    px = [0.5, 0.5, 0.5, 0.7] * (GRID * SW) ** 2
    for name, idx in SWATCH.items():
        (r, g, b), rough = PAINT[name]
        col, row = idx % GRID, idx // GRID  # row from the top of the image
        for y in range(SW):
            for x in range(SW):
                # Blender images run bottom-up; Workbench samples top-down (DX), and the FBX import flips V
                iy = (GRID - 1 - row) * SW + y
                i = (iy * GRID * SW + col * SW + x) * 4
                px[i:i + 4] = [r / 255, g / 255, b / 255, rough]
    img.pixels = px
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    return img, path


def swatch_uv(name):
    idx = SWATCH[name]
    col, row = idx % GRID, idx // GRID
    return ((col + 0.5) / GRID, 1 - (row + 0.5) / GRID)


# ---------- geometry ----------
class Builder:
    """Closed shells in one bmesh; each shell's faces carry a paint rule ("camo" or a fixed swatch)."""

    def __init__(self):
        self.bm = bmesh.new()
        self.paint = self.bm.faces.layers.int.new("paint")  # -1 = camo, else swatch index
        self.smooth = self.bm.faces.layers.int.new("smooth")  # 1 = smooth shaded
        self.mirror_parts = []

    def _faces(self, rings, paint, cap0=True, cap1=True, smooth=False):
        bm = self.bm
        vr = [[bm.verts.new(p) for p in ring] for ring in rings]
        faces = []
        for a, b in zip(vr, vr[1:]):
            n = len(a)
            if len(a) == 1 and len(b) == 1:
                continue
            if len(a) == 1:
                faces += [bm.faces.new((a[0], b[i], b[(i + 1) % len(b)])) for i in range(len(b))]
            elif len(b) == 1:
                faces += [bm.faces.new((a[i], a[(i + 1) % n], b[0])) for i in range(n)]
            else:
                faces += [bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i])) for i in range(n)]
        if cap0 and len(vr[0]) > 2:
            faces.append(bm.faces.new(vr[0]))
        if cap1 and len(vr[-1]) > 2:
            faces.append(bm.faces.new(list(reversed(vr[-1]))))
        code = -1 if paint == "camo" else SWATCH[paint]
        n_caps = int(cap0 and len(vr[0]) > 2) + int(cap1 and len(vr[-1]) > 2)
        for i, f in enumerate(faces):
            f[self.paint] = code
            f[self.smooth] = int(smooth and i < len(faces) - n_caps)
        return faces

    def loft(self, stations, paint="camo", y=0.0, n=2.4, seg=20, mirror=False, smooth=False):
        """Fuselage-like body along X. stations: (x, z centre, half width, half height); a zero half width
        closes the body to a point. n: superellipse exponent (2 round, higher boxier)."""
        for side in ((1, -1) if mirror else (1,)):
            rings = []
            for x, zc, hw, hh in stations:
                if hw < 1e-4:
                    rings.append([Vector((x, y * side, zc))])
                    continue
                ring = []
                for i in range(seg):
                    t = 2 * math.pi * i / seg
                    c, s = math.cos(t), math.sin(t)
                    ry = math.copysign(abs(c) ** (2 / n), c) * hw
                    rz = math.copysign(abs(s) ** (2 / n), s) * hh
                    ring.append(Vector((x, y * side + ry, zc + rz)))
                rings.append(ring)
            self._faces(rings, paint, smooth=smooth)

    def surface(self, root, tip, paint="camo", vertical=False, mirror=True, spans=6):
        """Wing/tail panel. root/tip: (x leading edge, y, z, chord, thickness as a fraction of chord). The
        section's thickness runs along Z, or along Y for a fin (vertical). Mirrored across Y=0 by default."""
        prof = [(0, 0), (0.06, 0.7), (0.25, 1), (0.55, 0.85), (0.85, 0.35), (1, 0),
                (0.85, -0.35), (0.55, -0.85), (0.25, -1), (0.06, -0.7)]
        for side in ((1, -1) if mirror else (1,)):
            rings = []
            for k in range(spans + 1):
                f = k / spans
                xl, y, z, ch, th = [root[i] + (tip[i] - root[i]) * f for i in range(5)]
                ring = []
                for u, v in prof:
                    off = v * th * ch * 0.5
                    if vertical:
                        ring.append(Vector((xl - u * ch, (y + off) * side, z)))
                    else:
                        ring.append(Vector((xl - u * ch, y * side, z + off)))
                rings.append(ring)
            self._faces(rings, paint)

    def box(self, lo, hi, paint="camo", mirror=False):
        for side in ((1, -1) if mirror else (1,)):
            (x0, y0, z0), (x1, y1, z1) = lo, hi
            y0, y1 = sorted((y0 * side, y1 * side))
            ring0 = [Vector((x1, y0, z0)), Vector((x1, y1, z0)), Vector((x1, y1, z1)), Vector((x1, y0, z1))]
            ring1 = [Vector((x0, p.y, p.z)) for p in ring0]
            self._faces([ring0, ring1], paint)

    def star(self, centre, radius, axis, depth, mirror=False):
        """Soviet star: a red five-point prism through a fin (axis "y") or wing (axis "z"), on a white
        border one size up, so it shows on both faces."""
        for side in ((1, -1) if mirror else (1,)):
            for r, paint, d in ((radius * 1.18, "white", depth), (radius, "red", depth + 0.02)):
                ring_a, ring_b = [], []
                for i in range(10):
                    t = math.pi / 2 + i * math.pi / 5
                    rr = r if i % 2 == 0 else r * 0.40
                    u, v = math.cos(t) * rr, math.sin(t) * rr
                    cx, cy, cz = centre
                    if axis == "y":  # fin: star in the XZ plane, nose (+X) to the left of an upright star
                        p = (cx - u, cy * side, cz + v)
                        ring_a.append(Vector((p[0], p[1] - d / 2, p[2])))
                        ring_b.append(Vector((p[0], p[1] + d / 2, p[2])))
                    else:  # wing: star in the XY plane, point towards the nose
                        p = (cx + v, cy * side + u, cz)
                        ring_a.append(Vector((p[0], p[1], p[2] - d / 2)))
                        ring_b.append(Vector((p[0], p[1], p[2] + d / 2)))
                self._faces([ring_a, ring_b], paint)

    def fin_star(self, root, tip, height, radius):
        """A star on a fin built by surface(root, tip, vertical=True): centred on the mid-chord at `height` (a
        fraction of the fin's span), just proud of its skin. The leading edge is swept, so a star placed by
        eye hung off the front of the fin (Jacob, 4 Nov)."""
        xl, _, z, ch, th = [root[i] + (tip[i] - root[i]) * height for i in range(5)]
        self.star((xl - 0.5 * ch, root[1], z), radius, "y", th * ch + 0.03)

    def store(self, x0, x1, y, z, r, paint, mirror=True, pylon=0.0, body=True):
        """Bomb / tank / pod: a pointed body, on a pylon of `pylon` m up to the wing. body=False: the pylon alone
        (the bombs are models of their own, which ACR_LocalFlyover hangs there and drops)."""
        L = x0 - x1
        st = [(x0, z, 0, 0), (x0 - 0.12 * L, z, r * 0.75, r * 0.75), (x0 - 0.3 * L, z, r, r),
              (x1 + 0.25 * L, z, r, r), (x1 + 0.05 * L, z, r * 0.6, r * 0.6), (x1, z, 0.0, 0)]
        if body:
            self.loft(st, paint, y=y, n=2, seg=12, mirror=mirror and abs(y) > 1e-3)
        if pylon > 0:
            self.box((x1 + 0.35 * L, y - 0.04, z + r * 0.8), (x0 - 0.3 * L, y + 0.04, z + r + pylon), paint,
                     mirror=mirror and abs(y) > 1e-3)

    def finish(self, name, plane):
        bm = self.bm
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        uv = bm.loops.layers.uv.new("UVMap")
        camo = plane["camo"]
        vals = {}
        for f in bm.faces:
            if f[self.paint] == -1:
                vals[f] = camo_value(f.calc_center_median(), plane)
        bands = band_edges(vals, f_areas={f: f.calc_area() for f in vals}, n=len(camo["colours"]))
        for f in bm.faces:
            code = f[self.paint]
            if code == -1:
                if camo.get("under") and f.normal.z < -0.35:
                    paint = camo["under"]
                else:
                    v = vals[f]
                    k = sum(1 for e in bands if v > e)
                    paint = camo["colours"][k]
                u = swatch_uv(paint)
            else:
                u = swatch_uv(list(SWATCH)[code])
            for loop in f.loops:
                loop[uv].uv = u
            f.smooth = bool(f[self.smooth])
        me = bpy.data.meshes.new(name)
        bm.to_mesh(me)
        bm.free()
        me.attributes.remove(me.attributes["paint"])
        me.attributes.remove(me.attributes["smooth"])
        ob = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(ob)
        return ob


def camo_value(p, plane):
    """Smooth pattern in plan and side view: a few crossing waves, so the bands read as the big soft-edged
    blotches of a real scheme (Euro I wraps around the whole aircraft; Soviet schemes cover the upper side)."""
    s = plane["camo"].get("scale", 1.0)
    x, y, z = p.x / s, abs(p.y) / s * plane["camo"].get("sym", 1) + p.y / s * (1 - plane["camo"].get("sym", 1)), p.z / s
    seed = plane["camo"].get("seed", 0)
    v = 0.0
    for i, (kx, ky, kz, ph) in enumerate(((0.55, 0.31, 0.2, 0.3), (-0.27, 0.62, 0.35, 1.7),
                                          (0.41, -0.48, 0.5, 2.9), (0.83, 0.12, -0.6, 4.1))):
        v += math.sin(kx * x + ky * y + kz * z + ph + seed * (i + 1)) / (1 + 0.3 * i)
    return v


def band_edges(vals, f_areas, n):
    """Area-weighted quantiles: each camouflage colour covers about the same share of the skin."""
    items = sorted(vals.items(), key=lambda kv: kv[1])
    total = sum(f_areas[f] for f, _ in items) or 1
    edges, acc, k = [], 0.0, 1
    for f, v in items:
        acc += f_areas[f]
        while k < n and acc >= total * k / n:
            edges.append(v)
            k += 1
    return edges


# ---------- the aircraft (metres; x forward from the centre of mass, z up) ----------
EURO = {"colours": ["euro_dkgreen", "euro_green", "euro_gray"], "scale": 1.25, "sym": 0.0, "seed": 0}
SOVIET = {"colours": ["sov_tan", "sov_brown", "sov_green", "sov_dkgreen"], "under": "sov_blue", "scale": 1.1,
          "sym": 0.0, "seed": 0}


def a10(b):
    """Fairchild A-10A Thunderbolt II (USAF bomb run): 16.26 m long, 17.53 m span, European I 'lizard'."""
    b.loft([(8.15, -0.25, 0, 0), (7.7, -0.15, 0.42, 0.45), (6.6, -0.02, 0.74, 0.82), (5.0, 0.05, 0.86, 0.98),
            (2.5, 0.0, 0.9, 1.0), (0.0, 0.0, 0.86, 0.9), (-3.0, 0.15, 0.62, 0.72), (-6.0, 0.3, 0.36, 0.46),
            (-7.7, 0.38, 0.2, 0.3), (-8.1, 0.4, 0, 0)])
    b.loft([(8.75, -0.32, 0.07, 0.07), (8.0, -0.32, 0.09, 0.09)], "nozzle", seg=8)  # GAU-8 muzzle
    b.loft([(6.05, 0.75, 0, 0), (5.5, 0.98, 0.42, 0.36), (4.4, 1.12, 0.46, 0.42), (3.6, 1.02, 0.34, 0.28),
            (3.2, 0.9, 0, 0)], "canopy")
    # straight wing: flat centre section, 7 deg dihedral outboard
    b.surface((1.3, 0.6, -0.55, 3.0, 0.16), (1.05, 3.0, -0.55, 2.7, 0.15), spans=3)
    b.surface((1.05, 3.0, -0.55, 2.7, 0.15), (0.55, 8.76, 0.16, 1.5, 0.12), spans=7)
    b.loft([(2.6, -0.62, 0, 0), (2.1, -0.66, 0.26, 0.24), (0.6, -0.66, 0.3, 0.26), (0.0, -0.62, 0, 0)],
           y=3.15, mirror=True)  # main gear fairings
    # TF34 nacelles high on the rear fuselage
    nac = [(-0.85, 1.05, 0.58, 0.58), (-1.5, 1.05, 0.66, 0.66), (-3.4, 1.05, 0.6, 0.6), (-4.3, 1.05, 0.46, 0.46),
           (-4.75, 1.05, 0.38, 0.38)]
    b.loft(nac, y=1.4, n=2, mirror=True)
    b.loft([(-0.8, 1.05, 0.5, 0.5), (-0.84, 1.05, 0.5, 0.5)], "intake", y=1.4, n=2, mirror=True)
    b.loft([(-4.7, 1.05, 0.34, 0.34), (-4.95, 1.05, 0.3, 0.3)], "nozzle", y=1.4, n=2, mirror=True)
    b.box((-3.3, 0.85, 0.45), (-1.7, 1.05, 0.62), mirror=True)  # pylons
    # tailplane with twin fins at its tips
    b.surface((-5.95, 0.0, 0.42, 1.65, 0.1), (-6.15, 2.95, 0.42, 1.45, 0.1), spans=4)
    for side in (1, -1):
        b.surface((-5.55, 2.95 * side, -0.35, 1.95, 0.08), (-6.2, 2.95 * side, 2.55, 1.25, 0.08), vertical=True,
                  mirror=False, spans=5)
    for y in (2.3, 3.8):  # pylons for four Mk 82s (ACR_Mk82, hung by ACR_LocalFlyover.Hardpoints)
        b.store(2.0, -0.2, y, -1.05, 0.15, "camo", pylon=0.25, body=False)


def a10_spec():
    return {"name": "A10A", "build": a10, "camo": EURO, "role": "West bomb run"}


def su25(b):
    """Sukhoi Su-25 'Frogfoot' (Soviet bomb run): 15.33 m long, 14.36 m span, 1980s four-colour over blue."""
    b.loft([(7.75, 0.0, 0.1, 0.12), (7.1, -0.02, 0.42, 0.45), (5.6, 0.05, 0.64, 0.74), (4.2, 0.1, 0.72, 0.84),
            (2.0, 0.12, 0.74, 0.86), (-1.0, 0.18, 0.68, 0.8), (-4.5, 0.32, 0.45, 0.6), (-6.8, 0.48, 0.26, 0.4),
            (-7.6, 0.52, 0, 0)], n=2.8)
    b.loft([(4.95, 0.86, 0, 0), (4.5, 1.06, 0.38, 0.28), (3.6, 1.1, 0.4, 0.3), (2.8, 1.0, 0.3, 0.2),
            (2.4, 0.92, 0, 0)], "canopy")
    b.loft([(2.4, 0.95, 0.3, 0.2), (0.5, 0.95, 0.34, 0.24), (-3.5, 0.75, 0.24, 0.16), (-4.4, 0.65, 0, 0)])  # spine
    # R-95Sh nacelles along the fuselage sides, under the wing root
    b.loft([(2.65, -0.1, 0.44, 0.56), (1.6, -0.1, 0.5, 0.62), (-2.0, -0.1, 0.5, 0.6), (-4.6, -0.05, 0.42, 0.46),
            (-5.5, 0.0, 0.32, 0.32)], y=0.98, n=3.2, mirror=True)
    b.loft([(2.7, -0.1, 0.36, 0.48), (2.66, -0.1, 0.36, 0.48)], "intake", y=0.98, n=3.2, mirror=True)
    b.loft([(-5.45, 0.0, 0.28, 0.28), (-5.75, 0.0, 0.26, 0.26)], "nozzle", y=0.98, n=2, mirror=True)
    # shoulder wing, 20 deg sweep, slight anhedral; speed-brake pods on the tips
    b.surface((1.9, 1.2, 0.48, 3.5, 0.13), (-0.3, 7.18, 0.2, 1.45, 0.1), spans=7)
    b.loft([(0.5, 0.2, 0, 0), (0.2, 0.2, 0.15, 0.15), (-1.4, 0.2, 0.15, 0.15), (-1.75, 0.2, 0, 0)], y=7.18,
           n=2, seg=10, mirror=True)
    b.surface((-4.85, 0.3, 1.05, 1.9, 0.09), (-5.8, 2.8, 1.3, 0.95, 0.08), spans=4)
    fin = ((-4.1, 0.0, 0.85, 3.2, 0.09), (-6.35, 0.0, 3.95, 1.3, 0.08))
    b.surface(*fin, vertical=True, mirror=False, spans=6)
    b.fin_star(*fin, 0.5, 0.55)
    for y in (3.0, 4.4):  # pylons for four FAB-250s (ACR_FAB250, hung by ACR_LocalFlyover.Hardpoints)
        b.store(1.4, -0.6, y, -0.25, 0.2, "camo", pylon=0.3, body=False)


def su25_spec():
    return {"name": "Su25", "build": su25, "camo": SOVIET, "role": "East bomb run"}


def rf4c(b):
    """McDonnell Douglas RF-4C Phantom II (USAF recon flight): 19.2 m long, 11.7 m span, European I."""
    b.loft([(9.65, 0.12, 0, 0), (9.0, 0.1, 0.32, 0.36), (7.8, 0.05, 0.55, 0.6), (6.2, 0.05, 0.7, 0.78),
            (4.5, 0.05, 0.84, 0.84), (2.5, 0.05, 0.95, 0.85), (0.0, 0.05, 1.1, 0.8), (-3.0, 0.1, 1.15, 0.75),
            (-5.5, 0.18, 0.92, 0.6), (-7.2, 0.32, 0.55, 0.45), (-9.5, 0.55, 0, 0)], n=2.6)
    b.loft([(4.1, 0.05, 0.36, 0.58), (3.0, 0.05, 0.4, 0.62), (0.2, 0.05, 0.38, 0.58), (-0.6, 0.05, 0, 0)],
           y=0.98, n=6, mirror=True)  # intakes
    b.loft([(4.15, 0.05, 0.28, 0.48), (4.1, 0.05, 0.28, 0.48)], "intake", y=0.98, n=6, mirror=True)
    b.loft([(6.95, 0.68, 0, 0), (6.4, 0.98, 0.42, 0.34), (5.0, 1.1, 0.45, 0.38), (3.6, 1.08, 0.42, 0.33),
            (2.8, 0.92, 0.26, 0.2), (2.3, 0.8, 0, 0)], "canopy")
    b.loft([(-6.7, -0.05, 0.46, 0.46), (-7.8, -0.05, 0.44, 0.44)], "nozzle", y=0.55, n=2, mirror=True)
    # low wing: 45 deg leading edge, flat inner panel, 12 deg dihedral outer panel
    b.surface((1.6, 0.9, -0.45, 5.4, 0.07), (-1.5, 4.0, -0.45, 2.7, 0.06), spans=5)
    b.surface((-1.5, 4.0, -0.45, 2.7, 0.06), (-3.35, 5.85, -0.06, 1.55, 0.05), spans=3)
    # anhedral stabilator, tall fin
    b.surface((-6.4, 0.5, 0.45, 2.6, 0.07), (-8.1, 2.9, -0.53, 1.0, 0.06), spans=4)
    b.surface((-4.2, 0.0, 0.7, 4.4, 0.07), (-7.6, 0.0, 3.6, 1.4, 0.06), vertical=True, mirror=False, spans=6)
    b.store(2.6, -3.0, 3.4, -1.05, 0.4, "euro_gray", pylon=0.3)  # 370 gal tanks


def rf4c_spec():
    return {"name": "RF4C", "build": rf4c, "camo": EURO, "role": "West recon flight"}


def su17(b):
    """Sukhoi Su-17M4R 'Fitter-K' (Soviet recon flight): 19.0 m long, wings at 45 deg (spread 13.68 m),
    KKR-1 recon pod on the centreline."""
    b.loft([(9.35, 0.0, 0.55, 0.55), (8.0, 0.0, 0.68, 0.74), (6.0, 0.05, 0.74, 0.82), (3.0, 0.05, 0.78, 0.86),
            (0.0, 0.05, 0.78, 0.86), (-4.0, 0.05, 0.72, 0.76), (-7.5, 0.05, 0.62, 0.62), (-8.6, 0.05, 0.55, 0.55)],
           n=2.2)
    b.loft([(9.36, 0.0, 0.47, 0.47), (9.3, 0.0, 0.47, 0.47)], "intake", n=2)
    b.loft([(10.05, 0.0, 0, 0), (9.35, 0.0, 0.3, 0.3), (9.1, 0.0, 0.3, 0.3)], "radome", n=2, seg=14)  # shock cone
    b.loft([(-8.55, 0.05, 0.5, 0.5), (-9.0, 0.05, 0.47, 0.47)], "nozzle", n=2)
    b.loft([(5.6, 0.75, 0.0, 0.0), (4.6, 0.85, 0.32, 0.26), (-3.0, 0.8, 0.3, 0.24), (-5.0, 0.7, 0, 0)])  # spine
    b.loft([(7.15, 0.6, 0, 0), (6.5, 0.95, 0.38, 0.3), (5.6, 1.0, 0.4, 0.32), (4.9, 0.92, 0.3, 0.2),
            (4.5, 0.85, 0, 0)], "canopy")
    # fixed glove and the outer panel at 45 deg
    b.surface((3.9, 0.7, -0.05, 5.1, 0.08), (1.0, 2.1, -0.05, 2.9, 0.08), spans=3)
    b.surface((1.0, 2.1, -0.05, 2.7, 0.08), (-1.85, 5.9, -0.05, 1.35, 0.07), spans=6)
    b.surface((-5.6, 0.6, -0.1, 2.4, 0.07), (-7.4, 3.3, -0.25, 1.0, 0.06), spans=4)
    fin = ((-3.3, 0.0, 0.7, 3.6, 0.08), (-6.7, 0.0, 3.6, 1.3, 0.07))
    b.surface(*fin, vertical=True, mirror=False, spans=6)
    b.surface((-5.5, 0.0, -0.5, 1.6, 0.06), (-6.3, 0.0, -1.0, 0.9, 0.06), vertical=True, mirror=False, spans=2)
    b.fin_star(*fin, 0.55, 0.58)
    b.store(4.2, -3.2, 0.0, -1.15, 0.3, "pod_gray", mirror=False, pylon=0.2)  # KKR-1
    b.store(2.2, -2.0, 1.55, -0.95, 0.34, "camo", pylon=0.3)  # drop tanks under the glove


def su17_spec():
    return {"name": "Su17M4R", "build": su17, "camo": SOVIET, "role": "East recon flight"}


def fins(b, x, root_r, span_r, root_ch, tip_ch, sweep, paint, th=0.025):
    """Four flat tail fins in an X (45 deg off the vertical, as a bomb hangs: clear of its pylon, and all four
    show from the side), root on the body at radius root_r, tip at span_r, the tip swept back by `sweep`."""
    for k in range(4):
        a = math.radians(45 + 90 * k)
        d, n = Vector((0, math.cos(a), math.sin(a))), Vector((0, -math.sin(a), math.cos(a))) * (th / 2)
        rings = []
        for r, xl, ch in ((root_r, x, root_ch), (span_r, x - sweep, tip_ch)):
            c = d * r
            rings.append([Vector((xl, 0, 0)) + c + n, Vector((xl - ch, 0, 0)) + c + n,
                          Vector((xl - ch, 0, 0)) + c - n, Vector((xl, 0, 0)) + c - n])
        b._faces(rings, paint)


def mk82(b):
    """Mk 82 500 lb low-drag bomb (the A-10's): 2.21 m, 273 mm across, olive drab with the yellow nose band
    of a filled bomb, conical tail with four fins. Origin at its middle, nose +X."""
    b.loft([(1.1, 0, 0, 0), (1.02, 0, 0.055, 0.055), (0.8, 0, 0.115, 0.115), (0.5, 0, 0.137, 0.137),
            (-0.4, 0, 0.137, 0.137), (-0.7, 0, 0.1, 0.1), (-0.95, 0, 0.065, 0.065), (-1.0, 0, 0, 0)], "od", n=2, smooth=True,
           seg=20)
    b.loft([(0.68, 0, 0.13, 0.13), (0.6, 0, 0.134, 0.134)], "yellow", n=2, seg=14)
    fins(b, -0.62, 0.09, 0.26, 0.45, 0.28, 0.18, "od")


def mk82_spec():
    return {"name": "Mk82", "build": mk82, "camo": EURO, "role": "A-10 bomb (Snakeye, petals shut)", "ortho": 3}


def mk82_open(b):
    """The Mk 82 Snakeye half a second after release: its Mk 15 tail's four petals sprung open into an X airbrake,
    hinged at the tail and flared out forward, broad face to the airflow (ACR_FallingBomb swaps this in)."""
    b.loft([(1.1, 0, 0, 0), (1.02, 0, 0.055, 0.055), (0.8, 0, 0.115, 0.115), (0.5, 0, 0.137, 0.137),
            (-0.4, 0, 0.137, 0.137), (-0.7, 0, 0.1, 0.1), (-0.95, 0, 0.065, 0.065), (-1.0, 0, 0, 0)], "od", n=2,
           smooth=True, seg=20)
    b.loft([(0.68, 0, 0.13, 0.13), (0.6, 0, 0.134, 0.134)], "yellow", n=2, seg=14)
    hinge_x, hinge_r, length, width, th = -0.97, 0.07, 0.48, 0.2, 0.012
    tilt = math.radians(70)  # off the tail's axis, swung forward
    for k in range(4):
        a = math.radians(45 + 90 * k)
        radial = Vector((0, math.cos(a), math.sin(a)))
        tangent = Vector((0, -math.sin(a), math.cos(a)))
        hinge = Vector((hinge_x, 0, 0)) + radial * hinge_r
        span = Vector((math.cos(tilt), 0, 0)) + radial * math.sin(tilt)
        normal = span.cross(tangent).normalized() * (th / 2)
        quad = [hinge - tangent * width / 2, hinge + tangent * width / 2,
                hinge + span * length + tangent * width / 2, hinge + span * length - tangent * width / 2]
        b._faces([[q + normal for q in quad], [q - normal for q in quad]], "od")
        b.box((hinge_x - 0.02, -0.01, -0.01), (hinge_x + 0.02, 0.01, 0.01), "od")


def mk82_open_spec():
    return {"name": "Mk82_Open", "build": mk82_open, "camo": EURO, "role": "A-10 bomb (Snakeye, petals open)",
            "ortho": 3}


def fab250(b):
    """FAB-250 M-62 (the Su-25's), measured off the museum photograph Jacob chose (Wikimedia Commons,
    "FAB-250-M62.JPG", 4 Nov), as fractions of its 1.96 m length and 325 mm body: a blunt elliptical nose with a
    small fuze tip (to 19 %), a straight cylinder (to 40 %, a seam), a long separate taper to a narrow tail cone
    (about a third of the diameter); four fins, upright and level, from 70 %, a short swept leading edge then a long straight outer
    edge running back into a narrow ring round the tail (92-100 %), as wide as the body; two loop lugs on top at
    19 and 30 %. Light grey. Origin at its middle, nose +X."""
    L, R = 1.96, 0.1625
    X = lambda f: L / 2 - f * L  # fraction of the length from the nose -> model x
    st = [(X(0.0), 0, 0, 0), (X(0.0), 0, 0.12 * R, 0.12 * R), (X(0.008), 0, 0.14 * R, 0.14 * R)]  # fuze tip
    for k in range(0, 11):  # elliptical nose, 0.8 % to 19 %
        f = 0.008 + (0.19 - 0.008) * k / 10
        r = max(0.2, math.sqrt(max(0.0, 1 - (1 - f / 0.19) ** 2)))
        st.append((X(f), 0, r * R, r * R))
    for f, r in ((0.36, 1.0), (0.45, 0.86), (0.55, 0.72), (0.65, 0.58), (0.74, 0.5), (0.85, 0.42),
                 (0.93, 0.37), (0.975, 0.33), (0.985, 0.2), (0.99, 0.0)):
        st.append((X(f), 0, r * R, r * R))
    b.loft(st, "fab_light", n=2, seg=24, smooth=True)
    # fins: flat plates upright and level (in line with the lugs, as photographed; they stay inside the body's
    # radius, clear of the pylon), outline (fraction, radius) from the photograph, outer edge inside the ring
    th = 0.006
    outline = [(0.70, 0.45), (0.757, 0.98), (1.0, 0.98), (1.0, 0.3)]
    for k in range(4):
        a = math.radians(90 * k)
        d = Vector((0, math.cos(a), math.sin(a)))
        n = Vector((0, -math.sin(a), math.cos(a))) * (th / 2)
        top = [Vector((X(f), 0, 0)) + d * (r * R) + n for f, r in outline]
        bot = [q - n * 2 for q in top]
        b._faces([top, bot], "fab_light")
    ring = []  # narrow band round the fin tips: outer face, back lip, inner face, front lip
    for r, f in ((1.0, 0.92), (1.0, 1.0), (0.965, 1.0), (0.965, 0.92), (1.0, 0.92)):
        ring.append([Vector((X(f), math.cos(2 * math.pi * i / 24) * r * R, math.sin(2 * math.pi * i / 24) * r * R))
                     for i in range(24)])
    b._faces(ring, "fab_light", cap0=False, cap1=False, smooth=True)
    for f in (0.19, 0.30):  # loop lugs: two posts and a bar
        x = X(f)
        b.box((x - 0.028, -0.006, R - 0.01), (x - 0.016, 0.006, R + 0.035), "fab_light")
        b.box((x + 0.016, -0.006, R - 0.01), (x + 0.028, 0.006, R + 0.035), "fab_light")
        b.box((x - 0.028, -0.006, R + 0.025), (x + 0.028, 0.006, R + 0.037), "fab_light")


def fab250_spec():
    return {"name": "FAB250", "build": fab250, "camo": SOVIET, "role": "Su-25 bomb", "ortho": 3}


OFAB_L, OFAB_R = 1.99, 0.1625
OFAB_X = lambda f: OFAB_L / 2 - f * OFAB_L  # fraction of the length from the nose -> model x


def cord(b, p0, p1, w, paint="cord"):
    """A line as a thin three-sided prism from p0 to p1."""
    d = (p1 - p0).normalized()
    a = d.cross(Vector((0, 0, 1)) if abs(d.z) < 0.9 else Vector((0, 1, 0))).normalized()
    c = d.cross(a)
    tri = [a * w, (-a * 0.5 + c * 0.866) * w, (-a * 0.5 - c * 0.866) * w]
    b._faces([[p0 + o for o in tri], [p1 + o for o in tri]], paint)


def ofab_body(b, deployed):
    """OFAB-250ShN, measured off the METIS side photograph (fenixinsight.com, "OFAB-250ShN") and the 16va.be
    MiG-27K loading photo, as fractions of 1.99 m and of its 325 mm body: a short truncated-cone nose with a small
    flat tip (to 16 %), a straight cylinder with weld seams at 16, 50 and 64 %, a gentle step down (to 82 %) into
    the parachute housing, which flares a little at the rear and carries four raised bands; four narrow fins upright
    and level (in line with the lugs) from 80 % to the end, each with a rail along its tip; two loop lugs at 31 and
    44 %. Light grey with darker seams. Stowed, the housing is shut by its cover; deployed, it is open."""
    X, R = OFAB_X, OFAB_R
    nose = [(0.0, 0.22), (0.003, 0.25), (0.03, 0.42), (0.07, 0.62), (0.11, 0.82), (0.145, 0.96), (0.16, 1.0),
            (0.64, 1.0), (0.70, 0.97), (0.78, 0.9), (0.82, 0.87), (0.90, 0.86), (0.93, 0.88), (0.96, 0.93),
            (0.99, 0.95), (1.0, 0.95)]
    b.loft([(X(f), 0, r * R, r * R) for f, r in nose], "fab_light", n=2, seg=28, smooth=True)
    b.loft([(X(0.0) + 0.004, 0, 0.0, 0.0), (X(0.0) + 0.004, 0, 0.18 * R, 0.18 * R), (X(0.0), 0, 0.22 * R, 0.22 * R)],
           "fab_seam", n=2, seg=16)  # fuze tip
    for f, r in ((0.16, 1.006), (0.5, 1.006), (0.64, 1.006), (0.87, 0.875), (0.905, 0.875), (0.935, 0.9),
                 (0.965, 0.945)):
        b.loft([(X(f) + 0.008, 0, r * R, r * R), (X(f) - 0.008, 0, r * R, r * R)], "fab_seam", n=2, seg=28,
               smooth=True)
    if deployed:  # the open housing: its inside, dark
        b.loft([(X(1.0), 0, 0.9 * R, 0.9 * R), (X(0.93), 0, 0.8 * R, 0.8 * R), (X(0.93) - 0.001, 0, 0.0, 0.0)],
               "intake", n=2, seg=28)
    else:  # its cover, a little recessed
        b.loft([(X(1.0), 0, 0.88 * R, 0.88 * R), (X(1.0) + 0.012, 0, 0.84 * R, 0.84 * R),
                (X(1.0) + 0.012, 0, 0.0, 0.0)], "fab_seam", n=2, seg=28)
    th = 0.007
    outline = [(0.80, 0.86), (0.83, 1.2), (0.975, 1.2), (0.985, 0.9)]
    for k in range(4):
        a = math.radians(90 * k)
        d = Vector((0, math.cos(a), math.sin(a)))
        n = Vector((0, -math.sin(a), math.cos(a))) * (th / 2)
        top = [Vector((X(f), 0, 0)) + d * (r * R) + n for f, r in outline]
        b._faces([top, [q - n * 2 for q in top]], "fab_light")
        cord(b, Vector((X(0.83), 0, 0)) + d * (1.2 * R), Vector((X(0.985), 0, 0)) + d * (1.2 * R), 0.008, "fab_light")
    for f in (0.31, 0.44):  # loop lugs
        x = X(f)
        b.box((x - 0.028, -0.006, R - 0.01), (x - 0.016, 0.006, R + 0.035), "fab_light")
        b.box((x + 0.016, -0.006, R - 0.01), (x + 0.028, 0.006, R + 0.035), "fab_light")
        b.box((x - 0.028, -0.006, R + 0.025), (x + 0.028, 0.006, R + 0.037), "fab_light")


def ofab250shn(b):
    ofab_body(b, False)


def ofab250shn_open(b):
    """Deployed: the brake chute streaming straight back on its riser, as the bomb falls nose first along its path.
    Four short bridle lines from the housing rim to a riser 1.1 m back; the riser to 2.0 m; twelve suspension lines
    out to the canopy's mouth 3.3 m behind the tail; the canopy a 1.4 m inflated cup, mouth towards the bomb, its
    twelve gores bulging between the lines. Two-sided (an outer and an inner skin), so it reads from any angle."""
    ofab_body(b, True)
    X, R = OFAB_X, OFAB_R
    tail = X(1.0)
    knot, link = Vector((tail - 1.1, 0, 0)), Vector((tail - 2.0, 0, 0))
    for k in range(4):
        a = math.radians(45 + 90 * k)
        cord(b, Vector((tail, math.cos(a) * 0.8 * R, math.sin(a) * 0.8 * R)), knot, 0.012)
    cord(b, knot, link, 0.02)
    mouth, gores = tail - 3.3, 12
    prof = [(0.0, 0.6), (0.12, 0.7), (0.26, 0.66), (0.4, 0.5), (0.5, 0.28), (0.55, 0.05)]  # metres back, radius
    rings = []

    def ring(s, r):
        out = []
        for i in range(gores * 4):
            th = 2 * math.pi * i / (gores * 4)
            lobe = 1 + 0.06 * abs(math.sin(gores / 2 * th))  # gores bulge between the lines
            out.append(Vector((mouth - s, math.cos(th) * r * lobe, math.sin(th) * r * lobe)))
        return out
    for s, r in prof:
        rings.append(ring(s, r))
    for s, r in reversed(prof):
        rings.append(ring(s - 0.015, max(0.02, r - 0.015)))
    rings.append(ring(0.0, 0.6))
    b._faces(rings, "chute", cap0=False, cap1=False, smooth=True)
    for i in range(gores):
        th = 2 * math.pi * i / gores
        cord(b, link, Vector((mouth, math.cos(th) * 0.6, math.sin(th) * 0.6)), 0.008)


def ofab_spec():
    return {"name": "OFAB250ShN", "build": ofab250shn, "camo": SOVIET, "role": "Su-25 bomb (stowed)", "ortho": 3}


def ofab_open_spec():
    return {"name": "OFAB250ShN_Open", "build": ofab250shn_open, "camo": SOVIET,
            "role": "Su-25 bomb (brake chute out)", "ortho": 6.5, "centre": (-2.3, 0, 0)}


PLANES = [a10_spec(), su25_spec(), rf4c_spec(), su17_spec(), mk82_spec(), mk82_open_spec(), fab250_spec(),
          ofab_spec(), ofab_open_spec()]

# ---------- build, export, preview ----------
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
palette_img, palette_path = write_palette()
mat = bpy.data.materials.new("ACR_Aircraft_Palette")  # the FBX material name: Workbench links Data/<name>.emat
mat.use_nodes = True
tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
tex.image = palette_img
tex.interpolation = "Closest"
bsdf = mat.node_tree.nodes["Principled BSDF"]
mat.node_tree.nodes.active = tex


def preview_texture(on):
    """Linked only for the previews and the local .blend: an FBX would carry the image's absolute path."""
    for link in list(mat.node_tree.links):
        if link.from_node == tex:
            mat.node_tree.links.remove(link)
    if on:
        mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])


posix = lambda q: q.replace("\\", "/")
JOB_DIR = os.path.join(ROOT, ".local-workbench", "profile")
os.makedirs(JOB_DIR, exist_ok=True)
job = []
emat = os.path.join(DATA, "ACR_Aircraft_Palette.emat")
job.append("EMAT %s||MetalnessScale=0|EnableFarHiding=0|BCRMap=%s" % (posix(emat), posix(palette_path)))
report = []
for plane in PLANES:
    if ONLY and plane["name"] not in ONLY:
        continue
    name = "ACR_" + plane["name"]
    b = Builder()
    plane["build"](b)
    ob = b.finish(name, plane)
    ob.data.materials.append(mat)
    bb = [ob.matrix_world @ Vector(c) for c in ob.bound_box]
    size = [max(p[i] for p in bb) - min(p[i] for p in bb) for i in range(3)]
    tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    report.append("%s (%s): %.2f x %.2f x %.2f m, %d tris" % (name, plane["role"], size[0], size[1], size[2], tris))
    for o in scene.objects:
        o.select_set(o == ob)
    bpy.context.view_layer.objects.active = ob
    fbx = os.path.join(OUT, name + ".fbx")
    preview_texture(False)
    bpy.ops.export_scene.fbx(filepath=fbx, use_selection=True, axis_forward="-Z", axis_up="Y",
                             apply_unit_scale=True, mesh_smooth_type="FACE", use_tspace=False,
                             add_leaf_bones=False, bake_anim=False, path_mode="STRIP")
    job.append("FBX %s" % posix(fbx))
    preview_texture(True)

    # previews: from below as the ground sees it, side, top, three-quarter
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 900, 600
    world = bpy.data.worlds.get("sky") or bpy.data.worlds.new("sky")
    world.color = (0.55, 0.68, 0.85)
    scene.world = world
    scene.display.shading.background_type = "WORLD"
    for o in list(scene.objects):
        o.hide_render = o != ob
    cam_data = bpy.data.cameras.get("cam") or bpy.data.cameras.new("cam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = plane.get("ortho", 23)
    cam = bpy.data.objects.get("cam") or bpy.data.objects.new("cam", cam_data)
    if cam.name not in scene.collection.objects:
        scene.collection.objects.link(cam)
    scene.camera = cam
    aim = 0.5 if plane.get("ortho", 23) > 10 else 0.0
    views = [("below", (0.01, 0.0, -40)), ("side", (0, -40, aim)), ("top", (0.01, 0, 40)), ("quarter", (22, -26, -16))]
    if plane.get("ortho", 23) <= 10:
        views.append(("rear", (-40, 0.0, 0.01)))
    centre = Vector(plane.get("centre", (0, 0, 0)))  # where the previews look (a trailing chute sits aft)
    for view, loc in views:
        cam.location = Vector(loc) + centre
        d = Vector((0, 0, aim)) - Vector(loc)
        cam.rotation_euler = d.to_track_quat("-Z", {"below": "X", "top": "X", "rear": "Y"}.get(view, "Z")).to_euler()
        cam_data.ortho_scale = 0.6 if view == "rear" else plane.get("ortho", 23)
        scene.render.filepath = os.path.join(PREV, "%s_%s.png" % (name, view))
        bpy.ops.render.render(write_still=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ROOT, ".local-aircraft", name + ".blend"), copy=True)

with open(os.path.join(JOB_DIR, "aircraft_job.txt"), "w") as f:
    f.write("\n".join(job) + "\n")
print("\n".join(["[aircraft] " + r for r in report] + ["[aircraft] job: " + os.path.join(JOB_DIR, "aircraft_job.txt")]))
