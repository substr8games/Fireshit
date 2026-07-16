#ifndef WCH_HUD_SETTINGS_H
#define WCH_HUD_SETTINGS_H

#include <glib.h>

typedef struct _HudApp HudApp;
typedef struct _HudModule HudModule;

typedef enum {
    HUD_PLACEMENT_LEFT = 0,
    HUD_PLACEMENT_RIGHT,
    HUD_PLACEMENT_FULLSCREEN,
} HudPlacement;

const char *hud_placement_name(HudPlacement placement);
gboolean hud_placement_parse(const char *text, HudPlacement *out_placement);
void hud_settings_load(HudApp *app);
void hud_settings_apply_module(HudApp *app, HudModule *module);
gboolean hud_settings_save_layout(HudApp *app, GError **error);

#endif
