#include "hud-slot.h"

#include "hud-app.h"
#include "hud-dnd.h"

static gboolean is_lifecycle(HudModule *module, const char *name)
{
    return module != NULL && g_strcmp0(module->lifecycle, name) == 0;
}

static HudModule *module_for_child(GtkWidget *child)
{
    return child == NULL ? NULL :
        g_object_get_data(G_OBJECT(child), "hud-module");
}

static gboolean slot_contains(HudSlot *slot, HudModule *module)
{
    return hud_slot_index_of(slot, module) < slot->modules->len;
}

static void activate_module(HudSlot *slot, HudModule *next)
{
    if (slot == NULL || slot->active == next) {
        return;
    }

    HudModule *previous = slot->active;
    if (previous != NULL) {
        hud_module_set_input(previous, FALSE);
        if (is_lifecycle(previous, "visible")) {
            hud_module_stop(previous);
        }
    }

    slot->active = next;
    if (next == NULL) {
        return;
    }

    if (is_lifecycle(next, "visible")) {
        hud_module_spawn(next);
    }
    hud_module_set_input(next, slot->app->module_input_enabled);
    if (slot->app->state == HUD_STATE_INTERACTIVE) {
        hud_module_focus(next);
    }
}

static void on_visible_child_changed(
    GObject *object,
    GParamSpec *parameter,
    gpointer user_data)
{
    (void)parameter;
    (void)user_data;
    GtkStack *stack = GTK_STACK(object);
    HudSlot *slot = g_object_get_data(object, "hud-slot");
    activate_module(
        slot,
        module_for_child(gtk_stack_get_visible_child(stack)));
}

static GtkWidget *empty_slot(void)
{
    GtkWidget *pane = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *label = gtk_label_new("DROP MODULE HERE");
    gtk_widget_add_css_class(pane, "hud-pane");
    gtk_widget_add_css_class(label, "hud-empty");
    gtk_widget_set_hexpand(pane, TRUE);
    gtk_widget_set_vexpand(pane, TRUE);
    gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(pane), label);
    return pane;
}

HudSlot *hud_slot_new(HudApp *app, HudModuleSlot id)
{
    HudSlot *slot = g_new0(HudSlot, 1);
    slot->app = app;
    slot->id = id;
    slot->modules = g_ptr_array_new();
    return slot;
}

void hud_slot_add_module(HudSlot *slot, HudModule *module)
{
    if (slot == NULL || module == NULL || module->slot != slot->id) {
        return;
    }

    guint index = 0;
    while (index < slot->modules->len) {
        HudModule *existing = g_ptr_array_index(slot->modules, index);
        if (module->bank_order < existing->bank_order ||
            (module->bank_order == existing->bank_order &&
             g_strcmp0(module->id, existing->id) < 0)) {
            break;
        }
        index++;
    }
    g_ptr_array_insert(slot->modules, index, module);
}

void hud_slot_insert_module(
    HudSlot *slot,
    HudModule *module,
    guint index)
{
    if (slot == NULL || module == NULL) { return; }
    g_ptr_array_insert(slot->modules, MIN(index, slot->modules->len), module);
}

guint hud_slot_index_of(HudSlot *slot, HudModule *module)
{
    if (slot == NULL || module == NULL) {
        return G_MAXUINT;
    }
    for (guint index = 0; index < slot->modules->len; index++) {
        if (g_ptr_array_index(slot->modules, index) == module) {
            return index;
        }
    }
    return slot->modules->len;
}

guint hud_slot_remove_module(HudSlot *slot, HudModule *module)
{
    guint index = hud_slot_index_of(slot, module);
    if (slot != NULL && index < slot->modules->len) {
        g_ptr_array_remove_index(slot->modules, index);
    }
    return index;
}

void hud_slot_normalize_orders(HudSlot *slot)
{
    if (slot == NULL) { return; }
    for (guint index = 0; index < slot->modules->len; index++) {
        HudModule *module = g_ptr_array_index(slot->modules, index);
        module->slot = slot->id;
        module->bank_order = (gint)((index + 1) * 10);
    }
}

void hud_slot_rebuild(HudSlot *slot, HudModule *preferred_active)
{
    if (slot == NULL || slot->stack == NULL) {
        return;
    }

    HudModule *previous = slot->active;
    if (slot->visible_handler != 0) {
        g_signal_handler_block(slot->stack, slot->visible_handler);
    }

    for (guint index = 0;
         slot->app->modules != NULL && index < slot->app->modules->len;
         index++) {
        HudModule *module = g_ptr_array_index(slot->app->modules, index);
        if (module->pane != NULL &&
            gtk_widget_get_parent(module->pane) == GTK_WIDGET(slot->stack)) {
            gtk_stack_remove(slot->stack, module->pane);
        }
    }

    GtkWidget *child = gtk_stack_get_child_by_name(slot->stack, "__empty__");
    if (child != NULL) {
        gtk_stack_remove(slot->stack, child);
    }

    if (slot->modules->len == 0) {
        gtk_stack_add_named(slot->stack, empty_slot(), "__empty__");
        slot->active = NULL;
    } else {
        for (guint index = 0; index < slot->modules->len; index++) {
            HudModule *module = g_ptr_array_index(slot->modules, index);
            if (module->pane == NULL) {
                hud_module_create_pane(module);
            }
            g_object_set_data(G_OBJECT(module->pane), "hud-module", module);
            gtk_stack_add_titled(
                slot->stack,
                module->pane,
                module->id,
                module->title);
        }

        HudModule *next = slot_contains(slot, preferred_active)
            ? preferred_active
            : g_ptr_array_index(slot->modules, 0);
        slot->active = next;
        gtk_stack_set_visible_child_name(slot->stack, next->id);
    }

    gtk_widget_set_visible(
        GTK_WIDGET(slot->switcher),
        slot->modules->len > 1);

    if (slot->visible_handler != 0) {
        g_signal_handler_unblock(slot->stack, slot->visible_handler);
    }

    if (previous != slot->active) {
        hud_module_set_input(previous, FALSE);
    }
    hud_module_set_input(slot->active, slot->app->module_input_enabled);
}

GtkWidget *hud_slot_finish(HudSlot *slot)
{
    slot->root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    g_object_ref_sink(slot->root);
    slot->stack = GTK_STACK(gtk_stack_new());
    slot->switcher = GTK_STACK_SWITCHER(gtk_stack_switcher_new());

    gtk_widget_add_css_class(slot->root, "hud-slot");
    gtk_widget_add_css_class(
        GTK_WIDGET(slot->switcher),
        "hud-slot-switcher");
    gtk_widget_set_focusable(slot->root, TRUE);
    gtk_widget_set_hexpand(slot->root, TRUE);
    gtk_widget_set_vexpand(slot->root, TRUE);
    gtk_stack_switcher_set_stack(slot->switcher, slot->stack);
    gtk_stack_set_transition_type(slot->stack, GTK_STACK_TRANSITION_TYPE_NONE);
    gtk_stack_set_hhomogeneous(slot->stack, TRUE);
    gtk_stack_set_vhomogeneous(slot->stack, TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(slot->stack), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(slot->stack), TRUE);

    gtk_box_append(GTK_BOX(slot->root), GTK_WIDGET(slot->switcher));
    gtk_box_append(GTK_BOX(slot->root), GTK_WIDGET(slot->stack));

    g_object_set_data(G_OBJECT(slot->stack), "hud-slot", slot);
    slot->visible_handler = g_signal_connect(
        slot->stack,
        "notify::visible-child",
        G_CALLBACK(on_visible_child_changed),
        NULL);
    hud_slot_rebuild(slot, NULL);
    hud_dnd_attach_slot(slot);
    return slot->root;
}

void hud_slot_spawn_active(HudSlot *slot)
{
    if (slot != NULL && is_lifecycle(slot->active, "visible")) {
        hud_module_spawn(slot->active);
    }
}

void hud_slot_set_input(HudSlot *slot, gboolean enabled)
{
    if (slot == NULL) {
        return;
    }
    for (guint index = 0; index < slot->modules->len; index++) {
        hud_module_set_input(g_ptr_array_index(slot->modules, index), FALSE);
    }
    hud_module_set_input(slot->active, enabled);
}

gboolean hud_slot_focus(HudSlot *slot)
{
    return slot != NULL && hud_module_focus(slot->active);
}

gboolean hud_slot_cycle(HudSlot *slot, int delta)
{
    if (slot == NULL || slot->stack == NULL || slot->modules->len < 2) {
        return FALSE;
    }

    guint current = hud_slot_index_of(slot, slot->active);
    int next = ((int)current + delta) % (int)slot->modules->len;
    if (next < 0) {
        next += slot->modules->len;
    }

    HudModule *module = g_ptr_array_index(slot->modules, next);
    gtk_stack_set_visible_child_name(slot->stack, module->id);
    return TRUE;
}

HudModule *hud_slot_active(HudSlot *slot)
{
    return slot == NULL ? NULL : slot->active;
}

void hud_slot_free(HudSlot *slot)
{
    if (slot == NULL) { return; }
    if (slot->stack != NULL && slot->visible_handler != 0) {
        g_signal_handler_disconnect(slot->stack, slot->visible_handler);
    }
    g_ptr_array_unref(slot->modules);
    g_clear_object(&slot->root);
    g_free(slot);
}
