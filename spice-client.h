
#ifndef __SPICE_CLIENT_H__
#define __SPICE_CLIENT_H__

#include <glib-object.h>

typedef struct _SpiceSession
{
    GObject parent;
    guint x;
} SpiceSession;

typedef struct _SpiceSessionClass
{
    GObjectClass parent_class;
} SpiceSessionClass;

#define SPICE_TYPE_SESSION            (spice_session_get_type ())
#define SPICE_SESSION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SPICE_TYPE_SESSION, SpiceSession))
#define SPICE_SESSION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SPICE_TYPE_SESSION, SpiceSessionClass))
#define SPICE_IS_SESSION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SPICE_TYPE_SESSION))
#define SPICE_IS_SESSION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SPICE_TYPE_SESSION))
#define SPICE_SESSION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SPICE_TYPE_SESSION, SpiceSessionClass))

GType spice_session_get_type(void);

#define SPICE_RESERVED_PADDING 32

#include "usb-device-manager.h"
#include <string.h>
#define g_cclosure_user_marshal_VOID__BOXED_BOXED g_cclosure_marshal_VOID__BOXED
#define spice_util_get_debug()

#endif

