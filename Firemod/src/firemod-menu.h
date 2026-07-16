#ifndef FIREMOD_MENU_H
#define FIREMOD_MENU_H

#include <gtk/gtk.h>

#include "firemod-capability.h"
#include "firemod-settings.h"

typedef struct FiremodMenu FiremodMenu;
typedef void (*FiremodActionRequest)(
    const gchar *capability_id,
    double global_x,
    double global_y,
    gpointer user_data);

FiremodMenu *firemod_menu_new(
    GtkApplication *application,
    FiremodCapabilityRegistry *registry,
    FiremodSettings *settings,
    FiremodActionRequest action_request,
    gpointer user_data);
void firemod_menu_show_at(FiremodMenu *menu, double global_x, double global_y);
void firemod_menu_close(FiremodMenu *menu);
void firemod_menu_free(FiremodMenu *menu);

#endif
