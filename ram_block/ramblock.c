/*
 * drivers/block/xd.c
 * drivers/block/z2ram.c
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/genhd.h>
#include <linux/hdreg.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/delay.h>
#include <linux/io.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/dma.h>


static struct gendisk  *ramblock_disk  = NULL;
static request_queue_t *ramblock_queue = NULL;
static int             ramblock_major  = 0;

static DEFINE_SPINLOCK(ramblock_lock);

/* 内存虚拟的磁盘空间 */
static unsigned char *ramblock_buf = NULL;

#define RAMBLOCK_SIZE   (1024 * 1024)


/* 为了兼容fdisk工具 */
static int ramblock_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
    /* size = heads*cylinders*sectors*512 */
    geo->heads     = 2;                      /* 磁头 */
    geo->cylinders = 32;                     /* 柱面 */
    geo->sectors   = RAMBLOCK_SIZE/2/32/512; /* 扇区 */
    return 0;
}


static struct block_device_operations ramblock_fops = {
    .owner  = THIS_MODULE,
    .getgeo = ramblock_getgeo,
};


static void do_ramblock_request(request_queue_t *q)
{
    struct request *req = NULL;

    while (NULL != (req = elv_next_request(q))) {
        unsigned long offset = req->sector * 512;
        unsigned long len = req->current_nr_sectors * 512;
        if (READ == rq_data_dir(req)) {
            memcpy(req->buffer, ramblock_buf + offset, len);
        }
        else {
            memcpy(ramblock_buf + offset, req->buffer, len);
        }
        /* wrap up, 0 = fail, 1 = success */
        end_request(req, 1);
    }
}

static int __init ramblock_init(void)
{
    /* 1. 分配一个gendisk结构体 */
    /* 次设备号个数： 分区个数 +1 -> 15*/
    ramblock_disk = alloc_disk(16);

    /* 2. 设置 */
    /* 2.1 分配/设置队列：提供读写能力 */
    ramblock_queue = blk_init_queue(do_ramblock_request, &ramblock_lock);
    ramblock_disk->queue = ramblock_queue;
    /* 2.2 设置其它属性：比如容量 */
    ramblock_major = register_blkdev(0, "ramblock"); /* cat /proc/devices */
    ramblock_disk->major = ramblock_major;
    ramblock_disk->first_minor = 0;
    sprintf(ramblock_disk->disk_name, "ramblock");
    ramblock_disk->fops = &ramblock_fops;
    /* 以扇区位单位，512字节 */
    set_capacity(ramblock_disk, RAMBLOCK_SIZE / 512);

    ramblock_buf = kzalloc(RAMBLOCK_SIZE, GFP_KERNEL);

    /* 3. 注册 */
    add_disk(ramblock_disk);
    return 0;
}

static void __exit ramblock_exit(void)
{
    unregister_blkdev(ramblock_major, "ramblock");
    del_gendisk(ramblock_disk);
    put_disk(ramblock_disk);
    blk_cleanup_queue(ramblock_queue);
    kfree(ramblock_buf);
}

module_init(ramblock_init);
module_exit(ramblock_exit);
MODULE_LICENSE("GPL");
