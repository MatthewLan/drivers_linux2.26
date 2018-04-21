#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>

#define  BUTTON_NAME                "button"
/**
 * cat /proc/devices -> [device_name]
 */
#define  BUTTON_DEVICE_NAME         BUTTON_NAME

int button_major;

/**
 * auto mknod by mdev
 */
static struct class         *button_class;
static struct class_device  *button_class_devs;

/**
 * ls /sys/class/[device_class]
 */
#define  BUTTON_DEVICE_CLASS    BUTTON_NAME
/**
 * ls /sys/class/[device_class]/[device_node]
 * -> /dev/[device_node]
 */
#define  BUTTON_DEVICE_NODE     BUTTON_NAME

volatile unsigned long *gpfcon;
volatile unsigned long *gpfdat;

#define  GPFCON_PHY_ADDR            0x56000050
#define  GPFCON_SIZE                16

volatile unsigned long *gpgcon;
volatile unsigned long *gpgdat;

#define  GPGCON_PHY_ADDR            0x56000060
#define  GPGCON_SIZE                16

#define  KEY_VAL(regval, pos)   ((regval) & (1 << (pos)) ? 1 : 0)


struct pin_desc_struct {
    unsigned int pin;
    unsigned int key_val;
};

/*
 * pressed : 0x01, 0x02, 0x03, 0x04
 * released: 0x81, 0x82, 0x83, 0x84
 */
struct pin_desc_struct pin_desc[4] = {
    {S3C2410_GPF0,  0x01},
    {S3C2410_GPF2,  0x02},
    {S3C2410_GPG3,  0x03},
    {S3C2410_GPG11, 0x04}
};

static unsigned char key_val;

static DECLARE_WAIT_QUEUE_HEAD(button_waitq);
/* 中断事件标志，中断服务程序将它置1； read函数将它清零 */
static volatile int ev_press = 0;
/* use for signal */
static struct fasync_struct *button_async;


// #define SYNC_ATOMIC
#define SYNC_SEMAPHORE

#if   defined SYNC_ATOMIC
/*
// define variate
atomic_t v = ATOMIC_INIT(0);
// get variate values
atomic_read(atomic_t *v);
// variate increase 1
void atomic_inc(atomic_t *v);
// variate decrease 1
void atomic_dec(atomic_t *v);
// test the result of variate decreasing 1
int  atomic_dec_and_test(atomic_t *v);
*/
static atomic_t can_open = ATOMIC_INIT(1);

#elif defined SYNC_SEMAPHORE
/*
 * linux kernel 2.6.22.6
// define variate
struct semaphore sem;
// initialize variate
void sema_init(struct semaphore *sem, int val);
void init_MUTEX(struct semaphore *sem);
static DECLARE_MUTEX(button_lock);
// get semaphore variate
void down(struct semaphore *sem);
int  down_interruptible(struct semaphore *sem);
int  down_trylock(struct semaphore *sem);
// release semaphore variate
void up(struct semaphore *sem);
*/
static DECLARE_MUTEX(button_lock);

#endif


static irqreturn_t buttons_irq(int irq, void *dev_id)
{
    struct pin_desc_struct *pindesc = (struct pin_desc_struct *)dev_id;
    unsigned int pin_val = 0;

    pin_val = s3c2410_gpio_getpin(pindesc->pin);

    if(pin_val) {
        key_val = 0x80 | pindesc->key_val;
    }
    else {
        key_val = pindesc->key_val;
    }

    ev_press = 1;
    wake_up_interruptible(&button_waitq);

    /* to send signal: SIGIO */
    kill_fasync(&button_async, SIGIO, POLL_IN);

    return IRQ_RETVAL(IRQ_HANDLED);
}

int button_open(struct inode *inode, struct file *file)
{
    int ret = -1;
#if   defined SYNC_ATOMIC
    if(!atomic_dec_and_test(&can_open)) {
        atomic_inc(&can_open);
        return -EBUSY;
    }

#elif defined SYNC_SEMAPHORE
    /* non block */
    if(O_NONBLOCK & file->f_flags) {
        if(down_trylock(&button_lock)) {
            return -EBUSY;
        }
    }
    /* block */
    else {
        down(&button_lock);
    }

#endif

    /**
     * GPF0,2: irq
     */
    ret = request_irq(IRQ_EINT0, buttons_irq, IRQT_BOTHEDGE, "S2", &pin_desc[0]);
    ret = request_irq(IRQ_EINT2, buttons_irq, IRQT_BOTHEDGE, "S3", &pin_desc[1]);
    /**
     * GPG3,11: irq
     */
    ret = request_irq(IRQ_EINT11, buttons_irq, IRQT_BOTHEDGE, "S4", &pin_desc[2]);
    ret = request_irq(IRQ_EINT19, buttons_irq, IRQT_BOTHEDGE, "S5", &pin_desc[3]);

    return ret;
}

ssize_t button_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
    unsigned long ret = -1;
    if (1 !=  size) {
        return -EINVAL;
    }

#if defined SYNC_SEMAPHORE
    /* non block */
    if(O_NONBLOCK & file->f_flags) {
        if(!ev_press) {
            return -EINVAL;
        }
    }
    /* block */
    else {
        wait_event_interruptible(button_waitq, ev_press);
    }

#else
    /* 如果没有按键动作，则休眠 */
    wait_event_interruptible(button_waitq, ev_press);

#endif

    ret = copy_to_user(buf, &key_val, sizeof(key_val));
    ev_press = 0;

    return sizeof(key_val);
}

int button_close(struct inode *inode, struct file *file)
{
    free_irq(IRQ_EINT0,  &pin_desc[0]);
    free_irq(IRQ_EINT2,  &pin_desc[1]);
    free_irq(IRQ_EINT11, &pin_desc[2]);
    free_irq(IRQ_EINT19, &pin_desc[3]);

#if   defined SYNC_ATOMIC
    atomic_dec(&can_open);

#elif defined SYNC_SEMAPHORE
    up(&button_lock);

#endif

    return 0;
}

unsigned int button_poll(struct file *file, struct poll_table_struct *wait)
{
    unsigned int mask = 0;
    poll_wait(file, &button_waitq, wait);

    if(ev_press) {
        mask |= POLLIN |POLLRDNORM;
    }
    return mask;
}

int button_fasync(int fd, struct file *file, int on)
{
    return fasync_helper(fd, file, on, &button_async);
}

struct file_operations button_fos = {
    .owner      = THIS_MODULE,
    .open       = button_open,
    .read       = button_read,
    .release    = button_close,
    .poll       = button_poll,
    .fasync     = button_fasync,
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

    class_device_destroy(button_class, MKDEV(button_major,0));
    class_destroy(button_class);

    iounmap(gpfcon);
    iounmap(gpgcon);
}

module_init(button_init);
module_exit(button_exit);

MODULE_LICENSE("GPL");
