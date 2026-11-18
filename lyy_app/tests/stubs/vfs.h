#ifndef TEST_VFS_H
#define TEST_VFS_H
#include <stdio.h>
#include <stdlib.h>
typedef FILE vfs_file;
#define VFS_REGION_1 1
static inline char *find_vfs_tag(int region) { (void)region; return getenv("TEST_FLASH_PREFIX"); }
#endif
