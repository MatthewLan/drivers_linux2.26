#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>

#define  BUTTON_NAME				"button"
/**
 * cat /proc/devices -> [device_name]
 */
#define  BUTTON_DEVICE_NAME			BUTTON_NAME

int button_major;

/**
 * auto mknod by mdev
 */
static struct class 		*button_class;
static struct class_device 	*button_class_devs;

/**
 * ls /sys/class/[device_class]
 */
#define  BUTTON_DEVICE_CLASS	BUTTON_NAME
/**
 * ls /sys/class/[device_class]/[device_node]
 * -> /dev/[device_node]
 */
#define  BUTTON_DEVICE_NODE		BUTTON_NAME

volatile unsigned long *gpfcon;
volatile unsigned long *gpfdat;

#define  GPFCON_PHY_ADDR			0x56000050
#define  GPFCON_SIZE				16

volatile unsigned long *gpgcon;
volatile unsigned long *gpgdat;

#define  GPGCON_PHY_ADDR			0x56000060
#define  GPGCON_SIZE				16

#define  KEY_VAL(regval, pos)	((regval) & (1 << (pos)) ? 1 : 0)


int button_open(struct inode *inode, struct file *file)
{
	/**
	 * GPF0,2: input
	 */
	*gpfcon &= ~((0x3 << 2) | (0x3 << 4));
	/**
	 * GPG3,11: input
	 */
	*gpgcon &= ~((0x3 << 6) | (0x3 << 22));

	return 0;
}

ssize_t button_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	if (4 !=  size) {
		return -EINVAL;
	}

	unsigned char key_vals[4] = {0};

	key_vals[0] = KEY_VAL(*gpfdat, 0);
	key_vals[1] = KEY_VAL(*gpfdat, 2);

	key_vals[2] = KEY_VAL(*gpgdat, 3);
	key_vals[4] = KEY_VAL(*gpgdat, 11);

	copy_to_user(buf, key_vals, sizeof(key_vals));

	return sizeof(key_vals);
}

struct file_operations button_fos = {
	.owner = THIS_MODULE,
	.open  = button_open,
	.read  = button_read,
};

static int __init button_init(void)
{
	button_major = register_chrdev(0, BUTTON_DEVICE_NAME, &button_fos);

	button_class = class_create(THIS_MODULE, BUTTON_DEVICE_CLASS);
	if (IS_ERR(button_class)) {
		return PTR_ERR(button_class);
	}

	button_class_devs = class_device_create(button_class, \
											NULL, \
											MKDEV(button_major,0), \
											NULL, \
											BUTTON_DEVICE_NODE);
	if (unlikely(IS_ERR(button_class_devs))) {
		return PTR_ERR(button_class_devs);
	}

	gpfcon = (volatile unsigned long *)ioremap(GPFCON_PHY_ADDR, GPFCON_SIZE);
	gpfdat = gpfcon + 1;

	gpgcon = (volatile unsigned long *)ioremap(GPGCON_PHY_ADDR, GPGCON_SIZE);
	gpgdat = gpgcon + 1;

	return 0;
}

static void __exit button_exit(void)
{
	unregister_chrdev(button_major, BUTTON_DEVICE_NAME);

	class_device_unregister(button_class_devs);
	class_destroy(button_class);

	iounmap(gpfcon);
	iounmap(gpgcon);
}

module_init(button_init);
module_exit(button_exit);

MODULE_LICENSE("GPL");
