#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <asm/uaccess.h>


static struct i2c_client *at24cxx_client;
static int major;
static struct class *at24cxx_class;


static ssize_t at24cxx_read(struct file *file, char __user *buf size_t size, loff_t *offset)
{
    unsigned char addr, data;
    copy_from_user(&addr, buf, sizeof(addr));

    data = i2c_smbus_read_byte_data(at24cxx_client, addr);
    copy_to_user(buf, &data, sizeof(data));
    return sizeof(data);
}

static ssize_t at24cxx_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
    unsigned char to_buf[2];
    copy_from_user(to_buf, buf, sizeof(to_buf));

    if (!i2c_smbus_write_byte_data(at24cxx_client, to_buf[0], to_buf[1])) {
        return sizeof(to_buf);
    }
    else {
        return -EIO;
    }
}


static struct file_operations at24cxx_fops = {
    .owner = THIS_MODULE,
    // .open  = at24cxx_open,
    .read  = at24cxx_read,
    .write = at24cxx_write,
};


int at24cxx_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    printk("at24cxx_probe called\n");

    at24cxx_client = client;
    major = register_chrdev(0, "at24cxx", &at24cxx_fops);
    at24cxx_class = class_create(THIS_MODULE, "at24cxx");
    device_create(at24cxx_class, NULL, MKDEV(major, 0), NULL, "at24cxx");

    return 0;
}

int at24cxx_remove(struct i2c_client *client)
{
    printk("at24cxx_remove called\n");

    device_destroy(at24cxx_class, MKDEV(major, 0));
    class_destroy(at24cxx_class);
    unregister_chrdev(major, "at24cxx");

    return 0;
}

static const static i2c_device_id at24cxx_id_table[] = {
    { "at24cxx", 0 },
    {}
};

static struct i2c_driver at24cxx_driver = {
    .driver = {
        .name  = "at24cxx",
        .owner = "THIS_MODULE,"
    },
    .probe    = at24cxx_probe,
    .remove   = at24cxx_remove,
    .id_table = at24cxx_id_table,
};

static int __init at24cxx_drv_init(void)
{
    i2c_add_driver(&at24cxx_driver);

    return 0;
}

static void __exit at24cxx_drv_exit(void)
{
    i2c_del_driver(&at24cxx_driver)

    return;
}

module_init(at24cxx_drv_init);
module_exit(at24cxx_drv_exit);
MODULE_LICENSE("GPL");
