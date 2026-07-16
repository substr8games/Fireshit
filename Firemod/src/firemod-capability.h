#ifndef FIREMOD_CAPABILITY_H
#define FIREMOD_CAPABILITY_H

#include <glib.h>

typedef enum {
    FIREMOD_ACTION_AVAILABLE = 0,
    FIREMOD_ACTION_UNAVAILABLE,
    FIREMOD_ACTION_HIDDEN,
    FIREMOD_ACTION_REJECTED
} FiremodActionState;

typedef enum {
    FIREMOD_ORIGIN_CANONICAL = 0,
    FIREMOD_ORIGIN_FIRESHIT_MODULE,
    FIREMOD_ORIGIN_EXTERNAL_MODULE,
    FIREMOD_ORIGIN_USER_APPLICATION,
    FIREMOD_ORIGIN_USER_SCRIPT
} FiremodActionOrigin;

typedef struct {
    gchar *id;
    gchar *owner;
    gchar *label;
    gchar *short_label;
    gchar *summary;
    gchar *icon_name;
    gchar *icon_resource;
    gchar *icon_path;
    gchar *color;
    gchar *menu_group;
    gint menu_order;
    gint canonical_slot;
    gboolean menu_visible;
    FiremodActionState state;
    gchar *availability_reason;
    gchar *executable;
    gchar *command;
    gchar **argv;
    gchar *working_directory;
    gboolean run_in_terminal;
    gboolean external;
    FiremodActionOrigin origin;
} FiremodCapability;

typedef struct {
    GPtrArray *items;
} FiremodCapabilityRegistry;

FiremodCapabilityRegistry *firemod_capability_registry_new(void);
void firemod_capability_registry_free(FiremodCapabilityRegistry *registry);
FiremodCapability *firemod_capability_find(
    FiremodCapabilityRegistry *registry,
    const gchar *id);
void firemod_capability_set_state(
    FiremodCapability *capability,
    FiremodActionState state,
    const gchar *reason);
void firemod_capability_registry_remove_user_actions(
    FiremodCapabilityRegistry *registry);
void firemod_capability_registry_add(
    FiremodCapabilityRegistry *registry,
    FiremodCapability *capability);
FiremodCapability *firemod_capability_new_user(
    const gchar *id,
    const gchar *label,
    const gchar *short_label,
    const gchar *summary,
    const gchar *icon_path,
    const gchar *color,
    gchar **argv,
    const gchar *working_directory,
    gboolean run_in_terminal,
    gint order,
    FiremodActionOrigin origin);
const gchar *firemod_action_state_name(FiremodActionState state);

#endif
