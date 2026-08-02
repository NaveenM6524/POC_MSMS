#!/bin/bash
# Builds and runs the CUnit unit-test suite against your real util.c.
#
# Usage:
#   1. Put this tests/ folder as a sibling of your POC/ source folder, e.g.:
#        msms/POC/            <- your existing source
#        msms/tests/          <- this folder (test_util.c, run_cunit_tests.sh)
#   2. From inside msms/tests/:  ./run_cunit_tests.sh

set -e
SRC_DIR="../POC"

if [ ! -f "$SRC_DIR/util.c" ]; then
    echo "Error: expected to find $SRC_DIR/util.c — adjust SRC_DIR at the top"
    echo "of this script if your POC/ folder lives somewhere else."
    exit 1
fi

echo "=== Building test_util ==="
gcc -Wall -Wextra -std=c11 -I"$SRC_DIR" -o test_util test_util.c "$SRC_DIR/util.c" -lcunit

echo ""
echo "=== Running test_util ==="
./test_util
EXIT=$?

echo ""
if [ "$EXIT" -eq 0 ]; then
    echo "All CUnit tests passed."
else
    echo "CUnit reported failures (exit code $EXIT) — see output above."
fi
exit $EXIT
