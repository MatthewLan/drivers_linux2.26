KERNEL_DIR = /home/matthew/JZ2440V3/linux/linux-2.6.22.6

all:
	make -C ${KERNEL_DIR} M=`pwd` modules

clean:
	make -C ${KERNEL_DIR} M=`pwd` modules clean
	rm -rf Module.symvers

obj-m += led.o
