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
    GtkWidget *window, *win_label;
    GtkWidget *dialog, *area, *usb_device_widget;
    SpiceSession *session;
    GError *err;

    window = gtk_application_window_new(app);

    gtk_window_set_default_size(GTK_WINDOW(window), 1400, 800);
    gtk_window_set_title(GTK_WINDOW(window), "USB Widget prototype app");
    gtk_container_set_border_width(GTK_CONTAINER(window), 12);

    win_label = gtk_label_new("USB Widget prototype app - this window will be closed with the dialog");
    gtk_container_add(GTK_CONTAINER(window), win_label);
    gtk_widget_set_valign(win_label, GTK_ALIGN_START);

    dialog = gtk_dialog_new_with_buttons(_("Select USB devices for redirection"), GTK_WINDOW(window),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _("_Close"), GTK_RESPONSE_ACCEPT,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 1200, 600);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 12);
    gtk_box_set_spacing(GTK_BOX(gtk_bin_get_child(GTK_BIN(dialog))), 12);

    session = g_initable_new(SPICE_TYPE_SESSION,
                                          NULL, /* cancellable */
                                          &err, /* error */
                                          NULL);;

    usb_device_widget = spice_usb_device_widget_new(session, "%s %s");

    area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_pack_start(GTK_BOX(area), usb_device_widget, TRUE, TRUE, 0);

    gtk_widget_show_all(window);

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    gtk_widget_destroy(window);
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
