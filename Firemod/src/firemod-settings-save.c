#include "firemod-settings-private.h"

#include <cairo.h>
#include <errno.h>
#include <glib/gstdio.h>
#include <string.h>

static gboolean valid_png(const gchar *path)
{
    if (path == NULL || *path == '\0') return TRUE;
    if (!g_str_has_suffix(path, ".png") && !g_str_has_suffix(path, ".PNG")) {
        return FALSE;
    }
    cairo_surface_t *surface = cairo_image_surface_create_from_png(path);
    gboolean valid = cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS;
    cairo_surface_destroy(surface);
    return valid;
}

static gboolean valid_color(const gchar *color)
{
    return color != NULL && g_regex_match_simple(
        "^#[0-9A-Fa-f]{6}$", color, 0, 0);
}

gboolean firemod_settings_validate(
    const FiremodSettingsState *state,
    gchar **detail)
{
    if (detail != NULL) *detail = NULL;
    if (state == NULL || state->menu_scale < 0.5 || state->menu_scale > 1.0) {
        if (detail != NULL) *detail = g_strdup(
            "Menu scale must remain between 0.50 and 1.00");
        return FALSE;
    }
    if (state->hover_delay_ms > 2000) {
        if (detail != NULL) *detail = g_strdup(
            "Hover delay must remain between 0 and 2000 ms");
        return FALSE;
    }
    GHashTable *ids = g_hash_table_new(g_str_hash, g_str_equal);
    for (guint i = 0; i < state->actions->len; i++) {
        FiremodCustomAction *action = g_ptr_array_index(state->actions, i);
        const gchar *problem = NULL;
        if (action->id == NULL || !g_regex_match_simple(
                "^user\\.[A-Za-z0-9._-]+$", action->id, 0, 0)) {
            problem = "has an invalid stable ID";
        } else if (g_hash_table_contains(ids, action->id)) {
            problem = "duplicates another stable ID";
        } else if (action->label == NULL || *action->label == '\0') {
            problem = "requires a name";
        } else if (action->short_label == NULL || *action->short_label == '\0') {
            problem = "requires a short label";
        } else if (action->hover_title == NULL || *action->hover_title == '\0') {
            problem = "requires hover text";
        } else if (action->target == NULL || *action->target == '\0') {
            problem = "requires an application or script target";
        } else if (!valid_color(action->color)) {
            problem = "requires an opaque #RRGGBB color";
        } else if (!valid_png(action->icon_path)) {
            problem = "has an invalid custom icon; only readable PNG files are accepted";
        }
        if (problem != NULL) {
            if (detail != NULL) *detail = g_strdup_printf(
                "%s %s", action->label != NULL ? action->label : "Custom action",
                problem);
            g_hash_table_unref(ids);
            return FALSE;
        }
        g_hash_table_add(ids, action->id);
    }
    g_hash_table_unref(ids);
    return TRUE;
}

static gchar *state_text(const FiremodSettingsState *state)
{
    GKeyFile *key = firemod_settings_state_to_key_file(state);
    gchar *text = g_key_file_to_data(key, NULL, NULL);
    g_key_file_unref(key);
    return text;
}

static gint compare_string_pointers(gconstpointer left, gconstpointer right)
{
    const gchar * const *a = left;
    const gchar * const *b = right;
    return g_strcmp0(*a, *b);
}

static void prune_history(const gchar *directory)
{
    GDir *dir = g_dir_open(directory, 0, NULL);
    if (dir == NULL) return;
    GPtrArray *files = g_ptr_array_new_with_free_func(g_free);
    const gchar *name = NULL;
    while ((name = g_dir_read_name(dir)) != NULL) {
        if (g_str_has_suffix(name, ".ini")) g_ptr_array_add(files, g_strdup(name));
    }
    g_dir_close(dir);
    g_ptr_array_sort(files, compare_string_pointers);
    while (files->len > 10) {
        gchar *oldest = g_ptr_array_index(files, 0);
        gchar *path = g_build_filename(directory, oldest, NULL);
        g_unlink(path);
        g_free(path);
        g_ptr_array_remove_index(files, 0);
    }
    g_ptr_array_free(files, TRUE);
}

static gboolean write_history(
    FiremodSettings *settings,
    const gchar *text,
    GError **error)
{
    if (g_mkdir_with_parents(settings->history_dir, 0700) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
            "Could not create Firemod history directory");
        return FALSE;
    }
    GDateTime *now = g_date_time_new_now_local();
    gchar *stamp = g_date_time_format(now, "%Y%m%dT%H%M%S");
    g_date_time_unref(now);
    gchar *checksum = g_compute_checksum_for_string(
        G_CHECKSUM_SHA256, text, -1);
    gchar short_hash[9] = {0};
    memcpy(short_hash, checksum, 8);
    gchar *name = g_strdup_printf("%s-%s.ini", stamp, short_hash);
    gchar *path = g_build_filename(settings->history_dir, name, NULL);
    gboolean ok = g_file_set_contents(path, text, -1, error);
    g_free(path); g_free(name); g_free(checksum); g_free(stamp);
    if (ok) prune_history(settings->history_dir);
    return ok;
}

static gboolean write_action_records(
    FiremodSettings *settings,
    const FiremodSettingsState *state,
    GError **error)
{
    if (g_mkdir_with_parents(settings->actions_dir, 0700) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
            "Could not create Firemod actions directory");
        return FALSE;
    }
    GHashTable *live = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    for (guint i = 0; i < state->actions->len; i++) {
        FiremodCustomAction *action = g_ptr_array_index(state->actions, i);
        FiremodSettingsState *one = firemod_settings_state_new_default();
        g_ptr_array_add(one->actions, firemod_custom_action_clone(action));
        GKeyFile *serialized = firemod_settings_state_to_key_file(one);
        gsize length = 0;
        gchar *data = g_key_file_to_data(serialized, &length, NULL);
        g_key_file_unref(serialized);
        firemod_settings_state_free(one);
        gchar *name = g_strdup_printf("%s.action", action->id);
        gchar *path = g_build_filename(settings->actions_dir, name, NULL);
        gboolean ok = g_file_set_contents(path, data, (gssize)length, error);
        g_hash_table_add(live, g_strdup(name));
        g_free(path); g_free(name); g_free(data);
        if (!ok) { g_hash_table_unref(live); return FALSE; }
    }
    GDir *dir = g_dir_open(settings->actions_dir, 0, NULL);
    const gchar *name = NULL;
    while (dir != NULL && (name = g_dir_read_name(dir)) != NULL) {
        if (g_str_has_suffix(name, ".action") &&
            !g_hash_table_contains(live, name)) {
            gchar *path = g_build_filename(settings->actions_dir, name, NULL);
            g_unlink(path);
            g_free(path);
        }
    }
    if (dir != NULL) g_dir_close(dir);
    g_hash_table_unref(live);
    return TRUE;
}

gboolean firemod_settings_save(FiremodSettings *settings, GError **error)
{
    g_return_val_if_fail(settings != NULL, FALSE);
    gchar *detail = NULL;
    if (!firemod_settings_validate(settings->draft, &detail)) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "%s", detail);
        g_free(detail);
        return FALSE;
    }
    gchar *new_text = state_text(settings->draft);
    gchar *old_text = state_text(settings->saved);
    if (g_strcmp0(new_text, old_text) == 0) {
        settings->dirty = FALSE;
        g_free(new_text); g_free(old_text);
        return TRUE;
    }
    gchar *directory = g_path_get_dirname(settings->config_path);
    if (g_mkdir_with_parents(directory, 0700) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
            "Could not create Firemod configuration directory");
        g_free(directory); g_free(new_text); g_free(old_text);
        return FALSE;
    }
    g_free(directory);
    if (!write_action_records(settings, settings->draft, error) ||
        !g_file_set_contents(settings->config_path, new_text, -1, error)) {
        g_free(new_text); g_free(old_text);
        return FALSE;
    }
    if (!write_history(settings, new_text, error)) {
        GError *restore_error = NULL;
        g_file_set_contents(settings->config_path, old_text, -1, &restore_error);
        g_clear_error(&restore_error);
        g_free(new_text); g_free(old_text);
        return FALSE;
    }
    firemod_settings_state_free(settings->saved);
    settings->saved = firemod_settings_state_clone(settings->draft);
    settings->dirty = FALSE;
    g_free(new_text); g_free(old_text);
    return TRUE;
}
