#include "firemod-paths.h"

#include <string.h>

#include <glib/gstdio.h>

char *firemod_find_wayfire_config(void)
{
    const char *override = g_getenv("WAYFIRE_CONFIG_FILE");
    if (override != NULL && g_file_test(override, G_FILE_TEST_IS_REGULAR)) {
        return g_canonicalize_filename(override, NULL);
    }

    char *candidate = g_build_filename(g_get_user_config_dir(), "wayfire.ini", NULL);
    if (g_file_test(candidate, G_FILE_TEST_IS_REGULAR)) {
        return candidate;
    }
    g_free(candidate);

    candidate = g_build_filename(
        g_get_user_config_dir(), "wayfire", "wayfire.ini", NULL);
    if (g_file_test(candidate, G_FILE_TEST_IS_REGULAR)) {
        return candidate;
    }
    g_free(candidate);
    return NULL;
}

char *firemod_expand_path(const char *value)
{
    if (value == NULL || *value == '\0') {
        return NULL;
    }
    if (g_str_has_prefix(value, "~/")) {
        return g_build_filename(g_get_home_dir(), value + 2, NULL);
    }
    if (g_path_is_absolute(value)) {
        return g_canonicalize_filename(value, NULL);
    }
    return g_canonicalize_filename(value, g_get_home_dir());
}

char *firemod_normalize_token(const char *value)
{
    GString *normalized = g_string_new(NULL);
    if (value != NULL) {
        for (const char *cursor = value; *cursor != '\0'; cursor++) {
            if (g_ascii_isalnum(*cursor)) {
                g_string_append_c(normalized, g_ascii_tolower(*cursor));
            }
        }
    }
    return g_string_free(normalized, FALSE);
}

static gboolean has_ignored_suffix(const char *name)
{
    const char *suffixes[] = {
        "~", ".bak", ".backup", ".old", ".orig", ".rej", ".swp",
        ".swo", ".tmp", ".lock", ".disabled", ".dpkg-old", ".pacnew",
        ".pacsave"
    };
    for (guint i = 0; i < G_N_ELEMENTS(suffixes); i++) {
        if (g_str_has_suffix(name, suffixes[i])) {
            return TRUE;
        }
    }
    return FALSE;
}

gboolean firemod_path_is_ignored(const char *path)
{
    char *base = g_path_get_basename(path);
    gboolean ignored = base[0] == '.' || has_ignored_suffix(base) ||
        g_strcmp0(base, "Cache") == 0 || g_strcmp0(base, "cache") == 0 ||
        g_strcmp0(base, "logs") == 0 || g_strcmp0(base, "log") == 0;
    g_free(base);
    return ignored;
}

gboolean firemod_path_is_regular_config(const char *path)
{
    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR) ||
        firemod_path_is_ignored(path)) {
        return FALSE;
    }

    const char *suffixes[] = {
        ".ini", ".conf", ".cfg", ".toml", ".json", ".jsonc", ".json5",
        ".yaml", ".yml", ".xml", ".properties", ".env", ".desktop",
        ".service", ".socket", ".timer", ".lua", ".py", ".js", ".sh",
        ".sqlite", ".sqlite3", ".db"
    };
    for (guint i = 0; i < G_N_ELEMENTS(suffixes); i++) {
        if (g_str_has_suffix(path, suffixes[i])) {
            return TRUE;
        }
    }
    return FALSE;
}
