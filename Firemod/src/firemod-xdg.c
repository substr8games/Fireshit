#include "firemod-xdg.h"

#include <gio/gio.h>

#include "firemod-parse.h"
#include "firemod-paths.h"

#define FIREMOD_MAX_AUTHORITIES_PER_APP 12

#include "firemod-app-index.h"

static void add_config_files(FiremodModule *module, const char *directory)
{
    GQueue queue = G_QUEUE_INIT;
    g_queue_push_tail(&queue, g_strdup(directory));
    guint added = 0;
    guint visited = 0;

    while (!g_queue_is_empty(&queue) &&
           added < FIREMOD_MAX_AUTHORITIES_PER_APP && visited < 40) {
        char *current = g_queue_pop_head(&queue);
        GDir *dir = g_dir_open(current, 0, NULL);
        if (dir == NULL) {
            g_free(current);
            continue;
        }
        const char *name = NULL;
        while ((name = g_dir_read_name(dir)) != NULL &&
               added < FIREMOD_MAX_AUTHORITIES_PER_APP) {
            char *path = g_build_filename(current, name, NULL);
            visited++;
            if (firemod_path_is_ignored(path)) {
                g_free(path);
                continue;
            }
            if (g_file_test(path, G_FILE_TEST_IS_DIR) &&
                g_strcmp0(current, directory) == 0) {
                g_queue_push_tail(&queue, path);
                continue;
            }
            if (!firemod_path_is_regular_config(path)) {
                g_free(path);
                continue;
            }
            FiremodAuthorityType type = firemod_parse_type_for_path(path);
            FiremodAuthority *authority = firemod_authority_new(
                name, path, type, FIREMOD_CAPABILITY_LOCATED,
                FIREMOD_CONFIDENCE_RECOGNIZED);
            firemod_parse_authority(authority);
            firemod_module_add_authority(module, authority);
            added++;
            g_free(path);
        }
        g_dir_close(dir);
        g_free(current);
    }
    g_queue_clear_full(&queue, g_free);

    if (module->authorities->len == 0) {
        FiremodAuthority *authority = firemod_authority_new(
            "Configuration directory", directory, FIREMOD_AUTHORITY_DIRECTORY,
            FIREMOD_CAPABILITY_LOCATED, FIREMOD_CONFIDENCE_RECOGNIZED);
        firemod_module_add_authority(module, authority);
    }
}

static FiremodModule *module_for_app(FiremodAppCandidate *candidate, const char *path)
{
    const char *id = g_app_info_get_id(firemod_app_candidate_info(candidate));
    const char *name = g_app_info_get_display_name(firemod_app_candidate_info(candidate));
    const char *description = g_app_info_get_description(firemod_app_candidate_info(candidate));
    const char *icon_name = "application-x-executable-symbolic";
    GIcon *icon = g_app_info_get_icon(firemod_app_candidate_info(candidate));
    if (G_IS_THEMED_ICON(icon)) {
        const char * const *names = g_themed_icon_get_names(G_THEMED_ICON(icon));
        if (names != NULL && names[0] != NULL) icon_name = names[0];
    }
    FiremodModule *module = firemod_module_new(
        id, name, description != NULL ? description : "Recognized desktop application",
        icon_name);
    module->confidence = FIREMOD_CONFIDENCE_RECOGNIZED;
    module->available = TRUE;
    module->executable = g_strdup(g_app_info_get_executable(firemod_app_candidate_info(candidate)));
    module->availability_note = g_strdup(path);
    add_config_files(module, path);
    return module;
}

static FiremodModule *uncertain_module(const char *base, const char *path)
{
    FiremodModule *module = firemod_module_new(
        path, base, "Ownership could not be attributed cleanly.",
        "dialog-question-symbolic");
    module->confidence = FIREMOD_CONFIDENCE_UNKNOWN;
    module->uncertain = TRUE;
    module->available = TRUE;
    module->availability_note = g_strdup(path);
    FiremodAuthority *authority = firemod_authority_new(
        "Unattributed configuration", path, FIREMOD_AUTHORITY_DIRECTORY,
        FIREMOD_CAPABILITY_LOCATED, FIREMOD_CONFIDENCE_UNKNOWN);
    authority->uncertain = TRUE;
    firemod_module_add_authority(module, authority);
    return module;
}

void firemod_xdg_discover(FiremodInventory *inventory)
{
    GPtrArray *candidates = firemod_app_index_load();
    const char *config = g_get_user_config_dir();
    GDir *dir = g_dir_open(config, 0, NULL);
    if (dir == NULL) {
        g_ptr_array_free(candidates, TRUE);
        return;
    }

    const char *base = NULL;
    while ((base = g_dir_read_name(dir)) != NULL) {
        char *path = g_build_filename(config, base, NULL);
        if (firemod_path_is_ignored(path) ||
            !g_file_test(path, G_FILE_TEST_IS_DIR)) {
            g_free(path);
            continue;
        }
        FiremodAppCandidate *candidate = firemod_app_index_match(candidates, base);
        if (candidate != NULL) {
            const char *id = g_app_info_get_id(firemod_app_candidate_info(candidate));
            if (g_strcmp0(id, "org.philabs.Firemod.desktop") != 0 &&
                g_strcmp0(g_app_info_get_executable(firemod_app_candidate_info(candidate)), "frametap") != 0 &&
                g_strcmp0(g_app_info_get_executable(firemod_app_candidate_info(candidate)), "wayfire-hud") != 0 &&
                firemod_inventory_find(inventory, id) == NULL) {
                firemod_inventory_add(inventory, module_for_app(candidate, path));
            }
        } else {
            firemod_inventory_add_uncertain(
                inventory, uncertain_module(base, path));
        }
        g_free(path);
    }
    g_dir_close(dir);
    g_ptr_array_free(candidates, TRUE);
}
