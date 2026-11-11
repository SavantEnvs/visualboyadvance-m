#!/usr/bin/env bash
#
# mayhem/test.sh — RUN this repo's OWN functional test suite (already built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

# Runs the programs mayhem/build.sh produced (NORMAL flags, no sanitizers) and checks their *output*:
#   video_capture_test   upstream core test: 6 PASS lines + "all passed"
#   pc_semantics_test    upstream core test: 28 "-> PASS" cases, "0 failing cases", ACE equivalence line
#   kat_probe            known-answer probes (mayhem/kat_probe.cc): exact expected values, incl. the
#                        CRC32 of frame 40 of the bundled suite.gba and the result of a hand-assembled
#                        ARM / GB program run through the emulated CPUs.
# A runner that is absent, exits early or prints nothing (e.g. a patch that makes main() return 0) fails.
BIN="$SRC/mayhem-build/test/bin"
passed=0; failed=0

check() {  # check <description> <command...>: counts one pass/fail
  local d="$1"; shift
  if "$@"; then passed=$((passed + 1)); else failed=$((failed + 1)); echo "FAILED: $d" >&2; fi
}
count_eq() { [ "$(grep -c -E -e "$2" "$1" || true)" -eq "$3" ]; }

run() {  # run <name>: output (stdout only) to $OUT/<name>.out
  local n="$1"
  if [ ! -x "$BIN/$n" ]; then echo "missing test runner $BIN/$n (mayhem/build.sh must build it)" >&2; return 1; fi
  timeout 600 "$BIN/$n" > "$OUT/$n.out" 2> "$OUT/$n.err"
  echo "$n: exit=$?"
}

OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT

for n in video_capture_test pc_semantics_test kat_probe; do
  run "$n" || { failed=$((failed + 1)); }
done

V="$OUT/video_capture_test.out"
check "video_capture_test: 6 PASS rows"        count_eq "$V" '[[:space:]]PASS$' 6
check "video_capture_test: no FAIL rows"       count_eq "$V" 'FAIL' 0
check "video_capture_test: all passed"         grep -qx 'video_capture_test: all passed' "$V"

P="$OUT/pc_semantics_test.out"
check "pc_semantics_test: 28 -> PASS cases"    count_eq "$P" '-> PASS ' 28
check "pc_semantics_test: no -> FAIL cases"    count_eq "$P" '-> FAIL' 0
check "pc_semantics_test: 0 failing cases"     grep -qx 'pc_semantics_test: 0 failing cases' "$P"
check "pc_semantics_test: ACE identical"       grep -q '^ACE equivalence: IDENTICAL' "$P"

K="$OUT/kat_probe.out"
check "kat: ARM fib(10) in work RAM"           grep -qx 'gba_arm_fib10: workram\[0\]=55 PASS' "$K"
check "kat: GB ld/add store"                   grep -qx 'gb_ld_add_store: wram\[c000\]=0x46 PASS' "$K"
check "kat: IPS patch size"                    grep -qx 'ips_patch_apply: size=256 PASS' "$K"
check "kat: suite.gba frame-40 CRC32"          grep -qx 'gba_suite_rom_frame40_crc: crc32=cbceba03 PASS' "$K"
check "kat: 0 failing"                         grep -qx 'kat_probe: 0 failing' "$K"

emit_ctrf "vbam-core-tests" "$passed" "$failed" 0
