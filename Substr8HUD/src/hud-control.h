#ifndef WCH_HUD_CONTROL_H
#define WCH_HUD_CONTROL_H

#include <glib.h>

typedef struct _HudApp HudApp;

gboolean hud_control_start(HudApp *app, GError **error);
void hud_control_stop(HudApp *app);

#endif
