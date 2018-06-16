/*
* register_chrdev_region()
* cdev_init()
* cdev_add()
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>


#define CHRDEV_COUNT        1

static int                  chrdev_major   = 0;
static struct cdev          chrdev_cdev;
static struct class         *chrdev_cls    = NULL;
static struct class_device  *chrdev_clsdev = NULL;


int chrdev_open(struct inode *inode, struct file *file)
{
    printk("chrdev_open called\n");
    return 0;
}


static struct file_operations chrdev_fops = {
    .owner      = THIS_MODULE,
    .open       = chrdev_open,
};


static int __init chrdev_init(void)
{
    int   ret = -1;
    int   i   = 0;
    dev_t devid;

    if (chrdev_major) {
        devid = MKDEV(chrdev_major, 0);
        ret   = register_chrdev_region(devid, CHRDEV_COUNT, "chrdev");
    }
    else {
        ret          = alloc_chrdev_region(&devid, 0, CHRDEV_COUNT, "chrdev");
        chrdev_major = MAJOR(devid);
    }
    if (0 > ret) {
        goto error_region;
    }
    cdev_init(&chrdev_cdev, &chrdev_fops);
    cdev_add(&chrdev_cdev, devid, CHRDEV_COUNT);

    chrdev_cls = class_create(THIS_MODULE, "chrdev");
    if (IS_ERR(chrdev_cls)) {
        ret = PTR_ERR(chrdev_cls);
        goto error_cls;
    }
    for (; i < CHRDEV_COUNT; i++) {
        chrdev_clsdev = class_device_create(chrdev_cls, NULL \
                        , MKDEV(chrdev_major, i), NULL, "chrdev_%d", i);
        if (IS_ERR(chrdev_clsdev)) {
            ret = PTR_ERR(chrdev_clsdev);
            goto error_clsdev;
        }
    }

error_clsdev:
    for (i = 0; i < CHRDEV_COUNT; i++) {
        class_device_destroy(chrdev_cls, MKDEV(chrdev_major, i));
    }
    class_destroy(chrdev_cls);
error_cls:
    cdev_del(&chrdev_cdev);
    unregister_chrdev_region(MKDEV(chrdev_major, 0), CHRDEV_COUNT);
error_region:
    return ret;
}

static void __exit chrdev_exit(void)
{
    int i = 0;
    for (; i < CHRDEV_COUNT; i++) {
        class_device_destroy(chrdev_cls, MKDEV(chrdev_major, i));
    }
    class_destroy(chrdev_cls);

    cdev_del(&chrdev_cdev);
    unregister_chrdev_region(MKDEV(chrdev_major, 0), CHRDEV_COUNT);
}

module_init(chrdev_init);
module_exit(chrdev_exit);

MODULE_LICENSE("GPL");
