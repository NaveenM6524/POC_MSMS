# Traceability Matrix

Maps every requirement in the original spec to the function(s) that
implement it.

## Core spec statement

> "Develop a C-based Medical Supply Management System that tracks
> medical inventory, manages stock levels, processes supply requests,
> and monitors the distribution of medicines and healthcare equipment
> to ensure timely availability and prevent shortages."

| Spec phrase | Implementing function(s) | File |
|---|---|---|
| tracks medical inventory | `inventoryAddNew`, `inventoryFindById`, `inventoryFindExact`, `inventoryGetAll` | `inventory.c` |
| manages stock levels | `inventoryIncreaseStock`, `inventoryDecreaseStock` | `inventory.c` |
| processes supply requests (incoming) | `supplyProcess` | `supply.c` |
| monitors the distribution... (outgoing) | `distributionProcessRequest` | `distribution.c` |
| ...to ensure timely availability and prevent shortages | `reportLowStock`, `reportExpiry` | `reports.c` |

## Real-world mapping

| Real-world system | Module |
|---|---|
| Hospital Inventory Systems | `inventory.c` |
| WHO Supply Chain Networks | `supply.c` |
| Pharmacy Management Systems / Healthcare Distribution Centers | `distribution.c` |
| Medical Logistics Platforms | `logging.c` + `reports.c` |

## Module-by-module requirement mapping

### 1. auth.c/h — authentication

| Requirement | Function | Notes |
|---|---|---|
| admin and staff login | `authLogin` | returns `OP_SUCCESS` / `OP_NOT_FOUND` / `OP_INVALID_INPUT` |
| password hashing (never plaintext) | `hashPassword` (static, wraps `fnv1aHash`) | stored in `User.passwordHash` |
| lockout after repeated failed attempts | `authLogin` (increments `failedAttempts`, sets `locked` at `MAX_LOGIN_ATTEMPTS`) | see TESTCASES.md TC12 |
| admin creates new users (admin or staff) | `authCreateUser` | stays in auth.c per spec — still an authentication concern |

### 2. inventory.c/h — the stock engine

| Requirement | Function | Notes |
|---|---|---|
| hash table with separate chaining, keyed by record id | `table[HASH_TABLE_SIZE]`, `hashId`, `insertNode` | file-scope static array of chain heads |
| CRUD | `inventoryAddNew` (create), `inventoryFindById`/`inventoryFindExact`/`inventoryGetAll`/`inventoryFindAllByName` (read), `inventoryUpdate` (update), `inventoryRemove` (delete) | |
| atomic file saves (temp file + rename) | `inventorySave` | writes `INVENTORY_TMP` then `rename()`s over `INVENTORY_FILE` |
| corrupt-row tolerant loading | `inventoryInit` | malformed line → `logEvent("SYSTEM","LOAD_WARNING",...)` and skip, no crash |
| handles both medicines and equipment, no type restriction | `Medicine` struct (`common.h`) | `name` field is free-text, not an enum |

### 3. supply.c/h — incoming stock

| Requirement | Function | Notes |
|---|---|---|
| records what arrived, quantity, supplier, date | `supplyProcess` builds a `SupplyTransaction` | |
| records WHICH ADMIN recorded it | `supplyProcess` parameter `adminUsername` → `SupplyTransaction.addedByAdmin` | |
| calls into inventory.c to increase stock | `supplyProcess` → `inventoryIncreaseStock` (exact-match batch) or `inventoryAddNew` (new batch) | |
| atomic file writes | `appendSupplyRecord` (static) | temp file + `rename()` over `SUPPLY_FILE` |

### 4. distribution.c/h — outgoing stock, FEFO

| Requirement | Function | Notes |
|---|---|---|
| processes outgoing supply requests | `distributionProcessRequest` | |
| FEFO: find matching batches, sort by expiry | `inventoryFindAllByName` + `qsort` with `compareByExpiry` | greedy algorithm, see README |
| drain earliest-expiring batch first, span multiple batches | `distributionProcessRequest` loop over sorted `batches[]` | |
| track outcome as FULFILLED / PARTIAL / REJECTED | `RequestStatus` enum assignment in `distributionProcessRequest` | |
| partial fulfillment shows exact quantity, not silent failure | `SupplyRequest.fulfilledQty` always populated and returned to caller | see TESTCASES.md TC8 |
| atomic file writes | `appendRequestRecord` (static) | temp file + `rename()` over `REQUESTS_FILE` |

### 5. reports.c/h — reporting

| Requirement | Function | Notes |
|---|---|---|
| stock report | `reportStock` | |
| low-stock report | `reportLowStock` | quantity ≤ reorderLevel |
| expiry report, configurable day window | `reportExpiry(int daysWindow, ...)` | |
| accurate across month/year boundaries and leap years via JDN | `reportExpiry` calls `dateToJDN` from `util.c` | |
| supply history | `reportSupplyHistory` | reads `SUPPLY_FILE` |
| distribution request history | `reportDistributionHistory` | reads `REQUESTS_FILE` |
| accountability report: who added/removed/supplied/modified what, when | `reportAccountability` | renders `AUDIT_LOG` directly |
| which staff viewed which report and when | every `report*` function ends with `logEvent(viewer, "VIEW_REPORT", ...)` | |

### 6. logging.c/h — audit trail

| Requirement | Function | Notes |
|---|---|---|
| logEvent(user, action, detail) | `logEvent` | single entry point, appends to `AUDIT_LOG` |
| called by auth.c (login/user creation) | `authLogin`, `authCreateUser` | `LOGIN_SUCCESS`, `LOGIN_FAILED`, `LOGIN_BLOCKED`, `ACCOUNT_LOCKED`, `CREATE_USER` |
| called by inventory.c (add/remove/modify, WHICH ADMIN) | `inventoryAddNew`, `inventoryRemove`, `inventoryUpdate`, `inventoryIncreaseStock`, `inventoryDecreaseStock` | `ADD_INVENTORY`, `REMOVE_INVENTORY`, `UPDATE_INVENTORY`, `STOCK_INCREASE`, `STOCK_DECREASE` |
| called by supply.c (stock received, WHICH ADMIN) | `supplyProcess` | `SUPPLY_RECEIVED` |
| called by distribution.c (request processed) | `distributionProcessRequest` | `DISTRIBUTION_REQUEST` |
| called by reports.c (every report view, WHICH STAFF/ADMIN) | every `report*` function | `VIEW_REPORT` |

### 7. util.c/h — date math

| Requirement | Function | Notes |
|---|---|---|
| date normalization (pad single-digit day/month) | `normalizeDate` | `"2-5-2026"` → `"02-05-2026"` |
| strict DD-MM-YYYY validation, leap-year aware | `isValidDate`, `isLeapYear`, `daysInMonth` | `(year%4==0 && year%100!=0) \|\| year%400==0` |
| date comparison | `compareDates` | via JDN difference |
| Julian Day Number conversion for day-difference math | `dateToJDN` | Fliegel & Van Flandern formula |

### 8. main.c — menu loop only

| Requirement | Function | Notes |
|---|---|---|
| menu loop, no business logic | `main`, `printMenu` | all decisions delegate to module functions |
| handlers use direct scanf/fgets, no wrapper helpers | `handleAddInventory`, `handleRemoveInventory`, `handleUpdateInventory`, `handleRecordSupply`, `handleDistributionRequest`, `handleExpiryReport`, `handleCreateUser`, `doLogin` | each repeats the scanf+`clearInputLine`/fgets+`strcspn` pattern directly |
| role-gated menu options | `printMenu(int isAdmin)` + `if (sessionUser.isAdmin)` checks in `main`'s switch | admin items 7-12 hidden from staff and blocked if attempted |
| wires everything together | `main` | calls `ensureDataDir`, `inventoryInit`, `authInit` at startup; `inventoryFreeAll`, `authFreeAll` at shutdown |

## common.h structs

| Required struct | Status | Notes |
|---|---|---|
| `Medicine` (id, name, batch, quantity, expiryDate, reorderLevel, next) | ✅ exact match | no cost/selling price field, per spec |
| `SupplyTransaction` (id, medicineName, quantity, supplierName, addedByAdmin, timestamp) | ✅ exact match | |
| `SupplyRequest` (requestId, medicineName, requestedQty, fulfilledQty, status enum, timestamp) | ✅ exact match | |
| `User` (username, passwordHash, isAdmin, failedAttempts, locked) | ✅ exact match | |
| `OpStatus` enum (OP_SUCCESS, OP_NOT_FOUND, OP_DUPLICATE, OP_INVALID_INPUT, OP_INSUFFICIENT_STOCK, OP_FILE_ERROR) | ✅ exact match | |

## Explicitly out-of-scope items (confirmed absent)

| Excluded feature | Confirmed absent from |
|---|---|
| customer self-service login | `auth.c` only exposes admin/staff login |
| billing / selling price / bill generation | `Medicine` and `SupplyTransaction` structs have no price field |
| email/API alert stubs | `reports.c` only prints to stdout, no network or mail code anywhere in the project |
