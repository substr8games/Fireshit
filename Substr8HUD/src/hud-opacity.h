#ifndef WCH_HUD_OPACITY_H
#define WCH_HUD_OPACITY_H

#include <gtk/gtk.h>

typedef struct _HudApp HudApp;

GtkWidget *hud_opacity_wrap_view(
    HudApp *app,
    GtkWidget *child);
void hud_opacity_attach(
    HudApp *app,
    GtkWidget *widget);
void hud_opacity_shutdown(HudApp *app);

#endif
