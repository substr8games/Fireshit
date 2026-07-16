#ifndef WCH_HUD_SURFACE_H
#define WCH_HUD_SURFACE_H

#include <gtk/gtk.h>

#include "hud-state.h"

typedef struct _HudApp HudApp;

GtkWindow *hud_surface_create(HudApp *app);
void hud_surface_set_layer(HudApp *app, HudState state);
void hud_surface_set_pointer_input(HudApp *app, gboolean enabled);
void hud_surface_show(HudApp *app);
void hud_surface_hide(HudApp *app);
void hud_surface_refresh_appearance(HudApp *app);
void hud_surface_refresh_settings(HudApp *app);

#endif
