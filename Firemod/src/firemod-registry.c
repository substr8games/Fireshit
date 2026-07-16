#include "firemod-registry.h"

#include <gio/gio.h>

#include "firemod-build-config.h"

static gboolean module_id_exists(FiremodInventory *inventory, const char *id)
{
    return firemod_inventory_find(inventory, id) != NULL;
}

static void resolve_availability(FiremodModule *module, gboolean always)
{
    if (always) {
        module->available = TRUE;
        module->availability_note = g_strdup("Core module");
        return;
    }

    char *path = module->executable != NULL ?
        g_find_program_in_path(module->executable) : NULL;
    module->available = path != NULL;
    module->availability_note = path != NULL ?
        g_strdup(path) : g_strdup("Not currently installed");
    g_free(path);
}

static void add_builtin(
    FiremodInventory *inventory,
    const char *id,
    const char *name,
    const char *summary,
    const char *icon,
    const char *executable,
    gboolean always)
{
    FiremodModule *module = firemod_module_new(id, name, summary, icon);
    module->first_party = TRUE;
    module->confidence = FIREMOD_CONFIDENCE_REGISTERED;
    module->executable = g_strdup(executable);
    resolve_availability(module, always);
    firemod_inventory_add(inventory, module);
}

static void register_builtins(FiremodInventory *inventory)
{
    add_builtin(
        inventory,
        "org.fireshit.Firemod",
        "Firemod",
        "Fireshit Core: native Wayfire module shell and settings inventory.",
        "applications-system-symbolic",
        "firemod",
        TRUE);
    add_builtin(
        inventory,
        "org.fireshit.FrameTap",
        "FrameTap",
        "Targeted Wayfire application capture with application-only audio.",
        "media-record-symbolic",
        "frametap",
        FALSE);
    add_builtin(
        inventory,
        "org.fireshit.WayfireConsoleHud",
        "Console HUD",
        "Persistent modular telemetry and terminal surface for Wayfire.",
        "utilities-terminal-symbolic",
        "wayfire-hud",
        FALSE);
}

static FiremodModule *module_from_manifest(const char *path)
{
    GKeyFile *key = g_key_file_new();
    if (!g_key_file_load_from_file(key, path, G_KEY_FILE_NONE, NULL) ||
        !g_key_file_has_group(key, "module")) {
        g_key_file_unref(key);
        return NULL;
    }

    char *id = g_key_file_get_string(key, "module", "id", NULL);
    char *name = g_key_file_get_string(key, "module", "name", NULL);
    char *summary = g_key_file_get_string(key, "module", "summary", NULL);
    char *icon = g_key_file_get_string(key, "module", "icon", NULL);
    if (id == NULL || name == NULL || summary == NULL) {
        g_free(id);
        g_free(name);
        g_free(summary);
        g_free(icon);
        g_key_file_unref(key);
        return NULL;
    }

    FiremodModule *module = firemod_module_new(
        id, name, summary, icon != NULL ? icon : "application-x-executable-symbolic");
    module->executable = g_key_file_get_string(
        key, "module", "executable", NULL);
    module->settings_command = g_key_file_get_string(
        key, "module", "settings-command", NULL);
    module->confidence = FIREMOD_CONFIDENCE_REGISTERED;
    resolve_availability(module, FALSE);

    g_free(id);
    g_free(name);
    g_free(summary);
    g_free(icon);
    g_key_file_unref(key);
    return module;
}

static void load_directory(FiremodInventory *inventory, const char *directory)
{
    if (directory == NULL || !g_file_test(directory, G_FILE_TEST_IS_DIR)) {
        return;
    }

    GDir *dir = g_dir_open(directory, 0, NULL);
    if (dir == NULL) {
        return;
    }
    const char *name = NULL;
    while ((name = g_dir_read_name(dir)) != NULL) {
        if (!g_str_has_suffix(name, ".module")) {
            continue;
        }
        char *path = g_build_filename(directory, name, NULL);
        FiremodModule *module = module_from_manifest(path);
        if (module != NULL && !module_id_exists(inventory, module->id)) {
            firemod_inventory_add(inventory, module);
        } else {
            firemod_module_free(module);
        }
        g_free(path);
    }
    g_dir_close(dir);
}

void firemod_registry_register(FiremodInventory *inventory)
{
    g_return_if_fail(inventory != NULL);
    register_builtins(inventory);

    const char *override = g_getenv("FIREMOD_MODULE_DIR");
    load_directory(inventory, override);
    load_directory(inventory, FIREMOD_MODULE_DIR);

    char *user_dir = g_build_filename(
        g_get_user_data_dir(), "firemod", "modules.d", NULL);
    load_directory(inventory, user_dir);
    g_free(user_dir);

    const char * const *system_dirs = g_get_system_data_dirs();
    for (guint i = 0; system_dirs[i] != NULL; i++) {
        char *path = g_build_filename(system_dirs[i], "firemod", "modules.d", NULL);
        load_directory(inventory, path);
        g_free(path);
    }
}
