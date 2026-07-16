#include "hud-output.h"

#include <gtk4-layer-shell.h>

#include "hud-app.h"
#include "hud-settings.h"

static void clear_monitor(HudApp *app)
{
    if (app->monitor != NULL &&
        app->monitor_notify_handler != 0) {
        g_signal_handler_disconnect(
            app->monitor,
            app->monitor_notify_handler);
    }

    app->monitor_notify_handler = 0;
    g_clear_object(&app->monitor);
}

static void on_monitor_changed(
    GObject *object,
    GParamSpec *parameter,
    gpointer user_data)
{
    (void)object;
    (void)parameter;
    hud_output_refresh(user_data);
}

static void apply_geometry(HudApp *app)
{
    if (app == NULL ||
        app->window == NULL ||
        app->monitor_model == NULL) {
        return;
    }

    if (g_list_model_get_n_items(app->monitor_model) == 0) {
        clear_monitor(app);
        hud_app_log(app, "no output available; retaining HUD geometry");
        return;
    }

    GdkMonitor *monitor = GDK_MONITOR(
        g_list_model_get_item(app->monitor_model, 0));
    if (monitor == NULL) {
        return;
    }

    if (app->monitor != monitor) {
        clear_monitor(app);
        app->monitor = g_object_ref(monitor);
        app->monitor_notify_handler = g_signal_connect(
            app->monitor,
            "notify",
            G_CALLBACK(on_monitor_changed),
            app);
    }

    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);
    gint width = app->placement == HUD_PLACEMENT_FULLSCREEN
        ? geometry.width
        : MAX(320, (geometry.width * app->width_percent + 99) / 100);

    gtk_layer_set_monitor(app->window, monitor);
    gtk_window_set_default_size(
        app->window,
        width,
        geometry.height);
    hud_app_log(
        app,
        "output geometry %dx%d logical; HUD=%dx%d placement=%s",
        geometry.width,
        geometry.height,
        width,
        geometry.height,
        hud_placement_name(app->placement));
    g_object_unref(monitor);
}

static void on_monitors_changed(
    GListModel *model,
    guint position,
    guint removed,
    guint added,
    gpointer user_data)
{
    (void)model;
    (void)position;
    (void)removed;
    (void)added;
    hud_output_refresh(user_data);
}

void hud_output_attach(HudApp *app)
{
    if (app == NULL || app->window == NULL) {
        return;
    }

    GdkDisplay *display = gtk_widget_get_display(
        GTK_WIDGET(app->window));
    app->monitor_model = g_object_ref(
        gdk_display_get_monitors(display));
    app->monitor_model_handler = g_signal_connect(
        app->monitor_model,
        "items-changed",
        G_CALLBACK(on_monitors_changed),
        app);
    apply_geometry(app);
}

void hud_output_refresh(HudApp *app)
{
    apply_geometry(app);
}

void hud_output_detach(HudApp *app)
{
    if (app == NULL) {
        return;
    }

    clear_monitor(app);
    if (app->monitor_model != NULL &&
        app->monitor_model_handler != 0) {
        g_signal_handler_disconnect(
            app->monitor_model,
            app->monitor_model_handler);
    }
    app->monitor_model_handler = 0;
    g_clear_object(&app->monitor_model);
}
