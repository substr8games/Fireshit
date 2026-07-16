#include "firemod-runtime.h"

#include <glib/gstdio.h>
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>

struct FiremodRuntime {
    GSocketService *service;
    gchar *path;
    FiremodMenuRequest menu_request;
    gpointer user_data;
};

static gboolean incoming(
    GSocketService *service,
    GSocketConnection *connection,
    GObject *source_object,
    gpointer user_data)
{
    (void)service; (void)source_object;
    FiremodRuntime *runtime = user_data;
    GInputStream *input = g_io_stream_get_input_stream(G_IO_STREAM(connection));
    GDataInputStream *data = g_data_input_stream_new(input);
    gsize length = 0;
    GError *error = NULL;
    char *line = g_data_input_stream_read_line(data, &length, NULL, &error);
    if (line != NULL && length <= 256) {
        double x = 0.0;
        double y = 0.0;
        int consumed = 0;
        if (sscanf(line, "FIREMOD/1 MENU %lf %lf %n",
                &x, &y, &consumed) == 2 && isfinite(x) && isfinite(y)) {
            while (line[consumed] == ' ' || line[consumed] == '\t') consumed++;
            if (line[consumed] == '\0') {
                runtime->menu_request(x, y, runtime->user_data);
            }
        }
    }
    g_clear_error(&error);
    g_free(line);
    g_object_unref(data);
    return TRUE;
}

FiremodRuntime *firemod_runtime_new(
    FiremodMenuRequest menu_request,
    gpointer user_data,
    GError **error)
{
    g_return_val_if_fail(menu_request != NULL, NULL);
    const char *runtime_dir = g_get_user_runtime_dir();
    if (runtime_dir == NULL) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
            "XDG_RUNTIME_DIR is unavailable");
        return NULL;
    }
    FiremodRuntime *runtime = g_new0(FiremodRuntime, 1);
    runtime->menu_request = menu_request;
    runtime->user_data = user_data;
    runtime->path = g_build_filename(runtime_dir, "firemod.sock", NULL);
    g_unlink(runtime->path);
    runtime->service = g_socket_service_new();
    GSocketAddress *address = g_unix_socket_address_new(runtime->path);
    gboolean added = g_socket_listener_add_address(
        G_SOCKET_LISTENER(runtime->service), address,
        G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT,
        NULL, NULL, error);
    g_object_unref(address);
    if (!added) {
        firemod_runtime_free(runtime);
        return NULL;
    }
    chmod(runtime->path, S_IRUSR | S_IWUSR);
    g_signal_connect(runtime->service, "incoming",
        G_CALLBACK(incoming), runtime);
    g_socket_service_start(runtime->service);
    return runtime;
}

void firemod_runtime_free(FiremodRuntime *runtime)
{
    if (runtime == NULL) return;
    if (runtime->service != NULL) {
        g_socket_service_stop(runtime->service);
        g_object_unref(runtime->service);
    }
    if (runtime->path != NULL) g_unlink(runtime->path);
    g_free(runtime->path);
    g_free(runtime);
}

const gchar *firemod_runtime_path(FiremodRuntime *runtime)
{
    return runtime != NULL ? runtime->path : NULL;
}
