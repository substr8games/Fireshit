#ifndef WCH_HUD_NAVIGATION_H
#define WCH_HUD_NAVIGATION_H

#include <gtk/gtk.h>

typedef struct _HudApp HudApp;

void hud_navigation_attach(HudApp *app, GtkWindow *window);
void hud_navigation_enter(HudApp *app);
void hud_navigation_leave(HudApp *app);

#endif
