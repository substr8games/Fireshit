#ifndef FIREMOD_APP_H
#define FIREMOD_APP_H

#include <gtk/gtk.h>

#include "firemod-capability.h"
#include "firemod-model.h"

typedef struct FiremodRuntime FiremodRuntime;
typedef struct FiremodMenu FiremodMenu;
typedef struct FiremodScreencopy FiremodScreencopy;
typedef struct FiremodColorPicker FiremodColorPicker;
typedef struct FiremodSettings FiremodSettings;
typedef struct FiremodFireshitPage FiremodFireshitPage;

typedef enum {
    FIREMOD_FILTER_ALL = 0,
    FIREMOD_FILTER_READABLE,
    FIREMOD_FILTER_LOCATED,
    FIREMOD_FILTER_FIRESHIT
} FiremodFilter;

typedef struct FiremodApp {
    GtkApplication *application;
    GtkWindow *window;
    GtkStack *page_stack;
    GtkSearchEntry *search;
    GtkDropDown *filter;
    GtkLabel *topbar_title;
    GtkWidget *browser_tools;
    GtkSwitch *uncertain_switch;
    GtkWidget *diagnostic_nav;
    GtkWidget *overview_page;
    GtkWidget *fireshit_page;
    GtkWidget *inventory_page;
    GtkWidget *modules_page;
    GtkWidget *diagnostics_page;
    GtkLabel *status_label;
    FiremodInventory *inventory;
    FiremodCapabilityRegistry *capabilities;
    FiremodRuntime *runtime;
    FiremodMenu *menu;
    FiremodScreencopy *screencopy;
    FiremodColorPicker *color_picker;
    FiremodSettings *settings;
    FiremodFireshitPage *fireshit_settings_page;
    FiremodFilter active_filter;
    gboolean show_uncertain;
} FiremodApp;

FiremodApp *firemod_app_new(GtkApplication *application);
void firemod_app_startup(GApplication *application, gpointer user_data);
void firemod_app_activate(GtkApplication *application, gpointer user_data);
gint firemod_app_command_line(
    GApplication *application,
    GApplicationCommandLine *command_line,
    gpointer user_data);
void firemod_app_shutdown(GApplication *application, gpointer user_data);
void firemod_app_free(FiremodApp *app);
void firemod_app_show_settings(FiremodApp *app);
void firemod_app_show_fireshit_settings(FiremodApp *app);
void firemod_app_rescan(FiremodApp *app);
void firemod_app_render(FiremodApp *app);

#endif
