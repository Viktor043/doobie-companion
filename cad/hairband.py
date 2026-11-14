"""
Doobie Companion — magnetic hairband (rev A)

Prints FLAT on the bed: the band's curve lies in XY, its width (front-back on the head)
is the Z extrusion. A flat fin rises out of the apex; its top (+Z) face is the mating face
for the Doobie case back: Doobie sits upright on top of the head, display facing forward.

Coordinates: band apex on +Y, ends at -Y, z = 0 on the bed. In wear: +Z = forward.
Fin coordinates == case coordinates (seen from the front) shifted by (0, FIN_CY):
the case back lies on the fin face with its outline centred on (0, FIN_CY).

Run:  ~/snrgy-ereader-cad/.venv/bin/python hairband.py
Out:  hairband.step, hairband.stl, assembly_check printout
"""
import math
from pathlib import Path
import numpy as np
import cadquery as cq

import case as C   # magnet positions, outline, pocket geometry come from the case

# ---------------------------------------------------------------- parameters
BAND_H = 9.0                 # band width (Z, front-back on the head)
INNER_HALF_W = 68.0          # inner half-width at the widest point
DEPTH = 150.0                # apex (centreline) to end tips
SUPER_N = 2.3                # superellipse exponent (2 = ellipse, higher = squarer crown)
T_APEX, T_END = 2.8, 2.0     # radial thickness
TAIL_R, TAIL_DEG = 40.0, 25.0  # inward curl at the ends
END_BULB_D = 4.0
TEETH = True
TEETH_N, TEETH_PITCH, TEETH_H, TEETH_BASE = 9, 3.6, 0.7, 1.6

FIN_T = C.MAG_SKIN + C.MAG_DEPTH      # 3.2: pockets open from the bed, skin on the mating face
FIN_INSET = 2.0                       # fin = case outline minus this all round
FIN_FILLET = 6.0                      # fin / band blend radius
CASE_GAP_Y = 0.3                      # case bottom edge above the band's outer surface
LEDGE_H, LEDGE_T, LEDGE_GAP, LEDGE_OPEN = 2.8, 2.0, 0.2, 14.0   # cup round the case's bottom corners
NECK_Z, NECK_HALF = 7.5, 9.0          # band top lowered at the apex so a USB-C plug clears
MIC_GROOVE_W, MIC_GROOVE_D = 2.0, 0.8 # sound path from the case mic port to the fin edge

# ---------------------------------------------------------------- derived
A = INNER_HALF_W + T_END / 2
TAIL_DROP = TAIL_R * math.sin(math.radians(TAIL_DEG))
B = DEPTH - TAIL_DROP                 # centreline apex y
FIN_CY = B + T_APEX / 2 + CASE_GAP_Y + C.OUT_L / 2
EPS = 0.01


def centreline(n_main=500, n_tail=40):
    """Points + thickness along the centreline, from the left tail tip to the right tail tip."""
    pts, th = [], []
    # left tail (reverse), main superellipse, right tail
    def tail(side):
        out = []
        for i in range(n_tail + 1):
            a = math.radians(TAIL_DEG) * i / n_tail
            x = side * (A - TAIL_R * (1 - math.cos(a)))
            y = -TAIL_R * math.sin(a)
            out.append((x, y))
        return out
    lt = tail(-1)[::-1]
    for p in lt[:-1]:
        pts.append(p); th.append(T_END)
    for i in range(n_main + 1):
        t = math.pi - math.pi * i / n_main            # from left (π) to right (0)
        c, s = math.cos(t), math.sin(t)
        x = A * math.copysign(abs(c) ** (2 / SUPER_N), c)
        y = B * abs(s) ** (2 / SUPER_N)
        pts.append((x, y)); th.append(T_END + (T_APEX - T_END) * abs(s) ** 1.5)
    for p in tail(1)[1:]:
        pts.append(p); th.append(T_END)
    return np.array(pts), np.array(th)


def band_outline():
    P, T = centreline()
    d = np.gradient(P, axis=0)
    d /= np.linalg.norm(d, axis=1)[:, None]
    nrm = np.stack([-d[:, 1], d[:, 0]], axis=1)       # left normal; for this direction = outward
    outer = P + nrm * (T / 2)[:, None]
    inner = P - nrm * (T / 2)[:, None]
    return P, T, outer, inner, nrm


def poly_face(pts):
    pts = [cq.Vector(float(x), float(y), 0) for x, y in pts]
    return cq.Face.makeFromWires(cq.Wire.makePolygon(pts, close=True))


def rrect_face(w, l, r, cx, cy):
    return cq.Face.makeFromWires(C.rrect_wire(w, l, r).translate(cq.Vector(cx, cy, 0)))


def extrude(face, z0, z1):
    faces = face.Faces() if not isinstance(face, cq.Face) else [face]
    solids = [cq.Solid.extrudeLinear(f.translate(cq.Vector(0, 0, z0)), cq.Vector(0, 0, z1 - z0)) for f in faces]
    out = solids[0]
    for s in solids[1:]:
        out = out.fuse(s)
    return out


def build_band():
    P, T, outer, inner, nrm = band_outline()
    body = extrude(poly_face(np.vstack([outer, inner[::-1]])), 0, BAND_H)
    for end in (P[0], P[-1]):                          # rounded/bulbed tips
        body = body.fuse(C.cyl(END_BULB_D, 0, BAND_H, float(end[0]), float(end[1])))
    if TEETH:                                          # grip ridges on the inner face near the ends
        L = np.concatenate([[0], np.cumsum(np.linalg.norm(np.diff(P, axis=0), axis=1))])
        for side in (0, 1):
            for k in range(TEETH_N):
                s = 6.0 + k * TEETH_PITCH if side == 0 else L[-1] - 6.0 - k * TEETH_PITCH
                i = int(np.searchsorted(L, s))
                p, n = inner[i], -nrm[i]
                tdir = np.array([-n[1], n[0]])
                tri = [p + tdir * TEETH_BASE / 2 - n * 0.2, p - tdir * TEETH_BASE / 2 - n * 0.2, p + n * TEETH_H]
                body = body.fuse(extrude(poly_face(tri), 0, BAND_H))
    return body, P, T


def case_outline_face(offset=0.0):
    w = C.rrect_wire(C.OUT_W, C.OUT_L, C.OUT_R).translate(cq.Vector(0, FIN_CY, 0))
    if offset:
        w = w.offset2D(offset, "arc")[0]
    return cq.Face.makeFromWires(w)


def build_fin(P, T):
    # fin plate footprint: inset case outline + ledge base, blended into the band apex
    fin = rrect_face(C.OUT_W - 2 * FIN_INSET, C.OUT_L - 2 * FIN_INSET, C.OUT_R - FIN_INSET, 0, FIN_CY)
    cup_out = case_outline_face(LEDGE_GAP + LEDGE_T)
    cup_in = case_outline_face(LEDGE_GAP)
    y_case_bot = FIN_CY - C.OUT_L / 2
    lower = poly_face([(-40, y_case_bot - 10), (40, y_case_bot - 10), (40, y_case_bot + C.OUT_R), (-40, y_case_bot + C.OUT_R)])
    mid = poly_face([(-LEDGE_OPEN / 2, y_case_bot - 10), (LEDGE_OPEN / 2, y_case_bot - 10),
                     (LEDGE_OPEN / 2, y_case_bot + 10), (-LEDGE_OPEN / 2, y_case_bot + 10)])
    cup = cup_out.cut(cup_in).intersect(lower)
    ledge = cup.cut(mid)
    # base under the ledge cup (full width, incl. the open middle) down into the band
    apex_band = poly_face([(-45, B - 6), (45, B - 6), (45, y_case_bot + 2), (-45, y_case_bot + 2)])
    Pb, Tb, outer, inner, _ = band_outline()
    band_face = poly_face(np.vstack([outer, inner[::-1]]))
    apex = band_face.intersect(apex_band)
    bridge = poly_face([(-C.OUT_W / 2 + FIN_INSET, y_case_bot - LEDGE_GAP - LEDGE_T),
                        (C.OUT_W / 2 - FIN_INSET, y_case_bot - LEDGE_GAP - LEDGE_T),
                        (C.OUT_W / 2 - FIN_INSET, FIN_CY), (-C.OUT_W / 2 + FIN_INSET, FIN_CY)])
    slab = extrude(fin, 0, 1)
    for f in (cup, apex, bridge):
        slab = slab.fuse(extrude(f, 0, 1))
    slab = slab.clean()
    bottom = cq.Workplane().add(slab).faces("<Z").val()
    # closing = generous fillets where the fin meets the band
    w = bottom.outerWire().offset2D(FIN_FILLET, "arc")[0].offset2D(-FIN_FILLET, "arc")[0]
    plate_face = cq.Face.makeFromWires(w)
    return plate_face, ledge


def build_hairband():
    band, P, T = build_band()
    plate_face, ledge_face = build_fin(P, T)
    body = band.fuse(extrude(plate_face, 0, FIN_T))
    body = body.fuse(extrude(ledge_face, FIN_T - EPS, FIN_T + LEDGE_H))
    # neck: lower the band top at the apex (45° ramps, print-friendly) so a USB-C plug clears
    neck = (cq.Workplane("XZ").polyline([(-NECK_HALF, NECK_Z), (NECK_HALF, NECK_Z),
                                         (NECK_HALF + (BAND_H - NECK_Z) + 1, BAND_H + 1),
                                         (-NECK_HALF - (BAND_H - NECK_Z) - 1, BAND_H + 1)]).close()
            .extrude(-60).translate((0, B - 20, 0))).val()
    body = body.cut(neck)
    # magnet pockets: open from the bed, skin on the mating (+Z) face; N dimples on the bed face
    for (mx, my) in C.MAG_POS:
        x, y = mx, FIN_CY + my
        body = body.cut(C.magnet_pocket(x, y, skin_at_bottom=False))
        body = body.cut(C.cyl(C.N_DIMPLE_D, -EPS, C.N_DIMPLE_H, x + C.N_DIMPLE_DX, y))
    # sound groove on the mating face: case mic port -> nearest fin edge (-x)
    if C.MIC_POCKET:
        px, py = C.MIC_PORT_XY
        g = C.box(-C.OUT_W / 2 - 1, px + MIC_GROOVE_W / 2, FIN_CY + py - MIC_GROOVE_W / 2, FIN_CY + py + MIC_GROOVE_W / 2,
                  FIN_T - MIC_GROOVE_D, FIN_T + 1)
        body = body.cut(g.fuse(C.cyl(3.0, FIN_T - MIC_GROOVE_D, FIN_T + 1, px, FIN_CY + py)))
    return body


if __name__ == "__main__":
    out = Path(__file__).parent
    hb = build_hairband()
    print("valid", hb.isValid())
    bb = hb.BoundingBox()
    print(f"hairband bbox x {bb.xmin:.1f}..{bb.xmax:.1f}  y {bb.ymin:.1f}..{bb.ymax:.1f}  z {bb.zmin:.1f}..{bb.zmax:.1f}"
          f"  -> {bb.xlen:.1f} x {bb.ylen:.1f} mm on the bed")
    print(f"apex centreline y={B:.1f}, fin centre y={FIN_CY:.2f}, magnets at {[(m[0], round(FIN_CY + m[1], 2)) for m in C.MAG_POS]}")
    cq.exporters.export(cq.Workplane().add(hb), str(out / "hairband.step"))
    cq.exporters.export(cq.Workplane().add(hb), str(out / "hairband.stl"), tolerance=0.03, angularTolerance=0.1)

    # assembly check: case back on the fin face, outline centred on (0, FIN_CY)
    case = C.build_case().translate(cq.Vector(0, FIN_CY, FIN_T))
    v = hb.intersect(case).Volume()
    print(f"assembly interference hairband∩case: {v:.4f} mm³")
    # case magnets vs band magnets (xy)
    for (mx, my) in C.MAG_POS:
        print(f"  magnet pair at x={mx:.2f} y={FIN_CY + my:.2f}: case skin z={FIN_T:.2f}, band skin top z={FIN_T:.2f}, gap {2*C.MAG_SKIN:.1f} mm")
    # ledge contact: distance from case bottom face to ledge inner face
    print(f"  case bottom edge y={FIN_CY - C.OUT_L/2:.2f}; ledge inner face y={FIN_CY - C.OUT_L/2 - LEDGE_GAP:.2f}")
    cq.exporters.export(cq.Workplane().add(case), str(out / "_assembly_case.stl"), tolerance=0.05, angularTolerance=0.2)
