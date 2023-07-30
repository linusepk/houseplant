#include "rebound.h"

void test_pool(void) {
    re_pool_t *pool = re_pool_create(16, sizeof(u32_t));

    re_pool_handle_t handle = re_pool_new(pool);
    RE_ENSURE(re_pool_handle_valid(handle), "re_pool_new failed");
    re_log_info("re_pool_new passed.");

    u32_t *value = re_pool_get_ptr(handle);
    *value = 32;
    RE_ENSURE(value != NULL, "re_pool_get_ptr failed");

    re_pool_delete(handle);
    RE_ENSURE(!re_pool_handle_valid(handle), "re_pool_delete failed");
    re_log_info("re_pool_delete passed.");

    for (u32_t i = 0; i < 8; i++) {
        re_pool_new(pool);
    }
    u32_t count = 0;
    for (re_pool_iter_t iter = re_pool_iter_new(pool); re_pool_iter_valid(iter); re_pool_iter_next(&iter)) {
        count++;
    }
    RE_ENSURE(count == 8, "re_pool_iter failed");
    re_log_info("re_pool_iter passed");

    re_pool_destroy(&pool);
}
