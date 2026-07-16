#include "hud-module-path.h"

#include <glib.h>

#include "hud-app.h"

static gboolean legacy_default_directory(const char *path)
{
    if (path == NULL) {
        return FALSE;
    }

    if (g_strcmp0(
            path,
            "/usr/local/share/wayfire-hud/modules.d") == 0 ||
        g_strcmp0(
            path,
            "/usr/share/wayfire-hud/modules.d") == 0) {
        return TRUE;
    }

    gchar *legacy_user = g_build_filename(
        g_get_user_config_dir(),
        "wayfire-hud",
        "modules.d",
        NULL);
    gboolean legacy = g_strcmp0(path, legacy_user) == 0;
    g_free(legacy_user);
    return legacy;
}

static gchar *first_directory(const char *const *paths)
{
    for (guint index = 0; paths[index] != NULL; index++) {
        if (g_file_test(paths[index], G_FILE_TEST_IS_DIR)) {
            return g_strdup(paths[index]);
        }
    }

    return NULL;
}

char *hud_module_directory_find(HudApp *app)
{
    if (app->module_directory != NULL &&
        !legacy_default_directory(app->module_directory) &&
        g_file_test(app->module_directory, G_FILE_TEST_IS_DIR)) {
        return g_strdup(app->module_directory);
    }

    if (legacy_default_directory(app->module_directory)) {
        hud_app_log(
            app,
            "ignoring legacy module directory: %s",
            app->module_directory);
    }

    const char *override = g_getenv("SUBSTR8_HUD_MODULE_DIR");
    if (override == NULL || *override == '\0') {
        override = g_getenv("WAYFIRE_HUD_MODULE_DIR");
    }
    if (override != NULL &&
        g_file_test(override, G_FILE_TEST_IS_DIR)) {
        return g_strdup(override);
    }

    gchar *user = g_build_filename(
        g_get_user_config_dir(),
        "substr8-hud",
        "modules.d",
        NULL);
    if (g_file_test(user, G_FILE_TEST_IS_DIR)) {
        return user;
    }
    g_free(user);

    static const char *installed[] = {
        SUBSTR8_HUD_DEFAULT_MODULE_DIR,
        "/usr/local/share/substr8-hud/modules.d",
        "/usr/share/substr8-hud/modules.d",
        NULL,
    };

    return first_directory(installed);
}
