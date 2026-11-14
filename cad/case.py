"""
Doobie Companion — 3D-printed back case
Board: Waveshare ESP32-S3-Touch-AMOLED-1.64 (28.60 x 43.50 PCB, 29.12 x 44.00 glass)

Coordinate system (mm):
  origin = centre of the case, z = 0 on the BACK face (print bed side)
  +z toward the display, +y toward the TOP of the board (antenna end), USB-C at -y

Run:  ~/snrgy-ereader-cad/.venv/bin/python case.py
Out:  case.step, case.stl  (+ board_dummy.step for checks)
"""
import math
from pathlib import Path
import cadquery as cq

# ---------------------------------------------------------------- parameters
# board (official Waveshare drawing)
GLASS_W, GLASS_L, GLASS_R, GLASS_T = 29.12, 44.00, 3.70, 4.00   # glass/display module
PCB_W, PCB_L, PCB_T = 28.60, 43.50, 1.60                        # 4.00 + 1.60 = 5.60 (drawing)
PCB_R = 3.0                # PCB corner radius (estimated from drawing)
HOLE_DX, HOLE_DY = 22.86, 38.50                                  # M2 threaded holes, centred
COMP_CLEAR = 3.20          # space behind PCB for back-side parts (USB-C, JST, buttons, SD)
HEADER_CLEARANCE = 0.0     # extra depth if pin headers are soldered (they'd need ~4 mm)

# case
WALL = 1.60
GAP = 0.20                 # clearance around glass module
GLASS_PROUD = 0.50         # glass stands proud of rim
FLOOR = 3.20               # back plate thickness (= magnet skin + pocket depth)
TOP_CHAMFER = 0.80
BACK_CHAMFER = 0.50
RIM_LEADIN = 0.40          # inner chamfer on the rim, helps the glass drop in

# battery (LiPo 402025 laid 25 mm across X)
BAT_W, BAT_L, BAT_T = 25.0, 20.0, 4.5   # x, y, z (4.0 cell + swelling/tape)
BAT_CLEAR = 0.5
BAT_FLOOR = 1.2
BAT_CY = -1.5                            # centred between the two magnets

# screws / posts
POST_D = 5.6
SCREW_CLEAR_D = 2.3
CBORE_D, CBORE_H = 4.2, 2.3   # M2x8 tip lands 0.2 mm short of the display module
POST_RECESS_D, POST_RECESS_H = 4.4, 0.6  # solder-fillet relief around the threaded pads
RIB_W = 2.0                              # ties posts into the walls (no thin slivers)

# magnets — SOLDR spec (measured from SOLDR module print files)
MAG_D = 6.0                 # Ø6 x 3 N52
MAG_POCKET_D = 6.5
MAG_RIB_D = 6.1             # crush ribs bite the magnet
MAG_RIBS = 6
MAG_RIB_W = 0.8
MAG_SKIN = 0.30             # closed on the OUTSIDE, magnets pressed in from inside
MAG_DEPTH = 2.90
MAG_LEADIN = 0.65
MAG_PITCH = 30.0            # SOLDR grid pitch (30 / 18) -> mates with SOLDR plates
MAG_Y = (BAT_CY + MAG_PITCH / 2, BAT_CY - MAG_PITCH / 2)   # +13.5, -16.5 on x = 0

# keychain: flush slot in the back with an under-bar channel
KEY_W, KEY_L = 10.0, 4.0     # slot footprint (x, y)
KEY_CY = 20.0
KEY_BAR_W, KEY_BAR_T = 3.0, 1.2
KEY_CHANNEL_TOP = 2.6        # channel ceiling (bridge printed over 4 mm)
KEY_BOSS_H = 1.0             # local floor thickening above the channel

# cut-outs
USB_W, USB_H, USB_R = 12.0, 6.4, 2.5
BTN_Y = -15.3                # RST (left) / BOOT (right), side-actuated — estimated from drawing
BTN_SLOT_L, BTN_SLOT_H = 3.6, 1.8
LED_HOLES = True
LED_POS = ((-6.7, -18.6), (6.7, -18.6))   # PWR / CHG on PCB back — estimated
LED_D = 1.4

# ---------------------------------------------------------------- derived
IN_W, IN_L, IN_R = GLASS_W + 2 * GAP, GLASS_L + 2 * GAP, GLASS_R + GAP
OUT_W, OUT_L, OUT_R = IN_W + 2 * WALL, IN_L + 2 * WALL, IN_R + WALL
BAT_TOP = BAT_FLOOR + BAT_T
PCB_BACK_Z = max(FLOOR, BAT_TOP) + COMP_CLEAR + HEADER_CLEARANCE
PCB_FRONT_Z = PCB_BACK_Z + PCB_T
GLASS_FRONT_Z = PCB_FRONT_Z + GLASS_T
H = GLASS_FRONT_Z - GLASS_PROUD
HOLES = [(sx * HOLE_DX / 2, sy * HOLE_DY / 2) for sx in (-1, 1) for sy in (-1, 1)]
EPS = 0.01


def rrect_wire(w, l, r, z=0.0):
    pts = [(-w / 2, -l / 2, z), (w / 2, -l / 2, z), (w / 2, l / 2, z), (-w / 2, l / 2, z)]
    wire = cq.Wire.makePolygon([cq.Vector(*p) for p in pts], close=True)
    return wire.fillet2D(r, wire.Vertices())


def rrect_solid(w, l, r, z0, h):
    return cq.Solid.extrudeLinear(cq.Face.makeFromWires(rrect_wire(w, l, r, z0)), cq.Vector(0, 0, h))


def box(x0, x1, y0, y1, z0, z1):
    return cq.Workplane("XY").box(x1 - x0, y1 - y0, z1 - z0, centered=False).translate((x0, y0, z0)).val()


def cyl(d, z0, z1, x=0.0, y=0.0):
    return cq.Solid.makeCylinder(d / 2, z1 - z0, cq.Vector(x, y, z0))


def build_case():
    # outer shell with crisp chamfers
    body = (cq.Workplane("XY").add(rrect_solid(OUT_W, OUT_L, OUT_R, 0, H))
            .edges(">Z").chamfer(TOP_CHAMFER)
            .edges("<Z").chamfer(BACK_CHAMFER)).val()

    # main cavity from the floor up, plus rim lead-in
    body = body.cut(rrect_solid(IN_W, IN_L, IN_R, FLOOR, H - FLOOR + 1))
    leadin = cq.Solid.makeLoft([rrect_wire(IN_W, IN_L, IN_R, H - RIM_LEADIN),
                                rrect_wire(IN_W + 2 * RIM_LEADIN, IN_L + 2 * RIM_LEADIN, IN_R + RIM_LEADIN, H + EPS)])
    body = body.cut(leadin)

    # posts + ribs into the side and end walls
    for (x, y) in HOLES:
        sx, sy = math.copysign(1, x), math.copysign(1, y)
        body = body.fuse(cyl(POST_D, FLOOR - EPS, PCB_BACK_Z, x, y))
        body = body.fuse(box(min(x, sx * (IN_W / 2 + 0.3)), max(x, sx * (IN_W / 2 + 0.3)),
                             y - RIB_W / 2, y + RIB_W / 2, FLOOR - EPS, PCB_BACK_Z))
        body = body.fuse(box(x - RIB_W / 2, x + RIB_W / 2,
                             min(y, sy * (IN_L / 2 + 0.3)), max(y, sy * (IN_L / 2 + 0.3)), FLOOR - EPS, PCB_BACK_Z))
        body = body.cut(cyl(POST_RECESS_D, PCB_BACK_Z - POST_RECESS_H, PCB_BACK_Z + EPS, x, y))
        body = body.cut(cyl(SCREW_CLEAR_D, -EPS, PCB_BACK_Z + EPS, x, y))
        body = body.cut(cyl(CBORE_D, -EPS, CBORE_H, x, y))

    # keychain boss (inside) then the slot/channel/bar from the back
    body = body.fuse(box(-KEY_W / 2 - 1.2, KEY_W / 2 + 1.2, KEY_CY - KEY_L / 2 - 1.2,
                         min(KEY_CY + KEY_L / 2 + 1.2, IN_L / 2 + 0.3), FLOOR - EPS, FLOOR + KEY_BOSS_H))
    slot = box(-KEY_W / 2, KEY_W / 2, KEY_CY - KEY_L / 2, KEY_CY + KEY_L / 2, -EPS, KEY_CHANNEL_TOP)
    bar = box(-KEY_BAR_W / 2, KEY_BAR_W / 2, KEY_CY - KEY_L / 2 - 1, KEY_CY + KEY_L / 2 + 1, -1, KEY_BAR_T)
    body = body.cut(slot.cut(bar))

    # battery pocket
    body = body.cut(cq.Workplane("XY").add(rrect_solid(BAT_W + 2 * BAT_CLEAR, BAT_L + 2 * BAT_CLEAR, 1.0,
                                                       BAT_FLOOR, H)).translate((0, BAT_CY, 0)).val())

    # magnet pockets: skin outside, ribs, lead-in at the mouth (z = FLOOR)
    for my in MAG_Y:
        pocket = cyl(MAG_POCKET_D, MAG_SKIN, FLOOR + EPS, 0, my)
        for i in range(MAG_RIBS):
            a = 360.0 / MAG_RIBS * i
            rib = box(MAG_RIB_D / 2, MAG_POCKET_D / 2 + 0.3, -MAG_RIB_W / 2, MAG_RIB_W / 2, MAG_SKIN - 1, FLOOR + 1)
            pocket = pocket.cut(rib.rotate(cq.Vector(0, 0, 0), cq.Vector(0, 0, 1), a).translate(cq.Vector(0, my, 0)))
        cone = cq.Solid.makeCone(MAG_POCKET_D / 2, MAG_POCKET_D / 2 + MAG_LEADIN, MAG_LEADIN + EPS,
                                 cq.Vector(0, my, FLOOR - MAG_LEADIN))
        body = body.cut(pocket).cut(cone)

    # USB-C through the bottom wall, centred on the receptacle
    usb_z = PCB_BACK_Z - 1.6
    usb = rrect_solid(USB_W, USB_H, USB_R, 0, WALL + 3.0)            # along +z ...
    usb = usb.rotate(cq.Vector(0, 0, 0), cq.Vector(1, 0, 0), 90)       # ... now along -y, height in z
    body = body.cut(usb.translate(cq.Vector(0, -IN_L / 2 + 1.0, usb_z)))

    # RST / BOOT pin-access slots in the side walls
    btn_z = PCB_BACK_Z - 1.0
    for sx in (-1, 1):
        s = (cq.Workplane("YZ").center(BTN_Y, btn_z).slot2D(BTN_SLOT_L, BTN_SLOT_H, 0)
             .extrude(WALL + 2).translate((sx * (IN_W / 2 - 0.5) + (0 if sx > 0 else -(WALL + 1.5)), 0, 0))).val()
        body = body.cut(s)

    if LED_HOLES:
        for (x, y) in LED_POS:
            body = body.cut(cyl(LED_D, -EPS, FLOOR + EPS, x, y))

    return body


def build_board_dummy():
    glass = cq.Workplane("XY").add(rrect_solid(GLASS_W, GLASS_L, GLASS_R, PCB_FRONT_Z, GLASS_T)).val()
    pcb = rrect_solid(PCB_W, PCB_L, PCB_R, PCB_BACK_Z, PCB_T)
    for (x, y) in HOLES:
        pcb = pcb.cut(cyl(2.0, PCB_BACK_Z - 1, PCB_FRONT_Z + 1, x, y))
    # back-side parts kept between the posts (headers unsoldered)
    comps = box(-9.5, 9.5, -16.0, 16.0, PCB_BACK_Z - COMP_CLEAR + 0.2, PCB_BACK_Z)
    usb = box(-4.5, 4.5, -PCB_L / 2 - 0.6, -PCB_L / 2 + 7.0, PCB_BACK_Z - 3.2, PCB_BACK_Z)
    bat = box(-BAT_W / 2, BAT_W / 2, BAT_CY - BAT_L / 2, BAT_CY + BAT_L / 2, BAT_FLOOR, BAT_FLOOR + BAT_T)
    return {"glass": glass, "pcb": pcb, "comps": comps, "usb": usb, "battery": bat}


if __name__ == "__main__":
    out = Path(__file__).parent
    case = build_case()
    assert case.isValid(), "case solid invalid"
    cq.exporters.export(cq.Workplane().add(case), str(out / "case.step"))
    cq.exporters.export(cq.Workplane().add(case), str(out / "case.stl"), tolerance=0.02, angularTolerance=0.1)
    dummy = build_board_dummy()
    cq.exporters.export(cq.Workplane().add(cq.Compound.makeCompound(list(dummy.values()))), str(out / "board_dummy.step"))
    # interference check
    print(f"outer {OUT_W:.2f} x {OUT_L:.2f} x {H:.2f} (glass front at {GLASS_FRONT_Z:.2f})")
    print(f"PCB back z={PCB_BACK_Z:.2f}  battery pocket top z={BAT_TOP:.2f}  magnets at y={MAG_Y}")
    for name, s in dummy.items():
        v = case.intersect(s).Volume()
        print(f"interference case∩{name}: {v:.4f} mm³")
    print("case volume", round(case.Volume(), 1), "mm³")
