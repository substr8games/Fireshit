#include "hud-config.h"

#include <glib.h>
#include <string.h>

#include "hud-app.h"

static gboolean section_line(
    const char *trimmed)
{
    gsize length = strlen(trimmed);
    return length >= 2 &&
        trimmed[0] == '[' &&
        trimmed[length - 1] == ']';
}

static gboolean opacity_line(
    const char *trimmed)
{
    const char *equals = strchr(trimmed, '=');
    if (equals == NULL) {
        return FALSE;
    }

    gchar *key = g_strndup(
        trimmed,
        equals - trimmed);
    g_strstrip(key);
    gboolean result = g_strcmp0(key, "opacity") == 0;
    g_free(key);
    return result;
}

static void append_opacity(
    GString *output,
    gdouble opacity)
{
    g_string_append_printf(
        output,
        "opacity = %.2f\n",
        opacity);
}

gboolean hud_config_save_opacity(
    HudApp *app,
    GError **error)
{
    if (app == NULL ||
        app->config_path == NULL ||
        *app->config_path == '\0') {
        g_set_error(
            error,
            G_FILE_ERROR,
            G_FILE_ERROR_NOENT,
            "Wayfire configuration path is unavailable");
        return FALSE;
    }

    gchar *contents = NULL;
    if (!g_file_get_contents(
            app->config_path,
            &contents,
            NULL,
            error)) {
        return FALSE;
    }

    gchar **lines = g_strsplit(contents, "\n", -1);
    GString *output = g_string_new(NULL);
    gboolean in_target = FALSE;
    gboolean group_found = FALSE;
    gboolean opacity_written = FALSE;

    for (guint index = 0;
         lines[index] != NULL;
         index++) {
        gchar *trimmed = g_strdup(lines[index]);
        g_strstrip(trimmed);

        if (section_line(trimmed)) {
            if (in_target && !opacity_written) {
                append_opacity(output, app->opacity);
            }

            in_target =
                g_strcmp0(trimmed, "[wayfire-hud]") == 0;
            if (in_target) {
                group_found = TRUE;
                opacity_written = FALSE;
            }
        }

        if (in_target && opacity_line(trimmed)) {
            if (!opacity_written) {
                append_opacity(output, app->opacity);
                opacity_written = TRUE;
            }

            g_free(trimmed);
            continue;
        }

        g_string_append(output, lines[index]);
        g_string_append_c(output, '\n');
        g_free(trimmed);
    }

    if (in_target && !opacity_written) {
        append_opacity(output, app->opacity);
    }

    if (!group_found) {
        if (output->len > 0 &&
            output->str[output->len - 1] != '\n') {
            g_string_append_c(output, '\n');
        }

        g_string_append(
            output,
            "\n[wayfire-hud]\n");
        append_opacity(output, app->opacity);
    }

    gboolean saved = g_file_set_contents(
        app->config_path,
        output->str,
        output->len,
        error);

    g_string_free(output, TRUE);
    g_strfreev(lines);
    g_free(contents);
    return saved;
}
