/* dirvers/input/keyboard/gpio_keys.c */

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
#include <linux/irq.h>
#include <linux/gpio_keys.h>

#include <asm/gpio.h>


struct pin_desc_struct {
    int          irq;
    char         *name;
    unsigned int pin;
    unsigned int key_val;
};

struct pin_desc_struct pin_desc[4] = {
    {IRQ_EINT0,  "S2", S3C2410_GPF0,  KEY_L         },
    {IRQ_EINT2,  "S3", S3C2410_GPF2,  KEY_S         },
    {IRQ_EINT11, "S4", S3C2410_GPG3,  KEY_ENTER     },
    {IRQ_EINT19, "S5", S3C2410_GPG11, KEY_LEFTSHIFT }
};

static struct input_dev *button_dev;

/* 定时器 */
static struct timer_list      button_timer;
static struct pin_desc_struct *irq_pd = NULL;


static irqreturn_t buttons_irq(int irq, void *dev_id)
{
    /* 10ms */
    irq_pd = (struct pin_desc_struct *)dev_id;
    mod_timer(&button_timer, jiffies + HZ/100);
    return IRQ_RETVAL(IRQ_HANDLED);
}

static void button_timer_expire(unsigned long data)
{
    struct pin_desc_struct *pindesc = irq_pd;
    unsigned int pin_val = 0;

    if (!irq_pd) {
        return;
    }

    pin_val = s3c2410_gpio_getpin(pindesc->pin);

#define BUTTON_RELEASE      0
#define BUTTON_PRESS        1
    if(pin_val) {
        input_event(button_dev, EV_KEY, pindesc->key_val, BUTTON_RELEASE);
        input_sync(button_dev);
    }
    else {
        input_event(button_dev, EV_KEY, pindesc->key_val, BUTTON_PRESS);
        input_sync(button_dev);
    }
}

static int __init button_init(void)
{
    int i = 0;
    button_dev = input_allocate_device();

    /* event */
    set_bit(EV_KEY, button_dev->evbit);
    set_bit(EV_REP, button_dev->evbit);
    /* key */
    set_bit(KEY_L, button_dev->keybit);
    set_bit(KEY_S, button_dev->keybit);
    set_bit(KEY_ENTER, button_dev->keybit);
    set_bit(KEY_LEFTSHIFT, button_dev->keybit);
    /*register device */
    input_register_device(button_dev);

    /* 定时器 */
    init_timer(&button_timer);
    button_timer.function = button_timer_expire;
    add_timer(&button_timer);

    /* hardware handle */
    for (; i < 4; i++) {
        request_irq(pin_desc[i].irq, \
                    buttons_irq, \
                    IRQT_BOTHEDGE, \
                    pin_desc[i].name, \
                    &pin_desc[i]);
    }
    return 0;
}

static void __exit button_exit(void)
{
    int i = 0;
    for (; i < 4; i++) {
        free_irq(pin_desc[i].irq, &pin_desc[i]);
    }
    del_timer(&button_timer);
    input_unregister_device(button_dev);
    input_free_device(button_dev);
}

module_init(button_init);
module_exit(button_exit);
MODULE_LICENSE("GPL");
