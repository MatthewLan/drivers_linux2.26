#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	int fd;
	unsigned char key_val = 0;

	fd = open("/dev/button", O_RDWR);
	if (fd < 0) {
		printf("can't open!!!\n");
	}

	while (1) {
		read(fd, key_val, sizeof(key_val));
		printf("key_val: %x\n", key_val);
	}

	return 0;
}
