#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

/*
 * i2c_test_app r addr
 * i2c_test_app w addr val
 */

const char pathname[] = "/dev/at24cxx";


int main(int argc, char const *argv[])
{
    int fd = -1;
    unsigned char buf[2];

    if (0 > (fd = open(pathname, O_RDWR))) {
        return 1;
    }

    if (3 > argc) {
        return 1;
    }

    if (!strcmp(argv[1], "r")) {
        buf[0] = strtoul(argv[2], NULL, 0);
        read(fd, buf, 1);
        printf("data: %c, %d, %#2x\n", buf[0], buf[0], buf[0]);
    }
    else if (!strcmp(argv[1], "w")) {
        if (4 > argc) {
            return 1;
        }
        buf[0] = strtoul(argv[2], NULL, 0);
        buf[1] = strtoul(argv[3], NULL, 0);
        write(fd, buf, 2);
    }
    else {
        return 1;
    }
    return 0;
}
