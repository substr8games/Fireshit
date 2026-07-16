#include <gtk/gtk.h>

#include "firemod-app.h"

int main(int argc, char **argv)
{
    GtkApplication *application = gtk_application_new(
        "org.philabs.Firemod", G_APPLICATION_HANDLES_COMMAND_LINE);
    FiremodApp *app = firemod_app_new(application);
    g_signal_connect(application, "startup",
        G_CALLBACK(firemod_app_startup), app);
    g_signal_connect(application, "activate",
        G_CALLBACK(firemod_app_activate), app);
    g_signal_connect(application, "command-line",
        G_CALLBACK(firemod_app_command_line), app);
    g_signal_connect(application, "shutdown",
        G_CALLBACK(firemod_app_shutdown), app);
    int status = g_application_run(G_APPLICATION(application), argc, argv);
    firemod_app_free(app);
    g_object_unref(application);
    return status;
}
