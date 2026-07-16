#include "firemod-shell.h"

#include "firemod-fireshit-page.h"
#include "firemod-settings.h"
static GtkWidget *new_page_box(void)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_add_css_class(box, "page-canvas");
    return box;
}
static GtkWidget *wrap_page(GtkWidget *box)
{
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_add_css_class(scroll, "page-scroll");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), box);
    return scroll;
}
static void activate_nav_button(GtkWidget *button)
{
    GtkWidget *parent = gtk_widget_get_parent(button);
    for (GtkWidget *child = gtk_widget_get_first_child(parent); child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        if (g_object_get_data(G_OBJECT(child), "firemod-page") != NULL) {
            gtk_widget_remove_css_class(child, "firemod-active");
        }
    }
    gtk_widget_add_css_class(button, "firemod-active");
}
static void activate_nav_page(GtkWidget *anchor, const char *page) {
    GtkWidget *parent = gtk_widget_get_parent(anchor);
    for (GtkWidget *child = gtk_widget_get_first_child(parent); child != NULL; child = gtk_widget_get_next_sibling(child)) {
        if (g_strcmp0(g_object_get_data(G_OBJECT(child), "firemod-page"), page) == 0) { activate_nav_button(child); return; }
    }
}
static const gchar *page_chrome_title(const gchar *page)
{
    if (g_strcmp0(page, "overview") == 0) return "Fireshit capability overview";
    if (g_strcmp0(page, "fireshit") == 0) return "Fireshit control surface";
    if (g_strcmp0(page, "modules") == 0) return "Capability registry";
    if (g_strcmp0(page, "diagnostics") == 0) return "Diagnostic archaeology";
    return "Settings authority map";
}

static void set_page_chrome(FiremodApp *app, const gchar *page)
{
    gboolean fireshit = g_strcmp0(page, "fireshit") == 0;
    if (app->topbar_title != NULL) {
        gtk_label_set_text(app->topbar_title, page_chrome_title(page));
    }
    if (app->browser_tools != NULL) {
        gtk_widget_set_visible(app->browser_tools, !fireshit);
    }
}
static void nav_clicked(GtkButton *button, gpointer user_data)
{
    FiremodApp *app = user_data;
    const char *page = g_object_get_data(G_OBJECT(button), "firemod-page");
    activate_nav_button(GTK_WIDGET(button));
    gtk_stack_set_visible_child_name(app->page_stack, page);
    set_page_chrome(app, page);
}
static GtkWidget *nav_button(FiremodApp *app, const char *label, const char *icon, const char *page)
{
    GtkWidget *button = gtk_button_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *image = gtk_image_new_from_icon_name(icon);
    gtk_image_set_pixel_size(GTK_IMAGE(image), 14);
    GtkWidget *text = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(text), 0.0f);
    gtk_widget_set_hexpand(text, TRUE);
    gtk_box_append(GTK_BOX(box), image);
    gtk_box_append(GTK_BOX(box), text);
    gtk_button_set_child(GTK_BUTTON(button), box);
    gtk_widget_add_css_class(button, "nav-button");
    if (g_strcmp0(page, "overview") == 0) {
        gtk_widget_add_css_class(button, "firemod-active");
    }
    g_object_set_data_full(G_OBJECT(button), "firemod-page", g_strdup(page), g_free);
    g_signal_connect(button, "clicked", G_CALLBACK(nav_clicked), app);
    return button;
}
static void search_changed(GtkSearchEntry *entry, gpointer user_data)
{ (void)entry; firemod_app_render(user_data); }
static void filter_changed(GObject *object, GParamSpec *pspec, gpointer user_data) {
    (void)pspec;
    FiremodApp *app = user_data;
    app->active_filter = (FiremodFilter)gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    firemod_app_render(app);
}
static void uncertainty_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    FiremodApp *app = user_data;
    app->show_uncertain = gtk_switch_get_active(GTK_SWITCH(object));
    gtk_widget_set_visible(app->diagnostic_nav, app->show_uncertain);
    if (!app->show_uncertain && g_strcmp0(
            gtk_stack_get_visible_child_name(app->page_stack), "diagnostics") == 0) {
        gtk_stack_set_visible_child_name(app->page_stack, "inventory");
        activate_nav_page(app->diagnostic_nav, "inventory");
    }
    firemod_app_render(app);
}
static void rescan_clicked(GtkButton *button, gpointer user_data)
{ (void)button; firemod_app_rescan(user_data); }
static GtkWidget *build_sidebar(FiremodApp *app)
{
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_size_request(sidebar, 236, -1);
    gtk_widget_add_css_class(sidebar, "sidebar");
    GtkWidget *brand = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_add_css_class(brand, "brand");
    GtkWidget *name = gtk_label_new("Firemod");
    gtk_label_set_xalign(GTK_LABEL(name), 0.0f);
    gtk_widget_add_css_class(name, "brand-name");
    GtkWidget *tagline = gtk_label_new("FIRESHIT CORE");
    gtk_label_set_xalign(GTK_LABEL(tagline), 0.0f);
    gtk_widget_add_css_class(tagline, "brand-tagline");
    gtk_box_append(GTK_BOX(brand), name);
    gtk_box_append(GTK_BOX(brand), tagline);
    gtk_box_append(GTK_BOX(sidebar), brand);
    GtkWidget *index = gtk_label_new("INDEX");
    gtk_label_set_xalign(GTK_LABEL(index), 0.0f);
    gtk_widget_add_css_class(index, "sidebar-title");
    gtk_box_append(GTK_BOX(sidebar), index);
    gtk_box_append(GTK_BOX(sidebar), nav_button(
        app, "Overview", "view-dashboard-symbolic", "overview"));
    gtk_box_append(GTK_BOX(sidebar), nav_button(
        app, "Fireshit Settings", "preferences-system-symbolic", "fireshit"));
    gtk_box_append(GTK_BOX(sidebar), nav_button(
        app, "Settings Browser", "system-search-symbolic", "inventory"));
    gtk_box_append(GTK_BOX(sidebar), nav_button(
        app, "Modules", "application-x-addon-symbolic", "modules"));
    app->diagnostic_nav = nav_button(
        app, "Diagnostics", "dialog-warning-symbolic", "diagnostics");
    gtk_widget_add_css_class(app->diagnostic_nav, "diagnostic-nav");
    gtk_widget_set_visible(app->diagnostic_nav, FALSE);
    gtk_box_append(GTK_BOX(sidebar), app->diagnostic_nav);
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(sidebar), spacer);
    GtkWidget *advanced = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(advanced, "advanced-box");
    GtkWidget *advanced_title = gtk_label_new("ARCHAEOLOGY");
    gtk_label_set_xalign(GTK_LABEL(advanced_title), 0.0f);
    gtk_widget_add_css_class(advanced_title, "advanced-title");
    GtkWidget *switch_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(switch_row, "advanced-row");
    GtkWidget *switch_label = gtk_label_new("Show ungrouped and uncertain results");
    gtk_label_set_xalign(GTK_LABEL(switch_label), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(switch_label), TRUE);
    gtk_widget_set_hexpand(switch_label, TRUE);
    app->uncertain_switch = GTK_SWITCH(gtk_switch_new());
    gtk_widget_set_valign(GTK_WIDGET(app->uncertain_switch), GTK_ALIGN_CENTER);
    g_signal_connect(app->uncertain_switch, "notify::active",
        G_CALLBACK(uncertainty_changed), app);
    gtk_box_append(GTK_BOX(switch_row), switch_label);
    gtk_box_append(GTK_BOX(switch_row), GTK_WIDGET(app->uncertain_switch));
    gtk_box_append(GTK_BOX(advanced), advanced_title);
    gtk_box_append(GTK_BOX(advanced), switch_row);
    gtk_box_append(GTK_BOX(sidebar), advanced);
    return sidebar;
}
static GtkWidget *build_header(FiremodApp *app)
{
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_add_css_class(header, "topbar");
    GtkWidget *title_block = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(title_block, TRUE);
    GtkWidget *kicker = gtk_label_new("FIRESHIT / NATIVE CONTROL");
    gtk_label_set_xalign(GTK_LABEL(kicker), 0.0f);
    gtk_widget_add_css_class(kicker, "topbar-kicker");
    app->topbar_title = GTK_LABEL(gtk_label_new("Fireshit capability overview"));
    gtk_label_set_xalign(app->topbar_title, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(app->topbar_title), "topbar-title");
    gtk_box_append(GTK_BOX(title_block), kicker);
    gtk_box_append(GTK_BOX(title_block), GTK_WIDGET(app->topbar_title));
    app->search = GTK_SEARCH_ENTRY(gtk_search_entry_new());
    gtk_widget_set_size_request(GTK_WIDGET(app->search), 340, -1);
    g_object_set(app->search, "placeholder-text",
        "Search settings, apps, values, paths…", NULL);
    g_signal_connect(app->search, "search-changed", G_CALLBACK(search_changed), app);
    const char *filters[] = {"All", "Readable", "Located", "Fireshit", NULL};
    app->filter = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(filters));
    g_signal_connect(app->filter, "notify::selected", G_CALLBACK(filter_changed), app);
    GtkWidget *rescan = gtk_button_new_from_icon_name("view-refresh-symbolic");
    gtk_widget_set_tooltip_text(rescan, "Rescan settings authorities");
    gtk_widget_add_css_class(rescan, "rescan-button");
    g_signal_connect(rescan, "clicked", G_CALLBACK(rescan_clicked), app);
    app->browser_tools = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(app->browser_tools, "browser-tools");
    gtk_box_append(GTK_BOX(app->browser_tools), GTK_WIDGET(app->search));
    gtk_box_append(GTK_BOX(app->browser_tools), GTK_WIDGET(app->filter));
    gtk_box_append(GTK_BOX(app->browser_tools), rescan);
    gtk_box_append(GTK_BOX(header), title_block);
    gtk_box_append(GTK_BOX(header), app->browser_tools);
    return header;
}
static void close_choice(GObject *source, GAsyncResult *result, gpointer user_data)
{
    FiremodApp *app = user_data;
    GError *error = NULL;
    gint choice = gtk_alert_dialog_choose_finish(
        GTK_ALERT_DIALOG(source), result, &error);
    if (error != NULL) { g_clear_error(&error); return; }
    if (choice == 1) {
        firemod_settings_discard(app->settings);
        firemod_fireshit_page_refresh(app->fireshit_settings_page);
        gtk_widget_set_visible(GTK_WIDGET(app->window), FALSE);
    } else if (choice == 2) {
        if (firemod_settings_save(app->settings, &error)) {
            gtk_widget_set_visible(GTK_WIDGET(app->window), FALSE);
        } else {
            GtkAlertDialog *failure = gtk_alert_dialog_new("Could not save Fireshit settings");
            gtk_alert_dialog_set_detail(failure, error != NULL ? error->message : "Unknown error");
            gtk_alert_dialog_show(failure, app->window);
            g_object_unref(failure);
            g_clear_error(&error);
        }
    }
}

static gboolean window_close_requested(GtkWindow *window, gpointer user_data)
{
    FiremodApp *app = user_data;
    if (!app->settings->dirty) return FALSE;
    const gchar *buttons[] = {"Cancel", "Discard", "Save", NULL};
    GtkAlertDialog *dialog = gtk_alert_dialog_new("Save Fireshit settings?");
    gtk_alert_dialog_set_detail(dialog,
        "The current Fireshit settings draft has not been saved.");
    gtk_alert_dialog_set_buttons(dialog, buttons);
    gtk_alert_dialog_set_cancel_button(dialog, 0);
    gtk_alert_dialog_set_default_button(dialog, 2);
    gtk_alert_dialog_choose(dialog, window, NULL, close_choice, app);
    g_object_unref(dialog);
    return TRUE;
}

void firemod_shell_install_css(void)
{
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(provider, "/org/philabs/Firemod/firemod.css");
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}
void firemod_shell_build(FiremodApp *app)
{
    firemod_shell_install_css();
    app->window = GTK_WINDOW(gtk_application_window_new(app->application));
    gtk_window_set_title(app->window, "Firemod");
    gtk_window_set_hide_on_close(app->window, TRUE);
    g_signal_connect(app->window, "close-request",
        G_CALLBACK(window_close_requested), app);
    gtk_window_set_default_size(app->window, 1180, 720);
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(root, "firemod-root");
    gtk_box_append(GTK_BOX(root), build_sidebar(app));
    GtkWidget *main = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(main, TRUE);
    gtk_widget_add_css_class(main, "main-surface");
    gtk_box_append(GTK_BOX(main), build_header(app));
    app->page_stack = GTK_STACK(gtk_stack_new());
    gtk_stack_set_transition_type(app->page_stack, GTK_STACK_TRANSITION_TYPE_NONE);
    gtk_widget_set_vexpand(GTK_WIDGET(app->page_stack), TRUE);
    app->overview_page = new_page_box();
    app->fireshit_settings_page = firemod_fireshit_page_new(app);
    app->fireshit_page = firemod_fireshit_page_widget(app->fireshit_settings_page);
    app->inventory_page = new_page_box();
    app->modules_page = new_page_box();
    app->diagnostics_page = new_page_box();
    gtk_stack_add_named(app->page_stack, wrap_page(app->overview_page), "overview");
    gtk_stack_add_named(app->page_stack, wrap_page(app->fireshit_page), "fireshit");
    gtk_stack_add_named(app->page_stack, wrap_page(app->inventory_page), "inventory");
    gtk_stack_add_named(app->page_stack, wrap_page(app->modules_page), "modules");
    gtk_stack_add_named(app->page_stack, wrap_page(app->diagnostics_page), "diagnostics");
    gtk_stack_set_visible_child_name(app->page_stack, "overview");
    gtk_box_append(GTK_BOX(main), GTK_WIDGET(app->page_stack));
    app->status_label = GTK_LABEL(gtk_label_new("Read-only discovery mode"));
    gtk_label_set_xalign(app->status_label, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(app->status_label), "statusbar");
    gtk_box_append(GTK_BOX(main), GTK_WIDGET(app->status_label));
    gtk_box_append(GTK_BOX(root), main);
    gtk_window_set_child(app->window, root);
}

void firemod_shell_select_page(FiremodApp *app, const gchar *page)
{
    if (app == NULL || app->page_stack == NULL || page == NULL) return;
    gtk_stack_set_visible_child_name(app->page_stack, page);
    set_page_chrome(app, page);
    if (app->diagnostic_nav != NULL) activate_nav_page(app->diagnostic_nav, page);
}
