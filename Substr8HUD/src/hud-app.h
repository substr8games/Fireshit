#ifndef WCH_HUD_APP_H
#define WCH_HUD_APP_H

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <vte/vte.h>

#include "hud-module.h"
#include "hud-settings.h"
#include "hud-state.h"

typedef struct _HudSlot HudSlot;

struct _HudApp {
    GtkApplication *application;
    GtkWindow *window;
    GPtrArray *modules;
    HudSlot *slots[HUD_SLOT_COUNT];
    GtkGrid *grid;
    HudModuleSlot selected_slot;
    HudModuleSlot last_focused_slot;
    gboolean has_last_focused_slot;
    gboolean pane_expanded;
    gboolean module_input_enabled;
    GtkCssProvider *css_provider;
    GListModel *monitor_model;
    GdkMonitor *monitor;
    gulong monitor_model_handler;
    gulong monitor_notify_handler;
    GtkWidget *opacity_indicator;
    guint opacity_hide_source;
    guint opacity_save_source;
    GSocketService *control_service;
    gchar *socket_path;
    gchar *diagnostic_path;
    gchar *config_path;
    gchar *module_directory;
    gchar *user_config_path;
    GKeyFile *user_settings;
    gboolean user_settings_valid;
    HudPlacement placement;
    gint width_percent;
    gdouble opacity;
    HudState state;
    gboolean activated;
    gboolean owns_socket;
};

HudApp *hud_app_new(GtkApplication *application);
void hud_app_activate(GtkApplication *application, gpointer user_data);
void hud_app_shutdown(GApplication *application, gpointer user_data);
void hud_app_free(HudApp *app);
void hud_app_log(HudApp *app, const char *format, ...) G_GNUC_PRINTF(2, 3);

#endif
