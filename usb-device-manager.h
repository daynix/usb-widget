/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2011, 2012 Red Hat, Inc.
 
   Stub header file modeled after spice-gtk header

   Red Hat Authors:
   Hans de Goede <hdegoede@redhat.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#ifndef __SPICE_USB_DEVICE_MANAGER_H__
#define __SPICE_USB_DEVICE_MANAGER_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define SPICE_TYPE_USB_DEVICE_MANAGER            (spice_usb_device_manager_get_type ())
#define SPICE_USB_DEVICE_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SPICE_TYPE_USB_DEVICE_MANAGER, SpiceUsbDeviceManager))
#define SPICE_USB_DEVICE_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SPICE_TYPE_USB_DEVICE_MANAGER, SpiceUsbDeviceManagerClass))
#define SPICE_IS_USB_DEVICE_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SPICE_TYPE_USB_DEVICE_MANAGER))
#define SPICE_IS_USB_DEVICE_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SPICE_TYPE_USB_DEVICE_MANAGER))
#define SPICE_USB_DEVICE_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SPICE_TYPE_USB_DEVICE_MANAGER, SpiceUsbDeviceManagerClass))

typedef struct _SpiceUsbDeviceManager SpiceUsbDeviceManager;
typedef struct _SpiceUsbDeviceManagerClass SpiceUsbDeviceManagerClass;
typedef struct _SpiceUsbDeviceManagerPrivate SpiceUsbDeviceManagerPrivate;

typedef struct _SpiceUsbDeviceInfo SpiceUsbDevice;

struct _SpiceUsbDeviceManager
{
    GObject parent;

    /*< private >*/
    SpiceUsbDeviceManagerPrivate *priv;
    /* Do not add fields to this struct */
};

struct _SpiceUsbDeviceManagerClass
{
    GObjectClass parent_class;

    /* signals */
    void (*device_added) (SpiceUsbDeviceManager *manager,
                          SpiceUsbDevice *device);
    void (*device_removed) (SpiceUsbDeviceManager *manager,
                            SpiceUsbDevice *device);
    void (*device_changed) (SpiceUsbDeviceManager *manager,
                            SpiceUsbDevice *device);
    void (*device_error) (SpiceUsbDeviceManager *manager,
                          SpiceUsbDevice *device, GError *error);
    /*< private >*/
};

typedef guint SpiceSession;

gchar *spice_usb_device_get_description(SpiceUsbDevice *device, const gchar *format);

SpiceUsbDeviceManager *spice_usb_device_manager_get(SpiceSession *session,
                                                    GError **err);

GPtrArray *spice_usb_device_manager_get_devices(SpiceUsbDeviceManager *manager);
GPtrArray* spice_usb_device_manager_get_devices_with_filter(
    SpiceUsbDeviceManager *manager, const gchar *filter);

gboolean spice_usb_device_manager_is_device_connected(SpiceUsbDeviceManager *manager,
                                                      SpiceUsbDevice *device);

/* should be replaced with async versions */
gboolean spice_usb_device_manager_connect_device_sync(SpiceUsbDeviceManager *self,
                                                  SpiceUsbDevice *device);

gboolean spice_usb_device_manager_disconnect_device_sync(SpiceUsbDeviceManager *self,
                                                     SpiceUsbDevice *device);

guint8 spice_usb_device_get_busnum(const SpiceUsbDevice *device);
guint8 spice_usb_device_get_devaddr(const SpiceUsbDevice *device);
guint16 spice_usb_device_get_vid(const SpiceUsbDevice *device);
guint16 spice_usb_device_get_pid(const SpiceUsbDevice *device);
void spice_usb_device_get_strings(const SpiceUsbDevice *device,
                                  gchar **manufacturer, gchar **product);

/* redirect interface */
gboolean
spice_usb_device_manager_can_redirect_device(SpiceUsbDeviceManager  *self,
                                             SpiceUsbDevice         *device,
                                             GError                **err);

gboolean spice_usb_device_manager_is_redirecting(SpiceUsbDeviceManager *self);

gboolean spice_usb_device_manager_is_device_cd(SpiceUsbDeviceManager *self,
                                               SpiceUsbDevice *device);

gboolean spice_usb_device_manager_device_max_luns(SpiceUsbDeviceManager *self,
                                                  SpiceUsbDevice *device);

/* array of guint LUN indices */
GArray *spice_usb_device_manager_get_device_luns(SpiceUsbDeviceManager *self,
                                                 SpiceUsbDevice *device);

typedef struct _spice_usb_device_lun_info
{
    const gchar *file_path;

    const gchar *vendor;
    const gchar *product;
    const gchar *revision;

    const gchar *alias;

    gboolean started;
    gboolean loaded;
    gboolean locked;
} spice_usb_device_lun_info;

/* CD LUN will be attached to a (possibly new) USB device automatically */
gboolean spice_usb_device_manager_add_cd_lun(SpiceUsbDeviceManager *self,
                                             spice_usb_device_lun_info *lun_info);

/* Get CD LUN info, intended primarily for enumerating LUNs */
gboolean
spice_usb_device_manager_device_lun_get_info(SpiceUsbDeviceManager *self,
                                             SpiceUsbDevice *device,
                                             guint lun,
                                             spice_usb_device_lun_info *lun_info);
/* lock or unlock device */
gboolean
spice_usb_device_manager_device_lun_lock(SpiceUsbDeviceManager *self,
                                         SpiceUsbDevice *device,
                                         guint lun,
                                         gboolean lock);

/* load or eject device */
gboolean
spice_usb_device_manager_device_lun_load(SpiceUsbDeviceManager *self,
                                         SpiceUsbDevice *device,
                                         guint lun,
                                         gboolean load);

/* change the media - device must be not currently loaded */

gboolean
spice_usb_device_manager_device_lun_change_media(SpiceUsbDeviceManager *self,
                                                 SpiceUsbDevice *device,
                                                 guint lun,
                                                 gchar *filename);
/* remove lun from the usb device */
gboolean
spice_usb_device_manager_device_lun_remove(SpiceUsbDeviceManager *self,
                                           SpiceUsbDevice *device,
                                           guint lun);

G_END_DECLS

#endif /* __SPICE_USB_DEVICE_MANAGER_H__ */
