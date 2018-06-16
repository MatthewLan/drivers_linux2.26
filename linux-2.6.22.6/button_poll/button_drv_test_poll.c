#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	int fd;
	unsigned char key_val = 0;
	int ret = 0;
	struct pollfd fds[1];

	fd = open("/dev/button", O_RDWR);
	if (fd < 0) {
		printf("can't open!!!\n");
		return -1;
	}

	fds[0].fd = fd;
	fds[0].events = POLLIN;

	while (1) {
		ret = poll(fds, 1, 5000);
		if(0 == ret) {
			printf("time out.\n");
		}
		else {
			read(fd, &key_val, sizeof(key_val));
			printf("key_val: %x\n", key_val);
		}
	}

	return 0;
}
