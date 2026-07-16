#include "hud-config.h"

#include <glib.h>

#include "hud-app.h"
#include "hud-state.h"

static gchar *find_wayfire_config(void)
{
    const char *override = g_getenv("WAYFIRE_CONFIG_FILE");
    if (override != NULL && g_file_test(override, G_FILE_TEST_IS_REGULAR)) {
        return g_strdup(override);
    }

    gchar *flat = g_build_filename(
        g_get_user_config_dir(), "wayfire.ini", NULL);
    if (g_file_test(flat, G_FILE_TEST_IS_REGULAR)) {
        return flat;
    }
    g_free(flat);

    gchar *nested = g_build_filename(
        g_get_user_config_dir(), "wayfire", "wayfire.ini", NULL);
    if (g_file_test(nested, G_FILE_TEST_IS_REGULAR)) {
        return nested;
    }
    g_free(nested);
    return NULL;
}

static gchar *expand_home(const char *path)
{
    if (path == NULL || *path == '\0') {
        return NULL;
    }

    if (g_str_has_prefix(path, "~/")) {
        return g_build_filename(g_get_home_dir(), path + 2, NULL);
    }

    return g_strdup(path);
}

static gint clamp_int(gint value, gint minimum, gint maximum)
{
    return MAX(minimum, MIN(maximum, value));
}

static gdouble clamp_double(
    gdouble value,
    gdouble minimum,
    gdouble maximum)
{
    return MAX(minimum, MIN(maximum, value));
}

void hud_config_load(HudApp *app)
{
    g_free(app->config_path);
    g_free(app->module_directory);

    app->config_path = find_wayfire_config();
    app->module_directory = NULL;
    app->width_percent = 50;
    app->opacity = 0.58;

    if (app->config_path == NULL) {
        return;
    }

    GKeyFile *key = g_key_file_new();
    GError *error = NULL;

    if (!g_key_file_load_from_file(
            key,
            app->config_path,
            G_KEY_FILE_NONE,
            &error)) {
        hud_app_log(app, "could not load %s: %s",
            app->config_path, error->message);
        g_clear_error(&error);
        g_key_file_unref(key);
        return;
    }

    if (!g_key_file_has_group(key, "wayfire-hud")) {
        g_key_file_unref(key);
        return;
    }

    if (g_key_file_has_key(
            key, "wayfire-hud", "width_percent", NULL)) {
        app->width_percent = clamp_int(
            g_key_file_get_integer(
                key, "wayfire-hud", "width_percent", NULL),
            25,
            100);
    }

    if (g_key_file_has_key(
            key, "wayfire-hud", "opacity", NULL)) {
        app->opacity = clamp_double(
            g_key_file_get_double(
                key, "wayfire-hud", "opacity", NULL),
            0.15,
            1.00);
    }

    gchar *modules = g_key_file_get_string(
        key, "wayfire-hud", "module_directory", NULL);
    app->module_directory = expand_home(modules);
    g_free(modules);

    gchar *initial = g_key_file_get_string(
        key, "wayfire-hud", "initial_state", NULL);
    HudState parsed;
    if (initial != NULL && hud_state_parse(initial, &parsed)) {
        app->state = parsed;
    }
    g_free(initial);

    g_key_file_unref(key);
}
