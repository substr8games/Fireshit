#include <gtk/gtk.h>

#include "hud-app.h"

int main(int argc, char **argv)
{
    GtkApplication *application = gtk_application_new(
        "org.philabs.WayfireConsoleHud",
        G_APPLICATION_NON_UNIQUE);
    HudApp *app = hud_app_new(application);

    g_signal_connect(application, "activate", G_CALLBACK(hud_app_activate), app);
    g_signal_connect(application, "shutdown", G_CALLBACK(hud_app_shutdown), app);

    int status = g_application_run(G_APPLICATION(application), argc, argv);

    hud_app_free(app);
    g_object_unref(application);
    return status;
}
