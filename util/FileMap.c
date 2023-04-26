#include "FileMap.h"

struct FileMap map_file(const char *filename)
{
	struct FileMap ret = {};

	int fd = open(filename, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return ret;
	}

	off_t len = lseek(fd, 0, SEEK_END);

	if (len) {
		void *data = mmap(0, len, PROT_READ, MAP_PRIVATE, fd, 0);

		if (data == MAP_FAILED) {
			perror("mmap");
			return ret;
		}

		ret.data = data;
	}

	ret.fd = fd;
	ret.len = len;

	return ret;
}

void unmap_file(struct FileMap m)
{
	munmap(m.data, m.len);
}
