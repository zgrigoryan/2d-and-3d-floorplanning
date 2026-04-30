#!/bin/sh
set -e

ROOT="${ROOT:-out}"
OUTPUT_DIR="${OUTPUT_DIR:-results/comparisons}"
PYTHON="${PYTHON:-python3}"

if ! "${PYTHON}" -c "import matplotlib" >/dev/null 2>&1; then
    echo "matplotlib is not installed for ${PYTHON}."
    echo "Install it with:"
    echo "  ${PYTHON} -m pip install matplotlib"
    exit 1
fi

"${PYTHON}" analyze_results.py --root "${ROOT}" --output-dir "${OUTPUT_DIR}"
