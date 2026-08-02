/* test_inventory.c
 *
 * CUnit unit tests for inventory.c.
 *
 * Unlike util.c, these functions touch global state (the hash table) and
 * the filesystem (data/inventory.dat, data/audit.log) via a FIXED relative
 * path baked into common.h - not something we can inject per test. So the
 * isolation strategy is: chdir() the whole test process into a throwaway
 * temp directory before running anything, so "data/" gets created THERE,
 * never touching your real project's data/. Between each test we wipe the
 * in-memory table and delete the temp data file so tests don't leak state
 * into one another.
 *
 * Build (from the same folder as inventory.c, util.c, logging.c):
 *   gcc -Wall -Wextra -std=c11 -o test_inventory test_inventory.c \
 *       inventory.c util.c logging.c -lcunit
 *
 * Run:
 *   ./test_inventory
 */

#define _POSIX_C_SOURCE 200809L
#include <CUnit/Basic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "inventory.h"
#include "common.h"

static char g_tempDir[] = "/tmp/msms_inventory_test_XXXXXX";

/* ---------- fixture helpers ---------- */

/* Wipes in-memory state AND the on-disk file, then reloads from (now
 * empty) disk - equivalent to a fresh program start with no data. Call
 * this at the top of every test so tests never see each other's records. */
static void resetInventoryState(void) {
    inventoryFreeAll();
    remove(INVENTORY_FILE);
    remove(INVENTORY_TMP);
    inventoryInit();
}

static int suiteInit(void) {
    if (mkdtemp(g_tempDir) == NULL) {
        return -1;
    }
    if (chdir(g_tempDir) != 0) {
        return -1;
    }
    inventoryInit();
    return 0;
}

static int suiteCleanup(void) {
    inventoryFreeAll();
    /* best-effort cleanup of the temp dir's contents; not critical if it
     * fails, it's just a /tmp scratch folder */
    remove(INVENTORY_FILE);
    remove(INVENTORY_TMP);
    remove(AUDIT_LOG);
    rmdir(DATA_DIR);
    return 0;
}

/* ---------- inventoryAddNew() ---------- */

static void test_addNew_success(void) {
    resetInventoryState();
    int id = -1;
    OpStatus st = inventoryAddNew("Paracetamol", "B100", 50, "02-05-2026", 20, "tester", &id);
    CU_ASSERT_EQUAL(st, OP_SUCCESS);
    CU_ASSERT_EQUAL(id, 1);

    Medicine *m = inventoryFindById(id);
    CU_ASSERT_PTR_NOT_NULL(m);
    if (m) {
        CU_ASSERT_STRING_EQUAL(m->name, "Paracetamol");
        CU_ASSERT_EQUAL(m->quantity, 50);
    }
}

static void test_addNew_exact_triple_duplicate_rejected(void) {
    resetInventoryState();
    int id1 = -1, id2 = -1;
    OpStatus st1 = inventoryAddNew("Paracetamol", "B100", 50, "02-05-2026", 20, "tester", &id1);
    OpStatus st2 = inventoryAddNew("Paracetamol", "B100", 999, "02-05-2026", 20, "tester", &id2);
    CU_ASSERT_EQUAL(st1, OP_SUCCESS);
    CU_ASSERT_EQUAL(st2, OP_DUPLICATE);
}

static void test_addNew_same_name_different_batch_allowed(void) {
    resetInventoryState();
    int id1 = -1, id2 = -1;
    OpStatus st1 = inventoryAddNew("Amoxicillin", "A1", 10, "01-01-2027", 5, "tester", &id1);
    OpStatus st2 = inventoryAddNew("Amoxicillin", "A2", 10, "01-01-2027", 5, "tester", &id2);
    CU_ASSERT_EQUAL(st1, OP_SUCCESS);
    CU_ASSERT_EQUAL(st2, OP_SUCCESS);
    CU_ASSERT_NOT_EQUAL(id1, id2);
}

static void test_addNew_same_name_batch_different_expiry_allowed(void) {
    /* documents the known, deliberately-not-fixed gap from the handoff:
     * same name+batch with a DIFFERENT expiry is not treated as a
     * duplicate/conflict - it silently creates a second record. */
    resetInventoryState();
    int id1 = -1, id2 = -1;
    OpStatus st1 = inventoryAddNew("Ibuprofen", "B1", 10, "01-01-2027", 5, "tester", &id1);
    OpStatus st2 = inventoryAddNew("Ibuprofen", "B1", 10, "01-01-2028", 5, "tester", &id2);
    CU_ASSERT_EQUAL(st1, OP_SUCCESS);
    CU_ASSERT_EQUAL(st2, OP_SUCCESS);
}

static void test_addNew_negative_quantity_rejected(void) {
    resetInventoryState();
    int id = -1;
    OpStatus st = inventoryAddNew("Aspirin", "B1", -5, "01-01-2027", 5, "tester", &id);
    CU_ASSERT_EQUAL(st, OP_INVALID_INPUT);
}

static void test_addNew_negative_reorder_level_rejected(void) {
    resetInventoryState();
    int id = -1;
    OpStatus st = inventoryAddNew("Aspirin", "B1", 10, "01-01-2027", -1, "tester", &id);
    CU_ASSERT_EQUAL(st, OP_INVALID_INPUT);
}

static void test_addNew_invalid_date_rejected(void) {
    resetInventoryState();
    int id = -1;
    OpStatus st = inventoryAddNew("Aspirin", "B1", 10, "31-02-2027", 5, "tester", &id);
    CU_ASSERT_EQUAL(st, OP_INVALID_INPUT);
}

static void test_addNew_empty_name_rejected(void) {
    resetInventoryState();
    int id = -1;
    OpStatus st = inventoryAddNew("", "B1", 10, "01-01-2027", 5, "tester", &id);
    CU_ASSERT_EQUAL(st, OP_INVALID_INPUT);
}

static void test_addNew_empty_batch_rejected(void) {
    resetInventoryState();
    int id = -1;
    OpStatus st = inventoryAddNew("Aspirin", "", 10, "01-01-2027", 5, "tester", &id);
    CU_ASSERT_EQUAL(st, OP_INVALID_INPUT);
}

static void test_addNew_zero_quantity_allowed(void) {
    /* qty < 0 is rejected, but qty == 0 is not - confirms 0 is a valid
     * boundary (e.g. "recording a batch we expect stock for later"). */
    resetInventoryState();
    int id = -1;
    OpStatus st = inventoryAddNew("ZeroStock", "B1", 0, "01-01-2027", 5, "tester", &id);
    CU_ASSERT_EQUAL(st, OP_SUCCESS);
}

static void test_addNew_ids_increment_sequentially(void) {
    resetInventoryState();
    int id1 = -1, id2 = -1, id3 = -1;
    inventoryAddNew("A", "B1", 1, "01-01-2027", 1, "tester", &id1);
    inventoryAddNew("B", "B2", 1, "01-01-2027", 1, "tester", &id2);
    inventoryAddNew("C", "B3", 1, "01-01-2027", 1, "tester", &id3);
    CU_ASSERT_EQUAL(id1, 1);
    CU_ASSERT_EQUAL(id2, 2);
    CU_ASSERT_EQUAL(id3, 3);
}

/* ---------- inventoryFindExact() ---------- */

static void test_findExact_no_match_returns_null(void) {
    resetInventoryState();
    Medicine *m = inventoryFindExact("Nonexistent", "X", "01-01-2027");
    CU_ASSERT_PTR_NULL(m);
}

/* ---------- inventoryIncreaseStock() ---------- */

static void test_increaseStock_success(void) {
    resetInventoryState();
    int id = -1;
    inventoryAddNew("Insulin", "B1", 10, "01-01-2027", 5, "tester", &id);
    OpStatus st = inventoryIncreaseStock(id, 5, "tester");
    CU_ASSERT_EQUAL(st, OP_SUCCESS);

    Medicine *m = inventoryFindById(id);
    CU_ASSERT_PTR_NOT_NULL(m);
    if (m) CU_ASSERT_EQUAL(m->quantity, 15);
}

static void test_increaseStock_zero_qty_rejected(void) {
    resetInventoryState();
    int id = -1;
    inventoryAddNew("Insulin", "B1", 10, "01-01-2027", 5, "tester", &id);
    OpStatus st = inventoryIncreaseStock(id, 0, "tester");
    CU_ASSERT_EQUAL(st, OP_INVALID_INPUT);
}

static void test_increaseStock_negative_qty_rejected(void) {
    resetInventoryState();
    int id = -1;
    inventoryAddNew("Insulin", "B1", 10, "01-01-2027", 5, "tester", &id);
    OpStatus st = inventoryIncreaseStock(id, -3, "tester");
    CU_ASSERT_EQUAL(st, OP_INVALID_INPUT);
}

static void test_increaseStock_nonexistent_id_returns_notfound(void) {
    resetInventoryState();
    OpStatus st = inventoryIncreaseStock(9999, 5, "tester");
    CU_ASSERT_EQUAL(st, OP_NOT_FOUND);
}

/* ---------- inventoryDecreaseStock() ---------- */

static void test_decreaseStock_success(void) {
    resetInventoryState();
    int id = -1;
    inventoryAddNew("Insulin", "B1", 10, "01-01-2027", 5, "tester", &id);
    OpStatus st = inventoryDecreaseStock(id, 4, "tester");
    CU_ASSERT_EQUAL(st, OP_SUCCESS);

    Medicine *m = inventoryFindById(id);
    CU_ASSERT_PTR_NOT_NULL(m);
    if (m) CU_ASSERT_EQUAL(m->quantity, 6);
}

static void test_decreaseStock_exact_boundary_to_zero_allowed(void) {
    /* requesting exactly the full available quantity should succeed
     * (the reject condition is qty > m->quantity, so qty == quantity
     * is the boundary and must be allowed, draining to exactly 0) */
    resetInventoryState();
    int id = -1;
    inventoryAddNew("Insulin", "B1", 10, "01-01-2027", 5, "tester", &id);
    OpStatus st = inventoryDecreaseStock(id, 10, "tester");
    CU_ASSERT_EQUAL(st, OP_SUCCESS);

    Medicine *m = inventoryFindById(id);
    CU_ASSERT_PTR_NOT_NULL(m);
    if (m) CU_ASSERT_EQUAL(m->quantity, 0);
}

static void test_decreaseStock_insufficient_stock_rejected(void) {
    resetInventoryState();
    int id = -1;
    inventoryAddNew("Insulin", "B1", 10, "01-01-2027", 5, "tester", &id);
    OpStatus st = inventoryDecreaseStock(id, 11, "tester");
    CU_ASSERT_EQUAL(st, OP_INSUFFICIENT_STOCK);

    /* confirm the rejected decrease didn't partially apply */
    Medicine *m = inventoryFindById(id);
    CU_ASSERT_PTR_NOT_NULL(m);
    if (m) CU_ASSERT_EQUAL(m->quantity, 10);
}

static void test_decreaseStock_negative_qty_rejected(void) {
    resetInventoryState();
    int id = -1;
    inventoryAddNew("Insulin", "B1", 10, "01-01-2027", 5, "tester", &id);
    OpStatus st = inventoryDecreaseStock(id, -1, "tester");
    CU_ASSERT_EQUAL(st, OP_INVALID_INPUT);
}

/* ---------- inventoryRemove() ---------- */

static void test_remove_success(void) {
    resetInventoryState();
    int id = -1;
    inventoryAddNew("Insulin", "B1", 10, "01-01-2027", 5, "tester", &id);
    OpStatus st = inventoryRemove(id, "tester");
    CU_ASSERT_EQUAL(st, OP_SUCCESS);
    CU_ASSERT_PTR_NULL(inventoryFindById(id));
}

static void test_remove_nonexistent_returns_notfound(void) {
    resetInventoryState();
    OpStatus st = inventoryRemove(9999, "tester");
    CU_ASSERT_EQUAL(st, OP_NOT_FOUND);
}

/* ---------- inventoryUpdate() ---------- */

static void test_update_success(void) {
    resetInventoryState();
    int id = -1;
    inventoryAddNew("Insulin", "B1", 10, "01-01-2027", 5, "tester", &id);
    OpStatus st = inventoryUpdate(id, "Insulin Renamed", "B1", 8, "tester");
    CU_ASSERT_EQUAL(st, OP_SUCCESS);

    Medicine *m = inventoryFindById(id);
    CU_ASSERT_PTR_NOT_NULL(m);
    if (m) {
        CU_ASSERT_STRING_EQUAL(m->name, "Insulin Renamed");
        CU_ASSERT_EQUAL(m->reorderLevel, 8);
    }
}

static void test_update_nonexistent_returns_notfound(void) {
    resetInventoryState();
    OpStatus st = inventoryUpdate(9999, "X", "Y", 1, "tester");
    CU_ASSERT_EQUAL(st, OP_NOT_FOUND);
}

static void test_update_empty_name_rejected(void) {
    resetInventoryState();
    int id = -1;
    inventoryAddNew("Insulin", "B1", 10, "01-01-2027", 5, "tester", &id);
    OpStatus st = inventoryUpdate(id, "", "B1", 5, "tester");
    CU_ASSERT_EQUAL(st, OP_INVALID_INPUT);
}

static void test_update_negative_reorder_rejected(void) {
    resetInventoryState();
    int id = -1;
    inventoryAddNew("Insulin", "B1", 10, "01-01-2027", 5, "tester", &id);
    OpStatus st = inventoryUpdate(id, "Insulin", "B1", -1, "tester");
    CU_ASSERT_EQUAL(st, OP_INVALID_INPUT);
}

/* ---------- inventoryGetAll() / inventoryFindAllByName() ---------- */

static void test_getAll_returns_correct_count(void) {
    resetInventoryState();
    inventoryAddNew("A", "B1", 1, "01-01-2027", 1, "tester", NULL);
    inventoryAddNew("B", "B2", 1, "01-01-2027", 1, "tester", NULL);
    inventoryAddNew("C", "B3", 1, "01-01-2027", 1, "tester", NULL);

    Medicine *results[10];
    int count = inventoryGetAll(results, 10);
    CU_ASSERT_EQUAL(count, 3);
}

static void test_getAll_respects_maxResults_cap(void) {
    resetInventoryState();
    inventoryAddNew("A", "B1", 1, "01-01-2027", 1, "tester", NULL);
    inventoryAddNew("B", "B2", 1, "01-01-2027", 1, "tester", NULL);
    inventoryAddNew("C", "B3", 1, "01-01-2027", 1, "tester", NULL);

    Medicine *results[10];
    int count = inventoryGetAll(results, 2);
    CU_ASSERT_EQUAL(count, 2);
}

static void test_findAllByName_excludes_zero_quantity(void) {
    /* inventoryFindAllByName only returns records with quantity > 0 -
     * a fully-drained batch (e.g. after FEFO fulfillment) shouldn't show
     * up as a live match for further requests. */
    resetInventoryState();
    int id1 = -1, id2 = -1;
    inventoryAddNew("Paracetamol", "B1", 0, "01-01-2027", 5, "tester", &id1);
    inventoryAddNew("Paracetamol", "B2", 10, "01-01-2027", 5, "tester", &id2);

    Medicine *results[10];
    int count = inventoryFindAllByName("Paracetamol", results, 10);
    CU_ASSERT_EQUAL(count, 1);
    if (count == 1) {
        CU_ASSERT_EQUAL(results[0]->id, id2);
    }
}

int main(void) {
    if (CUE_SUCCESS != CU_initialize_registry()) {
        return CU_get_error();
    }

    CU_pSuite suite = CU_add_suite("inventory_business_logic", suiteInit, suiteCleanup);
    if (suite == NULL) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    struct { const char *name; void (*fn)(void); } tests[] = {
        { "addNew: success",                              test_addNew_success },
        { "addNew: exact triple duplicate rejected",      test_addNew_exact_triple_duplicate_rejected },
        { "addNew: same name, different batch allowed",   test_addNew_same_name_different_batch_allowed },
        { "addNew: same name+batch, different expiry (KNOWN GAP, allowed)", test_addNew_same_name_batch_different_expiry_allowed },
        { "addNew: negative quantity rejected",           test_addNew_negative_quantity_rejected },
        { "addNew: negative reorder level rejected",      test_addNew_negative_reorder_level_rejected },
        { "addNew: invalid date rejected",                test_addNew_invalid_date_rejected },
        { "addNew: empty name rejected",                  test_addNew_empty_name_rejected },
        { "addNew: empty batch rejected",                 test_addNew_empty_batch_rejected },
        { "addNew: zero quantity allowed (boundary)",     test_addNew_zero_quantity_allowed },
        { "addNew: ids increment sequentially",           test_addNew_ids_increment_sequentially },

        { "findExact: no match returns NULL",             test_findExact_no_match_returns_null },

        { "increaseStock: success",                       test_increaseStock_success },
        { "increaseStock: zero qty rejected",             test_increaseStock_zero_qty_rejected },
        { "increaseStock: negative qty rejected",         test_increaseStock_negative_qty_rejected },
        { "increaseStock: nonexistent id -> NOT_FOUND",   test_increaseStock_nonexistent_id_returns_notfound },

        { "decreaseStock: success",                       test_decreaseStock_success },
        { "decreaseStock: exact boundary to zero allowed", test_decreaseStock_exact_boundary_to_zero_allowed },
        { "decreaseStock: insufficient stock rejected",   test_decreaseStock_insufficient_stock_rejected },
        { "decreaseStock: negative qty rejected",         test_decreaseStock_negative_qty_rejected },

        { "remove: success",                              test_remove_success },
        { "remove: nonexistent -> NOT_FOUND",             test_remove_nonexistent_returns_notfound },

        { "update: success",                              test_update_success },
        { "update: nonexistent -> NOT_FOUND",             test_update_nonexistent_returns_notfound },
        { "update: empty name rejected",                  test_update_empty_name_rejected },
        { "update: negative reorder rejected",            test_update_negative_reorder_rejected },

        { "getAll: correct count",                        test_getAll_returns_correct_count },
        { "getAll: respects maxResults cap",              test_getAll_respects_maxResults_cap },
        { "findAllByName: excludes zero-quantity records", test_findAllByName_excludes_zero_quantity },
    };

    size_t n = sizeof(tests) / sizeof(tests[0]);
    for (size_t i = 0; i < n; i++) {
        if (CU_add_test(suite, tests[i].name, tests[i].fn) == NULL) {
            CU_cleanup_registry();
            return CU_get_error();
        }
    }

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    unsigned int failures = CU_get_number_of_failures();
    CU_cleanup_registry();
    return (failures == 0) ? 0 : 1;
}
