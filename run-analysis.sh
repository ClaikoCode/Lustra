#!/usr/bin/env bash
# Runs CodeChecker (clangsa + clang-tidy) over the compilation database,
# reports findings new since the last run, exports the HTML report,
# and refreshes the baseline.
#
#   ./run-analysis.sh
#
# Requires CodeChecker on PATH, plus compile_commands.json and skip.txt.
#
# Written by Claude (Anthropic), then adapted for this project.

set -euo pipefail

# ---- paths ------------------------------------------------------------------
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CDB="${PROJECT_ROOT}/out/build/x64-debug/compile_commands.json" # compilation database
SKIP="${PROJECT_ROOT}/codechecker-skip.txt"                     # include/exclude globs
OUTDIR="${PROJECT_ROOT}/StaticAnalysisResults"
REPORTS="${OUTDIR}/reports"           # current run
BASELINE="${OUTDIR}/reports_baseline" # previous run
HTML="${OUTDIR}/reports_html"         # HTML export target
JOBS="$(nproc)"

# ---- verify inputs exist ----------------------------------------------------
if [[ ! -f "$CDB" ]]; then
	echo "error: no compile_commands.json at $CDB" >&2
	echo "       configure CMake first, or edit CDB above." >&2
	exit 1
fi
if [[ ! -f "$SKIP" ]]; then
	echo "error: no skip.txt at $SKIP" >&2
	exit 1
fi

# ---- analyze ----------------------------------------------------------------
# clangsa + clang-tidy, cross-TU on, sensitive profile, tidy reads .clang-tidy,
# vendor TUs skipped via skip.txt, report set rebuilt from scratch.
echo ">> analyzing  (jobs=$JOBS, CTU on, profile=sensitive)"
CodeChecker analyze "$CDB" \
	--analyzers clangsa clang-tidy \
	--ctu \
	--enable sensitive \
	--analyzer-config clang-tidy:take-config-from-directory=true \
	--ignore "$SKIP" \
	--jobs "$JOBS" \
	--clean \
	--output "$REPORTS"

# ---- report findings new since the baseline ---------------------------------
# diff/parse exit nonzero when findings exist, so guard with set +e.
if [[ -d "$BASELINE" ]]; then
	echo ">> new findings since last run:"
	set +e
	CodeChecker diff --basename "$BASELINE" --newname "$REPORTS" --new \
		--ignore "$SKIP"
	set -e
else
	echo ">> no baseline yet — showing all findings from this run:"
	set +e
	CodeChecker parse "$REPORTS" --ignore "$SKIP"
	set -e
fi

# ---- export HTML report -----------------------------------------------------
echo ">> writing HTML report"
set +e
CodeChecker parse "$REPORTS" --ignore "$SKIP" --export html --output "$HTML"
set -e
echo "   open: firefox $HTML/index.html"

# ---- update baseline --------------------------------------------------------
rm -rf "$BASELINE"
cp -r "$REPORTS" "$BASELINE"
echo ">> done. this run is now the baseline for next time."
