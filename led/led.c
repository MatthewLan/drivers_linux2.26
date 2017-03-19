#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <asm/irq.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>


#define  LED_NAME				"led"
/* cat /proc/devices -> [device_name]
 */
#define  LED_DEVICE_NAME		LED_NAME

/* #define  LED_MAJOR 			111
 */
int led_major;

/*
 * auto mknod by mdev
 */
static struct class 		*led_class;
static struct class_device 	*led_class_devs;

/* ls /sys/class/[device_class]
 */
#define  LED_DEVICE_CLASS		LED_NAME
/* ls /sys/class/[device_class]/[device_node]
 * -> /dev/[device_node]
 */
#define  LED_DEVICE_NODE		LED_NAME


volatile unsigned long *gpfcon = NULL;
volatile unsigned long *gpfdat = NULL;

#define GPFCON_PHY_ADDR			0x56000050
#define GPFCON_SIZE				16


int led_open(struct inode *inode, struct file *file)
{
	/* printk("[led_open] called\n");
	 */
	
	/* GPF4,5,6 : output */
	*gpfcon &= ~((0x3 << 8) | (0x3 << 10) | (0x3 << 12));
	*gpfcon |= ((0x1 << 8) | (0x1 << 10) | (0x1 << 12));
	
	return 0;
}

ssize_t led_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	int val = 0;
	
	/* printk("[led_write] called\n");
	 */
	
	copy_from_user(&val, buf, count);
	if (1 == val) {
		/* turn on leds */
		*gpfdat &= ~((1 << 4) | (1 << 5) | (1 << 6));
	} else {
		/* turn off leds */
		*gpfdat |= ((1 << 4) | (1 << 5) | (1 << 6));
	}
	
	return 0;
}

struct file_operations led_fos = {
	.owner = THIS_MODULE,
	.open  = led_open,
	.write = led_write,
};


static int __init led_init(void)
{
	/* register_chrdev(LED_MAJOR, LED_DEVICE_NAME, &led_fos);
	 */
	led_major = register_chrdev(0, LED_DEVICE_NAME, &led_fos);
	
	led_class = class_create(THIS_MODULE, LED_DEVICE_CLASS);
	if (IS_ERR(led_class)) {
		return PTR_ERR(led_class);
	}
	
	led_class_devs = class_device_create(led_class, \
											NULL, \
											MKDEV(led_major, 0), \
											NULL, \
											LED_DEVICE_NODE);
	if (unlikely(IS_ERR(led_class_devs))) {
		return PTR_ERR(led_class_devs);
	}
	
	gpfcon = (unsigned long *)ioremap(GPFCON_PHY_ADDR, GPFCON_SIZE);
	gpfdat = gpfcon + 1;
	
	return 0;
}

static void __exit led_exit(void)
{
	unregister_chrdev(led_major, LED_DEVICE_NAME);
	
	class_device_unregister(led_class_devs);
	class_destroy(led_class);
	
	iounmap(gpfcon);
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");

