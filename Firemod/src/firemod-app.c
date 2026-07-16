#include "firemod-app.h"

#include "firemod-color-picker.h"
#include "firemod-discovery.h"
#include "firemod-dispatch.h"
#include "firemod-first-party.h"
#include "firemod-fireshit-page.h"
#include "firemod-settings.h"
#include "firemod-menu.h"
#include "firemod-pages.h"
#include "firemod-runtime.h"
#include "firemod-screencopy.h"
#include "firemod-shell.h"

static void menu_requested(double x, double y, gpointer user_data)
{ firemod_menu_show_at(((FiremodApp *)user_data)->menu, x, y); }

static void configure_builtin_capabilities(FiremodApp *app, const GError *capture_error)
{
    FiremodCapability *screenshot = firemod_capability_find(
        app->capabilities, "capture.screenshot.output");
    FiremodCapability *snip = firemod_capability_find(
        app->capabilities, "capture.screenshot.region");
    FiremodCapability *picker = firemod_capability_find(
        app->capabilities, "utility.color.pick");
    const gchar *capture_reason = app->screencopy != NULL ?
        "Native Wayfire pixel capture available" :
        (capture_error != NULL ? capture_error->message :
            "Native Wayfire pixel capture unavailable");
    FiremodActionState capture_state = app->screencopy != NULL ?
        FIREMOD_ACTION_AVAILABLE : FIREMOD_ACTION_UNAVAILABLE;
    if (screenshot != NULL) firemod_capability_set_state(
        screenshot, capture_state, capture_reason);
    if (snip != NULL) firemod_capability_set_state(
        snip, capture_state, capture_reason);
    if (picker != NULL) firemod_capability_set_state(
        picker, capture_state, app->screencopy != NULL ?
            "Native Wayfire pixel sampling available" : capture_reason);

    FiremodCapability *record_output = firemod_capability_find(
        app->capabilities, "capture.record.output");
    FiremodCapability *record_region = firemod_capability_find(
        app->capabilities, "capture.record.region");
    FiremodCapability *record_application = firemod_capability_find(
        app->capabilities, "capture.record.application");
    gchar *frametap = g_find_program_in_path("frametap");
    FiremodActionState record_state = frametap != NULL ?
        FIREMOD_ACTION_AVAILABLE : FIREMOD_ACTION_UNAVAILABLE;
    const gchar *record_reason = frametap != NULL ?
        "FrameTap recording interface available" : "FrameTap is not installed";
    FiremodCapability *records[] = {
        record_output, record_region, record_application
    };
    for (guint i = 0; i < G_N_ELEMENTS(records); i++) {
        FiremodCapability *capability = records[i];
        if (capability == NULL) continue;
        g_free(capability->executable);
        capability->executable = frametap != NULL ? g_strdup(frametap) : NULL;
        firemod_capability_set_state(capability, record_state, record_reason);
    }
    g_free(frametap);
}

FiremodApp *firemod_app_new(GtkApplication *application)
{
    FiremodApp *app = g_new0(FiremodApp, 1);
    app->application = application; app->active_filter = FIREMOD_FILTER_ALL;
    return app;
}
void firemod_app_startup(GApplication *application, gpointer user_data)
{
    FiremodApp *app = user_data;
    firemod_shell_install_css();
    g_application_hold(application);
    app->capabilities = firemod_capability_registry_new();
    GError *error = NULL;
    app->settings = firemod_settings_new(&error);
    if (error != NULL) {
        g_warning("Firemod configuration was not loaded: %s", error->message);
        g_clear_error(&error);
    }
    if (app->settings == NULL) {
        g_error("Firemod could not initialize its settings authority");
    }
    if (!firemod_settings_apply(app->settings, app->capabilities, &error)) {
        g_warning("Saved Firemod configuration could not be applied: %s",
            error != NULL ? error->message : "unknown error");
        g_clear_error(&error);
    }
    app->screencopy = firemod_screencopy_new(
        gdk_display_get_default(), &error);
    configure_builtin_capabilities(app, error);
    g_clear_error(&error);
    app->color_picker = firemod_color_picker_new(
        app->application, app->screencopy);
    app->menu = firemod_menu_new(app->application, app->capabilities,
        app->settings, firemod_dispatch_action, app);
    app->runtime = firemod_runtime_new(menu_requested, app, &error);
    if (app->runtime == NULL) {
        g_warning("Firemod runtime endpoint unavailable: %s",
            error != NULL ? error->message : "unknown error");
    }
    g_clear_error(&error);
}

void firemod_app_show_settings(FiremodApp *app)
{
    if (app->window == NULL) {
        firemod_shell_build(app);
        firemod_app_rescan(app);
    }
    gtk_window_present(app->window);
}

void firemod_app_activate(GtkApplication *application, gpointer user_data)
{ (void)application; firemod_app_show_settings(user_data); }

void firemod_app_show_fireshit_settings(FiremodApp *app)
{
    firemod_app_show_settings(app);
    firemod_shell_select_page(app, "fireshit");
}

static gboolean parse_menu_args(char **argv, int argc, double *x, double *y)
{
    if (argc != 4 || g_strcmp0(argv[1], "--menu-at") != 0) return FALSE;
    char *end_x = NULL;
    char *end_y = NULL;
    *x = g_ascii_strtod(argv[2], &end_x);
    *y = g_ascii_strtod(argv[3], &end_y);
    return end_x != argv[2] && *end_x == '\0' &&
        end_y != argv[3] && *end_y == '\0';
}

gint firemod_app_command_line(
    GApplication *application,
    GApplicationCommandLine *command_line,
    gpointer user_data)
{
    FiremodApp *app = user_data;
    int argc = 0;
    char **argv = g_application_command_line_get_arguments(
        command_line, &argc);
    double x = 0.0;
    double y = 0.0;
    gint status = 0;
    if (argc == 1) {
        g_application_activate(application);
    } else if (argc == 2 && g_strcmp0(argv[1], "--background") == 0) {
        status = 0;
    } else if (parse_menu_args(argv, argc, &x, &y)) {
        firemod_menu_show_at(app->menu, x, y);
    } else if (argc == 2 && g_strcmp0(argv[1], "--quit") == 0) {
        g_application_quit(application);
    } else {
        g_application_command_line_printerr(command_line,
            "usage: firemod [--background | --menu-at X Y | --quit]\n");
        status = 2;
    }
    g_strfreev(argv);
    return status;
}

void firemod_app_rescan(FiremodApp *app)
{
    gtk_label_set_text(app->status_label, "Scanning configuration authorities…");
    while (g_main_context_iteration(NULL, FALSE)) { }
    firemod_inventory_free(app->inventory);
    app->inventory = firemod_discovery_scan();
    firemod_first_party_attach_capabilities(
        app->inventory, app->capabilities);
    char *status = g_strdup_printf(
        "%u grouped modules · %u uncertain results hidden · read-only discovery mode",
        app->inventory->modules->len,
        app->inventory->uncertain_modules->len);
    gtk_label_set_text(app->status_label, status);
    g_free(status);
    firemod_app_render(app);
}

void firemod_app_render(FiremodApp *app)
{ if (app->inventory != NULL) firemod_pages_render(app); }

void firemod_app_shutdown(GApplication *application, gpointer user_data)
{
    (void)application;
    FiremodApp *app = user_data;
    firemod_menu_free(app->menu);
    firemod_runtime_free(app->runtime);
    firemod_color_picker_free(app->color_picker);
    firemod_screencopy_free(app->screencopy);
    firemod_fireshit_page_free(app->fireshit_settings_page);
    firemod_settings_free(app->settings);
    firemod_capability_registry_free(app->capabilities);
    firemod_inventory_free(app->inventory);
    app->inventory = NULL;
}

void firemod_app_free(FiremodApp *app)
{ g_free(app); }
