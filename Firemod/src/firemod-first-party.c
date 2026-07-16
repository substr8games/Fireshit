#include "firemod-first-party.h"

#include <gio/gio.h>

#include "firemod-parse.h"
#include "firemod-paths.h"

static void add_core_authority(FiremodInventory *inventory)
{
    FiremodModule *module = firemod_inventory_find(
        inventory, "org.fireshit.Firemod");
    if (module == NULL) {
        return;
    }

    FiremodAuthority *authority = firemod_authority_new(
        "Inventory policy",
        NULL,
        FIREMOD_AUTHORITY_COMMAND,
        FIREMOD_CAPABILITY_READABLE,
        FIREMOD_CONFIDENCE_REGISTERED);
    authority->detail = g_strdup(
        "Core-owned runtime policy. Changes are never autosaved.");
    firemod_authority_add_setting(authority, firemod_setting_new(
        "Default inventory", "Curated and cleanly attributed", "core.inventory",
        FIREMOD_CAPABILITY_READABLE));
    firemod_authority_add_setting(authority, firemod_setting_new(
        "Uncertain results", "Hidden until explicitly enabled", "core.diagnostics",
        FIREMOD_CAPABILITY_READABLE));
    firemod_authority_add_setting(authority, firemod_setting_new(
        "Mutation policy", "Read-only until adapter review", "core.mutation",
        FIREMOD_CAPABILITY_READABLE));
    firemod_module_add_authority(module, authority);
}

static void add_wayfire_plugin_status(
    FiremodAuthority *authority,
    GKeyFile *key,
    const char *plugin)
{
    char *plugins = g_key_file_get_string(key, "core", "plugins", NULL);
    gboolean enabled = FALSE;
    if (plugins != NULL) {
        char **parts = g_strsplit_set(plugins, " ,;\t\n", -1);
        for (guint i = 0; parts[i] != NULL; i++) {
            if (g_strcmp0(parts[i], plugin) == 0) {
                enabled = TRUE;
                break;
            }
        }
        g_strfreev(parts);
    }
    firemod_authority_add_setting(authority, firemod_setting_new(
        "Wayfire plugin", enabled ? "Enabled" : "Not listed", "[core] plugins",
        FIREMOD_CAPABILITY_READABLE));
    g_free(plugins);
}

static void add_firemod_wayfire_authority(
    FiremodInventory *inventory,
    const char *path)
{
    FiremodModule *module = firemod_inventory_find(
        inventory, "org.fireshit.Firemod");
    if (module == NULL || path == NULL) return;
    GKeyFile *key = g_key_file_new();
    if (g_key_file_load_from_file(key, path, G_KEY_FILE_NONE, NULL)) {
        FiremodAuthority *authority = firemod_authority_new(
            "Wayfire global-menu bridge", path, FIREMOD_AUTHORITY_INI,
            FIREMOD_CAPABILITY_READABLE, FIREMOD_CONFIDENCE_VERIFIED);
        authority->detail = g_strdup(
            "The plugin consumes SUPER+middle-click and forwards pointer coordinates only.");
        add_wayfire_plugin_status(authority, key, "firemod-wayfire");
        firemod_authority_add_setting(authority, firemod_setting_new(
            "Default binding", "SUPER + BTN_MIDDLE",
            "[firemod-wayfire] menu", FIREMOD_CAPABILITY_READABLE));
        firemod_module_add_authority(module, authority);
    }
    g_key_file_unref(key);
}

static void add_frametap_authority(FiremodInventory *inventory, const char *path)
{
    FiremodModule *module = firemod_inventory_find(
        inventory, "org.fireshit.FrameTap");
    if (module == NULL) {
        return;
    }

    FiremodAuthority *command = firemod_authority_new(
        "Capture command interface",
        module->availability_note,
        FIREMOD_AUTHORITY_COMMAND,
        module->available ? FIREMOD_CAPABILITY_READABLE : FIREMOD_CAPABILITY_LOCATED,
        FIREMOD_CONFIDENCE_REGISTERED);
    command->detail = g_strdup(
        "FrameTap remains the runtime authority; Firemod only surfaces its contract.");
    firemod_authority_add_setting(command, firemod_setting_new(
        "Viewport default", "off", "--viewport", FIREMOD_CAPABILITY_READABLE));
    firemod_authority_add_setting(command, firemod_setting_new(
        "Launch audio default", "Application-only", "--audio", FIREMOD_CAPABILITY_READABLE));
    firemod_authority_add_setting(command, firemod_setting_new(
        "Canonical launch mode", "frametap [options] -- <application>", "argv",
        FIREMOD_CAPABILITY_READABLE));
    firemod_module_add_authority(module, command);

    if (path == NULL) {
        return;
    }
    GKeyFile *key = g_key_file_new();
    if (g_key_file_load_from_file(key, path, G_KEY_FILE_NONE, NULL)) {
        FiremodAuthority *wayfire = firemod_authority_new(
            "Wayfire plugin registration", path, FIREMOD_AUTHORITY_INI,
            FIREMOD_CAPABILITY_READABLE, FIREMOD_CONFIDENCE_VERIFIED);
        add_wayfire_plugin_status(wayfire, key, "frametap");
        firemod_module_add_authority(module, wayfire);
    }
    g_key_file_unref(key);
}

static void add_hud_authority(FiremodInventory *inventory, const char *path)
{
    FiremodModule *module = firemod_inventory_find(
        inventory, "org.fireshit.WayfireConsoleHud");
    if (module == NULL || path == NULL) {
        return;
    }

    GKeyFile *key = g_key_file_new();
    if (!g_key_file_load_from_file(key, path, G_KEY_FILE_KEEP_COMMENTS, NULL) ||
        !g_key_file_has_group(key, "wayfire-hud")) {
        g_key_file_unref(key);
        return;
    }

    FiremodAuthority *authority = firemod_authority_new(
        "Wayfire HUD configuration", path, FIREMOD_AUTHORITY_INI,
        FIREMOD_CAPABILITY_READABLE, FIREMOD_CONFIDENCE_VERIFIED);
    const char *keys[] = {
        "enabled", "autostart", "toggle", "width_percent", "opacity",
        "module_directory", "initial_state"
    };
    for (guint i = 0; i < G_N_ELEMENTS(keys); i++) {
        if (!g_key_file_has_key(key, "wayfire-hud", keys[i], NULL)) {
            continue;
        }
        char *value = g_key_file_get_value(key, "wayfire-hud", keys[i], NULL);
        char *key_path = g_strdup_printf("[wayfire-hud] %s", keys[i]);
        firemod_authority_add_setting(authority, firemod_setting_new(
            keys[i], value, key_path, FIREMOD_CAPABILITY_READABLE));
        g_free(key_path);
        g_free(value);
    }
    firemod_module_add_authority(module, authority);
    g_key_file_unref(key);
}


static void add_wayfire_authority(FiremodInventory *inventory, const char *path)
{
    if (path == NULL) {
        return;
    }
    FiremodModule *module = firemod_module_new(
        "org.wayfire.Wayfire", "Wayfire",
        "The compositor authority hosting Fireshit.",
        "preferences-desktop-display-symbolic");
    module->confidence = FIREMOD_CONFIDENCE_VERIFIED;
    char *executable = g_find_program_in_path("wayfire");
    module->available = executable != NULL;
    g_free(executable);
    module->availability_note = g_strdup(path);
    FiremodAuthority *authority = firemod_authority_new(
        "Wayfire configuration", path, FIREMOD_AUTHORITY_INI,
        FIREMOD_CAPABILITY_LOCATED, FIREMOD_CONFIDENCE_VERIFIED);
    firemod_parse_authority(authority);
    firemod_module_add_authority(module, authority);
    firemod_inventory_add(inventory, module);
}


void firemod_first_party_attach_capabilities(
    FiremodInventory *inventory,
    FiremodCapabilityRegistry *registry)
{
    FiremodModule *module = firemod_inventory_find(
        inventory, "org.fireshit.Firemod");
    if (module == NULL || registry == NULL) return;

    FiremodAuthority *authority = firemod_authority_new(
        "Global utility capability registry",
        "$XDG_RUNTIME_DIR/firemod.sock",
        FIREMOD_AUTHORITY_COMMAND,
        FIREMOD_CAPABILITY_READABLE,
        FIREMOD_CONFIDENCE_REGISTERED);
    authority->detail = g_strdup(
        "SUPER+middle-click projects this registry through the narrow Wayfire bridge.");
    for (guint i = 0; i < registry->items->len; i++) {
        FiremodCapability *capability = g_ptr_array_index(registry->items, i);
        if (!capability->menu_visible) continue;
        char *value = g_strdup_printf("%s — %s",
            firemod_action_state_name(capability->state),
            capability->availability_reason != NULL ?
                capability->availability_reason : "No status supplied");
        firemod_authority_add_setting(authority, firemod_setting_new(
            capability->label, value, capability->id,
            FIREMOD_CAPABILITY_READABLE));
        g_free(value);
    }
    firemod_module_add_authority(module, authority);
}

void firemod_first_party_discover(FiremodInventory *inventory)
{
    add_core_authority(inventory);
    char *wayfire = firemod_find_wayfire_config();
    add_wayfire_authority(inventory, wayfire);
    add_firemod_wayfire_authority(inventory, wayfire);
    add_frametap_authority(inventory, wayfire);
    add_hud_authority(inventory, wayfire);
    g_free(wayfire);
}
