#include "rebound.h"

void test_da(void) {
    i32_t arr[] = {2, 3, 4, 5, 6, 7};

    {
        i32_t expected[] = {5, 4, 3, 2, 1, 0};

        re_da_t(i32_t) da = NULL;
        re_da_create(da);

        for (u32_t i = 0; i < 6; i++) {
            re_da_insert(da, i, 0);
        }

        RE_ENSURE(memcmp(da, expected, sizeof(expected)) == 0, "re_da_insert failed.");
        RE_ENSURE(re_da_count(da) == 6, "re_da_insert failed.");

        re_da_destroy(da);

        re_log_info("re_da_insert passed.");
    }

    {
        i32_t expected[] = {5, 0, 1, 2, 3, 4};

        re_da_t(i32_t) da = NULL;
        re_da_create(da);

        for (u32_t i = 0; i < 6; i++) {
            re_da_insert_fast(da, i, 0);
        }

        RE_ENSURE(memcmp(da, expected, sizeof(expected)) == 0, "re_da_insert_fast failed.");
        RE_ENSURE(re_da_count(da) == 6, "re_da_insert_fast failed.");

        re_da_destroy(da);

        re_log_info("re_da_insert_fast passed.");
    }

    {
        i32_t expected[] = {0, 1, 2, 3, 4, 5};

        re_da_t(i32_t) da = NULL;
        re_da_create(da);

        for (u32_t i = 0; i < 6; i++) {
            re_da_push(da, i);
        }

        RE_ENSURE(memcmp(da, expected, sizeof(expected)) == 0, "re_da_push failed.");
        RE_ENSURE(re_da_count(da) == 6, "re_da_push failed.");

        re_da_destroy(da);

        re_log_info("re_da_push passed.");
    }

    {
        i32_t expected[] = {2, 3, 4, 5, 6, 7};

        re_da_t(i32_t) da = NULL;
        re_da_create(da);

        re_da_insert_arr(da, &arr[re_arr_len(arr) / 2], re_arr_len(arr) / 2, 0);
        re_da_insert_arr(da, arr, re_arr_len(arr) / 2, 0);

        RE_ENSURE(memcmp(da, expected, sizeof(expected)) == 0, "re_da_insert_arr failed.");
        RE_ENSURE(re_da_count(da) == 6, "re_da_insert_arr failed.");

        re_da_destroy(da);

        re_log_info("re_da_insert_arr passed.");
    }

    {
        i32_t expected[] = {2, 3, 4, 5, 6, 7};

        re_da_t(i32_t) da = NULL;
        re_da_create(da);

        re_da_push_arr(da, arr, re_arr_len(arr) / 2);
        re_da_push_arr(da, &arr[re_arr_len(arr) / 2], re_arr_len(arr) / 2);

        RE_ENSURE(memcmp(da, expected, sizeof(expected)) == 0, "re_da_push_arr failed.");

        re_da_destroy(da);

        re_log_info("re_da_push_arr passed.");
    }

    {
        i32_t expected[] = {2, 4, 5, 6, 7};

        re_da_t(i32_t) da = NULL;
        re_da_create(da);
        re_da_push_arr(da, arr, re_arr_len(arr));

        i32_t out = 0;
        re_da_remove(da, 1, &out);

        RE_ENSURE(memcmp(da, expected, sizeof(expected)) == 0, "re_da_remove failed.");
        RE_ENSURE(out == 3, "re_da_remove failed.");
        RE_ENSURE(re_da_count(da) == 5, "re_da_remove_fast failed.");

        re_da_destroy(da);

        re_log_info("re_da_remove passed.");
    }

    {
        i32_t expected[] = {2, 7, 4, 5, 6};

        re_da_t(i32_t) da = NULL;
        re_da_create(da);
        re_da_push_arr(da, arr, re_arr_len(arr));

        i32_t out = 0;
        re_da_remove_fast(da, 1, &out);

        RE_ENSURE(memcmp(da, expected, sizeof(expected)) == 0, "re_da_remove_fast failed.");
        RE_ENSURE(out == 3, "re_da_remove_fast failed.");
        RE_ENSURE(re_da_count(da) == 5, "re_da_remove_fast failed.");

        re_da_destroy(da);

        re_log_info("re_da_remove_fast passed.");
    }

    {
        i32_t expected[] = {2, 3, 4, 5, 6};

        re_da_t(i32_t) da = NULL;
        re_da_create(da);
        re_da_push_arr(da, arr, re_arr_len(arr));

        i32_t out = 0;
        re_da_pop(da, &out);

        RE_ENSURE(memcmp(da, expected, sizeof(expected)) == 0, "re_da_pop failed.");
        RE_ENSURE(out == 7, "re_da_pop failed.");
        RE_ENSURE(re_da_count(da) == 5, "re_da_pop failed.");

        re_da_destroy(da);

        re_log_info("re_da_pop passed.");
    }

    {
        i32_t expected[] = {5, 6, 7};
        i32_t expected_out[] = {2, 3, 4};

        re_da_t(i32_t) da = NULL;
        re_da_create(da);
        re_da_push_arr(da, arr, re_arr_len(arr));

        i32_t out[3] = {0};
        re_da_remove_arr(da, 3, 0, &out);

        RE_ENSURE(memcmp(da, expected, sizeof(expected)) == 0, "re_da_remove_arr failed.");
        RE_ENSURE(memcmp(out, expected_out, sizeof(expected_out)) == 0, "re_da_remove_arr failed.");
        RE_ENSURE(re_da_count(da) == 3, "re_da_remove_arr failed.");

        re_da_destroy(da);

        re_log_info("re_da_remove_arr passed.");
    }

    {
        i32_t expected[] = {2, 3, 4};
        i32_t expected_out[] = {5, 6, 7};

        re_da_t(i32_t) da = NULL;
        re_da_create(da);
        re_da_push_arr(da, arr, re_arr_len(arr));

        i32_t out[3] = {0};
        re_da_pop_arr(da, 3, &out);

        RE_ENSURE(memcmp(da, expected, sizeof(expected)) == 0, "re_da_pop_arr failed.");
        RE_ENSURE(memcmp(out, expected_out, sizeof(expected_out)) == 0, "re_da_pop_arr failed.");
        RE_ENSURE(re_da_count(da) == 3, "re_da_pop_arr failed.");

        re_da_destroy(da);

        re_log_info("re_da_pop_arr passed.");
    }

    {
        i32_t expected = 7;

        re_da_t(i32_t) da = NULL;
        re_da_create(da);
        re_da_push_arr(da, arr, re_arr_len(arr));

        i32_t result = re_da_last(da);
        RE_ENSURE(result == expected, "re_da_last failed.");

        re_da_destroy(da);

        re_log_info("re_da_last passed.");
    }
}
