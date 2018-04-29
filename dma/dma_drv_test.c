/*
 * arm-linux-gcc dma_drv_test.c -o dma_drv_test
 *
 * ./dma_drv_test nodma
 * ./dma_drv_test dma
 */

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>

#define MEM_CPY_NO_DMA      0
#define MEM_CPY_DMA         1


int main(int argc, char const *argv[])
{
    int ret = -1;
    int fd  = -1;

    do {
        if (2 != argc) {
            break;
        }
        if (-1 == (fd = open("/dev/s3c_dma", O_RDWR))) {
            break;
        }

        if (!strcmp(argv[1], "nodma")) {
            ioctl(fd, MEM_CPY_NO_DMA);
        }
        else if (!strcmp(argv[1], "dma")) {
            ioctl(fd, MEM_CPY_DMA);
        }
    } while (0);
    if (-1 != fd) {
        close(fd);
    }
    return ret;
}
