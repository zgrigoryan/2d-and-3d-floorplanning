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
import matplotlib.patches as mpatches
from matplotlib.patches import FancyBboxPatch


def read_placements(path):
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))
    placements = []
    for idx, row in enumerate(rows):
        placements.append(
            {
                "id": idx,
                "name": row["block_name"],
                "x": float(row["x"]),
                "y": float(row["y"]),
                "w": float(row["width"]),
                "h": float(row["height"]),
                "type": row.get("type", ""),
                "z": int(float(row.get("layer", 0) or 0)),
            }
        )
    return placements


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


def no_placement_figure(result_dir, summary, output, show):
    output = Path(output) if output is not None else Path(result_dir) / "floorplan.png"
    output.parent.mkdir(parents=True, exist_ok=True)
    fig, ax = plt.subplots(figsize=(8, 5))
    ax.axis("off")
    message = (
        f"{Path(result_dir).name}\n\n"
        "No placement rectangles to draw.\n"
        f"feasible: {summary.get('feasible')}\n"
        f"status: {summary.get('status')}\n"
        f"mode: {summary.get('mode')}\n"
        f"solver: {summary.get('solver')}\n"
        f"blocks: {summary.get('numBlocks')}\n"
        f"nets: {summary.get('numNets')}"
    )
    ax.text(0.5, 0.5, message, ha="center", va="center", fontsize=13)
    fig.tight_layout()
    fig.savefig(output, dpi=180)
    print(f"wrote {output} (no placement available)")
    if show:
        plt.show()
    plt.close(fig)


def draw_2d_layer_view(result_dir, modules, summary, output, show):
    fw, fh = footprint(summary, modules)
    num_layers = max(int(summary.get("numLayers", 1) or 1), max((m["z"] for m in modules), default=0) + 1)
    layers = {z: [m for m in modules if m["z"] == z] for z in range(num_layers)}
    palette = make_palette(len(modules))

    fig_w = max(6.0, 5.0 * num_layers)
    fig, axes = plt.subplots(1, num_layers, figsize=(fig_w, 6.0), squeeze=False)
    title = (
        f"{Path(result_dir).name}  |  {summary.get('mode')}  |  {num_layers}-layer\n"
        f"Footprint {fw:.0f}x{fh:.0f}   TSVs={summary.get('totalTsvCount', 0)}   "
        f"WL={summary.get('totalWirelength')}   Obj={summary.get('objective')}"
    )
    fig.suptitle(title, fontsize=12, fontweight="bold")

    for z in range(num_layers):
        ax = axes[0][z]
        mods = layers[z]
        ax.add_patch(
            mpatches.Rectangle(
                (0, 0),
                fw,
                fh,
                linewidth=1.5,
                edgecolor="#aaaaaa",
                facecolor="#f8f8f8",
                zorder=0,
            )
        )
        for mod in mods:
            color = palette[mod["id"]]
            ax.add_patch(
                FancyBboxPatch(
                    (mod["x"] + 1, mod["y"] + 1),
                    max(mod["w"] - 2, 1),
                    max(mod["h"] - 2, 1),
                    boxstyle="round,pad=0",
                    linewidth=0.8,
                    edgecolor="white",
                    facecolor=color,
                    alpha=0.85,
                    zorder=1,
                )
            )
            font_size = min(7, max(4, int(mod["w"] * mod["h"] / max(fw * fh, 1.0) * 200)))
            ax.text(
                mod["x"] + mod["w"] / 2,
                mod["y"] + mod["h"] / 2,
                mod["name"],
                ha="center",
                va="center",
                fontsize=font_size,
                color="black",
                fontweight="bold",
                zorder=2,
                clip_on=True,
            )

        ax.set_xlim(0, fw)
        ax.set_ylim(0, fh)
        ax.set_aspect("equal")
        ax.set_title(f"Layer {z}  ({len(mods)} modules)", fontsize=10)
        ax.set_xlabel("x")
        ax.set_ylabel("y")
        ax.tick_params(labelsize=7)
        ax.grid(True, linestyle="--", linewidth=0.4, alpha=0.5)

    fig.tight_layout()
    output = Path(output) if output is not None else Path(result_dir) / "floorplan.png"
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, dpi=150, bbox_inches="tight")
    print(f"wrote {output}")
    if show:
        plt.show()
    plt.close(fig)


def draw_floorplan(result_dir, output=None, show=False):
    result_dir = Path(result_dir)
    placements_path = result_dir / "placements.csv"
    summary_path = result_dir / "summary.json"

    if not placements_path.exists():
        raise FileNotFoundError(f"missing placements file: {placements_path}")
    if not summary_path.exists():
        raise FileNotFoundError(f"missing summary file: {summary_path}")

    modules = read_placements(placements_path)
    summary = read_summary(summary_path)

    if not modules:
        no_placement_figure(result_dir, summary, output, show)
        return

    output_path = Path(output) if output is not None else result_dir / "floorplan.png"
    draw_2d_layer_view(result_dir, modules, summary, output_path, show)


def main():
    parser = argparse.ArgumentParser(description="Visualize a floorplanner output directory.")
    parser.add_argument("result_dir", help="Directory containing placements.csv and summary.json")
    parser.add_argument("--output", "-o", help="Output PNG path. Default: result_dir/floorplan.png")
    parser.add_argument("--show", action="store_true", help="Show an interactive matplotlib window")
    args = parser.parse_args()
    draw_floorplan(args.result_dir, args.output, args.show)


if __name__ == "__main__":
    main()
