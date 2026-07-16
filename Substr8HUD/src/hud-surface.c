#include "hud-surface.h"

#include <cairo.h>
#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>

#include "hud-app.h"
#include "hud-navigation.h"
#include "hud-opacity.h"
#include "hud-output.h"
#include "hud-settings.h"
#include "hud-terminal.h"

static void install_css(HudApp *app, GdkDisplay *display)
{
    gdouble header_alpha = MIN(0.96, app->opacity + 0.18);
    gchar *css = g_strdup_printf(
        ".hud-root { background-color: transparent; }"
        ".hud-frame { background-color: transparent; }"
        ".hud-grid { row-spacing: 4px; column-spacing: 4px; }"
        ".hud-slot {"
        "  border: 2px solid transparent;"
        "}"
        ".hud-slot-selected {"
        "  border-color: #d7b56d;"
        "}"
        ".hud-slot-drop-target {"
        "  border-color: #69d7ff;"
        "  background-color: rgba(105, 215, 255, 0.08);"
        "}"
        ".hud-slot-dimmed {"
        "  opacity: 0.86;"
        "}"
        ".hud-slot-selected .hud-pane-header,"
        ".hud-slot-selected .hud-slot-switcher {"
        "  background-color: rgba(36, 51, 61, %.3f);"
        "}"
        ".hud-slot-selected .hud-pane-title,"
        ".hud-slot-selected .hud-slot-switcher button:checked {"
        "  color: #f0d28a;"
        "}"
        ".hud-drag-handle {"
        "  color: #697887;"
        "  min-width: 14px;"
        "  font-family: monospace;"
        "}"
        ".hud-pane-header:hover .hud-drag-handle {"
        "  color: #69d7ff;"
        "}"
        ".hud-selection-mark {"
        "  color: #d7b56d;"
        "  min-width: 12px;"
        "  opacity: 0;"
        "}"
        ".hud-slot-selected .hud-selection-mark {"
        "  opacity: 1;"
        "}"
        ".hud-slot-switcher {"
        "  background-color: rgba(7, 17, 26, %.3f);"
        "  padding: 2px;"
        "}"
        ".hud-slot-switcher button {"
        "  min-height: 20px;"
        "  padding: 2px 7px;"
        "  border-radius: 0;"
        "  color: #8796a1;"
        "  background-color: transparent;"
        "  font-family: monospace;"
        "  font-size: 9px;"
        "  font-weight: 700;"
        "}"
        ".hud-slot-switcher button:checked {"
        "  color: #d7b56d;"
        "  background-color: rgba(36, 51, 61, %.3f);"
        "}"
        ".hud-pane {"
        "  background-color: rgba(3, 6, 9, %.3f);"
        "  border: 1px solid rgba(61, 78, 90, %.3f);"
        "}"
        ".hud-pane-header {"
        "  background-color: rgba(7, 17, 26, %.3f);"
        "  padding: 5px 8px;"
        "}"
        ".hud-pane-title {"
        "  color: #aeb8bd;"
        "  font-family: monospace;"
        "  font-size: 10px;"
        "  font-weight: 700;"
        "  letter-spacing: 1px;"
        "}"
        ".hud-pane-status {"
        "  color: #69d7ff;"
        "  font-family: monospace;"
        "  font-size: 9px;"
        "  font-weight: 700;"
        "}"
        ".hud-empty {"
        "  color: #8796a1;"
        "  font-family: monospace;"
        "  font-size: 10px;"
        "}"
        ".hud-opacity-indicator {"
        "  color: #f0d28a;"
        "  background-color: rgba(3, 6, 9, 0.92);"
        "  border: 2px solid #d7b56d;"
        "  padding: 6px 12px;"
        "  font-family: monospace;"
        "  font-size: 11px;"
        "  font-weight: 700;"
        "  letter-spacing: 1px;"
        "}"
        ".hud-terminal {"
        "  background-color: transparent;"
        "  background-image: none;"
        "  padding: 8px;"
        "}",
        MAX(0.52, header_alpha),
        header_alpha,
        MAX(0.45, header_alpha),
        app->opacity,
        MAX(0.35, header_alpha),
        header_alpha);

    if (app->css_provider == NULL) {
        app->css_provider = gtk_css_provider_new();
        gtk_style_context_add_provider_for_display(
            display,
            GTK_STYLE_PROVIDER(app->css_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

    gtk_css_provider_load_from_string(
        app->css_provider,
        css);
    g_free(css);
}


static void apply_placement(HudApp *app, GtkWindow *window)
{
    gboolean left = app->placement == HUD_PLACEMENT_LEFT ||
        app->placement == HUD_PLACEMENT_FULLSCREEN;
    gboolean right = app->placement == HUD_PLACEMENT_RIGHT ||
        app->placement == HUD_PLACEMENT_FULLSCREEN;
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, left);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, right);
}

static void on_surface_map(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    HudApp *app = user_data;
    hud_surface_set_pointer_input(
        app, app->state == HUD_STATE_INTERACTIVE);

    if (app->state == HUD_STATE_INTERACTIVE) {
        hud_navigation_enter(app);
    }
}

static gboolean on_close_request(GtkWindow *window, gpointer user_data)
{
    (void)window;
    HudApp *app = user_data;
    hud_app_log(app, "close request ignored; use the session lifecycle");
    return TRUE;
}

GtkWindow *hud_surface_create(HudApp *app)
{
    if (!gtk_layer_is_supported()) {
        hud_app_log(app, "layer-shell protocol is not available");
        return NULL;
    }

    GtkWindow *window = GTK_WINDOW(
        gtk_application_window_new(app->application));
    GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(window));

    install_css(app, display);
    gtk_widget_add_css_class(GTK_WIDGET(window), "hud-root");
    hud_navigation_attach(app, window);
    hud_opacity_attach(app, GTK_WIDGET(window));
    gtk_window_set_title(window, "SUBSTR8 HUD");
    gtk_window_set_decorated(window, FALSE);
    gtk_window_set_resizable(window, FALSE);

    gtk_layer_init_for_window(window);
    gtk_layer_set_namespace(window, "wayfire-console-hud");
    gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_BOTTOM);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, FALSE);
    apply_placement(app, window);
    gtk_layer_set_exclusive_zone(window, 0);
    gtk_layer_set_keyboard_mode(
        window, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);

    gtk_window_set_default_size(window, 640, 720);

    g_signal_connect(window, "map", G_CALLBACK(on_surface_map), app);
    g_signal_connect(
        window, "close-request", G_CALLBACK(on_close_request), app);
    return window;
}

void hud_surface_set_layer(HudApp *app, HudState state)
{
    GtkLayerShellLayer layer = GTK_LAYER_SHELL_LAYER_BOTTOM;
    if (state == HUD_STATE_TOP) {
        layer = GTK_LAYER_SHELL_LAYER_TOP;
    } else if (state == HUD_STATE_INTERACTIVE) {
        layer = GTK_LAYER_SHELL_LAYER_OVERLAY;
    }
    gtk_layer_set_layer(app->window, layer);
}

void hud_surface_set_pointer_input(HudApp *app, gboolean enabled)
{
    if (app == NULL || app->window == NULL) {
        return;
    }

    GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(app->window));
    if (surface == NULL) {
        return;
    }

    if (enabled) {
        gdk_surface_set_input_region(surface, NULL);
        return;
    }

    cairo_region_t *empty = cairo_region_create();
    gdk_surface_set_input_region(surface, empty);
    cairo_region_destroy(empty);
}

void hud_surface_show(HudApp *app)
{
    gtk_window_present(app->window);
}

void hud_surface_hide(HudApp *app)
{
    gtk_widget_set_visible(GTK_WIDGET(app->window), FALSE);
}

void hud_surface_refresh_appearance(HudApp *app)
{
    if (app == NULL || app->window == NULL) {
        return;
    }

    GdkDisplay *display = gtk_widget_get_display(
        GTK_WIDGET(app->window));
    install_css(app, display);
}

void hud_surface_refresh_settings(HudApp *app)
{
    if (app == NULL || app->window == NULL) {
        return;
    }

    hud_surface_refresh_appearance(app);
    apply_placement(app, app->window);
    hud_output_refresh(app);
}
