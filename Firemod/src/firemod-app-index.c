#include "firemod-app-index.h"

#include <string.h>

#include "firemod-paths.h"

struct _FiremodAppCandidate {
    GAppInfo *app;
    GHashTable *aliases;
};

static void candidate_free(FiremodAppCandidate *candidate)
{
    if (candidate == NULL) return;
    g_clear_object(&candidate->app);
    g_hash_table_unref(candidate->aliases);
    g_free(candidate);
}

static void add_alias(GHashTable *aliases, const char *value)
{
    char *normalized = firemod_normalize_token(value);
    if (normalized[0] != '\0') {
        g_hash_table_add(aliases, normalized);
    } else {
        g_free(normalized);
    }
}

GPtrArray *firemod_app_index_load(void)
{
    GPtrArray *candidates = g_ptr_array_new_with_free_func(
        (GDestroyNotify)candidate_free);
    GList *apps = g_app_info_get_all();
    for (GList *item = apps; item != NULL; item = item->next) {
        GAppInfo *app = G_APP_INFO(item->data);
        const char *id = g_app_info_get_id(app);
        const char *executable = g_app_info_get_executable(app);
        if (id == NULL || executable == NULL) continue;
        FiremodAppCandidate *candidate = g_new0(FiremodAppCandidate, 1);
        candidate->app = g_object_ref(app);
        candidate->aliases = g_hash_table_new_full(
            g_str_hash, g_str_equal, g_free, NULL);
        add_alias(candidate->aliases, executable);
        char *id_copy = g_strdup(id);
        if (g_str_has_suffix(id_copy, ".desktop")) {
            id_copy[strlen(id_copy) - strlen(".desktop")] = '\0';
        }
        char **parts = g_strsplit(id_copy, ".", -1);
        guint count = g_strv_length(parts);
        if (count > 0) add_alias(candidate->aliases, parts[count - 1]);
        add_alias(candidate->aliases, id_copy);
        g_strfreev(parts);
        g_free(id_copy);
        g_ptr_array_add(candidates, candidate);
    }
    g_list_free_full(apps, g_object_unref);
    return candidates;
}

FiremodAppCandidate *firemod_app_index_match(
    GPtrArray *candidates,
    const char *base)
{
    char *normalized = firemod_normalize_token(base);
    FiremodAppCandidate *match = NULL;
    for (guint i = 0; i < candidates->len; i++) {
        FiremodAppCandidate *candidate = g_ptr_array_index(candidates, i);
        if (!g_hash_table_contains(candidate->aliases, normalized)) continue;
        if (match != NULL) {
            match = NULL;
            break;
        }
        match = candidate;
    }
    g_free(normalized);
    return match;
}

GAppInfo *firemod_app_candidate_info(FiremodAppCandidate *candidate)
{
    return candidate != NULL ? candidate->app : NULL;
}
