#include "platform/usb_composite.h"

#include <string.h>

#include <libopencm3/stm32/otg_fs.h>
#include <libopencm3/usb/cdc.h>
#include <libopencm3/usb/hid.h>
#include <libopencm3/usb/usbd.h>

#define USB_VID 0x1209
#define USB_PID 0x4110
#define USB_BCD_DEVICE 0x0100

#define EP_CDC_OUT 0x01
#define EP_CDC_IN  0x82
#define EP_CDC_INT 0x83
#define EP_KBD_IN  0x84
#define EP_MOUSE_IN 0x85
#define EP_GENERIC_IN 0x86
#define EP_GENERIC_OUT 0x06
#define GENERIC_REPORT_SIZE APP_GENERIC_REPORT_SIZE

#define IFACE_CDC_COMM 0
#define IFACE_CDC_DATA 1
#define IFACE_HID_KBD 2
#define IFACE_HID_MOUSE 3
#define IFACE_HID_GENERIC 4

static usbd_device *g_usbd;
static struct app_context *g_app;
static uint8_t g_ctrl_buffer[256];
static struct usb_cdc_line_coding g_line_coding = {
    .dwDTERate = 115200,
    .bCharFormat = 0,
    .bParityType = 0,
    .bDataBits = 8,
};

static const uint8_t hid_report_kbd[] = {
    0x05, 0x01, 0x09, 0x06, 0xa1, 0x01, 0x05, 0x07,
    0x19, 0xe0, 0x29, 0xe7, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01,
    0x75, 0x08, 0x81, 0x01, 0x95, 0x05, 0x75, 0x01,
    0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02,
    0x95, 0x01, 0x75, 0x03, 0x91, 0x01, 0x95, 0x06,
    0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05, 0x07,
    0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xc0
};

static const uint8_t hid_report_mouse[] = {
    0x05, 0x01, 0x09, 0x02, 0xa1, 0x01, 0x09, 0x01,
    0xa1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x03,
    0x15, 0x00, 0x25, 0x01, 0x95, 0x03, 0x75, 0x01,
    0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81, 0x01,
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x15, 0x81,
    0x25, 0x7f, 0x75, 0x08, 0x95, 0x02, 0x81, 0x06,
    0xc0, 0xc0
};

static const uint8_t hid_report_generic[] = {
    0x06, 0x00, 0xff,
    0x09, 0x01,
    0xa1, 0x01,
    0x85, 0x01,
    0x09, 0x02,
    0x15, 0x00,
    0x26, 0xff, 0x00,
    0x75, 0x08,
    0x95, 0x20,
    0x91, 0x02,
    0x09, 0x03,
    0x95, 0x20,
    0x81, 0x02,
    0xc0
};

static const struct usb_endpoint_descriptor cdc_comm_endp[] = {{
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = EP_CDC_INT,
    .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
    .wMaxPacketSize = 16,
    .bInterval = 16,
}};

static const struct usb_endpoint_descriptor cdc_data_endp[] = {
    {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = EP_CDC_OUT,
        .bmAttributes = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize = 64,
        .bInterval = 1,
    },
    {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = EP_CDC_IN,
        .bmAttributes = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize = 64,
        .bInterval = 1,
    },
};

static const struct usb_endpoint_descriptor hid_kbd_endp[] = {{
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = EP_KBD_IN,
    .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
    .wMaxPacketSize = 8,
    .bInterval = 10,
}};

static const struct usb_endpoint_descriptor hid_mouse_endp[] = {{
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = EP_MOUSE_IN,
    .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
    .wMaxPacketSize = 4,
    .bInterval = 4,
}};

static const struct usb_endpoint_descriptor hid_generic_endp[] = {
    {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = EP_GENERIC_OUT,
        .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
        .wMaxPacketSize = GENERIC_REPORT_SIZE,
        .bInterval = 4,
    },
    {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = EP_GENERIC_IN,
        .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
        .wMaxPacketSize = GENERIC_REPORT_SIZE,
        .bInterval = 4,
    },
};

static const struct {
    struct usb_cdc_header_descriptor header;
    struct usb_cdc_call_management_descriptor call_mgmt;
    struct usb_cdc_acm_descriptor acm;
    struct usb_cdc_union_descriptor union_desc;
} __attribute__((packed)) cdc_acm_functional_descriptors = {
    .header = {
        .bFunctionLength = sizeof(struct usb_cdc_header_descriptor),
        .bDescriptorType = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_HEADER,
        .bcdCDC = 0x0110,
    },
    .call_mgmt = {
        .bFunctionLength = sizeof(struct usb_cdc_call_management_descriptor),
        .bDescriptorType = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
        .bmCapabilities = 0,
        .bDataInterface = IFACE_CDC_DATA,
    },
    .acm = {
        .bFunctionLength = sizeof(struct usb_cdc_acm_descriptor),
        .bDescriptorType = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_ACM,
        .bmCapabilities = 0,
    },
    .union_desc = {
        .bFunctionLength = sizeof(struct usb_cdc_union_descriptor),
        .bDescriptorType = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_UNION,
        .bMasterInterface0 = IFACE_CDC_COMM,
        .bSlaveInterface0 = IFACE_CDC_DATA,
     },
};

static const struct usb_iface_assoc_descriptor cdc_iad = {
    .bLength = USB_DT_INTERFACE_ASSOCIATION_SIZE,
    .bDescriptorType = USB_DT_INTERFACE_ASSOCIATION,
    .bFirstInterface = IFACE_CDC_COMM,
    .bInterfaceCount = 2,
    .bFunctionClass = USB_CLASS_CDC,
    .bFunctionSubClass = USB_CDC_SUBCLASS_ACM,
    .bFunctionProtocol = USB_CDC_PROTOCOL_AT,
    .iFunction = 0,
};

static const struct hid_function_descriptor hid_kbd_function = {
    .bLength = sizeof(struct hid_function_descriptor),
    .bDescriptorType = HID_DT_HID,
    .bcdHID = 0x0111,
    .bCountryCode = 0,
    .bNumDescriptors = 1,
    .desc = {{
        .bDescriptorType = HID_DT_REPORT,
        .wDescriptorLength = sizeof(hid_report_kbd),
    }},
};

static const struct hid_function_descriptor hid_mouse_function = {
    .bLength = sizeof(struct hid_function_descriptor),
    .bDescriptorType = HID_DT_HID,
    .bcdHID = 0x0111,
    .bCountryCode = 0,
    .bNumDescriptors = 1,
    .desc = {{
        .bDescriptorType = HID_DT_REPORT,
        .wDescriptorLength = sizeof(hid_report_mouse),
    }},
};

static const struct hid_function_descriptor hid_generic_function = {
    .bLength = sizeof(struct hid_function_descriptor),
    .bDescriptorType = HID_DT_HID,
    .bcdHID = 0x0111,
    .bCountryCode = 0,
    .bNumDescriptors = 1,
    .desc = {{
        .bDescriptorType = HID_DT_REPORT,
        .wDescriptorLength = sizeof(hid_report_generic),
    }},
};

static const struct usb_interface_descriptor comm_iface[] = {{
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = IFACE_CDC_COMM,
    .bAlternateSetting = 0,
    .bNumEndpoints = 1,
    .bInterfaceClass = USB_CLASS_CDC,
    .bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
    .bInterfaceProtocol = USB_CDC_PROTOCOL_AT,
    .iInterface = 0,
    .endpoint = cdc_comm_endp,
    .extra = &cdc_acm_functional_descriptors,
    .extralen = sizeof(cdc_acm_functional_descriptors),
}};

static const struct usb_interface_descriptor data_iface[] = {{
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = IFACE_CDC_DATA,
    .bAlternateSetting = 0,
    .bNumEndpoints = 2,
    .bInterfaceClass = USB_CLASS_DATA,
    .bInterfaceSubClass = 0,
    .bInterfaceProtocol = 0,
    .iInterface = 0,
    .endpoint = cdc_data_endp,
}};

static const struct usb_interface_descriptor kbd_iface[] = {{
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = IFACE_HID_KBD,
    .bAlternateSetting = 0,
    .bNumEndpoints = 1,
    .bInterfaceClass = USB_CLASS_HID,
    .bInterfaceSubClass = 1,
    .bInterfaceProtocol = 1,
    .iInterface = 0,
    .endpoint = hid_kbd_endp,
    .extra = &hid_kbd_function,
    .extralen = sizeof(hid_kbd_function),
}};

static const struct usb_interface_descriptor mouse_iface[] = {{
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = IFACE_HID_MOUSE,
    .bAlternateSetting = 0,
    .bNumEndpoints = 1,
    .bInterfaceClass = USB_CLASS_HID,
    .bInterfaceSubClass = 1,
    .bInterfaceProtocol = 2,
    .iInterface = 0,
    .endpoint = hid_mouse_endp,
    .extra = &hid_mouse_function,
    .extralen = sizeof(hid_mouse_function),
}};

static const struct usb_interface_descriptor generic_iface[] = {{
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = IFACE_HID_GENERIC,
    .bAlternateSetting = 0,
    .bNumEndpoints = 2,
    .bInterfaceClass = USB_CLASS_HID,
    .bInterfaceSubClass = 0,
    .bInterfaceProtocol = 0,
    .iInterface = 0,
    .endpoint = hid_generic_endp,
    .extra = &hid_generic_function,
    .extralen = sizeof(hid_generic_function),
}};

static const struct usb_interface ifaces[] = {
    {.num_altsetting = 1, .altsetting = comm_iface, .iface_assoc = &cdc_iad},
    {.num_altsetting = 1, .altsetting = data_iface},
    {.num_altsetting = 1, .altsetting = kbd_iface},
    {.num_altsetting = 1, .altsetting = mouse_iface},
    {.num_altsetting = 1, .altsetting = generic_iface},
};

static const struct usb_device_descriptor dev_descr = {
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0xef,
    .bDeviceSubClass = 0x02,
    .bDeviceProtocol = 0x01,
    .bMaxPacketSize0 = 64,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = USB_BCD_DEVICE,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

static const struct usb_config_descriptor config = {
    .bLength = USB_DT_CONFIGURATION_SIZE,
    .bDescriptorType = USB_DT_CONFIGURATION,
    .wTotalLength = 0,
    .bNumInterfaces = 5,
    .bConfigurationValue = 1,
    .iConfiguration = 0,
    .bmAttributes = 0x80,
    .bMaxPower = 0x32,
    .interface = ifaces,
};

static const char *usb_strings[] = {
    "OpenAI",
    "STM32F411 Composite HID",
    "000000000001",
};

static enum usbd_request_return_codes hid_control_request(usbd_device *usbd_dev,
    struct usb_setup_data *req,
    uint8_t **buf,
    uint16_t *len,
    void (**complete)(usbd_device *usbd_dev, struct usb_setup_data *req))
{
    (void)complete;
    (void)usbd_dev;

    if ((req->bmRequestType != 0x81U && req->bmRequestType != 0xa1U) ||
        req->bRequest != USB_REQ_GET_DESCRIPTOR) {
        return USBD_REQ_NEXT_CALLBACK;
    }

    if ((req->wValue >> 8) != HID_DT_REPORT) {
        return USBD_REQ_NEXT_CALLBACK;
    }

    switch (req->wIndex) {
    case IFACE_HID_KBD:
        *buf = (uint8_t *)hid_report_kbd;
        *len = sizeof(hid_report_kbd);
        return USBD_REQ_HANDLED;
    case IFACE_HID_MOUSE:
        *buf = (uint8_t *)hid_report_mouse;
        *len = sizeof(hid_report_mouse);
        return USBD_REQ_HANDLED;
    case IFACE_HID_GENERIC:
        *buf = (uint8_t *)hid_report_generic;
        *len = sizeof(hid_report_generic);
        return USBD_REQ_HANDLED;
    default:
        return USBD_REQ_NEXT_CALLBACK;
    }
}

static enum usbd_request_return_codes cdc_control_request(usbd_device *usbd_dev,
    struct usb_setup_data *req,
    uint8_t **buf,
    uint16_t *len,
    void (**complete)(usbd_device *usbd_dev, struct usb_setup_data *req))
{
    (void)complete;
    (void)usbd_dev;

    switch (req->bRequest) {
    case USB_CDC_REQ_SET_CONTROL_LINE_STATE: {
        static uint8_t notification[10];
        struct usb_cdc_notification *notif = (struct usb_cdc_notification *)notification;
        notif->bmRequestType = 0xa1;
        notif->bNotification = USB_CDC_NOTIFY_SERIAL_STATE;
        notif->wValue = 0;
        notif->wIndex = IFACE_CDC_COMM;
        notif->wLength = 2;
        notification[8] = 0;
        notification[9] = 0;
        usbd_ep_write_packet(g_usbd, EP_CDC_INT, notification, sizeof(notification));
        return USBD_REQ_HANDLED;
    }
    case USB_CDC_REQ_SET_LINE_CODING:
        if (*len < sizeof(struct usb_cdc_line_coding)) {
            return USBD_REQ_NOTSUPP;
        }
        memcpy(&g_line_coding, *buf, sizeof(g_line_coding));
        if (g_app != 0) {
            struct app_line_coding coding = {
                .baudrate = g_line_coding.dwDTERate,
                .stop_bits = g_line_coding.bCharFormat,
                .parity = g_line_coding.bParityType,
                .data_bits = g_line_coding.bDataBits,
            };
            app_handle_line_coding(g_app, &coding);
        }
        return USBD_REQ_HANDLED;
    case USB_CDC_REQ_GET_LINE_CODING:
        *buf = (uint8_t *)&g_line_coding;
        *len = sizeof(g_line_coding);
        return USBD_REQ_HANDLED;
    default:
        return USBD_REQ_NEXT_CALLBACK;
    }
}

static void cdc_rx_cb(usbd_device *usbd_dev, uint8_t ep)
{
    (void)usbd_dev;
    uint8_t buf[64];
    const uint16_t len = usbd_ep_read_packet(g_usbd, ep, buf, sizeof(buf));
    if (g_app != 0 && len > 0U) {
        app_handle_cdc_rx(g_app, buf, len);
    }
}

static void generic_rx_cb(usbd_device *usbd_dev, uint8_t ep)
{
    (void)usbd_dev;
    uint8_t buf[GENERIC_REPORT_SIZE];
    const uint16_t len = usbd_ep_read_packet(g_usbd, ep, buf, sizeof(buf));
    if (g_app != 0 && len > 0U) {
        app_handle_generic_report(g_app, buf, len);
    }
}

static void set_config(usbd_device *usbd_dev, uint16_t wValue)
{
    (void)wValue;

    usbd_ep_setup(usbd_dev, EP_CDC_OUT, USB_ENDPOINT_ATTR_BULK, 64, cdc_rx_cb);
    usbd_ep_setup(usbd_dev, EP_CDC_IN, USB_ENDPOINT_ATTR_BULK, 64, 0);
    usbd_ep_setup(usbd_dev, EP_CDC_INT, USB_ENDPOINT_ATTR_INTERRUPT, 16, 0);
    usbd_ep_setup(usbd_dev, EP_KBD_IN, USB_ENDPOINT_ATTR_INTERRUPT, 8, 0);
    usbd_ep_setup(usbd_dev, EP_MOUSE_IN, USB_ENDPOINT_ATTR_INTERRUPT, 4, 0);
    usbd_ep_setup(usbd_dev, EP_GENERIC_OUT, USB_ENDPOINT_ATTR_INTERRUPT, GENERIC_REPORT_SIZE, generic_rx_cb);
    usbd_ep_setup(usbd_dev, EP_GENERIC_IN, USB_ENDPOINT_ATTR_INTERRUPT, GENERIC_REPORT_SIZE, 0);

    usbd_register_control_callback(
        usbd_dev,
        USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE,
        USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
        hid_control_request);

    usbd_register_control_callback(
        usbd_dev,
        USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
        USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
        cdc_control_request);
}

void usb_composite_init(struct app_context *app)
{
    g_app = app;
    g_usbd = usbd_init(&otgfs_usb_driver, &dev_descr, &config, usb_strings,
        3, g_ctrl_buffer, sizeof(g_ctrl_buffer));
    usbd_register_set_config_callback(g_usbd, set_config);
}

void usb_composite_poll(void)
{
    if (g_usbd != 0) {
        usbd_poll(g_usbd);
    }
}

void usb_cdc_tx(const uint8_t *data, size_t len)
{
    if (g_usbd != 0 && len > 0U) {
        usbd_ep_write_packet(g_usbd, EP_CDC_IN, data, (uint16_t)len);
    }
}

void usb_hid_mouse_move(int8_t dx, int8_t dy)
{
    uint8_t report[4] = {0U, 0U, (uint8_t)dx, (uint8_t)dy};
    if (g_usbd != 0) {
        usbd_ep_write_packet(g_usbd, EP_MOUSE_IN, report, sizeof(report));
    }
}

void usb_hid_keyboard_report(const uint8_t *report, size_t len)
{
    if (g_usbd != 0 && len > 0U) {
        usbd_ep_write_packet(g_usbd, EP_KBD_IN, report, (uint16_t)len);
    }
}

void usb_hid_generic_tx(const uint8_t *report, size_t len)
{
    if (g_usbd != 0 && len > 0U) {
        usbd_ep_write_packet(g_usbd, EP_GENERIC_IN, report, (uint16_t)len);
    }
}

void otg_fs_isr(void)
{
    if (g_usbd != 0) {
        usbd_poll(g_usbd);
    }
}
