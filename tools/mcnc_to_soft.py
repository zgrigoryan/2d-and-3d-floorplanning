#!/usr/bin/env python3
import argparse
import shutil
from pathlib import Path


def parse_block_file(path):
    header = []
    blocks = []
    terminals = []

    with open(path) as f:
        for raw in f:
            line = raw.strip()
            if not line:
                continue
            parts = line.split()
            if parts[0] in {"Outline:", "NumBlocks:", "NumTerminals:"}:
                header.append(line)
            elif len(parts) >= 4 and parts[1] == "terminal":
                terminals.append(line)
            elif len(parts) >= 3:
                blocks.append((parts[0], float(parts[1]), float(parts[2])))

    return header, blocks, terminals


def soft_line(name, width, height, ratio_padding):
    area = width * height
    ratio = height / width
    inverse = width / height
    r_min = min(ratio, inverse) / ratio_padding
    r_max = max(ratio, inverse) * ratio_padding
    return f"{name}\tsoft\t{area:.12g}\t{r_min:.12g}\t{r_max:.12g}\t# source {width:.12g} {height:.12g}"


def convert_case(case, mcnc_dir, output_dir, ratio_padding):
    block_path = mcnc_dir / f"{case}.block"
    nets_path = mcnc_dir / f"{case}.nets"
    out_block_path = output_dir / f"{case}.block"
    out_nets_path = output_dir / f"{case}.nets"

    header, blocks, terminals = parse_block_file(block_path)
    output_dir.mkdir(parents=True, exist_ok=True)

    with open(out_block_path, "w") as out:
        out.write("# Soft-block version generated from MCNC hard-block benchmark.\n")
        out.write("# Soft block syntax: name soft area minAspectRatio maxAspectRatio\n")
        out.write("# Aspect ratio is height / width. Bounds include original ratio and inverse.\n")
        out.write(f"# ratioPadding: {ratio_padding}\n")
        for line in header:
            out.write(line + "\n")
        out.write("\n")
        for name, width, height in blocks:
            out.write(soft_line(name, width, height, ratio_padding) + "\n")
        out.write("\n")
        for line in terminals:
            out.write(line + "\n")

    shutil.copyfile(nets_path, out_nets_path)
    print(f"wrote {out_block_path}")
    print(f"wrote {out_nets_path}")


def main():
    parser = argparse.ArgumentParser(
        description="Convert bundled MCNC hard-block .block/.nets files to soft-block .block/.nets files."
    )
    parser.add_argument("--mcnc-dir", default="mcnc_hard", help="Directory containing source .block/.nets files")
    parser.add_argument("--output-dir", default="mcnc_soft", help="Directory for generated soft .block/.nets files")
    parser.add_argument(
        "--cases",
        nargs="*",
        default=["apte", "xerox", "hp", "ami33", "ami49"],
        help="Benchmark base names to convert",
    )
    parser.add_argument(
        "--ratio-padding",
        type=float,
        default=1.0,
        help="Expand min/max aspect ratio bounds by this factor. 1.0 uses original ratio and inverse.",
    )
    args = parser.parse_args()

    if args.ratio_padding < 1.0:
        raise ValueError("--ratio-padding must be >= 1.0")

    mcnc_dir = Path(args.mcnc_dir)
    output_dir = Path(args.output_dir)
    for case in args.cases:
        convert_case(case, mcnc_dir, output_dir, args.ratio_padding)


if __name__ == "__main__":
    main()
