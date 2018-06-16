/*
* drivers/net/cs89x0.c
*/

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/ip.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>


struct net_device *vnet_dev = NULL;


static int emulator_rx_packet(struct sk_buff *skb, struct net_device *dev)
{
    /* 参考LDD3 */
    /*
     * skb->data:
     *  MAC,IP头,Type,data
     *
     * MAC:
     *  目的MAC,源MAC,...
     *
     * IP头:
     *  源IP,目的IP
     *
     * Type:
     *  0x8 - ping包
     *  0x0 - reply包
     */
    unsigned char     *type                  = NULL;
    struct iphdr      *ih                    = NULL;
    __be32            tmp                    = 0;
    unsigned char     tmp_dev_addr[ETH_ALEN] = {0};
    struct ethhdr     *ethhdr                = NULL;
    struct sk_buff    *rx_skb                = NULL;

    /* 从硬件读出/保存数据 */
    /* 对调 源/目的 的MAC地址 */
    ethhdr = (struct ethhdr *)skb->data;
    memcpy(tmp_dev_addr,     ethhdr->h_dest,   ETH_ALEN);
    memcpy(ethhdr->h_dest,   ethhdr->h_source, ETH_ALEN);
    memcpy(ethhdr->h_source, tmp_dev_addr,     ETH_ALEN);

    /* 对调 源/目的 的IP地址 */
    ih        = (struct iphdr *)(skb->data + sizeof(struct ethhdr));
    tmp       = ih->saddr;
    ih->saddr = ih->daddr;
    ih->daddr = tmp;

    /* Type */
    type  = skb->data + sizeof(struct ethhdr) + sizeof(struct iphdr);
    *type = 0;  /* reply */

    /* checksum */
    ih->check = 0;
    ih->check = ip_fast_csum((unsigned char *)ih, ih->ihl);

    /* 构造一个sk_buff */
    rx_skb = dev_alloc_skb(skb->len +2);
    skb_reserve(rx_skb, 2);
    memcpy(skb_put(rx_skb, skb->len), skb->data, skb->len);

    rx_skb->dev       = dev;
    rx_skb->protocol  = eth_type_trans(rx_skb, dev);
    rx_skb->ip_summed = CHECKSUM_UNNECESSARY;
    dev->stats.rx_packets++;
    dev->stats.rx_bytes += skb->len;

    /* 提交sk_buff */
    netif_rx(rx_skb);
}

static int virt_net_send_packet(struct sk_buff *skb, struct net_device *dev)
{
    /* 对于真实的网卡，把skb里的数据通过网卡发送出去 */

    /* 停止该网卡的队列 */
    netif_stop_queue(dev);

    /* 把skb的数据写入网卡 */
    /* TODO */
    /* 构造一个假的sk_buf，上报 */
    emulator_rx_packet(skb, dev);

    /* 释放skb */
    dev_kfree_skb(skb);

    /* 真实：数据全部发送出去后，会产生中断，在中断中唤醒网卡的队列 */
    netif_wake_queue(dev);

    /* 更新统计信息 */
    dev->stats.tx_packets++;
    dev->stats.tx_bytes += skb->len;
    return 0;
}

static int __init virt_net_init(void)
{
    /* 1. 分配一个 net_device 结构体 */
    // alloc_etherdev()
    vnet_dev = alloc_netdev(0, "vnet%d", ether_setup);

    /* 2. 设置 */
    /* 2.1 发包函数：hard_start_xmit() */
    vnet_dev->hard_start_xmit = virt_net_send_packet;
    /* 2.2 设置MAC地址 */
    vnet_dev->dev_addr[0] = 0x08;
    vnet_dev->dev_addr[1] = 0x89;
    vnet_dev->dev_addr[2] = 0x89;
    vnet_dev->dev_addr[3] = 0x89;
    vnet_dev->dev_addr[4] = 0x89;
    vnet_dev->dev_addr[5] = 0x11;
    /* 2.2 设置下面两项才能ping通*/
    vnet_dev->flags |= IFF_NOARP;
    vnet_dev->features |= NETIF_F_NO_CSUM;

    /* 3. 注册 */
    register_netdev(vnet_dev);

    return 0;
}

static void __exit virt_net_exit(void)
{
    unregister_netdev(vnet_dev);
    free_netdev(vnet_dev);
}

module_init(virt_net_init);
module_exit(virt_net_exit);
MODULE_LICENSE("GPL");
