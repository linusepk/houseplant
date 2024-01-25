#include <rebound.h>

void test_str(void) {
    re_str_t rebound = re_str_lit("rebound");

    {
        u8_t arena_str[] = {'r','e','b','o','u','n','d'};
        re_str_t b = re_str(arena_str, 7);

        RE_ENSURE(re_str_cmp(rebound, b) == 0, "re_str_cmp failed.");
        re_log_info("re_str_cmp passed.");
    }

    {
        re_str_t expected = re_str_lit("ebou");
        re_str_t sub = re_str_sub(rebound, 1, 4);

        RE_ENSURE(re_str_cmp(expected, sub) == 0, "re_str_sub failed.");
        re_log_info("re_str_sub passed.");
    }

    {
        re_str_t expected = re_str_lit("reb");
        re_str_t prefix = re_str_prefix(rebound, 3);

        RE_ENSURE(re_str_cmp(expected, prefix) == 0, "re_str_prefix failed.");
        re_log_info("re_str_prefix passed.");
    }

    {
        re_str_t expected = re_str_lit("und");
        re_str_t suffix = re_str_suffix(rebound, 3);

        RE_ENSURE(re_str_cmp(expected, suffix) == 0, "re_str_suffix failed.");
        re_log_info("re_str_suffix passed.");
    }

    {
        re_str_t expected = re_str_lit("rebo");
        re_str_t chop = re_str_chop(rebound, 3);

        RE_ENSURE(re_str_cmp(expected, chop) == 0, "re_str_chop failed.");
        re_log_info("re_str_chop passed.");
    }

    {
        re_str_t expected = re_str_lit("ound");
        re_str_t skip = re_str_skip(rebound, 3);

        RE_ENSURE(re_str_cmp(expected, skip) == 0, "re_str_skip failed.");
        re_log_info("re_str_skip passed.");
    }

    {
        re_str_t expected = re_str_lit("rebound");

        re_arena_temp_t scratch = re_arena_scratch_get(NULL, 0);
        re_str_list_t *list = re_str_list_append(NULL, re_str_lit("reb"), scratch.arena);
        re_str_list_append(list, re_str_lit("ound"), scratch.arena);
        re_str_t concat = re_str_list_concat(list, scratch.arena);

        RE_ENSURE(re_str_cmp(expected, concat) == 0, "re_str_list_concat failed.");

        re_arena_scratch_release(&scratch);
        re_log_info("re_str_skip passed.");
    }
}
