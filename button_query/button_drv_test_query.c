#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	int fd;
	unsigned char key_vals[4] = {0};

	fd = open("/dev/button", O_RDWR);
	if (fd < 0) {
		printf("can't open!!!\n");
	}

	while (1) {
		read(fd, key_vals, sizeof(key_vals));
		if (!key_vals[0]) {
			printf("button-0 was pressed.\n");
		}

		if (!key_vals[1]) {
			printf("button-1 was pressed.\n");
		}

		if (!key_vals[2]) {
			printf("button-2 was pressed.\n");
		}

		if (!key_vals[3]) {
			printf("button-3 was pressed.\n");
		}
	}

	return 0;
}
