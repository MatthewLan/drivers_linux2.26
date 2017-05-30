#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>


int fd;

void button_signal(int signum)
{
	unsigned char key_val = 0;
	read(fd, &key_val, 1);
	printf("[button_signal] key_val: %x\n", key_val);
}

int main(int argc, char **argv)
{
	int oflag = 0;

	fd = open("/dev/button", O_RDWR);
	if (fd < 0) {
		printf("can't open!!!\n");
	}

	signal(SIGIO, button_signal);

	/* tell kernel who the signal will be sended to */
	fcntl(fd, F_SETOWN, getpid());
	/*
	 * change the fasync's flag,
	 * and kernel can and will call the driver's fasync()
	 */
	oflag = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, oflag | FASYNC);

	while (1) {
		sleep(1000);
	}

	return 0;
}
