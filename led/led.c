#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
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


int led_open(struct inode *inode, struct file *file)
{
	printk("[led_open] called\n");
	return 0;
}

ssize_t led_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	printk("[led_write] called\n");
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
	
	return 0;
}

static void __exit led_exit(void)
{
	unregister_chrdev(led_major, LED_DEVICE_NAME);
	
	class_device_unregister(led_class_devs);
	class_destroy(led_class);
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");

