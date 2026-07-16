#ifndef WCH_HUD_MODULE_H
#define WCH_HUD_MODULE_H

#include <gtk/gtk.h>
#include <vte/vte.h>

typedef struct _HudApp HudApp;

typedef enum {
    HUD_MODULE_READY = 0,
    HUD_MODULE_RUNNING,
    HUD_MODULE_STOPPED,
    HUD_MODULE_UNAVAILABLE,
    HUD_MODULE_FAILED,
} HudModuleStatus;

typedef enum {
    HUD_SLOT_TOP_LEFT = 0,
    HUD_SLOT_TOP_CENTER,
    HUD_SLOT_TOP_RIGHT,
    HUD_SLOT_BOTTOM_LEFT_WIDE,
    HUD_SLOT_BOTTOM_RIGHT_WIDE,
    HUD_SLOT_COUNT,
} HudModuleSlot;

typedef struct _HudModule {
    HudApp *app;
    gchar *source_path;
    gchar *id;
    gchar *title;
    gchar *command;
    gchar *working_directory;
    gchar *lifecycle;
    gchar *restart_policy;
    gchar **requires;
    HudModuleSlot default_slot;
    HudModuleSlot slot;
    gint default_bank_order;
    gint bank_order;
    gboolean interactive;
    gint scrollback_lines;
    HudModuleStatus status;
    GtkWidget *pane;
    GtkWidget *status_label;
    VteTerminal *terminal;
    GPid pid;
    guint restart_source;
    guint kill_source;
    gboolean stopping;
} HudModule;

HudModule *hud_module_load(
    HudApp *app,
    const char *path,
    GError **error);
void hud_module_free(HudModule *module);
GtkWidget *hud_module_create_pane(HudModule *module);
void hud_module_spawn(HudModule *module);
void hud_module_stop(HudModule *module);
void hud_module_force_stop(HudModule *module);
gboolean hud_module_is_running(HudModule *module);
void hud_module_set_input(HudModule *module, gboolean enabled);
gboolean hud_module_focus(HudModule *module);
const char *hud_module_status_name(HudModuleStatus status);
const char *hud_module_slot_name(HudModuleSlot slot);
gboolean hud_module_slot_parse(const char *text, HudModuleSlot *slot);

#endif
