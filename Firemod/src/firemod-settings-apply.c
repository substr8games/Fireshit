#include "firemod-settings-private.h"

#include <string.h>

static gchar **build_action_argv(const FiremodCustomAction *action)
{
    gsize argument_count = action->arguments != NULL ?
        g_strv_length(action->arguments) : 0;
    gboolean desktop = action->type == FIREMOD_CUSTOM_APPLICATION &&
        g_str_has_suffix(action->target, ".desktop");
    gchar **argv = g_new0(gchar *, argument_count + (desktop ? 3 : 2));
    guint offset = 0;
    if (desktop) {
        argv[offset++] = g_strdup("gtk-launch");
        gchar *basename = g_path_get_basename(action->target);
        argv[offset++] = g_strndup(basename, strlen(basename) - 8);
        g_free(basename);
    } else {
        argv[offset++] = g_strdup(action->target);
    }
    for (gsize i = 0; i < argument_count; i++) {
        argv[offset++] = g_strdup(action->arguments[i]);
    }
    return argv;
}

static void apply_canonical_visibility(
    const FiremodSettingsState *state,
    FiremodCapabilityRegistry *registry)
{
    for (guint i = 0; i < FIREMOD_CANONICAL_COUNT; i++) {
        FiremodCapability *capability = firemod_capability_find(
            registry, firemod_settings_canonical_id(i));
        if (capability != NULL) {
            capability->menu_visible = state->canonical_visible[i];
        }
    }
}

static void apply_user_actions(
    const FiremodSettingsState *state,
    FiremodCapabilityRegistry *registry)
{
    firemod_capability_registry_remove_user_actions(registry);
    for (guint i = 0; i < state->actions->len; i++) {
        FiremodCustomAction *action = g_ptr_array_index(state->actions, i);
        if (!action->enabled) continue;
        gchar **argv = build_action_argv(action);
        FiremodCapability *capability = firemod_capability_new_user(
            action->id,
            action->hover_title,
            action->short_label,
            action->hover_subtitle,
            action->icon_path,
            action->color,
            argv,
            action->working_directory,
            action->run_in_terminal,
            (gint)i,
            action->type == FIREMOD_CUSTOM_SCRIPT ?
                FIREMOD_ORIGIN_USER_SCRIPT : FIREMOD_ORIGIN_USER_APPLICATION);
        g_free(capability->summary);
        capability->summary = g_strdup(action->hover_subtitle);
        g_free(capability->owner);
        capability->owner = g_strdup(action->label);
        firemod_capability_registry_add(registry, capability);
        g_strfreev(argv);
    }
}

gboolean firemod_settings_apply(
    FiremodSettings *settings,
    FiremodCapabilityRegistry *registry,
    GError **error)
{
    g_return_val_if_fail(settings != NULL && registry != NULL, FALSE);
    gchar *detail = NULL;
    if (!firemod_settings_validate(settings->saved, &detail)) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "%s", detail);
        g_free(detail);
        return FALSE;
    }
    apply_canonical_visibility(settings->saved, registry);
    apply_user_actions(settings->saved, registry);
    firemod_settings_state_free(settings->active);
    settings->active = firemod_settings_state_clone(settings->saved);
    return TRUE;
}
