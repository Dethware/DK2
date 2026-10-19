#!/usr/bin/env bash
# Run a Ghidra script against the analyzed DKII.EXE project (read-only, no re-analysis).
# Usage: tools/ghidra/run.sh Script.java [args...]
# GHIDRA_PROJ: directory holding the DK2 project (created by analyzeHeadless -import).
# GHIDRA_INSTALL_DIR: the Ghidra installation (default /opt/ghidra); JAVA_HOME: a JDK 21 or newer.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
: "${GHIDRA_PROJ:?set GHIDRA_PROJ to the Ghidra project directory}"
export JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-21-openjdk}"
script="$1"; shift
"${GHIDRA_INSTALL_DIR:-/opt/ghidra}/support/analyzeHeadless" "$GHIDRA_PROJ" DK2 -process DKII.EXE -noanalysis -readOnly \
    -scriptPath "$here" -postScript "$script" "$@" 2>&1 \
  | sed -n '/INFO  SCRIPT:/,$p' | grep -vE '^INFO  (ANALYZING|REPORT|Headless|  )' | sed -E 's/^INFO  [A-Za-z]+\.java> //; s/ \(GhidraScript\)$//'
