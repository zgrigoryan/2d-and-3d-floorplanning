#!/bin/sh
set -e

INPUT="${INPUT:-}"
BENCHMARK="${BENCHMARK:-apte}"
MCNC_DIR="${MCNC_DIR:-mcnc_hard}"
MODE="${MODE:-SA-LP}"
ITERATIONS="${ITERATIONS:-1000}"
OUTPUT="${OUTPUT:-out/${BENCHMARK}_mosek_check}"
MOSEK_HOME="${MOSEK_HOME:-/Users/zara/Downloads/mosek}"
MOSEK_VERSION="${MOSEK_VERSION:-11.1}"

if [ ! -x "./build/floorplanner" ]; then
    echo "floorplanner executable not found. Build first:"
    echo "  cmake -S . -B build -DFP_WITH_HIGHS=ON -DCMAKE_PREFIX_PATH=\$HOME/opt/highs"
    echo "  cmake --build build"
    exit 1
fi

MOSEK_EXE=$(find "${MOSEK_HOME}/${MOSEK_VERSION}/tools/platform" \
    -maxdepth 4 \
    -name mosek \
    -type f \
    -print 2>/dev/null \
    | head -n 1 || true)

if [ -z "${MOSEK_EXE}" ] || [ ! -x "${MOSEK_EXE}" ]; then
    echo "MOSEK command-line executable not found."
    echo "Looked under:"
    echo "  ${MOSEK_HOME}/${MOSEK_VERSION}/tools/platform"
    echo
    echo "Override with:"
    echo "  MOSEK_HOME=/path/to/mosek MOSEK_VERSION=11.1 sh run-mosek.sh"
    exit 1
fi

mkdir -p "${OUTPUT}"

echo "Running floorplanner to export corrected LP/MPS model"
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
echo "Verifying exported MPS with MOSEK CLI"
"${MOSEK_EXE}" "${OUTPUT}/model.mps"

echo
echo "Summary:"
echo "  floorplanner summary: ${OUTPUT}/summary.json"
echo "  exported MPS:         ${OUTPUT}/model.mps"
echo "  exported LP:          ${OUTPUT}/model.lp"
echo "  MOSEK executable:     ${MOSEK_EXE}"
