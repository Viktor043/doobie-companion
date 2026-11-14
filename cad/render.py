"""
Preview renders + wall-thickness check for the Doobie case and hairband.
Needs: trimesh, matplotlib, numpy (not CadQuery). Run after case.py and hairband.py:
    python render.py
"""
import sys
from collections import defaultdict
from pathlib import Path
import numpy as np
import trimesh
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

D = Path(__file__).parent
TEAL = (0.35, 0.85, 0.80)      # translucent-PETG-ish
GREY = (0.45, 0.47, 0.50)


def shade(ax, meshes, elev, azim, alpha=1.0, lim=None):
    e, a = np.radians(elev), np.radians(azim)
    cam = np.array([np.cos(e) * np.cos(a), np.cos(e) * np.sin(a), np.sin(e)])
    L = cam * 0.7 + np.array([0.3, -0.5, 0.8]) * 0.5
    L /= np.linalg.norm(L)
    allb = []
    for mesh, color, al in meshes:
        inten = np.clip(mesh.face_normals @ L, 0, 1) * 0.7 + 0.3
        cols = np.array(color)[None, :] * inten[:, None]
        pc = Poly3DCollection(mesh.vertices[mesh.faces], facecolors=np.c_[cols, np.full(len(cols), al)], edgecolor="none")
        ax.add_collection3d(pc)
        allb.append(mesh.bounds)
    b = np.array(allb)
    lo, hi = b[:, 0].min(0), b[:, 1].max(0)
    c = (lo + hi) / 2
    r = (hi - lo).max() / 2 if lim is None else lim
    ax.set_xlim(c[0] - r, c[0] + r); ax.set_ylim(c[1] - r, c[1] + r); ax.set_zlim(c[2] - r, c[2] + r)
    ax.view_init(elev, azim); ax.set_axis_off(); ax.set_box_aspect((1, 1, 1))


def thin_report(m, limit=1.2, label=""):
    pts = m.triangles_center - m.face_normals * 1e-3
    loc, ri, _ = m.ray.intersects_location(pts, -m.face_normals, multiple_hits=False)
    th = np.full(len(pts), np.inf)
    th[ri] = np.linalg.norm(loc - pts[ri], axis=1)
    g = defaultdict(float)
    for k in np.where(th < limit - 0.02)[0]:
        c = m.triangles_center[k]
        g[(round(th[k], 1), int(round(c[0] / 4) * 4), int(round(c[1] / 4) * 4), int(round(c[2])))] += m.area_faces[k]
    print(f"[{label}] faces thinner than {limit} mm:")
    for k, v in sorted(g.items(), key=lambda kv: -kv[1])[:20]:
        if v > 0.5:
            print("   t=%.1f near x~%d y~%d z~%d : %.1f mm²" % (k + (v,)))


case = trimesh.load(D / "case.stl")
dummy = trimesh.load(D / "_board_dummy.stl")
band = trimesh.load(D / "hairband.stl")
asm_case = trimesh.load(D / "_assembly_case.stl")
thin_report(case, label="case")
thin_report(band, label="hairband")

# 1) case views
fig = plt.figure(figsize=(15, 6), facecolor="white")
for i, (el, az, t) in enumerate([(40, -60, "inside (battery right, mic shelf left)"), (-45, -60, "back (bed side): magnets, keychain, mic port"), (20, -100, "bottom: USB-C, BOOT/RST slots")]):
    ax = fig.add_subplot(1, 3, i + 1, projection="3d"); shade(ax, [(case, TEAL, 1.0)], el, az); ax.set_title(t, fontsize=10)
plt.tight_layout(); plt.savefig(D / "render_case.png", dpi=110); plt.close()

# 2) sections with the board dummy
fig, axs = plt.subplots(1, 3, figsize=(17, 5.5))
for ax, (orig, nrm, title, idx) in zip(axs, [([6.46, 0, 0], [1, 0, 0], "x=+6.5 through battery (y vs z)", (1, 2)),
                                           ([-8.16, 0, 0], [1, 0, 0], "x=-8.2 through mic shelf (y vs z)", (1, 2)),
                                           ([0, -19.25, 0], [0, 1, 0], "y=-19.25 posts + standoffs (x vs z)", (0, 2))]):
    for mesh, col in ((case, "k"), (dummy, "tab:blue")):
        sec = mesh.section(plane_origin=orig, plane_normal=nrm)
        if sec is None:
            continue
        for e in sec.discrete:
            ax.plot(e[:, idx[0]], e[:, idx[1]], "-", color=col, lw=0.9)
    ax.set_aspect("equal"); ax.set_title(title, fontsize=10); ax.grid(alpha=0.3)
axs[0].plot([], [], "k-", label="case"); axs[0].plot([], [], "-", color="tab:blue", label="board/battery/mic dummy"); axs[0].legend(fontsize=7, loc="lower right")
plt.tight_layout(); plt.savefig(D / "render_sections.png", dpi=110); plt.close()

# 3) hairband: flat (as printed) + 3D
fig = plt.figure(figsize=(14, 7), facecolor="white")
ax = fig.add_subplot(1, 2, 1)
sec = band.section(plane_origin=[0, 0, 1.0], plane_normal=[0, 0, 1])
for e in sec.discrete: ax.plot(e[:, 0], e[:, 1], "k-", lw=0.8)
sec = band.section(plane_origin=[0, 0, 3.1], plane_normal=[0, 0, 1])
for e in sec.discrete: ax.plot(e[:, 0], e[:, 1], "-", color="tab:green", lw=0.6)
ax.set_aspect("equal"); ax.grid(alpha=0.3); ax.set_title("hairband as printed (top view, mm); green = slice at z 3.1 (magnet pockets)", fontsize=9)
ax = fig.add_subplot(1, 2, 2, projection="3d"); shade(ax, [(band, TEAL, 1.0)], 35, -70); ax.set_title("hairband", fontsize=10)
plt.tight_layout(); plt.savefig(D / "render_hairband.png", dpi=110); plt.close()

# 4) assembly: Doobie on the hairband, close-up + whole
fig = plt.figure(figsize=(14, 7), facecolor="white")
for i, (el, az, lim, t) in enumerate([(25, -55, 40, "assembly close-up (case on fin, ledge cups the bottom corners)"), (15, -80, None, "worn orientation: +Z = forward")]):
    ax = fig.add_subplot(1, 2, i + 1, projection="3d")
    if lim:
        clip = band.slice_plane([0, 120, 0], [0, 1, 0])
        shade(ax, [(clip, GREY, 1.0), (asm_case, TEAL, 1.0)], el, az, lim=lim)
    else:
        shade(ax, [(band, GREY, 1.0), (asm_case, TEAL, 1.0)], el, az)
    ax.set_title(t, fontsize=10)
plt.tight_layout(); plt.savefig(D / "render_assembly.png", dpi=110); plt.close()
print("renders written")
