#include "firemod-settings.h"

static const gchar *canonical_ids[FIREMOD_CANONICAL_COUNT] = {
    "core.settings.open",
    "capture.screenshot.output",
    "capture.screenshot.region",
    "capture.record.region",
    "capture.record.output",
    "capture.record.application",
    "utility.color.pick"
};

static const gchar *canonical_labels[FIREMOD_CANONICAL_COUNT] = {
    "Firemod Settings", "Screenshot", "Screen Snip", "Record Region",
    "Record Screen", "Record Application", "Color Picker"
};

const gchar *firemod_settings_canonical_id(guint slot)
{
    return slot < FIREMOD_CANONICAL_COUNT ? canonical_ids[slot] : NULL;
}

const gchar *firemod_settings_canonical_label(guint slot)
{
    return slot < FIREMOD_CANONICAL_COUNT ? canonical_labels[slot] : NULL;
}

FiremodCustomAction *firemod_custom_action_new(FiremodCustomType type)
{
    FiremodCustomAction *action = g_new0(FiremodCustomAction, 1);
    gchar *uuid = g_uuid_string_random();
    action->id = g_strdup_printf("user.%s", uuid);
    g_free(uuid);
    action->type = type;
    action->label = g_strdup(type == FIREMOD_CUSTOM_SCRIPT ?
        "New Script" : "New Application");
    action->short_label = g_strdup(type == FIREMOD_CUSTOM_SCRIPT ?
        "SCRIPT" : "APPLICATION");
    action->hover_title = g_strdup(action->label);
    action->hover_subtitle = g_strdup("");
    action->target = g_strdup("");
    action->arguments = g_new0(gchar *, 1);
    action->working_directory = g_strdup("");
    action->icon_path = g_strdup("");
    action->color = g_strdup("#891688");
    action->enabled = TRUE;
    return action;
}

FiremodCustomAction *firemod_custom_action_clone(
    const FiremodCustomAction *source)
{
    if (source == NULL) return NULL;
    FiremodCustomAction *copy = g_new0(FiremodCustomAction, 1);
    copy->id = g_strdup(source->id);
    copy->label = g_strdup(source->label);
    copy->short_label = g_strdup(source->short_label);
    copy->hover_title = g_strdup(source->hover_title);
    copy->hover_subtitle = g_strdup(source->hover_subtitle);
    copy->type = source->type;
    copy->target = g_strdup(source->target);
    copy->arguments = g_strdupv(source->arguments);
    copy->working_directory = g_strdup(source->working_directory);
    copy->icon_path = g_strdup(source->icon_path);
    copy->color = g_strdup(source->color);
    copy->run_in_terminal = source->run_in_terminal;
    copy->enabled = source->enabled;
    return copy;
}

void firemod_custom_action_free(FiremodCustomAction *action)
{
    if (action == NULL) return;
    g_free(action->id);
    g_free(action->label);
    g_free(action->short_label);
    g_free(action->hover_title);
    g_free(action->hover_subtitle);
    g_free(action->target);
    g_strfreev(action->arguments);
    g_free(action->working_directory);
    g_free(action->icon_path);
    g_free(action->color);
    g_free(action);
}

static FiremodSettingsState *state_new(void)
{
    FiremodSettingsState *state = g_new0(FiremodSettingsState, 1);
    state->menu_enabled = TRUE;
    state->menu_scale = 1.0;
    state->hover_enabled = TRUE;
    state->hover_delay_ms = 200;
    for (guint i = 0; i < FIREMOD_CANONICAL_COUNT; i++) {
        state->canonical_visible[i] = TRUE;
    }
    state->actions = g_ptr_array_new_with_free_func(
        (GDestroyNotify)firemod_custom_action_free);
    return state;
}

static FiremodSettingsState *state_clone(const FiremodSettingsState *source)
{
    FiremodSettingsState *copy = state_new();
    if (source == NULL) return copy;
    copy->menu_enabled = source->menu_enabled;
    copy->menu_scale = source->menu_scale;
    copy->hover_enabled = source->hover_enabled;
    copy->hover_delay_ms = source->hover_delay_ms;
    for (guint i = 0; i < FIREMOD_CANONICAL_COUNT; i++) {
        copy->canonical_visible[i] = source->canonical_visible[i];
    }
    for (guint i = 0; i < source->actions->len; i++) {
        g_ptr_array_add(copy->actions, firemod_custom_action_clone(
            g_ptr_array_index(source->actions, i)));
    }
    return copy;
}

static void state_free(FiremodSettingsState *state)
{
    if (state == NULL) return;
    g_ptr_array_free(state->actions, TRUE);
    g_free(state);
}

void firemod_settings_mark_dirty(FiremodSettings *settings)
{
    if (settings != NULL) settings->dirty = TRUE;
}

void firemod_settings_discard(FiremodSettings *settings)
{
    if (settings == NULL) return;
    state_free(settings->draft);
    settings->draft = state_clone(settings->saved);
    settings->dirty = FALSE;
}

void firemod_settings_free(FiremodSettings *settings)
{
    if (settings == NULL) return;
    state_free(settings->saved);
    state_free(settings->draft);
    state_free(settings->active);
    g_free(settings->config_path);
    g_free(settings->actions_dir);
    g_free(settings->history_dir);
    g_free(settings);
}

FiremodSettingsState *firemod_settings_state_new_default(void)
{
    return state_new();
}

FiremodSettingsState *firemod_settings_state_clone(
    const FiremodSettingsState *source)
{
    return state_clone(source);
}

void firemod_settings_state_free(FiremodSettingsState *state)
{
    state_free(state);
}
