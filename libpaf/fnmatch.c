#include "fnmatch.h"
#include <string.h>

int fnmatch(const char *pattern, const char *string, int flags) {
    // Very basic wildcard matcher: supports '*' only
    while (*pattern) {
        if (*pattern == '*') {
            pattern++;
            if (!*pattern) return 0;
            while (*string) {
                if (fnmatch(pattern, string, flags) == 0)
                    return 0;
                string++;
            }
            return FNM_NOMATCH;
        } else if (*pattern != *string) {
            return FNM_NOMATCH;
        } else {
            pattern++;
            string++;
        }
    }
    return *string ? FNM_NOMATCH : 0;
}