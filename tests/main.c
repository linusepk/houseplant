#include "rebound.h"

extern void test_da(void);
extern void test_ht(void);
extern void test_pool(void);

i32_t main(void) {
    re_log_info("----- DYNAMIC ARRAY -----");
    test_da();

    re_log_info("----- HASH TABLE -----");
    test_ht();

    re_log_info("----- POOL -----");
    test_pool();

    return 0;
}
