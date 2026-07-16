#include <gio/gio.h>
#include <string.h>

static gchar *socket_path(void)
{
    const char *runtime = g_get_user_runtime_dir();
    return runtime != NULL ? g_build_filename(runtime, "firemod.sock", NULL) : NULL;
}

static gboolean send_request(const char *request)
{
    gchar *path = socket_path();
    if (path == NULL) return FALSE;
    GSocketClient *client = g_socket_client_new();
    GSocketAddress *address = g_unix_socket_address_new(path);
    GError *error = NULL;
    GSocketConnection *connection = g_socket_client_connect(
        client, G_SOCKET_CONNECTABLE(address), NULL, &error);
    gboolean sent = FALSE;
    if (connection != NULL) {
        GOutputStream *output = g_io_stream_get_output_stream(
            G_IO_STREAM(connection));
        sent = g_output_stream_write_all(output, request,
            strlen(request), NULL, NULL, &error);
        g_io_stream_close(G_IO_STREAM(connection), NULL, NULL);
        g_object_unref(connection);
    }
    g_clear_error(&error);
    g_object_unref(address);
    g_object_unref(client);
    g_free(path);
    return sent;
}

static void start_core(void)
{
    char *argv[] = {"firemod", "--background", NULL};
    g_spawn_async(NULL, argv, NULL,
        G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}

int main(int argc, char **argv)
{
    if (argc != 4 || g_strcmp0(argv[1], "menu") != 0) {
        g_printerr("usage: firemodctl menu <global-x> <global-y>\n");
        return 2;
    }
    char *end_x = NULL;
    char *end_y = NULL;
    double x = g_ascii_strtod(argv[2], &end_x);
    double y = g_ascii_strtod(argv[3], &end_y);
    if (end_x == argv[2] || *end_x != '\0' ||
        end_y == argv[3] || *end_y != '\0') {
        g_printerr("firemodctl: invalid pointer coordinates\n");
        return 2;
    }
    char *request = g_strdup_printf("FIREMOD/1 MENU %.3f %.3f\n", x, y);
    if (!send_request(request)) {
        start_core();
        for (guint attempt = 0; attempt < 80; attempt++) {
            g_usleep(25000);
            if (send_request(request)) {
                g_free(request);
                return 0;
            }
        }
        g_printerr("firemodctl: Firemod Core did not become available\n");
        g_free(request);
        return 1;
    }
    g_free(request);
    return 0;
}
