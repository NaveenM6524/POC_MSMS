#!/bin/bash
# MSMS automated test harness
# Usage: ./run_all_tests.sh   (run from inside the msms/ source directory)
#
# Builds the project, runs static analysis, then runs every test case
# below against the real compiled binary, capturing real output into
# TEST_REPORT.md. Nothing in this script is simulated - every result
# comes from actually executing ./msms.

set -u
REPORT="TEST_REPORT.md"
PASS=0
FAIL=0

echo "# MSMS Test Report — $(date '+%d-%m-%Y %H:%M:%S')" > "$REPORT"
echo "" >> "$REPORT"

section() {
    echo "" >> "$REPORT"
    echo "## $1" >> "$REPORT"
    echo "" >> "$REPORT"
    echo ""
    echo "=== $1 ==="
}

record() {
    # record <name> <PASS|FAIL> <detail>
    local name="$1" result="$2" detail="$3"
    if [ "$result" = "PASS" ]; then
        PASS=$((PASS+1))
        echo "- ✅ **$name** — $detail" >> "$REPORT"
        echo "  PASS: $name — $detail"
    else
        FAIL=$((FAIL+1))
        echo "- ❌ **$name** — $detail" >> "$REPORT"
        echo "  FAIL: $name — $detail"
    fi
}

fresh_data() {
    rm -rf data
}

# ---------------------------------------------------------------------
section "0. Build"
# ---------------------------------------------------------------------
if gcc -Wall -Wextra -std=c11 -g -o msms *.c 2> /tmp/build_err.txt; then
    WARN_COUNT=$(wc -l < /tmp/build_err.txt)
    if [ "$WARN_COUNT" -eq 0 ]; then
        record "Compile with -Wall -Wextra -std=c11" "PASS" "zero warnings"
    else
        record "Compile with -Wall -Wextra -std=c11" "FAIL" "$WARN_COUNT warning(s) — see build_err.txt"
        cat /tmp/build_err.txt >> "$REPORT"
    fi
else
    record "Compile with -Wall -Wextra -std=c11" "FAIL" "build failed, see build_err.txt"
    cat /tmp/build_err.txt >> "$REPORT"
    echo "BUILD FAILED - aborting further tests."
    exit 1
fi

# ---------------------------------------------------------------------
section "1. Static analysis (cppcheck)"
# ---------------------------------------------------------------------
if command -v cppcheck >/dev/null 2>&1; then
    CPPOUT=$(cppcheck --enable=all --inconclusive --std=c11 --suppress=missingIncludeSystem . 2>&1)
    ERR_COUNT=$(echo "$CPPOUT" | grep -c "error:")
    if [ "$ERR_COUNT" -eq 0 ]; then
        record "cppcheck" "PASS" "0 errors ($(echo "$CPPOUT" | grep -c "style:") style-level notes, non-blocking)"
    else
        record "cppcheck" "FAIL" "$ERR_COUNT error(s) found"
    fi
    echo '```' >> "$REPORT"
    echo "$CPPOUT" >> "$REPORT"
    echo '```' >> "$REPORT"
else
    echo "  cppcheck not installed - skipping (install with: sudo dnf install cppcheck)"
fi

# ---------------------------------------------------------------------
section "2. Functional — login, add inventory, unpadded date normalization"
# ---------------------------------------------------------------------
fresh_data
OUT=$(printf "admin\nadmin123\n7\nParacetamol\nB100\n50\n2-5-2026\n20\n0\n0\n" | ./msms 2>&1)
if echo "$OUT" | grep -q "Added as record id 1" && grep -q "^1|Paracetamol|B100|50|02-05-2026|20$" data/inventory.dat 2>/dev/null; then
    record "Unpadded date normalization" "PASS" "2-5-2026 stored as 02-05-2026"
else
    record "Unpadded date normalization" "FAIL" "record not created or date not normalized as expected"
fi

# ---------------------------------------------------------------------
section "3. Input validation — invalid date, negative qty, empty field"
# ---------------------------------------------------------------------
OUT=$(printf "admin\nadmin123\n7\nBadDate\nB1\n10\n31-2-2026\n5\n0\n0\n" | ./msms 2>&1)
if echo "$OUT" | grep -q "Invalid expiry date"; then
    record "Reject invalid calendar date (31-2-2026)" "PASS" "rejected with clear message"
else
    record "Reject invalid calendar date (31-2-2026)" "FAIL" "was not rejected"
fi

OUT=$(printf "admin\nadmin123\n7\nNegQty\nB1\n-5\n1-1-2027\n5\n0\n0\n" | ./msms 2>&1)
if echo "$OUT" | grep -q "Invalid input - nothing was changed"; then
    record "Reject negative quantity" "PASS" "rejected with clear message"
else
    record "Reject negative quantity" "FAIL" "was not rejected"
fi

# ---------------------------------------------------------------------
section "4. Delimiter injection — '|' in name/batch/supplier/username"
# ---------------------------------------------------------------------
OUT=$(printf "admin\nadmin123\n7\nDrug|Pipe\nB1\n10\n1-1-2027\n5\n0\n0\n" | ./msms 2>&1)
if echo "$OUT" | grep -q "Invalid input - nothing was changed"; then
    record "Reject '|' in medicine name" "PASS" "rejected before it could corrupt inventory.dat"
else
    record "Reject '|' in medicine name" "FAIL" "was not rejected — check data/inventory.dat for corruption"
fi

# ---------------------------------------------------------------------
section "5. Duplicate batch handling — Add vs Record Supply"
# ---------------------------------------------------------------------
OUT=$(printf "admin\nadmin123\n7\nParacetamol\nB100\n10\n2-5-2026\n20\n0\n0\n" | ./msms 2>&1)
if echo "$OUT" | grep -q "already exists"; then
    record "Reject exact duplicate via Add Inventory" "PASS" "OP_DUPLICATE returned correctly"
else
    record "Reject exact duplicate via Add Inventory" "FAIL" "duplicate was not rejected"
fi

OUT=$(printf "admin\nadmin123\n10\nParacetamol\nB100\n15\n2-5-2026\n20\nAcme\n0\n0\n" | ./msms 2>&1)
QTY=$(grep "^1|Paracetamol|B100" data/inventory.dat 2>/dev/null | cut -d'|' -f4)
if [ "$QTY" = "65" ]; then
    record "Merge into exact batch via Record Supply" "PASS" "qty 50 -> 65 (merged, not duplicated)"
else
    record "Merge into exact batch via Record Supply" "FAIL" "expected qty 65, got '$QTY'"
fi

# ---------------------------------------------------------------------
section "6. FEFO — drains earliest-expiring batch first, spans batches"
# ---------------------------------------------------------------------
printf "admin\nadmin123\n7\nParacetamol\nB050\n20\n1-1-2026\n5\n0\n0\n" | ./msms > /dev/null 2>&1
OUT=$(printf "admin\nadmin123\n6\nParacetamol\n75\n0\n0\n" | ./msms 2>&1)
EARLY_QTY=$(grep "B050" data/inventory.dat | cut -d'|' -f4)
if echo "$OUT" | grep -q "FULFILLED (75 of 75" && [ "$EARLY_QTY" = "0" ]; then
    record "FEFO drains earliest batch first, spans batches" "PASS" "earlier batch (B050) drained to 0 first, request fully filled across 2 batches"
else
    record "FEFO drains earliest batch first, spans batches" "FAIL" "unexpected fulfillment or batch order (earlyQty=$EARLY_QTY)"
fi

# ---------------------------------------------------------------------
section "7. Over-request — partial fulfillment reports exact quantity"
# ---------------------------------------------------------------------
OUT=$(printf "admin\nadmin123\n6\nParacetamol\n1000\n0\n0\n" | ./msms 2>&1)
if echo "$OUT" | grep -qE "PARTIAL \([0-9]+ of 1000"; then
    record "Over-request yields PARTIAL with exact quantity" "PASS" "$(echo "$OUT" | grep -oE 'Request #[0-9]+: PARTIAL \([0-9]+ of [0-9]+ fulfilled\)')"
else
    record "Over-request yields PARTIAL with exact quantity" "FAIL" "did not report PARTIAL as expected"
fi

# ---------------------------------------------------------------------
section "8. Corrupted data row on load — skipped, not a crash"
# ---------------------------------------------------------------------
echo "GARBAGE_NOT_A_VALID_ROW" >> data/inventory.dat
echo "5|MissingFields|OnlyThree" >> data/inventory.dat
OUT=$(printf "admin\nadmin123\n1\n0\n0\n" | ./msms 2>&1)
EXIT=$?
if [ "$EXIT" -eq 0 ] && grep -q "LOAD_WARNING" data/audit.log; then
    record "Corrupted rows skipped without crash" "PASS" "exit code 0, corrupt rows logged and skipped"
else
    record "Corrupted rows skipped without crash" "FAIL" "exit code $EXIT or no LOAD_WARNING logged"
fi
sed -i '/GARBAGE_NOT_A_VALID_ROW/d;/5|MissingFields|OnlyThree/d' data/inventory.dat

# ---------------------------------------------------------------------
section "9. Non-admin gating — staff blocked from admin actions"
# ---------------------------------------------------------------------
printf "admin\nadmin123\n11\nteststaff\nstaffpass\n2\n0\n0\n" | ./msms > /dev/null 2>&1
OUT=$(printf "teststaff\nstaffpass\n7\n0\n0\n" | ./msms 2>&1)
if echo "$OUT" | grep -q "Admins only"; then
    record "Staff blocked from admin-only menu action" "PASS" "correctly refused"
else
    record "Staff blocked from admin-only menu action" "FAIL" "was not blocked"
fi

# ---------------------------------------------------------------------
section "10. Leap-year-aware date validation"
# ---------------------------------------------------------------------
OUT=$(printf "admin\nadmin123\n7\nLeapOK\nL1\n5\n29-2-2028\n2\n0\n0\n" | ./msms 2>&1)
LEAP_OK=$(echo "$OUT" | grep -c "Added as record")
OUT=$(printf "admin\nadmin123\n7\nLeapBad\nL2\n5\n29-2-2026\n2\n0\n0\n" | ./msms 2>&1)
LEAP_BAD=$(echo "$OUT" | grep -c "Invalid expiry date")
if [ "$LEAP_OK" -eq 1 ] && [ "$LEAP_BAD" -eq 1 ]; then
    record "Leap-year-aware validation (2028 valid, 2026 invalid)" "PASS" "both cases correct"
else
    record "Leap-year-aware validation (2028 valid, 2026 invalid)" "FAIL" "leap_ok=$LEAP_OK leap_bad=$LEAP_BAD"
fi

# ---------------------------------------------------------------------
section "11. Account lockout after repeated failed logins"
# ---------------------------------------------------------------------
OUT=$(printf "teststaff\nwrong\n1\nteststaff\nwrong\n1\nteststaff\nwrong\n0\n0\n" | ./msms 2>&1)
if echo "$OUT" | grep -q "Account is locked"; then
    record "Account locks after 3 failed attempts" "PASS" "locked as expected"
else
    record "Account locks after 3 failed attempts" "FAIL" "did not lock"
fi
OUT=$(printf "teststaff\nstaffpass\n0\n0\n" | ./msms 2>&1)
if echo "$OUT" | grep -q "Account is locked"; then
    record "Locked account stays locked even with correct password" "PASS" "still refused"
else
    record "Locked account stays locked even with correct password" "FAIL" "was allowed in"
fi

# ---------------------------------------------------------------------
section "12. Oversized input — buffer safety + no infinite loop on EOF"
# ---------------------------------------------------------------------
python3 -c "print('A'*2000)" > /tmp/msms_longname.txt 2>/dev/null || perl -e "print 'A' x 2000" > /tmp/msms_longname.txt
(echo "admin"; echo "admin123"; echo "7"; cat /tmp/msms_longname.txt; echo "B1"; echo "10"; echo "1-1-2027"; echo "5"; echo "0"; echo "0") | timeout 10 ./msms > /tmp/msms_long_out.txt 2>&1
EXIT=$?
if [ "$EXIT" -eq 0 ]; then
    record "Oversized input does not crash or hang" "PASS" "exit code 0 within 10s timeout"
elif [ "$EXIT" -eq 124 ]; then
    record "Oversized input does not crash or hang" "FAIL" "TIMED OUT — infinite loop on EOF"
else
    record "Oversized input does not crash or hang" "FAIL" "exit code $EXIT"
fi

# ---------------------------------------------------------------------
section "13. Scale test — 5000 records, full report accuracy"
# ---------------------------------------------------------------------
fresh_data
python3 -c "
names = ['Paracetamol','Amoxicillin','Ibuprofen','Insulin','Aspirin']
lines = [f'{i}|{names[i%5]}|B{i:05d}|{i%500}|15-06-2027|10' for i in range(1,5001)]
open('data/inventory.dat','w').write(chr(10).join(lines)+chr(10))
" 2>/dev/null
if [ -f data/inventory.dat ]; then
    START=$(date +%s%N)
    OUT=$(printf "admin\nadmin123\n1\n0\n0\n" | ./msms 2>&1)
    END=$(date +%s%N)
    MS=$(( (END-START)/1000000 ))
    COUNT=$(echo "$OUT" | grep -cE "^[0-9]+ +[A-Za-z]")
    if [ "$COUNT" -eq 5000 ]; then
        record "5000-record load + full stock report" "PASS" "all 5000 shown, took ${MS}ms"
    else
        record "5000-record load + full stock report" "FAIL" "expected 5000, got $COUNT"
    fi
else
    echo "  python3 not available - skipping scale test (install python3 or write inventory.dat rows manually)"
fi

# ---------------------------------------------------------------------
section "14. Memory safety — valgrind across a full session"
# ---------------------------------------------------------------------
if command -v valgrind >/dev/null 2>&1; then
    fresh_data
    printf "admin\nadmin123\n7\nParacetamol\nB1\n50\n1-1-2027\n10\n10\nParacetamol\nB1\n10\n1-1-2027\n10\nAcme\n6\nParacetamol\n20\n1\n2\n3\n7\n4\n5\n12\n0\n0\n" \
        | valgrind --leak-check=full --error-exitcode=42 ./msms > /dev/null 2> /tmp/msms_vg.txt
    VGEXIT=$?
    if [ "$VGEXIT" -eq 0 ]; then
        record "valgrind — full session, 0 errors 0 leaks" "PASS" "$(grep 'total heap usage' /tmp/msms_vg.txt)"
    else
        record "valgrind — full session, 0 errors 0 leaks" "FAIL" "valgrind reported errors, see valgrind_output.txt"
        cp /tmp/msms_vg.txt valgrind_output.txt
    fi
else
    echo "  valgrind not installed - skipping (install with: sudo dnf install valgrind)"
fi

# ---------------------------------------------------------------------
section "15. KNOWN LIMITATION — concurrent multi-process access"
# ---------------------------------------------------------------------
echo "This test is EXPECTED TO FAIL. It demonstrates a known, documented" >> "$REPORT"
echo "architectural limitation (no file locking yet), not a regression." >> "$REPORT"
echo "" >> "$REPORT"
fresh_data
printf "admin\nadmin123\n0\n0\n" | ./msms > /dev/null 2>&1
printf "admin\nadmin123\n7\nDrugA\nX1\n10\n1-1-2027\n5\n0\n0\n" > /tmp/msms_sessA.txt
printf "admin\nadmin123\n7\nDrugB\nY1\n20\n1-1-2027\n5\n0\n0\n" > /tmp/msms_sessB.txt
LOST=0
for trial in 1 2 3; do
    fresh_data
    printf "admin\nadmin123\n0\n0\n" | ./msms > /dev/null 2>&1
    ./msms < /tmp/msms_sessA.txt > /dev/null 2>&1 &
    PIDA=$!
    ./msms < /tmp/msms_sessB.txt > /dev/null 2>&1 &
    PIDB=$!
    wait $PIDA $PIDB
    HAS_A=$(grep -c "DrugA" data/inventory.dat)
    HAS_B=$(grep -c "DrugB" data/inventory.dat)
    if [ "$HAS_A" -eq 0 ] || [ "$HAS_B" -eq 0 ]; then
        LOST=$((LOST+1))
    fi
    echo "  Trial $trial: DrugA present=$HAS_A, DrugB present=$HAS_B" >> "$REPORT"
done
if [ "$LOST" -gt 0 ]; then
    record "Concurrent writes (2 processes, same data/)" "FAIL (expected/known)" "$LOST of 3 trials lost data — no file locking exists yet. This is the open item for the next phase, not a new bug."
else
    record "Concurrent writes (2 processes, same data/)" "PASS" "no data lost in 3 trials (re-run a few more times - this is a race condition and doesn't always trigger)"
fi

# ---------------------------------------------------------------------
fresh_data
echo "" >> "$REPORT"
echo "## Summary" >> "$REPORT"
echo "" >> "$REPORT"
echo "**$PASS passed, $FAIL failed (of $((PASS+FAIL)) total)**" >> "$REPORT"
echo ""
echo "=================================================="
echo "  $PASS passed, $FAIL failed (of $((PASS+FAIL)) total)"
echo "  Full report written to: $REPORT"
echo "=================================================="
