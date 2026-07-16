#ifndef WCH_HUD_CONFIG_H
#define WCH_HUD_CONFIG_H

#include <glib.h>

typedef struct _HudApp HudApp;

void hud_config_load(HudApp *app);
gboolean hud_config_save_opacity(HudApp *app, GError **error);

#endif
