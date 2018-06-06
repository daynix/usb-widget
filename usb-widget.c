#include <gtk/gtk.h>
#include "usb-device-manager.h"

enum column_id
{
    COL_ADDRESS = 0,
    COL_CD_ICON,
    COL_VENDOR,
    COL_PRODUCT,
    COL_REVISION,
    COL_ALIAS,
    COL_STARTED,
    COL_LOADED,
    COL_LOCKED,
    COL_FILE,
    /* flag columns */
    COL_CD_DEV,
    COL_LUN_ITEM,
    COL_ITEM_DATA,
    COL_ROW_COLOR,
    COL_ROW_COLOR_SET,
    NUM_COLS,

    INVALID_COL
};

static const char *col_name[NUM_COLS] =
{
    "Address",
    "CD",
    "Vendor",
    "Product", 
    "Revision",
    "Alias",
    "Started",
    "Loaded",
    "Locked",
    "File/Device Path",
    /* should not be displayed */
    "?CD_DEV",
    "?LUN_ITEM",
    "?ITEM_DATA",
    "?ROW_COLOR",
    "?ROW_COLOR_SET"
};

typedef struct _usb_widget_lun_item {
    SpiceUsbDeviceManager *manager;
    SpiceUsbDevice *device;
    guint lun;
    spice_usb_device_lun_info info;
} usb_widget_lun_item;

static void usb_widget_treestore_add_device(GtkTreeStore *treestore,
                                            GtkTreeIter *old_dev_iter,
                                            SpiceUsbDeviceManager *usb_dev_mgr,
                                            SpiceUsbDevice *usb_device);

typedef struct _tree_find_usb_dev {
    SpiceUsbDevice *usb_dev;
    GtkTreeIter *dev_iter;
} tree_find_usb_dev;

static gboolean tree_find_usb_dev_foreach_cb(GtkTreeModel *tree_model,
    GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
    tree_find_usb_dev *find_info = (tree_find_usb_dev *)data;
    SpiceUsbDevice *find_usb_device = find_info->usb_dev;
    SpiceUsbDevice *usb_device;
    gboolean is_lun_item;

    gtk_tree_model_get(tree_model, iter,
                       COL_LUN_ITEM, &is_lun_item,
                       COL_ITEM_DATA, (gpointer *)&usb_device,
                       -1);
    if (!is_lun_item && usb_device == find_usb_device) {
        find_info->dev_iter = iter;
        return TRUE; /* stop iterating */
    } else {
        return FALSE; /* continue iterating */
    }
}

static GtkTreeIter *tree_find_usb_device(GtkTreeModel *tree_model,
                                         SpiceUsbDevice *usb_device)
{
    tree_find_usb_dev find_info = { .usb_dev = usb_device };
    gtk_tree_model_foreach(tree_model, tree_find_usb_dev_foreach_cb, (gpointer)&find_info);
    return find_info.dev_iter;
}

static void device_added_cb(SpiceUsbDeviceManager *usb_dev_mgr,
    SpiceUsbDevice *usb_device, gpointer user_data)
{
    GtkWidget *tree_view = (GtkWidget *)user_data;
    GtkTreeModel *tree_model = (GtkTreeModel *)gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view));

    g_print("Signal: Device Added\n");

    usb_widget_treestore_add_device(GTK_TREE_STORE(tree_model), NULL, usb_dev_mgr, usb_device);
    gtk_widget_show_all(tree_view);
}

static void device_removed_cb(SpiceUsbDeviceManager *usb_dev_mgr,
    SpiceUsbDevice *usb_device, gpointer user_data)
{
    GtkWidget *tree_view = (GtkWidget *)user_data;
    GtkTreeModel *tree_model = (GtkTreeModel *)gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view));
    GtkTreeIter *old_dev_iter;

    g_print("Signal: Device Removed\n");

    old_dev_iter = tree_find_usb_device(tree_model, usb_device);
    if (old_dev_iter != NULL) {
        gtk_tree_store_remove(GTK_TREE_STORE(tree_model), old_dev_iter);
        gtk_widget_show_all(tree_view);
    } else {
        g_print("Device not found!\n");
    }
}

static void device_changed_cb(SpiceUsbDeviceManager *usb_dev_mgr,
    SpiceUsbDevice *usb_device, gpointer user_data)
{
    GtkWidget *tree_view = (GtkWidget *)user_data;
    GtkTreeModel *tree_model = (GtkTreeModel *)gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view));
    GtkTreeIter *old_dev_iter;

    g_print("Signal: Device Changed\n");

    old_dev_iter = tree_find_usb_device(tree_model, usb_device);
    if (old_dev_iter != NULL) {
        usb_widget_treestore_add_device(GTK_TREE_STORE(tree_model), old_dev_iter, usb_dev_mgr, usb_device);
        gtk_widget_show_all(tree_view);
    } else {
        g_print("Device not found!\n");
    }
}

static void device_error_cb(SpiceUsbDeviceManager *manager,
    SpiceUsbDevice *device, GError *err, gpointer user_data)
{
    GtkTreeStore *treestore = (GtkTreeStore *)user_data;
    g_print("Signal: Device Error, tree:%p\n", treestore);
}

static GdkPixbuf *get_named_icon(const gchar *name, gint size)
{
    GtkIconInfo *info = gtk_icon_theme_lookup_icon(gtk_icon_theme_get_default(), name, size, 0);
    GdkPixbuf *pixbuf = gtk_icon_info_load_icon(info, NULL);
    g_object_unref (info);
    return pixbuf;
}

static void usb_widget_treestore_add_device(GtkTreeStore *treestore,
                                            GtkTreeIter *old_dev_iter,
                                            SpiceUsbDeviceManager *usb_dev_mgr,
                                            SpiceUsbDevice *usb_device)
{
    GtkTreeIter new_dev_iter;
    GdkPixbuf *icon_cd;
    gchar *manufacturer, *product, *addr_str;
    GArray *lun_array;
    guint j;

    if (old_dev_iter == NULL) {
        gtk_tree_store_append(treestore, &new_dev_iter, NULL);
    } else {
        gtk_tree_store_insert_after(treestore, &new_dev_iter, NULL, old_dev_iter);
        gtk_tree_store_remove(treestore, old_dev_iter);
    }

    icon_cd = get_named_icon("media-optical", 24);

    spice_usb_device_get_strings(usb_device, &manufacturer, &product);
    //(gint)spice_usb_device_get_vid(usb_device),
    //(gint)spice_usb_device_get_pid(usb_device));
    addr_str = g_strdup_printf("%d:%d",
                                (gint)spice_usb_device_get_busnum(usb_device),
                                (gint)spice_usb_device_get_devaddr(usb_device));
    g_print("usb device a:[%s] p:[%s] m:[%s]\n", addr_str, manufacturer, product);

    gtk_tree_store_set(treestore, &new_dev_iter,
        COL_ADDRESS, addr_str,
        COL_CD_ICON, icon_cd,
        COL_VENDOR, manufacturer,
        COL_PRODUCT, product,
        COL_CD_DEV, spice_usb_device_manager_is_device_cd(usb_dev_mgr, usb_device),
        COL_LUN_ITEM, FALSE, /* USB device item */
        COL_ITEM_DATA, (gpointer)usb_device,
        COL_ROW_COLOR, "beige",
        COL_ROW_COLOR_SET, TRUE,
        -1);

    /* get all the luns */
    lun_array = spice_usb_device_manager_get_device_luns(usb_dev_mgr, usb_device);
    for (j = 0; j < lun_array->len; j++) {
        usb_widget_lun_item *lun_item;
        GtkTreeIter lun_iter;
        gchar lun_str[8];

        lun_item = g_malloc(sizeof(*lun_item));
        lun_item->manager = usb_dev_mgr;
        lun_item->device = usb_device;
        lun_item->lun = g_array_index(lun_array, guint, j);
        spice_usb_device_manager_device_lun_get_info(usb_dev_mgr, usb_device, lun_item->lun, &lun_item->info);
        g_print("lun %d v:[%s] p:[%s] r:[%s] file:[%s] lun_item:%p\n",
                j, lun_item->info.vendor, lun_item->info.product,
                lun_item->info.revision, lun_item->info.file_path, lun_item);
        g_snprintf(lun_str, 8, "↳%d", lun_item->lun);

        /* Append LUN as a child of USB device */
        gtk_tree_store_append(treestore, &lun_iter, &new_dev_iter);
        gtk_tree_store_set(treestore, &lun_iter,
                COL_ADDRESS, lun_str,
                COL_VENDOR, lun_item->info.vendor,
                COL_PRODUCT, lun_item->info.product,
                COL_REVISION, lun_item->info.revision,
                COL_ALIAS, lun_item->info.alias,
                COL_STARTED, lun_item->info.started,
                COL_LOADED, lun_item->info.loaded,
                COL_LOCKED, lun_item->info.locked,
                COL_FILE, lun_item->info.file_path,
                COL_CD_DEV, FALSE,
                COL_LUN_ITEM, TRUE, /* LUN item */
                COL_ITEM_DATA, (gpointer)lun_item,
                COL_ROW_COLOR, "azure",
                COL_ROW_COLOR_SET, TRUE,
                -1);
    }
}

static GtkTreeModel* create_and_fill_model(SpiceUsbDeviceManager *usb_dev_mgr)
{
    GtkTreeStore *treestore;
    GPtrArray *usb_dev_list;
    SpiceUsbDevice *usb_device;
    guint i;

    treestore = gtk_tree_store_new(NUM_COLS,
                        G_TYPE_STRING, /* COL_ADDRESS */
                        GDK_TYPE_PIXBUF, /* COL_CD_ICON */
                        G_TYPE_STRING, /* COL_VENDOR */
                        G_TYPE_STRING, /* COL_PRODUCT */
                        G_TYPE_STRING, /* COL_ADDR_REV */
                        G_TYPE_STRING, /* COL_ALIAS */
                        G_TYPE_BOOLEAN, /* COL_STARTED */
                        G_TYPE_BOOLEAN, /* COL_LOADED */
                        G_TYPE_BOOLEAN, /* COL_LOCKED */
                        G_TYPE_STRING, /* COL_FILE */
                        G_TYPE_BOOLEAN, /* COL_CD_DEV */
                        G_TYPE_BOOLEAN, /* COL_LUN_ITEM */
                        G_TYPE_POINTER, /* COL_ITEM_DATA */
                        G_TYPE_STRING, /* COL_ROW_COLOR */
                        G_TYPE_BOOLEAN /* COL_ROW_COLOR_SET */ );
    g_print("tree store created\n");

    usb_dev_list = spice_usb_device_manager_get_devices(usb_dev_mgr);
    g_print("got devices list, len:%d\n", usb_dev_list->len);

    for (i = 0; i < usb_dev_list->len; i++) {
        GtkTreeIter dev_iter;
        /* get the device and its properties */
        usb_device = g_ptr_array_index(usb_dev_list, i);
        usb_widget_treestore_add_device(treestore, NULL, usb_dev_mgr, usb_device);
    }

    return GTK_TREE_MODEL(treestore);
}

static GtkTreeViewColumn* view_add_text_column(GtkWidget *view, enum column_id col_id)
{
    GtkCellRenderer     *renderer;
    GtkTreeViewColumn   *view_col;
    
    renderer = gtk_cell_renderer_text_new();

    view_col = gtk_tree_view_column_new_with_attributes(
                    col_name[col_id],
                    renderer,
                    "text", col_id,
                    //"cell-background", COL_ROW_COLOR,
                    //"cell-background-set", COL_ROW_COLOR_SET,
                    NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(view), view_col);

    g_print("view added text column [%d : %s]\n", col_id, col_name[col_id]);
    return view_col;
}

static gboolean tree_item_toggle_get_val(GtkTreeModel *model, gchar *path_str, GtkTreeIter *iter, enum column_id col_id)
{
    gboolean toggle_val;

    gtk_tree_model_get_iter_from_string(model, iter, path_str);
    gtk_tree_model_get(model, iter, col_id, &toggle_val, -1);

    return toggle_val;
}

static usb_widget_lun_item* tree_item_toggle_lun_item(GtkTreeModel *model, GtkTreeIter *iter)
{
    usb_widget_lun_item *lun_item;
    gboolean is_lun;
    gtk_tree_model_get(model, iter, COL_LUN_ITEM, &is_lun, COL_ITEM_DATA, (gpointer *)&lun_item, -1);
    return is_lun ? lun_item : NULL;
}

static void tree_item_toggle_set(GtkTreeModel *model, GtkTreeIter *iter, enum column_id col_id, gboolean new_val)
{
    gtk_tree_store_set(GTK_TREE_STORE(model), iter, col_id, new_val, -1);
}

typedef void (*tree_item_toggled_cb)(GtkCellRendererToggle *, gchar *, gpointer);

static void tree_item_toggled_cb_started(GtkCellRendererToggle *cell, gchar *path_str, gpointer data)
{
    GtkTreeModel *model = (GtkTreeModel *)data;
    GtkTreeIter iter;
    gboolean started;
    usb_widget_lun_item *lun_item;
    
    started = tree_item_toggle_get_val(model, path_str, &iter, COL_STARTED);
    lun_item = tree_item_toggle_lun_item(model, &iter);
    if (!lun_item) {
        g_print("not a LUN toggled?\n");
        return;
    }
    g_print("toggled lun: %d [%s,%s,%s] alias:%s started: %d --> %d\n",
            lun_item->lun, lun_item->info.vendor, lun_item->info.product, lun_item->info.revision, lun_item->info.alias,
            started, !started);

    tree_item_toggle_set(model, &iter, COL_STARTED, !started);
}

static void tree_item_toggled_cb_locked(GtkCellRendererToggle *cell, gchar *path_str, gpointer data)
{
    GtkTreeModel *model = (GtkTreeModel *)data;
    GtkTreeIter iter;
    gboolean locked;
    usb_widget_lun_item *lun_item;
    
    locked = tree_item_toggle_get_val(model, path_str, &iter, COL_LOCKED);
    lun_item = tree_item_toggle_lun_item(model, &iter);
    if (!lun_item) {
        g_print("not a LUN toggled?\n");
        return;
    }

    g_print("toggled lun:%d [%s,%s,%s] alias:%s locked: %d --> %d\n",
            lun_item->lun, lun_item->info.vendor, lun_item->info.product, lun_item->info.revision, lun_item->info.alias,
            locked, !locked);

    tree_item_toggle_set(model, &iter, COL_LOCKED, !locked);
}

static void tree_item_toggled_cb_loaded(GtkCellRendererToggle *cell, gchar *path_str, gpointer data)
{
    GtkTreeModel *model = (GtkTreeModel *)data;
    GtkTreeIter iter;
    gboolean loaded;
    usb_widget_lun_item *lun_item;
    
    loaded = tree_item_toggle_get_val(model, path_str, &iter, COL_LOADED);
    lun_item = tree_item_toggle_lun_item(model, &iter);
    if (!lun_item) {
        g_print("not a LUN toggled?\n");
        return;
    }

    g_print("toggled lun:%d [%s,%s,%s] alias:%s loaded: %d --> %d\n",
            lun_item->lun, lun_item->info.vendor, lun_item->info.product, lun_item->info.revision, lun_item->info.alias,
            loaded, !loaded);

    tree_item_toggle_set(model, &iter, COL_LOADED, !loaded);
}

static GtkTreeViewColumn* view_add_toggle_column(GtkWidget *view, GtkTreeModel *model,
                                                 enum column_id col_id,
                                                 tree_item_toggled_cb toggled_cb)
{
    GtkCellRenderer     *renderer;
    GtkTreeViewColumn   *view_col;
    
    renderer = gtk_cell_renderer_toggle_new();

    view_col = gtk_tree_view_column_new_with_attributes(
                    col_name[col_id],
                    renderer,
                    "active", col_id,
                    "visible", COL_LUN_ITEM,
                    //"cell-background", COL_ROW_COLOR,
                    //"cell-background-set", COL_ROW_COLOR_SET,
                    NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(view), view_col);

    g_object_set_data(G_OBJECT(renderer), "column", (gint *)col_id);
    g_signal_connect(renderer, "toggled", G_CALLBACK(toggled_cb), model);

    g_print("view added toggle column [%d : %s]\n", col_id, col_name[col_id]);
    return view_col;
}

static GtkTreeViewColumn* view_add_pixbuf_column(GtkWidget *view, GtkTreeModel *model,
                                                 enum column_id col_id,
                                                 enum column_id visible_col_id)
{
    GtkCellRenderer     *renderer;
    GtkTreeViewColumn   *view_col;

    renderer = gtk_cell_renderer_pixbuf_new();

    if (visible_col_id == INVALID_COL) {
        view_col = gtk_tree_view_column_new_with_attributes(
                        col_name[col_id],
                        renderer,
                        "pixbuf", col_id,
                        NULL);
    } else {
        view_col = gtk_tree_view_column_new_with_attributes(
                        col_name[col_id],
                        renderer,
                        "pixbuf", col_id,
                        "visible", visible_col_id,
                        //"cell-background", COL_ROW_COLOR,
                        //"cell-background-set", COL_ROW_COLOR_SET,
                        NULL);
    }

    gtk_tree_view_append_column(GTK_TREE_VIEW(view), view_col);

    //g_object_set_data(G_OBJECT(renderer), "column", (gint *)col_id);
    //g_signal_connect(renderer, "toggled", G_CALLBACK(toggled_cb), model);

    g_print("view added pixbuf column [%d : %s]\n", col_id, col_name[col_id]);
    return view_col;
}

static void tree_selection_changed_cb(GtkTreeSelection *select, gpointer data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreePath *path;
    gboolean is_lun;
    usb_widget_lun_item *lun_item;
    gchar *txt[4];

    if (gtk_tree_selection_get_selected(select, &model, &iter)) {
        gtk_tree_model_get (model, &iter,
                COL_VENDOR, &txt[0],
                COL_PRODUCT, &txt[1],
                COL_REVISION, &txt[2],
                COL_ALIAS, &txt[3],
                COL_LUN_ITEM, &is_lun,
                COL_ITEM_DATA, (gpointer *)&lun_item,
                -1);
        path = gtk_tree_model_get_path(model, &iter);

        g_print("selected: %s,%s,%s,%s [%s %s] [%s]\n", 
                txt[0], txt[1], 
                is_lun ? txt[2] : "--", 
                is_lun ? txt[3] : "--",
                is_lun ? "LUN" : "USB-DEV",
                is_lun ? lun_item->info.file_path : "--",
                gtk_tree_path_to_string(path));

        if (txt[0])
        g_free(txt[0]);
        if (txt[1])
        g_free(txt[1]);
        if (txt[2])
        g_free(txt[2]);
        if (txt[3])
        g_free(txt[3]);
        gtk_tree_path_free(path);
    }
}

static GtkTreeSelection* set_selection_handler(GtkWidget* view)
{
    GtkTreeSelection *select;

    select = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);

    g_signal_connect(G_OBJECT(select), "changed",
                     G_CALLBACK(tree_selection_changed_cb),
                     NULL);

    g_print("selection handler set\n");
    return select;
}

static void view_popup_menu_on_eject(GtkWidget *menuitem, gpointer userdata)
{
    /* we passed the view as userdata when we connected the signal */
    GtkTreeView *treeview = GTK_TREE_VIEW(userdata);
    g_print ("Do Eject!\n");
}

static void view_popup_menu_on_remove(GtkWidget *menuitem, gpointer userdata)
{
    /* we passed the view as userdata when we connected the signal */
    GtkTreeView *treeview = GTK_TREE_VIEW(userdata);
    g_print ("Do Remove!\n");
}

static void view_popup_menu_on_settings(GtkWidget *menuitem, gpointer userdata)
{
    /* we passed the view as userdata when we connected the signal */
    GtkTreeView *treeview = GTK_TREE_VIEW(userdata);
    g_print ("Do Settings!\n");
}

static GtkWidget *view_popup_add_menu_item(GtkWidget *menu,
    const gchar *label_str,
    const gchar *icon_name,
    GCallback cb_func, gpointer userdata)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *menu_item = gtk_menu_item_new();
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_MENU);
    GtkWidget *label = gtk_accel_label_new(label_str);
    GtkAccelGroup *accel_group = gtk_accel_group_new();
    guint accel_key;

    g_signal_connect(menu_item, "activate", cb_func, userdata);

    /* add icon */
    gtk_container_add(GTK_CONTAINER(box), icon);

    /* add label */
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_label_set_use_underline(GTK_LABEL(label), TRUE);
    g_object_get(G_OBJECT(label), "mnemonic-keyval", &accel_key, NULL);
    gtk_widget_add_accelerator(menu_item, "activate", accel_group, accel_key,
                               GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_accel_label_set_accel_widget(GTK_ACCEL_LABEL(label), menu_item);
    gtk_box_pack_end(GTK_BOX(box), label, TRUE, TRUE, 0);

    /* add menu item */
    gtk_container_add(GTK_CONTAINER(menu_item), box);
    gtk_widget_show_all(menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

    return menu_item;
}

static void view_popup_menu(GtkWidget *treeview, GdkEventButton *event, gpointer userdata)
{
    GtkWidget *menu, *menu_item;

    menu = gtk_menu_new();

    menu_item = view_popup_add_menu_item(menu, "_Eject", "media-eject", G_CALLBACK(view_popup_menu_on_eject), userdata);
    menu_item = view_popup_add_menu_item(menu, "_Settings", "preferences-system", G_CALLBACK(view_popup_menu_on_settings), userdata);
    menu_item = view_popup_add_menu_item(menu, "_Remove", "edit-delete", G_CALLBACK(view_popup_menu_on_remove), userdata);

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
}

static gboolean treeview_on_button_pressed_cb(GtkWidget *treeview, GdkEventButton *event, gpointer userdata)
{
    /* single click with the right mouse button? */
    if (event->type == GDK_BUTTON_PRESS  &&  event->button == 3)
    {
        GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
        if (gtk_tree_selection_count_selected_rows(selection)  <= 1) {
            GtkTreePath *path;
            /* Get tree path for row that was clicked */
            if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview),
                                                (gint) event->x,
                                                (gint) event->y,
                                                &path, NULL, NULL, NULL))
            {
                gtk_tree_selection_unselect_all(selection);
                gtk_tree_selection_select_path(selection, path);
                gtk_tree_path_free(path);
            }
        }
        view_popup_menu(treeview, event, userdata);
        return TRUE; /* we handled this */
    } else {
        return FALSE; /* we did not handle this */
    }
}

static gboolean treeview_on_popup_menu_cb(GtkWidget *treeview, gpointer userdata)
{
    view_popup_menu(treeview, NULL, userdata);
    return TRUE; /* we handled this */
}

static void view_signals_connect(GtkWidget *view,
                                 SpiceUsbDeviceManager *usb_dev_mgr)
{
    g_signal_connect(usb_dev_mgr, "device-added",
                     G_CALLBACK(device_added_cb), view);
    g_signal_connect(usb_dev_mgr, "device-removed",
                     G_CALLBACK(device_removed_cb), view);
    g_signal_connect(usb_dev_mgr, "device-changed",
                     G_CALLBACK(device_changed_cb), view);
    g_signal_connect(usb_dev_mgr, "device-error",
                     G_CALLBACK(device_error_cb), view);

    g_signal_connect(view, "button-press-event",
                     G_CALLBACK(treeview_on_button_pressed_cb), NULL);
    g_signal_connect(view, "popup-menu",
                     G_CALLBACK(treeview_on_popup_menu_cb), NULL);
}

static GtkWidget* create_view_and_model (void)
{
    SpiceUsbDeviceManager *usb_dev_mgr;
    GtkWidget             *view;
    GtkTreeModel          *model;

    usb_dev_mgr = spice_usb_device_manager_get(NULL, NULL);
    g_print("got dev mgr:%p\n", usb_dev_mgr);

    model = create_and_fill_model(usb_dev_mgr);
    g_print("model filled\n");

    view = gtk_tree_view_new();

    //gtk_tree_view_set_rules_hint(GTK_TREE_VIEW (view), TRUE);
    //gtk_tree_view_set_search_column(GTK_TREE_VIEW (view), COL_VENDOR);

    view_add_text_column(view, COL_ADDRESS);

    view_add_pixbuf_column(view, model, COL_CD_ICON, COL_CD_DEV);

    view_add_text_column(view, COL_VENDOR);
    view_add_text_column(view, COL_PRODUCT);
    view_add_text_column(view, COL_REVISION);
    view_add_text_column(view, COL_ALIAS);

    view_add_toggle_column(view, model, COL_STARTED, tree_item_toggled_cb_started);
    view_add_toggle_column(view, model, COL_LOADED, tree_item_toggled_cb_loaded);
    view_add_toggle_column(view, model, COL_LOCKED, tree_item_toggled_cb_locked);

    view_add_text_column(view, COL_FILE);

    view_signals_connect(view, usb_dev_mgr);

    gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);

    g_object_unref(model); /* destroy model automatically with view */

    gtk_tree_selection_set_mode(
            gtk_tree_view_get_selection(GTK_TREE_VIEW(view)),
            GTK_SELECTION_NONE);

    g_print("view and model created\n");
    return view;
}

static void usb_cd_choose_file(GtkWidget *button, gpointer data)
{
    GtkWidget *file_entry = (GtkWidget *)data;
    GtkWidget *dialog;
    gint res;

    dialog = gtk_file_chooser_dialog_new ("Choose File for USB CD",
                                          GTK_WINDOW(gtk_widget_get_toplevel(file_entry)),
                                          GTK_FILE_CHOOSER_ACTION_OPEN,
                                          "_Cancel",
                                          GTK_RESPONSE_CANCEL,
                                          "_Ok",
                                          GTK_RESPONSE_ACCEPT,
                                          NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_alignment(GTK_ENTRY(file_entry), 1);
        gtk_entry_set_text(GTK_ENTRY(file_entry), filename);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

static void add_cd_lun_button_clicked_cb(GtkWidget *add_button, gpointer data)
{
    GtkWidget *parent_window = gtk_widget_get_toplevel(add_button);
    GtkWidget *dialog, *content_area, *grid;
    GtkWidget *file_entry, *choose_button;
    GtkWidget *vendor_entry, *model_entry, *revision_entry, *alias_entry;
    GtkWidget *started_toggle, *loaded_toggle, *locked_toggle;
    gint nrow = 0, resp;

    dialog = gtk_dialog_new_with_buttons ("Add CD LUN",
                    GTK_WINDOW(parent_window),
                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, /* flags */
                    "Add",
                    GTK_RESPONSE_ACCEPT,
                    "Cancel",
                    GTK_RESPONSE_REJECT,
                    NULL);

    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 12);
    gtk_box_set_spacing(GTK_BOX(gtk_bin_get_child(GTK_BIN(dialog))), 12);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG (dialog));

    grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(content_area), grid);

    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), FALSE);

    /* File path label */
    gtk_grid_attach(GTK_GRID(grid),
            gtk_label_new("Select file or device"),
            0, nrow++, // left top
            7, 1); // width height

    /* file/device path entry */
    file_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(file_entry), "file-path");
    gtk_grid_attach(GTK_GRID(grid),
            file_entry,
            0, nrow, // left top
            6, 1); // width height

    /* choose button */
    choose_button = gtk_button_new_with_mnemonic("_Choose File");
    g_signal_connect(GTK_BUTTON(choose_button),
                     "clicked", G_CALLBACK(usb_cd_choose_file), file_entry);
    gtk_grid_attach(GTK_GRID(grid),
            choose_button,
            6, nrow++, // left top
            1, 1); // width height
  
    /* product id labels */
    gtk_grid_attach(GTK_GRID(grid),
            gtk_label_new("Vendor"),
            0, nrow, // left top
            2, 1); // width height

    gtk_grid_attach(GTK_GRID(grid),
            gtk_label_new("Product"),
            2, nrow, // left top
            4, 1); // width height

    gtk_grid_attach(GTK_GRID(grid),
            gtk_label_new("Revision"),
            6, nrow++, // left top
            1, 1); // width height

    /* vendor entry */
    vendor_entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(vendor_entry), 8);
    gtk_entry_set_text(GTK_ENTRY(vendor_entry), "RedHat");
    gtk_grid_attach(GTK_GRID(grid),
            vendor_entry,
            0, nrow, // left top
            2, 1); // width height

    /* model entry */
    model_entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(model_entry), 16);
    gtk_entry_set_text(GTK_ENTRY(model_entry), "USB-CD");
    gtk_grid_attach(GTK_GRID(grid),
            model_entry,
            2, nrow, // left top
            4, 1); // width height

    /* revision entry */
    revision_entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(revision_entry), 4);
    gtk_entry_set_text(GTK_ENTRY(revision_entry), "0.1");
    gtk_grid_attach(GTK_GRID(grid),
            revision_entry,
            6, nrow++, // left top
            1, 1); // width height

    /* alias label */
    gtk_grid_attach(GTK_GRID(grid),
            gtk_label_new("Revision"),
            0, nrow++, // left top
            7, 1); // width height

    /* alias entry */
    alias_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(alias_entry), "device alias");
    gtk_grid_attach(GTK_GRID(grid),
            alias_entry,
            0, nrow++, // left top
            7, 1); // width height

    /* Started checkbox */
    started_toggle = gtk_check_button_new_with_label("Started");
    gtk_grid_attach(GTK_GRID(grid),
            started_toggle,
            1, nrow, // left top
            1, 1); // width height

    /* Loaded checkbox */
    loaded_toggle = gtk_check_button_new_with_label("Loaded");
    gtk_grid_attach(GTK_GRID(grid),
            loaded_toggle,
            3, nrow, // left top
            1, 1); // width height

    /* Locked checkbox */
    locked_toggle = gtk_check_button_new_with_label("Locked");
    gtk_grid_attach(GTK_GRID(grid),
            locked_toggle,
            6, nrow++, // left top
            1, 1); // width height

    gtk_widget_show_all(dialog);

    resp = gtk_dialog_run(GTK_DIALOG(dialog));
    if (resp == GTK_RESPONSE_ACCEPT) {
        spice_usb_device_lun_info lun_info;
        SpiceUsbDeviceManager *usb_dev_mgr;

        g_print("response is ACCEPT\n");
        lun_info.file_path = gtk_entry_get_text(GTK_ENTRY(file_entry));
        lun_info.vendor = gtk_entry_get_text(GTK_ENTRY(vendor_entry));
        lun_info.product = gtk_entry_get_text(GTK_ENTRY(model_entry));
        lun_info.revision = gtk_entry_get_text(GTK_ENTRY(revision_entry));
        lun_info.alias = gtk_entry_get_text(GTK_ENTRY(alias_entry));
        lun_info.started = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(started_toggle));
        lun_info.loaded = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(loaded_toggle));
        lun_info.locked = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(locked_toggle));

        usb_dev_mgr = spice_usb_device_manager_get(NULL, NULL);
        spice_usb_device_manager_add_cd_lun(usb_dev_mgr, &lun_info);
    } else {
        g_print("response is REJECT\n");
    }
    gtk_widget_destroy(dialog);
}

static void ok_button_clicked_cb(GtkWidget *widget, gpointer data)
{
    GtkWidget *window = (GtkWidget *)data;

    g_print ("OK pressed\n");
    gtk_widget_destroy(window);
}

static void activate(GtkApplication *app, gpointer data)
{
    GtkWidget *window; /* main window */
    GtkWidget *grid;
    GtkWidget *view;
    GtkWidget *sw;
    GtkWidget *add_button, *ok_button;

//    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);    
    window = gtk_application_window_new(app);
    //g_signal_connect(window, "delete_event", gtk_main_quit, NULL); /* dirty */

    grid = gtk_grid_new();

    /* top label */
    gtk_grid_attach(GTK_GRID(grid),
            gtk_label_new ("Redirected USB devices"),
            0, 0, // left top
            5, 1); // width height

    /* tree view */
    view = create_view_and_model();
    set_selection_handler(view);

    /* scrolled window */
    sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
            GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(sw, TRUE);
    gtk_widget_set_halign (sw, GTK_ALIGN_FILL);
    gtk_widget_set_vexpand(sw, TRUE);
    gtk_widget_set_valign (sw, GTK_ALIGN_FILL);
    gtk_container_add(GTK_CONTAINER(sw), view);

    gtk_grid_attach(GTK_GRID(grid),
            sw, /* scrolled view */
            0, 1,
            5, 1);

    /* add LUN button */
    add_button = gtk_button_new_with_label("Add CD");
    g_signal_connect(add_button, "clicked", G_CALLBACK(add_cd_lun_button_clicked_cb), window);
    gtk_grid_attach(GTK_GRID(grid), add_button,
            1, 3,
            1, 1);

    /* add OK button */
    ok_button = gtk_button_new_with_label("OK");
    g_signal_connect(ok_button, "clicked", G_CALLBACK(ok_button_clicked_cb), window);
    gtk_grid_attach(GTK_GRID(grid), ok_button,
            3, 3,
            1, 1);

    /* add the grid to the main window and show */
    gtk_container_add(GTK_CONTAINER(window), grid);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);    
    gtk_window_set_default_size(GTK_WINDOW (window), 900, 400);
    gtk_window_set_title (GTK_WINDOW (window), "USB Widget prototype app");
    gtk_widget_show_all(window);

    //gtk_main();
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
