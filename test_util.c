/* test_util.c
 *
 * CUnit unit tests for util.c's date/calendar functions.
 * These are pure functions (no file I/O, no globals), which makes them
 * ideal for true unit testing — we call them directly, not through the CLI.
 *
 * Build (from the tests/ directory, with POC/ as a sibling folder):
 *   gcc -Wall -Wextra -std=c11 -I../POC -o test_util test_util.c ../POC/util.c -lcunit
 *
 * Run:
 *   ./test_util
 */

#include <CUnit/Basic.h>
#include <string.h>
#include "util.h"

/* ---------- normalizeDate() ---------- */

static void test_normalizeDate_unpadded(void) {
    char out[MAX_DATE_LEN];
    CU_ASSERT_EQUAL(normalizeDate("2-5-2026", out), 1);
    CU_ASSERT_STRING_EQUAL(out, "02-05-2026");
}

static void test_normalizeDate_already_padded(void) {
    char out[MAX_DATE_LEN];
    CU_ASSERT_EQUAL(normalizeDate("02-05-2026", out), 1);
    CU_ASSERT_STRING_EQUAL(out, "02-05-2026");
}

static void test_normalizeDate_single_digit_year_rejected_by_isValidDate(void) {
    /* normalizeDate() itself doesn't check calendar validity, just format -
     * confirm it still normalizes (isValidDate is a separate, later check) */
    char out[MAX_DATE_LEN];
    CU_ASSERT_EQUAL(normalizeDate("1-1-1", out), 1);
    CU_ASSERT_STRING_EQUAL(out, "01-01-0001");
}

static void test_normalizeDate_zero_day_rejected(void) {
    char out[MAX_DATE_LEN];
    CU_ASSERT_EQUAL(normalizeDate("0-5-2026", out), 0);
}

static void test_normalizeDate_zero_month_rejected(void) {
    char out[MAX_DATE_LEN];
    CU_ASSERT_EQUAL(normalizeDate("5-0-2026", out), 0);
}

static void test_normalizeDate_negative_day_rejected(void) {
    char out[MAX_DATE_LEN];
    CU_ASSERT_EQUAL(normalizeDate("-1-5-2026", out), 0);
}

static void test_normalizeDate_missing_parts_rejected(void) {
    char out[MAX_DATE_LEN];
    CU_ASSERT_EQUAL(normalizeDate("5-5", out), 0);
}

static void test_normalizeDate_garbage_input_rejected(void) {
    char out[MAX_DATE_LEN];
    CU_ASSERT_EQUAL(normalizeDate("not-a-date", out), 0);
}

static void test_normalizeDate_trailing_garbage_KNOWN_GAP(void) {
    /* sscanf("%d-%d-%d", ...) stops at the first non-digit, so trailing
     * junk after a valid-looking date is silently accepted and the digits
     * still parse. This documents current behavior (not a crash, just a
     * looser-than-expected accept) rather than asserting it's correct -
     * flag it in your test report as a known input-validation looseness. */
    char out[MAX_DATE_LEN];
    int result = normalizeDate("5-5-2026xyz", out);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_STRING_EQUAL(out, "05-05-2026");
}

/* ---------- isLeapYear() ---------- */

static void test_isLeapYear_divisible_by_4(void) {
    CU_ASSERT_TRUE(isLeapYear(2024));
}

static void test_isLeapYear_divisible_by_100_not_leap(void) {
    CU_ASSERT_FALSE(isLeapYear(1900));
}

static void test_isLeapYear_divisible_by_400_is_leap(void) {
    CU_ASSERT_TRUE(isLeapYear(2000));
}

static void test_isLeapYear_ordinary_non_leap(void) {
    CU_ASSERT_FALSE(isLeapYear(2023));
}

static void test_isLeapYear_far_future_400_multiple(void) {
    CU_ASSERT_TRUE(isLeapYear(2400));
}

/* ---------- daysInMonth() ---------- */

static void test_daysInMonth_february_leap(void) {
    CU_ASSERT_EQUAL(daysInMonth(2, 2028), 29);
}

static void test_daysInMonth_february_non_leap(void) {
    CU_ASSERT_EQUAL(daysInMonth(2, 2026), 28);
}

static void test_daysInMonth_april_30(void) {
    CU_ASSERT_EQUAL(daysInMonth(4, 2026), 30);
}

static void test_daysInMonth_january_31(void) {
    CU_ASSERT_EQUAL(daysInMonth(1, 2026), 31);
}

static void test_daysInMonth_invalid_month_zero(void) {
    CU_ASSERT_EQUAL(daysInMonth(0, 2026), 0);
}

static void test_daysInMonth_invalid_month_13(void) {
    CU_ASSERT_EQUAL(daysInMonth(13, 2026), 0);
}

/* ---------- isValidDate() ---------- */

static void test_isValidDate_ordinary_valid(void) {
    CU_ASSERT_TRUE(isValidDate("01-01-2026"));
}

static void test_isValidDate_feb31_rejected(void) {
    CU_ASSERT_FALSE(isValidDate("31-02-2026"));
}

static void test_isValidDate_leap_feb29_accepted(void) {
    CU_ASSERT_TRUE(isValidDate("29-02-2028"));
}

static void test_isValidDate_non_leap_feb29_rejected(void) {
    CU_ASSERT_FALSE(isValidDate("29-02-2026"));
}

static void test_isValidDate_april31_rejected(void) {
    CU_ASSERT_FALSE(isValidDate("31-04-2026"));
}

static void test_isValidDate_day_zero_rejected(void) {
    CU_ASSERT_FALSE(isValidDate("00-01-2026"));
}

static void test_isValidDate_month_zero_rejected(void) {
    CU_ASSERT_FALSE(isValidDate("01-00-2026"));
}

static void test_isValidDate_month_13_rejected(void) {
    CU_ASSERT_FALSE(isValidDate("01-13-2026"));
}

static void test_isValidDate_year_lower_boundary_1900_valid(void) {
    CU_ASSERT_TRUE(isValidDate("01-01-1900"));
}

static void test_isValidDate_year_below_1900_rejected(void) {
    CU_ASSERT_FALSE(isValidDate("01-01-1899"));
}

static void test_isValidDate_year_upper_boundary_2100_valid(void) {
    CU_ASSERT_TRUE(isValidDate("31-12-2100"));
}

static void test_isValidDate_year_above_2100_rejected(void) {
    CU_ASSERT_FALSE(isValidDate("01-01-2101"));
}

static void test_isValidDate_wrong_length_unpadded_rejected(void) {
    /* isValidDate expects an already-normalized 10-char string;
     * an unpadded date like "1-1-2026" (8 chars) must be rejected here -
     * normalization is normalizeDate()'s job, not this function's. */
    CU_ASSERT_FALSE(isValidDate("1-1-2026"));
}

static void test_isValidDate_empty_string_rejected(void) {
    CU_ASSERT_FALSE(isValidDate(""));
}

/* ---------- dateToJDN() / compareDates() ---------- */

static void test_compareDates_earlier_is_negative(void) {
    CU_ASSERT_TRUE(compareDates("01-01-2026", "02-01-2026") < 0);
}

static void test_compareDates_later_is_positive(void) {
    CU_ASSERT_TRUE(compareDates("02-01-2026", "01-01-2026") > 0);
}

static void test_compareDates_equal_is_zero(void) {
    CU_ASSERT_EQUAL(compareDates("15-06-2026", "15-06-2026"), 0);
}

static void test_compareDates_across_year_boundary(void) {
    CU_ASSERT_TRUE(compareDates("31-12-2025", "01-01-2026") < 0);
}

static void test_compareDates_across_leap_feb_boundary(void) {
    CU_ASSERT_TRUE(compareDates("28-02-2028", "01-03-2028") < 0);
}

int main(void) {
    if (CUE_SUCCESS != CU_initialize_registry()) {
        return CU_get_error();
    }

    CU_pSuite suite = CU_add_suite("util_date_functions", NULL, NULL);
    if (suite == NULL) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    struct { const char *name; void (*fn)(void); } tests[] = {
        { "normalizeDate: unpadded -> padded",              test_normalizeDate_unpadded },
        { "normalizeDate: already padded stays same",       test_normalizeDate_already_padded },
        { "normalizeDate: single-digit year still pads",    test_normalizeDate_single_digit_year_rejected_by_isValidDate },
        { "normalizeDate: zero day rejected",                test_normalizeDate_zero_day_rejected },
        { "normalizeDate: zero month rejected",              test_normalizeDate_zero_month_rejected },
        { "normalizeDate: negative day rejected",            test_normalizeDate_negative_day_rejected },
        { "normalizeDate: missing parts rejected",           test_normalizeDate_missing_parts_rejected },
        { "normalizeDate: garbage input rejected",           test_normalizeDate_garbage_input_rejected },
        { "normalizeDate: trailing garbage (KNOWN GAP)",     test_normalizeDate_trailing_garbage_KNOWN_GAP },

        { "isLeapYear: divisible by 4",                      test_isLeapYear_divisible_by_4 },
        { "isLeapYear: divisible by 100, not leap",          test_isLeapYear_divisible_by_100_not_leap },
        { "isLeapYear: divisible by 400, is leap",           test_isLeapYear_divisible_by_400_is_leap },
        { "isLeapYear: ordinary non-leap",                   test_isLeapYear_ordinary_non_leap },
        { "isLeapYear: far future 400-multiple",             test_isLeapYear_far_future_400_multiple },

        { "daysInMonth: Feb leap = 29",                      test_daysInMonth_february_leap },
        { "daysInMonth: Feb non-leap = 28",                  test_daysInMonth_february_non_leap },
        { "daysInMonth: April = 30",                         test_daysInMonth_april_30 },
        { "daysInMonth: January = 31",                       test_daysInMonth_january_31 },
        { "daysInMonth: month 0 invalid",                    test_daysInMonth_invalid_month_zero },
        { "daysInMonth: month 13 invalid",                   test_daysInMonth_invalid_month_13 },

        { "isValidDate: ordinary valid",                     test_isValidDate_ordinary_valid },
        { "isValidDate: Feb 31 rejected",                    test_isValidDate_feb31_rejected },
        { "isValidDate: leap Feb 29 accepted",                test_isValidDate_leap_feb29_accepted },
        { "isValidDate: non-leap Feb 29 rejected",            test_isValidDate_non_leap_feb29_rejected },
        { "isValidDate: April 31 rejected",                   test_isValidDate_april31_rejected },
        { "isValidDate: day 0 rejected",                      test_isValidDate_day_zero_rejected },
        { "isValidDate: month 0 rejected",                    test_isValidDate_month_zero_rejected },
        { "isValidDate: month 13 rejected",                   test_isValidDate_month_13_rejected },
        { "isValidDate: year 1900 lower boundary valid",      test_isValidDate_year_lower_boundary_1900_valid },
        { "isValidDate: year 1899 below boundary rejected",   test_isValidDate_year_below_1900_rejected },
        { "isValidDate: year 2100 upper boundary valid",      test_isValidDate_year_upper_boundary_2100_valid },
        { "isValidDate: year 2101 above boundary rejected",   test_isValidDate_year_above_2100_rejected },
        { "isValidDate: unpadded wrong length rejected",      test_isValidDate_wrong_length_unpadded_rejected },
        { "isValidDate: empty string rejected",               test_isValidDate_empty_string_rejected },

        { "compareDates: earlier date is negative",           test_compareDates_earlier_is_negative },
        { "compareDates: later date is positive",             test_compareDates_later_is_positive },
        { "compareDates: equal dates is zero",                test_compareDates_equal_is_zero },
        { "compareDates: across year boundary",               test_compareDates_across_year_boundary },
        { "compareDates: across leap Feb boundary",           test_compareDates_across_leap_feb_boundary },
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
