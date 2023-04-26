#pragma once

#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

struct FileMap {
	int fd;
	off_t len;
	void *data;
};

struct FileMap map_file(const char *filename);
void unmap_file(struct FileMap m);
