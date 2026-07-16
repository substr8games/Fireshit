#include "firemod-parse.h"

#include <string.h>

#include <glib/gstdio.h>

#define FIREMOD_MAX_SETTINGS 36
#define FIREMOD_MAX_FILE_BYTES (1024 * 1024)

FiremodAuthorityType firemod_parse_type_for_path(const char *path)
{
    if (g_str_has_suffix(path, ".ini") || g_str_has_suffix(path, ".desktop") ||
        g_str_has_suffix(path, ".service") || g_str_has_suffix(path, ".socket") ||
        g_str_has_suffix(path, ".timer")) {
        return FIREMOD_AUTHORITY_INI;
    }
    if (g_str_has_suffix(path, ".toml")) return FIREMOD_AUTHORITY_TOML;
    if (g_str_has_suffix(path, ".json") || g_str_has_suffix(path, ".jsonc") ||
        g_str_has_suffix(path, ".json5")) return FIREMOD_AUTHORITY_JSON;
    if (g_str_has_suffix(path, ".yaml") || g_str_has_suffix(path, ".yml"))
        return FIREMOD_AUTHORITY_YAML;
    if (g_str_has_suffix(path, ".xml")) return FIREMOD_AUTHORITY_XML;
    if (g_str_has_suffix(path, ".properties") || g_str_has_suffix(path, ".env") ||
        g_str_has_suffix(path, ".conf") || g_str_has_suffix(path, ".cfg"))
        return FIREMOD_AUTHORITY_PROPERTIES;
    if (g_str_has_suffix(path, ".lua") || g_str_has_suffix(path, ".py") ||
        g_str_has_suffix(path, ".js") || g_str_has_suffix(path, ".sh"))
        return FIREMOD_AUTHORITY_SCRIPT;
    if (g_str_has_suffix(path, ".sqlite") || g_str_has_suffix(path, ".sqlite3") ||
        g_str_has_suffix(path, ".db")) return FIREMOD_AUTHORITY_SQLITE;
    return FIREMOD_AUTHORITY_UNKNOWN;
}

static void add_setting(
    FiremodAuthority *authority,
    const char *name,
    const char *value,
    const char *key_path)
{
    if (authority->settings->len >= FIREMOD_MAX_SETTINGS) {
        return;
    }
    char *trimmed = g_strstrip(g_strdup(value != NULL ? value : ""));
    if (strlen(trimmed) > 180) {
        trimmed[177] = '.';
        trimmed[178] = '.';
        trimmed[179] = '.';
        trimmed[180] = '\0';
    }
    firemod_authority_add_setting(authority, firemod_setting_new(
        name, trimmed, key_path, FIREMOD_CAPABILITY_READABLE));
    g_free(trimmed);
}

static gboolean parse_key_file(FiremodAuthority *authority)
{
    GKeyFile *key_file = g_key_file_new();
    GError *error = NULL;
    if (!g_key_file_load_from_file(
            key_file, authority->path, G_KEY_FILE_KEEP_COMMENTS, &error)) {
        g_clear_error(&error);
        g_key_file_unref(key_file);
        return FALSE;
    }

    gsize group_count = 0;
    char **groups = g_key_file_get_groups(key_file, &group_count);
    for (gsize i = 0; i < group_count &&
            authority->settings->len < FIREMOD_MAX_SETTINGS; i++) {
        gsize key_count = 0;
        char **keys = g_key_file_get_keys(key_file, groups[i], &key_count, NULL);
        for (gsize j = 0; j < key_count &&
                authority->settings->len < FIREMOD_MAX_SETTINGS; j++) {
            char *value = g_key_file_get_value(key_file, groups[i], keys[j], NULL);
            char *path = g_strdup_printf("[%s] %s", groups[i], keys[j]);
            add_setting(authority, keys[j], value, path);
            g_free(path);
            g_free(value);
        }
        g_strfreev(keys);
    }
    g_strfreev(groups);
    g_key_file_unref(key_file);
    return authority->settings->len > 0;
}

static gboolean parse_line_values(FiremodAuthority *authority)
{
    char *contents = NULL;
    gsize length = 0;
    if (!g_file_get_contents(authority->path, &contents, &length, NULL) ||
        length > FIREMOD_MAX_FILE_BYTES) {
        g_free(contents);
        return FALSE;
    }

    char **lines = g_strsplit(contents, "\n", -1);
    char *section = NULL;
    for (guint i = 0; lines[i] != NULL &&
            authority->settings->len < FIREMOD_MAX_SETTINGS; i++) {
        char *line = g_strstrip(lines[i]);
        if (*line == '\0' || *line == '#' || *line == ';' || *line == '{' ||
            *line == '}' || g_str_has_prefix(line, "//")) {
            continue;
        }
        if (*line == '[' && g_str_has_suffix(line, "]")) {
            g_free(section);
            section = g_strndup(line + 1, strlen(line) - 2);
            continue;
        }
        char *delimiter = strchr(line, '=');
        if (delimiter == NULL && authority->type == FIREMOD_AUTHORITY_PROPERTIES) {
            delimiter = strchr(line, ':');
        }
        if (delimiter == NULL) {
            continue;
        }
        *delimiter = '\0';
        char *name = g_strstrip(line);
        char *value = g_strstrip(delimiter + 1);
        if (*name == '\0' || strchr(name, ' ') != NULL) {
            continue;
        }
        char *key_path = section != NULL ?
            g_strdup_printf("[%s] %s", section, name) : g_strdup(name);
        add_setting(authority, name, value, key_path);
        g_free(key_path);
    }
    g_free(section);
    g_strfreev(lines);
    g_free(contents);
    return authority->settings->len > 0;
}

void firemod_parse_authority(FiremodAuthority *authority)
{
    g_return_if_fail(authority != NULL);
    if (authority->path == NULL || authority->uncertain ||
        !g_file_test(authority->path, G_FILE_TEST_IS_REGULAR)) {
        return;
    }

    gboolean parsed = FALSE;
    if (authority->type == FIREMOD_AUTHORITY_INI) {
        parsed = parse_key_file(authority);
    }
    if (!parsed && (authority->type == FIREMOD_AUTHORITY_TOML ||
            authority->type == FIREMOD_AUTHORITY_PROPERTIES)) {
        parsed = parse_line_values(authority);
    }
    if (parsed) {
        authority->capability = FIREMOD_CAPABILITY_READABLE;
    }
}
