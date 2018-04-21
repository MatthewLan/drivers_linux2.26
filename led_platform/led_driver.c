#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <asm/uaccess.h>
#include <asm/io.h>


static int                  led_major;
static struct class         *led_class;
static struct class_device  *led_class_devs;

static volatile unsigned long *gpio_con;
static volatile unsigned long *gpio_dat;
static int pin;


static int led_open(struct inode *inode, struct file *file)
{
    /* output */
    *gpio_con &= ~(0x3 << (pin*2));
    *gpio_con |= (0x1 << (pin*2));

    return 0;
}

static ssize_t led_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    int val = 0;
    unsigned long ret = 0;

    ret = copy_from_user(&val, buf, count);
    if (1 == val) {
        /* turn on leds */
        *gpio_dat &= ~(1 << pin);
    } else {
        /* turn off leds */
        *gpio_dat |= (1 << pin);
    }
    return 0;
}

static struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .open  = led_open,
    .write = led_write,
};

static int led_probe(struct platform_device *pdev)
{
    struct resource  *led_resource;

    led_resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    gpio_con = ioremap(led_resource->start, led_resource->end - led_resource->start + 1);
    gpio_dat = gpio_con + 1;

    led_resource = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
    pin = led_resource->start;

    led_major = register_chrdev(0, "jz2440_led", &led_fops);
    led_class = class_create(THIS_MODULE, "jz2440_led");
    if (IS_ERR(led_class)) {
        return PTR_ERR(led_class);
    }

    led_class_devs = class_device_create(led_class, \
                                         NULL, \
                                         MKDEV(led_major, 0), \
                                         NULL, \
                                         "jz2440_led");
    if (unlikely(IS_ERR(led_class_devs))) {
        return PTR_ERR(led_class_devs);
    }
    return 0;
}

static int led_remove(struct platform_device *pdev)
{
    class_device_destroy(led_class, MKDEV(led_major, 0));
    class_destroy(led_class);
    unregister_chrdev(led_major, "jz2440_led");
    iounmap(gpio_con);
    return 0;
}

struct platform_driver led_driver = {
    .probe  = led_probe,
    .remove = led_remove,
    .driver = {
        .name = "jz2440_led",
    },
};

static int __init led_driver_init(void)
{
    platform_driver_register(&led_driver);
    return 0;
}

static void __exit led_driver_exit(void)
{
    platform_driver_unregister(&led_driver);
}

module_init(led_driver_init);
module_exit(led_driver_exit);
MODULE_LICENSE("GPL");
