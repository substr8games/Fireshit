#include "firemod-dispatch.h"

#include <glib/gstdio.h>
#include <string.h>

#include "firemod-capability.h"
#include "firemod-capture.h"
#include "firemod-color-picker.h"

static void show_error(const char *detail)
{
    GtkAlertDialog *dialog = gtk_alert_dialog_new("Fireshit action failed");
    gtk_alert_dialog_set_detail(dialog, detail);
    gtk_alert_dialog_show(dialog, NULL);
    g_object_unref(dialog);
}

static GdkMonitor *monitor_at(GdkDisplay *display, double x, double y)
{
    GListModel *monitors = gdk_display_get_monitors(display);
    for (guint i = 0; i < g_list_model_get_n_items(monitors); i++) {
        GdkMonitor *monitor = g_list_model_get_item(monitors, i);
        GdkRectangle geometry;
        gdk_monitor_get_geometry(monitor, &geometry);
        if (x >= geometry.x && y >= geometry.y &&
            x < geometry.x + geometry.width &&
            y < geometry.y + geometry.height) return monitor;
        g_object_unref(monitor);
    }
    return NULL;
}

static gchar *recording_path(void)
{
    const char *videos = g_get_user_special_dir(G_USER_DIRECTORY_VIDEOS);
    char *base = videos != NULL ? g_strdup(videos) :
        g_build_filename(g_get_home_dir(), "Videos", NULL);
    char *directory = g_build_filename(base, "Fireshit", NULL);
    g_free(base);
    if (g_mkdir_with_parents(directory, 0700) != 0) {
        g_free(directory);
        return NULL;
    }
    GDateTime *now = g_date_time_new_now_local();
    char *stamp = g_date_time_format(now, "%Y%m%d-%H%M%S");
    g_date_time_unref(now);
    char *path = NULL;
    for (guint suffix = 0; suffix < 1000; suffix++) {
        char *name = suffix == 0 ?
            g_strdup_printf("frametap-%s.mkv", stamp) :
            g_strdup_printf("frametap-%s-%02u.mkv", stamp, suffix);
        path = g_build_filename(directory, name, NULL);
        g_free(name);
        if (!g_file_test(path, G_FILE_TEST_EXISTS)) break;
        g_clear_pointer(&path, g_free);
    }
    g_free(stamp);
    g_free(directory);
    return path;
}

typedef struct {
    gchar **argv;
} FrameTapLaunch;

static gboolean launch_frametap(gpointer user_data)
{
    FrameTapLaunch *launch = user_data;
    GError *error = NULL;
    if (!g_spawn_async(NULL, launch->argv, NULL, G_SPAWN_SEARCH_PATH,
            NULL, NULL, NULL, &error)) {
        show_error(error->message);
        g_clear_error(&error);
    }
    g_strfreev(launch->argv);
    g_free(launch);
    return G_SOURCE_REMOVE;
}

static void queue_frametap(gchar **argv)
{
    FrameTapLaunch *launch = g_new0(FrameTapLaunch, 1);
    launch->argv = argv;
    g_timeout_add(180, launch_frametap, launch);
}

static void dispatch_frametap_application(FiremodCapability *capability)
{
    char *path = recording_path();
    if (path == NULL) {
        show_error("Could not create the Fireshit recording directory");
        return;
    }
    gchar **argv = g_new0(gchar *, 5);
    argv[0] = g_strdup(capability->executable != NULL ?
        capability->executable : "frametap");
    argv[1] = g_strdup("--focused");
    argv[2] = g_strdup("--record");
    argv[3] = path;
    queue_frametap(argv);
}

static void dispatch_frametap_output(
    FiremodCapability *capability,
    double global_x,
    double global_y)
{
    GdkMonitor *monitor = monitor_at(
        gdk_display_get_default(), global_x, global_y);
    const gchar *connector = monitor != NULL ?
        gdk_monitor_get_connector(monitor) : NULL;
    if (connector == NULL || *connector == '\0') {
        show_error("The output beneath the pointer has no connector name");
        g_clear_object(&monitor);
        return;
    }
    char *path = recording_path();
    if (path == NULL) {
        show_error("Could not create the Fireshit recording directory");
        g_object_unref(monitor);
        return;
    }
    gchar **argv = g_new0(gchar *, 6);
    argv[0] = g_strdup(capability->executable != NULL ?
        capability->executable : "frametap");
    argv[1] = g_strdup("--output");
    argv[2] = g_strdup(connector);
    argv[3] = g_strdup("--record");
    argv[4] = path;
    g_object_unref(monitor);
    queue_frametap(argv);
}

static void dispatch_frametap_region(FiremodCapability *capability)
{
    char *path = recording_path();
    if (path == NULL) {
        show_error("Could not create the Fireshit recording directory");
        return;
    }
    gchar **argv = g_new0(gchar *, 5);
    argv[0] = g_strdup(capability->executable != NULL ?
        capability->executable : "frametap");
    argv[1] = g_strdup("--region");
    argv[2] = g_strdup("--record");
    argv[3] = path;
    queue_frametap(argv);
}

static gchar *replace_token(
    const char *input,
    const char *token,
    const char *value)
{
    gchar **parts = g_strsplit(input, token, -1);
    gchar *result = g_strjoinv(value, parts);
    g_strfreev(parts);
    return result;
}

static void dispatch_command(
    FiremodCapability *capability,
    double global_x,
    double global_y)
{
    char x[48];
    char y[48];
    g_ascii_dtostr(x, sizeof x, global_x);
    g_ascii_dtostr(y, sizeof y, global_y);
    char *with_x = replace_token(capability->command, "{x}", x);
    char *expanded = replace_token(with_x, "{y}", y);
    g_free(with_x);
    int argc = 0;
    char **argv = NULL;
    GError *error = NULL;
    if (!g_shell_parse_argv(expanded, &argc, &argv, &error) || argc == 0) {
        show_error(error != NULL ? error->message : "Invalid dispatch contract");
        g_clear_error(&error);
        g_free(expanded);
        return;
    }
    if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
            NULL, NULL, NULL, &error)) {
        show_error(error->message);
        g_clear_error(&error);
    }
    g_strfreev(argv);
    g_free(expanded);
}


static gchar *find_terminal(void)
{
    const gchar *preferred = g_getenv("TERMINAL");
    if (preferred != NULL && *preferred != '\0' && strchr(preferred, ' ') == NULL) {
        gchar *resolved = g_find_program_in_path(preferred);
        if (resolved != NULL) return resolved;
    }
    const gchar *candidates[] = {
        "footclient", "foot", "kitty", "alacritty", "wezterm", "xterm", NULL
    };
    for (guint i = 0; candidates[i] != NULL; i++) {
        gchar *resolved = g_find_program_in_path(candidates[i]);
        if (resolved != NULL) return resolved;
    }
    return NULL;
}

static void dispatch_argv(FiremodCapability *capability)
{
    if (capability->argv == NULL || capability->argv[0] == NULL) {
        show_error("The custom action has no executable argument vector");
        return;
    }
    gchar **argv = NULL;
    gchar *terminal = NULL;
    if (capability->run_in_terminal) {
        terminal = find_terminal();
        if (terminal == NULL) {
            show_error("No supported terminal executable is available");
            return;
        }
        gsize count = g_strv_length(capability->argv);
        argv = g_new0(gchar *, count + 3);
        argv[0] = g_strdup(terminal);
        argv[1] = g_strdup("-e");
        for (gsize i = 0; i < count; i++) argv[i + 2] = g_strdup(capability->argv[i]);
    } else {
        argv = g_strdupv(capability->argv);
    }
    GError *error = NULL;
    const gchar *working = capability->working_directory != NULL &&
        *capability->working_directory != '\0' ? capability->working_directory : NULL;
    if (!g_spawn_async(working, argv, NULL, G_SPAWN_SEARCH_PATH,
            NULL, NULL, NULL, &error)) {
        show_error(error->message);
        g_clear_error(&error);
    }
    g_free(terminal);
    g_strfreev(argv);
}

void firemod_dispatch_action(
    const gchar *capability_id,
    double global_x,
    double global_y,
    gpointer user_data)
{
    FiremodApp *app = user_data;
    FiremodCapability *capability = firemod_capability_find(
        app->capabilities, capability_id);
    if (capability == NULL || capability->state != FIREMOD_ACTION_AVAILABLE) {
        show_error(capability != NULL && capability->availability_reason != NULL ?
            capability->availability_reason : "Capability is unavailable");
        return;
    }
    if (capability->executable != NULL) {
        gboolean absolute = strchr(
            capability->executable, G_DIR_SEPARATOR) != NULL;
        char *resolved = absolute ? NULL :
            g_find_program_in_path(capability->executable);
        gboolean present = absolute ?
            g_file_test(capability->executable, G_FILE_TEST_IS_EXECUTABLE) :
            resolved != NULL;
        g_free(resolved);
        if (!present) {
            firemod_capability_set_state(capability, FIREMOD_ACTION_UNAVAILABLE,
                "Owning executable disappeared");
            show_error(capability->availability_reason);
            return;
        }
    }
    if (g_strcmp0(capability_id, "core.settings.open") == 0) {
        firemod_app_show_fireshit_settings(app);
    } else if (g_strcmp0(capability_id, "capture.screenshot.output") == 0) {
        firemod_capture_output(app, global_x, global_y);
    } else if (g_strcmp0(capability_id, "capture.screenshot.region") == 0) {
        firemod_capture_region(app, global_x, global_y);
    } else if (g_strcmp0(capability_id, "utility.color.pick") == 0) {
        firemod_color_picker_start(app->color_picker, global_x, global_y);
    } else if (g_strcmp0(capability_id, "capture.record.output") == 0) {
        dispatch_frametap_output(capability, global_x, global_y);
    } else if (g_strcmp0(capability_id, "capture.record.region") == 0) {
        dispatch_frametap_region(capability);
    } else if (g_strcmp0(capability_id, "capture.record.application") == 0) {
        dispatch_frametap_application(capability);
    } else if (capability->argv != NULL) {
        dispatch_argv(capability);
    } else if (capability->command != NULL) {
        dispatch_command(capability, global_x, global_y);
    } else {
        show_error("The registered owner supplied no dispatch contract");
    }
}
