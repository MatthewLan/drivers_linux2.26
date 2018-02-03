/*
 * drivers/mtd/maps/physmap.c
 * drivers/mtd/devices/mtdram.c - 用内存模拟MTD
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <asm/io.h>


static struct map_info *nor_map = NULL;
static struct mtd_info *nor_mtd = NULL;

static struct mtd_partition norflash_part[] = {
    [0] = {
        .name   = "bootloader",
        .size   = 0x00040000,
        .offset = 0,
    },
    [1] = {
        .name   = "root_nor",
        .offset = MTDPART_OFS_APPEND,
        .size   = MTDPART_SIZ_FULL,
    }
};

static int __init nor_flash_init(void)
{
    /* 1. 分配 map_info 结构体 */
    nor_map = kzalloc(sizeof(struct map_info), GFP_KERNEL);

    /* 2. 设置：物理基地址(phys)，大小(size)，位宽(bankwidth)，虚拟基地址(virt) */
    nor_map->name      = "nor_flash";
    nor_map->phys      = 0;
    nor_map->size      = 0x1000000; /* 16M >= nor_flash real size */
    nor_map->bankwidth = 2;         /* 16bit */
    nor_map->virt      = ioremap(nor_map->phys, nor_map->size);
    simple_map_init(nor_map);

    /* 3. 使用：调用 NOR FLASH协议层提供的函数来识别 */
    printk("use cfi_probe\n");
    nor_mtd = do_map_probe("cfi_probe", nor_map);
    if (!nor_mtd) {
        printk("use jedec_probe\n");
        nor_mtd = do_map_probe("jedec_probe", nor_map);
    }
    if (!nor_mtd) {
        iounmap(nor_map->virt);
        kfree(nor_map);
        return -EIO;
    }

    /* 4. add_mtd_partitions */
    add_mtd_partitions(nor_mtd, norflash_part, 2);

    return 0;
}

static void __exit nor_flash_exit(void)
{
    del_mtd_partitions(nor_mtd);
    iounmap(nor_map->virt);
    kfree(nor_map);
}

module_init(nor_flash_init);
module_exit(nor_flash_exit);
MODULE_LICENSE("GPL");
