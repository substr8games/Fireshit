#include "hud-navigation.h"

#include <vte/vte.h>

#include "hud-app.h"
#include "hud-layout.h"
#include "hud-module.h"
#include "hud-slot.h"

static void set_class(GtkWidget *widget, const char *name, gboolean enabled)
{
    if (widget == NULL) {
        return;
    }
    if (enabled) {
        gtk_widget_add_css_class(widget, name);
    } else {
        gtk_widget_remove_css_class(widget, name);
    }
}

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

static void apply_selection(HudApp *app)
{
    for (int index = 0; index < HUD_SLOT_COUNT; index++) {
        HudSlot *slot = app->slots[index];
        if (slot == NULL || slot->root == NULL) {
            continue;
        }
        gboolean visible = hud_layout_slot_visible(app, (HudModuleSlot)index);
        gboolean selected = visible && index == app->selected_slot;
        set_class(slot->root, "hud-slot-selected", selected);
        set_class(slot->root, "hud-slot-dimmed", visible && !selected);
        set_class(
            slot->root,
            "hud-slot-expanded",
            selected && app->pane_expanded);
    }
}

static void focus_selected(HudApp *app)
{
    HudSlot *slot = app->slots[app->selected_slot];
    if (slot != NULL && slot->root != NULL) {
        gtk_widget_grab_focus(slot->root);
    }
}

static void select_delta(HudApp *app, int delta)
{
    app->selected_slot = hud_layout_next_slot(
        app,
        app->selected_slot,
        delta);
    if (app->pane_expanded) {
        hud_layout_expand_selected(app);
    }
    apply_selection(app);
    focus_selected(app);
    hud_app_log(
        app,
        "selected pane %d%s",
        app->selected_slot,
        app->pane_expanded ? " expanded" : "");
}

static void toggle_expanded(HudApp *app)
{
    hud_layout_toggle_selected(app);
    apply_selection(app);
    focus_selected(app);
    hud_app_log(
        app,
        "pane %d %s",
        app->selected_slot,
        app->pane_expanded ? "expanded" : "collapsed");
}

static gboolean on_key_pressed(
    GtkEventControllerKey *controller,
    guint keyval,
    guint keycode,
    GdkModifierType state,
    gpointer user_data)
{
    (void)controller;
    (void)keycode;
    HudApp *app = user_data;
    if (app->state != HUD_STATE_INTERACTIVE) {
        return FALSE;
    }

    if ((state & GDK_CONTROL_MASK) != 0 &&
        (keyval == GDK_KEY_Tab || keyval == GDK_KEY_ISO_Left_Tab)) {
        int delta = ((state & GDK_SHIFT_MASK) != 0 ||
            keyval == GDK_KEY_ISO_Left_Tab) ? -1 : 1;
        if (hud_slot_cycle(app->slots[app->selected_slot], delta)) {
            hud_app_log(app, "cycled module bank in pane %d", app->selected_slot);
            return TRUE;
        }
    }

    if (keyval == GDK_KEY_Page_Down || keyval == GDK_KEY_KP_Page_Down) {
        select_delta(app, 1);
        return TRUE;
    }
    if (keyval == GDK_KEY_Page_Up || keyval == GDK_KEY_KP_Page_Up) {
        select_delta(app, -1);
        return TRUE;
    }
    if ((keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter ||
         keyval == GDK_KEY_ISO_Enter) && !focus_is_terminal(app)) {
        toggle_expanded(app);
        return TRUE;
    }
    return FALSE;
}

void hud_navigation_attach(HudApp *app, GtkWindow *window)
{
    GtkEventController *controller = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(controller, GTK_PHASE_CAPTURE);
    g_signal_connect(controller, "key-pressed", G_CALLBACK(on_key_pressed), app);
    gtk_widget_add_controller(GTK_WIDGET(window), controller);
}

void hud_navigation_enter(HudApp *app)
{
    if (app == NULL || app->grid == NULL) {
        return;
    }

    hud_layout_restore(app);
    app->selected_slot = app->has_last_focused_slot &&
        hud_layout_slot_visible(app, app->last_focused_slot)
        ? app->last_focused_slot
        : hud_layout_first_slot(app);
    apply_selection(app);
    focus_selected(app);
}

void hud_navigation_leave(HudApp *app)
{
    if (app == NULL) {
        return;
    }
    hud_layout_restore(app);
    for (int index = 0; index < HUD_SLOT_COUNT; index++) {
        HudSlot *slot = app->slots[index];
        if (slot == NULL || slot->root == NULL) {
            continue;
        }
        set_class(slot->root, "hud-slot-selected", FALSE);
        set_class(slot->root, "hud-slot-dimmed", FALSE);
        set_class(slot->root, "hud-slot-expanded", FALSE);
    }
}
