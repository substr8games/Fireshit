#include "hud-app.h"

#include <glib/gstdio.h>
#include <stdarg.h>
#include <stdio.h>

#include "hud-config.h"
#include "hud-control.h"
#include "hud-opacity.h"
#include "hud-output.h"
#include "hud-surface.h"
#include "hud-settings.h"
#include "hud-terminal.h"

HudApp *hud_app_new(GtkApplication *application)
{
    HudApp *app = g_new0(HudApp, 1);
    app->application = application;
    app->state = HUD_STATE_DESKTOP;
    app->selected_slot = HUD_SLOT_TOP_LEFT;
    app->diagnostic_path = g_build_filename(
        g_get_user_state_dir(), "wayfire-hud.log", NULL);
    hud_config_load(app);
    hud_settings_load(app);
    return app;
}

void hud_app_log(HudApp *app, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    gchar *message = g_strdup_vprintf(format, arguments);
    va_end(arguments);

    GDateTime *now = g_date_time_new_now_local();
    gchar *stamp = g_date_time_format_iso8601(now);
    gchar *line = g_strdup_printf("[%s] %s\n", stamp, message);

    g_printerr("%s", line);

    if (app != NULL && app->diagnostic_path != NULL) {
        gchar *directory = g_path_get_dirname(app->diagnostic_path);
        if (g_mkdir_with_parents(directory, 0700) == 0) {
            FILE *file = g_fopen(app->diagnostic_path, "a");
            if (file != NULL) {
                fputs(line, file);
                fclose(file);
            }
        }
        g_free(directory);
    }

    g_free(line);
    g_free(stamp);
    g_date_time_unref(now);
    g_free(message);
}

void hud_app_activate(GtkApplication *application, gpointer user_data)
{
    HudApp *app = user_data;
    if (app->activated) {
        return;
    }

    GError *error = NULL;
    if (!hud_control_start(app, &error)) {
        hud_app_log(app, "control socket unavailable: %s", error->message);
        g_clear_error(&error);
        g_application_quit(G_APPLICATION(application));
        return;
    }

    app->window = hud_surface_create(app);
    if (app->window == NULL) {
        hud_control_stop(app);
        g_application_quit(G_APPLICATION(application));
        return;
    }

    hud_output_attach(app);
    GtkWidget *view = hud_terminal_create_view(app);
    GtkWidget *overlay = hud_opacity_wrap_view(app, view);
    gtk_window_set_child(app->window, overlay);
    hud_terminal_spawn_all(app);

    app->activated = TRUE;
    HudState initial = app->state;
    app->state = HUD_STATE_INVISIBLE;
    hud_state_apply(app, initial);
    hud_app_log(
        app,
        "HUD authority started in %s state",
        hud_state_name(initial));
}

void hud_app_shutdown(GApplication *application, gpointer user_data)
{
    (void)application;
    HudApp *app = user_data;
    hud_app_log(app, "HUD authority shutting down");
    hud_opacity_shutdown(app);
    hud_terminal_stop_all(app);
    hud_output_detach(app);
    hud_control_stop(app);
}

void hud_app_free(HudApp *app)
{
    if (app == NULL) {
        return;
    }

    hud_terminal_free_all(app);
    g_free(app->socket_path);
    g_free(app->diagnostic_path);
    g_free(app->config_path);
    g_free(app->module_directory);
    g_free(app->user_config_path);
    if (app->user_settings != NULL) {
        g_key_file_unref(app->user_settings);
    }
    if (app->css_provider != NULL) {
        g_object_unref(app->css_provider);
    }
    g_free(app);
}
