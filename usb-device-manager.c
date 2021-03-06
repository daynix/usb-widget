/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include <config.h>
#include <gtk/gtk.h>
#include <string.h>
#include "spice-client.h"

// this is the structure behind SpiceUsbDevice
typedef struct _SpiceUsbDeviceInfo {
    gint ref;

    guint8  busnum;
    guint8  devaddr;
    guint16 vid;
    guint16 pid;

    gboolean redirecting;
    gboolean cd;
    gboolean connected;

    GPtrArray *luns_array;
} SpiceUsbDeviceInfo;

#define SPICE_USB_DEVICE_MANAGER_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SPICE_TYPE_USB_DEVICE_MANAGER, SpiceUsbDeviceManagerPrivate))

struct _SpiceUsbDeviceManagerPrivate {
    SpiceSession *session;
    guint max_luns;
    gint free_channels;
    gboolean auto_connect;
    gchar *auto_connect_filter;
    gchar *redirect_on_connect;
};

static SpiceUsbDeviceInfo _dev_array[] = {
    {
        .vid = 1200, .pid = 12,
        .redirecting = TRUE, .cd = TRUE, .connected = TRUE,
        .luns_array = NULL 
    },
    {
        .vid = 1700, .pid = 17,
        .redirecting = TRUE, .cd = FALSE, .connected = TRUE,
        .luns_array = NULL
    },
    {
        .vid = 1900, .pid = 19,
        .redirecting = FALSE, .cd = FALSE, .connected = FALSE,
        .luns_array = NULL
    },
};

enum {
    PROP_0,
    PROP_SESSION,
    PROP_AUTO_CONNECT,
    PROP_AUTO_CONNECT_FILTER,
    PROP_REDIRECT_ON_CONNECT,
    PROP_FREE_CHANNELS,
    PROP_SHARE_CD
};

enum
{
    DEVICE_ADDED,
    DEVICE_REMOVED,
    DEVICE_CHANGED,
    //AUTO_CONNECT_FAILED,
    DEVICE_ERROR,
    LAST_SIGNAL,
};

static SpiceUsbDeviceManager *_usb_dev_manager;
static gboolean _is_initialized = FALSE;
static GPtrArray *_dev_ptr_array = NULL;
static guint signals[LAST_SIGNAL] = { 0, };

static SpiceUsbDevice *spice_usb_device_ref(SpiceUsbDevice *dev_handle)
{
    SpiceUsbDeviceInfo *device = (SpiceUsbDeviceInfo *)dev_handle;

    g_return_val_if_fail(device != NULL, NULL);
    g_atomic_int_inc(&device->ref);
    return dev_handle;
}

static void spice_usb_device_unref(SpiceUsbDevice *dev_handle)
{
    SpiceUsbDeviceInfo *device = (SpiceUsbDeviceInfo *)dev_handle;
    gboolean ref_count_is_0;

    g_return_if_fail(device != NULL);

    ref_count_is_0 = g_atomic_int_dec_and_test(&device->ref);
    if (ref_count_is_0) {
        device->vid = device->pid = 0;
        SPICE_DEBUG("%s: deleting %p", __FUNCTION__, device);
        g_free(device);
    }
}

G_DEFINE_BOXED_TYPE(SpiceUsbDevice, spice_usb_device,
                    (GBoxedCopyFunc)spice_usb_device_ref,
                    (GBoxedFreeFunc)spice_usb_device_unref)

static void spice_usb_device_manager_initable_iface_init(GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE(SpiceUsbDeviceManager, spice_usb_device_manager, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, spice_usb_device_manager_initable_iface_init));

static void spice_usb_device_manager_init(SpiceUsbDeviceManager *self)
{
    SpiceUsbDeviceManagerPrivate *priv;
    priv = SPICE_USB_DEVICE_MANAGER_GET_PRIVATE(self);
    priv->max_luns = 4;
    priv->free_channels = 1;
    self->priv = priv;
}

static gboolean spice_usb_device_manager_initable_init(GInitable  *initable,
                                                       GCancellable  *cancellable,
                                                       GError        **err)
{
    SpiceUsbDeviceManager *self = SPICE_USB_DEVICE_MANAGER(initable);
    SpiceUsbDeviceManagerPrivate *priv = self->priv;
    g_print("spice_usb_device_manager_initable_init %p max_luns:%u\n", self, priv->max_luns);

    return TRUE;
}

static void spice_usb_device_manager_initable_iface_init(GInitableIface *iface)
{
    iface->init = spice_usb_device_manager_initable_init;
}

static void spice_usb_device_manager_get_property(GObject     *gobject,
                                                  guint        prop_id,
                                                  GValue      *value,
                                                  GParamSpec  *pspec)
{
    SpiceUsbDeviceManager *self = SPICE_USB_DEVICE_MANAGER(gobject);
    SpiceUsbDeviceManagerPrivate *priv = self->priv;

    switch (prop_id) {
    case PROP_SESSION:
        g_value_set_object(value, priv->session);
        break;
    case PROP_AUTO_CONNECT:
        g_value_set_boolean(value, priv->auto_connect);
        break;
    case PROP_AUTO_CONNECT_FILTER:
        g_value_set_string(value, priv->auto_connect_filter);
        break;
    case PROP_REDIRECT_ON_CONNECT:
        g_value_set_string(value, priv->redirect_on_connect);
        break;
    case PROP_SHARE_CD:
        /* get_property is not needed */
        g_value_set_string(value, "");
        break;
    case PROP_FREE_CHANNELS: {
#if 0
        int i;
        for (i = 0; i < priv->channels->len; i++) {
            SpiceUsbredirChannel *channel = g_ptr_array_index(priv->channels, i);

            if (!spice_usbredir_channel_get_device(channel))
                free_channels++;
        }
#endif
        g_value_set_int(value, priv->free_channels);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, prop_id, pspec);
        break;
    }
}

static void spice_usb_device_manager_set_property(GObject       *gobject,
                                                  guint          prop_id,
                                                  const GValue  *value,
                                                  GParamSpec    *pspec)
{
    SpiceUsbDeviceManager *self = SPICE_USB_DEVICE_MANAGER(gobject);
    SpiceUsbDeviceManagerPrivate *priv = self->priv;

    switch (prop_id) {
    case PROP_SESSION:
        priv->session = g_value_get_object(value);
        break;
    case PROP_AUTO_CONNECT:
        priv->auto_connect = g_value_get_boolean(value);
        break;
    case PROP_AUTO_CONNECT_FILTER: {
        const gchar *filter = g_value_get_string(value);
        g_free(priv->auto_connect_filter);
        priv->auto_connect_filter = g_strdup(filter);
        break;
    }
    case PROP_REDIRECT_ON_CONNECT: {
        const gchar *filter = g_value_get_string(value);
        g_free(priv->redirect_on_connect);
        priv->redirect_on_connect = g_strdup(filter);
        break;
    }
    case PROP_SHARE_CD:
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, prop_id, pspec);
        break;
    }
}

static void spice_usb_device_manager_class_init(SpiceUsbDeviceManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GParamSpec *pspec;

    /* Add properties */
    gobject_class->get_property = spice_usb_device_manager_get_property;
    gobject_class->set_property = spice_usb_device_manager_set_property;

    /* session */
    g_object_class_install_property
        (gobject_class, PROP_SESSION,
         g_param_spec_object("session",
                             "Session",
                             "SpiceSession",
                             SPICE_TYPE_SESSION,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));

    /* auto-connect */
    pspec = g_param_spec_boolean("auto-connect", "Auto Connect",
                                 "Auto connect plugged in USB devices",
                                 FALSE,
                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(gobject_class, PROP_AUTO_CONNECT, pspec);

    /* auto-connect-filter */
    pspec = g_param_spec_string("auto-connect-filter", "Auto Connect Filter ",
               "Filter determining which USB devices to auto connect",
               "0x03,-1,-1,-1,0|-1,-1,-1,-1,1",
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(gobject_class, PROP_AUTO_CONNECT_FILTER,
                                    pspec);

    /* redirect-on-connect */
    pspec = g_param_spec_string("redirect-on-connect", "Redirect on connect",
               "Filter selecting USB devices to redirect on connect", NULL,
               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(gobject_class, PROP_REDIRECT_ON_CONNECT,
                                    pspec);

    /* cd-share */
    pspec = g_param_spec_string("cd-share", "Share ISO file as CD",
        "File nameto share", NULL,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(gobject_class, PROP_SHARE_CD, pspec);

    /* free-channels */
    pspec = g_param_spec_int("free-channels", "Free channels",
               "The number of available channels for redirecting USB devices",
               0,
               G_MAXINT,
               0,
               G_PARAM_READABLE);
    g_object_class_install_property(gobject_class, PROP_FREE_CHANNELS,
                                    pspec);

    /* Add signals */
    signals[DEVICE_ADDED] =
        g_signal_new("device-added",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceUsbDeviceManagerClass, device_added),
                     NULL, NULL, /* accumulator */
                     g_cclosure_marshal_VOID__BOXED,
                     G_TYPE_NONE, /* return value */
                     1,
                     SPICE_TYPE_USB_DEVICE);

    signals[DEVICE_REMOVED] =
        g_signal_new("device-removed",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceUsbDeviceManagerClass, device_removed),
                     NULL, NULL, /* accumulator */
                     g_cclosure_marshal_VOID__BOXED,
                     G_TYPE_NONE,
                     1,
                     SPICE_TYPE_USB_DEVICE);

    signals[DEVICE_CHANGED] =
        g_signal_new("device-changed",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceUsbDeviceManagerClass, device_changed),
                     NULL, NULL, /* accumulator */
                     g_cclosure_marshal_VOID__BOXED,
                     G_TYPE_NONE, /* return value */
                     1,
                     SPICE_TYPE_USB_DEVICE);

    signals[DEVICE_ERROR] =
        g_signal_new("device-error",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceUsbDeviceManagerClass, device_error),
                     NULL, NULL, /* accumulator */
                     g_cclosure_marshal_VOID__BOXED,
                     G_TYPE_NONE, /* return value */
                     2,
                     SPICE_TYPE_USB_DEVICE,
                     G_TYPE_ERROR);

    g_type_class_add_private(klass, sizeof(SpiceUsbDeviceManagerPrivate));
}

SpiceUsbDeviceManager *spice_usb_device_manager_get(SpiceSession *session,
                                                    GError **err)
{
    if (!_is_initialized) {
        SpiceUsbDeviceLunInfo lun_info;
        guint i;

        _usb_dev_manager = g_initable_new(SPICE_TYPE_USB_DEVICE_MANAGER,
                                          NULL, /* cancellable */
                                          err, /* error */
                                          NULL);

        g_print("alloc mgr:%p\n", _usb_dev_manager);

        _dev_ptr_array = g_ptr_array_new();
        for (i = 0; i < G_N_ELEMENTS(_dev_array); i++) {
            SpiceUsbDeviceInfo *device;
            /* allocate new usb device and copy the pre-set device */
            device = g_malloc(sizeof(*device));
            memcpy(device, &_dev_array[i], sizeof(*device));
            device->ref = 0;
            device->busnum = 10 * (i + 1);
            device->devaddr = i + 1;
            /* allocate empty lun array */
            device->luns_array = g_ptr_array_new();
            /* add usb device to the global list */
            g_ptr_array_add(_dev_ptr_array, (gpointer)device);
            spice_usb_device_ref((SpiceUsbDevice *)device);
        }

        /* add lun 1 */
        lun_info.file_path = "/home/johnd/iso/fedora-25.iso";
        lun_info.vendor = "RedHat";
        lun_info.product = "Redir DVD";
        lun_info.revision = "1223";
        lun_info.started = TRUE;
        lun_info.loaded = TRUE;
        lun_info.locked = FALSE;
        
        spice_usb_device_manager_add_cd_lun(_usb_dev_manager, &lun_info);

        /* add lun 2 */
        lun_info.file_path = "/home/johnd/iso/ubuntu-18-04.iso";
        
        spice_usb_device_manager_add_cd_lun(_usb_dev_manager, &lun_info);

        _is_initialized = TRUE;
    }
    return _usb_dev_manager;
}

GPtrArray *spice_usb_device_manager_get_devices(SpiceUsbDeviceManager *manager)
{
    g_ptr_array_ref(_dev_ptr_array);
    return _dev_ptr_array;
}

GPtrArray* spice_usb_device_manager_get_devices_with_filter(
    SpiceUsbDeviceManager *manager, const gchar *filter)
{
    return _dev_ptr_array;
}

guint8 spice_usb_device_get_busnum(const SpiceUsbDevice *dev_handle)
{
    const SpiceUsbDeviceInfo *device = (const SpiceUsbDeviceInfo *)dev_handle;
    g_return_val_if_fail(device != NULL, 0);
    return device->busnum;
}

guint8 spice_usb_device_get_devaddr(const SpiceUsbDevice *dev_handle)
{
    const SpiceUsbDeviceInfo *device = (const SpiceUsbDeviceInfo *)dev_handle;
    g_return_val_if_fail(device != NULL, 0);
    return device->devaddr;
}

guint16 spice_usb_device_get_vid(const SpiceUsbDevice *dev_handle)
{
    const SpiceUsbDeviceInfo *device = (const SpiceUsbDeviceInfo *)dev_handle;
    g_return_val_if_fail(device != NULL, 0);
    return device->vid;
}

guint16 spice_usb_device_get_pid(const SpiceUsbDevice *dev_handle)
{
    const SpiceUsbDeviceInfo *device = (const SpiceUsbDeviceInfo *)dev_handle;
    g_return_val_if_fail(device != NULL, 0);
    return device->pid;
}

void spice_usb_util_get_device_strings(int bus, int address,
                                       int vendor_id, int product_id,
                                       gchar **manufacturer, gchar **product)
{
    *manufacturer = g_strdup("RedHat-Spice");
    *product = g_strdup("Redir-USB");
}

void spice_usb_device_get_info(SpiceUsbDeviceManager *manager,
                               SpiceUsbDevice *dev_handle,
                               SpiceUsbDeviceDescription *dev_descr)
{
    const SpiceUsbDeviceInfo *device = (const SpiceUsbDeviceInfo *)dev_handle;
    g_return_if_fail(device != NULL);

    dev_descr->bus = spice_usb_device_get_busnum(dev_handle);
    dev_descr->address = spice_usb_device_get_devaddr(dev_handle);
    dev_descr->vendor_id = spice_usb_device_get_vid(dev_handle);
    dev_descr->product_id = spice_usb_device_get_pid(dev_handle);

    spice_usb_util_get_device_strings(dev_descr->bus, dev_descr->address,
        dev_descr->vendor_id, dev_descr->product_id, &dev_descr->vendor, &dev_descr->product);
}

/**
 * spice_usb_device_get_description:
 * @device: #SpiceUsbDeviceInfo to get the description of
 * @format: (allow-none): an optional printf() format string with
 * positional parameters
 *
 * Get a string describing the device which is suitable as a description of
 * the device for the end user. The returned string should be freed with
 * g_free() when no longer needed.
 *
 * The @format positional parameters are the following:
 * - '%%1$s' manufacturer
 * - '%%2$s' product
 * - '%%3$s' descriptor (a [vendor_id:product_id] string)
 * - '%%4$d' bus
 * - '%%5$d' address
 *
 * (the default format string is "%%s %%s %%s at %%d-%%d")
 *
 * Returns: a newly-allocated string holding the description, or %NULL if failed
 */
gchar *spice_usb_device_get_description(SpiceUsbDevice *dev_handle, const gchar *format)
{
    const SpiceUsbDeviceInfo *device = (const SpiceUsbDeviceInfo *)dev_handle;
    guint16 bus, address, vid, pid;
    gchar *description, *descriptor, *manufacturer = NULL, *product = NULL;

    g_return_val_if_fail(device != NULL, NULL);

    bus     = spice_usb_device_get_busnum(dev_handle);
    address = spice_usb_device_get_devaddr(dev_handle);
    vid     = spice_usb_device_get_vid(dev_handle);
    pid     = spice_usb_device_get_pid(dev_handle);

    if ((vid > 0) && (pid > 0)) {
        descriptor = g_strdup_printf("[%04x:%04x]", vid, pid);
    } else {
        descriptor = g_strdup("");
    }

    spice_usb_util_get_device_strings(bus, address, vid, pid,
                                      &manufacturer, &product);

    if (!format)
        format = _("%s %s %s at %d-%d");

    description = g_strdup_printf(format, manufacturer, product, descriptor, bus, address);

    g_free(descriptor);

    return description;
}


gboolean spice_usb_device_manager_is_device_connected(SpiceUsbDeviceManager *manager,
                                                      SpiceUsbDevice *dev_handle)
{
    const SpiceUsbDeviceInfo *device = (const SpiceUsbDeviceInfo *)dev_handle;
    return device->connected;
}

gboolean spice_usb_device_manager_connect_device_sync(SpiceUsbDeviceManager *self,
                                                      SpiceUsbDevice *dev_handle)
{
    SpiceUsbDeviceManagerPrivate *priv = self->priv;
    SpiceUsbDeviceInfo *device = (SpiceUsbDeviceInfo *)dev_handle;

    if (!device->connected) {
        if (priv->free_channels > 0) {
            priv->free_channels--;
            device->connected = TRUE;
            return TRUE;
        }
    }
    return FALSE;
}

gboolean spice_usb_device_manager_disconnect_device_sync(SpiceUsbDeviceManager *self,
                                                         SpiceUsbDevice *dev_handle)
{
    SpiceUsbDeviceManagerPrivate *priv = self->priv;
    SpiceUsbDeviceInfo *device = (SpiceUsbDeviceInfo *)dev_handle;

    if (device->connected) {
        device->connected = FALSE;
        priv->free_channels++;
        return TRUE;
    } else {
        return FALSE;
    }
}

void spice_usb_device_manager_connect_device_async(
                                             SpiceUsbDeviceManager *self,
                                             SpiceUsbDevice *dev_handle,
                                             GCancellable *cancellable,
                                             GAsyncReadyCallback callback,
                                             gpointer user_data)
{
    GAsyncResult *res = (GAsyncResult *)((gpointer)dev_handle); /* provides device access in x_finish() */

    spice_usb_device_manager_connect_device_sync(self, dev_handle);
    callback(G_OBJECT(self), res, user_data);
}

void spice_usb_device_manager_disconnect_device_async(
                                             SpiceUsbDeviceManager *self,
                                             SpiceUsbDevice *dev_handle,
                                             GCancellable *cancellable,
                                             GAsyncReadyCallback callback,
                                             gpointer user_data)
{
    GAsyncResult *res = (GAsyncResult *)((gpointer)dev_handle); /* provides device access in x_finish() */

    spice_usb_device_manager_disconnect_device_sync(self, dev_handle);
    callback(G_OBJECT(self), res, user_data);
}

gboolean spice_usb_device_manager_connect_device_finish(
    SpiceUsbDeviceManager *self, GAsyncResult *res, GError **err)
{
    SpiceUsbDeviceInfo *device = (SpiceUsbDeviceInfo *)((gpointer)res);

    if (device->connected) {
        if (err) {
            *err = NULL;
        }
        return TRUE;
    } else {
        if (err) {
            *err = g_error_new_literal(g_quark_from_static_string("connect"), 1, "Failed to connect");
        }
        return FALSE;
    }
}

gboolean spice_usb_device_manager_disconnect_device_finish(
    SpiceUsbDeviceManager *self, GAsyncResult *res, GError **err)
{
    SpiceUsbDeviceInfo *device = (SpiceUsbDeviceInfo *)((gpointer)res);

    if (!device->connected) {
        if (err) {
            *err = NULL;
        }
        return TRUE;
    } else {
        if (err) {
            *err = g_error_new_literal(g_quark_from_static_string("disconnect"), 1, "Failed to disconnect");
        }
        return FALSE;
    }
}

gboolean
spice_usb_device_manager_can_redirect_device(SpiceUsbDeviceManager *self,
                                             SpiceUsbDevice *dev_handle,
                                             GError **err)
{
    return TRUE;
}

gboolean spice_usb_device_manager_is_redirecting(SpiceUsbDeviceManager *self)
{
    return FALSE;
}

guint spice_usb_device_manager_is_device_cd(SpiceUsbDeviceManager *self,
                                            SpiceUsbDevice *dev_handle)
{
    SpiceUsbDeviceManagerPrivate *priv = self->priv;
    const SpiceUsbDeviceInfo *device = (const SpiceUsbDeviceInfo *)dev_handle;

    return device->cd ? priv->max_luns : 0;
}

gboolean spice_usb_device_manager_device_max_luns(SpiceUsbDeviceManager *self,
                                                  SpiceUsbDevice *dev_handle)
{
    SpiceUsbDeviceManagerPrivate *priv = self->priv;
    return priv->max_luns;
}

/* array of guint LUN indices */
GArray *spice_usb_device_manager_get_device_luns(SpiceUsbDeviceManager *self,
                                                 SpiceUsbDevice *dev_handle)
{
    SpiceUsbDeviceInfo *device = (SpiceUsbDeviceInfo *)dev_handle;
    GArray *lun_array = g_array_new(FALSE, FALSE, sizeof(guint));
    guint i;

    for (i = 0; i < device->luns_array->len; i++) {
        g_array_append_val(lun_array, i);
    }

    return lun_array;
}

static void spice_usb_device_manager_copy_lun_info(SpiceUsbDeviceLunInfo *new_lun_info,
                                                   SpiceUsbDeviceLunInfo *lun_info)
{
    new_lun_info->file_path = g_strdup(lun_info->file_path);
    new_lun_info->vendor = g_strdup(lun_info->vendor);
    new_lun_info->product = g_strdup(lun_info->product);
    new_lun_info->revision = g_strdup(lun_info->revision);
    new_lun_info->started = lun_info->started;
    new_lun_info->loaded = lun_info->loaded;
    new_lun_info->locked = lun_info->locked;
}

static void spice_usb_device_manager_add_lun_to_dev(SpiceUsbDevice *dev_handle,
                                                    SpiceUsbDeviceLunInfo *lun_info,
                                                    gint dev_index, gint lun_index)
{
    SpiceUsbDeviceInfo *device = (SpiceUsbDeviceInfo *)dev_handle;
    SpiceUsbDeviceLunInfo *new_lun_info = g_malloc(sizeof(*lun_info));
    spice_usb_device_manager_copy_lun_info(new_lun_info, lun_info);
    g_ptr_array_add(device->luns_array, new_lun_info);
    g_print("add_cd_lun file:%s vendor:%s prod:%s rev:%s  "
            "started:%d loaded:%d locked:%d - usb dev:%d [%d:%d] as lun:%d\n",
            lun_info->file_path,
            lun_info->vendor,
            lun_info->product,
            lun_info->revision,
            lun_info->started,
            lun_info->loaded,
            lun_info->locked,
            dev_index,
            (gint)spice_usb_device_get_busnum(dev_handle),
            (gint)spice_usb_device_get_devaddr(dev_handle),
            lun_index);
}

/* CD LUN will be attached to a (possibly new) USB device automatically */
gboolean spice_usb_device_manager_add_cd_lun(SpiceUsbDeviceManager *self,
                                             SpiceUsbDeviceLunInfo *lun_info)
{
    SpiceUsbDeviceManagerPrivate *priv = self->priv;
    guint num_usb_devs = (_dev_ptr_array != NULL) ? _dev_ptr_array->len : 0;
    SpiceUsbDeviceInfo *device;
    guint dev_ind, num_luns;

    for (dev_ind = 0; dev_ind < num_usb_devs; dev_ind++) {
        device = g_ptr_array_index(_dev_ptr_array, dev_ind);
        if (!spice_usb_device_manager_is_device_cd(self, (SpiceUsbDevice *)device)) {
            continue;
        }
        num_luns = device->luns_array->len;
        if (num_luns < priv->max_luns) {
            spice_usb_device_manager_add_lun_to_dev((SpiceUsbDevice *)device,
                                                    lun_info, dev_ind, num_luns);
            if (_is_initialized) {
                g_signal_emit(self, signals[DEVICE_CHANGED], 0, device);
            }
            return TRUE;
        }
    }
    /* allocate new usb device */
    device = g_malloc(sizeof(*device));
    /* generate some usb dev info */
    memcpy(device, &_dev_array[0], sizeof(*device));
    device->ref = 0;
    device->devaddr = num_usb_devs + 1;
    device->busnum = 10 * device->devaddr;
    device->connected = FALSE;

    device->luns_array = g_ptr_array_new();
    g_ptr_array_add(_dev_ptr_array, (gpointer)device);
    spice_usb_device_ref((SpiceUsbDevice *)device);

    /* add the new LUN to it */
    spice_usb_device_manager_add_lun_to_dev((SpiceUsbDevice *)device, lun_info, num_usb_devs, 0);
    if (_is_initialized) {
        g_signal_emit(self, signals[DEVICE_ADDED], 0, device);
    }
    return TRUE;
}

/* Get CD LUN info, intended primarily for enumerating LUNs */
gboolean
spice_usb_device_manager_device_lun_get_info(SpiceUsbDeviceManager *self,
                                             SpiceUsbDevice *dev_handle,
                                             guint lun,
                                             SpiceUsbDeviceLunInfo *lun_info)
{
    const SpiceUsbDeviceInfo *device = (const SpiceUsbDeviceInfo *)dev_handle;
    SpiceUsbDeviceLunInfo *req_lun_info;

    if (lun >= device->luns_array->len) {
        return FALSE;
    }
    req_lun_info = g_ptr_array_index(device->luns_array, lun);
    spice_usb_device_manager_copy_lun_info(lun_info, req_lun_info);
    return TRUE;
}

/* lock or unlock device */
gboolean
spice_usb_device_manager_device_lun_lock(SpiceUsbDeviceManager *self,
                                         SpiceUsbDevice *dev_handle,
                                         guint lun,
                                         gboolean lock)
{
    const SpiceUsbDeviceInfo *device = (const SpiceUsbDeviceInfo *)dev_handle;
    SpiceUsbDeviceLunInfo *req_lun_info;

    if (lun >= device->luns_array->len) {
        return FALSE;
    }
    req_lun_info = g_ptr_array_index(device->luns_array, lun);

    if (!req_lun_info->locked && lock) {
        req_lun_info->locked = TRUE;
    } else if (req_lun_info->locked && !lock) {
        req_lun_info->locked = FALSE;
    } else {
        return FALSE;
    }
    g_signal_emit(self, signals[DEVICE_CHANGED], 0, device);
    return TRUE;
}

/* load or eject device */
gboolean
spice_usb_device_manager_device_lun_load(SpiceUsbDeviceManager *self,
                                         SpiceUsbDevice *dev_handle,
                                         guint lun,
                                         gboolean load)
{
    const SpiceUsbDeviceInfo *device = (const SpiceUsbDeviceInfo *)dev_handle;
    SpiceUsbDeviceLunInfo *req_lun_info;

    if (lun >= device->luns_array->len) {
        return FALSE;
    }
    req_lun_info = g_ptr_array_index(device->luns_array, lun);
 
    if (!req_lun_info->loaded && load) {
        req_lun_info->loaded = TRUE;
        g_signal_emit(self, signals[DEVICE_CHANGED], 0, device);
    } else if (req_lun_info->loaded && !load) {
        req_lun_info->loaded = FALSE;
    } else {
        return FALSE;
    }
    g_signal_emit(self, signals[DEVICE_CHANGED], 0, device);
    return TRUE;
}

/* change the media - device must be not currently loaded */

gboolean
spice_usb_device_manager_device_lun_change_media(SpiceUsbDeviceManager *self,
                                                 SpiceUsbDevice *dev_handle,
                                                 guint lun,
                                                 const SpiceUsbDeviceLunInfo *lun_info)
{
    const SpiceUsbDeviceInfo *device = (const SpiceUsbDeviceInfo *)dev_handle;
    SpiceUsbDeviceLunInfo *req_lun_info;

    if (lun >= device->luns_array->len) {
        return FALSE;
    }
    req_lun_info = g_ptr_array_index(device->luns_array, lun);

    if (!req_lun_info->loaded) {
        if (req_lun_info->file_path != NULL) {
            g_free((gpointer)req_lun_info->file_path);
        }
        req_lun_info->file_path = g_strdup(lun_info->file_path);
        g_signal_emit(self, signals[DEVICE_CHANGED], 0, device);
        return TRUE;
    } else {
        return FALSE;
    }
}

/* remove lun from the usb device */
gboolean
spice_usb_device_manager_device_lun_remove(SpiceUsbDeviceManager *self,
                                           SpiceUsbDevice *dev_handle,
                                           guint lun)
{
    const SpiceUsbDeviceInfo *device = (const SpiceUsbDeviceInfo *)dev_handle;
    SpiceUsbDeviceLunInfo *req_lun_info;

    if (lun >= device->luns_array->len) {
        return FALSE;
    }

    req_lun_info = g_ptr_array_index(device->luns_array, lun);
    g_ptr_array_remove_index(device->luns_array, lun);
    g_free(req_lun_info);

    if (device->luns_array->len == 0) {
        g_ptr_array_remove(_dev_ptr_array, (gpointer)device);
        if (_is_initialized) {
            g_signal_emit(self, signals[DEVICE_REMOVED], 0, device);
        }
    } else {
        if (_is_initialized) {
            g_signal_emit(self, signals[DEVICE_CHANGED], 0, device);
        }
    }
    return TRUE;
}
