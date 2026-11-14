"""
Doobie Companion — 3D-printed back case  (rev B)
Board: Waveshare ESP32-S3-Touch-AMOLED-1.64 (28.60 x 43.50 PCB, 29.12 x 44.00 glass)
       4x brass M2 female hex standoffs, ~3.0 mm proud of the PCB back
Battery: EEMB LP401730 (4.0 x 17 x 30, 150 mAh) -> JST 1.25 "BAT" socket near USB-C
Mic: Adafruit PDM MEMS breakout #3492 (14.0 x 12.8 x 2.8, port on the bottom side)
Material: Bambu PETG (translucent) on a Bambu P1S

Coordinates (mm), seen from the FRONT (display side):
  origin = case centre, z = 0 on the BACK face (print bed side), +z toward the display,
  +y toward the TOP of the board (antenna end, USB-C at -y), +x to the right.
  (From the back, BOOT is on the right and RST on the left.)

Back layout (seen from the front):
   ┌───────────────────────────┐   top:   keychain bar (x=+2.5)
   │ o   [key]               o │          posts/standoffs in the 4 corners
   │  M                        │   left:  magnet column x=-7, y=±15 (SOLDR 30 mm pitch)
   │ ┌mic┐   ┌──battery──────┐ │          mic breakout on a shelf between the magnets
   │ │ ◦ │   │ LP401730      │ │   right: battery lengthwise, between the right posts
   │ └───┘   │ (lengthwise)  │ │
   │  M      └───────────────┘ │
   │ o        [USB-C]        o │
   └───────────────────────────┘

Run:  ~/snrgy-ereader-cad/.venv/bin/python case.py
Out:  case.step, case.stl, board_dummy.step
"""
import math
from pathlib import Path
import cadquery as cq

# ---------------------------------------------------------------- parameters
# board (official Waveshare drawing + user photos)
GLASS_W, GLASS_L, GLASS_R, GLASS_T = 29.12, 44.00, 3.70, 4.00
PCB_W, PCB_L, PCB_T, PCB_R = 28.60, 43.50, 1.60, 3.0
HOLE_DX, HOLE_DY = 22.86, 38.50          # standoff centres
STANDOFF_H = 3.0                          # brass hex, proud of PCB back (photo estimate)
STANDOFF_AF = 3.2                         # M2 hex across flats (corners ~3.7)
JST_H = 3.5                               # BAT socket + plug height (photo estimate)
JST_XY = (-5.3, 1.1, -10.85, -7.4)        # BAT socket footprint x0,x1,y0,y1 (photo estimate)
USB_XY = (-4.6, 4.6, -21.9, -13.4)
USB_BODY_H = 3.2
PART_CLEAR = 3.8                          # PCB-back parts above the battery (JST+plug + 0.3)

# case
WALL = 1.60
MIN_WALL = 1.20
GAP = 0.20
GLASS_PROUD = 0.50
FLOOR = 3.20                              # = magnet skin + pocket depth
TOP_CHAMFER = 0.60
BACK_CHAMFER = 0.50
RIM_LEADIN = 0.40

# battery (EEMB LP401730), lengthwise on the right
BAT_X, BAT_Y, BAT_Z = 17.0, 30.0, 4.0
BAT_POCKET = (17.6, 31.0, 4.6)            # 0.3 / 0.5 clearance, 0.6 swelling margin
BAT_FLOOR = 1.2
BAT_CY = 0.0                              # passes between the right-hand standoffs

# screws / posts (posts land on the standoff ends)
POST_D = 6.4                               # locator rim wall 1.2
SCREW_CLEAR_D = 2.3
CBORE_D, CBORE_H = 3.9, 2.3               # ISO 7380 button head M2 (Ø3.5 x 1.3)
LOCATOR_D, LOCATOR_H = 4.0, 0.8           # hex standoff drops into this rim
SCREW_ENGAGE_MAX = 3.0
RIB_W = 2.0

# magnets — SOLDR spec
MAG_POCKET_D, MAG_RIB_D, MAG_RIBS, MAG_RIB_W = 6.5, 6.1, 6, 0.8
MAG_SKIN, MAG_DEPTH, MAG_LEADIN = 0.30, 2.90, 0.65
MAG_PITCH = 30.0
MAG_X = -7.0                              # left column (posts + battery force this)
MAG_POS = ((MAG_X, MAG_PITCH / 2), (MAG_X, -MAG_PITCH / 2))
N_DIMPLE_D, N_DIMPLE_H, N_DIMPLE_DX = 1.2, 0.4, -4.6   # "N pole faces in" marker

# keychain: flush slot, 2 mm bar, channel under it, boss inside
KEY_CX, KEY_CY = 2.5, 20.3
KEY_W, KEY_L = 8.0, 3.0                   # slot footprint (x, y); bar spans KEY_L
KEY_BAR_W, KEY_BAR_T = 3.5, 2.0
KEY_CHANNEL_H = 1.4
KEY_CEIL = 1.5
KEY_OPEN_R = 0.8                          # plan-view fillet = bar root fillet
KEY_ROOT_R = 0.6                          # channel-end fillet = bar root fillet (yz)

# mic breakout (Adafruit #3492) on a shelf over the left strip, port facing the back
MIC_POCKET = True
MIC_W, MIC_L, MIC_T = 12.8, 14.0, 2.8     # board x, y, total height (board + mic can)
MIC_PORT = (0.0, 0.0)                     # port offset from board centre (≈ centre)
MIC_CLEAR = 0.2
MIC_HOLE_D = 1.0
MIC_GASKET_D, MIC_GASKET_H = 3.0, 0.4
MIC_TOP_CLEAR = 1.8                       # mic top to PCB back (parts there are ≤ 0.9)

# cut-outs
USB_W, USB_H, USB_R = 12.0, 6.4, 2.5
BTN_Y, BTN_SLOT_L, BTN_SLOT_H = -13.5, 5.0, 1.8   # BOOT (-x), RST (+x); photo: y -12…-15
LED_HOLES = False                         # translucent PETG: the CHG LED glows through

# ---------------------------------------------------------------- derived
IN_W, IN_L, IN_R = GLASS_W + 2 * GAP, GLASS_L + 2 * GAP, GLASS_R + GAP
OUT_W, OUT_L, OUT_R = IN_W + 2 * WALL, IN_L + 2 * WALL, IN_R + WALL
BPX, BPY, BPZ = BAT_POCKET
BAT_X1 = OUT_W / 2 - MIN_WALL                       # battery pocket into the right wall
BAT_X0 = BAT_X1 - BPX
BAT_CX = (BAT_X0 + BAT_X1) / 2
BAT_TOP = BAT_FLOOR + BPZ
MIC_SHELF_Z = BAT_TOP + 0.2
PCB_BACK_Z = BAT_TOP + PART_CLEAR
if MIC_POCKET:
    PCB_BACK_Z = max(PCB_BACK_Z, MIC_SHELF_Z + MIC_T + MIC_TOP_CLEAR)
PCB_FRONT_Z = PCB_BACK_Z + PCB_T
GLASS_FRONT_Z = PCB_FRONT_Z + GLASS_T
H = GLASS_FRONT_Z - GLASS_PROUD
POST_TOP_Z = PCB_BACK_Z - STANDOFF_H
HOLES = [(sx * HOLE_DX / 2, sy * HOLE_DY / 2) for sx in (-1, 1) for sy in (-1, 1)]
SCREW_GRIP = POST_TOP_Z - CBORE_H
SCREW_LEN = int(SCREW_GRIP + SCREW_ENGAGE_MAX)       # longest standard length that stays ≤ max
MIC_X0 = -IN_W / 2 + MIC_CLEAR
MIC_CX, MIC_CY = MIC_X0 + MIC_W / 2, 0.6
MIC_PORT_XY = (MIC_CX + MIC_PORT[0], MIC_CY + MIC_PORT[1])
EPS = 0.01


def rrect_wire(w, l, r, z=0.0):
    pts = [(-w / 2, -l / 2, z), (w / 2, -l / 2, z), (w / 2, l / 2, z), (-w / 2, l / 2, z)]
    wire = cq.Wire.makePolygon([cq.Vector(*p) for p in pts], close=True)
    return wire.fillet2D(r, wire.Vertices())


def rrect_solid(w, l, r, z0, h, cx=0.0, cy=0.0):
    s = cq.Solid.extrudeLinear(cq.Face.makeFromWires(rrect_wire(w, l, r, z0)), cq.Vector(0, 0, h))
    return s.translate(cq.Vector(cx, cy, 0))


def box(x0, x1, y0, y1, z0, z1):
    return cq.Workplane("XY").box(x1 - x0, y1 - y0, z1 - z0, centered=False).translate((x0, y0, z0)).val()


def cyl(d, z0, z1, x=0.0, y=0.0):
    return cq.Solid.makeCylinder(d / 2, z1 - z0, cq.Vector(x, y, z0))


def magnet_pocket(x, y, skin_at_bottom=True, z_base=0.0):
    """SOLDR pocket: Ø6.5 with crush ribs to Ø6.1, lead-in at the open end.
    skin_at_bottom=True: skin at z_base, opening at z_base+skin+depth (case: opens inward).
    skin_at_bottom=False: opening at z_base (bed), skin on top (hairband fin)."""
    if skin_at_bottom:
        z0, z1, mouth_z, flare_up = z_base + MAG_SKIN, z_base + MAG_SKIN + MAG_DEPTH, z_base + MAG_SKIN + MAG_DEPTH, True
    else:
        z0, z1, mouth_z, flare_up = z_base, z_base + MAG_DEPTH, z_base, False
    p = cyl(MAG_POCKET_D, z0 - (0 if skin_at_bottom else 1), z1 + (EPS if skin_at_bottom else 0), x, y)
    for i in range(MAG_RIBS):
        rib = box(MAG_RIB_D / 2, MAG_POCKET_D / 2 + 0.3, -MAG_RIB_W / 2, MAG_RIB_W / 2, z0 - 2, z1 + 2)
        p = p.cut(rib.rotate(cq.Vector(0, 0, 0), cq.Vector(0, 0, 1), 360.0 / MAG_RIBS * i).translate(cq.Vector(x, y, 0)))
    if flare_up:
        cone = cq.Solid.makeCone(MAG_POCKET_D / 2, MAG_POCKET_D / 2 + MAG_LEADIN, MAG_LEADIN + EPS,
                                 cq.Vector(x, y, mouth_z - MAG_LEADIN))
    else:
        cone = cq.Solid.makeCone(MAG_POCKET_D / 2 + MAG_LEADIN, MAG_POCKET_D / 2, MAG_LEADIN + EPS,
                                 cq.Vector(x, y, mouth_z - EPS))
    return p.fuse(cone)


def build_case():
    body = (cq.Workplane("XY").add(rrect_solid(OUT_W, OUT_L, OUT_R, 0, H))
            .edges(">Z").chamfer(TOP_CHAMFER)
            .edges("<Z").chamfer(BACK_CHAMFER)).val()
    body = body.cut(rrect_solid(IN_W, IN_L, IN_R, FLOOR, H - FLOOR + 1))
    body = body.cut(cq.Solid.makeLoft([rrect_wire(IN_W, IN_L, IN_R, H - RIM_LEADIN),
                                       rrect_wire(IN_W + 2 * RIM_LEADIN, IN_L + 2 * RIM_LEADIN, IN_R + RIM_LEADIN, H + EPS)]))

    # posts up to the standoff ends, ribbed into the walls, hex locator rim on top
    for (x, y) in HOLES:
        sx, sy = math.copysign(1, x), math.copysign(1, y)
        top = POST_TOP_Z + LOCATOR_H
        body = body.fuse(cyl(POST_D, FLOOR - EPS, top, x, y))
        body = body.fuse(box(min(x, sx * (IN_W / 2 + 0.3)), max(x, sx * (IN_W / 2 + 0.3)), y - RIB_W / 2, y + RIB_W / 2, FLOOR - EPS, POST_TOP_Z))
        body = body.fuse(box(x - RIB_W / 2, x + RIB_W / 2, min(y, sy * (IN_L / 2 + 0.3)), max(y, sy * (IN_L / 2 + 0.3)), FLOOR - EPS, POST_TOP_Z))
        body = body.cut(cyl(LOCATOR_D, POST_TOP_Z, top + EPS, x, y))
        body = body.cut(cyl(SCREW_CLEAR_D, -EPS, top + EPS, x, y))
        body = body.cut(cyl(CBORE_D, -EPS, CBORE_H, x, y))

    # keychain: boss inside, then openings (rounded) + channel (rounded ends) from the back
    k_top = KEY_BAR_T + KEY_CHANNEL_H + KEY_CEIL
    ch_l = KEY_L + 2 * KEY_ROOT_R        # channel overshoots the openings so its root fillets sit on solid floor
    body = body.fuse(box(KEY_CX - KEY_W / 2 - 1.2, KEY_CX + KEY_W / 2 + 1.2, KEY_CY - ch_l / 2 - 1.5,
                         IN_L / 2 + 0.3, FLOOR - EPS, max(FLOOR, k_top)))
    op_w = (KEY_W - KEY_BAR_W) / 2
    for s in (-1, 1):
        ox = KEY_CX + s * (KEY_BAR_W / 2 + op_w / 2)
        opening = (cq.Workplane("XY").box(op_w, KEY_L, KEY_BAR_T + 1, centered=(True, True, False))
                   .edges("|Z").fillet(KEY_OPEN_R).translate((ox, KEY_CY, -1))).val()
        body = body.cut(opening)
    channel = (cq.Workplane("XY").box(KEY_W, ch_l, KEY_CHANNEL_H, centered=(True, True, False))
               .edges("|X").fillet(KEY_ROOT_R).translate((KEY_CX, KEY_CY, KEY_BAR_T))).val()
    body = body.cut(channel)

    # battery pocket (into the floor, and into the right wall down to MIN_WALL)
    body = body.cut(rrect_solid(BPX, BPY, 0.5, BAT_FLOOR, H, BAT_CX, BAT_CY))

    # magnets + N dimples on the inside floor
    for (mx, my) in MAG_POS:
        body = body.cut(magnet_pocket(mx, my, skin_at_bottom=True))
        body = body.cut(cyl(N_DIMPLE_D, FLOOR - N_DIMPLE_H, FLOOR + EPS, mx + N_DIMPLE_DX, my))

    # mic: two rails + acoustic chimney carry the breakout over the left strip
    if MIC_POCKET:
        px, py = MIC_PORT_XY
        y0, y1 = MIC_CY - MIC_L / 2, MIC_CY + MIC_L / 2
        rail_x1 = BAT_X0 - 0.4
        for yy in (y0 + 0.6, y1 - 0.6):
            body = body.fuse(box(-IN_W / 2 - 0.3, rail_x1, yy - 0.6, yy + 0.6, FLOOR - EPS, MIC_SHELF_Z))
        # retaining lips at the outer ends of the rails (0.8 above the shelf)
        for yy in (y0 - 1.2, y1):
            body = body.fuse(box(-IN_W / 2 - 0.3, rail_x1, yy, yy + 1.2, FLOOR - EPS, MIC_SHELF_Z + 0.8))
        body = body.fuse(cyl(5.4, FLOOR - EPS, MIC_SHELF_Z, px, py))
        body = body.cut(cyl(MIC_GASKET_D, MIC_SHELF_Z - MIC_GASKET_H, MIC_SHELF_Z + EPS, px, py))
        body = body.cut(cyl(MIC_HOLE_D, -EPS, MIC_SHELF_Z, px, py))
        # small outer countersink so the port reads as a deliberate dot
        body = body.cut(cq.Solid.makeCone(1.0, MIC_HOLE_D / 2, 0.5, cq.Vector(px, py, -EPS)))

    # USB-C
    usb_z = PCB_BACK_Z - 1.6
    usb = rrect_solid(USB_W, USB_H, USB_R, 0, WALL + 3.0).rotate(cq.Vector(0, 0, 0), cq.Vector(1, 0, 0), 90)
    body = body.cut(usb.translate(cq.Vector(0, -IN_L / 2 + 1.0, usb_z)))

    # BOOT / RST pin slots
    btn_z = PCB_BACK_Z - 1.0
    for sx in (-1, 1):
        s = (cq.Workplane("YZ").center(BTN_Y, btn_z).slot2D(BTN_SLOT_L, BTN_SLOT_H, 0)
             .extrude(WALL + 2).translate((sx * (IN_W / 2 - 0.5) + (0 if sx > 0 else -(WALL + 1.5)), 0, 0))).val()
        body = body.cut(s)
    return body


def build_board_dummy():
    glass = rrect_solid(GLASS_W, GLASS_L, GLASS_R, PCB_FRONT_Z, GLASS_T)
    pcb = rrect_solid(PCB_W, PCB_L, PCB_R, PCB_BACK_Z, PCB_T)
    standoffs = [cq.Workplane("XY").polygon(6, STANDOFF_AF / math.cos(math.pi / 6)).extrude(STANDOFF_H)
                 .translate((x, y, POST_TOP_Z)).val().cut(cyl(2.0, POST_TOP_Z - 1, PCB_BACK_Z + 1, x, y)) for (x, y) in HOLES]
    jx0, jx1, jy0, jy1 = JST_XY
    jst = box(jx0, jx1, jy0, jy1, PCB_BACK_Z - JST_H, PCB_BACK_Z)
    ux0, ux1, uy0, uy1 = USB_XY
    usb = box(ux0, ux1, uy0, uy1, PCB_BACK_Z - USB_BODY_H, PCB_BACK_Z)
    flat = box(-9.5, 9.5, -6.0, 16.0, PCB_BACK_Z - 1.0, PCB_BACK_Z)      # QFN, crystal, passives
    bat = box(BAT_CX - BAT_X / 2, BAT_CX + BAT_X / 2, BAT_CY - BAT_Y / 2, BAT_CY + BAT_Y / 2, BAT_FLOOR, BAT_FLOOR + BAT_Z)
    parts = {"glass": glass, "pcb": pcb, "jst": jst, "usb": usb, "flat_parts": flat, "battery": bat}
    for i, s in enumerate(standoffs):
        parts[f"standoff{i}"] = s
    if MIC_POCKET:
        parts["mic"] = box(MIC_CX - MIC_W / 2, MIC_CX + MIC_W / 2, MIC_CY - MIC_L / 2, MIC_CY + MIC_L / 2,
                           MIC_SHELF_Z, MIC_SHELF_Z + MIC_T)
    return parts


def report(case, dummy):
    print(f"outer {OUT_W:.2f} x {OUT_L:.2f} x {H:.2f}  (glass front {GLASS_FRONT_Z:.2f})")
    print(f"PCB back z={PCB_BACK_Z:.2f}  standoff ends / post tops z={POST_TOP_Z:.2f}")
    print(f"battery pocket x {BAT_X0:.2f}..{BAT_X1:.2f}  y ±{BPY/2:.2f}  z {BAT_FLOOR}..{BAT_TOP:.2f}")
    print(f"magnets {MAG_POS}  mic port {tuple(round(v,2) for v in MIC_PORT_XY)}  mic shelf z={MIC_SHELF_Z:.2f}")
    print(f"screw grip {SCREW_GRIP:.2f} -> M2x{SCREW_LEN} button head, engages {SCREW_LEN - SCREW_GRIP:.2f} mm")
    for name, s in dummy.items():
        if name in ("battery", "mic"):
            continue    # these sit in their pockets: check against the case only for overlap
        v = case.intersect(s).Volume()
        print(f"  interference case∩{name}: {v:.4f} mm³")
    for name in ("battery", "mic"):
        if name in dummy:
            print(f"  interference case∩{name}: {case.intersect(dummy[name]).Volume():.4f} mm³")
    # parts vs parts (battery/mic vs PCB-back parts)
    for a in ("battery", "mic"):
        for b in ("jst", "usb", "flat_parts", "standoff0", "standoff1", "standoff2", "standoff3"):
            if a in dummy:
                v = dummy[a].intersect(dummy[b]).Volume()
                if v > 1e-6:
                    print(f"  CLASH {a}∩{b}: {v:.3f}")


if __name__ == "__main__":
    out = Path(__file__).parent
    case = build_case()
    assert case.isValid(), "case solid invalid"
    cq.exporters.export(cq.Workplane().add(case), str(out / "case.step"))
    cq.exporters.export(cq.Workplane().add(case), str(out / "case.stl"), tolerance=0.02, angularTolerance=0.1)
    dummy = build_board_dummy()
    cq.exporters.export(cq.Workplane().add(cq.Compound.makeCompound(list(dummy.values()))), str(out / "board_dummy.step"))
    report(case, dummy)
    print("case volume", round(case.Volume(), 1), "mm³")
