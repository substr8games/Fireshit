#include "firemod-model.h"

FiremodSetting *firemod_setting_new(
    const gchar *name,
    const gchar *value,
    const gchar *key_path,
    FiremodSettingCapability capability)
{
    FiremodSetting *setting = g_new0(FiremodSetting, 1);
    setting->name = g_strdup(name);
    setting->value = g_strdup(value);
    setting->key_path = g_strdup(key_path);
    setting->capability = capability;
    return setting;
}

void firemod_setting_free(FiremodSetting *setting)
{
    if (setting == NULL) {
        return;
    }
    g_free(setting->name);
    g_free(setting->value);
    g_free(setting->key_path);
    g_free(setting);
}

FiremodAuthority *firemod_authority_new(
    const gchar *name,
    const gchar *path,
    FiremodAuthorityType type,
    FiremodSettingCapability capability,
    FiremodConfidence confidence)
{
    FiremodAuthority *authority = g_new0(FiremodAuthority, 1);
    authority->name = g_strdup(name);
    authority->path = g_strdup(path);
    authority->type = type;
    authority->capability = capability;
    authority->confidence = confidence;
    authority->settings = g_ptr_array_new_with_free_func(
        (GDestroyNotify)firemod_setting_free);
    return authority;
}

void firemod_authority_add_setting(
    FiremodAuthority *authority,
    FiremodSetting *setting)
{
    g_return_if_fail(authority != NULL);
    g_return_if_fail(setting != NULL);
    g_ptr_array_add(authority->settings, setting);
}

void firemod_authority_free(FiremodAuthority *authority)
{
    if (authority == NULL) {
        return;
    }
    g_free(authority->name);
    g_free(authority->path);
    g_free(authority->detail);
    g_ptr_array_free(authority->settings, TRUE);
    g_free(authority);
}

FiremodModule *firemod_module_new(
    const gchar *id,
    const gchar *name,
    const gchar *summary,
    const gchar *icon_name)
{
    FiremodModule *module = g_new0(FiremodModule, 1);
    module->id = g_strdup(id);
    module->name = g_strdup(name);
    module->summary = g_strdup(summary);
    module->icon_name = g_strdup(icon_name);
    module->confidence = FIREMOD_CONFIDENCE_RECOGNIZED;
    module->authorities = g_ptr_array_new_with_free_func(
        (GDestroyNotify)firemod_authority_free);
    return module;
}

void firemod_module_add_authority(
    FiremodModule *module,
    FiremodAuthority *authority)
{
    g_return_if_fail(module != NULL);
    g_return_if_fail(authority != NULL);
    g_ptr_array_add(module->authorities, authority);
}

void firemod_module_free(FiremodModule *module)
{
    if (module == NULL) {
        return;
    }
    g_free(module->id);
    g_free(module->name);
    g_free(module->summary);
    g_free(module->icon_name);
    g_free(module->executable);
    g_free(module->settings_command);
    g_free(module->availability_note);
    g_ptr_array_free(module->authorities, TRUE);
    g_free(module);
}

FiremodInventory *firemod_inventory_new(void)
{
    FiremodInventory *inventory = g_new0(FiremodInventory, 1);
    inventory->modules = g_ptr_array_new_with_free_func(
        (GDestroyNotify)firemod_module_free);
    inventory->uncertain_modules = g_ptr_array_new_with_free_func(
        (GDestroyNotify)firemod_module_free);
    return inventory;
}

void firemod_inventory_add(
    FiremodInventory *inventory,
    FiremodModule *module)
{
    g_return_if_fail(inventory != NULL);
    g_return_if_fail(module != NULL);
    g_ptr_array_add(inventory->modules, module);
}

void firemod_inventory_add_uncertain(
    FiremodInventory *inventory,
    FiremodModule *module)
{
    g_return_if_fail(inventory != NULL);
    g_return_if_fail(module != NULL);
    module->uncertain = TRUE;
    g_ptr_array_add(inventory->uncertain_modules, module);
}

FiremodModule *firemod_inventory_find(
    FiremodInventory *inventory,
    const gchar *id)
{
    g_return_val_if_fail(inventory != NULL, NULL);
    for (guint i = 0; i < inventory->modules->len; i++) {
        FiremodModule *module = g_ptr_array_index(inventory->modules, i);
        if (g_strcmp0(module->id, id) == 0) {
            return module;
        }
    }
    return NULL;
}

void firemod_inventory_free(FiremodInventory *inventory)
{
    if (inventory == NULL) {
        return;
    }
    g_ptr_array_free(inventory->modules, TRUE);
    g_ptr_array_free(inventory->uncertain_modules, TRUE);
    g_free(inventory);
}

const gchar *firemod_capability_name(FiremodSettingCapability capability)
{
    switch (capability) {
    case FIREMOD_CAPABILITY_EDITABLE:
        return "Editable";
    case FIREMOD_CAPABILITY_READABLE:
        return "Readable";
    default:
        return "Located";
    }
}

const gchar *firemod_confidence_name(FiremodConfidence confidence)
{
    switch (confidence) {
    case FIREMOD_CONFIDENCE_REGISTERED:
        return "Registered";
    case FIREMOD_CONFIDENCE_VERIFIED:
        return "Verified";
    case FIREMOD_CONFIDENCE_RECOGNIZED:
        return "Recognized";
    case FIREMOD_CONFIDENCE_PROBABLE:
        return "Probable";
    default:
        return "Unknown";
    }
}

const gchar *firemod_authority_type_name(FiremodAuthorityType type)
{
    static const gchar *names[] = {
        "Directory", "INI / KeyFile", "TOML", "JSON family", "YAML",
        "XML", "Properties", "Executable config", "GSettings", "systemd",
        "SQLite", "Command interface", "Unknown"
    };
    if ((guint)type >= G_N_ELEMENTS(names)) {
        return "Unknown";
    }
    return names[type];
}
