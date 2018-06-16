/*
 * K9F2G08U0C
 *
 * drivers/mtd/nand/s3c2410.c
 * drivers/mtd/nand/at91_nand.c
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>

#include <asm/io.h>
#include <asm/arch/regs-nand.h>
#include <asm/arch/nand.h>


struct nand_regs
{
    unsigned long nfconf;
    unsigned long nfcont;
    unsigned long nfcmd;
    unsigned long nfaddr;
    unsigned long nfdata;
    unsigned long nfeccd0;
    unsigned long nfeccd1;
    unsigned long nfeccd;
    unsigned long nfstat;
    unsigned long nfestat0;
    unsigned long nfestat1;
    unsigned long nfmecc0;
    unsigned long nfmecc1;
    unsigned long nfsecc;
    unsigned long nfsblk;
    unsigned long nfeblk;
};

static struct nand_chip *nandflash_chip = NULL;
static struct mtd_info  *nandflash_info = NULL;
static struct nand_regs *nandflash_regs = NULL;

static struct mtd_partition nandflash_part[] = {
    [0] = {
        .name   = "bootloader",
        .size   = 0x00040000,
        .offset = 0,
    },
    [1] = {
        .name   = "params",
        .offset = MTDPART_OFS_APPEND,
        .size   = 0x00020000,
    },
    [2] = {
        .name   = "kernel",
        .offset = MTDPART_OFS_APPEND,
        .size   = 0x00200000,
    },
    [3] = {
        .name   = "root",
        .offset = MTDPART_OFS_APPEND,
        .size   = MTDPART_SIZ_FULL,
    }
};


static void nand_flash_select_chip(struct mtd_info *mtd, int chip)
{
    /* 取消 */
    if (-1 == chip) {
        nandflash_regs->nfcont |= (1<<1);
    }
    /* 选中 */
    else {
        nandflash_regs->nfcont &= ~(1<<1);
    }
}

static void nand_flash_cmd_ctrl(struct mtd_info *mtd, int dat, unsigned int ctrl)
{
    if (NAND_CMD_NONE == ctrl) {
        return;
    }
    /* 发命令 */
    if (NAND_CLE & ctrl) {
        nandflash_regs->nfcmd = dat;
    }
    /* 发数据 */
    else {
        nandflash_regs->nfaddr = dat;
    }
}

static int nand_flash_dev_ready(struct mtd_info *mtd)
{
    return (nandflash_regs->nfstat & (1<<0));
}


static int __init nand_flash_init(void)
{
    struct clk *nandflash_clk = NULL;

    /* 1. 分配一个nand_chip结构体 */
    nandflash_chip = kzalloc(sizeof(struct nand_chip), GFP_KERNEL);
    nandflash_regs = ioremap(0x4e000000, sizeof(struct nand_regs));

    /* 2. 设置
     *
     * 提供以下功能函数：片选，发命令，发地址，发数据，读数据，判断状态
     */
    nandflash_chip->select_chip = nand_flash_select_chip;
    nandflash_chip->cmd_ctrl    = nand_flash_cmd_ctrl;
    nandflash_chip->IO_ADDR_R   = &nandflash_regs->nfdata;
    nandflash_chip->IO_ADDR_W   = &nandflash_regs->nfdata;
    nandflash_chip->dev_ready   = nand_flash_dev_ready;
    nandflash_chip->ecc.mode    = NAND_ECC_SOFT;

    /* 3. 硬件相关的设置 */
    /* 使能NAND FLASH的时钟控制器 */
    nandflash_clk = clk_get(NULL, "nand");
    clk_enable(nandflash_clk);
    /* 设置时间参数
     *     HCLK    = 100MHz
     *     TACLS  >= 0
     *     TWRPH0 >= 1
     *     TWRPH1 >= 0
     */
#define TACLS  0
#define TWRPH0 1
#define TWRPH1 0
    nandflash_regs->nfconf = (TACLS <<12) | (TWRPH0 <<8) | (TWRPH1 <<4);
    /* 取消片选(bit1 = 1)，使能NAND FLASH控制器(bit0 = 1) */
    nandflash_regs->nfcont = (1 <<1) | (1 <<0);

    /* 4. 使用：nand_scan */
    nandflash_info        = kzalloc(sizeof(struct mtd_info), GFP_KERNEL);
    nandflash_info->owner = THIS_MODULE;
    nandflash_info->priv  = nandflash_chip;
    nand_scan(nandflash_info, 1);

    /* 5. add_mtd_partitions */
#if 1
    /* 构造分区 */
    add_mtd_partitions(nandflash_info \
                       , nandflash_part \
                       , sizeof(nandflash_part)/sizeof(struct mtd_partition));
#else
    /* 只有一个分区 */
    add_mtd_device(nandflash_info);
#endif

    return 0;
}

static void __exit nand_flash_exit(void)
{
#if 1
    del_mtd_partitions(nandflash_info);
#else
    del_mtd_device(nandflash_info);
#endif
    kfree(nandflash_info);
    iounmap(nandflash_regs);
    kfree(nandflash_chip);
}

module_init(nand_flash_init);
module_exit(nand_flash_exit);
MODULE_LICENSE("GPL");
