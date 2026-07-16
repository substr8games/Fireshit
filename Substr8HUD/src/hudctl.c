#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include <stdio.h>
#include <string.h>

static gboolean valid_command(const char *command)
{
    static const char *commands[] = {
        "cycle",
        "desktop",
        "top",
        "interactive",
        "invisible",
        "status",
        "reload",
        "quit",
    };

    for (guint i = 0; i < G_N_ELEMENTS(commands); i++) {
        if (g_strcmp0(command, commands[i]) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

static void print_usage(const char *program)
{
    fprintf(
        stderr,
        "Usage: %s {cycle|desktop|top|interactive|invisible|status|reload|quit}\n",
        program);
}

int main(int argc, char **argv)
{
    if (argc != 2 || !valid_command(argv[1])) {
        print_usage(argv[0]);
        return 2;
    }

    gchar *socket_path = g_build_filename(
        g_get_user_runtime_dir(), "wayfire-hud.sock", NULL);
    GSocketAddress *address = g_unix_socket_address_new(socket_path);
    GSocketClient *client = g_socket_client_new();
    GError *error = NULL;
    GSocketConnection *connection = g_socket_client_connect(
        client, G_SOCKET_CONNECTABLE(address), NULL, &error);

    if (connection == NULL) {
        fprintf(stderr, "wayfire-hudctl: %s\n", error->message);
        g_clear_error(&error);
        g_object_unref(client);
        g_object_unref(address);
        g_free(socket_path);
        return 1;
    }

    gchar *request = g_strdup_printf("%s\n", argv[1]);
    GOutputStream *output = g_io_stream_get_output_stream(G_IO_STREAM(connection));
    GInputStream *input = g_io_stream_get_input_stream(G_IO_STREAM(connection));
    gboolean wrote = g_output_stream_write_all(
        output, request, strlen(request), NULL, NULL, &error);
    g_output_stream_flush(output, NULL, NULL);
    g_free(request);

    if (!wrote) {
        fprintf(stderr, "wayfire-hudctl: %s\n", error->message);
        g_clear_error(&error);
        g_io_stream_close(G_IO_STREAM(connection), NULL, NULL);
        g_object_unref(connection);
        g_object_unref(client);
        g_object_unref(address);
        g_free(socket_path);
        return 1;
    }

    gchar response[256] = {0};
    gssize count = g_input_stream_read(input, response, 255, NULL, &error);
    if (count <= 0) {
        fprintf(
            stderr,
            "wayfire-hudctl: %s\n",
            error != NULL ? error->message : "empty response");
        g_clear_error(&error);
        g_io_stream_close(G_IO_STREAM(connection), NULL, NULL);
        g_object_unref(connection);
        g_object_unref(client);
        g_object_unref(address);
        g_free(socket_path);
        return 1;
    }

    response[count] = '\0';
    fputs(response, stdout);
    gboolean failed = g_str_has_prefix(response, "ERROR");

    g_io_stream_close(G_IO_STREAM(connection), NULL, NULL);
    g_object_unref(connection);
    g_object_unref(client);
    g_object_unref(address);
    g_free(socket_path);
    return failed ? 1 : 0;
}
