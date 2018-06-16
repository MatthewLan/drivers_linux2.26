/*
 * driver/hid/usbhid/usbmouse.c
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>


static struct input_dev *uk_dev  = NULL;
static char             *usb_buf = NULL;
static dma_addr_t       usb_buf_addr;
static int              buf_len = 0;
static struct urb       *uk_urb;


static void usbmouse_complete(struct urb *urb)
{
#if 0
    int i = 0;
    static int cnt = 0;
    printk("data cnt: %d\n", ++cnt);
    for (; i < buf_len; i++) {
        printk("%#02x", usb_buf[i]);
    }
#else
    static unsigned char pre_val = 0;
    if ((pre_val & (1 << 0)) != (usb_buf[0] & (1 << 0))) {
        input_event(uk_dev, EV_KEY, KEY_L, (usb_buf[0] & (1 << 0)) ? 1 : 0);
        input_sync(uk_dev);
    }
    if ((pre_val & (1 << 1)) != (usb_buf[0] & (1 << 1))) {
        input_event(uk_dev, EV_KEY, KEY_S, (usb_buf[0] & (1 << 1)) ? 1 : 0);
        input_sync(uk_dev);
    }
    if ((pre_val & (1 << 2)) != (usb_buf[0] & (1 << 2))) {
        input_event(uk_dev, EV_KEY, KEY_ENTER, (usb_buf[0] & (1 << 2)) ? 1 : 0);
        input_sync(uk_dev);
    }
    pre_val = usb_buf[0];
#endif
    usb_submit_urb(uk_urb, GFP_KERNEL);
}

static int usbmouse_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
    struct usb_device *dev = interface_to_usbdev(intf);
    struct usb_host_interface *interface;
    struct usb_endpoint_descriptor *endpoint;
    int pipe = 0;

    printk("usbmouse_probe called");
    printk("bcdUSB: %#x\n", dev->descriptor.bcdUSB);
    printk("idVendor: %#x\n", dev->descriptor.idVendor);
    printk("idProduct: %#x\n", dev->descriptor.idProduct);

    uk_dev = input_allocate_device();
    set_bit(EV_KEY, uk_dev->evbit);
    set_bit(EV_REP, uk_dev->evbit);
    set_bit(KEY_L, uk_dev->keybit);
    set_bit(KEY_S, uk_dev->keybit);
    set_bit(KEY_ENTER, uk_dev->keybit);
    input_register_device(uk_dev);

    interface = intf->cur_altsetting;
    if (interface->desc.bNumEndpoints != 1) {
        return -ENODEV;
    }
    /* 除端点0外的第一个端点 */
    endpoint = &interface->endpoint[0].desc;
    if (!usb_endpoint_is_int_in(endpoint)) {
        return -ENODEV;
    }
    pipe    = usb_rcvintpipe(dev, endpoint->bEndpointAddress);
    buf_len = endpoint->wMaxPacketSize;
    usb_buf = usb_buffer_alloc(dev, buf_len, GFP_ATOMIC, &usb_buf_addr);

    uk_urb  = usb_alloc_urb(0, GFP_KERNEL);
    usb_fill_int_urb(uk_urb, dev, pipe, usb_buf, buf_len, \
                     usbmouse_complete, NULL, endpoint->bInterval);
    uk_urb->transfer_dma = usb_buf_addr;
    uk_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
    usb_submit_urb(uk_urb, GFP_KERNEL);

    return 0;
}

static void usbmouse_disconnect(struct usb_interface *intf)
{
    struct usb_device *dev = interface_to_usbdev(intf);
    printk("usbmouse_disconnect called");
    usb_kill_urb(uk_urb);
    usb_free_urb(uk_urb);
    usb_buffer_free(dev, buf_len, usb_buf, usb_buf_addr);
    input_unregister_device(uk_dev);
    input_free_device(uk_dev);
}


static struct usb_device_id usbmouse_id_table[] = {
    { USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT,
        USB_INTERFACE_PROTOCOL_MOUSE) },
    { }
};

static struct usb_driver usbmouse_driver = {
    .name       = "usbmouse_as_key",
    .probe      = usbmouse_probe,
    .disconnect = usbmouse_disconnect,
    .id_table   = usbmouse_id_table,
};


static __init int usbmouse_init(void)
{
	return usb_register(&usbmouse_driver);
}

static __exit void usbmouse_exit(void)
{
    usb_deregister(&usbmouse_driver);
}

module_init(usbmouse_init);
module_exit(usbmouse_exit);
MODULE_LICENSE("GPL");
