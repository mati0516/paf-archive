#ifndef FNMATCH_H
#define FNMATCH_H

#define FNM_NOMATCH 1

int fnmatch(const char *pattern, const char *string, int flags);

#endif // FNMATCH_H