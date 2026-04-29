#ifndef DIRENT_H
#define DIRENT_H

#ifdef _WIN32
#include <windows.h>
#include <string.h>
#include <stdlib.h>

typedef struct dirent {
    char d_name[MAX_PATH];
} dirent;

typedef struct DIR {
    HANDLE hFind;
    WIN32_FIND_DATA fd;
    dirent entry;
    int first;
} DIR;

static DIR* opendir(const char* path) {
    DIR* dir = (DIR*)malloc(sizeof(DIR));
    char search_path[MAX_PATH];
    sprintf(search_path, "%s\\*", path);
    dir->hFind = FindFirstFile(search_path, &dir->fd);
    if (dir->hFind == INVALID_HANDLE_VALUE) {
        free(dir);
        return NULL;
    }
    dir->first = 1;
    return dir;
}

static struct dirent* readdir(DIR* dir) {
    if (!dir->first) {
        if (!FindNextFile(dir->hFind, &dir->fd)) return NULL;
    }
    dir->first = 0;
    strcpy(dir->entry.d_name, dir->fd.cFileName);
    return &dir->entry;
}

static void closedir(DIR* dir) {
    if (dir) {
        FindClose(dir->hFind);
        free(dir);
    }
}

#else
#include <dirent.h>
#endif

#endif
