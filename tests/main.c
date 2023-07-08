#include "rebound.h"

extern void test_da(void);
extern void test_ht(void);

i32_t main(void) {
    re_log_info("----- DYNAMIC ARRAY -----");
    test_da();

    re_log_info("----- HASH TABLE -----");
    test_ht();

    return 0;
}
