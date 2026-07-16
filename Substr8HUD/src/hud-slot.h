#ifndef WCH_HUD_SLOT_H
#define WCH_HUD_SLOT_H

#include <gtk/gtk.h>

#include "hud-module.h"

typedef struct _HudSlot {
    HudApp *app;
    HudModuleSlot id;
    GtkWidget *root;
    GtkStack *stack;
    GtkStackSwitcher *switcher;
    GPtrArray *modules;
    HudModule *active;
    gulong visible_handler;
} HudSlot;

HudSlot *hud_slot_new(HudApp *app, HudModuleSlot id);
void hud_slot_add_module(HudSlot *slot, HudModule *module);
void hud_slot_insert_module(HudSlot *slot, HudModule *module, guint index);
guint hud_slot_remove_module(HudSlot *slot, HudModule *module);
guint hud_slot_index_of(HudSlot *slot, HudModule *module);
void hud_slot_normalize_orders(HudSlot *slot);
GtkWidget *hud_slot_finish(HudSlot *slot);
void hud_slot_rebuild(HudSlot *slot, HudModule *preferred_active);
void hud_slot_spawn_active(HudSlot *slot);
void hud_slot_set_input(HudSlot *slot, gboolean enabled);
gboolean hud_slot_focus(HudSlot *slot);
gboolean hud_slot_cycle(HudSlot *slot, int delta);
HudModule *hud_slot_active(HudSlot *slot);
void hud_slot_free(HudSlot *slot);

#endif
