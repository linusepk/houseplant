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

    re_pool_destroy(&pool);
}
