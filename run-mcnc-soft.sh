#!/bin/sh
set -e

MCNC_DIR="${MCNC_DIR:-mcnc_hard}"
OUTPUT_DIR="${OUTPUT_DIR:-mcnc_soft}"
CASES="${CASES:-apte xerox hp ami33 ami49}"
RATIO_PADDING="${RATIO_PADDING:-1.0}"

python3 tools/mcnc_to_soft.py \
    --mcnc-dir "${MCNC_DIR}" \
    --output-dir "${OUTPUT_DIR}" \
    --ratio-padding "${RATIO_PADDING}" \
    --cases ${CASES}

echo
echo "Soft-block MCNC-style benchmarks written under ${OUTPUT_DIR}"
echo "Example run:"
echo "  ./build/floorplanner --mcnc apte --mcnc-dir ${OUTPUT_DIR} --mode SA-CT-LP --solver highs --iterations 1000 --output out/apte_soft"
