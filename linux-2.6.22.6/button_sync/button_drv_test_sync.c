#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>


int main(int argc, char **argv)
{
	int fd;
	unsigned char key_val = 0;

	// fd = open("/dev/button", O_RDWR);
	fd = open("/dev/button", O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		printf("can't open!!!\n");
		return -1;
	}

	while (1) {
		read(fd, &key_val, 1);
		printf("[button_signal] key_val: %x\n", key_val);
		sleep(5);
	}

	return 0;
}
