#include "hud-arrangement.h"

#include "hud-app.h"
#include "hud-layout.h"
#include "hud-navigation.h"
#include "hud-settings.h"
#include "hud-slot.h"

static HudModule *find_module(HudApp *app, const char *id)
{
    for (guint index = 0;
         app->modules != NULL && index < app->modules->len;
         index++) {
        HudModule *module = g_ptr_array_index(app->modules, index);
        if (g_strcmp0(module->id, id) == 0) {
            return module;
        }
    }
    return NULL;
}

static void reconcile_visible_lifecycle(HudApp *app)
{
    for (guint index = 0; index < app->modules->len; index++) {
        HudModule *module = g_ptr_array_index(app->modules, index);
        if (g_strcmp0(module->lifecycle, "visible") != 0) {
            continue;
        }
        if (hud_slot_active(app->slots[module->slot]) == module) {
            hud_module_spawn(module);
        } else {
            hud_module_stop(module);
        }
    }
}

static GPtrArray *hold_all_panes(HudApp *app)
{
    GPtrArray *held = g_ptr_array_new_with_free_func(g_object_unref);
    for (guint index = 0; index < app->modules->len; index++) {
        HudModule *module = g_ptr_array_index(app->modules, index);
        if (module->pane != NULL) {
            g_ptr_array_add(held, g_object_ref(module->pane));
        }
    }
    return held;
}

static void detach_all_panes(HudApp *app)
{
    for (guint index = 0; index < app->modules->len; index++) {
        HudModule *module = g_ptr_array_index(app->modules, index);
        GtkWidget *parent = module->pane != NULL
            ? gtk_widget_get_parent(module->pane)
            : NULL;
        if (parent != NULL && GTK_IS_STACK(parent)) {
            gtk_stack_remove(GTK_STACK(parent), module->pane);
        }
    }
}

static void rebuild_all_slots(HudApp *app)
{
    GPtrArray *held = hold_all_panes(app);
    detach_all_panes(app);
    for (int slot = 0; slot < HUD_SLOT_COUNT; slot++) {
        HudModule *preferred = hud_slot_active(app->slots[slot]);
        hud_slot_rebuild(app->slots[slot], preferred);
    }
    g_ptr_array_unref(held);
    reconcile_visible_lifecycle(app);
}

void hud_arrangement_apply_saved(HudApp *app)
{
    if (app == NULL || app->modules == NULL) {
        return;
    }

    for (int slot = 0; slot < HUD_SLOT_COUNT; slot++) {
        g_ptr_array_set_size(app->slots[slot]->modules, 0);
    }
    for (guint index = 0; index < app->modules->len; index++) {
        HudModule *module = g_ptr_array_index(app->modules, index);
        hud_settings_apply_module(app, module);
        hud_slot_add_module(app->slots[module->slot], module);
    }

    rebuild_all_slots(app);
    hud_layout_restore(app);
    if (app->state == HUD_STATE_INTERACTIVE) {
        hud_navigation_enter(app);
    } else {
        hud_navigation_leave(app);
    }
}

static void rebuild_move_slots(
    HudApp *app,
    HudSlot *source,
    HudModule *source_preferred,
    HudSlot *destination,
    HudModule *destination_preferred)
{
    GPtrArray *held = hold_all_panes(app);
    hud_slot_rebuild(source, source_preferred);
    if (destination != source) {
        hud_slot_rebuild(destination, destination_preferred);
    }
    g_ptr_array_unref(held);
    reconcile_visible_lifecycle(app);
}

gboolean hud_arrangement_move_module(
    HudApp *app,
    const char *module_id,
    HudModuleSlot target,
    guint index,
    GError **error)
{
    HudModule *module = find_module(app, module_id);
    if (module == NULL || !hud_layout_slot_visible(app, target)) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "invalid module move target");
        return FALSE;
    }

    HudModuleSlot original_slot = module->slot;
    HudSlot *source = app->slots[original_slot];
    HudSlot *destination = app->slots[target];
    HudModule *source_active = hud_slot_active(source);
    HudModule *destination_active = hud_slot_active(destination);
    guint original_index = hud_slot_index_of(source, module);
    if (original_index >= source->modules->len) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
            "module %s is not present in its source panel", module_id);
        return FALSE;
    }

    hud_slot_remove_module(source, module);
    if (source == destination && index > original_index) {
        index--;
    }
    hud_slot_insert_module(destination, module, index);
    hud_slot_normalize_orders(source);
    if (destination != source) {
        hud_slot_normalize_orders(destination);
    }
    HudModule *source_preferred = source == destination
        ? source_active
        : (source_active == module ? NULL : source_active);
    rebuild_move_slots(
        app, source, source_preferred, destination, module);

    if (!hud_settings_save_layout(app, error)) {
        hud_slot_remove_module(destination, module);
        module->slot = original_slot;
        hud_slot_insert_module(source, module, original_index);
        hud_slot_normalize_orders(source);
        if (destination != source) {
            hud_slot_normalize_orders(destination);
        }
        rebuild_move_slots(
            app, source, source_active, destination, destination_active);
        return FALSE;
    }

    app->selected_slot = target;
    app->last_focused_slot = target;
    app->has_last_focused_slot = TRUE;
    hud_navigation_enter(app);
    hud_app_log(
        app,
        "module %s moved to %s at bank order %d; layout persisted",
        module->id,
        hud_module_slot_name(module->slot),
        module->bank_order);
    return TRUE;
}
