#ifndef WCH_HUD_OUTPUT_H
#define WCH_HUD_OUTPUT_H

#include <gtk/gtk.h>

typedef struct _HudApp HudApp;

void hud_output_attach(HudApp *app);
void hud_output_refresh(HudApp *app);
void hud_output_detach(HudApp *app);

#endif
