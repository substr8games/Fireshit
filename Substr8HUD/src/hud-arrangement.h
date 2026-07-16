#ifndef WCH_HUD_ARRANGEMENT_H
#define WCH_HUD_ARRANGEMENT_H

#include <glib.h>

#include "hud-module.h"

typedef struct _HudApp HudApp;

void hud_arrangement_apply_saved(HudApp *app);
gboolean hud_arrangement_move_module(
    HudApp *app,
    const char *module_id,
    HudModuleSlot target,
    guint index,
    GError **error);

#endif
