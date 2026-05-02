#!/bin/sh
set -e

BENCHMARKS="${BENCHMARKS:-apte xerox hp ami33 ami49}"
SOLVERS="${SOLVERS:-highs mosek}"
MODE="${MODE:-SA-3D-CT-LP}"
LAYERS="${LAYERS:-3}"
ITERATIONS="${ITERATIONS:-1000}"
EPOCH_LENGTH="${EPOCH_LENGTH:-100}"
INITIAL_TEMPERATURE="${INITIAL_TEMPERATURE:-100}"
COOLING_RATIO="${COOLING_RATIO:-0.95}"
SEED="${SEED:-1}"
ROOT="${ROOT:-out/mcnc_${MODE}_${LAYERS}L}"
BUILD_DIR="${BUILD_DIR:-build}"
MCNC_DIR="${MCNC_DIR:-mcnc_hard}"

for benchmark in ${BENCHMARKS}; do
    for solver in ${SOLVERS}; do
        echo
        echo "=== ${benchmark}: ${solver} (${MODE}, ${LAYERS} layers, ${ITERATIONS} iterations) ==="
        BENCHMARK="${benchmark}" \
        MCNC_DIR="${MCNC_DIR}" \
        MODE="${MODE}" \
        SOLVER="${solver}" \
        LAYERS="${LAYERS}" \
        ITERATIONS="${ITERATIONS}" \
        EPOCH_LENGTH="${EPOCH_LENGTH}" \
        INITIAL_TEMPERATURE="${INITIAL_TEMPERATURE}" \
        COOLING_RATIO="${COOLING_RATIO}" \
        SEED="${SEED}" \
        BUILD_DIR="${BUILD_DIR}" \
        OUTPUT="${ROOT}/${benchmark}_${solver}" \
        sh run-3d.sh || true
    done
done

echo
echo "All requested 3D MCNC runs completed under ${ROOT}"
