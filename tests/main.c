#include "rebound.h"

extern void test_da(void);
extern void test_ht(void);
extern void test_pool(void);

i32_t main(void) {
    re_init();

    re_log_info("----- DYNAMIC ARRAY -----");
    test_da();

    re_log_info("----- HASH TABLE -----");
    test_ht();

    re_log_info("----- POOL -----");
    test_pool();

    re_terminate();
    return 0;
}
