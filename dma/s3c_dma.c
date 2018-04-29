#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/dma-mapping.h>
#include <asm/irq.h>
#include <asm/io.h>


#define MEM_CPY_NO_DMA      0
#define MEM_CPY_DMA         1
#define BUF_SIZE            (512*1024)

#define DMA0_BASE_ADDR      0x4b000000
#define DMA1_BASE_ADDR      0x4b000040
#define DMA2_BASE_ADDR      0x4b000080
#define DMA3_BASE_ADDR      0x4b0000c0


struct s3c_dma_regs {
    unsigned long disrc;
    unsigned long disrcc;
    unsigned long didst;
    unsigned long didstc;
    unsigned long dcon;
    unsigned long dstat;
    unsigned long dcsrc;
    unsigned long dcdst;
    unsigned long dmasktrig;
};


static int                  dma_major   = 0;
static struct class         *dma_cls    = NULL;
static struct class_device  *dma_clsdev = NULL;

static char *src     = NULL;
static u32  src_phys = 0;
static char *dst     = NULL;
static u32  dst_phys = 0;

static volatile struct s3c_dma_regs *dma_regs = NULL;

static DECLARE_WAIT_QUEUE_HEAD(dma_waitq);
/* 中断事件标志，中断服务程序将它置1； ioctl函数将它清零 */
static volatile int ev_dma = 0;


static irqreturn_t dma_irq(int irq, void *dev_id)
{
    ev_dma = 1;
    /* 唤醒 */
    wake_up_interruptible(&dma_waitq);
    return IRQ_HANDLED;
}

int dma_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    memset(src, 0xAA, BUF_SIZE);
    memset(src, 0x55, BUF_SIZE);

    switch (cmd) {
    case MEM_CPY_NO_DMA: {
        int i = 0;
        for (; i < BUF_SIZE; i++) {
            dst[i] = src[i];
        }
        if (!memcpy(dst, src, BUF_SIZE)) {
            printk("MEM_CPY_NO_DMA OK\n");
        }
        else {
            printk("MEM_CPY_NO_DMA ERROR\n");
        }
        break;
    }
    case MEM_CPY_DMA: {
        ev_dma = 0;
        /* 1. 数据的源、目的、长度 */
        /* 源的物理地址 */
        dma_regs->disrc  = src_phys;
        /* 源位于AHB总线，源地址递增 */
        dma_regs->disrcc = (0 << 1) | (0 << 0);
        /* 目的的物理地址 */
        dma_regs->didst  = dst_phys;
        /* 目的位于AHB总线，源地址递增 */
        dma_regs->didstc = (0 << 2) | (0 << 1) | (0 << 0);
        /* 使能中断，单次传输，软件触发 */
        dma_regs->dcon   = (1 << 30) | (1 << 29) | (0 << 28) | (1 << 27) \
                            | (0 << 23) | (0 << 20) | (BUF_SIZE << 0);

        /* 2. 启动DMA */
        dma_regs->dmasktrig = (1 << 1) | (1 << 0);

        /* 3. 休眠 */
        wait_event_interruptible(dma_waitq, ev_dma);

        if (!memcpy(dst, src, BUF_SIZE)) {
            printk("MEM_CPY_DMA OK\n");
        }
        else {
            printk("MEM_CPY_DMA ERROR\n");
        }
        break;
    }
    }
    return 0;
}

static struct file_operations dma_fops = {
    .owner = THIS_MODULE,
    .ioctl = dma_ioctl,
};


static int __init dma_init(void)
{
    int ret = -1;

    if (request_irq(IRQ_DMA3, dma_irq, IRQT_BOTHEDGE, "s3c_dma", NULL)) {
        ret = -EBUSY;
        goto err_irq;
    }
    if (!(src = dma_alloc_writecombine(NULL, BUF_SIZE, &src_phys, GFP_KERNEL))) {
        ret = -ENOMEM;
        goto err_src;
    }
    if (!(dst = dma_alloc_writecombine(NULL, BUF_SIZE, &dst_phys, GFP_KERNEL))) {
        ret = -ENOMEM;
        goto err_dst;
    }

    dma_major = register_chrdev(0, "s3c_dma", &dma_fops);
    dma_cls = class_create(THIS_MODULE, "s3c_dma");
    if (IS_ERR(dma_cls)) {
        ret = PTR_ERR(dma_cls);
        goto err_cls;
    }
    dma_clsdev = class_device_create(dma_cls, NULL, MKDEV(dma_major, 0), NULL, "s3c_dma");
    if (IS_ERR(dma_clsdev)) {
        ret = PTR_ERR(dma_clsdev);
        goto err_clsdev;
    }

    dma_regs = (volatile struct s3c_dma_regs *)ioremap(DMA3_BASE_ADDR, sizeof(struct s3c_dma_regs));
    return 0;

err_clsdev:
    class_device_destroy(dma_cls, MKDEV(dma_major, 0));
    class_destroy(dma_cls);
err_cls:
    unregister_chrdev(dma_major, "s3c_dma");
    dma_free_writecombine(NULL, BUF_SIZE, dst, dst_phys);
err_dst:
    dma_free_writecombine(NULL, BUF_SIZE, src, src_phys);
err_src:
    free_irq(IRQ_DMA3, NULL);
err_irq:
    return ret;
}

static void __exit dma_exit(void)
{
    iounmap(dma_regs);
    class_device_destroy(dma_cls, MKDEV(dma_major, 0));
    class_destroy(dma_cls);
    unregister_chrdev(dma_major, "s3c_dma");
    dma_free_writecombine(NULL, BUF_SIZE, dst, dst_phys);
    dma_free_writecombine(NULL, BUF_SIZE, src, src_phys);
    free_irq(IRQ_DMA3, NULL);
}

module_init(dma_init);
module_exit(dma_exit);

MODULE_LICENSE("GPL");
