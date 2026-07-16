#ifndef WCH_HUD_STATE_H
#define WCH_HUD_STATE_H

#include <glib.h>

typedef struct _HudApp HudApp;

typedef enum {
    HUD_STATE_DESKTOP = 0,
    HUD_STATE_TOP,
    HUD_STATE_INTERACTIVE,
    HUD_STATE_INVISIBLE,
    HUD_STATE_COUNT
} HudState;

const char *hud_state_name(HudState state);
gboolean hud_state_parse(const char *text, HudState *out_state);
HudState hud_state_next(HudState state);
void hud_state_apply(HudApp *app, HudState target);

#endif
