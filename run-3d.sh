#!/bin/sh
set -e

INPUT="${INPUT:-}"
BENCHMARK="${BENCHMARK:-apte}"
MCNC_DIR="${MCNC_DIR:-mcnc_hard}"
MODE="${MODE:-SA-3D-CT-LP}"
LAYERS="${LAYERS:-3}"
ITERATIONS="${ITERATIONS:-1000}"
EPOCH_LENGTH="${EPOCH_LENGTH:-100}"
MAX_NO_IMPROVE_EPOCHS="${MAX_NO_IMPROVE_EPOCHS:-1000000}"
INITIAL_TEMPERATURE="${INITIAL_TEMPERATURE:-100}"
COOLING_RATIO="${COOLING_RATIO:-0.95}"
SEED="${SEED:-1}"
AUTO_TEMPERATURE="${AUTO_TEMPERATURE:-0}"
AUTO_EPOCH_LENGTH="${AUTO_EPOCH_LENGTH:-0}"
VERBOSE_SA="${VERBOSE_SA:-0}"
OBJECTIVE_MODE="${OBJECTIVE_MODE:-fixed-outline}"
TSV_WEIGHT="${TSV_WEIGHT:-100}"
THERMAL_WEIGHT="${THERMAL_WEIGHT:-0.00001}"
TSV_KEEPOUT_WEIGHT="${TSV_KEEPOUT_WEIGHT:-10}"
THERMAL_TSV_BENEFIT_WEIGHT="${THERMAL_TSV_BENEFIT_WEIGHT:-0}"
TSV_DIAMETER="${TSV_DIAMETER:-1}"
TSV_KEEPOUT_RADIUS="${TSV_KEEPOUT_RADIUS:-0}"
BUILD_DIR="${BUILD_DIR:-build}"
PYTHON="${PYTHON:-python3}"
EXPORT_MODEL="${EXPORT_MODEL:-1}"

case "${MODE}" in
    *LP*) SOLVER="${SOLVER:-highs}" ;;
    *) SOLVER="${SOLVER:-none}" ;;
esac

OUTPUT="${OUTPUT:-out/${BENCHMARK}_3d_${SOLVER}}"

SA_EXTRA_ARGS=""
if [ "${AUTO_TEMPERATURE}" = "1" ]; then
    SA_EXTRA_ARGS="${SA_EXTRA_ARGS} --auto-temperature"
fi
if [ "${AUTO_EPOCH_LENGTH}" = "1" ]; then
    SA_EXTRA_ARGS="${SA_EXTRA_ARGS} --auto-epoch-length"
fi
if [ "${VERBOSE_SA}" = "1" ]; then
    SA_EXTRA_ARGS="${SA_EXTRA_ARGS} --verbose-sa"
fi

if [ ! -x "${BUILD_DIR}/floorplanner" ]; then
    echo "floorplanner executable not found. Build first:"
    echo "  cmake --build ${BUILD_DIR}"
    echo "Or set BUILD_DIR=/path/to/build."
    exit 1
fi

mkdir -p "${OUTPUT}"

EXPORT_ARGS=""
if [ "${EXPORT_MODEL}" = "1" ]; then
    EXPORT_ARGS="--export-mps ${OUTPUT}/model.mps --export-lp ${OUTPUT}/model.lp"
fi

echo "Running 3D sequence-triple floorplanner"
echo "  benchmark=${BENCHMARK}"
echo "  mode=${MODE}"
echo "  solver=${SOLVER}"
echo "  layers=${LAYERS}"
echo "  output=${OUTPUT}"

set +e
if [ -n "${INPUT}" ]; then
    "${BUILD_DIR}/floorplanner" \
        --input "${INPUT}" \
        --mode "${MODE}" \
        --solver "${SOLVER}" \
        --layers "${LAYERS}" \
        --iterations "${ITERATIONS}" \
        --epoch-length "${EPOCH_LENGTH}" \
        --max-no-improve-epochs "${MAX_NO_IMPROVE_EPOCHS}" \
        --initial-temperature "${INITIAL_TEMPERATURE}" \
        --cooling-ratio "${COOLING_RATIO}" \
        --seed "${SEED}" \
        --objective-mode "${OBJECTIVE_MODE}" \
        --tsv-weight "${TSV_WEIGHT}" \
        --thermal-weight "${THERMAL_WEIGHT}" \
        --tsv-keepout-weight "${TSV_KEEPOUT_WEIGHT}" \
        --thermal-tsv-benefit-weight "${THERMAL_TSV_BENEFIT_WEIGHT}" \
        --tsv-diameter "${TSV_DIAMETER}" \
        --tsv-keepout-radius "${TSV_KEEPOUT_RADIUS}" \
        ${SA_EXTRA_ARGS} \
        ${EXPORT_ARGS} \
        --output "${OUTPUT}"
else
    "${BUILD_DIR}/floorplanner" \
        --mcnc "${BENCHMARK}" \
        --mcnc-dir "${MCNC_DIR}" \
        --mode "${MODE}" \
        --solver "${SOLVER}" \
        --layers "${LAYERS}" \
        --iterations "${ITERATIONS}" \
        --epoch-length "${EPOCH_LENGTH}" \
        --max-no-improve-epochs "${MAX_NO_IMPROVE_EPOCHS}" \
        --initial-temperature "${INITIAL_TEMPERATURE}" \
        --cooling-ratio "${COOLING_RATIO}" \
        --seed "${SEED}" \
        --objective-mode "${OBJECTIVE_MODE}" \
        --tsv-weight "${TSV_WEIGHT}" \
        --thermal-weight "${THERMAL_WEIGHT}" \
        --tsv-keepout-weight "${TSV_KEEPOUT_WEIGHT}" \
        --thermal-tsv-benefit-weight "${THERMAL_TSV_BENEFIT_WEIGHT}" \
        --tsv-diameter "${TSV_DIAMETER}" \
        --tsv-keepout-radius "${TSV_KEEPOUT_RADIUS}" \
        ${SA_EXTRA_ARGS} \
        ${EXPORT_ARGS} \
        --output "${OUTPUT}"
fi
floorplanner_status=$?
set -e

if [ "${floorplanner_status}" -ne 0 ]; then
    echo "floorplanner exited with status ${floorplanner_status}; continuing to plot any generated placement."
fi

if [ -f "visualize_floorplan.py" ] && "${PYTHON}" -c "import matplotlib" >/dev/null 2>&1; then
    "${PYTHON}" visualize_floorplan.py "${OUTPUT}" --output "${OUTPUT}/floorplan.png"
else
    echo "Skipping plot: visualize_floorplan.py or matplotlib is unavailable."
fi

if [ -f "visualize_floorplan_3d.py" ] && "${PYTHON}" -c "import mpl_toolkits.mplot3d" >/dev/null 2>&1; then
    "${PYTHON}" visualize_floorplan_3d.py "${OUTPUT}" --output "${OUTPUT}/floorplan_3d.png"
else
    echo "Skipping 3D plot: visualize_floorplan_3d.py or mpl_toolkits.mplot3d is unavailable."
fi

echo
echo "Summary:"
echo "  summary:        ${OUTPUT}/summary.json"
echo "  placements:     ${OUTPUT}/placements.csv"
echo "  floorplan:      ${OUTPUT}/floorplan.png"
echo "  3D floorplan:   ${OUTPUT}/floorplan_3d.png"
echo "  exported MPS:   ${OUTPUT}/model.mps"
echo "  exported LP:    ${OUTPUT}/model.lp"
