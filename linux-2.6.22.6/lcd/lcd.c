#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>

#include <asm/mach/map.h>
#include <asm/arch/regs-lcd.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/fb.h>


struct regs_info
{
    unsigned long lcdcon1;
    unsigned long lcdcon2;
    unsigned long lcdcon3;
    unsigned long lcdcon4;
    unsigned long lcdcon5;
    unsigned long lcdsaddr1;
    unsigned long lcdsaddr2;
    unsigned long lcdsaddr3;
    unsigned long redlut;
    unsigned long greenlut;
    unsigned long bluelut;
    unsigned long reserved[9];  /* align */
    unsigned long dithmode;
    unsigned long tpal;
    unsigned long lcdintpnd;
    unsigned long lcdsrcpnd;
    unsigned long lcdintmsk;
    unsigned long tconsel;
};


static struct fb_info *lcd_fb_info;

static volatile unsigned long *gpbcon;
static volatile unsigned long *gpbdat;
static volatile unsigned long *gpccon;
static volatile unsigned long *gpdcon;
static volatile unsigned long *gpgcon;

static volatile struct regs_info *lcd_regs;

static u32 pseudo_palette[16];


static int chan_to_field(unsigned int chan, struct fb_bitfield *bf)
{
    chan &= 0xffff;
    chan >>= 16 - bf->length;
    return (chan << bf->offset);
}

static int lcd_fb_setcolreg(unsigned int regno, \
                            unsigned int red, \
                            unsigned int green, \
                            unsigned int blue, \
                            unsigned int transp, \
                            struct fb_info *info \
                            )
{
    if ( 16 < regno) {
        return 1;
    }

    unsigned int val;
    val = chan_to_field(red, &info->var.red);
    val |= chan_to_field(green, &info->var.green);
    val |= chan_to_field(blue, &info->var.blue);

    // ((u32 *)info->pseudo_palette)[regno] = val;
    pseudo_palette[regno] = val;
    return 0;
}

static struct fb_ops lcd_fb_ops = {
    .owner        = THIS_MODULE,
    .fb_setcolreg = lcd_fb_setcolreg,
    .fb_fillrect  = cfb_fillrect,
    .fb_copyarea  = cfb_copyarea,
    .fb_imageblit = cfb_imageblit,
};

static int __init lcd_init(void)
{
    /* 1. 分配一个fb_info */
    lcd_fb_info = framebuffer_alloc(0, NULL);

    /* 2. 设置 */
    /* 2.1 设置固定的参数 */
    strcpy(lcd_fb_info->fix.id, "jz2440_lcd");
    lcd_fb_info->fix.smem_len    = 240 * 320 * 16 / 8;      /* jz2440: R-5 G-6 B-5 */
    lcd_fb_info->fix.type        = FB_TYPE_PACKED_PIXELS;
    lcd_fb_info->fix.visual      = FB_VISUAL_TRUECOLOR;     /* TFT */
    lcd_fb_info->fix.line_length = 240 * 16 / 8;

    /* 2.2 设置可变的参数 */
    lcd_fb_info->var.xres           = 240;
    lcd_fb_info->var.yres           = 320;
    lcd_fb_info->var.xres_virtual   = 240;
    lcd_fb_info->var.yres_virtual   = 320;
    lcd_fb_info->var.bits_per_pixel = 16;
    lcd_fb_info->var.red.offset     = 11;   /* RGB: 565 */
    lcd_fb_info->var.red.length     = 5;    /* RGB: 565 */
    lcd_fb_info->var.green.offset   = 5;    /* RGB: 565 */
    lcd_fb_info->var.green.length   = 6;    /* RGB: 565 */
    lcd_fb_info->var.blue.offset    = 0;    /* RGB: 565 */
    lcd_fb_info->var.blue.length    = 5;    /* RGB: 565 */
    lcd_fb_info->var.activate       = FB_ACTIVATE_NOW;

    /* 2.3 设置操作函数 */
    lcd_fb_info->fbops = &lcd_fb_ops;

    /* 2.4 其它设置 */
    lcd_fb_info->pseudo_palette = pseudo_palette; /* 调色板 */
    // lcd_fb_info->screen_base    = ; /* 显存的虚拟地址 */
    lcd_fb_info->screen_size    = 240 * 320 * 16 / 8;

    /* 3. 硬件相关的设置 */
    /* 3.1 配置GPIO用于LCD */
    gpbcon  = ioremap(0x56000010, 8);
    gpbdat  = gpbcon + 1;
    gpccon  = ioremap(0x56000020, 4);
    gpdcon  = ioremap(0x56000030, 4);
    gpgcon  = ioremap(0x56000060, 4);
    *gpccon = 0xaaaaaaaa;               /* VD[7:0],VM,VFRANME,VLINE,VCLK,LEND */
    *gpdcon = 0xaaaaaaaa;               /* vd[23:8] */
    *gpbcon &= ~3;
    *gpbcon |= 1;                       /* GPB0: out */
    *gpbdat &= 0;                       /* GPB0: out-low */
    *gpgcon |= (3<<8);                  /* GPG4: LDC_PWREN */

    /* 3.2 根据LCD手册，设置LCD控制器，eg.VCLK的频率等 */
    lcd_regs = ioremap(0x4d000000, sizeof(struct regs_info));
    /*
     * 频率等参数
     *
     * bit[17:8] : VCLK = HCLK/((CLKVAL + 1) * 2)
     *             HCLK : 100MHz
     *             VCLK : 100ns => 10MHz
     *             CLKVAL = 4
     * bit[6:5]  : 0b11, TFT LCD
     * bit[4:1]  : 0b1100, 16 bpp for TFT
     * bit[0]    : 0, disable the video output and the LCD control signal
     */
    lcd_regs->lcdcon1 = (4 << 8) | (3 << 5) | (0xc0 << 1);
    /*
     * 垂直方向的时间参数
     *
     * bit[31:24] : VBPD +1 = 4
     * bit[23:14] : LINEVAL +1 = 320
     * bit[13:6]  : VFPD +1 = 2
     * bit[5:0]   : VSPW +1 = 0
     */
    lcd_regs->lcdcon2 = (3 << 24) | (319 << 14) | (1 << 6);
    /*
     * 水平方向的时间参数
     *
     * bit[25:19] : HBPD +1 = 17
     * bit[18:8]  : HOZVAL +1 = 240
     * bit[7:0]   : HFPD +1 = 11
     */
    lcd_regs->lcdcon3 = (16 << 19) | (239 << 8) | (10 << 0);
    /*
     * 水平方向的同步信号
     *
     * bit[7:0] : HSPW +1 = 5
     */
    lcd_regs->lcdcon4 = 4;

    /*
     * 信号的极性
     *
     * bit[11] : FRM565, 1 = 5:6:5 Format
     * bit[10] : INVVCLK, 0 = The video data is fetched at VCLK falling edge
     * bit[9]  : INVVLINE, (HSYNC) 1 = Inverted
     * bit[8]  : INVVFRAME, (VSYNC) 1 = Inverted
     * bit[6]  : INVVDEN, 0 = Normal
     * bit[5]  : INVPWREN, 0 = Normal
     * bit[3]  : PWREN, 0 = Disable PWREN signal
     * bit[1]  : BSWP, Byte swap control bit, 0 = Disabel
     * bit[0]  : HWSWP, Half-Word swap control bit, 1 = Enable
     */
    lcd_regs->lcdcon5 = (1 << 11) | (1 << 9) | (1 << 8) | (1 << 0);

    /* 3.3 分配显存framebuffer，并把地址告诉LCD控制器 */
    /* get: 显存的虚拟地址 */
    lcd_fb_info->screen_base = dma_alloc_writecombine(NULL, \
                               lcd_fb_info->fix.smem_len,       /* 显存的大小 */
                               &lcd_fb_info->fix.smem_start,    /* 显存的物理地址 */
                               GFP_KERNEL);
    /*
     * bit[29:21] : LCDBANK, A[30:22]
     * bit[20:0]  : LCDBASEU, A[21:1]
     */
    lcd_regs->lcdsaddr1 = (lcd_fb_info->fix.smem_start >> 1) & (~(3 << 30));
    /*
     * bit[20:0] : LCDBASEL, A[21:1] of end address
     */
    lcd_regs->lcdsaddr2 = ((lcd_fb_info->fix.smem_start + lcd_fb_info->fix.smem_len) >> 1) & 0x1fffff;
    /*
     * bit[10:0] : PAGEWIDTH, 一行的长度，单位：2字节
     */
    lcd_regs->lcdsaddr3 = 240 * 16 / 16;

    /* 启动LCD */
    lcd_regs->lcdcon1 |= (1 << 0);  /* 使能LCD控制器 */
    lcd_regs->lcdcon5 |= (1 << 3);  /* PWREN: enable, 使能LCD本身  */
    *gpbdat |= 1;                   /* GPB0: out-high, 使能背光 */

    /* 4. 注册 */
    register_framebuffer(lcd_fb_info);

    return 0;
}

static void __exit lcd_exit(void)
{
    unregister_framebuffer(lcd_fb_info);
    lcd_regs->lcdcon1 &= 0;
    *gpbdat &= 0;
    dma_free_writecombine(NULL, \
                          lcd_fb_info->fix.smem_len, \
                          lcd_fb_info->screen_base, \
                          lcd_fb_info->fix.smem_start);
    iounmap(lcd_regs);
    iounmap(gpbcon);
    iounmap(gpbdat);
    iounmap(gpdcon);
    iounmap(gpgcon);
    framebuffer_release(lcd_fb_info);
}

module_init(lcd_init);
module_exit(lcd_exit);
MODULE_LICENSE("GPL");
