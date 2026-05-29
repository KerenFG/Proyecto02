#!/usr/bin/env bash
# run_tests.sh — Integration (end-to-end) tests for jqindex.
#
# Runs the compiled binary against real JSON files and verifies:
#   - correct exit codes
#   - .jnx file creation
#   - correct JSON output for known inputs
#   - error messages for bad inputs
#   - stale index detection
#
# Run from the project root:
#   bash tests/run_tests.sh
# Or via Makefile:
#   make test-int

set -euo pipefail

# ── Config ────────────────────────────────────────────────────────────────
BIN="./jqindex"
DATA="tests/data"
PASS=0
FAIL=0
TOTAL=0

# ── Helpers ───────────────────────────────────────────────────────────────

green() { printf '\033[32m%s\033[0m' "$*"; }
red()   { printf '\033[31m%s\033[0m' "$*"; }

check() {
    local desc="$1"
    local expected="$2"
    local actual="$3"
    TOTAL=$((TOTAL + 1))
    if [ "$actual" = "$expected" ]; then
        printf "  %-50s %s\n" "$desc" "$(green PASS)"
        PASS=$((PASS + 1))
    else
        printf "  %-50s %s\n" "$desc" "$(red FAIL)"
        printf "    expected: %s\n" "$expected"
        printf "    actual:   %s\n" "$actual"
        FAIL=$((FAIL + 1))
    fi
}

# Run command and capture exit code without crashing the script
run_exit() { "$@" >/dev/null 2>&1; echo $?; }

# Count occurrences of a string in the last command's output
count_in() { echo "$1" | grep -c "$2" || true; }

# Cleanup generated index files after tests
cleanup() {
    rm -f "$DATA"/*.jnx /tmp/jqindex_stale_test.json /tmp/jqindex_stale_test.jnx
}
trap cleanup EXIT

# ── Check binary exists ────────────────────────────────────────────────────
if [ ! -x "$BIN" ]; then
    echo "ERROR: '$BIN' not found. Run 'make' first."
    exit 1
fi

# ══════════════════════════════════════════════════════════════════════════
echo ""
echo "[ CLI / argument handling ]"
# ══════════════════════════════════════════════════════════════════════════

check "no args returns exit 1" \
    "1" "$(run_exit $BIN)"

check "unknown subcommand returns exit 1" \
    "1" "$(run_exit $BIN frobnicate x.json)"

check "build with no file arg returns exit 1" \
    "1" "$(run_exit $BIN build)"

check "search with missing pattern returns exit 1" \
    "1" "$(run_exit $BIN search $DATA/simple.json)"

check "--help returns exit 0" \
    "0" "$(run_exit $BIN --help)"

check "--version returns exit 0" \
    "0" "$(run_exit $BIN --version)"

# ══════════════════════════════════════════════════════════════════════════
echo ""
echo "[ build subcommand ]"
# ══════════════════════════════════════════════════════════════════════════

# Build simple.json
$BIN build "$DATA/simple.json" 2>/dev/null
check "build simple.json exits 0" \
    "0" "$(run_exit $BIN build $DATA/simple.json)"

check "build creates .jnx file" \
    "1" "$([ -f $DATA/simple.jnx ] && echo 1 || echo 0)"

check "build nonexistent file returns exit 2" \
    "2" "$(run_exit $BIN build /nonexistent/path.json)"

# Build all test files
for f in "$DATA"/nested.json "$DATA"/arrays.json "$DATA"/types.json "$DATA"/escapes.json; do
    $BIN build "$f" 2>/dev/null
    base="${f%.json}"
    check "build $(basename $f) creates index" \
        "1" "$([ -f ${base}.jnx ] && echo 1 || echo 0)"
done

# ══════════════════════════════════════════════════════════════════════════
echo ""
echo "[ search subcommand — simple.json ]"
# ══════════════════════════════════════════════════════════════════════════

# search without an index should fail cleanly
rm -f "$DATA/simple.jnx"
check "search without index returns exit 2" \
    "2" "$(run_exit $BIN search $DATA/simple.json '.*')"

# Rebuild the index
$BIN build "$DATA/simple.json" 2>/dev/null

# Root match
out=$($BIN search "$DATA/simple.json" '^\$$' 2>/dev/null)
check "search root '\$' returns 1 result" \
    "1" "$(count_in "$out" '"path"')"

# Exact field match
out=$($BIN search "$DATA/simple.json" '^\$\.name$' 2>/dev/null)
check "search '^\\.name\$' returns 1 result" \
    "1" "$(count_in "$out" '"path"')"
check "result contains path '$.name'" \
    "1" "$(count_in "$out" '"path": "\$.name"')"
check "result contains value Alice" \
    "1" "$(count_in "$out" 'Alice')"

# Match multiple fields
out=$($BIN search "$DATA/simple.json" '\$\.' 2>/dev/null)
check "search '\\.\\.' matches all fields (>1 result)" \
    "1" "$([ "$(count_in "$out" '"path"')" -gt 1 ] && echo 1 || echo 0)"

# No match → valid empty JSON array
out=$($BIN search "$DATA/simple.json" '^\$\.nonexistent$' 2>/dev/null)
check "no-match returns empty JSON array '[]'" \
    "1" "$(count_in "$out" '\[\]')"
check "no-match returns exit 0" \
    "0" "$(run_exit $BIN search $DATA/simple.json '^\$\.nonexistent$')"

# ══════════════════════════════════════════════════════════════════════════
echo ""
echo "[ search subcommand — nested.json ]"
# ══════════════════════════════════════════════════════════════════════════

out=$($BIN search "$DATA/nested.json" 'address' 2>/dev/null)
check "nested: 'address' matches $.user.address and children" \
    "1" "$([ "$(count_in "$out" '"path"')" -ge 1 ] && echo 1 || echo 0)"

out=$($BIN search "$DATA/nested.json" '^\$\.user\.name$' 2>/dev/null)
check "nested: exact path '$.user.name' returns 1 result" \
    "1" "$(count_in "$out" '"path"')"
check "nested: value is Bob" \
    "1" "$(count_in "$out" 'Bob')"

# ══════════════════════════════════════════════════════════════════════════
echo ""
echo "[ search subcommand — arrays.json ]"
# ══════════════════════════════════════════════════════════════════════════

out=$($BIN search "$DATA/arrays.json" '\[0\]' 2>/dev/null)
check "arrays: '[0]' matches all first-element paths" \
    "1" "$([ "$(count_in "$out" '"path"')" -ge 1 ] && echo 1 || echo 0)"

out=$($BIN search "$DATA/arrays.json" '^\$\.items\[' 2>/dev/null)
check "arrays: '^\\.items\\[' matches items elements" \
    "1" "$([ "$(count_in "$out" '"path"')" -ge 1 ] && echo 1 || echo 0)"

# ══════════════════════════════════════════════════════════════════════════
echo ""
echo "[ output format ]"
# ══════════════════════════════════════════════════════════════════════════

out=$($BIN search "$DATA/simple.json" '^\$\.name$' 2>/dev/null)
check "output starts with '['" \
    "1" "$(echo "$out" | head -1 | grep -c '^\[' || true)"
check "output ends with ']'" \
    "1" "$(echo "$out" | tail -1 | grep -c '^\]' || true)"
check "output contains '\"path\"' key" \
    "1" "$(count_in "$out" '"path"')"
check "output contains '\"value\"' key" \
    "1" "$(count_in "$out" '"value"')"

# Validate with python3 if available
if command -v python3 >/dev/null 2>&1; then
    valid=$(echo "$out" | python3 -m json.tool >/dev/null 2>&1 && echo 1 || echo 0)
    check "output is valid JSON (python3 check)" "1" "$valid"
fi

# ══════════════════════════════════════════════════════════════════════════
echo ""
echo "[ stale index detection ]"
# ══════════════════════════════════════════════════════════════════════════

echo '{"x":1}' > /tmp/jqindex_stale_test.json
$BIN build /tmp/jqindex_stale_test.json 2>/dev/null

# Make the .json newer than the .jnx by touching it 1 second later
sleep 1
touch /tmp/jqindex_stale_test.json

warn=$($BIN search /tmp/jqindex_stale_test.json '.*' 2>&1 >/dev/null)
check "stale index emits warning on stderr" \
    "1" "$(count_in "$warn" 'warning\|outdated\|newer')"

# ══════════════════════════════════════════════════════════════════════════
echo ""
printf "Integration: %s passed, %s failed (total %s)\n" \
    "$(green $PASS)" "$([ $FAIL -gt 0 ] && red $FAIL || echo $FAIL)" "$TOTAL"

[ "$FAIL" -eq 0 ] && exit 0 || exit 1
