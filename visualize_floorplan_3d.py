#!/usr/bin/env python3
import argparse
import csv
import json
import os
import tempfile
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", str(Path(tempfile.gettempdir()) / "floorplanner-matplotlib"))

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

try:
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection
except ImportError as exc:
    raise SystemExit("mpl_toolkits.mplot3d is required for 3D plots") from exc


def read_placements(path):
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))
    modules = []
    for idx, row in enumerate(rows):
        modules.append(
            {
                "id": idx,
                "name": row["block_name"],
                "x": float(row["x"]),
                "y": float(row["y"]),
                "w": float(row["width"]),
                "h": float(row["height"]),
                "z": int(float(row.get("layer", 0) or 0)),
            }
        )
    return modules


def read_summary(path):
    with open(path) as f:
        return json.load(f)


def make_palette(n):
    cmap = matplotlib.colormaps["tab20"]
    return [cmap(i % 20) for i in range(max(1, n))]


def footprint(summary, modules):
    fw = float(summary.get("chipWidth", 0.0) or 0.0)
    fh = float(summary.get("chipHeight", 0.0) or 0.0)
    if fw <= 0.0:
        fw = max((m["x"] + m["w"] for m in modules), default=1.0)
    if fh <= 0.0:
        fh = max((m["y"] + m["h"] for m in modules), default=1.0)
    return max(fw, 1.0), max(fh, 1.0)


def cuboid_faces(x0, y0, z0, x1, y1, z1):
    return [
        [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)],
        [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)],
        [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)],
        [(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)],
        [(x0, y0, z0), (x0, y1, z0), (x0, y1, z1), (x0, y0, z1)],
        [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)],
    ]


def draw_3d_stack(result_dir, output=None):
    result_dir = Path(result_dir)
    modules = read_placements(result_dir / "placements.csv")
    summary = read_summary(result_dir / "summary.json")
    if not modules:
        print(f"skipping 3D plot for {result_dir}: no placement available")
        return

    fw, fh = footprint(summary, modules)
    num_layers = max(int(summary.get("numLayers", 1) or 1), max((m["z"] for m in modules), default=0) + 1)
    if num_layers <= 1:
        print(f"skipping 3D plot for {result_dir}: only one layer")
        return

    palette = make_palette(len(modules))
    thick = max(fw, fh) * 0.08
    gap = thick * 0.5

    fig = plt.figure(figsize=(12, 8))
    ax = fig.add_subplot(111, projection="3d")
    for mod in modules:
        x0 = mod["x"]
        y0 = mod["y"]
        x1 = x0 + mod["w"]
        y1 = y0 + mod["h"]
        z0 = mod["z"] * (thick + gap)
        z1 = z0 + thick
        color = palette[mod["id"]]
        alphas = [0.25, 0.85, 0.55, 0.55, 0.55, 0.55]
        for face, alpha in zip(cuboid_faces(x0, y0, z0, x1, y1, z1), alphas):
            poly = Poly3DCollection([face], alpha=alpha, facecolor=color, edgecolor="white", linewidth=0.3)
            ax.add_collection3d(poly)

        cx = (x0 + x1) / 2
        cy = (y0 + y1) / 2
        font_size = min(6, max(4, int(mod["w"] * mod["h"] / max(fw * fh, 1.0) * 150)))
        ax.text(cx, cy, z1 + thick * 0.05, mod["name"], ha="center", va="bottom", fontsize=font_size, color="black", fontweight="bold")

    for z in range(num_layers):
        z_base = z * (thick + gap)
        floor = [(0, 0, z_base), (fw, 0, z_base), (fw, fh, z_base), (0, fh, z_base)]
        ax.add_collection3d(Poly3DCollection([floor], alpha=0.04, facecolor="gray", edgecolor="#aaaaaa", linewidth=0.8))
        ax.text(fw * 1.02, 0, z_base + thick / 2, f"L{z}", fontsize=9, color="#444444", va="center")

    z_max = num_layers * (thick + gap)
    ax.set_xlim(0, fw)
    ax.set_ylim(0, fh)
    ax.set_zlim(0, z_max)
    ax.set_xlabel("X", labelpad=6)
    ax.set_ylabel("Y", labelpad=6)
    ax.set_zlabel("Layer", labelpad=6)
    ax.set_zticks([z * (thick + gap) + thick / 2 for z in range(num_layers)])
    ax.set_zticklabels([f"L{z}" for z in range(num_layers)])
    ax.view_init(elev=25, azim=-55)
    ax.set_box_aspect([fw, fh, z_max * 1.5])
    fig.suptitle(
        f"{result_dir.name}  |  {summary.get('mode')}  |  {num_layers}-layer 3D view\n"
        f"Footprint {fw:.0f}x{fh:.0f}   TSVs={summary.get('totalTsvCount', 0)}   Obj={summary.get('objective')}",
        fontsize=12,
        fontweight="bold",
        y=0.97,
    )
    fig.subplots_adjust(left=0.05, right=0.95, top=0.88, bottom=0.05)

    if output is None:
        output = result_dir / "floorplan_3d.png"
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"wrote {output}")


def main():
    parser = argparse.ArgumentParser(description="Visualize a 3D floorplanner output directory.")
    parser.add_argument("result_dir", help="Directory containing placements.csv and summary.json")
    parser.add_argument("--output", "-o", help="Output PNG path. Default: result_dir/floorplan_3d.png")
    args = parser.parse_args()
    draw_3d_stack(args.result_dir, args.output)


if __name__ == "__main__":
    main()
