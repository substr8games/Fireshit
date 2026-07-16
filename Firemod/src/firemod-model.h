#ifndef FIREMOD_MODEL_H
#define FIREMOD_MODEL_H

#include <glib.h>

typedef enum {
    FIREMOD_CAPABILITY_LOCATED = 0,
    FIREMOD_CAPABILITY_READABLE,
    FIREMOD_CAPABILITY_EDITABLE
} FiremodSettingCapability;

typedef enum {
    FIREMOD_CONFIDENCE_UNKNOWN = 0,
    FIREMOD_CONFIDENCE_PROBABLE,
    FIREMOD_CONFIDENCE_RECOGNIZED,
    FIREMOD_CONFIDENCE_VERIFIED,
    FIREMOD_CONFIDENCE_REGISTERED
} FiremodConfidence;

typedef enum {
    FIREMOD_AUTHORITY_DIRECTORY = 0,
    FIREMOD_AUTHORITY_INI,
    FIREMOD_AUTHORITY_TOML,
    FIREMOD_AUTHORITY_JSON,
    FIREMOD_AUTHORITY_YAML,
    FIREMOD_AUTHORITY_XML,
    FIREMOD_AUTHORITY_PROPERTIES,
    FIREMOD_AUTHORITY_SCRIPT,
    FIREMOD_AUTHORITY_GSETTINGS,
    FIREMOD_AUTHORITY_SYSTEMD,
    FIREMOD_AUTHORITY_SQLITE,
    FIREMOD_AUTHORITY_COMMAND,
    FIREMOD_AUTHORITY_UNKNOWN
} FiremodAuthorityType;

typedef struct {
    gchar *name;
    gchar *value;
    gchar *key_path;
    FiremodSettingCapability capability;
} FiremodSetting;

typedef struct {
    gchar *name;
    gchar *path;
    gchar *detail;
    FiremodAuthorityType type;
    FiremodSettingCapability capability;
    FiremodConfidence confidence;
    gboolean uncertain;
    GPtrArray *settings;
} FiremodAuthority;

typedef struct {
    gchar *id;
    gchar *name;
    gchar *summary;
    gchar *icon_name;
    gchar *executable;
    gchar *settings_command;
    gchar *availability_note;
    gboolean first_party;
    gboolean available;
    gboolean uncertain;
    FiremodConfidence confidence;
    GPtrArray *authorities;
} FiremodModule;

typedef struct {
    GPtrArray *modules;
    GPtrArray *uncertain_modules;
} FiremodInventory;

FiremodSetting *firemod_setting_new(
    const gchar *name,
    const gchar *value,
    const gchar *key_path,
    FiremodSettingCapability capability);
void firemod_setting_free(FiremodSetting *setting);

FiremodAuthority *firemod_authority_new(
    const gchar *name,
    const gchar *path,
    FiremodAuthorityType type,
    FiremodSettingCapability capability,
    FiremodConfidence confidence);
void firemod_authority_add_setting(
    FiremodAuthority *authority,
    FiremodSetting *setting);
void firemod_authority_free(FiremodAuthority *authority);

FiremodModule *firemod_module_new(
    const gchar *id,
    const gchar *name,
    const gchar *summary,
    const gchar *icon_name);
void firemod_module_add_authority(
    FiremodModule *module,
    FiremodAuthority *authority);
void firemod_module_free(FiremodModule *module);

FiremodInventory *firemod_inventory_new(void);
void firemod_inventory_add(
    FiremodInventory *inventory,
    FiremodModule *module);
void firemod_inventory_add_uncertain(
    FiremodInventory *inventory,
    FiremodModule *module);
FiremodModule *firemod_inventory_find(
    FiremodInventory *inventory,
    const gchar *id);
void firemod_inventory_free(FiremodInventory *inventory);

const gchar *firemod_capability_name(FiremodSettingCapability capability);
const gchar *firemod_confidence_name(FiremodConfidence confidence);
const gchar *firemod_authority_type_name(FiremodAuthorityType type);

#endif
