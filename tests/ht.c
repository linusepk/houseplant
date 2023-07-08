#include "rebound.h"

usize_t hash_string(const void *data, usize_t size) {
    (void) size;
    const char *str = *(const char **) data;
    return re_fvn1a_hash(str, strlen(str));
}

void test_ht(void) {
    {
        re_ht_t(const char *, i32_t) ht;
        re_ht_create(ht, hash_string);

        char key[256] = "key";
        re_ht_set(ht, key, 3);
        RE_ENSURE(re_ht_count(ht) == 1, "re_ht_set failed.");

        memcpy(key, "yek", 3);
        re_ht_set(ht, key, 4);
        RE_ENSURE(re_ht_count(ht) == 2, "re_ht_set failed.");

        re_log_info("re_ht_set passed.");

        i32_t out = 0;
        re_ht_get(ht, (const char *) "key", out);
        RE_ENSURE(out == 3, "re_ht_get failed.");

        re_ht_get(ht, (const char *) "yek", out);
        RE_ENSURE(out == 4, "re_ht_get failed.");

        re_log_info("re_ht_get passed.");

        re_ht_remove(ht, (const char *) key);
        re_ht_get(ht, (const char *) "yek", out);
        RE_ENSURE(out == 4, "re_ht_remove failed.");
        RE_ENSURE(re_ht_count(ht) == 1, "re_ht_remove failed.");

        re_log_info("re_ht_remove passed.");

        re_ht_destroy(ht);
    }
}
