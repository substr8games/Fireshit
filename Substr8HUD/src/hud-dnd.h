#ifndef WCH_HUD_DND_H
#define WCH_HUD_DND_H

#include <gtk/gtk.h>

typedef struct _HudModule HudModule;
typedef struct _HudSlot HudSlot;

void hud_dnd_attach_module(HudModule *module, GtkWidget *handle);
void hud_dnd_attach_slot(HudSlot *slot);

#endif
