#!/usr/bin/env python3
import argparse
import csv
import json
import os
import tempfile
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", str(Path(tempfile.gettempdir()) / "floorplanner-matplotlib"))

import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle


def read_placements(path):
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))
    placements = []
    for row in rows:
        placements.append(
            {
                "name": row["block_name"],
                "x": float(row["x"]),
                "y": float(row["y"]),
                "width": float(row["width"]),
                "height": float(row["height"]),
                "type": row.get("type", ""),
            }
        )
    return placements


def read_summary(path):
    with open(path) as f:
        return json.load(f)


def draw_floorplan(result_dir, output=None, show=False):
    result_dir = Path(result_dir)
    placements_path = result_dir / "placements.csv"
    summary_path = result_dir / "summary.json"

    if not placements_path.exists():
        raise FileNotFoundError(f"missing placements file: {placements_path}")
    if not summary_path.exists():
        raise FileNotFoundError(f"missing summary file: {summary_path}")

    placements = read_placements(placements_path)
    summary = read_summary(summary_path)

    if not placements:
        output = Path(output) if output is not None else result_dir / "floorplan.png"
        output.parent.mkdir(parents=True, exist_ok=True)
        fig, ax = plt.subplots(figsize=(8, 5))
        ax.axis("off")
        message = (
            f"{result_dir.name}\n\n"
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
        return

    chip_w = float(summary.get("chipWidth", 0.0) or 0.0)
    chip_h = float(summary.get("chipHeight", 0.0) or 0.0)
    if chip_w <= 0.0:
        chip_w = max((p["x"] + p["width"] for p in placements), default=1.0)
    if chip_h <= 0.0:
        chip_h = max((p["y"] + p["height"] for p in placements), default=1.0)

    fig_w = max(7.0, min(14.0, 8.0 * chip_w / max(chip_h, 1e-9)))
    fig_h = max(6.0, min(12.0, 8.0 * chip_h / max(chip_w, 1e-9)))
    fig, ax = plt.subplots(figsize=(fig_w, fig_h))

    ax.add_patch(
        Rectangle(
            (0, 0),
            chip_w,
            chip_h,
            fill=False,
            linewidth=2.0,
            edgecolor="black",
        )
    )

    cmap = plt.get_cmap("tab20")
    for idx, block in enumerate(placements):
        color = cmap(idx % 20)
        rect = Rectangle(
            (block["x"], block["y"]),
            block["width"],
            block["height"],
            facecolor=color,
            edgecolor="black",
            linewidth=0.8,
            alpha=0.78,
        )
        ax.add_patch(rect)

        cx = block["x"] + 0.5 * block["width"]
        cy = block["y"] + 0.5 * block["height"]
        label = block["name"]
        area = max(block["width"] * block["height"], 1.0)
        font_size = max(5.5, min(9.0, 5.0 + 0.000002 * area))
        ax.text(
            cx,
            cy,
            label,
            ha="center",
            va="center",
            fontsize=font_size,
            color="black",
            clip_on=True,
        )

    title = (
        f"{result_dir.name} | "
        f"obj={summary.get('objective')} | "
        f"W={summary.get('chipWidth')} H={summary.get('chipHeight')} | "
        f"WL={summary.get('totalWirelength')} | "
        f"{summary.get('status')}"
    )
    ax.set_title(title, fontsize=10)
    ax.set_xlabel("x")
    ax.set_ylabel("y")
    ax.set_xlim(-0.02 * chip_w, chip_w * 1.02)
    ax.set_ylim(-0.02 * chip_h, chip_h * 1.02)
    ax.set_aspect("equal", adjustable="box")
    ax.grid(True, linewidth=0.3, alpha=0.35)

    metrics_text = "\n".join(
        [
            f"mode: {summary.get('mode')}",
            f"solver: {summary.get('solver')}",
            f"feasible: {summary.get('feasible')}",
            f"area: {summary.get('chipArea')}",
            f"blocks: {summary.get('numBlocks')}",
            f"nets: {summary.get('numNets')}",
        ]
    )
    ax.text(
        1.01,
        0.98,
        metrics_text,
        transform=ax.transAxes,
        ha="left",
        va="top",
        fontsize=9,
        bbox={"boxstyle": "round,pad=0.35", "facecolor": "white", "alpha": 0.9},
    )

    fig.tight_layout()

    if output is None:
        output = result_dir / "floorplan.png"
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, dpi=180)
    print(f"wrote {output}")

    if show:
        plt.show()
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description="Visualize a floorplanner output directory.")
    parser.add_argument("result_dir", help="Directory containing placements.csv and summary.json")
    parser.add_argument("--output", "-o", help="Output PNG path. Default: result_dir/floorplan.png")
    parser.add_argument("--show", action="store_true", help="Show an interactive matplotlib window")
    args = parser.parse_args()
    draw_floorplan(args.result_dir, args.output, args.show)


if __name__ == "__main__":
    main()
