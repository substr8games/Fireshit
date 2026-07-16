#include "hud-opacity.h"

#include <vte/vte.h>

#include "hud-app.h"
#include "hud-config.h"
#include "hud-state.h"
#include "hud-surface.h"

static gboolean focus_is_terminal(HudApp *app)
{
    GtkWidget *focus = gtk_window_get_focus(app->window);

    while (focus != NULL) {
        if (VTE_IS_TERMINAL(focus)) {
            return TRUE;
        }
        focus = gtk_widget_get_parent(focus);
    }

    return FALSE;
}

static gboolean hide_indicator(gpointer user_data)
{
    HudApp *app = user_data;
    app->opacity_hide_source = 0;

    if (app->opacity_indicator != NULL) {
        gtk_widget_set_visible(
            app->opacity_indicator,
            FALSE);
    }

    return G_SOURCE_REMOVE;
}

static gboolean persist_opacity(gpointer user_data)
{
    HudApp *app = user_data;
    app->opacity_save_source = 0;

    GError *error = NULL;
    if (!hud_config_save_opacity(app, &error)) {
        hud_app_log(
            app,
            "opacity persistence failed: %s",
            error->message);
        g_clear_error(&error);
        return G_SOURCE_REMOVE;
    }

    hud_app_log(
        app,
        "opacity persisted at %.2f",
        app->opacity);
    return G_SOURCE_REMOVE;
}

static void schedule_feedback(HudApp *app)
{
    gint percent = (gint)(app->opacity * 100.0 + 0.5);
    gchar *text = g_strdup_printf(
        "OPACITY %d%%",
        percent);

    gtk_label_set_text(
        GTK_LABEL(app->opacity_indicator),
        text);
    gtk_widget_set_visible(
        app->opacity_indicator,
        TRUE);
    g_free(text);

    if (app->opacity_hide_source != 0) {
        g_source_remove(app->opacity_hide_source);
    }
    app->opacity_hide_source = g_timeout_add(
        900,
        hide_indicator,
        app);

    if (app->opacity_save_source != 0) {
        g_source_remove(app->opacity_save_source);
    }
    app->opacity_save_source = g_timeout_add(
        450,
        persist_opacity,
        app);
}

static gboolean on_scroll(
    GtkEventControllerScroll *controller,
    gdouble dx,
    gdouble dy,
    gpointer user_data)
{
    (void)controller;
    (void)dx;

    HudApp *app = user_data;
    if (app->state != HUD_STATE_INTERACTIVE ||
        focus_is_terminal(app) ||
        dy == 0.0) {
        return FALSE;
    }

    gint percent = (gint)(app->opacity * 100.0 + 0.5);
    percent = ((percent + 2) / 5) * 5;
    percent += dy < 0.0 ? 5 : -5;
    percent = CLAMP(percent, 15, 100);

    gdouble next = percent / 100.0;
    if (next == app->opacity) {
        return TRUE;
    }

    app->opacity = next;
    hud_surface_refresh_appearance(app);
    schedule_feedback(app);
    return TRUE;
}

GtkWidget *hud_opacity_wrap_view(
    HudApp *app,
    GtkWidget *child)
{
    GtkWidget *overlay = gtk_overlay_new();
    GtkWidget *indicator = gtk_label_new(NULL);

    gtk_widget_add_css_class(
        indicator,
        "hud-opacity-indicator");
    gtk_widget_set_halign(
        indicator,
        GTK_ALIGN_CENTER);
    gtk_widget_set_valign(
        indicator,
        GTK_ALIGN_START);
    gtk_widget_set_margin_top(indicator, 18);
    gtk_widget_set_can_target(indicator, FALSE);
    gtk_widget_set_visible(indicator, FALSE);

    gtk_overlay_set_child(
        GTK_OVERLAY(overlay),
        child);
    gtk_overlay_add_overlay(
        GTK_OVERLAY(overlay),
        indicator);

    app->opacity_indicator = indicator;
    return overlay;
}

void hud_opacity_attach(
    HudApp *app,
    GtkWidget *widget)
{
    GtkEventController *controller =
        gtk_event_controller_scroll_new(
            GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);

    gtk_event_controller_set_propagation_phase(
        controller,
        GTK_PHASE_CAPTURE);
    g_signal_connect(
        controller,
        "scroll",
        G_CALLBACK(on_scroll),
        app);
    gtk_widget_add_controller(widget, controller);
}

void hud_opacity_shutdown(HudApp *app)
{
    if (app == NULL) {
        return;
    }

    if (app->opacity_save_source != 0) {
        g_source_remove(app->opacity_save_source);
        app->opacity_save_source = 0;
        persist_opacity(app);
    }

    if (app->opacity_hide_source != 0) {
        g_source_remove(app->opacity_hide_source);
        app->opacity_hide_source = 0;
    }

    app->opacity_indicator = NULL;
}
