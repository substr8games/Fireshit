#include "hud-dnd.h"

#include <gtk/gtk.h>

#include "hud-app.h"
#include "hud-arrangement.h"
#include "hud-layout.h"
#include "hud-module.h"
#include "hud-slot.h"
#include "hud-state.h"

typedef struct {
    HudApp *app;
    gchar *module_id;
    HudModuleSlot target;
    guint index;
} PendingMove;

static GdkContentProvider *on_prepare(
    GtkDragSource *source,
    double x,
    double y,
    gpointer user_data)
{
    (void)source;
    (void)x;
    (void)y;
    HudModule *module = user_data;

    if (module->app->state != HUD_STATE_INTERACTIVE) {
        return NULL;
    }

    return gdk_content_provider_new_typed(
        G_TYPE_STRING,
        module->id);
}

static gboolean perform_move(gpointer user_data)
{
    PendingMove *move = user_data;
    GError *error = NULL;

    if (!hud_arrangement_move_module(
            move->app,
            move->module_id,
            move->target,
            move->index,
            &error)) {
        hud_app_log(
            move->app,
            "module drag failed: %s",
            error != NULL ? error->message : "unknown error");
        g_clear_error(&error);
    }

    g_free(move->module_id);
    g_free(move);
    return G_SOURCE_REMOVE;
}

static guint insertion_index(HudSlot *slot, double x)
{
    guint count = slot->modules->len;
    gint width = gtk_widget_get_width(slot->root);
    if (width <= 0 || count == 0) {
        return count;
    }

    double ratio = CLAMP(x / (double)width, 0.0, 0.999999);
    return MIN((guint)(ratio * (count + 1)), count);
}

static gboolean on_drop(
    GtkDropTarget *target,
    const GValue *value,
    double x,
    double y,
    gpointer user_data)
{
    (void)target;
    (void)y;
    HudSlot *slot = user_data;
    const char *module_id = g_value_get_string(value);

    gtk_widget_remove_css_class(slot->root, "hud-slot-drop-target");
    if (slot->app->state != HUD_STATE_INTERACTIVE ||
        module_id == NULL ||
        !hud_layout_slot_visible(slot->app, slot->id)) {
        return FALSE;
    }

    PendingMove *move = g_new0(PendingMove, 1);
    move->app = slot->app;
    move->module_id = g_strdup(module_id);
    move->target = slot->id;
    move->index = insertion_index(slot, x);
    g_idle_add(perform_move, move);
    return TRUE;
}

static GdkDragAction on_motion(
    GtkDropTarget *target,
    double x,
    double y,
    gpointer user_data)
{
    (void)target;
    (void)x;
    (void)y;
    HudSlot *slot = user_data;
    if (slot->app->state != HUD_STATE_INTERACTIVE ||
        !hud_layout_slot_visible(slot->app, slot->id)) {
        return 0;
    }

    gtk_widget_add_css_class(slot->root, "hud-slot-drop-target");
    return GDK_ACTION_MOVE;
}

static void on_leave(GtkDropTarget *target, gpointer user_data)
{
    (void)target;
    HudSlot *slot = user_data;
    gtk_widget_remove_css_class(slot->root, "hud-slot-drop-target");
}

void hud_dnd_attach_module(
    HudModule *module,
    GtkWidget *handle)
{
    GtkDragSource *source = gtk_drag_source_new();
    gtk_drag_source_set_actions(source, GDK_ACTION_MOVE);
    g_signal_connect(source, "prepare", G_CALLBACK(on_prepare), module);
    gtk_widget_add_controller(
        handle,
        GTK_EVENT_CONTROLLER(source));
    gtk_widget_set_tooltip_text(
        handle,
        "Drag this module header to another panel");
}

void hud_dnd_attach_slot(HudSlot *slot)
{
    GtkDropTarget *target = gtk_drop_target_new(
        G_TYPE_STRING,
        GDK_ACTION_MOVE);
    g_signal_connect(target, "drop", G_CALLBACK(on_drop), slot);
    g_signal_connect(target, "motion", G_CALLBACK(on_motion), slot);
    g_signal_connect(target, "leave", G_CALLBACK(on_leave), slot);
    gtk_widget_add_controller(
        slot->root,
        GTK_EVENT_CONTROLLER(target));
}
