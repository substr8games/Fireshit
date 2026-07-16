#ifndef FIREMOD_SETTINGS_H
#define FIREMOD_SETTINGS_H

#include <gtk/gtk.h>

#include "firemod-capability.h"

#define FIREMOD_CANONICAL_COUNT 7

typedef enum {
    FIREMOD_CUSTOM_APPLICATION = 0,
    FIREMOD_CUSTOM_SCRIPT
} FiremodCustomType;

typedef struct {
    gchar *id;
    gchar *label;
    gchar *short_label;
    gchar *hover_title;
    gchar *hover_subtitle;
    FiremodCustomType type;
    gchar *target;
    gchar **arguments;
    gchar *working_directory;
    gchar *icon_path;
    gchar *color;
    gboolean run_in_terminal;
    gboolean enabled;
} FiremodCustomAction;

typedef struct {
    gboolean menu_enabled;
    gdouble menu_scale;
    gboolean hover_enabled;
    guint hover_delay_ms;
    gboolean canonical_visible[FIREMOD_CANONICAL_COUNT];
    GPtrArray *actions;
} FiremodSettingsState;

typedef struct FiremodSettings {
    FiremodSettingsState *saved;
    FiremodSettingsState *draft;
    FiremodSettingsState *active;
    gchar *config_path;
    gchar *actions_dir;
    gchar *history_dir;
    gboolean dirty;
} FiremodSettings;

FiremodSettings *firemod_settings_new(GError **error);
void firemod_settings_free(FiremodSettings *settings);
FiremodCustomAction *firemod_custom_action_new(FiremodCustomType type);
FiremodCustomAction *firemod_custom_action_clone(const FiremodCustomAction *source);
void firemod_custom_action_free(FiremodCustomAction *action);
void firemod_settings_mark_dirty(FiremodSettings *settings);
void firemod_settings_discard(FiremodSettings *settings);
gboolean firemod_settings_save(FiremodSettings *settings, GError **error);
gboolean firemod_settings_apply(
    FiremodSettings *settings,
    FiremodCapabilityRegistry *registry,
    GError **error);
gboolean firemod_settings_validate(
    const FiremodSettingsState *state,
    gchar **detail);
const gchar *firemod_settings_canonical_id(guint slot);
const gchar *firemod_settings_canonical_label(guint slot);

#endif
