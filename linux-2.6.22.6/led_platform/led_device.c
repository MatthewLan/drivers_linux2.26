#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>


static void led_device_release(struct device *dev)
{

}

static struct resource led_resource[] ={
    [0] = {
        .start = 0x56000050,
        .end   = 0x56000050 + 7,
        .flags = IORESOURCE_MEM,
    },
    [1] = {
        .start = 4,
        .end = 4,
        .flags = IORESOURCE_IRQ,
    }
};

static struct platform_device led_device = {
    .name          = "jz2440_led",
    .id            = -1,
    .num_resources = ARRAY_SIZE(led_resource),
    .resource      = led_resource,
    .dev           = {
        .release = led_device_release,
    },
};

static int __init led_device_init(void)
{
    platform_device_register(&led_device);
    return 0;
}

static void __exit led_device_exit(void)
{
    platform_device_unregister(&led_device);
}

module_init(led_device_init);
module_exit(led_device_exit);
MODULE_LICENSE("GPL");
