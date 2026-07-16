#ifndef WCH_HUD_LAYOUT_H
#define WCH_HUD_LAYOUT_H

#include <glib.h>

#include "hud-module.h"

typedef struct _HudApp HudApp;

gboolean hud_layout_slot_visible(HudApp *app, HudModuleSlot slot);
HudModuleSlot hud_layout_first_slot(HudApp *app);
HudModuleSlot hud_layout_next_slot(HudApp *app, HudModuleSlot slot, int delta);
void hud_layout_restore(HudApp *app);
void hud_layout_expand_selected(HudApp *app);
void hud_layout_toggle_selected(HudApp *app);

#endif
