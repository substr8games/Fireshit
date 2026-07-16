#include "firemod-settings-private.h"

#include <glib/gstdio.h>
#include <string.h>

static gchar *action_group(const gchar *id)
{
    return g_strdup_printf("action:%s", id);
}

static void action_to_key(
    GKeyFile *key,
    const gchar *group,
    const FiremodCustomAction *action)
{
    g_key_file_set_string(key, group, "id", action->id);
    g_key_file_set_string(key, group, "label", action->label);
    g_key_file_set_string(key, group, "short-label", action->short_label);
    g_key_file_set_string(key, group, "hover-title", action->hover_title);
    g_key_file_set_string(key, group, "hover-subtitle", action->hover_subtitle);
    g_key_file_set_string(key, group, "type",
        action->type == FIREMOD_CUSTOM_SCRIPT ? "script" : "application");
    g_key_file_set_string(key, group, "target", action->target);
    gsize count = g_strv_length(action->arguments);
    g_key_file_set_string_list(key, group, "arguments",
        (const gchar * const *)action->arguments, count);
    g_key_file_set_string(key, group, "working-directory",
        action->working_directory);
    g_key_file_set_string(key, group, "icon-path", action->icon_path);
    g_key_file_set_string(key, group, "color", action->color);
    g_key_file_set_boolean(key, group, "run-in-terminal",
        action->run_in_terminal);
    g_key_file_set_boolean(key, group, "enabled", action->enabled);
}

GKeyFile *firemod_settings_state_to_key_file(
    const FiremodSettingsState *state)
{
    GKeyFile *key = g_key_file_new();
    g_key_file_set_boolean(key, "menu", "enabled", state->menu_enabled);
    g_key_file_set_double(key, "menu", "scale", state->menu_scale);
    g_key_file_set_boolean(key, "menu", "hover-enabled", state->hover_enabled);
    g_key_file_set_integer(key, "menu", "hover-delay-ms",
        (gint)state->hover_delay_ms);
    for (guint i = 0; i < FIREMOD_CANONICAL_COUNT; i++) {
        g_key_file_set_boolean(key, "canonical",
            firemod_settings_canonical_id(i), state->canonical_visible[i]);
    }
    gchar **ids = g_new0(gchar *, state->actions->len + 1);
    for (guint i = 0; i < state->actions->len; i++) {
        FiremodCustomAction *action = g_ptr_array_index(state->actions, i);
        ids[i] = g_strdup(action->id);
        gchar *group = action_group(action->id);
        action_to_key(key, group, action);
        g_free(group);
    }
    g_key_file_set_string_list(key, "actions", "order",
        (const gchar * const *)ids, state->actions->len);
    g_strfreev(ids);
    return key;
}

static gchar *get_text(
    GKeyFile *key,
    const gchar *group,
    const gchar *field,
    const gchar *fallback)
{
    gchar *value = g_key_file_get_string(key, group, field, NULL);
    return value != NULL ? value : g_strdup(fallback);
}

static FiremodCustomAction *action_from_key(
    GKeyFile *key,
    const gchar *group)
{
    gchar *kind = get_text(key, group, "type", "application");
    FiremodCustomAction *action = firemod_custom_action_new(
        g_strcmp0(kind, "script") == 0 ?
            FIREMOD_CUSTOM_SCRIPT : FIREMOD_CUSTOM_APPLICATION);
    g_free(kind);
    g_free(action->id);
    action->id = get_text(key, group, "id", "user.invalid");
#define READ_TEXT(member, field, fallback) \
    g_free(action->member); action->member = get_text(key, group, field, fallback)
    READ_TEXT(label, "label", "Custom Action");
    READ_TEXT(short_label, "short-label", "ACTION");
    READ_TEXT(hover_title, "hover-title", action->label);
    READ_TEXT(hover_subtitle, "hover-subtitle", "");
    READ_TEXT(target, "target", "");
    READ_TEXT(working_directory, "working-directory", "");
    READ_TEXT(icon_path, "icon-path", "");
    READ_TEXT(color, "color", "#891688");
#undef READ_TEXT
    g_strfreev(action->arguments);
    action->arguments = g_key_file_get_string_list(
        key, group, "arguments", NULL, NULL);
    if (action->arguments == NULL) action->arguments = g_new0(gchar *, 1);
    action->run_in_terminal = g_key_file_get_boolean(
        key, group, "run-in-terminal", NULL);
    action->enabled = !g_key_file_has_key(key, group, "enabled", NULL) ||
        g_key_file_get_boolean(key, group, "enabled", NULL);
    return action;
}

FiremodSettingsState *firemod_settings_state_from_key_file(
    GKeyFile *key,
    GError **error)
{
    (void)error;
    FiremodSettingsState *state = firemod_settings_state_new_default();
    if (g_key_file_has_group(key, "menu")) {
        state->menu_enabled = g_key_file_get_boolean(key, "menu", "enabled", NULL);
        state->menu_scale = g_key_file_get_double(key, "menu", "scale", NULL);
        state->hover_enabled = g_key_file_get_boolean(
            key, "menu", "hover-enabled", NULL);
        state->hover_delay_ms = (guint)MAX(0, g_key_file_get_integer(
            key, "menu", "hover-delay-ms", NULL));
    }
    for (guint i = 0; i < FIREMOD_CANONICAL_COUNT; i++) {
        const gchar *id = firemod_settings_canonical_id(i);
        if (g_key_file_has_key(key, "canonical", id, NULL)) {
            state->canonical_visible[i] = g_key_file_get_boolean(
                key, "canonical", id, NULL);
        }
    }
    gsize count = 0;
    gchar **ids = g_key_file_get_string_list(
        key, "actions", "order", &count, NULL);
    for (gsize i = 0; ids != NULL && i < count; i++) {
        gchar *group = action_group(ids[i]);
        if (g_key_file_has_group(key, group)) {
            g_ptr_array_add(state->actions, action_from_key(key, group));
        }
        g_free(group);
    }
    g_strfreev(ids);
    return state;
}

static gboolean load_existing(
    FiremodSettings *settings,
    FiremodSettingsState **out,
    GError **error)
{
    GKeyFile *key = g_key_file_new();
    if (!g_file_test(settings->config_path, G_FILE_TEST_EXISTS)) {
        *out = firemod_settings_state_new_default();
        g_key_file_unref(key);
        return TRUE;
    }
    if (!g_key_file_load_from_file(key, settings->config_path,
            G_KEY_FILE_KEEP_COMMENTS, error)) {
        g_key_file_unref(key);
        return FALSE;
    }
    FiremodSettingsState *state = firemod_settings_state_from_key_file(
        key, error);
    g_key_file_unref(key);
    if (state == NULL) return FALSE;
    *out = state;
    return TRUE;
}

FiremodSettings *firemod_settings_new(GError **error)
{
    FiremodSettings *settings = g_new0(FiremodSettings, 1);
    gchar *config_dir = g_build_filename(g_get_user_config_dir(),
        "firemod", NULL);
    settings->config_path = g_build_filename(config_dir, "config.ini", NULL);
    settings->actions_dir = g_build_filename(config_dir, "actions.d", NULL);
    g_free(config_dir);
    settings->history_dir = g_build_filename(g_get_user_state_dir(),
        "firemod", "history", NULL);
    if (!load_existing(settings, &settings->saved, error)) {
        settings->saved = firemod_settings_state_new_default();
    }
    settings->draft = firemod_settings_state_clone(settings->saved);
    settings->active = firemod_settings_state_clone(settings->saved);
    return settings;
}
