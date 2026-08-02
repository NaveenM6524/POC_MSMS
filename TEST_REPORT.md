# MSMS Test Report — 03-08-2026 02:44:58


## 0. Build

- ✅ **Compile with -Wall -Wextra -std=c11** — zero warnings

## 1. Static analysis (cppcheck)

- ✅ **cppcheck** — 0 errors (9 style-level notes, non-blocking)
```
Checking auth.c ...
1/8 files checked 13% done
Checking distribution.c ...
distribution.c:0:0: information: Limiting analysis of branches. Use --check-level=exhaustive to analyze all branches. [normalCheckLevelMaxBranches]

^
2/8 files checked 21% done
Checking inventory.c ...
inventory.c:0:0: information: Limiting analysis of branches. Use --check-level=exhaustive to analyze all branches. [normalCheckLevelMaxBranches]

^
inventory.c:153:24: style: Variable 'cur' can be declared as pointer to const [constVariablePointer]
        for (Medicine *cur = table[i]; cur; cur = cur->next) {
                       ^
3/8 files checked 42% done
Checking logging.c ...
4/8 files checked 43% done
Checking main.c ...
main.c:0:0: information: Limiting analysis of branches. Use --check-level=exhaustive to analyze all branches. [normalCheckLevelMaxBranches]

^
5/8 files checked 71% done
Checking reports.c ...
reports.c:0:0: information: Limiting analysis of branches. Use --check-level=exhaustive to analyze all branches. [normalCheckLevelMaxBranches]

^
reports.c:138:24: style: The scope of the variable 'statusNames' can be reduced. [variableScope]
    static const char *statusNames[] = { "FULFILLED", "PARTIAL", "REJECTED" };
                       ^
6/8 files checked 86% done
Checking supply.c ...
supply.c:0:0: information: Limiting analysis of branches. Use --check-level=exhaustive to analyze all branches. [normalCheckLevelMaxBranches]

^
supply.c:66:15: style: Variable 'existing' can be declared as pointer to const [constVariablePointer]
    Medicine *existing = inventoryFindExact(medicineName, batch, normalizedExpiry);
              ^
7/8 files checked 93% done
Checking util.c ...
util.c:106:16: style: Variable 'tmNow' can be declared as pointer to const [constVariablePointer]
    struct tm *tmNow = localtime(&now);
               ^
util.c:113:16: style: Variable 'tmNow' can be declared as pointer to const [constVariablePointer]
    struct tm *tmNow = localtime(&now);
               ^
8/8 files checked 100% done
inventory.c:95:11: style: The function 'inventoryFindById' should have static linkage since it is not used outside of its translation unit. [staticFunction]
Medicine *inventoryFindById(int id)
          ^
inventory.c:143:10: style: The function 'inventorySave' should have static linkage since it is not used outside of its translation unit. [staticFunction]
OpStatus inventorySave(void)
         ^
util.c:39:5: style: The function 'isLeapYear' should have static linkage since it is not used outside of its translation unit. [staticFunction]
int isLeapYear(int year)
    ^
util.c:44:5: style: The function 'daysInMonth' should have static linkage since it is not used outside of its translation unit. [staticFunction]
int daysInMonth(int month, int year)
    ^
nofile:0:0: information: Active checkers: 119/186 (use --checkers-report=<filename> to see details) [checkersReport]
```

## 2. Functional — login, add inventory, unpadded date normalization

- ✅ **Unpadded date normalization** — 2-5-2026 stored as 02-05-2026

## 3. Input validation — invalid date, negative qty, empty field

- ✅ **Reject invalid calendar date (31-2-2026)** — rejected with clear message
- ✅ **Reject negative quantity** — rejected with clear message

## 4. Delimiter injection — '|' in name/batch/supplier/username

- ❌ **Reject '|' in medicine name** — was not rejected — check data/inventory.dat for corruption

## 5. Duplicate batch handling — Add vs Record Supply

- ✅ **Reject exact duplicate via Add Inventory** — OP_DUPLICATE returned correctly
- ✅ **Merge into exact batch via Record Supply** — qty 50 -> 65 (merged, not duplicated)

## 6. FEFO — drains earliest-expiring batch first, spans batches

- ✅ **FEFO drains earliest batch first, spans batches** — earlier batch (B050) drained to 0 first, request fully filled across 2 batches

## 7. Over-request — partial fulfillment reports exact quantity

- ✅ **Over-request yields PARTIAL with exact quantity** — Request #2: PARTIAL (10 of 1000 fulfilled)

## 8. Corrupted data row on load — skipped, not a crash

- ✅ **Corrupted rows skipped without crash** — exit code 0, corrupt rows logged and skipped

## 9. Non-admin gating — staff blocked from admin actions

- ✅ **Staff blocked from admin-only menu action** — correctly refused

## 10. Leap-year-aware date validation

- ✅ **Leap-year-aware validation (2028 valid, 2026 invalid)** — both cases correct

## 11. Account lockout after repeated failed logins

- ✅ **Account locks after 3 failed attempts** — locked as expected
- ✅ **Locked account stays locked even with correct password** — still refused

## 12. Oversized input — buffer safety + no infinite loop on EOF

- ❌ **Oversized input does not crash or hang** — TIMED OUT — infinite loop on EOF

## 13. Scale test — 5000 records, full report accuracy


## 14. Memory safety — valgrind across a full session

- ❌ **valgrind — full session, 0 errors 0 leaks** — valgrind reported errors, see valgrind_output.txt

## 15. KNOWN LIMITATION — concurrent multi-process access

This test is EXPECTED TO FAIL. It demonstrates a known, documented
architectural limitation (no file locking yet), not a regression.

  Trial 1: DrugA present=, DrugB present=
  Trial 2: DrugA present=, DrugB present=
  Trial 3: DrugA present=, DrugB present=
- ✅ **Concurrent writes (2 processes, same data/)** — no data lost in 3 trials (re-run a few more times - this is a race condition and doesn't always trigger)

## Summary

**15 passed, 3 failed (of 18 total)**
