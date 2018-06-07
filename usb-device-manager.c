/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include <gtk/gtk.h>
#include "usb-device-manager.h"

// this is the structure behind SpiceUsbDevice
typedef struct _SpiceUsbDeviceInfo {
    guint8  busnum;
    guint8  devaddr;
    guint16 vid;
    guint16 pid;

    gboolean redirecting;
    gboolean cd;
    gboolean connected;

    GPtrArray *luns_array;
} SpiceUsbDevice;

#define SPICE_USB_DEVICE_MANAGER_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SPICE_TYPE_USB_DEVICE_MANAGER, SpiceUsbDeviceManagerPrivate))

struct _SpiceUsbDeviceManagerPrivate {
    guint max_luns;
};

static SpiceUsbDevice _dev_array[] = {
    {
        .vid = 1200, .pid = 12,
        .redirecting = TRUE, .cd = TRUE, .connected = FALSE,
        .luns_array = NULL 
    },
    {
        .vid = 1700, .pid = 17,
        .redirecting = TRUE, .cd = FALSE, .connected = FALSE,
        .luns_array = NULL
    },
    {
        .vid = 1900, .pid = 19,
        .redirecting = FALSE, .cd = FALSE, .connected = FALSE,
        .luns_array = NULL
    },
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

static void spice_usb_device_manager_initable_iface_init(GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE(SpiceUsbDeviceManager, spice_usb_device_manager, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, spice_usb_device_manager_initable_iface_init));

static void spice_usb_device_manager_init(SpiceUsbDeviceManager *self)
{
    SpiceUsbDeviceManagerPrivate *priv;
    priv = SPICE_USB_DEVICE_MANAGER_GET_PRIVATE(self);
    priv->max_luns = 4;
    self->priv = priv;
}

static gboolean spice_usb_device_manager_initable_init(GInitable  *initable,
                                                       GCancellable  *cancellable,
                                                       GError        **err)
{
    SpiceUsbDeviceManager *self = SPICE_USB_DEVICE_MANAGER(initable);
    SpiceUsbDeviceManagerPrivate *priv = self->priv;
    g_print("spice_usb_device_manager_initable_init %p max_luns:%d\n", self, priv->max_luns);

    return TRUE;
}

static void spice_usb_device_manager_initable_iface_init(GInitableIface *iface)
{
    iface->init = spice_usb_device_manager_initable_init;
}

static void spice_usb_device_manager_class_init(SpiceUsbDeviceManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

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
        spice_usb_device_lun_info lun_info;
        guint i;

        _usb_dev_manager = g_initable_new(SPICE_TYPE_USB_DEVICE_MANAGER,
                                          NULL, /* cancellable */
                                          err, /* error */
                                          NULL);

        g_print("alloc mgr:%p\n", _usb_dev_manager);

        _dev_ptr_array = g_ptr_array_new();
        for (i = 0; i < G_N_ELEMENTS(_dev_array); i++) {
            _dev_array[i].busnum = 10 * (i + 1);
            _dev_array[i].devaddr = i + 1;
            /* allocate empty lun array */
            _dev_array[i].luns_array = g_ptr_array_new();
            /* add usb device to the global list */
            g_ptr_array_add(_dev_ptr_array, (gpointer)&_dev_array[i]);
        }

        /* add lun 1 */
        lun_info.file_path = "/home/johnd/iso/fedora-25.iso";
        lun_info.vendor = "RedHat";
        lun_info.product = "Redir DVD";
        lun_info.revision = "1223";
        lun_info.alias = "my_fedora_25";
        lun_info.started = TRUE;
        lun_info.loaded = TRUE;
        lun_info.locked = FALSE;
        
        spice_usb_device_manager_add_cd_lun(_usb_dev_manager, &lun_info);

        /* add lun 2 */
        lun_info.file_path = "/home/johnd/iso/ubuntu-18-04.iso";
        lun_info.alias = "my_ubuntu_18_04";
        
        spice_usb_device_manager_add_cd_lun(_usb_dev_manager, &lun_info);

        _is_initialized = TRUE;
    }
    return _usb_dev_manager;
}

GPtrArray *spice_usb_device_manager_get_devices(SpiceUsbDeviceManager *manager)
{
    return _dev_ptr_array;
}

GPtrArray* spice_usb_device_manager_get_devices_with_filter(
    SpiceUsbDeviceManager *manager, const gchar *filter)
{
    return _dev_ptr_array;
}

guint8 spice_usb_device_get_busnum(const SpiceUsbDevice *device)
{
    g_return_val_if_fail(device != NULL, 0);
    return device->busnum;
}

guint8 spice_usb_device_get_devaddr(const SpiceUsbDevice *device)
{
    g_return_val_if_fail(device != NULL, 0);
    return device->devaddr;
}

guint16 spice_usb_device_get_vid(const SpiceUsbDevice *device)
{
    g_return_val_if_fail(device != NULL, 0);
    return device->vid;
}

guint16 spice_usb_device_get_pid(const SpiceUsbDevice *device)
{
    g_return_val_if_fail(device != NULL, 0);
    return device->pid;
}

void spice_usb_util_get_device_strings(int bus, int address,
                                       int vendor_id, int product_id,
                                       gchar **manufacturer, gchar **product)
{
    *manufacturer = "Spice-USB";
    *product = "Redir-USB";
}

void spice_usb_device_get_strings(const SpiceUsbDevice *device,
                                  gchar **manufacturer, gchar **product)
{
    spice_usb_util_get_device_strings(device->busnum, device->devaddr,
                                      device->vid, device->pid,
                                      manufacturer, product);

}

/**
 * spice_usb_device_get_description:
 * @device: #SpiceUsbDevice to get the description of
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
gchar *spice_usb_device_get_description(SpiceUsbDevice *device, const gchar *format)
{
#ifdef USE_USBREDIR
    guint16 bus, address, vid, pid;
    gchar *description, *descriptor, *manufacturer = NULL, *product = NULL;

    g_return_val_if_fail(device != NULL, NULL);

    bus     = spice_usb_device_get_busnum(device);
    address = spice_usb_device_get_devaddr(device);
    vid     = spice_usb_device_get_vid(device);
    pid     = spice_usb_device_get_pid(device);

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

    g_free(manufacturer);
    g_free(descriptor);
    g_free(product);

    return description;
#else
    return NULL;
#endif
}


gboolean spice_usb_device_manager_is_device_connected(SpiceUsbDeviceManager *manager,
                                                      SpiceUsbDevice *device)
{
    return device->connected;
}

gboolean spice_usb_device_manager_connect_device_sync(SpiceUsbDeviceManager *self,
                                                      SpiceUsbDevice *device)
{
    if (!device->connected) {
        device->connected = TRUE;
        return TRUE;
    } else {
        return FALSE;
    }
}

gboolean spice_usb_device_manager_disconnect_device_sync(SpiceUsbDeviceManager *self,
                                                         SpiceUsbDevice *device)
{
    if (device->connected) {
        device->connected = FALSE;
        return TRUE;
    } else {
        return FALSE;
    }
}

void spice_usb_device_manager_connect_device_async(
                                             SpiceUsbDeviceManager *self,
                                             SpiceUsbDevice *device,
                                             GCancellable *cancellable,
                                             GAsyncReadyCallback callback,
                                             gpointer user_data)
{
    spice_usb_device_manager_connect_device_sync(self, device);
    callback(G_OBJECT(self), NULL, user_data);

}

void spice_usb_device_manager_disconnect_device_async(
                                             SpiceUsbDeviceManager *self,
                                             SpiceUsbDevice *device,
                                             GCancellable *cancellable,
                                             GAsyncReadyCallback callback,
                                             gpointer user_data)
{
    spice_usb_device_manager_disconnect_device_sync(self, device);
    callback(G_OBJECT(self), NULL, user_data);
}

gboolean spice_usb_device_manager_connect_device_finish(
    SpiceUsbDeviceManager *self, GAsyncResult *res, GError **err)
{
    return TRUE;
}

gboolean spice_usb_device_manager_disconnect_device_finish(
    SpiceUsbDeviceManager *self, GAsyncResult *res, GError **err)
{
    return TRUE;
}

gboolean
spice_usb_device_manager_can_redirect_device(SpiceUsbDeviceManager  *self,
                                             SpiceUsbDevice         *device,
                                             GError                **err)
{
    return TRUE;
}

gboolean spice_usb_device_manager_is_redirecting(SpiceUsbDeviceManager *self)
{
    return TRUE;
}

gboolean spice_usb_device_manager_is_device_cd(SpiceUsbDeviceManager *self,
                                               SpiceUsbDevice *device)
{
    return device->cd;
}

gboolean spice_usb_device_manager_device_max_luns(SpiceUsbDeviceManager *self,
                                                  SpiceUsbDevice *device)
{
    SpiceUsbDeviceManagerPrivate *priv = self->priv;
    return priv->max_luns;
}

/* array of guint LUN indices */
GArray *spice_usb_device_manager_get_device_luns(SpiceUsbDeviceManager *self,
                                                 SpiceUsbDevice *device)
{
    GArray *lun_array = g_array_new (FALSE, FALSE, sizeof(guint));
    guint i;

    for (i = 0;  i < device->luns_array->len; i++) {
        g_array_append_val(lun_array, i);
    }

    return lun_array;
}

static void spice_usb_device_manager_copy_lun_info(spice_usb_device_lun_info *new_lun_info,
                                                   spice_usb_device_lun_info *lun_info)
{
    new_lun_info->file_path = g_strdup(lun_info->file_path);
    new_lun_info->vendor = g_strdup(lun_info->vendor);
    new_lun_info->product = g_strdup(lun_info->product);
    new_lun_info->revision = g_strdup(lun_info->revision);
    new_lun_info->alias = g_strdup(lun_info->alias);
    new_lun_info->started = lun_info->started;
    new_lun_info->loaded = lun_info->loaded;
    new_lun_info->locked = lun_info->locked;
}

static void spice_usb_device_manager_add_lun_to_dev(SpiceUsbDevice *dev_info,
                                                    spice_usb_device_lun_info *lun_info,
                                                    gint dev_index, gint lun_index)
{
    spice_usb_device_lun_info *new_lun_info = g_malloc(sizeof(*lun_info));
    spice_usb_device_manager_copy_lun_info(new_lun_info, lun_info);
    g_ptr_array_add(dev_info->luns_array, new_lun_info);
    g_print("add_cd_lun file:%s vendor:%s prod:%s rev:%s alias:%s "
            "started:%d loaded:%d locked:%d - usb dev:%d [%d:%d] as lun:%d\n",
            lun_info->file_path,
            lun_info->vendor,
            lun_info->product,
            lun_info->revision,
            lun_info->alias,
            lun_info->started,
            lun_info->loaded,
            lun_info->locked,
            dev_index,
            (gint)spice_usb_device_get_busnum(dev_info),
            (gint)spice_usb_device_get_devaddr(dev_info),
            lun_index);
}

/* CD LUN will be attached to a (possibly new) USB device automatically */
gboolean spice_usb_device_manager_add_cd_lun(SpiceUsbDeviceManager *self,
                                             spice_usb_device_lun_info *lun_info)
{
    SpiceUsbDeviceManagerPrivate *priv = self->priv;
    guint num_usb_devs = (_dev_ptr_array != NULL) ? _dev_ptr_array->len : 0;
    SpiceUsbDevice *dev_info;
    guint dev_ind, num_luns;

    for (dev_ind = 0; dev_ind < num_usb_devs; dev_ind++) {
        dev_info = g_ptr_array_index(_dev_ptr_array, dev_ind);
        if (!spice_usb_device_manager_is_device_cd(self, dev_info)) {
            continue;
        }
        num_luns = dev_info->luns_array->len;
        if (num_luns < priv->max_luns) {
            spice_usb_device_manager_add_lun_to_dev(dev_info, lun_info, dev_ind, num_luns);
            if (_is_initialized) {
                g_signal_emit(self, signals[DEVICE_CHANGED], 0, dev_info);
            }
            return TRUE;
        }
    }
    /* allocate new usb device */
    dev_info = g_malloc(sizeof(*dev_info));
    /* generate some usb dev info */
    memcpy(dev_info, &_dev_array[0], sizeof(*dev_info));
    dev_info->devaddr ++;
    dev_info->busnum = 10 * dev_info->devaddr;

    dev_info->luns_array = g_ptr_array_new();
    g_ptr_array_add(_dev_ptr_array, (gpointer)dev_info);
    /* add the new LUN to it */
    spice_usb_device_manager_add_lun_to_dev(dev_info, lun_info, num_usb_devs, 0);
    if (_is_initialized) {
        g_signal_emit(self, signals[DEVICE_ADDED], 0, dev_info);
    }
    return TRUE;
}

/* Get CD LUN info, intended primarily for enumerating LUNs */
gboolean
spice_usb_device_manager_device_lun_get_info(SpiceUsbDeviceManager *self,
                                             SpiceUsbDevice *device,
                                             guint lun,
                                             spice_usb_device_lun_info *lun_info)
{
    spice_usb_device_lun_info *req_lun_info;

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
                                         SpiceUsbDevice *device,
                                         guint lun,
                                         gboolean lock)
{
    spice_usb_device_lun_info *req_lun_info;

    if (lun >= device->luns_array->len) {
        return FALSE;
    }
    req_lun_info = g_ptr_array_index(device->luns_array, lun);

    if (!req_lun_info->locked && lock) {
        req_lun_info->locked = TRUE;
        return TRUE;
    } else if (req_lun_info->locked && !lock) {
        req_lun_info->locked = FALSE;
        return TRUE;
    } else {
        return FALSE;
    }
}

/* load or eject device */
gboolean
spice_usb_device_manager_device_lun_load(SpiceUsbDeviceManager *self,
                                         SpiceUsbDevice *device,
                                         guint lun,
                                         gboolean load)
{
    spice_usb_device_lun_info *req_lun_info;

    if (lun >= device->luns_array->len) {
        return FALSE;
    }
    req_lun_info = g_ptr_array_index(device->luns_array, lun);
 
    if (!req_lun_info->loaded && load) {
        req_lun_info->loaded = TRUE;
        return TRUE;
    } else if (req_lun_info->loaded && !load) {
        req_lun_info->loaded = FALSE;
        return TRUE;
    } else {
        return FALSE;
    }
}

/* change the media - device must be not currently loaded */

gboolean
spice_usb_device_manager_device_lun_change_media(SpiceUsbDeviceManager *self,
                                                 SpiceUsbDevice *device,
                                                 guint lun,
                                                 gchar *filename)
{
    spice_usb_device_lun_info *req_lun_info;

    if (lun >= device->luns_array->len) {
        return FALSE;
    }
    req_lun_info = g_ptr_array_index(device->luns_array, lun);

    if (!req_lun_info->loaded) {
        if (req_lun_info->file_path != NULL) {
            g_free((gpointer)req_lun_info->file_path);
        }
        req_lun_info->file_path = g_strdup(filename);
        return TRUE;
    } else {
        return FALSE;
    }
}

/* remove lun from the usb device */
gboolean
spice_usb_device_manager_device_lun_remove(SpiceUsbDeviceManager *self,
                                           SpiceUsbDevice *device,
                                           guint lun)
{
    spice_usb_device_lun_info *req_lun_info;

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
