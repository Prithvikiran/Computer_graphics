#!/usr/bin/env python3
"""
Procedurally generates 128x128 24-bit BMP textures for the final project.
"""
import struct, random, math, os

W = H = 128

def write_bmp(path, pix):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    row_bytes = W * 3
    pad = (4 - (row_bytes % 4)) % 4
    img_size = (row_bytes + pad) * H
    file_size = 54 + img_size
    with open(path, "wb") as f:
        f.write(b"BM")
        f.write(struct.pack("<IHHI", file_size, 0, 0, 54))
        f.write(struct.pack("<IiiHHIIiiII",
                            40, W, H, 1, 24, 0, img_size, 2835, 2835, 0, 0))
        for y in range(H - 1, -1, -1):
            for x in range(W):
                r, g, b = pix[y][x]
                f.write(bytes((b & 255, g & 255, r & 255)))
            if pad:
                f.write(b"\x00" * pad)

def clamp(v):
    return 0 if v < 0 else 255 if v > 255 else int(v)



def grass():
    random.seed(1)
    p = [[(0,0,0)]*W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            n = random.randint(-18, 18)
            blade = 10 if (x*7+y) % 11 < 2 else 0
            p[y][x] = (clamp(45+n), clamp(115+n+blade), clamp(40+n))
    return p

def wood():
    random.seed(2)
    p = [[(0,0,0)]*W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            grain = 22*math.sin(x*0.45 + 2.0*math.sin(x*0.06))
            n = random.randint(-8, 8)
            p[y][x] = (clamp(135+grain+n), clamp(92+grain*0.6+n), clamp(48+grain*0.3+n))
    return p

def metal():
    random.seed(3)
    p = [[(0,0,0)]*W for _ in range(H)]
    for y in range(H):
        streak = 14*math.sin(y*0.8)
        for x in range(W):
            n = random.randint(-6, 6)
            v = 150 + streak + n
            p[y][x] = (clamp(v), clamp(v), clamp(v+6))
    return p

def plastic():
    random.seed(4)
    p = [[(0,0,0)]*W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            n = random.randint(-4, 4)
            sheen = 12*math.sin(x*0.10)
            v = 210 + n + sheen
            p[y][x] = (clamp(v), clamp(v), clamp(v))
    return p

def rubber():
    p = [[(0,0,0)]*W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            ridge = 40 if (x % 12) < 5 else 0
            v = 28 + ridge
            p[y][x] = (v, v, v)
    return p



def stone():
    """Grey stone with subtle variation for house walls"""
    random.seed(10)
    p = [[(0,0,0)]*W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            base = 165
            n = random.randint(-15, 15)
            mortar = 20 if (x % 32 < 2 or y % 16 < 2) else 0
            v = base + n - mortar
            p[y][x] = (clamp(v+5), clamp(v), clamp(v-3))
    return p

def roof():
    """Reddish-brown roof tiles"""
    random.seed(11)
    p = [[(0,0,0)]*W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            tile_edge = 15 if (y % 20 < 2) else 0
            n = random.randint(-8, 8)
            p[y][x] = (clamp(155+n-tile_edge), clamp(75+n-tile_edge), clamp(55+n-tile_edge))
    return p

def bark():
    """Brown bark with vertical streaks for tree trunks"""
    random.seed(12)
    p = [[(0,0,0)]*W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            streak = 18*math.sin(x*0.6 + 3*math.sin(x*0.08))
            n = random.randint(-10, 10)
            p[y][x] = (clamp(95+streak+n), clamp(65+streak*0.6+n), clamp(35+streak*0.3+n))
    return p

def leaf():
    """Green leafy texture for tree canopies"""
    random.seed(13)
    p = [[(0,0,0)]*W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            n = random.randint(-25, 25)
            cluster = 15*math.sin(x*0.3)*math.cos(y*0.4)
            p[y][x] = (clamp(35+n), clamp(110+n+cluster), clamp(30+n))
    return p

def hedge_tex():
    """Dense green hedge texture"""
    random.seed(14)
    p = [[(0,0,0)]*W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            n = random.randint(-12, 12)
            dot = 8 if ((x*3+y*7) % 13 < 3) else 0
            p[y][x] = (clamp(30+n), clamp(85+n+dot), clamp(25+n))
    return p

def gravel():
    """Grey-brown gravel path"""
    random.seed(15)
    p = [[(0,0,0)]*W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            n = random.randint(-20, 20)
            speckle = random.randint(0, 30)
            v = 140 + n + speckle
            p[y][x] = (clamp(v+8), clamp(v+3), clamp(v-5))
    return p

def dirt():
    """Dark brown garden bed soil"""
    random.seed(16)
    p = [[(0,0,0)]*W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            n = random.randint(-12, 12)
            p[y][x] = (clamp(90+n), clamp(60+n), clamp(35+n))
    return p

# ---- Write all textures ----
write_bmp("Textures/grass.bmp",   grass())
write_bmp("Textures/wood.bmp",    wood())
write_bmp("Textures/metal.bmp",   metal())
write_bmp("Textures/plastic.bmp", plastic())
write_bmp("Textures/rubber.bmp",  rubber())
write_bmp("Textures/stone.bmp",   stone())
write_bmp("Textures/roof.bmp",    roof())
write_bmp("Textures/bark.bmp",    bark())
write_bmp("Textures/leaf.bmp",    leaf())
write_bmp("Textures/hedge.bmp",   hedge_tex())
write_bmp("Textures/gravel.bmp",  gravel())
write_bmp("Textures/dirt.bmp",    dirt())
print("All 12 textures written to Textures/")
