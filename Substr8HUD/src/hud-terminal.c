#include "hud-terminal.h"

#include <glib/gstdio.h>

#include "hud-app.h"
#include "hud-layout.h"
#include "hud-module-path.h"
#include "hud-slot.h"

static gint compare_paths(gconstpointer left, gconstpointer right)
{
    const gchar *left_path = *(gchar * const *)left;
    const gchar *right_path = *(gchar * const *)right;
    return g_strcmp0(left_path, right_path);
}

static GPtrArray *module_paths(const char *directory, GError **error)
{
    GPtrArray *paths = g_ptr_array_new_with_free_func(g_free);
    GDir *dir = g_dir_open(directory, 0, error);
    if (dir == NULL) {
        g_ptr_array_unref(paths);
        return NULL;
    }

    const gchar *name = NULL;
    while ((name = g_dir_read_name(dir)) != NULL) {
        if (g_str_has_suffix(name, ".ini")) {
            g_ptr_array_add(paths, g_build_filename(directory, name, NULL));
        }
    }
    g_dir_close(dir);
    g_ptr_array_sort(paths, compare_paths);
    return paths;
}

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

static void load_modules(HudApp *app)
{
    app->modules = g_ptr_array_new_with_free_func((GDestroyNotify)hud_module_free);
    gchar *directory = hud_module_directory_find(app);
    if (directory == NULL) {
        hud_app_log(app, "no module directory found");
        return;
    }

    hud_app_log(app, "loading modules from %s", directory);
    GError *error = NULL;
    GPtrArray *paths = module_paths(directory, &error);
    if (paths == NULL) {
        hud_app_log(app, "module directory failed: %s", error->message);
        g_clear_error(&error);
        g_free(directory);
        return;
    }

    for (guint index = 0; index < paths->len; index++) {
        const char *path = g_ptr_array_index(paths, index);
        HudModule *module = hud_module_load(app, path, &error);
        if (module == NULL) {
            hud_app_log(
                app,
                "module rejected from %s: %s",
                path,
                error != NULL ? error->message : "unknown error");
            g_clear_error(&error);
            continue;
        }
        if (find_module(app, module->id) != NULL) {
            hud_app_log(app, "duplicate module id rejected: %s", module->id);
            hud_module_free(module);
            continue;
        }
        g_ptr_array_add(app->modules, module);
        hud_app_log(app, "module admitted: %s", module->id);
    }

    g_ptr_array_unref(paths);
    g_free(directory);
}

GtkWidget *hud_terminal_create_view(HudApp *app)
{
    load_modules(app);
    GtkWidget *frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *grid = gtk_grid_new();
    app->grid = GTK_GRID(grid);

    gtk_widget_add_css_class(frame, "hud-frame");
    gtk_widget_add_css_class(grid, "hud-grid");
    gtk_grid_set_row_homogeneous(app->grid, TRUE);
    gtk_grid_set_column_homogeneous(app->grid, TRUE);
    gtk_widget_set_hexpand(grid, TRUE);
    gtk_widget_set_vexpand(grid, TRUE);

    for (int slot = 0; slot < HUD_SLOT_COUNT; slot++) {
        app->slots[slot] = hud_slot_new(app, (HudModuleSlot)slot);
    }
    for (guint index = 0; index < app->modules->len; index++) {
        HudModule *module = g_ptr_array_index(app->modules, index);
        hud_slot_add_module(app->slots[module->slot], module);
    }
    for (int slot = 0; slot < HUD_SLOT_COUNT; slot++) {
        hud_slot_finish(app->slots[slot]);
    }

    hud_layout_restore(app);
    gtk_box_append(GTK_BOX(frame), grid);
    return frame;
}

void hud_terminal_spawn_all(HudApp *app)
{
    if (app == NULL || app->modules == NULL) { return; }
    for (guint index = 0; index < app->modules->len; index++) {
        HudModule *module = g_ptr_array_index(app->modules, index);
        if (g_strcmp0(module->lifecycle, "persistent") == 0) {
            hud_module_spawn(module);
        }
    }
    for (int slot = 0; slot < HUD_SLOT_COUNT; slot++) {
        if (hud_layout_slot_visible(app, (HudModuleSlot)slot)) {
            hud_slot_spawn_active(app->slots[slot]);
        }
    }
}

static gboolean modules_running(HudApp *app)
{
    if (app == NULL || app->modules == NULL) { return FALSE; }
    for (guint index = 0; index < app->modules->len; index++) {
        if (hud_module_is_running(g_ptr_array_index(app->modules, index))) {
            return TRUE;
        }
    }
    return FALSE;
}

void hud_terminal_stop_all(HudApp *app)
{
    if (app == NULL || app->modules == NULL) { return; }
    for (guint index = 0; index < app->modules->len; index++) {
        hud_module_stop(g_ptr_array_index(app->modules, index));
    }

    gint64 deadline = g_get_monotonic_time() + 1800 * G_TIME_SPAN_MILLISECOND;
    while (modules_running(app) && g_get_monotonic_time() < deadline) {
        while (g_main_context_iteration(NULL, FALSE)) {
        }
        g_usleep(20000);
    }
    for (guint index = 0; index < app->modules->len; index++) {
        hud_module_force_stop(g_ptr_array_index(app->modules, index));
    }

    deadline = g_get_monotonic_time() + 750 * G_TIME_SPAN_MILLISECOND;
    while (modules_running(app) && g_get_monotonic_time() < deadline) {
        while (g_main_context_iteration(NULL, FALSE)) {
        }
        g_usleep(20000);
    }
}

void hud_terminal_set_input(HudApp *app, gboolean enabled)
{
    if (app == NULL) { return; }
    app->module_input_enabled = enabled;
    for (int slot = 0; slot < HUD_SLOT_COUNT; slot++) {
        hud_slot_set_input(app->slots[slot], enabled);
    }
}

void hud_terminal_focus_default(HudApp *app)
{
    if (app == NULL || app->modules == NULL) { return; }
    for (guint pass = 0; pass < 2; pass++) {
        for (int slot = 0; slot < HUD_SLOT_COUNT; slot++) {
            if (!hud_layout_slot_visible(app, (HudModuleSlot)slot)) {
                continue;
            }
            HudModule *module = hud_slot_active(app->slots[slot]);
            if (pass == 0 && (module == NULL ||
                g_strcmp0(module->id, "console.shared") != 0)) {
                continue;
            }
            if (hud_slot_focus(app->slots[slot])) {
                return;
            }
        }
    }
}

void hud_terminal_free_all(HudApp *app)
{
    if (app == NULL) {
        return;
    }
    for (int slot = 0; slot < HUD_SLOT_COUNT; slot++) {
        hud_slot_free(app->slots[slot]);
        app->slots[slot] = NULL;
    }
    if (app->modules != NULL) {
        g_ptr_array_unref(app->modules);
        app->modules = NULL;
    }
    app->grid = NULL;
}
