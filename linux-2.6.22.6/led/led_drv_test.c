#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	int fd;
	int val = 1;

	fd = open("/dev/led", O_RDWR);
	if (fd < 0) {
		printf("can't open!!!\n");
	}

	if (argc != 2) {
		printf("Usage:\n");
		printf("%s <on|off>", argv[0]);
		return 0;
	}

	if (strcmp(argv[1], "on") == 0) {
		val = 1;
	} else if (strcmp(argv[1], "off") == 0) {
		val = 0;
	} else {
		printf("Param <on|off> is ERROE!!!\n");
		return 0;
	}

	write(fd, &val, sizeof(int));

	return 0;
}
