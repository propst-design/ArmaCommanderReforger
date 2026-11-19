"""Workshop preview images for ACA's add-ons (Cold War pack and the map add-ons).

Usage:  python tools/workshop/make_addon_thumbnails.py [KEY ...]
Writes tools/workshop/addons/<key>.png at 1920x1080. Needs only Pillow and Windows' Bahnschrift.

One family look: the ACA lockup lifted from acr-workshop-preview-1980s-v2.png, a title plate
under it, and a tactical-map overlay. The Cold War pack reuses the 1980s hero scene; each map
add-on gets a procedural topographic map in that terrain's character (not the terrain's own
data or art, which belong to its authors). Seeds are fixed, so reruns give the same images.
"""
import math
import os
import random
import sys

from PIL import Image, ImageChops, ImageDraw, ImageEnhance, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
HERO = os.path.join(HERE, 'acr-workshop-preview-1980s-v2.png')
OUT = os.path.join(HERE, 'addons')
W, H = 1920, 1080
FONT = 'C:/Windows/Fonts/bahnschrift.ttf'

CYAN = (110, 235, 225)
BLUE = (90, 160, 255)
RED = (235, 70, 60)
PAPER = (236, 232, 222)


def font(size, style='Bold Condensed'):
    f = ImageFont.truetype(FONT, size)
    f.set_variation_by_name(style)
    return f


# ---------------------------------------------------------------- logo

def aca_logo(width):
    """The ArmA / COMMANDER / REFORGER lockup as an RGBA image, lifted from the hero art."""
    hero = Image.open(HERO).convert('L')
    crop = hero.crop((80, 50, 585, 315))
    mask = crop.point(lambda v: 0 if v < 170 else (255 if v > 210 else (v - 170) * 255 // 40))
    scale = width / crop.width
    size = (width, round(crop.height * scale))
    mask = mask.resize(size, Image.LANCZOS)
    logo = Image.new('RGBA', size, PAPER + (0,))
    logo.putalpha(mask)
    return logo


def shadowed(base, overlay, pos, blur=10, strength=200):
    """Paste RGBA overlay with a soft dark shadow, for legibility on busy backgrounds."""
    pad = blur * 3
    a = overlay.getchannel('A')
    sh = Image.new('L', (overlay.width + pad * 2, overlay.height + pad * 2), 0)
    sh.paste(a, (pad, pad))
    sh = sh.filter(ImageFilter.GaussianBlur(blur)).point(lambda v: min(255, v * strength // 100))
    dark = Image.new('RGBA', sh.size, (0, 0, 0, 0))
    dark.putalpha(sh)
    base.alpha_composite(dark, (pos[0] - pad + 3, pos[1] - pad + 5))
    base.alpha_composite(overlay, pos)


def text_layer(text, fnt, fill, tracking=0):
    """Text as a tight RGBA layer; tracking in pixels between letters."""
    if tracking:
        widths = [fnt.getbbox(ch)[2] for ch in text]
        tw = sum(widths) + tracking * (len(text) - 1)
    else:
        tw = fnt.getbbox(text)[2]
    asc, desc = fnt.getmetrics()
    layer = Image.new('RGBA', (tw + 4, asc + desc), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    if tracking:
        x = 0
        for ch, cw in zip(text, widths):
            d.text((x, 0), ch, font=fnt, fill=fill)
            x += cw + tracking
    else:
        d.text((0, 0), text, font=fnt, fill=fill)
    return layer.crop(layer.getbbox())


def title_block(img, kicker, name, accent, footer, x=96, ly=70, logo_w=470, max_w=760):
    """Logo, then accent rule + kicker + big name; footer line bottom-left."""
    logo = aca_logo(logo_w)
    shadowed(img, logo, (x, ly), blur=14, strength=160)
    y = ly + logo.height + 46

    k = text_layer(kicker, font(30, 'SemiBold'), accent + (255,), tracking=7)
    shadowed(img, k, (x + 26, y), blur=6)
    size = 132
    while True:
        n = text_layer(name, font(size), PAPER + (255,), tracking=2)
        if n.width <= max_w or size <= 70:
            break
        size -= 4
    ny = y + k.height + 18
    shadowed(img, n, (x + 24, ny), blur=12, strength=180)
    d = ImageDraw.Draw(img)
    d.rectangle((x, y - 2, x + 9, ny + n.height + 2), fill=accent + (255,))

    if footer:
        f = text_layer(footer, font(30, 'SemiBold SemiCondensed'), (215, 220, 215, 255), tracking=2)
        shadowed(img, f, (x, H - 74 - f.height // 2), blur=6)


# ---------------------------------------------------------------- noise and terrain

def noise(rng, octaves, w=W, h=H, base=4, persistence=0.55):
    """Fractal value noise as an L image, normalised to 0..255."""
    acc = None
    amp, total = 1.0, 0.0
    layers = []
    for o in range(octaves):
        cw = max(2, int(base * (2 ** o) * w / h))
        ch = max(2, int(base * (2 ** o)))
        small = Image.new('L', (cw, ch))
        small.putdata([rng.randrange(256) for _ in range(cw * ch)])
        layers.append((small.resize((w, h), Image.BICUBIC), amp))
        total += amp
        amp *= persistence
    for layer, a in layers:
        weighted = layer.point(lambda v, a=a: v * a / total)
        acc = weighted if acc is None else ImageChops.add(acc, weighted)
    lo, hi = acc.getextrema()
    return acc.point(lambda v: (v - lo) * 255 // max(1, hi - lo))


def gradient(w, h, fn):
    """L image from fn(u, v) -> 0..1, built small then upscaled (fn is plain Python)."""
    sw, sh = 192, 108
    g = Image.new('L', (sw, sh))
    g.putdata([max(0, min(255, int(255 * fn(i / (sw - 1), j / (sh - 1)))))
               for j in range(sh) for i in range(sw)])
    return g.resize((w, h), Image.BICUBIC)


def multiply(a, b):
    return ImageChops.multiply(a, b)


def lut(stops):
    """256-entry RGB ramp from [(value, (r, g, b)), ...]."""
    out = []
    for v in range(256):
        for (v0, c0), (v1, c1) in zip(stops, stops[1:]):
            if v0 <= v <= v1:
                t = (v - v0) / max(1, v1 - v0)
                out.append(tuple(round(c0[i] + (c1[i] - c0[i]) * t) for i in range(3)))
                break
        else:
            out.append(stops[-1][1] if v > stops[-1][0] else stops[0][1])
    return out


def colourise(height, stops):
    ramp = lut(stops)
    return Image.merge('RGB', [height.point([c[i] for c in ramp]) for i in range(3)])


def hillshade(height, strength=0.9):
    """Cheap NW-lit relief from a shifted difference."""
    sm = height.filter(ImageFilter.GaussianBlur(2))
    a = ImageChops.offset(sm, 3, 3)
    diff = ImageChops.subtract(sm, a, scale=1, offset=128)
    diff = diff.filter(ImageFilter.GaussianBlur(1.5))
    return ImageEnhance.Contrast(diff).enhance(4.0 * strength)


def contours(height, step, sea=None):
    """Thin lines every `step`, index lines every 5th step, as L masks."""
    q = height.point(lambda v: v // step)
    edges = q.filter(ImageFilter.FIND_EDGES).point(lambda v: 255 if v else 0)
    qi = height.point(lambda v: v // (step * 5))
    index = qi.filter(ImageFilter.FIND_EDGES).point(lambda v: 255 if v else 0)
    index = index.filter(ImageFilter.MaxFilter(3))
    if sea is not None:
        land = height.point(lambda v: 255 if v > sea else 0)
        edges = ImageChops.multiply(edges, land)
        index = ImageChops.multiply(index, land)
    return edges, index


def topo_map(seed, shape, stops, sea=None, line=(40, 30, 20), line_alpha=70, forest=None,
             step=9, snow=False, relief=0.9):
    rng = random.Random(seed)
    h = noise(rng, 6, base=3)
    if shape is not None:
        h = shape(h, rng)
    img = colourise(h, stops)
    shade = hillshade(h, relief)
    img = ImageChops.overlay(img, Image.merge('RGB', [shade] * 3))
    if forest is not None:
        fcol, fcover = forest
        fn = noise(rng, 5, base=6)
        speck = Image.effect_noise((W, H), 90).point(lambda v: 255 if v > 150 else 0)
        mask = fn.point(lambda v: 255 if v > fcover else 0)
        if sea is not None:
            mask = ImageChops.multiply(mask, h.point(lambda v: 255 if v > sea + 6 else 0))
        mask = ImageChops.multiply(mask, ImageChops.lighter(speck, Image.new('L', (W, H), 120)))
        mask = mask.filter(ImageFilter.GaussianBlur(1.2)).point(lambda v: v * 6 // 10)
        img.paste(Image.new('RGB', (W, H), fcol), (0, 0), mask)
    thin, index = contours(h, step, sea)
    img.paste(Image.new('RGB', (W, H), line), (0, 0), thin.point(lambda v: v * line_alpha // 255))
    img.paste(Image.new('RGB', (W, H), line), (0, 0),
              index.point(lambda v: v * min(255, line_alpha * 2) // 255))
    if sea is not None:
        coast = h.point(lambda v: 255 if v > sea else 0).filter(ImageFilter.FIND_EDGES)
        coast = coast.filter(ImageFilter.MaxFilter(3))
        img.paste(Image.new('RGB', (W, H), line), (0, 0), coast.point(lambda v: v * 3 // 5))
    if snow:
        img = ImageEnhance.Color(img).enhance(0.55)
    return img.convert('RGBA'), h


# ---------------------------------------------------------------- tactical overlay

def grid(img, colour=CYAN, cell=240, alpha=55):
    d = ImageDraw.Draw(img)
    f = font(22, 'SemiBold')
    for i, x in enumerate(range(0, W + 1, cell)):
        d.line((x, 0, x, H), fill=colour + (alpha,), width=1)
    for j, y in enumerate(range(0, H + 1, cell)):
        d.line((0, y, W, y), fill=colour + (alpha,), width=1)
    for i, x in enumerate(range(0, W, cell)):
        for j, y in enumerate(range(0, H, cell)):
            if i >= 4:
                d.text((x + 8, y + 6), f'{"ABCDEFGHIJ"[i]}{j + 1}', font=f, fill=colour + (alpha + 40,))


def unit_marker(d, x, y, colour, kind='inf', label=None, w=46, h=30):
    fill = colour + (60,)
    d.rectangle((x - w // 2, y - h // 2, x + w // 2, y + h // 2), outline=colour + (255,), width=3, fill=fill)
    if kind == 'inf':
        d.line((x - w // 2, y - h // 2, x + w // 2, y + h // 2), fill=colour + (255,), width=3)
        d.line((x - w // 2, y + h // 2, x + w // 2, y - h // 2), fill=colour + (255,), width=3)
    elif kind == 'armour':
        d.ellipse((x - w // 2 + 7, y - h // 2 + 7, x + w // 2 - 7, y + h // 2 - 7), outline=colour + (255,), width=3)
    elif kind == 'air':
        d.arc((x - 15, y - 8, x, y + 8), 200, 340, fill=colour + (255,), width=3)
        d.arc((x, y - 8, x + 15, y + 8), 200, 340, fill=colour + (255,), width=3)
    if label:
        d.text((x + w // 2 + 10, y - 13), label, font=font(24, 'SemiBold'), fill=colour + (255,),
               stroke_width=3, stroke_fill=(8, 12, 14, 230))


def objective(d, x, y, colour, label):
    for r, a in ((34, 255), (46, 90)):
        d.ellipse((x - r, y - r, x + r, y + r), outline=colour + (a,), width=3)
    d.line((x - 10, y, x + 10, y), fill=colour + (255,), width=3)
    d.line((x, y - 10, x, y + 10), fill=colour + (255,), width=3)
    d.text((x + 52, y - 14), label, font=font(26, 'Bold SemiCondensed'), fill=colour + (255,),
           stroke_width=3, stroke_fill=(8, 12, 14, 230))


def route(d, pts, colour, width=4, dash=None):
    if dash is None:
        d.line(pts, fill=colour + (230,), width=width, joint='curve')
    else:
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            n = max(1, int(math.hypot(x1 - x0, y1 - y0) // (dash * 2)))
            for k in range(n):
                t0, t1 = k / n, (k + 0.5) / n
                d.line((x0 + (x1 - x0) * t0, y0 + (y1 - y0) * t0, x0 + (x1 - x0) * t1, y0 + (y1 - y0) * t1),
                       fill=colour + (230,), width=width)
    for x, y in pts[1:-1]:
        d.ellipse((x - 6, y - 6, x + 6, y + 6), fill=colour + (255,))
    x0, y0 = pts[-2]
    x1, y1 = pts[-1]
    ang = math.atan2(y1 - y0, x1 - x0)
    tip = [(x1, y1), (x1 - 22 * math.cos(ang - 0.45), y1 - 22 * math.sin(ang - 0.45)),
           (x1 - 22 * math.cos(ang + 0.45), y1 - 22 * math.sin(ang + 0.45))]
    d.polygon(tip, fill=colour + (255,))


def overlay_layer(draw_fn):
    layer = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    draw_fn(ImageDraw.Draw(layer))
    glow = layer.filter(ImageFilter.GaussianBlur(6))
    return glow, layer


def apply_overlay(img, draw_fn):
    glow, layer = overlay_layer(draw_fn)
    img.alpha_composite(glow)
    img.alpha_composite(layer)


def left_shade(img, reach=0.62, strength=210, bottom=150):
    """Darken the left and bottom so the title reads on any map."""
    g = gradient(W, H, lambda u, v: max(max(0.0, 1 - u / reach) ** 1.4,
                                        max(0.0, (v - 0.78) / 0.22) ** 1.6 * bottom / strength))
    g = g.point(lambda v: v * strength // 255)
    dark = Image.new('RGBA', (W, H), (8, 12, 14, 0))
    dark.putalpha(g)
    img.alpha_composite(dark)


def vignette(img, strength=120):
    g = gradient(W, H, lambda u, v: min(1.0, (((u - .5) * 1.6) ** 2 + ((v - .5) * 1.9) ** 2) ** 1.5))
    dark = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    dark.putalpha(g.point(lambda v: v * strength // 255))
    img.alpha_composite(dark)


# ---------------------------------------------------------------- per add-on

def cold_war():
    hero = Image.open(HERO).convert('RGB').resize((W, H), Image.LANCZOS)
    # colder, harder grade with a red cast in the shadows: same war, other side of the wire
    hero = ImageEnhance.Color(hero).enhance(0.55)
    hero = ImageEnhance.Contrast(hero).enhance(1.12)
    r, g, b = hero.split()
    r = r.point(lambda v: min(255, int(v * 1.02 + (40 - v * 0.16 if v < 250 else 0))))
    hero = Image.merge('RGB', (r, g, b)).convert('RGBA')
    left_shade(hero, reach=0.55, strength=150, bottom=170)

    # the scene already shows our platoon's plan to OBJ ECHO; add the Soviet side closing on it
    def tac(d):
        unit_marker(d, 1760, 250, RED, 'armour', None)
        d.text((1700, 285), 'T-72B', font=font(24, 'SemiBold'), fill=RED + (255,),
               stroke_width=3, stroke_fill=(8, 12, 14, 230))
        unit_marker(d, 1300, 150, RED, 'air', 'MI-24V')
        route(d, [(1760, 280), (1720, 380), (1640, 470)], RED, dash=12)
        route(d, [(1330, 175), (1450, 300), (1560, 420)], RED, dash=12)
    apply_overlay(hero, tac)
    # the hero's own lockup sits at (80, 50), 505 px wide, in 1678 px art: redraw it exactly on top
    s = W / 1678
    title_block(hero, 'ARMA COMMANDER ACTUAL ADD-ON', 'COLD WAR PACK', RED,
                '1979-1991   |   ARMOUR   |   ATTACK HELICOPTERS   |   PERIOD INFANTRY   |   FIA GUERRILLAS',
                x=round(80 * s), ly=round(50 * s), logo_w=round(505 * s))
    vignette(hero, 90)
    return hero


def map_card(name, seed, terrain, accent, footer, tac):
    img, _ = terrain(seed)
    grid(img)
    left_shade(img)
    apply_overlay(img, tac)
    title_block(img, 'ARMA COMMANDER ACTUAL  MAP ADD-ON', name, accent, footer)
    vignette(img)
    return img


def coast_south(h, rng):
    """Land falling to a sea along the bottom-right (south coast)."""
    g = gradient(W, H, lambda u, v: min(1.0, max(0.0, 1.25 - 0.95 * v - 0.25 * u + 0.25)))
    return ImageChops.multiply(h, g).point(lambda v: min(255, int(v * 1.35)))


def islands(h, rng):
    blobs = noise(rng, 4, base=2)
    g = ImageChops.multiply(h, blobs)
    return ImageChops.multiply(g, gradient(W, H, lambda u, v: 0.55 + 0.45 * u)).point(lambda v: min(255, v * 2))


def ridged(h, rng):
    """Sharp ridges and deep valleys: mountain country."""
    r = h.point(lambda v: 255 - abs(v - 128) * 2)
    return ImageChops.add(r.point(lambda v: v * 7 // 10), h.point(lambda v: v * 3 // 10))


def rolling(h, rng):
    return h.filter(ImageFilter.GaussianBlur(6))


GREEN_LAND = [(0, (38, 72, 96)), (70, (44, 86, 108)), (74, (176, 168, 128)), (84, (110, 132, 82)),
              (150, (128, 140, 92)), (210, (150, 142, 108)), (255, (176, 166, 140))]


def chernarus(seed):
    return topo_map(seed, coast_south, GREEN_LAND, sea=72, forest=((52, 82, 46), 120))


def gulfcoast(seed):
    stops = [(0, (18, 70, 98)), (60, (28, 108, 128)), (88, (60, 150, 150)), (92, (214, 200, 150)),
             (104, (120, 150, 86)), (190, (96, 128, 70)), (255, (150, 150, 110))]
    return topo_map(seed, islands, stops, sea=90, forest=((62, 100, 52), 110), line=(30, 40, 30))


def kunar(seed):
    # green river-valley fields, ochre slopes, bare rock and snow on the high ridges
    stops = [(0, (70, 100, 52)), (55, (96, 112, 64)), (80, (150, 126, 86)), (160, (170, 146, 108)),
             (215, (150, 140, 128)), (238, (220, 222, 224)), (255, (246, 248, 250))]
    return topo_map(seed, ridged, stops, line=(50, 34, 18), line_alpha=60, step=7, relief=1.8)


def west_zagoria(seed):
    stops = [(0, (70, 96, 58)), (120, (98, 124, 72)), (200, (130, 140, 90)), (255, (160, 154, 112))]
    return topo_map(seed, rolling, stops, forest=((44, 70, 40), 110), step=8)


def zimnitrita(seed):
    stops = [(0, (40, 70, 96)), (66, (70, 104, 128)), (70, (200, 212, 220)), (160, (226, 232, 238)),
             (255, (250, 252, 255))]
    return topo_map(seed, coast_south, stops, sea=68, forest=((44, 62, 56), 150),
                    line=(60, 80, 100), line_alpha=80, snow=True, relief=1.4)


def tac_attack(blue_label, obj_label, red_label):
    def tac(d):
        unit_marker(d, 1080, 760, BLUE, 'inf', blue_label)
        unit_marker(d, 1180, 860, BLUE, 'armour')
        objective(d, 1500, 420, PAPER, obj_label)
        unit_marker(d, 1640, 280, RED, 'inf', red_label)
        route(d, [(1080, 730), (1200, 620), (1330, 520), (1450, 450)], CYAN)
        route(d, [(1640, 310), (1600, 380), (1545, 420)], RED, dash=10)
    return tac


SICK = (150, 230, 90)


def zombies():
    """The zombie add-on: an Everon-like coast at dusk, nests in the towns and hordes on the move."""
    img, _ = topo_map(66, coast_south, GREEN_LAND, sea=72, forest=((52, 82, 46), 120))
    rgb = ImageEnhance.Brightness(img.convert('RGB')).enhance(0.62)
    rgb = ImageEnhance.Color(rgb).enhance(0.5)
    r, g, b = rgb.split()
    img = Image.merge('RGB', (r.point(lambda v: int(v * 0.9)), g, b.point(lambda v: int(v * 0.85)))).convert('RGBA')
    grid(img, colour=SICK, alpha=40)
    left_shade(img)

    def tac(d):
        objective(d, 1240, 330, SICK, 'NEST')
        objective(d, 1640, 640, SICK, 'NEST')
        objective(d, 1450, 900, SICK, 'GROUND ZERO')
        route(d, [(1640, 610), (1560, 500), (1420, 420), (1290, 360)], SICK, dash=10)
        route(d, [(1450, 870), (1400, 760), (1300, 640)], SICK, dash=10)
        unit_marker(d, 1250, 700, BLUE, 'inf', '1 PLT')
        unit_marker(d, 1320, 780, BLUE, 'armour')
        unit_marker(d, 1780, 200, RED, 'inf', 'OPFOR')
        route(d, [(1250, 670), (1230, 540), (1235, 395)], CYAN)
    apply_overlay(img, tac)
    title_block(img, 'ARMA COMMANDER ACTUAL ADD-ON', 'ZOMBIES', SICK,
                'PROJECT OUTBREAK   |   NESTS AND HORDES   |   NIGHT RAIDS   |   TVTVZ OR TVZ')
    vignette(img, 140)
    return img


VIOLET = (190, 140, 255)


def civilians():
    """The Civil Population compat: a town on the command map, violet civilian rings, an evac out."""
    img, _ = topo_map(77, coast_south, GREEN_LAND, sea=72, forest=((52, 82, 46), 120))
    grid(img)
    left_shade(img)

    def ring(d, x, y, r, label):
        d.ellipse((x - r, y - r, x + r, y + r), outline=VIOLET + (235,), width=3, fill=VIOLET + (28,))
        d.text((x - r + 6, y - r - 34), label, font=font(24, 'SemiBold'), fill=VIOLET + (255,),
               stroke_width=3, stroke_fill=(8, 12, 14, 230))

    def tac(d):
        ring(d, 1330, 420, 95, 'CIVILIANS SEEN: MANY')
        ring(d, 1530, 560, 70, 'CIVILIANS SEEN: SOME')
        ring(d, 1180, 640, 55, 'A FEW')
        objective(d, 1600, 820, PAPER, 'EVAC POINT')
        route(d, [(1360, 500), (1440, 640), (1530, 760), (1575, 800)], VIOLET, dash=10)
        unit_marker(d, 1720, 900, BLUE, 'armour', 'EVAC TRUCK')
        route(d, [(1690, 885), (1650, 860)], CYAN)
        unit_marker(d, 1100, 860, BLUE, 'inf', '1 PLT')
        unit_marker(d, 1780, 260, RED, 'inf', 'OPFOR')
    apply_overlay(img, tac)
    title_block(img, 'ARMA COMMANDER ACTUAL ADD-ON', 'CIVIL POPULATION', VIOLET,
                'CIVILIAN RINGS   |   EVACUATIONS   |   CIVILIAN CASUALTIES IN YOUR FEED   |   NEEDS CIVIL POPULATION')
    vignette(img, 110)
    return img


ADDONS = {
    'coldwar': cold_war,
    'civpop': civilians,
    'zombies': zombies,
    'chernarus-minus': lambda: map_card('CHERNARUS MINUS', 11, chernarus, (120, 200, 110),
                                        'GENERATED BASES AND HQS   |   NEEDS THE CHERNARUS MINUS TERRAIN',
                                        tac_attack('1 PLT', 'OBJ ECHO', 'INS')),
    'gulfcoast-islands': lambda: map_card('GULFCOAST ISLANDS', 23, gulfcoast, (90, 200, 220),
                                          'GENERATED BASES AND HQS   |   NEEDS THE GULFCOAST ISLANDS TERRAIN',
                                          tac_attack('2 PLT', 'OBJ HARBOR', 'OPFOR')),
    'kunar-province': lambda: map_card('KUNAR PROVINCE', 37, kunar, (230, 170, 80),
                                       'GENERATED BASES AND HQS   |   NEEDS THE KUNAR PROVINCE TERRAIN',
                                       tac_attack('3 PLT', 'OBJ RIDGE', 'INS')),
    'west-zagoria': lambda: map_card('WEST ZAGORIA', 41, west_zagoria, (160, 210, 90),
                                     'GENERATED BASES AND HQS   |   NEEDS THE WEST ZAGORIA TERRAIN',
                                     tac_attack('1 PLT', 'OBJ FARM', 'USSR')),
    'zimnitrita': lambda: map_card('ZIMNITRITA', 53, zimnitrita, (140, 200, 255),
                                   'GENERATED BASES AND HQS   |   NEEDS THE ZIMNITRITA TERRAIN',
                                   tac_attack('2 PLT', 'OBJ DEPOT', 'USSR')),
}


def main(keys):
    os.makedirs(OUT, exist_ok=True)
    for key in keys or ADDONS:
        img = ADDONS[key]().convert('RGB')
        path = os.path.join(OUT, f'aca-{key}.png')
        img.save(path, optimize=True)
        print(path, os.path.getsize(path) // 1024, 'KB')


if __name__ == '__main__':
    main(sys.argv[1:])
