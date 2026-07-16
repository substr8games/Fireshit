#include "hud-layout.h"

#include <gtk/gtk.h>

#include "hud-app.h"
#include "hud-settings.h"
#include "hud-slot.h"

static const HudModuleSlot side_slots[] = {
    HUD_SLOT_TOP_LEFT,
    HUD_SLOT_TOP_RIGHT,
    HUD_SLOT_BOTTOM_LEFT_WIDE,
};

static const HudModuleSlot fullscreen_slots[] = {
    HUD_SLOT_TOP_LEFT,
    HUD_SLOT_TOP_CENTER,
    HUD_SLOT_TOP_RIGHT,
    HUD_SLOT_BOTTOM_LEFT_WIDE,
    HUD_SLOT_BOTTOM_RIGHT_WIDE,
};

static const HudModuleSlot *visible_slots(HudApp *app, guint *count)
{
    if (app->placement == HUD_PLACEMENT_FULLSCREEN) {
        *count = G_N_ELEMENTS(fullscreen_slots);
        return fullscreen_slots;
    }
    *count = G_N_ELEMENTS(side_slots);
    return side_slots;
}

gboolean hud_layout_slot_visible(HudApp *app, HudModuleSlot slot)
{
    guint count = 0;
    const HudModuleSlot *slots = visible_slots(app, &count);
    for (guint index = 0; index < count; index++) {
        if (slots[index] == slot) {
            return TRUE;
        }
    }
    return FALSE;
}

HudModuleSlot hud_layout_first_slot(HudApp *app)
{
    guint count = 0;
    return visible_slots(app, &count)[0];
}

HudModuleSlot hud_layout_next_slot(
    HudApp *app,
    HudModuleSlot slot,
    int delta)
{
    guint count = 0;
    const HudModuleSlot *slots = visible_slots(app, &count);
    guint current = 0;
    for (guint index = 0; index < count; index++) {
        if (slots[index] == slot) {
            current = index;
            break;
        }
    }

    int next = ((int)current + delta) % (int)count;
    if (next < 0) {
        next += count;
    }
    return slots[next];
}

static void detach_all(HudApp *app)
{
    for (int index = 0; index < HUD_SLOT_COUNT; index++) {
        HudSlot *slot = app->slots[index];
        if (slot == NULL || slot->root == NULL) {
            continue;
        }
        g_object_ref(slot->root);
        if (gtk_widget_get_parent(slot->root) == GTK_WIDGET(app->grid)) {
            gtk_grid_remove(app->grid, slot->root);
        }
    }
}

static void release_all(HudApp *app)
{
    for (int index = 0; index < HUD_SLOT_COUNT; index++) {
        HudSlot *slot = app->slots[index];
        if (slot != NULL && slot->root != NULL) {
            g_object_unref(slot->root);
        }
    }
}

static void attach(
    HudApp *app,
    HudModuleSlot id,
    int column,
    int row,
    int width)
{
    HudSlot *slot = app->slots[id];
    gtk_grid_attach(app->grid, slot->root, column, row, width, 1);
    gtk_widget_set_visible(slot->root, TRUE);
}

void hud_layout_restore(HudApp *app)
{
    if (app == NULL || app->grid == NULL) {
        return;
    }

    detach_all(app);
    for (int index = 0; index < HUD_SLOT_COUNT; index++) {
        HudSlot *slot = app->slots[index];
        if (slot != NULL && slot->root != NULL) {
            gtk_widget_set_visible(slot->root, FALSE);
        }
    }

    if (app->placement == HUD_PLACEMENT_FULLSCREEN) {
        attach(app, HUD_SLOT_TOP_LEFT, 0, 0, 2);
        attach(app, HUD_SLOT_TOP_CENTER, 2, 0, 2);
        attach(app, HUD_SLOT_TOP_RIGHT, 4, 0, 2);
        attach(app, HUD_SLOT_BOTTOM_LEFT_WIDE, 0, 1, 3);
        attach(app, HUD_SLOT_BOTTOM_RIGHT_WIDE, 3, 1, 3);
    } else {
        attach(app, HUD_SLOT_TOP_LEFT, 0, 0, 1);
        attach(app, HUD_SLOT_TOP_RIGHT, 1, 0, 1);
        attach(app, HUD_SLOT_BOTTOM_LEFT_WIDE, 0, 1, 2);
    }

    release_all(app);
    app->pane_expanded = FALSE;
    if (!hud_layout_slot_visible(app, app->selected_slot)) {
        app->selected_slot = hud_layout_first_slot(app);
    }
}

void hud_layout_expand_selected(HudApp *app)
{
    if (app == NULL || app->grid == NULL) {
        return;
    }

    hud_layout_restore(app);
    app->pane_expanded = TRUE;
    for (int index = 0; index < HUD_SLOT_COUNT; index++) {
        HudSlot *slot = app->slots[index];
        if (slot != NULL && slot->root != NULL) {
            gtk_widget_set_visible(slot->root, index == app->selected_slot);
        }
    }

    HudSlot *selected = app->slots[app->selected_slot];
    g_object_ref(selected->root);
    gtk_grid_remove(app->grid, selected->root);
    gtk_grid_attach(
        app->grid,
        selected->root,
        0,
        0,
        app->placement == HUD_PLACEMENT_FULLSCREEN ? 6 : 2,
        2);
    g_object_unref(selected->root);
}

void hud_layout_toggle_selected(HudApp *app)
{
    if (app == NULL) {
        return;
    }
    if (app->pane_expanded) {
        hud_layout_restore(app);
    } else {
        hud_layout_expand_selected(app);
    }
}
