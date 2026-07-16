#include "firemod-actions.h"

#include <gio/gio.h>

static void show_error(GtkWindow *parent, const char *message)
{
    GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", message);
    gtk_alert_dialog_set_detail(dialog, "Firemod did not change any settings.");
    gtk_alert_dialog_show(dialog, parent);
    g_object_unref(dialog);
}

void firemod_action_open_path(GtkWindow *parent, const char *path)
{
    if (path == NULL || !g_file_test(path, G_FILE_TEST_EXISTS)) {
        show_error(parent, "The source path is not currently available.");
        return;
    }
    char *uri = g_filename_to_uri(path, NULL, NULL);
    GError *error = NULL;
    if (uri == NULL || !g_app_info_launch_default_for_uri(uri, NULL, &error)) {
        show_error(parent, error != NULL ? error->message : "Could not open the source.");
        g_clear_error(&error);
    }
    g_free(uri);
}

void firemod_action_open_folder(GtkWindow *parent, const char *path)
{
    if (path == NULL) {
        show_error(parent, "No source folder is registered for this item.");
        return;
    }
    char *folder = g_file_test(path, G_FILE_TEST_IS_DIR) ?
        g_strdup(path) : g_path_get_dirname(path);
    firemod_action_open_path(parent, folder);
    g_free(folder);
}

void firemod_action_copy_path(GtkWidget *widget, const char *path)
{
    if (path == NULL) {
        return;
    }
    GdkClipboard *clipboard = gtk_widget_get_clipboard(widget);
    gdk_clipboard_set_text(clipboard, path);
}

void firemod_action_run(GtkWindow *parent, const char *command)
{
    if (command == NULL || *command == '\0') {
        show_error(parent, "No native action is registered for this module.");
        return;
    }
    GError *error = NULL;
    if (!g_spawn_command_line_async(command, &error)) {
        show_error(parent, error->message);
        g_clear_error(&error);
    }
}
