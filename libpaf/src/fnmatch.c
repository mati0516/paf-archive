#include "fnmatch.h"
#include <stddef.h>

int fnmatch(const char *pattern, const char *string, int flags) {
    (void)flags;
    const char *star = NULL;
    const char *ss = string;

    while (*string) {
        if (*pattern == '*') {
            star = ++pattern;
            ss = string;
            if (!*pattern) return 0;
        } else if (*pattern == *string) {
            pattern++;
            string++;
        } else if (star) {
            pattern = star;
            string = ++ss;
        } else {
            return FNM_NOMATCH;
        }
    }

    while (*pattern == '*') pattern++;
    return *pattern ? FNM_NOMATCH : 0;
}
