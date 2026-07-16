#include "hud-control.h"
#include <errno.h>
#include <string.h>

#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include <glib/gstdio.h>

#include "hud-app.h"
#include "hud-arrangement.h"
#include "hud-config.h"
#include "hud-surface.h"
#include "hud-state.h"
#include "hud-settings.h"

#define CONTROL_LIMIT 64


static gboolean quit_idle(gpointer user_data)
{
    HudApp *app = user_data;
    hud_app_log(app, "quit requested through control socket");
    g_application_quit(G_APPLICATION(app->application));
    return G_SOURCE_REMOVE;
}

static gboolean socket_is_live(const char *path)
{
    GSocketClient *client = g_socket_client_new();
    GSocketAddress *address = g_unix_socket_address_new(path);
    GError *error = NULL;
    GSocketConnection *connection = g_socket_client_connect(
        client, G_SOCKET_CONNECTABLE(address), NULL, &error);
    gboolean live = connection != NULL;

    if (connection != NULL) {
        g_io_stream_close(G_IO_STREAM(connection), NULL, NULL);
        g_object_unref(connection);
    }
    g_clear_error(&error);
    g_object_unref(address);
    g_object_unref(client);
    return live;
}

static gchar *dispatch_command(HudApp *app, const char *command)
{
    if (g_strcmp0(command, "status") == 0) {
        return g_strdup_printf("%s\n", hud_state_name(app->state));
    }

    if (g_strcmp0(command, "reload") == 0) {
        HudState current = app->state;
        hud_config_load(app);
        hud_settings_load(app);
        app->state = current;
        hud_arrangement_apply_saved(app);
        hud_surface_refresh_settings(app);
        hud_app_log(app, "configuration reloaded");
        return g_strdup("reloaded\n");
    }

    if (g_strcmp0(command, "quit") == 0) {
        g_idle_add(quit_idle, app);
        return g_strdup("quitting\n");
    }

    HudState target;
    if (g_strcmp0(command, "cycle") == 0) {
        target = hud_state_next(app->state);
    } else if (!hud_state_parse(command, &target)) {
        return g_strdup_printf("ERROR unknown command: %s\n", command);
    }

    hud_state_apply(app, target);
    return g_strdup_printf("%s\n", hud_state_name(app->state));
}

static gboolean on_incoming(
    GSocketService *service,
    GSocketConnection *connection,
    GObject *source_object,
    gpointer user_data)
{
    (void)service;
    (void)source_object;
    HudApp *app = user_data;
    GInputStream *input = g_io_stream_get_input_stream(G_IO_STREAM(connection));
    GOutputStream *output = g_io_stream_get_output_stream(G_IO_STREAM(connection));
    gchar request[CONTROL_LIMIT + 1] = {0};
    GError *error = NULL;

    gssize count = g_input_stream_read(
        input, request, CONTROL_LIMIT, NULL, &error);
    if (count <= 0) {
        g_clear_error(&error);
        g_io_stream_close(G_IO_STREAM(connection), NULL, NULL);
        return TRUE;
    }

    request[count] = '\0';
    gchar *command = g_strstrip(request);
    gchar *response = dispatch_command(app, command);
    g_output_stream_write_all(
        output,
        response,
        strlen(response),
        NULL,
        NULL,
        &error);
    g_output_stream_flush(output, NULL, NULL);

    if (error != NULL) {
        hud_app_log(app, "control response failed: %s", error->message);
        g_clear_error(&error);
    }

    g_free(response);
    g_io_stream_close(G_IO_STREAM(connection), NULL, NULL);
    return TRUE;
}

gboolean hud_control_start(HudApp *app, GError **error)
{
    app->socket_path = g_build_filename(
        g_get_user_runtime_dir(), "wayfire-hud.sock", NULL);

    if (g_file_test(app->socket_path, G_FILE_TEST_EXISTS)) {
        if (socket_is_live(app->socket_path)) {
            g_set_error(
                error,
                G_IO_ERROR,
                G_IO_ERROR_EXISTS,
                "another HUD owns %s",
                app->socket_path);
            return FALSE;
        }
        if (g_unlink(app->socket_path) != 0) {
            g_set_error(
                error,
                G_IO_ERROR,
                g_io_error_from_errno(errno),
                "could not remove stale socket %s",
                app->socket_path);
            return FALSE;
        }
    }

    app->control_service = g_socket_service_new();
    GSocketAddress *address = g_unix_socket_address_new(app->socket_path);
    gboolean bound = g_socket_listener_add_address(
        G_SOCKET_LISTENER(app->control_service),
        address,
        G_SOCKET_TYPE_STREAM,
        G_SOCKET_PROTOCOL_DEFAULT,
        NULL,
        NULL,
        error);
    g_object_unref(address);

    if (!bound) {
        g_clear_object(&app->control_service);
        return FALSE;
    }

    app->owns_socket = TRUE;

    if (g_chmod(app->socket_path, 0600) != 0) {
        g_set_error(
            error,
            G_IO_ERROR,
            g_io_error_from_errno(errno),
            "could not secure socket %s",
            app->socket_path);
        hud_control_stop(app);
        return FALSE;
    }

    g_signal_connect(
        app->control_service, "incoming", G_CALLBACK(on_incoming), app);
    g_socket_service_start(app->control_service);
    hud_app_log(app, "control socket ready at %s", app->socket_path);
    return TRUE;
}

void hud_control_stop(HudApp *app)
{
    if (app == NULL) {
        return;
    }

    if (app->control_service != NULL) {
        g_socket_service_stop(app->control_service);
        g_clear_object(&app->control_service);
    }
    if (app->owns_socket && app->socket_path != NULL) {
        g_unlink(app->socket_path);
        app->owns_socket = FALSE;
    }
}
