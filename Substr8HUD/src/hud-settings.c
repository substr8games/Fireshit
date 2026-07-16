#include "hud-settings.h"

#include <glib/gstdio.h>
#include <errno.h>

#include "hud-app.h"
#include "hud-module.h"

static const char *layout_group(HudPlacement placement)
{
    return placement == HUD_PLACEMENT_FULLSCREEN
        ? "layout.fullscreen"
        : "layout.side";
}

const char *hud_placement_name(HudPlacement placement)
{
    switch (placement) {
    case HUD_PLACEMENT_LEFT:
        return "left";
    case HUD_PLACEMENT_FULLSCREEN:
        return "fullscreen";
    case HUD_PLACEMENT_RIGHT:
    default:
        return "right";
    }
}

gboolean hud_placement_parse(
    const char *text,
    HudPlacement *out_placement)
{
    if (text == NULL || out_placement == NULL) {
        return FALSE;
    }

    for (int value = HUD_PLACEMENT_LEFT;
         value <= HUD_PLACEMENT_FULLSCREEN;
         value++) {
        if (g_strcmp0(text, hud_placement_name((HudPlacement)value)) == 0) {
            *out_placement = (HudPlacement)value;
            return TRUE;
        }
    }

    return FALSE;
}

static gboolean slot_allowed(
    HudPlacement placement,
    HudModuleSlot slot)
{
    if (placement == HUD_PLACEMENT_FULLSCREEN) {
        return slot >= HUD_SLOT_TOP_LEFT && slot < HUD_SLOT_COUNT;
    }

    return slot == HUD_SLOT_TOP_LEFT ||
        slot == HUD_SLOT_TOP_RIGHT ||
        slot == HUD_SLOT_BOTTOM_LEFT_WIDE;
}

void hud_settings_load(HudApp *app)
{
    if (app == NULL) {
        return;
    }

    g_clear_pointer(&app->user_config_path, g_free);
    g_clear_pointer(&app->user_settings, g_key_file_unref);

    app->placement = HUD_PLACEMENT_RIGHT;
    app->user_settings_valid = TRUE;
    app->user_config_path = g_build_filename(
        g_get_user_config_dir(),
        "substr8-hud",
        "config.ini",
        NULL);
    app->user_settings = g_key_file_new();

    if (!g_file_test(app->user_config_path, G_FILE_TEST_IS_REGULAR)) {
        hud_app_log(
            app,
            "user settings absent; using right-side defaults: %s",
            app->user_config_path);
        return;
    }

    GError *error = NULL;
    if (!g_key_file_load_from_file(
            app->user_settings,
            app->user_config_path,
            G_KEY_FILE_KEEP_COMMENTS,
            &error)) {
        hud_app_log(
            app,
            "could not load user settings %s: %s",
            app->user_config_path,
            error->message);
        app->user_settings_valid = FALSE;
        g_clear_error(&error);
        return;
    }

    gchar *placement = g_key_file_get_string(
        app->user_settings,
        "hud",
        "placement",
        NULL);
    HudPlacement parsed;
    if (placement != NULL && hud_placement_parse(placement, &parsed)) {
        app->placement = parsed;
    } else if (placement != NULL) {
        hud_app_log(
            app,
            "invalid placement '%s'; using right",
            placement);
    }
    g_free(placement);
}

void hud_settings_apply_module(
    HudApp *app,
    HudModule *module)
{
    if (app == NULL || module == NULL) {
        return;
    }

    module->slot = module->default_slot;
    module->bank_order = module->default_bank_order;

    if (app->user_settings == NULL) {
        return;
    }

    const char *group = layout_group(app->placement);
    gchar *value = g_key_file_get_string(
        app->user_settings,
        group,
        module->id,
        NULL);
    if (value == NULL) {
        return;
    }

    gchar **parts = g_strsplit(value, ":", 2);
    HudModuleSlot slot;
    gchar *end = NULL;
    gint64 order = parts[1] != NULL
        ? g_ascii_strtoll(parts[1], &end, 10)
        : module->default_bank_order;

    gboolean valid_order = parts[1] == NULL ||
        (end != parts[1] && end != NULL && *end == '\0' &&
         order >= 0 && order <= G_MAXINT);
    gboolean valid = hud_module_slot_parse(parts[0], &slot) &&
        slot_allowed(app->placement, slot) && valid_order;

    if (valid) {
        module->slot = slot;
        module->bank_order = (gint)order;
    } else {
        hud_app_log(
            app,
            "invalid saved layout for %s: %s",
            module->id,
            value);
    }

    g_strfreev(parts);
    g_free(value);
}

static gboolean write_settings(
    HudApp *app,
    GError **error)
{
    gchar *directory = g_path_get_dirname(app->user_config_path);
    if (g_mkdir_with_parents(directory, 0700) != 0) {
        g_set_error(
            error,
            G_FILE_ERROR,
            g_file_error_from_errno(errno),
            "could not create %s",
            directory);
        g_free(directory);
        return FALSE;
    }
    g_free(directory);

    gsize length = 0;
    gchar *data = g_key_file_to_data(app->user_settings, &length, error);
    if (data == NULL) {
        return FALSE;
    }

    gboolean saved = g_file_set_contents(
        app->user_config_path,
        data,
        length,
        error);
    g_free(data);

    if (saved) {
        g_chmod(app->user_config_path, 0600);
    }
    return saved;
}

gboolean hud_settings_save_layout(
    HudApp *app,
    GError **error)
{
    if (app == NULL || app->user_settings == NULL ||
        app->user_config_path == NULL) {
        g_set_error(
            error,
            G_IO_ERROR,
            G_IO_ERROR_NOT_INITIALIZED,
            "SUBSTR8 HUD user settings are unavailable");
        return FALSE;
    }
    if (!app->user_settings_valid) {
        g_set_error(
            error,
            G_IO_ERROR,
            G_IO_ERROR_INVALID_DATA,
            "refusing to overwrite malformed SUBSTR8 HUD settings: %s",
            app->user_config_path);
        return FALSE;
    }

    g_key_file_set_string(
        app->user_settings,
        "hud",
        "placement",
        hud_placement_name(app->placement));

    const char *group = layout_group(app->placement);

    for (guint index = 0;
         app->modules != NULL && index < app->modules->len;
         index++) {
        HudModule *module = g_ptr_array_index(app->modules, index);
        gchar *value = g_strdup_printf(
            "%s:%d",
            hud_module_slot_name(module->slot),
            module->bank_order);
        g_key_file_set_string(
            app->user_settings,
            group,
            module->id,
            value);
        g_free(value);
    }

    return write_settings(app, error);
}
