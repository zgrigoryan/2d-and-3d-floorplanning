#!/bin/sh
set -e

INPUT="${INPUT:-}"
BENCHMARK="${BENCHMARK:-apte}"
MCNC_DIR="${MCNC_DIR:-mcnc_hard}"
MODE="${MODE:-SA-LP}"
ITERATIONS="${ITERATIONS:-1000}"
OUTPUT="${OUTPUT:-out/${BENCHMARK}_highs}"
HIGHS_BIN="${HIGHS_BIN:-$HOME/opt/highs/bin/highs}"

if [ ! -x "./build/floorplanner" ]; then
    echo "floorplanner executable not found. Build first:"
    echo "  cmake -S . -B build -DFP_WITH_HIGHS=ON -DCMAKE_PREFIX_PATH=\$HOME/opt/highs"
    echo "  cmake --build build"
    exit 1
fi

if [ ! -x "${HIGHS_BIN}" ]; then
    echo "HiGHS executable not found at:"
    echo "  ${HIGHS_BIN}"
    echo "Override with:"
    echo "  HIGHS_BIN=/path/to/highs sh run-highs.sh"
    exit 1
fi

mkdir -p "${OUTPUT}"

echo "Running floorplanner with integrated HiGHS backend"
if [ -n "${INPUT}" ]; then
    ./build/floorplanner \
        --input "${INPUT}" \
        --mode "${MODE}" \
        --solver highs \
        --iterations "${ITERATIONS}" \
        --output "${OUTPUT}" \
        --export-mps "${OUTPUT}/model.mps" \
        --export-lp "${OUTPUT}/model.lp"
else
    ./build/floorplanner \
        --mcnc "${BENCHMARK}" \
        --mcnc-dir "${MCNC_DIR}" \
        --mode "${MODE}" \
        --solver highs \
        --iterations "${ITERATIONS}" \
        --output "${OUTPUT}" \
        --export-mps "${OUTPUT}/model.mps" \
        --export-lp "${OUTPUT}/model.lp"
fi

echo
echo "Verifying exported MPS with HiGHS CLI"
"${HIGHS_BIN}" "${OUTPUT}/model.mps" --solution_file "${OUTPUT}/highs.sol"

echo
echo "Summary:"
echo "  floorplanner summary: ${OUTPUT}/summary.json"
echo "  exported MPS:         ${OUTPUT}/model.mps"
echo "  exported LP:          ${OUTPUT}/model.lp"
echo "  HiGHS solution:       ${OUTPUT}/highs.sol"
