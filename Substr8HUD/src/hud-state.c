#include "hud-state.h"

#include <gtk4-layer-shell.h>

#include "hud-app.h"
#include "hud-navigation.h"
#include "hud-surface.h"
#include "hud-terminal.h"

static gboolean focus_navigation_idle(gpointer user_data)
{
    HudApp *app = user_data;
    if (app->state == HUD_STATE_INTERACTIVE) {
        hud_navigation_enter(app);
    }
    return G_SOURCE_REMOVE;
}

const char *hud_state_name(HudState state)
{
    static const char *names[] = {
        "desktop",
        "top",
        "interactive",
        "invisible",
    };

    if (state < 0 || state >= HUD_STATE_COUNT) {
        return "unknown";
    }
    return names[state];
}

gboolean hud_state_parse(const char *text, HudState *out_state)
{
    if (text == NULL || out_state == NULL) {
        return FALSE;
    }

    for (int state = 0; state < HUD_STATE_COUNT; state++) {
        if (g_strcmp0(text, hud_state_name((HudState)state)) == 0) {
            *out_state = (HudState)state;
            return TRUE;
        }
    }
    return FALSE;
}

HudState hud_state_next(HudState state)
{
    return (HudState)((state + 1) % HUD_STATE_COUNT);
}

void hud_state_apply(HudApp *app, HudState target)
{
    if (app == NULL || app->window == NULL || target < 0 || target >= HUD_STATE_COUNT) {
        return;
    }

    HudState previous = app->state;
    app->state = target;

    if (previous == HUD_STATE_INTERACTIVE &&
        target != HUD_STATE_INTERACTIVE) {
        hud_navigation_leave(app);
    }

    hud_terminal_set_input(app, FALSE);
    gtk_layer_set_keyboard_mode(
        app->window, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    hud_surface_set_pointer_input(app, FALSE);

    if (target == HUD_STATE_INVISIBLE) {
        hud_surface_hide(app);
    } else {
        hud_surface_set_layer(app, target);

        if (target == HUD_STATE_INTERACTIVE) {
            gtk_layer_set_keyboard_mode(
                app->window, GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
        }

        hud_surface_show(app);
        hud_surface_set_pointer_input(
            app, target == HUD_STATE_INTERACTIVE);

        if (target == HUD_STATE_INTERACTIVE) {
            hud_terminal_set_input(app, TRUE);
            g_idle_add(focus_navigation_idle, app);
        }
    }

    hud_app_log(
        app,
        "state transition %s -> %s",
        hud_state_name(previous),
        hud_state_name(target));
}
