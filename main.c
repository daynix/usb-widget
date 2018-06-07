#include "config.h"
#include <gtk/gtk.h>
#include "usb-device-manager.h"
#include "usb-device-widget.h"

static void spice_session_initable_iface_init(GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE(SpiceSession, spice_session, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, spice_session_initable_iface_init));

static void spice_session_init(SpiceSession *self)
{
}

static gboolean spice_session_initable_init(GInitable  *initable,
                                                       GCancellable  *cancellable,
                                                       GError        **err)
{
    return TRUE;
}

static void spice_session_initable_iface_init(GInitableIface *iface)
{
    iface->init = spice_session_initable_init;
}

static void spice_session_class_init(SpiceSessionClass *klass)
{
}


static void activate(GtkApplication *app, gpointer data)
{
    GtkWidget *window = gtk_application_window_new(app);
    GError *err;
    SpiceSession *session = g_initable_new(SPICE_TYPE_SESSION,
                                          NULL, /* cancellable */
                                          &err, /* error */
                                          NULL);;
    GtkWidget *usb_device_widget = spice_usb_device_widget_new(session, "%s %s");

//    GtkWidget *tree_grid = create_usb_widget_tree_view();
    gtk_container_add(GTK_CONTAINER(window), usb_device_widget);

    gtk_container_set_border_width(GTK_CONTAINER(window), 10);    
    gtk_window_set_default_size(GTK_WINDOW (window), 900, 400);
    gtk_window_set_title (GTK_WINDOW (window), "USB Widget prototype app");
    gtk_widget_show_all(window);
}


int main (int argc, char **argv)
{
    GtkApplication *app;
    int status;

    //gtk_init(&argc, &argv);

    app = gtk_application_new(NULL, G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref (app);

    return status;
}
