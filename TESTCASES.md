# Test Cases — Real Output From the Compiled Binary

All output below was captured by actually running `./msms` after
`gcc -Wall -Wextra -std=c11 -o msms *.c` (zero warnings). Each test
builds on the data left by the previous one, in order, starting from an
empty `data/` folder — this is one continuous session history, not 14
isolated resets, so record ids and quantities accumulate realistically.

Build confirmation:

```
$ gcc -Wall -Wextra -std=c11 -o msms *.c
$ echo $?
0
```
(no warnings, no errors)

---

## TC1 — Unpadded date input normalizes correctly

Input `2-5-2026` (no leading zeros) should store as `02-05-2026`.

**Command:** login as default admin, add inventory item with expiry `2-5-2026`.

**Captured output (relevant excerpt):**
```
Medicine/equipment name: Batch number: Quantity: Expiry date (D-M-YYYY): Reorder level: Added as record id 1.
```

**`data/inventory.dat` after:**
```
1|Paracetamol|B100|50|02-05-2026|20
```
✅ `2-5-2026` normalized to `02-05-2026` before storage.

---

## TC2 — Invalid calendar date rejected

Input `31-2-2026` (February has no 31st day).

**Captured output:**
```
Medicine/equipment name: Batch number: Quantity: Expiry date (D-M-YYYY): Reorder level: Invalid expiry date - nothing was changed.
```

**`data/inventory.dat` after (unchanged):**
```
1|Paracetamol|B100|50|02-05-2026|20
```
✅ Rejected with a clear message, record count unchanged.

---

## TC3 — Negative quantity rejected

Input quantity `-5`.

**Captured output:**
```
Medicine/equipment name: Batch number: Quantity: Expiry date (D-M-YYYY): Reorder level: Invalid input - nothing was changed.
```

**`data/inventory.dat` after (unchanged):**
```
1|Paracetamol|B100|50|02-05-2026|20
```
✅ Rejected, no crash, no partial write.

---

## TC4 — Duplicate batch (exact name + batch + expiry) rejected

Re-adding `Paracetamol / B100 / 02-05-2026` via **Add Inventory Item**
(admin menu 7), which creates new records rather than merging.

**Captured output:**
```
Menu: Add inventory item →
Medicine/equipment name: Batch number: Quantity: Expiry date (D-M-YYYY): Reorder level: That already exists.
```

**`data/inventory.dat` after (unchanged, still one record, still qty 50):**
```
1|Paracetamol|B100|50|02-05-2026|20
```
✅ `OP_DUPLICATE` returned, no second record created.

---

## TC5 — Record Supply *merges* into an existing exact batch

Using **Record Supply Received** (admin menu 10) for the *same*
`Paracetamol / B100 / 02-05-2026` with quantity 15. This is the WHO-style
receiving workflow — it should increase stock on the existing record,
not create a duplicate.

**Captured output:**
```
Medicine/equipment name: Batch number: Quantity received: Expiry date (D-M-YYYY): Reorder level (used only if this is a new batch): Supplier name: Done.
```

**`data/inventory.dat` after (still one record, qty 50 → 65):**
```
1|Paracetamol|B100|65|02-05-2026|20
```
✅ Merged correctly: 50 + 15 = 65, no duplicate record.

---

## TC6 — Record Supply creates a new record for a genuinely new batch

Supplying `Ibuprofen / B200 / 15-3-2029` (a medicine/batch never seen
before) via Record Supply.

**Captured output:**
```
Medicine/equipment name: Batch number: Quantity received: Expiry date (D-M-YYYY): Reorder level (used only if this is a new batch): Supplier name: Done.
```

**`data/inventory.dat` after (new record id=2):**
```
1|Paracetamol|B100|65|02-05-2026|20
2|Ibuprofen|B200|30|15-03-2029|10
```
✅ New batch → new record via `inventoryAddNew`, correctly separate from
the merge path in TC5.

---

## TC7 — FEFO drains the earliest-expiring batch first, spans batches

Setup: added a second Paracetamol batch, `B050-early`, qty 20, expiring
`01-01-2026` (earlier than `B100`'s `02-05-2026`).

**`data/inventory.dat` before the request:**
```
1|Paracetamol|B100|65|02-05-2026|20
2|Ibuprofen|B200|30|15-03-2029|10
3|Paracetamol|B050-early|20|01-01-2026|5
```

**Command:** process a distribution request for 75 units of Paracetamol.

**Captured output:**
```
Medicine/equipment name: Quantity requested: Request #1: FULFILLED (75 of 75 fulfilled)
```

**`data/inventory.dat` after:**
```
1|Paracetamol|B100|10|02-05-2026|20
2|Ibuprofen|B200|30|15-03-2029|10
3|Paracetamol|B050-early|0|01-01-2026|5
```
✅ `B050-early` (earlier expiry) drained fully first (20 → 0), then the
remaining 55 came out of `B100` (65 → 10). Total fulfilled = 75. This is
the FEFO greedy algorithm working as intended, spanning two batches.

---

## TC8 — Over-request results in PARTIAL fulfillment with exact quantity

**Command:** request 1000 units of Paracetamol when only 10 remain
(from TC7's leftover `B100` stock).

**Captured output:**
```
Medicine/equipment name: Quantity requested: Request #2: PARTIAL (10 of 1000 fulfilled)
```

**`data/inventory.dat` after (both Paracetamol batches now at 0):**
```
1|Paracetamol|B100|0|02-05-2026|20
2|Ibuprofen|B200|30|15-03-2029|10
3|Paracetamol|B050-early|0|01-01-2026|5
```
✅ Did not fail silently — `fulfilledQty` reported exactly (10), status
correctly set to `PARTIAL`, not `FULFILLED` or `REJECTED`.

---

## TC9 — Corrupted data row on load is skipped, not a crash

Manually appended two malformed lines directly to `data/inventory.dat`:
```
GARBAGE_NOT_A_VALID_ROW
5|MissingFields|OnlyThree
```

**Command:** start `./msms`, log in, view stock report.

**Captured output:**
```
Choice: 
ID    Name                     Batch        Qty      Expiry       Reorder 
---------------------------------------------------------------------
1     Paracetamol              B100         0        02-05-2026   20      
2     Ibuprofen                B200         30       15-03-2029   10      
3     Paracetamol              B050-early   0        01-01-2026   5       
```

**`data/audit.log` tail:**
```
29-07-2026 06:53:13 | SYSTEM | LOAD_WARNING | corrupt inventory row skipped
29-07-2026 06:53:13 | SYSTEM | LOAD_WARNING | corrupt inventory row skipped
29-07-2026 06:53:13 | admin | LOGIN_SUCCESS | login ok
29-07-2026 06:53:13 | admin | VIEW_REPORT | stock report
```
✅ Program did not crash. Both corrupt rows were skipped and logged
individually. All 3 valid records still loaded and displayed correctly.

---

## TC10 — Non-admin (staff) blocked from an admin-only action

**Command:** admin creates `staffuser` (role=staff), logs out, logs in
as `staffuser`, attempts menu option 7 (Add Inventory Item — admin only).

**Captured output (admin creating the user):**
```
New username: New password: Role (1 = admin, 2 = staff): Done.
```

**Captured output (staffuser's session — note the menu itself has no
options 7-12):**
```
--- Login ---
Username: Password: Welcome, staffuser (staff).

===== Medical Supply Management System =====
 1. View stock report
 2. View low stock report
 3. View expiry report
 4. View supply history
 5. View distribution history
 6. Process a distribution request
 0. Logout
Choice: Admins only.
```
✅ Admin-only menu items aren't even shown to staff, and typing the
number anyway (7) still hits the `sessionUser.isAdmin` guard in `main.c`
and prints "Admins only." — no inventory action occurred.

---

## TC11 — Expiry across a leap year boundary

Two attempts: `29-2-2028` (2028 **is** a leap year) and `29-2-2026`
(2026 is **not** a leap year).

**Captured output, 2028 (leap year, valid):**
```
Medicine/equipment name: Batch number: Quantity: Expiry date (D-M-YYYY): Reorder level: Added as record id 4.
```

**Captured output, 2026 (not a leap year, invalid):**
```
Medicine/equipment name: Batch number: Quantity: Expiry date (D-M-YYYY): Reorder level: Invalid expiry date - nothing was changed.
```
✅ `isLeapYear` correctly applies `(year%4==0 && year%100!=0) || year%400==0`:
2028 is divisible by 4 and not by 100 → leap → Feb 29 valid.
2026 is not divisible by 4 → not leap → Feb 29 invalid.

---

## TC12 — Account lockout after repeated failed attempts

**Command:** 3 consecutive wrong-password attempts for `staffuser`.

**Captured output:**
```
Username: Password: Invalid password. Attempts used: 1/3
Try again? (1 = yes, 0 = exit program): Username: Password: Invalid password. Attempts used: 2/3
Try again? (1 = yes, 0 = exit program): Username: Password: Account is locked. Contact an administrator.
```

**Follow-up — correct password on the now-locked account:**
```
Username: Password: Account is locked. Contact an administrator.
```
✅ Locked after the 3rd failed attempt (`MAX_LOGIN_ATTEMPTS = 3`).
Critically, the account **stays locked even when the correct password is
used afterward** — lockout isn't just a soft warning, it actually blocks
login until an admin intervenes (no unlock feature was requested by the
spec, so there currently isn't one).

---

## TC13 — Audit log correctly attributes an admin's stock addition

Referencing TC11's addition of `LeapMed` (record id 4) by `admin`.

**`data/audit.log` entry:**
```
29-07-2026 06:53:24 | admin | ADD_INVENTORY | added record id=4 name=LeapMed batch=L1 qty=5
```
✅ Username `admin` attributed correctly, action and detail both present.

---

## TC14 — Audit log correctly attributes a staff member's report view

**Command:** admin creates `staffuser2` (role=staff), logs in as
`staffuser2`, views the low stock report.

**`data/audit.log` entries:**
```
29-07-2026 06:53:34 | admin | CREATE_USER | created user=staffuser2 role=staff
29-07-2026 06:53:34 | staffuser2 | LOGIN_SUCCESS | login ok
29-07-2026 06:53:34 | staffuser2 | VIEW_REPORT | low stock report
```
✅ The `VIEW_REPORT` line is attributed to `staffuser2` (the viewer), not
`admin` (who created the account) — confirming `reports.c` logs the
actual session user on every report call, not a hardcoded actor.

---

## Summary

| # | Case | Result |
|---|---|---|
| TC1 | Unpadded date normalization | ✅ Pass |
| TC2 | Invalid calendar date rejected | ✅ Pass |
| TC3 | Negative quantity rejected | ✅ Pass |
| TC4 | Exact duplicate batch rejected (Add Inventory) | ✅ Pass |
| TC5 | Supply merges into existing exact batch | ✅ Pass |
| TC6 | Supply creates new record for new batch | ✅ Pass |
| TC7 | FEFO drains earliest batch first, spans batches | ✅ Pass |
| TC8 | Over-request → PARTIAL with exact quantity | ✅ Pass |
| TC9 | Corrupted row skipped, no crash | ✅ Pass |
| TC10 | Non-admin blocked from admin action | ✅ Pass |
| TC11 | Leap-year-aware expiry validation | ✅ Pass |
| TC12 | Lockout after 3 failed attempts, stays locked | ✅ Pass |
| TC13 | Audit log attributes admin's stock addition | ✅ Pass |
| TC14 | Audit log attributes staff's report view | ✅ Pass |

14/14 passing against the actual compiled binary.
