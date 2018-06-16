#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <asm/uaccess.h>


static int i2c_at24cxx_attach_adapter(struct i2c_adapter *adapter);
static int i2c_at24cxx_detect(struct i2c_adapter *adapter, int address, int kind);
static int i2c_at24cxx_detach_client(struct i2c_client *client);

static ssize_t i2c_at24cxx_read(struct file *file, char __user *buf, size_t size, loff_t *offset);
static ssize_t i2c_at24cxx_write(struct file *file, const char __user *buf, size_t size, loff_t *offset);

static unsigned short ignore[]      = {I2C_CLIENT_END};
static unsigned short normal_addr[] = {0x50, I2C_CLIENT_END}; /* 设备地址的7位，不包括最后的读写位 */

// static unsigned short forces_addr[][3] = {{ANY_I2C_BUS, 0x50, I2C_CLIENT_END}, NULL};

static struct i2c_client_address_data i2c_addr_data = {
    .normal_i2c = normal_addr,      /* 要发出S信号和设备地址并得到ACK信号，才能确定存在这个设备 */
    .probe      = ignore,
    .ignore     = ignore,
    // .forces     = &forces_addr,     /* 强制认为存在这个设备 */
};

/*
 * 1. 分配一个i2c_driver结构体
 * 2. 设置i2c_driver结构体
 */
static struct i2c_driver i2c_at24cxx_driver = {
    .driver = {
        .name = "at24cxx",
    },
    .attach_adapter = i2c_at24cxx_attach_adapter,
    .detach_client  = i2c_at24cxx_detach_client,
};


struct i2c_client *i2c_at24cxx_client;

static int i2c_major;
static struct class *i2c_class;

static struct file_operations i2c_at24cxx_fops = {
    .owner = THIS_MODULE,
    .read  = i2c_at24cxx_read,
    .write = i2c_at24cxx_write,
};

static ssize_t i2c_at24cxx_read(struct file *file, char __user *buf, size_t size, loff_t *offset)
{
    unsigned char addr;
    unsigned char data;
    struct i2c_msg msgs[2];
    int ret;

    if (1 != size) {
        return -EINVAL;
    }
    ret = copy_from_user(&addr, buf, 1);

    msgs[0].addr  = i2c_at24cxx_client->addr;
    msgs[0].buf   = &addr;
    msgs[0].len   = sizeof(addr);
    msgs[0].flags = 0;

    msgs[1].addr  = i2c_at24cxx_client->addr;
    msgs[1].buf   = &data;
    msgs[1].len   = sizeof(data);
    msgs[1].flags = I2C_M_RD;

    ret = i2c_transfer(i2c_at24cxx_client->adapter, msgs, 2);
    if (2 == ret) {
        ret = copy_to_user(buf, &data, 1);
        ret = 2;
    }
    else {
        ret = -EIO;
    }
    return ret;
}

static ssize_t i2c_at24cxx_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
    /*
     * buf[0] : address
     * buf[1] : data
     */
    unsigned char val[2];
    struct i2c_msg msgs[1];
    int ret;

    if (2 != size) {
        return -EINVAL;
    }
    ret = copy_from_user(val, buf, 2);

    msgs[0].addr  = i2c_at24cxx_client->addr;
    msgs[0].buf   = val;
    msgs[0].len   = sizeof(val);
    msgs[0].flags = 0;
    ret = i2c_transfer(i2c_at24cxx_client->adapter, msgs, 1);
    ret = (1 == ret) ? sizeof(val) : -EIO;
    return ret;
}


static int i2c_at24cxx_attach_adapter(struct i2c_adapter *adapter)
{
    printk("i2c_at24cxx_attach_adapter\n");
    return i2c_probe(adapter, &i2c_addr_data, i2c_at24cxx_detect);
}

static int i2c_at24cxx_detect(struct i2c_adapter *adapter, int address, int kind)
{
    printk("i2c_at24cxx_detect\n");

    i2c_at24cxx_client          = kzalloc(sizeof(struct i2c_client), GFP_KERNEL);
    i2c_at24cxx_client->addr    = address;
    i2c_at24cxx_client->adapter = adapter;
    i2c_at24cxx_client->driver  = &i2c_at24cxx_driver;
    strcpy(i2c_at24cxx_client->name, "at24cxx");
    i2c_attach_client(i2c_at24cxx_client);

    i2c_major = register_chrdev(0, "at24cxx", &i2c_at24cxx_fops);
    i2c_class = class_create(THIS_MODULE, "at24cxx");
    class_device_create(i2c_class, NULL, MKDEV(i2c_major, 0), NULL, "at24cxx");

    return 0;
}

static int i2c_at24cxx_detach_client(struct i2c_client *client)
{
    printk("i2c_at24cxx_detach_client\n");
    class_device_destroy(i2c_class, MKDEV(i2c_major, 0));
    class_destroy(i2c_class);
    unregister_chrdev(i2c_major, "at24cxx");

    i2c_detach_client(i2c_at24cxx_client);
    kfree(i2c_get_clientdata(i2c_at24cxx_client));
    return 0;
}


static int __init i2c_at24cxx_init(void)
{
    i2c_add_driver(&i2c_at24cxx_driver);
    return 0;
}

static void __exit i2c_at24cxx_exit(void)
{
    i2c_del_driver(&i2c_at24cxx_driver);
}

module_init(i2c_at24cxx_init);
module_exit(i2c_at24cxx_exit);
MODULE_LICENSE("GPL");
