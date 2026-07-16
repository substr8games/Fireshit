#ifndef WCH_HUD_TERMINAL_H
#define WCH_HUD_TERMINAL_H

#include <gtk/gtk.h>

typedef struct _HudApp HudApp;

GtkWidget *hud_terminal_create_view(HudApp *app);
void hud_terminal_spawn_all(HudApp *app);
void hud_terminal_stop_all(HudApp *app);
void hud_terminal_set_input(HudApp *app, gboolean enabled);
void hud_terminal_focus_default(HudApp *app);
void hud_terminal_free_all(HudApp *app);

#endif
