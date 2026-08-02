# Medical Supply Management System (MSMS)

A C-based system that tracks medical inventory, manages stock levels,
processes supply requests, and monitors distribution of medicines and
healthcare equipment.

## Build

Requires `gcc` and a POSIX environment (uses `mkdir`, `rename`).

```
gcc -Wall -Wextra -std=c11 -o msms *.c
```

Compiles with **zero warnings**. Run it with:

```
./msms
```

On first run it creates a `data/` folder next to the binary and seeds a
default admin account: `admin` / `admin123`. Log in and create a real
admin user before handing this to anyone else.

## Architecture

8 modules, ~1,680 lines total:

| File | Lines | Responsibility |
|---|---|---|
| `common.h` | 83 | shared structs, enums, file paths — no logic |
| `util.c/h` | 115 / 33 | date normalization, leap-year validation, JDN math |
| `logging.c/h` | 22 / 8 | one function: append a line to `data/audit.log` |
| `auth.c/h` | 209 / 20 | login, lockout, password hashing, user creation |
| `inventory.c/h` | 324 / 33 | hash table CRUD, the only module touching stock records |
| `supply.c/h` | 103 / 13 | incoming stock → increases inventory |
| `distribution.c/h` | 115 / 13 | outgoing requests → FEFO stock draining |
| `reports.c/h` | 193 / 11 | read-only reporting + audit log rendering |
| `main.c` | 383 | menu loop and role gating only, no business logic |

**Data flow:** `main.c` never touches a data file or the hash table
directly — it only calls into the module that owns that concern. Every
module that mutates state (`auth`, `inventory`, `supply`, `distribution`)
calls `logging.c` itself, so the audit trail can't be bypassed by adding
a new entry point later.

```
main.c (menu, role gating)
  ├─ auth.c ──────────────┐
  ├─ inventory.c  <────────┼── supply.c (increases stock)
  │                        └── distribution.c (FEFO, decreases stock)
  ├─ reports.c (reads inventory.c + supply.dat + requests.dat + audit.log)
  └─ util.c, logging.c (used by everything above)
```

### Why each module is separate

- **auth.c** — owns all login state (failed-attempt counters, lockout,
  hashed passwords) and user creation. Creating a user is still "who's
  allowed in," so it stays here instead of splitting authentication
  logic across two files.
- **inventory.c** — the only module that walks the hash table. Every
  other module asks it for records by id or name instead of touching
  chains directly, so the storage engine can be swapped later without
  breaking callers.
- **supply.c** — owns the "stock went up" direction only: merge into an
  exact existing batch or create a new one, then hand off to
  `inventory.c`. No FEFO logic belongs here.
- **distribution.c** — owns the "stock went down" direction: FEFO batch
  selection and draining. Kept separate from `supply.c` so the incoming
  and outgoing flows can't accidentally share state.
- **reports.c** — strictly read-only. It cannot be the source of a data
  bug because it never writes to `inventory.dat`, `supply.dat`, or
  `requests.dat`.
- **logging.c** — one function (`logEvent`), so every module produces
  audit lines in exactly the same format instead of five slightly
  different logging styles.
- **util.c** — pure date/calendar math with no state and no side
  effects, so it's testable in isolation.

## Algorithms

| Algorithm | Where | Purpose |
|---|---|---|
| Hash table + separate chaining | `inventory.c` | O(1) average lookup by record id |
| Greedy FEFO + `qsort()` | `distribution.c` | drain earliest-expiring batch first, span batches if needed |
| Julian Day Number conversion | `util.c` | accurate day-difference math across month/year boundaries |
| Leap-year-aware date validation | `util.c` | reject invalid calendar dates (e.g. `31-2-2026`, `29-2-2026`) |
| Date normalization/padding | `util.c` | `2-5-2026` → `02-05-2026` before validation/storage |
| Atomic file writes (temp file + `rename()`) | `inventory.c`, `supply.c`, `distribution.c`, `auth.c` | a crash mid-write can't corrupt existing data |
| Defensive/fault-tolerant line parsing | every module that loads a file | skip and log a corrupt row instead of crashing |
| FNV-1a password hashing | `auth.c` | avoids plaintext passwords on disk — **not** cryptographically secure |

## Design decisions worth flagging

- **Supply vs. Add Inventory are different entry points on purpose.**
  "Add Inventory Item" (admin menu 7) always creates a brand-new record
  and rejects an exact name+batch+expiry duplicate. "Record Supply
  Received" (admin menu 10) is the WHO-style receiving workflow: it
  merges into an existing exact-match batch if one exists, or creates a
  new one if not. Both funnel through `inventory.c` so stock totals stay
  consistent either way.
- **Update Inventory only touches name, batch, and reorder level.**
  Quantity changes only ever happen through `inventoryIncreaseStock` /
  `inventoryDecreaseStock`, which are only called from `supply.c` and
  `distribution.c`. That means the audit log always shows *why* a
  quantity changed (a supply event or a distribution event), never a
  bare unexplained edit.
- **Distribution requests are recorded even when rejected.** A
  `REQ_REJECTED` outcome (zero matching stock) still writes a row to
  `requests.dat` and the audit log, so the accountability report shows
  every request that came in, not just the successful ones.
- **`data/*.tmp` never accumulates.** Atomic saves write to a `.tmp`
  file and immediately `rename()` over the real file in the same
  function call, so no cleanup step is needed.
- **Menu options are hidden, not just blocked, from staff.** `main.c`
  only prints admin menu items 7–12 when `isAdmin` is true, and the
  handlers double-check the role anyway in case a number is typed blind.
- **Default admin bootstrap.** The spec doesn't define a provisioning
  process, so `authInit()` seeds `admin` / `admin123` only when
  `users.dat` is missing or empty. This is flagged loudly on first run.

## Known limitations

- Password entry is not hidden/masked at the terminal (no `termios` raw
  mode) — out of scope for this version per the original spec.
- FNV-1a is a fast general-purpose hash, not a password KDF. It has no
  salt and no work factor, so it should not be treated as secure storage
  in a real deployment — the spec asked for "not plaintext," not "secure."
- Concurrent access isn't handled — no file locking. Two instances of
  `msms` running against the same `data/` folder at once can race on
  writes.
- `inventoryUpdate` and `inventoryRemove` do not cascade — removing a
  record does not retroactively touch past `supply.dat` or
  `requests.dat` rows that reference the medicine by name, which is
  intentional (those files are a historical log, not a live view).
- Reorder level on `supplyProcess` is only used if the exact batch
  doesn't already exist; supplying more of an existing batch does not
  update its reorder level (by design — reorder level is a property of
  the batch, not the delivery).
- No CSV/export or search-by-partial-name; reports print full result
  sets to stdout.
